#pragma once

#include <jni.h>

#include "AndroidAudio.h"
#include "AndroidEmulatedController.h"
#include "AndroidFilesystemCallbacks.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "CafeSystemUtils.h"
#include "Cafe/CafeSystem.h"
#include "Cemu/GuiSystem/GuiSystem.h"
#include "GameTitleLoader.h"
#include "Utils.h"
#include "input/ControllerFactory.h"
#include "input/InputManager.h"
#include "input/api/Android/AndroidController.h"
#include "input/api/Android/AndroidControllerProvider.h"

void CemuCommonInit();

class EmulationState
{
	GameTitleLoader m_gameTitleLoader;
	std::unordered_map<int64_t, GraphicPackPtr> m_graphicPacks;
	void fillGraphicPacks()
	{
		m_graphicPacks.clear();
		auto graphicPacks = GraphicPack2::GetGraphicPacks();
		for (auto&& graphicPack : graphicPacks)
		{
			m_graphicPacks[reinterpret_cast<int64_t>(graphicPack.get())] = graphicPack;
		}
	}
	void onTouchEvent(sint32 x, sint32 y, bool isTV, std::optional<bool> status = {})
	{
		auto& instance = InputManager::instance();
		auto& touchInfo = isTV ? instance.m_main_mouse : instance.m_pad_mouse;
		std::scoped_lock lock(touchInfo.m_mutex);
		touchInfo.position = {x, y};
		if (status.has_value())
			touchInfo.left_down = touchInfo.left_down_toggle = status.value();
	}
	WiiUMotionHandler m_wiiUMotionHandler{};
	long m_lastMotionTimestamp;

  public:
	void initializeEmulation()
	{
		g_config.SetFilename(ActiveSettings::GetConfigPath("settings.xml").generic_wstring());
		g_config.Load();
		FilesystemAndroid::setFilesystemCallbacks(std::make_shared<AndroidFilesystemCallbacks>());
		NetworkConfig::LoadOnce();
		InputManager::instance().load();
		auto& instance = InputManager::instance();
		InitializeGlobalVulkan();
		createCemuDirectories();
		LatteOverlay_init();
		CemuCommonInit();
		fillGraphicPacks();
	}

	void initializeActiveSettings(const fs::path& dataPath, const fs::path& cachePath)
	{
		std::set<fs::path> failedWriteAccess;
		ActiveSettings::SetPaths(false, {}, dataPath, dataPath, cachePath, dataPath, failedWriteAccess);
	}

	void clearSurface(bool isMainCanvas)
	{
		if (!isMainCanvas)
		{
			auto renderer = static_cast<VulkanRenderer*>(g_renderer.get());
			if (renderer)
				renderer->StopUsingPadAndWait();
		}
	}

	void notifySurfaceChanged(bool isMainCanvas)
	{
	}

	void setSurface(JNIEnv* env, jobject surface, bool isMainCanvas)
	{
		cemu_assert_debug(surface != nullptr);
		auto& windowHandleInfo = isMainCanvas ? GuiSystem::getWindowInfo().canvas_main : GuiSystem::getWindowInfo().canvas_pad;
		if (windowHandleInfo.surface)
		{
			ANativeWindow_release(static_cast<ANativeWindow*>(windowHandleInfo.surface));
			windowHandleInfo.surface = nullptr;
		}
		windowHandleInfo.surface = ANativeWindow_fromSurface(env, surface);
		int width, height;
		if (isMainCanvas)
			GuiSystem::getWindowPhysSize(width, height);
		else
			GuiSystem::getPadWindowPhysSize(width, height);
		VulkanRenderer::GetInstance()->InitializeSurface({width, height}, isMainCanvas);
	}

	void setSurfaceSize(int width, int height, bool isMainCanvas)
	{
		auto& windowInfo = GuiSystem::getWindowInfo();
		if (isMainCanvas)
		{
			windowInfo.width = windowInfo.phys_width = width;
			windowInfo.height = windowInfo.phys_height = height;
		}
		else
		{
			windowInfo.pad_width = windowInfo.phys_pad_width = width;
			windowInfo.pad_height = windowInfo.phys_pad_height = height;
		}
	}

	void onKeyEvent(const std::string& deviceDescriptor, const std::string& deviceName, int keyCode, bool isPressed)
	{
		auto apiProvider = InputManager::instance().get_api_provider(InputAPI::Android);
		auto androidControllerProvider = dynamic_cast<AndroidControllerProvider*>(apiProvider.get());
		androidControllerProvider->on_key_event(deviceDescriptor, deviceName, keyCode, isPressed);
	}

