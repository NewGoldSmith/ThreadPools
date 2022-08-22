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
#include <semaphore>
#include <exception>
#include <algorithm>
#include "MainPooll.h"
#include "ScoketContextPooll.h"
//#include <sal.h>
#include "RingBuf.h"

namespace SevPooll {
    constexpr auto ELM_SIZE = 0x4000;   //0x4000;/*16384*/

    VOID CALLBACK TryAcceptTimerCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_TIMER             Timer
    );

    VOID CALLBACK RecvAndSendTimerCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_TIMER             Timer
    );

    VOID CALLBACK MeasureConnectedPerSecCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_TIMER             Timer
    );


    void CleanupSocket(SocketContext* pSocket);
    int StartListen(SocketContext*);
    void EndListen(SocketContext* pListen);
    void ShowStatus();
    void ClearStatus();
    std::string SplitLastLineBreak(std::string& str);
#ifdef MY_DEBUG
#define    MyTRACE(lpsz) OutputDebugStringA(lpsz);
#else
#define MyTRACE __noop
#endif
}
