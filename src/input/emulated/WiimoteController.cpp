#include "input/emulated/WiimoteController.h"

#include "input/api/Controller.h"
#include "input/api/Wiimote/NativeWiimoteController.h"
#include <wx/intl.h>

WiimoteController::WiimoteController(size_t player_index)
	: WPADController(player_index, kDataFormat_CORE_ACC_DPD)
{
}

void WiimoteController::set_device_type(WPADDeviceType device_type)
{
	m_device_type = device_type;
	m_data_format = get_default_data_format();
}

bool WiimoteController::is_mpls_attached()
{
	return m_device_type == kWAPDevMPLS || m_device_type == kWAPDevMPLSClassic || m_device_type == kWAPDevMPLSFreeStyle;
}

uint32 WiimoteController::get_emulated_button_flag(uint32 id) const
{
	return s_get_emulated_button_flag(id);
}

bool WiimoteController::set_default_mapping(const std::shared_ptr<ControllerBase>& controller)
{
	std::vector<std::pair<uint64, uint64>> mapping;
	switch (controller->api())
	{
	case InputAPI::Wiimote: {
		const auto sdl_controller = std::static_pointer_cast<NativeWiimoteController>(controller);
		mapping =
		{
			{kButtonId_A, kWiimoteButton_A},
			{kButtonId_B, kWiimoteButton_B},
			{kButtonId_1, kWiimoteButton_One},
			{kButtonId_2, kWiimoteButton_Two},

			{kButtonId_Home, kWiimoteButton_Home},

			{kButtonId_Plus, kWiimoteButton_Plus},
			{kButtonId_Minus, kWiimoteButton_Minus},

			{kButtonId_Up, kWiimoteButton_Up},
			{kButtonId_Down, kWiimoteButton_Down},
			{kButtonId_Left, kWiimoteButton_Left},
			{kButtonId_Right, kWiimoteButton_Right},

			{kButtonId_Nunchuck_Z, kWiimoteButton_Z},
			{kButtonId_Nunchuck_C, kWiimoteButton_C},

			{kButtonId_Nunchuck_Up, kAxisYP},
			{kButtonId_Nunchuck_Down, kAxisYN},
			{kButtonId_Nunchuck_Left, kAxisXN},
			{kButtonId_Nunchuck_Right, kAxisXP},
		};
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

glm::vec2 WiimoteController::get_axis() const
{
	const auto left = get_axis_value(kButtonId_Nunchuck_Left);
	const auto right = get_axis_value(kButtonId_Nunchuck_Right);

	const auto up = get_axis_value(kButtonId_Nunchuck_Up);
	const auto down = get_axis_value(kButtonId_Nunchuck_Down);

	glm::vec2 result;
	result.x = (left > right) ? -left : right;
	result.y = (up > down) ? up : -down;
	return result;
}

glm::vec2 WiimoteController::get_rotation() const
{
	return {};
}

glm::vec2 WiimoteController::get_trigger() const
{
	return {};
}

void WiimoteController::load(const pugi::xml_node& node)
{
	base_type::load(node);

	if (const auto value = node.child("device_type"))
		m_device_type = ConvertString<WPADDeviceType>(value.child_value());
}

void WiimoteController::save(pugi::xml_node& node)
{
	base_type::save(node);

	node.append_child("device_type").append_child(pugi::node_pcdata).set_value(fmt::format("{}", (int)m_device_type).c_str());
}

uint32 WiimoteController::s_get_emulated_button_flag(uint32 id)
{
	switch (id)
	{
		case kButtonId_A:
			return kWPADButton_A;
		case kButtonId_B:
			return kWPADButton_B;
		case kButtonId_1:
			return kWPADButton_1;
		case kButtonId_2:
			return kWPADButton_2;

		case kButtonId_Plus:
			return kWPADButton_Plus;
		case kButtonId_Minus:
			return kWPADButton_Minus;
		case kButtonId_Home:
			return kWPADButton_Home;

		case kButtonId_Up:
			return kWPADButton_Up;
		case kButtonId_Down:
			return kWPADButton_Down;
		case kButtonId_Left:
			return kWPADButton_Left;
		case kButtonId_Right:
			return kWPADButton_Right;

		case kButtonId_Nunchuck_Z:
			return kWPADButton_Z;
		case kButtonId_Nunchuck_C:
			return kWPADButton_C;
	}

	return 0;
}

std::string_view WiimoteController::get_button_name(ButtonId id)
{
	switch (id)
	{
	case kButtonId_A: return "A";
	case kButtonId_B: return "B";
	case kButtonId_1: return "1";
	case kButtonId_2: return "2";

	case kButtonId_Home: return wxTRANSLATE("home");
	case kButtonId_Plus: return "+";
	case kButtonId_Minus: return "-";

	case kButtonId_Up: return wxTRANSLATE("up");
	case kButtonId_Down: return wxTRANSLATE("down");
	case kButtonId_Left: return wxTRANSLATE("left");
	case kButtonId_Right: return wxTRANSLATE("right");

	case kButtonId_Nunchuck_Z: return "Z";
	case kButtonId_Nunchuck_C: return "C";

	case kButtonId_Nunchuck_Up: return wxTRANSLATE("up");
	case kButtonId_Nunchuck_Down: return wxTRANSLATE("down");
	case kButtonId_Nunchuck_Left: return wxTRANSLATE("left");
	case kButtonId_Nunchuck_Right: return wxTRANSLATE("right");

	default:
		return "";
	}
}


