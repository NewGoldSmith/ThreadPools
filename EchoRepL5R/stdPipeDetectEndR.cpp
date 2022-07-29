#include "stdPipeDetectEnd.h"


void DetectEnd_EchoL5::StdPipeThread(ThreadParam* pParam)
{
	while ( !(pParam->bForcedTermination))
	{
		std::cin >> pParam->INCommand;
		if (!(pParam->INCommand.compare("quit")))
		{
			pParam->bEndFlag = true;
		}
	}
}
