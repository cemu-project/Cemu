#include "AndroidGameIconLoadedCallback.h"
#include "AndroidGameTitleLoadedCallback.h"
#include "EmulationState.h"
#include "JNIUtils.h"

EmulationState s_emulationState;

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, [[maybe_unused]] void *reserved)
{
	JNIUtils::g_jvm = vm;
	return JNI_VERSION_1_6;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setSurface(JNIEnv *env, [[maybe_unused]] jclass clazz, jobject surface, jboolean is_main_canvas)
{
	s_emulationState.setSurface(env, surface, is_main_canvas);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setSurfaceSize([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jint width, jint height, jboolean is_main_canvas)
{
	s_emulationState.setSurfaceSize(width, height, is_main_canvas);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_startGame([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jlong title_id)
{
	s_emulationState.startGame(*reinterpret_cast<TitleId *>(&title_id));
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setGameTitleLoadedCallback(JNIEnv *env, [[maybe_unused]] jclass clazz, jobject game_title_loaded_callback)
{
	jclass gameTitleLoadedCallbackClass = env->GetObjectClass(game_title_loaded_callback);
	jmethodID onGameTitleLoadedMID = env->GetMethodID(gameTitleLoadedCallbackClass, "onGameTitleLoaded", "(JLjava/lang/String;)V");
	env->DeleteLocalRef(gameTitleLoadedCallbackClass);
	s_emulationState.setOnGameTitleLoaded(std::make_shared<AndroidGameTitleLoadedCallback>(onGameTitleLoadedMID, game_title_loaded_callback));
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setGameIconLoadedCallback(JNIEnv *env, [[maybe_unused]] jclass clazz, jobject game_icon_loaded_callback)
{
	jclass gameIconLoadedCallbackClass = env->GetObjectClass(game_icon_loaded_callback);
	jmethodID gameIconLoadedMID = env->GetMethodID(gameIconLoadedCallbackClass, "onGameIconLoaded", "(J[III)V");
	s_emulationState.setOnGameIconLoaded(std::make_shared<AndroidGameIconLoadedCallback>(gameIconLoadedMID, game_icon_loaded_callback));
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_requestGameIcon([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jlong title_id)
{
	s_emulationState.requestGameIcon(*reinterpret_cast<TitleId *>(&title_id));
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_reloadGameTitles([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz)
{
	s_emulationState.reloadGameTitles();
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializeActiveSettings(JNIEnv *env, [[maybe_unused]] jclass clazz, jstring data_path, jstring cache_path)
{
	std::string dataPath = JNIUtils::JStringToString(env, data_path);
	std::string cachePath = JNIUtils::JStringToString(env, cache_path);
	s_emulationState.initializeActiveSettings(dataPath, cachePath);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializeEmulation([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz)
{
	s_emulationState.initializeEmulation();
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializerRenderer([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz)
{
	s_emulationState.initializeRenderer();
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializeRendererSurface([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jboolean is_main_canvas)
{
	s_emulationState.initializeRenderSurface(is_main_canvas);
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setDPI([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jfloat dpi)
{
	s_emulationState.setDPI(dpi);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_clearSurface([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jboolean is_main_canvas)
{
	s_emulationState.clearSurface(is_main_canvas);
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_recreateRenderSurface([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jboolean is_main_canvas)
{
	s_emulationState.notifySurfaceChanged(is_main_canvas);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_addGamePath(JNIEnv *env, [[maybe_unused]] jclass clazz, jstring uri)
{
	s_emulationState.addGamePath(JNIUtils::JStringToString(env, uri));
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onNativeKey(JNIEnv *env, [[maybe_unused]] jclass clazz, jstring device_descriptor, jstring device_name, jint key, jboolean is_pressed)
{
	auto deviceDescriptor = JNIUtils::JStringToString(env, device_descriptor);
	auto deviceName = JNIUtils::JStringToString(env, device_name);
	s_emulationState.onKeyEvent(deviceDescriptor, deviceName, key, is_pressed);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onNativeAxis(JNIEnv *env, [[maybe_unused]] jclass clazz, jstring device_descriptor, jstring device_name, jint axis, jfloat value)
{
	auto deviceDescriptor = JNIUtils::JStringToString(env, device_descriptor);
	auto deviceName = JNIUtils::JStringToString(env, device_name);
	s_emulationState.onAxisEvent(deviceDescriptor, deviceName, axis, value);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setControllerType([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jint index, jint emulated_controller_type)
{
	s_emulationState.setEmulatedControllerType(index, static_cast<EmulatedController::Type>(emulated_controller_type));
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getControllerType([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jint index)
{
	return s_emulationState.getEmulatedControllerType(index);
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getWPADControllersCount([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz)
{
	return s_emulationState.getWPADControllersCount();
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getVPADControllersCount([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz)
{
	return s_emulationState.getVPADControllersCount();
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_isControllerDisabled([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jint index)
{
	return s_emulationState.isEmulatedControllerDisabled(index);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setControllerMapping(JNIEnv *env, [[maybe_unused]] jclass clazz, jstring device_descriptor, jstring device_name, jint index, jint mapping_id, jint button_id)
{
	auto deviceName = JNIUtils::JStringToString(env, device_name);
	auto deviceDescriptor = JNIUtils::JStringToString(env, device_descriptor);
	s_emulationState.setControllerMapping(deviceDescriptor, deviceName, index, mapping_id, button_id);
}

extern "C" [[maybe_unused]] JNIEXPORT jstring JNICALL
Java_info_cemu_Cemu_NativeLibrary_getControllerMapping(JNIEnv *env, [[maybe_unused]] jclass clazz, jint index, jint mapping_id)
{
	auto mapping = s_emulationState.getEmulatedControllerMapping(index, mapping_id);
	return env->NewStringUTF(mapping.value_or("").c_str());
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_clearControllerMapping([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jint index, jint mapping_id)
{
	s_emulationState.clearEmulatedControllerMapping(index, mapping_id);
}

extern "C" [[maybe_unused]] JNIEXPORT jobject JNICALL
Java_info_cemu_Cemu_NativeLibrary_getControllerMappings(JNIEnv *env, [[maybe_unused]] jclass clazz, jint index)
{
	jclass hashMapClass = env->FindClass("java/util/HashMap");
	jmethodID hashMapConstructor = env->GetMethodID(hashMapClass, "<init>", "()V");
	jmethodID hashMapPut = env->GetMethodID(hashMapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
	jclass integerClass = env->FindClass("java/lang/Integer");
	jmethodID integerConstructor = env->GetMethodID(integerClass, "<init>", "(I)V");
	jobject hashMapObj = env->NewObject(hashMapClass, hashMapConstructor);
	auto mappings = s_emulationState.getEmulatedControllerMappings(index);
	for (const auto &pair : mappings)
	{
		jint key = pair.first;
		jstring buttonName = env->NewStringUTF(pair.second.c_str());
		jobject mappingId = env->NewObject(integerClass, integerConstructor, key);
		env->CallObjectMethod(hashMapObj, hashMapPut, mappingId, buttonName);
	}
	return hashMapObj;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onKeyEvent(JNIEnv *env, [[maybe_unused]] jclass clazz, jstring device_descriptor, jstring device_name, jint key_code, jboolean is_pressed)
{
	auto deviceDescriptor = JNIUtils::JStringToString(env, device_descriptor);
	auto deviceName = JNIUtils::JStringToString(env, device_name);
	s_emulationState.onKeyEvent(deviceDescriptor, deviceName, key_code, is_pressed);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_onAxisEvent([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jstring device_descriptor, jstring device_name, jint axis_code, jfloat value)
{
	auto deviceDescriptor = JNIUtils::JStringToString(env, device_descriptor);
	auto deviceName = JNIUtils::JStringToString(env, device_name);
	s_emulationState.onAxisEvent(deviceDescriptor, deviceName, axis_code, value);
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_getAsyncShaderCompile([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().async_compile;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setAsyncShaderCompile([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().async_compile = enabled;
	g_config.Save();
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getVSyncMode([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().vsync;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setVSyncMode([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jint vsync_mode)
{
	g_config.data().vsync = vsync_mode;
	g_config.Save();
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_getAccurateBarriers([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz)
{
	return g_config.data().vk_accurate_barriers;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setAccurateBarriers([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	g_config.data().vk_accurate_barriers = enabled;
	g_config.Save();
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_Cemu_NativeLibrary_getAudioDeviceEnabled([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jboolean tv)
{
	const auto &device = tv ? g_config.data().tv_device : g_config.data().pad_device;
	return !device.empty();
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setAudioDeviceEnabled([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jboolean enabled, jboolean tv)
{
	auto &device = tv ? g_config.data().tv_device : g_config.data().pad_device;
	if (enabled)
		device = L"Default";
	else
		device.clear();
	g_config.Save();
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getAudioDeviceChannels([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jboolean tv)
{
	const auto &deviceChannels = tv ? g_config.data().tv_channels : g_config.data().pad_channels;
	return deviceChannels;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setAudioDeviceChannels([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jint channels, jboolean tv)
{
	auto &deviceChannels = tv ? g_config.data().tv_channels : g_config.data().pad_channels;
	deviceChannels = static_cast<AudioChannels>(channels);
	g_config.Save();
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_Cemu_NativeLibrary_getAudioDeviceVolume([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jboolean tv)
{
	const auto &deviceVolume = tv ? g_config.data().tv_volume : g_config.data().pad_volume;
	return deviceVolume;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setAudioDeviceVolume([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass clazz, jint volume, jboolean tv)
{
	auto &deviceVolume = tv ? g_config.data().tv_volume : g_config.data().pad_volume;
	deviceVolume = volume;
	g_config.Save();
}
