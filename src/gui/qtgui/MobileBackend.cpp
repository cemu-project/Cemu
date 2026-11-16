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

#ifdef ENABLE_VULKAN
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#endif

#include <unistd.h>

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

	void RestartProcess()
	{
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
} // namespace MobileBackend
