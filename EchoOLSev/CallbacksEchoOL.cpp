//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "CallbacksEchoOL.h"

using namespace std;

namespace EchoOLSev {

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
				cerr << "Err!GetConsoleMode"<<" LINE:"<<to_string(__LINE__)<<"\r\n";
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
		//���b�X���\�P�b�g
		SocketListenContext* pListenSocket = (SocketListenContext*)Context;
		//�A�N�Z�v�g�\�P�b�g
		SocketContext* pSocket = (SocketContext*)Overlapped;

		//�G���[�m�F
		if (IoResult)
		{
			MyTRACE(("Err! OLSev OnListenCompCB Result:" + to_string(IoResult) + " Line:" + to_string(__LINE__) + "SocketID:" + to_string(pSocket->ID) + "\r\n").c_str());
			cout << "End Listen\r\n";
			return;
		}

		//���b�X���̊����|�[�g�Đݒ�
		StartThreadpoolIo(Io);

		++gTotalConnected;
		gMaxConnecting.store(max(gMaxConnecting.load(), (gTotalConnected.load() - gCDel.load())));

		if (!NumberOfBytesTransferred)
		{
			MyTRACE(("OLSev OnListenCompCB Socket" + to_string(pSocket->ID) + " Closed\r\n").c_str());
			CleanupSocket(pSocket);
			//���̃A�N�Z�v�g
			if (!PreAccept(pListenSocket))
			{
				cerr << "Err! OnListenCompCB. PreAccept.LINE:" << __LINE__ << "\r\n";
				return;
			}
			return;
		}

		pListenSocket->ReadBuf.resize(NumberOfBytesTransferred);

		//�A�N�Z�v�g�\�P�b�g�t�����g�����|�[�g�ݒ�
		if (!(pSocket->pTPIo = CreateThreadpoolIo((HANDLE)pSocket->hSocket, OnSocketFrontNoticeCompCB, pSocket, &*pcbe)))
		{
			DWORD Err = GetLastError();
			cerr << "Err! OnListenCompCB. CreateThreadpoolIo. CODE:" << to_string(Err) << "\r\n";
			CleanupSocket(pSocket);
			return;
		}

		//�ǂݎ��f�[�^���A�N�Z�v�g�\�P�b�g�Ɉڂ��B
		pSocket->RemBuf = pListenSocket->ReadBuf;


		//���̃A�N�Z�v�g
		if (!PreAccept(pListenSocket))
		{
			cerr << "Err! OnListenCompCB. PreAccept.LINE:" << __LINE__ << "\r\n";
			return;
		}

