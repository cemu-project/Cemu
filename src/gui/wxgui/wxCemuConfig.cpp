#include "wxCemuConfig.h"
#include "Common/precompiled.h"
#include "config/CemuConfig.h"
#include "config/XMLConfig.h"
#include "util/helpers/helpers.h"
#include <wx/language.h>

XMLWxCemuConfig_t g_wxConfig(&GetConfigHandle);

void wxCemuConfig::AddRecentlyLaunchedFile(std::string_view file)
{
	recent_launch_files.insert(recent_launch_files.begin(), std::string(file));
	RemoveDuplicatesKeepOrder(recent_launch_files);
	while (recent_launch_files.size() > kMaxRecentEntries)
		recent_launch_files.pop_back();
}

void wxCemuConfig::AddRecentNfcFile(std::string_view file)
{
	recent_nfc_files.insert(recent_nfc_files.begin(), std::string(file));
	RemoveDuplicatesKeepOrder(recent_nfc_files);
	while (recent_nfc_files.size() > kMaxRecentEntries)
		recent_nfc_files.pop_back();
}

void wxCemuConfig::Load(XMLConfigParser& parser)
{
	language = parser.get<sint32>("language", wxLANGUAGE_DEFAULT);
	use_discord_presence = parser.get("use_discord_presence", true);
	fullscreen_menubar = parser.get("fullscreen_menubar", false);
	feral_gamemode = parser.get("feral_gamemode", false);
	check_update = parser.get("check_update", check_update);
	receive_untested_updates = parser.get("receive_untested_updates", receive_untested_updates);
	save_screenshot = parser.get("save_screenshot", save_screenshot);
	did_show_vulkan_warning = parser.get("vk_warning", did_show_vulkan_warning);
	did_show_graphic_pack_download = parser.get("gp_download", did_show_graphic_pack_download);
	did_show_macos_disclaimer = parser.get("macos_disclaimer", did_show_macos_disclaimer);
	fullscreen = parser.get("fullscreen", fullscreen);

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

	show_icon_column = parser.get("show_icon_column", true);

	// return default width if value in config file out of range
	auto loadColumnSize = [&gamelist](const char* name, uint32 defaultWidth) {
		sint64 val = gamelist.get(name, DefaultColumnSize::name);
		if (val < 0 || val > (sint64)std::numeric_limits<uint32>::max)
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
		} catch (const std::exception&)
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
		} catch (const std::exception&)
		{
			cemuLog_log(LogType::Force, "config load error: can't load recently launched nfc file: {}", path);
		}
	}

	// hotkeys
	auto xml_hotkeys = parser.get("Hotkeys");
	hotkeys.modifiers = xml_hotkeys.get("modifiers", sHotkeyCfg{});
	hotkeys.exitFullscreen = xml_hotkeys.get("ExitFullscreen", sHotkeyCfg{uKeyboardHotkey{WXK_ESCAPE}});
	hotkeys.toggleFullscreen = xml_hotkeys.get("ToggleFullscreen", sHotkeyCfg{uKeyboardHotkey{WXK_F11}});
	hotkeys.toggleFullscreenAlt = xml_hotkeys.get("ToggleFullscreenAlt", sHotkeyCfg{uKeyboardHotkey{WXK_CONTROL_M, true}}); // ALT+ENTER
	hotkeys.takeScreenshot = xml_hotkeys.get("TakeScreenshot", sHotkeyCfg{uKeyboardHotkey{WXK_F12}});
	hotkeys.toggleFastForward = xml_hotkeys.get("ToggleFastForward", sHotkeyCfg{});
}

void wxCemuConfig::Save(XMLConfigParser& config)
{
	// general settings
	config.set<sint32>("language", language);
	config.set<bool>("use_discord_presence", use_discord_presence);
	config.set<bool>("fullscreen_menubar", fullscreen_menubar);
	config.set<bool>("feral_gamemode", feral_gamemode);
	config.set<bool>("check_update", check_update);
	config.set<bool>("receive_untested_updates", receive_untested_updates);
	config.set<bool>("save_screenshot", save_screenshot);
	config.set<bool>("vk_warning", did_show_vulkan_warning);
	config.set<bool>("gp_download", did_show_graphic_pack_download);
	config.set<bool>("macos_disclaimer", did_show_macos_disclaimer);
	config.set<bool>("fullscreen", fullscreen);

	auto wpos = config.set("window_position");
	wpos.set<sint32>("x", window_position.x);
	wpos.set<sint32>("y", window_position.y);
	auto wsize = config.set("window_size");
	wsize.set<sint32>("x", window_size.x);
	cemu_assert_debug(window_size.x != 0);
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
	config.set<bool>("show_icon_column", show_icon_column);

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

	// hotkeys
	auto xml_hotkeys = config.set("Hotkeys");
	xml_hotkeys.set("modifiers", hotkeys.modifiers);
	xml_hotkeys.set("ExitFullscreen", hotkeys.exitFullscreen);
	xml_hotkeys.set("ToggleFullscreen", hotkeys.toggleFullscreen);
	xml_hotkeys.set("ToggleFullscreenAlt", hotkeys.toggleFullscreenAlt);
	xml_hotkeys.set("TakeScreenshot", hotkeys.takeScreenshot);
	xml_hotkeys.set("ToggleFastForward", hotkeys.toggleFastForward);
}
