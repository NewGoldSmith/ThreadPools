//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#pragma once
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>
#include <vector>
#include <iostream>
#include <string>
#include <tchar.h>
#include "SevWorkR.h"

//リード・ライトループ。

constexpr auto N_FIN = 2;