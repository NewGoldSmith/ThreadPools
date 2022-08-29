//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#include "RoundContext.h"

using namespace std;
namespace FrontSevEv {
	RoundContext::RoundContext()
		:hSocket(NULL)
		, ID(0)
		, hEvent{ []() {return WSACreateEvent(); }(), WSACloseEvent }
		, pFrontSocket(NULL)
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

	RoundContext::~RoundContext()
	{
		if (hSocket)
		{
			WSAEventSelect(hSocket, hEvent.get(), 0);
			closesocket(hSocket);
		}
	}

	void RoundContext::ReInitialize()
	{
		if (hSocket)
		{
			WSAEventSelect(hSocket, hEvent.get(), 0);
			closesocket(hSocket);
			hSocket = NULL;
		}
	}


}
