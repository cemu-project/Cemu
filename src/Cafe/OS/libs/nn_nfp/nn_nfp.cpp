#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "Cafe/OS/libs/nn_common.h"
#include "nn_nfp.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Common/FileStream.h"
#include "Cafe/CafeSystem.h"

std::recursive_mutex g_nfpMutex;

void nnNfpLock()
{
	g_nfpMutex.lock();
}

bool nnNfpTryLock()
{
	return g_nfpMutex.try_lock();
}

void nnNfpUnlock()
{
	g_nfpMutex.unlock();
}

struct AmiiboInternal
{
	/* +0x000 */ uint16 lockBytes;
	/* +0x002 */ uint16 staticLock;
	/* +0x004 */ uint32 cc;
	/* +0x008 */ uint8 dataHMAC[32];
	/* +0x028 */ uint8 ukn_A5; // always 0xA5
	/* +0x029 */ uint8 writeCounterHigh;
	/* +0x029 */ uint8 writeCounterLow;
	/* +0x02B */ uint8 unk02B;
	/* encrypted region starts here */
	struct
	{
		/* +0x02C */ uint8 flags;
		/* +0x02D */ uint8 countryCode;
		/* +0x02E */ uint16be crcWriteCounter;
		/* +0x030 */ uint16be date1;
		/* +0x032 */ uint16be date2;
		/* +0x034 */ uint32be crc;
		/* +0x038 */ uint16be nickname[10];
		/* +0x04C */ uint8 mii[0x60];
		/* +0x0AC */ uint32be appDataTitleIdHigh;
		/* +0x0B0 */ uint32be appDataTitleIdLow;
		/* +0x0B4 */ uint16be appWriteCounter;
		/* +0x0B6 */ uint16be appDataIdHigh;
		/* +0x0B8 */ uint16be appDataIdLow;
		/* +0x0BA */ uint16be ukn0BA;
		/* +0x0BC */ uint32be ukn0BC;
		/* +0x0C0 */ uint32be ukn0C0;
		/* +0x0C4 */ uint32be ukn0C4;
		/* +0x0C8 */ uint32be ukn0C8;
		/* +0x0CC */ uint32be ukn0CC;
		/* +0x0D0 */ uint32be ukn0D0;
		/* +0x0D4 */ uint32be ukn0D4;
		/* +0x0D8 */ uint32be ukn0D8;
		uint32 getAppDataAppId()
		{
			uint32 appId = (uint32)appDataIdHigh << 16;
			appId |= (uint32)appDataIdLow;
			return appId;
		}

		void setAppDataAppId(uint32 appId)
		{
			appDataIdHigh = (uint16)(appId >> 16);
			appDataIdLow = (uint16)(appId & 0xFFFF);
		}
	}amiiboSettings;
	/* +0x0DC */ uint8 applicationData[0xD8];
	/* encrypted region ends here */
	/* +0x1B4 */ uint8 tagHMAC[32];
	/* +0x1D4 */ uint8 ntagSerial[7];
	/* +0x1DB */ uint8 nintendoId;
	struct
	{
		/* +0x1DC */ uint8 gameAndCharacterId[2];
		/* +0x1DE */ uint8 characterVariation;
		/* +0x1DF */ uint8 amiiboFigureType;
		/* +0x1E0 */ uint8 amiiboModelNumber[2];
		/* +0x1E2 */ uint8 amiiboSeries;
		/* +0x1E3 */ uint8 ukn_02; // always 0x02 ?
		/* +0x1E4 */ uint8 ukn5C[4];
	}amiiboIdentificationBlock;
	/* +0x1E8 */ uint8 keygenSalt[32];
};

static_assert(sizeof(AmiiboInternal) == 0x208);
static_assert(offsetof(AmiiboInternal, dataHMAC) == 0x8);
static_assert(offsetof(AmiiboInternal, amiiboSettings.appDataIdHigh) == 0xB6);
static_assert(offsetof(AmiiboInternal, amiiboSettings.ukn0D8) == 0xD8);
static_assert(offsetof(AmiiboInternal, tagHMAC) == 0x1B4);


