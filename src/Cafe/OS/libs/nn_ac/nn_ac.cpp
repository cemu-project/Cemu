#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/nn_common.h"
#include "nn_ac.h"

#if BOOST_OS_WINDOWS
#include <iphlpapi.h>
#endif

// AC lib (manages internet connection)

#define AC_STATUS_FAILED (-1)
#define AC_STATUS_OK (0)

void nn_acExport_ConnectAsync(PPCInterpreter_t* hCPU)
{
	forceLogDebug_printf("nn_ac.ConnectAsync();");
	uint32 nnResultCode = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	osLib_returnFromFunction(hCPU, nnResultCode);
}

void nn_acExport_Connect(PPCInterpreter_t* hCPU)
{
	forceLogDebug_printf("nn_ac.Connect();");

	// Terraria expects this (or GetLastErrorCode) to return 0 on success
	// investigate on the actual console
	// maybe all success codes are always 0 and dont have any of the other fields set?

	uint32 nnResultCode = 0;// BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0); // Splatoon freezes if this function fails?
	osLib_returnFromFunction(hCPU, nnResultCode);
}

static_assert(TRUE == 1, "TRUE not 1");

void nn_acExport_IsApplicationConnected(PPCInterpreter_t* hCPU)
{
	//forceLogDebug_printf("nn_ac.IsApplicationConnected(0x%08x)", hCPU->gpr[3]);
	ppcDefineParamMEMPTR(connected, uint8, 0);
	if (connected)
		*connected = TRUE;
	//memory_writeU8(hCPU->gpr[3], 1); // always return true regardless of actual online state

	const uint32 nnResultCode = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	osLib_returnFromFunction(hCPU, nnResultCode);
}

void nn_acExport_GetConnectStatus(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(status, uint32, 0);
	if (status)
		*status = AC_STATUS_OK;

	const uint32 nnResultCode = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	osLib_returnFromFunction(hCPU, nnResultCode);
}

void nn_acExport_GetLastErrorCode(PPCInterpreter_t* hCPU)
{
	//forceLogDebug_printf("nn_ac.GetLastErrorCode();");
	ppcDefineParamMEMPTR(errorCode, uint32, 0);
	if (errorCode)
		*errorCode = 0;
	const uint32 nnResultCode = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	osLib_returnFromFunction(hCPU, nnResultCode);
}

void nn_acExport_GetStatus(PPCInterpreter_t* hCPU)
{
	forceLogDebug_printf("nn_ac.GetStatus();");
	ppcDefineParamMEMPTR(status, uint32, 0);
	if (status)
		*status = AC_STATUS_OK;
	const uint32 nnResultCode = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	osLib_returnFromFunction(hCPU, nnResultCode);
}

void nn_acExport_GetConnectResult(PPCInterpreter_t* hCPU)
{
	// GetConnectStatus__Q2_2nn2acFPQ3_2nn2ac6Status
	forceLogDebug_printf("nn_ac.GetConnectResult(0x%08x) LR %08x", hCPU->spr.LR, hCPU->gpr[3]);
	ppcDefineParamMEMPTR(result, uint32, 0);
	const uint32 nnResultCode = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	if (result)
		*result = nnResultCode;
	osLib_returnFromFunction(hCPU, nnResultCode);
}

void _GetLocalIPAndSubnetMaskFallback(uint32& localIp, uint32& subnetMask)
{
	// default to some hardcoded values
	localIp = (192 << 24) | (168 << 16) | (0 << 8) | (100 << 0);
	subnetMask = (255 << 24) | (255 << 16) | (255 << 8) | (0 << 0);
}

#if BOOST_OS_WINDOWS
void _GetLocalIPAndSubnetMask(uint32& localIp, uint32& subnetMask)
{
	std::vector<IP_ADAPTER_ADDRESSES> buf_adapter_addresses;
	buf_adapter_addresses.resize(32);
	DWORD buf_size;
	DWORD r;

	for (uint32 i = 0; i < 6; i++) 
	{
		buf_size = (uint32)(buf_adapter_addresses.size() * sizeof(IP_ADAPTER_ADDRESSES));
		r = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_GATEWAYS, nullptr, buf_adapter_addresses.data(), &buf_size);
		if (r != ERROR_BUFFER_OVERFLOW)
			break;
		buf_adapter_addresses.resize(buf_adapter_addresses.size() * 2);
	}
	if (r != ERROR_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Failed to acquire local IP and subnet mask");
		_GetLocalIPAndSubnetMaskFallback(localIp, subnetMask);
		return;
	}
	IP_ADAPTER_ADDRESSES* currentAddress = buf_adapter_addresses.data();
	while (currentAddress)
	{
		if (currentAddress->OperStatus != IfOperStatusUp)
		{
			currentAddress = currentAddress->Next;
			continue;
		}
		if (!currentAddress->FirstUnicastAddress || !currentAddress->FirstUnicastAddress->Address.lpSockaddr)
		{
			currentAddress = currentAddress->Next;
			continue;
		}
		if (!currentAddress->FirstGatewayAddress)
		{
			currentAddress = currentAddress->Next;
			continue;
		}

		SOCKADDR* sockAddr = currentAddress->FirstUnicastAddress->Address.lpSockaddr;
		if (sockAddr->sa_family == AF_INET)
		{
			ULONG mask = 0;
			if (ConvertLengthToIpv4Mask(currentAddress->FirstUnicastAddress->OnLinkPrefixLength, &mask) != NO_ERROR)
				mask = 0;
			sockaddr_in* inAddr = (sockaddr_in*)sockAddr;
			localIp = _byteswap_ulong(inAddr->sin_addr.S_un.S_addr);
			subnetMask = _byteswap_ulong(mask);
			return;
		}
		currentAddress = currentAddress->Next;
	}
	cemuLog_logDebug(LogType::Force, "_GetLocalIPAndSubnetMask(): Failed to find network IP and subnet mask");
	_GetLocalIPAndSubnetMaskFallback(localIp, subnetMask);
}
#else
void _GetLocalIPAndSubnetMask(uint32& localIp, uint32& subnetMask)
{
	cemuLog_logDebug(LogType::Force, "_GetLocalIPAndSubnetMask(): Not implemented");
	_GetLocalIPAndSubnetMaskFallback(localIp, subnetMask);
}
#endif

