#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/ntag/ntag.h"
#include "Cafe/OS/libs/nfc/nfc.h"
#include "Cafe/OS/libs/coreinit/coreinit_IPC.h"
#include "Cafe/IOSU/ccr_nfc/iosu_ccr_nfc.h"

namespace ntag
{
	struct NTAGWriteData
	{
		uint16 size;
		uint8 data[0x1C8];
		nfc::NFCUid uid;
		nfc::NFCUid uidMask;
	};
	NTAGWriteData gWriteData[2];

	bool ccrNfcOpened = false;
	IOSDevHandle gCcrNfcHandle;

	NTAGFormatSettings gFormatSettings;

	MPTR gDetectCallbacks[2];
	MPTR gAbortCallbacks[2];
	MPTR gReadCallbacks[2];
	MPTR gWriteCallbacks[2];

	sint32 __NTAGConvertNFCResult(sint32 result)
	{
		if (result == NFC_RESULT_SUCCESS)
		{
			return NTAG_RESULT_SUCCESS;
		}

		switch (result & NFC_RESULT_MASK)
		{
		case NFC_RESULT_UNINITIALIZED:
			return NTAG_RESULT_UNINITIALIZED;
		case NFC_RESULT_INVALID_STATE:
			return NTAG_RESULT_INVALID_STATE;
		case NFC_RESULT_NO_TAG:
			return NTAG_RESULT_NO_TAG;
		case NFC_RESULT_UID_MISMATCH:
			return NTAG_RESULT_UID_MISMATCH;
		}

		// TODO convert more errors
		return NTAG_RESULT_INVALID;
	}

	sint32 NTAGInit(uint32 chan)
	{
		return NTAGInitEx(chan);
	}

	sint32 NTAGInitEx(uint32 chan)
	{
		sint32 result = nfc::NFCInitEx(chan, 1);
		return __NTAGConvertNFCResult(result);
	}

	sint32 NTAGShutdown(uint32 chan)
	{
		sint32 result = nfc::NFCShutdown(chan);

		if (ccrNfcOpened)
		{
			coreinit::IOS_Close(gCcrNfcHandle);
			ccrNfcOpened = false;
		}

		gDetectCallbacks[chan] = MPTR_NULL;
		gAbortCallbacks[chan] = MPTR_NULL;
		gReadCallbacks[chan] = MPTR_NULL;
		gWriteCallbacks[chan] = MPTR_NULL;

		return __NTAGConvertNFCResult(result);
	}

	bool NTAGIsInit(uint32 chan)
	{
		return nfc::NFCIsInit(chan);
	}

	void NTAGProc(uint32 chan)
	{
		nfc::NFCProc(chan);
	}

	void NTAGSetFormatSettings(NTAGFormatSettings* formatSettings)
	{
		gFormatSettings.version = formatSettings->version;
		gFormatSettings.makerCode = _swapEndianU32(formatSettings->makerCode);
		gFormatSettings.identifyCode = _swapEndianU32(formatSettings->identifyCode);
	}

	void __NTAGDetectCallback(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(chan, 0);
		ppcDefineParamU32(hasTag, 1);
		ppcDefineParamPtr(context, void, 2);

		cemuLog_log(LogType::NTAG, "__NTAGDetectCallback: {} {} {}", chan, hasTag, context);

		PPCCoreCallback(gDetectCallbacks[chan], chan, hasTag, context);

		osLib_returnFromFunction(hCPU, 0);
	}

	void NTAGSetTagDetectCallback(uint32 chan, MPTR callback, void* context)
	{
		cemu_assert(chan < 2);

		gDetectCallbacks[chan] = callback;
		nfc::NFCSetTagDetectCallback(chan, RPLLoader_MakePPCCallable(__NTAGDetectCallback), context);
	}

	void __NTAGAbortCallback(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(chan, 0);
		ppcDefineParamS32(error, 1);
		ppcDefineParamPtr(context, void, 2);

		PPCCoreCallback(gAbortCallbacks[chan], chan, __NTAGConvertNFCResult(error), context);

		osLib_returnFromFunction(hCPU, 0);
	}