union AmiiboRawNFCData
{
	// each page is 4 bytes
	struct
	{
		uint8 page0[4];
		uint8 page1[4];
		uint8 page2[4];
		uint8 page3[4];
		uint8 page4[4];
		uint8 page5[4];
		uint8 page6[4];
		uint8 page7[4];
		uint8 page8[4];
		uint8 page9[4];
		uint8 page10[4];
		uint8 page11[4];
		uint8 page12[4];
		uint8 page13[4];
		uint8 page14[4];
		uint8 page15[4];
		uint8 page16[4];
	};
	struct
	{
		uint8 rawByte[16 * 4];
	};
	struct
	{
		/* +0x000 */ uint8 ntagSerial[7];
		/* +0x007 */ uint8 nintendoId;
		/* +0x008 */ uint8 lockBytes[2];
		/* +0x00A */ uint8 staticLock[2];
		/* +0x00C */ uint8 cc[4]; // compatibility container
		/* +0x010 */ uint8 ukn_A5; // always 0xA5
		/* +0x011 */ uint8 writeCounter[2];
		/* +0x013 */ uint8 unk013;
		/* +0x014 */ uint8 encryptedSettings[32];
		/* +0x034 */ uint8 tagHMAC[32]; // data hmac
		/* +0x054 */
		struct
		{
			/* +0x54 */ uint8 gameAndCharacterId[2];
			/* +0x56 */ uint8 characterVariation;
			/* +0x57 */ uint8 amiiboFigureType;
			/* +0x58 */ uint8 amiiboModelNumber[2];
			/* +0x5A */ uint8 amiiboSeries;
			/* +0x5B */ uint8 ukn_02; // always 0x02 ?
			/* +0x5C */ uint8 ukn5C[4];
		}amiiboIdentificationBlock;
		/* +0x060 */ uint8 keygenSalt[32];
		/* +0x080 */ uint8 dataHMAC[32];
		/* +0x0A0 */ uint8 encryptedMii[0x60];
		/* +0x100 */ uint8 encryptedTitleId[8];
		/* +0x108 */ uint8 encryptedApplicationWriteCounter[2];
		/* +0x10A */ uint8 encryptedApplicationAreaId[4];
		/* +0x10E */ uint8 ukn10E[2];
		/* +0x110 */ uint8 unk110[32];
		/* +0x130 */ uint8 encryptedApplicationArea[0xD8];
		/* +0x208 */ uint8 dynamicLock[4];
		/* +0x20C */ uint8 cfg0[4];
		/* +0x210 */ uint8 cfg1[4];
	};
};

static_assert(sizeof(AmiiboRawNFCData) == 532);

struct AmiiboProcessedData
{
	uint8 uidLength;
	uint8 uid[7];
};

struct  
{
	bool nfpIsInitialized;
	MPTR activateEvent;
	MPTR deactivateEvent;
	bool isDetecting;
	bool isMounted;
	bool isReadOnly;
	bool hasOpenApplicationArea; // set to true if application area was opened or created
	// currently active Amiibo
	bool hasActiveAmiibo;
	fs::path amiiboPath;
	bool hasInvalidHMAC;
	uint32 amiiboTouchTime;
	AmiiboRawNFCData amiiboNFCData; // raw data
	AmiiboInternal amiiboInternal; // decrypted internal format
	AmiiboProcessedData amiiboProcessedData;
}nfp_data = { 0 };

bool nnNfp_writeCurrentAmiibo();

#include "AmiiboCrypto.h"

void nnNfpExport_SetActivateEvent(PPCInterpreter_t* hCPU)
{
	// parameters:
	// r3	OSEvent_t*
	ppcDefineParamStructPtr(osEvent, coreinit::OSEvent, 0);
	ppcDefineParamMPTR(osEventMPTR, 0);

	debug_printf("nn_nfp.SetActivateEvent(0x%08x)\n", osEventMPTR);

	coreinit::OSInitEvent(osEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_AUTO);

	nnNfpLock();
	nfp_data.activateEvent = osEventMPTR;
	nnNfpUnlock();

	osLib_returnFromFunction(hCPU, 0); // return value ukn
}

void nnNfpExport_SetDeactivateEvent(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(osEvent, coreinit::OSEvent, 0);
	ppcDefineParamMPTR(osEventMPTR, 0);

	cemuLog_log(LogType::NN_NFP, "SetDeactivateEvent(0x{:08x})", osEventMPTR);

	coreinit::OSInitEvent(osEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_AUTO);

	nnNfpLock();
	nfp_data.deactivateEvent = osEventMPTR;
	nnNfpUnlock();

	osLib_returnFromFunction(hCPU, 0); // return value ukn
}

void nnNfpExport_Initialize(PPCInterpreter_t* hCPU)
{
	debug_printf("Nfp Initialize()\n");
	nfp_data.nfpIsInitialized = true;
	nfp_data.isDetecting = false;
	nfp_data.hasActiveAmiibo = false;
	nfp_data.hasOpenApplicationArea = false;
	nfp_data.activateEvent = MPTR_NULL;
	nfp_data.deactivateEvent = MPTR_NULL;
	osLib_returnFromFunction(hCPU, 0);
}

void nnNfpExport_StartDetection(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "StartDetection()");
	nnNfpLock();
	nfp_data.isDetecting = true;
	nnNfpUnlock();
	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

void nnNfpExport_StopDetection(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "StopDetection()");
	nnNfpLock();
	nfp_data.isDetecting = false;
	nnNfpUnlock();
	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

#define NFP_TAG_MAX_LENGTH	(10)

typedef struct  
{
	/* +0x00 */ uint8 uidLength;
	/* +0x01 */ uint8 uid[0xA];
	/* +0x0B */ uint8 unused0B[0x15];
	/* +0x20 */ uint32 ukn20[4];
	uint32 ukn30[4];
	uint32 ukn40[4];
	uint32 ukn50;
}nfpTagInfo_t;

static_assert(sizeof(nfpTagInfo_t) == 0x54, "nfpTagInfo_t has invalid size");

