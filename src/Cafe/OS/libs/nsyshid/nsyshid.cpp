#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include <bitset>
#include <mutex>
#include "nsyshid.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Backend.h"
#include "Whitelist.h"

namespace nsyshid
{

	std::list<std::shared_ptr<Backend>> backendList;

	std::list<std::shared_ptr<Device>> deviceList;

	typedef struct _HIDClient_t
	{
		uint32be callbackFunc; // attach/detach callback
	} HIDClient_t;

	std::list<HIDClient_t*> HIDClientList;

	std::recursive_mutex hidMutex;

	void AttachClientToList(HIDClient_t* hidClient)
	{
		std::lock_guard<std::recursive_mutex> lock(hidMutex);
		// todo - append at the beginning or end of the list? List order matters because it also controls the order in which attach callbacks are called
		HIDClientList.push_front(hidClient);
	}

	void DetachClientFromList(HIDClient_t* hidClient)
	{
		std::lock_guard<std::recursive_mutex> lock(hidMutex);
		HIDClientList.remove(hidClient);
	}

	std::shared_ptr<Device> GetDeviceByHandle(uint32 handle, bool openIfClosed = false)
	{
		std::shared_ptr<Device> device;
		{
			std::lock_guard<std::recursive_mutex> lock(hidMutex);
			for (const auto& d : deviceList)
			{
				if (d->m_hid->handle == handle)
				{
					device = d;
					break;
				}
			}
		}
		if (device != nullptr)
		{
			if (openIfClosed && !device->IsOpened())
			{
				if (!device->Open())
				{
					return nullptr;
				}
			}
			return device;
		}
		return nullptr;
	}

	uint32 _lastGeneratedHidHandle = 1;

	uint32 GenerateHIDHandle()
	{
		std::lock_guard<std::recursive_mutex> lock(hidMutex);
		_lastGeneratedHidHandle++;
		return _lastGeneratedHidHandle;
	}

	const int HID_MAX_NUM_DEVICES = 128;

	SysAllocator<HID_t, HID_MAX_NUM_DEVICES> HIDPool;
	std::queue<size_t> HIDPoolIndexQueue;

	void InitHIDPoolIndexQueue()
	{
		static bool HIDPoolIndexQueueInitialized = false;
		std::lock_guard<std::recursive_mutex> lock(hidMutex);
		if (HIDPoolIndexQueueInitialized)
		{
			return;
		}
		HIDPoolIndexQueueInitialized = true;
		for (size_t i = 0; i < HID_MAX_NUM_DEVICES; i++)
		{
			HIDPoolIndexQueue.push(i);
		}
	}

	HID_t* GetFreeHID()
	{
		std::lock_guard<std::recursive_mutex> lock(hidMutex);
		InitHIDPoolIndexQueue();
		if (HIDPoolIndexQueue.empty())
		{
			return nullptr;
		}
		size_t index = HIDPoolIndexQueue.front();
		HIDPoolIndexQueue.pop();
		return HIDPool.GetPtr() + index;
	}

	void ReleaseHID(HID_t* device)
	{
		// this should never happen, but having a safeguard can't hurt
		if (device == nullptr)
		{
			cemu_assert_error();
		}
		std::lock_guard<std::recursive_mutex> lock(hidMutex);
		InitHIDPoolIndexQueue();
		size_t index = device - HIDPool.GetPtr();
		HIDPoolIndexQueue.push(index);
	}

	const int HID_CALLBACK_DETACH = 0;
	const int HID_CALLBACK_ATTACH = 1;

	uint32 DoAttachCallback(HIDClient_t* hidClient, const std::shared_ptr<Device>& device)
	{
		return PPCCoreCallback(hidClient->callbackFunc, memory_getVirtualOffsetFromPointer(hidClient),
							   memory_getVirtualOffsetFromPointer(device->m_hid), HID_CALLBACK_ATTACH);
	}

