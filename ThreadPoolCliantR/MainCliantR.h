//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Cliant side
#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <iostream>
#include "CallbacksCliR.h"
#include "SocketContextCliR.h"
#pragma comment(lib, "ws2_32.lib")


constexpr auto MAX_TASKS = 3;
constexpr auto MIN_TASKS = 1;
