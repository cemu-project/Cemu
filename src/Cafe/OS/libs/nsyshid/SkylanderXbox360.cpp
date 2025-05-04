#include "SkylanderXbox360.h"

namespace nsyshid
{
	SkylanderXbox360PortalLibusb::SkylanderXbox360PortalLibusb(std::shared_ptr<Device> usbPortal)
	: Device(0x1430, 0x0150, 1, 2, 0)
	{
		m_IsOpened = false;
		m_usbPortal = std::static_pointer_cast<backend::libusb::DeviceLibusb>(usbPortal);
	}

	bool SkylanderXbox360PortalLibusb::Open()
	{
		return m_usbPortal->Open();
	}

	void SkylanderXbox360PortalLibusb::Close()
	{
		return m_usbPortal->Close();
	}

	bool SkylanderXbox360PortalLibusb::IsOpened()
	{
		return m_usbPortal->IsOpened();
	}

	Device::ReadResult SkylanderXbox360PortalLibusb::Read(ReadMessage* message)
	{
		std::vector<uint8> xboxData(std::min<uint32>(32, message->length + sizeof(XBOX_DATA_HEADER)), 0);
		memcpy(xboxData.data(), XBOX_DATA_HEADER, sizeof(XBOX_DATA_HEADER));
		memcpy(xboxData.data() + sizeof(XBOX_DATA_HEADER), message->data, message->length - sizeof(XBOX_DATA_HEADER));

		ReadMessage xboxMessage(xboxData.data(), xboxData.size(), 0);
		auto result = m_usbPortal->Read(&xboxMessage);

		memcpy(message->data, xboxData.data() + sizeof(XBOX_DATA_HEADER), message->length);
		message->bytesRead = xboxMessage.bytesRead;

		return result;
	}

	// Use InterruptTransfer instead of ControlTransfer
	bool SkylanderXbox360PortalLibusb::SetReport(ReportMessage* message)
	{
		if (message->data[0] == 'M' && message->data[1] == 0x01) // Enables Speaker
			g72x_init_state(&m_state);

		std::vector<uint8> xboxData(message->length + sizeof(XBOX_DATA_HEADER), 0);
		memcpy(xboxData.data(), XBOX_DATA_HEADER, sizeof(XBOX_DATA_HEADER));
		memcpy(xboxData.data() + sizeof(XBOX_DATA_HEADER), message->data, message->length);

		WriteMessage xboxMessage(xboxData.data(), xboxData.size(), 0);
		auto result = m_usbPortal->Write(&xboxMessage);

		memcpy(message->data, xboxData.data() + sizeof(XBOX_DATA_HEADER), message->length);

		return result == WriteResult::Success;
	}

	Device::WriteResult SkylanderXbox360PortalLibusb::Write(WriteMessage* message)
	{
		std::vector<uint8> audioData(message->data, message->data + message->length);

		std::vector<uint8_t> xboxAudioData;
		for (size_t i = 0; i < audioData.size(); i += 4)
		{
			int16_t sample1 = (static_cast<int16_t>(audioData[i + 1]) << 8) | audioData[i];
			int16_t sample2 = (static_cast<int16_t>(audioData[i + 3]) << 8) | audioData[i + 2];

			uint8_t encoded1 = g721_encoder(sample1, &m_state) & 0x0F;
			uint8_t encoded2 = g721_encoder(sample2, &m_state) & 0x0F;

			xboxAudioData.push_back((encoded2 << 4) | encoded1);
		}

		std::vector<uint8> xboxData(xboxAudioData.size() + sizeof(XBOX_AUDIO_DATA_HEADER), 0);
		memcpy(xboxData.data(), XBOX_AUDIO_DATA_HEADER, sizeof(XBOX_AUDIO_DATA_HEADER));
		memcpy(xboxData.data() + sizeof(XBOX_AUDIO_DATA_HEADER), xboxAudioData.data(), xboxAudioData.size());

		WriteMessage xboxMessage(xboxData.data(), xboxData.size(), 0);
		auto result = m_usbPortal->Write(&xboxMessage);

		memcpy(message->data, xboxData.data() + sizeof(XBOX_AUDIO_DATA_HEADER), xboxAudioData.size());
		message->bytesWritten = xboxMessage.bytesWritten - sizeof(XBOX_AUDIO_DATA_HEADER);
		return result;
	}

