#include "input/InputManager.h"
#include "config/ActiveSettings.h"
#include "input/ControllerFactory.h"
#include <pugixml.hpp>
#include "Cafe/GameProfile/GameProfile.h"
#include "util/EventService.h"

InputManager::InputManager()
{
	/*
	auto create_provider = []
	template <typename TProvider>
	()
	{
		static_assert(std::is_base_of_v<ControllerProvider, TProvider>);
		try
		{
			auto controller = std::make_shared<TProvider>();
			m_api_available[controller->api()] = controller;
		}
		catch (const std::exception& ex)
		{
			cemuLog_log(LogType::Force, ex.what());
		}
	}
	*/
#if HAS_KEYBOARD
	create_provider<KeyboardControllerProvider>();
#endif
#ifdef HAS_SDL
	create_provider<SDLControllerProvider>();
#endif
#if HAS_XINPUT
	create_provider<XInputControllerProvider>();
#endif
#if HAS_DIRECTINPUT
	create_provider<DirectInputControllerProvider>();
#endif
#if HAS_DSU
	create_provider<DSUControllerProvider>();
#endif
#if defined(HAS_GAMECUBE) && HAS_GAMECUBE && defined(HAS_LIBUSB)
	create_provider<GameCubeControllerProvider>();
#endif
#if HAS_WIIMOTE
	create_provider<WiimoteControllerProvider>();
#endif

	m_update_thread_shutdown.store(false);
	m_update_thread = std::thread(&InputManager::update_thread, this);
}

InputManager::~InputManager()
{
	// destructors will not invoked forever, so we manually release resources in Shutdown().
}

void InputManager::load() noexcept
{
	for (size_t i = 0; i < kMaxController; ++i)
	{
		try
		{
			load(i);
		}
		catch (const std::exception& ex)
		{
			cemuLog_log(LogType::Force, "can't load controller profile: {}", ex.what());
		}
	}
}

bool InputManager::load(size_t player_index, std::string_view filename)
{
	fs::path file_path;
	if (filename.empty())
		file_path = ActiveSettings::GetConfigPath("controllerProfiles/controller{}", player_index);
	else
		file_path = ActiveSettings::GetConfigPath("controllerProfiles/{}", filename);

	file_path.replace_extension(".xml"); // force .xml extension

	if (!fs::exists(file_path))
		return false;

	try
	{
		auto xmlData = FileStream::LoadIntoMemory(file_path);
		if (!xmlData || xmlData->empty())
			return false;
	
		pugi::xml_document doc;
		if (!doc.load_buffer(xmlData->data(), xmlData->size()))
			return false;

		const pugi::xml_node root = doc.document_element();

		const auto type_node = root.child("type");
		if (!type_node)
			return false;

		const auto emulate = EmulatedController::type_from_string(type_node.child_value());
		auto emulated_controller = ControllerFactory::CreateEmulatedController(player_index, emulate);


		if (const auto profile_name_node = root.child("profile"))
			emulated_controller->m_profile_name = profile_name_node.child_value();

		// custom settings
		emulated_controller->load(root);

		for (const auto controller_node : root.select_nodes("controller"))
		{
			const auto cnode = controller_node.node();
			const auto api_node = cnode.child("api");
			if (!api_node)
				continue;

			const auto uuid_node = cnode.child("uuid");
			if (!uuid_node)
				continue;

			const auto* display_name = cnode.child_value("display_name");

			try
			{
				const auto api = InputAPI::from_string(api_node.child_value());
				auto controller = ControllerFactory::CreateController(api, uuid_node.child_value(), display_name);
				emulated_controller->add_controller(controller);

				// load optional settings
				auto settings = controller->get_settings();
				if (const auto axis_node = cnode.child("axis"))
				{
					if (const auto value = axis_node.child("deadzone"))
						settings.axis.deadzone = ConvertString<float>(value.child_value());

					if (const auto value = axis_node.child("range"))
						settings.axis.range = ConvertString<float>(value.child_value());
				}
				if (const auto rotation_node = cnode.child("rotation"))
				{
					if (const auto value = rotation_node.child("deadzone"))
						settings.rotation.deadzone = ConvertString<float>(value.child_value());

					if (const auto value = rotation_node.child("range"))
						settings.rotation.range = ConvertString<float>(value.child_value());
				}
				if (const auto trigger_node = cnode.child("trigger"))
				{
					if (const auto value = trigger_node.child("deadzone"))
						settings.trigger.deadzone = ConvertString<float>(value.child_value());

					if (const auto value = trigger_node.child("range"))
						settings.trigger.range = ConvertString<float>(value.child_value());
				}

				if (const auto value = cnode.child("rumble"))
					settings.rumble = ConvertString<float>(value.child_value());

				if (const auto value = cnode.child("motion"))
					settings.motion = ConvertString<bool>(value.child_value());

				controller->set_settings(settings);

				// custom settings
				controller->load(cnode);
				

				// mappings
				if (const auto mappings_node = cnode.child("mappings"))
				{
					for (const auto& entry : mappings_node.select_nodes("entry"))
					{
						const auto enode = entry.node();

						const auto mapping_node = enode.child("mapping");
						if (!mapping_node)
							continue;

						const auto button_node = enode.child("button");
						if (!button_node)
							continue;

						const auto mapping = ConvertString<uint64>(mapping_node.child_value());
						const auto button = ConvertString<uint64>(button_node.child_value());

						emulated_controller->set_mapping(mapping, controller, button);
					}
				}
			}
			catch (const std::exception& ex)
			{
				cemuLog_log(LogType::Force, "can't load controller: {}", ex.what());
			}
		}

		set_controller(emulated_controller);
		return true;
	}
	catch (const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "can't load config file: {}", ex.what());
		return false;
	}
}

