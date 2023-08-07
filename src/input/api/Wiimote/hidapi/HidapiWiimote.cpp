#include "HidapiWiimote.h"

HidapiWiimote::HidapiWiimote(fs::path const& device_path, uint64_t identifier)
 : m_handle(hid_open_path(_pathToUtf8(device_path).c_str())), m_identifier(identifier) {

}

bool HidapiWiimote::write_data(const std::vector<uint8> &data) {
    return hid_write(m_handle, data.data(), data.size()) >= 0;
}

std::optional<std::vector<uint8>> HidapiWiimote::read_data() {
    std::array<uint8_t, 21> read_data{};
    const auto result = hid_read(m_handle, read_data.data(), 20);
    if (result < 0)
        return {};
    return {{read_data.cbegin(), read_data.cend()}};
}

std::vector<WiimoteDevicePtr> HidapiWiimote::get_devices() {
    std::vector<WiimoteDevicePtr> wiimote_devices;
    hid_init();
    const auto device_enumeration = hid_enumerate(0x057e, 0x0306);
    auto it = device_enumeration;
    while (it){
        // Enough to have a unique id for each device within a session
        uint64_t id = (static_cast<uint64>(it->interface_number) << 32) |
                (static_cast<uint64>(it->usage_page) << 16) |
                (it->usage);
        wiimote_devices.push_back(std::make_shared<HidapiWiimote>(it->path, id));
        it = it->next;
    }
    hid_free_enumeration(device_enumeration);
    return wiimote_devices;
}

bool HidapiWiimote::operator==(WiimoteDevice& o) const  {
    return m_identifier == dynamic_cast<HidapiWiimote&>(o).m_identifier;
}

HidapiWiimote::~HidapiWiimote() {
    hid_close(m_handle);
}
