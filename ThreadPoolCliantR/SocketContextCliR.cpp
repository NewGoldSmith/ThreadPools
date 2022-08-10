//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Cliant side
#include "SocketContextCliR.h"



namespace ThreadPoolCliantR {

	SocketContext::SocketContext()
		:hSocket(NULL)
		, ID(0)
		, CountDown(0)
		, ReadString(BUFFER_SIZE, '\0')
		, WriteString(BUFFER_SIZE, '\0')
		, hEvent(NULL)
		, tSend{}
		, tRecv{}
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
	}

	SocketContext::~SocketContext()
	{
		if (hSocket)
		{
			shutdown(hSocket, SD_SEND);
			closesocket(hSocket);
			hSocket = NULL;
		}
		CloseHandle(hEvent);
		hEvent = NULL;
	}

	void SocketContext::ReInitialize()
	{
		if (hSocket)
		{
			shutdown(hSocket, SD_SEND);
			closesocket(hSocket);
			hSocket = NULL;
		}
		ID = 0;
		CountDown = 0;
		ReadString.clear();
		WriteString.clear();
		RemString.clear();
	}

	ULONGLONG SocketContext::GetMaxResponce()
	{
		ULONGLONG  tMax(0);
		for (int i(0); i < N_COUNTDOWNS; ++i)
		{
			ULONGLONG t64Send=(((ULONGLONG)tSend[i].dwHighDateTime) << 32) + tSend[i].dwLowDateTime;
			ULONGLONG t64Recv= (((ULONGLONG)tRecv[i].dwHighDateTime) << 32) + tRecv[i].dwLowDateTime;
			tMax = std::max<ULONGLONG>((t64Recv-t64Send)/ 10000.0,  tMax);
		}
		return tMax;
	}

	ULONGLONG SocketContext::GetMinResponce()
	{
		ULONGLONG tMin(LLONG_MAX);
		for (int i(0); i < N_COUNTDOWNS; ++i)
		{
			ULONGLONG t64Send = (((ULONGLONG)tSend[i].dwHighDateTime) << 32) + tSend[i].dwLowDateTime;
			ULONGLONG t64Recv = (((ULONGLONG)tRecv[i].dwHighDateTime) << 32) + tRecv[i].dwLowDateTime;
			tMin = std::min<ULONGLONG>((t64Recv - t64Send) / 10000.0, tMin);
		}
		return tMin;
	}
}