//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#include "CBBack.h"
using namespace std;
namespace FrontSevEv {
	atomic_uint gIDBack(0);
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
	extern 	RingBuf<FrontContext> gSocketsPool;
	BackContext RC[NUM_BACK_CONNECTION];
	RingBuf BackContextPool(RC, _countof(RC));
	atomic_uint gLostBackSocket(0);

	VOID OnBackEvSocketCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{
		WSANETWORKEVENTS NetworkEvents{};
		DWORD Err = 0;
		DWORD dwBytes = 0;
		BackContext* pBackSocket = (BackContext*)Context;

		if (WSAEnumNetworkEvents(pBackSocket->hSocket, pBackSocket->hEvent.get(), &NetworkEvents))
		{
			Err = WSAGetLastError();
			if (Err != WSANOTINITIALISED)
			{
				stringstream  ss;
				ss << "FrontSevEv. WSAEnumNetworkEvents. Code:" << Err << " LINE:" << __LINE__ << std::endl;
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
			}
			CloseThreadpoolWait(Wait);
			return;
		}

		//BackがCloseした。
		if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			//バックソケットはCloseした
			stringstream  ss;
			ss << "FrontSevEv. BackSocket ID:" << pBackSocket->ID << " Closed. LINE:" <<__LINE__<< "\r\n";
			std::cerr << ss.str();
			MyTRACE(ss.str().c_str());

			DecStatusFront();
			pBackSocket->pFrontSocket->ReInitialize();
			gSocketsPool.Push(pBackSocket->pFrontSocket);

			//バックソケットも閉じる。セマフォは解放しない。
			pBackSocket->ReInitialize();
			//再接続を試みる。
			if (!BackTryConnect(pBackSocket))
			{
				stringstream  ss;
				ss << "FrontSevEv. BackSocket. Stoped. ID:"<< pBackSocket->ID << " WSAEnumNetworkEvents.Reconnect failure."<< " LINE : " << __LINE__ <<"\r\n";
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
				//停止
				while(TRUE)
				{
					++gLostBackSocket;
				}
			}
			BackContextPool.Push(pBackSocket);
			//セマフォ解放設定
			ReleaseSemaphoreWhenCallbackReturns(Instance, gpSem.get(), 1);
			return;
		}

		//読み込み可能
		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			pBackSocket->pFrontSocket->ReadBuf.clear();
			pBackSocket->pFrontSocket->ReadBuf.resize(BUFFER_SIZE, '\0');
			//フロントのバッファに直接読み込み。
			int size = recv(pBackSocket->hSocket, pBackSocket->pFrontSocket->ReadBuf.data(), pBackSocket->pFrontSocket->ReadBuf.size(), 0);
			if (size == SOCKET_ERROR)
			{
				pBackSocket->pFrontSocket->ReadBuf.clear();
				Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. Back recv Code: " << Err << " LINE: " << __LINE__ << "\r\n";
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
				//フロントソケットは閉じる。
				DecStatusFront();
				pBackSocket->pFrontSocket->ReInitialize();
				gSocketsPool.Push(pBackSocket->pFrontSocket);
				return;
			}
			//サイズの確認
			if (size == 0)
			{
				//切断
				stringstream  ss;
				ss << "FrontSevEv. Back. recv size 0. Code: " << Err << " LINE: " << __LINE__ << "\r\n";
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
				//フロントソケットはクローズ
				FrontContext* pFrontSocket=pBackSocket->pFrontSocket;
				pBackSocket->pFrontSocket = NULL;
				DecStatusFront();
				pFrontSocket->ReInitialize();
				gSocketsPool.Push(pFrontSocket);

				pBackSocket->ReInitialize();
				ss.clear();
				if (!BackTryConnect(pBackSocket))
				{
					ss << "FrontSevEv. Back. Reconnect failure.ID:" << pBackSocket->ID << "\r\n";
					MyTRACE(ss.str().c_str());
					//バックソケットは使用禁止にする為セマフォを空けない。
					//イベントも停止。
					while(TRUE)
					{
						++gLostBackSocket;
					}
				}
				ss << "FrontSevEv. Back. Reconnect success.ID:" << pBackSocket->ID << "\r\n";
				MyTRACE(ss.str().c_str());
				BackContextPool.Push(pBackSocket);

				//正常終了セマフォ解放設定。
				ReleaseSemaphoreWhenCallbackReturns(Instance, gpSem.get(), 1);
				return;
			}