void InputManager::save() noexcept
{
	for (size_t i = 0; i < kMaxController; ++i)
	{
		try
		{
			save(i);
		}
		catch (const std::exception& ex)
		{
			cemuLog_log(LogType::Force, "can't save controller profile: {}", ex.what());
		}
	}
}

bool InputManager::save(size_t player_index, std::string_view filename)
{
	// dont overwrite files if set by gameprofile
	if (m_is_gameprofile_set[player_index])
		return true;

	auto emulated_controller = get_controller(player_index);
	if (!emulated_controller)
		return false;

	fs::path file_path = ActiveSettings::GetConfigPath("controllerProfiles");
	fs::create_directories(file_path);

	const auto is_default_file = filename.empty();
	if (is_default_file)
		file_path /= fmt::format("controller{}", player_index);
	else
		file_path /= _utf8ToPath(filename);

	file_path.replace_extension(".xml"); // force .xml extension

	pugi::xml_document doc;
	auto declaration_node = doc.append_child(pugi::node_declaration);
	declaration_node.append_attribute("version") = "1.0";
	declaration_node.append_attribute("encoding") = "UTF-8";

	auto emulated_controller_node = doc.append_child("emulated_controller");
	emulated_controller_node.append_child("type").append_child(pugi::node_pcdata).set_value(std::string{
		emulated_controller->type_string()
	}.c_str());

	if(!is_default_file)
		emulated_controller->m_profile_name = std::string{filename};

	if (emulated_controller->has_profile_name())
		emulated_controller_node.append_child("profile").append_child(pugi::node_pcdata).set_value(
			emulated_controller->get_profile_name().c_str());

	// custom settings
	emulated_controller->save(emulated_controller_node);

	for (const auto& controller : emulated_controller->get_controllers())
	{
		auto controller_node = emulated_controller_node.append_child("controller");

		// general
		controller_node.append_child("api").append_child(pugi::node_pcdata).set_value(std::string{
			controller->api_name()
		}.c_str());
		controller_node.append_child("uuid").append_child(pugi::node_pcdata).set_value(controller->uuid().c_str());
		controller_node.append_child("display_name").append_child(pugi::node_pcdata).set_value(
			controller->display_name().c_str());

		// settings
		const auto& settings = controller->get_settings();

		if (controller->has_motion())
			controller_node.append_child("motion").append_child(pugi::node_pcdata).set_value(
				fmt::format("{}", settings.motion).c_str());

		if (controller->has_rumble())
			controller_node.append_child("rumble").append_child(pugi::node_pcdata).set_value(
				fmt::format("{}", settings.rumble).c_str());

		auto axis_node = controller_node.append_child("axis");
		axis_node.append_child("deadzone").append_child(pugi::node_pcdata).set_value(
			fmt::format("{}", settings.axis.deadzone).c_str());
		axis_node.append_child("range").append_child(pugi::node_pcdata).set_value(
			fmt::format("{}", settings.axis.range).c_str());

		auto rotation_node = controller_node.append_child("rotation");
		rotation_node.append_child("deadzone").append_child(pugi::node_pcdata).set_value(
			fmt::format("{}", settings.rotation.deadzone).c_str());
		rotation_node.append_child("range").append_child(pugi::node_pcdata).set_value(
			fmt::format("{}", settings.rotation.range).c_str());

		auto trigger_node = controller_node.append_child("trigger");
		trigger_node.append_child("deadzone").append_child(pugi::node_pcdata).set_value(
			fmt::format("{}", settings.trigger.deadzone).c_str());
		trigger_node.append_child("range").append_child(pugi::node_pcdata).set_value(
			fmt::format("{}", settings.trigger.range).c_str());

		// custom settings
		controller->save(controller_node);

		// mappings for current controller
		auto mappings_node = controller_node.append_child("mappings");
		for (const auto& mapping : emulated_controller->m_mappings)
		{
			if (!mapping.second.controller.expired() && *controller == *mapping.second.controller.lock())
			{
				auto entry_node = mappings_node.append_child("entry");
				entry_node.append_child("mapping").append_child(pugi::node_pcdata).set_value(
					fmt::format("{}", mapping.first).c_str());
				entry_node.append_child("button").append_child(pugi::node_pcdata).set_value(
					fmt::format("{}", mapping.second.button).c_str());
			}
		}
	}
	FileStream* fs = FileStream::createFile2(file_path);
	if (!fs)
		return false;
	std::stringstream xmlData;
	doc.save(xmlData);
	std::string xmlStr = xmlData.str();
	fs->writeData(xmlStr.data(), xmlStr.size());
	delete fs;
	return true;
}

