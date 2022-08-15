//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "ScoketContextPooll.h"

namespace SevPooll {

	SocketContext::SocketContext()
		:
		 hSocket(NULL)
		, ReadBuf(BUFFER_SIZE, '\0')
		, WriteBuf(BUFFER_SIZE, '\0')
		, RemBuf(BUFFER_SIZE, '\0')
		, ID(0)

	{
		RemBuf.clear();
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
		WriteBuf.clear();
		WriteBuf.resize(BUFFER_SIZE, '\0');
		RemBuf.clear();
		ID = 0;
	}
}