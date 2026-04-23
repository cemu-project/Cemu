#include "JNIUtils.h"
#include "config/ActiveSettings.h"

extern "C" [[maybe_unused]] JNIEXPORT jstring JNICALL
Java_info_cemu_cemu_nativeinterface_NativeActiveSettings_getMLCPath(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return JNIUtils::ToJString(env, ActiveSettings::GetMlcPath());
}

extern "C" [[maybe_unused]] JNIEXPORT jstring JNICALL
Java_info_cemu_cemu_nativeinterface_NativeActiveSettings_getUserDataPath(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return JNIUtils::ToJString(env, ActiveSettings::GetUserDataPath());
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeActiveSettings_initializeActiveSettings(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring userDataPath, jstring configPath, jstring dataPath, jstring cachePath)
{
	std::set<fs::path> failedWriteAccess;
	ActiveSettings::SetPaths(
		false,
		{},
		JNIUtils::FromJString(env, userDataPath),
		JNIUtils::FromJString(env, configPath),
		JNIUtils::FromJString(env, cachePath),
		JNIUtils::FromJString(env, dataPath),
		failedWriteAccess);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeActiveSettings_setNativeLibDir(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring nativeLibDir)
{
	ActiveSettings::SetNativeLibPath(JNIUtils::FromJString(env, nativeLibDir));
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeActiveSettings_setInternalDir(JNIEnv* env, [[maybe_unused]] jclass clazz, jstring internalDir)
{
	ActiveSettings::SetInternalDir(JNIUtils::FromJString(env, internalDir));
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeActiveSettings_hasRequiredOnlineFiles(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return ActiveSettings::HasRequiredOnlineFiles();
}