bool InputManager::is_gameprofile_set(size_t player_index) const
{
	return m_is_gameprofile_set[player_index];
}

EmulatedControllerPtr InputManager::set_controller(EmulatedControllerPtr controller)
{
	auto prev_controller = delete_controller(controller->player_index());

	// assign controllers to new emulated controller if empty
	if (prev_controller && controller->get_controllers().empty())
	{
		for (const auto& c : prev_controller->get_controllers())
		{
			controller->add_controller(c);
		}
	}

	// try to connect all controllers
	/*for (auto& c : controller->get_controllers())
	{
		c->connect();
	}*/

	std::scoped_lock lock(m_mutex);
	switch (controller->type())
	{
	case EmulatedController::Type::VPAD:
		for (auto& pad : m_vpad)
		{
			if (!pad)
			{
				pad.swap(controller);
				return prev_controller;
			}
		}

		break;

	default:
		for (auto& pad : m_wpad)
		{
			if (!pad)
			{
				pad.swap(controller);
				return prev_controller;
			}
		}

		break;
	}
	
	cemu_assert_debug(false);
	return prev_controller;
}

EmulatedControllerPtr InputManager::set_controller(size_t player_index, EmulatedController::Type type)
{
	try
	{
		auto emulated_controller = ControllerFactory::CreateEmulatedController(player_index, type);
		set_controller(emulated_controller);
		return emulated_controller;
	}
	catch (const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "Unable to set controller type {} on player index {}: {}", type, player_index, ex.what());
	}

	return {};
}

EmulatedControllerPtr InputManager::set_controller(size_t player_index, EmulatedController::Type type,
                                                   const std::shared_ptr<ControllerBase>& controller)
{
	auto result = set_controller(player_index, type);
	if (result)
		result->add_controller(controller);

	return result;
}

