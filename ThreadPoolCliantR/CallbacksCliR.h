//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Cliant side
#pragma once
#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>
#include <iostream>
#include <string>
#include <semaphore>
#include <exception>
#include < ctime >
#include "SocketContextCliR.h"

constexpr auto ELM_SIZE = 20000;

namespace ThreadPoolCliantR {
	class SocketContext;

    VOID CALLBACK OnEvSocketCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WAIT              Wait,
        TP_WAIT_RESULT        WaitResult
	);

	VOID CALLBACK OneSecTimerCB(
		PTP_CALLBACK_INSTANCE Instance,
		PVOID                 Context,
		PTP_TIMER             Timer
	);

    VOID CALLBACK LateOneTempoCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_TIMER             Timer
    );

    /// <summary>
    /// コンソール出力中に、他のスレッドから割り込んで出力しないようにする。
    /// </summary>
    /// <param name="Instance"></param>
    /// <param name="Context"></param>
    /// <param name="Work"></param>
    VOID CALLBACK SerializedDisplayCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WORK              Work
    );

    VOID CALLBACK CloseSocketCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_TIMER             Timer
    );

    VOID CALLBACK TryConnectCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WORK              Work
    );

    VOID CALLBACK SerializedDebugPrintCB(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID                 Context,
        PTP_WORK              Work
    );


    std::vector<std::string> SplitLineBreak(std::string& strDisplay);
    int TryConnect();
    void ShowStatus();
    void StartTimer(SocketContext* pSocket);
    FILETIME* Make1000mSecFileTime(FILETIME *pfiletime);
    FILETIME* MakeNmSecFileFime(FILETIME* pfiletime, u_int ntime);
   void MakeAndSendSocketMessage(SocketContext* pSocket);
   u_int FindCountDown(const std::string& str);
   void CloseSocketContext(SocketContext* pSocket);
   /// <summary>
   /// コンソール出力中に他のスレッドから割り込んで出力しないようにする。
   /// </summary>
   /// <param name="pSocket">vstrにpush_backしたpSocket</param>
   void SerializedDebugPrint(ThreadPoolCliantR::SocketContext* pSocket);
#ifdef MY_DEBUG_
#define    SockTRACE(pSocket) SerializedDebugPrint( pSocket)
#define    MyTRACE(lpsz) OutputDebugStringA(lpsz);
#else
#define SockTRACE __noop
#define MyTRACE __noop
#endif


}