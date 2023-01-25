#pragma once

namespace Espresso
{
	constexpr inline int CORE_COUNT = 3;

	constexpr inline uint64 CORE_CLOCK = 1243125000;
	constexpr inline uint64 BUS_CLOCK = 248625000;
	constexpr inline uint64 TIMER_CLOCK = BUS_CLOCK / 4;

	constexpr inline uint32 MEM_PAGE_SIZE = 0x20000;
};