#include "Whitelist.h"

namespace nsyshid
{
	Whitelist& Whitelist::GetInstance()
	{
		static Whitelist whitelist;
		return whitelist;
	}

	Whitelist::Whitelist()
	{
		// add known devices
		{
			// lego dimensions portal
			m_devices.emplace_back(0x0e6f, 0x0241);
			// skylanders portal
			m_devices.emplace_back(0x1430, 0x0150);
			// disney infinity base
			m_devices.emplace_back(0x0e6f, 0x0129);
		}
	}

	bool Whitelist::IsDeviceWhitelisted(uint16 vendorId, uint16 productId)
	{
		auto it = std::find(m_devices.begin(), m_devices.end(),
							std::tuple<uint16, uint16>(vendorId, productId));
		return it != m_devices.end();
	}

	void Whitelist::AddDevice(uint16 vendorId, uint16 productId)
	{
		if (!IsDeviceWhitelisted(vendorId, productId))
		{
			m_devices.emplace_back(vendorId, productId);
		}
	}

	void Whitelist::RemoveDevice(uint16 vendorId, uint16 productId)
	{
		m_devices.remove(std::tuple<uint16, uint16>(vendorId, productId));
	}

	std::list<std::tuple<uint16, uint16>> Whitelist::GetDevices()
	{
		return m_devices;
	}

	void Whitelist::RemoveAllDevices()
	{
		m_devices.clear();
	}
} // namespace nsyshid
