//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "SocketContextR.h"

namespace ThreadPoolServerR {

	SocketContext::SocketContext()
		:hSocket(NULL)
		, ID(0)
		, ReadString(BUFFER_SIZE, '\0')
		, readlock{ 1 }
		, WriteString(BUFFER_SIZE, '\0')
		, writelock{ 1 }
		,RemString(BUFFER_SIZE,'\0')
		, vstr()
		, vstrlock{1}
		, hEvent(NULL)
		, ptpwaitOnEvListen(NULL)
		, ptpwaitOnEvSocket(NULL)
	{
		try {
			hEvent = WSACreateEvent();
			if (!hEvent)
				throw std::runtime_error("error! SocketContext::SocketContext() WSACreateEvent()");
		}
		catch (std::exception& e) {
			std::cout << e.what() << std::endl;
		}
		ReadString.resize(0);
		WriteString.resize(0);
		RemString.resize(0);
	};

	SocketContext::~SocketContext()
	{
		if (ptpwaitOnEvListen)
		{
			CancelIo((HANDLE)hSocket);
			WaitForThreadpoolWaitCallbacks(ptpwaitOnEvListen, TRUE);
			CloseThreadpoolWait(ptpwaitOnEvListen);
		}
		//if (ptpwaitOnEvSocket)
		//{
		//	//CancelIo(hEvent);
		//	//WaitForThreadpoolWaitCallbacks(ptpwaitOnEvSocket, TRUE);
		//	CloseThreadpoolWait(ptpwaitOnEvSocket);
		//}
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
		//if (ptpwaitOnEvSocket)
		//{
		//	CancelIo(hEvent);
		//	WaitForThreadpoolWaitCallbacks(ptpwaitOnEvSocket, TRUE);
		//	CloseThreadpoolWait(ptpwaitOnEvSocket);
		//	ptpwaitOnEvSocket = NULL;
		//}
		ID = 0;
		ReadString.clear();
		readlock.release();
		WriteString.clear();
		writelock.release();
		RemString.clear();
		vstr.clear();
		vstrlock.release();
	}
}