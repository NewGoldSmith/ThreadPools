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
#include <tchar.h>
#include <iostream>
#include <string>
#include <semaphore>
#include <exception>
#include "MainR.h"
#include "SocketContextR.h"

namespace ThreadPoolServerR {

	VOID OnEvListenCB(
        PTP_CALLBACK_INSTANCE Instance, 
        PVOID Context, 
        PTP_WAIT Wait, 
        TP_WAIT_RESULT WaitResult
    );

    VOID CALLBACK OnEvSocketCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WAIT              Wait,
        TP_WAIT_RESULT        WaitResult
    );

    VOID CALLBACK SerializedSocketPrintCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WORK              Work
    );

    VOID CALLBACK SerializedSocketDebugPrintCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WORK              Work
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
    std::vector<std::string> SplitLineBreak(std::string& str);
    void SerializedPrint(ThreadPoolServerR::SocketContext* pSocket);
    FILETIME* Make1000mSecFileTime(FILETIME *pfiletime);
    void SerializedSocketDebugPrint(ThreadPoolServerR::SocketContext* pSocket);
#ifdef _DEBUG
#define    SockTRACE(pSocket) SerializedSocketDebugPrint( pSocket)
#define    MyTRACE(lpsz) OutputDebugStringA(lpsz);
#else
#define SockTRACE __noop
#define MyTRACE __noop
#endif
}