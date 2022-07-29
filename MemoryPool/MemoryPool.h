#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>
#include <vector>
#include <iostream>
#include <string>

struct Context {
	SOCKET hSocket;
};