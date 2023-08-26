#include "BackendWindowsHID.h"

#if NSYSHID_ENABLE_BACKEND_WINDOWS_HID

#include <setupapi.h>
#include <initguid.h>
#include <hidsdi.h>

#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "hid.lib")

DEFINE_GUID(GUID_DEVINTERFACE_HID,
			0x4D1E55B2L, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);

namespace nsyshid::backend::windows
{
	BackendWindowsHID::BackendWindowsHID()
	{
	}

	void BackendWindowsHID::AttachVisibleDevices()
	{
		// add all currently connected devices
		HDEVINFO hDevInfo;
		SP_DEVICE_INTERFACE_DATA DevIntfData;
		PSP_DEVICE_INTERFACE_DETAIL_DATA DevIntfDetailData;
		SP_DEVINFO_DATA DevData;

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

				DevIntfDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
																				dwSize);
				DevIntfDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

				if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &DevIntfData,
													DevIntfDetailData, dwSize, &dwSize, &DevData))
				{
					HANDLE hHIDDevice = OpenDevice(DevIntfDetailData->DevicePath);
					if (hHIDDevice != INVALID_HANDLE_VALUE)
					{
						auto device = CheckAndCreateDevice(DevIntfDetailData->DevicePath, hHIDDevice);
						if (device != nullptr)
						{
							if (IsDeviceWhitelisted(device->m_vendorId, device->m_productId))
							{
								if (!AttachDevice(device))
								{
									cemuLog_log(LogType::Force,
												"nsyshid::BackendWindowsHID: failed to attach device: {:04x}:{:04x}",
												device->m_vendorId,
												device->m_productId);
								}
							}
							else
							{
								cemuLog_log(LogType::Force,
											"nsyshid::BackendWindowsHID: device not on whitelist: {:04x}:{:04x}",
											device->m_vendorId,
											device->m_productId);
							}
						}
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

	BackendWindowsHID::~BackendWindowsHID()
	{
	}

	bool BackendWindowsHID::IsInitialisedOk()
	{
		return true;
	}

	std::shared_ptr<Device> BackendWindowsHID::CheckAndCreateDevice(wchar_t* devicePath, HANDLE hDevice)
	{
		HIDD_ATTRIBUTES hidAttr;
		hidAttr.Size = sizeof(HIDD_ATTRIBUTES);
		if (HidD_GetAttributes(hDevice, &hidAttr) == FALSE)
			return nullptr;

		auto device = std::make_shared<DeviceWindowsHID>(hidAttr.VendorID,
														 hidAttr.ProductID,
														 1,
														 2,
														 0,
														 _wcsdup(devicePath));
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
			cemuLog_log(LogType::Force, "HID: Input packet length not available or out of range (length = {})",
						maxPacketInputLength);
			maxPacketInputLength = 0x20;
		}
		if (maxPacketOutputLength <= 0 || maxPacketOutputLength >= 0xF000)
		{
			cemuLog_log(LogType::Force, "HID: Output packet length not available or out of range (length = {})",
						maxPacketOutputLength);
			maxPacketOutputLength = 0x20;
		}

		device->m_maxPacketSizeRX = maxPacketInputLength;
		device->m_maxPacketSizeTX = maxPacketOutputLength;

		return device;
	}

	DeviceWindowsHID::DeviceWindowsHID(uint16 vendorId,
									   uint16 productId,
									   uint8 interfaceIndex,
									   uint8 interfaceSubClass,
									   uint8 protocol,
									   wchar_t* devicePath)
		: Device(vendorId,
				 productId,
				 interfaceIndex,
				 interfaceSubClass,
				 protocol),
		  m_devicePath(devicePath),
		  m_hFile(INVALID_HANDLE_VALUE)
	{
	}

	DeviceWindowsHID::~DeviceWindowsHID()
	{
		if (m_hFile != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_hFile);
			m_hFile = INVALID_HANDLE_VALUE;
		}
	}

	bool DeviceWindowsHID::Open()
	{
		if (IsOpened())
		{
			return true;
		}
		m_hFile = OpenDevice(m_devicePath);
		if (m_hFile == INVALID_HANDLE_VALUE)
		{
			return false;
		}
		HidD_SetNumInputBuffers(m_hFile, 2); // don't cache too many reports
		return true;
	}

	void DeviceWindowsHID::Close()
	{
		if (m_hFile != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_hFile);
			m_hFile = INVALID_HANDLE_VALUE;
		}
	}

	bool DeviceWindowsHID::IsOpened()
	{
		return m_hFile != INVALID_HANDLE_VALUE;
	}

	Device::ReadResult DeviceWindowsHID::Read(uint8* data, sint32 length, sint32& bytesRead)
	{
		bytesRead = 0;
		DWORD bt;
		OVERLAPPED ovlp = {0};
		ovlp.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		uint8* tempBuffer = (uint8*)malloc(length + 1);
		sint32 transferLength = 0; // minus report byte

		_debugPrintHex("HID_READ_BEFORE", data, length);

		cemuLog_logDebug(LogType::Force, "HidRead Begin (Length 0x{:08x})", length);
		BOOL readResult = ReadFile(this->m_hFile, tempBuffer, length + 1, &bt, &ovlp);
		if (readResult != FALSE)
		{
			// sometimes we get the result immediately
			if (bt == 0)
				transferLength = 0;
			else
				transferLength = bt - 1;
			cemuLog_logDebug(LogType::Force, "HidRead Result received immediately (error 0x{:08x}) Length 0x{:08x}",
							 GetLastError(), transferLength);
		}
		else
		{
			// wait for result
			cemuLog_logDebug(LogType::Force, "HidRead WaitForResult (error 0x{:08x})", GetLastError());
			// async hid read is never supposed to return unless there is a response? Lego Dimensions stops HIDRead calls as soon as one of them fails with a non-zero error (which includes time out)
			DWORD r = WaitForSingleObject(ovlp.hEvent, 2000 * 100);
			if (r == WAIT_TIMEOUT)
			{
				cemuLog_logDebug(LogType::Force, "HidRead internal timeout (error 0x{:08x})", GetLastError());
				// return -108 in case of timeout
				free(tempBuffer);
				CloseHandle(ovlp.hEvent);
				return ReadResult::ErrorTimeout;
			}

			cemuLog_logDebug(LogType::Force, "HidRead WaitHalfComplete");
			GetOverlappedResult(this->m_hFile, &ovlp, &bt, false);
			if (bt == 0)
				transferLength = 0;
			else
				transferLength = bt - 1;
			cemuLog_logDebug(LogType::Force, "HidRead WaitComplete Length: 0x{:08x}", transferLength);
		}
		sint32 returnCode = 0;
		ReadResult result = ReadResult::Success;
		if (bt != 0)
		{
			memcpy(data, tempBuffer + 1, transferLength);
			sint32 hidReadLength = transferLength;

			char debugOutput[1024] = {0};
			for (sint32 i = 0; i < transferLength; i++)
			{
				sprintf(debugOutput + i * 3, "%02x ", tempBuffer[1 + i]);
			}
			cemuLog_logDebug(LogType::Force, "HIDRead data: {}", debugOutput);

			bytesRead = transferLength;
			result = ReadResult::Success;
		}
		else
		{
			cemuLog_log(LogType::Force, "Failed HID read");
			result = ReadResult::Error;
		}
		free(tempBuffer);
		CloseHandle(ovlp.hEvent);
		return result;
	}

	Device::WriteResult DeviceWindowsHID::Write(uint8* data, sint32 length, sint32& bytesWritten)
	{
		bytesWritten = 0;
		DWORD bt;
		OVERLAPPED ovlp = {0};
		ovlp.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		uint8* tempBuffer = (uint8*)malloc(length + 1);
		memcpy(tempBuffer + 1, data, length);
		tempBuffer[0] = 0; // report byte?

		cemuLog_logDebug(LogType::Force, "HidWrite Begin (Length 0x{:08x})", length);
		BOOL writeResult = WriteFile(this->m_hFile, tempBuffer, length + 1, &bt, &ovlp);
		if (writeResult != FALSE)
		{
			// sometimes we get the result immediately
			cemuLog_logDebug(LogType::Force, "HidWrite Result received immediately (error 0x{:08x}) Length 0x{:08x}",
							 GetLastError());
		}
		else
		{
			// wait for result
			cemuLog_logDebug(LogType::Force, "HidWrite WaitForResult (error 0x{:08x})", GetLastError());
			// todo - check for error type
			DWORD r = WaitForSingleObject(ovlp.hEvent, 2000);
			if (r == WAIT_TIMEOUT)
			{
				cemuLog_logDebug(LogType::Force, "HidWrite internal timeout");
				// return -108 in case of timeout
				free(tempBuffer);
				CloseHandle(ovlp.hEvent);
				return WriteResult::ErrorTimeout;
			}

			cemuLog_logDebug(LogType::Force, "HidWrite WaitHalfComplete");
			GetOverlappedResult(this->m_hFile, &ovlp, &bt, false);
			cemuLog_logDebug(LogType::Force, "HidWrite WaitComplete");
		}

		free(tempBuffer);
		CloseHandle(ovlp.hEvent);

		if (bt != 0)
		{
			bytesWritten = length;
			return WriteResult::Success;
		}
		return WriteResult::Error;
	}

	bool DeviceWindowsHID::GetDescriptor(uint8 descType,
										 uint8 descIndex,
										 uint8 lang,
										 uint8* output,
										 uint32 outputMaxLength)
	{
		if (!IsOpened())
		{
			cemuLog_logDebug(LogType::Force, "nsyshid::DeviceWindowsHID::getDescriptor(): device is not opened");
			return false;
		}
		if (descType == 0x02)
		{
			uint8 configurationDescriptor[0x29];

			uint8* currentWritePtr;

			// configuration descriptor
			currentWritePtr = configurationDescriptor + 0;
			*(uint8*)(currentWritePtr + 0) = 9;			// bLength
			*(uint8*)(currentWritePtr + 1) = 2;			// bDescriptorType
			*(uint16be*)(currentWritePtr + 2) = 0x0029; // wTotalLength
			*(uint8*)(currentWritePtr + 4) = 1;			// bNumInterfaces
			*(uint8*)(currentWritePtr + 5) = 1;			// bConfigurationValue
			*(uint8*)(currentWritePtr + 6) = 0;			// iConfiguration
			*(uint8*)(currentWritePtr + 7) = 0x80;		// bmAttributes
			*(uint8*)(currentWritePtr + 8) = 0xFA;		// MaxPower
			currentWritePtr = currentWritePtr + 9;
			// configuration descriptor
			*(uint8*)(currentWritePtr + 0) = 9;	   // bLength
			*(uint8*)(currentWritePtr + 1) = 0x04; // bDescriptorType
			*(uint8*)(currentWritePtr + 2) = 0;	   // bInterfaceNumber
			*(uint8*)(currentWritePtr + 3) = 0;	   // bAlternateSetting
			*(uint8*)(currentWritePtr + 4) = 2;	   // bNumEndpoints
			*(uint8*)(currentWritePtr + 5) = 3;	   // bInterfaceClass
			*(uint8*)(currentWritePtr + 6) = 0;	   // bInterfaceSubClass
			*(uint8*)(currentWritePtr + 7) = 0;	   // bInterfaceProtocol
			*(uint8*)(currentWritePtr + 8) = 0;	   // iInterface
			currentWritePtr = currentWritePtr + 9;
			// configuration descriptor
			*(uint8*)(currentWritePtr + 0) = 9;			// bLength
			*(uint8*)(currentWritePtr + 1) = 0x21;		// bDescriptorType
			*(uint16be*)(currentWritePtr + 2) = 0x0111; // bcdHID
			*(uint8*)(currentWritePtr + 4) = 0x00;		// bCountryCode
			*(uint8*)(currentWritePtr + 5) = 0x01;		// bNumDescriptors
			*(uint8*)(currentWritePtr + 6) = 0x22;		// bDescriptorType
			*(uint16be*)(currentWritePtr + 7) = 0x001D; // wDescriptorLength
			currentWritePtr = currentWritePtr + 9;
			// endpoint descriptor 1
			*(uint8*)(currentWritePtr + 0) = 7;	   // bLength
			*(uint8*)(currentWritePtr + 1) = 0x05; // bDescriptorType
			*(uint8*)(currentWritePtr + 2) = 0x81; // bEndpointAddress
			*(uint8*)(currentWritePtr + 3) = 0x03; // bmAttributes
			*(uint16be*)(currentWritePtr + 4) =
				this->m_maxPacketSizeRX;		   // wMaxPacketSize
			*(uint8*)(currentWritePtr + 6) = 0x01; // bInterval
			currentWritePtr = currentWritePtr + 7;
			// endpoint descriptor 2
			*(uint8*)(currentWritePtr + 0) = 7;	   // bLength
			*(uint8*)(currentWritePtr + 1) = 0x05; // bDescriptorType
			*(uint8*)(currentWritePtr + 2) = 0x02; // bEndpointAddress
			*(uint8*)(currentWritePtr + 3) = 0x03; // bmAttributes
			*(uint16be*)(currentWritePtr + 4) =
				this->m_maxPacketSizeTX;		   // wMaxPacketSize
			*(uint8*)(currentWritePtr + 6) = 0x01; // bInterval
			currentWritePtr = currentWritePtr + 7;

			cemu_assert_debug((currentWritePtr - configurationDescriptor) == 0x29);

			memcpy(output, configurationDescriptor,
				   std::min<uint32>(outputMaxLength, sizeof(configurationDescriptor)));
			return true;
		}
		else
		{
			cemu_assert_unimplemented();
		}
		return false;
	}

	bool DeviceWindowsHID::SetProtocol(uint32 ifIndef, uint32 protocol)
	{
		// ToDo: implement this
		// pretend that everything is fine
		return true;
	}

	bool DeviceWindowsHID::SetReport(uint8* reportData, sint32 length, uint8* originalData, sint32 originalLength)
	{
		sint32 retryCount = 0;
		while (true)
		{
			BOOL r = HidD_SetOutputReport(this->m_hFile, reportData, length);
			if (r != FALSE)
				break;
			Sleep(20); // retry
			retryCount++;
			if (retryCount >= 50)
			{
				cemuLog_log(LogType::Force, "nsyshid::DeviceWindowsHID::SetReport(): HID SetReport failed");
				return false;
			}
		}
		return true;
	}

	HANDLE OpenDevice(wchar_t* devicePath)
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
} // namespace nsyshid::backend::windows

#endif // NSYSHID_ENABLE_BACKEND_WINDOWS_HID
