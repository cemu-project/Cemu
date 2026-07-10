#include "AndroidFilesystemCallbacks.h"

AndroidFilesystemCallbacks::AndroidFilesystemCallbacks()
{
	JNIEnv* env = JNIUtils::GetEnv();
	m_fileUtilClass = JNIUtils::Scopedjclass("info/cemu/cemu/nativeinterface/NativeFiles");
	m_openContentUriMid = env->GetStaticMethodID(*m_fileUtilClass, "openContentUri", "(Ljava/lang/String;)I");
	m_listFilesMid = env->GetStaticMethodID(*m_fileUtilClass, "listFiles", "(Ljava/lang/String;)[Ljava/lang/String;");
	m_isDirectoryMid = env->GetStaticMethodID(*m_fileUtilClass, "isDirectory", "(Ljava/lang/String;)Z");
	m_isFileMid = env->GetStaticMethodID(*m_fileUtilClass, "isFile", "(Ljava/lang/String;)Z");
	m_existsMid = env->GetStaticMethodID(*m_fileUtilClass, "exists", "(Ljava/lang/String;)Z");
}

int AndroidFilesystemCallbacks::OpenContentUri(const std::filesystem::path& uri)
{
	int fd = -1;
	JNIUtils::FiberSafeJNICall([&](JNIEnv* env) {
		jstring uriString = JNIUtils::ToJString(env, uri);
		fd = env->CallStaticIntMethod(*m_fileUtilClass, m_openContentUriMid, uriString);
		env->DeleteLocalRef(uriString);
	});
	return fd;
}

std::vector<std::filesystem::path> AndroidFilesystemCallbacks::ListFiles(const std::filesystem::path& uri)
{
	std::vector<std::filesystem::path> paths;
	JNIUtils::FiberSafeJNICall([&](JNIEnv* env) {
		jstring uriString = JNIUtils::ToJString(env, uri);
		jobjectArray pathsObjArray = static_cast<jobjectArray>(env->CallStaticObjectMethod(*m_fileUtilClass, m_listFilesMid, uriString));
		env->DeleteLocalRef(uriString);
		jsize arrayLength = env->GetArrayLength(pathsObjArray);
		paths.reserve(arrayLength);
		for (jsize i = 0; i < arrayLength; i++)
		{
			jstring pathStr = static_cast<jstring>(env->GetObjectArrayElement(pathsObjArray, i));
			paths.push_back(JNIUtils::FromJString(env, pathStr));
			env->DeleteLocalRef(pathStr);
		}
		env->DeleteLocalRef(pathsObjArray);
	});
	return paths;
}

bool AndroidFilesystemCallbacks::IsDirectory(const std::filesystem::path& uri)
{
	return CallBooleanFunction(uri, m_isDirectoryMid);
}

bool AndroidFilesystemCallbacks::IsFile(const std::filesystem::path& uri)
{
	return CallBooleanFunction(uri, m_isFileMid);
}

bool AndroidFilesystemCallbacks::Exists(const std::filesystem::path& uri)
{
	return CallBooleanFunction(uri, m_existsMid);
}