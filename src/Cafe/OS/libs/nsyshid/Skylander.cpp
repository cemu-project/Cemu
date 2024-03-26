#include "Skylander.h"

#include "nsyshid.h"
#include "Backend.h"

namespace nsyshid
{
	SkylanderUSB g_skyportal;

	SkylanderPortalDevice::SkylanderPortalDevice()
		: Device(0x1430, 0x0150, 1, 2, 0)
	{
		m_IsOpened = false;
	}

	bool SkylanderPortalDevice::Open()
	{
		if (!IsOpened())
		{
			m_IsOpened = true;
		}
		return true;
	}

	void SkylanderPortalDevice::Close()
	{
		if (IsOpened())
		{
			m_IsOpened = false;
		}
	}

	bool SkylanderPortalDevice::IsOpened()
	{
		return m_IsOpened;
	}

	Device::ReadResult SkylanderPortalDevice::Read(ReadMessage* message)
	{
		memcpy(message->data, g_skyportal.get_status().data(), message->length);
		message->bytesRead = message->length;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return Device::ReadResult::Success;
	}

	Device::WriteResult SkylanderPortalDevice::Write(WriteMessage* message)
	{
		message->bytesWritten = message->length;
		return Device::WriteResult::Success;
	}

	bool SkylanderPortalDevice::GetDescriptor(uint8 descType,
											  uint8 descIndex,
											  uint8 lang,
											  uint8* output,
											  uint32 outputMaxLength)
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
		// configuration descriptor
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
		// configuration descriptor
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
		*(uint16be*)(currentWritePtr + 4) = 0x40; // wMaxPacketSize
		*(uint8*)(currentWritePtr + 6) = 0x01;	  // bInterval
		currentWritePtr = currentWritePtr + 7;
		// endpoint descriptor 2
		*(uint8*)(currentWritePtr + 0) = 7;		  // bLength
		*(uint8*)(currentWritePtr + 1) = 0x05;	  // bDescriptorType
		*(uint8*)(currentWritePtr + 2) = 0x02;	  // bEndpointAddress
		*(uint8*)(currentWritePtr + 3) = 0x03;	  // bmAttributes
		*(uint16be*)(currentWritePtr + 4) = 0x40; // wMaxPacketSize
		*(uint8*)(currentWritePtr + 6) = 0x01;	  // bInterval
		currentWritePtr = currentWritePtr + 7;

		cemu_assert_debug((currentWritePtr - configurationDescriptor) == 0x29);

