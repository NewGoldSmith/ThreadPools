//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#include "SocketContext.h"

namespace MainSevWorkR
{

	SocketContext::SocketContext()
		:hSocket(NULL)
		, ID(0)
		, Buffer(BUFFER_SIZE, '\0')
		, buflock(1)
		, len(0)
	{}

	SocketContext::~SocketContext()
	{
		if (hSocket)
		{
			closesocket(hSocket);
		}
	}
	void SocketContext::ReInit()
	{
		if (hSocket)
		{
			closesocket(hSocket);
			hSocket = NULL;
		}
		ID = 0;
		Buffer.clear();
		len = 0;
	}
}
