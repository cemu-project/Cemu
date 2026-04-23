#include "JNIUtils.h"

static JavaVM* s_jvm = nullptr;

namespace JNIUtils
{
	void SetJavaVM(JavaVM* jvm)
	{
		s_jvm = jvm;
	}

	Scopedjobject::Scopedjobject(Scopedjobject&& other) noexcept
	{
		this->m_jobject = other.m_jobject;
		other.m_jobject = nullptr;
	}

	void Scopedjobject::DeleteReference()
	{
		if (m_jobject)
		{
			GetEnv()->DeleteGlobalRef(m_jobject);
			m_jobject = nullptr;
		}
	}

	Scopedjobject& Scopedjobject::operator=(Scopedjobject&& other) noexcept
	{
		if (this != &other)
		{
			DeleteReference();
			m_jobject = other.m_jobject;
			other.m_jobject = nullptr;
		}
		return *this;
	}

	jobject Scopedjobject::operator*() const
	{
		return m_jobject;
	}

	Scopedjobject::Scopedjobject(jobject obj)
	{
		if (obj)
			m_jobject = GetEnv()->NewGlobalRef(obj);
	}

	Scopedjobject::~Scopedjobject()
	{
		DeleteReference();
	}

	Scopedjclass::Scopedjclass(Scopedjclass&& other) noexcept
	{
		this->m_jclass = other.m_jclass;
		other.m_jclass = nullptr;
	}

	Scopedjclass::Scopedjclass(jclass javaClass)
	{
		if (javaClass)
			m_jclass = static_cast<jclass>(GetEnv()->NewGlobalRef(javaClass));
	}

	Scopedjclass& Scopedjclass::operator=(Scopedjclass&& other) noexcept
	{
		if (this != &other)
		{
			if (m_jclass)
				GetEnv()->DeleteGlobalRef(m_jclass);
			m_jclass = other.m_jclass;
			other.m_jclass = nullptr;
		}
		return *this;
	}

	Scopedjclass::Scopedjclass(const char* className)
	{
		JNIEnv* env = GetEnv();
		jclass tempObj = env->FindClass(className);
		m_jclass = static_cast<jclass>(env->NewGlobalRef(tempObj));
		env->DeleteLocalRef(tempObj);
	}

	Scopedjclass::~Scopedjclass()
	{
		if (m_jclass)
			GetEnv()->DeleteGlobalRef(m_jclass);
	}

	jclass Scopedjclass::operator*() const
	{
		return m_jclass;
	}

	Scopedjobject GetEnumValue(JNIEnv* env, const std::string& enumClassName, const std::string& enumName)
	{
		jclass enumClass = env->FindClass(enumClassName.c_str());
		jfieldID fieldID = env->GetStaticFieldID(enumClass, enumName.c_str(), ("L" + enumClassName + ";").c_str());
		jobject enumValue = env->GetStaticObjectField(enumClass, fieldID);
		env->DeleteLocalRef(enumClass);
		Scopedjobject enumObj = Scopedjobject(enumValue);
		env->DeleteLocalRef(enumValue);
		return enumObj;
	}

	JNIEnv* GetEnv()
	{
		thread_local static struct OwnedEnv
		{
			JNIEnv* env;
			jint result;

			OwnedEnv()
			{
				result = s_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

				if (result == JNI_EDETACHED)
					s_jvm->AttachCurrentThread(&env, nullptr);
			}

			~OwnedEnv()
			{
				if (result == JNI_EDETACHED)
					s_jvm->DetachCurrentThread();
			}
		} owned;

		return owned.env;
	}
}; // namespace JNIUtils
