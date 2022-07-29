//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#include "SevWorkR.h"


namespace MainSevWorkR
{
	SocketContext gpSocket[ELM_SIZE];
	SocketContext gListenSocket;
	std::vector<SocketContext*> gpSocketPool = { []()->std::vector<SocketContext*> {std::vector<SocketContext*>vec;
	for (int i(0); i < ELM_SIZE; ++i)
	{
		vec.push_back(&gpSocket[i]);
	}
	return vec; }() };

	std::vector<SocketContext*> gpConnectedSockets;
	std::binary_semaphore gPoolLock(1);
	std::binary_semaphore gConnectLock(1);
	std::atomic_uint gID(0);
	std::atomic_uint gCDel(0);
	std::atomic_uint gMaxConnected(0);
	std::atomic_uint gMaxAcceptedPerSec(0);
	HANDLE ghThListen(NULL);
	HANDLE ghThSocket(NULL);
	extern HANDLE ghEvDoEnd;
	HANDLE ghEvDoCliEnd;

	HANDLE SevWork()
	{
		DWORD Err = 0;
		std::vector<SOCKET> SevSocketArray;
		if ((gListenSocket.hSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
		{
			std::cerr << "socket error! Line:" << __LINE__ << "\r\n";
			return NULL;
		}

		//ソケットリユースオプション
		BOOL yes = 1;
		if (setsockopt(gListenSocket.hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)))
		{
			std::cerr << "setsockopt Error! Line:" << __LINE__ << "\r\n";
			return NULL;
		}

		//ホストバインド設定
		struct sockaddr_in addr = { };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(50000);
		Err = inet_pton(AF_INET, "127.0.0.2", &(addr.sin_addr));
		if (Err != 1)
		{
			if (Err == 0)
			{
				fprintf(stderr, "socket error:Listen inet_pton return val 0\n");
				return NULL;
			}
			else if (Err == -1)
			{
				fprintf(stderr, "socket error:Listen inet_pton %d\n", WSAGetLastError());
				return NULL;
			}
		}
		if ((Err = bind(gListenSocket.hSocket, (struct sockaddr*)&(addr), sizeof(addr))) == SOCKET_ERROR)
		{
			Err = WSAGetLastError();
			std::cerr << "bind error! Code:" << std::to_string(Err) << " LINE:" << __LINE__ << "\r\n";
			return NULL;
		}

		//リッスン
		if (listen(gListenSocket.hSocket, SOMAXCONN))
		{
			Err = GetLastError();
			switch (Err)
			{
			case WSAEWOULDBLOCK:
				break;
			default:
				std::cerr << "listen error! Code: " << std::to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				return NULL;
			}
		}

		std::cout << "Listen start.\r\n";

		if (!(ghEvDoCliEnd = CreateEvent(NULL, TRUE, FALSE, NULL)))
		{
			std::cerr << "CreateEvent error! Line:" << __LINE__ << "\r\n";
		}

		if (!(ghThListen = (HANDLE)_beginthreadex(NULL, NULL, thprocListen, (void*)&gListenSocket, 0, NULL)))
		{
			std::cerr << "error! _beginthreadex. Line:" << __LINE__ << "\r\n";
			return NULL;
		}
		if (!(ghThSocket = (HANDLE)_beginthreadex(NULL, NULL, thprocSocket, (void*)NULL, 0, NULL)))
		{
			std::cerr << "error! _beginthreadex. Line:" << __LINE__ << "\r\n";
			return NULL;
		}

		return (HANDLE)gListenSocket.hSocket;
	}

//リッスンスレッド
	u_int thprocListen(void* Context)
	{
		SocketContext* pListen = (SocketContext*)Context;

		//測定用タイマーの設定
		HANDLE hTimer;
		if (!(hTimer = CreateWaitableTimer(NULL, FALSE, NULL)))
		{
			std::cerr << "CreateWaitableTimer error! Line:" << __LINE__ << "\r\n";
			return NULL;
		}

		FILETIME ft{};
		Make1000mSecFileTime(&ft);
		SetWaitableTimer(
			hTimer,           // Handle to the timer object
			(LARGE_INTEGER*) & ft,       // When timer will become signaled
			1000,             // Periodic timer interval of 2 seconds
			TimerAPCProc,     // Completion routine
			NULL,          // Argument to the completion routine
			FALSE);          // Do not restore a suspended system


		//メインループ
		for (;;)
		{
			DWORD dw = WaitForSingleObject(ghEvDoEnd, 0);
			switch(dw)
			{
			case WAIT_OBJECT_0:
				break;
			case WAIT_TIMEOUT:
			default:;
			}
			sockaddr  SevSocAddr{};
			int Len = sizeof(sockaddr);
			SOCKET hSocket = accept(pListen->hSocket, &(SevSocAddr), &Len);
			if (hSocket == INVALID_SOCKET)
			{
				int Err = GetLastError();
				if (Err == WSAEWOULDBLOCK)
				{
					continue;
				}
				else if (Err == WSAEINTR)
				{
					break;
				} else{
					break;
				}
			}
			else {
				std::atomic_uint uID = gID++;
				gPoolLock.acquire();
				SocketContext* pSocket = gpSocketPool.front();

				pSocket->ID = uID;
				//ノンブロックに変更
				u_long flag = 1;
				if (ioctlsocket(hSocket, FIONBIO, &flag) == SOCKET_ERROR)
				{
					std::cerr << "(hSocket, FIONBIO, &flag) error! Line:" << __LINE__ << "\r\n";
					closesocket(hSocket);
					gPoolLock.release();
					++gCDel;
					return false;
				}
				pSocket->hSocket = hSocket;
				gpSocketPool.erase(gpSocketPool.begin());
				gPoolLock.release();
				gConnectLock.acquire();
				gpConnectedSockets.push_back(pSocket);
				gConnectLock.release();
			}
		}

		//終了処理
		shutdown(pListen->hSocket, SD_SEND);
		SetEvent(ghEvDoCliEnd);
		WaitForSingleObject(ghThSocket, INFINITE);
		CloseHandle(ghThSocket);
		CloseHandle(ghEvDoCliEnd);
		_endthreadex(0);
		return 0;
	}

//接続ソケットスレッド
	u_int thprocSocket(void*)
	{
		//メイン作業ループ
		for (;;)
		{
			DWORD dw = WaitForSingleObject(ghEvDoCliEnd, 0);
			if (dw == WAIT_TIMEOUT/*258*/)
			{	;}
			else if (dw ==  WAIT_OBJECT_0)
			{
				break;
			} else {
				break;
			}
			gConnectLock.acquire();
			for (auto it = gpConnectedSockets.begin(); it != gpConnectedSockets.end();) {
				std::string str(BUFFER_SIZE, '\0');

				//レシーブ
				size_t size = (recv((*it)->hSocket, str.data(), str.size(), 0));

				if (size == SOCKET_ERROR)
				{
					int Err = WSAGetLastError();
					switch (Err)
					{
					case WSAEWOULDBLOCK:
						++it;
						continue;
					default:
						break;
					}

					//ソケットエラーで削除
					(*it)->ReInit();
					gPoolLock.acquire();
					gpSocketPool.push_back(*it);
					gPoolLock.release();
					it = gpConnectedSockets.erase(it);
					++gCDel;
					continue;  // ここでitはすでに次の要素を指しているので、インクリメント不要！
				}
				else if (size == 0)
				{
					//切断されているので削除
					(*it)->ReInit();
					gPoolLock.acquire();
					gpSocketPool.push_back(*it);
					gPoolLock.release();
					it = gpConnectedSockets.erase(it);
					++gCDel;
					continue;
				}
				else {
					//解析及び送信
					str.resize(size);
					(*it)->buflock.acquire();
					(*it)->Buffer = (*it)->Buffer + str;
					std::vector<std::string> vstr = SplitLineBreak((*it)->Buffer);
					(*it)->buflock.release();
					for (std::string s : vstr)
					{
						s += "\r\n";
						send((*it)->hSocket, s.data(), s.size(), 0);
					}
					it++;
				}
			}
			gConnectLock.release();
		}

		//ループ終了後の後処理
		gConnectLock.acquire();
		gPoolLock.acquire();
		//プールに移す
		for (SocketContext* p : gpConnectedSockets)
		{
			p->ReInit();
			gpSocketPool.push_back(p);
			++gCDel;
		}
		gpConnectedSockets.clear();
		gPoolLock.release();
		gConnectLock.release();
//		SetEvent(*gphEvConnectFins);
		_endthreadex(0);
		return 0;
	}

//改行ごとにベクターに格納
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

	void ShowStatus()
	{
		std::cout << "Total Connected: " << gID << "\r" << std::endl;
		std::cout << "Current Connected: " << gID - gCDel << "\r\n";
		std::cout << "Max Connecting: " << gMaxConnected << "\r" << std::endl;
		std::cout << "Max Accepted/Sec: " << gMaxAcceptedPerSec << "\r" << std::endl;
	}

	VOID TimerAPCProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
	{
		static std::atomic_uint uOldConnect(0);
		gMaxConnected.store( __max(gMaxConnected.load(), gID.load()-gCDel.load()));
		gMaxAcceptedPerSec.store(__max(gID.load() - uOldConnect.load(),gMaxAcceptedPerSec.load()));
		uOldConnect.store(gID.load());
		return VOID();
	}

	FILETIME* Make1000mSecFileTime(FILETIME* pFiletime)
	{
		ULARGE_INTEGER ulDueTime;
		ulDueTime.QuadPart = (ULONGLONG)-(1 * 10 * 1000 * 1000);
		pFiletime->dwHighDateTime = ulDueTime.HighPart;
		pFiletime->dwLowDateTime = ulDueTime.LowPart;
		return pFiletime;
	}

}