			//クライアントへ返信
			pBackSocket->pFrontSocket->ReadBuf.resize(size);
			int rsize = send(pBackSocket->pFrontSocket->hSocket, pBackSocket->pFrontSocket->ReadBuf.data(), pBackSocket->pFrontSocket->ReadBuf.size(), 0);
			if (rsize == SOCKET_ERROR)
			{
				pBackSocket->pFrontSocket->ReadBuf.clear();
				Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. WSAEnumNetworkEvents. send Code: " << Err << " LINE: " << __LINE__ << "\r\n";
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
				DecStatusFront();
				pBackSocket->pFrontSocket->ReInitialize();
				gSocketsPool.Push(pBackSocket->pFrontSocket);
				pBackSocket->pFrontSocket = NULL;
				BackContextPool.Push(pBackSocket);
				//セマフォ解放設定。
				ReleaseSemaphoreWhenCallbackReturns(Instance, gpSem.get(), 1);
				return;
			}
		}

		//待機オブジェクト再設定
		SetThreadpoolWait(Wait, pBackSocket->hEvent.get(), NULL);

		//次に呼ばれるまでプールに移動
		pBackSocket->pFrontSocket = NULL;
		BackContextPool.Push(pBackSocket);

		//正常終了セマフォ解放設定。
		ReleaseSemaphoreWhenCallbackReturns(Instance, gpSem.get(), 1);
	}

	//セマフォで書き込みの順番が回ってきた。
	VOID WriteBackWaitCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{
		FrontContext* pSocket = (FrontContext*)Context;
		if (WaitResult)
		{
			cerr << "FrontSevEv. Back WriteBackWaitCB. Code:" << to_string(WaitResult) << "\r\n";
			return;
		}

		//セマフォが空いているからこのコールバックが呼ばれたので在るはず。
		//pRCは共有でNUM_BACK_CONNECTIONの数しかない。
		BackContext* pBackSocket = BackContextPool.Pull();

		//フロントソケットと結びつけ。
		pBackSocket->pFrontSocket = pSocket;
		pSocket->vBufLock.acquire();
		string str = pSocket->vBuf.front();
		pSocket->vBuf.erase(pSocket->vBuf.begin());
		pSocket->vBufLock.release();
		
		//フロントのデータをバックに書き込み。
		//返信はイベント設定されているので、OnBackEvSocketCBが呼ばれる。
		//ContextはRoundContext
		if (send(pBackSocket->hSocket, str.data(), str.length(), 0) == SOCKET_ERROR)
		{
			DWORD Err = WSAGetLastError();
			stringstream  ss;
			ss << "FrontSevEv. Back send. Code:" << to_string(Err) << " LINE;" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			pBackSocket->ReInitialize();
			//エラーの場合再接続を試みる。
			if (!BackTryConnect(pBackSocket))
			{
				stringstream  ss;
				ss << "FrontSevEv. Back send. Code:" << to_string(Err) << " LINE;" << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				while (TRUE)
				{
					stringstream  ss;
					//不能な為、STOP
					ss << "Stop. Incompetent. BackSocket ID:" << pBackSocket->ID << " LINE:"<<__LINE__<<"\r\n";
					cerr << ss.str();
					MyTRACE(ss.str().c_str());
					++gLostBackSocket;
				}
			}
			//再接続後送信。
			if (send(pBackSocket->hSocket, str.data(), str.length(), 0) == SOCKET_ERROR)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. Back send. Code:" << to_string(Err) << " LINE;" << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				while (TRUE)
				{
					stringstream  ss;
					ss << "Stop. Incompetent. BackSocket ID:" << pBackSocket->ID << " LINE:" << __LINE__ << "\r\n";
					cerr << ss.str();
					MyTRACE(ss.str().c_str());
					++gLostBackSocket;
				}
			}
		}
		return VOID();
	}

	//ライトからリードまでが一つの流れ
	void QueryBack(FrontContext* pSocket)
	{
		//セマフォの待機関数としてセットする。
		if (!(pSocket->pTPWait = CreateThreadpoolWait(WriteBackWaitCB, pSocket, &*pcbe)))
		{
			DWORD err = GetLastError();
			cerr << "FrontSevEv. Back CreateThreadpoolWait. Code:" << to_string(err) << "__LINE__" << __LINE__ << "\r\n";
			return;
		}
		SetThreadpoolWait(pSocket->pTPWait, gpSem.get(), NULL);
	}

	BOOL InitBack()
	{
		for (u_int i = 0; i < NUM_BACK_CONNECTION; ++i)
		{
			BackContext* pBackSocket = BackContextPool.Pull();
			if (!BackTryConnect(pBackSocket))
			{
				return FALSE;
			}
			//設定が済んだので格納。
			BackContextPool.Push(pBackSocket);
		}
		return TRUE;
	}

	BOOL BackTryConnect(BackContext* pBackSocket)
	{
		//デバック用ID
		pBackSocket->ID = gIDBack++;

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
				ss << "FrontSevEv. Back Socket:inet_pton input value invalided. LINE:" << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}
			else if (rVal == -1)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. Back socket error:inet_pton.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
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
				ss << "FrontSevEv. Back socket error:inet_pton input value invalided. LINE:" << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}
			else if (rVal == -1)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. Back socket error:inet_pton.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}
		}

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
			ss << "FrontSevEv. Back bind Error! Code:" << std::to_string(Err) << " Line: " << __LINE__ << "\r\n";
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
				ss << "FrontSevEv. Back connect Error. Code :" << std::to_string(Err) << " Line : " << __LINE__ << "\r\n";
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
			ss << "FrontSevEv. Back WSAEventSelect. Code:" << to_string(Err) << " Line:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			return FALSE;
		}

		//コールバックイベントの設定
		if (!(pBackSocket->pTPWait = CreateThreadpoolWait(OnBackEvSocketCB, pBackSocket, &*pcbe)))
		{
			DWORD Err = GetLastError();
			stringstream  ss;
			ss << "FrontSevEv. Back CreateThreadpoolWait. CODE:" << to_string(Err) << " Line:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			return FALSE;
		}
		SetThreadpoolWait(pBackSocket->pTPWait, pBackSocket->hEvent.get(), NULL);
		return TRUE;
	}

	void BackClose()
	{
		for (u_int i = 0; i < NUM_BACK_CONNECTION; ++i)
		{
			BackContext* pBackSocket = BackContextPool.Pull();
			gLostBackSocket;

			if (pBackSocket->hSocket)
			{
				pBackSocket->ReInitialize();
			}
		}
	}
}