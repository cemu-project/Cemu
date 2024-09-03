#include "Cafe/GraphicPack/GraphicPack2.h"
#include "AndroidGameTitleLoadedCallback.h"
#include "EmulationState.h"
#include "JNIUtils.h"

EmulationState s_emulationState;

static_assert(sizeof(TitleId) == sizeof(jlong));

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, [[maybe_unused]] void* reserved)
{
	JNIUtils::g_jvm = vm;
	return JNI_VERSION_1_6;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setSurface(JNIEnv* env, [[maybe_unused]] jclass clazz, jobject surface, jboolean is_main_canvas)
{
	s_emulationState.setSurface(env, surface, is_main_canvas);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setSurfaceSize([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint width, jint height, jboolean is_main_canvas)
{
	s_emulationState.setSurfaceSize(width, height, is_main_canvas);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_startGame([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jlong title_id)
{
	s_emulationState.startGame(static_cast<TitleId>(title_id));
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setGameTitleLoadedCallback(JNIEnv* env, [[maybe_unused]] jclass clazz, jobject game_title_loaded_callback)
{
	if (game_title_loaded_callback == nullptr)
	{
		s_emulationState.setOnGameTitleLoaded(nullptr);
		return;
	}
	jclass gameTitleLoadedCallbackClass = env->GetObjectClass(game_title_loaded_callback);
	jmethodID onGameTitleLoadedMID = env->GetMethodID(gameTitleLoadedCallbackClass, "onGameTitleLoaded", "(JLjava/lang/String;[III)V");
	env->DeleteLocalRef(gameTitleLoadedCallbackClass);
	s_emulationState.setOnGameTitleLoaded(std::make_shared<AndroidGameTitleLoadedCallback>(onGameTitleLoadedMID, game_title_loaded_callback));
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_reloadGameTitles([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	s_emulationState.reloadGameTitles();
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializeActiveSettings(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring data_path, jstring cache_path)
{
	std::string dataPath = JNIUtils::JStringToString(env, data_path);
	std::string cachePath = JNIUtils::JStringToString(env, cache_path);
	s_emulationState.initializeActiveSettings(dataPath, cachePath);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializeEmulation([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	s_emulationState.initializeEmulation();
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializerRenderer(JNIEnv* env, [[maybe_unused]] jclass clazz, jobject testSurface)
{
	s_emulationState.initializeRenderer(env, testSurface);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setDPI([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jfloat dpi)
{
	s_emulationState.setDPI(dpi);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_clearSurface([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean is_main_canvas)
{
	s_emulationState.clearSurface(is_main_canvas);
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_recreateRenderSurface([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean is_main_canvas)
{
	s_emulationState.notifySurfaceChanged(is_main_canvas);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_addGamesPath(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring uri)
{
	s_emulationState.addGamesPath(JNIUtils::JStringToString(env, uri));
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_removeGamesPath(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring uri)
{
	s_emulationState.removeGamesPath(JNIUtils::JStringToString(env, uri));
}

extern "C" [[maybe_unused]] JNIEXPORT jobject JNICALL
Java_info_cemu_Cemu_NativeLibrary_getGamesPaths(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return JNIUtils::createJavaStringArrayList(env, g_config.data().game_paths);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onNativeKey(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring device_descriptor, jstring device_name, jint key, jboolean is_pressed)
{
	auto deviceDescriptor = JNIUtils::JStringToString(env, device_descriptor);
	auto deviceName = JNIUtils::JStringToString(env, device_name);
	s_emulationState.onKeyEvent(deviceDescriptor, deviceName, key, is_pressed);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onNativeAxis(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring device_descriptor, jstring device_name, jint axis, jfloat value)
{
	auto deviceDescriptor = JNIUtils::JStringToString(env, device_descriptor);
	auto deviceName = JNIUtils::JStringToString(env, device_name);
	s_emulationState.onAxisEvent(deviceDescriptor, deviceName, axis, value);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setControllerType([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint index, jint emulated_controller_type)
{
	s_emulationState.setEmulatedControllerType(index, static_cast<EmulatedController::Type>(emulated_controller_type));
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getControllerType([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint index)
{
	return s_emulationState.getEmulatedControllerType(index);
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getWPADControllersCount([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return s_emulationState.getWPADControllersCount();
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getVPADControllersCount([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return s_emulationState.getVPADControllersCount();
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isControllerDisabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint index)
{
	return s_emulationState.isEmulatedControllerDisabled(index);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setControllerMapping(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring device_descriptor, jstring device_name, jint index, jint mapping_id, jint button_id)
{
	auto deviceName = JNIUtils::JStringToString(env, device_name);
	auto deviceDescriptor = JNIUtils::JStringToString(env, device_descriptor);
	s_emulationState.setControllerMapping(deviceDescriptor, deviceName, index, mapping_id, button_id);
}

extern "C" [[maybe_unused]] JNIEXPORT jstring JNICALL
Java_info_cemu_Cemu_NativeLibrary_getControllerMapping(JNIEnv* env, [[maybe_unused]] jclass clazz, jint index, jint mapping_id)
{
	auto mapping = s_emulationState.getEmulatedControllerMapping(index, mapping_id);
	return env->NewStringUTF(mapping.value_or("").c_str());
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_clearControllerMapping([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint index, jint mapping_id)
{
	s_emulationState.clearEmulatedControllerMapping(index, mapping_id);
}

extern "C" [[maybe_unused]] JNIEXPORT jobject JNICALL
Java_info_cemu_Cemu_NativeLibrary_getControllerMappings(JNIEnv* env, [[maybe_unused]] jclass clazz, jint index)
{
	jclass hashMapClass = env->FindClass("java/util/HashMap");
	jmethodID hashMapConstructor = env->GetMethodID(hashMapClass, "<init>", "()V");
	jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
	jclass integerClass = env->FindClass("java/lang/Integer");
	jmethodID integerConstructor = env->GetMethodID(integerClass, "<init>", "(I)V");
	jobject hashMapObj = env->NewObject(hashMapClass, hashMapConstructor);
	auto mappings = s_emulationState.getEmulatedControllerMappings(index);
	for (const auto& pair : mappings)
	{
		jint key = pair.first;
		jstring buttonName = env->NewStringUTF(pair.second.c_str());
		jobject mappingId = env->NewObject(integerClass, integerConstructor, key);
		env->CallObjectMethod(hashMapObj, hashMapPut, mappingId, buttonName);
	}
	return hashMapObj;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onKeyEvent(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring device_descriptor, jstring device_name, jint key_code, jboolean is_pressed)
{
	auto deviceDescriptor = JNIUtils::JStringToString(env, device_descriptor);
	auto deviceName = JNIUtils::JStringToString(env, device_name);
	s_emulationState.onKeyEvent(deviceDescriptor, deviceName, key_code, is_pressed);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onAxisEvent([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jstring device_descriptor, jstring device_name, jint axis_code, jfloat value)
{
	auto deviceDescriptor = JNIUtils::JStringToString(env, device_descriptor);
	auto deviceName = JNIUtils::JStringToString(env, device_name);
	s_emulationState.onAxisEvent(deviceDescriptor, deviceName, axis_code, value);
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_getAsyncShaderCompile([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().async_compile;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setAsyncShaderCompile([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().async_compile = enabled;
	g_config.Save();
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getVSyncMode([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().vsync;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setVSyncMode([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint vsync_mode)
{
	g_config.data().vsync = vsync_mode;
	g_config.Save();
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_getAccurateBarriers([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().vk_accurate_barriers;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setAccurateBarriers([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().vk_accurate_barriers = enabled;
	g_config.Save();
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_getAudioDeviceEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean tv)
{
	const auto& device = tv ? g_config.data().tv_device : g_config.data().pad_device;
	return !device.empty();
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setAudioDeviceEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled, jboolean tv)
{
	auto& device = tv ? g_config.data().tv_device : g_config.data().pad_device;
	if (enabled)
		device = L"Default";
	else
		device.clear();
	g_config.Save();
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getAudioDeviceChannels([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean tv)
{
	const auto& deviceChannels = tv ? g_config.data().tv_channels : g_config.data().pad_channels;
	return deviceChannels;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setAudioDeviceChannels([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint channels, jboolean tv)
{
	auto& deviceChannels = tv ? g_config.data().tv_channels : g_config.data().pad_channels;
	deviceChannels = static_cast<AudioChannels>(channels);
	g_config.Save();
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getAudioDeviceVolume([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean tv)
{
	const auto& deviceVolume = tv ? g_config.data().tv_volume : g_config.data().pad_volume;
	return deviceVolume;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setAudioDeviceVolume([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint volume, jboolean tv)
{
	auto& deviceVolume = tv ? g_config.data().tv_volume : g_config.data().pad_volume;
	deviceVolume = volume;
	g_config.Save();
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_refreshGraphicPacks([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	s_emulationState.refreshGraphicPacks();
}

extern "C" JNIEXPORT jobject JNICALL
Java_info_cemu_Cemu_NativeLibrary_getGraphicPackBasicInfos(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	auto graphicPackInfoClass = env->FindClass("info/cemu/Cemu/NativeLibrary$GraphicPackBasicInfo");
	auto graphicPackInfoCtorId = env->GetMethodID(graphicPackInfoClass, "<init>", "(JLjava/lang/String;Ljava/util/ArrayList;)V");

	auto graphicPacks = s_emulationState.getGraphicPacks();
	std::vector<jobject> graphicPackInfoJObjects;
	for (auto&& graphicPack : graphicPacks)
	{
		jstring virtualPath = env->NewStringUTF(graphicPack.second->GetVirtualPath().c_str());
		jlong id = graphicPack.first;
		jobject titleIds = JNIUtils::createJavaLongArrayList(env, graphicPack.second->GetTitleIds());
		jobject jGraphicPack = env->NewObject(graphicPackInfoClass, graphicPackInfoCtorId, id, virtualPath, titleIds);
		graphicPackInfoJObjects.push_back(jGraphicPack);
	}
	return JNIUtils::createArrayList(env, graphicPackInfoJObjects);
}

jobject getGraphicPresets(JNIEnv* env, GraphicPackPtr graphicPack, int64_t id)
{
	auto graphicPackPresetClass = env->FindClass("info/cemu/Cemu/NativeLibrary$GraphicPackPreset");
	auto graphicPackPresetCtorId = env->GetMethodID(graphicPackPresetClass, "<init>", "(JLjava/lang/String;Ljava/util/ArrayList;Ljava/lang/String;)V");

	std::vector<std::string> order;
	auto presets = graphicPack->GetCategorizedPresets(order);

	std::vector<jobject> presetsJobjects;
	for (const auto& category : order)
	{
		const auto& entry = presets[category];
		// test if any preset is visible and update its status
		if (std::none_of(entry.cbegin(), entry.cend(), [graphicPack](const auto& p) { return p->visible; }))
		{
			continue;
		}

		jstring categoryJStr = category.empty() ? nullptr : env->NewStringUTF(category.c_str());
		std::vector<std::string> presetSelections;
		std::optional<std::string> activePreset;
		for (auto& pentry : entry)
		{
			if (!pentry->visible)
				continue;

			presetSelections.push_back(pentry->name);

			if (pentry->active)
				activePreset = pentry->name;
		}

		jstring activePresetJstr = nullptr;
		if (activePreset)
			activePresetJstr = env->NewStringUTF(activePreset->c_str());
		else if (!presetSelections.empty())
			activePresetJstr = env->NewStringUTF(presetSelections.front().c_str());
		auto presetJObject = env->NewObject(graphicPackPresetClass,
											graphicPackPresetCtorId,
											id,
											categoryJStr,
											JNIUtils::createJavaStringArrayList(env, presetSelections),
											activePresetJstr);
		presetsJobjects.push_back(presetJObject);
	}
	return JNIUtils::createArrayList(env, presetsJobjects);
}
extern "C" JNIEXPORT jobject JNICALL
Java_info_cemu_Cemu_NativeLibrary_getGraphicPack(JNIEnv* env, [[maybe_unused]] jclass clazz, jlong id)
{
	auto graphicPackClass = env->FindClass("info/cemu/Cemu/NativeLibrary$GraphicPack");
	auto graphicPackCtorId = env->GetMethodID(graphicPackClass, "<init>", "(JZLjava/lang/String;Ljava/lang/String;Ljava/util/ArrayList;)V");
	auto graphicPack = s_emulationState.getGraphicPack(id);

	jstring graphicPackName = env->NewStringUTF(graphicPack->GetName().c_str());
	jstring graphicPackDescription = env->NewStringUTF(graphicPack->GetDescription().c_str());
	return env->NewObject(graphicPackClass,
						  graphicPackCtorId,
						  id,
						  graphicPack->IsEnabled(),
						  graphicPackName,
						  graphicPackDescription,
						  getGraphicPresets(env, graphicPack, id));
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setGraphicPackActive([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jlong id, jboolean active)
{
	s_emulationState.setEnabledStateForGraphicPack(id, active);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setGraphicPackActivePreset([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jlong id, jstring category, jstring preset)
{
	std::string presetCategory = category == nullptr ? "" : JNIUtils::JStringToString(env, category);
	s_emulationState.setGraphicPackActivePreset(id, presetCategory, JNIUtils::JStringToString(env, preset));
}

extern "C" JNIEXPORT jobject JNICALL
Java_info_cemu_Cemu_NativeLibrary_getGraphicPackPresets(JNIEnv* env, [[maybe_unused]] jclass clazz, jlong id)
{
	return getGraphicPresets(env, s_emulationState.getGraphicPack(id), id);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onOverlayButton([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint controllerIndex, jint mappingId, jboolean state)
{
	s_emulationState.getEmulatedController(controllerIndex).setButtonValue(mappingId, state);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onOverlayAxis([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint controllerIndex, jint mappingId, jfloat value)
{
	s_emulationState.getEmulatedController(controllerIndex).setAxisValue(mappingId, value);
}

extern "C" JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getOverlayPosition([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return static_cast<jint>(g_config.data().overlay.position);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setOverlayPosition([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint position)
{
	g_config.data().overlay.position = static_cast<ScreenPosition>(position);
	g_config.Save();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isOverlayFPSEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().overlay.fps;
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setOverlayFPSEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().overlay.fps = enabled;
	g_config.Save();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isOverlayDrawCallsPerFrameEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().overlay.drawcalls;
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setOverlayDrawCallsPerFrameEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().overlay.drawcalls = enabled;
	g_config.Save();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isOverlayCPUUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().overlay.cpu_usage;
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setOverlayCPUUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().overlay.cpu_usage = enabled;
	g_config.Save();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isOverlayCPUPerCoreUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().overlay.cpu_per_core_usage;
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setOverlayCPUPerCoreUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().overlay.cpu_per_core_usage = enabled;
	g_config.Save();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isOverlayRAMUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().overlay.ram_usage;
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setOverlayRAMUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().overlay.ram_usage = enabled;
	g_config.Save();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isOverlayVRAMUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().overlay.vram_usage;
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setOverlayVRAMUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().overlay.vram_usage = enabled;
	g_config.Save();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isOverlayDebugEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().overlay.debug;
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setOverlayDebugEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().overlay.debug = enabled;
	g_config.Save();
}

extern "C" JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getNotificationsPosition([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return static_cast<jint>(g_config.data().notification.position);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setNotificationsPosition([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint position)
{
	g_config.data().notification.position = static_cast<ScreenPosition>(position);
	g_config.Save();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isNotificationControllerProfilesEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().notification.controller_profiles;
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setNotificationControllerProfilesEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().notification.controller_profiles = enabled;
	g_config.Save();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isNotificationShaderCompilerEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().notification.shader_compiling;
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setNotificationShaderCompilerEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().notification.shader_compiling = enabled;
	g_config.Save();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isNotificationFriendListEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().notification.friends;
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setNotificationFriendListEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().notification.friends = enabled;
	g_config.Save();
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setReplaceTVWithPadView([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean swapped)
{
	s_emulationState.setReplaceTVWithPadView(swapped);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onTouchDown([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint x, jint y, jboolean isTV)
{
	s_emulationState.onTouchDown(x, y, isTV);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onTouchUp([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint x, jint y, jboolean isTV)
{
	s_emulationState.onTouchUp(x, y, isTV);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onTouchMove([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint x, jint y, jboolean isTV)
{
	s_emulationState.onTouchMove(x, y, isTV);
}

extern "C" JNIEXPORT jobject JNICALL
Java_info_cemu_Cemu_NativeLibrary_getInstalledGamesTitleIds(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return JNIUtils::createJavaLongArrayList(env, CafeTitleList::GetAllTitleIds());
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onMotion([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jlong timestamp, jfloat gyroX, jfloat gyroY, jfloat gyroZ, jfloat accelX, jfloat accelY, jfloat accelZ)
{
	s_emulationState.onMotion(timestamp, gyroX, gyroY, gyroZ, accelX, accelY, accelZ);
}

extern "C" JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setMotionEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean motionEnabled)
{
	s_emulationState.setMotionEnabled(motionEnabled);
}