	void onAxisEvent(const std::string& deviceDescriptor, const std::string& deviceName, int axisCode, float value)
	{
		auto apiProvider = InputManager::instance().get_api_provider(InputAPI::Android);
		auto androidControllerProvider = dynamic_cast<AndroidControllerProvider*>(apiProvider.get());
		androidControllerProvider->on_axis_event(deviceDescriptor, deviceName, axisCode, value);
	}

	std::optional<std::string> getEmulatedControllerMapping(size_t index, uint64 mappingId)
	{
		return AndroidEmulatedController::getAndroidEmulatedController(index).getMapping(mappingId);
	}

	AndroidEmulatedController& getEmulatedController(size_t index)
	{
		return AndroidEmulatedController::getAndroidEmulatedController(index);
	}

	int getVPADControllersCount()
	{
		int vpadCount = 0;
		for (int i = 0; i < InputManager::kMaxController; i++)
		{
			auto emulatedController = AndroidEmulatedController::getAndroidEmulatedController(i).getEmulatedController();
			if (!emulatedController)
				continue;
			if (emulatedController->type() == EmulatedController::Type::VPAD)
				++vpadCount;
		}
		return vpadCount;
	}

	int getWPADControllersCount()
	{
		int wpadCount = 0;
		for (int i = 0; i < InputManager::kMaxController; i++)
		{
			auto emulatedController = AndroidEmulatedController::getAndroidEmulatedController(
										  i)
										  .getEmulatedController();
			if (!emulatedController)
				continue;
			if (emulatedController->type() != EmulatedController::Type::VPAD)
				++wpadCount;
		}
		return wpadCount;
	}

	EmulatedController::Type getEmulatedControllerType(size_t index)
	{
		auto emulatedController = AndroidEmulatedController::getAndroidEmulatedController(index).getEmulatedController();
		if (emulatedController)
			return emulatedController->type();
		throw std::runtime_error(fmt::format("can't get type for emulated controller {}", index));
	}

	void clearEmulatedControllerMapping(size_t index, uint64 mapping)
	{
		AndroidEmulatedController::getAndroidEmulatedController(index).clearMapping(mapping);
	}

	void setEmulatedControllerType(size_t index, EmulatedController::Type type)
	{
		auto& androidEmulatedController = AndroidEmulatedController::getAndroidEmulatedController(index);
		if (EmulatedController::Type::VPAD <= type && type < EmulatedController::Type::MAX)
			androidEmulatedController.setType(type);
		else
			androidEmulatedController.setDisabled();
	}

	void initializeRenderer(JNIEnv* env, jobject testSurface)
	{
		cemu_assert_debug(testSurface != nullptr);
		// TODO: cleanup surface
		GuiSystem::getWindowInfo().window_main.surface = ANativeWindow_fromSurface(env, testSurface);
		g_renderer = std::make_unique<VulkanRenderer>();
	}
	void setReplaceTVWithPadView(bool showDRC)
	{
		// Emulate pressing the TAB key for showing DRC instead of TV
		GuiSystem::getWindowInfo().set_keystate(GuiSystem::PlatformKeyCodes::TAB, showDRC);
	}
	void setDPI(float dpi)
	{
		auto& windowInfo = GuiSystem::getWindowInfo();
		windowInfo.dpi_scale = windowInfo.pad_dpi_scale = dpi;
	}

	bool isEmulatedControllerDisabled(size_t index)
	{
		return AndroidEmulatedController::getAndroidEmulatedController(index).getEmulatedController() == nullptr;
	}

	std::map<uint64, std::string> getEmulatedControllerMappings(size_t index)
	{
		return AndroidEmulatedController::getAndroidEmulatedController(index).getMappings();
	}

	void setControllerMapping(const std::string& deviceDescriptor, const std::string& deviceName, size_t index, uint64 mappingId, uint64 buttonId)
	{
		auto apiProvider = InputManager::instance().get_api_provider(InputAPI::Android);
		auto controller = ControllerFactory::CreateController(InputAPI::Android, deviceDescriptor, deviceName);
		AndroidEmulatedController::getAndroidEmulatedController(index).setMapping(mappingId, controller, buttonId);
	}
	void initializeAudioDevices()
	{
		auto& config = g_config.data();
		if (!config.tv_device.empty())
			AndroidAudio::createAudioDevice(IAudioAPI::AudioAPI::Cubeb, config.tv_channels, config.tv_volume, true);

		if (!config.pad_device.empty())
			AndroidAudio::createAudioDevice(IAudioAPI::AudioAPI::Cubeb, config.pad_channels, config.pad_volume, false);
	}

