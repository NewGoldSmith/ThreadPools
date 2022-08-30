//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "CallbacksR.h"

using namespace std;
namespace ThreadPoolServerR {
	std::atomic_uint gAcceptedPerSec(0);
	std::atomic_uint gID(0);
	std::atomic_uint gCDel(0);
	std::atomic_uint gMaxConnecting(0);
	ThreadPoolServerR::SocketContext gSockets[ELM_SIZE];
	RingBuf gSocketsPool(gSockets, ELM_SIZE);
	SocketContext* gpListenSocket(NULL);
	PTP_TIMER gpTPTimer(NULL);

	const std::unique_ptr
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

	const std::unique_ptr
		< TP_POOL
		, decltype(CloseThreadpool)*
		> ptpp
	{ /*WINBASEAPI Must_inspect_result PTP_POOL WINAPI*/CreateThreadpool
		( /*Reserved PVOID reserved*/nullptr
		)
	, /*WINBASEAPI VOID WINAPI */CloseThreadpool/*(_Inout_ PTP_POOL ptpp)*/
	};

	const std::unique_ptr
		< FILETIME
		, void (*)(FILETIME*)
		> gp1000msecFT
	{ []()
		{
			const auto gp1000msecFT = new FILETIME;
			Make1000mSecFileTime(gp1000msecFT);
			return gp1000msecFT;
		}()
	,[](_Inout_ FILETIME* gp1000msecFT)
		{
				delete gp1000msecFT;
		}
	};

	VOID ThreadPoolServerR::OnEvListenCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{

		SocketContext* gpListenSocket = (SocketContext*)Context;
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
			u_int uID(gID++);
			SocketContext* pSocket = gSocketsPool.Pull();
			pSocket->ID = uID;
			if ((pSocket->hSocket = accept(gpListenSocket->hSocket, NULL, NULL)) == INVALID_SOCKET)
			{
				int Err = WSAGetLastError();
				stringstream  ss;
				ss << "SevID:" + std::to_string(pSocket->ID) + " accept. code:" + std::to_string(Err)+" File:"<<__FILE__<<" Line"<<  __LINE__ + "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
			}

			//接続ソケットの通知イベントを設定。
			if (WSAEventSelect(pSocket->hSocket, pSocket->hEvent, FD_CLOSE | FD_READ))
			{
				stringstream  ss;
				ss << "SevID: " << std::to_string(pSocket->ID)<< "err:WSAEventSelect" << __FILE__ << __LINE__ << std::endl;
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return;
			}

			gMaxConnecting.store( __max(gMaxConnecting.load(), uID - gCDel));
//			pSocket->vstr.push_back( "SevID:"+std::to_string(pSocket->ID)+" Success Accepted Socket.\r\n");

			//イベントと待機コールバック関数の結びつけ。
			PTP_WAIT pTPWait(NULL);
			if (!(pTPWait = CreateThreadpoolWait(OnEvSocketCB, pSocket, &*pcbe)))
				return;
			//待機コールバック開始。
			SetThreadpoolWait(pTPWait, pSocket->hEvent, NULL);
		}

		//リッスンソケット待機イベント再設定。
		SetThreadpoolWait(Wait, gpListenSocket->hEvent, NULL);
	}

	VOID ThreadPoolServerR::OnEvSocketCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{
		WSANETWORKEVENTS NetworkEvents{};
		DWORD Err = 0;
		DWORD dwBytes = 0;
		SocketContext* pSocket = (SocketContext*)Context;

		if (WSAEnumNetworkEvents(pSocket->hSocket, pSocket->hEvent, &NetworkEvents))
		{
			Err = WSAGetLastError();
			if (Err != WSANOTINITIALISED)
			{
				stringstream  ss;
				ss << "Socket Err: " << Err << "FILE NAME: " << __FILE__ << "LINE: " << __LINE__ << std::endl;
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
			}
			CloseThreadpoolWait(Wait);
			return;
		}

		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			pSocket->Buf.resize(BUFFER_SIZE, '\0');
			pSocket->Buf.resize(recv(pSocket->hSocket, pSocket->Buf.data(), pSocket->Buf.size(), 0));
			if (pSocket->Buf.size()==SOCKET_ERROR)
			{
				pSocket->Buf.clear();
				Err = WSAGetLastError();
				stringstream  ss;
				ss << "Err! recv Code: " << Err << "FILE NAME: " << __FILE__ << " LINE: " << __LINE__ << std::endl;
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				CloseThreadpoolWait(Wait);
				return;
			}
			else if (pSocket->Buf.size() == 0)
			{
				//切断
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				CloseThreadpoolWait(Wait);
				return;
			}
			else {
				//エコー開始
				pSocket->RemString += pSocket->Buf;
				pSocket->Buf = SplitLastLineBreak(pSocket->RemString);
				if (!pSocket->Buf.empty())
				{
					pSocket->Buf += "\r\n";
					send(pSocket->hSocket, pSocket->Buf.data(), pSocket->Buf.size(), 0);
				}
				SetThreadpoolWait(Wait, pSocket->hEvent, NULL);
				return;
			}
		}

		if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			pSocket->ReInitialize();
			gSocketsPool.Push(pSocket);
			//接続数が０になるとステータスを表示。
			if (!(gID - (++gCDel) - 1))
			{
				ShowStatus();
			}
			CloseThreadpoolWait(Wait);
			return;
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
		gpListenSocket->ID = gID++;

		//ソケット作成
		WSAPROTOCOL_INFOA prot_info{};
		gpListenSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0/*WSA_FLAG_OVERLAPPED*/);
		if (!gpListenSocket->hSocket)
		{
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			++gCDel;
			return false;
		}

		//ソケットリユースオプション
		BOOL yes = 1;
		if (setsockopt(gpListenSocket->hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)))
		{
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			++gCDel;
			stringstream  ss;
			ss << "setsockopt Error! Line:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			return false;
		}

		//ホストバインド設定
