//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side.

#include <iostream>
#include "MainOL.h"
	//サーバーサイド
namespace SevOL {
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

	extern const std::unique_ptr
		< TP_POOL
		, decltype(CloseThreadpool)*
		> ptpp
	{ /*WINBASEAPI Must_inspect_result PTP_POOL WINAPI*/CreateThreadpool
		( /*Reserved PVOID reserved*/nullptr
		)
	, /*WINBASEAPI VOID WINAPI */CloseThreadpool/*(_Inout_ PTP_POOL ptpp)*/
	};

	SocketListenContext gListenContext;
	SocketListenContext* gpListenContext = &gListenContext;
}

int main()
{
	using namespace SevOL;


	FILETIME filetime{};
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

	const std::unique_ptr
		< TP_CLEANUP_GROUP
		, decltype(CloseThreadpoolCleanupGroup)*
		> ptpcg
	{ /*WINBASEAPI Must_inspect_result PTP_CLEANUP_GROUP WINAPI */CreateThreadpoolCleanupGroup(/*VOID*/)
	, [](_Inout_ PTP_CLEANUP_GROUP ptpcg)
		{
			/*WINBASEAPI VOID WINAPI*/CloseThreadpoolCleanupGroupMembers
			( /*Inout     PTP_CLEANUP_GROUP ptpcg                  */ptpcg
			, /*In        BOOL              fCancelPendingCallbacks*/false
			, /*Inout_opt PVOID             pvCleanupContext       */nullptr
			);
			/*WINBASEAPI VOID WINAPI*/CloseThreadpoolCleanupGroup(/*Inout PTP_CLEANUP_GROUP ptpcg*/ptpcg);
		}
	};

	/*FORCEINLINE VOID*/SetThreadpoolCallbackCleanupGroup
	( /*Inout  PTP_CALLBACK_ENVIRON              pcbe */&*pcbe
		, /*In     PTP_CLEANUP_GROUP                 ptpcg*/&*ptpcg
		, /*In_opt PTP_CLEANUP_GROUP_CANCEL_CALLBACK pfng */nullptr
	);

	{

		StartListen(gpListenContext);
		for (;;)
		{
			std::string strin;
			std::getline(std::cin, strin);
			if (strin == "quit")
			{
				break;
			}
			else if (strin == "status") {
				ShowStatus();
			}
			else if (strin == "clearstatus")
			{
				ClearStatus();
			}
		}
		EndListen(gpListenContext);
	}

	return 0;
}