EmulatedControllerPtr InputManager::get_controller(size_t player_index) const
{
	std::shared_lock lock(m_mutex);
	for (const auto& pad : m_vpad)
	{
		if (pad && pad->player_index() == player_index)
			return pad;
	}

	for (const auto& pad : m_wpad)
	{
		if (pad && pad->player_index() == player_index)
			return pad;
	}

	return {};
}

EmulatedControllerPtr InputManager::delete_controller(size_t player_index, bool delete_profile)
{
	std::scoped_lock lock(m_mutex);
	for (auto& controller : m_vpad)
	{
		auto result = controller;
		if (result && result->player_index() == player_index)
		{
			controller = {};

			if(delete_profile)
			{
				std::error_code ec{};
				fs::remove(ActiveSettings::GetConfigPath("controllerProfiles/controller{}.xml", player_index), ec);
			}

			return result;
		}
	}

	for (auto& controller : m_wpad)
	{
		auto result = controller;
		if (result && result->player_index() == player_index)
		{
			controller = {};

			if(delete_profile)
			{
				std::error_code ec{};
				fs::remove(ActiveSettings::GetConfigPath("controllerProfiles/controller{}.xml", player_index), ec);
			}

			return result;
		}
	}

	return {};
}


std::shared_ptr<VPADController> InputManager::get_vpad_controller(size_t index) const
{
	if (index >= m_vpad.size())
		return {};

	std::shared_lock lock(m_mutex);
	return std::dynamic_pointer_cast<VPADController>(m_vpad[index]);
}

std::shared_ptr<WPADController> InputManager::get_wpad_controller(size_t index) const
{
	if (index >= m_wpad.size())
		return {};

	std::shared_lock lock(m_mutex);
	return std::dynamic_pointer_cast<WPADController>(m_wpad[index]);
}

std::pair<size_t, size_t> InputManager::get_controller_count() const
{
	std::shared_lock lock(m_mutex);
	const size_t vpad = std::count_if(m_vpad.cbegin(), m_vpad.cend(), [](const auto& v) { return v != nullptr; });
	const size_t wpad = std::count_if(m_wpad.cbegin(), m_wpad.cend(), [](const auto& v) { return v != nullptr; });
	return std::make_pair(vpad, wpad);
}

void InputManager::on_device_changed()
{
	std::shared_lock lock(m_mutex);
	for (auto& pad : m_vpad)
	{
		if (pad)
			pad->connect();
	}

	for (auto& pad : m_wpad)
	{
		if (pad)
			pad->connect();
	}
	lock.unlock();

	EventService::instance().signal<Events::ControllerChanged>();
}

ControllerProviderPtr InputManager::get_api_provider(InputAPI::Type api) const
{
	if(!m_api_available[api].empty())
		return *(m_api_available[api].begin());
	
	cemu_assert_debug(false);
	return {};
}

ControllerProviderPtr InputManager::get_api_provider(InputAPI::Type api, const ControllerProviderSettings& settings)
{
	for(const auto& p : m_api_available[api])
	{
		if(*p == settings)
		{
			return p;
		}
	}

	const auto result = ControllerFactory::CreateControllerProvider(api, settings);
	m_api_available[api].emplace_back(result);
	return result;
}

void InputManager::apply_game_profile()
{
	const auto& profiles = g_current_game_profile->GetControllerProfile();
	for (int i = 0; i < kMaxController; ++i)
	{
		if (profiles[i] && !profiles[i]->empty())
		{
			if (load(i, profiles[i].value()))
			{
				m_is_gameprofile_set[i] = true;
				if (const auto controller = get_controller(i))
				{
					if (!controller->has_profile_name())
						controller->m_profile_name = profiles[i].value();
				}
			}
		}
	}
}

std::vector<std::string> InputManager::get_profiles()
{
	const auto path = ActiveSettings::GetConfigPath("controllerProfiles");
	if (!exists(path))
		return {};

	std::set<std::string> tmp;
	for (const auto& entry : fs::directory_iterator(path))
	{
		const auto& p = entry.path();
		if (p.extension() == ".xml")
		{
			auto stem = _pathToUtf8(p.filename().stem());
			if (is_valid_profilename(stem))
			{
				tmp.emplace(stem);
			}
		}
	}

	std::vector<std::string> result;
	result.reserve(tmp.size());
	result.insert(result.end(), tmp.begin(), tmp.end());
	return result;
}

