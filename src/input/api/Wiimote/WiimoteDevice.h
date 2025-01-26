#pragma once

class WiimoteDevice
{
	friend class WiimoteInfo;
public:
	virtual ~WiimoteDevice() = default;

	virtual bool write_data(const std::vector<uint8>& data) = 0;
	virtual std::optional<std::vector<uint8_t>> read_data() = 0;

	virtual bool operator==(const WiimoteDevice& o) const = 0;
};

using WiimoteDevicePtr = std::shared_ptr<WiimoteDevice>;
