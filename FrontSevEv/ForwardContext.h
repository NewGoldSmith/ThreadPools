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
#include <vector>
#include "RoundContext.h"


constexpr auto BUFFER_SIZE = 1024;

namespace FrontSevEv {
    using namespace std;
    class RoundContext;

    class ForwardContext {
        friend RoundContext;
    public:
        ForwardContext();
        ~ForwardContext();
        void ReInitialize();
        SOCKET hSocket;
        u_short ID;
        std::string ReadBuf;
        vector<string> vBuf;
        binary_semaphore vBufLock;
        std::string RemString;
        WSAEVENT hEvent;
        PTP_WAIT ptpwaitOnEvListen;
        RoundContext* RoundContext;
        PTP_WAIT pTPWait;
    };
}