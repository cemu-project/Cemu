#include "input/emulated/VPADController.h"
#include "input/api/Controller.h"
#include "input/api/SDL/SDLController.h"
#include "gui/guiWrapper.h"
#include "input/InputManager.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/CafeSystem.h"
#include <wx/intl.h>

enum ControllerVPADMapping2 : uint32
{
	VPAD_A = 0x8000,
	VPAD_B = 0x4000,
	VPAD_X = 0x2000,
	VPAD_Y = 0x1000,

	VPAD_L = 0x0020,
	VPAD_R = 0x0010,
	VPAD_ZL = 0x0080,
	VPAD_ZR = 0x0040,

	VPAD_PLUS = 0x0008,
	VPAD_MINUS = 0x0004,
	VPAD_HOME = 0x0002,

	VPAD_UP = 0x0200,
	VPAD_DOWN = 0x0100,
	VPAD_LEFT = 0x0800,
	VPAD_RIGHT = 0x0400,

	VPAD_STICK_R = 0x00020000,
	VPAD_STICK_L = 0x00040000,

	VPAD_STICK_L_UP = 0x10000000,
	VPAD_STICK_L_DOWN = 0x08000000,
	VPAD_STICK_L_LEFT = 0x40000000,
	VPAD_STICK_L_RIGHT = 0x20000000,

	VPAD_STICK_R_UP = 0x01000000,
	VPAD_STICK_R_DOWN = 0x00800000,
	VPAD_STICK_R_LEFT = 0x04000000,
	VPAD_STICK_R_RIGHT = 0x02000000,

	// special flag
	VPAD_REPEAT = 0x80000000,
};

void VPADController::VPADRead(VPADStatus_t& status, const BtnRepeat& repeat)
{
	controllers_update_states();
	m_mic_active = false;
	m_screen_active = false;
	for (uint32 i = kButtonId_A; i < kButtonId_Max; ++i)
	{
		// axis will be aplied later
		if (is_axis_mapping(i))
			continue;

		if (is_mapping_down(i))
		{
			const uint32 value = get_emulated_button_flag(i);
			if (value == 0)
			{
				// special buttons
				if (i == kButtonId_Mic)
					m_mic_active = true;
				else if (i == kButtonId_Screen)
					m_screen_active = true;

				continue;
			}

			status.hold |= value;
		}
	}

	m_homebutton_down |= is_home_down();

	const auto axis = get_axis();
	status.leftStick.x = axis.x;
	status.leftStick.y = axis.y;

	constexpr float kAxisThreshold = 0.5f;
	constexpr float kHoldAxisThreshold = 0.1f;
	const uint32 last_hold = m_last_holdvalue;

	if (axis.x <= -kAxisThreshold || (HAS_FLAG(last_hold, VPAD_STICK_L_LEFT) && axis.x <= -kHoldAxisThreshold))
		status.hold |= VPAD_STICK_L_LEFT;
	else if (axis.x >= kAxisThreshold || (HAS_FLAG(last_hold, VPAD_STICK_L_RIGHT) && axis.x >= kHoldAxisThreshold))
		status.hold |= VPAD_STICK_L_RIGHT;

	if (axis.y <= -kAxisThreshold || (HAS_FLAG(last_hold, VPAD_STICK_L_DOWN) && axis.y <= -kHoldAxisThreshold))
		status.hold |= VPAD_STICK_L_DOWN;
	else if (axis.y >= kAxisThreshold || (HAS_FLAG(last_hold, VPAD_STICK_L_UP) && axis.y >= kHoldAxisThreshold))
		status.hold |= VPAD_STICK_L_UP;

	const auto rotation = get_rotation();
	status.rightStick.x = rotation.x;
	status.rightStick.y = rotation.y;

	if (rotation.x <= -kAxisThreshold || (HAS_FLAG(last_hold, VPAD_STICK_R_LEFT) && rotation.x <= -kHoldAxisThreshold))
		status.hold |= VPAD_STICK_R_LEFT;
	else if (rotation.x >= kAxisThreshold || (HAS_FLAG(last_hold, VPAD_STICK_R_RIGHT) && rotation.x >=
		kHoldAxisThreshold))
		status.hold |= VPAD_STICK_R_RIGHT;

	if (rotation.y <= -kAxisThreshold || (HAS_FLAG(last_hold, VPAD_STICK_R_DOWN) && rotation.y <= -kHoldAxisThreshold))
		status.hold |= VPAD_STICK_R_DOWN;
	else if (rotation.y >= kAxisThreshold || (HAS_FLAG(last_hold, VPAD_STICK_R_UP) && rotation.y >= kHoldAxisThreshold))
		status.hold |= VPAD_STICK_R_UP;

	// button repeat
	const auto now = std::chrono::high_resolution_clock::now();
	if (status.hold != m_last_holdvalue)
	{
		m_last_hold_change = m_last_pulse = now;
	}

	if (repeat.pulse > 0)
	{
		if (m_last_hold_change + std::chrono::milliseconds(repeat.delay) >= now)
		{
			if ((m_last_pulse + std::chrono::milliseconds(repeat.pulse)) < now)
			{
				m_last_pulse = now;
				status.hold |= VPAD_REPEAT;
			}
		}
	}

	// general
	status.release = m_last_holdvalue & ~status.hold;
	status.trig = ~m_last_holdvalue & status.hold;
	m_last_holdvalue = status.hold;

	// touch
	update_touch(status);

	// motion
	status.dir.x = {1, 0, 0};
	status.dir.y = {0, 1, 0};
	status.dir.z = {0, 0, 1};
	status.accXY = {1.0f, 0.0f};
	update_motion(status);
}