		memcpy(output, configurationDescriptor,
			   std::min<uint32>(outputMaxLength, sizeof(configurationDescriptor)));
		return true;
	}

	bool SkylanderPortalDevice::SetProtocol(uint32 ifIndex, uint32 protocol)
	{
		return true;
	}

	bool SkylanderPortalDevice::SetReport(ReportMessage* message)
	{
		g_skyportal.control_transfer(message->originalData, message->originalLength);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		return true;
	}

	void SkylanderUSB::control_transfer(uint8* buf, sint32 originalLength)
	{
		std::array<uint8, 64> interrupt_response = {};
		switch (buf[0])
		{
		case 'A':
		{
			interrupt_response = {buf[0], buf[1], 0xFF, 0x77};
			g_skyportal.activate();
			break;
		}
		case 'C':
		{
			// Colours
			break;
		}
		case 'J':
		{
			interrupt_response = {buf[0]};
			break;
		}
		case 'L':
		{
			// Trap Team Portal side lights
			break;
		}
		case 'M':
		{
			interrupt_response = {buf[0], buf[1], 0x00, 0x19};
			break;
		}
		case 'Q':
		{
			const uint8 sky_num = buf[1] & 0xF;
			const uint8 block = buf[2];
			g_skyportal.query_block(sky_num, block, interrupt_response.data());
			break;
		}
		case 'R':
		{
			interrupt_response = {buf[0], 0x02, 0x1b};
			// g_skyportal.deactivate();
			break;
		}
		case 'S':
		{
			// Status
			break;
		}
		case 'V':
		{
			// Unsure
			break;
		}
		case 'W':
		{
			const uint8 sky_num = buf[1] & 0xF;
			const uint8 block = buf[2];
			g_skyportal.write_block(sky_num, block, &buf[3], interrupt_response.data());
			break;
		}
		default:
			cemu_assert_error();
			break;
		}
		if (interrupt_response[0] != 0)
		{
			std::lock_guard lock(m_queryMutex);
			m_queries.push(interrupt_response);
		}
	}

	void SkylanderUSB::activate()
	{
		std::lock_guard lock(sky_mutex);
		if (m_activated)
		{
			// If the portal was already active no change is needed
			return;
		}

		// If not we need to advertise change to all the figures present on the portal
		for (auto& s : skylanders)
		{
			if (s.status & 1)
			{
				s.queued_status.push(3);
				s.queued_status.push(1);
			}
		}

		m_activated = true;
	}

	void SkylanderUSB::deactivate()
	{
		std::lock_guard lock(sky_mutex);

		for (auto& s : skylanders)
		{
			// check if at the end of the updates there would be a figure on the portal
			if (!s.queued_status.empty())
			{
				s.status = s.queued_status.back();
				s.queued_status = std::queue<uint8>();
			}

			s.status &= 1;
		}

		m_activated = false;
	}

	uint8 SkylanderUSB::load_skylander(uint8* buf, std::FILE* file)
	{
		std::lock_guard lock(sky_mutex);

		uint32 sky_serial = 0;
		for (int i = 3; i > -1; i--)
		{
			sky_serial <<= 8;
			sky_serial |= buf[i];
		}
		uint8 found_slot = 0xFF;

		// mimics spot retaining on the portal
		for (auto i = 0; i < 16; i++)
		{
			if ((skylanders[i].status & 1) == 0)
			{
				if (skylanders[i].last_id == sky_serial)
				{
					found_slot = i;
					break;
				}

				if (i < found_slot)
				{
					found_slot = i;
				}
			}
		}

		if (found_slot != 0xFF)
		{
			auto& skylander = skylanders[found_slot];
			memcpy(skylander.data.data(), buf, skylander.data.size());
			skylander.sky_file = std::move(file);
			skylander.status = Skylander::ADDED;
			skylander.queued_status.push(Skylander::ADDED);
			skylander.queued_status.push(Skylander::READY);
			skylander.last_id = sky_serial;
		}
		return found_slot;
	}

	bool SkylanderUSB::remove_skylander(uint8 sky_num)
	{
		std::lock_guard lock(sky_mutex);
		auto& thesky = skylanders[sky_num];

		if (thesky.status & 1)
		{
			thesky.status = 2;
			thesky.queued_status.push(2);
			thesky.queued_status.push(0);
			thesky.Save();
			std::fclose(thesky.sky_file);
			return true;
		}

		return false;
	}

	void SkylanderUSB::query_block(uint8 sky_num, uint8 block, uint8* reply_buf)
	{
		std::lock_guard lock(sky_mutex);

		const auto& skylander = skylanders[sky_num];

		reply_buf[0] = 'Q';
		reply_buf[2] = block;
		if (skylander.status & 1)
		{
			reply_buf[1] = (0x10 | sky_num);
			memcpy(reply_buf + 3, skylander.data.data() + (16 * block), 16);
		}
		else
		{
			reply_buf[1] = sky_num;
		}
	}

	void SkylanderUSB::write_block(uint8 sky_num, uint8 block,
								   const uint8* to_write_buf, uint8* reply_buf)
	{
		std::lock_guard lock(sky_mutex);

		auto& skylander = skylanders[sky_num];

		reply_buf[0] = 'W';
		reply_buf[2] = block;

		if (skylander.status & 1)
		{
			reply_buf[1] = (0x10 | sky_num);
			memcpy(skylander.data.data() + (block * 16), to_write_buf, 16);
			skylander.Save();
		}
		else
		{
			reply_buf[1] = sky_num;
		}
	}

	std::array<uint8, 64> SkylanderUSB::get_status()
	{
		std::array<uint8, 64> interrupt_response = {};

		if (!m_queries.empty())
		{
			std::lock_guard lock(m_queryMutex);
			interrupt_response = m_queries.front();
			m_queries.pop();
			// This needs to happen after ~22 milliseconds
		}
		else
		{
			std::lock_guard lock(sky_mutex);

			uint32 status = 0;
			uint8 active = 0x00;
			if (m_activated)
			{
				active = 0x01;
			}

			for (int i = 16 - 1; i >= 0; i--)
			{
				auto& s = skylanders[i];

				if (!s.queued_status.empty())
				{
					s.status = s.queued_status.front();
					s.queued_status.pop();
				}
				status <<= 2;
				status |= s.status;
			}
			interrupt_response = {0x53, 0x00, 0x00, 0x00, 0x00, m_interrupt_counter++,
								  active, 0x00, 0x00, 0x00, 0x00, 0x00,
								  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
								  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
								  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
								  0x00, 0x00};
			memcpy(&interrupt_response[1], &status, sizeof(status));
		}
		return interrupt_response;
	}

	void SkylanderUSB::Skylander::Save()
	{
		if (!sky_file)
			return;

#if BOOST_OS_WINDOWS
		_fseeki64(sky_file, 0, 0);
#else
		fseeko(sky_file, 0, 0);
#endif
		std::fwrite(&data[0], sizeof(data[0]), data.size(), sky_file);
	}
} // namespace nsyshid