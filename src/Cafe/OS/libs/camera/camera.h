#pragma once
#include "Cafe/OS/RPL/COSModule.h"

namespace camera
{
	sint32 CAMOpen(sint32 camHandle);
	sint32 CAMClose(sint32 camHandle);

	COSModule* GetModule();
};