void VPADController::update()
{
	EmulatedController::update();

	if (!CafeSystem::IsTitleRunning())
		return;

	std::unique_lock lock(m_rumble_mutex);
	if (m_rumble_queue.empty())
	{
		m_parser = 0;
		lock.unlock();

		stop_rumble();

		return;
	}

	const auto tick = now_cached();
	if (std::chrono::duration_cast<std::chrono::milliseconds>(tick - m_last_rumble_check).count() < 1000 / 60)
		return;

	m_last_rumble_check = tick;

	const auto& it = m_rumble_queue.front();
	if (it[m_parser])
		start_rumble();
	else
		stop_rumble();

	++m_parser;
	if (m_parser >= it.size())
	{
		m_rumble_queue.pop();
		m_parser = 0;
	}
}

void VPADController::update_touch(VPADStatus_t& status)
{
	status.tpData.touch = kTpTouchOff;
	status.tpData.validity = kTpInvalid;
	// keep x,y from previous update
	// NGDK (Neko Game Development Kit 2) games (e.g. Mysterios Cities of Gold) rely on x/y remaining intact after touch is released
	status.tpData.x = (uint16)m_last_touch_position.x;
	status.tpData.y = (uint16)m_last_touch_position.y;

	auto& instance = InputManager::instance();
	bool pad_view;
	if (has_position())
	{
		const auto mouse = get_position();

		status.tpData.touch = kTpTouchOn;
		status.tpData.validity = kTpValid;
		status.tpData.x = (uint16)(mouse.x * 3883.0f + 92.0f);
		status.tpData.y = (uint16)(4095.0f - mouse.y * 3694.0f - 254.0f);

		m_last_touch_position = glm::ivec2{status.tpData.x, status.tpData.y};
	}
	else if (const auto left_mouse = instance.get_left_down_mouse_info(&pad_view))
	{
		glm::ivec2 image_pos, image_size;
		LatteRenderTarget_getScreenImageArea(&image_pos.x, &image_pos.y, &image_size.x, &image_size.y, nullptr, nullptr, pad_view);

		glm::vec2 relative_mouse_pos = left_mouse.value() - image_pos;
		relative_mouse_pos = { std::min(relative_mouse_pos.x, (float)image_size.x), std::min(relative_mouse_pos.y, (float)image_size.y) };
		relative_mouse_pos = { std::max(relative_mouse_pos.x, 0.0f), std::max(relative_mouse_pos.y, 0.0f) };
		relative_mouse_pos /= image_size;

		status.tpData.touch = kTpTouchOn;
		status.tpData.validity = kTpValid;
		status.tpData.x = (uint16)((relative_mouse_pos.x * 3883.0f) + 92.0f);
		status.tpData.y = (uint16)(4095.0f - (relative_mouse_pos.y * 3694.0f) - 254.0f);

		m_last_touch_position = glm::ivec2{ status.tpData.x, status.tpData.y };

		/*cemuLog_log(LogType::Force, "TDATA: {},{} -> {},{} -> {},{} -> {},{} -> {},{} -> {},{}",
			left_mouse->x, left_mouse->y,
			(left_mouse.value() - image_pos).x, (left_mouse.value() - image_pos).y,
			relative_mouse_pos.x, relative_mouse_pos.y,
			(uint16)(relative_mouse_pos.x * 3883.0 + 92.0), (uint16)(4095.0 - relative_mouse_pos.y * 3694.0 - 254.0),
			status.tpData.x.value(), status.tpData.y.value(), status.tpData.x.bevalue(), status.tpData.y.bevalue()
		);*/
	}

	status.tpProcessed1 = status.tpData;
	status.tpProcessed2 = status.tpData;
}

