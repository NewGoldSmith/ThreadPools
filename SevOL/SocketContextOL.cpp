//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "SocketContextOL.h"

namespace SevOL {

	SocketContext::SocketContext()
		:
		wsaFrontReadBuf{}
		, wsaFrontWriteBuf{}
		, wsaReadBackBuf{}
		, wsaWriteBackBuf{}
		, hFrontSocket(NULL)
		, hBackSocket(NULL)
		, FrontReadBuf(BUFFER_SIZE, '\0')
		, FrontWriteBuf(BUFFER_SIZE, '\0')
		, BackReadBuf(BUFFER_SIZE,'\0')
		, BackWriteBuf(BUFFER_SIZE,'\0')
		, FrontRemBuf(BUFFER_SIZE, '\0')
		, ID(0)
		, Dir(eDir::DIR_NOT_SELECTED)
		, OLBack{}
		, flags(0)
		, flagsBack(0)
		, pForwardTPIo(NULL)
		, pBackTPIo(NULL)
		, lockCleanup(1)
		, FrontEnterlock(1)
		, BackEnterlock(1)
	{
		wsaFrontReadBuf.buf = FrontReadBuf.data();
		wsaFrontReadBuf.len = FrontReadBuf.length();
		wsaFrontWriteBuf.buf = FrontWriteBuf.data();
		wsaFrontWriteBuf.len = FrontWriteBuf.length();
		FrontRemBuf.clear();
		hEvent = WSACreateEvent();
	};

	SocketContext::~SocketContext()
	{
		if (hFrontSocket)
		{
			shutdown(hFrontSocket, SD_SEND);
			closesocket(hFrontSocket);
			hFrontSocket = NULL;
		}
		if (hEvent)
		{
			WSACloseEvent(hEvent);
		}
	}
	void SocketContext::ReInitialize()
	{
		if (hFrontSocket)
		{
			shutdown(hFrontSocket, SD_SEND);
			closesocket(hFrontSocket);
			hFrontSocket = NULL;
		}
		if (hBackSocket)
		{
			shutdown(hBackSocket, SD_SEND);
			closesocket(hBackSocket);
			hBackSocket = NULL;
		}
		FrontReadBuf.clear();
		FrontWriteBuf.clear();
		FrontRemBuf.clear();
		BackReadBuf.clear();
		BackWriteBuf.clear();
		ID = 0;
		Dir = eDir::DIR_NOT_SELECTED;
		OLBack = {};
		flags = 0;
		flagsBack = 0;
		Sleep(100);
		lockCleanup.release();
		FrontEnterlock.release();
		BackEnterlock.release();
		//pForwardTPIo = NULL;
		//pBackTPIo = NULL;
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