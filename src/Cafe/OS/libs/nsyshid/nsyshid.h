#pragma once

#include <libusb.h>
namespace nsyshid {

typedef struct {
  /* +0x00 */ uint32be handle;
  /* +0x04 */ uint32 ukn04;
  /* +0x08 */ uint16 vendorId;  // little-endian ?
  /* +0x0A */ uint16 productId; // little-endian ?
  /* +0x0C */ uint8 ifIndex;
  /* +0x0D */ uint8 subClass;
  /* +0x0E */ uint8 protocol;
  /* +0x0F */ uint8 paddingGuessed0F;
  /* +0x10 */ uint16be maxPacketSizeRX;
  /* +0x12 */ uint16be maxPacketSizeTX;
} HIDDevice_t;

static_assert(offsetof(HIDDevice_t, vendorId) == 0x8, "");
static_assert(offsetof(HIDDevice_t, productId) == 0xA, "");
static_assert(offsetof(HIDDevice_t, ifIndex) == 0xC, "");
static_assert(offsetof(HIDDevice_t, protocol) == 0xE, "");

typedef struct _HIDClient_t {
  MEMPTR<_HIDClient_t> next;
  uint32be callbackFunc; // attach/detach callback
} HIDClient_t;
class usb_device {
public:
protected:
private:
  uint32 handle;
  uint32 physicalDeviceInstance;
  uint16 vendorId;
  uint16 productId;
  uint8 interfaceIndex;
  uint8 interfaceSubClass;
  uint8 protocol;
  HIDDevice_t
      *hidDevice; // this info is passed to applications and must remain intact
  wchar_t *devicePath;
};

class libusb_device : public usb_device {
public:
protected:
  void send_libusb_transfer(libusb_transfer *transfer);

protected:
  libusb_device *lusb_device = nullptr;
  libusb_device_handle *lusb_handle = nullptr;
};
void load();

} // namespace nsyshid