	bool SkylanderXbox360PortalLibusb::GetDescriptor(uint8 descType, uint8 descIndex, uint16 lang, uint8* output, uint32 outputMaxLength)
	{
		uint8 configurationDescriptor[0x29];

		uint8* currentWritePtr;

		// configuration descriptor
		currentWritePtr = configurationDescriptor + 0;
		*(uint8*)(currentWritePtr + 0) = 9;			// bLength
		*(uint8*)(currentWritePtr + 1) = 2;			// bDescriptorType
		*(uint16be*)(currentWritePtr + 2) = 0x0029; // wTotalLength
		*(uint8*)(currentWritePtr + 4) = 1;			// bNumInterfaces
		*(uint8*)(currentWritePtr + 5) = 1;			// bConfigurationValue
		*(uint8*)(currentWritePtr + 6) = 0;			// iConfiguration
		*(uint8*)(currentWritePtr + 7) = 0x80;		// bmAttributes
		*(uint8*)(currentWritePtr + 8) = 0xFA;		// MaxPower
		currentWritePtr = currentWritePtr + 9;
		// interface descriptor
		*(uint8*)(currentWritePtr + 0) = 9;	   // bLength
		*(uint8*)(currentWritePtr + 1) = 0x04; // bDescriptorType
		*(uint8*)(currentWritePtr + 2) = 0;	   // bInterfaceNumber
		*(uint8*)(currentWritePtr + 3) = 0;	   // bAlternateSetting
		*(uint8*)(currentWritePtr + 4) = 2;	   // bNumEndpoints
		*(uint8*)(currentWritePtr + 5) = 3;	   // bInterfaceClass
		*(uint8*)(currentWritePtr + 6) = 0;	   // bInterfaceSubClass
		*(uint8*)(currentWritePtr + 7) = 0;	   // bInterfaceProtocol
		*(uint8*)(currentWritePtr + 8) = 0;	   // iInterface
		currentWritePtr = currentWritePtr + 9;
		// HID descriptor
		*(uint8*)(currentWritePtr + 0) = 9;			// bLength
		*(uint8*)(currentWritePtr + 1) = 0x21;		// bDescriptorType
		*(uint16be*)(currentWritePtr + 2) = 0x0111; // bcdHID
		*(uint8*)(currentWritePtr + 4) = 0x00;		// bCountryCode
		*(uint8*)(currentWritePtr + 5) = 0x01;		// bNumDescriptors
		*(uint8*)(currentWritePtr + 6) = 0x22;		// bDescriptorType
		*(uint16be*)(currentWritePtr + 7) = 0x001D; // wDescriptorLength
		currentWritePtr = currentWritePtr + 9;
		// endpoint descriptor 1
		*(uint8*)(currentWritePtr + 0) = 7;		  // bLength
		*(uint8*)(currentWritePtr + 1) = 0x05;	  // bDescriptorType
		*(uint8*)(currentWritePtr + 2) = 0x81;	  // bEndpointAddress
		*(uint8*)(currentWritePtr + 3) = 0x03;	  // bmAttributes
		*(uint16be*)(currentWritePtr + 4) = 0x0040; // wMaxPacketSize
		*(uint8*)(currentWritePtr + 6) = 0x01;	  // bInterval
		currentWritePtr = currentWritePtr + 7;
		// endpoint descriptor 2
		*(uint8*)(currentWritePtr + 0) = 7;		  // bLength
		*(uint8*)(currentWritePtr + 1) = 0x05;	  // bDescriptorType
		*(uint8*)(currentWritePtr + 2) = 0x02;	  // bEndpointAddress
		*(uint8*)(currentWritePtr + 3) = 0x03;	  // bmAttributes
		*(uint16be*)(currentWritePtr + 4) = 0x0040; // wMaxPacketSize
		*(uint8*)(currentWritePtr + 6) = 0x01;	  // bInterval
		currentWritePtr = currentWritePtr + 7;

		cemu_assert_debug((currentWritePtr - configurationDescriptor) == 0x29);

		memcpy(output, configurationDescriptor,
			std::min<uint32>(outputMaxLength, sizeof(configurationDescriptor)));
		return true;
	}

	bool SkylanderXbox360PortalLibusb::SetIdle(uint8 ifIndex,
		uint8 reportId,
		uint8 duration)
	{
		return true;
	}

	bool SkylanderXbox360PortalLibusb::SetProtocol(uint8 ifIndex, uint8 protocol)
	{
		return true;
	}
}