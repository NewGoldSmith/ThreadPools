//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "CallbacksPooll.h"

using namespace std;
using namespace SevPooll;
namespace SevPooll {

	std::atomic_uint gAcceptedPerSec(0);
	std::atomic_uint gID(0);
	std::atomic_uint gTotalConnected(0);
	std::atomic_uint gCDel(0);
	std::atomic_uint gMaxConnecting(0);
	SocketContext gSockets[ELM_SIZE];
	PTP_TIMER gpTPTimer(NULL);
	u_int gfEnd(0);

	binary_semaphore gvPoollock(1);
	binary_semaphore gvConnectedlock(1);

	RingBuf<SocketContext> gSocketsPool(gSockets, ELM_SIZE);

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
			[gp1000msecFT]()
			{
				ULARGE_INTEGER ulDueTime;
				ulDueTime.QuadPart = (ULONGLONG)-(1 * 10 * 1000 * 1000);
				gp1000msecFT->dwHighDateTime = ulDueTime.HighPart;
				gp1000msecFT->dwLowDateTime = ulDueTime.LowPart;
				return gp1000msecFT;
			}();
			return gp1000msecFT;
		}()
	,[](_Inout_ FILETIME* gp1000msecFT)
		{
				delete gp1000msecFT;
		}
	};

	const std::unique_ptr
		< FILETIME
		, void (*)(FILETIME*)
		> gp100msecFT
	{ []()
		{
			const auto gp100msecFT = new FILETIME;
			[gp100msecFT]()
				{
					ULARGE_INTEGER ulDueTime;
					ulDueTime.QuadPart = (ULONGLONG)-(1 * 10 * 1000 * 100);
					gp100msecFT->dwHighDateTime = ulDueTime.HighPart;
					gp100msecFT->dwLowDateTime = ulDueTime.LowPart;
					return gp100msecFT;
			}();
			return gp100msecFT;
		}()
	,[](_Inout_ FILETIME* gp100msecFT)
		{
				delete gp100msecFT;
		}
	};

	const std::unique_ptr
		< FILETIME
		, void (*)(FILETIME*)
		> gp10msecFT
	{ []()
		{
			const auto gp10msecFT = new FILETIME;
			[gp10msecFT]()
				{
					ULARGE_INTEGER ulDueTime;
					ulDueTime.QuadPart = (ULONGLONG)-(1 * 10 * 1000 * 10);
					gp10msecFT->dwHighDateTime = ulDueTime.HighPart;
					gp10msecFT->dwLowDateTime = ulDueTime.LowPart;
					return gp10msecFT;
			}();
			return gp10msecFT;
		}()
	,[](_Inout_ FILETIME* gp10msecFT)
		{
				delete gp10msecFT;
		}
	};


	VOID TryAcceptTimerCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
	{
		SocketContext* pListen = (SocketContext*)Context;
		//エンドフラグが立っていたら終了。
		if (gfEnd)
		{
			SetThreadpoolTimer(Timer, NULL, 0, 0);
			CloseThreadpoolTimer(Timer);
			return;
		}
		SOCKET hSocket(NULL);
		if ((hSocket = accept(pListen->hSocket, NULL, NULL)) == INVALID_SOCKET)
		{
			DWORD Err = WSAGetLastError();
			if (!(Err == WSAEALREADY || Err== WSAEWOULDBLOCK))
			{
				cerr << "Err! TryAcceptTimerCB. accept. Code:" << Err << " LINE:" << __LINE__ << "\r\n";
				SetThreadpoolTimer(Timer, NULL, 0, 0);
				CloseThreadpoolTimer(Timer);
				return;
			}
			SetThreadpoolTimer(Timer, &*gp10msecFT, 0, 0);
			return;
		}
		else {
			//接続あり
			++gTotalConnected;
			gMaxConnecting.store(max(gMaxConnecting.load(), (gTotalConnected.load() - gCDel.load())));

			SocketContext* pSocket = gSocketsPool.Pull();
			pSocket->hSocket = hSocket;

			//ノンブロッキングモードに変更
			u_long flag = 1;
			ioctlsocket(pSocket->hSocket, FIONBIO, &flag);

			TP_TIMER* pTPTimer(NULL);
			if (!(pTPTimer = CreateThreadpoolTimer(RecvAndSendTimerCB, pSocket, &*pcbe)))
			{
				DWORD Err = GetLastError();
				cerr << "Err! TryAcceptTimerCB. CreateThreadpoolTimer. Code:" << Err << " LINE:" << __LINE__ << "\r\n";
				SetThreadpoolTimer(Timer, NULL, 0, 0);
				CloseThreadpoolTimer(Timer);
				CleanupSocket(pSocket);
				return;
			}
			SetThreadpoolTimer(pTPTimer, &*gp10msecFT, 0, 0);
		}
		SetThreadpoolTimer(Timer, &*gp10msecFT, 0, 0);
		return VOID();
	}

	VOID RecvAndSendTimerCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
	{
		SocketContext* pSocket = (SocketContext*)Context;
		if (gfEnd)
		{
			SetThreadpoolTimer(Timer, NULL, 0, 0);
			CloseThreadpoolTimer(Timer);
			CleanupSocket(pSocket);
			return;
		}
		pSocket->ReadBuf.resize(BUFFER_SIZE,'\0');
		int result(NULL);
		if((result=recv(pSocket->hSocket, pSocket->ReadBuf.data(), pSocket->ReadBuf.length(), 0))==SOCKET_ERROR)
		{
			DWORD Err = WSAGetLastError();
			if (Err != WSAEWOULDBLOCK)
			{
				cerr << "Err! RecvAndSendTimerCB. recv. Code:" << Err << " LINE:" << __LINE__ << "\r\n";
				SetThreadpoolTimer(Timer, NULL, 0, 0);
				CloseThreadpoolTimer(Timer);
				CleanupSocket(pSocket);
				return;
			}
			else {
				SetThreadpoolTimer(Timer, &*gp10msecFT, 0, 0);
				return;
			}
		}
		pSocket->ReadBuf.resize(result);
		if (pSocket->ReadBuf.size() == 0)
		{
			MyTRACE("Socket Close\r\n");
			SetThreadpoolTimer(Timer, NULL, 0, 0);
			CloseThreadpoolTimer(Timer);
			CleanupSocket(pSocket);
			return;
		}
		pSocket->RemBuf += pSocket->ReadBuf;
		pSocket->WriteBuf = SplitLastLineBreak(pSocket->RemBuf);
		if (pSocket->WriteBuf.size() > 0)
		{
			//センド
			int len(0);
			pSocket->WriteBuf += "\r\n";
			if((len=send(pSocket->hSocket, pSocket->WriteBuf.data(), pSocket->WriteBuf.size(), 0))== SOCKET_ERROR)
			{
				DWORD Err = WSAGetLastError();
				cerr << "Err! RecvAndSendTimerCB. send. Code:" << Err << " LINE:" << __LINE__ << "\r\n";
				SetThreadpoolTimer(Timer, NULL, 0, 0);
				CloseThreadpoolTimer(Timer);
				CleanupSocket(pSocket);
				return;
			}
		}
		SetThreadpoolTimer(Timer, &*gp10msecFT, 0, 0);
		return VOID();
	}

	VOID MeasureConnectedPerSecCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
	{
		static u_int oldNum(0);
		u_int nowNum(gTotalConnected);
		if (nowNum >= 1)
		{
			std::atomic_uint* pgConnectPerSec = (std::atomic_uint*)Context;
			*pgConnectPerSec = __max(pgConnectPerSec->load(), nowNum - oldNum);
		}
		oldNum = nowNum;
	}

	void CleanupSocket(SocketContext* pSocket)
	{
		pSocket->ReInitialize();
		gSocketsPool.Push(pSocket);
		++gCDel;
		if (!(gTotalConnected - gCDel))
		{
			ShowStatus();
		}
	}


	int StartListen(SocketContext* pListenContext)
	{
		cout << "Start Listen\r\n";
		//デバック用にIDをつける。リッスンソケットIDは0。
		pListenContext->ID = gID++;

		//ソケット作成
		WSAPROTOCOL_INFOA prot_info{};
		pListenContext->hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (!pListenContext->hSocket)
		{
			return S_FALSE;
		}

		//ソケットリユースオプション
		BOOL yes = 1;
		if (setsockopt(pListenContext->hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)))
		{
			++gCDel;
			std::cerr << "setsockopt Error! Line:" << __LINE__ << "\r\n";
		}

		//ノンブロッキングモードに変更
		u_long flag=1;
		ioctlsocket(pListenContext->hSocket, FIONBIO, &flag);

		//
		//ホストバインド設定
		CHAR strHostAddr[] = "127.0.0.2";
		u_short usHostPort = 50000;
		DWORD Err = 0;
		struct sockaddr_in addr = { };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(usHostPort);
		int rVal = inet_pton(AF_INET, strHostAddr, &(addr.sin_addr));
		if (rVal != 1)
		{
			if (rVal == 0)
			{
				cerr << "error:inet_pton return val 0\r\n";
				return false;
			}
			else if (rVal == -1)
			{
				cerr << "Err inet_pton return val 0 Code:" << WSAGetLastError() << " \r\n";
				return false;
			}
		}
		if ((rVal = ::bind(pListenContext->hSocket, (struct sockaddr*)&(addr), sizeof(addr)) == SOCKET_ERROR))
		{
			cerr << "Err ::bind Code:" << WSAGetLastError() << " Line:" << __LINE__ << "\r\n";
			return false;
		}

		//リッスン
		if (listen(pListenContext->hSocket, SOMAXCONN)) {
			cerr << "Err listen Code:" << to_string(WSAGetLastError()) << " Line:" << __LINE__ << "\r\n";
			return false;
		}

		// Accepted/sec測定用タイマーコールバック設定
		if (!(gpTPTimer = CreateThreadpoolTimer(MeasureConnectedPerSecCB, &gAcceptedPerSec, &*pcbe)))
		{
			std::cerr << "err:CreateThreadpoolTimer. Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
			return false;
		}
		SetThreadpoolTimer(gpTPTimer, &*gp1000msecFT, 1000, 0);

		//アクセプトpoolling
		TP_TIMER* pTPTimer(NULL);
		if (!(pTPTimer = CreateThreadpoolTimer(TryAcceptTimerCB, pListenContext, &*pcbe)))
		{
			std::cerr << "err:CreateThreadpoolTimer. Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
			return false;
		}
		SetThreadpoolTimer(pTPTimer, &*gp10msecFT, 0, 0);
	}

	void EndListen(SocketContext* pListen)
	{
		if (gpTPTimer)
		{
			SetThreadpoolTimer(gpTPTimer, NULL, 0, 0);
			CloseThreadpoolTimer(gpTPTimer);
			gpTPTimer = NULL;
		}
		gfEnd = TRUE;

	}

	void ShowStatus()
	{
		std::cout << "Total Connected: " << gTotalConnected << "\r\n";
		std::cout << "Current Connecting: " << gTotalConnected - gCDel << "\r\n";
		std::cout << "Max Connectting: " << gMaxConnecting << "\r\n";
		std::cout << "Max Accepted/Sec: " << gAcceptedPerSec << "\r\n\r\n";
	}

	void ClearStatus()
	{
		gCDel = 0;
		gTotalConnected = 0;
		gMaxConnecting = 0;
		gAcceptedPerSec = 0;
		ShowStatus();
	}

	std::string SplitLastLineBreak(std::string& str)
	{
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