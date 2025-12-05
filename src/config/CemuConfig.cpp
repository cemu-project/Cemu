#include "config/CemuConfig.h"
#include "WindowSystem.h"

#include "util/helpers/helpers.h"
#include "config/ActiveSettings.h"

#include "ActiveSettings.h"

void CemuConfig::SetMLCPath(fs::path path, bool save)
{
	mlc_path.SetValue(_pathToUtf8(path));
	if(save)
		GetConfigHandle().Save();
	Account::RefreshAccounts();
}

XMLConfigParser CemuConfig::Load(XMLConfigParser& parser)
{
	auto new_parser = parser.get("content");
	if (new_parser.valid())
		parser = new_parser;

	// general settings
	log_flag = parser.get("logflag", log_flag.GetInitValue());
	cemuLog_setActiveLoggingFlags(GetConfig().log_flag.GetValue());
	advanced_ppc_logging = parser.get("advanced_ppc_logging", advanced_ppc_logging.GetInitValue());

	const char* mlc = parser.get("mlc_path", "");
	mlc_path = mlc;

	permanent_storage = parser.get("permanent_storage", permanent_storage);

	proxy_server = parser.get("proxy_server", "");
	disable_screensaver = parser.get("disable_screensaver", disable_screensaver);
	play_boot_sound = parser.get("play_boot_sound", play_boot_sound);
	console_language = parser.get("console_language", console_language.GetInitValue());

	game_paths.clear();
	auto game_path_parser = parser.get("GamePaths");
	for (auto element = game_path_parser.get("Entry"); element.valid(); element = game_path_parser.get("Entry", element))
	{
		const std::string path = element.value("");
		if (path.empty())
			continue;
		try
		{
			game_paths.emplace_back(path);
		}
		catch (const std::exception&)
		{
			cemuLog_log(LogType::Force, "config load error: can't load game path: {}", path);
		}
	}

	std::unique_lock _lock(game_cache_entries_mutex);
	game_cache_entries.clear();
	auto game_cache_parser = parser.get("GameCache");
	for (auto element = game_cache_parser.get("Entry"); element.valid(); element = game_cache_parser.get("Entry", element))
	{
		const char* rpx = element.get("path", "");
		try
		{
			GameEntry entry{};
			entry.rpx_file = boost::nowide::widen(rpx);
			entry.title_id = element.get<decltype(entry.title_id)>("title_id");
			entry.legacy_name = boost::nowide::widen(element.get("name", ""));
			entry.custom_name = element.get("custom_name", "");
			entry.legacy_region = element.get("region", 0);
			entry.legacy_version = element.get("version", 0);
			entry.legacy_update_version = element.get("version", 0);
			entry.legacy_dlc_version = element.get("dlc_version", 0);
			entry.legacy_time_played = element.get<decltype(entry.legacy_time_played)>("time_played");
			entry.legacy_last_played = element.get<decltype(entry.legacy_last_played)>("last_played");
			entry.favorite = element.get("favorite", false);
			game_cache_entries.emplace_back(entry);

			if (entry.title_id != 0)
			{
				if (entry.favorite)
					game_cache_favorites.emplace(entry.title_id);
				else
					game_cache_favorites.erase(entry.title_id);
			}
		}
		catch (const std::exception&)
		{
			cemuLog_log(LogType::Force, "config load error: can't load game cache entry: {}", rpx);
		}
	}
	_lock.unlock();

	graphic_pack_entries.clear();
	auto graphic_pack_parser = parser.get("GraphicPack");
	for (auto element = graphic_pack_parser.get("Entry"); element.valid(); element = graphic_pack_parser.get("Entry", element))
	{
		std::string filename = element.get_attribute("filename", "");
		if(filename.empty()) // legacy loading
		{
			filename = element.get("filename", "");
			fs::path path = fs::path(filename).lexically_normal();
			graphic_pack_entries.try_emplace(path);
			const std::string category = element.get("category", "");
			const std::string preset = element.get("preset", "");
			graphic_pack_entries[filename].try_emplace(category, preset);
		}
		else
		{
			fs::path path = fs::path(filename).lexically_normal();
			graphic_pack_entries.try_emplace(path);

			const bool disabled = element.get_attribute("disabled", false);
			if (disabled)
			{
				graphic_pack_entries[path].try_emplace("_disabled", "true");
			}

			for (auto preset = element.get("Preset"); preset.valid(); preset = element.get("Preset", preset))
			{
				const std::string category = preset.get("category", "");
				const std::string active_preset = preset.get("preset", "");
				graphic_pack_entries[path].try_emplace(category, active_preset);
			}
		}

	}

	// graphics
	auto graphic = parser.get("Graphic");
	graphic_api = graphic.get("api", kOpenGL);
	graphic.get("vkDevice", vk_graphic_device_uuid);
	mtl_graphic_device_uuid = graphic.get("mtlDevice", 0);
	vsync = graphic.get("VSync", 0);
	overrideAppGammaPreference = graphic.get("OverrideAppGammaPreference", false);
	overrideGammaValue = graphic.get("OverrideGammaValue", 2.2f);
	if(overrideGammaValue < 0)
		overrideGammaValue = 2.2f;
	userDisplayGamma = graphic.get("UserDisplayGamma", 2.2f);
	if(userDisplayGamma < 0)
		userDisplayGamma = 2.2f;
	gx2drawdone_sync = graphic.get("GX2DrawdoneSync", true);
	upscale_filter = graphic.get("UpscaleFilter", kBicubicHermiteFilter);
	downscale_filter = graphic.get("DownscaleFilter", kLinearFilter);
	fullscreen_scaling = graphic.get("FullscreenScaling", kKeepAspectRatio);
	async_compile = graphic.get("AsyncCompile", async_compile);
	vk_accurate_barriers = graphic.get("vkAccurateBarriers", true); // this used to be "VulkanAccurateBarriers" but because we changed the default to true in 1.27.1 the option name had to be changed
	force_mesh_shaders = graphic.get("ForceMeshShaders", false);

	auto overlay_node = graphic.get("Overlay");
	if(overlay_node.valid())
	{
		overlay.position = overlay_node.get("Position", ScreenPosition::kDisabled);
		overlay.text_color = overlay_node.get("TextColor", 0xFFFFFFFF);
		overlay.text_scale = overlay_node.get("TextScale", 100);
		overlay.fps = overlay_node.get("FPS", true);
		overlay.drawcalls = overlay_node.get("DrawCalls", false);
		overlay.cpu_usage = overlay_node.get("CPUUsage", false);
		overlay.cpu_per_core_usage = overlay_node.get("CPUPerCoreUsage", false);
		overlay.ram_usage = overlay_node.get("RAMUsage", false);
		overlay.vram_usage = overlay_node.get("VRAMUsage", false);
		overlay.debug = overlay_node.get("Debug", false);

		notification.controller_profiles = overlay_node.get("ControllerProfiles", true);
		notification.controller_battery = overlay_node.get("ControllerBattery", true);
		notification.shader_compiling = overlay_node.get("ShaderCompiling", true);
	}
	else
	{
		// legacy support
		overlay.position = graphic.get("OverlayPosition", ScreenPosition::kDisabled);
		overlay.text_color = graphic.get("OverlayTextColor", 0xFFFFFFFF);
		overlay.fps = graphic.get("OverlayFPS", true);
		overlay.drawcalls = graphic.get("OverlayDrawCalls", false);
		overlay.cpu_usage = graphic.get("OverlayCPUUsage", false);
		overlay.cpu_per_core_usage = graphic.get("OverlayCPUPerCoreUsage", false);
		overlay.ram_usage = graphic.get("OverlayRAMUsage", false);

		notification.controller_profiles = graphic.get("OverlayControllerProfiles", true);
		notification.controller_battery = graphic.get("OverlayControllerBattery", true);
		notification.shader_compiling = graphic.get("ShaderCompiling", true);
	}

	auto notification_node = graphic.get("Notification");
	if (notification_node.valid())
	{
		notification.position = notification_node.get("Position", ScreenPosition::kTopLeft);
		notification.text_color = notification_node.get("TextColor", 0xFFFFFFFF);
		notification.text_scale = notification_node.get("TextScale", 100);
		notification.controller_profiles = notification_node.get("ControllerProfiles", true);
		notification.controller_battery = notification_node.get("ControllerBattery", false);
		notification.shader_compiling = notification_node.get("ShaderCompiling", true);
		notification.friends = notification_node.get("FriendService", true);
	}

	// audio
	auto audio = parser.get("Audio");
	audio_api = audio.get("api", 0);
	audio_delay = audio.get("delay", 2);
	tv_channels = audio.get("TVChannels", kStereo);
	pad_channels = audio.get("PadChannels", kStereo);
	input_channels = audio.get("InputChannels", kMono);
	tv_volume = audio.get("TVVolume", 20);
	pad_volume = audio.get("PadVolume", 0);
	input_volume = audio.get("InputVolume", 20);
	portal_volume = audio.get("PortalVolume", 20);

	const auto tv = audio.get("TVDevice", "");
	try
	{
		tv_device = boost::nowide::widen(tv);
	}
	catch (const std::exception&)
	{
		cemuLog_log(LogType::Force, "config load error: can't load tv device: {}", tv);
	}

	const auto pad = audio.get("PadDevice", "");
	try
	{
		pad_device = boost::nowide::widen(pad);
	}
	catch (const std::exception&)
	{
		cemuLog_log(LogType::Force, "config load error: can't load pad device: {}", pad);
	}

	const auto input_device_name = audio.get("InputDevice", "");
	try
	{
		input_device = boost::nowide::widen(input_device_name);
	}
	catch (const std::exception&)
	{
		cemuLog_log(LogType::Force, "config load error: can't load input device: {}", input_device_name);
	}

	const auto portal_device_name = audio.get("PortalDevice", "");
	try
	{
		portal_device = boost::nowide::widen(portal_device_name);
	}
	catch (const std::exception&)
	{
		cemuLog_log(LogType::Force, "config load error: can't load input device: {}", portal_device_name);
	}

	// account
	auto acc = parser.get("Account");
	account.m_persistent_id = acc.get("PersistentId", account.m_persistent_id);
	// legacy online settings, we only parse these for upgrading purposes
	account.legacy_online_enabled = acc.get("OnlineEnabled", account.legacy_online_enabled);
	account.legacy_active_service = acc.get("ActiveService",account.legacy_active_service);
	// per-account online setting
	auto accService = parser.get("AccountService");
	account.service_select.clear();
	for (auto element = accService.get("SelectedService"); element.valid(); element = accService.get("SelectedService", element))
	{
		uint32 persistentId = element.get_attribute<uint32>("PersistentId", 0);
		sint32 serviceIndex = element.get_attribute<sint32>("Service", 0);
		NetworkService networkService = static_cast<NetworkService>(serviceIndex);
		if (persistentId < Account::kMinPersistendId)
			continue;
		if(networkService == NetworkService::Offline || networkService == NetworkService::Nintendo || networkService == NetworkService::Pretendo || networkService == NetworkService::Custom)
			account.service_select.emplace(persistentId, networkService);
	}
	// debug
	auto debug = parser.get("Debug");
#if BOOST_OS_WINDOWS
	crash_dump = debug.get("CrashDumpWindows", crash_dump);
#elif BOOST_OS_UNIX
	crash_dump = debug.get("CrashDumpUnix", crash_dump);
#endif
	gdb_port = debug.get("GDBPort", 1337);
	gpu_capture_dir = debug.get("GPUCaptureDir", "");
	framebuffer_fetch = debug.get("FramebufferFetch", true);

	// input
	auto input = parser.get("Input");
	auto dsuc = input.get("DSUC");
	dsu_client.host = dsuc.get_attribute("host", dsu_client.host);
	dsu_client.port = dsuc.get_attribute("port", dsu_client.port);

	// emulatedusbdevices
	auto usbdevices = parser.get("EmulatedUsbDevices");
	emulated_usb_devices.emulate_skylander_portal = usbdevices.get("EmulateSkylanderPortal", emulated_usb_devices.emulate_skylander_portal);
	emulated_usb_devices.emulate_infinity_base = usbdevices.get("EmulateInfinityBase", emulated_usb_devices.emulate_infinity_base);
	emulated_usb_devices.emulate_dimensions_toypad = usbdevices.get("EmulateDimensionsToypad", emulated_usb_devices.emulate_dimensions_toypad);

	return parser;
}

