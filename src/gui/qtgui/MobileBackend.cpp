#include "MobileBackend.h"

#include "interface/WindowSystem.h"
#include "Cafe/CafeSystem.h"
#include "Cafe/TitleList/TitleList.h"
#include "Cafe/TitleList/GameInfo.h"
#include "Cafe/Filesystem/fsc.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "config/CemuConfig.h"
#include "config/ActiveSettings.h"
#include "input/InputManager.h"
#include "util/helpers/helpers.h"
#include "Common/version.h"
#include "Cafe/GraphicPack/GraphicPack2.h"
#include "Cafe/TitleList/TitleId.h"
#include "audio/IAudioAPI.h"

#include <boost/nowide/convert.hpp>

#ifdef ENABLE_VULKAN
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#endif

#include <unistd.h>

extern "C" int __llvm_profile_write_file(void) __attribute__((weak));

namespace MobileBackend
{
	namespace
	{
		std::mutex s_callbackMutex;
		std::function<void()> s_gameListChangedCb;
		std::function<void()> s_gameExitedCb;
		std::mutex s_iconMutex;
		std::atomic_bool s_gameRunning = false;
		std::string s_runningGameName;
		bool s_initialized = false;

		void NotifyGameListChanged()
		{
			std::function<void()> cb;
			{
				std::lock_guard lock(s_callbackMutex);
				cb = s_gameListChangedCb;
			}
			if (cb)
				cb();
		}

		class SystemImpl : public CafeSystem::SystemImplementation
		{
		  public:
			void CafeRecreateCanvas() override
			{
				// the render window stays alive for the whole session, nothing to recreate
				cemuLog_log(LogType::Force, "MobileUI: canvas recreation requested (ignored)");
			}

			void CafePPCProcessExit() override
			{
				// called from the emulated PPC thread
				std::function<void()> cb;
				{
					std::lock_guard lock(s_callbackMutex);
					cb = s_gameExitedCb;
				}
				if (cb)
					cb();
			}
		} s_systemImpl;

		const char* RegionToString(CafeConsoleRegion region)
		{
			switch (region)
			{
			case CafeConsoleRegion::JPN: return "JPN";
			case CafeConsoleRegion::USA: return "USA";
			case CafeConsoleRegion::EUR: return "EUR";
			case CafeConsoleRegion::CHN: return "CHN";
			case CafeConsoleRegion::KOR: return "KOR";
			case CafeConsoleRegion::TWN: return "TWN";
			default: return "";
			}
		}