	sint32 NTAGAbort(uint32 chan, MPTR callback, void* context)
	{
		cemu_assert(chan < 2);
		
		// TODO is it normal that Rumble U calls this?

		gAbortCallbacks[chan] = callback;
		sint32 result = nfc::NFCAbort(chan, RPLLoader_MakePPCCallable(__NTAGAbortCallback), context);
		return __NTAGConvertNFCResult(result);
	}

	bool __NTAGRawDataToNfcData(iosu::ccr_nfc::CCRNFCCryptData* raw, iosu::ccr_nfc::CCRNFCCryptData* nfc)
	{  
		memcpy(nfc, raw, sizeof(iosu::ccr_nfc::CCRNFCCryptData));

		if (raw->version == 0)
		{
			nfc->version = 0;
			nfc->dataSize = 0x1C8;
			nfc->seedOffset = 0x25;
			nfc->keyGenSaltOffset = 0x1A8;
			nfc->uuidOffset = 0x198;
			nfc->unfixedInfosOffset = 0x28;
			nfc->unfixedInfosSize = 0x120;
			nfc->lockedSecretOffset = 0x168;
			nfc->lockedSecretSize = 0x30;
			nfc->unfixedInfosHmacOffset = 0;
			nfc->lockedSecretHmacOffset = 0x148;
		}
		else if (raw->version == 2)
		{
			nfc->version = 2;
			nfc->dataSize = 0x208;
			nfc->seedOffset = 0x29;
			nfc->keyGenSaltOffset = 0x1E8;
			nfc->uuidOffset = 0x1D4;
			nfc->unfixedInfosOffset = 0x2C;
			nfc->unfixedInfosSize = 0x188;
			nfc->lockedSecretOffset = 0x1DC;
			nfc->lockedSecretSize = 0;
			nfc->unfixedInfosHmacOffset = 0x8;
			nfc->lockedSecretHmacOffset = 0x1B4;

			memcpy(nfc->data + 0x1d4, raw->data, 0x8);
			memcpy(nfc->data, raw->data + 0x8, 0x8);
			memcpy(nfc->data + 0x28, raw->data + 0x10, 0x4);
			memcpy(nfc->data + nfc->unfixedInfosOffset, raw->data + 0x14, 0x20);
			memcpy(nfc->data + nfc->lockedSecretHmacOffset, raw->data + 0x34, 0x20);
			memcpy(nfc->data + nfc->lockedSecretOffset, raw->data + 0x54, 0xC);
			memcpy(nfc->data + nfc->keyGenSaltOffset, raw->data + 0x60, 0x20);
			memcpy(nfc->data + nfc->unfixedInfosHmacOffset, raw->data + 0x80, 0x20);
			memcpy(nfc->data + nfc->unfixedInfosOffset + 0x20, raw->data + 0xa0, 0x168);
			memcpy(nfc->data + 0x208, raw->data + 0x208, 0x14);
		}
		else
		{
			return false;
		}

		return true;
	}

