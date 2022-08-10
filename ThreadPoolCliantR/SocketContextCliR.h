//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Cliant side
#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>
#include <iostream>
#include <string>
#include <semaphore>
#include <cassert>
#include <exception>
#include <SYS\Timeb.h >
#include "CallbacksCliR.h"

namespace ThreadPoolCliantR {

	constexpr auto BUFFER_SIZE = 1024;
	constexpr auto N_COUNTDOWNS = 3;

	class SocketContext {
	public:
		SocketContext();
		SocketContext(SocketContext&& moveSocket) = delete;
		SocketContext(SocketContext& Socket) = delete;
		~SocketContext();
		void ReInitialize();
		ULONGLONG GetMaxResponce();
		ULONGLONG GetMinResponce();
		SOCKET hSocket;
		u_short ID;
		int CountDown;
		std::string ReadString;
		std::string WriteString;
		std::string RemString;
		std::string DispString;
		WSAEVENT hEvent;
		FILETIME  tSend[N_COUNTDOWNS + 1];
		FILETIME  tRecv[N_COUNTDOWNS + 1];
	};

}
