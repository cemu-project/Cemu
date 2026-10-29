#include "Mouse.h"
#include "Cafe/CafeSystem.h"

namespace nsyshid
{
	// five buttons, 16-bit x and y and a wheel
	// in the boot protocol (HID specification appendix B.2) x and y are 8-bit and the wheel is left out
	static constexpr uint8 s_reportDescriptor[] = {
		0x05, 0x01,		  // Usage Page (Generic Desktop)
		0x09, 0x02,		  // Usage (Mouse)
		0xA1, 0x01,		  // Collection (Application)
		0x09, 0x01,		  //   Usage (Pointer)
		0xA1, 0x00,		  //   Collection (Physical)
		0x05, 0x09,		  //     Usage Page (Buttons)
		0x19, 0x01,		  //     Usage Minimum (1)
		0x29, 0x05,		  //     Usage Maximum (5)
		0x15, 0x00,		  //     Logical Minimum (0)
		0x25, 0x01,		  //     Logical Maximum (1)
		0x95, 0x05,		  //     Report Count (5)
		0x75, 0x01,		  //     Report Size (1)
		0x81, 0x02,		  //     Input (Data, Variable, Absolute) - buttons
		0x95, 0x01,		  //     Report Count (1)
		0x75, 0x03,		  //     Report Size (3)
		0x81, 0x01,		  //     Input (Constant) - padding
		0x05, 0x01,		  //     Usage Page (Generic Desktop)
		0x09, 0x30,		  //     Usage (X)
		0x09, 0x31,		  //     Usage (Y)
		0x16, 0x01, 0x80, //     Logical Minimum (-32767)
		0x26, 0xFF, 0x7F, //     Logical Maximum (32767)
		0x75, 0x10,		  //     Report Size (16)
		0x95, 0x02,		  //     Report Count (2)
		0x81, 0x06,		  //     Input (Data, Variable, Relative) - x, y
		0x09, 0x38,		  //     Usage (Wheel)
		0x15, 0x81,		  //     Logical Minimum (-127)
		0x25, 0x7F,		  //     Logical Maximum (127)
		0x75, 0x08,		  //     Report Size (8)
		0x95, 0x01,		  //     Report Count (1)
		0x81, 0x06,		  //     Input (Data, Variable, Relative) - wheel
		0xC0,			  //   End Collection
		0xC0,			  // End Collection
	};

	static constexpr uint8 s_deviceDescriptor[] = {
		18,			// bLength
		0x01,		// bDescriptorType
		0x00, 0x02, // bcdUSB 2.00
		0x00,		// bDeviceClass (defined per interface)
		0x00,		// bDeviceSubClass
		0x00,		// bDeviceProtocol
		8,			// bMaxPacketSize0
		MOUSE_VENDOR_ID & 0xFF, MOUSE_VENDOR_ID >> 8,
		MOUSE_PRODUCT_ID & 0xFF, MOUSE_PRODUCT_ID >> 8,
		0x00, 0x72, // bcdDevice
		0,			// iManufacturer
		0,			// iProduct
		0,			// iSerialNumber
		1,			// bNumConfigurations
	};

	static constexpr uint8 s_configurationDescriptor[] = {
		// configuration descriptor
		9, 0x02, 34, 0, // bLength, bDescriptorType, wTotalLength
		1,				// bNumInterfaces
		1,				// bConfigurationValue
		0,				// iConfiguration
		0xA0,			// bmAttributes (bus powered, remote wakeup)
		50,				// MaxPower (100mA)
		// interface descriptor
		9, 0x04,
		0,	  // bInterfaceNumber
		0,	  // bAlternateSetting
		1,	  // bNumEndpoints
		0x03, // bInterfaceClass (HID)
		0x01, // bInterfaceSubClass (boot interface)
		0x02, // bInterfaceProtocol (mouse)
		0,	  // iInterface
		// HID descriptor
		9, 0x21,
		0x11, 0x01, // bcdHID 1.11
		0x00,		// bCountryCode
		1,			// bNumDescriptors
		0x22,		// bDescriptorType (report)
		sizeof(s_reportDescriptor), 0,
		// endpoint descriptor
		7, 0x05,
		0x81, // bEndpointAddress (in, 1)
		0x03, // bmAttributes (interrupt)
		8, 0, // wMaxPacketSize
		4,	  // bInterval (4ms)
	};

