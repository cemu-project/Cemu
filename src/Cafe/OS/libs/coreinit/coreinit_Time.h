#pragma once
#include "Cafe/HW/Espresso/Const.h"

namespace coreinit
{
	typedef struct
	{
		/* +0x00 */ sint32be second;			// 0-59
		/* +0x04 */ sint32be minute;			// 0-59
		/* +0x08 */ sint32be hour;				// 0-23
		/* +0x0C */ sint32be dayOfMonth;		// 1-31
		/* +0x10 */ sint32be month;				// 0-11
		/* +0x14 */ sint32be year;				// 2000-...
		/* +0x18 */ sint32be dayOfWeek;			// 0-6
		/* +0x1C */ sint32be dayOfYear;			// 0-365
		/* +0x20 */ sint32be millisecond;		// 0-999
		/* +0x24 */ sint32be microsecond;		// 0-999
	}OSCalendarTime_t;

	static_assert(sizeof(OSCalendarTime_t) == 0x28);

	namespace EspressoTime
	{
		typedef sint64 TimerTicks;

		constexpr sint64 GetCoreClock()
		{
			return Espresso::CORE_CLOCK;
		}

		constexpr sint64 GetBusClock()
		{
			return Espresso::BUS_CLOCK;
		}

		constexpr sint64 GetTimerClock()
		{
			return Espresso::TIMER_CLOCK;
		}

		inline TimerTicks ConvertNsToTimerTicks(uint64 ns)
		{
			return static_cast<TimerTicks>((static_cast<uint64>(GetTimerClock()) / 31250ULL) * (ns) / 32000ULL);
		}

		inline TimerTicks ConvertMsToTimerTicks(uint64 ms)
		{
			return static_cast<TimerTicks>(ms * static_cast<uint64>(GetTimerClock()) / 1000ULL);
		}
	};

	void OSTicksToCalendarTime(uint64 ticks, OSCalendarTime_t* calenderStruct);
	void FSTimeToCalendarTime(uint64 fsTime_microseconds, OSCalendarTime_t* outCalendarTime);

	uint64 OSGetSystemTime();
	uint64 OSGetTime();
	uint32 OSGetSystemTick();
	uint32 OSGetTick();

	void InitializeTimeAndCalendar();
};
