//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "SocketContextOL.h"




namespace SevOL {

	SocketContext::SocketContext()
		:
		wsaReadBuf{}
		, wsaWriteBuf{}
		, wsaReadBackBuf{}
		, wsaWriteBackBuf{}
		, hSocket(NULL)
		, hSocketBack(NULL)
		, ReadBuf(BUFFER_SIZE, '\0')
		, WriteBuf(BUFFER_SIZE, '\0')
		, ReadBackBuf(BUFFER_SIZE,'\0')
		, WriteBackBuf(BUFFER_SIZE,'\0')
		, RemBuf(BUFFER_SIZE, '\0')
		, ID(0)
		, Dir(eDir::DIR_NOT_SELECTED)
		, OLBack{}
		, flags(0)
		, flagsBack(0)
		, pTPIo(NULL)
		, pTPBackIo(NULL)
		, fFrontReEnterGuard(FALSE)
		, fBackReEnterGuard(FALSE)
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
		pTPBackIo = NULL;
		if (hSocket)
		{
			shutdown(hSocket, SD_SEND);
			closesocket(hSocket);
			hSocket = NULL;
		}
		if (hSocketBack)
		{
			shutdown(hSocketBack, SD_SEND);
			closesocket(hSocketBack);
			hSocketBack = NULL;
		}
		ReadBuf.clear();
		WriteBuf.clear();
		RemBuf.clear();
		ID = 0;
		Dir = eDir::DIR_NOT_SELECTED;
		OLBack = {};
		flags = 0;
		flagsBack = 0;
		fFrontReEnterGuard=FALSE;
		fBackReEnterGuard=FALSE;
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