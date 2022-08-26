//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "SocketContext.h"

namespace FrontSevEv {

	SocketContext::SocketContext()
		:hSocket(NULL)
		, ID(0)
		,RemString(BUFFER_SIZE,'\0')
		,Buf(BUFFER_SIZE,'\0')
		, hEvent(NULL)
		, ptpwaitOnEvListen(NULL)
		, pRC(NULL)
	{
		try {
			hEvent = WSACreateEvent();
			if (!hEvent)
				throw std::runtime_error("error! SocketContext::SocketContext() WSACreateEvent()");
		}
		catch (std::exception& e) {
			std::cout << e.what() << std::endl;
		}
		RemString.resize(0);
		Buf.resize(0);
	};

	SocketContext::~SocketContext()
	{
		if (ptpwaitOnEvListen)
		{
			CancelIo((HANDLE)hSocket);
			WaitForThreadpoolWaitCallbacks(ptpwaitOnEvListen, TRUE);
			CloseThreadpoolWait(ptpwaitOnEvListen);
		}
		CloseHandle(hEvent);
		hEvent = NULL;
		if (hSocket)
		{
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
		if (ptpwaitOnEvListen)
		{
			CancelIo(hEvent);
			WaitForThreadpoolWaitCallbacks(ptpwaitOnEvListen, TRUE);
			CloseThreadpoolWait(ptpwaitOnEvListen);
			ptpwaitOnEvListen = NULL;
		}
		ID = 0;
		RemString.clear();
		Buf.clear();
		pRC = NULL;
	}
}