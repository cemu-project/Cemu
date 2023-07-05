#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_IOS.h"
#include "Cafe/IOSU/legacy/iosu_act.h"
#include "Cafe/IOSU/legacy/iosu_ioctl.h"
#include "nn_act.h"
#include "Cafe/OS/libs/nn_common.h"
#include "Cafe/CafeSystem.h"

sint32 numAccounts = 1;

#define actPrepareRequest() \
StackAllocator<iosuActCemuRequest_t> _buf_actRequest; \
StackAllocator<ioBufferVector_t> _buf_bufferVector; \
iosuActCemuRequest_t* actRequest = _buf_actRequest.GetPointer(); \
ioBufferVector_t* actBufferVector = _buf_bufferVector.GetPointer(); \
memset(actRequest, 0, sizeof(iosuActCemuRequest_t)); \
memset(actBufferVector, 0, sizeof(ioBufferVector_t)); \
actBufferVector->buffer = (uint8*)actRequest;

#define actPrepareRequest2() \
StackAllocator<iosuActCemuRequest_t> _buf_actRequest; \
iosuActCemuRequest_t* actRequest = _buf_actRequest.GetPointer(); \
memset(actRequest, 0, sizeof(iosuActCemuRequest_t));

uint32 getNNReturnCode(uint32 iosError, iosuActCemuRequest_t* actRequest)
{
	if ((iosError & 0x80000000) != 0)
		return iosError;
	return actRequest->returnCode;
}

uint32 _doCemuActRequest(iosuActCemuRequest_t* actRequest)
{
	StackAllocator<ioBufferVector_t> _buf_bufferVector;
	ioBufferVector_t* actBufferVector = _buf_bufferVector.GetPointer();
	memset(actBufferVector, 0, sizeof(ioBufferVector_t));
	actBufferVector->buffer = (uint8*)actRequest;

	uint32 ioctlResult = __depr__IOS_Ioctlv(IOS_DEVICE_ACT, IOSU_ACT_REQUEST_CEMU, 1, 1, actBufferVector);
	return getNNReturnCode(ioctlResult, actRequest);
}

namespace nn
{
namespace act
{
		
		uint32 GetPersistentIdEx(uint8 slot)
		{
			actPrepareRequest();
			actRequest->requestCode = IOSU_ARC_PERSISTENTID;
			actRequest->accountSlot = slot;

			__depr__IOS_Ioctlv(IOS_DEVICE_ACT, IOSU_ACT_REQUEST_CEMU, 1, 1, actBufferVector);

			return actRequest->resultU32.u32;
		}

		uint32 GetMiiEx(void* miiData, uint8 slot)
		{
			actPrepareRequest2();
			actRequest->requestCode = IOSU_ARC_MIIDATA;
			actRequest->accountSlot = slot;

			uint32 result = _doCemuActRequest(actRequest);
			memcpy(miiData, actRequest->resultBinary.binBuffer, MII_FFL_STORAGE_SIZE);

			return result;
		}

		uint32 GetUuidEx(uint8* uuid, uint8 slot, sint32 name)
		{
			actPrepareRequest2();
			actRequest->requestCode = IOSU_ARC_UUID;
			actRequest->accountSlot = slot;
			actRequest->uuidName = name;

			uint32 result = _doCemuActRequest(actRequest);

			memcpy(uuid, actRequest->resultBinary.binBuffer, 16);

			return result;
		}

		uint32 GetSimpleAddressIdEx(uint32be* simpleAddressId, uint8 slot)
		{
			actPrepareRequest2();
			actRequest->requestCode = IOSU_ARC_SIMPLEADDRESS;
			actRequest->accountSlot = slot;

			uint32 result = _doCemuActRequest(actRequest);

			*simpleAddressId = actRequest->resultU32.u32;

			return result;
		}

		uint32 GetTransferableIdEx(uint64* transferableId, uint32 unique, uint8 slot)
		{
			actPrepareRequest2();
			actRequest->requestCode = IOSU_ARC_TRANSFERABLEID;
			actRequest->accountSlot = slot;
			actRequest->unique = unique;

			uint32 result = _doCemuActRequest(actRequest);

			*transferableId = _swapEndianU64(actRequest->resultU64.u64);

			return result;
		}

