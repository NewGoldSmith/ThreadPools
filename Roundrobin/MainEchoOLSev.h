//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <MSWSock.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <algorithm>
#include <iostream>
#include "CallbacksRoundrobin.h"
#include "SocketContextRoundrobin.h"

namespace RoundrobinSev {

	constexpr auto MAX_TASKS = 2;
	constexpr auto MIN_TASKS = 1;
}