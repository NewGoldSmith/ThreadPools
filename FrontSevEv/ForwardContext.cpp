//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "ForwardContext.h"

namespace FrontSevEv {

	ForwardContext::ForwardContext()
		:hSocket(NULL)
		, ID(0)
		, RemString(BUFFER_SIZE, '\0')
		, ReadBuf(BUFFER_SIZE, '\0')
		, vBufLock(1)
		, hEvent(NULL)
		, ptpwaitOnEvListen(NULL)
		, RoundContext(NULL)
		, pTPWait(NULL)
	{
		try {
			hEvent = WSACreateEvent();
			if (!hEvent)
				throw std::runtime_error("error! ForwardContext::ForwardContext() WSACreateEvent()");
		}
		catch (std::exception& e) {
			std::cout << e.what() << std::endl;
		}
		RemString.resize(0);
		ReadBuf.resize(0);
	};

	ForwardContext::~ForwardContext()
	{
		if (ptpwaitOnEvListen)
		{
			CancelIo((HANDLE)hSocket);
			WaitForThreadpoolWaitCallbacks(ptpwaitOnEvListen, TRUE);
			CloseThreadpoolWait(ptpwaitOnEvListen);
		}
		CloseHandle(hEvent);
		hEvent = NULL;
		if (hSocket)
		{
			closesocket(hSocket);
			hSocket = NULL;
		}
	}
	void ForwardContext::ReInitialize()
	{
		if (pTPWait)
		{
			SetThreadpoolWait(pTPWait, NULL, 0);
			WaitForThreadpoolWaitCallbacks(pTPWait, TRUE);
			CloseThreadpoolWait(pTPWait);
			pTPWait = NULL;
		}
		if (ptpwaitOnEvListen)
		{
			SetThreadpoolWait(ptpwaitOnEvListen, NULL, 0);
			WaitForThreadpoolWaitCallbacks(ptpwaitOnEvListen, TRUE);
			CloseThreadpoolWait(ptpwaitOnEvListen);
			ptpwaitOnEvListen = NULL;
		}
		if (hSocket)
		{
			//ソケットイベントを無しにする。
			if (WSAEventSelect(hSocket, hEvent, 0) == SOCKET_ERROR)
			{
				DWORD Err = WSAGetLastError();
				stringstream  ss;
				ss << "FrontSevEv. WSAEventSelect. Socket ID:" << ID << " Code:" << Err << " Line: " << __LINE__ << "\r\n";
				//cerr << ss.str();
				MyTRACE(ss.str().c_str());
			}
			shutdown(hSocket, SD_SEND);
			closesocket(hSocket);
			hSocket = NULL;
		}
		ID = 0;
		RemString.clear();
		ReadBuf.clear();
		RoundContext = NULL;
		vBufLock.release();
	}
}