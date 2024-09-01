#pragma once

#include <android/native_window_jni.h>
#include <jni.h>

namespace JNIUtils
{
	extern JavaVM* g_jvm;

	std::string JStringToString(JNIEnv* env, jstring jstr);

	jobject createJavaStringArrayList(JNIEnv* env, const std::vector<std::string>& stringList);

	jobject createJavaStringArrayList(JNIEnv* env, const std::vector<std::wstring>& stringList);

	class ScopedJNIENV
	{
	  public:
		ScopedJNIENV()
		{
			jint result = g_jvm->GetEnv((void**)&m_env, JNI_VERSION_1_6);
			if (result == JNI_EDETACHED)
			{
				JavaVMAttachArgs args;
				args.version = JNI_VERSION_1_6;
				args.name = nullptr;
				args.group = nullptr;
				result = g_jvm->AttachCurrentThread(&m_env, &args);
				if (result == JNI_OK)
					m_threadWasAttached = true;
			}
		}

		JNIEnv*& operator*()
		{
			return m_env;
		}

		JNIEnv* operator->()
		{
			return m_env;
		}

		~ScopedJNIENV()
		{
			if (m_threadWasAttached)
				g_jvm->DetachCurrentThread();
		}

	  private:
		JNIEnv* m_env = nullptr;
		bool m_threadWasAttached = false;
	};

	class Scopedjobject
	{
	  public:
		Scopedjobject() = default;

		Scopedjobject(Scopedjobject&& other)
		{
			this->m_jobject = other.m_jobject;
			other.m_jobject = nullptr;
		}
		void deleteRef()
		{
			if (m_jobject)
			{
				ScopedJNIENV()->DeleteGlobalRef(m_jobject);
				m_jobject = nullptr;
			}
		}
		Scopedjobject& operator=(Scopedjobject&& other)
		{
			if (this != &other)
			{
				deleteRef();
				m_jobject = other.m_jobject;
				other.m_jobject = nullptr;
			}
			return *this;
		}
		jobject& operator*()
		{
			return m_jobject;
		}

		Scopedjobject(jobject obj)
		{
			if (obj)
				m_jobject = ScopedJNIENV()->NewGlobalRef(obj);
		}

		~Scopedjobject()
		{
			deleteRef();
		}

		bool isValid() const
		{
			return m_jobject;
		}

	  private:
		jobject m_jobject = nullptr;
	};

	class Scopedjclass
	{
	  public:
		Scopedjclass() = default;

		Scopedjclass(Scopedjclass&& other)
		{
			this->m_jclass = other.m_jclass;
			other.m_jclass = nullptr;
		}

		Scopedjclass& operator=(Scopedjclass&& other)
		{
			if (this != &other)
			{
				if (m_jclass)
					ScopedJNIENV()->DeleteGlobalRef(m_jclass);
				m_jclass = other.m_jclass;
				other.m_jclass = nullptr;
			}
			return *this;
		}

		Scopedjclass(const std::string& className)
		{
			ScopedJNIENV scopedEnv;
			jobject tempObj = scopedEnv->FindClass(className.c_str());
			m_jclass = static_cast<jclass>(scopedEnv->NewGlobalRef(tempObj));
			scopedEnv->DeleteLocalRef(tempObj);
		}

		~Scopedjclass()
		{
			if (m_jclass)
				ScopedJNIENV()->DeleteGlobalRef(m_jclass);
		}

		bool isValid() const
		{
			return m_jclass;
		}

		jclass& operator*()
		{
			return m_jclass;
		}

	  private:
		jclass m_jclass = nullptr;
	};

	Scopedjobject getEnumValue(JNIEnv* env, const std::string& enumClassName, const std::string& enumName);
	jobject createArrayList(JNIEnv* env, const std::vector<jobject>& objects);
	jobject createJavaLongArrayList(JNIEnv* env, const std::vector<uint64_t>& values);
} // namespace JNIUtils