XMLConfigParser CemuConfig::Save(XMLConfigParser& parser)
{
	auto config = parser.set("content");
	// general settings
	config.set("logflag", log_flag.GetValue());
	config.set("advanced_ppc_logging", advanced_ppc_logging.GetValue());
	config.set("mlc_path", mlc_path.GetValue().c_str());
	config.set<bool>("permanent_storage", permanent_storage);
	config.set("proxy_server", proxy_server.GetValue().c_str());
	config.set<bool>("play_boot_sound", play_boot_sound);

	// config.set("cpu_mode", cpu_mode.GetValue());
	//config.set("console_region", console_region.GetValue());
	config.set("console_language", console_language.GetValue());

	// game paths
	auto game_path_parser = config.set("GamePaths");
	for (const auto& entry : game_paths)
	{
		game_path_parser.set("Entry", entry.c_str());
	}

	// game list cache
	std::unique_lock _lock(game_cache_entries_mutex);
	auto game_cache_parser = config.set("GameCache");
	for (const auto& game : game_cache_entries)
	{
		auto entry = game_cache_parser.set("Entry");

		entry.set("title_id", (sint64)game.title_id);
		entry.set("name", boost::nowide::narrow(game.legacy_name).c_str());
		entry.set("custom_name", game.custom_name.c_str());
		entry.set("region", (sint32)game.legacy_region);
		entry.set("version", (sint32)game.legacy_update_version);
		entry.set("dlc_version", (sint32)game.legacy_dlc_version);
		entry.set("path", boost::nowide::narrow(game.rpx_file).c_str());
		entry.set("time_played", game.legacy_time_played);
		entry.set("last_played", game.legacy_last_played);
		entry.set("favorite", game.favorite);
	}
	_lock.unlock();

	auto graphic_pack_parser = config.set("GraphicPack");
	for (const auto& game : graphic_pack_entries)
	{
		auto entry = graphic_pack_parser.set("Entry");
		entry.set_attribute("filename",_pathToUtf8(game.first).c_str());
		for(const auto& kv : game.second)
		{
			// TODO: less hacky pls
			if(boost::iequals(kv.first, "_disabled"))
			{
				entry.set_attribute("disabled", true);
				continue;
			}

			auto preset = entry.set("Preset");
			if(!kv.first.empty())
				preset.set("category", kv.first.c_str());

			preset.set("preset", kv.second.c_str());
		}
	}

	// graphics
	auto graphic = config.set("Graphic");
	graphic.set("api", graphic_api);
	graphic.set("vkDevice", vk_graphic_device_uuid);
	graphic.set("mtlDevice", mtl_graphic_device_uuid);
	graphic.set("VSync", vsync);
	graphic.set("OverrideAppGammaPreference", overrideAppGammaPreference);
	graphic.set("OverrideGammaValue", overrideGammaValue);
	graphic.set("UserDisplayGamma", userDisplayGamma);
	graphic.set("GX2DrawdoneSync", gx2drawdone_sync);
	graphic.set("ForceMeshShaders", force_mesh_shaders);
	//graphic.set("PrecompiledShaders", precompiled_shaders.GetValue());
	graphic.set("UpscaleFilter", upscale_filter);
	graphic.set("DownscaleFilter", downscale_filter);
	graphic.set("FullscreenScaling", fullscreen_scaling);
	graphic.set("AsyncCompile", async_compile.GetValue());
	graphic.set("vkAccurateBarriers", vk_accurate_barriers);

	auto overlay_node = graphic.set("Overlay");
	overlay_node.set("Position", overlay.position);
	overlay_node.set("TextColor", overlay.text_color);
	overlay_node.set("TextScale", overlay.text_scale);
	overlay_node.set("FPS", overlay.fps);
	overlay_node.set("DrawCalls", overlay.drawcalls);
	overlay_node.set("CPUUsage", overlay.cpu_usage);
	overlay_node.set("CPUPerCoreUsage", overlay.cpu_per_core_usage);
	overlay_node.set("RAMUsage", overlay.ram_usage);
	overlay_node.set("VRAMUsage", overlay.vram_usage);
	overlay_node.set("Debug", overlay.debug);

	auto notification_node = graphic.set("Notification");
	notification_node.set("Position", notification.position);
	notification_node.set("TextColor", notification.text_color);
	notification_node.set("TextScale", notification.text_scale);
	notification_node.set("ControllerProfiles", notification.controller_profiles);
	notification_node.set("ControllerBattery", notification.controller_battery);
	notification_node.set("ShaderCompiling", notification.shader_compiling);
	notification_node.set("FriendService", notification.friends);

	// audio
	auto audio = config.set("Audio");
	audio.set("api", audio_api);
	audio.set("delay", audio_delay);
	audio.set("TVChannels", tv_channels);
	audio.set("PadChannels", pad_channels);
	audio.set("InputChannels", input_channels);
	audio.set("TVVolume", tv_volume);
	audio.set("PadVolume", pad_volume);
	audio.set("InputVolume", input_volume);
	audio.set("PortalVolume", portal_volume);
	audio.set("TVDevice", boost::nowide::narrow(tv_device).c_str());
	audio.set("PadDevice", boost::nowide::narrow(pad_device).c_str());
	audio.set("InputDevice", boost::nowide::narrow(input_device).c_str());
	audio.set("PortalDevice", boost::nowide::narrow(portal_device).c_str());

	// account
	auto acc = config.set("Account");
	acc.set("PersistentId", account.m_persistent_id.GetValue());
	// legacy online mode setting
	acc.set("OnlineEnabled", account.legacy_online_enabled.GetValue());
	acc.set("ActiveService",account.legacy_active_service.GetValue());
	// per-account online setting
	auto accService = config.set("AccountService");
	for(auto& it : account.service_select)
	{
		auto entry = accService.set("SelectedService");
		entry.set_attribute("PersistentId", it.first);
		entry.set_attribute("Service", static_cast<sint32>(it.second));
	}
	// debug
	auto debug = config.set("Debug");
#if BOOST_OS_WINDOWS
	debug.set("CrashDumpWindows", crash_dump.GetValue());
#elif BOOST_OS_UNIX
	debug.set("CrashDumpUnix", crash_dump.GetValue());
#endif
	debug.set("GDBPort", gdb_port);
	debug.set("GPUCaptureDir", gpu_capture_dir);
	debug.set("FramebufferFetch", framebuffer_fetch);

	// input
	auto input = config.set("Input");
	auto dsuc = input.set("DSUC");
	dsuc.set_attribute("host", dsu_client.host);
	dsuc.set_attribute("port", dsu_client.port);

	// emulated usb devices
	auto usbdevices = config.set("EmulatedUsbDevices");
	usbdevices.set("EmulateSkylanderPortal", emulated_usb_devices.emulate_skylander_portal.GetValue());
	usbdevices.set("EmulateInfinityBase", emulated_usb_devices.emulate_infinity_base.GetValue());
	usbdevices.set("EmulateDimensionsToypad", emulated_usb_devices.emulate_dimensions_toypad.GetValue());

	return config;
}

