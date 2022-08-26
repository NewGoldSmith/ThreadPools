//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#include "CallbacksRoundrobin.h"
using namespace std;
namespace RoundrobinSev{
	const std::unique_ptr
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

	const std::unique_ptr
		< TP_POOL
		, decltype(CloseThreadpool)*
		> ptpp
	{ /*WINBASEAPI Must_inspect_result PTP_POOL WINAPI*/CreateThreadpool
		( /*Reserved PVOID reserved*/nullptr
		)
	, /*WINBASEAPI VOID WINAPI */CloseThreadpool/*(_Inout_ PTP_POOL ptpp)*/
	};

	const std::unique_ptr
		< std::remove_pointer_t<HANDLE>
		, decltype(CloseHandle)*
		> gpSem
	{ []()
		{
			return CreateSemaphoreA(NULL, 4, 4, NULL);
		}()
		,
		CloseHandle
	};

	RoundContext RC[16];
	RingBuf rBuf(RC, _countof(RC));

	VOID RoundWaitCB(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
	{
		RoundContext* pRC = (RoundContext*)Context;
		if (WaitResult)
		{
			cerr << "Err!Code:" << to_string(WaitResult) << "\r\n";
			return;
		}
		ReleaseSemaphoreWhenCallbackReturns(Instance, pRC->hRoundSem, 1);
		cout << "RoundWaitCB Start"+ to_string(pRC->ID)+"\r\n";
		Sleep(1000);
		cout << "RoundWaitCB End" + to_string(pRC->ID) + "\r\n";
		CloseThreadpoolWait(Wait);
		pRC->ReInit();
		rBuf.Push(pRC);

		return;
	}

	void Start( TP_CLEANUP_GROUP * ptpcg)
	{
		cout << "Start\r\n";

		for (int i(0); i < 16; ++i)
		{
			RoundContext* pRC = rBuf.Pull();
			pRC->hRoundSem = gpSem.get();
			pRC->ID = i;
			TP_WAIT* pTPWait(NULL);
			if (!(pTPWait = CreateThreadpoolWait(RoundWaitCB, pRC, &*pcbe)))
			{
				DWORD err = GetLastError();
				cerr << "Err!CreateThreadpoolWait. Code:" << to_string(err) << "\r\n";
				return;
			}
			SetThreadpoolWait(pTPWait, gpSem.get(), NULL);
			WaitForThreadpoolWaitCallbacks(pTPWait, FALSE);
		}
		cout << "End run.\r\n";
	}
}