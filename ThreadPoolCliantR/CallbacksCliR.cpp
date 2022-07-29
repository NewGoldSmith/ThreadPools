//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Cliant side
#include "CallbacksCliR.h"

namespace ThreadPoolCliantR {

	std::atomic_uint gMaxConnect{};
	std::atomic_uint gCDel{};
	std::atomic_uint gID{};
	std::atomic<std::time_t> gtMinRepTime{ LLONG_MAX };
	std::atomic<std::time_t> gtMaxRepTime{};
	class SocketContext gSockets[ELM_SIZE];
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
		< FILETIME
		, void (*)(FILETIME*)
		> p100msecFT
	{ []()
		{
			const auto p100msecFT = new FILETIME;
			MakeNmSecFileFime(p100msecFT,100);
			return p100msecFT;
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
			Err = WSAGetLastError();
			std::cerr << "Socket Err: " << Err << "FILE NAME: " << __FILE__ << " LINE: " << __LINE__ <<" Code:"<<WSAGetLastError()<< "\r\n";
			CloseThreadpoolWait(Wait);
			CloseSocketContext(pSocket);
			return;
		}

		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			//リードロック
			pSocket->readlock.acquire();


			//レシーブ
			std::string strDisplay(BUFFER_SIZE, '\0');
			strDisplay.resize(recv(pSocket->hSocket, strDisplay.data(), strDisplay.size(), 0));

			MyTRACE(("CliID:"+std::to_string(pSocket->ID) + " Received: "+strDisplay).c_str());
			
			if (strDisplay.size() == SOCKET_ERROR)
			{
				strDisplay.clear();
				Err = WSAGetLastError();
				std::cerr << "Socket Err: " << Err << "FILE NAME: " << __FILE__ << " LINE: " << __LINE__ << std::endl;
				pSocket->readlock.release();
				CloseThreadpoolWait(Wait);
				CloseSocketContext(pSocket);
				return;
			}
			else if (strDisplay.size() == 0)
			{
				pSocket->readlock.release();
				CloseThreadpoolWait(Wait);
				CloseSocketContext(pSocket);
				return;
			}
			else {
				pSocket->ReadString += strDisplay;
				std::vector<std::string> v = SplitLineBreak(pSocket->ReadString);
				pSocket->readlock.release();

				pSocket->vstrlock.acquire();
				for (std::string& s : v) {
					//レスポンス計測用。
					::GetLocalTime(&pSocket->tRecv[N_COUNTDOWNS - FindCountDown(s)]);

					std::string str;
					str = "CliID:" + std::to_string(pSocket->ID) + " Recieved Message: " + s + "\r\n";
					pSocket->vstr.push_back(str);
				}
				pSocket->vstrlock.release();

				PTP_WORK pTPWork(NULL);
				if (!(pTPWork = CreateThreadpoolWork(SerializedDisplayCB, pSocket, &*pcbe)))
				{
					std::cerr << "err! " << __FILE__<<" "<<__LINE__ << "\r\n";
				}
				else {
					SubmitThreadpoolWork(pTPWork);
				}

				if (pSocket->CountDown == 0)
				{
					CloseThreadpoolWait(Wait);
					CloseSocketContext(pSocket);
					return;
				}
			}
		}

