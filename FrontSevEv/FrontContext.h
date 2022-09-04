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
#include "BackContext.h"


constexpr auto BUFFER_SIZE = 1024;

namespace FrontSevEv {
    using namespace std;
    class BackContext;

    class FrontContext {
        friend BackContext;
    public:
        FrontContext();
        ~FrontContext();
        void ReInitialize();
        SOCKET hSocket;
        u_short ID;
        string ReadBuf;
        string WriteBuf;
        vector<string> vBuf;
        binary_semaphore vBufLock;
        string RemString;
        WSAEVENT hEvent;
        PTP_WAIT ptpwaitOnEvListen;
        BackContext* pBackSocket;
        PTP_WAIT pTPWait;
    };
}