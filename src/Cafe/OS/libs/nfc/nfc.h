#pragma once

// CEMU NFC error codes
#define NFC_TOUCH_TAG_ERROR_NONE			(0)
#define NFC_TOUCH_TAG_ERROR_NO_ACCESS			(1)
#define NFC_TOUCH_TAG_ERROR_INVALID_FILE_FORMAT		(2)

// NFC result base
#define NFC_RESULT_BASE_INIT		(-0x100)
#define NFC_RESULT_BASE_READ		(-0x200)
#define NFC_RESULT_BASE_WRITE		(-0x300)
#define NFC_RESULT_BASE_FORMAT		(-0x400)
#define NFC_RESULT_BASE_SET_READ_ONLY	(-0x500)
#define NFC_RESULT_BASE_IS_TAG_PRESENT	(-0x600)
#define NFC_RESULT_BASE_ABORT		(-0x700)
#define NFC_RESULT_BASE_SHUTDOWN	(-0x800)
#define NFC_RESULT_BASE_DETECT		(-0x900)
#define NFC_RESULT_BASE_SEND_RAW_DATA	(-0xA00)
#define NFC_RESULT_BASE_SET_MODE	(-0xB00)
#define NFC_RESULT_BASE_TAG_PARSE	(-0xC00)
#define NFC_RESULT_BASE_GET_TAG_INFO	(-0x1400)

// NFC result status
#define NFC_RESULT_NO_TAG		(0x01)
#define NFC_RESULT_INVALID_TAG		(0x02)
#define NFC_RESULT_UID_MISMATCH		(0x0A)
#define NFC_RESULT_UNINITIALIZED	(0x20)
#define NFC_RESULT_INVALID_STATE	(0x21)
#define NFC_RESULT_INVALID_MODE		(0x24)
#define NFC_RESULT_TAG_INFO_TIMEOUT	(0x7A)

// Result macros
#define NFC_RESULT_SUCCESS		(0)
#define NFC_RESULT_BASE_MASK		(0xFFFFFF00)
#define NFC_RESULT_MASK			(0x000000FF)
#define NFC_MAKE_RESULT(base, result)	((base) | (result))

#define NFC_PROTOCOL_T1T	0x1
#define NFC_PROTOCOL_T2T	0x2

#define NFC_TECHNOLOGY_A	0x0
#define NFC_TECHNOLOGY_B	0x1
#define NFC_TECHNOLOGY_F	0x2

namespace nfc
{
	struct NFCUid
	{
		/* +0x00 */ uint8 uid[7];
	};
	static_assert(sizeof(NFCUid) == 0x7);

	struct NFCTagInfo
	{
		/* +0x00 */ uint8 uidSize;
		/* +0x01 */ uint8 uid[10];
		/* +0x0B */ uint8 technology;
		/* +0x0C */ uint8 protocol;
		/* +0x0D */ uint8 reserved[0x20];
	};
	static_assert(sizeof(NFCTagInfo) == 0x2D);

	sint32 NFCInit(uint32 chan);

	sint32 NFCInitEx(uint32 chan, uint32 powerMode);

	sint32 NFCShutdown(uint32 chan);

	bool NFCIsInit(uint32 chan);

	void NFCProc(uint32 chan);

	sint32 NFCGetMode(uint32 chan);

	sint32 NFCSetMode(uint32 chan, sint32 mode);

	void NFCSetTagDetectCallback(uint32 chan, MPTR callback, void* context);

	sint32 NFCGetTagInfo(uint32 chan, uint32 discoveryTimeout, MPTR callback, void* context);

	sint32 NFCSendRawData(uint32 chan, bool startDiscovery, uint32 discoveryTimeout, uint32 commandTimeout, uint32 commandSize, uint32 responseSize, void* commandData, MPTR callback, void* context);

	sint32 NFCAbort(uint32 chan, MPTR callback, void* context);

	sint32 NFCRead(uint32 chan, uint32 discoveryTimeout, NFCUid* uid, NFCUid* uidMask, MPTR callback, void* context);

	sint32 NFCWrite(uint32 chan, uint32 discoveryTimeout, NFCUid* uid, NFCUid* uidMask, uint32 size, void* data, MPTR callback, void* context);

	void Initialize();

	bool TouchTagFromFile(const fs::path& filePath, uint32* nfcError);
}