		// minimal TGA decoder (uncompressed and RLE true-color, 24/32 bit). Enough for meta/iconTex.tga
		bool DecodeTGA(const std::vector<uint8>& data, std::vector<uint8_t>& rgba, int& width, int& height)
		{
			if (data.size() < 18)
				return false;
			const uint8 idLength = data[0];
			const uint8 colorMapType = data[1];
			const uint8 imageType = data[2];
			const uint32 w = data[12] | (data[13] << 8);
			const uint32 h = data[14] | (data[15] << 8);
			const uint8 bpp = data[16];
			const uint8 descriptor = data[17];
			if (colorMapType != 0 || (imageType != 2 && imageType != 10))
				return false;
			if (bpp != 24 && bpp != 32)
				return false;
			if (w == 0 || h == 0 || w > 2048 || h > 2048)
				return false;
			const uint32 bytesPerPixel = bpp / 8;
			const size_t pixelCount = (size_t)w * h;
			size_t pos = 18 + (size_t)idLength;

			std::vector<uint8_t> out(pixelCount * 4);
			auto storePixel = [&](size_t index, const uint8* src) {
				// TGA stores BGR(A)
				out[index * 4 + 0] = src[2];
				out[index * 4 + 1] = src[1];
				out[index * 4 + 2] = src[0];
				out[index * 4 + 3] = bytesPerPixel == 4 ? src[3] : 0xFF;
			};

			if (imageType == 2)
			{
				if (data.size() < pos + pixelCount * bytesPerPixel)
					return false;
				for (size_t i = 0; i < pixelCount; i++)
					storePixel(i, data.data() + pos + i * bytesPerPixel);
			}
			else
			{
				size_t i = 0;
				while (i < pixelCount)
				{
					if (pos >= data.size())
						return false;
					const uint8 packet = data[pos++];
					const size_t count = (size_t)(packet & 0x7F) + 1;
					if (i + count > pixelCount)
						return false;
					if (packet & 0x80)
					{
						if (pos + bytesPerPixel > data.size())
							return false;
						for (size_t n = 0; n < count; n++)
							storePixel(i++, data.data() + pos);
						pos += bytesPerPixel;
					}
					else
					{
						if (pos + count * bytesPerPixel > data.size())
							return false;
						for (size_t n = 0; n < count; n++)
						{
							storePixel(i++, data.data() + pos);
							pos += bytesPerPixel;
						}
					}
				}
			}

			// rows are stored bottom-up unless bit 5 of the descriptor is set
			if ((descriptor & 0x20) == 0)
			{
				const size_t rowSize = (size_t)w * 4;
				std::vector<uint8_t> tmp(rowSize);
				for (uint32 y = 0; y < h / 2; y++)
				{
					uint8_t* top = out.data() + (size_t)y * rowSize;
					uint8_t* bottom = out.data() + (size_t)(h - 1 - y) * rowSize;
					memcpy(tmp.data(), top, rowSize);
					memcpy(top, bottom, rowSize);
					memcpy(bottom, tmp.data(), rowSize);
				}
			}
			rgba = std::move(out);
			width = (int)w;
			height = (int)h;
			return true;
		}

		void ApplyScanPaths()
		{
			CafeTitleList::ClearScanPaths();
			for (auto& it : GetConfig().game_paths)
				CafeTitleList::AddScanPath(_utf8ToPath(it));
			CafeTitleList::Refresh();
		}
	} // namespace

	void Initialize()
	{
		if (s_initialized)
			return;
		s_initialized = true;
		CafeSystem::SetImplementation(&s_systemImpl);
		WindowSystem::GetWindowInfo().app_active = true;
		// the mobile interface only has a Vulkan render window
		GetConfig().graphic_api = kVulkan;
		CafeTitleList::RegisterCallback([](CafeTitleListCallbackEvent* evt, void* ctx) { NotifyGameListChanged(); }, nullptr);
	}

	void SetGameListChangedCallback(std::function<void()> cb)
	{
		std::lock_guard lock(s_callbackMutex);
		s_gameListChangedCb = std::move(cb);
	}

	void SetGameExitedCallback(std::function<void()> cb)
	{
		std::lock_guard lock(s_callbackMutex);
		s_gameExitedCb = std::move(cb);
	}

	std::vector<GameEntry> GetGames()
	{
		std::vector<GameEntry> result;
		std::unordered_set<uint64> visited;
		for (TitleId titleId : CafeTitleList::GetAllTitleIds())
		{
			TitleId baseTitleId;
			if (!CafeTitleList::FindBaseTitleId(titleId, baseTitleId))
				continue;
			if (!visited.emplace(baseTitleId).second)
				continue;
			GameInfo2 gameInfo = CafeTitleList::GetGameInfo(baseTitleId);
			if (!gameInfo.IsValid() || gameInfo.IsSystemDataTitle())
				continue;
			GameEntry entry;
			entry.titleId = baseTitleId;
			entry.name = gameInfo.GetTitleName();
			std::replace(entry.name.begin(), entry.name.end(), '\n', ' ');
			std::replace(entry.name.begin(), entry.name.end(), '\r', ' ');
			if (entry.name.empty())
				entry.name = fmt::format("{:016x}", baseTitleId);
			entry.region = RegionToString(gameInfo.GetRegion());
			entry.version = gameInfo.GetVersion();
			entry.hasUpdate = gameInfo.HasUpdate();
			entry.hasDLC = gameInfo.HasAOC();
			result.emplace_back(std::move(entry));
		}
		std::sort(result.begin(), result.end(), [](const GameEntry& a, const GameEntry& b) {
			return boost::ilexicographical_compare(a.name, b.name);
		});
		return result;
	}