void VPADController::update_motion(VPADStatus_t& status)
{
	if (has_motion())
	{
		auto motionSample = get_motion_data();

		glm::vec3 acc;
		motionSample.getVPADAccelerometer(&acc[0]);
		//const auto& acc = motionSample.getVPADAccelerometer();
		status.acc.x = acc.x;
		status.acc.y = acc.y;
		status.acc.z = acc.z;
		status.accMagnitude = motionSample.getVPADAccMagnitude();
		status.accAcceleration = motionSample.getVPADAccAcceleration();

		glm::vec3 gyroChange;
		motionSample.getVPADGyroChange(&gyroChange[0]);
		//const auto& gyroChange = motionSample.getVPADGyroChange();
		status.gyroChange.x = gyroChange.x;
		status.gyroChange.y = gyroChange.y;
		status.gyroChange.z = gyroChange.z;

		//debug_printf("GyroChange %7.2lf %7.2lf %7.2lf\n", (float)status.gyroChange.x, (float)status.gyroChange.y, (float)status.gyroChange.z);

		glm::vec3 gyroOrientation;
		motionSample.getVPADOrientation(&gyroOrientation[0]);
		//const auto& gyroOrientation = motionSample.getVPADOrientation();
		status.gyroOrientation.x = gyroOrientation.x;
		status.gyroOrientation.y = gyroOrientation.y;
		status.gyroOrientation.z = gyroOrientation.z;

		float attitude[9];
		motionSample.getVPADAttitudeMatrix(attitude);
		status.dir.x.x = attitude[0];
		status.dir.x.y = attitude[1];
		status.dir.x.z = attitude[2];
		status.dir.y.x = attitude[3];
		status.dir.y.y = attitude[4];
		status.dir.y.z = attitude[5];
		status.dir.z.x = attitude[6];
		status.dir.z.y = attitude[7];
		status.dir.z.z = attitude[8];
		return;
	}

	bool pad_view;
	auto& input_manager = InputManager::instance();
	if (const auto right_mouse = input_manager.get_right_down_mouse_info(&pad_view))
	{
		const Vector2<float> mousePos(right_mouse->x, right_mouse->y);

		int w, h;
		if (pad_view)
			gui_getPadWindowPhysSize(w, h);
		else
			gui_getWindowPhysSize(w, h);

		float wx = mousePos.x / w;
		float wy = mousePos.y / h;

		static glm::vec3 m_lastGyroRotation{}, m_startGyroRotation{};
		static bool m_startGyroRotationSet{};

		float rotX = (wy * 2 - 1.0f) * 135.0f; // up/down best
		float rotY = (wx * 2 - 1.0f) * -180.0f; // left/right
		float rotZ = input_manager.m_mouse_wheel * 14.0f + m_lastGyroRotation.z;
		input_manager.m_mouse_wheel = 0.0f;

		if (!m_startGyroRotationSet)
		{
			m_startGyroRotation = {rotX, rotY, rotZ};
			m_startGyroRotationSet = true;
		}

		/*	debug_printf("\n\ngyro:\n<%.02f, %.02f, %.02f>\n\n",
				rotX, rotY, rotZ);*/

		Quaternion<float> q(rotX, rotY, rotZ);
		auto rot = q.GetTransposedRotationMatrix();

		/*m_forward = std::get<0>(rot);
		m_right = std::get<1>(rot);
		m_up = std::get<2>(rot);*/

		status.dir.x = std::get<0>(rot);
		status.dir.y = std::get<1>(rot);
		status.dir.z = std::get<2>(rot);

		/*debug_printf("rot:\n<%.02f, %.02f, %.02f>\n<%.02f, %.02f, %.02f>\n<%.02f, %.02f, %.02f>\n\n",
			(float)status.dir.x.x, (float)status.dir.x.y, (float)status.dir.x.z,
			(float)status.dir.y.x, (float)status.dir.y.y, (float)status.dir.y.z,
			(float)status.dir.z.x, (float)status.dir.z.y, (float)status.dir.z.z);*/

		glm::vec3 rotation(rotX - m_lastGyroRotation.x, (rotY - m_lastGyroRotation.y) * 15.0f,
		                   rotZ - m_lastGyroRotation.z);

		rotation.x = std::min(1.0f, std::max(-1.0f, rotation.x / 360.0f));
		rotation.y = std::min(1.0f, std::max(-1.0f, rotation.y / 360.0f));
		rotation.z = std::min(1.0f, std::max(-1.0f, rotation.z / 360.0f));

		/*debug_printf("\n\ngyro:\n<%.02f, %.02f, %.02f>\n\n",
			rotation.x, rotation.y, rotation.z);*/

		constexpr float pi2 = (float)(M_PI * 2);
		status.gyroChange = {rotation.x, rotation.y, rotation.z};
		status.gyroOrientation = {rotation.x, rotation.y, rotation.z};
		//status.angle = { rotation.x / pi2, rotation.y / pi2, rotation.z / pi2 };

		status.acc = {rotation.x, rotation.y, rotation.z};
		status.accAcceleration = 1.0f;
		status.accMagnitude = 1.0f;

		status.accXY = {1.0f, 0.0f};

		m_lastGyroRotation = {rotX, rotY, rotZ};
	}
}


