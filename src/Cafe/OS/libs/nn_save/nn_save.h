#pragma once

namespace nn
{
namespace save
{
	void save(MemStreamWriter& s);
	void restore(MemStreamReader& s);

	void load();
    void ResetToDefaultState();

	bool GetPersistentIdEx(uint8 accountSlot, uint32* persistentId);
}
}