	bool __NTAGNfcDataToRawData(iosu::ccr_nfc::CCRNFCCryptData* nfc, iosu::ccr_nfc::CCRNFCCryptData* raw)
	{
		memcpy(raw, nfc, sizeof(iosu::ccr_nfc::CCRNFCCryptData));

		if (nfc->version == 0)
		{
			raw->version = 0;
			raw->dataSize = 0x1C8;
			raw->seedOffset = 0x25;
			raw->keyGenSaltOffset = 0x1A8;
			raw->uuidOffset = 0x198;
			raw->unfixedInfosOffset = 0x28;
			raw->unfixedInfosSize = 0x120;
			raw->lockedSecretOffset = 0x168;
			raw->lockedSecretSize = 0x30;
			raw->unfixedInfosHmacOffset = 0;
			raw->lockedSecretHmacOffset = 0x148;
		}
		else if (nfc->version == 2)
		{
			raw->version = 2;
			raw->dataSize = 0x208;
			raw->seedOffset = 0x11;
			raw->keyGenSaltOffset = 0x60;
			raw->uuidOffset = 0;
			raw->unfixedInfosOffset = 0x14;
			raw->unfixedInfosSize = 0x188;
			raw->lockedSecretOffset = 0x54;
			raw->lockedSecretSize = 0xC;
			raw->unfixedInfosHmacOffset = 0x80;
			raw->lockedSecretHmacOffset = 0x34;

			memcpy(raw->data + 0x8, nfc->data, 0x8);
			memcpy(raw->data + raw->unfixedInfosHmacOffset, nfc->data + 0x8, 0x20);
			memcpy(raw->data + 0x10, nfc->data + 0x28, 0x4);
			memcpy(raw->data + raw->unfixedInfosOffset, nfc->data + 0x2C, 0x20);
			memcpy(raw->data + 0xa0, nfc->data + 0x4C, 0x168);
			memcpy(raw->data + raw->lockedSecretHmacOffset, nfc->data + 0x1B4, 0x20);
			memcpy(raw->data + raw->uuidOffset, nfc->data + 0x1D4, 0x8);
			memcpy(raw->data + raw->lockedSecretOffset, nfc->data + 0x1DC, 0xC);
			memcpy(raw->data + raw->keyGenSaltOffset, nfc->data + 0x1E8, 0x20);
			memcpy(raw->data + 0x208, nfc->data + 0x208, 0x14);
		}
		else
		{
			return false;
		}

		return true;
	}

	sint32 __NTAGDecryptData(void* decryptedData, const void* rawData)
	{
		StackAllocator<iosu::ccr_nfc::CCRNFCCryptData> nfcRawData, nfcInData, nfcOutData;

		if (!ccrNfcOpened)
		{
			gCcrNfcHandle = coreinit::IOS_Open("/dev/ccr_nfc", 0);
		}

		// Prepare nfc buffer
		nfcRawData->version = 0;
		memcpy(nfcRawData->data, rawData, 0x1C8);
		__NTAGRawDataToNfcData(nfcRawData.GetPointer(), nfcInData.GetPointer());

		// Decrypt
		sint32 result = coreinit::IOS_Ioctl(gCcrNfcHandle, 2, nfcInData.GetPointer(), sizeof(iosu::ccr_nfc::CCRNFCCryptData), nfcOutData.GetPointer(), sizeof(iosu::ccr_nfc::CCRNFCCryptData));

		// Unpack nfc buffer
		__NTAGNfcDataToRawData(nfcOutData.GetPointer(), nfcRawData.GetPointer());
		memcpy(decryptedData, nfcRawData->data, 0x1C8);

		// Convert result
		if (result == CCR_NFC_INVALID_UNFIXED_INFOS)
		{
			return -0x2708;
		}
		else if (result == CCR_NFC_INVALID_LOCKED_SECRET)
		{
			return -0x2707;
		}

		return result;
	}

	sint32 __NTAGValidateHeaders(NTAGNoftHeader* noftHeader, NTAGInfoHeader* infoHeader, NTAGAreaHeader* rwHeader, NTAGAreaHeader* roHeader)
	{
		if (infoHeader->formatVersion != gFormatSettings.version || noftHeader->version != 0x1)
		{
			cemuLog_log(LogType::Force, "Invalid format version");
			return -0x2710;
		}

		if (_swapEndianU32(noftHeader->magic) != 0x4E4F4654 /* 'NOFT' */ ||
			_swapEndianU16(rwHeader->magic) != 0x5257 /* 'RW' */ ||
			_swapEndianU16(roHeader->magic) != 0x524F /* 'RO' */)
		{
			cemuLog_log(LogType::Force, "Invalid header magic");
			return -0x270F;
		}

		if (_swapEndianU32(rwHeader->makerCode) != gFormatSettings.makerCode ||
			_swapEndianU32(roHeader->makerCode) != gFormatSettings.makerCode)
		{
			cemuLog_log(LogType::Force, "Invalid maker code");
			return -0x270E;
		}

		if (infoHeader->formatVersion != 0 &&
			(_swapEndianU32(rwHeader->identifyCode) != gFormatSettings.identifyCode ||
			 _swapEndianU32(roHeader->identifyCode) != gFormatSettings.identifyCode))
		{
			cemuLog_log(LogType::Force, "Invalid identify code");
			return -0x2709;
		}

		if (_swapEndianU16(rwHeader->size) + _swapEndianU16(roHeader->size) != 0x130)
		{
			cemuLog_log(LogType::Force, "Invalid data size");
			return -0x270D;
		}

		return 0;
	}

