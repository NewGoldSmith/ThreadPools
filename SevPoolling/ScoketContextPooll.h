//Copyright (c) 2021, Gold Smith
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
#include <mutex>
#include <semaphore>
#include <cassert>
#include <exception>



namespace SevPooll {
    constexpr auto BUFFER_SIZE = 1024;

    using namespace std;

    class SocketContext {
    public:
        SocketContext();
        ~SocketContext();
        void ReInitialize();
        SOCKET hSocket;
        string ReadBuf;
        string WriteBuf;
        string RemBuf;
        u_short ID;
    };
}