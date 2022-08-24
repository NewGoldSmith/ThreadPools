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
#include "MainRoundrobin.h"
#include "ContextRoundrobin.h"
//#include <sal.h>
#include "RingBuf.h"

namespace RoundrobinSev {

	VOID CALLBACK RoundWaitCB(
		PTP_CALLBACK_INSTANCE Instance,
		PVOID                 Context,
		PTP_WAIT              Wait,
		TP_WAIT_RESULT        WaitResult
	);

	void Start( TP_CLEANUP_GROUP* ptpcg);
#ifdef _DEBUG
#define    MyTRACE(lpsz) OutputDebugStringA(lpsz);
#else
#define MyTRACE __noop
#endif
}