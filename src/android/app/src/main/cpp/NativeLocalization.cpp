#include "JNIUtils.h"

namespace NativeLocalization
{
	std::map<std::string, std::string, std::less<>> s_messages;

	std::string Translate(std::string_view msgId)
	{
		if (auto message = s_messages.find(msgId); message != s_messages.end())
		{
			return message->second;
		}

		return std::string{msgId};
	}
} // namespace NativeLocalization

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeLocalization_setTranslations(JNIEnv* env, [[maybe_unused]] jclass clazz, jobject translations)
{
    NativeLocalization::s_messages.clear();

	jclass mapClass = env->GetObjectClass(translations);
	jmethodID keySetMethodId = env->GetMethodID(mapClass, "keySet", "()Ljava/util/Set;");
	jmethodID getMethodId = env->GetMethodID(mapClass, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
	jobject keySet = env->CallObjectMethod(translations, keySetMethodId);
	jclass setClass = env->GetObjectClass(keySet);
	jmethodID toArrayMethodId = env->GetMethodID(setClass, "toArray", "()[Ljava/lang/Object;");
	auto keyArray = static_cast<jobjectArray>(env->CallObjectMethod(keySet, toArrayMethodId));
	jint size = env->GetArrayLength(keyArray);

	for (jint i = 0; i < size; i++)
	{
		auto keyJava = static_cast<jstring>(env->GetObjectArrayElement(keyArray, i));
		std::string key = JNIUtils::FromJString(env, keyJava);
		auto translationJava = static_cast<jstring>(env->CallObjectMethod(translations, getMethodId, keyJava));
		std::string translation = JNIUtils::FromJString(env, translationJava);
		NativeLocalization::s_messages[key] = translation;
	}

    SetTranslationCallback(NativeLocalization::Translate);
}