void nnNfpExport_GetTagInfo(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "GetTagInfo(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamStructPtr(tagInfo, nfpTagInfo_t, 0);

	nnNfpLock();
	if (nfp_data.hasActiveAmiibo == false)
	{
		nnNfpUnlock();
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0)); // todo: Return correct error code
		return;
	}
	memset(tagInfo, 0x00, sizeof(nfpTagInfo_t));

	memcpy(tagInfo->uid, nfp_data.amiiboProcessedData.uid, nfp_data.amiiboProcessedData.uidLength);
	tagInfo->uidLength = (uint8)nfp_data.amiiboProcessedData.uidLength;
	// todo - remaining values

	nnNfpUnlock();
	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

typedef struct  
{
	/* +0x00 */ uint8 uidLength;
	/* +0x01 */ uint8 uid[0xA];
	/* +0x0B */ uint8 ukn0B;
	/* +0x0C */ uint8 ukn0C;
	/* +0x0D */ uint8 ukn0D;
	// more?
}NFCTagInfoCallbackParam_t;

uint32 NFCGetTagInfo(uint32 index, uint32 timeout, MPTR functionPtr, void* userParam)
{
	cemuLog_log(LogType::NN_NFP, "NFCGetTagInfo({},{},0x{:08x},0x{:08x})", index, timeout, functionPtr, userParam ? memory_getVirtualOffsetFromPointer(userParam) : 0);


	cemu_assert(index == 0);

	nnNfpLock();

	StackAllocator<NFCTagInfoCallbackParam_t> _callbackParam;
	NFCTagInfoCallbackParam_t* callbackParam = _callbackParam.GetPointer();

	memset(callbackParam, 0x00, sizeof(NFCTagInfoCallbackParam_t));

	memcpy(callbackParam->uid, nfp_data.amiiboProcessedData.uid, nfp_data.amiiboProcessedData.uidLength);
	callbackParam->uidLength = (uint8)nfp_data.amiiboProcessedData.uidLength;

	PPCCoreCallback(functionPtr, index, 0, _callbackParam.GetPointer(), userParam);

	nnNfpUnlock();


	return 0; // 0 -> success
}

void nnNfpExport_Mount(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "Mount()");
	nnNfpLock();
	if (nfp_data.hasActiveAmiibo == false)
	{
		nnNfpUnlock();
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0)); // todo: Return correct error code
		return;
	}
	nfp_data.isMounted = true;
	nfp_data.isReadOnly = false;
	nfp_data.isDetecting = false;
	nnNfpUnlock();
	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

void nnNfpExport_Unmount(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "Unmount()");
	nfp_data.hasOpenApplicationArea = false;
	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

void nnNfpExport_MountRom(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "MountRom()");
	nnNfpLock();
	if (nfp_data.hasActiveAmiibo == false)
	{
		nnNfpUnlock();
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0)); // todo: Return correct error code
		return;
	}
	nfp_data.isMounted = true;
	nfp_data.isReadOnly = true;
	nfp_data.isDetecting = false;
	nnNfpUnlock();
	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

typedef struct
{
	/* +0x00 */ uint8 characterId[3];
	/* +0x03 */ uint8 amiiboSeries;
	/* +0x04 */ uint16be number;
	/* +0x06 */ uint8 nfpType;
	/* +0x07 */ uint8 unused[0x2F];
}nfpRomInfo_t;

static_assert(offsetof(nfpRomInfo_t, amiiboSeries) == 0x3, "nfpRomInfo.seriesId has invalid offset");
static_assert(offsetof(nfpRomInfo_t, number) == 0x4, "nfpRomInfo.number has invalid offset");
static_assert(offsetof(nfpRomInfo_t, nfpType) == 0x6, "nfpRomInfo.nfpType has invalid offset");
static_assert(sizeof(nfpRomInfo_t) == 0x36, "nfpRomInfo_t has invalid size");

void nnNfpExport_GetNfpRomInfo(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "GetNfpRomInfo(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamStructPtr(romInfo, nfpRomInfo_t, 0);

	nnNfpLock();
	if (nfp_data.hasActiveAmiibo == false)
	{
		nnNfpUnlock();
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0)); // todo: Return correct error code
		return;
	}
	memset(romInfo, 0x00, sizeof(nfpRomInfo_t));

	romInfo->characterId[0] = nfp_data.amiiboNFCData.amiiboIdentificationBlock.gameAndCharacterId[0];
	romInfo->characterId[1] = nfp_data.amiiboNFCData.amiiboIdentificationBlock.gameAndCharacterId[1];
	romInfo->characterId[2] = nfp_data.amiiboNFCData.amiiboIdentificationBlock.characterVariation; // guessed

	romInfo->amiiboSeries = nfp_data.amiiboNFCData.amiiboIdentificationBlock.amiiboSeries; // guessed
	romInfo->number = *(uint16be*)nfp_data.amiiboNFCData.amiiboIdentificationBlock.amiiboModelNumber; // guessed
	romInfo->nfpType = nfp_data.amiiboNFCData.amiiboIdentificationBlock.amiiboFigureType; // guessed

	nnNfpUnlock();
	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

typedef struct  
{
	uint16be year;
	uint8 month;
	uint8 day;
}nfpDate_t;

