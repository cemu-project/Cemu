#pragma once

#include <jni.h>

#include "AndroidAudio.h"
#include "AndroidEmulatedController.h"
#include "AndroidFilesystemCallbacks.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "CafeSystemUtils.h"
#include "Cafe/CafeSystem.h"
#include "Cemu/GuiSystem/GuiSystem.h"
#include "GameIconLoader.h"
#include "GameTitleLoader.h"
#include "Utils.h"
#include "input/ControllerFactory.h"
#include "input/InputManager.h"
#include "input/api/Android/AndroidController.h"
#include "input/api/Android/AndroidControllerProvider.h"

int mainEmulatorHLE();

class EmulationState {
	GameIconLoader m_gameIconLoader;
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

  public:
	void initializeEmulation()
	{
		g_config.Load();
		FilesystemAndroid::setFilesystemCallbacks(std::make_shared<AndroidFilesystemCallbacks>());
		NetworkConfig::LoadOnce();
		InputManager::instance().load();
		InitializeGlobalVulkan();
		createCemuDirectories();
		mainEmulatorHLE();
		fillGraphicPacks();
	}

	void initializeActiveSettings(const fs::path& dataPath, const fs::path& cachePath)
	{
		ActiveSettings::LoadOnce({}, dataPath, dataPath, cachePath, dataPath);
	}

	void clearSurface(bool isMainCanvas)
	{
	}

	void notifySurfaceChanged(bool isMainCanvas)
	{
	}

	void setSurface(JNIEnv* env, jobject surface, bool isMainCanvas)
	{
		auto& windowHandleInfo = isMainCanvas ? GuiSystem::getWindowInfo().canvas_main : GuiSystem::getWindowInfo().canvas_pad;
		if (windowHandleInfo.surface)
		{
			ANativeWindow_release(static_cast<ANativeWindow*>(windowHandleInfo.surface));
			windowHandleInfo.surface = nullptr;
		}
		if (surface)
			windowHandleInfo.surface = ANativeWindow_fromSurface(env, surface);
		if (isMainCanvas)
			GuiSystem::getWindowInfo().window_main.surface = windowHandleInfo.surface;
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
	void initializeRenderer()
	{
		g_renderer = std::make_unique<VulkanRenderer>();
	}
	void initializeRenderSurface(bool isMainCanvas)
	{
		int width, height;
		if (isMainCanvas)
			GuiSystem::getWindowPhysSize(width, height);
		else
			GuiSystem::getPadWindowPhysSize(width, height);
		VulkanRenderer::GetInstance()->InitializeSurface({width, height}, isMainCanvas);
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

	const Image& getGameIcon(TitleId titleId)
	{
		return m_gameIconLoader.getGameIcon(titleId);
	}

	void setOnGameIconLoaded(const std::shared_ptr<class GameIconLoadedCallback>& onGameIconLoaded)
	{
		m_gameIconLoader.setOnIconLoaded(onGameIconLoaded);
	}

	void addGamePath(const fs::path& gamePath)
	{
		m_gameTitleLoader.addGamePath(gamePath);
	}

	void requestGameIcon(TitleId titleId)
	{
		m_gameIconLoader.requestIcon(titleId);
	}

	void reloadGameTitles()
	{
		m_gameTitleLoader.reloadGameTitles();
	}

	void startGame(TitleId titleId)
	{
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
};