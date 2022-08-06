//Copyright (c) 2021, Gold Smith
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
	std::atomic_uint gTotalConnected(0);
	std::atomic_uint gCDel(0);
	std::atomic_uint gMaxConnecting(0);
	SocketContext gSockets[ELM_SIZE];
	PTP_TIMER gpTPTimer(NULL);

	binary_semaphore gvPoollock(1);
	binary_semaphore gvConnectedlock(1);

	RingBuf<SocketContext> gSocketsPool(gSockets,ELM_SIZE);

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
			Make1000mSecFileTime(gp1000msecFT);
			return gp1000msecFT;
		}()
	,[](_Inout_ FILETIME* gp1000msecFT)
		{
				delete gp1000msecFT;
		}
	};


	VOID OnListenCompCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
	{
		//���b�X���\�P�b�g
		SocketListenContext* pListenSocket = (SocketListenContext*)Context;
		//�A�N�Z�v�g�\�P�b�g
		SocketContext* pSocket = (SocketContext*)Overlapped;
		PreAccept(pListenSocket);

		DWORD dw=WaitForSingleObject(pListenSocket->hEvent, 0);
		if (dw == WAIT_OBJECT_0)
		{
			MyTRACE( ("pListenSocket Event:" + to_string(dw)+"\r\n"
				).c_str());
		}
		WSAResetEvent(pListenSocket->hEvent);
		dw = WaitForSingleObject(pSocket->hEvent, 0);
		if (dw == WAIT_OBJECT_0)
		{
			int i = 0;
		}
		WSAResetEvent(pListenSocket->hEvent);
		//�g�[�^���R�l�N�g�J�E���g�A�}�b�N�X�R�l�N�e�B���O�J�E���g�L�^�B
		gMaxConnecting.store(std::max< std::atomic_uint>(std::atomic_uint(++gTotalConnected - gCDel), gMaxConnecting.load()));

		//�G���[�m�F
		if (IoResult)
		{
			//CancelIOEX�����s���ꂽ�B
			if (IoResult == ERROR_OPERATION_ABORTED)
			{
				//�N���[���A�b�v
				pSocket->ReInitialize();
				gSocketsPool.Push(pSocket);
				//���b�X�������|�[�g���s�B
				StartThreadpoolIo(Io);
				return;
			}
			MyTRACE(("Err! OnListenCompCB Result:"+  to_string(IoResult) + " Line:" +to_string(__LINE__) + "\r\n").c_str());
			cout << "End Listen\r\n";

			//�N���[���A�b�v
			CleanupSocket(pSocket);

			//���b�X�������|�[�g���s�B
			StartThreadpoolIo(Io);
			return;
		}
		//�O�Ȃ�ؒf
		if (!NumberOfBytesTransferred)
		{
			//�N���[���A�b�v
			CleanupSocket(pSocket);

			//���b�X�������|�[�g���s
			StartThreadpoolIo(Io);
			return;
		}
		else {
			pSocket->ReadBuf.resize(NumberOfBytesTransferred);
		}


		//���b�X�������|�[�g���s
		StartThreadpoolIo(Io);

		//�\�P�b�g�����[�X�I�v�V����
		BOOL yes = 1;
		if (setsockopt(pSocket->hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)))
		{
			pSocket->ReInitialize();
			gSocketsPool.Push(pSocket);
			++gCDel;
			std::cerr << "setsockopt Error! Line:" << __LINE__ << "\r\n";
			return;
		}

		//�A�N�Z�v�g�\�P�b�g�̃��b�Z�[�W����
		pSocket->RemBuf = pSocket->ReadBuf;
		pSocket->WriteBuf = SplitLastLineBreak(pSocket->RemBuf);
		if (pSocket->WriteBuf.size())
		{
			pSocket->WriteBuf += "\r\n";
			pSocket->StrToWsa(&pSocket->WriteBuf, &pSocket->wsaWriteBuf);
			pSocket->Dir = OL_SEND_CYCLE;
			pSocket->NumberOfBytesSent = 0;
			if (WSASend(pSocket->hSocket, &pSocket->wsaWriteBuf, 1, &pSocket->NumberOfBytesSent, 0/*dwflags*/, pSocket, NULL))
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					cerr << "WSASend err. Code:" << to_string(WSAGetLastError()) << " Line:" << to_string(__LINE__) << "\r\n";
					CleanupSocket(pSocket);

					return;
				}
			}
			pSocket->NumberOfBytesSent = 0;
		}
		//�A�N�Z�v�g�\�P�b�gIO�����|�[�g�ݒ�
		PTP_IO pTPioSocket(NULL);
		if (!(pTPioSocket = CreateThreadpoolIo((HANDLE)pSocket->hSocket, OnSocketNoticeCompCB, pSocket, &*pcbe)))
		{
			cerr << "CreateThreadpoolIo error! Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
			CleanupSocket(pSocket);
			return;
		}
		//�����|�[�g�X�^�[�g
		StartThreadpoolIo(pTPioSocket);

		//�A�N�Z�v�g�\�P�b�g��M����
		pSocket->Dir = OL_RECV_CYCLE;
		pSocket->NumberOfBytesRecvd = 0;
		pSocket->ReadBuf.resize(BUFFER_SIZE, '\0');
		pSocket->StrToWsa(&pSocket->ReadBuf, &pSocket->wsaReadBuf);
		if (WSARecv(pSocket->hSocket, &pSocket->wsaReadBuf, 1, &pSocket->NumberOfBytesRecvd, &pSocket->flags, pSocket, NULL))
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				cerr << "WSARecv err. Code:" << to_string(WSAGetLastError()) << " Line:" << to_string(__LINE__) <<  "\r\n";
				CleanupSocket(pSocket);
				return;
			}
		}

		return;
	}

	VOID OnSocketNoticeCompCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
	{
		SocketContext* pSocket = (SocketContext*)Context;

		DWORD dw = WaitForSingleObject(pSocket->hEvent, 0);
		if (dw == WAIT_OBJECT_0)
		{
			int i = 0;
		}

		//�G���[�m�F
		if (IoResult)
		{
			//IO�L�����Z���̈ג����I���B
			if (IoResult == ERROR_OPERATION_ABORTED)
			{
				CleanupSocket(pSocket);
				return;
			}

			MyTRACE(( "Err! OnSocketNoticeCompCB Code:" + to_string(IoResult) + " Line:" + to_string(__LINE__) + "\r\n").c_str());
			CloseThreadpoolIo(Io);
			CleanupSocket(pSocket);
			return;
		}

		//�ؒf���m�F�B
		if (!NumberOfBytesTransferred)
		{
//			MyTRACE("Socket Closed\r\n");
			//2�Ԗڂ̃f�[�^����M����O�ɁA�ؒf�����Ƃ����ɗ���B
			CloseThreadpoolIo(Io);
			CleanupSocket(pSocket);
			return;
		}

		//�Z���h�������m�F�B
		if (pSocket->Dir== OL_SEND_CYCLE)
		{
			MyTRACE((string("Completed Send:") + string(pSocket->wsaWriteBuf.buf, pSocket->NumberOfBytesSent)).c_str());
			pSocket->NumberOfBytesSent = 0;
			//�����܂łŁA�Z���h�̊��������I���B

			StartThreadpoolIo(Io);

			//��M�̏���
			pSocket->Dir = OL_RECV_CYCLE;
			pSocket->ReadBuf.resize(BUFFER_SIZE, '\0');
			pSocket->StrToWsa(&pSocket->ReadBuf, &pSocket->wsaReadBuf);
			pSocket->NumberOfBytesRecvd = 0;
			//���V�[�u
			if (WSARecv(pSocket->hSocket, &pSocket->wsaReadBuf, 1, &pSocket->NumberOfBytesRecvd, &pSocket->flags, pSocket, NULL))
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					cerr << "WSARecv err. Code:" << to_string(WSAGetLastError()) << " Line:" << to_string(__LINE__) << "\r\n";
					CloseThreadpoolIo(Io);
					CleanupSocket(pSocket);
					return;
				}
			}
			return;
		}else 
		//���V�[�u�̊������m�F�B
		if (pSocket->Dir== OL_RECV_CYCLE)
		{
			pSocket->RemBuf += {pSocket->wsaReadBuf.buf, NumberOfBytesTransferred};
			//�Ō�̉��s�����WriteBuf�ɓ����B
			pSocket->WriteBuf = SplitLastLineBreak(pSocket->RemBuf);

			//WriteBuf�ɒ��g������΁A�G�R�[���M�̊J�n
			if (pSocket->WriteBuf.size())
			{
				StartThreadpoolIo(Io);

				pSocket->WriteBuf += "\r\n";
				pSocket->StrToWsa(&pSocket->WriteBuf, &pSocket->wsaWriteBuf);
				pSocket->Dir = OL_SEND_CYCLE;
				pSocket->NumberOfBytesSent = 0;
				if (WSASend(pSocket->hSocket, &pSocket->wsaWriteBuf, 1, &pSocket->NumberOfBytesSent, 0/*dwflags*/, pSocket, NULL))
				{
					DWORD dw = WSAGetLastError();
					if (dw != WSA_IO_PENDING)
					{
						cerr << "WSASend err. Code:" << to_string(dw) << " Line:" << to_string(__LINE__) << "\r\n";
						CloseThreadpoolIo(Io);
						CleanupSocket(pSocket);
						return;
					}
				}
			}
			else {
				//WriteBuf�ɒ��g���Ȃ��ꍇ���M�͂��Ȃ��B�����|�[�g�X�^�[�g�B
				StartThreadpoolIo(Io);
				return;
			}

			return;
		}
		else {
			int i=0;//�ʏ킱���ɂ͗��Ȃ��B
		}
		return;
	}

	VOID MeasureConnectedPerSecCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
	{
		static u_int oldtime(0);
		u_int now(gID);
		if (now > 1)
		{
			std::atomic_uint* pgConnectPerSec = (std::atomic_uint*)Context;
			*pgConnectPerSec = __max(pgConnectPerSec->load(), now - oldtime);
		}
		oldtime = now;
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
//		PreAccept(gpListenContext);
	}


	int StartListen(SocketListenContext*pListenContext)
	{
		cout << "Start Listen\r\n";
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

		//���b�X���\�P�b�g�����|�[�g�쐬
		if (!(pListenContext->pTPListen = CreateThreadpoolIo((HANDLE)pListenContext->hSocket, OnListenCompCB, pListenContext, &*pcbe)))
		{
			cerr << "CreateThreadpoolIo error! Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
			return false;
		}
		StartThreadpoolIo(pListenContext->pTPListen);
		for (int i(0); i < PRE_ACCEPT; ++i)
		{
			PreAccept(pListenContext);
		}
		return 	true;
	}

	LPFN_ACCEPTEX GetAcceptEx(SocketListenContext* pListenSocket)
	{
		GUID GuidAcceptEx = WSAID_ACCEPTEX;
		int iResult = 0;
		static LPFN_ACCEPTEX lpfnAcceptEx(NULL);
		static SOCKET hSocket(NULL);
		DWORD dwBytes;
		if (!lpfnAcceptEx || hSocket!=pListenSocket->hSocket)
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

		CancelIoEx((HANDLE)pListen->hSocket, NULL);
		if (pListen->pTPListen)
		{
			pListen->pTPListen = NULL;
		}
	}

	void ShowStatus()
	{
		std::cout << "Total Connected: " << gTotalConnected << "\r" << std::endl;
		std::cout << "Current Connected: " << gTotalConnected - gCDel  << "\r" << std::endl;
		std::cout << "Max Connecting: " << SevOL::gMaxConnecting << "\r" << std::endl;
		std::cout << "Max Accepted/Sec: " << SevOL::gAcceptedPerSec << "\r" << std::endl;
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

	//WSABUF* CopyStdStringToWsaString(std::string strsrc, WSABUF* pWsaBuf)
	//{
	//	CopyMemory(pWsaBuf->buf, strsrc.data(), strsrc.length());
	//	pWsaBuf->len = strsrc.length();
	//	return pWsaBuf;
	//}

	bool DoSend(SocketContext* pSocket)
	{
		//�Z���h�����𑼂�CB�ɉ񂷁B
		pSocket->Dir = OL_SEND_CYCLE;
		if (WSASend(pSocket->hSocket, &pSocket->wsaWriteBuf, 1, NULL, 0/*dwflags*/, pSocket, NULL))
		{
			DWORD dw = WSAGetLastError();
			if (dw != WSA_IO_PENDING)
			{
				cerr << "WSASend err. Code:" << to_string(dw) << " Line:" << to_string(__LINE__)  << "\r\n";
				return false;
			}
			return true;
		}
		return true;
	}

	bool DoRecv(SocketContext* pSocket)
	{
		u_long flag =0 /*MSG_PUSH_IMMEDIATE*/;
		DWORD dw(NULL);
		pSocket->Dir = OL_RECV_CYCLE;
		if (WSARecv(pSocket->hSocket, &pSocket->wsaReadBuf, 1, NULL, &pSocket->flags, pSocket, NULL))
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				cerr << "WSARecv err. Code:" << to_string(WSAGetLastError()) << " Line:" << to_string(__LINE__) <<  "\r\n";

				return false;
			}
			return true;
		}
		return true;
	}

	void PreAccept(SocketListenContext* pListenSocket)
	{

		//�A�N�Z�v�g�p�\�P�b�g���o��
		SocketContext* pAcceptSocket = gSocketsPool.Pop();
		//�I�[�v���\�P�b�g�쐬
		if (!(pAcceptSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
			cerr << "Err WSASocket Code:" << to_string(WSAGetLastError()) << " Line:"<<__LINE__ << "\r\n";
		}

		//�f�o�b�N�pID�ݒ�
		pAcceptSocket->ID = gID++;

		//AcceptEx�̊֐��|�C���^���擾���A���s�B�p�����[�^�[�̓T���v���܂�܁B
		try {
			if (!(*GetAcceptEx(pListenSocket))(pListenSocket->hSocket, pAcceptSocket->hSocket, pAcceptSocket->ReadBuf.data(), BUFFER_SIZE - ((sizeof(sockaddr_in) + 16) * 2), sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, (OVERLAPPED*)pAcceptSocket))
			{
				DWORD dwCode = GetLastError();
				if (GetLastError() != ERROR_IO_PENDING)
				{
					throw std::runtime_error("AcceptEx return value error! Code:" + to_string(WSAGetLastError()) + " LINE:" + to_string(__LINE__) + "\r\n");
				}
			}
		}
		catch (std::exception& e) {
			// ��O��ߑ��A�G���[���R���o�͂���
			std::cerr << e.what() << std::endl;
			return ;
		}

		return ;
	}

	FILETIME* Make1000mSecFileTime(FILETIME* pFiletime)
	{
		ULARGE_INTEGER ulDueTime;
		ulDueTime.QuadPart = (ULONGLONG)-(1 * 10 * 1000 * 1000);
		pFiletime->dwHighDateTime = ulDueTime.HighPart;
		pFiletime->dwLowDateTime = ulDueTime.LowPart;
		return pFiletime;
	}

	//void SerializedSocketDebugPrint(SevOL::SocketContext* pSocket)
	//{
	//	PTP_WORK ptpwork(NULL);
	//	if (!(ptpwork = CreateThreadpoolWork(SerializedSocketDebugPrintCB
	//		, pSocket, &*pcbe)))
	//	{
	//		std::cerr << "Err" << __FUNCTION__ << __LINE__ << std::endl;
	//		return;
	//	}
	//	SubmitThreadpoolWork(ptpwork);
	//}

}