#pragma once

#include "Cafe/IOSU/legacy/iosu_act.h"

struct independentServiceToken_t
{
	/* +0x000 */ char token[0x201];
};
static_assert(sizeof(independentServiceToken_t) == 0x201); // todo - verify size

namespace nn
{
namespace act
{
	uint32 Initialize();

	uint32 GetPersistentIdEx(uint8 slot);
	uint32 GetUuidEx(uint8* uuid, uint8 slot, sint32 name = -2);
	uint32 GetSimpleAddressIdEx(uint32be* simpleAddressId, uint8 slot);
	uint32 GetTransferableIdEx(uint64* transferableId, uint32 unique, uint8 slot);

	sint64 GetUtcOffset();
	sint32 GetUtcOffsetEx(sint64be* pOutOffset, uint8 slotNo);

	uint32 AcquireIndependentServiceToken(independentServiceToken_t* token, const char* clientId, uint32 cacheDurationInSeconds);

	static uint32 getCountryCodeFromSimpleAddress(uint32 simpleAddressId)
	{
		return (simpleAddressId>>24)&0xFF;
	}

	const uint8 ACT_SLOT_CURRENT = 0xFE;
}
}

void nnAct_load();