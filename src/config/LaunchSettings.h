#pragma once

#include <optional>
#include <string>

class LaunchSettings
{
public:
	// winmain
	static bool HandleCommandline(const wchar_t* lpCmdLine);
	// wmain
	static bool HandleCommandline(int argc, wchar_t* argv[]);
	// main (unix)
	static bool HandleCommandline(int argc, char* argv[]);

	static bool HandleCommandline(const std::vector<std::wstring>& args);

	static std::optional<fs::path> GetLoadFile() { return s_load_game_file; }
    static std::optional<uint64> GetLoadTitleID() {return s_load_title_id;}
	static std::optional<fs::path> GetMLCPath() { return s_mlc_path; }

	static std::optional<bool> RenderUpsideDownEnabled() { return s_render_upside_down; }
	static std::optional<bool> FullscreenEnabled() { return s_fullscreen; }

	static bool GDBStubEnabled() { return s_enable_gdbstub; }
	static bool NSightModeEnabled() { return s_nsight_mode; }
	static bool ForceIntelLegacyEnabled() { return s_force_intel_legacy; }

	static bool ForceInterpreter() { return s_force_interpreter; };

	static std::optional<uint32> GetPersistentId() { return s_persistent_id; }

	static std::string GetActURLPrefix() { return serviceURL_ACT; }
	static std::string GetServiceURL_ecs() { return serviceURL_ECS; }
	static void ChangeNetworkServiceURL(int ID);

private:
	inline static std::optional<fs::path> s_load_game_file{};
    inline static std::optional<uint64> s_load_title_id{};
	inline static std::optional<fs::path> s_mlc_path{};

	inline static std::optional<bool> s_render_upside_down{};
	inline static std::optional<bool> s_fullscreen{};
	
	inline static bool s_enable_gdbstub = false;
	inline static bool s_nsight_mode = false;
	inline static bool s_force_intel_legacy = false;

	inline static bool s_force_interpreter = false;
	
	inline static std::optional<uint32> s_persistent_id{};

	// service URLS
	inline static std::string serviceURL_ACT;
	inline static std::string serviceURL_ECS;
	// todo - npts and other boss urls


	static bool ExtractorTool(std::wstring_view wud_path, std::string_view output_path, std::wstring_view log_path);
};


