//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "CallbacksOL.h"

using namespace std;
using namespace SevOL;
namespace SevOL {

	extern SocketListenContext* gpListenContext;
	std::atomic_uint gAcceptedPerSec(0);
	std::atomic_uint gID(0);
	std::atomic_uint gIDBack(0);
	std::atomic_uint gTotalConnected(0);
	std::atomic_uint gCDel(0);
	std::atomic_uint gMaxConnecting(0);
	std::atomic<BOOL> gFrontReEnterGuard(FALSE);
	std::atomic<BOOL> gBackReEnterGuard(FALSE);
	SocketContext gSockets[ELM_SIZE];
	SocketContext gSocketsBack[ELM_SIZE];
	PTP_TIMER gpTPTimer(NULL);
	RingBuf<SocketContext> gSocketsPool(gSockets, ELM_SIZE);
	BOOL TryConnectBackFirstMessage(1);

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

	VOID OnListenCompCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
	{
		//リッスンソケット
		SocketListenContext* pListenSocket = (SocketListenContext*)Context;
		//アクセプトソケット
		SocketContext* pSocket = (SocketContext*)Overlapped;

		//エラー確認
		if (IoResult)
		{
			//アプリケーションにより終了
			if (IoResult == ERROR_OPERATION_ABORTED)
			{
				return;
			}
			MyTRACE(("Err! OnListenCompCB Result:" + to_string(IoResult) + " Line:" + to_string(__LINE__) + "\r\n").c_str());
			cout << "End Listen\r\n";
			return;
		}
		++gTotalConnected;
		gMaxConnecting.store(max(gMaxConnecting.load(), (gTotalConnected.load() - gCDel.load())));

		if (!NumberOfBytesTransferred)
		{
			CleanupSocket(pSocket);
			//次のアクセプト
			if (!PreAccept(pListenSocket))
			{
				cerr << "Err! OnListenCompCB. PreAccept.LINE:" << __LINE__ << "\r\n";
				return;
			}
			return;
		}

		//バックエンドのDBに接続
		if (!TryConnectBack(pSocket))
		{
			cerr << "Err! OnListenCompCB. TryConnectBack.LINE:" << __LINE__ << "\r\n";
			CleanupSocket(pSocket);
			return;
		}

		pListenSocket->ReadBuf.resize(NumberOfBytesTransferred);

		//アクセプトソケットフロント完了ポート設定
		if (!(pSocket->pTPIo = CreateThreadpoolIo((HANDLE)pSocket->hSocket, OnSocketNoticeCompCB, pSocket, &*pcbe)))
		{
			DWORD Err = GetLastError();
			cerr << "Err! OnListenCompCB. CreateThreadpoolIo. CODE:" << to_string(Err) << "\r\n";
			CleanupSocket(pSocket);
			return;
		}
		StartThreadpoolIo(pSocket->pTPIo);

		//バックエンドDB向けIO完了ポート設定
		if (!(pSocket->pTPBackIo = CreateThreadpoolIo((HANDLE)pSocket->hSocketBack, OnSocketBackNoticeCompCB, pSocket, &*pcbe)))
		{
			DWORD Err = GetLastError();
			cerr << "Err! OnListenCompCB. CreateThreadpoolIo. CODE:" << to_string(Err) << "\r\n";
			CleanupSocket(pSocket);
			return;
		}
		//これをしておかないとメモリ破損
		StartThreadpoolIo(pSocket->pTPBackIo);

		//読み取りデータをアクセプトソケットに移す。
		pSocket->RemBuf = pListenSocket->ReadBuf;

		//リッスンの完了ポート再設定
		StartThreadpoolIo(Io);

		//次のアクセプト
		if (!PreAccept(pListenSocket))
		{
			cerr << "Err! OnListenCompCB. PreAccept.LINE:" << __LINE__ << "\r\n";
			return;
		}

		//改行で分ける。
		pSocket->WriteBackBuf = SplitLastLineBreak(pSocket->RemBuf);
		//送信できるものがあれば送信
		if (pSocket->WriteBackBuf.size())
		{
			pSocket->WriteBackBuf += "\r\n";
			TP_WORK* pTPWork(NULL);
			//再帰が面倒くさいのでバックへの送信と、フロントの受信待ちをCBで行う
			if (!(pTPWork = CreateThreadpoolWork(SendBackWorkCB, pSocket, &*pcbe)))
			{
				DWORD Err = GetLastError();
				cerr << "Err! OnListenCompCB. CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				CleanupSocket(pSocket);
				return;
			}
			SubmitThreadpoolWork(pTPWork);

			if (!(pTPWork = CreateThreadpoolWork(RecvWorkCB, pSocket, &*pcbe)))
			{
				DWORD Err = GetLastError();
				cerr << "Err! OnListenCompCB. CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				CleanupSocket(pSocket);
				return;
			}
			SubmitThreadpoolWork(pTPWork);

		//でなければ、フロントからの受信体制
		}else {
		
			TP_WORK* pTPWork(NULL);
			if (!(pTPWork = CreateThreadpoolWork(RecvWorkCB, pSocket, &*pcbe)))
			{
				DWORD Err = GetLastError();
				cerr << "Err! OnListenCompCB. CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				CleanupSocket(pSocket);
				return;
			}
			SubmitThreadpoolWork(pTPWork);
		}
		return;
	}

