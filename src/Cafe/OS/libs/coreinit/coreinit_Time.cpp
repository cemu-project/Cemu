#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"

namespace coreinit
{

	uint64 coreinit_getTimerTick()
	{
		// bus clock is 1/5th of core clock
		// timer clock is 1/4th of bus clock
		return PPCInterpreter_getMainCoreCycleCounter() / 20ULL;
	}

	uint64 coreinit_getOSTime()
	{
		return coreinit_getTimerTick() + ppcCyclesSince2000TimerClock;
	}

	void export_OSGetTick(PPCInterpreter_t* hCPU)
	{
		uint64 osTime = coreinit_getOSTime();
		osLib_returnFromFunction(hCPU, (uint32)osTime);
	}

	void export_OSGetTime(PPCInterpreter_t* hCPU)
	{
		uint64 osTime = coreinit_getOSTime();
		osLib_returnFromFunction64(hCPU, osTime);
	}

	void export_OSGetSystemTime(PPCInterpreter_t* hCPU)
	{
		osLib_returnFromFunction64(hCPU, coreinit_getTimerTick());
	}

	void export_OSGetSystemTick(PPCInterpreter_t* hCPU)
	{
		osLib_returnFromFunction(hCPU, (uint32)coreinit_getTimerTick());
	}

	uint32 getLeapDaysUntilYear(uint32 year)
	{
		if (year == 0)
			return 0;
		return (year + 3) / 4 - (year - 1) / 100 + (year - 1) / 400;
	}

	bool IsLeapYear(uint32 year)
	{
		if (((year & 3) == 0) && (year % 100) != 0)
			return true;
		if ((year % 400) == 0)
			return true;
		return false;
	}

	uint32 dayToMonth[12] =
	{
		0,31,59,90,120,151,181,212,243,273,304,334
	};

	uint32 dayToMonthLeapYear[12] =
	{
		0,31,60,91,121,152,182,213,244,274,305,335
	};

	uint32 getDayInYearByYearAndMonth(uint32 year, uint32 month)
	{
		// Project Zero Maiden of Black Water (JPN) gives us an invalid calendar object
		month %= 12; // or return 0 if too big?
		
		if (IsLeapYear(year))
			return dayToMonthLeapYear[month];
		
		return dayToMonth[month];
	}


	inline const uint64 DAY_BIAS_2000 = 0xB2575;

	uint64 OSCalendarTimeToTicks(OSCalendarTime_t *calendar)
	{
		uint32 year = calendar->year;

		uint32 leapDays = getLeapDaysUntilYear(year);
		uint32 startDayOfCurrentMonth = getDayInYearByYearAndMonth(year, calendar->month);

		uint64 dayInYear = (startDayOfCurrentMonth + calendar->dayOfMonth) - 1;
		
		uint64 dayCount = dayInYear + year * 365 + leapDays - DAY_BIAS_2000;

		// convert date to seconds
		uint64 tSeconds = 0;
		tSeconds += (uint64)dayCount * 24 * 60 * 60;
		tSeconds += (uint64)calendar->hour * 60 * 60;
		tSeconds += (uint64)calendar->minute * 60;
		tSeconds += (uint64)calendar->second;

		uint64 tSubSecondTicks = 0;
		tSubSecondTicks += (uint64)calendar->millisecond * ESPRESSO_TIMER_CLOCK / 1000;
		tSubSecondTicks += (uint64)calendar->microsecond * ESPRESSO_TIMER_CLOCK / 1000000;

		return tSeconds * ESPRESSO_TIMER_CLOCK + tSubSecondTicks;
	}