		uint32 AcquireIndependentServiceToken(independentServiceToken_t* token, const char* clientId, uint32 cacheDurationInSeconds)
		{
			memset(token, 0, sizeof(independentServiceToken_t));
			actPrepareRequest();
			actRequest->requestCode = IOSU_ARC_ACQUIREINDEPENDENTTOKEN;
			actRequest->titleId = CafeSystem::GetForegroundTitleId();
			actRequest->titleVersion = CafeSystem::GetForegroundTitleVersion();
			actRequest->expiresIn = cacheDurationInSeconds;
			strcpy(actRequest->clientId, clientId);

			uint32 resultCode = __depr__IOS_Ioctlv(IOS_DEVICE_ACT, IOSU_ACT_REQUEST_CEMU, 1, 1, actBufferVector);

			memcpy(token, actRequest->resultBinary.binBuffer, sizeof(independentServiceToken_t));
			return getNNReturnCode(resultCode, actRequest);
		}

		sint64 GetUtcOffset()
		{
			return ((ppcCyclesSince2000 / ESPRESSO_CORE_CLOCK) - (ppcCyclesSince2000_UTC / ESPRESSO_CORE_CLOCK)) * 1'000'000;
		}

		sint32 GetUtcOffsetEx(sint64be* pOutOffset, uint8 slotNo)
		{

			if (!pOutOffset)
				return 0xc0712c80;

			*pOutOffset = GetUtcOffset();
			return 0;
		}

		sint32 g_initializeCount = 0; // inc in Initialize and dec in Finalize
		uint32 Initialize()
		{
			// usually uses a mutex which is initialized in -> global constructor keyed to'_17_act_shim_util_cpp
			if (g_initializeCount == 0)
			{
				actPrepareRequest();
				actRequest->requestCode = IOSU_ARC_INIT;

				__depr__IOS_Ioctlv(IOS_DEVICE_ACT, IOSU_ACT_REQUEST_CEMU, 1, 1, actBufferVector);
			}

			g_initializeCount++;

			return 0;
		}

		NN_ERROR_CODE GetErrorCode(betype<nnResult>* nnResult)
		{
			NN_ERROR_CODE errCode = NNResultToErrorCode(*nnResult, NN_RESULT_MODULE_NN_ACT);
			return errCode;
		}

	}
}


void nnActExport_CreateConsoleAccount(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "CreateConsoleAccount(...)");
	//numAccounts++;
	osLib_returnFromFunction(hCPU, 0);
}

void nnActExport_GetNumOfAccounts(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetNumOfAccounts()");
	osLib_returnFromFunction(hCPU, numAccounts); // account count
}

void nnActExport_IsSlotOccupied(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.IsSlotOccupied({})", hCPU->gpr[3]);
	ppcDefineParamU8(slot, 0);
	
	osLib_returnFromFunction(hCPU, nn::act::GetPersistentIdEx(slot) != 0 ? 1 : 0);
}

uint32 GetAccountIdEx(char* accountId, uint8 slot)
{
	// returns zero for non-network accounts?
	actPrepareRequest2();
	actRequest->requestCode = IOSU_ARC_ACCOUNT_ID;
	actRequest->accountSlot = slot;

	uint32 result = _doCemuActRequest(actRequest);

	strcpy(accountId, actRequest->resultString.strBuffer);

	return result;
}

uint32 GetPrincipalIdEx(uint32be* principalId, uint8 slot)
{
	actPrepareRequest2();
	actRequest->requestCode = IOSU_ARC_PRINCIPALID;
	actRequest->accountSlot = slot;

	uint32 result = _doCemuActRequest(actRequest);

	*principalId = actRequest->resultU32.u32;

	return result;
}

uint32 GetCountryEx(char* country, uint8 slot)
{
	actPrepareRequest2();
	actRequest->requestCode = IOSU_ARC_COUNTRY;
	actRequest->accountSlot = slot;

	uint32 result = _doCemuActRequest(actRequest);

	strcpy(country, actRequest->resultString.strBuffer);

	return result;
}

