#include "config/CemuConfig.h"

#include "util/helpers/helpers.h"
#include "config/ActiveSettings.h"

#include <wx/language.h>

#include "PermanentConfig.h"
#include "ActiveSettings.h"

XMLCemuConfig_t g_config(L"settings.xml");

void CemuConfig::SetMLCPath(fs::path path, bool save)
{
	mlc_path.SetValue(_pathToUtf8(path));
	if(save)
		g_config.Save();

	// if custom mlc path has been selected, store it in permanent config
	if (path != ActiveSettings::GetDefaultMLCPath())
	{
		try
		{
			auto pconfig = PermanentConfig::Load();
			pconfig.custom_mlc_path = _pathToUtf8(path);
			pconfig.Store();
		}
		catch (const PSDisabledException&) {}
		catch (const std::exception& ex)
		{
			cemuLog_log(LogType::Force, "can't store custom mlc path in permanent storage: {}", ex.what());
		}
	}

	Account::RefreshAccounts();
}

void CemuConfig::Load(XMLConfigParser& parser)
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
	
	language = parser.get<sint32>("language", wxLANGUAGE_DEFAULT);
	use_discord_presence = parser.get("use_discord_presence", true);
	fullscreen_menubar = parser.get("fullscreen_menubar", false);
	feral_gamemode = parser.get("feral_gamemode", false);
	check_update = parser.get("check_update", check_update);
	save_screenshot = parser.get("save_screenshot", save_screenshot);
	did_show_vulkan_warning = parser.get("vk_warning", did_show_vulkan_warning);
	did_show_graphic_pack_download = parser.get("gp_download", did_show_graphic_pack_download);
	did_show_macos_disclaimer = parser.get("macos_disclaimer", did_show_macos_disclaimer);
	fullscreen = parser.get("fullscreen", fullscreen);
	proxy_server = parser.get("proxy_server", "");
	disable_screensaver = parser.get("disable_screensaver", disable_screensaver);
	console_language = parser.get("console_language", console_language.GetInitValue());

	window_position.x = parser.get("window_position").get("x", -1);
	window_position.y = parser.get("window_position").get("y", -1);

	window_size.x = parser.get("window_size").get("x", -1);
	window_size.y = parser.get("window_size").get("y", -1);
	window_maximized = parser.get("window_maximized", false);

	pad_open = parser.get("open_pad", false);
	pad_position.x = parser.get("pad_position").get("x", -1);
	pad_position.y = parser.get("pad_position").get("y", -1);

	pad_size.x = parser.get("pad_size").get("x", -1);
	pad_size.y = parser.get("pad_size").get("y", -1);
	pad_maximized = parser.get("pad_maximized", false);

	auto gamelist = parser.get("GameList");
	game_list_style = gamelist.get("style", 0);
	game_list_column_order = gamelist.get("order", "");

	// return default width if value in config file out of range
	auto loadColumnSize = [&gamelist] (const char *name, uint32 defaultWidth)
	{
		sint64 val = gamelist.get(name, DefaultColumnSize::name);
		if (val < 0 || val > (sint64) std::numeric_limits<uint32>::max)
			return defaultWidth;
		return static_cast<uint32>(val);
	};
	column_width.name = loadColumnSize("name_width", DefaultColumnSize::name);
	column_width.version = loadColumnSize("version_width", DefaultColumnSize::version);
	column_width.dlc = loadColumnSize("dlc_width", DefaultColumnSize::dlc);
	column_width.game_time = loadColumnSize("game_time_width", DefaultColumnSize::game_time);
	column_width.game_started = loadColumnSize("game_started_width", DefaultColumnSize::game_started);
	column_width.region = loadColumnSize("region_width", DefaultColumnSize::region);
    column_width.title_id = loadColumnSize("title_id", DefaultColumnSize::title_id);

	recent_launch_files.clear();
	auto launch_parser = parser.get("RecentLaunchFiles");
	for (auto element = launch_parser.get("Entry"); element.valid(); element = launch_parser.get("Entry", element))
	{
		const std::string path = element.value("");
		if (path.empty())
			continue;

		try
		{
			recent_launch_files.emplace_back(path);
		}
		catch (const std::exception&)
		{
			cemuLog_log(LogType::Force, "config load error: can't load recently launched game file: {}", path);
		}
	}
	
	recent_nfc_files.clear();
	auto nfc_parser = parser.get("RecentNFCFiles");
	for (auto element = nfc_parser.get("Entry"); element.valid(); element = nfc_parser.get("Entry", element))
	{
		const std::string path = element.value("");
		if (path.empty())
			continue;
		try
		{
			recent_nfc_files.emplace_back(path);
		}
		catch (const std::exception&)
		{
			cemuLog_log(LogType::Force, "config load error: can't load recently launched nfc file: {}", path);
		}
	}

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
	graphic.get("device", graphic_device_uuid);
	vsync = graphic.get("VSync", 0);
	gx2drawdone_sync = graphic.get("GX2DrawdoneSync", true);
	upscale_filter = graphic.get("UpscaleFilter", kBicubicHermiteFilter);
	downscale_filter = graphic.get("DownscaleFilter", kLinearFilter);
	fullscreen_scaling = graphic.get("FullscreenScaling", kKeepAspectRatio);
	async_compile = graphic.get("AsyncCompile", async_compile);
	vk_accurate_barriers = graphic.get("vkAccurateBarriers", true); // this used to be "VulkanAccurateBarriers" but because we changed the default to true in 1.27.1 the option name had to be changed

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

	// account
	auto acc = parser.get("Account");
	account.m_persistent_id = acc.get("PersistentId", account.m_persistent_id);
	account.online_enabled = acc.get("OnlineEnabled", account.online_enabled);
	account.active_service = acc.get("ActiveService",account.active_service);
	// debug
	auto debug = parser.get("Debug");
