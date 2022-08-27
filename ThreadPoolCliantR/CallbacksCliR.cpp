//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Cliant side
#include "CallbacksCliR.h"
using namespace std;
namespace ThreadPoolCliantR {

	std::atomic_uint gMaxConnect{};
	std::atomic_uint gCDel{};
	std::atomic_uint gID{};
	std::atomic<ULONGLONG> gtMinRepTime{ ULLONG_MAX };
	std::atomic<ULONGLONG> gtMaxRepTime{};
	FILETIME gJobStartTime{};
	FILETIME gJobEndTime{};
	SocketContext gSockets[ELM_SIZE];
	RingBuf<SocketContext> gSocketsPool(gSockets, ELM_SIZE);

	const std::unique_ptr
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
		< DWORD
		, void (*)(DWORD*)
		> gpOldConsoleMode
	{ []()
		{
			const auto gpOldConsoleMode = new DWORD;
			HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
			if (!GetConsoleMode(hOut, gpOldConsoleMode))
			{
				cerr << "Err!GetConsoleMode" << " LINE:" << to_string(__LINE__) << "\r\n";
			}
			DWORD ConModeOut =
				0
				| ENABLE_PROCESSED_OUTPUT
				| ENABLE_WRAP_AT_EOL_OUTPUT
				| ENABLE_VIRTUAL_TERMINAL_PROCESSING
				//		|DISABLE_NEWLINE_AUTO_RETURN       
				//		|ENABLE_LVB_GRID_WORLDWIDE
				;
			if (!SetConsoleMode(hOut, ConModeOut))
			{
				cerr << "Err!SetConsoleMode" << " LINE:" << to_string(__LINE__) << "\r\n";
			}
			return gpOldConsoleMode;
		}()
	,[](_Inout_ DWORD* gpOldConsoleMode)
		{
			HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
			if (!SetConsoleMode(hOut, *gpOldConsoleMode))
			{
				cerr << "Err!SetConsoleMode" << " LINE:" << to_string(__LINE__) << "\r\n";
			}
			delete gpOldConsoleMode;
		}
	};

	TryConnectContext gTryConnectContext[NUM_THREAD];

	VOID OnEvSocketCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{
		WSANETWORKEVENTS NetworkEvents{};
		DWORD Err = 0;
		DWORD dwBytes = 0;
		SocketContext* pSocket = (SocketContext*)Context;
		if (WSAEnumNetworkEvents(pSocket->hSocket, pSocket->hEvent, &NetworkEvents))
		{
			std::cerr << "Socket Err. Code:" << to_string(WSAGetLastError()) << " LINE: " << __LINE__ << "\r\n";
			CloseThreadpoolWait(Wait);
			pSocket->ReInitialize();
			gSocketsPool.Push(pSocket);
			++gCDel;
			return;
		}

		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			//レシーブ
			pSocket->ReadString.resize(BUFFER_SIZE, '\0');

			int size=recv(pSocket->hSocket, pSocket->ReadString.data(), pSocket->ReadString.size(), 0);

			if (size == SOCKET_ERROR)
			{
				pSocket->ReadString.clear();
				Err = WSAGetLastError();
				std::cerr << "Socket Err! Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				CloseThreadpoolWait(Wait);
				return;
			}
			else if (size == 0)
			{
				CloseThreadpoolWait(Wait);
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				return;
			}
			else {
				//受信成功
				pSocket->ReadString.resize(size);
				pSocket->RemString += pSocket->ReadString;
				pSocket->DispString = SplitLastLineBreak(pSocket->RemString);
				pSocket->DispString += "\r\n";
				PTP_WORK pTPWork(NULL);
#ifndef DISPLAY_SUPPRESSION
				//受信したデータを表示する。
				if (!(pTPWork = CreateThreadpoolWork(SerializedDisplayCB, pSocket, &*pcbe)))
				{
					stringstream  ss;
					ss << "Err! CreateThreadpoolWork(SerializedDisplayCB, pSocket, &*pcbe). Line:" << to_string(__LINE__) << "\r\n";
					MyTRACE(ss.str().c_str());
					cerr << ss.str();
				}
				else {
					SubmitThreadpoolWork(pTPWork);
				}
#endif
				//レスポンス測定用。
				u_int uiCount=pSocket->FindAndConfirmCountDownNumber(pSocket->DispString);
				//見つけたカウントが範囲内か確認。
				if (0 <= uiCount && uiCount <= N_COUNTDOWN) {
					GetSystemTimeAsFileTime(&pSocket->tRecv[N_COUNTDOWN - uiCount]);
					if (uiCount == 0)
					{
						CloseThreadpoolWait(Wait);
						//ジョブの終わり時間計測
						GetSystemTimeAsFileTime(&gJobEndTime);						gtMaxRepTime.store(__max(gtMaxRepTime.load(), pSocket->GetMaxResponce()));
						gtMinRepTime.store(__min(gtMinRepTime.load(), pSocket->GetMinResponce()));

						pSocket->ReInitialize();
						gSocketsPool.Push(pSocket);

						++gCDel;
						if (!(gID - gCDel))
						{

							Sleep(2000);
							std::cout << "End Work" << "\r\n" << std::flush;

							//ステータスを表示
							std::cout << "\r\nstatus\r\n";
							ShowStatus();
						}

						return;
					}
				}
				else {
					stringstream  ss;
					ss << "Err! Reply data is wrong.Socket ID:" << to_string(pSocket->ID) << "\r\n";
					cerr << ss.str();
					MyTRACE(ss.str().c_str());
					CloseThreadpoolWait(Wait);
					pSocket->ReInitialize();
					gSocketsPool.Push(pSocket);
					++gCDel;
				}
			}
		}

