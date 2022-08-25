//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "CallbacksDelay.h"

using namespace std;
using namespace SevDelay;
namespace SevDelay {

	extern SocketListenContext* gpListenContext;
	std::atomic_uint gAcceptedPerSec(0);
	std::atomic_uint gID(0);
	std::atomic_uint gTotalConnected(0);
	std::atomic_uint gCDel(0);
	std::atomic_uint gMaxConnecting(0);

	SocketContext gSockets[ELM_SIZE];
	PTP_TIMER gpTPTimer(NULL);

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
		< FILETIME
		, void (*)(FILETIME*)
		> gp100msecFT
	{	[]()
		{
			const auto gp100msecFT = new FILETIME;
			ULARGE_INTEGER ulDueTime;
			ulDueTime.QuadPart = (ULONGLONG)-(1 * 10 * 1000 * 100);
			gp100msecFT->dwHighDateTime = ulDueTime.HighPart;
			gp100msecFT->dwLowDateTime = ulDueTime.LowPart;
			return gp100msecFT;
		}()
		,[](_Inout_ FILETIME* gp100msecFT)
		{
			delete gp100msecFT;
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

	VOID OnListenCompCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
	{
		//リッスンソケット
		SocketListenContext* pListenSocket = (SocketListenContext*)Context;
		//アクセプトソケット
		SocketContext* pSocket = (SocketContext*)Overlapped;

		//エラー確認
		if (IoResult)
		{
			MyTRACE(("Err! DelayEchoSev OnListenCompCB IoResult:" + to_string(IoResult) + " Line:" + to_string(__LINE__) + "\r\n").c_str());
			cout << "End Listen\r\n";
			return;
		}
		++gTotalConnected;
		gMaxConnecting.store(max(gMaxConnecting.load(), (gTotalConnected.load() - gCDel.load())));

		if (!NumberOfBytesTransferred)
		{
			MyTRACE(("DelayEchoSev OnListenCompCB SocketID:" + to_string(pSocket->ID) + " Closed.\r\n").c_str());
			CleanupSocket(pSocket);
			//次のアクセプト
			if (!PreAccept(pListenSocket))
			{
				cerr << "Err! OnListenCompCB. PreAccept.LINE:" << __LINE__ << "\r\n";
				return;
			}
			return;
		}

		pListenSocket->ReadBuf.resize(NumberOfBytesTransferred);

		//アクセプトIO完了ポート設定
		
		if (!(pSocket->pTPIo = CreateThreadpoolIo((HANDLE)pSocket->hSocket, OnSocketNoticeCompCB, pSocket, &*pcbe)))
		{
			DWORD Err = GetLastError();
			cerr << "Err! OnListenCompCB. CreateThreadpoolIo. CODE:" << to_string(Err) << "\r\n";
			CleanupSocket(pSocket);
			return;
		}

		//読み取りデータをアクセプトソケットに移す。
		pSocket->RemBuf = pListenSocket->ReadBuf;

		//次のリッスン完了ポートスタート
		StartThreadpoolIo(Io);
		if (!PreAccept(pListenSocket))
		{
			cerr << "Err! OnListenCompCB. PreAccept.LINE:" << __LINE__ << "\r\n";
			return;
		}

		//改行で分ける。
		pSocket->WriteBuf = SplitLastLineBreak(pSocket->RemBuf);
		//送信できるものがあれば送信
		if (pSocket->WriteBuf.size())
		{
			pSocket->WriteBuf += "\r\n";
			//ディレイタイマーコールバック設定
			TP_TIMER* pTPTimer(NULL);
			if (!(pTPTimer = CreateThreadpoolTimer(DelaySendTimerCB, pSocket, &*pcbe)))
			{
				std::cerr << "err:CreateThreadpoolTimer. Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
				CleanupSocket(pSocket);
				return;
			}
			SetThreadpoolTimer(pTPTimer, &*gp100msecFT, 0, 0);
		}
		//でなければ、受信体制
		else {
			TP_WORK* pTPWork(NULL);
			if (!(pTPWork = CreateThreadpoolWork(RecvWorkCB, pSocket, &*pcbe)))
			{
				DWORD Err = GetLastError();
				cerr << "Err! OnSocketNoticeCompCB CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				CleanupSocket(pSocket);
				return;
			}
			SubmitThreadpoolWork(pTPWork);
		}
		return;
	}

	VOID OnSocketNoticeCompCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
	{
//		MyTRACE("Enter OnSocketNoticeCompCB\r\n");
		SocketContext* pSocket = (SocketContext*)Context;
		if (pSocket->fReEnterGuard)
		{
			StartThreadpoolIo(Io);
			MyTRACE("ReEnter OnSocketNoticeCompCB\r\n");
			return;
		}
		pSocket->fReEnterGuard = TRUE;

		//エラー確認
		if (IoResult)
		{
			MyTRACE(("Err! DelayEchoSev OnSocketNoticeCompCB. Code:" + to_string(IoResult) + " Line:" + to_string(__LINE__) + " SocketID:" + to_string(pSocket->ID) + "\r\n").c_str());
			CleanupSocket(pSocket);
			pSocket->fReEnterGuard = FALSE;
			return;
		}

		//切断か確認。
		if (!NumberOfBytesTransferred)
		{
			MyTRACE(("DelayEchoSev Socket ID:" + to_string(pSocket->ID) + " Closed\r\n").c_str());
			CleanupSocket(pSocket);
			pSocket->fReEnterGuard = FALSE;
			return;
		}

			//レシーブの完了か確認。
		if (pSocket->Dir == SocketContext::eDir::OL_RECV)
		{
			pSocket->RemBuf += {pSocket->wsaReadBuf.buf, NumberOfBytesTransferred};
			//最後の改行からをWriteBufに入れる。
			pSocket->WriteBuf = SplitLastLineBreak(pSocket->RemBuf);

			//WriteBufに中身があれば、エコー送信の開始
			if (pSocket->WriteBuf.size())
			{
				pSocket->WriteBuf += "\r\n";
				//ディレイコールバック設定
				TP_TIMER* pTPTimer(NULL);
				if (!(pTPTimer = CreateThreadpoolTimer(DelaySendTimerCB, pSocket, &*pcbe)))
				{
					std::cerr << "err:DelayEchoSev CreateThreadpoolTimer. Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
					CleanupSocket(pSocket);
					pSocket->fReEnterGuard = FALSE;
					return;
				}
				SetThreadpoolTimer(pTPTimer, &*gp100msecFT, 0, 0);

				//更に受信体制にする。
				TP_WORK* pTPWork(NULL);
				if (!(pTPWork = CreateThreadpoolWork(RecvWorkCB, pSocket, &*pcbe)))
				{
					DWORD Err = GetLastError();
					cerr << "Err! DelayEchoSev OnSocketNoticeCompCB CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
					CleanupSocket(pSocket);
					pSocket->fReEnterGuard = FALSE;
					return;
				}
				SubmitThreadpoolWork(pTPWork);

			}
			else{
				//WriteBufに中身がない場合送信はしない。受信完了ポートスタート。
				TP_WORK* pTPWork(NULL);
				if (!(pTPWork = CreateThreadpoolWork(RecvWorkCB, pSocket, &*pcbe)))
				{
					DWORD Err = GetLastError();
					cerr << "Err! DelayEchoSev OnSocketNoticeCompCB CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
					CleanupSocket(pSocket);
					pSocket->fReEnterGuard = FALSE;
					return;
				}
				SubmitThreadpoolWork(pTPWork);
			}
		}
		//送信完了の場合
		else if (pSocket->Dir == SocketContext::eDir::OL_SEND)
		{
			MyTRACE(("DelayEchoSev Sent:" + pSocket->WriteBuf).c_str());
			pSocket->WriteBuf.clear();
			//受信体制
			TP_WORK* pTPWork(NULL);
			if (!(pTPWork = CreateThreadpoolWork(RecvWorkCB, pSocket, &*pcbe)))
			{
				DWORD Err = GetLastError();
				cerr << "Err! DelayEchoSev OnSocketNoticeCompCB CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				CleanupSocket(pSocket);
				pSocket->fReEnterGuard = FALSE;
				return;
			}
			SubmitThreadpoolWork(pTPWork);
		}
		else {
			int i = 0;//通常ここには来ない。
		}
		pSocket->fReEnterGuard = FALSE;
		return;
	}


	VOID SendWorkCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		CloseThreadpoolWork(Work);
		SocketContext* pSocket = (SocketContext*)Context;
		if (pSocket->pTPIo)
		{
			StartThreadpoolIo(pSocket->pTPIo);
		}
		else {
			cout << "Err!pSocket is empty.\r\n";
		}
		pSocket->Dir = SocketContext::eDir::OL_SEND;
		pSocket->StrToWsa(&pSocket->WriteBuf, &pSocket->wsaWriteBuf);
		if (WSASend(pSocket->hSocket, &pSocket->wsaWriteBuf, 1, NULL, 0, pSocket, NULL))
		{
			DWORD Err= WSAGetLastError();
			if (Err != WSA_IO_PENDING && Err) {
				cerr << "Err! WSASend.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				CleanupSocket(pSocket);
				return;
			}
		}
		return;
	}

	VOID RecvWorkCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		CloseThreadpoolWork(Work);
		SocketContext* pSocket = (SocketContext*)Context;
		if (!pSocket->hSocket)
		{
			return;
		}
		//受信体制を取る。

		pSocket->Dir = SocketContext::eDir::OL_RECV;
		pSocket->ReadBuf.resize(BUFFER_SIZE, '\0');
		pSocket->StrToWsa(&pSocket->ReadBuf, &pSocket->wsaReadBuf);

		StartThreadpoolIo(pSocket->pTPIo);
		if (!WSARecv(pSocket->hSocket, &pSocket->wsaReadBuf, 1, NULL, &pSocket->flags, pSocket, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (Err != WSA_IO_PENDING && Err)
			{
				cerr << "Err! RecvWorkCB.Code:" << to_string(Err) << " LINE:"<<__LINE__<<"\r\n";
				CleanupSocket(pSocket);
				return;
			}
		}
	}

	VOID DelaySendTimerCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
	{
		CloseThreadpoolTimer(Timer);
		TP_WORK* pTPWork(NULL);
		SocketContext* pSocket = (SocketContext*)Context;
		if (!(pTPWork = CreateThreadpoolWork(SendWorkCB, pSocket, &*pcbe)))
		{
			DWORD Err = GetLastError();
			cerr << "Err! DelayEchoSev DelaySendTimerCB CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
			CleanupSocket(pSocket);
			return;
		}
		SubmitThreadpoolWork(pTPWork);
		return;
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
		static std::binary_semaphore sem(1);
		sem.acquire();
		//タイミングによって、何回も呼ばれる。
		if (pSocket->hSocket)
		{
			MyTRACE(("CleanupSocket SocketID:" + to_string(pSocket->ID) + "\r\n").c_str());
			++gCDel;
		}
		else {
			sem.release();
			return;
		}
		if (pSocket->pTPTimer)
		{
			WaitForThreadpoolTimerCallbacks(pSocket->pTPTimer, FALSE);
			pSocket->pTPTimer = NULL;
		}
		pSocket->ReInitialize();
		gSocketsPool.Push(pSocket);
		if (!(gTotalConnected.load() - gCDel.load()))
		{
			ShowStatus();
		}
		sem.release();
	}


	int StartListen(SocketListenContext* pListenContext)
	{
		cout << "Start Listen\r\n";
		cout << "Backend DB\r\n";
		cout << HOST_FRONT_LISTEN_BASE_ADDR << ":" << HOST_FRONT_LISTEN_PORT << "\r\n";

		//デバック用にIDをつける。リッスンソケットIDは0。
		pListenContext->ID = gID++;

		//ソケット作成
		WSAPROTOCOL_INFOA prot_info{};
		pListenContext->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (!pListenContext->hSocket)
		{
			return S_FALSE;
		}

		//ソケットリユースオプション
		BOOL yes = 1;
		if (setsockopt(pListenContext->hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)))
		{
			std::cerr << "setsockopt Error! Line:" << __LINE__ << "\r\n";
		}

		//ホストバインド設定
		DWORD Err = 0;
		struct sockaddr_in addr = { };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(HOST_FRONT_LISTEN_PORT);
		int rVal = inet_pton(AF_INET, HOST_FRONT_LISTEN_BASE_ADDR, &(addr.sin_addr));
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

		if (!PreAccept(pListenContext))
		{
			cerr << "Err! StartListen.PreAccept. LINE:" << __LINE__ << "\r\n";
			return false;
		}

		//リッスンソケット完了ポート作成
		if (!(pListenContext->pTPListen = CreateThreadpoolIo((HANDLE)pListenContext->hSocket,OnListenCompCB, pListenContext, &*pcbe))) {
			cerr << "Err! CreateThreadpoolIo. LINE:" << __LINE__ << "\r\n";
			return false;
		}
		StartThreadpoolIo(pListenContext->pTPListen);
		return 	true;
	}

