#include "input/InputManager.h"
#include "config/ActiveSettings.h"
#include "input/ControllerFactory.h"
#include <boost/property_tree/ini_parser.hpp>
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
#if HAS_SDL
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
#if HAS_GAMECUBE
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
	m_update_thread_shutdown.store(true);
	m_update_thread.join();
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

	auto old_file = file_path;
	old_file.replace_extension(".txt"); // test .txt extension
	file_path.replace_extension(".xml"); // force .xml extension

	if (fs::exists(old_file) && !fs::exists(file_path))
		migrate_config(old_file);

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

bool InputManager::migrate_config(const fs::path& file_path)
{
	try
	{
		auto xmlData = FileStream::LoadIntoMemory(file_path);
		if (!xmlData || xmlData->empty())
			return false;

		std::string iniDataStr((const char*)xmlData->data(), xmlData->size());

		std::stringstream iniData(iniDataStr);
		boost::property_tree::ptree m_data;
		read_ini(iniData, m_data);

		const auto emulate_string = m_data.get<std::string>("General.emulate");
		const auto api_string = m_data.get<std::string>("General.api");
		auto uuid_opt = m_data.get_optional<std::string>("General.controller");
		const auto display_name = m_data.get_optional<std::string>("General.display");

		std::string uuid;
		if (api_string == to_string(InputAPI::Keyboard))
			uuid = to_string(InputAPI::Keyboard);
		else
		{
			if (!uuid_opt)
				return false;

			uuid = uuid_opt.value();
			if (api_string == to_string(InputAPI::SDLController))
			{
				uuid += "_0";
			}
		}

		fs::path out_file = file_path;
		out_file.replace_extension(".xml");

		pugi::xml_document doc;
		auto declaration_node = doc.append_child(pugi::node_declaration);
		declaration_node.append_attribute("version") = "1.0";
		declaration_node.append_attribute("encoding") = "UTF-8";

		auto emulated_controller = doc.append_child("emulated_controller");
		emulated_controller.append_child("type").append_child(pugi::node_pcdata).set_value(emulate_string.c_str());

		bool has_keyboard = api_string == to_string(InputAPI::Keyboard);
		if (!has_keyboard) // test if only keyboard configured
		{
			auto controller = emulated_controller.append_child("controller");
			controller.append_child("api").append_child(pugi::node_pcdata).set_value(api_string.c_str());
			controller.append_child("uuid").append_child(pugi::node_pcdata).set_value(uuid.c_str());
			if (display_name.has_value() && !display_name->empty())
				controller.append_child("display_name").append_child(pugi::node_pcdata).set_value(
					display_name.value().c_str());


			controller.append_child("rumble").append_child(pugi::node_pcdata).set_value(
				m_data.get<std::string>("Controller.rumble").c_str());

			auto axis_node = controller.append_child("axis");
			axis_node.append_child("deadzone").append_child(pugi::node_pcdata).set_value(
				m_data.get<std::string>("Controller.leftDeadzone").c_str());
			axis_node.append_child("range").append_child(pugi::node_pcdata).set_value(
				m_data.get<std::string>("Controller.leftRange").c_str());

			auto rotation_node = controller.append_child("rotation");
			rotation_node.append_child("deadzone").append_child(pugi::node_pcdata).set_value(
				m_data.get<std::string>("Controller.rightDeadzone").c_str());
			rotation_node.append_child("range").append_child(pugi::node_pcdata).set_value(
				m_data.get<std::string>("Controller.rightRange").c_str());

			auto mappings_node = controller.append_child("mappings");
			for (int i = 1; i < 28; ++i) // test all possible mappings (max is 27 for vpad controller)
			{
				auto mapping = m_data.get_optional<std::string>(fmt::format("Controller.{}", i));
				if (!mapping || mapping->empty())
					continue;

				if (!boost::starts_with(mapping.value(), "button_"))
				{
					if (boost::starts_with(mapping.value(), "key_"))
						has_keyboard = true;

					continue;
				}

				const auto button = ConvertString<uint64>(mapping.value().substr(7), 16);

				uint64 flag_bit = 0;
				for (auto b = 0; b < 64; ++b)
				{
					if (HAS_BIT(button, b))
					{
						flag_bit = b;
						break;
					}
				}

				// fix old flag layout to new one for all kind of axis stuff
				if (flag_bit >= 24 && flag_bit <= 31)
					flag_bit += 8;
				else if (flag_bit == 32) flag_bit = kTriggerXP;
				else if (flag_bit == 33) flag_bit = kRotationXP;
				else if (flag_bit == 34) flag_bit = kRotationYP;
				else if (flag_bit == 35) flag_bit = kTriggerYP;
				else if (flag_bit == 36) flag_bit = kAxisXN;
				else if (flag_bit == 37) flag_bit = kAxisYN;
				else if (flag_bit == 38) flag_bit = kTriggerXN;
				else if (flag_bit == 39) flag_bit = kRotationXN;
				else if (flag_bit == 40) flag_bit = kRotationYN;
				else if (flag_bit == 41) flag_bit = kTriggerYN;

				// fix old api mappings
				if (api_string == to_string(InputAPI::XInput))
				{
					const std::unordered_map<uint64, uint64> xinput =
					{
						{kButton0, 12}, // XINPUT_GAMEPAD_A
						{kButton1, 13}, // XINPUT_GAMEPAD_B
						{kButton2, 14}, // XINPUT_GAMEPAD_X
						{kButton3, 15}, // XINPUT_GAMEPAD_Y

						{kButton4, 8}, // XINPUT_GAMEPAD_LEFT_SHOULDER
						{kButton5, 9}, // XINPUT_GAMEPAD_LEFT_SHOULDER

						{kButton6, 4}, // XINPUT_GAMEPAD_START
						{kButton7, 5}, // XINPUT_GAMEPAD_BACK

						{kButton8, 6}, // XINPUT_GAMEPAD_LEFT_THUMB
						{kButton9, 7}, // XINPUT_GAMEPAD_RIGHT_THUMB

						{kButton10, 0}, // XINPUT_GAMEPAD_DPAD_UP
						{kButton11, 1}, // XINPUT_GAMEPAD_DPAD_DOWN
						{kButton12, 2}, // XINPUT_GAMEPAD_DPAD_LEFT
						{kButton13, 3}, // XINPUT_GAMEPAD_DPAD_RIGHT
					};

					const auto it = xinput.find(flag_bit);
					if (it != xinput.cend())
						flag_bit = it->second;
				}
				else if (api_string == "DSU")
				{
					const std::unordered_map<uint64, uint64> dsu =
					{
						{7, kButton0}, // ButtonSelect
						{8, kButton1}, // ButtonLStick
						{9, kButton2}, // ButtonRStick
						{6, kButton3}, // ButtonStart

						{4, kButton10}, // ButtonL
						{5, kButton11}, // ButtonR

						{0, kButton14}, // ButtonA
						{1, kButton13}, // ButtonB
						{2, kButton15}, // ButtonX
						{3, kButton12}, // ButtonY
					};

					const auto it = dsu.find(flag_bit);
					if (it != dsu.cend())
						flag_bit = it->second;
				}


				auto entry_node = mappings_node.append_child("entry");
				entry_node.append_child("mapping").append_child(pugi::node_pcdata).set_value(
					fmt::format("{}", i).c_str());
				entry_node.append_child("button").append_child(pugi::node_pcdata).set_value(
					fmt::format("{}", flag_bit).c_str());
			}
		}

		if (has_keyboard)
		{
			auto controller = emulated_controller.append_child("controller");
			controller.append_child("api").append_child(pugi::node_pcdata).set_value("Keyboard");
			controller.append_child("uuid").append_child(pugi::node_pcdata).set_value("Keyboard");

			auto mappings_node = controller.append_child("mappings");
			for (int i = 1; i < 28; ++i) // test all possible mappings (max is 27 for vpad controller)
			{
				auto mapping = m_data.get_optional<std::string>(fmt::format("Controller.{}", i));
				if (!mapping || mapping->empty())
					continue;

				if (!boost::starts_with(mapping.value(), "key_"))
					continue;

				const auto button = ConvertString<uint64>(mapping.value().substr(4));

				auto entry_node = mappings_node.append_child("entry");
				entry_node.append_child("mapping").append_child(pugi::node_pcdata).set_value(
					fmt::format("{}", i).c_str());
				entry_node.append_child("button").append_child(pugi::node_pcdata).set_value(
					fmt::format("{}", button).c_str());
			}
		}

		std::ofstream write_file(out_file, std::ios::out | std::ios::trunc);
		if (write_file.is_open())
		{
			doc.save(write_file);
			return true;
		}
	}
	catch (const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "can't migrate config file {}: {}", file_path.string(), ex.what());
	}

	return false;
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
				fs::remove(ActiveSettings::GetConfigPath("controllerProfiles/controller{}.txt", player_index), ec);
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

			std::error_code ec{};
			fs::remove(ActiveSettings::GetConfigPath("controllerProfiles/controller{}.xml", player_index), ec);
			fs::remove(ActiveSettings::GetConfigPath("controllerProfiles/controller{}.txt", player_index), ec);

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
	return std::static_pointer_cast<VPADController>(m_vpad[index]);
}