	sint32 __NTAGParseHeaders(const uint8* data, NTAGNoftHeader* noftHeader, NTAGInfoHeader* infoHeader, NTAGAreaHeader* rwHeader, NTAGAreaHeader* roHeader)
	{
		memcpy(noftHeader, data + 0x20, sizeof(NTAGNoftHeader));
		memcpy(infoHeader, data + 0x198, sizeof(NTAGInfoHeader));

		cemu_assert(_swapEndianU16(infoHeader->rwHeaderOffset) + sizeof(NTAGAreaHeader) < 0x200);
		cemu_assert(_swapEndianU16(infoHeader->roHeaderOffset) + sizeof(NTAGAreaHeader) < 0x200);

		memcpy(rwHeader, data + _swapEndianU16(infoHeader->rwHeaderOffset), sizeof(NTAGAreaHeader));
		memcpy(roHeader, data + _swapEndianU16(infoHeader->roHeaderOffset), sizeof(NTAGAreaHeader));

		return __NTAGValidateHeaders(noftHeader, infoHeader, rwHeader, roHeader);
	}

	sint32 __NTAGParseData(void* rawData, void* rwData, void* roData, nfc::NFCUid* uid, uint32 lockedDataSize, NTAGNoftHeader* noftHeader, NTAGInfoHeader* infoHeader, NTAGAreaHeader* rwHeader, NTAGAreaHeader* roHeader)
	{
		uint8 decryptedData[0x1C8];
		sint32 result = __NTAGDecryptData(decryptedData, rawData);
		if (result < 0)
		{
			return result;
		}

		result = __NTAGParseHeaders(decryptedData, noftHeader, infoHeader, rwHeader, roHeader);
		if (result < 0)
		{
			return result;
		}

		if (_swapEndianU16(roHeader->size) + 0x70 != lockedDataSize)
		{
			cemuLog_log(LogType::Force, "Invalid locked area size");
			return -0x270C;
		}

		if (memcmp(infoHeader->uid.uid, uid->uid, sizeof(nfc::NFCUid)) != 0)
		{
			cemuLog_log(LogType::Force, "UID mismatch");
			return -0x270B;
		}

		cemu_assert(_swapEndianU16(rwHeader->offset) + _swapEndianU16(rwHeader->size) < 0x200);
		cemu_assert(_swapEndianU16(roHeader->offset) + _swapEndianU16(roHeader->size) < 0x200);

		memcpy(rwData, decryptedData + _swapEndianU16(rwHeader->offset), _swapEndianU16(rwHeader->size));
		memcpy(roData, decryptedData + _swapEndianU16(roHeader->offset), _swapEndianU16(roHeader->size));

		return 0;
	}