	LPFN_ACCEPTEX GetAcceptEx(SocketListenContext* pListenSocket)
	{
		GUID GuidAcceptEx = WSAID_ACCEPTEX;
		int iResult = 0;
		static LPFN_ACCEPTEX lpfnAcceptEx(NULL);
		static SOCKET hSocket(NULL);
		DWORD dwBytes;
		if (!lpfnAcceptEx || hSocket != pListenSocket->hSocket)
		{
			try {
				if (WSAIoctl(pListenSocket->hSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
					&GuidAcceptEx, sizeof(GuidAcceptEx),
					&lpfnAcceptEx, sizeof(lpfnAcceptEx),
					&dwBytes, NULL, NULL) == SOCKET_ERROR)
				{
					throw std::runtime_error("Create'GetAcceptEx'error! Code:" + to_string(WSAGetLastError()) + " LINE:" + to_string(__LINE__) + "\r\n");
				}
			}
			catch (std::exception& e) {
				// 例外を捕捉
				// エラー理由を出力する
				std::cerr << e.what() << std::endl;
			}
		}
		hSocket = pListenSocket->hSocket;
		return lpfnAcceptEx;
	}

	LPFN_GETACCEPTEXSOCKADDRS GetGetAcceptExSockaddrs(SocketContext* pSocket)
	{
		GUID GuidAcceptEx = WSAID_GETACCEPTEXSOCKADDRS;
		int iResult = 0;
		static LPFN_GETACCEPTEXSOCKADDRS lpfnAcceptEx(NULL);
		static SOCKET hSocket(NULL);
		DWORD dwBytes;
		if (!lpfnAcceptEx || hSocket != pSocket->hSocket)
		{
			try {
				if (WSAIoctl(pSocket->hSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
					&GuidAcceptEx, sizeof(GuidAcceptEx),
					&lpfnAcceptEx, sizeof(lpfnAcceptEx),
					&dwBytes, NULL, NULL) == SOCKET_ERROR)
				{
					throw std::runtime_error("Create'GetAcceptEx'error! Code:" + to_string(WSAGetLastError()) + " LINE:" + to_string(__LINE__) + "File:" + __FILE__ + "\r\n");
				}
			}
			catch (std::exception& e) {
				// 例外を捕捉
				// エラー理由を出力する
				std::cerr << e.what() << std::endl;
			}
		}
		hSocket = pSocket->hSocket;
		return lpfnAcceptEx;
	}

