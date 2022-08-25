//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php
#include "RRContext.h"

namespace FrontSevEv {
		RoundrobinContext::RoundrobinContext()
			:ID(0)
			,hSem(NULL)
		{
		}

		RoundrobinContext::~RoundrobinContext()
		{
		}

		void RoundrobinContext::ReInit()
		{
			ID = 0;
			hSem = NULL;
		}

}