	VOID OnSocketNoticeCompCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
	{
		MyTRACE("Enter OnSocketNoticeCompCB\r\n");
		int expected = FALSE;
		if (!gFrontReEnterGuard.compare_exchange_strong(expected, TRUE))
		{
			StartThreadpoolIo(Io);
			MyTRACE("ReEnter OnSocketNoticeCompCB");
			return;
		}

		SocketContext* pSocket = (SocketContext*)Context;

		//エラー確認
		if (IoResult)
		{
			//即刻終了
			if (IoResult == ERROR_CONNECTION_ABORTED)
			{
//				CloseThreadpoolIo(Io);
				CleanupSocket(pSocket);
				gFrontReEnterGuard = FALSE;
				return;
			}
			MyTRACE(("Err! OnSocketNoticeCompCB Code:" + to_string(IoResult) + " Line:" + to_string(__LINE__) + "\r\n").c_str());
//			CloseThreadpoolIo(Io);
			CleanupSocket(pSocket);
			gFrontReEnterGuard = FALSE;
			return;
		}

		//切断か確認。
		if (!NumberOfBytesTransferred)
		{
			MyTRACE("Socket Closed\r\n");
//			CloseThreadpoolIo(Io);
			CleanupSocket(pSocket);
			gFrontReEnterGuard = FALSE;
			return;
		}
//		StartThreadpoolIo(Io);

			//レシーブの完了か確認。
		if (pSocket->Dir == SocketContext::eDir::DIR_TO_BACK)
		{
			pSocket->RemBuf += {pSocket->wsaReadBuf.buf, NumberOfBytesTransferred};
			pSocket->ReadBuf.clear();
			//最後の改行からをWriteBackBufに入れる。
			pSocket->WriteBackBuf = SplitLastLineBreak(pSocket->RemBuf);

			//WriteBufに中身があれば、バックへの送信の開始
			if (pSocket->WriteBackBuf.size())
			{
				pSocket->WriteBackBuf += "\r\n";
				TP_WORK* pTPWork(NULL);
				if (!(pTPWork = CreateThreadpoolWork(SendBackWorkCB, pSocket, &*pcbe)))
				{
					DWORD Err = GetLastError();
					cerr << "Err! OnSocketNoticeCompCB CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
					gFrontReEnterGuard = FALSE;
					return;
				}
				SubmitThreadpoolWork(pTPWork);
				//受信の準備
				if (!(pTPWork = CreateThreadpoolWork(RecvWorkCB, pSocket, &*pcbe)))
				{
					DWORD Err = GetLastError();
					cerr << "Err! OnSocketNoticeCompCB CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
					gFrontReEnterGuard = FALSE;
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
					cerr << "Err! OnSocketNoticeCompCB CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
					gFrontReEnterGuard = FALSE;
					return;
				}
				SubmitThreadpoolWork(pTPWork);
			}
			gFrontReEnterGuard = FALSE;
			return;
		}
		//送信完了。
		else if (pSocket->Dir == SocketContext::eDir::DIR_TO_FRONT)
		{
			MyTRACE(("SevOL Front Sent:" + pSocket->WriteBuf).c_str());
			pSocket->WriteBuf.clear();
			//受信完了ポートスタート。
			TP_WORK* pTPWork(NULL);
			if (!(pTPWork = CreateThreadpoolWork(RecvWorkCB, pSocket, &*pcbe)))
			{
				DWORD Err = GetLastError();
				cerr << "Err! OnSocketNoticeCompCB CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				gFrontReEnterGuard = FALSE;
				return;
			}
			SubmitThreadpoolWork(pTPWork);
		}
		else {
			int i = 0;//通常ここには来ない。
		}
		gFrontReEnterGuard = FALSE;
		return;
	}

	VOID OnSocketBackNoticeCompCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
	{
		MyTRACE("Enter OnSocketBackNoticeCompCB\r\n");
		int expected = FALSE;
		if (!gBackReEnterGuard.compare_exchange_strong(expected, TRUE))
		{
			StartThreadpoolIo(Io);
			MyTRACE("ReEnter OnSocketNoticeCompCB");
			return;
		}

		SocketContext* pSocket = (SocketContext*)Context;
		//エラー確認
		if (IoResult)
		{
			//即刻終了
			if (IoResult == ERROR_CONNECTION_ABORTED)
			{
//				CloseThreadpoolIo(Io);
				CleanupSocket(pSocket);
				gBackReEnterGuard = FALSE;
				return;
			}
			MyTRACE(("Err! OnSocketBackNoticeCompCB Code:" + to_string(IoResult) + " Line:" + to_string(__LINE__) + "\r\n").c_str());
//			CloseThreadpoolIo(Io);
			CleanupSocket(pSocket);
			gBackReEnterGuard = FALSE;
			return;
		}

		//切断か確認。
		if (!NumberOfBytesTransferred)
		{
			MyTRACE("Socket Closed\r\n");
//			CloseThreadpoolIo(Io);
			CleanupSocket(pSocket);
			gBackReEnterGuard = FALSE;
			return;
		}

//		StartThreadpoolIo(Io);

		//センドの完了か確認。
		if (pSocket->DirBack == SocketContext::eDir::DIR_TO_BACK)
		{
			//バックに送信完了。
			MyTRACE(("SevOL Back Sent:" + pSocket->WriteBackBuf).c_str());
			pSocket->WriteBackBuf.clear();
//
// 			StartThreadpoolIo(Io);
			//バックからの受信準備。
			TP_WORK* pTPWork(NULL);
			if (!(pTPWork = CreateThreadpoolWork(RecvBackWorkCB, pSocket, &*pcbe)))
			{
				DWORD Err = GetLastError();
				cerr << "Err! OnSocketBackNoticeCompCB. CreateThreadpoolWork.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				CleanupSocket(pSocket);
				gBackReEnterGuard = FALSE;
				return;
			}
			SubmitThreadpoolWork(pTPWork);
		}
		//バックから受信完了
		else if (pSocket->DirBack == SocketContext::eDir::DIR_TO_FRONT)
		{

			//フロントへ送信。
			pSocket->ReadBackBuf.resize(NumberOfBytesTransferred) ;
			pSocket->WriteBuf = pSocket->ReadBackBuf;
			pSocket->ReadBackBuf.clear();
			TP_WORK* pTPWork(NULL);
			if (!(pTPWork = CreateThreadpoolWork(SendWorkCB, pSocket, &*pcbe)))
			{
				DWORD Err = GetLastError();
				cerr << "Err! OnSocketBackNoticeCompCB.  CreateThreadpoolWork. Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				gBackReEnterGuard = FALSE;
				return;
			}
			SubmitThreadpoolWork(pTPWork);
		}
		else {
			int i = 0;//通常ここには来ない。
			gBackReEnterGuard = FALSE;
			return;
		}
		gBackReEnterGuard = FALSE;
		return;
	}