#if BOOST_OS_WINDOWS
	crash_dump = debug.get("CrashDumpWindows", crash_dump);
#elif BOOST_OS_UNIX
	crash_dump = debug.get("CrashDumpUnix", crash_dump);
#endif
	gdb_port = debug.get("GDBPort", 1337);

	// input
	auto input = parser.get("Input");
	auto dsuc = input.get("DSUC");
	dsu_client.host = dsuc.get_attribute("host", dsu_client.host);
	dsu_client.port = dsuc.get_attribute("port", dsu_client.port);
}

void CemuConfig::Save(XMLConfigParser& parser)
{
	auto config = parser.set("content");
	// general settings
	config.set("logflag", log_flag.GetValue());
	config.set("advanced_ppc_logging", advanced_ppc_logging.GetValue());
	config.set("mlc_path", mlc_path.GetValue().c_str());
	config.set<bool>("permanent_storage", permanent_storage);
	config.set<sint32>("language", language);
	config.set<bool>("use_discord_presence", use_discord_presence);
	config.set<bool>("fullscreen_menubar", fullscreen_menubar);
    	config.set<bool>("feral_gamemode", feral_gamemode);
	config.set<bool>("check_update", check_update);
	config.set<bool>("save_screenshot", save_screenshot);
	config.set<bool>("vk_warning", did_show_vulkan_warning);
	config.set<bool>("gp_download", did_show_graphic_pack_download);
	config.set<bool>("macos_disclaimer", did_show_macos_disclaimer);
	config.set<bool>("fullscreen", fullscreen);
	config.set("proxy_server", proxy_server.GetValue().c_str());
	config.set<bool>("disable_screensaver", disable_screensaver);

	// config.set("cpu_mode", cpu_mode.GetValue());
	//config.set("console_region", console_region.GetValue());
	config.set("console_language", console_language.GetValue());
	
	auto wpos = config.set("window_position");
	wpos.set<sint32>("x", window_position.x);
	wpos.set<sint32>("y", window_position.y);
	auto wsize = config.set("window_size");
	wsize.set<sint32>("x", window_size.x);
	wsize.set<sint32>("y", window_size.y);
	config.set<bool>("window_maximized", window_maximized);

	config.set<bool>("open_pad", pad_open);
	auto ppos = config.set("pad_position");
	ppos.set<sint32>("x", pad_position.x);
	ppos.set<sint32>("y", pad_position.y);
	auto psize = config.set("pad_size");
	psize.set<sint32>("x", pad_size.x);
	psize.set<sint32>("y", pad_size.y);
	config.set<bool>("pad_maximized", pad_maximized);

	auto gamelist = config.set("GameList");
	gamelist.set("style", game_list_style);
	gamelist.set("order", game_list_column_order);
	gamelist.set("name_width", column_width.name);
	gamelist.set("version_width", column_width.version);
	gamelist.set("dlc_width", column_width.dlc);
	gamelist.set("game_time_width", column_width.game_time);
	gamelist.set("game_started_width", column_width.game_started);
	gamelist.set("region_width", column_width.region);
    gamelist.set("title_id", column_width.title_id);

	auto launch_files_parser = config.set("RecentLaunchFiles");
	for (const auto& entry : recent_launch_files)
	{
		launch_files_parser.set("Entry", entry.c_str());
	}
	
	auto nfc_files_parser = config.set("RecentNFCFiles");
	for (const auto& entry : recent_nfc_files)
	{
		nfc_files_parser.set("Entry", entry.c_str());
	}
		
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
	graphic.set("device", graphic_device_uuid);
	graphic.set("VSync", vsync);
	graphic.set("GX2DrawdoneSync", gx2drawdone_sync);
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
	audio.set("TVDevice", boost::nowide::narrow(tv_device).c_str());
	audio.set("PadDevice", boost::nowide::narrow(pad_device).c_str());
	audio.set("InputDevice", boost::nowide::narrow(input_device).c_str());

	// account
	auto acc = config.set("Account");
	acc.set("PersistentId", account.m_persistent_id.GetValue());
	acc.set("OnlineEnabled", account.online_enabled.GetValue());
	acc.set("ActiveService",account.active_service.GetValue());
	// debug
	auto debug = config.set("Debug");
#if BOOST_OS_WINDOWS
	debug.set("CrashDumpWindows", crash_dump.GetValue());
#elif BOOST_OS_UNIX
	debug.set("CrashDumpUnix", crash_dump.GetValue());
#endif
	debug.set("GDBPort", gdb_port);

	// input
	auto input = config.set("Input");
	auto dsuc = input.set("DSUC");
	dsuc.set_attribute("host", dsu_client.host);
	dsuc.set_attribute("port", dsu_client.port);
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

void CemuConfig::AddRecentlyLaunchedFile(std::string_view file)
{
	recent_launch_files.insert(recent_launch_files.begin(), std::string(file));
	RemoveDuplicatesKeepOrder(recent_launch_files);
	while(recent_launch_files.size() > kMaxRecentEntries)
		recent_launch_files.pop_back();
}

void CemuConfig::AddRecentNfcFile(std::string_view file)
{
	recent_nfc_files.insert(recent_nfc_files.begin(), std::string(file));
	RemoveDuplicatesKeepOrder(recent_nfc_files);
	while (recent_nfc_files.size() > kMaxRecentEntries)
		recent_nfc_files.pop_back();
}
