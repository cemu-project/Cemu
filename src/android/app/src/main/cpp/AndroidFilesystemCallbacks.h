#pragma once

#include "JNIUtils.h"
#include "Common/android/FilesystemAndroid.h"

class AndroidFilesystemCallbacks : public FilesystemAndroid::FilesystemCallbacks
{
	jmethodID m_openContentUriMid;
	jmethodID m_listFilesMid;
	jmethodID m_isDirectoryMid;
	jmethodID m_isFileMid;
	jmethodID m_existsMid;
	JNIUtils::Scopedjclass m_fileUtilClass;

	inline bool CallBooleanFunction(const std::filesystem::path& uri, jmethodID methodId)
	{
		bool result = false;
		JNIUtils::FiberSafeJNICall([&](JNIEnv* env) {
			jstring uriString = JNIUtils::ToJString(env, uri);
			result = env->CallStaticBooleanMethod(*m_fileUtilClass, methodId, uriString);
			env->DeleteLocalRef(uriString);
		});
		return result;
	}

  public:
	AndroidFilesystemCallbacks();

	int OpenContentUri(const std::filesystem::path& uri) override;

	std::vector<std::filesystem::path> ListFiles(const std::filesystem::path& uri) override;
	bool IsDirectory(const std::filesystem::path& uri) override;

	bool IsFile(const std::filesystem::path& uri) override;

	bool Exists(const std::filesystem::path& uri) override;
};