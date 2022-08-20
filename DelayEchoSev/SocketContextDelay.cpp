//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "SocketContextDelay.h"




namespace SevDelay {

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
		, flags(0)
		, pTPIo(NULL)
		, pTPTimer(NULL)
	{
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
		if (hSocket)
		{
			shutdown(hSocket, SD_SEND);
			closesocket(hSocket);
			hSocket = NULL;
		}
		ReadBuf.clear();
		WriteBuf.clear();
		RemBuf.clear();
		ID = 0;
		Dir = eDir::OL_NOT_SELECTED;
		flags = 0;
		fReEnterGuard = FALSE;
		pTPIo = NULL;
		pTPTimer = NULL;
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