	void __NTAGReadCallback(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(chan, 0);
		ppcDefineParamS32(error, 1);
		ppcDefineParamPtr(uid, nfc::NFCUid, 2);
		ppcDefineParamU32(readOnly, 3);
		ppcDefineParamU32(dataSize, 4);
		ppcDefineParamPtr(data, void, 5);
		ppcDefineParamU32(lockedDataSize, 6);
		ppcDefineParamPtr(lockedData, void, 7);
		ppcDefineParamPtr(context, void, 8);

		uint8 rawData[0x1C8]{};
		StackAllocator<NTAGData> readResult;
		StackAllocator<uint8, 0x1C8> rwData;
		StackAllocator<uint8, 0x1C8> roData;
		NTAGNoftHeader noftHeader;
		NTAGInfoHeader infoHeader;
		NTAGAreaHeader rwHeader;
		NTAGAreaHeader roHeader;

		readResult->readOnly = readOnly;

		error = __NTAGConvertNFCResult(error);
		if (error == 0)
		{
			memset(rwData.GetPointer(), 0, 0x1C8);
			memset(roData.GetPointer(), 0, 0x1C8);

			// Copy raw and locked data into a contigous buffer
			memcpy(rawData, data, dataSize);
			memcpy(rawData + dataSize, lockedData, lockedDataSize);

			error = __NTAGParseData(rawData, rwData.GetPointer(), roData.GetPointer(), uid, lockedDataSize, &noftHeader, &infoHeader, &rwHeader, &roHeader);
			if (error == 0)
			{
				memcpy(readResult->uid.uid, uid->uid, sizeof(uid->uid));
				readResult->rwInfo.data = _swapEndianU32(rwData.GetMPTR());
				readResult->roInfo.data = _swapEndianU32(roData.GetMPTR());
				readResult->rwInfo.makerCode = rwHeader.makerCode;
				readResult->rwInfo.size = rwHeader.size;
				readResult->roInfo.makerCode = roHeader.makerCode;
				readResult->rwInfo.identifyCode = rwHeader.identifyCode;
				readResult->roInfo.identifyCode = roHeader.identifyCode;
				readResult->formatVersion = infoHeader.formatVersion;
				readResult->roInfo.size = roHeader.size;

				cemuLog_log(LogType::NTAG, "__NTAGReadCallback: {} {} {}", chan, error, context);

				PPCCoreCallback(gReadCallbacks[chan], chan, 0, readResult.GetPointer(), context);
				osLib_returnFromFunction(hCPU, 0);
				return;
			}
		}

		if (uid)
		{
			memcpy(readResult->uid.uid, uid->uid, sizeof(uid->uid));
		}
		readResult->roInfo.size = 0;
		readResult->rwInfo.size = 0;
		readResult->roInfo.data = MPTR_NULL;
		readResult->formatVersion = 0;
		readResult->rwInfo.data = MPTR_NULL;
		cemuLog_log(LogType::NTAG, "__NTAGReadCallback: {} {} {}", chan, error, context);
		PPCCoreCallback(gReadCallbacks[chan], chan, error, readResult.GetPointer(), context);
		osLib_returnFromFunction(hCPU, 0);
	}

	sint32 NTAGRead(uint32 chan, uint32 timeout, nfc::NFCUid* uid, nfc::NFCUid* uidMask, MPTR callback, void* context)
	{
		cemu_assert(chan < 2);

		gReadCallbacks[chan] = callback;

		nfc::NFCUid _uid{}, _uidMask{};
		if (uid && uidMask)
		{
			memcpy(&_uid, uid, sizeof(*uid));
			memcpy(&_uidMask, uidMask, sizeof(*uidMask));
		}

		sint32 result = nfc::NFCRead(chan, timeout, &_uid, &_uidMask, RPLLoader_MakePPCCallable(__NTAGReadCallback), context);
		return __NTAGConvertNFCResult(result);
	}

	sint32 __NTAGEncryptData(void* encryptedData, const void* rawData)
	{
		StackAllocator<iosu::ccr_nfc::CCRNFCCryptData> nfcRawData, nfcInData, nfcOutData;

		if (!ccrNfcOpened)
		{
			gCcrNfcHandle = coreinit::IOS_Open("/dev/ccr_nfc", 0);
		}

		// Prepare nfc buffer
		nfcRawData->version = 0;
		memcpy(nfcRawData->data, rawData, 0x1C8);
		__NTAGRawDataToNfcData(nfcRawData.GetPointer(), nfcInData.GetPointer());

		// Encrypt
		sint32 result = coreinit::IOS_Ioctl(gCcrNfcHandle, 1, nfcInData.GetPointer(), sizeof(iosu::ccr_nfc::CCRNFCCryptData), nfcOutData.GetPointer(), sizeof(iosu::ccr_nfc::CCRNFCCryptData));

		// Unpack nfc buffer
		__NTAGNfcDataToRawData(nfcOutData.GetPointer(), nfcRawData.GetPointer());
		memcpy(encryptedData, nfcRawData->data, 0x1C8);

		return result;
	}

