#pragma once

#include "input/api/Android/AndroidControllerProvider.h"
#include "input/api/Controller.h"
#include "input/motion/MotionSample.h"
#include "ControllerManager.h"

class AndroidController : public Controller<AndroidControllerProvider>
{
  public:
	AndroidController(std::string_view device_descriptor, std::string_view device_name);

	std::string_view api_name() const override
	{
		static_assert(to_string(InputAPI::Android) == "Android");
		return to_string(InputAPI::Android);
	}

	InputAPI::Type api() const override
	{
		return InputAPI::Android;
	}

	bool is_connected() override;

	bool has_motion() override;

	MotionSample get_motion_sample() override;

	bool has_rumble() override;

	void start_rumble() override;
	void stop_rumble() override;

	std::string get_button_name(uint64 button) const override;

	ControllerState raw_state() override;

  private:
	std::shared_ptr<ControllerManager::ControllerRuntimeState> m_state;
};
