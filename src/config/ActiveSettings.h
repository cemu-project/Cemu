#pragma once

#include "config/CemuConfig.h"

// global active settings for fast access (reflects settings from command line and game profile)
class ActiveSettings
{
public:
	static void LoadOnce();
	
	[[nodiscard]] static fs::path GetFullPath() { return s_full_path; }
	[[nodiscard]] static fs::path GetPath() { return s_path; }
	[[nodiscard]] static fs::path GetFilename() { return s_filename; }
	
	[[nodiscard]] static fs::path GetMlcPath();

	[[nodiscard]] static fs::path GetPath(std::string_view p) 
	{
		std::basic_string_view<char8_t> s((const char8_t*)p.data(), p.size());
		return s_path / fs::path(s);
	}

	[[nodiscard]] static fs::path GetMlcPath(std::string_view p) 
	{ 
		std::basic_string_view<char8_t> s((const char8_t*)p.data(), p.size());
		return GetMlcPath() / fs::path(s);
	}
	
	template <typename ...TArgs>
	[[nodiscard]] static fs::path GetPath(std::string_view format, TArgs&&... args)
	{
		cemu_assert_debug(format.empty() || (format[0] != '/' && format[0] != '\\'));
		std::string tmpPathStr = fmt::format(fmt::runtime(format), std::forward<TArgs>(args)...);
		std::basic_string_view<char8_t> s((const char8_t*)tmpPathStr.data(), tmpPathStr.size());
		return s_path / fs::path(s);
	}
	
	template <typename ...TArgs>
	[[nodiscard]] static fs::path GetPath(std::wstring_view format, TArgs&&... args)
	{
		cemu_assert_debug(format.empty() || (format[0] != L'/' && format[0] != L'\\'));
		return s_path / fmt::format(format, std::forward<TArgs>(args)...);
	}
	
	template <typename ...TArgs>
	[[nodiscard]] static fs::path GetMlcPath(std::string_view format, TArgs&&... args)
	{
		cemu_assert_debug(format.empty() || (format[0] != '/' && format[0] != '\\'));
		auto tmp = fmt::format(fmt::runtime(format), std::forward<TArgs>(args)...);
		return GetMlcPath() / fs::path(_asUtf8(tmp));
	}
	
	template <typename ...TArgs>
	[[nodiscard]] static fs::path GetMlcPath(std::wstring_view format, TArgs&&... args)
	{
		cemu_assert_debug(format.empty() || (format[0] != L'/' && format[0] != L'\\'));
		return GetMlcPath() / fmt::format(fmt::runtime(format), std::forward<TArgs>(args)...);
	}
	
	// get mlc path to default cemu root dir/mlc01
	[[nodiscard]] static fs::path GetDefaultMLCPath();

private:
	inline static fs::path s_full_path; // full filename
	inline static fs::path s_path; // path
	inline static fs::path s_filename; // cemu.exe
	inline static fs::path s_mlc_path;

public:	
	// general
	[[nodiscard]] static bool LoadSharedLibrariesEnabled();
	[[nodiscard]] static bool DisplayDRCEnabled();
	[[nodiscard]] static bool FullscreenEnabled();
	
	// cpu
	[[nodiscard]] static CPUMode GetCPUMode();
	[[nodiscard]] static uint8 GetTimerShiftFactor();

	static void SetTimerShiftFactor(uint8 shiftFactor);
	
	// gpu
	[[nodiscard]] static PrecompiledShaderOption GetPrecompiledShadersOption();
	[[nodiscard]] static bool RenderUpsideDownEnabled();
	[[nodiscard]] static bool WaitForGX2DrawDoneEnabled();
	[[nodiscard]] static GraphicAPI GetGraphicsAPI();

	// audio
	[[nodiscard]] static bool AudioOutputOnlyAux();
	static void EnableAudioOnlyAux(bool state);

	// account
	[[nodiscard]] static uint32 GetPersistentId();
	[[nodiscard]] static bool IsOnlineEnabled();
	[[nodiscard]] static bool HasRequiredOnlineFiles();

	// dump options
	[[nodiscard]] static bool DumpShadersEnabled();
	[[nodiscard]] static bool DumpTexturesEnabled();
	[[nodiscard]] static bool DumpLibcurlRequestsEnabled();
	static void EnableDumpShaders(bool state);
	static void EnableDumpTextures(bool state);
	static void EnableDumpLibcurlRequests(bool state);

	// debug
	[[nodiscard]] static bool FrameProfilerEnabled();
	static void EnableFrameProfiler(bool state);

	// hacks
	[[nodiscard]] static bool VPADDelayEnabled();
	[[nodiscard]] static bool ShaderPreventInfiniteLoopsEnabled();
	[[nodiscard]] static bool FlushGPUCacheOnSwap();
	[[nodiscard]] static bool ForceSamplerRoundToPrecision();

private:
	// dump options
	inline static bool s_dump_shaders = false;
	inline static bool s_dump_textures = false;
	inline static bool s_dump_libcurl_requests = false;

	// timer speed
	inline static uint8 s_timer_shift = 3; // right shift factor, 0 -> 8x, 3 -> 1x, 4 -> 0.5x

	// debug
	inline static bool s_frame_profiler_enabled = false;
	inline static bool s_audio_aux_only = false;

	inline static bool s_has_required_online_files = false;
};

