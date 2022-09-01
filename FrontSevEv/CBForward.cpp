//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "CBForward.h"

using namespace std;

namespace FrontSevEv {
	std::atomic_uint gAcceptedPerSec(0);
	std::atomic_uint gID(1);
	std::atomic_uint gCDel(0);
	std::atomic_uint gMaxConnecting(0);
	ForwardContext gSockets[ELM_SIZE];
	RingBuf gSocketsPool(gSockets, ELM_SIZE);
	ForwardContext* gpListenSocket(NULL);
	PTP_TIMER gpTPTimer(NULL);

	extern const unique_ptr
		< TP_CALLBACK_ENVIRON
		, decltype(DestroyThreadpoolEnvironment)*
		> pcbe
	{ []()
		{
			const auto pcbe = new TP_CALLBACK_ENVIRON;
			/*FORCEINLINE VOID*/InitializeThreadpoolEnvironment
			( /*Out PTP_CALLBACK_ENVIRON pcbe*/pcbe
			);
			return pcbe;
		}()
	, [](_Inout_ PTP_CALLBACK_ENVIRON pcbe)
		{
			/*FORCEINLINE VOID*/DestroyThreadpoolEnvironment
			( /*Inout PTP_CALLBACK_ENVIRON pcbe*/pcbe
			);
			delete pcbe;
		}
	};

	extern const unique_ptr
		< TP_POOL
		, decltype(CloseThreadpool)*
		> ptpp
	{ /*WINBASEAPI Must_inspect_result PTP_POOL WINAPI*/CreateThreadpool
		( /*Reserved PVOID reserved*/nullptr
		)
	, /*WINBASEAPI VOID WINAPI */CloseThreadpool/*(_Inout_ PTP_POOL ptpp)*/
	};

	extern const unique_ptr
		< FILETIME
		, void (*)(FILETIME*)
		> gp1000msecFT
	{ []()
		{
			const auto gp1000msecFT = new FILETIME;
			ULARGE_INTEGER ulDueTime;
			ulDueTime.QuadPart = (ULONGLONG)-(1 * 10 * 1000 * 1000);
			gp1000msecFT->dwHighDateTime = ulDueTime.HighPart;
			gp1000msecFT->dwLowDateTime = ulDueTime.LowPart;
			return gp1000msecFT;
		}()
	,[](_Inout_ FILETIME* gp1000msecFT)
		{
			delete gp1000msecFT;
		}
	};

	VOID OnEvListenCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{

		ForwardContext* gpListenSocket = (ForwardContext*)Context;
		//デバック用ID==0か確認。ELM_SIZEがべき乗になっているか確認。
		assert(gpListenSocket->ID == 0);

		WSANETWORKEVENTS NetworkEvents{};
		DWORD Err = 0;
		DWORD dwBytes = 0;
		if (WSAEnumNetworkEvents(gpListenSocket->hSocket, gpListenSocket->hEvent, &NetworkEvents))
		{
			Err = WSAGetLastError();
			MyTRACE(("SevError: WSAEnumNetworkEvents.Code:" + std::to_string(Err)).c_str());
			return;
		}

		if (NetworkEvents.lNetworkEvents & FD_ACCEPT)
		{
			atomic_uint uID(gID++);
			ForwardContext* pSocket = gSocketsPool.Pull();
			pSocket->ID = uID;
			if ((pSocket->hSocket = accept(gpListenSocket->hSocket, NULL, NULL)) == INVALID_SOCKET)
			{
				int Err = WSAGetLastError();
				stringstream  ss;
				ss << "SevID:" + std::to_string(pSocket->ID) + " accept. code:" + std::to_string(Err)+" File:"<<__FILE__<<" Line"<<  __LINE__ + "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				DecStatusFront();
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				SetThreadpoolWait(gpListenSocket->ptpwaitOnEvListen, gpListenSocket->hEvent, NULL);
				return;
			}

			//接続ソケットの通知イベントを設定。
			if (WSAEventSelect(pSocket->hSocket, pSocket->hEvent, FD_CLOSE | FD_READ))
			{
				stringstream  ss;
				ss << "SevID: " << std::to_string(pSocket->ID)<< "err:WSAEventSelect" << __FILE__ << __LINE__ << std::endl;
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				DecStatusFront();
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				return;
			}

			gMaxConnecting.store( __max(gMaxConnecting.load(), uID - gCDel));
//			pSocket->vstr.push_back( "SevID:"+std::to_string(pSocket->ID)+" Success Accepted Socket.\r\n");

			//イベントと待機コールバック関数の結びつけ。
			if (!(pSocket->pTPWait = CreateThreadpoolWait(OnEvSocketFrontCB, pSocket, &*pcbe)))
				return;
			//待機コールバック開始。
			SetThreadpoolWait(pSocket->pTPWait, pSocket->hEvent, NULL);
		}

		//リッスンソケット待機イベント再設定。
		SetThreadpoolWait(Wait, gpListenSocket->hEvent, NULL);
	}

