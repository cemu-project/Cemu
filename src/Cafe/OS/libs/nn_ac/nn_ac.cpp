#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/nn_common.h"
#include "nn_ac.h"

#if BOOST_OS_WINDOWS
#include <iphlpapi.h>
#endif

// AC lib (manages internet connection)

enum class AC_STATUS : uint32
{
	FAILED = (uint32)-1,
	OK = 0,
};

static_assert(TRUE == 1, "TRUE not 1");

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
	cemuLog_logDebug(LogType::Force, "GetAssignedAddress() called");
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
	cemuLog_logDebug(LogType::Force, "GetAssignedSubnet() called");

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

	cemuLog_logDebug(LogType::Force, "nn_ac.IsSystemConnected() - placeholder");
	*apTypeOut = 0; // ukn
	*isConnectedOut = 1;

	osLib_returnFromFunction(hCPU, 0);
}

void nnAcExport_IsConfigExisting(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_ac.IsConfigExisting() - placeholder");

	ppcDefineParamU32(configId, 0);
	ppcDefineParamTypePtr(isConfigExisting, uint8, 1);
	
	*isConfigExisting = 0;

	osLib_returnFromFunction(hCPU, 0);
}

namespace nn_ac
{
	nnResult Initialize()
	{
		return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	}

	nnResult ConnectAsync()
	{
		return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	}

	nnResult IsApplicationConnected(uint8be* connected)
	{
		if (connected)
			*connected = TRUE;
		return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	}

	uint32 Connect()
	{
		// Terraria expects this (or GetLastErrorCode) to return 0 on success
		// investigate on the actual console
		// maybe all success codes are always 0 and dont have any of the other fields set?
		uint32 nnResultCode = 0;// BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0); // Splatoon freezes if this function fails?
		return nnResultCode;
	}

	nnResult GetConnectStatus(betype<AC_STATUS>* status)
	{
		if (status)
			*status = AC_STATUS::OK;
		return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	}

	nnResult GetStatus(betype<AC_STATUS>* status)
	{
		return GetConnectStatus(status);
	}

	nnResult GetLastErrorCode(uint32be* errorCode)
	{
		if (errorCode)
			*errorCode = 0;
		return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
	}

	nnResult GetConnectResult(uint32be* connectResult)
	{
		const uint32 nnResultCode = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_AC, 0);
		if (connectResult)
			*connectResult = nnResultCode;
		return nnResultCode;
	}

	static_assert(sizeof(betype<AC_STATUS>) == 4);
	static_assert(sizeof(betype<nnResult>) == 4);

	nnResult ACInitialize()
	{
		return Initialize();
	}

	bool ACIsSuccess(betype<nnResult>* r)
	{
		return NN_RESULT_IS_SUCCESS(*r) ? 1 : 0;
	}

	bool ACIsFailure(betype<nnResult>* r)
	{
		return NN_RESULT_IS_FAILURE(*r) ? 1 : 0;
	}

	nnResult ACGetConnectStatus(betype<AC_STATUS>* connectionStatus)
	{
		return GetConnectStatus(connectionStatus);
	}

	nnResult ACGetStatus(betype<AC_STATUS>* connectionStatus)
	{
		return GetStatus(connectionStatus);
	}

	nnResult ACConnectAsync()
	{
		return ConnectAsync();
	}

	nnResult ACIsApplicationConnected(uint32be* connectedU32)
	{
		uint8be connected = 0;
		nnResult r = IsApplicationConnected(&connected);
		*connectedU32 = connected; // convert to uint32
		return r;
	}

	void load()
	{
		cafeExportRegisterFunc(Initialize, "nn_ac", "Initialize__Q2_2nn2acFv", LogType::Placeholder);

		cafeExportRegisterFunc(Connect, "nn_ac", "Connect__Q2_2nn2acFv", LogType::Placeholder);
		cafeExportRegisterFunc(ConnectAsync, "nn_ac", "ConnectAsync__Q2_2nn2acFv", LogType::Placeholder);

		cafeExportRegisterFunc(GetConnectResult, "nn_ac", "GetConnectResult__Q2_2nn2acFPQ2_2nn6Result", LogType::Placeholder);
		cafeExportRegisterFunc(GetLastErrorCode, "nn_ac", "GetLastErrorCode__Q2_2nn2acFPUi", LogType::Placeholder);
		cafeExportRegisterFunc(GetConnectStatus, "nn_ac", "GetConnectStatus__Q2_2nn2acFPQ3_2nn2ac6Status", LogType::Placeholder);
		cafeExportRegisterFunc(GetStatus, "nn_ac", "GetStatus__Q2_2nn2acFPQ3_2nn2ac6Status", LogType::Placeholder);
		cafeExportRegisterFunc(IsApplicationConnected, "nn_ac", "IsApplicationConnected__Q2_2nn2acFPb", LogType::Placeholder);

		// AC also offers C-style wrappers
		cafeExportRegister("nn_ac", ACInitialize, LogType::Placeholder);
		cafeExportRegister("nn_ac", ACIsSuccess, LogType::Placeholder);
		cafeExportRegister("nn_ac", ACIsFailure, LogType::Placeholder);
		cafeExportRegister("nn_ac", ACGetConnectStatus, LogType::Placeholder);
		cafeExportRegister("nn_ac", ACGetStatus, LogType::Placeholder);
		cafeExportRegister("nn_ac", ACConnectAsync, LogType::Placeholder);
		cafeExportRegister("nn_ac", ACIsApplicationConnected, LogType::Placeholder);
	}

}

void nnAc_load()
{
	osLib_addFunction("nn_ac", "GetAssignedAddress__Q2_2nn2acFPUl", nnAcExport_GetAssignedAddress);
	osLib_addFunction("nn_ac", "GetAssignedSubnet__Q2_2nn2acFPUl", nnAcExport_GetAssignedSubnet);

	osLib_addFunction("nn_ac", "IsSystemConnected__Q2_2nn2acFPbPQ3_2nn2ac6ApType", nnAcExport_IsSystemConnected);

	osLib_addFunction("nn_ac", "IsConfigExisting__Q2_2nn2acFQ3_2nn2ac11ConfigIdNumPb", nnAcExport_IsConfigExisting);

	osLib_addFunction("nn_ac", "ACGetAssignedAddress", nnAcExport_ACGetAssignedAddress);

	nn_ac::load();
}