typedef struct  
{
	/* +0x00 */ nfpDate_t date;
	/* +0x04 */ uint8 writeCount[2];
	/* +0x06 */ uint8 characterId[3];
	/* +0x09 */ uint8 amiiboSeries;
	/* +0x0A */ uint16be number;
	/* +0x0C */ uint8 nfpType;
	/* +0x0D */ uint8 nfpVersion;
	/* +0x0E */ uint16be applicationAreaSize;
	/* +0x10 */ uint8 unused[0x30];
}nfpCommonData_t;

static_assert(sizeof(nfpCommonData_t) == 0x40, "nfpCommonData_t has invalid size");
static_assert(offsetof(nfpCommonData_t, writeCount) == 0x4, "nfpCommonData.writeCount has invalid offset");
static_assert(offsetof(nfpCommonData_t, amiiboSeries) == 0x9, "nfpCommonData.seriesId has invalid offset");
static_assert(offsetof(nfpCommonData_t, nfpType) == 0xC, "nfpCommonData.nfpType has invalid offset");
static_assert(offsetof(nfpCommonData_t, applicationAreaSize) == 0xE, "nfpCommonData.applicationAreaSize has invalid offset");

void nnNfpExport_GetNfpCommonInfo(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "GetNfpCommonInfo(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamStructPtr(commonInfo, nfpCommonData_t, 0);

	nnNfpLock();
	if (nfp_data.hasActiveAmiibo == false)
	{
		nnNfpUnlock();
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0)); // todo: Return correct error code
		return;
	}
	// tag info format is currently unknown, so we just set it to all zeros for now
	if (sizeof(nfpCommonData_t) != 0x40 )
		assert_dbg();
	memset(commonInfo, 0x00, sizeof(nfpCommonData_t));

	cemuLog_logDebug(LogType::Force, "GetNfpCommonInfo(0x{:08x})");

	commonInfo->writeCount[0] = nfp_data.amiiboNFCData.writeCounter[0];
	commonInfo->writeCount[1] = nfp_data.amiiboNFCData.writeCounter[1];

	commonInfo->characterId[0] = nfp_data.amiiboNFCData.amiiboIdentificationBlock.gameAndCharacterId[0];
	commonInfo->characterId[1] = nfp_data.amiiboNFCData.amiiboIdentificationBlock.gameAndCharacterId[1];
	commonInfo->characterId[2] = nfp_data.amiiboNFCData.amiiboIdentificationBlock.characterVariation;

	commonInfo->number = *(uint16be*)nfp_data.amiiboNFCData.amiiboIdentificationBlock.amiiboModelNumber;
	commonInfo->amiiboSeries = nfp_data.amiiboNFCData.amiiboIdentificationBlock.amiiboSeries;
	commonInfo->nfpType = nfp_data.amiiboNFCData.amiiboIdentificationBlock.amiiboFigureType; // guessed

	commonInfo->applicationAreaSize = sizeof(nfp_data.amiiboInternal.applicationData); // not 100% sure if this is always set to the maximum

	nnNfpUnlock();
	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

typedef struct  
{
	/* +0x00 */ uint8 ownerMii[0x60]; 
	/* +0x60 */ uint8 nickname[0x14];
	/* +0x74 */ uint16be ukn74;
	/* +0x76 */ uint8 ukn76;
	/* +0x77 */ uint8 _padding77;
	/* +0x78 */ nfpDate_t registerDate;
	/* +0x7C */ uint8 ukn7C[0x2C];
}nfpRegisterInfo_t;

typedef struct  
{
	/* +0x00 */ uint8 ownerMii[0x60];
	/* +0x60 */ uint8 nickname[0x14];
	// maybe has more fields?
}nfpRegisterInfoSet_t;