bool InputManager::is_valid_profilename(const std::string& name)
{
	if (!IsValidFilename(name))
		return false;

	// dont allow default profile names
	for (size_t i = 0; i < kMaxController; i++)
	{
		if (name == fmt::format("controller{}", i))
			return false;
	}

	return true;
}

glm::ivec2 InputManager::get_mouse_position(bool pad_window) const
{
	if (pad_window)
	{
		std::shared_lock lock(m_pad_mouse.m_mutex);
		return m_pad_mouse.position;
	}
	else
	{
		std::shared_lock lock(m_main_mouse.m_mutex);
		return m_main_mouse.position;
	}
}

std::optional<glm::ivec2> InputManager::get_left_down_mouse_info(bool* is_pad)
{
	if (is_pad)
		*is_pad = false;

	{
		std::unique_lock lock(m_main_mouse.m_mutex);
		if (std::exchange(m_main_mouse.left_down_toggle, false))
			return m_main_mouse.position;

		if (m_main_mouse.left_down)
			return m_main_mouse.position;
	}

	{
		std::unique_lock lock(m_main_touch.m_mutex);
		if (std::exchange(m_main_touch.left_down_toggle, false))
			return m_main_touch.position;

		if (m_main_touch.left_down)
			return m_main_touch.position;
	}

	if (is_pad)
		*is_pad = true;

	{
		std::unique_lock lock(m_pad_mouse.m_mutex);
		if (std::exchange(m_pad_mouse.left_down_toggle, false))
			return m_pad_mouse.position;

		if (m_pad_mouse.left_down)
			return m_pad_mouse.position;
	}

	{
		std::unique_lock lock(m_pad_touch.m_mutex);
		if (std::exchange(m_pad_touch.left_down_toggle, false))
			return m_pad_touch.position;

		if (m_pad_touch.left_down)
			return m_pad_touch.position;
	}

	return {};
}

std::optional<glm::ivec2> InputManager::get_right_down_mouse_info(bool* is_pad)
{
	if (is_pad)
		*is_pad = false;

	{
		std::unique_lock lock(m_main_mouse.m_mutex);
		if (std::exchange(m_main_mouse.right_down_toggle, false))
			return m_main_mouse.position;

		if (m_main_mouse.right_down)
			return m_main_mouse.position;
	}

	{
		std::unique_lock lock(m_main_touch.m_mutex);
		if (std::exchange(m_main_touch.right_down_toggle, false))
			return m_main_touch.position;

		if (m_main_touch.right_down)
			return m_main_touch.position;
	}

	if (is_pad)
		*is_pad = true;

	{
		std::unique_lock lock(m_pad_mouse.m_mutex);
		if (std::exchange(m_pad_mouse.right_down_toggle, false))
			return m_pad_mouse.position;

		if (m_pad_mouse.right_down)
			return m_pad_mouse.position;
	}

	{
		std::unique_lock lock(m_pad_touch.m_mutex);
		if (std::exchange(m_pad_touch.right_down_toggle, false))
			return m_pad_touch.position;

		if (m_pad_touch.right_down)
			return m_pad_touch.position;
	}

	return {};
}

void InputManager::update_thread()
{
	SetThreadName("Input_update");
	while (!m_update_thread_shutdown.load(std::memory_order::relaxed))
	{
		std::shared_lock lock(m_mutex);
		for (auto& pad : m_vpad)
		{
			if (pad)
				pad->update();
		}

		for (auto& pad : m_wpad)
		{
			if (pad)
				pad->update();
		}
		lock.unlock();

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		std::this_thread::yield();
	}
}

void InputManager::Shutdown()
{
    m_update_thread_shutdown = true;

    if (m_update_thread.joinable())
    {
        m_update_thread.join();
    }

    for (auto& pad : m_vpad) pad.reset();
	for (auto& pad : m_wpad) pad.reset();

	for (auto& providers : m_api_available)
	{
		providers.clear();
	}
}
