#include "JNIUtils.h"
#include "audio/IAudioAPI.h"
#include "config/CemuConfig.h"
#include "config/NetworkSettings.h"

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getOverlayPosition([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return static_cast<jint>(GetConfig().overlay.position);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setOverlayPosition([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint position)
{
	GetConfig().overlay.position = static_cast<ScreenPosition>(position);
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getOverlayTextScalePercentage([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().overlay.text_scale;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setOverlayTextScalePercentage([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint scalePercentage)
{
	GetConfig().overlay.text_scale = scalePercentage;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isOverlayFPSEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().overlay.fps;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setOverlayFPSEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().overlay.fps = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isOverlayDrawCallsPerFrameEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().overlay.drawcalls;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setOverlayDrawCallsPerFrameEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().overlay.drawcalls = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isOverlayCPUUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().overlay.cpu_usage;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setOverlayCPUUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().overlay.cpu_usage = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isOverlayCPUPerCoreUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().overlay.cpu_per_core_usage;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setOverlayCPUPerCoreUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().overlay.cpu_per_core_usage = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isOverlayRAMUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().overlay.ram_usage;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setOverlayRAMUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().overlay.ram_usage = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isOverlayVRAMUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().overlay.vram_usage;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setOverlayVRAMUsageEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().overlay.vram_usage = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isOverlayDebugEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().overlay.debug;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setOverlayDebugEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().overlay.debug = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getNotificationsPosition([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return static_cast<jint>(GetConfig().notification.position);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setNotificationsPosition([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint position)
{
	GetConfig().notification.position = static_cast<ScreenPosition>(position);
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getNotificationsTextScalePercentage([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().notification.text_scale;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setNotificationsTextScalePercentage([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint scalePercentage)
{
	GetConfig().notification.text_scale = scalePercentage;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isNotificationControllerProfilesEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().notification.controller_profiles;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setNotificationControllerProfilesEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().notification.controller_profiles = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isNotificationShaderCompilerEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().notification.shader_compiling;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setNotificationShaderCompilerEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().notification.shader_compiling = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isNotificationFriendListEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().notification.friends;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setNotificationFriendListEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().notification.friends = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_addGamesPath(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring uri)
{
	auto& gamePaths = GetConfig().game_paths;
	auto gamePath = JNIUtils::FromJString(env, uri);
	if (std::any_of(gamePaths.begin(), gamePaths.end(), [&](const auto& path) { return path == gamePath; }))
		return;
	gamePaths.push_back(gamePath);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_removeGamesPath(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring uri)
{
	auto gamePath = JNIUtils::FromJString(env, uri);
	auto& gamePaths = GetConfig().game_paths;
	std::erase_if(gamePaths, [&](const auto& path) { return path == gamePath; });
}

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getGamesPaths(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return JNIUtils::CreateStringObjectArray(env, GetConfig().game_paths);
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getAsyncShaderCompile([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().async_compile;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setAsyncShaderCompile([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().async_compile = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getVsyncMode([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().vsync;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setVsyncMode([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint vsync_mode)
{
	GetConfig().vsync = vsync_mode;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getAccurateBarriers([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().vk_accurate_barriers;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setUpscalingFilter([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint upscaling_filter)
{
	GetConfig().upscale_filter = upscaling_filter;
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getUpscalingFilter([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().upscale_filter;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setDownscalingFilter([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint downscaling_filter)
{
	GetConfig().downscale_filter = downscaling_filter;
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getDownscalingFilter([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().downscale_filter;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setFullscreenScaling([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint fullscreen_scaling)
{
	GetConfig().fullscreen_scaling = fullscreen_scaling;
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getFullscreenScaling([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().fullscreen_scaling;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setAccurateBarriers([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().vk_accurate_barriers = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getAudioDeviceEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean tv)
{
	const auto& device = tv ? GetConfig().tv_device : GetConfig().pad_device;
	return !device.empty();
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setAudioDeviceEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled, jboolean tv)
{
	auto& device = tv ? GetConfig().tv_device : GetConfig().pad_device;
	if (enabled)
		device = L"Default";
	else
		device.clear();
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getAudioDeviceChannels([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean tv)
{
	const auto& deviceChannels = tv ? GetConfig().tv_channels : GetConfig().pad_channels;
	return deviceChannels;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setAudioDeviceChannels([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint channels, jboolean tv)
{
	auto& deviceChannels = tv ? GetConfig().tv_channels : GetConfig().pad_channels;
	deviceChannels = static_cast<AudioChannels>(channels);
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getAudioDeviceVolume([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean tv)
{
	const auto& deviceVolume = tv ? GetConfig().tv_volume : GetConfig().pad_volume;
	return deviceVolume;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setAudioDeviceVolume([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint volume, jboolean tv)
{
	auto& deviceVolume = tv ? GetConfig().tv_volume : GetConfig().pad_volume;
	deviceVolume = volume;
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getAudioLatency([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().audio_delay * 12;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setAudioLatency([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint latency)
{
	sint32 audioDelay = latency / 12;
	GetConfig().audio_delay = audioDelay;
	IAudioAPI::SetAudioDelay(audioDelay);
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getConsoleLanguage([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return static_cast<jint>(GetConfig().console_language.GetValue());
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setConsoleLanguage([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint console_language)
{
	GetConfig().console_language = static_cast<CafeConsoleLanguage>(console_language);
}

extern "C" [[maybe_unused]] JNIEXPORT jstring JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getCustomDriverPath(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	std::string customDriverPath = GetConfig().custom_driver_path;
	if (customDriverPath.empty())
		return nullptr;
	return JNIUtils::ToJString(env, customDriverPath);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setCustomDriverPath(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring custom_driver_path)
{
	GetConfig().custom_driver_path = JNIUtils::FromJString(env, custom_driver_path);
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getAccountNetworkService([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint persistent_id)
{
	return static_cast<jint>(GetConfig().GetAccountNetworkService(persistent_id));
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setAccountNetworkService([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint persistent_id, jint network_service)
{
	GetConfig().SetAccountSelectedService(persistent_id, static_cast<NetworkService>(network_service));
}

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_getAccountPersistentId([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return static_cast<jint>(GetConfig().account.m_persistent_id);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setAccountPersistentId([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint persistent_id)
{
	GetConfig().account.m_persistent_id = persistent_id;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isEmulateSkylanderPortalEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().emulated_usb_devices.emulate_skylander_portal;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setEmulateSkylanderPortalEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().emulated_usb_devices.emulate_skylander_portal = enabled;
}


extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isEmulateInfinityBaseEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().emulated_usb_devices.emulate_infinity_base;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setEmulateInfinityBaseEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().emulated_usb_devices.emulate_infinity_base = enabled;
}


extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_isEmulateDimensionsToypadEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return GetConfig().emulated_usb_devices.emulate_dimensions_toypad;
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_setEmulateDimensionsToypadEnabled([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean enabled)
{
	GetConfig().emulated_usb_devices.emulate_dimensions_toypad = enabled;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_hasCustomNetworkConfiguration([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return NetworkConfig::XMLExists();
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSettings_saveSettings([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	GetConfigHandle().Save();
}