void nnNfpExport_GetNfpRegisterInfo(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "GetNfpRegisterInfo(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamStructPtr(registerInfo, nfpRegisterInfo_t, 0);

	if(!registerInfo)
	{
		osLib_returnFromFunction(hCPU, 0xC1B03780);
		return;
	}

	memset(registerInfo, 0, sizeof(nfpRegisterInfo_t));
	if ((nfp_data.amiiboInternal.amiiboSettings.flags & 0x10) != 0)
	{
		memcpy(registerInfo->ownerMii, nfp_data.amiiboInternal.amiiboSettings.mii, sizeof(nfp_data.amiiboInternal.amiiboSettings.mii));
		memcpy(registerInfo->nickname, nfp_data.amiiboInternal.amiiboSettings.nickname, sizeof(nfp_data.amiiboInternal.amiiboSettings.nickname));
	}
	
	// todo - missing fields

	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

void nnNfpExport_InitializeRegisterInfoSet(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "InitializeRegisterInfoSet(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamStructPtr(registerInfoSet, nfpRegisterInfoSet_t, 0);

	memset(registerInfoSet, 0, sizeof(nfpRegisterInfoSet_t));

	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

void nnNfpExport_SetNfpRegisterInfo(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "SetNfpRegisterInfo(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamStructPtr(registerInfoSet, nfpRegisterInfoSet_t, 0);

	memcpy(nfp_data.amiiboInternal.amiiboSettings.mii, registerInfoSet->ownerMii, sizeof(nfp_data.amiiboInternal.amiiboSettings.mii));
	memcpy(nfp_data.amiiboInternal.amiiboSettings.nickname, registerInfoSet->nickname, sizeof(nfp_data.amiiboInternal.amiiboSettings.nickname));
	// todo - set register date and other values
	nfp_data.amiiboInternal.amiiboSettings.flags |= 0x10; // set registered bit

	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

void nnNfpExport_IsExistApplicationArea(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "IsExistApplicationArea()");
	if (!nfp_data.hasActiveAmiibo || !nfp_data.isMounted)
	{
		osLib_returnFromFunction(hCPU, 0);
		return;
	}
	bool appAreaExists = (nfp_data.amiiboInternal.amiiboSettings.flags & 0x20) != 0;
	osLib_returnFromFunction(hCPU, appAreaExists ? 1 : 0);
}

void nnNfpExport_OpenApplicationArea(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "OpenApplicationArea(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamU32(appAreaId, 0);

	// note - this API doesn't fail if the application area has already been opened?

	if (!(nfp_data.amiiboInternal.amiiboSettings.flags & 0x20))
	{
		// no application data set
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, NN_RESULT_NFP_CODE_NOAPPAREA));
		return;
	}

	uint32 nfpAppAreaId = nfp_data.amiiboInternal.amiiboSettings.getAppDataAppId();
	if (nfpAppAreaId != appAreaId)
	{
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, NN_RESULT_NFP_CODE_APPAREAIDMISMATCH));
		return;
	}
	nfp_data.hasOpenApplicationArea = true;

	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

void nnNfpExport_ReadApplicationArea(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "ReadApplicationArea(0x{:08x}, 0x{:x})", hCPU->gpr[3], hCPU->gpr[4]);
	ppcDefineParamPtr(bufferPtr, uint8*, 0);
	ppcDefineParamU32(len, 1);

	if (!nfp_data.hasOpenApplicationArea)
	{
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}
	len = std::min(len, (uint32)sizeof(nfp_data.amiiboInternal.applicationData));
	memcpy(bufferPtr, nfp_data.amiiboInternal.applicationData, len);

	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

void nnNfpExport_WriteApplicationArea(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "WriteApplicationArea(0x{:08x}, 0x{:x}, 0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
	ppcDefineParamPtr(bufferPtr, uint8*, 0);
	ppcDefineParamU32(len, 1);
	
	if (!nfp_data.hasOpenApplicationArea)
	{
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}
	if (nfp_data.isReadOnly)
	{
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}

	len = std::min(len, (uint32)sizeof(nfp_data.amiiboInternal.applicationData));
	memcpy(nfp_data.amiiboInternal.applicationData, bufferPtr, len);
	// remaining data is filled with random bytes
	for (uint32 i = len; i < sizeof(nfp_data.amiiboInternal.applicationData); i++)
		nfp_data.amiiboInternal.applicationData[i] = rand() & 0xFF;

	nfp_data.amiiboInternal.amiiboSettings.appWriteCounter = nfp_data.amiiboInternal.amiiboSettings.appWriteCounter + 1;

	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

typedef struct  
{
	uint32be appAreaId;
	MEMPTR<uint8> initialData;
	uint32be initialLen;
	// more?
}NfpCreateInfo_t;

void nnNfpExport_CreateApplicationArea(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "CreateApplicationArea(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamPtr(createInfo, NfpCreateInfo_t, 0);

	if (nfp_data.hasOpenApplicationArea || (nfp_data.amiiboInternal.amiiboSettings.flags&0x20))
	{
		// cant create app area if it already exists
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}
	if (nfp_data.isReadOnly)
	{
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}

	void* writePtr = createInfo->initialData.GetPtr();
	uint32 writeSize = createInfo->initialLen;
	if (writeSize > sizeof(nfp_data.amiiboInternal.applicationData))
	{
		// requested write size larger than available space
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}

	nfp_data.amiiboInternal.amiiboSettings.setAppDataAppId(createInfo->appAreaId);
	nfp_data.amiiboInternal.amiiboSettings.flags |= 0x20; // set application data exists bit
	nfp_data.amiiboInternal.amiiboSettings.appWriteCounter = nfp_data.amiiboInternal.amiiboSettings.appWriteCounter + 1;

	nfp_data.hasOpenApplicationArea = false;

	// write initial data to application area
	memcpy(nfp_data.amiiboInternal.applicationData, writePtr, writeSize);
	// remaining data is filled with random bytes
	for (uint32 i = writeSize; i < sizeof(nfp_data.amiiboInternal.applicationData); i++)
		nfp_data.amiiboInternal.applicationData[i] = rand() & 0xFF;

	// this API forces a flush (unsure, but without this data written by Smash doesn't stick)
	if (!nnNfp_writeCurrentAmiibo())
	{
		cemuLog_log(LogType::Force, "Failed to write Amiibo file data when trying to remove appArea");
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}

	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

void nnNfpExport_DeleteApplicationArea(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "DeleteApplicationArea()");

	if (nfp_data.isReadOnly)
	{
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}
	if ((nfp_data.amiiboInternal.amiiboSettings.flags & 0x20) == 0)
	{
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}

	nfp_data.amiiboInternal.amiiboSettings.setAppDataAppId(0);
	nfp_data.amiiboInternal.amiiboSettings.flags &= ~0x20;
	nfp_data.amiiboInternal.amiiboSettings.appWriteCounter = nfp_data.amiiboInternal.amiiboSettings.appWriteCounter + 1;

	// this API forces a flush
	if (!nnNfp_writeCurrentAmiibo())
	{
		cemuLog_log(LogType::Force, "Failed to write Amiibo file data when trying to remove appArea");
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}

	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

void nnNfpExport_Flush(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "Flush()");

	// write Amiibo data
	if (nfp_data.isReadOnly) 
	{
		cemuLog_log(LogType::Force, "Cannot write to Amiibo when it is mounted in read-only mode");
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}

	if (!nnNfp_writeCurrentAmiibo())
	{
		cemuLog_log(LogType::Force, "Failed to write Amiibo data");
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NFP, 0));
		return;
	}

	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

