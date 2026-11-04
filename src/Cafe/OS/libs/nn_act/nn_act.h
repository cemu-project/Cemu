#pragma once

#include "Cafe/IOSU/legacy/iosu_act.h"
#include "Cafe/OS/RPL/COSModule.h"

struct independentServiceToken_t
{
	/* +0x000 */ char token[0x201];
};
static_assert(sizeof(independentServiceToken_t) == 0x201); // todo - verify size

// Passed to GetMiiImageEx. Each value maps to miiimgXX.dat in the account folder.
// Sizes and storage formats confirmed from real Wii U MLC dumps.
enum class ACTMiiImageType : uint32
{
	FaceIcon        = 0, // 128x128 BGRA TGA, stored raw (miiimg00.dat)
	FaceExpression1 = 1, // 96x96  BGRA TGA, zlib-compressed (miiimg01.dat)
	FaceExpression2 = 2, // 96x96  BGRA TGA, zlib-compressed (miiimg02.dat)
	FaceExpression3 = 3, // 96x96  BGRA TGA, zlib-compressed (miiimg03.dat)
	FaceExpression4 = 4, // 96x96  BGRA TGA, zlib-compressed (miiimg04.dat)
	FaceExpression5 = 5, // 96x96  BGRA TGA, zlib-compressed (miiimg05.dat)
	FaceExpression6 = 6, // 96x96  BGRA TGA, zlib-compressed (miiimg06.dat)
	FullBody        = 7, // 270x360 BGRA TGA, zlib-compressed (miiimg07.dat) - full standing body render
	FaceIconAlt     = 8, // 128x128 BGRA TGA, zlib-compressed (miiimg08.dat)
};
static constexpr uint32 ACT_MII_IMAGE_TYPE_MAX = 8;

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

	COSModule* GetModule();
}
}
