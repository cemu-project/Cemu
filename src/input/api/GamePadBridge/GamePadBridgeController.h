#pragma once

#include "input/api/Controller.h"
#include "input/api/GamePadBridge/GamePadBridgeControllerProvider.h"
#include "input/motion/MotionHandler.h"

// Buttons are numbered like the bridge's DRCB_BTN_* bits (drcbridge_ipc.h): kButton0 = A ... kButton18 = Power.
// Sticks: axis = left, rotation = right, +y up. Touch: the position API (VPADController::update_touch).
class GamePadBridgeController : public Controller<GamePadBridgeControllerProvider>
{
public:
	GamePadBridgeController();

	std::string_view api_name() const override
	{
		static_assert(to_string(InputAPI::GamePadBridge) == "WiiUGamePad");
		return to_string(InputAPI::GamePadBridge);
	}
	InputAPI::Type api() const override { return InputAPI::GamePadBridge; }

	bool is_connected() override;

	bool has_motion() override { return true; }
	MotionSample get_motion_sample() override;

	bool has_position() override;
	glm::vec2 get_position() override;
	glm::vec2 get_prev_position() override;
	PositionVisibility GetPositionVisibility() override;

	std::string get_button_name(uint64 button) const override;

	// Writes the "Wii U GamePad" controller profile (Input settings > Profile) unless it exists: GamePad type, this
	// controller, one-to-one mapping, motion on. Lets the real pad be picked for any player.
	static void WriteProfile();
	static constexpr const char* kProfileName = "Wii U GamePad";

protected:
	ControllerState raw_state() override;

private:
	void Poll();

	std::mutex m_mutex;
	bool m_connected = false;
	uint64 m_lastSeq = 0;
	sint64 m_lastInputNs = 0;
	bool m_touchDown = false;
	glm::vec2 m_touch{}, m_prevTouch{};
	WiiUMotionHandler m_motion;
	MotionSample m_motionSample{};
	ControllerState m_state{};
};
