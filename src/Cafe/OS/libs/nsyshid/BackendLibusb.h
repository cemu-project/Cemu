#ifndef CEMU_NSYSHID_BACKEND_LIBUSB_H
#define CEMU_NSYSHID_BACKEND_LIBUSB_H

#include "nsyshid.h"

#if NSYSHID_ENABLE_BACKEND_LIBUSB

#include <libusb-1.0/libusb.h>
#include "Backend.h"

namespace nsyshid::backend::libusb
{
	class BackendLibusb : public nsyshid::Backend {
	  public:
		BackendLibusb();

		~BackendLibusb();

		bool IsInitialisedOk() override;

	  protected:
		void AttachVisibleDevices() override;

	  private:
		libusb_context* m_ctx;
		int m_initReturnCode;
		bool m_callbackRegistered;
		libusb_hotplug_callback_handle m_hotplugCallbackHandle;
		std::thread m_hotplugThread;
		std::atomic<bool> m_hotplugThreadStop;

		// called by libusb
		static int HotplugCallback(libusb_context* ctx, libusb_device* dev,
								   libusb_hotplug_event event, void* user_data);

		int OnHotplug(libusb_device* dev, libusb_hotplug_event event);

		std::shared_ptr<Device> CheckAndCreateDevice(libusb_device* dev);

		std::shared_ptr<Device> FindLibusbDevice(libusb_device* dev);

		bool FindDefaultDeviceEndpoints(libusb_device* dev,
										bool& endpointInFound, uint8& endpointIn, uint16& endpointInMaxPacketSize,
										bool& endpointOutFound, uint8& endpointOut, uint16& endpointOutMaxPacketSize);
	};

	class DeviceLibusb : public nsyshid::Device {
	  public:
		DeviceLibusb(libusb_context* ctx,
					 uint16 vendorId,
					 uint16 productId,
					 uint8 interfaceIndex,
					 uint8 interfaceSubClass,
					 uint8 protocol,
					 uint8 libusbBusNumber,
					 uint8 libusbDeviceAddress);

		~DeviceLibusb() override;

		bool Open() override;

		void Close() override;

		bool IsOpened() override;

		ReadResult Read(ReadMessage* message) override;

		WriteResult Write(WriteMessage* message) override;

		bool GetDescriptor(uint8 descType,
						   uint8 descIndex,
						   uint8 lang,
						   uint8* output,
						   uint32 outputMaxLength) override;

		bool SetProtocol(uint32 ifIndex, uint32 protocol) override;

		bool SetReport(ReportMessage* message) override;

		uint8 m_libusbBusNumber;
		uint8 m_libusbDeviceAddress;
		bool m_libusbHasEndpointIn;
		uint8 m_libusbEndpointIn;
		bool m_libusbHasEndpointOut;
		uint8 m_libusbEndpointOut;

	  private:
		void CloseDevice();

		libusb_context* m_ctx;
		std::mutex m_handleMutex;
		std::atomic<sint32> m_handleInUseCounter;
		std::condition_variable m_handleInUseCounterDecremented;
		libusb_device_handle* m_libusbHandle;

		class HandleLock {
		  public:
			HandleLock() = delete;

			HandleLock(libusb_device_handle** handle,
					   std::mutex& handleMutex,
					   std::atomic<sint32>& handleInUseCounter,
					   std::condition_variable& handleInUseCounterDecremented,
					   DeviceLibusb& device);

			~HandleLock();

			HandleLock(const HandleLock&) = delete;

			HandleLock& operator=(const HandleLock&) = delete;

			bool IsValid();

			libusb_device_handle* GetHandle();

		  private:
			libusb_device_handle* m_handle;
			std::mutex& m_handleMutex;
			std::atomic<sint32>& m_handleInUseCounter;
			std::condition_variable& m_handleInUseCounterDecremented;
		};

		std::unique_ptr<HandleLock> AquireHandleLock();
	};
} // namespace nsyshid::backend::libusb

#endif // NSYSHID_ENABLE_BACKEND_LIBUSB

#endif // CEMU_NSYSHID_BACKEND_LIBUSB_H
