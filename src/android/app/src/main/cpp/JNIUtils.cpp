#include "JNIUtils.h"

namespace JNIUtils
{
	JavaVM* g_jvm = nullptr;
};

std::string JNIUtils::JStringToString(JNIEnv* env, jstring jstr)
{
	const char* c_str = env->GetStringUTFChars(jstr, nullptr);
	std::string str(c_str);
	env->ReleaseStringUTFChars(jstr, c_str);
	return str;
}

jobject JNIUtils::createJavaStringArrayList(JNIEnv* env, const std::vector<std::string>& strings)
{
	jclass clsArrayList = env->FindClass("java/util/ArrayList");
	jmethodID arrayListConstructor = env->GetMethodID(clsArrayList, "<init>", "()V");
	jobject arrayListObject = env->NewObject(clsArrayList, arrayListConstructor);
	jmethodID addMethod = env->GetMethodID(clsArrayList, "add", "(Ljava/lang/Object;)Z");
	env->DeleteLocalRef(clsArrayList);

	for (const auto& string : strings)
	{
		jstring element = env->NewStringUTF(string.c_str());
		env->CallBooleanMethod(arrayListObject, addMethod, element);
		env->DeleteLocalRef(element);
	}
	return arrayListObject;
}

JNIUtils::Scopedjobject JNIUtils::getEnumValue(JNIEnv* env, const std::string& enumClassName, const std::string& enumName)
{
	jclass enumClass = env->FindClass(enumClassName.c_str());
	jfieldID fieldID = env->GetStaticFieldID(enumClass, enumName.c_str(), ("L" + enumClassName + ";").c_str());
	jobject enumValue = env->GetStaticObjectField(enumClass, fieldID);
	env->DeleteLocalRef(enumClass);
	Scopedjobject enumObj = Scopedjobject(enumValue);
	env->DeleteLocalRef(enumValue);
	return enumObj;
}

jobject JNIUtils::createArrayList(JNIEnv* env, const std::vector<jobject>& objects)
{
	static Scopedjclass listClass = Scopedjclass("java/util/ArrayList");
	static jmethodID listConstructor = env->GetMethodID(*listClass, "<init>", "()V");
	static jmethodID listAdd = env->GetMethodID(*listClass, "add", "(Ljava/lang/Object;)Z");

	jobject arrayList = env->NewObject(*listClass, listConstructor);
	for (auto&& obj : objects)
		env->CallBooleanMethod(arrayList, listAdd, obj);
	return arrayList;
}

jobject JNIUtils::createJavaLongArrayList(JNIEnv* env, const std::vector<uint64_t>& values)
{
	jclass longClass = env->FindClass("java/lang/Long");
	jmethodID valueOf = env->GetStaticMethodID(longClass, "valueOf", "(J)Ljava/lang/Long;");
	std::vector<jobject> valuesJava;
	for (auto&& value : values)
		valuesJava.push_back(env->CallStaticObjectMethod(longClass, valueOf, static_cast<jlong>(value)));
	env->DeleteLocalRef(longClass);
	return JNIUtils::createArrayList(env, valuesJava);
}

jobject JNIUtils::createJavaStringArrayList(JNIEnv* env, const std::vector<std::wstring>& strings)
{
	jclass arrayListClass = env->FindClass("java/util/ArrayList");
	jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "()V");
	jobject arrayListObject = env->NewObject(arrayListClass, arrayListConstructor);
	jmethodID addMethod = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
	for (const auto& string : strings)
	{
		jstring element = env->NewString((jchar*)string.c_str(), string.length());
		env->CallBooleanMethod(arrayListObject, addMethod, element);
		env->DeleteLocalRef(element);
	}
	return arrayListObject;
}