uint32 IsNetworkAccount(uint8* isNetworkAccount, uint8 slot)
{
	actPrepareRequest2();
	actRequest->requestCode = IOSU_ARC_ISNETWORKACCOUNT;
	actRequest->accountSlot = slot;

	uint32 result = _doCemuActRequest(actRequest);

	*isNetworkAccount = (actRequest->resultU32.u32 != 0) ? 1 : 0;

	return result;
}

void nnActExport_IsNetworkAccount(PPCInterpreter_t* hCPU)
{
	//cemuLog_logDebug(LogType::Force, "nn_act.IsNetworkAccount()");
	uint8 isNetAcc = 0;
	IsNetworkAccount(&isNetAcc, 0xFE);
	osLib_returnFromFunction(hCPU, isNetAcc);
}

void nnActExport_IsNetworkAccountEx(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU8(slot, 0);
	cemuLog_logDebug(LogType::Force, "nn_act.IsNetworkAccountEx({})", slot);
	uint8 isNetAcc = 0;
	IsNetworkAccount(&isNetAcc, slot);
	osLib_returnFromFunction(hCPU, isNetAcc);
}

void nnActExport_GetSimpleAddressId(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetSimpleAddressId()");

	uint32be simpleAddressId;
	nn::act::GetSimpleAddressIdEx(&simpleAddressId, iosu::act::ACT_SLOT_CURRENT);

	osLib_returnFromFunction(hCPU, (uint32)simpleAddressId);
}

void nnActExport_GetSimpleAddressIdEx(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetSimpleAddressIdEx(0x{:08x}, {})", hCPU->gpr[3], hCPU->gpr[4] & 0xFF);

	ppcDefineParamU32BEPtr(simpleAddressId, 0);
	ppcDefineParamU8(slot, 1);

	nn::act::GetSimpleAddressIdEx(simpleAddressId, slot);

	osLib_returnFromFunction(hCPU, 0); // ResultSuccess
}

void nnActExport_GetPrincipalId(PPCInterpreter_t* hCPU)
{
	// return error for non-nnid accounts?
	cemuLog_logDebug(LogType::Force, "nn_act.GetPrincipalId()");
	uint32be principalId;
	GetPrincipalIdEx(&principalId, iosu::act::ACT_SLOT_CURRENT);
	osLib_returnFromFunction(hCPU, (uint32)principalId);
}

void nnActExport_GetPrincipalIdEx(PPCInterpreter_t* hCPU)
{
	// return error for non-nnid accounts?
	cemuLog_logDebug(LogType::Force, "nn_act.GetPrincipalIdEx(0x{:08x}, {})", hCPU->gpr[3], hCPU->gpr[4]&0xFF);
	ppcDefineParamU32BEPtr(principalId, 0);
	ppcDefineParamU8(slot, 1);
	GetPrincipalIdEx(principalId, slot);
	
	osLib_returnFromFunction(hCPU, 0); // ResultSuccess
}

