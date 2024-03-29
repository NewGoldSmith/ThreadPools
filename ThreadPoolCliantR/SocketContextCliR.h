//Copyright (c) 2022, Gold Smith
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
	using namespace std;

	constexpr auto BUFFER_SIZE = 1024;
	constexpr auto N_COUNTDOWN = 5;

	class SocketContext {
	public:
		SocketContext();
		SocketContext(SocketContext&& moveSocket) = delete;
		SocketContext(SocketContext& SocketArray) = delete;
		~SocketContext();
		void ReInitialize();
		ULONGLONG GetMaxResponce();
		ULONGLONG GetMinResponce();
		u_int FindAndConfirmCountDownNumber(const std::string& str);
		SOCKET hSocket;
		u_short SocketID;
		int CountDown;
		std::string ReadString;
		std::string WriteString;
		std::string RemString;
		std::string DispString;
		WSAEVENT hEvent;
		FILETIME  tSend[N_COUNTDOWN + 1];
		FILETIME  tRecv[N_COUNTDOWN + 1];
		TP_WAIT* pTPWait;
		TP_TIMER* pTPTimer;
	protected:
	};

}
