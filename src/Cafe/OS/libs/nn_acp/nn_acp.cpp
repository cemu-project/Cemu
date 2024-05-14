#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/Filesystem/fsc.h"
#include "nn_acp.h"
#include "Cafe/OS/libs/nn_common.h"
#include "Cafe/OS/libs/coreinit/coreinit_IOS.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"

#include <cinttypes>
#include <filesystem>
#include <fstream>

#include "config/ActiveSettings.h"
#include "Cafe/IOSU/legacy/iosu_acp.h"
#include "Cafe/IOSU/legacy/iosu_ioctl.h"

#include "Cafe/OS/libs/sysapp/sysapp.h"
#include "Common/FileStream.h"
#include "Cafe/CafeSystem.h"

using ACPDeviceType = iosu::acp::ACPDeviceType;

#define acpPrepareRequest() \
StackAllocator<iosuAcpCemuRequest_t> _buf_acpRequest; \
StackAllocator<ioBufferVector_t> _buf_bufferVector; \
iosuAcpCemuRequest_t* acpRequest = _buf_acpRequest.GetPointer(); \
ioBufferVector_t* acpBufferVector = _buf_bufferVector.GetPointer(); \
memset(acpRequest, 0, sizeof(iosuAcpCemuRequest_t)); \
memset(acpBufferVector, 0, sizeof(ioBufferVector_t)); \
acpBufferVector->buffer = (uint8*)acpRequest;

namespace nn
{
namespace acp
{
	ACPStatus ACPConvertResultToACPStatus(uint32* nnResult, const char* functionName, uint32 lineNumber)
	{
		// todo
		return ACPStatus::SUCCESS;
	}

	#define _ACPConvertResultToACPStatus(nnResult) ACPConvertResultToACPStatus(nnResult, __func__, __LINE__)

	ACPStatus ACPGetApplicationBox(uint32be* applicationBox, uint64 titleId)
	{
		// todo
		*applicationBox = 3; // storage mlc
		return ACPStatus::SUCCESS;
	}

	ACPStatus ACPGetOlvAccesskey(uint32be* accessKey)
	{
		nnResult r = iosu::acp::ACPGetOlvAccesskey(accessKey);
		return _ACPConvertResultToACPStatus(&r);
	}

    bool sSaveDirMounted{false};

	ACPStatus ACPMountSaveDir()
	{
        cemu_assert_debug(!sSaveDirMounted);
		uint64 titleId = CafeSystem::GetForegroundTitleId();
		uint32 high = GetTitleIdHigh(titleId) & (~0xC);
		uint32 low = GetTitleIdLow(titleId);

		// mount save path
		const auto mlc = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/user/", high, low);
		FSCDeviceHostFS_Mount("/vol/save/", _pathToUtf8(mlc), FSC_PRIORITY_BASE);
		nnResult mountResult = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_ACP, 0);
		return _ACPConvertResultToACPStatus(&mountResult);
	}

    ACPStatus ACPUnmountSaveDir()
    {
        cemu_assert_debug(!sSaveDirMounted);
        fsc_unmount("/vol/save/", FSC_PRIORITY_BASE);
        return ACPStatus::SUCCESS;
    }

	ACPStatus ACPUpdateSaveTimeStamp(uint32 persistentId, uint64 titleId, ACPDeviceType deviceType)
	{
		nnResult r = iosu::acp::ACPUpdateSaveTimeStamp(persistentId, titleId, deviceType);
		return ACPStatus::SUCCESS;
	}

	ACPStatus ACPCheckApplicationDeviceEmulation(uint32be* isEmulated)
	{
		*isEmulated = 0;
		return ACPStatus::SUCCESS;
	}

	ACPStatus ACPCreateSaveDir(uint32 persistentId, ACPDeviceType type)
	{
		nnResult result = iosu::acp::ACPCreateSaveDir(persistentId, type);
		return _ACPConvertResultToACPStatus(&result);
	}

