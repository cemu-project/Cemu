#include "input/api/Wiimote/windows/WinWiimoteDevice.h"

#include <hidsdi.h>
#include <SetupAPI.h>

#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "hid.lib")

WinWiimoteDevice::WinWiimoteDevice(HANDLE handle, std::vector<uint8_t> identifier)
	: m_handle(handle), m_identifier(std::move(identifier))
{
	m_overlapped.hEvent = CreateEvent(nullptr, TRUE, TRUE, nullptr);
}

WinWiimoteDevice::~WinWiimoteDevice()
{
	CancelIo(m_handle);
	ResetEvent(m_overlapped.hEvent);
	CloseHandle(m_handle);
}

bool WinWiimoteDevice::write_data(const std::vector<uint8>& data)
{
	return HidD_SetOutputReport(m_handle, (void*)data.data(), (ULONG)data.size());
}

std::optional<std::vector<uint8_t>> WinWiimoteDevice::read_data()
{
	DWORD read = 0;
	std::array<uint8_t, 32> buffer{};

	if (!ReadFile(m_handle, buffer.data(), (DWORD)buffer.size(), &read, &m_overlapped))
	{
		const auto error = GetLastError();
		if (error == ERROR_DEVICE_NOT_CONNECTED)
			return {};
		else if (error == ERROR_IO_PENDING)
		{
			const auto wait_result = WaitForSingleObject(m_overlapped.hEvent, 100);
			if (wait_result == WAIT_TIMEOUT)
			{
				CancelIo(m_handle);
				ResetEvent(m_overlapped.hEvent);
				return {};
			}
			else if (wait_result == WAIT_FAILED)
				return {};

			if (GetOverlappedResult(m_handle, &m_overlapped, &read, FALSE) == FALSE)
				return {};
		}
		else if (error == ERROR_INVALID_HANDLE)
		{
			ResetEvent(m_overlapped.hEvent);
			return {};
		}
		else
		{
			cemu_assert_debug(false);
		}
	}

	ResetEvent(m_overlapped.hEvent);
	if (read == 0)
		return {};

	return {{buffer.cbegin(), buffer.cbegin() + read}};
}

std::vector<WiimoteDevicePtr> WinWiimoteDevice::get_devices()
{
	std::vector<WiimoteDevicePtr> result;

	GUID hid_guid;
	HidD_GetHidGuid(&hid_guid);

	const auto device_info = SetupDiGetClassDevs(&hid_guid, nullptr, nullptr, (DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));

	for (DWORD index = 0; ; ++index)
	{
		SP_DEVICE_INTERFACE_DATA device_data{};
		device_data.cbSize = sizeof(device_data);
		if (SetupDiEnumDeviceInterfaces(device_info, nullptr, &hid_guid, index, &device_data) == FALSE)
			break;

		DWORD device_data_len;
		if (SetupDiGetDeviceInterfaceDetail(device_info, &device_data, nullptr, 0, &device_data_len, nullptr) == FALSE
			&& GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			continue;

		std::vector<uint8_t> detail_data_buffer;
		detail_data_buffer.resize(device_data_len);

		const auto detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA)detail_data_buffer.data();
		detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if (SetupDiGetDeviceInterfaceDetail(device_info, &device_data, detail_data, device_data_len, nullptr, nullptr)
			== FALSE)
			continue;

		HANDLE device_handle = CreateFile(detail_data->DevicePath, (GENERIC_READ | GENERIC_WRITE),
		                                  (FILE_SHARE_READ | FILE_SHARE_WRITE), nullptr, OPEN_EXISTING,
		                                  FILE_FLAG_OVERLAPPED, nullptr);
		if (device_handle == INVALID_HANDLE_VALUE)
			continue;

		HIDD_ATTRIBUTES attributes{};
		attributes.Size = sizeof(attributes);
		if (HidD_GetAttributes(device_handle, &attributes) == FALSE)
		{
			CloseHandle(device_handle);
			continue;
		}

		if (attributes.VendorID != 0x057e || (attributes.ProductID != 0x0306 && attributes.ProductID != 0x0330))
		{
			CloseHandle(device_handle);
			continue;
		}

		result.emplace_back(std::make_shared<WinWiimoteDevice>(device_handle, detail_data_buffer));
	}

	return result;
}

bool WinWiimoteDevice::operator==(WiimoteDevice& o) const
{
	return m_identifier == static_cast<WinWiimoteDevice&>(o).m_identifier;
}
