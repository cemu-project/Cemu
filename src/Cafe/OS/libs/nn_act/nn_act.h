#pragma once

namespace nn
{
namespace act
{
	uint32 Initialize();

	uint32 GetPersistentIdEx(uint8 slot);
	uint32 GetUuidEx(uint8* uuid, uint8 slot, sint32 name = -2);
	uint32 GetSimpleAddressIdEx(uint32be* simpleAddressId, uint8 slot);

	static uint32 getCountryCodeFromSimpleAddress(uint32 simpleAddressId)
	{
		return (simpleAddressId>>24)&0xFF;
	}

	const uint8 ACT_SLOT_CURRENT = 0xFE;
}
}

void nnAct_load();