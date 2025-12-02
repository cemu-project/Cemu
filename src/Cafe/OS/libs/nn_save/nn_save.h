#pragma once
#include "Cafe/OS/RPL/COSModule.h"

namespace nn::save
{
    void ResetToDefaultState();

	bool GetPersistentIdEx(uint8 accountSlot, uint32* persistentId);

	COSModule* GetModule();
}
