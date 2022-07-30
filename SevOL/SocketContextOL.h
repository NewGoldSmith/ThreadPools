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



namespace SevOL {
    constexpr auto ELM_SIZE = 0x4000;   //0x4000;/*16384*/
    constexpr auto BUFFER_SIZE = 1024;
    constexpr auto OL_READ_CYCLE = 1;
    constexpr auto OL_WRITE_CYCLE = 2;

    using namespace std;
 
    class SocketContext :public OVERLAPPED {
    public:
        SocketContext();
        ~SocketContext();
        void ReInitialize();
        void InitWsaBuf(WSABUF* pwsa, string* pstr);
        void WsaToStr(WSABUF* pwsa, string* pstr);
        void StrToWsa(string* pstr, WSABUF* pwsa);
        WSABUF wsaReadBuf;
        WSABUF wsaWriteBuf;
        WSABUF wsaRemBuf;
        SOCKET hSocket;
        string ReadBuf;
        string WriteBuf;
        string RemBuf;
        u_short ID;
        u_short usCycle;
        DWORD NumberOfBytesSent;
        DWORD NumberOfBytesRecvd;
        DWORD flags;

    };

    class SocketListenContext
        : public SocketContext
    {
    public:
        SocketListenContext();
        ~SocketListenContext();
        void ReInitialize();
        PTP_IO pTPListen;
    };

    WSABUF* SocketInitWsaBuf(WSABUF* pwsabuf);
}