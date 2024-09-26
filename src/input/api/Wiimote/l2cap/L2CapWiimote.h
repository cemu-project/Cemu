#pragma once
#include <api/Wiimote/WiimoteDevice.h>
#include <bluetooth/bluetooth.h>

class L2CapWiimote : public WiimoteDevice
{
  public:
	L2CapWiimote(int recvFd, int sendFd);
	~L2CapWiimote() override;

	bool write_data(const std::vector<uint8>& data) override;
	std::optional<std::vector<uint8>> read_data() override;
	bool operator==(WiimoteDevice& o) const override;

	static std::vector<WiimoteDevicePtr> get_devices();
  private:
	int m_recvFd;
	int m_sendFd;
};

