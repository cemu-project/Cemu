#include "input/api/Keyboard/KeyboardController.h"

#include "gui/guiWrapper.h"

KeyboardController::KeyboardController()
	: base_type("keyboard", "Keyboard")
{
	
}

std::string KeyboardController::get_button_name(uint64 button) const
{
#if BOOST_OS_WINDOWS
	LONG scan_code = MapVirtualKeyA((UINT)button, MAPVK_VK_TO_VSC_EX);
	if(HIBYTE(scan_code))
		scan_code |= 0x100;

	// because MapVirtualKey strips the extended bit for some keys
	switch (button)
	{
	case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN: // arrow keys
	case VK_PRIOR: case VK_NEXT: // page up and page down
	case VK_END: case VK_HOME:
	case VK_INSERT: case VK_DELETE:
	case VK_DIVIDE: // numpad slash
	case VK_NUMLOCK:
	{
		scan_code |= 0x100; // set extended bit
		break;
	}
	}

	scan_code <<= 16;

	char key_name[128];
	if (GetKeyNameTextA(scan_code, key_name, std::size(key_name)) != 0)
		return key_name;
#endif
	
	return fmt::format("key_{}", button);
}

extern WindowInfo g_window_info;

ControllerState KeyboardController::raw_state()
{
	ControllerState result{};
	if (g_window_info.app_active)
	{
		static_assert(result.buttons.size() == std::size(g_window_info.keydown), "invalid size");
		for (uint32 i = wxKeyCode::WXK_BACK; i < result.buttons.size(); ++i)
		{
			if(const bool v = g_window_info.keydown[i])
			{
				result.buttons.set(i, v);
			}
		}
		// ignore generic key codes on Windows when there is also a left/right variant
#if BOOST_OS_WINDOWS
		result.buttons.set(VK_SHIFT, false);
		result.buttons.set(VK_CONTROL, false);
		result.buttons.set(VK_MENU, false);
#endif
	}
	return result;
}
