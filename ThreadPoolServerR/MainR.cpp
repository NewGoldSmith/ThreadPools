//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side.

#include <iostream>
#include "MainR.h"
	//サーバーサイド

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
	InitTP();
	StartListen();
	for (;;)
	{
		std::string strin;
		std::getline(std::cin, strin);
		if (strin == "quit")
		{
			break;
		}
		else if (strin == "status") {
			ThreadPoolServerR::ShowStatus();
		}
	}
	EndListen();
	return 0;
}
