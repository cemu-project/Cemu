#pragma once

#include "JNIUtils.h"
#include "Common/unix/FilesystemAndroid.h"

class AndroidFilesystemCallbacks : public FilesystemAndroid::FilesystemCallbacks
{
	jmethodID m_openContentUriMid;
	jmethodID m_listFilesMid;
	jmethodID m_isDirectoryMid;
	jmethodID m_isFileMid;
	jmethodID m_existsMid;
	JNIUtils::Scopedjclass m_fileUtilClass;
	std::function<void(JNIEnv *)> m_function = nullptr;
	std::mutex m_functionMutex;
	std::condition_variable m_functionCV;
	std::thread m_thread;
	std::mutex m_threadMutex;
	std::condition_variable m_threadCV;
	std::atomic_bool m_continue = true;

	bool callBooleanFunction(const std::filesystem::path &uri, jmethodID methodId)
	{
		std::lock_guard lock(m_functionMutex);
		bool functionFinished = false;
		bool result = false;
		m_function = [&, this](JNIEnv *env)
		{
			jstring uriString = env->NewStringUTF(uri.c_str());
			result = env->CallStaticBooleanMethod(*m_fileUtilClass, methodId, uriString);
			env->DeleteLocalRef(uriString);
			functionFinished = true;
		};
		m_threadCV.notify_one();
		{
			std::unique_lock threadLock(m_threadMutex);
			m_functionCV.wait(threadLock, [&]() -> bool
			                  { return functionFinished; });
		}
		return result;
	}

	void runFilesystemCallbacks()
	{
		JNIUtils::ScopedJNIENV env;
		while (m_continue)
		{
			std::unique_lock threadLock(m_threadMutex);
			m_threadCV.wait(threadLock, [&]
			                { return m_function || !m_continue; });
			if (!m_continue)
				return;
			m_function(*env);
			m_function = nullptr;
			m_functionCV.notify_one();
		}
	}

   public:
	AndroidFilesystemCallbacks()
	{
		JNIUtils::ScopedJNIENV env;
		m_fileUtilClass = JNIUtils::Scopedjclass("info/cemu/Cemu/FileUtil");
		m_openContentUriMid = env->GetStaticMethodID(*m_fileUtilClass, "openContentUri", "(Ljava/lang/String;)I");
		m_listFilesMid = env->GetStaticMethodID(*m_fileUtilClass, "listFiles", "(Ljava/lang/String;)[Ljava/lang/String;");
		m_isDirectoryMid = env->GetStaticMethodID(*m_fileUtilClass, "isDirectory", "(Ljava/lang/String;)Z");
		m_isFileMid = env->GetStaticMethodID(*m_fileUtilClass, "isFile", "(Ljava/lang/String;)Z");
		m_existsMid = env->GetStaticMethodID(*m_fileUtilClass, "exists", "(Ljava/lang/String;)Z");
		m_thread = std::thread(&AndroidFilesystemCallbacks::runFilesystemCallbacks, this);
	}

	~AndroidFilesystemCallbacks()
	{
		m_continue = false;
		m_threadCV.notify_one();
		m_thread.join();
	}

	int openContentUri(const std::filesystem::path &uri) override
	{
		std::lock_guard lock(m_functionMutex);
		bool functionFinished = false;
		int fd = -1;
		m_function = [&, this](JNIEnv *env)
		{
			jstring uriString = env->NewStringUTF(uri.c_str());
			fd = env->CallStaticIntMethod(*m_fileUtilClass, m_openContentUriMid, uriString);
			env->DeleteLocalRef(uriString);
			functionFinished = true;
		};
		m_threadCV.notify_one();
		{
			std::unique_lock threadLock(m_threadMutex);
			m_functionCV.wait(threadLock, [&]()
			                  { return functionFinished; });
		}
		return fd;
	}

	std::vector<std::filesystem::path> listFiles(const std::filesystem::path &uri) override
	{
		std::lock_guard lock(m_functionMutex);
		bool functionFinished = false;
		std::vector<std::filesystem::path> paths;
		m_function = [&, this](JNIEnv *env)
		{
			jstring uriString = env->NewStringUTF(uri.c_str());
			jobjectArray pathsObjArray = static_cast<jobjectArray>(env->CallStaticObjectMethod(*m_fileUtilClass, m_listFilesMid, uriString));
			env->DeleteLocalRef(uriString);
			jsize arrayLength = env->GetArrayLength(pathsObjArray);
			paths.reserve(arrayLength);
			for (jsize i = 0; i < arrayLength; i++)
			{
				jstring pathStr = static_cast<jstring>(env->GetObjectArrayElement(pathsObjArray, i));
				paths.push_back(JNIUtils::JStringToString(env, pathStr));
				env->DeleteLocalRef(pathStr);
			}
			env->DeleteLocalRef(pathsObjArray);
			functionFinished = true;
		};
		m_threadCV.notify_one();
		{
			std::unique_lock threadLock(m_threadMutex);
			m_functionCV.wait(threadLock, [&]()
			                  { return functionFinished; });
		}
		return paths;
	}

	bool isDirectory(const std::filesystem::path &uri) override
	{
		return callBooleanFunction(uri, m_isDirectoryMid);
	}

	bool isFile(const std::filesystem::path &uri) override
	{
		return callBooleanFunction(uri, m_isFileMid);
	}

	bool exists(const std::filesystem::path &uri) override
	{
		return callBooleanFunction(uri, m_existsMid);
	}
};