	sint32 __NTAGPrepareWriteData(void* outBuffer, uint32 dataSize, const void* data, const void* tagData, NTAGNoftHeader* noftHeader, NTAGAreaHeader* rwHeader)
	{
		uint8 decryptedBuffer[0x1C8];
		uint8 encryptedBuffer[0x1C8];

		memcpy(decryptedBuffer, tagData, 0x1C8);

		// Fill the rest of the rw area with random data
		if (dataSize < _swapEndianU16(rwHeader->size))
		{
			uint8 randomBuffer[0x1C8];
			for (int i = 0; i < sizeof(randomBuffer); i++)
			{
				randomBuffer[i] = rand() & 0xFF;
			}

			memcpy(decryptedBuffer + _swapEndianU16(rwHeader->offset) + dataSize, randomBuffer, _swapEndianU16(rwHeader->size) - dataSize);
		}
		
		// Make sure the data fits into the rw area
		if (_swapEndianU16(rwHeader->size) < dataSize)
		{
			return -0x270D;
		}

		// Update write count (check for overflow)
		if ((_swapEndianU16(noftHeader->writeCount) & 0x7fff) == 0x7fff)
		{
			noftHeader->writeCount = _swapEndianU16(_swapEndianU16(noftHeader->writeCount) & 0x8000);
		}
		else
		{
			noftHeader->writeCount = _swapEndianU16(_swapEndianU16(noftHeader->writeCount) + 1);
		}

		memcpy(decryptedBuffer + 0x20, noftHeader, sizeof(NTAGNoftHeader));
		memcpy(decryptedBuffer + _swapEndianU16(rwHeader->offset), data, dataSize);

		// Encrypt
		sint32 result = __NTAGEncryptData(encryptedBuffer, decryptedBuffer);
		if (result < 0)
		{
			return result;
		}

		memcpy(outBuffer, encryptedBuffer, _swapEndianU16(rwHeader->size) + 0x28);
		return 0;
	}

	void __NTAGWriteCallback(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(chan, 0);
		ppcDefineParamS32(error, 1);
		ppcDefineParamPtr(context, void, 2);

		PPCCoreCallback(gWriteCallbacks[chan], chan, __NTAGConvertNFCResult(error), context);

		osLib_returnFromFunction(hCPU, 0);
	}