	void DoAttachCallbackAsync(HIDClient_t* hidClient, const std::shared_ptr<Device>& device)
	{
		coreinitAsyncCallback_add(hidClient->callbackFunc, 3, memory_getVirtualOffsetFromPointer(hidClient),
								  memory_getVirtualOffsetFromPointer(device->m_hid), HID_CALLBACK_ATTACH);
	}

	void DoDetachCallback(HIDClient_t* hidClient, const std::shared_ptr<Device>& device)
	{
		PPCCoreCallback(hidClient->callbackFunc, memory_getVirtualOffsetFromPointer(hidClient),
						memory_getVirtualOffsetFromPointer(device->m_hid), HID_CALLBACK_DETACH);
	}

	void DoDetachCallbackAsync(HIDClient_t* hidClient, const std::shared_ptr<Device>& device)
	{
		coreinitAsyncCallback_add(hidClient->callbackFunc, 3, memory_getVirtualOffsetFromPointer(hidClient),
								  memory_getVirtualOffsetFromPointer(device->m_hid), HID_CALLBACK_DETACH);
	}

	void AttachBackend(const std::shared_ptr<Backend>& backend)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(hidMutex);
			backendList.push_back(backend);
		}
		backend->OnAttach();
	}

	void DetachBackend(const std::shared_ptr<Backend>& backend)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(hidMutex);
			backendList.remove(backend);
		}
		backend->OnDetach();
	}

	void DetachAllBackends()
	{
		std::list<std::shared_ptr<Backend>> backendListCopy;
		{
			std::lock_guard<std::recursive_mutex> lock(hidMutex);
			backendListCopy = backendList;
			backendList.clear();
		}
		for (const auto& backend : backendListCopy)
		{
			backend->OnDetach();
		}
	}

	void AttachDefaultBackends()
	{
		backend::AttachDefaultBackends();
	}

	bool AttachDevice(const std::shared_ptr<Device>& device)
	{
		std::lock_guard<std::recursive_mutex> lock(hidMutex);

		// is the device already attached?
		{
			auto it = std::find(deviceList.begin(), deviceList.end(), device);
			if (it != deviceList.end())
			{
				cemuLog_logDebug(LogType::Force,
								 "nsyshid.AttachDevice(): failed to attach device: {:04x}:{:04x}: already attached",
								 device->m_vendorId,
								 device->m_productId);
				return false;
			}
		}

		HID_t* hidDevice = GetFreeHID();
		if (hidDevice == nullptr)
		{
			cemuLog_logDebug(LogType::Force,
							 "nsyshid.AttachDevice(): failed to attach device: {:04x}:{:04x}: no free device slots left",
							 device->m_vendorId,
							 device->m_productId);
			return false;
		}
		hidDevice->handle = GenerateHIDHandle();
		device->AssignHID(hidDevice);
		deviceList.push_back(device);

		// do attach callbacks
		for (auto client : HIDClientList)
		{
			DoAttachCallbackAsync(client, device);
		}

		cemuLog_logDebug(LogType::Force, "nsyshid.AttachDevice(): device attached: {:04x}:{:04x}",
						 device->m_vendorId,
						 device->m_productId);
		return true;
	}

	void DetachDevice(const std::shared_ptr<Device>& device)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(hidMutex);

			// remove from list
			auto it = std::find(deviceList.begin(), deviceList.end(), device);
			if (it == deviceList.end())
			{
				cemuLog_logDebug(LogType::Force, "nsyshid.DetachDevice(): device not found: {:04x}:{:04x}",
								 device->m_vendorId,
								 device->m_productId);
				return;
			}
			deviceList.erase(it);

			// do detach callbacks
			for (auto client : HIDClientList)
			{
				DoDetachCallbackAsync(client, device);
			}
			ReleaseHID(device->m_hid);
		}

		device->Close();

		cemuLog_logDebug(LogType::Force, "nsyshid.DetachDevice(): device removed: {:04x}:{:04x}",
						 device->m_vendorId,
						 device->m_productId);
	}

	void export_HIDAddClient(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamTypePtr(hidClient, HIDClient_t, 0);
		ppcDefineParamMPTR(callbackFuncMPTR, 1);
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDAddClient(0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4]);
		hidClient->callbackFunc = callbackFuncMPTR;

		std::lock_guard<std::recursive_mutex> lock(hidMutex);
		AttachClientToList(hidClient);

		// do attach callbacks
		for (const auto& device : deviceList)
		{
			DoAttachCallback(hidClient, device);
		}

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_HIDDelClient(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamTypePtr(hidClient, HIDClient_t, 0);
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDDelClient(0x{:08x})", hCPU->gpr[3]);

		std::lock_guard<std::recursive_mutex> lock(hidMutex);
		DetachClientFromList(hidClient);

		// do detach callbacks
		for (const auto& device : deviceList)
		{
			DoDetachCallback(hidClient, device);
		}

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_HIDGetDescriptor(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0);	   // r3
		ppcDefineParamU8(descType, 1);		   // r4
		ppcDefineParamU8(descIndex, 2);		   // r5
		ppcDefineParamU8(lang, 3);			   // r6
		ppcDefineParamUStr(output, 4);		   // r7
		ppcDefineParamU32(outputMaxLength, 5); // r8
		ppcDefineParamMPTR(cbFuncMPTR, 6);	   // r9
		ppcDefineParamMPTR(cbParamMPTR, 7);	   // r10

		int returnValue = -1;
		std::shared_ptr<Device> device = GetDeviceByHandle(hidHandle, true);
		if (device)
		{
			memset(output, 0, outputMaxLength);
			if (device->GetDescriptor(descType, descIndex, lang, output, outputMaxLength))
			{
				returnValue = 0;
			}
			else
			{
				returnValue = -1;
			}
		}
		else
		{
			cemu_assert_suspicious();
		}
		osLib_returnFromFunction(hCPU, returnValue);
	}

	void _debugPrintHex(std::string prefix, uint8* data, size_t len)
	{
		char debugOutput[1024] = {0};
		len = std::min(len, (size_t)100);
		for (sint32 i = 0; i < len; i++)
		{
			sprintf(debugOutput + i * 3, "%02x ", data[i]);
		}
		fmt::print("{} Data: {}\n", prefix, debugOutput);
		cemuLog_logDebug(LogType::Force, "[{}] Data: {}", prefix, debugOutput);
	}

	void DoHIDTransferCallback(MPTR callbackFuncMPTR, MPTR callbackParamMPTR, uint32 hidHandle, uint32 errorCode,
							   MPTR buffer, sint32 length)
	{
		coreinitAsyncCallback_add(callbackFuncMPTR, 5, hidHandle, errorCode, buffer, length, callbackParamMPTR);
	}

	void export_HIDSetIdle(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0);		  // r3
		ppcDefineParamU32(ifIndex, 1);			  // r4
		ppcDefineParamU32(ukn, 2);				  // r5
		ppcDefineParamU32(duration, 3);			  // r6
		ppcDefineParamMPTR(callbackFuncMPTR, 4);  // r7
		ppcDefineParamMPTR(callbackParamMPTR, 5); // r8
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDSetIdle(...)");

		// todo
		if (callbackFuncMPTR)
		{
			DoHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, hidHandle, 0, MPTR_NULL, 0);
		}
		else
		{
			cemu_assert_unimplemented();
		}
		osLib_returnFromFunction(hCPU, 0); // for non-async version, return number of bytes transferred
	}

	void export_HIDSetProtocol(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0);		  // r3
		ppcDefineParamU32(ifIndex, 1);			  // r4
		ppcDefineParamU32(protocol, 2);			  // r5
		ppcDefineParamMPTR(callbackFuncMPTR, 3);  // r6
		ppcDefineParamMPTR(callbackParamMPTR, 4); // r7
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDSetProtocol(...)");

		std::shared_ptr<Device> device = GetDeviceByHandle(hidHandle, true);
		sint32 returnCode = -1;
		if (device)
		{
			if (!device->IsOpened())
			{
				cemuLog_logDebug(LogType::Force, "nsyshid.HIDSetProtocol(): error: device is not opened");
			}
			else
			{
				if (device->SetProtocol(ifIndex, protocol))
				{
					returnCode = 0;
				}
			}
		}
		else
		{
			cemu_assert_suspicious();
		}

		if (callbackFuncMPTR)
		{
			DoHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, hidHandle, 0, MPTR_NULL, 0);
		}
		osLib_returnFromFunction(hCPU, returnCode);
	}

	// handler for async HIDSetReport transfers
	void _hidSetReportAsync(std::shared_ptr<Device> device, uint8* reportData, sint32 length,
							uint8* originalData,
							sint32 originalLength, MPTR callbackFuncMPTR, MPTR callbackParamMPTR)
	{
		cemuLog_logDebug(LogType::Force, "_hidSetReportAsync begin");
		if (device->SetReport(reportData, length, originalData, originalLength))
		{
			DoHIDTransferCallback(callbackFuncMPTR,
								  callbackParamMPTR,
								  device->m_hid->handle,
								  0,
								  memory_getVirtualOffsetFromPointer(originalData),
								  originalLength);
		}
		else
		{
			DoHIDTransferCallback(callbackFuncMPTR,
								  callbackParamMPTR,
								  device->m_hid->handle,
								  -1,
								  memory_getVirtualOffsetFromPointer(originalData),
								  0);
		}
		free(reportData);
	}

	// handler for synchronous HIDSetReport transfers
	sint32 _hidSetReportSync(std::shared_ptr<Device> device, uint8* reportData, sint32 length,
							 uint8* originalData,
							 sint32 originalLength, OSThread_t* osThread)
	{
		_debugPrintHex("_hidSetReportSync Begin", reportData, length);
		sint32 returnCode = 0;
		if (device->SetReport(reportData, length, originalData, originalLength))
		{
			returnCode = originalLength;
		}
		free(reportData);
		cemuLog_logDebug(LogType::Force, "_hidSetReportSync end. returnCode: {}", returnCode);
		coreinit_resumeThread(osThread, 1000);
		return returnCode;
	}

	void export_HIDSetReport(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0);		  // r3
		ppcDefineParamU32(reportRelatedUkn, 1);	  // r4
		ppcDefineParamU32(reportId, 2);			  // r5
		ppcDefineParamUStr(data, 3);			  // r6
		ppcDefineParamU32(dataLength, 4);		  // r7
		ppcDefineParamMPTR(callbackFuncMPTR, 5);  // r8
		ppcDefineParamMPTR(callbackParamMPTR, 6); // r9
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDSetReport({},0x{:02x},0x{:02x},...)", hidHandle, reportRelatedUkn,
						 reportId);

		_debugPrintHex("HIDSetReport", data, dataLength);

