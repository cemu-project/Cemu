#pragma once

#define OSSCREEN_TV		(0)
#define OSSCREEN_DRC	(1)

namespace coreinit
{
	void OSScreenInit();
	uint32 OSScreenGetBufferSizeEx(uint32 screenIndex);
	void OSScreenSetBufferEx(uint32 screenIndex, uint32 buffer);
	void OSScreenEnableEx(uint32 screenIndex, uint32 isEnabled);
	void OSScreenClearBufferEx(uint32 screenIndex, uint32 color);
	void OSScreenFlipBuffersEx(uint32 screenIndex);
	void OSScreenPutPixelEx(uint32 screenIndex, sint32 x, sint32 y, uint32 color);
	void OSScreenPutFontEx(uint32 screenIndex, sint32 x, sint32 y, const char* str);

	void InitializeOSScreen();
}
