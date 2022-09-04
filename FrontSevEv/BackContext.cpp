//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#include "BackContext.h"

using namespace std;
namespace FrontSevEv {
	BackContext::BackContext()
		:hSocket(NULL)
		, ID(0)
		, hEvent{ []() {return WSACreateEvent(); }(), WSACloseEvent }
		, pFrontSocket(NULL)
		, pTPWait(NULL)
		, semConnectLock(1)
	{
		try {
			if (hEvent.get() == WSA_INVALID_EVENT)
			{
				throw runtime_error("The Event creation failed.\r\n");
			}
		}
		catch (runtime_error e) {
			cerr << e.what();
			MyTRACE(e.what());
			exception_ptr ep = current_exception();
			rethrow_exception(ep);
		};
	}

	BackContext::~BackContext()
	{
		if (hSocket)
		{
			WSAEventSelect(hSocket, hEvent.get(), 0);
			shutdown(hSocket, SD_SEND);
			closesocket(hSocket);
		}
	}

	void BackContext::ReInitialize()
	{
		if (pFrontSocket)
		{
			pFrontSocket->ReInitialize();
			pFrontSocket = NULL;
		}
		if (pTPWait)
		{
			SetThreadpoolWait(pTPWait, NULL, 0);
			WaitForThreadpoolWaitCallbacks(pTPWait, TRUE);
			CloseThreadpoolWait(pTPWait);
			pTPWait = NULL;
		}
		if (hSocket)
		{
			stringstream  ss;
			ss << "FrontSevEv. ReInitialize. Back Socket ID:" << ID << " Closed.\r\n";
			MyTRACE(ss.str().c_str());
			WSAEventSelect(hSocket, hEvent.get(), 0);
			shutdown(hSocket, SD_SEND);
			closesocket(hSocket);
			hSocket = NULL;
		}
	}


}
