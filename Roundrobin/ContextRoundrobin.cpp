//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

//Server side
#include "ContextRoundrobin.h"




namespace RoundrobinSev {
	RoundContext::RoundContext()
		:ID(0)
		,hRoundSem(NULL)
	{
	}

	RoundContext::~RoundContext()
	{
	}

	void RoundContext::ReInit()
	{
		ID = 0;
		hRoundSem = NULL;
	}
}