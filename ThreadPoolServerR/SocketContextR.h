//Copyright (c) 2021, Gold Smith
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
        u_short ID;
        std::string ReadString;
        std::binary_semaphore readlock;
        std::string WriteString;
        std::binary_semaphore writelock;
        std::vector<std::string> vstr;
        std::binary_semaphore vstrlock;
        std::string RemString;
        WSAEVENT hEvent;
        PTP_WAIT ptpwaitOnEvListen;
        PTP_WAIT ptpwaitOnEvSocket;
    };
}