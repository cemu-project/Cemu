#pragma once

#include "ConfigValue.h"
#include "XMLConfig.h"
#include "util/math/vector2.h"
#include "Cafe/Account/Account.h"

#include <wx/language.h>
#include <wx/intl.h>

enum class NetworkService;

struct GameEntry
{
	GameEntry() = default;

	std::wstring rpx_file;
	std::wstring legacy_name;
	std::string product_code;
	std::string company_code;

	std::string custom_name;

	sint32 icon = -1;
	sint32 icon_small = -1;
	sint32 legacy_version = 0;
	sint32 legacy_update_version = -1;
	sint32 legacy_dlc_version = -1;
	uint64 title_id = 0;
	uint32 legacy_region = 0;

	std::wstring save_folder;
	std::wstring update_folder;
	std::wstring dlc_folder;
	
	uint64 legacy_time_played = 0;
	uint64 legacy_last_played = 0;

	bool favorite = false;

	bool is_update = false;
};

struct GraphicPackEntry
{
	GraphicPackEntry(std::wstring_view filename,std::string_view category, std::string_view preset)
		: filename(filename)
	{
		presets[std::string{category}] = preset;
	}

	GraphicPackEntry(std::wstring_view filename, std::string_view preset)
		: filename(filename)
	{
		presets[""] = preset;
	}

	GraphicPackEntry(std::wstring filename)
		: filename(std::move(filename))
	{}

	struct Preset
	{
		std::string name;
		std::string category;
	};
	std::unordered_map<std::string, std::string> presets; // category, active_preset

	std::wstring filename;
	bool enabled = true;
};

enum GraphicAPI
{
	kOpenGL = 0,
	kVulkan,
};

enum AudioChannels
{
	kMono = 0,
	kStereo,
	kSurround,
};

enum UpscalingFilter
{
	kLinearFilter,
	kBicubicFilter,
	kBicubicHermiteFilter,
	kNearestNeighborFilter,
};

enum FullscreenScaling
{
	kKeepAspectRatio,
	kStretch,
};

enum class ScreenPosition
{
	kDisabled = 0,
	kTopLeft,
	kTopCenter,
	kTopRight,
	kBottomLeft,
	kBottomCenter,
	kBottomRight,	
};

enum class PrecompiledShaderOption
{
	Auto,
	Enable,
	Disable,
};
ENABLE_ENUM_ITERATORS(PrecompiledShaderOption, PrecompiledShaderOption::Auto, PrecompiledShaderOption::Disable);

enum class AccurateShaderMulOption
{
	False = 0, // always use standard multiplication
	True = 1 // fully emulate non-ieee MUL special cases (0*anything = 0)
};
ENABLE_ENUM_ITERATORS(AccurateShaderMulOption, AccurateShaderMulOption::False, AccurateShaderMulOption::True);

enum class CPUMode
{
	SinglecoreInterpreter = 0,
	SinglecoreRecompiler = 1,
	DualcoreRecompiler = 2, // deprecated and not used anymore
	MulticoreRecompiler = 3,
	Auto = 4,
};
ENABLE_ENUM_ITERATORS(CPUMode, CPUMode::SinglecoreInterpreter, CPUMode::Auto);


enum class CPUModeLegacy 
{
	SinglecoreInterpreter = 0,
	SinglecoreRecompiler = 1,
	DualcoreRecompiler = 2,
	TriplecoreRecompiler = 3,
	Auto = 4
};
ENABLE_ENUM_ITERATORS(CPUModeLegacy, CPUModeLegacy::SinglecoreInterpreter, CPUModeLegacy::Auto);

enum class CafeConsoleRegion
{
	JPN = 0x1,
	USA = 0x2,
	EUR = 0x4,
	AUS_DEPR = 0x8,
	CHN = 0x10,
	KOR = 0x20,
	TWN = 0x40,
	Auto = 0xFF,
};
ENABLE_BITMASK_OPERATORS(CafeConsoleRegion);

enum class CafeConsoleLanguage
{
	JA = 0,
	EN = 1,
	FR = 2,
	DE = 3,
	IT = 4,
	ES = 5,
	ZH = 6,
	KO = 7,
	NL = 8,
	PT = 9,
	RU = 10,
	TW = 11,
};
ENABLE_ENUM_ITERATORS(CafeConsoleLanguage, CafeConsoleLanguage::JA, CafeConsoleLanguage::TW);