std::string_view VPADController::get_button_name(ButtonId id)
{
	switch (id)
	{
	case kButtonId_A: return "A";
	case kButtonId_B: return "B";
	case kButtonId_X: return "X";
	case kButtonId_Y: return "Y";
	case kButtonId_L: return "L";
	case kButtonId_R: return "R";
	case kButtonId_ZL: return "ZL";
	case kButtonId_ZR: return "ZR";
	case kButtonId_Plus: return "+";
	case kButtonId_Minus: return "-";
	case kButtonId_Up: return wxTRANSLATE("up");
	case kButtonId_Down: return wxTRANSLATE("down");
	case kButtonId_Left: return wxTRANSLATE("left");
	case kButtonId_Right: return wxTRANSLATE("right");
	case kButtonId_StickL: return wxTRANSLATE("click");
	case kButtonId_StickR: return wxTRANSLATE("click");
	case kButtonId_StickL_Up: return wxTRANSLATE("up");
	case kButtonId_StickL_Down: return wxTRANSLATE("down");
	case kButtonId_StickL_Left: return wxTRANSLATE("left");
	case kButtonId_StickL_Right: return wxTRANSLATE("right");
	case kButtonId_StickR_Up: return wxTRANSLATE("up");
	case kButtonId_StickR_Down: return wxTRANSLATE("down");
	case kButtonId_StickR_Left: return wxTRANSLATE("left");
	case kButtonId_StickR_Right: return wxTRANSLATE("right");
	case kButtonId_Home: return wxTRANSLATE("home");
	default:
		cemu_assert_debug(false);
		return "";
	}
}

void VPADController::clear_rumble()
{
	std::scoped_lock lock(m_rumble_mutex);
	while (!m_rumble_queue.empty())
		m_rumble_queue.pop();

	m_parser = 0;
}

bool VPADController::push_rumble(uint8* pattern, uint8 length)
{
	if (pattern == nullptr || length == 0)
	{
		clear_rumble();
		return true;
	}

	std::scoped_lock lock(m_rumble_mutex);
	if (m_rumble_queue.size() >= 5)
	{
		cemuLog_logDebugOnce(LogType::Force, "VPADControlMotor(): Pattern too long");
		return false;
	}

	// len = max 15 bytes of data = 120 bits = 1 seconds
	// we will use 60 hz for 1 second
	std::vector<bool> bitset;
	int byte = 0;
	int len = (int)length;
	while (len > 0)
	{
		const uint8 p = pattern[byte];
		for (int j = 0; j < 8 && j < len; j += 2)
		{
			const bool set = (p & (3 << j)) != 0;
			bitset.push_back(set);
		}

		++byte;
		len -= 8;
	}


	m_rumble_queue.emplace(std::move(bitset));
	m_last_rumble_check = {};

	return true;
}

