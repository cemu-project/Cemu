#include "Cafe/CafeSystem.h"
#include "Cafe/OS/libs/swkbd/swkbd.h"
#include "input/InputManager.h"

namespace CafeSystem
{
	void NotifyMouseMove(bool padWindow, sint32 x, sint32 y)
	{
		auto& mouse = padWindow ? InputManager::instance().m_pad_mouse : InputManager::instance().m_main_mouse;
		std::scoped_lock lock(mouse.m_mutex);
		mouse.position = {x, y};
	}

	void NotifyMouseButton(bool padWindow, MouseButton button, bool pressed, sint32 x, sint32 y)
	{
		auto& mouse = padWindow ? InputManager::instance().m_pad_mouse : InputManager::instance().m_main_mouse;
		std::scoped_lock lock(mouse.m_mutex);
		mouse.position = {x, y};
		if (button == MouseButton::Left)
		{
			mouse.left_down = pressed;
			mouse.left_down_toggle |= pressed;
		}
		else if (button == MouseButton::Right)
		{
			mouse.right_down = pressed;
			mouse.right_down_toggle |= pressed;
		}
	}

	void NotifyMouseWheel(sint32 rotation)
	{
		InputManager::instance().m_mouse_wheel = rotation / 120.0f;
	}

	void NotifyTextInput(uint32 codepoint)
	{
		if (swkbd_hasKeyboardInputHook())
			swkbd_keyInput(codepoint);
	}

	bool IsTextInputActive()
	{
		return swkbd_hasKeyboardInputHook();
	}
}