GameEntry* CemuConfig::GetGameEntryByTitleId(uint64 titleId)
{
	// assumes game_cache_entries_mutex is already held
	for (auto& it : game_cache_entries)
	{
		if (it.title_id == titleId)
			return &it;
	}
	return nullptr;
}

GameEntry* CemuConfig::CreateGameEntry(uint64 titleId)
{
	// assumes game_cache_entries_mutex is already held
	GameEntry gameEntry;
	gameEntry.title_id = titleId;
	game_cache_entries.emplace_back(gameEntry);
	return &game_cache_entries.back();
}

bool CemuConfig::IsGameListFavorite(uint64 titleId)
{
	std::unique_lock _lock(game_cache_entries_mutex);
	return game_cache_favorites.find(titleId) != game_cache_favorites.end();
}

void CemuConfig::SetGameListFavorite(uint64 titleId, bool isFavorite)
{
	std::unique_lock _lock(game_cache_entries_mutex);
	GameEntry* gameEntry = GetGameEntryByTitleId(titleId);
	if (!gameEntry)
		gameEntry = CreateGameEntry(titleId);
	gameEntry->favorite = isFavorite;
	if (isFavorite)
		game_cache_favorites.emplace(titleId);
	else
		game_cache_favorites.erase(titleId);
}

