#pragma once
#include <SocketHelper.h>
#include "CommandOpEchoRepL5.h"
#include "EchoRepL5.h"


namespace MainCliWork
{
	bool CliWork();
	bool CliReadWriteWork(SOCKET CliSocket);
}