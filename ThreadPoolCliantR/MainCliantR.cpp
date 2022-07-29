//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Cliant side.

#include <iostream>
#include "MainCliantR.h"
	//クライアントサイド


namespace ThreadPoolCliantR {

	extern const std::unique_ptr
		< TP_CALLBACK_ENVIRON
		, decltype(DestroyThreadpoolEnvironment)*
		> pcbe
	{ []()
		{
			const auto pcbe = new TP_CALLBACK_ENVIRON;
			/*FORCEINLINE VOID*/InitializeThreadpoolEnvironment
			( /*Out PTP_CALLBACK_ENVIRON pcbe*/pcbe
			);
			return pcbe;
		}()
	, [](_Inout_ PTP_CALLBACK_ENVIRON pcbe)
		{
			/*FORCEINLINE VOID*/DestroyThreadpoolEnvironment
			( /*Inout PTP_CALLBACK_ENVIRON pcbe*/pcbe
			);
			delete pcbe;
		}
	};
}

int main()
{
	using namespace ThreadPoolCliantR;

	const std::unique_ptr
		< WSADATA
		, void(*)(WSADATA*)
		> pwsadata
	{ []()
		{
			auto pwsadata = new WSADATA;
				if (WSAStartup(MAKEWORD(2, 2), pwsadata))
				{
					throw std::runtime_error("error!");
				}
		return pwsadata;
		}()
	, [](_Inout_ WSADATA* pwsadata)
		{
			WSACleanup();
			delete pwsadata;
		}
	};

	const std::unique_ptr
		< TP_POOL
		, decltype(CloseThreadpool)*
		> ptpp
	{ /*WINBASEAPI Must_inspect_result PTP_POOL WINAPI*/CreateThreadpool
		( /*Reserved PVOID reserved*/nullptr
		)
	, /*WINBASEAPI VOID WINAPI */CloseThreadpool/*(_Inout_ PTP_POOL ptpp)*/
	};
	/*WINBASEAPI VOID WINAPI*/SetThreadpoolThreadMaximum
	( /*Inout PTP_POOL ptpp     */&*ptpp
		, /*In    DWORD    cthrdMost*/MAX_TASKS
	);
	(void)/*WINBASEAPI BOOL WINAPI*/SetThreadpoolThreadMinimum
	( /*Inout PTP_POOL ptpp    */&*ptpp
		, /*In    DWORD    cthrdMic*/MIN_TASKS
	);

	/*FORCEINLINE VOID*/SetThreadpoolCallbackPool
	( /*Inout PTP_CALLBACK_ENVIRON pcbe*/&*pcbe
		, /*In    PTP_POOL             ptpp*/&*ptpp
	);

	Sleep(2000);
	TryConnect();

	for (;;)
	{
		std::string strin;
		std::getline(std::cin, strin);
		if (strin == "quit")
		{
			break;
		}
		else if (strin == "tryconnect") {
			TryConnect();
		}
		else if (strin == "status") {
			ShowStatus();
		}
	}
}
