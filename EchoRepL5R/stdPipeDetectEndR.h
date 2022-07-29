#pragma once
#include <thread>
#include <iostream>
#include <string>
#include <WinSock2.h>

namespace DetectEnd_EchoL5 {
	//標準入力終了コマンド入力検出スレッド
	struct ThreadParam
	{
		OVERLAPPED OL = {};
		std::string INCommand = "";
		bool bForcedTermination = false;
		bool bEndFlag = false;
	};
	void StdPipeThread(ThreadParam* pParam);
}
