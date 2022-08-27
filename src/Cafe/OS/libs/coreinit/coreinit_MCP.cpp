#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/IOSU/legacy/iosu_ioctl.h"
#include "Cafe/IOSU/legacy/iosu_mcp.h"
#include "Cafe/OS/libs/coreinit/coreinit_IOS.h"
#include "Cafe/OS/libs/coreinit/coreinit_MCP.h"


#include "Cafe/OS/libs/nn_common.h"
#include "Cafe/OS/libs/nn_act/nn_act.h"
#include "config/CemuConfig.h"

#include "Cafe/CafeSystem.h"

#define mcpPrepareRequest() \
StackAllocator<iosuMcpCemuRequest_t> _buf_mcpRequest; \
StackAllocator<ioBufferVector_t> _buf_bufferVector; \
iosuMcpCemuRequest_t* mcpRequest = _buf_mcpRequest.GetPointer(); \
ioBufferVector_t* mcpBufferVector = _buf_bufferVector.GetPointer(); \
memset(mcpRequest, 0, sizeof(iosuMcpCemuRequest_t)); \
memset(mcpBufferVector, 0, sizeof(ioBufferVector_t)); \
mcpBufferVector->buffer = (uint8*)mcpRequest;

#define MCP_FAKE_HANDLE		(0x00000010)

typedef struct
{
	uint32be n0;
	uint32be n1;
	uint32be n2;
}mcpSystemVersion_t;

static_assert(sizeof(MCPTitleInfo) == 0x61, "title entry size is invalid");
static_assert(offsetof(MCPTitleInfo, appPath) == 0xC, "title entry appPath has invalid offset");
static_assert(offsetof(MCPTitleInfo, titleVersion) == 0x48, "title entry titleVersion has invalid offset");
static_assert(offsetof(MCPTitleInfo, osVersion) == 0x4A, "title entry osVersion has invalid offset");

MCPHANDLE MCP_Open()
{
	// placeholder
	return MCP_FAKE_HANDLE;
}

void MCP_Close(MCPHANDLE mcpHandle)
{
	// placeholder
}

void coreinitExport_MCP_Open(PPCInterpreter_t* hCPU)
{
	// placeholder
	osLib_returnFromFunction(hCPU, MCP_Open());
}

void coreinitExport_MCP_Close(PPCInterpreter_t* hCPU)
{
	// placeholder
	osLib_returnFromFunction(hCPU, 0);
}

sint32 MCP_GetSysProdSettings(MCPHANDLE mcpHandle, SysProdSettings* sysProdSettings)
{
	memset(sysProdSettings, 0x00, sizeof(SysProdSettings));
	// todo: Other fields are currently unknown

	sysProdSettings->gameRegion = (uint8)CafeSystem::GetForegroundTitleRegion();
	sysProdSettings->platformRegion = (uint8)CafeSystem::GetPlatformRegion();

	// contains Wii U serial parts at +0x1A and +0x22?
	return 0;
}

