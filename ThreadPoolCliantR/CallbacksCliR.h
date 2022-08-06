//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Cliant side
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
#include < ctime >
#include "SocketContextCliR.h"
#include "RingBuf.h"


namespace ThreadPoolCliantR {

	constexpr auto ELM_SIZE = 0x4000;
	constexpr auto NUM_THREAD = 3;
	constexpr auto NUM_CONNECT = 3000;

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

	VOID CALLBACK TryConnectCB(
		PTP_CALLBACK_INSTANCE Instance,
		PVOID                 Context,
		PTP_WORK              Work
	);

	std::string SplitLastLineBreak(std::string& str);
	int TryConnect();
	void ShowStatus();
	void ClearStatus();
	void StartTimer(SocketContext* pSocket);
	FILETIME* Make1000mSecFileTime(FILETIME* pfiletime);
	bool MakeAndSendSocketMessage(SocketContext* pSocket);
	u_int FindCountDown(const std::string& str);

#ifdef MY_DEBUG
#define    MyTRACE(lpsz) OutputDebugStringA(lpsz);
#else
#define MyTRACE __noop
#endif


}