#pragma once
#include <thread>
#include <iostream>
#include <string>
#include <WinSock2.h>

namespace DetectEnd_EchoL5 {
	//�W�����͏I���R�}���h���͌��o�X���b�h
	struct ThreadParam
	{
		OVERLAPPED OL = {};
		std::string INCommand = "";
		bool bForcedTermination = false;
		bool bEndFlag = false;
	};
	void StdPipeThread(ThreadParam* pParam);
}
