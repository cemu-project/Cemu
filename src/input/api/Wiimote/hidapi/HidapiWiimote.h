#pragma once

#include <api/Wiimote/WiimoteDevice.h>
#include <hidapi.h>

class HidapiWiimote : public WiimoteDevice {
public:
    HidapiWiimote(fs::path const& device_path, uint64_t identifier);
    ~HidapiWiimote() override;

    bool write_data(const std::vector<uint8> &data) override;
    std::optional<std::vector<uint8>> read_data() override;
    bool operator==(WiimoteDevice& o) const override;

    static std::vector<WiimoteDevicePtr> get_devices();

private:
    hid_device* m_handle;
    uint64_t m_identifier;

};

using WiimoteDevice_t = HidapiWiimote;