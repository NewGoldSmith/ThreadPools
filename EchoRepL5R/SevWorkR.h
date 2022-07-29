//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>
#include "MainEchoRepL5R.h"
#include "SocketContext.h"
#include <vector>
namespace MainSevWorkR
{
constexpr auto ELM_SIZE = 1000;
constexpr auto _SECOND = 10000000;
/// <summary>
	/// サーバー起動
	/// </summary>
	/// <returns>リッスンハンドル。NULLは失敗。</returns>
	HANDLE SevWork();
	u_int thprocListen(void*);
	u_int thprocSocket(void*);
	std::vector<std::string> SplitLineBreak(std::string& str);
	void ShowStatus();
	FILETIME* Make1000mSecFileTime(FILETIME* pFiletime);
	VOID CALLBACK TimerAPCProc(
		LPVOID lpArg,               // Data value
		DWORD dwTimerLowValue,      // Timer low value
		DWORD dwTimerHighValue);    // Timer high value

}