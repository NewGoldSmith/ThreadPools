//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side.

#include <iostream>
#include "MainR.h"
	//サーバーサイド

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

int main()
{
	using namespace ThreadPoolServerR;
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
	{
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
			InitTP();
			StartListen();
			for (;;)
			{
				std::string strin;
				std::getline(std::cin, strin);
				std::transform(strin.begin(),strin.end(),strin.begin(), tolower);
				if (strin == "quit")
				{
					break;
				}
				else if (strin == "status") {
					ThreadPoolServerR::ShowStatus();
				}
				else if (strin == "clearstatus")
				{
					ClearStatus();
				}
			}
			EndListen();
		}
	}
	return 0;
}
