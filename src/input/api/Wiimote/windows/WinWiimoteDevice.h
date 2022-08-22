#pragma once

#include "input/api/Wiimote/WiimoteDevice.h"

class WinWiimoteDevice : public WiimoteDevice
{
public:
	WinWiimoteDevice(HANDLE handle, std::vector<uint8_t> identifier);
	~WinWiimoteDevice();

	bool write_data(const std::vector<uint8>& data) override;
	std::optional<std::vector<uint8_t>> read_data() override;

	static std::vector<WiimoteDevicePtr> get_devices();

	bool operator==(WiimoteDevice& o) const override;

private:
	HANDLE m_handle;
	OVERLAPPED m_overlapped{};
	std::vector<uint8_t> m_identifier;
};

using WiimoteDevice_t = WinWiimoteDevice;
