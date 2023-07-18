#pragma once

namespace coreinit
{
	uint32 __OSGetProcessSDKVersion();
	uint32 OSLaunchTitleByPathl(const char* path, uint32 pathLength, uint32 argc);
	uint32 OSRestartGame(uint32 argc, MEMPTR<char>* argv);

	void miscInit();
};