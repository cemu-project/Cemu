#ifndef CEMU_NSYSHID_BACKEND_H
#define CEMU_NSYSHID_BACKEND_H

#include <list>
#include <memory>
#include <mutex>

#include "Common/precompiled.h"

namespace nsyshid
{
	typedef struct
	{
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
	} HID_t;

	static_assert(offsetof(HID_t, vendorId) == 0x8, "");
	static_assert(offsetof(HID_t, productId) == 0xA, "");
	static_assert(offsetof(HID_t, ifIndex) == 0xC, "");
	static_assert(offsetof(HID_t, protocol) == 0xE, "");

	class Device {
	  public:
		Device() = delete;

		Device(uint16 vendorId,
			   uint16 productId,
			   uint8 interfaceIndex,
			   uint8 interfaceSubClass,
			   uint8 protocol);

		Device(const Device& device) = delete;

		Device& operator=(const Device& device) = delete;

		virtual ~Device() = default;

		HID_t* m_hid; // this info is passed to applications and must remain intact

		uint16 m_vendorId;
		uint16 m_productId;
		uint8 m_interfaceIndex;
		uint8 m_interfaceSubClass;
		uint8 m_protocol;
		uint16 m_maxPacketSizeRX;
		uint16 m_maxPacketSizeTX;

		virtual void AssignHID(HID_t* hid);

		virtual bool Open() = 0;

		virtual void Close() = 0;

		virtual bool IsOpened() = 0;

		enum class ReadResult
		{
			Success,
			Error,
			ErrorTimeout,
		};

		virtual ReadResult Read(uint8* data, sint32 length, sint32& bytesRead) = 0;

		enum class WriteResult
		{
			Success,
			Error,
			ErrorTimeout,
		};

		virtual WriteResult Write(uint8* data, sint32 length, sint32& bytesWritten) = 0;

		virtual bool GetDescriptor(uint8 descType,
								   uint8 descIndex,
								   uint8 lang,
								   uint8* output,
								   uint32 outputMaxLength) = 0;

		virtual bool SetProtocol(uint32 ifIndef, uint32 protocol) = 0;

		virtual bool SetReport(uint8* reportData, sint32 length, uint8* originalData, sint32 originalLength) = 0;
	};

	class Backend {
	  public:
		Backend();

		Backend(const Backend& backend) = delete;

		Backend& operator=(const Backend& backend) = delete;

		virtual ~Backend() = default;

		void DetachAllDevices();

		// called from nsyshid when this backend is attached - do not call this yourself!
		void OnAttach();

		// called from nsyshid when this backend is detached - do not call this yourself!
		void OnDetach();

		bool IsBackendAttached();

		virtual bool IsInitialisedOk() = 0;

	  protected:
		// try to attach a device - only works if this backend is attached
		bool AttachDevice(const std::shared_ptr<Device>& device);

		void DetachDevice(const std::shared_ptr<Device>& device);

		std::shared_ptr<Device> FindDevice(std::function<bool(const std::shared_ptr<Device>&)> isWantedDevice);

		bool IsDeviceWhitelisted(uint16 vendorId, uint16 productId);

		// called from OnAttach() - attach devices that your backend can see here
		virtual void AttachVisibleDevices() = 0;

	  private:
		std::list<std::shared_ptr<Device>> m_devices;
		std::recursive_mutex m_devicesMutex;
		bool m_isAttached;
	};

	namespace backend
	{
		void AttachDefaultBackends();
	}
} // namespace nsyshid

#endif // CEMU_NSYSHID_BACKEND_H