typedef struct  
{
	/* +0x000 */ uint8 ukn00[0x10];
	/* +0x010 */ uint32be mode;
	/* +0x014 */ uint8 tagInfo[0x54];
	/* +0x068 */ uint8 isRegistered; // could be uint32be?
	/* +0x069 */ uint8 _padding69[3];
	/* +0x06C */ uint8 registerInfo[0xA8];
	/* +0x114 */ uint8 commonInfo[0x40];
	/* +0x154 */ uint8 ukn154[0x20];
}AmiiboSettingsArgs_t;

static_assert(sizeof(AmiiboSettingsArgs_t) == 0x174);
static_assert(offsetof(AmiiboSettingsArgs_t, mode) == 0x10);
static_assert(offsetof(AmiiboSettingsArgs_t, tagInfo) == 0x14);
static_assert(offsetof(AmiiboSettingsArgs_t, isRegistered) == 0x68);
static_assert(offsetof(AmiiboSettingsArgs_t, registerInfo) == 0x6C);
static_assert(offsetof(AmiiboSettingsArgs_t, commonInfo) == 0x114);

void nnNfpExport_GetAmiiboSettingsArgs(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "GetAmiiboSettingsArgs(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamStructPtr(settingsArg, AmiiboSettingsArgs_t, 0);

	memset(settingsArg, 0, sizeof(AmiiboSettingsArgs_t));

	// modes:
	// 0 -> Register owner and nickname
	// 0x64 -> Launch normally

	settingsArg->mode = 0x64;

	osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NFP, 0));
}

void nnNfp_unloadAmiibo()
{
	nnNfpLock();
	nfp_data.isMounted = false;
	nfp_data.hasActiveAmiibo = false;
	nnNfpUnlock();
}

bool nnNfp_touchNfcTagFromFile(const fs::path& filePath, uint32* nfcError)
{
	AmiiboRawNFCData rawData = { 0 };
	auto nfcData = FileStream::LoadIntoMemory(filePath);
	if (!nfcData)
	{
		*nfcError = NFC_ERROR_NO_ACCESS;
		return false;
	}
	if (nfcData->size() < sizeof(AmiiboRawNFCData))
	{
		*nfcError = NFC_ERROR_INVALID_FILE_FORMAT;
		return false;
	}
	memcpy(&rawData, nfcData->data(), sizeof(AmiiboRawNFCData));

	// verify if the file is a valid ntag215/amiibo file
	if (rawData.dynamicLock[0] != 0x01 || rawData.dynamicLock[1] != 0x00 || rawData.dynamicLock[2] != 0x0F || rawData.dynamicLock[3] != 0xBD)
	{
		// Temporary workaround to fix corrupted files by old cemu versions
		rawData.dynamicLock[0] = 0x01;
		rawData.dynamicLock[1] = 0x00;
		rawData.dynamicLock[2] = 0x0F;
		rawData.dynamicLock[3] = 0xBD;

		// *nfcError = NFC_ERROR_INVALID_FILE_FORMAT;
		// return false;
	}
	if (rawData.cfg0[0] != 0x00 || rawData.cfg0[1] != 0x00 || rawData.cfg0[2] != 0x00 || rawData.cfg0[3] != 0x04)
	{
		*nfcError = NFC_ERROR_INVALID_FILE_FORMAT;
		return false;
	}
	if (rawData.cfg1[0] != 0x5F || rawData.cfg1[1] != 0x00 || rawData.cfg1[2] != 0x00 || rawData.cfg1[3] != 0x00)
	{
		*nfcError = NFC_ERROR_INVALID_FILE_FORMAT;
		return false;
	}
	if (rawData.staticLock[0] != 0x0F || rawData.staticLock[1] != 0xE0)
	{
		*nfcError = NFC_ERROR_INVALID_FILE_FORMAT;
		return false;
	}
	if (rawData.cc[0] != 0xF1 || rawData.cc[1] != 0x10 || rawData.cc[2] != 0xFF || rawData.cc[3] != 0xEE)
	{
		*nfcError = NFC_ERROR_INVALID_FILE_FORMAT;
		return false;
	}

	// process uid
	uint8 serialNumber[7];
	serialNumber[0] = rawData.ntagSerial[0];
	serialNumber[1] = rawData.ntagSerial[1];
	serialNumber[2] = rawData.ntagSerial[2];
	serialNumber[3] = rawData.ntagSerial[4];
	serialNumber[4] = rawData.ntagSerial[5];
	serialNumber[5] = rawData.ntagSerial[6];
	serialNumber[6] = rawData.nintendoId;

	uint8 serialCheckByte0 = rawData.ntagSerial[3];
	uint8 serialCheckByte1 = rawData.lockBytes[0];

	uint8 bcc0 = serialNumber[0] ^ serialNumber[1] ^ serialNumber[2] ^ 0x88;
	uint8 bcc1 = serialNumber[3] ^ serialNumber[4] ^ serialNumber[5] ^ serialNumber[6];

	if (serialCheckByte0 != bcc0 || serialCheckByte1 != bcc1)
	{
		cemuLog_log(LogType::Force, "nn_nfp: Mismatch in serial checksum of scanned NFC tag");
	}
	nfp_data.amiiboProcessedData.uidLength = 7;
	memcpy(nfp_data.amiiboProcessedData.uid, serialNumber, 7);
	// signal activation event
	nnNfp_unloadAmiibo();
	nnNfpLock();
	memcpy(&nfp_data.amiiboNFCData, &rawData, sizeof(AmiiboRawNFCData));
	// decrypt amiibo
	amiiboDecrypt();
	nfp_data.amiiboPath = filePath;
	nfp_data.hasActiveAmiibo = true;
	if (nfp_data.activateEvent)
	{
		MEMPTR<coreinit::OSEvent> osEvent(nfp_data.activateEvent);
		coreinit::OSSignalEvent(osEvent);
	}
	nfp_data.amiiboTouchTime = GetTickCount();
	nnNfpUnlock();
	*nfcError = NFC_ERROR_NO_ACCESS;
	return true;
}