uint32 VPADController::get_emulated_button_flag(uint32 id) const
{
	switch (id)
	{
	case kButtonId_A: return VPAD_A;
	case kButtonId_B: return VPAD_B;
	case kButtonId_X: return VPAD_X;
	case kButtonId_Y: return VPAD_Y;
	case kButtonId_L: return VPAD_L;
	case kButtonId_R: return VPAD_R;
	case kButtonId_ZL: return VPAD_ZL;
	case kButtonId_ZR: return VPAD_ZR;
	case kButtonId_Plus: return VPAD_PLUS;
	case kButtonId_Minus: return VPAD_MINUS;
	case kButtonId_Up: return VPAD_UP;
	case kButtonId_Down: return VPAD_DOWN;
	case kButtonId_Left: return VPAD_LEFT;
	case kButtonId_Right: return VPAD_RIGHT;
	case kButtonId_StickL: return VPAD_STICK_L;
	case kButtonId_StickR: return VPAD_STICK_R;

	case kButtonId_StickL_Up: return VPAD_STICK_L_UP;
	case kButtonId_StickL_Down: return VPAD_STICK_L_DOWN;
	case kButtonId_StickL_Left: return VPAD_STICK_L_LEFT;
	case kButtonId_StickL_Right: return VPAD_STICK_L_RIGHT;

	case kButtonId_StickR_Up: return VPAD_STICK_R_UP;
	case kButtonId_StickR_Down: return VPAD_STICK_R_DOWN;
	case kButtonId_StickR_Left: return VPAD_STICK_R_LEFT;
	case kButtonId_StickR_Right: return VPAD_STICK_R_RIGHT;
	}
	return 0;
}

glm::vec2 VPADController::get_axis() const
{
	const auto left = get_axis_value(kButtonId_StickL_Left);
	const auto right = get_axis_value(kButtonId_StickL_Right);

	const auto up = get_axis_value(kButtonId_StickL_Up);
	const auto down = get_axis_value(kButtonId_StickL_Down);

	glm::vec2 result;
	result.x = (left > right) ? -left : right;
	result.y = (up > down) ? up : -down;
	return length(result) > 1.0f ? normalize(result) : result;
}

glm::vec2 VPADController::get_rotation() const
{
	const auto left = get_axis_value(kButtonId_StickR_Left);
	const auto right = get_axis_value(kButtonId_StickR_Right);

	const auto up = get_axis_value(kButtonId_StickR_Up);
	const auto down = get_axis_value(kButtonId_StickR_Down);

	glm::vec2 result;
	result.x = (left > right) ? -left : right;
	result.y = (up > down) ? up : -down;
	return length(result) > 1.0f ? normalize(result) : result;
}

glm::vec2 VPADController::get_trigger() const
{
	const auto left = get_axis_value(kButtonId_ZL);
	const auto right = get_axis_value(kButtonId_ZR);
	return {left, right};
}

