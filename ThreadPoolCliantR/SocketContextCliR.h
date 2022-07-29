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

constexpr auto BUFFER_SIZE = 1024;
constexpr auto N_COUNTDOWNS = 7;

namespace ThreadPoolCliantR {
	class SocketContext {
	public:
		SocketContext();
		SocketContext(SocketContext&& moveSocket) = delete;
		SocketContext(SocketContext& Socket)=delete;
		~SocketContext();
		void ReInitialize();
		std::time_t GetMaxResponce();
		std::time_t GetMinResponce();
		SOCKET hSocket;
		u_short ID;
		u_int CountDown;
		std::string ReadString;
		std::binary_semaphore readlock;
		std::string WriteString;
		std::binary_semaphore writelock;
		WSAEVENT hEvent;
		std::vector<std::string> vstr;
		std::binary_semaphore vstrlock;
		SYSTEMTIME tSend[N_COUNTDOWNS+1];
		SYSTEMTIME tRecv[N_COUNTDOWNS+1];
	};


}