	VOID OnEvSocketFrontCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{
		WSANETWORKEVENTS NetworkEvents{};
		DWORD dwBytes = 0;
		ForwardContext* pSocket = (ForwardContext*)Context;

		if (WSAEnumNetworkEvents(pSocket->hSocket, pSocket->hEvent, &NetworkEvents))
		{
			DWORD Err = WSAGetLastError();
			stringstream  ss;
			ss << "FrontSevEv. Front. WSAEnumNetworkEvents. Code:" << Err << "LINE: " << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			DecStatusFront();
			pSocket->ReInitialize();
			gSocketsPool.Push(pSocket);
			return;
		}

		if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			stringstream  ss;
			ss << ("FrontSevEv. Socket ID:" + to_string(pSocket->ID) + " Closed\r\n").c_str();
			//cerr << ss.str();
			//MyTRACE(ss.str().c_str());
			DecStatusFront();
			pSocket->ReInitialize();
			gSocketsPool.Push(pSocket);
			return;
		}

		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			pSocket->ReadBuf.resize(BUFFER_SIZE, '\0');
			int size=recv(pSocket->hSocket, pSocket->ReadBuf.data(), pSocket->ReadBuf.size(), 0);
			if (size==SOCKET_ERROR)
			{
				pSocket->ReadBuf.clear();
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. Front. recv. Code: " << Err << "FILE NAME: " << __FILE__ << " LINE: " << __LINE__ << std::endl;
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				DecStatusFront();
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				return;
			}
			else if (size == 0)
			{
				//切断
				stringstream  ss;
				ss << "FrontSevEv. Front. recv. size 0" << " LINE: " << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				pSocket->ReInitialize();
				DecStatusFront();
				gSocketsPool.Push(pSocket);
				return;
			}
			else {
				//エコー開始
				pSocket->ReadBuf.resize(size);
				pSocket->RemString += pSocket->ReadBuf;
				string str =SplitLastLineBreak(pSocket->RemString);
				if (!str.empty())
				{
					str += "\r\n";
					pSocket->vBufLock.acquire();
					pSocket->vBuf.push_back(str);
					pSocket->vBufLock.release();
					QueryBack(pSocket);
				}
				SetThreadpoolWait(Wait, pSocket->hEvent, NULL);
				return;
			}
		}
	}

	VOID MeasureConnectedPerSecCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
	{
		static u_int oldtime(0);
		u_int now(gID);
		if (now > 1)
		{
			std::atomic_uint* pgConnectPerSec = (std::atomic_uint*)Context;
			*pgConnectPerSec = __max(pgConnectPerSec->load(), now - oldtime);
		}
		oldtime = now;
	}

	void InitTP()
	{
		constexpr auto MAX_TASKS = 2;
		constexpr auto MIN_TASKS = 1;
		/*WINBASEAPI VOID WINAPI*/SetThreadpoolThreadMaximum
		( /*Inout PTP_POOL ptpp     */&*ptpp
			, /*In    DWORD    cthrdMost*/MAX_TASKS
		);
		(void)/*WINBASEAPI BOOL WINAPI*/SetThreadpoolThreadMinimum
		( /*Inout PTP_POOL ptpp    */&*ptpp
			, /*In    DWORD    cthrdMic*/MIN_TASKS
		);

		/*FORCEINLINE VOID*/SetThreadpoolCallbackPool
		( /*Inout PTP_CALLBACK_ENVIRON pcbe*/&*pcbe
			, /*In    PTP_POOL             ptpp*/&*ptpp
		);
	}

	int StartListen()
	{
		gpListenSocket = gSocketsPool.Pull();
		//デバック用にIDをつける。リッスンソケットIDは0。
		gpListenSocket->ID = 0;

		//ソケット作成
		WSAPROTOCOL_INFOA prot_info{};
		gpListenSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0/*WSA_FLAG_OVERLAPPED*/);
		if (!gpListenSocket->hSocket)
		{
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			return false;
		}

		//ソケットリユースオプション
		BOOL yes = 1;
		if (setsockopt(gpListenSocket->hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)))
		{
			stringstream  ss;
			ss << "FrontSevEv. Front. setsockopt Error! Line:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			return false;
		}

		//ホストバインド設定
		DWORD Err = 0;
		struct sockaddr_in addr = { };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(HOST_FRONT_LISTEN_PORT);
		int addr_size = sizeof(addr.sin_addr);
		int rVal = inet_pton(AF_INET, HOST_FRONT_LISTEN_BASE_ADDR, &(addr.sin_addr));
		if (rVal != 1)
		{
			if (rVal == 0)
			{
				stringstream  ss;
				ss<< "FrontSevEv. Front. Listen inet_pton return val 0\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				gpListenSocket->ReInitialize();
				gSocketsPool.Push(gpListenSocket);
				return false;
			}
			else if (rVal == -1)
			{
				Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. Front. Listen return val is -1 by inet_pton. Code:"<<to_string(Err)<<"\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				gpListenSocket->ReInitialize();
				gSocketsPool.Push(gpListenSocket);
				return false;
			}
		}
		rVal = ::bind(gpListenSocket->hSocket, (struct sockaddr*)&(addr), sizeof(addr));
		if (rVal == SOCKET_ERROR)
		{
			Err = WSAGetLastError();
			stringstream  ss;
			ss << "FrontSevEv. Front. Listen Socket bind. Code:" << to_string(rVal) << " LINE:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			return false;
		}

		//イベント設定
		if (WSAEventSelect(gpListenSocket->hSocket, gpListenSocket->hEvent, FD_ACCEPT/* | FD_CLOSE | FD_READ | FD_CONNECT | FD_WRITE*/))
		{
			stringstream  ss;
			ss << "FrontSevEv. Front. Listen Socket WSAEventSelect. Code:" << to_string(WSAGetLastError()) << "LINE:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			return false;
		}

		//イベントハンドラ設定
		if (!(gpListenSocket->ptpwaitOnEvListen = CreateThreadpoolWait(OnEvListenCB, gpListenSocket, &*pcbe)))
		{
			stringstream  ss;
			ss << "FrontSevEv. Front. Listen Socket CreateThreadpoolWait. Code:" << to_string(WSAGetLastError()) << "__LINE__" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			return false;
		}
		SetThreadpoolWait(gpListenSocket->ptpwaitOnEvListen, gpListenSocket->hEvent, NULL);

		//リッスン
		if (listen(gpListenSocket->hSocket, SOMAXCONN))
		{
			stringstream  ss;
			ss << "FrontSevEv. Front. Listen Socket listen. Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			return false;
		}

		//成功したので表示。
		cout << "Listen Start\r\n";
		cout <<"Listen Address: " << HOST_FRONT_LISTEN_BASE_ADDR << ":" << to_string(HOST_FRONT_LISTEN_PORT) << "\r\n";

		// Accepted/sec測定用タイマーコールバック設定
		if (!(gpTPTimer = CreateThreadpoolTimer(MeasureConnectedPerSecCB, &gAcceptedPerSec, &*pcbe)))
		{
			stringstream  ss;
			ss << "FrontSevEv. Front. CreateThreadpoolTimer. Code:"<<to_string(WSAGetLastError())<<" LINE:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			return false;
		}
		SetThreadpoolTimer(gpTPTimer, &*gp1000msecFT, 1000, 0);
		return TRUE;
	}

	void EndListen()
	{
		gpListenSocket->ReInitialize();
		gSocketsPool.Push(gpListenSocket);
	}

	void ShowStatus()
	{
		std::cout << "\r\nTotal Connected: " << to_string(gID - 1) << "\r\n";
		std::cout << "Current Connected: " << gID - gCDel - 1 << "\r\n";
		std::cout << "Max Connecting: " << gMaxConnecting << "\r\n" ;
		std::cout << "Max Accepted/Sec: " << gAcceptedPerSec << "\r\n";
		std::cout << "Host: "<< HOST_FRONT_LISTEN_BASE_ADDR<<":"<<to_string(HOST_FRONT_LISTEN_PORT)<<"\r\n";
		cout << "Listen Address: " << HOST_FRONT_LISTEN_BASE_ADDR << ":" << to_string(HOST_FRONT_LISTEN_PORT) << "\r\n";
		cout << "Back Target Address: " << PEER_BACK_BASE_ADDR << ":" << to_string(PEER_BACK_PORT) << "\r\n\r\n";
	}

	void ClearStatus()
	{
		gID = 1;
		gCDel = 0;
		gMaxConnecting = 0;
		gAcceptedPerSec = 0;
		ShowStatus();
	}

	void Cls()
	{
		cout << "\x1b[2J";
		cout << "\x1b[0;0H";
	}

	void DecStatusFront()
	{
		++gCDel;
		if (!(gID - gCDel-1))
		{
			cout << "End Work" << "\r\n";

			//ステータスを表示
			cout << "\r\nstatus\r\n";
			ShowStatus();
		}
	}

	std::string SplitLastLineBreak(std::string& str)
	{
		using namespace std;
		std::string strsub;
		auto pos = str.rfind("\r\n");
		if (pos == string::npos)
		{
			pos = str.rfind("\n");
			if (pos != string::npos)
			{
				strsub = str.substr(0, pos);
				str.erase(0, pos + 1);
			}
		}
		else {
			strsub = str.substr(0, pos);
			str.erase(0, pos + 2);
		}
		return strsub;
	}

}