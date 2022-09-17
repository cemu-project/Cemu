#include "input/emulated/ClassicController.h"

#include "input/api/Controller.h"
#include "input/api/SDL/SDLController.h"

#include <wx/intl.h>

ClassicController::ClassicController(size_t player_index)
	: WPADController(player_index, kDataFormat_CLASSIC)
{
}

uint32 ClassicController::get_emulated_button_flag(uint32 id) const
{
	return s_get_emulated_button_flag(id);
}

uint32 ClassicController::s_get_emulated_button_flag(uint32 id)
{
	switch (id)
	{
	case kButtonId_A:
		return kCLButton_A;
	case kButtonId_B:
		return kCLButton_B;
	case kButtonId_X:
		return kCLButton_X;
	case kButtonId_Y:
		return kCLButton_Y;

	case kButtonId_Plus:
		return kCLButton_Plus;
	case kButtonId_Minus:
		return kCLButton_Minus;

	case kButtonId_Up:
		return kCLButton_Up;
	case kButtonId_Down:
		return kCLButton_Down;
	case kButtonId_Left:
		return kCLButton_Left;
	case kButtonId_Right:
		return kCLButton_Right;

	case kButtonId_L:
		return kCLButton_L;
	case kButtonId_ZL:
		return kCLButton_ZL;
	case kButtonId_R:
		return kCLButton_R;
	case kButtonId_ZR:
		return kCLButton_ZR;
	}

	return 0;
}

std::string_view ClassicController::get_button_name(ButtonId id)
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
	case kButtonId_Home: return wxTRANSLATE("home");

	case kButtonId_Up: return wxTRANSLATE("up");
	case kButtonId_Down: return wxTRANSLATE("down");
	case kButtonId_Left: return wxTRANSLATE("left");
	case kButtonId_Right: return wxTRANSLATE("right");

	case kButtonId_StickL_Up: return wxTRANSLATE("up");
	case kButtonId_StickL_Down: return wxTRANSLATE("down");
	case kButtonId_StickL_Left: return wxTRANSLATE("left");
	case kButtonId_StickL_Right: return wxTRANSLATE("right");

	case kButtonId_StickR_Up: return wxTRANSLATE("up");
	case kButtonId_StickR_Down: return wxTRANSLATE("down");
	case kButtonId_StickR_Left: return wxTRANSLATE("left");
	case kButtonId_StickR_Right: return wxTRANSLATE("right");

	default:
		return "";
	}
}

glm::vec2 ClassicController::get_axis() const
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

glm::vec2 ClassicController::get_rotation() const
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

glm::vec2 ClassicController::get_trigger() const
{
	const auto left = get_axis_value(kButtonId_ZL);
	const auto right = get_axis_value(kButtonId_ZR);
	return { left, right };
}

bool ClassicController::set_default_mapping(const std::shared_ptr<ControllerBase>& controller)
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

				{kButtonId_StickL_Up, kAxisYN},
				{kButtonId_StickL_Down, kAxisYP},
				{kButtonId_StickL_Left, kAxisXN},
				{kButtonId_StickL_Right, kAxisXP},
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