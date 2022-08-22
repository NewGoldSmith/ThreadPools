//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "CallbacksOL.h"

using namespace std;
namespace SevOL {

	extern SocketListenContext* gpListenContext;
	std::atomic_uint gAcceptedPerSec(0);
	std::atomic_uint gID(0);
	std::atomic_uint gIDBack(0);
	std::atomic_uint gTotalConnected(0);
	std::atomic_uint gCDel(0);
	std::atomic_uint gMaxConnecting(0);
	SocketContext gSockets[ELM_SIZE];
	SocketContext gSocketsBack[ELM_SIZE];
	PTP_TIMER gpTPTimer(NULL);
	RingBuf<SocketContext> gSocketsPool(gSockets, ELM_SIZE);
	BOOL TryConnectBackFirstMessage(TRUE);

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
			MyTRACE(("Err! OLSev OnListenCompCB Result:" + to_string(IoResult) + " Line:" + to_string(__LINE__) + "SocketID:"+to_string(pSocket->ID) + "\r\n").c_str());
			cout << "End Listen\r\n";
			return;
		}

		//リッスンの完了ポート再設定
		StartThreadpoolIo(Io);

		//次のアクセプト
		if (!PreAccept(pListenSocket))
		{
			cerr << "Err! OnListenCompCB. PreAccept.LINE:" << __LINE__ << "\r\n";
			return;
		}

		//トータルコネクトカウント
		++gTotalConnected;
		gMaxConnecting.store(max(gMaxConnecting.load(), (gTotalConnected.load() - gCDel.load())));

		//切断か確認
		if (!NumberOfBytesTransferred)
		{
			MyTRACE(("OLSev OnListenCompCB Socket"+to_string(pSocket->ID)+ " Closed\r\n").c_str());
			FrontCleanupSocket(pSocket);
			return;
		}

		//読み取りデータのサイズ設定PreAccept参考。
		pSocket->FrontRemBuf.resize(NumberOfBytesTransferred);

		//バックエンドのDBに接続
		if (!TryConnectBack(pSocket))
		{
			cerr << "Err! OnListenCompCB. TryConnectBack.LINE:" << __LINE__ << "\r\n";
			FrontCleanupSocket(pSocket);
			MyTRACE("TryConnectBack. FAIL\r\n");
			return;
		}
		MyTRACE(("Connected Back End.SocketID:" + to_string(pSocket->ID) + "\r\n").c_str());

		//アクセプトソケットフロント完了ポート設定
		if (!(pSocket->pForwardTPIo = CreateThreadpoolIo((HANDLE)pSocket->hFrontSocket, OnSocketFrontNoticeCompCB, pSocket, &*pcbe)))
		{
			DWORD Err = GetLastError();
			cerr << "Err! OnListenCompCB. CreateThreadpoolIo. CODE:" << to_string(Err) << "\r\n";
			FrontCleanupSocket(pSocket);
			return;
		}

		//バックエンドDB向けIO完了ポート設定
		if (!(pSocket->pBackTPIo = CreateThreadpoolIo((HANDLE)pSocket->hBackSocket, OnSocketBackNoticeCompCB, pSocket, &*pcbe)))
		{
			DWORD Err = GetLastError();
			cerr << "Err! OnListenCompCB. CreateThreadpoolIo. CODE:" << to_string(Err) << " LINE:"<< __LINE__<<"\r\n";
			FrontCleanupSocket(pSocket);
			return;
		}

		//改行で分ける。
		pSocket->BackWriteBuf = SplitLastLineBreak(pSocket->FrontRemBuf);
		//送信できるものがあれば送信
		if (pSocket->BackWriteBuf.size())
		{
			pSocket->BackWriteBuf += "\r\n";

			PTP_WORK pTPWork(NULL);
			if (!(pTPWork = CreateThreadpoolWork(SendBackWorkCB, pSocket, &*pcbe)))
			{
				std::cerr << "Err! OnListenCompCB. CreateThreadpoolWork. Line:" << to_string(__LINE__) << "\r\n";
				FrontCleanupSocket(pSocket);
				return;
			}
			else {
				SubmitThreadpoolWork(pTPWork);
			}
		}

		//更にフロントからの受信体制
		PTP_WORK pTPWork(NULL);
		if (!(pTPWork = CreateThreadpoolWork(RecvFrontWorkCB, pSocket, &*pcbe)))
		{
			std::cerr << "Err! OnListenCompCB. CreateThreadpoolWork. Line:" << to_string(__LINE__) << "\r\n";
			FrontCleanupSocket(pSocket);
			return;
		}
		else {
			SubmitThreadpoolWork(pTPWork);
		}
		return;
	}

	VOID OnSocketFrontNoticeCompCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
	{
		MyTRACE("Enter OnSocketFrontNoticeCompCB\r\n");
		SocketContext* pSocket = (SocketContext*)Context;

		//エラー確認
		if (IoResult)
		{
			MyTRACE(("Err! OLSev OnSocketFrontNoticeCompCB Code:" + to_string(IoResult) + " Line:" + to_string(__LINE__) + " SocketID:" + to_string(pSocket->ID) + "\r\n").c_str());
			FrontCleanupSocket(pSocket);
			return;
		}

		//切断か確認。
		if (!NumberOfBytesTransferred)
		{
			MyTRACE(("Front Socket ID:"+to_string(pSocket->ID)+ " Closed\r\n").c_str());
			FrontCleanupSocket(pSocket);
			return;
		}

		//レシーブの完了か確認。
		if (pSocket->Dir == SocketContext::eDir::DIR_TO_BACK)
		{
			MyTRACE("Recved\r\n");
			pSocket->FrontRemBuf += {pSocket->wsaFrontReadBuf.buf, NumberOfBytesTransferred};
			pSocket->FrontReadBuf.clear();
			//最後の改行からをWriteBackBufに入れる。
			pSocket->BackWriteBuf = SplitLastLineBreak(pSocket->FrontRemBuf);

			//WriteBufに中身があれば、バックへの送信の開始
			if (pSocket->BackWriteBuf.size())
			{
				pSocket->BackWriteBuf += "\r\n";
				PTP_WORK pTPWork(NULL);
				if (!(pTPWork = CreateThreadpoolWork(SendBackWorkCB, pSocket, &*pcbe)))
				{
					std::cerr << "Err! OnSocketFrontNoticeCompCB. CreateThreadpoolWork. Line:" << to_string(__LINE__) << "\r\n";
					FrontCleanupSocket(pSocket);
					return;
				}
				else {
					SubmitThreadpoolWork(pTPWork);
				}

				//更にフロントからの受信の準備
				if (!(pTPWork = CreateThreadpoolWork(RecvFrontWorkCB, pSocket, &*pcbe)))
				{
					std::cerr << "Err! OnSocketFrontNoticeCompCB. CreateThreadpoolWork. Line:" << to_string(__LINE__) << "\r\n";
					FrontCleanupSocket(pSocket);
					return;
				}
				else {
					SubmitThreadpoolWork(pTPWork);
				}
			}
			else{
				//WriteBufに中身がない場合送信はしない。更に受信完了ポートスタート。
				PTP_WORK pTPWork(NULL);
				if (!(pTPWork = CreateThreadpoolWork(RecvFrontWorkCB, pSocket, &*pcbe)))
				{
					std::cerr << "Err! OnSocketFrontNoticeCompCB. CreateThreadpoolWork. Line:" << to_string(__LINE__) << "\r\n";
					FrontCleanupSocket(pSocket);
					return;
				}
				else {
					SubmitThreadpoolWork(pTPWork);
				}
			}
			return;
		}
		//送信完了。
		else if (pSocket->Dir == SocketContext::eDir::DIR_TO_FRONT)
		{
			MyTRACE(("SevOL Front Sent:" + pSocket->FrontWriteBuf).c_str());
			pSocket->FrontWriteBuf.clear();
			//受信完了ポートスタート。
			PTP_WORK pTPWork(NULL);
			if (!(pTPWork = CreateThreadpoolWork(RecvFrontWorkCB, pSocket, &*pcbe)))
			{
				std::cerr << "Err! OnSocketFrontNoticeCompCB. CreateThreadpoolWork. Line:" << to_string(__LINE__) << "\r\n";
				FrontCleanupSocket(pSocket);
				return;
			}
			else {
				SubmitThreadpoolWork(pTPWork);
			}
		}
		else {
			int i = 0;//通常ここには来ない。
		}
		return;
	}

	VOID OnSocketBackNoticeCompCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
	{
		MyTRACE("Enter OnSocketBackNoticeCompCB\r\n");
		SocketContext* pSocket = (SocketContext*)Context;
		//エラー確認
		if (IoResult)
		{
			MyTRACE(("Err! OLSev OnSocketBackNoticeCompCB Code:" + to_string(IoResult) + " Line:" + to_string(__LINE__) + " SocketID:"+to_string(pSocket->ID) + "\r\n").c_str());
			BackCleanupSocket(pSocket);
			return;
		}

		//切断か確認。
		if (!NumberOfBytesTransferred)
		{
			MyTRACE(("Back SocketID:"+to_string(pSocket->ID)+ " Closed\r\n").c_str());
			BackCleanupSocket(pSocket);
			return;
		}

		//バックターゲットへの送信完了か確認。
		if (pSocket->DirBack == SocketContext::eDir::DIR_TO_BACK)
		{
			//バックに送信完了。
			MyTRACE(("SevOL Back Sent:" + pSocket->BackWriteBuf).c_str());
			pSocket->BackWriteBuf.clear();

			//バックからの受信準備。
			PTP_WORK pTPWork(NULL);
			if (!(pTPWork = CreateThreadpoolWork(RecvBackWorkCB, pSocket, &*pcbe)))
			{
				std::cerr << "Err! OnSocketBackNoticeCompCB. CreateThreadpoolWork. Line:" << to_string(__LINE__) << "\r\n";
				FrontCleanupSocket(pSocket);
				return;
			}
			else {
				SubmitThreadpoolWork(pTPWork);
			}
		}

		//バックターゲットから受信完了
		else if (pSocket->DirBack == SocketContext::eDir::DIR_TO_FRONT)
		{
			MyTRACE("Recved\r\n");

			//フロントへ送信。
			pSocket->BackReadBuf.resize(NumberOfBytesTransferred) ;
			pSocket->FrontWriteBuf = pSocket->BackReadBuf;
			pSocket->BackReadBuf.clear();
			PTP_WORK pTPWork(NULL);
			if (!(pTPWork = CreateThreadpoolWork(SendFrontWorkCB, pSocket, &*pcbe)))
			{
				std::cerr << "Err! OnSocketBackNoticeCompCB. CreateThreadpoolWork. Line:" << to_string(__LINE__) << "\r\n";
				FrontCleanupSocket(pSocket);
				return;
			}
			else {
				SubmitThreadpoolWork(pTPWork);
			}
		}
		else {
			int i = 0;//通常ここには来ない。
			return;
		}
		return;
	}

	VOID SendFrontWorkCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		SocketContext* pSocket = (SocketContext*)Context;
		if (!pSocket->pForwardTPIo)
		{
			return;
		}
		StartThreadpoolIo(pSocket->pForwardTPIo);
		pSocket->Dir = SocketContext::eDir::DIR_TO_FRONT;
		pSocket->StrToWsa(&pSocket->FrontWriteBuf, &pSocket->wsaFrontWriteBuf);
		if (WSASend(pSocket->hFrontSocket, &pSocket->wsaFrontWriteBuf, 1, NULL, 0, pSocket, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (Err != WSA_IO_PENDING && Err != 0) {
				cerr << "Err! SendFront. Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				pSocket->lockCleanup.release();
				return;
			}
		}
		return;
	}

	BOOL SendFront(SocketContext* pSocket)
	{
		if (!pSocket->pForwardTPIo)
		{
			return FALSE;
		}
		StartThreadpoolIo(pSocket->pForwardTPIo);
		pSocket->Dir = SocketContext::eDir::DIR_TO_FRONT;
		pSocket->StrToWsa(&pSocket->FrontWriteBuf, &pSocket->wsaFrontWriteBuf);
		if (WSASend(pSocket->hFrontSocket, &pSocket->wsaFrontWriteBuf, 1, NULL, 0, pSocket, NULL))
		{
			DWORD Err=WSAGetLastError();
			if (Err!= WSA_IO_PENDING && Err!=0) {
				cerr << "Err! SendFront. Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				pSocket->lockCleanup.release();
				return FALSE;
			}
		}
		return TRUE;
	}

	VOID SendBackWorkCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		SocketContext* pSocket = (SocketContext*)Context;
		if (!pSocket->pBackTPIo)
		{
			return;
		}
		StartThreadpoolIo(pSocket->pBackTPIo);
		pSocket->DirBack = SocketContext::eDir::DIR_TO_BACK;
		pSocket->StrToWsa(&pSocket->BackWriteBuf, &pSocket->wsaWriteBackBuf);
		if (WSASend(pSocket->hBackSocket, &pSocket->wsaWriteBackBuf, 1, NULL, 0, &pSocket->OLBack, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (Err && Err != WSA_IO_PENDING) {
				cerr << "Err! SendBack. WSASend.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				return;
			}
		}
		return;
	}

	BOOL SendBack(SocketContext* pSocket)
	{
		StartThreadpoolIo(pSocket->pBackTPIo);
		pSocket->DirBack = SocketContext::eDir::DIR_TO_BACK;
		pSocket->StrToWsa(&pSocket->BackWriteBuf, &pSocket->wsaWriteBackBuf);
		if (WSASend(pSocket->hBackSocket, &pSocket->wsaWriteBackBuf, 1, NULL, 0, &pSocket->OLBack, NULL))
		{
			DWORD Err=WSAGetLastError();
			if (Err && Err !=WSA_IO_PENDING) {
				cerr << "Err! SendBack. WSASend.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				return FALSE;
			}
		}
		return TRUE;
	}

	VOID RecvFrontWorkCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{

		SocketContext* pSocket = (SocketContext*)Context;
		if (!pSocket->pForwardTPIo)
		{
			return;
		}
		StartThreadpoolIo(pSocket->pForwardTPIo);
		pSocket->Dir = SocketContext::eDir::DIR_TO_BACK;
		pSocket->FrontReadBuf.resize(BUFFER_SIZE, '\0');
		pSocket->StrToWsa(&pSocket->FrontReadBuf, &pSocket->wsaFrontReadBuf);
		if (!WSARecv(pSocket->hFrontSocket, &pSocket->wsaFrontReadBuf, 1, NULL, &pSocket->flags, pSocket, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (Err != WSA_IO_PENDING && Err)
			{
				cerr << "Err! RecvFront. Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				return;
			}
		}
		return;
	}

	BOOL RecvFront(SocketContext* pSocket)
	{
		if (!pSocket->pForwardTPIo)
		{
			return FALSE;
		}
		StartThreadpoolIo(pSocket->pForwardTPIo);
		pSocket->Dir = SocketContext::eDir::DIR_TO_BACK;
		pSocket->FrontReadBuf.resize(BUFFER_SIZE, '\0');
		pSocket->StrToWsa(&pSocket->FrontReadBuf, &pSocket->wsaFrontReadBuf);
		if (!WSARecv(pSocket->hFrontSocket, &pSocket->wsaFrontReadBuf, 1, NULL, &pSocket->flags, pSocket, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (Err != WSA_IO_PENDING && Err)
			{
				cerr << "Err! RecvFront. Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				return FALSE;
			}
		}
		return TRUE;
	}

	VOID RecvBackWorkCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		SocketContext* pSocket = (SocketContext*)Context;
		if (!pSocket->pBackTPIo)
		{
			return;
		}
		StartThreadpoolIo(pSocket->pBackTPIo);
		pSocket->DirBack = SocketContext::eDir::DIR_TO_FRONT;
		pSocket->BackReadBuf.resize(BUFFER_SIZE, '\0');
		pSocket->StrToWsa(&pSocket->BackReadBuf, &pSocket->wsaReadBackBuf);

		if (!WSARecv(pSocket->hBackSocket, &pSocket->wsaReadBackBuf, 1, NULL, &pSocket->flags, &pSocket->OLBack, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (Err != WSA_IO_PENDING && Err)
			{
				cerr << "Err! RecvBack. Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				return;
			}
		}
		return;
	}

	BOOL RecvBack(SocketContext* pSocket)
	{
		if (!pSocket->pBackTPIo)
		{
			return FALSE;
		}
		StartThreadpoolIo(pSocket->pBackTPIo);
		pSocket->DirBack = SocketContext::eDir::DIR_TO_FRONT;
		pSocket->BackReadBuf.resize(BUFFER_SIZE, '\0');
		pSocket->StrToWsa(&pSocket->BackReadBuf, &pSocket->wsaReadBackBuf);

		if (!WSARecv(pSocket->hBackSocket, &pSocket->wsaReadBackBuf, 1, NULL, &pSocket->flags, &pSocket->OLBack, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (Err != WSA_IO_PENDING && Err)
			{
				cerr << "Err! RecvBack. Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				return FALSE;
			}
		}
		return TRUE;
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

	void FrontCleanupSocket(SocketContext* pSocket)
	{
		++gCDel;
		if (!(gTotalConnected - gCDel))
		{
			TryConnectBackFirstMessage = TRUE;
			ShowStatus();
		}
		pSocket->ReInitialize();
		pSocket->pForwardTPIo = NULL;
		pSocket->pBackTPIo = NULL;
		gSocketsPool.Push(pSocket);
	}

	void BackCleanupSocket(SocketContext* pSocket)
	{
		closesocket(pSocket->hBackSocket);
		pSocket->hBackSocket = NULL;
		closesocket(pSocket->hFrontSocket);
		pSocket->hFrontSocket = NULL;
//		FrontCleanupSocket( pSocket);
	}


	int StartListen(SocketListenContext* pListenContext)
	{
		cout << "Start Listen\r\n";
		cout << HOST_ADDR <<":" << HOST_PORT << "\r\n\r\n";
		//デバック用にIDをつける。リッスンソケットIDは0。
		pListenContext->ID = gID++;

		//ソケット作成
		WSAPROTOCOL_INFOA prot_info{};
		pListenContext->hFrontSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (!pListenContext->hFrontSocket)
		{
			return S_FALSE;
		}

		//ソケットリユースオプション
		BOOL yes = 1;
		if (setsockopt(pListenContext->hFrontSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)))
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
		if ((rVal = ::bind(pListenContext->hFrontSocket, (struct sockaddr*)&(addr), sizeof(addr)) == SOCKET_ERROR))
		{
			cerr << "Err ::bind Code:" << WSAGetLastError() << " Line:" << __LINE__ << "\r\n";
			return false;
		}

		//リッスン
		if (listen(pListenContext->hFrontSocket, SOMAXCONN)) {
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

		//AcceptExによる事前アクセプト
		if (!PreAccept(pListenContext))
		{
			cerr << "Err! StartListen.PreAccept. LINE:" << __LINE__ << "\r\n";
			return false;
		}

		//リッスンソケット完了ポート作成
		if (!(pListenContext->pTPListen = CreateThreadpoolIo((HANDLE)pListenContext->hFrontSocket,OnListenCompCB, pListenContext, &*pcbe))) {
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
		static SOCKET hFrontSocket(NULL);
		DWORD dwBytes;
		if (!lpfnAcceptEx || hFrontSocket != pListenSocket->hFrontSocket)
		{
			try {
				if (WSAIoctl(pListenSocket->hFrontSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
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
		hFrontSocket = pListenSocket->hFrontSocket;
		return lpfnAcceptEx;
	}

	LPFN_GETACCEPTEXSOCKADDRS GetGetAcceptExSockaddrs(SocketContext* pSocket)
	{
		GUID GuidAcceptEx = WSAID_GETACCEPTEXSOCKADDRS;
		int iResult = 0;
		static LPFN_GETACCEPTEXSOCKADDRS lpfnAcceptEx(NULL);
		static SOCKET hFrontSocket(NULL);
		DWORD dwBytes;
		if (!lpfnAcceptEx || hFrontSocket != pSocket->hFrontSocket)
		{
			try {
				if (WSAIoctl(pSocket->hFrontSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
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
		hFrontSocket = pSocket->hFrontSocket;
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
		if (((pSocket->hBackSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, /*NULL*/WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET))
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
		if (::bind(pSocket->hBackSocket, (struct sockaddr*)&(addr), sizeof(addr)) == SOCKET_ERROR)
		{
			Err = WSAGetLastError();
			std::cerr << "bind Error! Code:" << std::to_string(Err) << " Line: " << __LINE__ << "\r\n";
			return FALSE;
		}

		//バックエンド接続用のターゲットアドレスを設定
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
		if (connect(pSocket->hBackSocket, (SOCKADDR*)&Peeraddr, sizeof(Peeraddr)) == SOCKET_ERROR)
		{
			if ((Err = WSAGetLastError()) != WSAEWOULDBLOCK)
			{
				std::cerr << "connect Error. Code :" << std::to_string(Err) << " Line : " << __LINE__ << "\r\n";
				return FALSE;
			}
		}
		MyTRACE(("Connected Back End.SocketID:" + to_string(pSocket->ID) + "\r\n").c_str());
		return TRUE;
	}

	void ShowStatus()
	{
		cout << "\r\n";
		std::cout << "Total Connected: " << gTotalConnected << "\r\n";
		std::cout << "Current Connecting: " << gTotalConnected - gCDel <<"\r\n";
		std::cout << "Max Connected: " << gMaxConnecting << "\r\n";
		std::cout << "Max Accepted/Sec: " << gAcceptedPerSec << "\r\n\r\n";
		cout <<"Front Host Address: " << HOST_ADDR << ":" << HOST_PORT << "\r\n";
		cout << "Back Host Address: " << BACK_HOST_ADDR << ":" << BACK_HOST_PORT << "\r\n";
		cout << "Back Target Address: " << TO_BACK_END_ADDR << ":" << TO_BACK_END_PORT << "\r\n";
		cout << "\r\n";
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
		SocketContext* pAcceptSocket = gSocketsPool.Pop();
		//オープンソケット作成
		if (!(pAcceptSocket->hFrontSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
			cerr << "Err WSASocket Code:" << to_string(WSAGetLastError()) << " Line:" << __LINE__ << "\r\n";
			return false;
		}

		//デバック用ID設定
		pAcceptSocket->ID = gID++;
		pAcceptSocket->FrontRemBuf.resize(BUFFER_SIZE, '\0');
		//AcceptExの関数ポインタを取得し、実行。パラメーターはサンプルまんま。
		if (!(*GetAcceptEx(pListenSocket))(pListenSocket->hFrontSocket, pAcceptSocket->hFrontSocket, pAcceptSocket->FrontRemBuf.data(), BUFFER_SIZE - ((sizeof(sockaddr_in) + 16) * 2), sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, (OVERLAPPED*)pAcceptSocket))
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