	void RefreshGameList()
	{
		CafeTitleList::Refresh();
	}

	bool IsScanning()
	{
		return CafeTitleList::IsScanning();
	}

	bool LoadGameIcon(uint64_t titleId, std::vector<uint8_t>& rgbaOut, int& widthOut, int& heightOut)
	{
		std::lock_guard lock(s_iconMutex);
		TitleInfo titleInfo;
		if (!CafeTitleList::GetFirstByTitleId(titleId, titleInfo))
			return false;
		std::string tempMountPath = TitleInfo::GetUniqueTempMountingPath();
		if (!titleInfo.Mount(tempMountPath, "", FSC_PRIORITY_BASE))
			return false;
		auto tgaData = fsc_extractFile((tempMountPath + "/meta/iconTex.tga").c_str());
		if (!tgaData)
		{
			tgaData = fsc_extractFile((tempMountPath + "/meta/iconTex.tga.gz").c_str());
			if (tgaData)
			{
				auto decompressed = zlibDecompress(*tgaData, 70 * 1024);
				std::swap(tgaData, decompressed);
			}
		}
		titleInfo.Unmount(tempMountPath);
		if (!tgaData || tgaData->size() <= 18)
			return false;
		return DecodeTGA(*tgaData, rgbaOut, widthOut, heightOut);
	}

	std::vector<std::string> GetGamePaths()
	{
		return GetConfig().game_paths;
	}

	void AddGamePath(const std::string& path)
	{
		if (path.empty())
			return;
		auto& paths = GetConfig().game_paths;
		if (std::find(paths.begin(), paths.end(), path) != paths.end())
			return;
		paths.emplace_back(path);
		GetConfigHandle().Save();
		ApplyScanPaths();
	}

	void RemoveGamePath(const std::string& path)
	{
		auto& paths = GetConfig().game_paths;
		auto it = std::find(paths.begin(), paths.end(), path);
		if (it == paths.end())
			return;
		paths.erase(it);
		GetConfigHandle().Save();
		ApplyScanPaths();
		NotifyGameListChanged();
	}

	void SetWindowSize(int width, int height, int physWidth, int physHeight, double scale)
	{
		auto& info = WindowSystem::GetWindowInfo();
		info.width = width;
		info.height = height;
		info.phys_width = physWidth;
		info.phys_height = physHeight;
		info.dpi_scale = scale;
	}

