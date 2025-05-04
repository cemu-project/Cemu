#pragma once

#include "nsyshid.h"
#include "BackendLibusb.h"
#include "g721/g721.h"

namespace nsyshid
{
	class SkylanderXbox360PortalLibusb final : public Device {
	  public:
		SkylanderXbox360PortalLibusb(std::shared_ptr<Device> usbPortal);
		~SkylanderXbox360PortalLibusb() = default;

		bool Open() override;

		void Close() override;

		bool IsOpened() override;

		ReadResult Read(ReadMessage* message) override;

		WriteResult Write(WriteMessage* message) override;

		bool GetDescriptor(uint8 descType,
			uint8 descIndex,
			uint16 lang,
			uint8* output,
			uint32 outputMaxLength) override;

		bool SetIdle(uint8 ifIndex,
			uint8 reportId,
			uint8 duration) override;

		bool SetProtocol(uint8 ifIndex, uint8 protocol) override;

		bool SetReport(ReportMessage* message) override;
		
	  private:
		std::shared_ptr<backend::libusb::DeviceLibusb> m_usbPortal;
		bool m_IsOpened;
		struct g72x_state m_state;
	};

	constexpr uint8 XBOX_DATA_HEADER[] = { 0x0B, 0x14 };
	constexpr uint8 XBOX_AUDIO_DATA_HEADER[] = { 0x0B, 0x17 };
} // namespace nsyshid