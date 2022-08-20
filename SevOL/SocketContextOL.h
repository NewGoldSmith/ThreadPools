//Copyright (c) 2022, Gold Smith
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
    constexpr auto BUFFER_SIZE = 1024;

    using namespace std;
    struct DirOVERLAPPED :public WSAOVERLAPPED {
        DirOVERLAPPED(int dir) :WSAOVERLAPPED{},Dir(dir) {}
        const int Dir;
    };

    class SocketContext :public WSAOVERLAPPED {
    public:
        SocketContext();
        ~SocketContext();
        void ReInitialize();
        void WsaToStr(WSABUF* pwsa, string* pstr);
        void StrToWsa(string* pstr, WSABUF* pwsa);
        WSABUF wsaReadBuf;
        WSABUF wsaWriteBuf;
        WSABUF wsaReadBackBuf;
        WSABUF wsaWriteBackBuf;
        SOCKET hSocket;
        SOCKET hSocketBack;
        string ReadBuf;
        string WriteBuf;
        string ReadBackBuf;
        string WriteBackBuf;
        string RemBuf;
        u_short ID;
        enum eDir { DIR_NOT_SELECTED = 0, DIR_TO_BACK, DIR_TO_FRONT };
        eDir Dir;
        eDir DirBack;
        WSAOVERLAPPED OLBack;
        DWORD flags;
        DWORD flagsBack;
        TP_IO* pTPIo;
        TP_IO* pTPBackIo;
        BOOL fFrontReEnterGuard;
        BOOL fBackReEnterGuard;
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