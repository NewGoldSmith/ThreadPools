//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>
#include <iostream>
#include <string>
#include <mutex>
#include <semaphore>
#include <cassert>
#include <exception>


constexpr auto BUFFER_SIZE = 1024;

namespace ThreadPoolServerR {

    class SocketContext {
    public:
        SocketContext();
        ~SocketContext();
        void ReInitialize();
        SOCKET hSocket;
        u_short SocketID;
        std::string Buf;
        std::string RemString;
        WSAEVENT hEvent;
        PTP_WAIT ptpwaitOnEvListen;
    };
}