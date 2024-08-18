#pragma once

namespace nn
{
namespace save
{
	void load();
    void ResetToDefaultState();

	bool GetPersistentIdEx(uint8 accountSlot, uint32* persistentId);
}
}