	void EndListen(SocketListenContext* pListen)
	{
		WaitForThreadpoolTimerCallbacks(gpTPTimer, FALSE);
		CloseThreadpoolTimer(gpTPTimer);
	}

	void ShowStatus()
	{
		std::cout << "Total Connected: " << gTotalConnected << "\r\n";
		std::cout << "Current Connecting: " << gTotalConnected - gCDel <<"\r\n";
		std::cout << "Max Connected: " << gMaxConnecting << "\r\n";
		std::cout << "Max Accepted/Sec: " << gAcceptedPerSec << "\r\n";
		cout<<"IP Address: "<< HOST_FRONT_LISTEN_BASE_ADDR << ":" << HOST_FRONT_LISTEN_PORT << "\r\n\r\n";

	}

	void ClearStatus()
	{
		gCDel = 0;
		gTotalConnected = 0;
		gMaxConnecting = 0;
		gAcceptedPerSec = 0;
		ShowStatus();
	}

	void Cls()
	{
		cout << "\x1b[2J";
		cout << "\x1b[0;0H";
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

	bool PreAccept(SocketListenContext* pListenSocket)
	{

		//アクセプト用ソケット取り出し
		SocketContext* pAcceptSocket = gSocketsPool.Pull();
		//オープンソケット作成
		if (!(pAcceptSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
			cerr << "Err WSASocket Code:" << to_string(WSAGetLastError()) << " Line:" << __LINE__ << "\r\n";
			return false;
		}

		//デバック用ID設定
		pAcceptSocket->ID = gID++;

		//バッファー初期化
		pListenSocket->ReadBuf.resize(BUFFER_SIZE, '\0');

		//AcceptExの関数ポインタを取得し、実行。パラメーターはサンプルまんま。
		if (!(*GetAcceptEx(pListenSocket))(pListenSocket->hSocket, pAcceptSocket->hSocket, pListenSocket->ReadBuf.data(), BUFFER_SIZE - ((sizeof(sockaddr_in) + 16) * 2), sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, (OVERLAPPED*)pAcceptSocket))
		{
			DWORD err = GetLastError();
			if (err != ERROR_IO_PENDING)
			{
				cerr << "AcceptEx return value error! Code:" + to_string(err) + " LINE:" + to_string(__LINE__) + "\r\n";
				return false;
			}
		}
		return true;
	}

}