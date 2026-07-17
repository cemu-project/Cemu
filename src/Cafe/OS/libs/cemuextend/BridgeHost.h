#pragma once

#include "cemuextend/services.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Cafe/OS/libs/cemuextend/Cex2Host.h"

struct VPADStatus;

namespace cemuextend_hle
{
	constexpr uint32 kDefaultReadMask =
		(1U << (static_cast<uint32>(cemuextend::wire::ServiceId::Core) - 1U)) |
		(1U << (static_cast<uint32>(cemuextend::wire::ServiceId::Input) - 1U)) |
		(1U << (static_cast<uint32>(cemuextend::wire::ServiceId::Configuration) - 1U)) |
		(1U << (static_cast<uint32>(cemuextend::wire::ServiceId::File) - 1U)) |
		(1U << (static_cast<uint32>(cemuextend::wire::ServiceId::Window) - 1U)) |
		(1U << (static_cast<uint32>(cemuextend::wire::ServiceId::Diagnostics) - 1U));
	constexpr uint32 kDefaultWriteMask =
		(1U << (static_cast<uint32>(cemuextend::wire::ServiceId::Input) - 1U)) |
		(1U << (static_cast<uint32>(cemuextend::wire::ServiceId::Logging) - 1U));
	constexpr uint32 kDefaultInjectMask = 0;

	class BridgeHost
	{
	public:
		static BridgeHost& Instance();

		sint32 Register(uint32 abiVersion, void* region, uint32 regionSize, uint32& sessionId);
		sint32 Notify(uint32 sessionId, uint32 flags);
		sint32 Unregister(uint32 sessionId);
		void Stop();

		void ObserveVpad(sint32 channel, const VPADStatus& status, sint32 error, sint32 sampleCount);
		void ApplyMappedVpad(sint32 channel, VPADStatus& status);
		void KeyboardEvent(uint16 usbHidUsage, bool pressed, uint8 modifiers);
		void KeyboardFocusLost();
		void PermissionsChanged();

		void CompleteClipboard(uint32 sessionId, uint32 correlationId, uint16 operation,
			bool success, std::string text);
		void CompleteCapture(uint32 sessionId, uint32 correlationId, std::vector<uint8> rgb,
			int width, int height, bool mainWindow);

	private:
		BridgeHost();
		~BridgeHost();
		BridgeHost(const BridgeHost&) = delete;
		BridgeHost& operator=(const BridgeHost&) = delete;

		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

	inline void ObserveVpad(sint32 channel, const VPADStatus& status, sint32 error, sint32 sampleCount)
	{
		BridgeHost::Instance().ObserveVpad(channel, status, error, sampleCount);
		Cex2Host::Instance().ObserveVpad(channel, status, error, sampleCount);
	}

	inline void ApplyMappedVpad(sint32 channel, VPADStatus& status)
	{
		BridgeHost::Instance().ApplyMappedVpad(channel, status);
		Cex2Host::Instance().ApplyMappedVpad(channel, status);
	}

	inline void KeyboardEvent(uint16 usbHidUsage, bool pressed, uint8 modifiers)
	{
		BridgeHost::Instance().KeyboardEvent(usbHidUsage, pressed, modifiers);
		Cex2Host::Instance().KeyboardEvent(usbHidUsage, pressed, modifiers);
	}

	inline void KeyboardFocusLost()
	{
		BridgeHost::Instance().KeyboardFocusLost();
		Cex2Host::Instance().KeyboardFocusLost();
	}

	inline void PermissionsChanged()
	{
		BridgeHost::Instance().PermissionsChanged();
	}
}
