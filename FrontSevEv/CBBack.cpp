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

		//�ǂݍ��݉\
		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			pBackSocket->pFrontSocket->Buf.resize(BUFFER_SIZE, '\0');
			//�t�����g�̃o�b�t�@�ɒ��ړǂݍ��݁B
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
				//�ؒf
				stringstream  ss;
				ss << "FrontSevEv. WSAEnumNetworkEvents. recv size 0. Code: " << Err << " LINE: " << __LINE__ << "\r\n";
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
				//�t�����g�\�P�b�g�̓N���[�Y
				pBackSocket->pFrontSocket->ReInitialize();
				gSocketsPool.Push(pBackSocket->pFrontSocket);
				pBackSocket->pFrontSocket = NULL;
				//�o�b�N�\�P�b�g�͎g�p�֎~�ɂ���׃Z�}�t�H���󂯂Ȃ��B
				//�C�x���g����~�B
				return;
			}
			else {
				//�N���C�A���g�֕ԐM
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
		//Back��Close�����B
		else if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			//�t�����g�\�P�b�g��Close��
			pBackSocket->pFrontSocket->ReInitialize();
			gSocketsPool.Push(pBackSocket->pFrontSocket);
			//�o�b�N�\�P�b�g������B�Z�}�t�H�͉�����Ȃ��B
			CloseThreadpoolWait(Wait);
			return;
		}

//�ҋ@�I�u�W�F�N�g�Đݒ�
		SetThreadpoolWait(Wait, pBackSocket->hEvent.get(), NULL);

		//���ɌĂ΂��܂Ńv�[���Ɉړ�
		pBackSocket->pFrontSocket = NULL;
		BackContextPool.Push(pBackSocket);

		//�ҋ@�I�u�W�F�N�g���		
//		CloseThreadpoolWait(Wait);

		//����I���Z�}�t�H����ݒ�B
		ReleaseSemaphoreWhenCallbackReturns(Instance, gpSem.get(), 1);
	}

	//���E���h���r���ŏ������݂̏��Ԃ�����Ă����B
	VOID WriteBackWaitCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{
		SocketContext* pSocket = (SocketContext*)Context;
		if (WaitResult)
		{
			cerr << "FrontSevEv. WriteBackWaitCB. Code:" << to_string(WaitResult) << "\r\n";
			return;
		}

		//�Z�}�t�H���󂢂Ă��邩�炱�̃R�[���o�b�N���Ă΂ꂽ�̂ō݂�͂��B
		//pRC�͋��L��NUM_BACK_CONNECTION�̐������Ȃ��B
		RoundContext* pBackSocket = BackContextPool.Pull();

		//�t�����g�\�P�b�g�ƌ��т��B
		pBackSocket->pFrontSocket = pSocket;

		//�t�����g�̃f�[�^���o�b�N�ɏ������݁B
		//�ԐM�̓C�x���g�ݒ肳��Ă���̂ŁAOnBackEvSocketCB���Ă΂��B
		//Context��RoundContext
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

	//���C�g���烊�[�h�܂ł���̗���
	void QueryBack(SocketContext* pSocket)
	{
		//�Z�}�t�H�̑ҋ@�֐��Ƃ��ăZ�b�g����B
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
		//�z�X�gsockeaddr_in�ݒ�
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

		//�T�[�o�[�ڑ��p��sockaddr_in��ݒ�
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

			//�z�X�g�o�C���h
			if (::bind(pBackSocket->hSocket, (struct sockaddr*)&(addr), sizeof(addr)) == SOCKET_ERROR)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. bind Error! Code:" << std::to_string(Err) << " Line: " << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}

			//�R�l�N�g
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

			//�\�P�b�g�̃C�x���g�ݒ�
			if (WSAEventSelect(pBackSocket->hSocket, pBackSocket->hEvent.get(), /*FD_ACCEPT |*/ FD_CLOSE | FD_READ/* | FD_CONNECT | FD_WRITE*/) == SOCKET_ERROR)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. WSAEventSelect. Code:" << to_string(Err) << " Line:" << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				return FALSE;
			}

			//�C�x���g�n���h���̐ݒ�
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

			//�ݒ肪�ς񂾂̂ōĊi�[�B
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