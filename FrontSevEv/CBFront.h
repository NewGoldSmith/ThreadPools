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
#include <tchar.h>
#include <iostream>
#include <string>
#include <sstream>
#include <semaphore>
#include <exception>
#include "Main.h"
#include "FrontContext.h"
#include "RingBuf.h"
#include <vector>

namespace FrontSevEv {

    constexpr u_int ELM_SIZE = 0x4000;
    constexpr auto HOST_FRONT_LISTEN_BASE_ADDR= "127.0.0.2";
    constexpr u_short HOST_FRONT_LISTEN_PORT = 50000;

    VOID OnEvListenCB(
        PTP_CALLBACK_INSTANCE Instance, 
        PVOID Context, 
        PTP_WAIT Wait, 
        TP_WAIT_RESULT WaitResult
    );

    VOID CALLBACK OnEvSocketFrontCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WAIT              Wait,
        TP_WAIT_RESULT        WaitResult
    );

    VOID CALLBACK MeasureConnectedPerSecCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_TIMER             Timer
    );


    void InitTP();
    int StartListen();
    void EndListen();
    void ShowStatus();
    void ClearStatus();
    void Cls();
    void DecStatusFront();
    std::string SplitLastLineBreak(std::string& str);

#ifdef _DEBUG
#define MY_DEBUG
#endif
#ifdef MY_DEBUG
#define    MyTRACE(lpsz) OutputDebugStringA(lpsz);
#else
#define MyTRACE __noop
#endif
}