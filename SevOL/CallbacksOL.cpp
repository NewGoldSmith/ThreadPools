//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "CallbacksOL.h"

using namespace std;
using namespace SevOL;
namespace SevOL {
	std::atomic_uint gAcceptedPerSec(0);
	std::atomic_uint gID(0);
	std::atomic_uint gCDel(0);
	std::atomic_uint gMaxConnect(0);
	struct SocketContext gSockets[ELM_SIZE];
	SocketListenContext* gpListenSocket(NULL);
	PTP_TIMER gpTPTimer(NULL);

	vector<SocketContext*> gvSocketsPool;
	binary_semaphore gvPoollock(1);
	binary_semaphore gvConnectedlock(1);
	extern const std::unique_ptr
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

	extern const std::unique_ptr
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
		//�A�N�Z�v�g�\�P�b�g
		SocketListenContext* pListenSocket = (SocketListenContext*)Context;
		SocketContext* pSocket = (SocketContext*)Overlapped;
		//�G���[�m�F
		if (IoResult)
		{
			//IO�L�����Z���̈ג����I���B
			if (IoResult == ERROR_OPERATION_ABORTED)
			{
				return;
			}
			MyTRACE(("Err! OnListenCompCB Result:"+  to_string(IoResult) + " Line:" +to_string(__LINE__) + "\r\n").c_str());
			cout << "End Listen\r\n";
			return;
		}
		//�O�Ȃ�ؒf
		if (!NumberOfBytesTransferred)
		{
			CreanupAndPushSocket(pSocket);
			//�����|�[�g���s
			StartThreadpoolIo(Io);
			return;
		}
		else {
			pSocket->ReadBuf.resize(NumberOfBytesTransferred);
		}

		//�����|�[�g���s
		StartThreadpoolIo(Io);