void coreinitExport_MCP_GetSysProdSettings(PPCInterpreter_t* hCPU)
{
	forceLogDebug_printf("MCP_GetSysProdSettings(0x%08x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4]);
	sint32 result = MCP_GetSysProdSettings(hCPU->gpr[3], (SysProdSettings*)memory_getPointerFromVirtualOffset(hCPU->gpr[4]));
	osLib_returnFromFunction(hCPU, result);
}

void coreinitExport_MCP_TitleListByAppType(PPCInterpreter_t* hCPU)
{
	forceLogDebug_printf("MCP_TitleListByAppType(0x%08x,0x%08x,0x%08x,0x%08x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7]);

	ppcDefineParamU32(mcpHandle, 0);
	ppcDefineParamU32(appType, 1);
	ppcDefineParamU32BEPtr(countOutput, 2);
	ppcDefineParamStructPtr(titleList, MCPTitleInfo, 3);
	ppcDefineParamU32(titleListSize, 4);

	sint32 maxCount = titleListSize / sizeof(MCPTitleInfo);

	mcpPrepareRequest();
	mcpRequest->requestCode = IOSU_MCP_GET_TITLE_LIST_BY_APP_TYPE;
	mcpRequest->titleListRequest.titleCount = maxCount;
	mcpRequest->titleListRequest.titleList = titleList;
	mcpRequest->titleListRequest.titleListBufferSize = sizeof(MCPTitleInfo) * 1;
	mcpRequest->titleListRequest.appType = appType;
	__depr__IOS_Ioctlv(IOS_DEVICE_MCP, IOSU_MCP_REQUEST_CEMU, 1, 1, mcpBufferVector);

	*countOutput = mcpRequest->titleListRequest.titleCount;
			
	osLib_returnFromFunction(hCPU, 0);
}

void coreinitExport_MCP_TitleList(PPCInterpreter_t* hCPU)
{
	forceLogDebug_printf("MCP_TitleList(...) unimplemented");
	ppcDefineParamU32(mcpHandle, 0);
	ppcDefineParamU32BEPtr(countOutput, 1);
	ppcDefineParamStructPtr(titleList, MCPTitleInfo, 2);
	ppcDefineParamU32(titleListBufferSize, 3);

	// todo -> Other parameters

	mcpPrepareRequest();
	mcpRequest->requestCode = IOSU_MCP_GET_TITLE_LIST;
	mcpRequest->titleListRequest.titleCount = *countOutput;
	mcpRequest->titleListRequest.titleList = titleList;
	mcpRequest->titleListRequest.titleListBufferSize = titleListBufferSize;
	__depr__IOS_Ioctlv(IOS_DEVICE_MCP, IOSU_MCP_REQUEST_CEMU, 1, 1, mcpBufferVector);

	*countOutput = mcpRequest->titleListRequest.titleCount;

	osLib_returnFromFunction(hCPU, mcpRequest->returnCode);
}

void coreinitExport_MCP_TitleCount(PPCInterpreter_t* hCPU)
{
	forceLogDebug_printf("MCP_TitleCount(): Untested");
	ppcDefineParamU32(mcpHandle, 0);

	mcpPrepareRequest();
	mcpRequest->requestCode = IOSU_MCP_GET_TITLE_COUNT;
	mcpRequest->titleListRequest.titleCount = 0;
	__depr__IOS_Ioctlv(IOS_DEVICE_MCP, IOSU_MCP_REQUEST_CEMU, 1, 1, mcpBufferVector);

	osLib_returnFromFunction(hCPU, mcpRequest->titleListRequest.titleCount);
}

void coreinitExport_MCP_GetTitleInfo(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(mcpHandle, 0);
	ppcDefineParamU64(titleId, 2);
	ppcDefineParamStructPtr(titleList, MCPTitleInfo, 4);

	forceLogDebug_printf("MCP_GetTitleInfo() called");

	mcpPrepareRequest();
	mcpRequest->requestCode = IOSU_MCP_GET_TITLE_INFO;
	mcpRequest->titleListRequest.titleCount = 1;
	mcpRequest->titleListRequest.titleList = titleList;
	mcpRequest->titleListRequest.titleListBufferSize = sizeof(MCPTitleInfo);
	mcpRequest->titleListRequest.titleId = titleId;
	__depr__IOS_Ioctlv(IOS_DEVICE_MCP, IOSU_MCP_REQUEST_CEMU, 1, 1, mcpBufferVector);

	if (mcpRequest->titleListRequest.titleCount == 0)
	{
		forceLog_printf("MCP_GetTitleInfo() failed to get title info");
	}

	osLib_returnFromFunction(hCPU, mcpRequest->returnCode);

}

void coreinitExport_MCP_GetTitleInfoByTitleAndDevice(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(mcpHandle, 0);
	ppcDefineParamU64(titleId, 2);
	ppcDefineParamStr(device, 4); // e.g. "odd"
	ppcDefineParamStructPtr(titleList, MCPTitleInfo, 5);

	forceLogDebug_printf("MCP_GetTitleInfoByTitleAndDevice() called (todo - device type support)");

	mcpPrepareRequest();
	mcpRequest->requestCode = IOSU_MCP_GET_TITLE_INFO;
	mcpRequest->titleListRequest.titleCount = 1;
	mcpRequest->titleListRequest.titleList = titleList;
	mcpRequest->titleListRequest.titleListBufferSize = sizeof(MCPTitleInfo);
	mcpRequest->titleListRequest.titleId = titleId;
	__depr__IOS_Ioctlv(IOS_DEVICE_MCP, IOSU_MCP_REQUEST_CEMU, 1, 1, mcpBufferVector);

	if (mcpRequest->titleListRequest.titleCount == 0)
	{
		forceLogDebug_printf("MCP_GetTitleInfoByTitleAndDevice() no title found");
		osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_MCP, 0)); // E-Shop/nn_vctl.rpl expects error to be returned when no title is found
		return;
	}

	osLib_returnFromFunction(hCPU, mcpRequest->returnCode);

}

