#include "HidapiWiimote.h"

static constexpr uint16 WIIMOTE_VENDOR_ID = 0x057e;
static constexpr uint16 WIIMOTE_PRODUCT_ID = 0x0306;
static constexpr uint16 WIIMOTE_MP_PRODUCT_ID = 0x0330;
static constexpr uint16 WIIMOTE_MAX_INPUT_REPORT_LENGTH = 22;

HidapiWiimote::HidapiWiimote(hid_device* dev, uint64_t identifier, std::string_view path)
 : m_handle(dev), m_identifier(identifier), m_path(path) {

}

bool HidapiWiimote::write_data(const std::vector<uint8> &data) {
    return hid_write(m_handle, data.data(), data.size()) >= 0;
}

std::optional<std::vector<uint8>> HidapiWiimote::read_data() {
    std::array<uint8, WIIMOTE_MAX_INPUT_REPORT_LENGTH> read_data{};
    const auto result = hid_read(m_handle, read_data.data(), WIIMOTE_MAX_INPUT_REPORT_LENGTH);
    if (result < 0)
        return {};
    return {{read_data.cbegin(), read_data.cbegin() + result}};
}

std::vector<WiimoteDevicePtr> HidapiWiimote::get_devices() {
    std::vector<WiimoteDevicePtr> wiimote_devices;
    hid_init();
    const auto device_enumeration = hid_enumerate(WIIMOTE_VENDOR_ID, 0x0);

    for (auto it = device_enumeration; it != nullptr; it = it->next){
        if (it->product_id != WIIMOTE_PRODUCT_ID && it->product_id != WIIMOTE_MP_PRODUCT_ID)
            continue;
        auto dev = hid_open_path(it->path);
        if (!dev){
            cemuLog_logDebug(LogType::Force, "Unable to open Wiimote device at {}: {}", it->path, boost::nowide::narrow(hid_error(nullptr)));
        }
        else {
            hid_set_nonblocking(dev, true);
            // Enough to have a unique id for each device within a session
            uint64_t id = (static_cast<uint64>(it->interface_number) << 32) |
                          (static_cast<uint64>(it->usage_page) << 16) |
                          (it->usage);
            wiimote_devices.push_back(std::make_shared<HidapiWiimote>(dev, id, it->path));
        }
    }
    hid_free_enumeration(device_enumeration);
    return wiimote_devices;
}

bool HidapiWiimote::operator==(WiimoteDevice& o) const  {
    auto const& other_mote = static_cast<HidapiWiimote const&>(o);
    return m_identifier == other_mote.m_identifier && other_mote.m_path == m_path;
}

HidapiWiimote::~HidapiWiimote() {
    hid_close(m_handle);
}
