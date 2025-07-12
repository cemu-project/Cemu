#pragma once

#include "config/CemuConfig.h"
#include "config/XMLConfig.h"
#include "util/math/vector2.h"

#include "wxgui.h"

namespace DefaultColumnSize
{
	enum : uint32
	{
		name = 500u,
		version = 60u,
		dlc = 50u,
		game_time = 140u,
		game_started = 160u,
		region = 80u,
		title_id = 160u
	};
};

typedef union
{
	struct
	{
		uint16 key : 13; // enough bits for all keycodes
		uint16 alt : 1;
		uint16 ctrl : 1;
		uint16 shift : 1;
	};
	uint16 raw;
} uKeyboardHotkey;

typedef sint16 ControllerHotkey_t;

struct sHotkeyCfg
{
	static constexpr uint8 keyboardNone{WXK_NONE};
	static constexpr sint8 controllerNone{-1}; // no enums to work with, but buttons start from 0

	uKeyboardHotkey keyboard{keyboardNone};
	ControllerHotkey_t controller{controllerNone};

	/* for defaults */
	sHotkeyCfg(const uKeyboardHotkey& keyboard = {keyboardNone}, const ControllerHotkey_t& controller = {controllerNone}) : keyboard(keyboard), controller(controller) {};

	/* for reading from xml */
	sHotkeyCfg(const char* xml_values)
	{
		std::istringstream iss(xml_values);
		iss >> keyboard.raw >> controller;
	}
};

template<>
struct fmt::formatter<sHotkeyCfg> : formatter<string_view>
{
	template<typename FormatContext>
	auto format(const sHotkeyCfg c, FormatContext& ctx) const
	{
		std::string xml_values = fmt::format("{} {}", c.keyboard.raw, c.controller);
		return formatter<string_view>::format(xml_values, ctx);
	}
};

struct wxCemuConfig
{
	ConfigValue<sint32> language{wxLANGUAGE_DEFAULT};
	ConfigValue<bool> use_discord_presence{true};
	ConfigValue<bool> fullscreen{ false };
	ConfigValue<bool> fullscreen_menubar{false};
	ConfigValue<bool> feral_gamemode{false};

	// max 15 entries
	static constexpr size_t kMaxRecentEntries = 15;
	std::vector<std::string> recent_launch_files;
	std::vector<std::string> recent_nfc_files;

	Vector2i window_position{-1, -1};
	Vector2i window_size{-1, -1};
	ConfigValue<bool> window_maximized;

	ConfigValue<bool> pad_open;
	Vector2i pad_position{-1, -1};
	Vector2i pad_size{-1, -1};
	ConfigValue<bool> pad_maximized;

	ConfigValue<bool> check_update{true};
	ConfigValue<bool> receive_untested_updates{false};
	ConfigValue<bool> save_screenshot{true};

	ConfigValue<bool> did_show_vulkan_warning{false};
	ConfigValue<bool> did_show_graphic_pack_download{false}; // no longer used but we keep the config value around in case people downgrade Cemu. Despite the name this was used for the Getting Started dialog
	ConfigValue<bool> did_show_macos_disclaimer{false};

	ConfigValue<bool> show_icon_column{true};

	int game_list_style = 0;
	std::string game_list_column_order;
	struct
	{
		uint32 name = DefaultColumnSize::name;
		uint32 version = DefaultColumnSize::version;
		uint32 dlc = DefaultColumnSize::dlc;
		uint32 game_time = DefaultColumnSize::game_time;
		uint32 game_started = DefaultColumnSize::game_started;
		uint32 region = DefaultColumnSize::region;
		uint32 title_id = 0;
	} column_width{};

	// hotkeys
	struct
	{
		sHotkeyCfg modifiers;
		sHotkeyCfg toggleFullscreen;
		sHotkeyCfg toggleFullscreenAlt;
		sHotkeyCfg exitFullscreen;
		sHotkeyCfg takeScreenshot;
		sHotkeyCfg toggleFastForward;
	} hotkeys{};

	void AddRecentlyLaunchedFile(std::string_view file);
	void AddRecentNfcFile(std::string_view file);

	void Load(XMLConfigParser& parser);
	void Save(XMLConfigParser& parser);
};

typedef XMLChildConfig<wxCemuConfig, &wxCemuConfig::Load, &wxCemuConfig::Save> XMLWxCemuConfig_t;

extern XMLWxCemuConfig_t g_wxConfig;

inline XMLWxCemuConfig_t& GetWxGuiConfigHandle()
{
	return g_wxConfig;
}

inline wxCemuConfig& GetWxGUIConfig()
{
	return GetWxGuiConfigHandle().Data();
}