bool CemuConfig::GetGameListCustomName(uint64 titleId, std::string& customName)
{
	std::unique_lock _lock(game_cache_entries_mutex);
	GameEntry* gameEntry = GetGameEntryByTitleId(titleId);
	if (gameEntry && !gameEntry->custom_name.empty())
	{
		customName = gameEntry->custom_name;
		return true;
	}
	return false;
}

void CemuConfig::SetGameListCustomName(uint64 titleId, std::string customName)
{
	std::unique_lock _lock(game_cache_entries_mutex);
	GameEntry* gameEntry = GetGameEntryByTitleId(titleId);
	if (!gameEntry)
	{
		if (customName.empty())
			return;
		gameEntry = CreateGameEntry(titleId);
	}
	gameEntry->custom_name = std::move(customName);
}

NetworkService CemuConfig::GetAccountNetworkService(uint32 persistentId)
{
	auto it = account.service_select.find(persistentId);
	if (it != account.service_select.end())
	{
		NetworkService serviceIndex = it->second;
		// make sure the returned service is valid
		if (serviceIndex != NetworkService::Offline &&
			serviceIndex != NetworkService::Nintendo &&
			serviceIndex != NetworkService::Pretendo &&
			serviceIndex != NetworkService::Custom)
			return NetworkService::Offline;
		if( static_cast<NetworkService>(serviceIndex) == NetworkService::Custom && !NetworkConfig::XMLExists() )
			return NetworkService::Offline; // custom is selected but no custom config exists
		return serviceIndex;
	}
	// if not found, return the legacy value
	if(!account.legacy_online_enabled)
		return NetworkService::Offline;
	return static_cast<NetworkService>(account.legacy_active_service.GetValue() + 1); // +1 because "Offline" now takes index 0
}

void CemuConfig::SetAccountSelectedService(uint32 persistentId, NetworkService serviceIndex)
{
	account.service_select[persistentId] = serviceIndex;
}
