//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#include "CBBack.h"
using namespace std;
namespace FrontSevEv {
	extern const std::unique_ptr
		< TP_CALLBACK_ENVIRON
		, decltype(DestroyThreadpoolEnvironment)*
		> pcbe;

	const std::unique_ptr
		< std::remove_pointer_t<HANDLE>
		, decltype(CloseHandle)*
		> gpSem
	{ []()
		{
			return CreateSemaphoreA(NULL, NUM_BACK_CONNECTION, NUM_BACK_CONNECTION, NULL);
		}()
		,
		CloseHandle
	};
	extern 	RingBuf<SocketContext> gSocketsPool;
	RoundContext RC[NUM_BACK_CONNECTION];
	RingBuf BackContextPool(RC, _countof(RC));

	VOID OnBackEvSocketCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{
		WSANETWORKEVENTS NetworkEvents{};
		DWORD Err = 0;
		DWORD dwBytes = 0;
		RoundContext* pBackSocket = (RoundContext*)Context;

		if (WSAEnumNetworkEvents(pBackSocket->hSocket, pBackSocket->hEvent.get(), &NetworkEvents))
		{
			Err = WSAGetLastError();
			if (Err != WSANOTINITIALISED)
			{
				stringstream  ss;
				ss << "FrontSevEv. WSAEnumNetworkEvents. Code:"<<Err<<" LINE:" << __LINE__ << std::endl;
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
			}
			CloseThreadpoolWait(Wait);
			return;
		}

		//読み込み可能
		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			pBackSocket->pFrontSocket->Buf.resize(BUFFER_SIZE, '\0');
			//フロントのバッファに直接読み込み。
			int size=recv(pBackSocket->hSocket, pBackSocket->pFrontSocket->Buf.data(), pBackSocket->pFrontSocket->Buf.size(), 0);
			if (size == SOCKET_ERROR)
			{
				pBackSocket->pFrontSocket->Buf.clear();
				Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. WSAEnumNetworkEvents. recv Code: " << Err <<  " LINE: " << __LINE__ <<"\r\n";
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
				pBackSocket->pFrontSocket->ReInitialize();
				gSocketsPool.Push(pBackSocket->pFrontSocket);
				return;
			}
			else if (size == 0)
			{
				//切断
				stringstream  ss;
				ss << "FrontSevEv. WSAEnumNetworkEvents. recv size 0. Code: " << Err << " LINE: " << __LINE__ << "\r\n";
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
				//フロントソケットはクローズ
				pBackSocket->pFrontSocket->ReInitialize();
				gSocketsPool.Push(pBackSocket->pFrontSocket);
				pBackSocket->pFrontSocket = NULL;
				//バックソケットは使用禁止にする為セマフォを空けない。
				//イベントも停止。
				return;
			}
			else {
				//クライアントへ返信
				pBackSocket->pFrontSocket->Buf.resize(size);
				int rsize =send(pBackSocket->pFrontSocket->hSocket, pBackSocket->pFrontSocket->Buf.data(), pBackSocket->pFrontSocket->Buf.size(), 0);
				if (rsize == SOCKET_ERROR)
				{
					pBackSocket->pFrontSocket->Buf.clear();
					Err = WSAGetLastError();
					stringstream  ss;
					ss << "FrontSevEv. WSAEnumNetworkEvents. send Code: " << Err << " LINE: " << __LINE__ << "\r\n";
					std::cerr << ss.str();
					MyTRACE(ss.str().c_str());
					pBackSocket->pFrontSocket->ReInitialize();
					gSocketsPool.Push(pBackSocket->pFrontSocket);
					pBackSocket->pFrontSocket = NULL;
					return;
				}
			}
		}
		//BackがCloseした。
		else if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			//フロントソケットはCloseし
			pBackSocket->pFrontSocket->ReInitialize();
			gSocketsPool.Push(pBackSocket->pFrontSocket);
			//バックソケットも閉じる。セマフォは解放しない。
			CloseThreadpoolWait(Wait);
			return;
		}

//待機オブジェクト再設定
		SetThreadpoolWait(Wait, pBackSocket->hEvent.get(), NULL);

		//次に呼ばれるまでプールに移動
		pBackSocket->pFrontSocket = NULL;
		BackContextPool.Push(pBackSocket);

		//待機オブジェクト解放		
