#pragma once
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"

namespace coreinit
{
	class OSHostAlarm;
	OSHostAlarm* OSHostAlarmCreate(uint64 nextFire, uint64 period, void(*callbackFunc)(uint64 currentTick, void* context), void* context);
	void OSHostAlarmDestroy(OSHostAlarm* hostAlarm);

	struct OSAlarm_t
	{
		/* +0x00 */ betype<uint32>  magic;
		/* +0x04 */ MEMPTR<const char> name;
		/* +0x08 */ uint32	        ukn08;
		/* +0x0C */ MPTR			handler;
		/* +0x10 */ uint32          ukn10;
		/* +0x14 */ uint32			padding14;
		/* +0x18 */ uint64	        nextTime; // next fire time
		/* +0x20 */ MPTR			prev; // pointer to OSAlarm
		/* +0x24 */ MPTR			next; // pointer to OSAlarm
		/* +0x28 */ uint64          period; // period (zero for non-periodic timer)
		/* +0x30 */ uint64          startTime; // period start
		/* +0x38 */ uint32be        userData;
		/* +0x3C */ uint32		    ukn3C;
		/* +0x40 */ OSThreadQueue   uknThreadQueue;
		/* +0x50 */ MPTR			alarmQueue;
		/* +0x54 */ MPTR			ukn54;

		void setMagic()
		{
			magic = (uint32)'aLrM';
		}

		bool checkMagic()
		{
			return magic == (uint32)'aLrM';
		}
	};

	static_assert(sizeof(OSAlarm_t) == 0x58);

	void OSCreateAlarm(OSAlarm_t* alarm);
	bool OSCancelAlarm(OSAlarm_t* alarm);
	void OSSetAlarm(OSAlarm_t* alarm, uint64 time, MPTR handlerFunc);
	void OSSetAlarmUserData(OSAlarm_t* alarm, uint32 userData);
	void OSSetPeriodicAlarm(OSAlarm_t* OSAlarm, uint64 startTick, uint64 periodTick, MPTR OSAlarmHandler);

	void OSAlarm_Shutdown();

	void alarm_update();

	void InitializeAlarm();
}