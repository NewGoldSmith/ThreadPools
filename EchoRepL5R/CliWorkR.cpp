#include "CliWork.h"

extern CommandOpEchoRepL5 cmdOp;
extern DetectEnd_EchoL5::ThreadParam TP;

namespace MainCliWork
{

	bool CliWork()
	{
		using namespace SocketHelper;

		SockPrintMessage(cmdOp.m_ShowStatus, "Client Work Start");

		bool bContinue = true;
		CBinaryString ByteData(1024);
		SOCKET CliSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (CliSocket == INVALID_SOCKET)
		{
			fprintf(stderr, "socket error:%d\n", WSAGetLastError());
			return false;
		}

		int Err;
		//ホストバインド設定
		struct sockaddr_in addr = { };
		addr.sin_family = AF_INET;
		addr.sin_port = htons(cmdOp.m_HostPort);
		Err = inet_pton(AF_INET, cmdOp.m_strHostAddress.c_str(), &(addr.sin_addr));
		if (Err != 1)
		{
			if (Err == 0)
			{
				fprintf(stderr, "socket error:Listen inet_pton return val 0\n");
				bContinue = false;
			}
			else if (Err == -1)
			{
				SockPrintErr();
				bContinue = false;
			}
		}
		Err = bind(CliSocket, (struct sockaddr*)&(addr), sizeof(addr));
		if (Err == SOCKET_ERROR)
		{
			SockPrintMessage(cmdOp.m_ShowStatus, "Client Socket bind err\r\n");
			SockPrintErr();

		}

		//ノンブロックに変更
		u_long flag = 1;
		Err = ioctlsocket(CliSocket, FIONBIO, &flag);
		if (Err == SOCKET_ERROR)
		{
			SockPrintErr();
			return false;
		}

		//コネクト
		addr.sin_family = AF_INET;
		addr.sin_port = htons(cmdOp.m_PeerPort);
		Err = inet_pton(AF_INET, cmdOp.m_strPeerAddress.c_str(), &(addr.sin_addr));
		if (Err != 1)
		{
			if (Err == 0)
			{
				SockPrintErr();
				return false;
			}
			else if (Err == -1)
			{
				SockPrintErr();
				return false;
			}
		}
		Err = connect(CliSocket, (struct sockaddr*)&(addr), sizeof(addr));
		if (Err == SOCKET_ERROR)
		{
			SockPrintErr();
		}
		ShowSocketStatus(cmdOp.m_ShowStatus, CliSocket, std::string("The client socket is connected."));

		//リード・ライトループ
		Err = CliReadWriteWork(CliSocket);

		//ブロックに変更
		flag = 0;
		Err = ioctlsocket(CliSocket, FIONBIO, &flag);
		if (Err == SOCKET_ERROR)
		{
			SockPrintErr();
		}
		Err = shutdown(CliSocket, SD_BOTH);
		if (Err == SOCKET_ERROR)
		{
			SockPrintErr();
		}
		Err = closesocket(CliSocket);
		SockPrintMessage(cmdOp.m_ShowStatus, "The Client socket has been closed.");
		SockPrintMessage(cmdOp.m_ShowStatus, "Client work finished.");
		return true;
	}

	bool CliReadWriteWork(SOCKET CliSocket)
	{
		using namespace SocketHelper;

		int Count = 0;
		//リード・ライトループ
		int Err = 0;
		bool bContinue = true;
		while (bContinue)
		{
			int len;
			CBinaryString ByteData(1024);
			len = SockRead(CliSocket, &ByteData);
			if (len == SOCKET_ERROR)
			{
				Err = GetLastError();
				switch (Err)
				{
				case WSAEWOULDBLOCK:
					break;
				default:
					SockPrintErr();
					ShowSocketStatus(cmdOp.m_ShowStatus, CliSocket, "Socket disconnection.");
					bContinue = false;
					break;
				}
			}
			if (len == 0)
			{
				//受信したのに長さが０の場合切断したとみなす。
				SockPrintMessage(cmdOp.m_ShowStatus, "Disconnected from the peer.");
				ShowSocketStatus(cmdOp.m_ShowStatus, CliSocket, "Socket disconnection.");
				bContinue = false;
			}
			if (_stricmp((char*)ByteData.c_strA(), (char*)u8"quit\r\n") == 0)
			{
				SockPrintMessage(cmdOp.m_ShowStatus, "The peer has entered the \"quit\" command.");
				ShowSocketStatus(cmdOp.m_ShowStatus, CliSocket, "Socket disconnection.");
				bContinue = false;
			}
			len = SockWrite(CliSocket, &ByteData);
			if (len == SOCKET_ERROR)
			{
				int Err = GetLastError();
				switch (Err)
				{
				case WSAEWOULDBLOCK:
					break;
				default:
					SockPrintErr();
					ShowSocketStatus(cmdOp.m_ShowStatus, CliSocket, "Socket disconnection.");
					bContinue = false;
					break;
				}
			}
			Count++;
			//CPU使用率下げる
			Sleep(500);
			//コンソールの終了コマンドの状態確認
			if (TP.bEndFlag)
			{
				SockPrintMessage(cmdOp.m_ShowStatus, "You have entered the \"quit\" command from the command prompt.");
				ShowSocketStatus(cmdOp.m_ShowStatus, CliSocket, "Socket disconnection.");
				bContinue = false;
			}
		}
		return true;
	}

}