void nnActExport_GetTransferableIdEx(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(transferableId, uint64, 0);
	ppcDefineParamU32(unique, 1);
	ppcDefineParamU8(slot, 2);

	cemuLog_logDebug(LogType::Force, "nn_act.GetTransferableIdEx(0x{:08x}, 0x{:08x}, {})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5] & 0xFF);
	
	uint32 r = nn::act::GetTransferableIdEx(transferableId, unique, slot);

	osLib_returnFromFunction(hCPU, 0); // ResultSuccess
}

void nnActExport_GetPersistentId(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetPersistentId()");
	uint32 persistentId = nn::act::GetPersistentIdEx(iosu::act::ACT_SLOT_CURRENT);
	osLib_returnFromFunction(hCPU, persistentId);
}

void nnActExport_GetPersistentIdEx(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU8(slot, 0);
	cemuLog_logDebug(LogType::Force, "nn_act.GetPersistentIdEx({})", slot);

	uint32 persistentId = nn::act::GetPersistentIdEx(slot);

	osLib_returnFromFunction(hCPU, persistentId);
}

void nnActExport_GetCountry(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetCountry(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamStr(country, 0);
	uint32 r = GetCountryEx(country, iosu::act::ACT_SLOT_CURRENT);
	osLib_returnFromFunction(hCPU, r);
}

bool g_isParentalControlCheckEnabled = false;
void nnActExport_EnableParentalControlCheck(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.EnableParentalControlCheck({})", hCPU->gpr[3]);
	ppcDefineParamU8(isEnabled, 0);
	g_isParentalControlCheckEnabled = isEnabled != 0;
	osLib_returnFromFunction(hCPU, 0);
}

void nnActExport_IsParentalControlCheckEnabled(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.IsParentalControlCheckEnabled() -> {}", g_isParentalControlCheckEnabled);
	osLib_returnFromFunction(hCPU, g_isParentalControlCheckEnabled);
}

void nnActExport_GetHostServerSettings(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "GetHostServerSettings() - stub");
	ppcDefineParamStr(ukn, 1);
	strcpy(ukn, "");
	osLib_returnFromFunction(hCPU, 0x00000000);
}

void nnActExport_GetMii(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetMii(...)");
	ppcDefineParamUStr(miiData, 0);
	uint32 r = nn::act::GetMiiEx(miiData, iosu::act::ACT_SLOT_CURRENT);
	osLib_returnFromFunction(hCPU, r);
}

void nnActExport_GetMiiEx(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetMiiEx(...)");
	ppcDefineParamUStr(miiData, 0);
	ppcDefineParamU8(slot, 1);
	uint32 r = nn::act::GetMiiEx(miiData, slot);
	osLib_returnFromFunction(hCPU, r);
}

void nnActExport_GetMiiImageEx(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "GetMiiImageEx unimplemented");

	osLib_returnFromFunction(hCPU, 0);
}

void nnActExport_GetMiiName(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "GetMiiName(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamWStrBE(miiName, 0);

	StackAllocator<FFLData_t> miiData;

	uint32 r = nn::act::GetMiiEx(miiData, iosu::act::ACT_SLOT_CURRENT);
	// extract name
	sint32 miiNameLength = 0;
	for (sint32 i = 0; i < MII_FFL_NAME_LENGTH; i++)
	{
		miiName[i] = miiData->miiName[i];
		if (miiData->miiName[i] == (const uint16be)'\0')
			break;
		miiNameLength = i+1;
	}
	miiName[miiNameLength] = '\0';

	osLib_returnFromFunction(hCPU, 0);
}

void nnActExport_GetMiiNameEx(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "GetMiiNameEx(0x{:08x}, {})", hCPU->gpr[3], hCPU->gpr[4] & 0xFF);
	ppcDefineParamWStrBE(miiName, 0);
	ppcDefineParamU8(slot, 1);

	StackAllocator<FFLData_t> miiData;

	uint32 r = nn::act::GetMiiEx(miiData, slot);
	// extract name
	sint32 miiNameLength = 0;
	for (sint32 i = 0; i < MII_FFL_NAME_LENGTH; i++)
	{
		miiName[i] = miiData->miiName[i];
		if (miiData->miiName[i] == (const uint16be)'\0')
			break;
		miiNameLength = i + 1;
	}
	miiName[miiNameLength] = '\0';

	osLib_returnFromFunction(hCPU, 0);
}

typedef struct  
{
	uint32be ukn00;
	uint32be ukn04; // transferable id high?
	uint32be accountTransferableIdLow; //
	uint32be ukn0C;
	uint32be ukn10;
	uint32be ukn14;
	uint32be ukn18;
	uint32be ukn1C;
	uint32be ukn20;
	uint32be ukn24;
	uint32be ukn28;
	uint32be ukn2C;
	uint32be ukn30;
	uint32be ukn34;
	uint32be ukn38;
	uint32be ukn3C;
	uint32be ukn40;
	uint32be ukn44;
	uint32be ukn48;
	uint32be ukn4C;
	uint32be ukn50;
	uint32be ukn54;
	uint32be tlsModuleIndex;
	uint32be ukn5C;
}FFLStoreDataDepr_t;

