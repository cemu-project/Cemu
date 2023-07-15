#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include <bitset>
#include "nsyshid.h"

#include <libusb.h>

namespace nsyshid
{

	HIDClient_t *firstHIDClient = nullptr;
	std::vector<std::shared_ptr<usb_device>> usb_devices;
	libusb_context *ctx = nullptr;
	uint32 _lastGeneratedHidHandle = 1;

	uint32 generateHIDHandle()
	{
		_lastGeneratedHidHandle++;
		return _lastGeneratedHidHandle;
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

	std::shared_ptr<usb_device> getHIDDeviceInfoByHandle(uint32 handle, bool openFileHandle = false)
	{
		for (std::shared_ptr<usb_device> device : usb_devices) {
			if (device->handle == handle) {
				return device;
			}
		}
		return nullptr;
	}

	void initDeviceList()
	{
		if (usb_devices.empty())
			return;

		if (int res = libusb_init(&ctx); res < 0)
		{
			return;
		}
		// look if any device which we could be interested in is actually connected
		libusb_device** list = nullptr;
		ssize_t ndev         = libusb_get_device_list(ctx, &list);

		if (ndev < 0)
		{
			return;
		}

		bool found_skylander = false;
		bool found_infinity  = false;
		bool found_dimension = false;

		for (ssize_t index = 0; index < ndev; index++)
		{
			libusb_device_descriptor desc;
			if (int res = libusb_get_device_descriptor(list[index], &desc); res < 0)
			{
				continue;
			}

			auto check_device = [&](const uint16 id_vendor, const uint16 id_product_min, const uint16 id_product_max, const char* s_name) -> bool
			{
				if (desc.idVendor == id_vendor && desc.idProduct >= id_product_min && desc.idProduct <= id_product_max)
				{
					libusb_ref_device(list[index]);
					std::shared_ptr<usb_device_passthrough> usb_dev = std::make_shared<usb_device_passthrough>(list[index], desc, generateHIDHandle());
					usb_devices.push_back(usb_dev);
					return true;
				}
				return false;
			};

			// Portals
			if (check_device(0x1430, 0x0150, 0x0150, "Skylanders Portal"))
			{
				found_skylander = true;
			}

			if (check_device(0x0E6F, 0x0129, 0x0129, "Disney Infinity Base"))
			{
				found_infinity = true;
			}
			
			if (check_device(0x0E6F, 0x0241, 0x0241, "Lego Dimensions Portal"))
			{
				found_dimension = true;
			}
		}

		libusb_free_device_list(list, 1);
	}

	const int HID_CALLBACK_DETACH = 0;
	const int HID_CALLBACK_ATTACH = 1;

	uint32 doAttachCallback(HIDClient_t* hidClient, std::shared_ptr<usb_device> deviceInfo)
	{
		return PPCCoreCallback(hidClient->callbackFunc, memory_getVirtualOffsetFromPointer(hidClient), memory_getVirtualOffsetFromPointer(deviceInfo->hidDevice), HID_CALLBACK_ATTACH);
	}

	void doDetachCallback(HIDClient_t* hidClient, std::shared_ptr<usb_device> deviceInfo)
	{
		PPCCoreCallback(hidClient->callbackFunc, memory_getVirtualOffsetFromPointer(hidClient), memory_getVirtualOffsetFromPointer(deviceInfo->hidDevice), HID_CALLBACK_DETACH);
	}
	
	void export_HIDAddClient(PPCInterpreter_t* hCPU) {
		ppcDefineParamTypePtr(hidClient, HIDClient_t, 0);
		ppcDefineParamMPTR(callbackFuncMPTR, 1);
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDAddClient(0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4]);
		hidClient->callbackFunc = callbackFuncMPTR;
		attachClientToList(hidClient);
		initDeviceList();
		// do attach callbacks
		for (std::shared_ptr<usb_device> deviceItr : usb_devices) {
			if (doAttachCallback(hidClient, deviceItr) != 0)
				break;
		}

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_HIDDelClient(PPCInterpreter_t* hCPU) {
		ppcDefineParamTypePtr(hidClient, HIDClient_t, 0);
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDDelClient(0x{:08x})", hCPU->gpr[3]);
	
		// todo
		// do detach callbacks
		for (std::shared_ptr<usb_device> deviceItr : usb_devices)
		{
			doDetachCallback(hidClient, deviceItr);
		}

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_HIDGetDescriptor(PPCInterpreter_t* hCPU) {
		ppcDefineParamU32(hidHandle, 0); // r3
		ppcDefineParamU8(descType, 1); // r4
		ppcDefineParamU8(descIndex, 2); // r5
		ppcDefineParamU8(lang, 3); // r6
		ppcDefineParamUStr(output, 4); // r7
		ppcDefineParamU32(outputMaxLength, 5); // r8
		ppcDefineParamMPTR(cbFuncMPTR, 6); // r9
		ppcDefineParamMPTR(cbParamMPTR, 7); // r10

		std::shared_ptr<usb_device> hidDeviceInfo = getHIDDeviceInfoByHandle(hidHandle);
		if (hidDeviceInfo)
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
		cemuLog_logDebug(LogType::Force, "[{}] Data: {}", prefix, debugOutput);
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
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDSetIdle(...)");

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

	void export_HIDSetProtocol(PPCInterpreter_t* hCPU) {
		ppcDefineParamU32(hidHandle, 0); // r3
		ppcDefineParamU32(ifIndex, 1); // r4
		ppcDefineParamU32(protocol, 2); // r5
		ppcDefineParamMPTR(callbackFuncMPTR, 3); // r6
		ppcDefineParamMPTR(callbackParamMPTR, 4); // r7
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDSetProtocol(...)");
		
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
	void _hidSetReportAsync(std::shared_ptr<usb_device> hidDeviceInfo, uint8* reportData, sint32 length, uint8* originalData, sint32 originalLength, MPTR callbackFuncMPTR, MPTR callbackParamMPTR)
	{
		sint32 retryCount = 0;
		while (true)
		{
			// TODO libsub control transfer
			retryCount++;
			if (retryCount >= 40)
			{
				cemuLog_log(LogType::Force, "HID async SetReport failed");
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
	sint32 _hidSetReportSync(std::shared_ptr<usb_device> hidDeviceInfo, uint8* reportData, sint32 length, uint8* originalData, sint32 originalLength, OSThread_t* osThread)
	{
		//cemuLog_logDebug(LogType::Force, "_hidSetReportSync begin");
		_debugPrintHex("_hidSetReportSync Begin", reportData, length);
		sint32 retryCount = 0;
		sint32 returnCode = 0;
		while (true)
		{
			// TODO libsub control transfer
		}
		free(reportData);
		cemuLog_logDebug(LogType::Force, "_hidSetReportSync end. returnCode: {}", returnCode);
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
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDSetReport({},0x{:02x},0x{:02x},...)", hidHandle, reportRelatedUkn, reportId);

		_debugPrintHex("HIDSetReport", data, dataLength);

#ifdef CEMU_DEBUG_ASSERT
		if (reportRelatedUkn != 2 || reportId != 0)
			assert_dbg();
#endif

		std::shared_ptr<usb_device> hidDeviceInfo = getHIDDeviceInfoByHandle(hidHandle, true);
		if (hidDeviceInfo == nullptr)
		{
			cemuLog_log(LogType::Force, "nsyshid.HIDSetReport(): Unable to find device with hid handle {}", hidHandle);
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

	sint32 _hidReadInternalSync(std::shared_ptr<usb_device> hidDeviceInfo, uint8* data, sint32 maxLength)
	{
		sint32 returnCode = 0;
		// TODO libusb interrupt transfer
		return returnCode;
	}

	void _hidReadAsync(std::shared_ptr<usb_device> hidDeviceInfo, uint8* data, sint32 maxLength, MPTR callbackFuncMPTR, MPTR callbackParamMPTR)
	{
		sint32 returnCode = _hidReadInternalSync(hidDeviceInfo, data, maxLength);
		sint32 errorCode = 0;
		if (returnCode < 0)
			errorCode = returnCode; // dont return number of bytes in error code
		doHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, hidDeviceInfo->handle, errorCode, memory_getVirtualOffsetFromPointer(data), (returnCode>0)?returnCode:0);
	}

	sint32 _hidReadSync(std::shared_ptr<usb_device> hidDeviceInfo, uint8* data, sint32 maxLength, OSThread_t* osThread)
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
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDRead(0x{:x},0x{:08x},0x{:08x},0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7]);

		std::shared_ptr<usb_device> hidDeviceInfo = getHIDDeviceInfoByHandle(hidHandle, true);
		if (hidDeviceInfo == nullptr)
		{
			cemuLog_log(LogType::Force, "nsyshid.HIDRead(): Unable to find device with hid handle {}", hidHandle);
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

	sint32 _hidWriteInternalSync(std::shared_ptr<usb_device> hidDeviceInfo, uint8* data, sint32 maxLength)
	{
		sint32 returnCode = 0;
		// TODO libusb interrupt transfer
		return returnCode;
	}

	void _hidWriteAsync(std::shared_ptr<usb_device> hidDeviceInfo, uint8* data, sint32 maxLength, MPTR callbackFuncMPTR, MPTR callbackParamMPTR)
	{
		sint32 returnCode = _hidWriteInternalSync(hidDeviceInfo, data, maxLength);
		sint32 errorCode = 0;
		if (returnCode < 0)
			errorCode = returnCode; // dont return number of bytes in error code
		doHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, hidDeviceInfo->handle, errorCode, memory_getVirtualOffsetFromPointer(data), (returnCode > 0) ? returnCode : 0);
	}

	sint32 _hidWriteSync(std::shared_ptr<usb_device> hidDeviceInfo, uint8* data, sint32 maxLength, OSThread_t* osThread)
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
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDWrite(0x{:x},0x{:08x},0x{:08x},0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7]);

		std::shared_ptr<usb_device> hidDeviceInfo = getHIDDeviceInfoByHandle(hidHandle, true);
		if (hidDeviceInfo == nullptr)
		{
			cemuLog_log(LogType::Force, "nsyshid.HIDWrite(): Unable to find device with hid handle {}", hidHandle);
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
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDDecodeError(0x{:08x},0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);

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
	};

	usb_device::usb_device(uint32 handle)
	{
		handle = handle;
	}

	usb_device_passthrough::usb_device_passthrough(libusb_device* _device, libusb_device_descriptor& desc, uint32 handle)
		: usb_device(handle), lusb_device(_device)
	{
		vendorId = desc.idVendor;
		productId = desc.idProduct;
	}

	usb_device_passthrough::~usb_device_passthrough()
	{
		if (lusb_handle)
		{
			libusb_release_interface(lusb_handle, 0);
			libusb_close(lusb_handle);
		}

		if (lusb_device)
		{
			libusb_unref_device(lusb_device);
		}
	}
};
