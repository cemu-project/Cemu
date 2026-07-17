#pragma once

#include "Cafe/OS/RPL/COSModule.h"

class ModExecutionContext;
class CemodRuntime;

namespace cemuextend_hle
{
	COSModule* GetModule();
	void ConfigureCex2HleAccess(ModExecutionContext& context);
	CemodRuntime& GetCemodRuntime();
}
