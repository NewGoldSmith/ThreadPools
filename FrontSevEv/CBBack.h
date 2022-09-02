//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php
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
#include <vector>
#include "Main.h"
#include "ForwardContext.h"
#include "RingBuf.h"
#include "RoundContext.h"
#include "CBForward.h"

namespace FrontSevEv {
	class ForwardContext;
	class RoundContext;
	constexpr auto HOST_BACK_BASE_ADDR = "127.0.0.3";
	constexpr auto PEER_BACK_BASE_ADDR = "127.0.0.10";
	constexpr u_short PEER_BACK_PORT = 50000;
	constexpr u_int NUM_BACK_CONNECTION = 4;//Ç◊Ç´èÊ
	VOID OnBackEvSocketCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult);
	VOID WriteBackWaitCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult);

	void QueryBack(ForwardContext* pSocket);
	BOOL InitBack();
	BOOL BackTryConnect(RoundContext*pBackSocket);
	void BackClose();
	BOOL BackReConnect();
}