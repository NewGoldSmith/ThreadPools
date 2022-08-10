//Copyright (c) 2021, Gold Smith
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
	SocketContext gSockets[ELM_SIZE];
	RingBuf<SocketContext> gSocketsPool(gSockets, ELM_SIZE);
	const std::unique_ptr
		< FILETIME
		, void (*)(FILETIME*)
		> p1000msecFT
	{ []()
		{
			const auto p1000msecFT = new FILETIME;
			Make1000mSecFileTime(p1000msecFT);
			return p1000msecFT;
		}()
	,[](_Inout_ FILETIME* p1000msecFT)
		{
				delete p1000msecFT;
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
			pSocket->ReadString.resize(recv(pSocket->hSocket, pSocket->ReadString.data(), pSocket->ReadString.size(), 0));

			if (pSocket->ReadString.size() == SOCKET_ERROR)
			{
				pSocket->ReadString.clear();
				Err = WSAGetLastError();
				std::cerr << "Socket Err! Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				CloseThreadpoolWait(Wait);
				return;
			}
			else if (pSocket->ReadString.size() == 0)
			{
				CloseThreadpoolWait(Wait);
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				return;
			}
			else {
				//受信成功
				pSocket->RemString += pSocket->ReadString;
				pSocket->DispString = SplitLastLineBreak(pSocket->RemString);
				pSocket->DispString += "\r\n";
				PTP_WORK pTPWork(NULL);
#ifndef DISPLAY_SUPPRESSION
				//受信したデータを表示する。
				if (!(pTPWork = CreateThreadpoolWork(SerializedDisplayCB, pSocket, &*pcbe)))
				{
					std::cerr << "Err! CreateThreadpoolWork(SerializedDisplayCB, pSocket, &*pcbe). Line:" << to_string(__LINE__) << "\r\n";
				}
				else {
					SubmitThreadpoolWork(pTPWork);
				}
#endif
				//レスポンス測定用。
				u_int uiCount=FindAndConfirmCountDownNumber(pSocket->DispString);
				//見つけたカウントが範囲内か確認。
				if (0 <= uiCount && uiCount <= N_COUNTDOWNS) {
					GetSystemTimeAsFileTime(&pSocket->tRecv[N_COUNTDOWNS - uiCount]);
					if (uiCount == 0)
					{
						CloseThreadpoolWait(Wait);
						gtMaxRepTime.store(__max(gtMaxRepTime.load(), pSocket->GetMaxResponce()));
						gtMinRepTime.store(__min(gtMinRepTime.load(), pSocket->GetMinResponce()));

						pSocket->ReInitialize();
						gSocketsPool.Push(pSocket);
						++gCDel;
						if (!(gID - gCDel))
						{
							Sleep(1000);
							std::cout << "End Work" << "\r\n" << std::flush;

							//ステータスを表示
							std::cout << "\r\nstatus\r\n";
							ShowStatus();
						}

						return;
					}
				}
				else {
					cerr << "Err! Reply data is wrong.Socket ID:" << to_string(pSocket->ID) << "\r\n";
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
		PTP_WORK ptpwork(NULL);
		for (int i = 0; i < NUM_THREAD; ++i)
		{
			if (!(ptpwork = CreateThreadpoolWork(TryConnectCB, (PVOID)NUM_CONNECT, &*pcbe)))
			{
				std::cerr << "TryConnect Error.Line:" + __LINE__ << "\r\n";
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
	}

	void ClearStatus()
	{
		gID = 0;
		gMaxConnect = 0;
		gCDel = 0;
		gtMinRepTime = LLONG_MAX;
		gtMaxRepTime = 0;
		cout << "\r\n";
		ShowStatus();
	}

	void StartTimer(SocketContext* pSocket)
	{
		pSocket->CountDown = N_COUNTDOWNS;
		if (!MakeAndSendSocketMessage(pSocket))
		{
			++gCDel;
			pSocket->ReInitialize();
			gSocketsPool.Push(pSocket);
			return;
		}
		//レスポンス測定用。
		GetSystemTimeAsFileTime(&pSocket->tSend[N_COUNTDOWNS - pSocket->CountDown]);
		PTP_TIMER pTPTimer(0);
		if (!(pTPTimer = CreateThreadpoolTimer(OneSecTimerCB, pSocket, &*pcbe)))
		{
			std::cerr << "err CreateThreadpoolTimer. Line: " << __LINE__ << "\r\n";
			++gCDel;
			pSocket->ReInitialize();
			gSocketsPool.Push(pSocket);
			return;
		}
		SetThreadpoolTimer(pTPTimer, &*p1000msecFT, 0, NULL);
	}

	FILETIME* Make1000mSecFileTime(FILETIME*pfiletime)
	{
		ULARGE_INTEGER ulDueTime;
		ulDueTime.QuadPart = (ULONGLONG)-(1 * 10 * 1000 * 1000);
		pfiletime->dwHighDateTime = ulDueTime.HighPart;
		pfiletime->dwLowDateTime = ulDueTime.LowPart;
		return pfiletime;
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
			GetSystemTimeAsFileTime(&pSocket->tSend[N_COUNTDOWNS - pSocket->CountDown]);

			SetThreadpoolTimer(pTPTimer, &*p1000msecFT, 0, 0);
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
		for (u_int i = 0; i < (u_int)Context; ++i)
		{
//			std::atomic_uint index(gID++);
			SocketContext* pSocket = gSocketsPool.Pop();
			pSocket->ID = gID++;

			if (((pSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, NULL/*WSA_FLAG_OVERLAPPED*/)) == INVALID_SOCKET))
			{
				pSocket->hSocket = NULL;
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				++gCDel;
				std::cout << "WSASocket Error! "<<"Line: "<< __LINE__<<"\r\n";
				return;
			}

			//ホストバインド設定
			CHAR strHostAddr[] = "127.0.0.3";
			u_short usHostPort = 0;
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
					std::cerr<< "socket error:inet_pton input value invalided\r\n";
					pSocket->ReInitialize();
					gSocketsPool.Push(pSocket);
					++gCDel;
					break ;
				}
				else if (rVal == -1)
				{
					Err = WSAGetLastError();
					std::cerr << "socket error:inet_pton.Code:" << std::to_string(Err) << "\r\n";
					pSocket->ReInitialize();
					gSocketsPool.Push(pSocket);
					++gCDel;
					break ;
				}
			}
			if (::bind(pSocket->hSocket, (struct sockaddr*)&(addr), sizeof(addr)) == SOCKET_ERROR)
			{
				Err = WSAGetLastError();
				std::cerr << "bind Error! Code:"<<std::to_string(Err) << " Line: " << __LINE__ << "\r\n";
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				++gCDel;
				break ;
			}

			//サーバー接続用のadd_inを設定
			CHAR strPeerAddr[] = "127.0.0.2";
			u_short usPeerPort = 50000;
			struct sockaddr_in Peeraddr = { };
			Peeraddr.sin_family = AF_INET;
			Peeraddr.sin_port = htons(usPeerPort);
			int Peeraddr_size = sizeof(Peeraddr.sin_addr);
			rVal = inet_pton(AF_INET, strPeerAddr, &(Peeraddr.sin_addr));
			if (rVal != 1)
			{
				if (rVal == 0)
				{
					std::cerr << "socket error:inet_pton input value invalided\r\n";
					pSocket->ReInitialize();
					gSocketsPool.Push(pSocket);
					++gCDel;
					break ;
				}
				else if (rVal == -1)
				{
					Err = WSAGetLastError();
					std::cerr << "socket error:inet_pton.Code:" << std::to_string(Err) << "\r\n";
					pSocket->ReInitialize();
					gSocketsPool.Push(pSocket);
					++gCDel;
					break ;
				}
			}

			//コネクト
			if (connect(pSocket->hSocket, (SOCKADDR*)&Peeraddr, sizeof(Peeraddr)) == SOCKET_ERROR)
			{
				if ((Err = WSAGetLastError()) != WSAEWOULDBLOCK)
				{
					std::cerr <<"connect Error. Code :" << std::to_string(Err) <<  " Line : " << __LINE__ << "\r\n";
					pSocket->ReInitialize();
					gSocketsPool.Push(pSocket);
					++gCDel;
					break;
				}
			}

			gMaxConnect = __max(gMaxConnect.load(), gID - gCDel);

			//ソケットのイベント設定
			if (WSAEventSelect(pSocket->hSocket, pSocket->hEvent, /*FD_ACCEPT |*/ FD_CLOSE | FD_READ/* | FD_CONNECT | FD_WRITE*/))
			{
				std::cerr << "Error WSAEventSelect. Line:" << __LINE__ << "\r\n";
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				++gCDel;
				break;
			}
			
			//イベントハンドラの設定
			PTP_WAIT pTPWait(NULL);
			if (!(pTPWait = CreateThreadpoolWait(OnEvSocketCB, pSocket, &*ThreadPoolCliantR::pcbe)))
			{
				std::cerr << "Error CreateThreadpoolWait. Line:" << __LINE__ << "\r\n";
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

		MyTRACE(("CliID:" + to_string(pSocket->ID) + " Send    : " + pSocket->WriteString).c_str());

		if (send(pSocket->hSocket, pSocket->WriteString.data(), pSocket->WriteString.length(), 0) == SOCKET_ERROR)
		{
			cerr << "Err! Code:" << to_string(WSAGetLastError()) << " LINE;" << __LINE__ << "\r\n";
			return false;
		}
		pSocket->WriteString.clear();
		return true;
	}

	u_int FindAndConfirmCountDownNumber(const std::string& str)
	{
		std::string s(str);
		size_t it2=s.rfind("\r\n");
		size_t it1 = s.rfind(":", it2);
		std::string substr = s.substr(it1+1, it2-it1-1);
		u_int uc(0);
		try {
			uc=std::stoi(substr);
		}
		catch (const std::invalid_argument& e) {
			std::cerr << "Err! FindAndConfirmCountDownNumber. "<<e.what() << " File:" << __FILE__ << " LINE:" << __LINE__ << "\r\n";
			std::abort();
		}
		return uc;
	}

}