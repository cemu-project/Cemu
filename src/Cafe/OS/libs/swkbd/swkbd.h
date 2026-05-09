#pragma once

#include "Cafe/OS/RPL/COSModule.h"

namespace swkbd
{
	constexpr uint32 BACKSPACE_KEYCODE = 8;
	constexpr uint32 BACKWARDS_DELETE_KEYCODE = 127;
	constexpr uint32 RETURN_KEYCODE = 13;
	constexpr uint32 CONTROL_KEYCODE = 32;

	class swkbdCallbacks
	{
	  public:
		virtual void showSoftwareKeyboard(const std::string& initialText, sint32 maxLength) = 0;
		virtual void hideSoftwareKeyboard() = 0;
	};

	void setSwkbdCallbacks(std::shared_ptr<swkbdCallbacks> callbacks);

	void render(bool mainWindow);
	bool hasKeyboardInputHook();
	void keyInput(uint32 keyCode);

	COSModule* GetModule();
} // namespace swkbd