	static_assert(sizeof(s_configurationDescriptor) == 34);

	constexpr auto POLLING_INTERVAL = std::chrono::milliseconds(4);

	struct HostMouseState
	{
		sint32 deltaX;
		sint32 deltaY;
		sint32 wheel;
		uint8 buttons;
	};

	static std::mutex s_hostMouseMutex;
	static HostMouseState s_hostMouse{};
	static uint8 s_hostButtonsClicked = 0;
	static std::atomic_bool s_mouseInUse{false};

	bool IsMouseInUse()
	{
		return s_mouseInUse;
	}

	void AddHostMouseMotion(sint32 deltaX, sint32 deltaY, sint32 wheel)
	{
		std::unique_lock lock(s_hostMouseMutex);
		s_hostMouse.deltaX += deltaX;
		s_hostMouse.deltaY += deltaY;
		s_hostMouse.wheel += wheel;
	}

	void SetHostMouseButton(uint8 button, bool pressed)
	{
		std::unique_lock lock(s_hostMouseMutex);
		if (pressed)
		{
			s_hostMouse.buttons |= (1 << button);
			s_hostButtonsClicked |= (1 << button);
		}
		else
			s_hostMouse.buttons &= ~(1 << button);
	}

	void ReleaseHostMouse()
	{
		std::unique_lock lock(s_hostMouseMutex);
		s_hostMouse = {};
		s_hostButtonsClicked = 0;
	}

	// a click between two polls is still reported as held once
	static HostMouseState PopHostMouseState()
	{
		std::unique_lock lock(s_hostMouseMutex);
		HostMouseState state = s_hostMouse;
		state.buttons |= s_hostButtonsClicked;
		s_hostButtonsClicked = 0;
		s_hostMouse.deltaX = 0;
		s_hostMouse.deltaY = 0;
		s_hostMouse.wheel = 0;
		return state;
	}

	MouseDevice::MouseDevice()
		: Device(MOUSE_VENDOR_ID, MOUSE_PRODUCT_ID, 0, 1, 2) // boot interface subclass, mouse protocol
	{
		m_maxPacketSizeRX = 8;
		m_maxPacketSizeTX = 8;
	}

	bool MouseDevice::Open()
	{
		std::unique_lock lock(m_mutex);
		if (!m_isOpened)
		{
			m_isOpened = true;
			PopHostMouseState(); // drop motion from before the title opened the mouse
			m_buttons = 0;
			m_lastButtons = 0;
			m_pendingX = 0;
			m_pendingY = 0;
			m_pendingWheel = 0;
			m_lastReportTime = std::chrono::steady_clock::now();
		}
		return true;
	}

	void MouseDevice::Close()
	{
		{
			std::unique_lock lock(m_mutex);
			if (!m_isOpened)
				return;
			m_isOpened = false;
			if (s_mouseInUse.exchange(false))
				CafeSystem::NotifyUSBInputChanged();
		}
		m_closeCondition.notify_all();
	}

	bool MouseDevice::IsOpened()
	{
		std::unique_lock lock(m_mutex);
		return m_isOpened;
	}

	void MouseDevice::PollHostMouse()
	{
		const HostMouseState state = PopHostMouseState();
		m_buttons = state.buttons;
		m_pendingX += state.deltaX;
		m_pendingY += state.deltaY;
		m_pendingWheel += state.wheel;
	}

