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
	}

	std::time_t SocketContext::GetMaxResponce()
	{
		std::time_t tMax(0);
		for (int i(0); i < N_COUNTDOWNS; ++i)
		{
			FILETIME ftimeOld;
			FILETIME ftimeNew;
			SystemTimeToFileTime(&tSend[i], &ftimeOld);
			SystemTimeToFileTime(&tRecv[i], &ftimeNew);

			std::time_t* nTimeOld = (std::time_t*)&ftimeOld;
			std::time_t* nTimeNew = (std::time_t*)&ftimeNew;

			tMax = __max((*nTimeNew - *nTimeOld) / 10000/* / 1000*/, tMax);
		}
		return tMax;
	}

	std::time_t SocketContext::GetMinResponce()
	{
		std::time_t tMin(LLONG_MAX);
		for (int i(0); i < N_COUNTDOWNS; ++i)
		{
			FILETIME ftimeOld;
			FILETIME ftimeNew;
			SystemTimeToFileTime(&tSend[i], &ftimeOld);
			SystemTimeToFileTime(&tRecv[i], &ftimeNew);

			std::time_t* nTimeOld = (std::time_t*)&ftimeOld;
			std::time_t* nTimeNew = (std::time_t*)&ftimeNew;

			tMin = __min((*nTimeNew - *nTimeOld) / 10000/* / 1000*/, tMin);
		}
		return tMin;
	}
}