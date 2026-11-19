#pragma once
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"

// Profiling of the time guest threads spend blocked in GPU related waits (only active when CEMU_PERF_LOG is set).
// Time is wall-clock from entering the wait until the guest thread runs again, so it includes the
// delay between wake-up and being scheduled. Values are summed over all guest threads.
namespace GuestWaitProf
{
	enum Cat : uint32
	{
		VSYNC = 0, // GX2WaitForVsync
		FLIP = 1, // GX2WaitForFlip
		DRAWDONE = 2, // GX2DrawDone (flush + wait for retired timestamp)
		WAITTS = 3, // GX2WaitTimeStamp called directly by the game
		CMDBUF = 4, // waiting for a command buffer to be retired (GPU ring full)
		COUNT = 5
	};

	inline const bool g_enabled = getenv("CEMU_PERF_LOG") != nullptr;
	inline std::atomic<uint64> g_ticks[COUNT]{};
	inline std::atomic<uint32> g_calls[COUNT]{};

	struct Scope
	{
		explicit Scope(Cat cat) : m_cat(cat)
		{
			if (g_enabled)
				m_start = PPCTimer_getRawTsc();
		}
		~Scope()
		{
			if (!g_enabled)
				return;
			g_ticks[m_cat].fetch_add(PPCTimer_getRawTsc() - m_start, std::memory_order_relaxed);
			g_calls[m_cat].fetch_add(1, std::memory_order_relaxed);
		}
	private:
		Cat m_cat;
		uint64 m_start{};
	};

	inline void GetAndReset(Cat cat, uint64& ticks, uint32& calls)
	{
		ticks = g_ticks[cat].exchange(0, std::memory_order_relaxed);
		calls = g_calls[cat].exchange(0, std::memory_order_relaxed);
	}
}
