#pragma once

#include "cemuextend/services.hpp"

#include <cstdint>

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

	inline void ObserveVpad(sint32 channel, const VPADStatus& status, sint32 error, sint32 sampleCount)
	{
		Cex2Host::Instance().ObserveVpad(channel, status, error, sampleCount);
	}

	inline void ApplyMappedVpad(sint32 channel, VPADStatus& status)
	{
		Cex2Host::Instance().ApplyMappedVpad(channel, status);
	}

	inline void KeyboardEvent(uint16 usbHidUsage, bool pressed, uint8 modifiers)
	{
		Cex2Host::Instance().KeyboardEvent(usbHidUsage, pressed, modifiers);
	}

	inline void KeyboardFocusLost()
	{
		Cex2Host::Instance().KeyboardFocusLost();
	}

}
