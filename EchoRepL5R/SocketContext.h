//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>
#include <process.h>
#include <iostream>
#include <string>
#include <semaphore>
#include <cassert>
#include <exception>

namespace MainSevWorkR
{
constexpr auto BUFFER_SIZE = 1024;
	class SocketContext {
	public:
		SocketContext();
		~SocketContext();
		SocketContext(SocketContext&) = delete;
		SocketContext(SocketContext&&) = delete;
		void ReInit();
		SOCKET hSocket;
		u_int ID;
		std::string Buffer;
		std::binary_semaphore buflock;
		size_t len;
	};

}