		//�A�N�Z�v�g�\�P�b�g�̃��b�Z�[�W����
		pSocket->RemBuf = pSocket->ReadBuf;
		string substr = SplitLastLineBreak(pSocket->RemBuf);
		if (substr.size())
		{
			substr += "\r\n";
			CopyStdStringToWsaString(substr, &pSocket->wsaWriteBuf);
			pSocket->usCycle = OL_WRITE_CYCLE;
			if (WSASend(pSocket->hSocket, &pSocket->wsaWriteBuf, 1, NULL, 0/*dwflags*/, pSocket, NULL))
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					cerr << "WSASend err. Code:" << to_string(WSAGetLastError()) << " Line:" << to_string(__LINE__) << " File:" << __FILE__ << "\r\n";
					return;
				}
			}
		}
		//�A�N�Z�v�g�\�P�b�gIO�����|�[�g�ݒ�
		PTP_IO pTPioSocket(NULL);
		if (!(pTPioSocket = CreateThreadpoolIo((HANDLE)pSocket->hSocket, OnSocketNoticeCompCB, pSocket, &*pcbe)))
		{
			cerr << "CreateThreadpoolIo error! Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
			return;
		}
		//�����|�[�g�X�^�[�g
		StartThreadpoolIo(pTPioSocket);
		//�A�N�Z�v�g�\�P�b�grecv
		pSocket->usCycle = OL_READ_CYCLE;
		SocketInitWsaBuf(&pSocket->wsaReadBuf);
		if (WSARecv(pSocket->hSocket, &pSocket->wsaReadBuf, 1, &pSocket->NumberOfBytesRecvd, &pSocket->flags, (OVERLAPPED*)pSocket, NULL))
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				cerr << "WSARecv err. Code:" << to_string(WSAGetLastError()) << " Line:" << to_string(__LINE__) << " File:" << __FILE__ << "\r\n";

				return;
			}
		}

		return;
	}

	VOID OnSocketNoticeCompCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
	{
		SocketContext* pSocket = (SocketContext*)Context;
		//�G���[�m�F
		if (IoResult)
		{
			//IO�L�����Z���̈ג����I���B
			if (IoResult == ERROR_OPERATION_ABORTED)
			{
				return;
			}
			cerr << "Err! OnSocketNoticeCompCB Code:" << to_string(IoResult) << " Line:" << __LINE__ << "\r\n";
			CloseThreadpoolIo(Io);
			CreanupAndPushSocket(pSocket);
			return;
		}

		//�ؒf���m�F�B
		if (!NumberOfBytesTransferred)
		{
//			MyTRACE("Socket Closed\r\n");
			CloseThreadpoolIo(Io);
			CreanupAndPushSocket(pSocket);
			return;
		}

		//�Z���h�̊������m�F�B
		if (pSocket->usCycle== OL_WRITE_CYCLE)
		{
			MyTRACE((string("Completed Send:") + string(pSocket->wsaWriteBuf.buf, pSocket->wsaWriteBuf.len)).c_str());
			SocketInitWsaBuf( &pSocket->wsaReadBuf);
			if (!DoRecv(pSocket)) {
				cerr << "DoRecv Error.Line:" << to_string(__LINE__) << "\r\n";
			}
			//�X�^�[�g�����|�[�g
			(void)StartThreadpoolIo(Io);
			return;
		}
		//���V�[�u�̊������m�F�B
		else if (pSocket->usCycle == OL_READ_CYCLE)
		{
			string strsrc(pSocket->wsaReadBuf.buf, NumberOfBytesTransferred);
			string remstr(pSocket->wsaRemBuf.buf, pSocket->wsaRemBuf.len);
			remstr += strsrc;
			string strdst = SplitLastLineBreak(remstr);
			CopyStdStringToWsaString(remstr, &pSocket->wsaRemBuf);
			if (strdst.size())
			{
				strdst += "\r\n";
				CopyStdStringToWsaString(strdst, &pSocket->wsaWriteBuf);
				//�X�^�[�g�����|�[�g
				(void)StartThreadpoolIo(Io);
				DoSend(pSocket);
				return;
			}
			DoRecv(pSocket);
			//�X�^�[�g�����|�[�g
			(void)StartThreadpoolIo(Io);
			return;
		}
		else
		{
			//�ʏ�͂����ɂ͗��Ȃ��B
			cerr << "OnSocketNoticeCompCB No direction.\r\n";
		}
		return VOID();
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

	void InitSocketPool()
	{
		gvSocketsPool.reserve(ELM_SIZE);
		for (u_int ui(0); ui < ELM_SIZE; ++ui)
		{
			gvSocketsPool.push_back(&gSockets[ui]);
		}
	}

	void CreanupAndPushSocket(SocketContext* pSocket)
	{
		pSocket->ReInitialize();
		gvSocketsPool.push_back(pSocket);
	}

	SocketContext* PopSocket()
	{
		SocketContext* pSocket = gvSocketsPool.front();
		erase(gvSocketsPool,pSocket);
		return pSocket;
	}

	int StartListen()
	{
		cout << "Start Listen\r\n";
		gpListenSocket = new SocketListenContext;
		//�f�o�b�N�p��ID������B���b�X���\�P�b�gID��0�B
		gpListenSocket->ID = gID++;

		//�\�P�b�g�쐬
		WSAPROTOCOL_INFOA prot_info{};
		gpListenSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (!gpListenSocket->hSocket)
		{
			return S_FALSE;
		}

		//�\�P�b�g�����[�X�I�v�V����
		BOOL yes = 1;
		if (setsockopt(gpListenSocket->hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)))
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
		int addr_size = sizeof(addr.sin_addr);
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
		if ((rVal = ::bind(gpListenSocket->hSocket, (struct sockaddr*)&(addr), sizeof(addr)) == SOCKET_ERROR))
		{
			cerr << "Err ::bind Code:" << WSAGetLastError() << " Line:" << __LINE__ << "\r\n";
			return false;
		}

		//���b�X��
		if (listen(gpListenSocket->hSocket, SOMAXCONN)) {
			cerr << "Err listen Code:" << to_string(WSAGetLastError()) << " Line:" << __LINE__ << "\r\n";
			return false;
		}

		//���b�X���\�P�b�g�����|�[�g�쐬
		PTP_IO pTPioSocket(NULL);
		if (!(pTPioSocket = CreateThreadpoolIo((HANDLE)gpListenSocket->hSocket, OnListenCompCB, gpListenSocket, &*pcbe)))
		{
			cerr << "CreateThreadpoolIo error! Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
			return false;
		}
		PreAccept(gpListenSocket);
		PreAccept(gpListenSocket);
		StartThreadpoolIo(pTPioSocket);
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
					throw std::runtime_error("Create'GetAcceptEx'error! Code:" + to_string(WSAGetLastError()) + " LINE:" + to_string(__LINE__) + "File:" + __FILE__ + "\r\n");
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

	void EndListen()
	{
		if (gpListenSocket->hSocket)
		{
			shutdown(gpListenSocket->hSocket, SD_SEND);
			closesocket(gpListenSocket->hSocket);
		}
		delete gpListenSocket;
		gpListenSocket = NULL;
	}

	void ShowStatus()
	{
		std::cout << "Total Connected: " << SevOL::gID - 1 << "\r" << std::endl;
		std::cout << "Current Connected: " << SevOL::gID - SevOL::gCDel - 1 << "\r" << std::endl;
		std::cout << "Max Connecting: " << SevOL::gMaxConnect << "\r" << std::endl;
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

	WSABUF* CopyStdStringToWsaString(std::string strsrc, WSABUF* pWsaBuf)
	{
		CopyMemory(pWsaBuf->buf, strsrc.data(), strsrc.length());
		pWsaBuf->len = strsrc.length();
		return pWsaBuf;
	}

	bool DoSend(SocketContext* pSocket)
	{
		//�Z���h�����𑼂�CB�ɉ񂷁B
		pSocket->usCycle = OL_WRITE_CYCLE;
		if (WSASend(pSocket->hSocket, &pSocket->wsaWriteBuf, 1, NULL, 0/*dwflags*/, pSocket, NULL))
		{
			DWORD dw = WSAGetLastError();
			if (dw != WSA_IO_PENDING)
			{
				cerr << "WSASend err. Code:" << to_string(dw) << " Line:" << to_string(__LINE__) << " File:" << __FILE__ << "\r\n";
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
		pSocket->usCycle = OL_READ_CYCLE;
		if (WSARecv(pSocket->hSocket, &pSocket->wsaReadBuf, 1, NULL, &pSocket->flags, pSocket, NULL))
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				cerr << "WSARecv err. Code:" << to_string(WSAGetLastError()) << " Line:" << to_string(__LINE__) << " File:" << __FILE__ << "\r\n";

				return false;
			}
			return true;
		}
		return true;
	}

	//void TryAccept()
	//{
	//	//�A�N�Z�v�g�p�\�P�b�g���o��
	//	SocketContext* pAcceptSocket = gvSocketsPool.front();
	//	erase(gvSocketsPool, pAcceptSocket);

	//	//�I�[�v���\�P�b�g�쐬
	//	pAcceptSocket->hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,NULL,0, WSA_FLAG_OVERLAPPED);

	//	//�f�o�b�N�pID�ݒ�
	//	pAcceptSocket->ID = gID++;

	//	//�o�b�t�@������
	//	gpListenSocket->ReadString.resize(BUFFER_SIZE, '\0');
	//	pAcceptSocket->ReadString.resize(BUFFER_SIZE, '\0');

	//	//AcceptEx�̊֐��|�C���^���擾���A���s�B�p�����[�^�[�̓T���v���܂�܁B
	//	try {
	//		if (!(*GetAcceptEx(gpListenSocket))(gpListenSocket->hSocket, pAcceptSocket->hSocket, gpListenSocket->ReadString.data(), gpListenSocket->ReadString.size() - ((sizeof(sockaddr_in) + 16) * 2), sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &gpListenSocket->olRead))
	//		{
	//			DWORD dwCode = GetLastError();
	//			if (GetLastError() != ERROR_IO_PENDING)
	//			{
	//				throw std::runtime_error("AcceptEx return value error! Code:" + to_string(WSAGetLastError()) + " LINE:" + to_string(__LINE__) + "File:" + __FILE__ + "\r\n");
	//			}
	//		}
	//	}
	//	catch (std::exception& e) {
	//		// ��O��ߑ��A�G���[���R���o�͂���
	//		std::cerr << e.what() << std::endl;
	//		return;
	//	}

	//	gpListenSocket->pAcceptSocket = pAcceptSocket;

	//	//���b�X�������|�[�g�쐬
	//	if (!(gpListenSocket->pTPListen = CreateThreadpoolIo((HANDLE)gpListenSocket->hSocket, OnListenCompCB, gpListenSocket, &*pcbe)))
	//	{
	//		cerr << "CreateThreadpoolIo error! Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
	//		return;
	//	}
	//	//���b�X�������|�[�g�J�n�B
	//	StartThreadpoolIo(gpListenSocket->pTPListen);

	//	PTP_IO pTPIo(NULL);
	//	//�A�N�Z�v�g�����|�[�g�쐬
	//	if (!(pTPIo = CreateThreadpoolIo((HANDLE)pAcceptSocket->hSocket, OnSocketNoticeCompCB, pAcceptSocket, &*pcbe)))
	//	{
	//		cerr << "CreateThreadpoolIo error! Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
	//		return;
	//	}
	//	//�A�N�Z�v�g�����|�[�g�J�n�B
	//	StartThreadpoolIo(pTPIo);


	//	return;

	//}

	void PreAccept(SocketListenContext* pListenSocket)
	{

		//�A�N�Z�v�g�p�\�P�b�g���o��
		SocketContext* pAcceptSocket = PopSocket();
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
					throw std::runtime_error("AcceptEx return value error! Code:" + to_string(WSAGetLastError()) + " LINE:" + to_string(__LINE__) + " File:" + __FILE__ + "\r\n");
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

	bool SetIOCPOnSocketNoticeCompCB(SocketContext* pSocket)
	{
		PTP_IO pTPIo(NULL);
			if (!(pTPIo = CreateThreadpoolIo((HANDLE)pSocket->hSocket, OnSocketNoticeCompCB, pSocket, &*pcbe)))
			{
				cerr << "CreateThreadpoolIo error! Code:" << to_string(WSAGetLastError()) << " LINE:" << __LINE__ << "\r\n";
				return false;
			}
			//�A�N�Z�v�g�����|�[�g�J�n�B
			StartThreadpoolIo(pTPIo);
	}
	//void SerializedPrint(SevOL::SocketContext* pSocket)
	//{
	//	PTP_WORK ptpwork(NULL);
	//	if (!(ptpwork = CreateThreadpoolWork(SerializedSocketPrintCB, pSocket, &*pcbe)))
	//	{
	//		std::cerr << "Err" << __FUNCTION__ << __LINE__ << std::endl;
	//		return;
	//	}
	//	SubmitThreadpoolWork(ptpwork);
	//}

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