#include "GuiSystem.h"

namespace GuiSystem
{
std::function<std::string(uint32)> s_key_code_to_string;
void registerKeyCodeToStringCallback(const std::function<std::string(uint32)>& keyCodeToString)
{
	s_key_code_to_string = keyCodeToString;
}
void unregisterKeyCodeToStringCallback()
{
	s_key_code_to_string = {};
}
std::string keyCodeToString(uint32 key)
{
	if (!s_key_code_to_string)
		return "";
	return s_key_code_to_string(key);
}

WindowInfo s_window_info;

WindowInfo& getWindowInfo()
{
	return s_window_info;
}

void getWindowSize(int& w, int& h)
{
	w = s_window_info.width;
	h = s_window_info.height;
}

void getPadWindowSize(int& w, int& h)
{
	if (s_window_info.pad_open)
	{
		w = s_window_info.pad_width;
		h = s_window_info.pad_height;
	}
	else
	{
		w = 0;
		h = 0;
	}
}

void getWindowPhysSize(int& w, int& h)
{
	w = s_window_info.phys_width;
	h = s_window_info.phys_height;
}

void getPadWindowPhysSize(int& w, int& h)
{
	if (s_window_info.pad_open)
	{
		w = s_window_info.phys_pad_width;
		h = s_window_info.phys_pad_height;
	}
	else
	{
		w = 0;
		h = 0;
	}
}

double getWindowDPIScale()
{
	return s_window_info.dpi_scale;
}

double getPadDPIScale()
{
	return s_window_info.pad_open ? s_window_info.pad_dpi_scale.load() : 1.0;
}

bool isPadWindowOpen()
{
	return s_window_info.pad_open;
}

bool isKeyDown(uint32 key)
{
	return s_window_info.get_keystate(key);
}

bool isKeyDown(PlatformKeyCodes key)
{
	return s_window_info.get_keystate(key);
}

bool isFullScreen()
{
	return s_window_info.is_fullscreen;
}
}  // namespace GuiSystem