//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#include "MainEchoRepL5R.h"


//グローバル変数

namespace MainSevWorkR
{
	HANDLE ghEvDoEnd;
	extern HANDLE ghThListen;
//	HANDLE ghEvFin;

};


//メイン関数
int main(int argc, char* argv[], char* envp[])
{
	using namespace std;
	using namespace MainSevWorkR;
	HANDLE hListen(NULL);
	const std::unique_ptr
		< WSADATA
		, void(*)(WSADATA*)
		> pwsadata
	{ []()
		{
			auto pwsadata = new WSADATA;
				if (WSAStartup(MAKEWORD(2, 2), pwsadata))
				{
					throw std::runtime_error("error! WSAStartup. Line:" + __LINE__);
				}
		return pwsadata;
		}()
	, [](_Inout_ WSADATA* pwsadata)
		{
			WSACleanup();
			delete pwsadata;
		}
	};

	if (!(ghEvDoEnd = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		std::cerr << "CreateEvent err! Line:" << __LINE__ << "\r\n";
	}
	std::cout << "Enter the command.\r\n";

	hListen=SevWork();
	for (;;)
	{
		std::string str;
		std::getline(std::cin, str);
		std::string strlow;
		std::transform(str.begin(), str.end(), std::back_inserter(strlow), [](int x) {
			std::locale l = std::locale::classic();
			return std::tolower(x, l); });
		if (strlow == "sev")
		{
			hListen = SevWork();
		}
		else if (strlow == "quit")
		{
			break;
		}
		else if (strlow == "status")
		{
			ShowStatus();
		}
	}
	SetEvent(ghEvDoEnd);

	if (hListen)
	{
		CancelIoEx(hListen, NULL);
//		WaitForSingleObject(ghEvFin, INFINITE);
	}
	WaitForSingleObject(ghThListen, INFINITE);
	CloseHandle(ghThListen);
	CloseHandle(ghEvDoEnd);
//	CloseHandle(ghEvFin);
}

