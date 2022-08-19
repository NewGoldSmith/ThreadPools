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
#include "MainOL.h"
#include "SocketContextOL.h"
//#include <sal.h>
#include "RingBuf.h"

namespace SevOL {
    constexpr auto ELM_SIZE = 0x4000;   //0x4000;/*16384*/
    constexpr auto HOST_ADDR = "127.0.0.2";
    constexpr u_int HOST_PORT = 50000;
    constexpr auto BACK_HOST_ADDR = "127.0.0.3";
    constexpr u_short BACK_HOST_PORT = 0;
    constexpr auto TO_BACK_END_ADDR = "127.0.0.4";
    constexpr u_int TO_BACK_END_PORT = 50000;

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
    /// <summary>
    /// バックエンドDBとのCB
    /// </summary>
    /// <param name="Instance"></param>
    /// <param name="Context"></param>
    /// <param name="Overlapped"></param>
    /// <param name="IoResult"></param>
    /// <param name="NumberOfBytesTransferred"></param>
    /// <param name="Io"></param>
    VOID CALLBACK OnSocketBackNoticeCompCB(
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

    VOID CALLBACK SendBackWorkCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WORK              Work
    );

    VOID CALLBACK RecvWorkCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WORK              Work
    );

    VOID CALLBACK RecvBackWorkCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WORK              Work
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
    /// <summary>
    /// クライアントとしてバックエンドDBに接続。クリンナップはしない。成功はTRUE、失敗はFALSE。
    /// </summary>
    /// <param name="pSocket"></param>
    /// <returns></returns>
    BOOL TryConnectBack(SocketContext*pSocket);
    void ShowStatus();
    void ClearStatus();
    std::string SplitLastLineBreak(std::string &str);
    bool PreAccept(SocketListenContext*pListenSocket);
#ifdef _DEBUG
#define    MyTRACE(lpsz) OutputDebugStringA(lpsz);
#else
#define MyTRACE __noop
#endif
}