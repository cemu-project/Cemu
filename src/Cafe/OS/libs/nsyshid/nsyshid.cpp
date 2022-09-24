#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include <bitset>
#include "nsyshid.h"

#if BOOST_OS_WINDOWS

#include <setupapi.h>
#include <initguid.h>
#include <hidsdi.h>

#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"

#pragma comment(lib,"Setupapi.lib")
#pragma comment(lib,"hid.lib")

DEFINE_GUID(GUID_DEVINTERFACE_HID, 0x4D1E55B2L, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);

namespace nsyshid
{

	typedef struct
	{
		/* +0x00 */ uint32be handle;
		/* +0x04 */ uint32 ukn04;
		/* +0x08 */ uint16 vendorId; // little-endian ?
		/* +0x0A */ uint16 productId; // little-endian ?
		/* +0x0C */ uint8 ifIndex;
		/* +0x0D */ uint8 subClass;
		/* +0x0E */ uint8 protocol;
		/* +0x0F */ uint8 paddingGuessed0F;
		/* +0x10 */ uint16be maxPacketSizeRX;
		/* +0x12 */ uint16be maxPacketSizeTX;
	}HIDDevice_t;

	static_assert(offsetof(HIDDevice_t, vendorId) == 0x8, "");
	static_assert(offsetof(HIDDevice_t, productId) == 0xA, "");
	static_assert(offsetof(HIDDevice_t, ifIndex) == 0xC, "");
	static_assert(offsetof(HIDDevice_t, protocol) == 0xE, "");

	typedef struct _HIDDeviceInfo_t
	{
		uint32 handle;
		uint32 physicalDeviceInstance;
		uint16 vendorId;
		uint16 productId;
		uint8 interfaceIndex;
		uint8 interfaceSubClass;
		uint8 protocol;
		HIDDevice_t* hidDevice; // this info is passed to applications and must remain intact
		wchar_t* devicePath;
		_HIDDeviceInfo_t* next;
		// host
		HANDLE hFile;
	}HIDDeviceInfo_t;

	HIDDeviceInfo_t* firstDevice = nullptr;


	typedef struct _HIDClient_t
	{
		MEMPTR<_HIDClient_t> next;
		uint32be callbackFunc; // attach/detach callback
	}HIDClient_t;

	HIDClient_t* firstHIDClient = nullptr;

	HANDLE openDevice(wchar_t* devicePath)
	{
		return CreateFile(devicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ |
			FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
			NULL);
	}

	void attachClientToList(HIDClient_t* hidClient)
	{
		// todo - append at the beginning or end of the list? List order matters because it also controls the order in which attach callbacks are called
		if (firstHIDClient)
		{
			hidClient->next = firstHIDClient;
			firstHIDClient = hidClient;
		}
		else
		{
			hidClient->next = nullptr;
			firstHIDClient = hidClient;
		}
	}

	void attachDeviceToList(HIDDeviceInfo_t* hidDeviceInfo)
	{
		if (firstDevice)
		{
			hidDeviceInfo->next = firstDevice;
			firstDevice = hidDeviceInfo;
		}
		else
		{
			hidDeviceInfo->next = nullptr;
			firstDevice = hidDeviceInfo;
		}
	}

	HIDDeviceInfo_t* getHIDDeviceInfoByHandle(uint32 handle, bool openFileHandle = false)
	{
		HIDDeviceInfo_t* deviceItr = firstDevice;
		while (deviceItr)
		{
			if (deviceItr->handle == handle)
			{
				if (openFileHandle && deviceItr->hFile == INVALID_HANDLE_VALUE)
				{
					deviceItr->hFile = openDevice(deviceItr->devicePath);
					if (deviceItr->hFile == INVALID_HANDLE_VALUE)
					{
						forceLog_printfW(L"HID: Failed to open device \"%s\"", deviceItr->devicePath);
						return nullptr;
					}
					HidD_SetNumInputBuffers(deviceItr->hFile, 2); // dont cache too many reports
				}
				return deviceItr;
			}
			deviceItr = deviceItr->next;
		}
		return nullptr;
	}

	uint32 _lastGeneratedHidHandle = 1;

	uint32 generateHIDHandle()
	{
		_lastGeneratedHidHandle++;
		return _lastGeneratedHidHandle;
	}

