#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/gx2/GX2.h"
#include "Cafe/HW/Latte/Core/Latte.h"

#include "Cafe/OS/libs/coreinit/coreinit_OSScreen_font.h"
#include "Cafe/OS/libs/coreinit/coreinit_OSScreen.h"

#define OSSCREEN_TV		(0)
#define OSSCREEN_DRC	(1)

namespace coreinit
{

	struct
	{
		sint32 x;
		sint32 y;
		sint32 pitch;
	}screenSizes[2] =
	{
		{ 1280, 720, 1280}, // TV
		{ 896, 480, 896 } // DRC (values might be incorrect)
	};

	void* currentScreenBasePtr[2] = { 0 };

	void _OSScreen_Clear(uint32 screenIndex, uint32 color)
	{
		if (!currentScreenBasePtr[screenIndex])
			return;

		uint32* output = (uint32*)currentScreenBasePtr[screenIndex];
		sint32 sizeInPixels = screenSizes[screenIndex].pitch * screenSizes[screenIndex].y;
		color = _swapEndianU32(color);
		for (sint32 i = 0; i < sizeInPixels; i++)
		{
			*output = color;
			output++;
		}
	}

	void coreinitExport_OSScreenInit(PPCInterpreter_t* hCPU)
	{
		// todo - init VI registers?

		osLib_returnFromFunction(hCPU, 0);
	}

	void coreinitExport_OSScreenGetBufferSizeEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(screenIndex, 0);
		cemu_assert(screenIndex < 2);
		uint32 bufferSize = screenSizes[screenIndex].pitch * screenSizes[screenIndex].y * 4 * 2;
		osLib_returnFromFunction(hCPU, bufferSize);
	}

	void _updateCurrentDrawScreen(sint32 screenIndex)
	{
		uint32 screenDataSize = screenSizes[screenIndex].pitch * screenSizes[screenIndex].y * 4;

		if ((LatteGPUState.osScreen.screen[screenIndex].flipRequestCount & 1) != 0)
			currentScreenBasePtr[screenIndex] = memory_getPointerFromPhysicalOffset(LatteGPUState.osScreen.screen[screenIndex].physPtr + screenDataSize);
		else
			currentScreenBasePtr[screenIndex] = memory_getPointerFromPhysicalOffset(LatteGPUState.osScreen.screen[screenIndex].physPtr);
	}

	void coreinitExport_OSScreenSetBufferEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(screenIndex, 0);
		ppcDefineParamU32(buffer, 1);
		cemu_assert(screenIndex < 2);
		LatteGPUState.osScreen.screen[screenIndex].physPtr = buffer;
		_updateCurrentDrawScreen(screenIndex);
		osLib_returnFromFunction(hCPU, 0);
	}

	void coreinitExport_OSScreenEnableEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(screenIndex, 0);
		ppcDefineParamU32(isEnabled, 1);
		cemu_assert(screenIndex < 2);
		LatteGPUState.osScreen.screen[screenIndex].isEnabled = isEnabled != 0;
		osLib_returnFromFunction(hCPU, 0);
	}

	void coreinitExport_OSScreenClearBufferEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(screenIndex, 0);
		ppcDefineParamU32(color, 1);
		cemu_assert(screenIndex < 2);
		_OSScreen_Clear(screenIndex, color);
		osLib_returnFromFunction(hCPU, 0);
	}

	void coreinitExport_OSScreenFlipBuffersEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(screenIndex, 0);
		cemu_assert(screenIndex < 2);
		cemuLog_logDebug(LogType::Force, "OSScreenFlipBuffersEx {}", screenIndex);
		LatteGPUState.osScreen.screen[screenIndex].flipRequestCount++;
		_updateCurrentDrawScreen(screenIndex);
		osLib_returnFromFunction(hCPU, 0);
	}

	void coreinitExport_OSScreenPutPixelEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(screenIndex, 0);
		ppcDefineParamS32(x, 1);
		ppcDefineParamS32(y, 2);
		ppcDefineParamU32(color, 3);
		if (screenIndex >= 2)
		{
			osLib_returnFromFunction(hCPU, 0);
			return;
		}
		if (x >= 0 && x < screenSizes[screenIndex].x && y >= 0 && y < screenSizes[screenIndex].y)
		{
			*(uint32*)((uint8*)currentScreenBasePtr[screenIndex] + (x + y * screenSizes[screenIndex].pitch) * 4) = _swapEndianS32(color);
		}
		osLib_returnFromFunction(hCPU, 0);
	}

	const char* osScreenCharset = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

	sint32 _getOSScreenFontCharIndex(char c)
	{
		const char* charset = osScreenCharset;
		while (*charset)
		{
			if (*charset == c)
			{
				return (sint32)(charset - osScreenCharset);
			}
			charset++;
		}
		return -1;
	}

	void coreinitExport_OSScreenPutFontEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(screenIndex, 0);
		ppcDefineParamS32(x, 1);
		ppcDefineParamS32(y, 2);
		ppcDefineParamStr(str, 3);

		// characters are:
		// 16 x 32 (including the margin)
		// with a margin of 4 x 8

		if (y < 0)
		{
			debug_printf("OSScreenPutFontEx: y has invalid value\n");
			osLib_returnFromFunction(hCPU, 0);
			return;
		}

		sint32 px = x * 16;
		sint32 py = y * 24;

		while (*str)
		{
			sint32 charIndex = _getOSScreenFontCharIndex(*str);
			if (charIndex >= 0)
			{
				const uint8* charBitmap = osscreenBitmapFont + charIndex * 50;
				for (sint32 fy = 0; fy < 25; fy++)
				{
					for (sint32 fx = 0; fx < 14; fx++)
					{
						if (((charBitmap[(fx / 8) + (fy) * 2] >> (7 - (fx & 7))) & 1) == 0)
							continue;
						*(uint32*)((uint8*)currentScreenBasePtr[screenIndex] + ((px + fx) + (py + fy) * screenSizes[screenIndex].pitch) * 4) = 0xFFFFFFFF;
					}
				}
			}
			px += 16;
			str++;
		}
		osLib_returnFromFunction(hCPU, 0);
	}

	void InitializeOSScreen()
	{
		osLib_addFunction("coreinit", "OSScreenInit", coreinitExport_OSScreenInit);
		osLib_addFunction("coreinit", "OSScreenGetBufferSizeEx", coreinitExport_OSScreenGetBufferSizeEx);
		osLib_addFunction("coreinit", "OSScreenSetBufferEx", coreinitExport_OSScreenSetBufferEx);
		osLib_addFunction("coreinit", "OSScreenEnableEx", coreinitExport_OSScreenEnableEx);
		osLib_addFunction("coreinit", "OSScreenClearBufferEx", coreinitExport_OSScreenClearBufferEx);
		osLib_addFunction("coreinit", "OSScreenFlipBuffersEx", coreinitExport_OSScreenFlipBuffersEx);
		osLib_addFunction("coreinit", "OSScreenPutPixelEx", coreinitExport_OSScreenPutPixelEx);
		osLib_addFunction("coreinit", "OSScreenPutFontEx", coreinitExport_OSScreenPutFontEx);
	}
}