std::shared_ptr<WPADController> InputManager::get_wpad_controller(size_t index) const
{
	if (index >= m_wpad.size())
		return {};

	std::shared_lock lock(m_mutex);
	return std::static_pointer_cast<WPADController>(m_wpad[index]);
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
		if (p.has_extension() && (p.extension() == ".xml" || p.extension() == ".txt"))
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
		std::shared_lock lock(m_main_mouse.m_mutex);
		if (std::exchange(m_main_mouse.left_down_toggle, false))
			return m_main_mouse.position;

		if (m_main_mouse.left_down)
			return m_main_mouse.position;
	}

	{
		std::shared_lock lock(m_main_touch.m_mutex);
		if (std::exchange(m_main_touch.left_down_toggle, false))
			return m_main_touch.position;

		if (m_main_touch.left_down)
			return m_main_touch.position;
	}

	if (is_pad)
		*is_pad = true;

	{
		std::shared_lock lock(m_pad_mouse.m_mutex);
		if (std::exchange(m_pad_mouse.left_down_toggle, false))
			return m_pad_mouse.position;

		if (m_pad_mouse.left_down)
			return m_pad_mouse.position;
	}

	{
		std::shared_lock lock(m_pad_touch.m_mutex);
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
		std::shared_lock lock(m_main_mouse.m_mutex);
		if (std::exchange(m_main_mouse.right_down_toggle, false))
			return m_main_mouse.position;

		if (m_main_mouse.right_down)
			return m_main_mouse.position;
	}

	{
		std::shared_lock lock(m_main_touch.m_mutex);
		if (std::exchange(m_main_touch.right_down_toggle, false))
			return m_main_touch.position;

		if (m_main_touch.right_down)
			return m_main_touch.position;
	}

	if (is_pad)
		*is_pad = true;

	{
		std::shared_lock lock(m_pad_mouse.m_mutex);
		if (std::exchange(m_pad_mouse.right_down_toggle, false))
			return m_pad_mouse.position;

		if (m_pad_mouse.right_down)
			return m_pad_mouse.position;
	}

	{
		std::shared_lock lock(m_pad_touch.m_mutex);
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
