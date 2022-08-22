#pragma once

#include "input/api/XInput/XInputControllerProvider.h"
#include "input/api/Controller.h"

class XInputController : public Controller<XInputControllerProvider>
{
public:
	XInputController(uint32 index);
	
	std::string_view api_name() const override
	{
		static_assert(to_string(InputAPI::XInput) == "XInput");
		return to_string(InputAPI::XInput);
	}
	InputAPI::Type api() const override { return InputAPI::XInput; }
	
	bool connect() override;
	bool is_connected() override;
	
	bool has_rumble() override { return m_has_rumble; }
	bool has_battery() override { return m_has_battery; }
	bool has_low_battery() override;
	
	void start_rumble() override;
	void stop_rumble() override;

	std::string get_button_name(uint64 button) const override;

protected:
	ControllerState raw_state() override;

private:
	uint32 m_index;
	bool m_connected = false;
	bool m_has_battery = false;
	bool m_has_rumble = false;
};
