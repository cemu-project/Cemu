#include "input/emulated/EmulatedController.h"

#include "input/api/Controller.h"

#ifdef SUPPORTS_WIIMOTE
#include "input/api/Wiimote/NativeWiimoteController.h"
#endif

std::string_view EmulatedController::type_to_string(Type type)
{
	switch (type)
	{
	case VPAD: return "Wii U GamePad";
	case Pro: return "Wii U Pro Controller";
	case Classic: return "Wii U Classic Controller";
	case Wiimote: return "Wiimote";
	}

	throw std::runtime_error(fmt::format("unknown emulated controller: {}", to_underlying(type)));
}

EmulatedController::Type EmulatedController::type_from_string(std::string_view str)
{
	if (str == "Wii U GamePad")
		return VPAD;
	else if (str == "Wii U Pro Controller")
		return Pro;
	else if (str == "Wii U Classic Controller")
		return Classic;
	else if (str == "Wiimote")
		return Wiimote;

	throw std::runtime_error(fmt::format("unknown emulated controller: {}", str));
}

EmulatedController::EmulatedController(size_t player_index)
	: m_player_index(player_index)
{
}

void EmulatedController::calibrate()
{
	std::shared_lock lock(m_mutex);
	for (const auto& controller : m_controllers)
	{
		controller->calibrate();
	}
}

void EmulatedController::connect()
{
	std::shared_lock lock(m_mutex);
	for (const auto& controller : m_controllers)
	{
		controller->connect();
	}
}

void EmulatedController::update()
{
	std::shared_lock lock(m_mutex);
	for(const auto& controller : m_controllers)
	{
		controller->update();
	}
}

void EmulatedController::controllers_update_states()
{
	std::shared_lock lock(m_mutex);
	for (const auto& controller : m_controllers)
	{
		controller->update_state();
	}
}

void EmulatedController::start_rumble()
{
	m_rumble = true;
	std::shared_lock lock(m_mutex);
	for (const auto& controller : m_controllers)
	{
		controller->start_rumble();
	}
}

void EmulatedController::stop_rumble()
{
	if (!m_rumble)
		return;

	m_rumble = false;

	std::shared_lock lock(m_mutex);
	for (const auto& controller : m_controllers)
	{
		controller->stop_rumble();
	}
}

bool EmulatedController::is_battery_low() const
{
	std::shared_lock lock(m_mutex);
	return std::any_of(m_controllers.cbegin(), m_controllers.cend(), [](const auto& c) {return c->has_low_battery(); });
}

bool EmulatedController::has_motion() const
{
	std::shared_lock lock(m_mutex);
	return std::any_of(m_controllers.cbegin(), m_controllers.cend(), [](const auto& c) {return c->use_motion(); });
}

MotionSample EmulatedController::get_motion_data() const
{
	std::shared_lock lock(m_mutex);
	for (const auto& controller : m_controllers)
	{
		if (controller->use_motion())
			return controller->get_motion_sample();
	}

	return {};
}

bool EmulatedController::has_second_motion() const
{
	int motion = 0;
	std::shared_lock lock(m_mutex);
	for(const auto& controller : m_controllers)
	{
		if(controller->use_motion())
		{
			// if wiimote has nunchuck connected, we use its acceleration
            #if SUPPORTS_WIIMOTE
            if(controller->api() == InputAPI::Wiimote)
			{
				if(((NativeWiimoteController*)controller.get())->get_extension() == NativeWiimoteController::Nunchuck)
				{
					return true;
				}
			}
            #endif
			motion++;
		}
	}

	return motion >= 2;
}

MotionSample EmulatedController::get_second_motion_data() const
{
	int motion = 0;
	std::shared_lock lock(m_mutex);
	for (const auto& controller : m_controllers)
	{
		if (controller->use_motion())
		{
			// if wiimote has nunchuck connected, we use its acceleration
            #ifdef SUPPORTS_WIIMOTE
			if (controller->api() == InputAPI::Wiimote)
			{
				if (((NativeWiimoteController*)controller.get())->get_extension() == NativeWiimoteController::Nunchuck)
				{
					return ((NativeWiimoteController*)controller.get())->get_nunchuck_motion_sample();
				}
			}
			#endif

			motion++;
			if(motion == 2)
			{
				return controller->get_motion_sample();
			}
		}
	}

	return {};
}