	const int HID_MAX_NUM_DEVICES = 128;

	SysAllocator<HIDDevice_t, HID_MAX_NUM_DEVICES> _devicePool;
	std::bitset<HID_MAX_NUM_DEVICES> _devicePoolMask;

	HIDDevice_t* getFreeDevice()
	{
		for (sint32 i = 0; i < HID_MAX_NUM_DEVICES; i++)
		{
			if (_devicePoolMask.test(i) == false)
			{
				_devicePoolMask.set(i);
				return _devicePool.GetPtr() + i;
			}
		}
		return nullptr;
	}

	void checkAndAddDevice(wchar_t* devicePath, HANDLE hDevice)
	{
		HIDD_ATTRIBUTES hidAttr;
		hidAttr.Size = sizeof(HIDD_ATTRIBUTES);
		if (HidD_GetAttributes(hDevice, &hidAttr) == FALSE)
			return;
		HIDDevice_t* hidDevice = getFreeDevice();
		if (hidDevice == nullptr)
		{
			forceLog_printf("HID: Maximum number of supported devices exceeded");
			return;
		}

		HIDDeviceInfo_t* deviceInfo = (HIDDeviceInfo_t*)malloc(sizeof(HIDDeviceInfo_t));
		memset(deviceInfo, 0, sizeof(HIDDeviceInfo_t));
		deviceInfo->devicePath = _wcsdup(devicePath);
		deviceInfo->vendorId = hidAttr.VendorID;
		deviceInfo->productId = hidAttr.ProductID;
		deviceInfo->hFile = INVALID_HANDLE_VALUE;
		// generate handle
		deviceInfo->handle = generateHIDHandle();
		// get additional device info
		sint32 maxPacketInputLength = -1;
		sint32 maxPacketOutputLength = -1;
		PHIDP_PREPARSED_DATA ppData = nullptr;
		if (HidD_GetPreparsedData(hDevice, &ppData))
		{
			HIDP_CAPS caps;
			if (HidP_GetCaps(ppData, &caps) == HIDP_STATUS_SUCCESS)
			{
				// length includes the report id byte
				maxPacketInputLength = caps.InputReportByteLength - 1;
				maxPacketOutputLength = caps.OutputReportByteLength - 1;
			}
			HidD_FreePreparsedData(ppData);
		}
		if (maxPacketInputLength <= 0 || maxPacketInputLength >= 0xF000)
		{
			forceLog_printf("HID: Input packet length not available or out of range (length = %d)", maxPacketInputLength);
			maxPacketInputLength = 0x20;
		}
		if (maxPacketOutputLength <= 0 || maxPacketOutputLength >= 0xF000)
		{
			forceLog_printf("HID: Output packet length not available or out of range (length = %d)", maxPacketOutputLength);
			maxPacketOutputLength = 0x20;
		}
		// setup HIDDevice struct
		deviceInfo->hidDevice = hidDevice;
		memset(hidDevice, 0, sizeof(HIDDevice_t));
		hidDevice->handle = deviceInfo->handle;
		hidDevice->vendorId = deviceInfo->vendorId;
		hidDevice->productId = deviceInfo->productId;
		hidDevice->maxPacketSizeRX = maxPacketInputLength;
		hidDevice->maxPacketSizeTX = maxPacketOutputLength;

		hidDevice->ukn04 = 0x11223344;

		hidDevice->ifIndex = 1;
		hidDevice->protocol = 0;
		hidDevice->subClass = 2;

		// todo - other values
		//hidDevice->ifIndex = 1;


		attachDeviceToList(deviceInfo);

	}

