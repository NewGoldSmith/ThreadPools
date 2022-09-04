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

		//Back��Close�����B
		if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			//�o�b�N�\�P�b�g��Close����
			stringstream  ss;
			ss << "FrontSevEv. BackSocket ID:" << pBackSocket->ID << " Closed. LINE:" <<__LINE__<< "\r\n";
			std::cerr << ss.str();
			MyTRACE(ss.str().c_str());

			DecStatusFront();
			pBackSocket->pFrontSocket->ReInitialize();
			gSocketsPool.Push(pBackSocket->pFrontSocket);

			//�o�b�N�\�P�b�g������B�Z�}�t�H�͉�����Ȃ��B
			pBackSocket->ReInitialize();
			//�Đڑ������݂�B
			if (!BackTryConnect(pBackSocket))
			{
				stringstream  ss;
				ss << "FrontSevEv. BackSocket. Stoped. ID:"<< pBackSocket->ID << " WSAEnumNetworkEvents.Reconnect failure."<< " LINE : " << __LINE__ <<"\r\n";
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
				//��~
				while(TRUE)
				{
					++gLostBackSocket;
				}
			}
			BackContextPool.Push(pBackSocket);
			//�Z�}�t�H����ݒ�
			ReleaseSemaphoreWhenCallbackReturns(Instance, gpSem.get(), 1);
			return;
		}

		//�ǂݍ��݉\
		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			pBackSocket->pFrontSocket->ReadBuf.clear();
			pBackSocket->pFrontSocket->ReadBuf.resize(BUFFER_SIZE, '\0');
			//�t�����g�̃o�b�t�@�ɒ��ړǂݍ��݁B
			int size = recv(pBackSocket->hSocket, pBackSocket->pFrontSocket->ReadBuf.data(), pBackSocket->pFrontSocket->ReadBuf.size(), 0);
			if (size == SOCKET_ERROR)
			{
				pBackSocket->pFrontSocket->ReadBuf.clear();
				Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. Back recv Code: " << Err << " LINE: " << __LINE__ << "\r\n";
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
				//�t�����g�\�P�b�g�͕���B
				DecStatusFront();
				pBackSocket->pFrontSocket->ReInitialize();
				gSocketsPool.Push(pBackSocket->pFrontSocket);
				return;
			}
			//�T�C�Y�̊m�F
			if (size == 0)
			{
				//�ؒf
				stringstream  ss;
				ss << "FrontSevEv. Back. recv size 0. Code: " << Err << " LINE: " << __LINE__ << "\r\n";
				std::cerr << ss.str();
				MyTRACE(ss.str().c_str());
				//�t�����g�\�P�b�g�̓N���[�Y
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
					//�o�b�N�\�P�b�g�͎g�p�֎~�ɂ���׃Z�}�t�H���󂯂Ȃ��B
					//�C�x���g����~�B
					while(TRUE)
					{
						++gLostBackSocket;
					}
				}
				ss << "FrontSevEv. Back. Reconnect success.ID:" << pBackSocket->ID << "\r\n";
				MyTRACE(ss.str().c_str());
				BackContextPool.Push(pBackSocket);

				//����I���Z�}�t�H����ݒ�B
				ReleaseSemaphoreWhenCallbackReturns(Instance, gpSem.get(), 1);
				return;
			}

			//�N���C�A���g�֕ԐM
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
				//�Z�}�t�H����ݒ�B
				ReleaseSemaphoreWhenCallbackReturns(Instance, gpSem.get(), 1);
				return;
			}
		}

		//�ҋ@�I�u�W�F�N�g�Đݒ�
		SetThreadpoolWait(Wait, pBackSocket->hEvent.get(), NULL);

		//���ɌĂ΂��܂Ńv�[���Ɉړ�
		pBackSocket->pFrontSocket = NULL;
		BackContextPool.Push(pBackSocket);

		//����I���Z�}�t�H����ݒ�B
		ReleaseSemaphoreWhenCallbackReturns(Instance, gpSem.get(), 1);
	}

	//�Z�}�t�H�ŏ������݂̏��Ԃ�����Ă����B
	VOID WriteBackWaitCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{
		FrontContext* pSocket = (FrontContext*)Context;
		if (WaitResult)
		{
			cerr << "FrontSevEv. Back WriteBackWaitCB. Code:" << to_string(WaitResult) << "\r\n";
			return;
		}

		//�Z�}�t�H���󂢂Ă��邩�炱�̃R�[���o�b�N���Ă΂ꂽ�̂ō݂�͂��B
		//pRC�͋��L��NUM_BACK_CONNECTION�̐������Ȃ��B
		BackContext* pBackSocket = BackContextPool.Pull();

		//�t�����g�\�P�b�g�ƌ��т��B
		pBackSocket->pFrontSocket = pSocket;
		pSocket->vBufLock.acquire();
		string str = pSocket->vBuf.front();
		pSocket->vBuf.erase(pSocket->vBuf.begin());
		pSocket->vBufLock.release();
		
		//�t�����g�̃f�[�^���o�b�N�ɏ������݁B
		//�ԐM�̓C�x���g�ݒ肳��Ă���̂ŁAOnBackEvSocketCB���Ă΂��B
		//Context��RoundContext
		if (send(pBackSocket->hSocket, str.data(), str.length(), 0) == SOCKET_ERROR)
		{
			DWORD Err = WSAGetLastError();
			stringstream  ss;
			ss << "FrontSevEv. Back send. Code:" << to_string(Err) << " LINE;" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			pBackSocket->ReInitialize();
			//�G���[�̏ꍇ�Đڑ������݂�B
			if (!BackTryConnect(pBackSocket))
			{
				stringstream  ss;
				ss << "FrontSevEv. Back send. Code:" << to_string(Err) << " LINE;" << __LINE__ << "\r\n";
				cerr << ss.str();
				MyTRACE(ss.str().c_str());
				while (TRUE)
				{
					stringstream  ss;
					//�s�\�ȈׁASTOP
					ss << "Stop. Incompetent. BackSocket ID:" << pBackSocket->ID << " LINE:"<<__LINE__<<"\r\n";
					cerr << ss.str();
					MyTRACE(ss.str().c_str());
					++gLostBackSocket;
				}
			}
			//�Đڑ��㑗�M�B
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

	//���C�g���烊�[�h�܂ł���̗���
	void QueryBack(FrontContext* pSocket)
	{
		//�Z�}�t�H�̑ҋ@�֐��Ƃ��ăZ�b�g����B
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
			//�ݒ肪�ς񂾂̂Ŋi�[�B
			BackContextPool.Push(pBackSocket);
		}
		return TRUE;
	}

	BOOL BackTryConnect(BackContext* pBackSocket)
	{
		//�f�o�b�N�pID
		pBackSocket->ID = gIDBack++;

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

		//�z�X�g�o�C���h
		if (::bind(pBackSocket->hSocket, (struct sockaddr*)&(addr), sizeof(addr)) == SOCKET_ERROR)
		{
			DWORD Err = WSAGetLastError();
			stringstream  ss;
			ss << "FrontSevEv. Back bind Error! Code:" << std::to_string(Err) << " Line: " << __LINE__ << "\r\n";
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
				ss << "FrontSevEv. Back connect Error. Code :" << std::to_string(Err) << " Line : " << __LINE__ << "\r\n";
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
			ss << "FrontSevEv. Back WSAEventSelect. Code:" << to_string(Err) << " Line:" << __LINE__ << "\r\n";
			cerr << ss.str();
			MyTRACE(ss.str().c_str());
			return FALSE;
		}

		//�R�[���o�b�N�C�x���g�̐ݒ�
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