		if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			std::cout << "Socket is Closed " << "SocketID: " << pSocket->ID << std::endl;
			pSocket->ReInitialize();
			gSocketsPool.Push(pSocket);
			++gCDel;
			return;
		}
		//接続ソケット待機イベント再設定。
		SetThreadpoolWait(Wait, pSocket->hEvent, NULL);
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

	int TryConnect()
	{
		using namespace ThreadPoolCliantR;
		//Jobタイム開始時間測定
		GetSystemTimeAsFileTime(&gJobStartTime);

		PTP_WORK ptpwork(NULL);
		for (int i = 0; i < NUM_THREAD; ++i)
		{
			gTryConnectContext[i].pAddr = HOST_BASE_ADDR;
			gTryConnectContext[i].inc = i;
			if (!(ptpwork = CreateThreadpoolWork(TryConnectCB, (PVOID)&gTryConnectContext[i], &*pcbe)))
			{
				stringstream  ss;
				ss << "TryConnect Error.Line:" + __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}
			SubmitThreadpoolWork(ptpwork);
		}
		return true;
	}

	void ShowStatus()
	{
		std::cout << "Total Connected:" << ThreadPoolCliantR::gID << "\r\n";
		std::cout << "Current Connected:" << ThreadPoolCliantR::gID - ThreadPoolCliantR::gCDel << "\r\n";
		std::cout << "Max Connecting:" << ThreadPoolCliantR::gMaxConnect << "\r\n";
		std::cout << "Min Responce msec:" << std::to_string(gtMinRepTime
		) << "\r\n";
		std::cout << "Max Responce msec:" << gtMaxRepTime.load() << "\r\n";
		cout << "Target Address: " << PEER_ADDR<<":"<<to_string(PEER_PORT)<<"\r\n";
		cout << "Host Base Address: " << HOST_BASE_ADDR << ":" << to_string(HOST_PORT) << "\r\n";
		cout << "Job Time:" << to_string(GetDeffSec(gJobEndTime, gJobStartTime)) << "."<<to_string(GetDeffmSec(gJobEndTime,gJobStartTime))<<"\r\n";
		cout << "\r\n";
	}

	void ClearStatus()
	{
		gID = 0;
		gMaxConnect = 0;
		gCDel = 0;
		gtMinRepTime = LLONG_MAX;
		gtMaxRepTime = 0;
		gJobStartTime = {};
		gJobEndTime = {};
		cout << "\r\n";
		ShowStatus();
	}

	void Cls()
	{
		cout << "\x1b[2J";
		cout << "\x1b[0;0H";
	}

	void StartTimer(SocketContext* pSocket)
	{
		pSocket->CountDown = N_COUNTDOWN;
		if (!MakeAndSendSocketMessage(pSocket))
		{
			++gCDel;
			pSocket->ReInitialize();
			gSocketsPool.Push(pSocket);
			return;
		}
		//レスポンス測定用。
		GetSystemTimeAsFileTime(&pSocket->tSend[N_COUNTDOWN - pSocket->CountDown]);
		PTP_TIMER pTPTimer(0);
		if (!(pTPTimer = CreateThreadpoolTimer(OneSecTimerCB, pSocket, &*pcbe)))
		{
			stringstream  ss;
			ss << "err CreateThreadpoolTimer. Line: " << __LINE__ << "\r\n";
			++gCDel;
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			pSocket->ReInitialize();
			gSocketsPool.Push(pSocket);
			return;
		}
		SetThreadpoolTimer(pTPTimer, &*gp1000msecFT, 0, NULL);
	}

	u_int GetDeffSec(const FILETIME& end, const FILETIME& start)
	{
		ULONGLONG t64start = (((ULONGLONG)start.dwHighDateTime) << 32) + start.dwLowDateTime;
		ULONGLONG t64end = (((ULONGLONG)end.dwHighDateTime) << 32) + end.dwLowDateTime;

		ULONGLONG Sec(0);
		if (t64start > t64end)
		{
			int i = 0;
		}
		else {
			Sec = (t64end - t64start) / 10000.0 / 1000;
		}
		return Sec;
	}

	u_int GetDeffmSec(const FILETIME& end, const FILETIME& start)
	{
		ULONGLONG t64start = (((ULONGLONG)start.dwHighDateTime) << 32) + start.dwLowDateTime;
		ULONGLONG t64end = (((ULONGLONG)end.dwHighDateTime) << 32) + end.dwLowDateTime;

		ULONGLONG mSec(0);
		if (t64start > t64end)
		{
			int i = 0;
		}
		else {
			mSec = (ULONGLONG)((t64end - t64start) / 10000.0) % 1000;
		}
		return mSec;
	}

	VOID OneSecTimerCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER pTPTimer)
	{
		SocketContext* pSocket = (SocketContext*)Context;
		--pSocket->CountDown;
		if (pSocket->CountDown >= 0)
		{
			if (!MakeAndSendSocketMessage(pSocket))
			{
				++gCDel;
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				return;
			}

			//レスポンス測定用。
			GetSystemTimeAsFileTime(&pSocket->tSend[N_COUNTDOWN - pSocket->CountDown]);

			SetThreadpoolTimer(pTPTimer, &*gp1000msecFT, 0, 0);
		}
		else {
			//スレッドプールタイマー終了
			SetThreadpoolTimer(pTPTimer, NULL, 0, 0);
			CloseThreadpoolTimer(pTPTimer);
		}
		return VOID();
	}

	VOID SerializedDisplayCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		static	std::binary_semaphore oslock{ 1 };
		oslock.acquire();
		{
			SocketContext* pSocket = (SocketContext*)Context;
			std::cout << pSocket->DispString;
			CloseThreadpoolWork(Work);
		}
		oslock.release();
	}

	VOID TryConnectCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		using namespace ThreadPoolCliantR;
		TryConnectContext* pContext = (TryConnectContext*)Context;

		//ホストsockeaddr_in設定
		struct sockaddr_in addr = { };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(HOST_PORT);
		int addr_size = sizeof(addr.sin_addr);
		int rVal = inet_pton(AF_INET, HOST_BASE_ADDR, &(addr.sin_addr));
		if (rVal != 1)
		{
			if (rVal == 0)
			{
				stringstream  ss;
				ss << "socket error:inet_pton input value invalided\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return;
			}
			else if (rVal == -1)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "CliR. socket error:inet_pton.Code:" << to_string(Err) << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return;
			}
		}
		//ベースアドレスからincだけ増加する
		addr.sin_addr.S_un.S_un_b.s_b4 += pContext->inc;

		//サーバー接続用のsockaddr_inを設定
		struct sockaddr_in Peeraddr = { };
		Peeraddr.sin_family = AF_INET;
		Peeraddr.sin_port = htons(PEER_PORT);
		int Peeraddr_size = sizeof(Peeraddr.sin_addr);
		rVal = inet_pton(AF_INET, PEER_ADDR, &(Peeraddr.sin_addr));
		if (rVal != 1)
		{
			if (rVal == 0)
			{
				stringstream  ss;
				ss << "socket error:inet_pton input value invalided\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return;
			}
			else if (rVal == -1)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "CliR. socket error:inet_pton.Code:" << to_string(Err) << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return;
			}
		}

		for (u_int i = 0; i < NUM_CONNECT; ++i)
		{
			SocketContext* pSocket = gSocketsPool.Pull();
			pSocket->ID = gID++;

			if (((pSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, NULL/*WSA_FLAG_OVERLAPPED*/)) == INVALID_SOCKET))
			{
				DWORD Err = WSAGetLastError();
				pSocket->hSocket = NULL;
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				++gCDel;
				stringstream  ss;
				ss << "CliR. WSASocket Error! Code:"<<to_string(Err) << " Line: " << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return;
			}

			//ホストバインド
			if (::bind(pSocket->hSocket, (struct sockaddr*)&(addr), sizeof(addr)) == SOCKET_ERROR)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "CliR bind Error! Code:"<<std::to_string(Err) << " Line: " << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				++gCDel;
				break ;
			}

			//コネクト
			if (connect(pSocket->hSocket, (SOCKADDR*)&Peeraddr, sizeof(Peeraddr)) == SOCKET_ERROR)
			{
				DWORD Err = WSAGetLastError();
				if (Err != WSAEWOULDBLOCK && Err)
				{
					stringstream  ss;
					ss <<"connect Error. Code :" << std::to_string(Err) <<  " Line : " << __LINE__ << "\r\n";
					cerr << ss.str();
					MyTRACE(ss.str().c_str());
					pSocket->ReInitialize();
					gSocketsPool.Push(pSocket);
					++gCDel;
					break;
				}
			}

			gMaxConnect = __max(gMaxConnect.load(), gID - gCDel);

			//ソケットのイベント設定
			if (WSAEventSelect(pSocket->hSocket, pSocket->hEvent, /*FD_ACCEPT |*/ FD_CLOSE | FD_READ/* | FD_CONNECT | FD_WRITE*/)== SOCKET_ERROR)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "Error WSAEventSelect. Code:"<<to_string(Err)<<" Line:" << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				++gCDel;
				break;
			}
			
			//イベントハンドラの設定
			PTP_WAIT pTPWait(NULL);
			if (!(pTPWait = CreateThreadpoolWait(OnEvSocketCB, pSocket, &*ThreadPoolCliantR::pcbe)))
			{
				stringstream  ss;
				ss << "Error CreateThreadpoolWait. Line:" << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				++gCDel;
				break;
			}
			else {
				SetThreadpoolWait(pTPWait, pSocket->hEvent, NULL);
			}
			StartTimer(pSocket);
		}
		CloseThreadpoolWork(Work);
	}

	bool MakeAndSendSocketMessage(SocketContext* pSocket)
	{
		pSocket->WriteString = "CliID:"+ to_string( pSocket->ID) + " NumCount:" +to_string( pSocket->CountDown)+"\r\n";

		//MyTRACE(("CliID:" + to_string(pSocket->ID) + " Send    : " + pSocket->WriteString).c_str());

		if (send(pSocket->hSocket, pSocket->WriteString.data(), pSocket->WriteString.length(), 0) == SOCKET_ERROR)
		{
			DWORD Err = WSAGetLastError();
			stringstream  ss;
			ss << "Err! CliR. send. Code:" << to_string(Err) << " LINE;" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			return false;
		}
		pSocket->WriteString.clear();
		return true;
	}

	//u_int FindAndConfirmCountDownNumber(const std::string& str)
	//{
	//	std::string s(str);
	//	size_t it2=s.rfind("\r\n");
	//	size_t it1 = s.rfind(":", it2);
	//	std::string substr = s.substr(it1+1, it2-it1-1);
	//	u_int uc(0);
	//	//try {
	//		uc = std::stoi(substr);
	//	//}
	//	//catch (const std::invalid_argument& e) {
	//	//	std::cerr << "Err! FindAndConfirmCountDownNumber. "<<e.what() << " File:" << __FILE__ << " LINE:" << __LINE__ << "\r\n";
	//	//}
	//	return uc;
	//}

}