	void OSTicksToCalendarTime(uint64 ticks, OSCalendarTime_t* calenderStruct)
	{
		uint64 tSubSecondTicks = ticks % ESPRESSO_TIMER_CLOCK;
		uint64 tSeconds = ticks / ESPRESSO_TIMER_CLOCK;

		uint64 microsecond = tSubSecondTicks * 1000000ull / ESPRESSO_TIMER_CLOCK;
		microsecond %= 1000ull;
		calenderStruct->microsecond = (uint32)microsecond;

		uint64 millisecond = tSubSecondTicks * 1000ull / ESPRESSO_TIMER_CLOCK;
		millisecond %= 1000ull;
		calenderStruct->millisecond = (uint32)millisecond;

		uint64 dayOfWeek = (tSeconds/(24ull * 60 * 60) + 6ull) % 7ull;
		uint64 secondOfDay = (tSeconds % (24ull * 60 * 60));

		calenderStruct->dayOfWeek = (sint32)dayOfWeek;

		uint64 daysSince0AD = tSeconds / (24ull * 60 * 60) + DAY_BIAS_2000;
		uint32 year = (uint32)(daysSince0AD / 365ull);
		uint64 yearStartDay = year * 365 + getLeapDaysUntilYear(year);
		while (yearStartDay > daysSince0AD)
		{
			year--;
			if (IsLeapYear(year))
				yearStartDay -= 366;
			else
				yearStartDay -= 365;
		}

		calenderStruct->year = year;

		// calculate time of day
		uint32 tempSecond = (uint32)secondOfDay;
		calenderStruct->second = tempSecond % 60;
		tempSecond /= 60;
		calenderStruct->minute = tempSecond % 60;
		tempSecond /= 60;
		calenderStruct->hour = tempSecond % 24;
		tempSecond /= 24;

		// calculate month and day
		uint32 dayInYear = (uint32)(daysSince0AD - yearStartDay);
		bool isLeapYear = IsLeapYear(year);

		uint32 month = 0; // 0-11
		uint32 dayInMonth = 0;

		if (isLeapYear && dayInYear < (31+29))
		{
			if (dayInYear < 31)
			{
				// January
				month = 0;
				dayInMonth = dayInYear;
			}
			else
			{
				// February
				month = 1;
				dayInMonth = dayInYear-31;
			}
		}
		else
		{
			if (isLeapYear)
				dayInYear--;

			dayInMonth = dayInYear;
			if (dayInYear >= 334)
			{
				// December
				month = 11;
				dayInMonth -= 334;
			}
			else if (dayInYear >= 304)
			{
				// November
				month = 10;
				dayInMonth -= 304;
			}
			else if (dayInYear >= 273)
			{
				// October
				month = 9;
				dayInMonth -= 273;
			}
			else if (dayInYear >= 243)
			{
				// September
				month = 8;
				dayInMonth -= 243;
			}
			else if (dayInYear >= 212)
			{
				// August
				month = 7;
				dayInMonth -= 212;
			}
			else if (dayInYear >= 181)
			{
				// July
				month = 6;
				dayInMonth -= 181;
			}
			else if (dayInYear >= 151)
			{
				// June
				month = 5;
				dayInMonth -= 151;
			}
			else if (dayInYear >= 120)
			{
				// May
				month = 4;
				dayInMonth -= 120;
			}
			else if (dayInYear >= 90)
			{
				// April
				month = 3;
				dayInMonth -= 90;
			}
			else if (dayInYear >= 59)
			{
				// March
				month = 2;
				dayInMonth -= 59;
			}
			else if (dayInYear >= 31)
			{
				// February
				month = 1;
				dayInMonth -= 31;
			}
			else
			{
				// January
				month = 0;
				dayInMonth -= 0;
			}
		}

		calenderStruct->dayOfYear = dayInYear;
		calenderStruct->month = month;
		calenderStruct->dayOfMonth = dayInMonth + 1;
	}

	uint32 getDaysInMonth(uint32 year, uint32 month)
	{
		switch (month)
		{
		case 0: // January
			return 31;
		case 1: // February
			return IsLeapYear(year) ? 29 : 28;
		case 2: // March
			return 31;
		case 3: // April
			return 30;
		case 4: // May
			return 31;
		case 5: // June
			return 30;
		case 6: // July
			return 31;
		case 7: // August
			return 31;
		case 8: // September
			return 30;
		case 9: // October
			return 31;
		case 10: // November
			return 30;
		case 11: // December
			return 31;
		default:
			cemu_assert(false);
		}
		return 31;
	}

	void verifyDateMatch(OSCalendarTime_t* ct1, OSCalendarTime_t* ct2)
	{
		sint64 m1 = ct1->millisecond * 1000 + ct1->microsecond;
		sint64 m2 = ct2->millisecond * 1000 + ct2->microsecond;
		sint64 microDif = std::abs(m1 - m2);

		if (ct1->year != ct2->year ||
			ct1->month != ct2->month ||
			ct1->dayOfMonth != ct2->dayOfMonth ||
			ct1->hour != ct2->hour ||
			ct1->minute != ct2->minute ||
			ct1->second != ct2->second ||
			microDif > 1ull)
		{
			debug_printf("Mismatch\n");
			assert_dbg();
		}
	}

	void timeTest()
	{
		sint32 iterCount = 0;

		OSCalendarTime_t ct{};
		ct.year = 2000;
		ct.month = 1;
		ct.dayOfMonth = 1;
		ct.hour = 1;
		ct.minute = 1;
		ct.second = 1;
		ct.millisecond = 123;
		ct.microsecond = 321;

		while (true)
		{
			iterCount++;


			uint64 ticks = OSCalendarTimeToTicks(&ct);

			// make sure converting it back results in the same date
			OSCalendarTime_t ctTemp;
			OSTicksToCalendarTime(ticks, &ctTemp);
			verifyDateMatch(&ct, &ctTemp);

			// add a day
			ticks += 24ull * 60 * 60 * ESPRESSO_TIMER_CLOCK;

			OSCalendarTime_t ctOutput;
			OSTicksToCalendarTime(ticks, &ctOutput);

			OSCalendarTime_t ctExpected;
			ctExpected = ct;
			// add a day manually
			sint32 daysInMonth = getDaysInMonth(ctExpected.year, ctExpected.month);
			ctExpected.dayOfMonth = ctExpected.dayOfMonth + 1;
			if (ctExpected.dayOfMonth >= daysInMonth+1)
			{
				ctExpected.dayOfMonth = 1;
				ctExpected.month = ctExpected.month + 1;
				if (ctExpected.month > 11)
				{
					ctExpected.month = 0;
					ctExpected.year = ctExpected.year + 1;
				}
			}

			verifyDateMatch(&ctExpected, &ctOutput);

			ct = ctOutput;
		}
	}

	void InitializeTimeAndCalendar()
	{
		osLib_addFunction("coreinit", "OSGetTime", export_OSGetTime);
		osLib_addFunction("coreinit", "OSGetSystemTime", export_OSGetSystemTime);
		osLib_addFunction("coreinit", "OSGetTick", export_OSGetTick);
		osLib_addFunction("coreinit", "OSGetSystemTick", export_OSGetSystemTick);

		cafeExportRegister("coreinit", OSTicksToCalendarTime, LogType::Placeholder);
		cafeExportRegister("coreinit", OSCalendarTimeToTicks, LogType::Placeholder);


		//timeTest();
	}
};