		//���s�ŕ�����B
		pSocket->WriteBuf = SplitLastLineBreak(pSocket->RemBuf);
		//���M�ł�����̂�����Α��M
		if (pSocket->WriteBuf.size())
		{
			pSocket->WriteBuf += "\r\n";

			//�N���C�A���g�ւ̑��M
			if (!Send(pSocket))
			{
				CleanupSocket(pSocket);
				return;
			}
			//�łȂ���΁A�t�����g����̎�M�̐�
		}
		else {
			if (!Recv(pSocket))
			{
				CleanupSocket(pSocket);
				return;
			}
		}
		return;
	}

	VOID OnSocketFrontNoticeCompCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
	{
//		MyTRACE("Enter OnSocketFrontNoticeCompCB\r\n");
		SocketContext* pSocket = (SocketContext*)Context;

		//�G���[�m�F
		if (IoResult)
		{
			MyTRACE(("Err! OLSev OnSocketFrontNoticeCompCB Code:" + to_string(IoResult) + " Line:" + to_string(__LINE__) + " SocketID:" + to_string(pSocket->ID) + "\r\n").c_str());
			return;
		}

		//�ؒf���m�F�B
		if (!NumberOfBytesTransferred)
		{
			MyTRACE(("Socket ID:" + to_string(pSocket->ID) + " Closed\r\n").c_str());
			CleanupSocket(pSocket);
			return;
		}

		//���V�[�u�̊������m�F�B
		if (pSocket->Dir == SocketContext::eDir::DIR_TO_BACK)
		{
			pSocket->RemBuf += {pSocket->wsaReadBuf.buf, NumberOfBytesTransferred};
			pSocket->ReadBuf.clear();
			//�Ō�̉��s�����WriteBackBuf�ɓ����B
			pSocket->WriteBuf = SplitLastLineBreak(pSocket->RemBuf);

			//WriteBuf�ɒ��g������΁A�N���C�A���g�ւ̑��M�̊J�n
			if (pSocket->WriteBuf.size())
			{
				pSocket->WriteBuf += "\r\n";
				if (!Send(pSocket))
				{
					CleanupSocket(pSocket);
					return;
				}
			}
			else {
				//WriteBuf�ɒ��g���Ȃ��ꍇ���M�͂��Ȃ��B��M�����|�[�g�X�^�[�g�B
				if (!Recv(pSocket))
				{
					CleanupSocket(pSocket);
					return;
				}
			}
			return;
		}
		//���M�����B
		else if (pSocket->Dir == SocketContext::eDir::DIR_TO_FRONT)
		{
//			MyTRACE(("SevOL Front Sent:" + pSocket->WriteBuf).c_str());
			pSocket->WriteBuf.clear();
			//��M�����|�[�g�X�^�[�g�B
			if (!Recv(pSocket))
			{
				CleanupSocket(pSocket);
				return;
			}
		}
		else {
			int i = 0;//�ʏ킱���ɂ͗��Ȃ��B
		}
		return;
	}

	VOID SendWorkCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		CloseThreadpoolWork(Work);
		SocketContext* pSocket = (SocketContext*)Context;
		StartThreadpoolIo(pSocket->pTPIo);
		pSocket->Dir = SocketContext::eDir::DIR_TO_FRONT;
		pSocket->StrToWsa(&pSocket->WriteBuf, &pSocket->wsaWriteBuf);
		if (WSASend(pSocket->hSocket, &pSocket->wsaWriteBuf, 1, NULL, 0, pSocket, NULL))
		{
			DWORD Err = WSAGetLastError();
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
			MyTRACE("RecvWorkCB hSocket is NULL\r\n");
			return;
		}
		//��M�̐������B

		pSocket->Dir = SocketContext::eDir::DIR_TO_BACK;
		pSocket->ReadBuf.resize(BUFFER_SIZE, '\0');
		pSocket->StrToWsa(&pSocket->ReadBuf, &pSocket->wsaReadBuf);

		StartThreadpoolIo(pSocket->pTPIo);
		if (!WSARecv(pSocket->hSocket, &pSocket->wsaReadBuf, 1, NULL, &pSocket->flags, pSocket, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (Err != WSA_IO_PENDING && Err)
			{
				cerr << "Err! RecvWorkCB.Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				CleanupSocket(pSocket);
				return;
			}
		}
	}

	BOOL Send(SocketContext* pSocket)
	{
		StartThreadpoolIo(pSocket->pTPIo);
		pSocket->Dir = SocketContext::eDir::DIR_TO_FRONT;
		pSocket->StrToWsa(&pSocket->WriteBuf, &pSocket->wsaWriteBuf);
		if (WSASend(pSocket->hSocket, &pSocket->wsaWriteBuf, 1, NULL, 0, pSocket, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (Err != WSA_IO_PENDING && Err != 0) {
				cerr << "Err! Send. Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
				return FALSE;
			}
		}
		return TRUE;
	}

	BOOL Recv(SocketContext* pSocket)
	{
		StartThreadpoolIo(pSocket->pTPIo);
		pSocket->Dir = SocketContext::eDir::DIR_TO_BACK;
		pSocket->ReadBuf.resize(BUFFER_SIZE, '\0');
		pSocket->StrToWsa(&pSocket->ReadBuf, &pSocket->wsaReadBuf);
		if (!WSARecv(pSocket->hSocket, &pSocket->wsaReadBuf, 1, NULL, &pSocket->flags, pSocket, NULL))
		{
			DWORD Err = WSAGetLastError();
			if (Err != WSA_IO_PENDING && Err != 0)
			{
				cerr << "Err! Recv. Code:" << to_string(Err) << " LINE:" << __LINE__ << "\r\n";
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


	int StartListen(SocketListenContext* pListenContext)
	{
		cout << "Start Listen\r\n";
		cout << HOST_FRONT_LISTEN_BASE_ADDR << ":" << HOST_FRONT_LISTEN_PORT << "\r\n";
		//�f�o�b�N�p��ID������B���b�X���\�P�b�gID��0�B
		pListenContext->ID = gID++;

		//�\�P�b�g�쐬
		WSAPROTOCOL_INFOA prot_info{};
		pListenContext->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (!pListenContext->hSocket)
		{
			return S_FALSE;
		}

		//�\�P�b�g�����[�X�I�v�V����
		BOOL yes = 1;
		if (setsockopt(pListenContext->hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)))
		{
			++gCDel;
			std::cerr << "setsockopt Error! Line:" << __LINE__ << "\r\n";
		}

		//�z�X�g�o�C���h�ݒ�
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

		//���b�X��
		if (listen(pListenContext->hSocket, SOMAXCONN)) {
			cerr << "Err listen Code:" << to_string(WSAGetLastError()) << " Line:" << __LINE__ << "\r\n";
			return false;
		}


		// Accepted/sec����p�^�C�}�[�R�[���o�b�N�ݒ�
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

		//���b�X���\�P�b�g�����|�[�g�쐬
		if (!(pListenContext->pTPListen = CreateThreadpoolIo((HANDLE)pListenContext->hSocket, OnListenCompCB, pListenContext, &*pcbe))) {
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
				// ��O��ߑ�
				// �G���[���R���o�͂���
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
				// ��O��ߑ�
				// �G���[���R���o�͂���
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
		std::cout << "Current Connecting: " << gTotalConnected - gCDel << "\r\n";
		std::cout << "Max Connecting: " << gMaxConnecting << "\r\n";
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

		//�A�N�Z�v�g�p�\�P�b�g���o��
		SocketContext* pAcceptSocket = gSocketsPool.Pull();
		//�I�[�v���\�P�b�g�쐬
		if (!(pAcceptSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
			cerr << "Err WSASocket Code:" << to_string(WSAGetLastError()) << " Line:" << __LINE__ << "\r\n";
			return false;
		}

		//�f�o�b�N�pID�ݒ�
		pAcceptSocket->ID = gID++;

		//AcceptEx�̊֐��|�C���^���擾���A���s�B�p�����[�^�[�̓T���v���܂�܁B
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