	bool LaunchGame(uint64_t titleId, const NativeWindow& window, std::string& errorOut)
	{
#ifndef ENABLE_VULKAN
		errorOut = "This build has no Vulkan support.";
		return false;
#else
		if (s_gameRunning)
		{
			errorOut = "A game is already running.";
			return false;
		}
		if (!g_vulkan_available)
		{
			errorOut = "Vulkan is not available on this device.";
			return false;
		}
		if (!window.display || !window.surface)
		{
			errorOut = "Could not get the native window of the render surface.";
			return false;
		}
		TitleId baseTitleId;
		if (!CafeTitleList::FindBaseTitleId(titleId, baseTitleId))
		{
			errorOut = "Unable to launch game because the base files were not found.";
			return false;
		}
		const CafeSystem::PREPARE_STATUS_CODE r = CafeSystem::PrepareForegroundTitle(baseTitleId);
		if (r == CafeSystem::PREPARE_STATUS_CODE::UNABLE_TO_MOUNT)
		{
			errorOut = "Unable to mount title. Make sure the game folders are still valid and refresh the game list.";
			return false;
		}
		else if (r != CafeSystem::PREPARE_STATUS_CODE::SUCCESS)
		{
			errorOut = "Failed to launch game.";
			return false;
		}
		if (ActiveSettings::GetGraphicsAPI() != kVulkan)
		{
			errorOut = "The game profile of this title selects OpenGL, which the mobile interface does not support.";
			return false;
		}

		auto& info = WindowSystem::GetWindowInfo();
		WindowSystem::WindowHandleInfo handle;
		handle.backend = window.isWayland ? WindowSystem::WindowHandleInfo::Backend::Wayland : WindowSystem::WindowHandleInfo::Backend::X11;
		handle.display = window.display;
		handle.surface = window.surface;
		info.window_main = handle;
		info.canvas_main = handle;
		info.pad_open = false;
		info.is_fullscreen = true;
		SetWindowSize(window.width, window.height, window.physWidth, window.physHeight, window.scale);

		try
		{
			g_renderer = std::make_unique<VulkanRenderer>();
			VulkanRenderer::GetInstance()->InitializeSurface({window.physWidth, window.physHeight}, true);
		}
		catch (const std::exception& ex)
		{
			cemuLog_log(LogType::Force, "MobileUI: Error when initializing Vulkan renderer: {}", ex.what());
			errorOut = fmt::format("Error when initializing Vulkan renderer:\n{}", ex.what());
			return false;
		}

		s_runningGameName = CafeSystem::GetForegroundTitleName();
		s_gameRunning = true;
		CafeSystem::LaunchForegroundTitle();
		return true;
#endif
	}

	bool IsGameRunning()
	{
		return s_gameRunning;
	}

	std::string GetRunningGameName()
	{
		return s_runningGameName;
	}

	void Shutdown()
	{
		static bool s_shutdownDone = false;
		if (s_shutdownDone)
			return;
		s_shutdownDone = true;
		{
			std::lock_guard lock(s_callbackMutex);
			s_gameListChangedCb = nullptr;
			s_gameExitedCb = nullptr;
		}
		CafeSystem::Shutdown();
		InputManager::instance().Shutdown();
		s_gameRunning = false;
	}

	void FlushProfileData()
	{
		// PGO instrumented builds only: execv()/_exit() skip the atexit handler that writes the profile
		if (&__llvm_profile_write_file)
			__llvm_profile_write_file();
	}

	void RestartProcess()
	{
		FlushProfileData();
		char exePath[4096];
		const ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
		if (len <= 0)
			return;
		exePath[len] = '\0';
		char argMobile[] = "--mobile-ui";
		char* const argv[] = {exePath, argMobile, nullptr};
		fflush(stdout);
		fflush(stderr);
		execv(exePath, argv);
		cemuLog_log(LogType::Force, "MobileUI: failed to restart process");
	}

	void SetAppActive(bool active)
	{
		auto& info = WindowSystem::GetWindowInfo();
		info.app_active = active;
		if (!active)
			info.set_keystatesup();
	}

	void SetKeyState(uint32_t platformKeyCode, bool down)
	{
		WindowSystem::GetWindowInfo().set_keystate(platformKeyCode, down);
	}

	void ReleaseAllKeys()
	{
		WindowSystem::GetWindowInfo().set_keystatesup();
	}

	void SetMouse(int physX, int physY, bool leftDown, bool rightDown)
	{
		auto& instance = InputManager::instance();
		std::scoped_lock lock(instance.m_main_mouse.m_mutex);
		instance.m_main_mouse.position = {physX, physY};
		if (leftDown && !instance.m_main_mouse.left_down)
			instance.m_main_mouse.left_down_toggle = true;
		if (rightDown && !instance.m_main_mouse.right_down)
			instance.m_main_mouse.right_down_toggle = true;
		instance.m_main_mouse.left_down = leftDown;
		instance.m_main_mouse.right_down = rightDown;
	}

	void SetTouch(int physX, int physY, bool down)
	{
		auto& instance = InputManager::instance();
		std::scoped_lock lock(instance.m_main_touch.m_mutex);
		instance.m_main_touch.position = {physX, physY};
		instance.m_main_touch.left_down = down;
		if (down)
			instance.m_main_touch.left_down_toggle = true;
	}