void nnAcExport_GetAssignedAddress(PPCInterpreter_t* hCPU)
{
	forceLogDebug_printf("GetAssignedAddress() called");
	ppcDefineParamU32BEPtr(ipAddrOut, 0);

	uint32 localIp;
	uint32 subnetMask;
	_GetLocalIPAndSubnetMask(localIp, subnetMask);

	*ipAddrOut = localIp;

	const uint32 nnResultCode = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	osLib_returnFromFunction(hCPU, nnResultCode);
}

void nnAcExport_GetAssignedSubnet(PPCInterpreter_t* hCPU)
{
	forceLogDebug_printf("GetAssignedSubnet() called");

	ppcDefineParamU32BEPtr(subnetMaskOut, 0);

	uint32 localIp;
	uint32 subnetMask;
	_GetLocalIPAndSubnetMask(localIp, subnetMask);

	*subnetMaskOut = subnetMask;

	const uint32 nnResultCode = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	osLib_returnFromFunction(hCPU, nnResultCode);
}

void nnAcExport_ACGetAssignedAddress(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32BEPtr(ipAddrOut, 0);

	uint32 localIp;
	uint32 subnetMask;
	_GetLocalIPAndSubnetMask(localIp, subnetMask);
	*ipAddrOut = localIp;

	const uint32 nnResultCode = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	osLib_returnFromFunction(hCPU, nnResultCode);
}

void nnAcExport_IsSystemConnected(PPCInterpreter_t* hCPU)
{
	ppcDefineParamTypePtr(isConnectedOut, uint8, 0);
	ppcDefineParamTypePtr(apTypeOut, uint32be, 1);

	forceLogDebug_printf("nn_ac.IsSystemConnected() - placeholder");
	*apTypeOut = 0; // ukn
	*isConnectedOut = 1;

	osLib_returnFromFunction(hCPU, 0);
}

void nnAcExport_IsConfigExisting(PPCInterpreter_t* hCPU)
{
	forceLogDebug_printf("nn_ac.IsConfigExisting() - placeholder");

	ppcDefineParamU32(configId, 0);
	ppcDefineParamTypePtr(isConfigExisting, uint8, 1);
	
	*isConfigExisting = 0;

	osLib_returnFromFunction(hCPU, 0);
}

namespace nn_ac
{
	bool ACIsSuccess(betype<nnResult>* r)
	{
		return NN_RESULT_IS_SUCCESS(*r) ? 1 : 0;
	}

	nnResult ACGetConnectStatus(uint32be* connectionStatus)
	{

		*connectionStatus = 0; // 0 means connected?

		return NN_RESULT_SUCCESS;
	}

	void load()
	{
		cafeExportRegister("nn_ac", ACIsSuccess, LogType::Placeholder);
		cafeExportRegister("nn_ac", ACGetConnectStatus, LogType::Placeholder);
	}

}

void nnAc_load()
{
	osLib_addFunction("nn_ac", "Connect__Q2_2nn2acFv", nn_acExport_Connect);
	osLib_addFunction("nn_ac", "ConnectAsync__Q2_2nn2acFv", nn_acExport_ConnectAsync);
	osLib_addFunction("nn_ac", "IsApplicationConnected__Q2_2nn2acFPb", nn_acExport_IsApplicationConnected);
	osLib_addFunction("nn_ac", "GetConnectStatus__Q2_2nn2acFPQ3_2nn2ac6Status", nn_acExport_GetConnectStatus);
	osLib_addFunction("nn_ac", "GetConnectResult__Q2_2nn2acFPQ2_2nn6Result", nn_acExport_GetConnectResult);
	osLib_addFunction("nn_ac", "GetLastErrorCode__Q2_2nn2acFPUi", nn_acExport_GetLastErrorCode);
	osLib_addFunction("nn_ac", "GetStatus__Q2_2nn2acFPQ3_2nn2ac6Status", nn_acExport_GetStatus);

	osLib_addFunction("nn_ac", "GetAssignedAddress__Q2_2nn2acFPUl", nnAcExport_GetAssignedAddress);
	osLib_addFunction("nn_ac", "GetAssignedSubnet__Q2_2nn2acFPUl", nnAcExport_GetAssignedSubnet);

	osLib_addFunction("nn_ac", "IsSystemConnected__Q2_2nn2acFPbPQ3_2nn2ac6ApType", nnAcExport_IsSystemConnected);

	osLib_addFunction("nn_ac", "IsConfigExisting__Q2_2nn2acFQ3_2nn2ac11ConfigIdNumPb", nnAcExport_IsConfigExisting);

	osLib_addFunction("nn_ac", "ACGetAssignedAddress", nnAcExport_ACGetAssignedAddress);

	nn_ac::load();
}