namespace coreinit
{

	void export_MCP_GetSystemVersion(PPCInterpreter_t* hCPU)
	{
		forceLogDebug_printf("MCP_GetSystemVersion(%d,0x%08x)", hCPU->gpr[3], hCPU->gpr[4]);
		ppcDefineParamU32(mcpHandle, 0);
		ppcDefineParamStructPtr(systemVersion, mcpSystemVersion_t, 1);

		systemVersion->n0 = 0x5;
		systemVersion->n1 = 0x5;
		systemVersion->n2 = 0x2;
		// todo: Load this from \sys\title\00050010\10041200\content\version.bin

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_MCP_Get4SecondOffStatus(PPCInterpreter_t* hCPU)
	{
		// r3 = mcpHandle
		forceLogDebug_printf("MCP_Get4SecondOffStatus(...) placeholder");
		// if this returns 1 then Barista will display the warning about cold-shutdown ('Holding the POWER button for at least four seconds...')
		osLib_returnFromFunction(hCPU, 0);
	}

	void export_MCP_TitleListByDevice(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(mcpHandle, 0);
		ppcDefineParamStr(deviceName, 1);
		ppcDefineParamU32BEPtr(titleCount, 2);
		ppcDefineParamStructPtr(titleList, MCPTitleInfo, 3); // type guessed
		// purpose of r7 parameter is unknown

		cemu_assert_unimplemented();

		titleList[0].titleIdHigh = 0x00050000;
		titleList[0].titleIdLow = 0x11223344;
		strcpy(titleList[0].appPath, "titlePathTest");

		*titleCount = 1;

		forceLogDebug_printf("MCP_TitleListByDevice() - placeholder");

		osLib_returnFromFunction(hCPU, 0);
	}

#pragma pack(1)
	typedef struct
	{
		/* +0x000 */ char storageName[0x90]; // the name in the storage path
		/* +0x090 */ char storagePath[0x280 - 1]; // /vol/storage_%s%02x
		/* +0x30F */ uint32be storageSubindexOrMask; // the id in the storage path, but this might also be a MASK of indices (e.g. 1 -> Only device 1, 7 -> Device 1,2,3) men.rpx expects 0xF (or 0x7?) to be set for MLC, SLC and USB for MLC_FullDeviceList
		uint8 ukn313[4];
		uint8 ukn317[4];
	}MCPDevice_t;
#pragma pack()

	static_assert(sizeof(MCPDevice_t) == 0x31B, "MCPDevice_t has invalid size");
	static_assert(offsetof(MCPDevice_t, storagePath) == 0x090, "MCPDevice_t.storagePath has invalid offset");
	static_assert(offsetof(MCPDevice_t, storageSubindexOrMask) == 0x30F, "MCPDevice_t.storageSubindex has invalid offset");
	static_assert(offsetof(MCPDevice_t, ukn313) == 0x313, "MCPDevice_t.ukn313 has invalid offset");
	static_assert(offsetof(MCPDevice_t, ukn317) == 0x317, "MCPDevice_t.ukn317 has invalid offset");

	void MCP_DeviceListEx(uint32 mcpHandle, uint32be* deviceCount, MCPDevice_t* deviceList, uint32 deviceListSize, bool returnFullList)
	{
		sint32 maxDeviceCount = deviceListSize / sizeof(MCPDevice_t);

		if (maxDeviceCount < 3*3)
			assert_dbg();

		// if this doesnt return both MLC and SLC friendlist (frd.rpx) will softlock during boot

		memset(deviceList, 0, sizeof(MCPDevice_t) * 1);
		sint32 index = 0;
		for (sint32 f = 0; f < 1; f++)
		{
			// 0
			strcpy(deviceList[index].storageName, "mlc");
			deviceList[index].storageSubindexOrMask = 0xF; // bitmask?
			sprintf(deviceList[index].storagePath, "/vol/storage_%s%02x", deviceList[index].storageName, (sint32)deviceList[index].storageSubindexOrMask);
			index++;
			// 1
			strcpy(deviceList[index].storageName, "slc");
			deviceList[index].storageSubindexOrMask = 0xF; // bitmask?
			sprintf(deviceList[index].storagePath, "/vol/storage_%s%02x", deviceList[index].storageName, (sint32)deviceList[index].storageSubindexOrMask);
			index++;
			// 2
			strcpy(deviceList[index].storageName, "usb");
			deviceList[index].storageSubindexOrMask = 0xF;
			sprintf(deviceList[index].storagePath, "/vol/storage_%s%02x", deviceList[index].storageName, (sint32)deviceList[index].storageSubindexOrMask);
			index++;
		}


		*deviceCount = index;
	}


	void export_MCP_DeviceList(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(mcpHandle, 0);
		ppcDefineParamU32BEPtr(deviceCount, 1);
		ppcDefineParamStructPtr(deviceList, MCPDevice_t, 2);
		ppcDefineParamU32(deviceListSize, 3);

		forceLogDebug_printf("MCP_DeviceList() - placeholder LR %08x", hCPU->spr.LR);

		sint32 maxDeviceCount = deviceListSize / sizeof(MCPDevice_t);

		if (maxDeviceCount < 2)
			assert_dbg();

		// if this doesnt return both MLC and SLC friendlist (frd.rpx) will softlock during boot

		memset(deviceList, 0, sizeof(MCPDevice_t) * 1);
		// 0
		strcpy(deviceList[0].storageName, "mlc");
		deviceList[0].storageSubindexOrMask = (0x01); // bitmask?
		sprintf(deviceList[0].storagePath, "/vol/storage_%s%02x", deviceList[0].storageName, (sint32)deviceList[0].storageSubindexOrMask);
		// 1
		strcpy(deviceList[1].storageName, "slc");
		deviceList[1].storageSubindexOrMask = (0x01); // bitmask?
		sprintf(deviceList[1].storagePath, "/vol/storage_%s%02x", deviceList[1].storageName, (sint32)deviceList[1].storageSubindexOrMask);

		// 2
		//strcpy(deviceList[2].storageName, "usb");
		//deviceList[2].storageSubindex = 1;
		//sprintf(deviceList[2].storagePath, "/vol/storage_%s%02x", deviceList[2].storageName, (sint32)deviceList[2].storageSubindex);


		*deviceCount = 2;

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_MCP_FullDeviceList(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(mcpHandle, 0);
		ppcDefineParamU32BEPtr(deviceCount, 1);
		ppcDefineParamStructPtr(deviceList, MCPDevice_t, 2);
		ppcDefineParamU32(deviceListSize, 3);

		forceLogDebug_printf("MCP_FullDeviceList() - placeholder LR %08x", hCPU->spr.LR);

		MCP_DeviceListEx(mcpHandle, deviceCount, deviceList, deviceListSize, true);

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_MCP_UpdateCheckContext(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(mcpHandle, 0);
		ppcDefineParamU32BEPtr(unknownParam, 1);

		forceLogDebug_printf("MCP_UpdateCheckContext() - placeholder (might be wrong)");

		*unknownParam = 1;


		osLib_returnFromFunction(hCPU, 0);
	}

	void export_MCP_TitleListUpdateGetNext(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(mcpHandle, 0);
		ppcDefineParamMPTR(callbackMPTR, 1);

		forceLogDebug_printf("MCP_TitleListUpdateGetNext() - placeholder/unimplemented");

		// this callback is to let the app know when the title list changed?

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_MCP_GetOverlayAppInfo(PPCInterpreter_t* hCPU)
	{
		forceLogDebug_printf("MCP_GetOverlayAppInfo(...) placeholder");
		ppcDefineParamU32(mcpHandle, 0);
		ppcDefineParamU32(appType, 1);
		ppcDefineParamU32(uknR5, 2);
		ppcDefineParamStructPtr(titleList, MCPTitleInfo, 3);

		// hacky

		mcpPrepareRequest();
		mcpRequest->requestCode = IOSU_MCP_GET_TITLE_LIST_BY_APP_TYPE;
		mcpRequest->titleListRequest.titleCount = 1;
		mcpRequest->titleListRequest.titleList = titleList;
		mcpRequest->titleListRequest.titleListBufferSize = sizeof(MCPTitleInfo)*1;
		mcpRequest->titleListRequest.appType = appType;
		__depr__IOS_Ioctlv(IOS_DEVICE_MCP, IOSU_MCP_REQUEST_CEMU, 1, 1, mcpBufferVector);

		if (mcpRequest->titleListRequest.titleCount != 1)
			assert_dbg();

		osLib_returnFromFunction(hCPU, 0);
	}

	void InitializeMCP()
	{
		osLib_addFunction("coreinit", "MCP_Open", coreinitExport_MCP_Open);
		osLib_addFunction("coreinit", "MCP_Close", coreinitExport_MCP_Close);
		osLib_addFunction("coreinit", "MCP_GetSysProdSettings", coreinitExport_MCP_GetSysProdSettings);
		osLib_addFunction("coreinit", "MCP_TitleListByAppType", coreinitExport_MCP_TitleListByAppType);
		osLib_addFunction("coreinit", "MCP_TitleList", coreinitExport_MCP_TitleList);
		osLib_addFunction("coreinit", "MCP_TitleCount", coreinitExport_MCP_TitleCount);
		osLib_addFunction("coreinit", "MCP_GetTitleInfo", coreinitExport_MCP_GetTitleInfo);
		osLib_addFunction("coreinit", "MCP_GetTitleInfoByTitleAndDevice", coreinitExport_MCP_GetTitleInfoByTitleAndDevice);

		osLib_addFunction("coreinit", "MCP_TitleListByDevice", export_MCP_TitleListByDevice);
		osLib_addFunction("coreinit", "MCP_GetSystemVersion", export_MCP_GetSystemVersion);
		osLib_addFunction("coreinit", "MCP_Get4SecondOffStatus", export_MCP_Get4SecondOffStatus);

		osLib_addFunction("coreinit", "MCP_DeviceList", export_MCP_DeviceList);
		osLib_addFunction("coreinit", "MCP_FullDeviceList", export_MCP_FullDeviceList);

		osLib_addFunction("coreinit", "MCP_UpdateCheckContext", export_MCP_UpdateCheckContext);
		osLib_addFunction("coreinit", "MCP_TitleListUpdateGetNext", export_MCP_TitleListUpdateGetNext);
		osLib_addFunction("coreinit", "MCP_GetOverlayAppInfo", export_MCP_GetOverlayAppInfo);
	}

}

typedef struct  
{
	char settingName[0x40];
	uint32 ukn1;
	uint32 ukn2;
	uint32 ukn3;
	uint32 ukn4_size; // size guessed
	MPTR resultPtr; // pointer to output value
}UCParamStruct_t;

static_assert(sizeof(UCParamStruct_t) == 0x54); // unsure

#if BOOST_OS_LINUX || BOOST_OS_MACOS
#define _strcmpi strcasecmp
#endif

void coreinitExport_UCReadSysConfig(PPCInterpreter_t* hCPU)
{
	// parameters:
	// r3 = UC handle?
	// r4 = Number of values to read (count of UCParamStruct_t entries)
	// r5 = UCParamStruct_t*
	UCParamStruct_t* ucParamBase = (UCParamStruct_t*)memory_getPointerFromVirtualOffset(hCPU->gpr[5]);
	uint32 ucParamCount = hCPU->gpr[4];
	for(uint32 i=0; i<ucParamCount; i++)
	{
		UCParamStruct_t* ucParam = ucParamBase+i;
		if(_strcmpi(ucParam->settingName, "cafe.cntry_reg") == 0)
		{
			// country code
			// get country from simple address
			uint32be simpleAddress = 0;
			nn::act::GetSimpleAddressIdEx(&simpleAddress, nn::act::ACT_SLOT_CURRENT);

			uint32 countryCode = nn::act::getCountryCodeFromSimpleAddress(simpleAddress);

			if( ucParam->resultPtr != _swapEndianU32(MPTR_NULL) )
				memory_writeU32(_swapEndianU32(ucParam->resultPtr), countryCode);
		}
		else if (_strcmpi(ucParam->settingName, "cafe.language") == 0)
		{
			// language
			// 0 -> Japanese
			// 1 -> English
			// ...
			uint32 languageId = (uint32)GetConfig().console_language.GetValue();
			if (ucParam->resultPtr != _swapEndianU32(MPTR_NULL))
				memory_writeU32(_swapEndianU32(ucParam->resultPtr), languageId);
		}
		else if (_strcmpi(ucParam->settingName, "cafe.initial_launch") == 0)
		{
			// initial launch
			// possible ids:
			// 0xFF		Set by SCIFormatSystem (default?)
			// 0x01		Inited but no user created yet (setting this will make the home menu go into user creation dialog)
			// 0x02		Inited, with user
			// other values are unknown
			memory_writeU8(_swapEndianU32(ucParam->resultPtr), 2);
		}
		else if (_strcmpi(ucParam->settingName, "cafe.eula_version") == 0)
		{
			// todo - investigate values
			memory_writeU32(_swapEndianU32(ucParam->resultPtr), 0);
		}
		else if (_strcmpi(ucParam->settingName, "cafe.eula_agree") == 0)
		{
			// todo - investigate values
			memory_writeU8(_swapEndianU32(ucParam->resultPtr), 0);
		}
		else if (_strcmpi(ucParam->settingName, "cafe.version") == 0)
		{
			// todo - investigate values
			memory_writeU16(_swapEndianU32(ucParam->resultPtr), 0);
		}
		else if (_strcmpi(ucParam->settingName, "cafe.eco") == 0)
		{
			// todo - investigate values
			memory_writeU8(_swapEndianU32(ucParam->resultPtr), 0);
		}
		else if (_strcmpi(ucParam->settingName, "cafe.fast_boot") == 0)
		{
			// todo - investigate values
			memory_writeU8(_swapEndianU32(ucParam->resultPtr), 0);
		}
		else if (_strcmpi(ucParam->settingName, "parent.enable") == 0)
		{
			// parental feature enabled
			if (ucParam->resultPtr != _swapEndianU32(MPTR_NULL))
				memory_writeU32(_swapEndianU32(ucParam->resultPtr), 0);
		}
		else if (_strcmpi(ucParam->settingName, "nn.act.account_repaired") == 0)
		{
			if (ucParam->resultPtr != _swapEndianU32(MPTR_NULL))
				memory_writeU8(_swapEndianU32(ucParam->resultPtr), 0);
		}
		else if (_strcmpi(ucParam->settingName, "p_acct1.net_communication_on_game") == 0)
		{
			// get parental online control for online features
			// note: This option is account-bound, the p_acct1 prefix indicates that the account in slot 1 is used
			// account in slot 1
			if (ucParam->resultPtr != _swapEndianU32(MPTR_NULL))
				memory_writeU8(_swapEndianU32(ucParam->resultPtr), 0); // data type is guessed
		}
		else if (_strcmpi(ucParam->settingName, "p_acct1.int_movie") == 0)
		{
			// get parental online control for movies?
			// used by Pikmin HD movies
			if (ucParam->resultPtr != _swapEndianU32(MPTR_NULL))
				memory_writeU8(_swapEndianU32(ucParam->resultPtr), 0); // 0 -> allowed
		}
		else if (_strcmpi(ucParam->settingName, "p_acct1.network_launcher") == 0)
		{
			// miiverse restrictions
			if (ucParam->resultPtr != _swapEndianU32(MPTR_NULL))
				memory_writeU8(_swapEndianU32(ucParam->resultPtr), 0); // data type is guessed (0 -> no restrictions, 1 -> read only?, 2 -> no access?)
		}
		else if (_strcmpi(ucParam->settingName, "s_acct01.uuid") == 0)
		{
			if (ucParam->resultPtr != _swapEndianU32(MPTR_NULL) && ucParam->ukn4_size >= 37)
			{
				StackAllocator<uint8, 16> _uuid;
				uint8* uuid = _uuid.GetPointer();
				nn::act::GetUuidEx(uuid, 1, 0);
				char tempStr[64];
				sprintf(tempStr, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X", uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
					uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
				strcpy((char*)memory_getPointerFromVirtualOffset(_swapEndianU32(ucParam->resultPtr)), tempStr);
			}
		}
		else if (_strcmpi(ucParam->settingName, "s_acct01.nn.ec.eshop_initialized") == 0)
		{
			// E-Shop welcome screen
			if (ucParam->resultPtr != _swapEndianU32(MPTR_NULL))
				memory_writeU8(_swapEndianU32(ucParam->resultPtr), 1); // data type is guessed
		}
		else if (_strcmpi(ucParam->settingName, "p_acct1.int_browser") == 0)
		{
			if (ucParam->resultPtr != _swapEndianU32(MPTR_NULL))
				memory_writeU8(_swapEndianU32(ucParam->resultPtr), 0);
		}
		else
		{
			forceLogDebug_printf("Unsupported SCI value: %s Size %08x\n", ucParam->settingName, ucParam->ukn4_size);
		}
	}
	osLib_returnFromFunction(hCPU, 0); // 0 -> success
}