bool nnNfp_writeCurrentAmiibo()
{
	nnNfpLock();
	if (!nfp_data.hasActiveAmiibo)
	{
		nnNfpUnlock();
		return false;
	}

	uint16 writeCounter = nfp_data.amiiboInternal.writeCounterLow + (nfp_data.amiiboInternal.writeCounterHigh << 8);
	writeCounter++;
	nfp_data.amiiboInternal.writeCounterLow = writeCounter & 0xFF;
	nfp_data.amiiboInternal.writeCounterHigh = (writeCounter >> 8) & 0xFF;

	// open file for writing
	FileStream* fs = FileStream::openFile2(nfp_data.amiiboPath, true);
	if (!fs)
	{
		nnNfpUnlock();
		return false;
	}
	// encrypt Amiibo and convert to NFC format
	AmiiboRawNFCData nfcData;
	amiiboEncrypt(&nfcData);
	// write to file
	fs->writeData(&nfcData, sizeof(AmiiboRawNFCData));
	delete fs;

	nnNfpUnlock();
	return true;
}

void nnNfp_update()
{
	// lock-free check if amiibo is touching
	if (nfp_data.hasActiveAmiibo == false)
		return;
	if (!nnNfpTryLock())
		return;
	// make sure amiibo is still touching after acquiring lock
	if (nfp_data.hasActiveAmiibo == false)
		return;
	uint32 amiiboElapsedTouchTime = GetTickCount() - nfp_data.amiiboTouchTime;
	if (amiiboElapsedTouchTime >= 1500)
	{
		nnNfp_unloadAmiibo();
	}
	nnNfpUnlock();
	if (nfp_data.deactivateEvent)
	{
		coreinit::OSEvent* osEvent = (coreinit::OSEvent*)memory_getPointerFromVirtualOffset(nfp_data.deactivateEvent);
		coreinit::OSSignalEvent(osEvent);
	}
}

void nnNfpExport_GetNfpState(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::NN_NFP, "GetNfpState()");

	// workaround for Mario Party 10 eating CPU cycles in an infinite loop (maybe due to incorrect NFP detection handling?)
	uint64 titleId = CafeSystem::GetForegroundTitleId();
	if (titleId == 0x0005000010162d00 || titleId == 0x0005000010162e00)
	{
		coreinit::OSSleepTicks(ESPRESSO_CORE_CLOCK / 100); // pause for 10ms
	}

	uint32 nfpState;
	if (nfp_data.nfpIsInitialized == false)
	{
		nfpState = NFP_STATE_NONE;
	}
	else
	{
		if (nfp_data.isMounted && nfp_data.hasActiveAmiibo)
		{
			if (nfp_data.isReadOnly)
				nfpState = NFP_STATE_RW_MOUNT_ROM;
			else
				nfpState = NFP_STATE_RW_MOUNT;
		}
		else if (nfp_data.isDetecting)
		{
			// todo: is this handled correctly?
			if (nfp_data.hasActiveAmiibo)
			{
				nfpState = NFP_STATE_RW_ACTIVE;
			}
			else
				nfpState = NFP_STATE_RW_SEARCH;
		}
		else
		{
			nfpState = NFP_STATE_INIT;
		}
	}
	// returns state of nfp library
	osLib_returnFromFunction(hCPU, nfpState);
}