bool VPADController::set_default_mapping(const std::shared_ptr<ControllerBase>& controller)
{
	std::vector<std::pair<uint64, uint64>> mapping;
	switch (controller->api())
	{
	case InputAPI::SDLController: {
		const auto sdl_controller = std::static_pointer_cast<SDLController>(controller);
		if (sdl_controller->get_guid() == SDLController::kLeftJoyCon)
		{
			mapping =
			{
				{kButtonId_L, kButton9},
				{kButtonId_ZL, kTriggerXP},

				{kButtonId_Minus, kButton4},

				{kButtonId_Up, kButton11},
				{kButtonId_Down, kButton12},
				{kButtonId_Left, kButton13},
				{kButtonId_Right, kButton14},

				{kButtonId_StickL, kButton7},

				{kButtonId_StickL_Up, kAxisYN},
				{kButtonId_StickL_Down, kAxisYP},
				{kButtonId_StickL_Left, kAxisXN},
				{kButtonId_StickL_Right, kAxisXP},

				{kButtonId_Mic, kButton15},
			};
		}
		else if (sdl_controller->get_guid() == SDLController::kRightJoyCon)
		{
			mapping =
			{
				{kButtonId_A, kButton0},
				{kButtonId_B, kButton1},
				{kButtonId_X, kButton2},
				{kButtonId_Y, kButton3},

				{kButtonId_R, kButton10},
				{kButtonId_ZR, kTriggerYP},

				{kButtonId_Plus, kButton6},

				{kButtonId_StickR, kButton8},

				{kButtonId_StickR_Up, kRotationYN},
				{kButtonId_StickR_Down, kRotationYP},
				{kButtonId_StickR_Left, kRotationXN},
				{kButtonId_StickR_Right, kRotationXP},
			};
		}
		else if (sdl_controller->get_guid() == SDLController::kSwitchProController)
		{
			// Switch Pro Controller is similar to default mapping, but with a/b and x/y swapped
			mapping =
			{
				{kButtonId_A, kButton0},
				{kButtonId_B, kButton1},
				{kButtonId_X, kButton2},
				{kButtonId_Y, kButton3},

				{kButtonId_L, kButton9},
				{kButtonId_R, kButton10},
				{kButtonId_ZL, kTriggerXP},
				{kButtonId_ZR, kTriggerYP},

				{kButtonId_Plus, kButton6},
				{kButtonId_Minus, kButton4},

				{kButtonId_Up, kButton11},
				{kButtonId_Down, kButton12},
				{kButtonId_Left, kButton13},
				{kButtonId_Right, kButton14},

				{kButtonId_StickL, kButton7},
				{kButtonId_StickR, kButton8},

				{kButtonId_StickL_Up, kAxisYN},
				{kButtonId_StickL_Down, kAxisYP},
				{kButtonId_StickL_Left, kAxisXN},
				{kButtonId_StickL_Right, kAxisXP},

				{kButtonId_StickR_Up, kRotationYN},
				{kButtonId_StickR_Down, kRotationYP},
				{kButtonId_StickR_Left, kRotationXN},
				{kButtonId_StickR_Right, kRotationXP},
			};
		}
		else
		{
			mapping =
			{
				{kButtonId_A, kButton1},
				{kButtonId_B, kButton0},
				{kButtonId_X, kButton3},
				{kButtonId_Y, kButton2},

				{kButtonId_L, kButton9},
				{kButtonId_R, kButton10},
				{kButtonId_ZL, kTriggerXP},
				{kButtonId_ZR, kTriggerYP},

				{kButtonId_Plus, kButton6},
				{kButtonId_Minus, kButton4},

				{kButtonId_Up, kButton11},
				{kButtonId_Down, kButton12},
				{kButtonId_Left, kButton13},
				{kButtonId_Right, kButton14},

				{kButtonId_StickL, kButton7},
				{kButtonId_StickR, kButton8},

				{kButtonId_StickL_Up, kAxisYN},
				{kButtonId_StickL_Down, kAxisYP},
				{kButtonId_StickL_Left, kAxisXN},
				{kButtonId_StickL_Right, kAxisXP},

				{kButtonId_StickR_Up, kRotationYN},
				{kButtonId_StickR_Down, kRotationYP},
				{kButtonId_StickR_Left, kRotationXN},
				{kButtonId_StickR_Right, kRotationXP},
			};
		}
		break;
	}
	case InputAPI::XInput:
	{
		mapping =
		{
			{kButtonId_A, kButton13},
			{kButtonId_B, kButton12},
			{kButtonId_X, kButton15},
			{kButtonId_Y, kButton14},

			{kButtonId_L, kButton8},
			{kButtonId_R, kButton9},
			{kButtonId_ZL, kTriggerXP},
			{kButtonId_ZR, kTriggerYP},

			{kButtonId_Plus, kButton4},
			{kButtonId_Minus, kButton5},

			{kButtonId_Up, kButton0},
			{kButtonId_Down, kButton1},
			{kButtonId_Left, kButton2},
			{kButtonId_Right, kButton3},

			{kButtonId_StickL, kButton6},
			{kButtonId_StickR, kButton7},

			{kButtonId_StickL_Up, kAxisYP},
			{kButtonId_StickL_Down, kAxisYN},
			{kButtonId_StickL_Left, kAxisXN},
			{kButtonId_StickL_Right, kAxisXP},

			{kButtonId_StickR_Up, kRotationYP},
			{kButtonId_StickR_Down, kRotationYN},
			{kButtonId_StickR_Left, kRotationXN},
			{kButtonId_StickR_Right, kRotationXP},
		};
		
		break;
	}
	}

	bool mapping_updated = false;
	std::for_each(mapping.cbegin(), mapping.cend(), [this, &controller, &mapping_updated](const auto& m)
		{
			if (m_mappings.find(m.first) == m_mappings.cend())
			{
				set_mapping(m.first, controller, m.second);
				mapping_updated = true;
			}
		});

	return mapping_updated;
}

void VPADController::load(const pugi::xml_node& node)
{
	if (const auto value = node.child("toggle_display"))
		m_screen_active_toggle = ConvertString<bool>(value.child_value());
}

void VPADController::save(pugi::xml_node& node)
{
	node.append_child("toggle_display").append_child(pugi::node_pcdata).set_value(fmt::format("{}", (int)m_screen_active_toggle).c_str());
}
