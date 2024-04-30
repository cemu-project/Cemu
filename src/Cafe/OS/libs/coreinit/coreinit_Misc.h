#pragma once

namespace coreinit
{
	uint32 OSGetUPID();
	uint32 OSGetPFID();
	uint64 OSGetTitleID();
	uint32 __OSGetProcessSDKVersion();
	uint32 OSLaunchTitleByPathl(const char* path, uint32 pathLength, uint32 argc);
	uint32 OSRestartGame(uint32 argc, MEMPTR<char>* argv);

	void OSReleaseForeground();

	void StartBackgroundForegroundTransition();

	struct OSDriverInterface
	{
		MEMPTR<void> getDriverName;
		MEMPTR<void> init;
		MEMPTR<void> onAcquireForeground;
		MEMPTR<void> onReleaseForeground;
		MEMPTR<void> done;
	};
	static_assert(sizeof(OSDriverInterface) == 0x14);

	uint32 OSDriver_Register(uint32 moduleHandle, sint32 priority, OSDriverInterface* driverCallbacks, sint32 driverId, uint32be* outUkn1, uint32be* outUkn2, uint32be* outUkn3);
	uint32 OSDriver_Deregister(uint32 moduleHandle, sint32 driverId);

	void miscInit();
};