#include "Cafe/CafeSystem.h"
#include "Cafe/OS/libs/swkbd/swkbd.h"
#include "Cafe/OS/libs/nsyskbd/nsyskbd.h"
#include "Cafe/OS/libs/nsyshid/nsyshid.h"
#include "input/InputManager.h"
#include "config/CemuConfig.h"

namespace CafeSystem
{
	static sint32 s_mouseWheelRemainder = 0;

	void NotifyMouseMove(bool padWindow, sint32 x, sint32 y)
	{
		auto& mouse = padWindow ? InputManager::instance().m_pad_mouse : InputManager::instance().m_main_mouse;
		std::scoped_lock lock(mouse.m_mutex);
		mouse.position = {x, y};
	}

	void NotifyMouseButton(bool padWindow, MouseButton button, bool pressed, sint32 x, sint32 y)
	{
		// the software keyboard is used by touch
		if (!padWindow && nsyshid::IsMouseInUse() && !swkbd_hasKeyboardInputHook())
		{
			nsyshid::SetHostMouseButton((uint8)button, pressed);
			return;
		}
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
		if (nsyshid::IsMouseInUse())
		{
			// high resolution wheels send fractions of a notch
			s_mouseWheelRemainder += rotation;
			const sint32 notches = s_mouseWheelRemainder / 120;
			s_mouseWheelRemainder -= notches * 120;
			if (notches != 0)
				nsyshid::AddHostMouseMotion(0, 0, notches);
			return;
		}
		InputManager::instance().m_mouse_wheel = rotation / 120.0f;
	}

	void NotifyMouseMotion(sint32 deltaX, sint32 deltaY)
	{
		nsyshid::AddHostMouseMotion(deltaX, deltaY, 0);
	}

	bool IsUSBMouseInUse()
	{
		return nsyshid::IsMouseInUse();
	}

	void SetUSBMouseEnabled(bool enabled)
	{
		GetConfig().emulated_usb_devices.emulate_mouse = enabled;
		nsyshid::UpdateEmulatedDevices();
	}

	void NotifyKeyEvent(uint8 hidUsage, bool pressed)
	{
		// typed text goes to the software keyboard instead
		if (pressed && swkbd_hasKeyboardInputHook())
			return;
		nsyskbd::SetHostKeyState(hidUsage, pressed);
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

	bool IsUSBKeyboardInUse()
	{
		return nsyskbd::IsHostKeyboardConnected();
	}

	void SetUSBKeyboardEnabled(bool enabled)
	{
		// picked up by the next nsyskbd poll
		GetConfig().emulated_usb_devices.emulate_keyboard = enabled;
	}

	void ReleaseInput()
	{
		nsyskbd::ReleaseHostKeys();
		nsyshid::ReleaseHostMouse();
		s_mouseWheelRemainder = 0;
	}
}
