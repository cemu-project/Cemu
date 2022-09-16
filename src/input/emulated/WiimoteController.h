#pragma once

#include "input/emulated/WPADController.h"

class WiimoteController : public WPADController
{
	using base_type = WPADController;
public:
	enum ButtonId
	{
		kButtonId_None,

		kButtonId_A,
		kButtonId_B,
		kButtonId_1,
		kButtonId_2,

		kButtonId_Nunchuck_Z,
		kButtonId_Nunchuck_C,

		kButtonId_Plus,
		kButtonId_Minus,


		kButtonId_Up,
		kButtonId_Down,
		kButtonId_Left,
		kButtonId_Right,


		kButtonId_Nunchuck_Up,
		kButtonId_Nunchuck_Down,
		kButtonId_Nunchuck_Left,
		kButtonId_Nunchuck_Right,

		kButtonId_Home,

		kButtonId_Max,
	};

	WiimoteController(size_t player_index);

	Type type() const override { return Type::Wiimote; }
	WPADDeviceType get_device_type() const override { return m_device_type; }
	void set_device_type(WPADDeviceType device_type);

	bool is_mpls_attached() override;

	uint32 get_emulated_button_flag(uint32 id) const override;
	size_t get_highest_mapping_id() const override { return kButtonId_Max; }
	bool is_axis_mapping(uint64 mapping) const override { return mapping >= kButtonId_Nunchuck_Up && mapping <= kButtonId_Nunchuck_Right; }

	bool set_default_mapping(const std::shared_ptr<ControllerBase>& controller) override;

	glm::vec2 get_axis() const override;
	glm::vec2 get_rotation() const override;
	glm::vec2 get_trigger() const override;

	void load(const pugi::xml_node& node) override;
	void save(pugi::xml_node& node) override;
	
	static uint32 s_get_emulated_button_flag(uint32 id);

	static std::string_view get_button_name(ButtonId id);

	bool is_start_down() const override { return is_mapping_down(kButtonId_Plus); }
	bool is_left_down() const override { return is_mapping_down(kButtonId_Left); }
	bool is_right_down() const override { return is_mapping_down(kButtonId_Right); }
	bool is_up_down() const override { return is_mapping_down(kButtonId_Up); }
	bool is_down_down() const override { return is_mapping_down(kButtonId_Down); }
	bool is_a_down() const override { return is_mapping_down(kButtonId_A); }
	bool is_b_down() const override { return is_mapping_down(kButtonId_B); }
	bool is_home_down() const override { return is_mapping_down(kButtonId_Home); }

private:
	WPADDeviceType m_device_type = kWAPDevCore;
};
