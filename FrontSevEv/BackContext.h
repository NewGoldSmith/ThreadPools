//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php
#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <semaphore>
#include <algorithm>
#include "FrontContext.h"
#include "CBFront.h"
using namespace std;
namespace FrontSevEv {
	class FrontContext;
	class BackContext
	{
		friend FrontContext;
	public:
		BackContext();
		~BackContext();
		void ReInitialize();
		SOCKET hSocket;
		u_int ID;
		FrontContext* pFrontSocket;
		unique_ptr<remove_pointer_t<WSAEVENT>, decltype(WSACloseEvent)*> hEvent;
		PTP_WAIT pTPWait;
	protected:
		binary_semaphore semConnectLock;
	};
}