	std::string GetVersionString()
	{
		return BUILD_VERSION_WITH_NAME_STRING;
	}
	// ---------------------------------------------------------------- settings

	namespace
	{
		struct SettingAccessor
		{
			std::function<int(CemuConfig&)> get;
			std::function<void(CemuConfig&, int)> set;
		};

		const std::unordered_map<std::string, SettingAccessor>& SettingTable()
		{
			// clang-format off
			static const std::unordered_map<std::string, SettingAccessor> table = {
				// general
				{"console_language",    {[](CemuConfig& c) { return (int)c.console_language.GetValue(); },
				                         [](CemuConfig& c, int v) { c.console_language = (CafeConsoleLanguage)std::clamp(v, (int)CafeConsoleLanguage::JA, (int)CafeConsoleLanguage::TW); }}},
				{"play_boot_sound",     {[](CemuConfig& c) { return (int)c.play_boot_sound.GetValue(); },
				                         [](CemuConfig& c, int v) { c.play_boot_sound = v != 0; }}},
				{"disable_screensaver", {[](CemuConfig& c) { return (int)c.disable_screensaver.GetValue(); },
				                         [](CemuConfig& c, int v) { c.disable_screensaver = v != 0; }}},
				// graphics (the mobile interface always uses Vulkan)
				{"vsync",               {[](CemuConfig& c) { return (int)c.vsync.GetValue(); },
				                         [](CemuConfig& c, int v) { c.vsync = std::clamp(v, 0, 2); }}},
				{"async_compile",       {[](CemuConfig& c) { return (int)c.async_compile.GetValue(); },
				                         [](CemuConfig& c, int v) { c.async_compile = v != 0; }}},
				{"upscale_filter",      {[](CemuConfig& c) { return (int)c.upscale_filter.GetValue(); },
				                         [](CemuConfig& c, int v) { c.upscale_filter = std::clamp(v, 0, 3); }}},
				{"downscale_filter",    {[](CemuConfig& c) { return (int)c.downscale_filter.GetValue(); },
				                         [](CemuConfig& c, int v) { c.downscale_filter = std::clamp(v, 0, 3); }}},
				{"fullscreen_scaling",  {[](CemuConfig& c) { return (int)c.fullscreen_scaling.GetValue(); },
				                         [](CemuConfig& c, int v) { c.fullscreen_scaling = std::clamp(v, 0, 1); }}},
				// audio
				{"audio_delay",         {[](CemuConfig& c) { return (int)c.audio_delay; },
				                         [](CemuConfig& c, int v) { c.audio_delay = std::clamp(v, 0, (int)IAudioAPI::kBlockCount - 1); }}},
				{"tv_channels",         {[](CemuConfig& c) { return (int)c.tv_channels; },
				                         [](CemuConfig& c, int v) { c.tv_channels = (AudioChannels)std::clamp(v, 0, 2); }}},
				{"tv_volume",           {[](CemuConfig& c) { return (int)c.tv_volume; },
				                         [](CemuConfig& c, int v) { c.tv_volume = std::clamp(v, 0, 100); }}},
				// overlay
				{"overlay_position",    {[](CemuConfig& c) { return (int)c.overlay.position; },
				                         [](CemuConfig& c, int v) { c.overlay.position = (ScreenPosition)std::clamp(v, 0, 6); }}},
				{"overlay_scale",       {[](CemuConfig& c) { return (int)c.overlay.text_scale; },
				                         [](CemuConfig& c, int v) { c.overlay.text_scale = std::clamp(v, 50, 300); }}},
				{"overlay_fps",         {[](CemuConfig& c) { return (int)c.overlay.fps; },
				                         [](CemuConfig& c, int v) { c.overlay.fps = v != 0; }}},
				{"overlay_cpu",         {[](CemuConfig& c) { return (int)c.overlay.cpu_usage; },
				                         [](CemuConfig& c, int v) { c.overlay.cpu_usage = v != 0; }}},
				{"overlay_ram",         {[](CemuConfig& c) { return (int)c.overlay.ram_usage; },
				                         [](CemuConfig& c, int v) { c.overlay.ram_usage = v != 0; }}},
				{"overlay_vram",        {[](CemuConfig& c) { return (int)c.overlay.vram_usage; },
				                         [](CemuConfig& c, int v) { c.overlay.vram_usage = v != 0; }}},
				{"notification_position", {[](CemuConfig& c) { return (int)c.notification.position; },
				                         [](CemuConfig& c, int v) { c.notification.position = (ScreenPosition)std::clamp(v, 0, 6); }}},
				{"notification_scale",  {[](CemuConfig& c) { return (int)c.notification.text_scale; },
				                         [](CemuConfig& c, int v) { c.notification.text_scale = std::clamp(v, 50, 300); }}},
				{"notification_shaders", {[](CemuConfig& c) { return (int)c.notification.shader_compiling; },
				                         [](CemuConfig& c, int v) { c.notification.shader_compiling = v != 0; }}},
			};
			// clang-format on
			return table;
		}
	} // namespace

