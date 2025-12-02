#include "Cafe/OS/RPL/COSModule.h"

bool mic_isActive(uint32 drcIndex);
void mic_updateOnAXFrame();

namespace mic
{
	COSModule* GetModule();
};