bool EmulatedController::has_position() const
{
	std::shared_lock lock(m_mutex);
	return std::any_of(m_controllers.cbegin(), m_controllers.cend(), [](const auto& c) {return c->has_position(); });
}

glm::vec2 EmulatedController::get_position() const
{
	std::shared_lock lock(m_mutex);
	for (const auto& controller : m_controllers)
	{
		if (controller->has_position())
			return controller->get_position();
	}

	return {};
}

glm::vec2 EmulatedController::get_prev_position() const
{
	std::shared_lock lock(m_mutex);
	for (const auto& controller : m_controllers)
	{
		if (controller->has_position())
			return controller->get_prev_position();
	}

	return {};
}

void EmulatedController::add_controller(std::shared_ptr<ControllerBase> controller)
{
	controller->connect();

    #ifdef SUPPORTS_WIIMOTE
    if (const auto wiimote = std::dynamic_pointer_cast<NativeWiimoteController>(controller)) {
		wiimote->set_player_index(m_player_index);
	}
    #endif
	std::scoped_lock lock(m_mutex);
	m_controllers.emplace_back(std::move(controller));
}

void EmulatedController::remove_controller(const std::shared_ptr<ControllerBase>& controller)
{
	std::scoped_lock lock(m_mutex);
	const auto it = std::find(m_controllers.cbegin(), m_controllers.cend(), controller);
	if (it != m_controllers.cend())
	{
		m_controllers.erase(it);

		for(auto m = m_mappings.begin(); m != m_mappings.end();)
		{
			if(auto mc = m->second.controller.lock())
			{
				if(*mc == *controller)
				{
					m = m_mappings.erase(m);
					continue;
				}
			}

			++m;
		}
	}
}

void EmulatedController::clear_controllers()
{
	std::scoped_lock lock(m_mutex);
	m_controllers.clear();
}

float EmulatedController::get_axis_value(uint64 mapping) const
{
	const auto it = m_mappings.find(mapping);
	if (it != m_mappings.cend())
	{
		if (const auto controller = it->second.controller.lock()) {
			return controller->get_axis_value(it->second.button);
		}
	}

	return 0;
}

bool EmulatedController::is_mapping_down(uint64 mapping) const
{
	const auto it = m_mappings.find(mapping);
	if (it != m_mappings.cend())
	{
		if (const auto controller = it->second.controller.lock())
			return controller->get_state().buttons.GetButtonState(it->second.button);
	}
	return false;
}


std::string EmulatedController::get_mapping_name(uint64 mapping) const
{
	const auto it = m_mappings.find(mapping);
	if (it != m_mappings.cend())
	{
		if (const auto controller = it->second.controller.lock()) {
			return controller->get_button_name(it->second.button);
		}
	}

	return {};
}

std::shared_ptr<ControllerBase> EmulatedController::get_mapping_controller(uint64 mapping) const
{
	const auto it = m_mappings.find(mapping);
	if (it != m_mappings.cend())
	{
		if (const auto controller = it->second.controller.lock()) {
			return controller;
		}
	}

	return {};
}

void EmulatedController::delete_mapping(uint64 mapping)
{
	m_mappings.erase(mapping);
}

void EmulatedController::clear_mappings()
{
	m_mappings.clear();
}

void EmulatedController::set_mapping(uint64 mapping, const std::shared_ptr<ControllerBase>& controller,
                                     uint64 button)
{
	m_mappings[mapping] = { controller, button };
}

bool EmulatedController::operator==(const EmulatedController& o) const
{
	return type() == o.type() && m_player_index == o.m_player_index;
}

bool EmulatedController::operator!=(const EmulatedController& o) const
{
	return !(*this == o);
}
