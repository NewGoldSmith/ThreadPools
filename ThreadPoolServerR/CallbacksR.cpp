//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "CallbacksR.h"


namespace ThreadPoolServerR {
	constexpr auto ELM_SIZE = 65535;
	std::atomic_uint gAcceptedPerSec(0);
	std::atomic_uint gID(0);
	std::atomic_uint gCDel(0);
	std::atomic_uint gMaxConnect(0);
	ThreadPoolServerR::SocketContext gSockets[ELM_SIZE];
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
		//デバック用ID=0か確認。
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

		int i = 0;
		if (NetworkEvents.lNetworkEvents & FD_ACCEPT)
		{
			u_int uID(gID++);
			SocketContext* pConnectSocket = &gSockets[uID];
			pConnectSocket->ID = uID;
			if ((pConnectSocket->hSocket = accept(gpListenSocket->hSocket, NULL, NULL)) == INVALID_SOCKET)
			{
				int Err = WSAGetLastError();
				std::cerr << "SevID:" + std::to_string(pConnectSocket->ID) + " accept. code:" + std::to_string(Err)+" File:"<<__FILE__<<" Line"<<  __LINE__ + "\r\n";
			}

			//接続ソケットの通知イベントを設定。
			if (WSAEventSelect(pConnectSocket->hSocket, pConnectSocket->hEvent, FD_CLOSE | FD_READ))
			{
				std::cerr << "SevID: " << std::to_string(pConnectSocket->ID)<< "err:WSAEventSelect" << __FILE__ << __LINE__ << std::endl;
				return;
			}

			gMaxConnect.store( __max(gMaxConnect.load(), uID - gCDel));
			pConnectSocket->vstr.push_back( "SevID:"+std::to_string(pConnectSocket->ID)+" Success Accepted Socket.\r\n");
			SockTRACE(pConnectSocket);

			//イベントと待機コールバック関数の結びつけ。
			if (!(pConnectSocket->ptpwaitOnEvSocket = CreateThreadpoolWait(OnEvSocketCB, pConnectSocket, &*pcbe)))
				return;
			//待機コールバック開始。
			SetThreadpoolWait(pConnectSocket->ptpwaitOnEvSocket, pConnectSocket->hEvent, NULL);
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
				std::cerr << "Socket Err: " << Err << "FILE NAME: " << __FILE__ << "LINE: " << __LINE__ << std::endl;
			}
			return;
		}

		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			pSocket->readlock.acquire();
			std::string str(BUFFER_SIZE, '\0');
			str.resize(recv(pSocket->hSocket, str.data(), str.size(), 0));
			MyTRACE(("SevID:" + std::to_string(pSocket->ID)+" Received: "+str).c_str());
			if (str.size()==SOCKET_ERROR)
			{
				str.clear();
				Err = WSAGetLastError();
				std::cerr << "Socket Err: " << Err << "FILE NAME: " << __FILE__ << " LINE: " << __LINE__ << std::endl;
				pSocket->readlock.release();
				return;
			}
			else if (str.size() == 0)
			{
				pSocket->readlock.release();
				return;
			}
			else {
				str = pSocket->ReadString + str;
				std::vector<std::string> v = SplitLineBreak(str);
				for (std::string& s : v) {
					s += "\r\n";
					MyTRACE(("SevID:" + std::to_string(pSocket->ID) + " Send    : " + s).c_str());
					send(pSocket->hSocket, s.data(), s.length(), 0);
				}
				pSocket->ReadString = str;
				pSocket->readlock.release();
			}
		}

		if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			pSocket->vstr.push_back(("SevID:"+std::to_string(pSocket->ID)+" Peer Closed.\r\n").c_str());
			SockTRACE(pSocket);
			//接続数が０になるとステータスを表示。
			if (!(gID - (++gCDel) - 1))
			{
				ShowStatus();
			}
			return;
		}

		if (NetworkEvents.lNetworkEvents & FD_CONNECT)
		{
			pSocket->vstr.push_back(("SevID: " + std::to_string(pSocket->ID) +" Connected.\r\n").c_str());
			SockTRACE(pSocket);

			return;
		}

		//接続ソケット待機イベント再設定。
		SetThreadpoolWait(Wait, pSocket->hEvent, NULL);

	}

	VOID SerializedSocketPrintCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		static std::binary_semaphore lock(1);
		lock.acquire();
		SocketContext* pSocket = (SocketContext*)Context;
		pSocket->vstrlock.acquire();
		for (std::string& str : pSocket->vstr)
		{
			std::cout << str<<std::flush;
		}
		pSocket->vstr.clear();
		pSocket->vstrlock.release();
		lock.release();
		CloseThreadpoolWork(Work);
		return VOID();
	}

	VOID SerializedSocketDebugPrintCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		static std::binary_semaphore lock(1);
		lock.acquire();
		SocketContext* pSocket = (SocketContext*)Context;
		pSocket->vstrlock.acquire();
		for (std::string& str : pSocket->vstr)
		{
			OutputDebugStringA(str.c_str());
		}
		pSocket->vstr.clear();
		pSocket->vstrlock.release();
		lock.release();
		CloseThreadpoolWork(Work);
		return VOID();
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
		gpListenSocket = &gSockets[gID];
		//デバック用にIDをつける。リッスンソケットIDは0。
		gpListenSocket->ID = gID++;

		//ソケット作成
		WSAPROTOCOL_INFOA prot_info{};
		gpListenSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (!gpListenSocket->hSocket)
		{
			return S_FALSE;
		}

		//ソケットリユースオプション
		BOOL yes = 1;
		if (setsockopt(gpListenSocket->hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)))
		{
			++gCDel;
			std::cerr << "setsockopt Error! Line:" << __LINE__ << "\r\n";
		}

		//ホストバインド設定
		CHAR strHostAddr[] = "127.0.0.2";
		u_short usHostPort = 50000;
		DWORD Err = 0;
		struct sockaddr_in addr = { };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(usHostPort);
		int addr_size = sizeof(addr.sin_addr);
		int rVal = inet_pton(AF_INET, strHostAddr, &(addr.sin_addr));
		if (rVal != 1)
		{
			if (rVal == 0)
			{
				fprintf(stderr, "socket error:Listen inet_pton return val 0\n");
				return false;
			}
			else if (rVal == -1)
			{
				Err = WSAGetLastError();
				return false;
			}
		}
		rVal = ::bind(gpListenSocket->hSocket, (struct sockaddr*)&(addr), sizeof(addr));
		if (rVal == SOCKET_ERROR)
		{
			Err = WSAGetLastError();
			return false;
		}

		//イベント設定
		if (WSAEventSelect(gpListenSocket->hSocket, gpListenSocket->hEvent, FD_ACCEPT/* | FD_CLOSE | FD_READ | FD_CONNECT | FD_WRITE*/))
			return 1;

		//イベントハンドラ設定
		if (!(gpListenSocket->ptpwaitOnEvListen = CreateThreadpoolWait(OnEvListenCB, gpListenSocket, &*pcbe)))
			return 1;
		SetThreadpoolWait(gpListenSocket->ptpwaitOnEvListen, gpListenSocket->hEvent, NULL);
		std::cout << "Listen Start\r\n";

		//リッスン
		if (listen(gpListenSocket->hSocket, SOMAXCONN))
			return 1;

		// Accepted/sec測定用タイマーコールバック設定
		if (!(gpTPTimer = CreateThreadpoolTimer(MeasureConnectedPerSecCB, &gAcceptedPerSec, &*pcbe)))
		{
			std::cerr << "err:CreateThreadpoolTimer" << __FILE__ << __LINE__ << "\r\n";
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
		std::cout << "Total Connected: " << ThreadPoolServerR::gID - 1 << "\r" << std::endl;
		std::cout << "Current Connected: " << gID - gCDel - 1 << "\r" << std::endl;
		std::cout << "Max Connecting: " << gMaxConnect << "\r" << std::endl;
		std::cout << "Max Accepted/Sec: " << gAcceptedPerSec << "\r" << std::endl;
	}

	std::vector<std::string> SplitLineBreak(std::string& str)
	{
		std::vector<std::string> v;
		for (;;)
		{
			std::string::size_type pos = str.find("\n");
			if (pos != std::string::npos)
			{
				std::string s = str.substr(0, pos);
				if (*(s.end() - 1) == '\r')
				{
					s.resize(s.size() - 1);
				}
				v.push_back(s);
				str.erase(str.begin(), str.begin() + pos + 1);
			}
			else {
				break;
			}
		}
		return v;
	}

	void SerializedPrint(ThreadPoolServerR::SocketContext * pSocket)
	{
		PTP_WORK ptpwork(NULL);
		if (!(ptpwork = CreateThreadpoolWork(SerializedSocketPrintCB, pSocket, &*pcbe)))
		{
			std::cerr << "Err" << __FUNCTION__ << __LINE__ << std::endl;
			return;
		}
		SubmitThreadpoolWork(ptpwork);
	}

	FILETIME* Make1000mSecFileTime(FILETIME * pFiletime)
	{
		ULARGE_INTEGER ulDueTime;
		ulDueTime.QuadPart = (ULONGLONG)-(1 * 10 * 1000 * 1000);
		pFiletime->dwHighDateTime = ulDueTime.HighPart;
		pFiletime->dwLowDateTime = ulDueTime.LowPart;
		return pFiletime;
	}

	void SerializedSocketDebugPrint(ThreadPoolServerR::SocketContext* pSocket)
	{
		PTP_WORK ptpwork(NULL);
		if (!(ptpwork = CreateThreadpoolWork(SerializedSocketDebugPrintCB
			, pSocket, &*pcbe)))
		{
			std::cerr << "Err" << __FUNCTION__ << __LINE__ << std::endl;
			return;
		}
		SubmitThreadpoolWork(ptpwork);
	}

}