	void __NTAGReadBeforeWriteCallback(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(chan, 0);
		ppcDefineParamS32(error, 1);
		ppcDefineParamPtr(uid, nfc::NFCUid, 2);
		ppcDefineParamU32(readOnly, 3);
		ppcDefineParamU32(dataSize, 4);
		ppcDefineParamPtr(data, void, 5);
		ppcDefineParamU32(lockedDataSize, 6);
		ppcDefineParamPtr(lockedData, void, 7);
		ppcDefineParamPtr(context, void, 8);

		uint8 rawData[0x1C8]{};
		uint8 rwData[0x1C8]{};
		uint8 roData[0x1C8]{};
		NTAGNoftHeader noftHeader;
		NTAGInfoHeader infoHeader;
		NTAGAreaHeader rwHeader;
		NTAGAreaHeader roHeader;
		uint8 writeBuffer[0x1C8]{};

		error = __NTAGConvertNFCResult(error);
		if (error == 0)
		{
			// Copy raw and locked data into a contigous buffer
			memcpy(rawData, data, dataSize);
			memcpy(rawData + dataSize, lockedData, lockedDataSize);

			error = __NTAGParseData(rawData, rwData, roData, uid, lockedDataSize, &noftHeader, &infoHeader, &rwHeader, &roHeader);
			if (error < 0)
			{
				cemuLog_log(LogType::Force, "Failed to parse data before write");
				PPCCoreCallback(gWriteCallbacks[chan], chan, -0x3E3, context);
				osLib_returnFromFunction(hCPU, 0);
				return;
			}

			// Prepare data
			memcpy(rawData + _swapEndianU16(infoHeader.rwHeaderOffset), &rwHeader, sizeof(rwHeader));
			memcpy(rawData + _swapEndianU16(infoHeader.roHeaderOffset), &roHeader, sizeof(roHeader));
			memcpy(rawData + _swapEndianU16(roHeader.offset), roData, _swapEndianU16(roHeader.size));
			error = __NTAGPrepareWriteData(writeBuffer, gWriteData[chan].size, gWriteData[chan].data, rawData, &noftHeader, &rwHeader);
			if (error < 0)
			{
				cemuLog_log(LogType::Force, "Failed to prepare write data");
				PPCCoreCallback(gWriteCallbacks[chan], chan, -0x3E3, context);
				osLib_returnFromFunction(hCPU, 0);
				return;
			}

			// Write data to tag
			error = nfc::NFCWrite(chan, 200, &gWriteData[chan].uid, &gWriteData[chan].uidMask,
				_swapEndianU16(rwHeader.size) + 0x28, writeBuffer, RPLLoader_MakePPCCallable(__NTAGWriteCallback), context);
			if (error >= 0)
			{
				osLib_returnFromFunction(hCPU, 0);
				return;
			}

			error = __NTAGConvertNFCResult(error);
		}

		PPCCoreCallback(gWriteCallbacks[chan], chan, error, context);
		osLib_returnFromFunction(hCPU, 0);
	}

	sint32 NTAGWrite(uint32 chan, uint32 timeout, nfc::NFCUid* uid, uint32 rwSize, void* rwData, MPTR callback, void* context)
	{
		cemu_assert(chan < 2);
		cemu_assert(rwSize < 0x1C8);

		gWriteCallbacks[chan] = callback;

		if (uid)
		{
			memcpy(&gWriteData[chan].uid, uid, sizeof(nfc::NFCUid));
		}
		memset(&gWriteData[chan].uidMask, 0xff, sizeof(nfc::NFCUid));

		gWriteData[chan].size = rwSize;
		memcpy(gWriteData[chan].data, rwData, rwSize);

		sint32 result = nfc::NFCRead(chan, timeout, &gWriteData[chan].uid, &gWriteData[chan].uidMask, RPLLoader_MakePPCCallable(__NTAGReadBeforeWriteCallback), context);
		return __NTAGConvertNFCResult(result);
	}

	sint32 NTAGFormat(uint32 chan, uint32 timeout, nfc::NFCUid* uid, uint32 rwSize, void* rwData, MPTR callback, void* context)
	{
		cemu_assert(chan < 2);

		// TODO
		cemu_assert_debug(false);

		return NTAG_RESULT_INVALID;
	}

	void Initialize()
	{
		cafeExportRegister("ntag", NTAGInit, LogType::NTAG);
		cafeExportRegister("ntag", NTAGInitEx, LogType::NTAG);
		cafeExportRegister("ntag", NTAGShutdown, LogType::NTAG);
		cafeExportRegister("ntag", NTAGIsInit, LogType::Placeholder); // disabled logging, since this gets spammed
		cafeExportRegister("ntag", NTAGProc, LogType::Placeholder); // disabled logging, since this gets spammed
		cafeExportRegister("ntag", NTAGSetFormatSettings, LogType::NTAG);
		cafeExportRegister("ntag", NTAGSetTagDetectCallback, LogType::NTAG);
		cafeExportRegister("ntag", NTAGAbort, LogType::NTAG);
		cafeExportRegister("ntag", NTAGRead, LogType::NTAG);
		cafeExportRegister("ntag", NTAGWrite, LogType::NTAG);
		cafeExportRegister("ntag", NTAGFormat, LogType::NTAG);
	}
}
