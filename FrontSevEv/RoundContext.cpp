//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#include "RoundContext.h"

using namespace std;
namespace FrontSevEv {
	RoundContext::RoundContext()
		:hSocket(NULL)
		, hEvent{ []() {return WSACreateEvent(); }(), WSACloseEvent }
		, pFrontSocket(NULL)
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
			closesocket(hSocket);
		}
	}

	void RoundContext::ReInitialize()
	{
		if (hSocket)
		{
			closesocket(hSocket);
			hSocket = NULL;
		}
		WSAResetEvent(hEvent.get());
	}


}
