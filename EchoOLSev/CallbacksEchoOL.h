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
#include "MainEchoOLSev.h"
#include "SocketContextEchoOL.h"
//#include <sal.h>
#include "RingBuf.h"

namespace EchoOLSev {
    constexpr auto ELM_SIZE = 0x8000;   //0x4000;/*16384*/
    constexpr auto HOST_FRONT_LISTEN_BASE_ADDR = "127.0.0.2";
    constexpr u_int HOST_FRONT_LISTEN_PORT = 50000;

    VOID CALLBACK OnListenCompCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PVOID                 Overlapped,
        ULONG                 IoResult,
        ULONG_PTR             NumberOfBytesTransferred,
        PTP_IO                Io
    );

    VOID CALLBACK OnSocketFrontNoticeCompCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PVOID                 Overlapped,
        ULONG                 IoResult,
        ULONG_PTR             NumberOfBytesTransferred,
        PTP_IO                Io
    );

    VOID CALLBACK SendWorkCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WORK              Work
    );

    VOID CALLBACK RecvWorkCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WORK              Work
    );

    BOOL Send(SocketContext* pSocket);
    BOOL Recv(SocketContext* pSocket);
 
    VOID CALLBACK MeasureConnectedPerSecCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_TIMER             Timer
    );

    void CleanupSocket(SocketContext* pSocket);
    int StartListen(SocketListenContext*);
    LPFN_ACCEPTEX GetAcceptEx(SocketListenContext*pAcceptSocket);
    LPFN_GETACCEPTEXSOCKADDRS GetGetAcceptExSockaddrs(SocketContext* pListenSocket);
    void EndListen(SocketListenContext*pListen);
    void ShowStatus();
    void ClearStatus();
    void Cls();
    std::string SplitLastLineBreak(std::string &str);
    bool PreAccept(SocketListenContext*pListenSocket);
#ifdef _DEBUG
#define    MyTRACE(lpsz) OutputDebugStringA(lpsz);
#else
#define MyTRACE __noop
#endif
}