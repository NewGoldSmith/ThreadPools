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
    struct DirOVERLAPPED :public WSAOVERLAPPED {
        DirOVERLAPPED(int dir) :WSAOVERLAPPED{}, Dir(dir) {}
        const int Dir;
    };

    class SocketContext :public WSAOVERLAPPED {
    public:
        SocketContext();
        ~SocketContext();
        void ReInitialize();
        void InitWsaBuf(WSABUF* pwsa, string* pstr);
        void WsaToStr(WSABUF* pwsa, string* pstr);
        void StrToWsa(string* pstr, WSABUF* pwsa);
        HANDLE *phEvEnd;


        WSABUF wsaReadBuf;
        WSABUF wsaWriteBuf;
        SOCKET hSocket;
        string ReadBuf;
        string WriteBuf;
        string RemBuf;
        u_short ID;
        enum eDir {
            OL_NOT_SELECTED = 0, OL_RECV, OL_SEND
        };
        enum eDir Dir;
        DWORD NumberOfBytesSent;
        DWORD NumberOfBytesRecvd;
        DWORD flags;
        TP_IO* pTPIo;
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

}