//		CloseThreadpoolWait(Wait);

		//正常終了セマフォ解放設定。
		ReleaseSemaphoreWhenCallbackReturns(Instance, gpSem.get(), 1);
	}

	//ラウンドロビンで書き込みの順番が回ってきた。
	VOID WriteBackWaitCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{
		SocketContext* pSocket = (SocketContext*)Context;
		if (WaitResult)
		{
			cerr << "FrontSevEv. WriteBackWaitCB. Code:" << to_string(WaitResult) << "\r\n";
			return;
		}

		//セマフォが空いているからこのコールバックが呼ばれたので在るはず。
		//pRCは共有でNUM_BACK_CONNECTIONの数しかない。
		RoundContext* pBackSocket = BackContextPool.Pull();

		//フロントソケットと結びつけ。
		pBackSocket->pFrontSocket = pSocket;

		//フロントのデータをバックに書き込み。
		//返信はイベント設定されているので、OnBackEvSocketCBが呼ばれる。
		//ContextはRoundContext
		if (send(pBackSocket->hSocket, pSocket->Buf.data(), pSocket->Buf.length(), 0) == SOCKET_ERROR)
		{
			DWORD Err = WSAGetLastError();
			stringstream  ss;
			ss << "FrontSevEv. send. Code:" << to_string(Err) << " LINE;" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			return;
		}
		pSocket->Buf.clear();

		return VOID();
	}

	//ライトからリードまでが一つの流れ
	void QueryBack(SocketContext* pSocket)
	{
		//セマフォの待機関数としてセットする。
		TP_WAIT* pTPWait(NULL);
		if (!(pTPWait = CreateThreadpoolWait(WriteBackWaitCB, pSocket, &*pcbe)))
		{
			DWORD err = GetLastError();
			cerr << "FrontSevEv. CreateThreadpoolWait. Code:" << to_string(err) << "__LINE__"<<__LINE__<<"\r\n";
			return;
		}
		SetThreadpoolWait(pTPWait, gpSem.get(), NULL);
	}

	BOOL BackTryConnect()
	{
		//ホストsockeaddr_in設定
		struct sockaddr_in addr = { };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(0);
		int addr_size = sizeof(addr.sin_addr);
		int rVal = inet_pton(AF_INET, HOST_BACK_BASE_ADDR, &(addr.sin_addr));
		if (rVal != 1)
		{
			if (rVal == 0)
			{
				stringstream  ss;
				ss << "FrontSevEv. Back Socket:inet_pton input value invalided. LINE:"<< __LINE__<<"\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}
			else if (rVal == -1)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. socket error:inet_pton.Code:" << to_string(Err) << " LINE:"<<__LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}
		}

		//サーバー接続用のsockaddr_inを設定
		struct sockaddr_in Peeraddr = { };
		Peeraddr.sin_family = AF_INET;
		Peeraddr.sin_port = htons(PEER_BACK_PORT);
		int Peeraddr_size = sizeof(Peeraddr.sin_addr);
		rVal = inet_pton(AF_INET, PEER_BACK_BASE_ADDR, &(Peeraddr.sin_addr));
		if (rVal != 1)
		{
			if (rVal == 0)
			{
				stringstream  ss;
				ss << "FrontSevEv. socket error:inet_pton input value invalided. LINE:"<<__LINE__<<"\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}
			else if (rVal == -1)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "CliR. socket error:inet_pton.Code:" << to_string(Err) << " LINE:"<<__LINE__<<"\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}
		}

		for (u_int i = 0; i < NUM_BACK_CONNECTION; ++i)
		{
			RoundContext *pBackSocket = BackContextPool.Pull();

			if (((pBackSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, NULL/*WSA_FLAG_OVERLAPPED*/)) == INVALID_SOCKET))
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. Back Socket WSASocket Error! Code:" << to_string(Err) << " Line: " << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}

			//ホストバインド
			if (::bind(pBackSocket->hSocket, (struct sockaddr*)&(addr), sizeof(addr)) == SOCKET_ERROR)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. bind Error! Code:" << std::to_string(Err) << " Line: " << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}

			//コネクト
			if (connect(pBackSocket->hSocket, (SOCKADDR*)&Peeraddr, sizeof(Peeraddr)) == SOCKET_ERROR)
			{
				DWORD Err = WSAGetLastError();
				if (Err != WSAEWOULDBLOCK && Err)
				{
					stringstream  ss;
					ss << "FrontSevEv. connect Error. Code :" << std::to_string(Err) << " Line : " << __LINE__ << "\r\n";
					cerr << ss.str();
					MyTRACE(ss.str().c_str());
					return FALSE;
				}
			}

			//ソケットのイベント設定
			if (WSAEventSelect(pBackSocket->hSocket, pBackSocket->hEvent.get(), /*FD_ACCEPT |*/ FD_CLOSE | FD_READ/* | FD_CONNECT | FD_WRITE*/) == SOCKET_ERROR)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. WSAEventSelect. Code:" << to_string(Err) << " Line:" << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}

			//イベントハンドラの設定
			PTP_WAIT pTPWait(NULL);
			if (!(pTPWait = CreateThreadpoolWait(OnBackEvSocketCB, pBackSocket, &*pcbe)))
			{
				DWORD Err = GetLastError();
				stringstream  ss;
				ss << "FrontSevEv. CreateThreadpoolWait. CODE:"<<to_string(Err) << " Line:" << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}
			SetThreadpoolWait(pTPWait, pBackSocket->hEvent.get(), NULL);

			//設定が済んだので再格納。
			BackContextPool.Push(pBackSocket);
		}
		return TRUE;
	}

	void BackClose()
	{
		for (u_int i = 0; i < NUM_BACK_CONNECTION; ++i)
		{
			RoundContext* pBackSocket = BackContextPool.Pull();

			if (pBackSocket->hSocket)
			{
				shutdown(pBackSocket->hSocket, SD_SEND);
				closesocket(pBackSocket->hSocket);
				pBackSocket->hSocket = NULL;
			}
		}
	}
}