	nnResult ACPCreateSaveDirEx(uint8 accountSlot, uint64 titleId)
	{
		acpPrepareRequest();
		acpRequest->requestCode = IOSU_ACP_CREATE_SAVE_DIR_EX;
		acpRequest->accountSlot = accountSlot;
		acpRequest->titleId = titleId;

		__depr__IOS_Ioctlv(IOS_DEVICE_ACP_MAIN, IOSU_ACP_REQUEST_CEMU, 1, 1, acpBufferVector);
		
		nnResult result = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_ACP, 0);
		return result;
	}

	void nnACPExport_ACPCreateSaveDirEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU8(accountSlot, 0);
		ppcDefineParamU64(titleId, 2); // index 2 because of alignment -> guessed parameters
		nnResult result = ACPCreateSaveDirEx(accountSlot, titleId);
		osLib_returnFromFunction(hCPU, _ACPConvertResultToACPStatus(&result));
	}

	void export_ACPGetSaveDataTitleIdList(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(uknType, 0);
		ppcDefineParamStructPtr(titleIdList, acpTitleId_t, 1);
		ppcDefineParamU32(maxCount, 2);
		ppcDefineParamU32BEPtr(count, 3);

		if (uknType != 3)
			assert_dbg();

		acpPrepareRequest();
		acpRequest->requestCode = IOSU_ACP_GET_SAVE_DATA_TITLE_ID_LIST;
		acpRequest->ptr = titleIdList;
		acpRequest->maxCount = maxCount;
		acpRequest->type = uknType;

		__depr__IOS_Ioctlv(IOS_DEVICE_ACP_MAIN, IOSU_ACP_REQUEST_CEMU, 1, 1, acpBufferVector);

		*count = acpRequest->resultU32.u32;

		osLib_returnFromFunction(hCPU, acpRequest->returnCode);
	}

	void export_ACPGetTitleSaveMetaXml(PPCInterpreter_t* hCPU)
	{
		// r3/r4 = titleId
		// r5 = pointer
		// r6 = 3 (probably some sort of type? Same as in ACPGetSaveDataTitleIdList?)
		ppcDefineParamU64(titleId, 0);
		ppcDefineParamStructPtr(acpMetaXml, acpMetaXml_t, 2);
		ppcDefineParamU32(uknR6, 3);

		if (uknR6 != 3)
			assert_dbg();

		acpPrepareRequest();
		acpRequest->requestCode = IOSU_ACP_GET_TITLE_SAVE_META_XML;
		acpRequest->ptr = acpMetaXml;
		acpRequest->titleId = titleId;
		acpRequest->type = uknR6;

		__depr__IOS_Ioctlv(IOS_DEVICE_ACP_MAIN, IOSU_ACP_REQUEST_CEMU, 1, 1, acpBufferVector);

		osLib_returnFromFunction(hCPU, acpRequest->returnCode);
	}

	static_assert(sizeof(acpSaveDirInfo_t) == 0x80, "acpSaveDirInfo_t has invalid size");

	void export_ACPGetTitleSaveDirEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU64(titleId, 0);
		ppcDefineParamU32(uknR5, 2); // storage device id?
		ppcDefineParamU32(uknR6, 3); // ukn
		ppcDefineParamStructPtr(saveDirInfoOut, acpSaveDirInfo_t, 4);
		ppcDefineParamU32(uknR8, 5); // max count?
		ppcDefineParamU32BEPtr(countOut, 6);

		if (uknR5 != 3)
			assert_dbg();
		if (uknR6 != 0)
			assert_dbg();

		acpPrepareRequest();
		acpRequest->requestCode = IOSU_ACP_GET_TITLE_SAVE_DIR;
		acpRequest->ptr = saveDirInfoOut;
		acpRequest->titleId = titleId;
		acpRequest->type = uknR5;
		acpRequest->maxCount = uknR8;

		__depr__IOS_Ioctlv(IOS_DEVICE_ACP_MAIN, IOSU_ACP_REQUEST_CEMU, 1, 1, acpBufferVector);
		*countOut = acpRequest->resultU32.u32;

		osLib_returnFromFunction(hCPU, acpRequest->returnCode);
	}

	void export_ACPCheckTitleNotReferAccountLaunch(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU64(titleId, 0);
		cemuLog_logDebug(LogType::Force, "ACPCheckTitleNotReferAccountLaunch(): Placeholder");

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_ACPGetLaunchMetaData(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamStructPtr(acpMetaData, acpMetaData_t, 0);
		
		cemuLog_logDebug(LogType::Force, "ACPGetLaunchMetaData(): Placeholder");

		acpPrepareRequest();
		acpRequest->requestCode = IOSU_ACP_GET_TITLE_META_DATA;
		acpRequest->ptr = acpMetaData;
		acpRequest->titleId = CafeSystem::GetForegroundTitleId();

		__depr__IOS_Ioctlv(IOS_DEVICE_ACP_MAIN, IOSU_ACP_REQUEST_CEMU, 1, 1, acpBufferVector);

		osLib_returnFromFunction(hCPU, acpRequest->returnCode);
	}

	void export_ACPGetLaunchMetaXml(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamStructPtr(acpMetaXml, acpMetaXml_t, 0);

		cemuLog_logDebug(LogType::Force, "ACPGetLaunchMetaXml(): Placeholder");

		acpPrepareRequest();
		acpRequest->requestCode = IOSU_ACP_GET_TITLE_META_XML;
		acpRequest->ptr = acpMetaXml;
		acpRequest->titleId = CafeSystem::GetForegroundTitleId();

		__depr__IOS_Ioctlv(IOS_DEVICE_ACP_MAIN, IOSU_ACP_REQUEST_CEMU, 1, 1, acpBufferVector);

		osLib_returnFromFunction(hCPU, acpRequest->returnCode);
	}

	void export_ACPGetTitleIdOfMainApplication(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamTypePtr(titleId, uint64be, 0);

		uint64 currentTitleId = CafeSystem::GetForegroundTitleId();
		*titleId = currentTitleId;

		// for applets we return the menu titleId
		if (((currentTitleId >> 32) & 0xFF) == 0x30)
		{
			// get menu titleId
			uint64 menuTitleId = _SYSGetSystemApplicationTitleId(0);
			*titleId = menuTitleId;
		}

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_ACPGetTitleMetaDirByDevice(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU64(titleId, 0);
		ppcDefineParamStr(path, 2);
		ppcDefineParamU32(pathSize, 3);
		ppcDefineParamU32(deviceId, 4);

		if (deviceId != 3)
			assert_dbg();

		if (((titleId >> 32) & 0xFF) == 0x10 || ((titleId >> 32) & 0xFF) == 0x30)
		{
			sprintf(path, "/vol/storage_mlc01/sys/title/%08x/%08x/meta", (uint32)(titleId>>32), (uint32)(titleId&0xFFFFFFFF));
		}
		else
		{
			sprintf(path, "/vol/storage_mlc01/usr/title/%08x/%08x/meta", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
		}

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_ACPGetTitleMetaXmlByDevice(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU64(titleId, 0);
		ppcDefineParamStructPtr(acpMetaXml, acpMetaXml_t, 2);
		ppcDefineParamU32(deviceId, 3);

		if (deviceId != 3)
			cemuLog_logDebug(LogType::Force, "ACPGetTitleMetaXmlByDevice(): Unsupported deviceId");

		acpPrepareRequest();
		acpRequest->requestCode = IOSU_ACP_GET_TITLE_META_XML;
		acpRequest->ptr = acpMetaXml;
		acpRequest->titleId = titleId;//CafeSystem::GetForegroundTitleId();

		__depr__IOS_Ioctlv(IOS_DEVICE_ACP_MAIN, IOSU_ACP_REQUEST_CEMU, 1, 1, acpBufferVector);

		osLib_returnFromFunction(hCPU, acpRequest->returnCode);
	}

	uint32 ACPGetTitleMetaXml(uint64 titleId, acpMetaXml_t* acpMetaXml)
	{
		acpPrepareRequest();
		acpRequest->requestCode = IOSU_ACP_GET_TITLE_META_XML;
		acpRequest->ptr = acpMetaXml;
		acpRequest->titleId = titleId;

		__depr__IOS_Ioctlv(IOS_DEVICE_ACP_MAIN, IOSU_ACP_REQUEST_CEMU, 1, 1, acpBufferVector);

		return acpRequest->returnCode;
	}

	void export_ACPIsOverAgeEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(age, 0);
		ppcDefineParamU8(slot, 1);

		bool isOverAge = true;
		osLib_returnFromFunction(hCPU, isOverAge ? 1 : 0);
	}

	void export_ACPGetNetworkTime(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32BEPtr(timestamp64, 0);
		ppcDefineParamU32BEPtr(ukn, 1); // probably timezone or offset? Could also be a bool for success/failed

		uint64 t = coreinit::coreinit_getOSTime() + (uint64)((sint64)(ppcCyclesSince2000_UTC - ppcCyclesSince2000) / 20LL);

		timestamp64[0] = (uint32)(t >> 32);
		timestamp64[1] = (uint32)(t & 0xFFFFFFFF);

		*ukn = 1; // E-Shop doesnt want this to be zero (games also check for it)

		osLib_returnFromFunction(hCPU, 0); // error code
	}

	void export_ACPConvertNetworkTimeToOSCalendarTime(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU64(networkTime, 0);
		ppcDefineParamStructPtr(calendarTime, coreinit::OSCalendarTime_t, 2);

		coreinit::OSTicksToCalendarTime(networkTime, calendarTime);

		osLib_returnFromFunction(hCPU, 0);
	}

	void load()
	{
		cafeExportRegister("nn_acp", ACPCheckApplicationDeviceEmulation, LogType::Placeholder);

		osLib_addFunction("nn_acp", "ACPCreateSaveDirEx", nnACPExport_ACPCreateSaveDirEx);
		cafeExportRegister("nn_acp", ACPUpdateSaveTimeStamp, LogType::Placeholder);

		osLib_addFunction("nn_acp", "ACPGetSaveDataTitleIdList", export_ACPGetSaveDataTitleIdList);
		osLib_addFunction("nn_acp", "ACPGetTitleSaveMetaXml", export_ACPGetTitleSaveMetaXml);
		osLib_addFunction("nn_acp", "ACPGetTitleSaveDirEx", export_ACPGetTitleSaveDirEx);

		osLib_addFunction("nn_acp", "ACPCheckTitleNotReferAccountLaunch", export_ACPCheckTitleNotReferAccountLaunch);
		osLib_addFunction("nn_acp", "ACPGetLaunchMetaData", export_ACPGetLaunchMetaData);
		osLib_addFunction("nn_acp", "ACPGetLaunchMetaXml", export_ACPGetLaunchMetaXml);
		osLib_addFunction("nn_acp", "ACPGetTitleIdOfMainApplication", export_ACPGetTitleIdOfMainApplication);

		osLib_addFunction("nn_acp", "ACPGetTitleMetaDirByDevice", export_ACPGetTitleMetaDirByDevice);
		osLib_addFunction("nn_acp", "ACPGetTitleMetaXmlByDevice", export_ACPGetTitleMetaXmlByDevice);
		cafeExportRegister("nn_acp", ACPGetTitleMetaXml, LogType::Placeholder);

		cafeExportRegister("nn_acp", ACPGetApplicationBox, LogType::Placeholder);

		cafeExportRegister("nn_acp", ACPGetOlvAccesskey, LogType::Placeholder);

		osLib_addFunction("nn_acp", "ACPIsOverAgeEx", export_ACPIsOverAgeEx);

		osLib_addFunction("nn_acp", "ACPGetNetworkTime", export_ACPGetNetworkTime);
		osLib_addFunction("nn_acp", "ACPConvertNetworkTimeToOSCalendarTime", export_ACPConvertNetworkTimeToOSCalendarTime);
	}
}
}
