#ifndef CEMU_NSYSHID_BACKEND_WINDOWS_HID_H
#define CEMU_NSYSHID_BACKEND_WINDOWS_HID_H

#include "nsyshid.h"

#if NSYSHID_ENABLE_BACKEND_WINDOWS_HID

#include "Backend.h"

namespace nsyshid::backend::windows
{
	class BackendWindowsHID : public nsyshid::Backend {
	  public:
		BackendWindowsHID();

		~BackendWindowsHID();

		bool IsInitialisedOk() override;

	  protected:
		void AttachVisibleDevices() override;

	  private:
		std::shared_ptr<Device> CheckAndCreateDevice(wchar_t* devicePath, HANDLE hDevice);
	};

	class DeviceWindowsHID : public nsyshid::Device {
	  public:
		DeviceWindowsHID(uint16 vendorId,
						 uint16 productId,
						 uint8 interfaceIndex,
						 uint8 interfaceSubClass,
						 uint8 protocol,
						 wchar_t* devicePath);

		~DeviceWindowsHID();

		bool Open() override;

		void Close() override;

		bool IsOpened() override;

		ReadResult Read(ReadMessage* message) override;

		WriteResult Write(WriteMessage* message) override;

		bool GetDescriptor(uint8 descType, uint8 descIndex, uint8 lang, uint8* output, uint32 outputMaxLength) override;

		bool SetProtocol(uint32 ifIndef, uint32 protocol) override;

		bool SetReport(ReportMessage* message) override;

	  private:
		wchar_t* m_devicePath;
		HANDLE m_hFile;
	};

	HANDLE OpenDevice(wchar_t* devicePath);

	void _debugPrintHex(std::string prefix, uint8* data, size_t len);
} // namespace nsyshid::backend::windows

#endif // NSYSHID_ENABLE_BACKEND_WINDOWS_HID

#endif // CEMU_NSYSHID_BACKEND_WINDOWS_HID_H