	VOID SendWorkCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		CloseThreadpoolWork(Work);
		SocketContext* pSocket = (SocketContext*)Context;
//		WaitForThreadpoolIoCallbacks(pSocket->pTPIo, TRUE);
		StartThreadpoolIo(pSocket->pTPIo);
		pSocket->Dir = SocketContext::eDir::DIR_TO_FRONT;
		pSocket->StrToWsa(&pSocket->WriteBuf, &pSocket->wsaWriteBuf);
		if (WSASend(pSocket->hSocket, &pSocket->wsaWriteBuf, 1, NULL, 0, pSocket, NULL))
		{
			DWORD Err(NULL);
			if ((Err = WSAGetLastError()) != WSA_IO_PENDING) {
				cerr << "Err! WSASend.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
//				CloseThreadpoolIo(pSocket->pTPIo);
				CleanupSocket(pSocket);
				return;
			}
		}
		return VOID();
	}

	VOID SendBackWorkCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		CloseThreadpoolWork(Work);
		SocketContext* pSocket = (SocketContext*)Context;
		StartThreadpoolIo(pSocket->pTPBackIo);
		pSocket->DirBack = SocketContext::eDir::DIR_TO_BACK;
		pSocket->StrToWsa(&pSocket->WriteBackBuf, &pSocket->wsaWriteBackBuf);
		if (WSASend(pSocket->hSocketBack, &pSocket->wsaWriteBackBuf, 1, NULL, 0, &pSocket->OLBack, NULL))
		{
			DWORD Err(NULL);
			if ((Err = WSAGetLastError()) != WSA_IO_PENDING) {
				cerr << "Err! SendBackWorkCB. WSASend.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
//				CloseThreadpoolIo(pSocket->pTPBackIo);
				CleanupSocket(pSocket);
				return;
			}
		}
		return VOID();
	}

	VOID RecvWorkCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		CloseThreadpoolWork(Work);
		SocketContext* pSocket = (SocketContext*)Context;
		//受信体制を取る。

		pSocket->Dir = SocketContext::eDir::DIR_TO_BACK;
		pSocket->ReadBuf.resize(BUFFER_SIZE, '\0');
		pSocket->StrToWsa(&pSocket->ReadBuf, &pSocket->wsaReadBuf);

		StartThreadpoolIo(pSocket->pTPIo);
		if (!WSARecv(pSocket->hSocket, &pSocket->wsaReadBuf, 1, NULL, &pSocket->flags, pSocket, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (!(Err != WSA_IO_PENDING || Err!=0))
			{
				cerr << "Err! RecvWorkCB.Code:" << to_string(Err) << " LINE:"<<__LINE__<<"\r\n";
//				CloseThreadpoolIo(pSocket->pTPIo);
				CleanupSocket(pSocket);
				return;
			}
		}
	}

	VOID RecvBackWorkCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		CloseThreadpoolWork(Work);
		SocketContext* pSocket = (SocketContext*)Context;
		//受信体制を取る。

		pSocket->DirBack = SocketContext::eDir::DIR_TO_FRONT;
		pSocket->ReadBackBuf.resize(BUFFER_SIZE, '\0');
		pSocket->StrToWsa(&pSocket->ReadBackBuf, &pSocket->wsaReadBackBuf);

		StartThreadpoolIo(pSocket->pTPBackIo);
		if (!WSARecv(pSocket->hSocketBack, &pSocket->wsaReadBackBuf, 1, NULL, &pSocket->flags, &pSocket->OLBack, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (!(Err != WSA_IO_PENDING || Err != 0))
			{
				cerr << "Err! RecvBackWorkCB. Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
//				CloseThreadpoolIo(pSocket->pTPBackIo);
				CleanupSocket(pSocket);
				return;
			}
		}
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
		oldNum=nowNum;
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


	int StartListen(SocketListenContext* pListenContext)
	{
		cout << "Start Listen\r\n";
		cout << HOST_ADDR <<":" << HOST_PORT << "\r\n";
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
			++gCDel;
			std::cerr << "setsockopt Error! Line:" << __LINE__ << "\r\n";
		}

		//ホストバインド設定
		CHAR strHostAddr[] = "127.0.0.2";
		u_short usHostPort = 50000;
		DWORD Err = 0;
		struct sockaddr_in addr = { };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(HOST_PORT);
		int rVal = inet_pton(AF_INET, HOST_ADDR, &(addr.sin_addr));
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

	BOOL TryConnectBack(SocketContext*pSocket)
	{
		using namespace SevOL;

		if (TryConnectBackFirstMessage)
		{
			cout << "TryConnectBack :" << TO_BACK_END_ADDR << ":" << TO_BACK_END_PORT << "\r\n";
		}
		if (((pSocket->hSocketBack = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, /*NULL*/WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET))
		{
			std::cout << "WSASocket Error! " << "Line: " << __LINE__ << "\r\n";
			return FALSE;
		}

		//ホストバインド設定
		DWORD Err = 0;
		struct sockaddr_in addr = { };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(BACK_HOST_PORT);
		int addr_size = sizeof(addr.sin_addr);
		int rVal = inet_pton(AF_INET, BACK_HOST_ADDR, &(addr.sin_addr));
		if (rVal != 1)
		{
			if (rVal == 0)
			{
				std::cerr << "socket error:inet_pton input value invalided\r\n";
				return FALSE;
			}
			else if (rVal == -1)
			{
				Err = WSAGetLastError();
				std::cerr << "socket error:inet_pton.Code:" << std::to_string(Err) << "\r\n";
				return FALSE;
			}
		}
		if (::bind(pSocket->hSocketBack, (struct sockaddr*)&(addr), sizeof(addr)) == SOCKET_ERROR)
		{
			Err = WSAGetLastError();
			std::cerr << "bind Error! Code:" << std::to_string(Err) << " Line: " << __LINE__ << "\r\n";
			return FALSE;
		}

		//バックエンドDB接続用のadd_inを設定
		struct sockaddr_in Peeraddr = { };
		Peeraddr.sin_family = AF_INET;
		Peeraddr.sin_port = htons(TO_BACK_END_PORT);
		int Peeraddr_size = sizeof(Peeraddr.sin_addr);
		rVal = inet_pton(AF_INET, TO_BACK_END_ADDR, &(Peeraddr.sin_addr));
		if (rVal != 1)
		{
			if (rVal == 0)
			{
				std::cerr << "socket error:inet_pton input value invalided\r\n";
				return FALSE;
			}
			else if (rVal == -1)
			{
				Err = WSAGetLastError();
				std::cerr << "socket error:inet_pton.Code:" << std::to_string(Err) << "\r\n";
				return FALSE;
			}
		}

		//コネクト
		if (connect(pSocket->hSocketBack, (SOCKADDR*)&Peeraddr, sizeof(Peeraddr)) == SOCKET_ERROR)
		{
			if ((Err = WSAGetLastError()) != WSAEWOULDBLOCK)
			{
				std::cerr << "connect Error. Code :" << std::to_string(Err) << " Line : " << __LINE__ << "\r\n";
				return FALSE;
			}
		}
		return TRUE;
	}

	void ShowStatus()
	{
		std::cout << "Total Connected: " << gTotalConnected << "\r\n";
		std::cout << "Current Connecting: " << gTotalConnected - gCDel <<"\r\n";
		std::cout << "Max Connected: " << gMaxConnecting << "\r\n";
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

	bool PreAccept(SocketListenContext* pListenSocket)
	{

		//アクセプト用ソケット取り出し
		SocketContext* pAcceptSocket = gSocketsPool.Pop();
		//オープンソケット作成
		if (!(pAcceptSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
			cerr << "Err WSASocket Code:" << to_string(WSAGetLastError()) << " Line:" << __LINE__ << "\r\n";
			return false;
		}

		//デバック用ID設定
		pAcceptSocket->ID = gID++;

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