	void setOnGameTitleLoaded(const std::shared_ptr<GameTitleLoadedCallback>& onGameTitleLoaded)
	{
		m_gameTitleLoader.setOnTitleLoaded(onGameTitleLoaded);
	}

	void addGamesPath(const std::string& gamePath)
	{
		auto& gamePaths = g_config.data().game_paths;
		if (std::any_of(gamePaths.begin(), gamePaths.end(), [&](auto path) { return path == gamePath; }))
			return;
		gamePaths.push_back(gamePath);
		g_config.Save();
		CafeTitleList::ClearScanPaths();
		for (auto& it : gamePaths)
			CafeTitleList::AddScanPath(it);
		CafeTitleList::Refresh();
	}

	void removeGamesPath(const std::string& gamePath)
	{
		auto& gamePaths = g_config.data().game_paths;
		std::erase_if(gamePaths, [&](auto path) { return path == gamePath; });
		g_config.Save();
		CafeTitleList::ClearScanPaths();
		for (auto& it : gamePaths)
			CafeTitleList::AddScanPath(it);
		CafeTitleList::Refresh();
	}

	void reloadGameTitles()
	{
		m_gameTitleLoader.reloadGameTitles();
	}

	void startGame(TitleId titleId)
	{
		GuiSystem::getWindowInfo().set_keystates_up();
		initializeAudioDevices();
		CafeSystemUtils::startGame(titleId);
	}

	void refreshGraphicPacks()
	{
		if (!CafeSystem::IsTitleRunning())
		{
			GraphicPack2::ClearGraphicPacks();
			GraphicPack2::LoadAll();
			fillGraphicPacks();
		}
	}

	const std::unordered_map<int64_t, GraphicPackPtr>& getGraphicPacks() const
	{
		return m_graphicPacks;
	}

	void setEnabledStateForGraphicPack(int64_t id, bool state)
	{
		auto graphicPack = m_graphicPacks.at(id);
		graphicPack->SetEnabled(state);
		saveGraphicPackStateToConfig(graphicPack);
	}

	GraphicPackPtr getGraphicPack(int64_t id) const
	{
		return m_graphicPacks.at(id);
	}
	void setGraphicPackActivePreset(int64_t id, const std::string& presetCategory, const std::string& preset) const
	{
		auto graphicPack = m_graphicPacks.at(id);
		graphicPack->SetActivePreset(presetCategory, preset);
		saveGraphicPackStateToConfig(graphicPack);
	}
	void saveGraphicPackStateToConfig(GraphicPackPtr graphicPack) const
	{
		auto& data = g_config.data();
		auto filename = _utf8ToPath(graphicPack->GetNormalizedPathString());
		if (data.graphic_pack_entries.contains(filename))
			data.graphic_pack_entries.erase(filename);
		if (graphicPack->IsEnabled())
		{
			data.graphic_pack_entries.try_emplace(filename);
			auto& it = data.graphic_pack_entries[filename];
			// otherwise store all selected presets
			for (const auto& preset : graphicPack->GetActivePresets())
				it.try_emplace(preset->category, preset->name);
		}
		else if (graphicPack->IsDefaultEnabled())
		{
			// save that its disabled
			data.graphic_pack_entries.try_emplace(filename);
			auto& it = data.graphic_pack_entries[filename];
			it.try_emplace("_disabled", "false");
		}
		g_config.Save();
	}
	void onTouchMove(sint32 x, sint32 y, bool isTV)
	{
		onTouchEvent(x, y, isTV);
	}
	void onTouchUp(sint32 x, sint32 y, bool isTV)
	{
		onTouchEvent(x, y, isTV, false);
	}
	void onTouchDown(sint32 x, sint32 y, bool isTV)
	{
		onTouchEvent(x, y, isTV, true);
	}
	void onMotion(long timestamp, float gyroX, float gyroY, float gyroZ, float accelX, float accelY, float accelZ)
	{
		float deltaTime = (timestamp - m_lastMotionTimestamp) * 1e-9f;
		m_wiiUMotionHandler.processMotionSample(deltaTime, gyroX, gyroY, gyroZ, accelX * 0.098066f, -accelY * 0.098066f, -accelZ * 0.098066f);
		m_lastMotionTimestamp = timestamp;
		auto& deviceMotion = InputManager::instance().m_device_motion;
		std::scoped_lock lock{deviceMotion.m_mutex};
		deviceMotion.m_motion_sample = m_wiiUMotionHandler.getMotionSample();
	}
	void setMotionEnabled(bool enabled)
	{
		auto& deviceMotion = InputManager::instance().m_device_motion;
		std::scoped_lock lock{deviceMotion.m_mutex};
		deviceMotion.m_device_motion_enabled = enabled;
	}
};