//		CHAR strHostAddr[] = "127.0.0.2";
//		u_short usHostPort = 50000;
		DWORD Err = 0;
		struct sockaddr_in addr = { };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(HOST_LISTEN_PORT);
		int addr_size = sizeof(addr.sin_addr);
		int rVal = inet_pton(AF_INET, HOST_BASE_ADDR, &(addr.sin_addr));
		if (rVal != 1)
		{
			if (rVal == 0)
			{
				stringstream  ss;
				ss<< "socket error:Listen inet_pton return val 0\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				gpListenSocket->ReInitialize();
				gSocketsPool.Push(gpListenSocket);
				++gCDel;
				return false;
			}
			else if (rVal == -1)
			{
				Err = WSAGetLastError();
				stringstream  ss;
				ss << "socket error:Listen return val is -1 by inet_pton. Code:"<<to_string(Err)<<"\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				gpListenSocket->ReInitialize();
				gSocketsPool.Push(gpListenSocket);
				++gCDel;
				return false;
			}
		}
		rVal = ::bind(gpListenSocket->hSocket, (struct sockaddr*)&(addr), sizeof(addr));
		if (rVal == SOCKET_ERROR)
		{
			Err = WSAGetLastError();
			stringstream  ss;
			ss << "Err! Listen Socket bind. Code:" << to_string(rVal) << " LINE:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			++gCDel;
			return false;
		}

		//イベント設定
		if (WSAEventSelect(gpListenSocket->hSocket, gpListenSocket->hEvent, FD_ACCEPT/* | FD_CLOSE | FD_READ | FD_CONNECT | FD_WRITE*/))
		{
			stringstream  ss;
			ss << "Err! Listen Socket WSAEventSelect. Code:" << to_string(WSAGetLastError()) << "LINE:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			++gCDel;
			return false;
		}

		//イベントハンドラ設定
		if (!(gpListenSocket->ptpwaitOnEvListen = CreateThreadpoolWait(OnEvListenCB, gpListenSocket, &*pcbe)))
		{
			stringstream  ss;
			ss << "Err! Listen Socket CreateThreadpoolWait. Code:" << to_string(WSAGetLastError()) << "__LINE__" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			++gCDel;
			return false;
		}
		SetThreadpoolWait(gpListenSocket->ptpwaitOnEvListen, gpListenSocket->hEvent, NULL);
		std::cout << "Listen Start\r\n"<< HOST_BASE_ADDR <<":"<<to_string(HOST_LISTEN_PORT)<<"\r\n";

		//リッスン
		if (listen(gpListenSocket->hSocket, SOMAXCONN))
		{
			stringstream  ss;
			ss << "Err! Listen Socket listen. Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			++gCDel;
			return false;
		}

		// Accepted/sec測定用タイマーコールバック設定
		if (!(gpTPTimer = CreateThreadpoolTimer(MeasureConnectedPerSecCB, &gAcceptedPerSec, &*pcbe)))
		{
			stringstream  ss;
			ss << "err:CreateThreadpoolTimer. Code:"<<to_string(WSAGetLastError())<<" LINE:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			gpListenSocket->ReInitialize();
			gSocketsPool.Push(gpListenSocket);
			++gCDel;
			return false;
		}
		SetThreadpoolTimer(gpTPTimer, &*gp1000msecFT, 1000, 0);
	}

	void EndListen()
	{
		WaitForThreadpoolTimerCallbacks(gpTPTimer, FALSE);
		CloseThreadpoolTimer(gpTPTimer);

		shutdown(gpListenSocket->hSocket, SD_SEND);
	}

	void ShowStatus()
	{
		std::cout << "\r\nTotal Connected: " << to_string(ThreadPoolServerR::gID - 1) << "\r\n";
		std::cout << "Current Connected: " << gID - gCDel - 1 << "\r\n";
		std::cout << "Max Connecting: " << gMaxConnecting << "\r\n" ;
		std::cout << "Max Accepted/Sec: " << gAcceptedPerSec << "\r\n";
		std::cout << "Host: "<< HOST_BASE_ADDR <<":"<<to_string(HOST_LISTEN_PORT)<<"\r\n\r\n";
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
		cout << "\033[0;0H";
		cout << "\033[2J";
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

	FILETIME* Make1000mSecFileTime(FILETIME * pFiletime)
	{
		ULARGE_INTEGER ulDueTime;
		ulDueTime.QuadPart = (ULONGLONG)-(1 * 10 * 1000 * 1000);
		pFiletime->dwHighDateTime = ulDueTime.HighPart;
		pFiletime->dwLowDateTime = ulDueTime.LowPart;
		return pFiletime;
	}

}