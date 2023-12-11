#include "HidapiWiimote.h"
#include <cwchar>

static constexpr uint16 WIIMOTE_VENDOR_ID = 0x057e;
static constexpr uint16 WIIMOTE_PRODUCT_ID = 0x0306;
static constexpr uint16 WIIMOTE_MP_PRODUCT_ID = 0x0330;
static constexpr uint16 WIIMOTE_MAX_INPUT_REPORT_LENGTH = 22;
static constexpr auto PRO_CONTROLLER_NAME = L"Nintendo RVL-CNT-01-UC";

HidapiWiimote::HidapiWiimote(hid_device* dev, std::string_view path)
 : m_handle(dev), m_path(path) {

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
        if (std::wcscmp(it->product_string, PRO_CONTROLLER_NAME) == 0)
            continue;
        auto dev = hid_open_path(it->path);
        if (!dev){
            cemuLog_logDebug(LogType::Force, "Unable to open Wiimote device at {}: {}", it->path, boost::nowide::narrow(hid_error(nullptr)));
        }
        else {
            hid_set_nonblocking(dev, true);
            wiimote_devices.push_back(std::make_shared<HidapiWiimote>(dev, it->path));
        }
    }
    hid_free_enumeration(device_enumeration);
    return wiimote_devices;
}

bool HidapiWiimote::operator==(WiimoteDevice& o) const  {
    return static_cast<HidapiWiimote const&>(o).m_path == m_path;
}

HidapiWiimote::~HidapiWiimote() {
    hid_close(m_handle);
}
