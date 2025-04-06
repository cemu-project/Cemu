#pragma once
#include "Cafe/OS/libs/nfc/nfc.h"

#define NTAG_RESULT_SUCCESS		(0)
#define NTAG_RESULT_UNINITIALIZED	(-0x3E7)
#define NTAG_RESULT_INVALID_STATE	(-0x3E6)
#define NTAG_RESULT_NO_TAG		(-0x3E5)
#define NTAG_RESULT_INVALID		(-0x3E1)
#define NTAG_RESULT_UID_MISMATCH	(-0x3DB)

namespace ntag
{
	struct NTAGFormatSettings
	{
		/* +0x00 */ uint8 version;
		/* +0x04 */ uint32 makerCode;
		/* +0x08 */ uint32 identifyCode;
		/* +0x0C */ uint8 reserved[0x1C];
	};
	static_assert(sizeof(NTAGFormatSettings) == 0x28);

#pragma pack(1)
	struct NTAGNoftHeader
	{
		/* +0x00 */ uint32 magic;
		/* +0x04 */ uint8 version;
		/* +0x05 */ uint16 writeCount;
		/* +0x07 */ uint8 unknown;
	};
	static_assert(sizeof(NTAGNoftHeader) == 0x8);
#pragma pack()

	struct NTAGInfoHeader
	{
		/* +0x00 */ uint16 rwHeaderOffset;
		/* +0x02 */ uint16 rwSize;
		/* +0x04 */ uint16 roHeaderOffset;
		/* +0x06 */ uint16 roSize;
		/* +0x08 */ nfc::NFCUid uid;
		/* +0x0F */ uint8 formatVersion;
	};
	static_assert(sizeof(NTAGInfoHeader) == 0x10);

	struct NTAGAreaHeader
	{
		/* +0x00 */ uint16 magic;
		/* +0x02 */ uint16 offset;
		/* +0x04 */ uint16 size;
		/* +0x06 */ uint16 padding;
		/* +0x08 */ uint32 makerCode;
		/* +0x0C */ uint32 identifyCode;
	};
	static_assert(sizeof(NTAGAreaHeader) == 0x10);

	struct NTAGAreaInfo
	{
		/* +0x00 */ MPTR data;
		/* +0x04 */ uint16 size;
		/* +0x06 */ uint16 padding;
		/* +0x08 */ uint32 makerCode;
		/* +0x0C */ uint32 identifyCode;
		/* +0x10 */ uint8 reserved[0x20];
	};
	static_assert(sizeof(NTAGAreaInfo) == 0x30);

	struct NTAGData
	{
		/* +0x00 */ nfc::NFCUid uid;
		/* +0x07 */ uint8 readOnly;
		/* +0x08 */ uint8 formatVersion;
		/* +0x09 */ uint8 padding[3];
		/* +0x0C */ NTAGAreaInfo rwInfo;
		/* +0x3C */ NTAGAreaInfo roInfo;
		/* +0x6C */ uint8 reserved[0x20];
	};
	static_assert(sizeof(NTAGData) == 0x8C);

	sint32 NTAGInit(uint32 chan);

	sint32 NTAGInitEx(uint32 chan);

	sint32 NTAGShutdown(uint32 chan);

	bool NTAGIsInit(uint32 chan);

	void NTAGProc(uint32 chan);

	void NTAGSetFormatSettings(NTAGFormatSettings* formatSettings);

	void NTAGSetTagDetectCallback(uint32 chan, MPTR callback, void* context);

	sint32 NTAGAbort(uint32 chan, MPTR callback, void* context);

	sint32 NTAGRead(uint32 chan, uint32 timeout, nfc::NFCUid* uid, nfc::NFCUid* uidMask, MPTR callback, void* context);

	sint32 NTAGWrite(uint32 chan, uint32 timeout, nfc::NFCUid* uid, uint32 rwSize, void* rwData, MPTR callback, void* context);

	sint32 NTAGFormat(uint32 chan, uint32 timeout, nfc::NFCUid* uid, uint32 rwSize, void* rwData, MPTR callback, void* context);

	void Initialize();
}