	void initDeviceList()
	{
		if (firstDevice)
			return;
		HDEVINFO                         hDevInfo;
		SP_DEVICE_INTERFACE_DATA         DevIntfData;
		PSP_DEVICE_INTERFACE_DETAIL_DATA DevIntfDetailData;
		SP_DEVINFO_DATA                  DevData;

		DWORD dwSize, dwMemberIdx;

		hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_HID, NULL, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

		if (hDevInfo != INVALID_HANDLE_VALUE)
		{
			DevIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
			dwMemberIdx = 0;

			SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVINTERFACE_HID,
				dwMemberIdx, &DevIntfData);

			while (GetLastError() != ERROR_NO_MORE_ITEMS)
			{
				DevData.cbSize = sizeof(DevData);
				SetupDiGetDeviceInterfaceDetail(
					hDevInfo, &DevIntfData, NULL, 0, &dwSize, NULL);

				DevIntfDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize);
				DevIntfDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

				if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &DevIntfData,
					DevIntfDetailData, dwSize, &dwSize, &DevData))
				{
					HANDLE hHIDDevice = openDevice(DevIntfDetailData->DevicePath);
					if (hHIDDevice != INVALID_HANDLE_VALUE)
					{
						checkAndAddDevice(DevIntfDetailData->DevicePath, hHIDDevice);
						CloseHandle(hHIDDevice);
					}
				}
				HeapFree(GetProcessHeap(), 0, DevIntfDetailData);
				// next
				SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVINTERFACE_HID, ++dwMemberIdx, &DevIntfData);
			}
			SetupDiDestroyDeviceInfoList(hDevInfo);
		}
	}

	const int HID_CALLBACK_DETACH = 0;
	const int HID_CALLBACK_ATTACH = 1;

	uint32 doAttachCallback(HIDClient_t* hidClient, HIDDeviceInfo_t* deviceInfo)
	{
		return PPCCoreCallback(hidClient->callbackFunc, memory_getVirtualOffsetFromPointer(hidClient), memory_getVirtualOffsetFromPointer(deviceInfo->hidDevice), HID_CALLBACK_ATTACH);
	}

	void doDetachCallback(HIDClient_t* hidClient, HIDDeviceInfo_t* deviceInfo)
	{
		PPCCoreCallback(hidClient->callbackFunc, memory_getVirtualOffsetFromPointer(hidClient), memory_getVirtualOffsetFromPointer(deviceInfo->hidDevice), HID_CALLBACK_DETACH);
	}

	void export_HIDAddClient(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamTypePtr(hidClient, HIDClient_t, 0);
		ppcDefineParamMPTR(callbackFuncMPTR, 1);
		forceLogDebug_printf("nsyshid.HIDAddClient(0x%08x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4]);
		hidClient->callbackFunc = callbackFuncMPTR;
		attachClientToList(hidClient);
		initDeviceList();
		// do attach callbacks
		HIDDeviceInfo_t* deviceItr = firstDevice;
		while (deviceItr)
		{
			if (doAttachCallback(hidClient, deviceItr) != 0)
				break;
			deviceItr = deviceItr->next;
		}

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_HIDDelClient(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamTypePtr(hidClient, HIDClient_t, 0);
		forceLogDebug_printf("nsyshid.HIDDelClient(0x%08x)", hCPU->gpr[3]);
	
		// todo
		// do detach callbacks
		HIDDeviceInfo_t* deviceItr = firstDevice;
		while (deviceItr)
		{
			doDetachCallback(hidClient, deviceItr);
			deviceItr = deviceItr->next;
		}

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_HIDGetDescriptor(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0); // r3
		ppcDefineParamU8(descType, 1); // r4
		ppcDefineParamU8(descIndex, 2); // r5
		ppcDefineParamU8(lang, 3); // r6
		ppcDefineParamUStr(output, 4); // r7
		ppcDefineParamU32(outputMaxLength, 5); // r8
		ppcDefineParamMPTR(cbFuncMPTR, 6); // r9
		ppcDefineParamMPTR(cbParamMPTR, 7); // r10

		HIDDeviceInfo_t* hidDeviceInfo = getHIDDeviceInfoByHandle(hidHandle);
		if (hidDeviceInfo)
		{
			HANDLE hHIDDevice = openDevice(hidDeviceInfo->devicePath);
			if (hHIDDevice != INVALID_HANDLE_VALUE)
			{
				if (descType == 0x02)
				{
					uint8 configurationDescriptor[0x29];

					uint8* currentWritePtr;

					// configuration descriptor
					currentWritePtr = configurationDescriptor + 0;
					*(uint8*)(currentWritePtr + 0) = 9; // bLength
					*(uint8*)(currentWritePtr + 1) = 2; // bDescriptorType
					*(uint16be*)(currentWritePtr + 2) = 0x0029; // wTotalLength
					*(uint8*)(currentWritePtr + 4) = 1; // bNumInterfaces
					*(uint8*)(currentWritePtr + 5) = 1; // bConfigurationValue
					*(uint8*)(currentWritePtr + 6) = 0; // iConfiguration
					*(uint8*)(currentWritePtr + 7) = 0x80; // bmAttributes
					*(uint8*)(currentWritePtr + 8) = 0xFA; // MaxPower
					currentWritePtr = currentWritePtr + 9;
					// configuration descriptor
					*(uint8*)(currentWritePtr + 0) = 9; // bLength
					*(uint8*)(currentWritePtr + 1) = 0x04; // bDescriptorType
					*(uint8*)(currentWritePtr + 2) = 0; // bInterfaceNumber
					*(uint8*)(currentWritePtr + 3) = 0; // bAlternateSetting
					*(uint8*)(currentWritePtr + 4) = 2; // bNumEndpoints
					*(uint8*)(currentWritePtr + 5) = 3; // bInterfaceClass
					*(uint8*)(currentWritePtr + 6) = 0; // bInterfaceSubClass
					*(uint8*)(currentWritePtr + 7) = 0; // bInterfaceProtocol
					*(uint8*)(currentWritePtr + 8) = 0; // iInterface
					currentWritePtr = currentWritePtr + 9;
					// configuration descriptor
					*(uint8*)(currentWritePtr + 0) = 9; // bLength
					*(uint8*)(currentWritePtr + 1) = 0x21; // bDescriptorType
					*(uint16be*)(currentWritePtr + 2) = 0x0111; // bcdHID
					*(uint8*)(currentWritePtr + 4) = 0x00; // bCountryCode
					*(uint8*)(currentWritePtr + 5) = 0x01; // bNumDescriptors
					*(uint8*)(currentWritePtr + 6) = 0x22; // bDescriptorType
					*(uint16be*)(currentWritePtr + 7) = 0x001D; // wDescriptorLength
					currentWritePtr = currentWritePtr + 9;
					// endpoint descriptor 1
					*(uint8*)(currentWritePtr + 0) = 7; // bLength
					*(uint8*)(currentWritePtr + 1) = 0x05; // bDescriptorType
					*(uint8*)(currentWritePtr + 1) = 0x81; // bEndpointAddress
					*(uint8*)(currentWritePtr + 2) = 0x03; // bmAttributes
					*(uint16be*)(currentWritePtr + 3) = 0x40; // wMaxPacketSize
					*(uint8*)(currentWritePtr + 5) = 0x01; // bInterval
					currentWritePtr = currentWritePtr + 7;
					// endpoint descriptor 2
					*(uint8*)(currentWritePtr + 0) = 7; // bLength
					*(uint8*)(currentWritePtr + 1) = 0x05; // bDescriptorType
					*(uint8*)(currentWritePtr + 1) = 0x02; // bEndpointAddress
					*(uint8*)(currentWritePtr + 2) = 0x03; // bmAttributes
					*(uint16be*)(currentWritePtr + 3) = 0x40; // wMaxPacketSize
					*(uint8*)(currentWritePtr + 5) = 0x01; // bInterval
					currentWritePtr = currentWritePtr + 7;

					cemu_assert_debug((currentWritePtr - configurationDescriptor) == 0x29);

					memcpy(output, configurationDescriptor, std::min<uint32>(outputMaxLength, sizeof(configurationDescriptor)));
				}
				else
				{
					cemu_assert_unimplemented();
				}
				CloseHandle(hHIDDevice);
			}
			else
			{
				cemu_assert_unimplemented();
			}
		}
		else
		{
			cemu_assert_suspicious();
		}
		osLib_returnFromFunction(hCPU, 0);
	}

	void _debugPrintHex(std::string prefix, uint8* data, size_t len)
	{
		char debugOutput[1024] = { 0 };
		len = std::min(len, (size_t)100);
		for (sint32 i = 0; i < len; i++)
		{
			sprintf(debugOutput + i * 3, "%02x ", data[i]);
		}
		forceLogDebug_printf("[%s] Data: %s", prefix.c_str(), debugOutput);
	}

	void doHIDTransferCallback(MPTR callbackFuncMPTR, MPTR callbackParamMPTR, uint32 hidHandle, uint32 errorCode, MPTR buffer, sint32 length)
	{
		coreinitAsyncCallback_add(callbackFuncMPTR, 5, hidHandle, errorCode, buffer, length, callbackParamMPTR);
	}

	void export_HIDSetIdle(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0); // r3
		ppcDefineParamU32(ifIndex, 1); // r4
		ppcDefineParamU32(ukn, 2); // r5
		ppcDefineParamU32(duration, 3); // r6
		ppcDefineParamMPTR(callbackFuncMPTR, 4); // r7
		ppcDefineParamMPTR(callbackParamMPTR, 5); // r8
		forceLogDebug_printf("nsyshid.HIDSetIdle(...)");

		// todo
		if (callbackFuncMPTR)
		{
			doHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, hidHandle, 0, MPTR_NULL, 0);
		}
		else
		{
			cemu_assert_unimplemented();
		}
		osLib_returnFromFunction(hCPU, 0); // for non-async version, return number of bytes transferred
	}

	void export_HIDSetProtocol(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0); // r3
		ppcDefineParamU32(ifIndex, 1); // r4
		ppcDefineParamU32(protocol, 2); // r5
		ppcDefineParamMPTR(callbackFuncMPTR, 3); // r6
		ppcDefineParamMPTR(callbackParamMPTR, 4); // r7
		forceLogDebug_printf("nsyshid.HIDSetProtocol(...)");
		
		if (callbackFuncMPTR)
		{
			doHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, hidHandle, 0, MPTR_NULL, 0);
		}
		else
		{
			cemu_assert_unimplemented();
		}
		osLib_returnFromFunction(hCPU, 0); // for non-async version, return number of bytes transferred
	}

	// handler for async HIDSetReport transfers
	void _hidSetReportAsync(HIDDeviceInfo_t* hidDeviceInfo, uint8* reportData, sint32 length, uint8* originalData, sint32 originalLength, MPTR callbackFuncMPTR, MPTR callbackParamMPTR)
	{
		sint32 retryCount = 0;
		while (true)
		{
			BOOL r = HidD_SetOutputReport(hidDeviceInfo->hFile, reportData, length);
			if (r != FALSE)
				break;
			Sleep(20); // retry
			retryCount++;
			if (retryCount >= 40)
			{
				forceLog_printf("HID async SetReport failed");
				sint32 errorCode = -1;
				doHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, hidDeviceInfo->handle, errorCode, memory_getVirtualOffsetFromPointer(originalData), 0);
				free(reportData);
				return;
			}
		}
		doHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, hidDeviceInfo->handle, 0, memory_getVirtualOffsetFromPointer(originalData), originalLength);
		free(reportData);
	}

	// handler for synchronous HIDSetReport transfers
	sint32 _hidSetReportSync(HIDDeviceInfo_t* hidDeviceInfo, uint8* reportData, sint32 length, uint8* originalData, sint32 originalLength, OSThread_t* osThread)
	{
		//forceLogDebug_printf("_hidSetReportSync begin");
		_debugPrintHex("_hidSetReportSync Begin", reportData, length);
		sint32 retryCount = 0;
		sint32 returnCode = 0;
		while (true)
		{
			BOOL r = HidD_SetOutputReport(hidDeviceInfo->hFile, reportData, length);
			if (r != FALSE)
			{
				returnCode = originalLength;
				break;
			}
			Sleep(100); // retry
			retryCount++;
			if (retryCount >= 10)
				assert_dbg();
		}
		free(reportData);
		forceLogDebug_printf("_hidSetReportSync end. returnCode: %d", returnCode);
		coreinit_resumeThread(osThread, 1000);
		return returnCode;
	}

	void export_HIDSetReport(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0); // r3
		ppcDefineParamU32(reportRelatedUkn, 1); // r4
		ppcDefineParamU32(reportId, 2); // r5
		ppcDefineParamUStr(data, 3); // r6
		ppcDefineParamU32(dataLength, 4); // r7
		ppcDefineParamMPTR(callbackFuncMPTR, 5); // r8
		ppcDefineParamMPTR(callbackParamMPTR, 6); // r9
		forceLogDebug_printf("nsyshid.HIDSetReport(%d,0x%02x,0x%02x,...)", hidHandle, reportRelatedUkn, reportId);

		_debugPrintHex("HIDSetReport", data, dataLength);

