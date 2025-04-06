#pragma once

#include "input/InputManager.h"
#include "input/motion/MotionSample.h"
#include "input/api/ControllerState.h"

namespace pugi
{
	class xml_node;
}

enum Buttons2 : uint64
{
	// General
	kButton0,
	kButton1,
	kButton2,
	kButton3,
	kButton4,
	kButton5,
	kButton6,
	kButton7,
	kButton8,
	kButton9,
	kButton10,
	kButton11,
	kButton12,
	kButton13,
	kButton14,
	kButton15,
	kButton16,
	kButton17,
	kButton18,
	kButton19,
	kButton20,
	kButton21,
	kButton22,
	kButton23,
	kButton24,
	kButton25,
	kButton26,
	kButton27,
	kButton28,
	kButton29,
	kButton30,
	kButton31,

	// Trigger
	kButtonZL,
	kButtonZR,

	// DPAD
	kButtonUp,
	kButtonDown,
	kButtonLeft,
	kButtonRight,

	// positive values
	kAxisXP,
	kAxisYP,

	kRotationXP,
	kRotationYP,

	kTriggerXP,
	kTriggerYP,

	// negative values
	kAxisXN,
	kAxisYN,

	kRotationXN,
	kRotationYN,

	kTriggerXN,
	kTriggerYN,
	
	kButtonMAX,

	kButtonNoneAxisMAX = kButtonRight,
	kButtonAxisStart = kAxisXP,
};

class ControllerBase
{
public:
	ControllerBase(std::string_view uuid, std::string_view display_name);
	virtual ~ControllerBase() = default;

	const std::string& uuid() const { return m_uuid; }
	const std::string& display_name() const { return m_display_name; }
	
	virtual std::string_view api_name() const = 0;
	virtual InputAPI::Type api() const = 0;

	virtual void update() {}

	virtual bool connect() { return is_connected(); }
	virtual bool is_connected() = 0;
		
	virtual bool has_battery() { return false; }
	virtual bool has_low_battery() { return false; }

	const ControllerState& calibrate();
	const ControllerState& update_state();
	const ControllerState& get_state() const { return m_last_state; }
	const ControllerState& get_default_state() { return is_calibrated() ? m_default_state : calibrate(); }
	virtual ControllerState raw_state() = 0;

	bool is_calibrated() const { return m_is_calibrated; }

	float get_axis_value(uint64 button) const;
	virtual bool has_axis() const { return true; }

	bool use_motion() { return has_motion() && m_settings.motion; }
	virtual bool has_motion() { return false; }
	virtual MotionSample get_motion_sample() { return {}; }

	virtual bool has_position() { return false; }
	virtual glm::vec2 get_position() { return {}; }
	virtual glm::vec2 get_prev_position() { return {}; }
	virtual PositionVisibility GetPositionVisibility() {return PositionVisibility::NONE;};

	virtual bool has_rumble() { return false; }
	virtual void start_rumble() {}
	virtual void stop_rumble() {}

	virtual std::string get_button_name(uint64 button) const;

	virtual void save(pugi::xml_node& node){}
	virtual void load(const pugi::xml_node& node){}

	struct AxisSetting
	{
		AxisSetting(float deadzone = 0.25f) : deadzone(deadzone) {}
		float deadzone;
		float range = 1.0f;
	};
	struct Settings
	{
		AxisSetting axis{}, rotation{}, trigger{};
		float rumble = 0;
		bool motion = false; // only valid when has_motion is true
	};
	Settings get_settings() const;
	void set_settings(const Settings& settings);
	void set_axis_settings(const AxisSetting& settings);
	void set_rotation_settings(const AxisSetting& settings);
	void set_trigger_settings(const AxisSetting& settings);
	void set_rumble(float rumble);
	void set_use_motion(bool state);

	void apply_axis_setting(glm::vec2& axis, const glm::vec2& default_value, const AxisSetting& setting) const;

	bool operator==(const ControllerBase& c) const;
	bool operator!=(const ControllerBase& c) const { return !(*this == c); }

protected:
	std::string m_uuid;
	std::string m_display_name;

	ControllerState m_last_state{};

	bool m_is_calibrated = false;
	ControllerState m_default_state{};

	mutable std::mutex m_settings_mutex;
	Settings m_settings{};
};

template<class TProvider>
class Controller : public ControllerBase
{
public:
	Controller(std::string_view uuid, std::string_view display_name)
		: ControllerBase(uuid, display_name)
	{
		static_assert(std::is_base_of_v<ControllerProviderBase, TProvider>);
		m_provider = std::dynamic_pointer_cast<TProvider>(InputManager::instance().get_api_provider(TProvider::kAPIType));
		cemu_assert_debug(m_provider != nullptr);
	}

	Controller(std::string_view uuid, std::string_view display_name, const ControllerProviderSettings& settings)
		: ControllerBase(uuid, display_name)
	{
		static_assert(std::is_base_of_v<ControllerProviderBase, TProvider>);
		m_provider = std::dynamic_pointer_cast<TProvider>(InputManager::instance().get_api_provider(TProvider::kAPIType, settings));
		cemu_assert_debug(m_provider != nullptr);
	}

	// update provider if settings are different from default provider
	void update_provider(std::shared_ptr<TProvider> provider)
	{
		m_provider = std::move(provider);
	}

protected:
	using base_type = Controller<TProvider>;
	std::shared_ptr<TProvider> m_provider;
};

using ControllerPtr = std::shared_ptr<ControllerBase>;

