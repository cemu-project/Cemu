#pragma once

#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/nn_common.h"
#include "Cafe/OS/libs/nn_act/nn_act.h"
#include "Cafe/CafeSystem.h"
#include "Cemu/napi/napi.h"

#include "nn_olv_Common.h"

namespace nn
{
	namespace olv
	{

		extern ParamPackStorage g_ParamPack;
		extern DiscoveryResultStorage g_DiscoveryResults;

		sint32 GetOlvAccessKey(uint32_t* pOutKey);

		void load();
		void unload();
	}
}