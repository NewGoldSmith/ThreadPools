
#include <windows.h>
#include <vector>
#include <memory>
#include <iostream>
#include <algorithm>

VOID NTAPI Callback1(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
{
	for (int i = 0; i < 3; i++)
	{
		std::cout << "wait3 " << i + 1 << "/3 " << static_cast<LPCSTR>(Context) << std::endl;
		Sleep(1000);
	}
}
VOID NTAPI Callback2(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
{
	std::cout << "wait1 1/1 " << static_cast<LPCSTR>(Context) << std::endl;
	Sleep(1000);
}
int main()
{
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
	constexpr auto MAX_TASKS = 2;
	constexpr auto MIN_TASKS = 1;
	/*WINBASEAPI VOID WINAPI*/SetThreadpoolThreadMaximum
	( /*Inout PTP_POOL ptpp     */&*ptpp
		, /*In    DWORD    cthrdMost*/MAX_TASKS
	);
	(void)/*WINBASEAPI BOOL WINAPI*/SetThreadpoolThreadMinimum
	( /*Inout PTP_POOL ptpp    */&*ptpp
		, /*In    DWORD    cthrdMic*/MIN_TASKS
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
	/*FORCEINLINE VOID*/SetThreadpoolCallbackPool
	( /*Inout PTP_CALLBACK_ENVIRON pcbe*/&*pcbe
		, /*In    PTP_POOL             ptpp*/&*ptpp
	);
	const auto rgpwk = [&pcbe](const std::initializer_list< std::pair<PTP_WORK_CALLBACK, const void*>>& a)->std::vector<PTP_WORK>
	{
		std::vector<PTP_WORK> retval;
		retval.reserve(a.size());
		for (const auto& r : a) {
			retval.push_back(/*WINBASEAPI Must_inspect_result PTP_WORK WINAPI*/::CreateThreadpoolWork
			( /*In        PTP_WORK_CALLBACK    pfnwk*/r.first
				, /*Inout_opt PVOID                pv   */const_cast<void*>(r.second)
				, /*In_opt    PTP_CALLBACK_ENVIRON pcbe */&*pcbe
			));
			::SubmitThreadpoolWork(retval.back());
			Sleep(5);//îªÇËÇ‚Ç∑Ç¢ÇÊÇ§Ç…è≠ÇµÇ∏ÇÁÇ∑ÅB
		}
		return retval;
	}(
		{ { Callback1, "test1"}
		, { Callback2, "test2"}
		, { Callback2, "test3"}
		});
	std::for_each(rgpwk.begin(), rgpwk.end(), [](const auto& pwk)
		{
			/*WINBASEAPI VOID WINAPI*/::WaitForThreadpoolWorkCallbacks
			( /*Inout PTP_WORK pwk                    */pwk
				, /*In    BOOL     fCancelPendingCallbacks*/false
			);
		});
	return 0;
}