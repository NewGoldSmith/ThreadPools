//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "SocketContextOL.h"




namespace SevOL {

	SocketContext::SocketContext()
		:
		wsaReadBuf{}
		,wsaWriteBuf{}
		,wsaRemBuf{}
		,hSocket(NULL)
		,ReadBuf(BUFFER_SIZE,'\0')
		,WriteBuf(BUFFER_SIZE, '\0')
		,RemBuf(BUFFER_SIZE, '\0')
		,ID(0)
		,usCycle(0)
		,NumberOfBytesSent(0)
		, NumberOfBytesRecvd(0)
		,flags(0)
	{
		wsaReadBuf.buf = ReadBuf.data();
		wsaReadBuf.len = ReadBuf.length();
		wsaWriteBuf.buf = WriteBuf.data();
		wsaWriteBuf.len = WriteBuf.length();
		wsaRemBuf.buf = RemBuf.data();
		wsaRemBuf.len = RemBuf.length();
	};

	SocketContext::~SocketContext()
	{
		if (hSocket)
		{
			shutdown(hSocket, SD_SEND);
			closesocket(hSocket);
			hSocket = NULL;
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
		ReadBuf.resize(BUFFER_SIZE, '\0');
		wsaReadBuf.buf = ReadBuf.data();
		wsaReadBuf.len = ReadBuf.length();
		WriteBuf.clear();
		WriteBuf.resize(BUFFER_SIZE, '\0');
		wsaWriteBuf.buf = ReadBuf.data();
		wsaWriteBuf.len = ReadBuf.length();
		RemBuf.clear();
		RemBuf.resize(BUFFER_SIZE, '\0');
		wsaRemBuf.buf = ReadBuf.data();
		wsaRemBuf.len = ReadBuf.length();
		ID = 0;
		usCycle = 0;
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
		,pTPListen(NULL)
	{
		;
	}

	SocketListenContext::~SocketListenContext()
	{
		if (pTPListen)
		{
			CancelThreadpoolIo(pTPListen);
			WaitForThreadpoolIoCallbacks(pTPListen, TRUE);
			CloseThreadpoolIo(pTPListen);
			pTPListen = NULL;
		}
		SocketContext::~SocketContext();
	}
	void SocketListenContext::ReInitialize()
	{
		SocketContext::ReInitialize();
		if (pTPListen)
		{
			CloseThreadpoolIo(pTPListen);
			pTPListen = NULL;
		}
	}

	WSABUF* SocketInitWsaBuf(WSABUF* pwsabuf)
	{
		ZeroMemory(pwsabuf->buf, BUFFER_SIZE);
		pwsabuf->len = BUFFER_SIZE;
		return pwsabuf;
	}
}