	// motion beyond the range of a report carries over to the next one
	MouseDevice::Report MouseDevice::TakeReport()
	{
		Report report{};
		report[0] = m_buttons;
		m_lastButtons = m_buttons;
		if (m_reportProtocol == 0)
		{
			const sint32 x = std::clamp(m_pendingX, -127, 127);
			const sint32 y = std::clamp(m_pendingY, -127, 127);
			m_pendingX -= x;
			m_pendingY -= y;
			m_pendingWheel = 0;
			report[1] = (uint8)(sint8)x;
			report[2] = (uint8)(sint8)y;
			return report;
		}
		const sint32 x = std::clamp(m_pendingX, -32767, 32767);
		const sint32 y = std::clamp(m_pendingY, -32767, 32767);
		const sint32 wheel = std::clamp(m_pendingWheel, -127, 127);
		m_pendingX -= x;
		m_pendingY -= y;
		m_pendingWheel -= wheel;
		report[1] = (uint8)x;
		report[2] = (uint8)(x >> 8);
		report[3] = (uint8)y;
		report[4] = (uint8)(y >> 8);
		report[5] = (uint8)(sint8)wheel;
		return report;
	}

	uint32 MouseDevice::GetReportLength() const
	{
		return m_reportProtocol == 0 ? 3 : 6;
	}

	Device::ReadResult MouseDevice::Read(ReadMessage* message)
	{
		std::unique_lock lock(m_mutex);
		if (!m_isOpened || message->length == 0)
			return ReadResult::Error;
		// titles open every device to read its descriptors, only reading input means the mouse is used
		if (!s_mouseInUse.exchange(true))
			CafeSystem::NotifyUSBInputChanged();
		m_closeCondition.wait_until(lock, m_lastReportTime + POLLING_INTERVAL, [this] { return !m_isOpened; });
		while (m_isOpened)
		{
			PollHostMouse();
			const bool hasWheel = m_reportProtocol != 0 && m_pendingWheel != 0;
			const bool changed = m_buttons != m_lastButtons || m_pendingX != 0 || m_pendingY != 0 || hasWheel;
			const auto now = std::chrono::steady_clock::now();
			const bool idleExpired = m_idleDuration != 0 && now - m_lastReportTime >= std::chrono::milliseconds(4 * m_idleDuration);
			if (changed || idleExpired)
			{
				const Report report = TakeReport();
				m_lastReportTime = now;
				const uint32 length = std::min<uint32>(message->length, GetReportLength());
				memcpy(message->data, report.data(), length);
				message->bytesRead = length;
				return ReadResult::Success;
			}
			m_closeCondition.wait_for(lock, POLLING_INTERVAL);
		}
		return ReadResult::Error;
	}

	Device::WriteResult MouseDevice::Write(WriteMessage* message)
	{
		return WriteResult::Error;
	}

	bool MouseDevice::GetDescriptor(uint8 descType,
									uint8 descIndex,
									uint16 lang,
									uint8* output,
									uint32 outputMaxLength)
	{
		std::span<const uint8> descriptor;
		switch (descType)
		{
		case 0x01: // device
			descriptor = s_deviceDescriptor;
			break;
		case 0x02: // configuration
			descriptor = s_configurationDescriptor;
			break;
		case 0x22: // report
			descriptor = s_reportDescriptor;
			break;
		default:
			cemuLog_logDebug(LogType::Force, "nsyshid::MouseDevice::GetDescriptor(): unsupported descriptor type 0x{:02x}", descType);
			return false;
		}
		memcpy(output, descriptor.data(), std::min<uint32>(outputMaxLength, descriptor.size()));
		return true;
	}

	bool MouseDevice::SetIdle(uint8 ifIndex,
							  uint8 reportId,
							  uint8 duration)
	{
		std::unique_lock lock(m_mutex);
		m_idleDuration = duration;
		return true;
	}

	bool MouseDevice::SetProtocol(uint8 ifIndex, uint8 protocol)
	{
		std::unique_lock lock(m_mutex);
		m_reportProtocol = protocol;
		return true;
	}

	bool MouseDevice::SetReport(ReportMessage* message)
	{
		return false;
	}
} // namespace nsyshid