static_assert(sizeof(FFLStoreDataDepr_t) == 96);

void nnActExport_UpdateMii(PPCInterpreter_t* hCPU)
{
	if (sizeof(FFLStoreDataDepr_t) != MII_FFL_STORAGE_SIZE)
		assert_dbg();

	ppcDefineParamU8(slot, 0);
	ppcDefineParamStructPtr(fflStoreData, FFLStoreDataDepr_t, 1);
	ppcDefineParamStructPtr(uknName, uint16, 2); // mii name
	ppcDefineParamStructPtr(uknNameR6, uint8, 3);
	ppcDefineParamStructPtr(uknNameR7, uint8, 4);
	ppcDefineParamStructPtr(uknNameR8, uint8, 5);
	ppcDefineParamStructPtr(uknNameR9, uint8, 6);
	ppcDefineParamStructPtr(uknNameR10, uint8, 7);
	ppcDefineParamStructPtr(uknNameSP4, uint8, 8);
	
	cemu_assert_unimplemented();

	osLib_returnFromFunction(hCPU, 0);
}

void nnActExport_GetUuid(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetUuid(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamUStr(uuid, 0);
	nn::act::GetUuidEx(uuid, iosu::act::ACT_SLOT_CURRENT);
	osLib_returnFromFunction(hCPU, 0); // 0 -> result ok
}

void nnActExport_GetUuidEx(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetUuidEx(0x{:08x},0x{:02x})", hCPU->gpr[3], hCPU->gpr[3]&0xFF);
	ppcDefineParamUStr(uuid, 0);
	ppcDefineParamU8(slot, 1);
	nn::act::GetUuidEx(uuid, slot);
	osLib_returnFromFunction(hCPU, 0); // 0 -> result ok
}

void nnActExport_GetUuidEx2(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetUuidEx(0x{:08x},0x{:02x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
	ppcDefineParamUStr(uuid, 0);
	ppcDefineParamU8(slot, 1);
	ppcDefineParamS32(name, 2);
	nn::act::GetUuidEx(uuid, iosu::act::ACT_SLOT_CURRENT, name);
	osLib_returnFromFunction(hCPU, 0); // 0 -> result ok
}

void nnActExport_GetAccountId(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetAccountId(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamUStr(accId, 0);
	GetAccountIdEx((char*)accId, iosu::act::ACT_SLOT_CURRENT);
	osLib_returnFromFunction(hCPU, 0);
}

void nnActExport_GetAccountIdEx(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetAccountIdEx(0x{:08x}, 0x{:02x})", hCPU->gpr[3], hCPU->gpr[3] & 0xFF);
	ppcDefineParamUStr(accId, 0);
	ppcDefineParamU8(slot, 1);
	GetAccountIdEx((char*)accId, slot);
	osLib_returnFromFunction(hCPU, 0);
}

void nnActExport_GetParentalControlSlotNoEx(PPCInterpreter_t* hCPU)
{
	// GetParentalControlSlotNoEx(uint8* output, uint8 slot)
	cemuLog_logDebug(LogType::Force, "nn_act.GetParentalControlSlotNoEx(0x{:08x}, 0x{:02x})", hCPU->gpr[3], hCPU->gpr[4]);
	//memory_writeU8(hCPU->gpr[3], 0x01); // returned slot no (slot indices start at 1)
	memory_writeU8(hCPU->gpr[3], 1); // 0 -> No parental control for slot?
	//memory_writeU8(hCPU->gpr[3], 0); // 0 -> No parental control for slot?
	osLib_returnFromFunction(hCPU, 0);
}

void nnActExport_GetDefaultAccount(PPCInterpreter_t* hCPU)
{
	// todo
	cemuLog_logDebug(LogType::Force, "GetDefaultAccount(): Return 1?");
	osLib_returnFromFunction(hCPU, 1);
}

void nnActExport_GetSlotNo(PPCInterpreter_t* hCPU)
{
	// id of active account
	// uint8 GetSlotNo(void);
	cemuLog_logDebug(LogType::Force, "nn_act.GetSlotNo()");
	osLib_returnFromFunction(hCPU, 1); // 1 is the first slot (0 is invalid)
}

void nnActExport_GetSlotNoEx(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.GetSlotNoEx(...)");
	// get slot no by uuid
	ppcDefineParamUStr(uuid, 0);

	// get slot no for a specific uuid
	for (uint8 i = 1; i < 12; i++)
	{
		uint8 accUuid[16]{};
		nn::act::GetUuidEx(accUuid, i);
		if (memcmp(uuid, accUuid, 16) == 0)
		{
			osLib_returnFromFunction(hCPU, i);
			return;
		}
	}

	cemu_assert_debug(false); // suspicious behavior?

	osLib_returnFromFunction(hCPU, 0); // account not found
}

void nnActExport_Initialize(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_act.Initialize()");

	nn::act::Initialize();

	osLib_returnFromFunction(hCPU, 0);
}

void nnActExport_HasNfsAccount(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "Called nn_act.HasNfsAccount");
	osLib_returnFromFunction(hCPU, 1); // Nfs = Nintendo Friend System? (Splatoon tries to call nn_fp.RegisterAccount if we set this to false)
}

typedef struct  
{
	/* +0x000 */ char token[0x201]; // /nex_token/token
	/* +0x201 */ uint8 padding201[3];
	/* +0x204 */ char nexPassword[0x41]; // /nex_token/nex_password
	/* +0x245 */ uint8 padding245[3];
	/* +0x248 */ char host[0x10]; // /nex_token/host
	/* +0x258 */ uint16be port; // /nex_token/port
	/* +0x25A */ uint16be padding25A;
}nexServiceToken_t;

static_assert(sizeof(nexServiceToken_t) == 0x25C, "nexServiceToken_t has invalid size");

void nnActExport_AcquireNexServiceToken(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(token, nexServiceToken_t, 0);
	ppcDefineParamU32(serverId, 1);
	memset(token, 0, sizeof(nexServiceToken_t));
	actPrepareRequest();
	actRequest->requestCode = IOSU_ARC_ACQUIRENEXTOKEN;
	actRequest->titleId = CafeSystem::GetForegroundTitleId();
	actRequest->titleVersion = CafeSystem::GetForegroundTitleVersion();
	actRequest->serverId = serverId;

	uint32 resultCode = __depr__IOS_Ioctlv(IOS_DEVICE_ACT, IOSU_ACT_REQUEST_CEMU, 1, 1, actBufferVector);

	memcpy(token, actRequest->resultBinary.binBuffer, sizeof(nexServiceToken_t));

	cemuLog_logDebug(LogType::Force, "Called nn_act.AcquireNexServiceToken");
	osLib_returnFromFunction(hCPU, getNNReturnCode(resultCode, actRequest));
}

void nnActExport_AcquireIndependentServiceToken(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(token, independentServiceToken_t, 0);
	ppcDefineParamMEMPTR(serviceToken, const char, 1);
	uint32 result = nn::act::AcquireIndependentServiceToken(token.GetPtr(), serviceToken.GetPtr(), 0);
	cemuLog_logDebug(LogType::Force, "nn_act.AcquireIndependentServiceToken(0x{}, {}) -> {:x}", (void*)token.GetPtr(), serviceToken.GetPtr(), result);
	cemuLog_logDebug(LogType::Force, "Token: {}", serviceToken.GetPtr());
	osLib_returnFromFunction(hCPU, result);
}

void nnActExport_AcquireIndependentServiceToken2(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(token, independentServiceToken_t, 0);
	ppcDefineParamMEMPTR(clientId, const char, 1);
	ppcDefineParamU32(cacheDurationInSeconds, 2); 
	uint32 result = nn::act::AcquireIndependentServiceToken(token, clientId.GetPtr(), cacheDurationInSeconds);
	cemuLog_logDebug(LogType::Force, "Called nn_act.AcquireIndependentServiceToken2");
	osLib_returnFromFunction(hCPU, result);
}

void nnActExport_AcquireEcServiceToken(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(pEcServiceToken, independentServiceToken_t, 0);
	uint32 result = nn::act::AcquireIndependentServiceToken(pEcServiceToken.GetPtr(), "71a6f5d6430ea0183e3917787d717c46", 0);
	cemuLog_logDebug(LogType::Force, "Called nn_act.AcquireEcServiceToken");
	osLib_returnFromFunction(hCPU, result);
}

void nnActExport_AcquirePrincipalIdByAccountId(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(principalId, uint32be, 0);
	ppcDefineParamMEMPTR(nnid, char, 1);
	ppcDefineParamU32(ukn, 2); // some option, can be 0 or 1 ?
	cemuLog_logDebug(LogType::Force, "nn_act.AcquirePrincipalIdByAccountId(0x{:08x},\"{}\", {}) - last param unknown", principalId.GetMPTR(), nnid.GetPtr(), ukn);
	actPrepareRequest2();
	actRequest->requestCode = IOSU_ARC_ACQUIREPIDBYNNID;
	strcpy(actRequest->clientId, nnid.GetPtr());

	uint32 result = _doCemuActRequest(actRequest);

	*principalId.GetPtr() = actRequest->resultU32.u32;

	osLib_returnFromFunction(hCPU, result);
}

// register account functions
void nnAct_load()
{
	
	osLib_addFunction("nn_act", "Initialize__Q2_2nn3actFv", nnActExport_Initialize);

	osLib_addFunction("nn_act", "CreateConsoleAccount__Q2_2nn3actFv", nnActExport_CreateConsoleAccount);

	osLib_addFunction("nn_act", "GetNumOfAccounts__Q2_2nn3actFv", nnActExport_GetNumOfAccounts);
	osLib_addFunction("nn_act", "IsSlotOccupied__Q2_2nn3actFUc", nnActExport_IsSlotOccupied);
	osLib_addFunction("nn_act", "GetSlotNo__Q2_2nn3actFv", nnActExport_GetSlotNo);
	osLib_addFunction("nn_act", "GetSlotNoEx__Q2_2nn3actFRC7ACTUuid", nnActExport_GetSlotNoEx);
	osLib_addFunction("nn_act", "IsNetworkAccount__Q2_2nn3actFv", nnActExport_IsNetworkAccount);
	osLib_addFunction("nn_act", "IsNetworkAccountEx__Q2_2nn3actFUc", nnActExport_IsNetworkAccountEx);
	// account id
	osLib_addFunction("nn_act", "GetAccountId__Q2_2nn3actFPc", nnActExport_GetAccountId);
	osLib_addFunction("nn_act", "GetAccountIdEx__Q2_2nn3actFPcUc", nnActExport_GetAccountIdEx);
	// simple address
	osLib_addFunction("nn_act", "GetSimpleAddressId__Q2_2nn3actFv", nnActExport_GetSimpleAddressId);
	osLib_addFunction("nn_act", "GetSimpleAddressIdEx__Q2_2nn3actFPUiUc", nnActExport_GetSimpleAddressIdEx);
	// principal id
	osLib_addFunction("nn_act", "GetPrincipalId__Q2_2nn3actFv", nnActExport_GetPrincipalId);
	osLib_addFunction("nn_act", "GetPrincipalIdEx__Q2_2nn3actFPUiUc", nnActExport_GetPrincipalIdEx);
	// transferable id
	osLib_addFunction("nn_act", "GetTransferableIdEx__Q2_2nn3actFPULUiUc", nnActExport_GetTransferableIdEx);
	// persistent id
	osLib_addFunction("nn_act", "GetPersistentId__Q2_2nn3actFv", nnActExport_GetPersistentId);
	osLib_addFunction("nn_act", "GetPersistentIdEx__Q2_2nn3actFUc", nnActExport_GetPersistentIdEx);
	// country
	osLib_addFunction("nn_act", "GetCountry__Q2_2nn3actFPc", nnActExport_GetCountry);

	// parental
	osLib_addFunction("nn_act", "EnableParentalControlCheck__Q2_2nn3actFb", nnActExport_EnableParentalControlCheck);
	osLib_addFunction("nn_act", "IsParentalControlCheckEnabled__Q2_2nn3actFv", nnActExport_IsParentalControlCheckEnabled);

	osLib_addFunction("nn_act", "GetMii__Q2_2nn3actFP12FFLStoreData", nnActExport_GetMii);
	osLib_addFunction("nn_act", "GetMiiEx__Q2_2nn3actFP12FFLStoreDataUc", nnActExport_GetMiiEx);
	osLib_addFunction("nn_act", "GetMiiImageEx__Q2_2nn3actFPUiPvUi15ACTMiiImageTypeUc", nnActExport_GetMiiImageEx);
	osLib_addFunction("nn_act", "GetMiiName__Q2_2nn3actFPw", nnActExport_GetMiiName);
	osLib_addFunction("nn_act", "GetMiiNameEx__Q2_2nn3actFPwUc", nnActExport_GetMiiNameEx);

	osLib_addFunction("nn_act", "UpdateMii__Q2_2nn3actFUcRC12FFLStoreDataPCwPCvUiT4T5T4T5T4T5T4T5T4T5T4T5T4T5T4T5", nnActExport_UpdateMii);

	osLib_addFunction("nn_act", "GetUuid__Q2_2nn3actFP7ACTUuid", nnActExport_GetUuid);
	osLib_addFunction("nn_act", "GetUuidEx__Q2_2nn3actFP7ACTUuidUc", nnActExport_GetUuidEx);
	osLib_addFunction("nn_act", "GetUuidEx__Q2_2nn3actFP7ACTUuidUcUi", nnActExport_GetUuidEx2);

	osLib_addFunction("nn_act", "GetParentalControlSlotNoEx__Q2_2nn3actFPUcUc", nnActExport_GetParentalControlSlotNoEx);

	osLib_addFunction("nn_act", "GetDefaultAccount__Q2_2nn3actFv", nnActExport_GetDefaultAccount);

	osLib_addFunction("nn_act", "AcquireEcServiceToken__Q2_2nn3actFPc", nnActExport_AcquireEcServiceToken);
	osLib_addFunction("nn_act", "AcquireNexServiceToken__Q2_2nn3actFP26ACTNexAuthenticationResultUi", nnActExport_AcquireNexServiceToken);
	osLib_addFunction("nn_act", "AcquireIndependentServiceToken__Q2_2nn3actFPcPCc", nnActExport_AcquireIndependentServiceToken);
	osLib_addFunction("nn_act", "AcquireIndependentServiceToken__Q2_2nn3actFPcPCcUibT4", nnActExport_AcquireIndependentServiceToken2);
	osLib_addFunction("nn_act", "AcquireIndependentServiceToken__Q2_2nn3actFPcPCcUi", nnActExport_AcquireIndependentServiceToken2);
	
	osLib_addFunction("nn_act", "AcquirePrincipalIdByAccountId__Q2_2nn3actFPUiPA17_CcUi", nnActExport_AcquirePrincipalIdByAccountId);

	cafeExportRegisterFunc(nn::act::GetErrorCode, "nn_act", "GetErrorCode__Q2_2nn3actFRCQ2_2nn6Result", LogType::Placeholder);

	// placeholders / incomplete implementations
	osLib_addFunction("nn_act", "HasNfsAccount__Q2_2nn3actFv", nnActExport_HasNfsAccount);
	osLib_addFunction("nn_act", "GetHostServerSettings__Q2_2nn3actFPcT1Uc", nnActExport_GetHostServerSettings);
	cafeExportRegisterFunc(nn::act::GetUtcOffset, "nn_act", "GetUtcOffset__Q2_2nn3actFv", LogType::Placeholder);
	cafeExportRegisterFunc(nn::act::GetUtcOffsetEx, "nn_act", "GetUtcOffsetEx__Q2_2nn3actFPLUc", LogType::Placeholder);

}