	int GetSetting(const std::string& key)
	{
		const auto& table = SettingTable();
		auto it = table.find(key);
		if (it == table.end())
		{
			cemuLog_log(LogType::Force, "Mobile UI: unknown setting {}", key);
			return 0;
		}
		return it->second.get(GetConfig());
	}

	void SetSetting(const std::string& key, int value)
	{
		const auto& table = SettingTable();
		auto it = table.find(key);
		if (it == table.end())
		{
			cemuLog_log(LogType::Force, "Mobile UI: unknown setting {}", key);
			return;
		}
		it->second.set(GetConfig(), value);
		GetConfigHandle().Save();
	}

	std::vector<AudioDevice> GetAudioDevices()
	{
		std::vector<AudioDevice> result;
		auto api = (IAudioAPI::AudioAPI)GetConfig().audio_api;
		if (!IAudioAPI::IsAudioAPIAvailable(api))
		{
			// same fallback as IAudioAPI::CreateDeviceFromConfig
			for (sint32 i = 0; i < (sint32)IAudioAPI::AudioAPIEnd; i++)
			{
				if (IAudioAPI::IsAudioAPIAvailable((IAudioAPI::AudioAPI)i))
				{
					api = (IAudioAPI::AudioAPI)i;
					break;
				}
			}
		}
		if (!IAudioAPI::IsAudioAPIAvailable(api))
			return result;
		for (const auto& device : IAudioAPI::GetDevices(api))
			result.push_back({boost::nowide::narrow(device->GetIdentifier()), boost::nowide::narrow(device->GetName())});
		return result;
	}

	std::string GetTVAudioDevice()
	{
		return boost::nowide::narrow(GetConfig().tv_device);
	}

	void SetTVAudioDevice(const std::string& id)
	{
		GetConfig().tv_device = boost::nowide::widen(id);
		GetConfigHandle().Save();
	}

	// ---------------------------------------------------------------- per game actions

	namespace
	{
		// same list as the desktop game list ("Remove shader caches")
		std::vector<fs::path> GetExistingShaderCaches(uint64 titleId)
		{
			titleId = TitleIdParser::MakeBaseTitleId(titleId);
			std::vector<fs::path> paths{
				ActiveSettings::GetCachePath(L"shaderCache/driver/vk/{:016x}.bin", titleId),
				ActiveSettings::GetCachePath(L"shaderCache/precompiled/{:016x}_spirv.bin", titleId),
				ActiveSettings::GetCachePath(L"shaderCache/precompiled/{:016x}_gl.bin", titleId),
				ActiveSettings::GetCachePath(L"shaderCache/precompiled/{:016x}_air.bin", titleId),
				ActiveSettings::GetCachePath(L"shaderCache/transferable/{:016x}_shaders.bin", titleId),
				ActiveSettings::GetCachePath(L"shaderCache/transferable/{:016x}_mtlshaders.bin", titleId),
				ActiveSettings::GetCachePath(L"shaderCache/transferable/{:016x}_vkpipeline.bin", titleId),
				ActiveSettings::GetCachePath(L"shaderCache/transferable/{:016x}_mtlpipeline.bin", titleId)};
			std::erase_if(paths, [](const fs::path& p) {
				std::error_code ec;
				return !fs::exists(p, ec);
			});
			return paths;
		}
	} // namespace