#if BOOST_OS_WINDOWS
enum class CrashDump
{
	Disabled,
	Lite,
	Full
};
ENABLE_ENUM_ITERATORS(CrashDump, CrashDump::Disabled, CrashDump::Full);
#elif BOOST_OS_UNIX
enum class CrashDump
{
	Disabled,
	Enabled
};
ENABLE_ENUM_ITERATORS(CrashDump, CrashDump::Disabled, CrashDump::Enabled);
#endif

template <>
struct fmt::formatter<PrecompiledShaderOption> : formatter<string_view> {
	template <typename FormatContext>
	auto format(const PrecompiledShaderOption c, FormatContext &ctx) {
		string_view name;
		switch (c)
		{
		case PrecompiledShaderOption::Auto: name = "auto"; break;
		case PrecompiledShaderOption::Enable: name = "true"; break;
		case PrecompiledShaderOption::Disable: name = "false"; break;
		default: name = "unknown"; break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};
template <>
struct fmt::formatter<AccurateShaderMulOption> : formatter<string_view> {
	template <typename FormatContext>
	auto format(const AccurateShaderMulOption c, FormatContext &ctx) {
		string_view name;
		switch (c)
		{
		case AccurateShaderMulOption::True: name = "true"; break;
		case AccurateShaderMulOption::False: name = "false"; break;
		default: name = "unknown"; break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};
template <>
struct fmt::formatter<CPUMode> : formatter<string_view> {
	template <typename FormatContext>
	auto format(const CPUMode c, FormatContext &ctx) {
		string_view name;
		switch (c)
		{
		case CPUMode::SinglecoreInterpreter: name = "Single-core interpreter"; break;
		case CPUMode::SinglecoreRecompiler: name = "Single-core recompiler"; break;
		case CPUMode::DualcoreRecompiler: name = "Dual-core recompiler"; break;
		case CPUMode::MulticoreRecompiler: name = "Multi-core recompiler"; break;
		case CPUMode::Auto: name = "Auto"; break;
		default: name = "unknown"; break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};
template <>
struct fmt::formatter<CPUModeLegacy> : formatter<string_view> {
	template <typename FormatContext>
	auto format(const CPUModeLegacy c, FormatContext &ctx) {
		string_view name;
		switch (c)
		{
		case CPUModeLegacy::SinglecoreInterpreter: name = "Singlecore-Interpreter"; break;
		case CPUModeLegacy::SinglecoreRecompiler: name = "Singlecore-Recompiler"; break;
		case CPUModeLegacy::DualcoreRecompiler: name = "Dualcore-Recompiler"; break;
		case CPUModeLegacy::TriplecoreRecompiler: name = "Triplecore-Recompiler"; break;
		case CPUModeLegacy::Auto: name = "Auto"; break;
		default: name = "unknown"; break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};
template <>
struct fmt::formatter<CafeConsoleRegion> : formatter<string_view> {
	template <typename FormatContext>
	auto format(const CafeConsoleRegion v, FormatContext &ctx) {
		string_view name;
		switch (v)
		{
		case CafeConsoleRegion::JPN: name = wxTRANSLATE("Japan"); break;
		case CafeConsoleRegion::USA: name = wxTRANSLATE("USA"); break;
		case CafeConsoleRegion::EUR: name = wxTRANSLATE("Europe"); break;
		case CafeConsoleRegion::AUS_DEPR: name = wxTRANSLATE("Australia"); break;
		case CafeConsoleRegion::CHN: name = wxTRANSLATE("China"); break;
		case CafeConsoleRegion::KOR: name = wxTRANSLATE("Korea"); break;
		case CafeConsoleRegion::TWN: name = wxTRANSLATE("Taiwan"); break;
		case CafeConsoleRegion::Auto: name = wxTRANSLATE("Auto"); break;
		default: name = wxTRANSLATE("many"); break;
		
		}
		return formatter<string_view>::format(name, ctx);
	}
};
template <>
struct fmt::formatter<CafeConsoleLanguage> : formatter<string_view> {
	template <typename FormatContext>
	auto format(const CafeConsoleLanguage v, FormatContext &ctx) {
		string_view name;
		switch (v)
		{
		case CafeConsoleLanguage::JA: name = "Japanese"; break;
		case CafeConsoleLanguage::EN: name = "English"; break;
		case CafeConsoleLanguage::FR: name = "French"; break;
		case CafeConsoleLanguage::DE: name = "German"; break;
		case CafeConsoleLanguage::IT: name = "Italian"; break;
		case CafeConsoleLanguage::ES: name = "Spanish"; break;
		case CafeConsoleLanguage::ZH: name = "Chinese"; break;
		case CafeConsoleLanguage::KO: name = "Korean"; break;
		case CafeConsoleLanguage::NL: name = "Dutch"; break;
		case CafeConsoleLanguage::PT: name = "Portugese"; break;
		case CafeConsoleLanguage::RU: name = "Russian"; break;
		case CafeConsoleLanguage::TW: name = "Taiwanese"; break;
		default: name = "unknown"; break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};

#if BOOST_OS_WINDOWS
template <>
struct fmt::formatter<CrashDump> : formatter<string_view> {
	template <typename FormatContext>
	auto format(const CrashDump v, FormatContext &ctx) {
		string_view name;
		switch (v)
		{
		case CrashDump::Disabled: name = "Disabled"; break;
		case CrashDump::Lite: name = "Lite"; break;
		case CrashDump::Full: name = "Full"; break;
		default: name = "unknown"; break;
		
		}
		return formatter<string_view>::format(name, ctx);
	}
};
#elif BOOST_OS_UNIX
template <>
struct fmt::formatter<CrashDump> : formatter<string_view> {
	template <typename FormatContext>
	auto format(const CrashDump v, FormatContext &ctx) {
		string_view name;
		switch (v)
		{
		case CrashDump::Disabled: name = "Disabled"; break;
		case CrashDump::Enabled: name = "Enabled"; break;
		default: name = "unknown"; break;

		}
		return formatter<string_view>::format(name, ctx);
	}
};
#endif

namespace DefaultColumnSize {
	enum : uint32 {
		name = 500u,
		version = 60u,
		dlc = 50u,
		game_time = 140u,
		game_started = 160u,
		region = 80u,
        title_id = 160u
	};
};

struct CemuConfig
{
	CemuConfig()
	{

	};

	CemuConfig(const CemuConfig&) = delete;

	// sets mlc path, updates permanent config value, saves config
	void SetMLCPath(fs::path path, bool save = true);

	ConfigValue<uint64> log_flag{ 0 };
	ConfigValue<bool> advanced_ppc_logging{ false };

	ConfigValue<bool> permanent_storage{ true };
	
	ConfigValue<sint32> language{ wxLANGUAGE_DEFAULT };
	ConfigValue<bool> use_discord_presence{ true };
	ConfigValue<std::string> mlc_path{};
	ConfigValue<bool> fullscreen_menubar{ false };
	ConfigValue<bool> fullscreen{ false };
	ConfigValue<bool> feral_gamemode{false};
	ConfigValue<std::string> proxy_server{};

	// temporary workaround because feature crashes on macOS
#if BOOST_OS_MACOS
#define DISABLE_SCREENSAVER_DEFAULT false
#else
#define DISABLE_SCREENSAVER_DEFAULT true
#endif
	ConfigValue<bool> disable_screensaver{DISABLE_SCREENSAVER_DEFAULT};
#undef DISABLE_SCREENSAVER_DEFAULT

	std::vector<std::string> game_paths;
	std::mutex game_cache_entries_mutex;
	std::vector<GameEntry> game_cache_entries;

	// optimized access
	std::set<uint64> game_cache_favorites; // per titleId
	
	struct _path_hash {
		std::size_t operator()(const fs::path& path) const {
			return fs::hash_value(path);
		}
	};
	// <filename, <category, preset>>
	std::unordered_map<fs::path, std::unordered_map<std::string, std::string>, _path_hash> graphic_pack_entries;

	ConfigValueBounds<CPUMode> cpu_mode{ CPUMode::Auto };
	ConfigValueBounds<CafeConsoleLanguage> console_language{ CafeConsoleLanguage::EN };

	// max 15 entries
	static constexpr size_t kMaxRecentEntries = 15;
	std::vector<std::string> recent_launch_files;
	std::vector<std::string> recent_nfc_files;

	Vector2i window_position{-1,-1};
	Vector2i window_size{ -1,-1 };
	ConfigValue<bool> window_maximized;

	ConfigValue<bool> pad_open;
	Vector2i pad_position{ -1,-1 };
	Vector2i pad_size{ -1,-1 };
	ConfigValue<bool> pad_maximized;

	ConfigValue<bool> check_update{false};
	ConfigValue<bool> save_screenshot{true};

	ConfigValue<bool> did_show_vulkan_warning{false};
	ConfigValue<bool> did_show_graphic_pack_download{false}; // no longer used but we keep the config value around in case people downgrade Cemu. Despite the name this was used for the Getting Started dialog
	ConfigValue<bool> did_show_macos_disclaimer{false};

	ConfigValue<bool> show_icon_column{ true };

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

	// graphics
	ConfigValue<GraphicAPI> graphic_api{ kVulkan };
	std::array<uint8, 16> graphic_device_uuid;
	ConfigValue<int> vsync{ 0 }; // 0 = off, 1+ = on depending on render backend
	ConfigValue<bool> gx2drawdone_sync {true};
	ConfigValue<bool> render_upside_down{ false };
	ConfigValue<bool> async_compile{ true };

	ConfigValue<bool> vk_accurate_barriers{ true };

	struct
	{
		ScreenPosition position = ScreenPosition::kDisabled;
		uint32 text_color = 0xFFFFFFFF;
		sint32 text_scale = 100;
		bool fps = true;
		bool drawcalls = false;
		bool cpu_usage = false;
		bool cpu_per_core_usage = false;
		bool ram_usage = false;
		bool vram_usage = false;
		bool debug = false;
	} overlay{};

	struct
	{
		ScreenPosition position = ScreenPosition::kTopLeft;
		uint32 text_color = 0xFFFFFFFF;
		sint32 text_scale = 100;
		bool controller_profiles = true;
		bool controller_battery = false;
		bool shader_compiling = true;
		bool friends = true;
	} notification{};

	ConfigValue<sint32> upscale_filter{kBicubicFilter};
	ConfigValue<sint32> downscale_filter{kLinearFilter};
	ConfigValue<sint32> fullscreen_scaling{kKeepAspectRatio};

	// audio
	sint32 audio_api = 0;
	sint32 audio_delay = 2;
	AudioChannels tv_channels = kStereo, pad_channels = kStereo, input_channels = kMono;
	sint32 tv_volume = 50, pad_volume = 0, input_volume = 50;
	std::wstring tv_device{ L"default" }, pad_device, input_device;

	// account
	struct
	{
		ConfigValueBounds<uint32> m_persistent_id{ Account::kMinPersistendId, Account::kMinPersistendId, 0xFFFFFFFF };
		ConfigValue<bool> legacy_online_enabled{false};
		ConfigValue<int> legacy_active_service{0};
		std::unordered_map<uint32, NetworkService> service_select; // per-account service index. Key is persistentId
	}account{};

	// input
	struct
	{
		ConfigValue<std::string> host{"127.0.0.1"};
		ConfigValue<uint16> port{ 26760 };
	}dsu_client{};

	// debug
	ConfigValueBounds<CrashDump> crash_dump{ CrashDump::Disabled };
	ConfigValue<uint16> gdb_port{ 1337 };

	void Load(XMLConfigParser& parser);
	void Save(XMLConfigParser& parser);

	void AddRecentlyLaunchedFile(std::string_view file);
	void AddRecentNfcFile(std::string_view file);

	bool IsGameListFavorite(uint64 titleId);
	void SetGameListFavorite(uint64 titleId, bool isFavorite);
	bool GetGameListCustomName(uint64 titleId, std::string& customName);
	void SetGameListCustomName(uint64 titleId, std::string customName);

	NetworkService GetAccountNetworkService(uint32 persistentId);
	void SetAccountSelectedService(uint32 persistentId, NetworkService serviceIndex);
	
	// emulated usb devices
	struct
	{
		ConfigValue<bool> emulate_skylander_portal{false};
		ConfigValue<bool> emulate_infinity_base{false};
	}emulated_usb_devices{};

	private:
	GameEntry* GetGameEntryByTitleId(uint64 titleId);
	GameEntry* CreateGameEntry(uint64 titleId);
};

typedef XMLDataConfig<CemuConfig, &CemuConfig::Load, &CemuConfig::Save> XMLCemuConfig_t;
extern XMLCemuConfig_t g_config;
inline CemuConfig& GetConfig() { return g_config.data(); }