#ifdef CEMU_DEBUG_ASSERT
		if (reportRelatedUkn != 2 || reportId != 0)
			assert_dbg();
#endif

		HIDDeviceInfo_t* hidDeviceInfo = getHIDDeviceInfoByHandle(hidHandle, true);
		if (hidDeviceInfo == nullptr)
		{
			forceLog_printf("nsyshid.HIDSetReport(): Unable to find device with hid handle %d", hidHandle);
			osLib_returnFromFunction(hCPU, -1);
			return;
		}

		// prepare report data
		// note: Currently we need to pad the data to 0x20 bytes for it to work (plus one extra byte for HidD_SetOutputReport)
		// Does IOSU pad data to 0x20 byte? Also check if this is specific to Skylanders portal 
		sint32 paddedLength = (dataLength +0x1F)&~0x1F;
		uint8* reportData = (uint8*)malloc(paddedLength+1);
		memset(reportData, 0, paddedLength+1);
		reportData[0] = 0;
		memcpy(reportData + 1, data, dataLength);


		// issue request (synchronous or asynchronous)
		sint32 returnCode = 0;
		if (callbackFuncMPTR == MPTR_NULL)
		{
			std::future<sint32> res = std::async(std::launch::async, &_hidSetReportSync, hidDeviceInfo, reportData, paddedLength + 1, data, dataLength, coreinitThread_getCurrentThreadDepr(hCPU));
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(hCPU), 1000);
			PPCCore_switchToScheduler();
			returnCode = res.get();
		}
		else
		{
			// asynchronous
			std::thread(&_hidSetReportAsync, hidDeviceInfo, reportData, paddedLength+1, data, dataLength, callbackFuncMPTR, callbackParamMPTR).detach();
			returnCode = 0;
		}
		osLib_returnFromFunction(hCPU, returnCode);
	}

	sint32 _hidReadInternalSync(HIDDeviceInfo_t* hidDeviceInfo, uint8* data, sint32 maxLength)
	{
		DWORD bt;
		OVERLAPPED ovlp = { 0 };
		ovlp.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		uint8* tempBuffer = (uint8*)malloc(maxLength + 1);
		sint32 transferLength = 0; // minus report byte

		_debugPrintHex("HID_READ_BEFORE", data, maxLength);

		forceLogDebug_printf("HidRead Begin (Length 0x%08x)", maxLength);
		BOOL readResult = ReadFile(hidDeviceInfo->hFile, tempBuffer, maxLength + 1, &bt, &ovlp);
		if (readResult != FALSE)
		{
			// sometimes we get the result immediately
			if (bt == 0)
				transferLength = 0;
			else
				transferLength = bt - 1;
			forceLogDebug_printf("HidRead Result received immediately (error 0x%08x) Length 0x%08x", GetLastError(), transferLength);
		}
		else
		{
			// wait for result
			forceLogDebug_printf("HidRead WaitForResult (error 0x%08x)", GetLastError());
			// async hid read is never supposed to return unless there is an response? Lego Dimensions stops HIDRead calls as soon as one of them fails with a non-zero error (which includes time out)
			DWORD r = WaitForSingleObject(ovlp.hEvent, 2000*100);
			if (r == WAIT_TIMEOUT)
			{
				forceLogDebug_printf("HidRead internal timeout (error 0x%08x)", GetLastError());
				// return -108 in case of timeout
				free(tempBuffer);
				CloseHandle(ovlp.hEvent);
				return -108;
			}


			forceLogDebug_printf("HidRead WaitHalfComplete");
			GetOverlappedResult(hidDeviceInfo->hFile, &ovlp, &bt, false);
			if (bt == 0)
				transferLength = 0;
			else
				transferLength = bt - 1;
			forceLogDebug_printf("HidRead WaitComplete Length: 0x%08x", transferLength);
		}
		sint32 returnCode = 0;
		if (bt != 0)
		{
			memcpy(data, tempBuffer + 1, transferLength);
			sint32 hidReadLength = transferLength;

			char debugOutput[1024] = { 0 };
			for (sint32 i = 0; i < transferLength; i++)
			{
				sprintf(debugOutput + i * 3, "%02x ", tempBuffer[1 + i]);
			}
			forceLogDebug_printf("HIDRead data: %s", debugOutput);

			returnCode = transferLength;
		}
		else
		{
			forceLog_printf("Failed HID read");
			returnCode = -1;
		}
		free(tempBuffer);
		CloseHandle(ovlp.hEvent);
		return returnCode;
	}

	void _hidReadAsync(HIDDeviceInfo_t* hidDeviceInfo, uint8* data, sint32 maxLength, MPTR callbackFuncMPTR, MPTR callbackParamMPTR)
	{
		sint32 returnCode = _hidReadInternalSync(hidDeviceInfo, data, maxLength);
		sint32 errorCode = 0;
		if (returnCode < 0)
			errorCode = returnCode; // dont return number of bytes in error code
		doHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, hidDeviceInfo->handle, errorCode, memory_getVirtualOffsetFromPointer(data), (returnCode>0)?returnCode:0);
	}

	sint32 _hidReadSync(HIDDeviceInfo_t* hidDeviceInfo, uint8* data, sint32 maxLength, OSThread_t* osThread)
	{
		sint32 returnCode = _hidReadInternalSync(hidDeviceInfo, data, maxLength);
		coreinit_resumeThread(osThread, 1000);
		return returnCode;
	}

	void export_HIDRead(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0); // r3
		ppcDefineParamUStr(data, 1); // r4
		ppcDefineParamU32(maxLength, 2); // r5
		ppcDefineParamMPTR(callbackFuncMPTR, 3); // r6
		ppcDefineParamMPTR(callbackParamMPTR, 4); // r7
		forceLogDebug_printf("nsyshid.HIDRead(0x%x,0x%08x,0x%08x,0x%08x,0x%08x) LR %08x", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7], hCPU->spr.LR);

		HIDDeviceInfo_t* hidDeviceInfo = getHIDDeviceInfoByHandle(hidHandle, true);
		if (hidDeviceInfo == nullptr)
		{
			forceLog_printf("nsyshid.HIDRead(): Unable to find device with hid handle %d", hidHandle);
			osLib_returnFromFunction(hCPU, -1);
			return;
		}
		sint32 returnCode = 0;
		if (callbackFuncMPTR != MPTR_NULL)
		{
			// asynchronous transfer
			std::thread(&_hidReadAsync, hidDeviceInfo, data, maxLength, callbackFuncMPTR, callbackParamMPTR).detach();
			returnCode = 0;
		}
		else
		{
			// synchronous transfer
			std::future<sint32> res = std::async(std::launch::async, &_hidReadSync, hidDeviceInfo, data, maxLength, coreinitThread_getCurrentThreadDepr(hCPU));
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(hCPU), 1000);
			PPCCore_switchToScheduler();
			returnCode = res.get();
		}

		osLib_returnFromFunction(hCPU, returnCode);
	}

	sint32 _hidWriteInternalSync(HIDDeviceInfo_t* hidDeviceInfo, uint8* data, sint32 maxLength)
	{
		DWORD bt;
		OVERLAPPED ovlp = { 0 };
		ovlp.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		uint8* tempBuffer = (uint8*)malloc(maxLength + 1);
		memcpy(tempBuffer + 1, data, maxLength);
		tempBuffer[0] = 0; // report byte?

		forceLogDebug_printf("HidWrite Begin (Length 0x%08x)", maxLength);
		BOOL WriteResult = WriteFile(hidDeviceInfo->hFile, tempBuffer, maxLength + 1, &bt, &ovlp);
		if (WriteResult != FALSE)
		{
			// sometimes we get the result immediately
			forceLogDebug_printf("HidWrite Result received immediately (error 0x%08x) Length 0x%08x", GetLastError());
		}
		else
		{
			// wait for result
			forceLogDebug_printf("HidWrite WaitForResult (error 0x%08x)", GetLastError());
			// todo - check for error type
			DWORD r = WaitForSingleObject(ovlp.hEvent, 2000);
			if (r == WAIT_TIMEOUT)
			{
				forceLogDebug_printf("HidWrite internal timeout");
				// return -108 in case of timeout
				free(tempBuffer);
				CloseHandle(ovlp.hEvent);
				return -108;
			}


			forceLogDebug_printf("HidWrite WaitHalfComplete");
			GetOverlappedResult(hidDeviceInfo->hFile, &ovlp, &bt, false);
			forceLogDebug_printf("HidWrite WaitComplete");
		}
		sint32 returnCode = 0;
		if (bt != 0)
			returnCode = maxLength;
		else
			returnCode = -1;
		
		free(tempBuffer);
		CloseHandle(ovlp.hEvent);
		return returnCode;
	}

	void _hidWriteAsync(HIDDeviceInfo_t* hidDeviceInfo, uint8* data, sint32 maxLength, MPTR callbackFuncMPTR, MPTR callbackParamMPTR)
	{
		sint32 returnCode = _hidWriteInternalSync(hidDeviceInfo, data, maxLength);
		sint32 errorCode = 0;
		if (returnCode < 0)
			errorCode = returnCode; // dont return number of bytes in error code
		doHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, hidDeviceInfo->handle, errorCode, memory_getVirtualOffsetFromPointer(data), (returnCode > 0) ? returnCode : 0);
	}

	sint32 _hidWriteSync(HIDDeviceInfo_t* hidDeviceInfo, uint8* data, sint32 maxLength, OSThread_t* osThread)
	{
		sint32 returnCode = _hidWriteInternalSync(hidDeviceInfo, data, maxLength);
		coreinit_resumeThread(osThread, 1000);
		return returnCode;
	}

	void export_HIDWrite(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0); // r3
		ppcDefineParamUStr(data, 1); // r4
		ppcDefineParamU32(maxLength, 2); // r5
		ppcDefineParamMPTR(callbackFuncMPTR, 3); // r6
		ppcDefineParamMPTR(callbackParamMPTR, 4); // r7
		forceLogDebug_printf("nsyshid.HIDWrite(0x%x,0x%08x,0x%08x,0x%08x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7]);

		HIDDeviceInfo_t* hidDeviceInfo = getHIDDeviceInfoByHandle(hidHandle, true);
		if (hidDeviceInfo == nullptr)
		{
			forceLog_printf("nsyshid.HIDWrite(): Unable to find device with hid handle %d", hidHandle);
			osLib_returnFromFunction(hCPU, -1);
			return;
		}
		sint32 returnCode = 0;
		if (callbackFuncMPTR != MPTR_NULL)
		{
			// asynchronous transfer
			std::thread(&_hidWriteAsync, hidDeviceInfo, data, maxLength, callbackFuncMPTR, callbackParamMPTR).detach();
			returnCode = 0;
		}
		else
		{
			// synchronous transfer
			std::future<sint32> res = std::async(std::launch::async, &_hidWriteSync, hidDeviceInfo, data, maxLength, coreinitThread_getCurrentThreadDepr(hCPU));
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(hCPU), 1000);
			PPCCore_switchToScheduler();
			returnCode = res.get();
		}

		osLib_returnFromFunction(hCPU, returnCode);
	}

	void export_HIDDecodeError(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(errorCode, 0);
		ppcDefineParamTypePtr(ukn0, uint32be, 1);
		ppcDefineParamTypePtr(ukn1, uint32be, 2);
		forceLogDebug_printf("nsyshid.HIDDecodeError(0x%08x,0x%08x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);

		// todo
		*ukn0 = 0x3FF;
		*ukn1 = (uint32)-0x7FFF;

		osLib_returnFromFunction(hCPU, 0);
	}

	void load()
	{
		osLib_addFunction("nsyshid", "HIDAddClient", export_HIDAddClient);
		osLib_addFunction("nsyshid", "HIDDelClient", export_HIDDelClient);
		osLib_addFunction("nsyshid", "HIDGetDescriptor", export_HIDGetDescriptor);
		osLib_addFunction("nsyshid", "HIDSetIdle", export_HIDSetIdle);
		osLib_addFunction("nsyshid", "HIDSetProtocol", export_HIDSetProtocol);
		osLib_addFunction("nsyshid", "HIDSetReport", export_HIDSetReport);

		osLib_addFunction("nsyshid", "HIDRead", export_HIDRead);
		osLib_addFunction("nsyshid", "HIDWrite", export_HIDWrite);

		osLib_addFunction("nsyshid", "HIDDecodeError", export_HIDDecodeError);
		firstHIDClient = nullptr;
	}
}

#else

namespace nsyshid
{
	void load()
	{
		// unimplemented
	};
};


#endif