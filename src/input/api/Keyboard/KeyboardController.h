#pragma once

#include "input/api/Keyboard/KeyboardControllerProvider.h"
#include "input/api/Controller.h"

class KeyboardController : public Controller<KeyboardControllerProvider>
{
public:
	KeyboardController();

	std::string_view api_name() const override // TODO: use constexpr virtual function with c++20
	{
		static_assert(to_string(InputAPI::Keyboard) == "Keyboard");
		return to_string(InputAPI::Keyboard);
	}
	InputAPI::Type api() const override { return InputAPI::Keyboard; }

	bool is_connected() override { return true; }

	bool has_axis() const override { return false; }

	std::string get_button_name(uint64 button) const override;

protected:
	ControllerState raw_state() override;

};
