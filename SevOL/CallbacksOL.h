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
#include "MainOL.h"
#include "SocketContextOL.h"
#include <sal.h>

namespace SevOL {

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

    void InitSocketPool();
    void CreanupAndPushSocket(SocketContext* pSocket);
    SocketContext* PopSocket();
    int StartListen();
    LPFN_ACCEPTEX GetAcceptEx(SocketListenContext*pAcceptSocket);
    LPFN_GETACCEPTEXSOCKADDRS GetGetAcceptExSockaddrs(SocketContext* pListenSocket);
    void EndListen();
    void ShowStatus();
    std::string SplitLastLineBreak(std::string &str);
    WSABUF* CopyStdStringToWsaString(std::string strsrc, WSABUF* pWsaBuf);
    bool DoSend(SocketContext* pAcceptSocket);
    bool DoRecv(SocketContext* pAcceptSocket);
    void PreAccept(SocketListenContext*pListenSocket);
    bool SetIOCPOnSocketNoticeCompCB(SocketContext* pSocket);
//    void SerializedPrint(SevOL::SocketContext* pSocket);
    FILETIME* Make1000mSecFileTime(FILETIME *pfiletime);
//    void SerializedSocketDebugPrint(SevOL::SocketContext* pSocket);
#ifdef _DEBUG
#define    SockTRACE(pAcceptSocket) SerializedSocketDebugPrint( pAcceptSocket)
#define    MyTRACE(lpsz) OutputDebugStringA(lpsz);
#else
#define SockTRACE __noop
#define MyTRACE __noop
#endif
}