namespace nn::nfp
{
	uint32 GetErrorCode(uint32 result)
	{
		uint32 level = (result >> 0x1b) & 3;
		uint32 mask = 0x7f00000;
		if (level != 3) 
		{
			mask = 0x1ff00000;
		}
		if (((result & mask) == 0x1b00000) && ((result & 0x80000000) != 0)) 
		{
			mask = 0x3ff;
			if (level != 3) 
			{
				mask = 0xfffff;
			}
			return ((result & mask) >> 7) + 1680000;
		}
		return 1680000;
	}

	void nnNfp_load()
	{
		osLib_addFunction("nn_nfp", "SetActivateEvent__Q2_2nn3nfpFP7OSEvent", nnNfpExport_SetActivateEvent);
		osLib_addFunction("nn_nfp", "SetDeactivateEvent__Q2_2nn3nfpFP7OSEvent", nnNfpExport_SetDeactivateEvent);
		osLib_addFunction("nn_nfp", "StartDetection__Q2_2nn3nfpFv", nnNfpExport_StartDetection);
		osLib_addFunction("nn_nfp", "StopDetection__Q2_2nn3nfpFv", nnNfpExport_StopDetection);

		osLib_addFunction("nn_nfp", "GetTagInfo__Q2_2nn3nfpFPQ3_2nn3nfp7TagInfo", nnNfpExport_GetTagInfo);
		osLib_addFunction("nn_nfp", "Mount__Q2_2nn3nfpFv", nnNfpExport_Mount);
		osLib_addFunction("nn_nfp", "MountRom__Q2_2nn3nfpFv", nnNfpExport_MountRom);
		osLib_addFunction("nn_nfp", "Unmount__Q2_2nn3nfpFv", nnNfpExport_Unmount);

		osLib_addFunction("nn_nfp", "GetNfpRomInfo__Q2_2nn3nfpFPQ3_2nn3nfp7RomInfo", nnNfpExport_GetNfpRomInfo);
		osLib_addFunction("nn_nfp", "GetNfpCommonInfo__Q2_2nn3nfpFPQ3_2nn3nfp10CommonInfo", nnNfpExport_GetNfpCommonInfo);
		osLib_addFunction("nn_nfp", "GetNfpRegisterInfo__Q2_2nn3nfpFPQ3_2nn3nfp12RegisterInfo", nnNfpExport_GetNfpRegisterInfo);

		osLib_addFunction("nn_nfp", "InitializeRegisterInfoSet__Q2_2nn3nfpFPQ3_2nn3nfp15RegisterInfoSet", nnNfpExport_InitializeRegisterInfoSet);
		osLib_addFunction("nn_nfp", "SetNfpRegisterInfo__Q2_2nn3nfpFRCQ3_2nn3nfp15RegisterInfoSet", nnNfpExport_SetNfpRegisterInfo);

		osLib_addFunction("nn_nfp", "IsExistApplicationArea__Q2_2nn3nfpFv", nnNfpExport_IsExistApplicationArea);
		osLib_addFunction("nn_nfp", "OpenApplicationArea__Q2_2nn3nfpFUi", nnNfpExport_OpenApplicationArea);
		osLib_addFunction("nn_nfp", "CreateApplicationArea__Q2_2nn3nfpFRCQ3_2nn3nfp25ApplicationAreaCreateInfo", nnNfpExport_CreateApplicationArea);
		osLib_addFunction("nn_nfp", "DeleteApplicationArea__Q2_2nn3nfpFv", nnNfpExport_DeleteApplicationArea);
		osLib_addFunction("nn_nfp", "ReadApplicationArea__Q2_2nn3nfpFPvUi", nnNfpExport_ReadApplicationArea);
		osLib_addFunction("nn_nfp", "WriteApplicationArea__Q2_2nn3nfpFPCvUiRCQ3_2nn3nfp5TagId", nnNfpExport_WriteApplicationArea);

		osLib_addFunction("nn_nfp", "Flush__Q2_2nn3nfpFv", nnNfpExport_Flush);

		osLib_addFunction("nn_nfp", "Initialize__Q2_2nn3nfpFv", nnNfpExport_Initialize);
		osLib_addFunction("nn_nfp", "GetNfpState__Q2_2nn3nfpFv", nnNfpExport_GetNfpState);

		osLib_addFunction("nn_nfp", "GetAmiiboSettingsArgs__Q2_2nn3nfpFPQ3_2nn3nfp18AmiiboSettingsArgs", nnNfpExport_GetAmiiboSettingsArgs);
	}

	void load()
	{
		nnNfp_load(); // legacy interface, update these to use cafeExportRegister / cafeExportRegisterFunc

		cafeExportRegisterFunc(nn::nfp::GetErrorCode, "nn_nfp", "GetErrorCode__Q2_2nn3nfpFRCQ2_2nn6Result", LogType::Placeholder);

		// NFC API 
		cafeExportRegister("nn_nfp", NFCGetTagInfo, LogType::Placeholder);
	}

}