	int CountShaderCaches(uint64_t titleId)
	{
		return (int)GetExistingShaderCaches(titleId).size();
	}

	int RemoveShaderCaches(uint64_t titleId)
	{
		int removed = 0;
		for (const auto& path : GetExistingShaderCaches(titleId))
		{
			std::error_code ec;
			if (fs::remove(path, ec))
				removed++;
			else
				cemuLog_log(LogType::Force, "Mobile UI: failed to remove {}: {}", _pathToUtf8(path), ec.message());
		}
		return removed;
	}

	namespace
	{
		std::shared_ptr<GraphicPack2> GraphicPackByIndex(int index)
		{
			const auto& packs = GraphicPack2::GetGraphicPacks();
			if (index < 0 || index >= (int)packs.size())
				return {};
			return packs[index];
		}

		// same format as GraphicPacksWindow2::SaveStateToConfig
		void SaveGraphicPackState()
		{
			auto& data = GetConfig();
			data.graphic_pack_entries.clear();
			for (const auto& gp : GraphicPack2::GetGraphicPacks())
			{
				auto filename = _utf8ToPath(gp->GetNormalizedPathString());
				if (gp->IsEnabled())
				{
					auto& entry = data.graphic_pack_entries[filename];
					for (const auto& preset : gp->GetActivePresets())
						entry.try_emplace(preset->category, preset->name);
				}
				else if (gp->IsDefaultEnabled())
				{
					data.graphic_pack_entries[filename].try_emplace("_disabled", "false");
				}
			}
			GetConfigHandle().Save();
		}
	} // namespace

	std::vector<GraphicPackInfo> GetGraphicPacks(uint64_t titleId)
	{
		std::vector<GraphicPackInfo> result;
		const auto& packs = GraphicPack2::GetGraphicPacks();
		for (int i = 0; i < (int)packs.size(); i++)
		{
			const auto& gp = packs[i];
			if (gp->IsUniversal() || !gp->ContainsTitleId(titleId))
				continue;
			GraphicPackInfo info;
			info.index = i;
			info.name = gp->GetName();
			info.path = gp->GetVirtualPath();
			info.description = gp->GetDescription();
			info.enabled = gp->IsEnabled();

			std::vector<std::string> order;
			auto categorized = gp->GetCategorizedPresets(order);
			for (const auto& category : order)
			{
				GraphicPackPresetGroup group;
				group.category = category;
				for (const auto& preset : categorized[category])
				{
					if (!preset->visible)
						continue;
					group.presets.push_back(preset->name);
					if (preset->active)
						group.active = preset->name;
				}
				if (group.presets.empty())
					continue;
				if (group.active.empty())
					group.active = group.presets.front();
				info.presetGroups.push_back(std::move(group));
			}
			result.push_back(std::move(info));
		}
		std::sort(result.begin(), result.end(), [](const GraphicPackInfo& a, const GraphicPackInfo& b) { return a.path < b.path; });
		return result;
	}

	void SetGraphicPackEnabled(int index, bool enabled)
	{
		auto gp = GraphicPackByIndex(index);
		if (!gp)
			return;
		gp->SetEnabled(enabled);
		SaveGraphicPackState();
	}

	void SetGraphicPackPreset(int index, const std::string& category, const std::string& preset)
	{
		auto gp = GraphicPackByIndex(index);
		if (!gp)
			return;
		gp->SetActivePreset(category, preset);
		SaveGraphicPackState();
	}
} // namespace MobileBackend
