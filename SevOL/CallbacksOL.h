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
//#include <tchar.h>
#include <iostream>
#include <string>
#include <semaphore>
#include <exception>
#include <algorithm>
#include "MainOL.h"
#include "SocketContextOL.h"
//#include <sal.h>
#include "RingBuf.h"

namespace SevOL {
    constexpr auto ELM_SIZE = 0x4000;   //0x4000;/*16384*/
    constexpr auto PRE_ACCEPT = 1;

    VOID CALLBACK OnListenCompCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PVOID                 Overlapped,
        ULONG                 IoResult,
        ULONG_PTR             NumberOfBytesTransferred,
        PTP_IO                Io
    );

    VOID CALLBACK OnSocketNoticeCompCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PVOID                 Overlapped,
        ULONG                 IoResult,
        ULONG_PTR             NumberOfBytesTransferred,
        PTP_IO                Io
    );

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
    std::string SplitLastLineBreak(std::string &str);
     bool DoSend(SocketContext* pAcceptSocket);
    bool DoRecv(SocketContext* pAcceptSocket);
    void PreAccept(SocketListenContext*pListenSocket);
    FILETIME* Make1000mSecFileTime(FILETIME *pfiletime);
#ifdef _DEBUG
#define    SockTRACE(pAcceptSocket) SerializedSocketDebugPrint( pAcceptSocket)
#define    MyTRACE(lpsz) OutputDebugStringA(lpsz);
#else
#define SockTRACE __noop
#define MyTRACE __noop
#endif
}