#ifdef CEMU_DEBUG_ASSERT
		if (reportRelatedUkn != 2 || reportId != 0)
			assert_dbg();
#endif

		std::shared_ptr<Device> device = GetDeviceByHandle(hidHandle, true);
		if (device == nullptr)
		{
			cemuLog_log(LogType::Force, "nsyshid.HIDSetReport(): Unable to find device with hid handle {}", hidHandle);
			osLib_returnFromFunction(hCPU, -1);
			return;
		}

		// prepare report data
		// note: Currently we need to pad the data to 0x20 bytes for it to work (plus one extra byte for HidD_SetOutputReport)
		// Does IOSU pad data to 0x20 byte? Also check if this is specific to Skylanders portal
		sint32 paddedLength = (dataLength + 0x1F) & ~0x1F;
		uint8* reportData = (uint8*)malloc(paddedLength + 1);
		memset(reportData, 0, paddedLength + 1);
		reportData[0] = 0;
		memcpy(reportData + 1, data, dataLength);

		// issue request (synchronous or asynchronous)
		sint32 returnCode = 0;
		if (callbackFuncMPTR == MPTR_NULL)
		{
			std::future<sint32> res = std::async(std::launch::async, &_hidSetReportSync, device, reportData,
												 paddedLength + 1, data, dataLength,
												 coreinitThread_getCurrentThreadDepr(hCPU));
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(hCPU), 1000);
			PPCCore_switchToScheduler();
			returnCode = res.get();
		}
		else
		{
			// asynchronous
			std::thread(&_hidSetReportAsync, device, reportData, paddedLength + 1, data, dataLength,
						callbackFuncMPTR, callbackParamMPTR)
				.detach();
			returnCode = 0;
		}
		osLib_returnFromFunction(hCPU, returnCode);
	}

	sint32 _hidReadInternalSync(std::shared_ptr<Device> device, uint8* data, sint32 maxLength)
	{
		cemuLog_logDebug(LogType::Force, "HidRead Begin (Length 0x{:08x})", maxLength);
		if (!device->IsOpened())
		{
			cemuLog_logDebug(LogType::Force, "nsyshid.hidReadInternalSync(): cannot read from a non-opened device");
			return -1;
		}
		memset(data, 0, maxLength);

		sint32 bytesRead = 0;
		Device::ReadResult readResult = device->Read(data, maxLength, bytesRead);
		switch (readResult)
		{
		case Device::ReadResult::Success:
		{
			cemuLog_logDebug(LogType::Force, "nsyshid.hidReadInternalSync(): read {} of {} bytes",
							 bytesRead,
							 maxLength);
			return bytesRead;
		}
		break;
		case Device::ReadResult::Error:
		{
			cemuLog_logDebug(LogType::Force, "nsyshid.hidReadInternalSync(): read error");
			return -1;
		}
		break;
		case Device::ReadResult::ErrorTimeout:
		{
			cemuLog_logDebug(LogType::Force, "nsyshid.hidReadInternalSync(): read error: timeout");
			return -108;
		}
		break;
		}
		cemuLog_logDebug(LogType::Force, "nsyshid.hidReadInternalSync(): read error: unknown");
		return -1;
	}

	void _hidReadAsync(std::shared_ptr<Device> device,
					   uint8* data, sint32 maxLength,
					   MPTR callbackFuncMPTR,
					   MPTR callbackParamMPTR)
	{
		sint32 returnCode = _hidReadInternalSync(device, data, maxLength);
		sint32 errorCode = 0;
		if (returnCode < 0)
			errorCode = returnCode; // don't return number of bytes in error code
		DoHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, device->m_hid->handle, errorCode,
							  memory_getVirtualOffsetFromPointer(data), (returnCode > 0) ? returnCode : 0);
	}

	sint32 _hidReadSync(std::shared_ptr<Device> device,
						uint8* data,
						sint32 maxLength,
						OSThread_t* osThread)
	{
		sint32 returnCode = _hidReadInternalSync(device, data, maxLength);
		coreinit_resumeThread(osThread, 1000);
		return returnCode;
	}

	void export_HIDRead(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0);		  // r3
		ppcDefineParamUStr(data, 1);			  // r4
		ppcDefineParamU32(maxLength, 2);		  // r5
		ppcDefineParamMPTR(callbackFuncMPTR, 3);  // r6
		ppcDefineParamMPTR(callbackParamMPTR, 4); // r7
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDRead(0x{:x},0x{:08x},0x{:08x},0x{:08x},0x{:08x})", hCPU->gpr[3],
						 hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7]);

		std::shared_ptr<Device> device = GetDeviceByHandle(hidHandle, true);
		if (device == nullptr)
		{
			cemuLog_log(LogType::Force, "nsyshid.HIDRead(): Unable to find device with hid handle {}", hidHandle);
			osLib_returnFromFunction(hCPU, -1);
			return;
		}
		sint32 returnCode = 0;
		if (callbackFuncMPTR != MPTR_NULL)
		{
			// asynchronous transfer
			std::thread(&_hidReadAsync, device, data, maxLength, callbackFuncMPTR, callbackParamMPTR).detach();
			returnCode = 0;
		}
		else
		{
			// synchronous transfer
			std::future<sint32> res = std::async(std::launch::async, &_hidReadSync, device, data, maxLength,
												 coreinitThread_getCurrentThreadDepr(hCPU));
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(hCPU), 1000);
			PPCCore_switchToScheduler();
			returnCode = res.get();
		}

		osLib_returnFromFunction(hCPU, returnCode);
	}

	sint32 _hidWriteInternalSync(std::shared_ptr<Device> device, uint8* data, sint32 maxLength)
	{
		cemuLog_logDebug(LogType::Force, "HidWrite Begin (Length 0x{:08x})", maxLength);
		if (!device->IsOpened())
		{
			cemuLog_logDebug(LogType::Force, "nsyshid.hidWriteInternalSync(): cannot write to a non-opened device");
			return -1;
		}
		sint32 bytesWritten = 0;
		Device::WriteResult writeResult = device->Write(data, maxLength, bytesWritten);
		switch (writeResult)
		{
		case Device::WriteResult::Success:
		{
			cemuLog_logDebug(LogType::Force, "nsyshid.hidWriteInternalSync(): wrote {} of {} bytes", bytesWritten,
							 maxLength);
			return bytesWritten;
		}
		break;
		case Device::WriteResult::Error:
		{
			cemuLog_logDebug(LogType::Force, "nsyshid.hidWriteInternalSync(): write error");
			return -1;
		}
		break;
		case Device::WriteResult::ErrorTimeout:
		{
			cemuLog_logDebug(LogType::Force, "nsyshid.hidWriteInternalSync(): write error: timeout");
			return -108;
		}
		break;
		}
		cemuLog_logDebug(LogType::Force, "nsyshid.hidWriteInternalSync(): write error: unknown");
		return -1;
	}

	void _hidWriteAsync(std::shared_ptr<Device> device,
						uint8* data,
						sint32 maxLength,
						MPTR callbackFuncMPTR,
						MPTR callbackParamMPTR)
	{
		sint32 returnCode = _hidWriteInternalSync(device, data, maxLength);
		sint32 errorCode = 0;
		if (returnCode < 0)
			errorCode = returnCode; // don't return number of bytes in error code
		DoHIDTransferCallback(callbackFuncMPTR, callbackParamMPTR, device->m_hid->handle, errorCode,
							  memory_getVirtualOffsetFromPointer(data), (returnCode > 0) ? returnCode : 0);
	}

	sint32 _hidWriteSync(std::shared_ptr<Device> device,
						 uint8* data,
						 sint32 maxLength,
						 OSThread_t* osThread)
	{
		sint32 returnCode = _hidWriteInternalSync(device, data, maxLength);
		coreinit_resumeThread(osThread, 1000);
		return returnCode;
	}

	void export_HIDWrite(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(hidHandle, 0);		  // r3
		ppcDefineParamUStr(data, 1);			  // r4
		ppcDefineParamU32(maxLength, 2);		  // r5
		ppcDefineParamMPTR(callbackFuncMPTR, 3);  // r6
		ppcDefineParamMPTR(callbackParamMPTR, 4); // r7
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDWrite(0x{:x},0x{:08x},0x{:08x},0x{:08x},0x{:08x})", hCPU->gpr[3],
						 hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7]);

		std::shared_ptr<Device> device = GetDeviceByHandle(hidHandle, true);
		if (device == nullptr)
		{
			cemuLog_log(LogType::Force, "nsyshid.HIDWrite(): Unable to find device with hid handle {}", hidHandle);
			osLib_returnFromFunction(hCPU, -1);
			return;
		}
		sint32 returnCode = 0;
		if (callbackFuncMPTR != MPTR_NULL)
		{
			// asynchronous transfer
			std::thread(&_hidWriteAsync, device, data, maxLength, callbackFuncMPTR, callbackParamMPTR).detach();
			returnCode = 0;
		}
		else
		{
			// synchronous transfer
			std::future<sint32> res = std::async(std::launch::async, &_hidWriteSync, device, data, maxLength,
												 coreinitThread_getCurrentThreadDepr(hCPU));
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
		cemuLog_logDebug(LogType::Force, "nsyshid.HIDDecodeError(0x{:08x},0x{:08x},0x{:08x})", hCPU->gpr[3],
						 hCPU->gpr[4], hCPU->gpr[5]);

		// todo
		*ukn0 = 0x3FF;
		*ukn1 = (uint32)-0x7FFF;

		osLib_returnFromFunction(hCPU, 0);
	}

	void Backend::DetachAllDevices()
	{
		std::lock_guard<std::recursive_mutex> lock(this->m_devicesMutex);
		if (m_isAttached)
		{
			for (const auto& device : this->m_devices)
			{
				nsyshid::DetachDevice(device);
			}
			this->m_devices.clear();
		}
	}

	bool Backend::AttachDevice(const std::shared_ptr<Device>& device)
	{
		std::lock_guard<std::recursive_mutex> lock(this->m_devicesMutex);
		if (m_isAttached && nsyshid::AttachDevice(device))
		{
			this->m_devices.push_back(device);
			return true;
		}
		return false;
	}

	void Backend::DetachDevice(const std::shared_ptr<Device>& device)
	{
		std::lock_guard<std::recursive_mutex> lock(this->m_devicesMutex);
		if (m_isAttached)
		{
			nsyshid::DetachDevice(device);
			this->m_devices.remove(device);
		}
	}

	std::shared_ptr<Device> Backend::FindDevice(std::function<bool(const std::shared_ptr<Device>&)> isWantedDevice)
	{
		std::lock_guard<std::recursive_mutex> lock(this->m_devicesMutex);
		auto it = std::find_if(this->m_devices.begin(), this->m_devices.end(), std::move(isWantedDevice));
		if (it != this->m_devices.end())
		{
			return *it;
		}
		return nullptr;
	}

	bool Backend::IsDeviceWhitelisted(uint16 vendorId, uint16 productId)
	{
		return Whitelist::GetInstance().IsDeviceWhitelisted(vendorId, productId);
	}

	Backend::Backend()
		: m_isAttached(false)
	{
	}

	void Backend::OnAttach()
	{
		std::lock_guard<std::recursive_mutex> lock(this->m_devicesMutex);
		m_isAttached = true;
		AttachVisibleDevices();
	}

	void Backend::OnDetach()
	{
		std::lock_guard<std::recursive_mutex> lock(this->m_devicesMutex);
		DetachAllDevices();
		m_isAttached = false;
	}

	bool Backend::IsBackendAttached()
	{
		std::lock_guard<std::recursive_mutex> lock(this->m_devicesMutex);
		return m_isAttached;
	}

	Device::Device(uint16 vendorId,
				   uint16 productId,
				   uint8 interfaceIndex,
				   uint8 interfaceSubClass,
				   uint8 protocol)
		: m_hid(nullptr),
		  m_vendorId(vendorId),
		  m_productId(productId),
		  m_interfaceIndex(interfaceIndex),
		  m_interfaceSubClass(interfaceSubClass),
		  m_protocol(protocol),
		  m_maxPacketSizeRX(0x20),
		  m_maxPacketSizeTX(0x20)
	{
	}

	void Device::AssignHID(HID_t* hid)
	{
		if (hid != nullptr)
		{
			hid->vendorId = this->m_vendorId;
			hid->productId = this->m_productId;
			hid->ifIndex = this->m_interfaceIndex;
			hid->subClass = this->m_interfaceSubClass;
			hid->protocol = this->m_protocol;
			hid->ukn04 = 0x11223344;
			hid->paddingGuessed0F = 0;
			hid->maxPacketSizeRX = this->m_maxPacketSizeRX;
			hid->maxPacketSizeTX = this->m_maxPacketSizeTX;
		}
		this->m_hid = hid;
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

		// initialise whitelist
		Whitelist::GetInstance();

		AttachDefaultBackends();
	}
} // namespace nsyshid
