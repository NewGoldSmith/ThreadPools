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
#include "Main.h"
#include "SocketContext.h"
#include "RingBuf.h"
#include <vector>