		if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			std::cout << "Socket is Closed " << "SocketID: " << pSocket->ID << std::endl;
			CloseThreadpoolWait(Wait);
			CloseSocketContext(pSocket);
			return;
		}
		//接続ソケット待機イベント再設定。
		SetThreadpoolWait(Wait, pSocket->hEvent, NULL);
	}

	std::vector<std::string> SplitLineBreak(std::string& strDisplay)
	{
		std::vector<std::string> v;
		for (;;)
		{
			std::string::size_type pos = strDisplay.find("\n");
			if (pos != std::string::npos)
			{
				std::string s = strDisplay.substr(0, pos);
				if (*(s.end() - 1) == '\r')
				{
					s.resize(s.size() - 1);
				}
				v.push_back(s);
				strDisplay.erase(strDisplay.begin(), strDisplay.begin() + pos + 1);
			}
			else {
				break;
			}
		}
		return v;
	}

	int TryConnect()
	{
		using namespace ThreadPoolCliantR;
		PTP_WORK ptpwork(NULL);
		for (int i = 0; i < 2; ++i)
		{
			if (!(ptpwork = CreateThreadpoolWork(TryConnectCB, (PVOID)100, &*pcbe)))
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
		std::time_t t = gtMinRepTime;
		std::cout << "Min Responce msec:" << std::to_string(gtMinRepTime
		) << "\r\n";
		t = gtMaxRepTime;
		std::cout << "Max Responce msec:" << std::to_string(gtMaxRepTime
		) << "\r\n";
	}

	void StartTimer(SocketContext* pSocket)
	{
		pSocket->CountDown = N_COUNTDOWNS;
		MakeAndSendSocketMessage(pSocket);
		PTP_TIMER pTPTimer(0);
		if (!(pTPTimer = CreateThreadpoolTimer(OneSecTimerCB, pSocket, &*pcbe)))
		{
			std::cerr << "err CreateThreadpoolTimer. Line: " << __LINE__ << "\r\n";
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

	FILETIME* MakeNmSecFileFime(FILETIME* pfiletime, u_int ntime)
	{
		ULARGE_INTEGER ulDueTime;
		ulDueTime.QuadPart = (ULONGLONG)-(1 * 10 * 1000)*ntime;
		pfiletime->dwHighDateTime = ulDueTime.HighPart;
		pfiletime->dwLowDateTime = ulDueTime.LowPart;
		return pfiletime;
	}


	VOID OneSecTimerCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER pTPTimer)
	{
		using namespace ThreadPoolCliantR;
		ThreadPoolCliantR::SocketContext* pSocket = (ThreadPoolCliantR::SocketContext*)Context;
		--pSocket->CountDown;
		MakeAndSendSocketMessage(pSocket);
		if (pSocket->CountDown > 0)
		{
			SetThreadpoolTimer(pTPTimer, &*p1000msecFT, 0, 0);
		}
		else {
			//スレッドプールタイマー終了
			SetThreadpoolTimer(pTPTimer, NULL, 0, 0);
			CloseThreadpoolTimer(pTPTimer);

			//レスポンスデータを計算
			gtMaxRepTime.store(__max(gtMaxRepTime.load(), pSocket->GetMaxResponce()));
			gtMinRepTime.store(__min(gtMinRepTime.load(), pSocket->GetMinResponce()));
		}
		return VOID();
	}

	VOID LateOneTempoCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
	{
		using namespace ThreadPoolCliantR;
		StartTimer((SocketContext*)Context);
		CloseThreadpoolTimer(Timer);
		return VOID();
	}

	VOID SerializedDisplayCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		static	std::binary_semaphore oslock{ 1 };
		oslock.acquire();
		{
			SocketContext* pSocket = (SocketContext*)Context;
			pSocket->vstrlock.acquire();
			for (std::string& str : pSocket->vstr)
			{
				std::cout << str<<std::flush;
			}
			pSocket->vstr.clear();
			pSocket->vstrlock.release();
			CloseThreadpoolWork(Work);
		}
		oslock.release();
	}

	VOID CloseSocketCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
	{
		SocketContext* pSocket = (SocketContext*)Context;
		pSocket->ReInitialize();
		++gCDel;
		if (!(gID - gCDel))
		{
			std::cout << "End Work" << "\r\n" << std::flush;

			//ステータスを表示
			std::cout << "\r\nstatus\r\n";
			ShowStatus();
		}
		CloseThreadpoolTimer(Timer);
	}

	VOID TryConnectCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		using namespace ThreadPoolCliantR;
		for (u_int i = 0; i < (u_int)Context; ++i)
		{
			std::atomic_uint index(gID++);
			SocketContext* pSocket = &gSockets[index];
			pSocket->ID = index;

			if (((pSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, NULL/*WSA_FLAG_OVERLAPPED*/)) == INVALID_SOCKET))
			{
				++gCDel;
				std::cout << "WSASocket Error! "<<"Line: "<< __LINE__<<"\r\n";
				break;
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
					++gCDel;
					break ;
				}
				else if (rVal == -1)
				{
					Err = WSAGetLastError();
					std::cerr << "socket error:inet_pton.Code:" << std::to_string(Err) << "\r\n";
					++gCDel;
					break ;
				}
			}
			if (::bind(pSocket->hSocket, (struct sockaddr*)&(addr), sizeof(addr)) == SOCKET_ERROR)
			{
				Err = WSAGetLastError();
				std::cerr << "bind Error! Code:"<<std::to_string(Err) << " Line: " << __LINE__ << "\r\n";
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
					++gCDel;
					break ;
				}
				else if (rVal == -1)
				{
					Err = WSAGetLastError();
					std::cerr << "socket error:inet_pton.Code:" << std::to_string(Err) << "\r\n";
					++gCDel;
					break ;
				}
			}

			//コネクト
			if (connect(pSocket->hSocket, (SOCKADDR*)&Peeraddr, sizeof(Peeraddr)) == SOCKET_ERROR)
			{
				if ((Err = WSAGetLastError()) != WSAEWOULDBLOCK)
				{
					std::cerr <<"connect Error. Code :" << std::to_string(Err) << " FILE:" << __FILE__ << " Line : " << __LINE__ << "\r\n";
					pSocket->hSocket = NULL;
					++gCDel;
					break;
				}
			}

			gMaxConnect = __max(gMaxConnect.load(), gID - gCDel);

			//ソケットのイベント設定
			if (WSAEventSelect(pSocket->hSocket, pSocket->hEvent, /*FD_ACCEPT |*/ FD_CLOSE | FD_READ/* | FD_CONNECT | FD_WRITE*/))
			{
				std::cerr << "Error WSAEventSelect. Line:" << __LINE__ << "\r\n";
				++gCDel;
				break;
			}
			
			//イベントハンドラの設定
			PTP_WAIT pTPWait(NULL);
			if (!(pTPWait = CreateThreadpoolWait(OnEvSocketCB, pSocket, &*ThreadPoolCliantR::pcbe)))
			{
				std::cerr << "Error CreateThreadpoolWait. Line:" << __LINE__ << "\r\n";
				++gCDel;
				break;
			}
			else {
				SetThreadpoolWait(pTPWait, pSocket->hEvent, NULL);
			}
			//コネクトしてからsendまでワンテンポ遅らせないと、サーバーの取りこぼしが起こる可能性がある。
			PTP_TIMER pTPTimer(NULL);
			if (!(pTPTimer = CreateThreadpoolTimer(LateOneTempoCB, pSocket, &*pcbe)))
			{
				std::cerr << "Error CreateThreadpoolTimer. Line:" << __LINE__ << "\r\n";
				++gCDel;
				break;
			}
			SetThreadpoolTimer(pTPTimer, &*p100msecFT, 0, NULL);
		}
		CloseThreadpoolWork(Work);
	}

	void MakeAndSendSocketMessage(SocketContext* pSocket)
	{
		//ライトロック
		pSocket->writelock.acquire();
		pSocket->WriteString = "CliID:"+ std::to_string( pSocket->ID) + " NumCount:" +std::to_string( pSocket->CountDown)+"\r\n";

		PTP_WORK pTPWork(NULL);
		if (!(pTPWork = CreateThreadpoolWork(SerializedDisplayCB, (void*)pSocket, &*pcbe)))
		{
			std::cerr << "Err" << __FUNCTION__ << __LINE__ << std::endl;
			return;
		}
		SubmitThreadpoolWork(pTPWork);

		MyTRACE(("CliID:" + std::to_string(pSocket->ID) + " Send    : " + pSocket->WriteString).c_str());

		//レスポンス測定用。
		::GetLocalTime(&pSocket->tSend[N_COUNTDOWNS - pSocket->CountDown]);

		send(pSocket->hSocket, pSocket->WriteString.data(), pSocket->WriteString.length(), 0);
		pSocket->WriteString.clear();
		pSocket->writelock.release();
	}

	u_int FindCountDown(const std::string& str)
	{
		std::string s(str);
		size_t it2=s.rfind("\r\n");
		size_t it1 = s.rfind(":", it2);
		std::string substr = s.substr(it1+1, it2-it1-1);
		u_int uc(0);
		try {
			uc=std::stoi(substr);
		}
		catch (std::exception& e) {
			std::cout << e.what() << std::endl;
			std::abort();
		}
		return uc;
	}

	void CloseSocketContext(SocketContext* pSocket)
	{
		PTP_TIMER ptimer(NULL);
		if (!(ptimer=CreateThreadpoolTimer(CloseSocketCB, (void*)pSocket, &*pcbe)))
		{
			std::cerr << "Err" << __FUNCTION__ << __LINE__ << std::endl;
			return;
		}
		SetThreadpoolTimer(ptimer, &*p1000msecFT, 0, 0);
	}

	void SerializedDebugPrint(ThreadPoolCliantR::SocketContext* pSocket)
	{
		PTP_WORK ptpwork(NULL);
		if (!(ptpwork = CreateThreadpoolWork(SerializedDebugPrintCB
			, pSocket, &*pcbe)))
		{
			std::cerr << "Err" << __FUNCTION__ << __LINE__ << std::endl;
			return;
		}
		SubmitThreadpoolWork(ptpwork);
	}

	VOID SerializedDebugPrintCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		static std::binary_semaphore lock(1);
		lock.acquire();
		SocketContext* pSocket = (SocketContext*)Context;
		for (std::string& str : pSocket->vstr)
		{
			OutputDebugStringA(str.c_str());
		}
		lock.release();
		CloseThreadpoolWork(Work);
		return VOID();
	}

}