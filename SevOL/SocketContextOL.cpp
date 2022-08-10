//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "SocketContextOL.h"




namespace SevOL {

	SocketContext::SocketContext()
		:
		wsaReadBuf{}
		, wsaWriteBuf{}
		, hSocket(NULL)
		, ReadBuf(BUFFER_SIZE, '\0')
		, WriteBuf(BUFFER_SIZE, '\0')
		, RemBuf(BUFFER_SIZE, '\0')
		, ID(0)
		, Dir(eDir::OL_NOT_SELECTED)
		, NumberOfBytesSent(0)
		, NumberOfBytesRecvd(0)
		, flags(0)
		, pTPIo(0)
	{
		wsaReadBuf.buf = ReadBuf.data();
		wsaReadBuf.len = ReadBuf.length();
		wsaWriteBuf.buf = WriteBuf.data();
		wsaWriteBuf.len = WriteBuf.length();
		RemBuf.clear();
		hEvent = WSACreateEvent();
	};

	SocketContext::~SocketContext()
	{
		if (hSocket)
		{
			shutdown(hSocket, SD_SEND);
			closesocket(hSocket);
			hSocket = NULL;
		}
		if (hEvent)
		{
			WSACloseEvent(hEvent);
		}
	}
	void SocketContext::ReInitialize()
	{
		pTPIo = NULL;
		if (hSocket)
		{
			shutdown(hSocket, SD_SEND);
			closesocket(hSocket);
			hSocket = NULL;
		}
		ReadBuf.clear();
		ReadBuf.resize(BUFFER_SIZE, '\0');
		wsaReadBuf.buf = ReadBuf.data();
		wsaReadBuf.len = ReadBuf.length();
		WriteBuf.clear();
		WriteBuf.resize(BUFFER_SIZE, '\0');
		wsaWriteBuf.buf = ReadBuf.data();
		wsaWriteBuf.len = ReadBuf.length();
		RemBuf.clear();
		ID = 0;
		Dir = eDir::OL_NOT_SELECTED;
		NumberOfBytesSent = 0;
		NumberOfBytesRecvd = 0;
		flags = 0;
	}

	void SocketContext::InitWsaBuf(WSABUF* pwsa, string* pstr)
	{
		pstr->clear();
		pstr->resize(BUFFER_SIZE, '\0');
		pwsa->buf = pstr->data();
		pwsa->len = pstr->length();
	}

	void SocketContext::WsaToStr(WSABUF* pwsa, string* pstr)
	{
		pstr->assign(pwsa->buf, pwsa->len);
	}

	void SocketContext::StrToWsa(string* pstr, WSABUF* pwsa)
	{
		pwsa->buf = pstr->data();
		pwsa->len = pstr->length();
	}



	SocketListenContext::SocketListenContext()
		:SocketContext()
		, pTPListen(NULL)
	{
		;
	}

	SocketListenContext::~SocketListenContext()
	{
		pTPListen == NULL;
	}
	void SocketListenContext::ReInitialize()
	{
		SocketContext::ReInitialize();
		pTPListen = NULL;
	}

}