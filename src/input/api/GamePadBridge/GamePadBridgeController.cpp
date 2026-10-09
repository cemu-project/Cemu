#include "input/api/GamePadBridge/GamePadBridgeController.h"
#include "Cafe/GamePad/GamePadSink.h"
#include "Cafe/GamePad/drcbridge_ipc.h"
#include "config/ActiveSettings.h"
#include "input/emulated/VPADController.h"
#include <pugixml.hpp>

std::vector<std::shared_ptr<ControllerBase>> GamePadBridgeControllerProvider::get_controllers()
{
	return {std::make_shared<GamePadBridgeController>()};
}

GamePadBridgeController::GamePadBridgeController()
	: base_type("0", "Wii U GamePad")
{
	m_settings.motion = true; // it's the GamePad: its own gyro is what games expect (a saved profile overrides this)
}

// Takes the bridge's latest input once per new packet: buttons, sticks, touch and one motion integration step.
void GamePadBridgeController::Poll()
{
	drcb_input_state in;
	std::lock_guard lock(m_mutex);
	m_connected = GamePadSink::LatestInput(in);
	if (!m_connected)
	{
		m_state = ControllerState{};
		m_touchDown = false;
		return;
	}
	if (in.seq == m_lastSeq)
		return;
	m_lastSeq = in.seq;

	ControllerState st{};
	for (int bit = 0; bit < 19; bit++)
		if (in.buttons & (1u << bit))
			st.buttons.SetButtonState(bit, true);
	st.axis = {in.stick_l[0], in.stick_l[1]};
	st.rotation = {in.stick_r[0], in.stick_r[1]};
	m_state = std::move(st);

	m_prevTouch = m_touch;
	m_touchDown = in.touch_down != 0;
	if (m_touchDown)
		m_touch = {in.touch[0], in.touch[1]};

	// Same axes and units as the bridge's DSU server (bridge/src/dsu_server.cpp: accel negated, gyro y/z negated)
	// followed by Cemu's DSU path (DSUControllerProvider::integrate_motion: deg -> rad, accel y/z negated): the
	// combination that made Nintendo Land's camera follow the pad's tilt.
	if (m_lastInputNs != 0)
	{
		const float dt = std::clamp(float(in.t_received_ns - m_lastInputNs) / 1e9f, 0.0f, 1.0f);
		constexpr float kRad = 0.0174533f;
		m_motion.processMotionSample(dt, in.gyro[0] * kRad, -in.gyro[1] * kRad, -in.gyro[2] * kRad,
									 -in.accel[0], in.accel[1], in.accel[2]);
		m_motionSample = m_motion.getMotionSample();
	}
	m_lastInputNs = in.t_received_ns;
}

bool GamePadBridgeController::is_connected()
{
	Poll();
	std::lock_guard lock(m_mutex);
	return m_connected;
}

ControllerState GamePadBridgeController::raw_state()
{
	Poll();
	std::lock_guard lock(m_mutex);
	return m_state;
}

MotionSample GamePadBridgeController::get_motion_sample()
{
	Poll();
	std::lock_guard lock(m_mutex);
	return m_motionSample;
}

bool GamePadBridgeController::has_position()
{
	Poll();
	std::lock_guard lock(m_mutex);
	return m_touchDown;
}

glm::vec2 GamePadBridgeController::get_position()
{
	std::lock_guard lock(m_mutex);
	return m_touch;
}

glm::vec2 GamePadBridgeController::get_prev_position()
{
	std::lock_guard lock(m_mutex);
	return m_prevTouch;
}

PositionVisibility GamePadBridgeController::GetPositionVisibility()
{
	std::lock_guard lock(m_mutex);
	return m_touchDown ? PositionVisibility::FULL : PositionVisibility::NONE;
}

std::string GamePadBridgeController::get_button_name(uint64 button) const
{
	switch (button)
	{
	case kButton0: return "A";
	case kButton1: return "B";
	case kButton2: return "X";
	case kButton3: return "Y";
	case kButton4: return "Left";
	case kButton5: return "Right";
	case kButton6: return "Up";
	case kButton7: return "Down";
	case kButton8: return "ZL";
	case kButton9: return "ZR";
	case kButton10: return "L";
	case kButton11: return "R";
	case kButton12: return "Plus";
	case kButton13: return "Minus";
	case kButton14: return "Home";
	case kButton15: return "Stick L";
	case kButton16: return "Stick R";
	case kButton17: return "TV";
	case kButton18: return "Power";
	}
	return base_type::get_button_name(button);
}

// Same layout InputManager::save writes, so Cemu loads it like any profile.
void GamePadBridgeController::WriteProfile()
{
	fs::path path = ActiveSettings::GetConfigPath("controllerProfiles");
	std::error_code ec;
	fs::create_directories(path, ec);
	path /= _utf8ToPath(fmt::format("{}.xml", kProfileName));
	if (fs::exists(path, ec))
		return; // the user's to change from here on

	pugi::xml_document doc;
	auto decl = doc.append_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";
	decl.append_attribute("encoding") = "UTF-8";
	auto root = doc.append_child("emulated_controller");
	root.append_child("type").append_child(pugi::node_pcdata).set_value("Wii U GamePad");
	root.append_child("profile").append_child(pugi::node_pcdata).set_value(kProfileName);
	auto c = root.append_child("controller");
	c.append_child("api").append_child(pugi::node_pcdata).set_value(std::string{to_string(InputAPI::GamePadBridge)}.c_str());
	c.append_child("uuid").append_child(pugi::node_pcdata).set_value("0");
	c.append_child("display_name").append_child(pugi::node_pcdata).set_value("Wii U GamePad");
	c.append_child("motion").append_child(pugi::node_pcdata).set_value("true");
	for (const char* group : {"axis", "rotation", "trigger"})
	{
		auto n = c.append_child(group);
		n.append_child("deadzone").append_child(pugi::node_pcdata).set_value("0.1");
		n.append_child("range").append_child(pugi::node_pcdata).set_value("1");
	}
	auto mappings = c.append_child("mappings");
	for (const auto& [mapping, button] : VPADController::real_gamepad_mapping())
	{
		auto e = mappings.append_child("entry");
		e.append_child("mapping").append_child(pugi::node_pcdata).set_value(fmt::format("{}", mapping).c_str());
		e.append_child("button").append_child(pugi::node_pcdata).set_value(fmt::format("{}", button).c_str());
	}
	if (!doc.save_file(path.c_str()))
		cemuLog_log(LogType::Force, "GamePad Bridge: couldn't write the controller profile {}", _pathToUtf8(path));
}
