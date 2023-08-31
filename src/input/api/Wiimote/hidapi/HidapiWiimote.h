#pragma once

#include <api/Wiimote/WiimoteDevice.h>
#include <hidapi.h>

class HidapiWiimote : public WiimoteDevice {
public:
    HidapiWiimote(hid_device* dev, uint64_t identifier, std::string_view path);
    ~HidapiWiimote() override;

    bool write_data(const std::vector<uint8> &data) override;
    std::optional<std::vector<uint8>> read_data() override;
    bool operator==(WiimoteDevice& o) const override;

    static std::vector<WiimoteDevicePtr> get_devices();

private:
    hid_device* m_handle;
    const uint64_t m_identifier;
    const std::string m_path;

};

using WiimoteDevice_t = HidapiWiimote;