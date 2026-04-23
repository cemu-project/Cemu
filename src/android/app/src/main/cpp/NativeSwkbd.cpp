#include "JNIUtils.h"

#include "Cafe/OS/libs/swkbd/swkbd.h"

namespace NativeSwkbd
{
	std::atomic<bool> s_isOpen;

	std::string s_currentInputText;

	struct StrDiffs
	{
		size_t newTextStartIndex;
		size_t numberOfCharacterToDelete;
	};

	StrDiffs GetStringDiffs(const std::string& newText, const std::string& currentText)
	{
		if (newText.length() < currentText.length() && currentText.starts_with(newText))
			return {newText.length(), currentText.length() - newText.length()};
		if (newText.length() >= currentText.length() && newText.starts_with(currentText))
			return {currentText.length(), 0};
		return {0, currentText.length()};
	}

	class AndroidSwkbdCallbacks : public swkbd::swkbdCallbacks
	{
		JNIUtils::Scopedjclass m_emulationActivityClass;
		jmethodID m_showSoftwareKeyboardMethodID;
		jmethodID m_hideSoftwareKeyboardMethodID;

	  public:
		AndroidSwkbdCallbacks()
		{
			JNIEnv* env = JNIUtils::GetEnv();
			m_emulationActivityClass = JNIUtils::Scopedjclass("info/cemu/cemu/nativeinterface/NativeSwkbd$SwkbdState");
			m_showSoftwareKeyboardMethodID = env->GetStaticMethodID(*m_emulationActivityClass, "makeVisible", "(Ljava/lang/String;I)V");
			m_hideSoftwareKeyboardMethodID = env->GetStaticMethodID(*m_emulationActivityClass, "hide", "()V");
		}

		void showSoftwareKeyboard(const std::string& initialText, sint32 maxLength) override
		{
			s_isOpen = true;

			s_currentInputText = initialText;

			JNIUtils::FiberSafeJNICall([&](JNIEnv* env) {
				jstring j_initialText = JNIUtils::ToJString(env, initialText);
				JNIUtils::GetEnv()->CallStaticVoidMethod(*m_emulationActivityClass, m_showSoftwareKeyboardMethodID, j_initialText, maxLength);
				env->DeleteLocalRef(j_initialText);
			});
		}

		void hideSoftwareKeyboard() override
		{
			s_isOpen = false;

			JNIUtils::FiberSafeJNICall([&](JNIEnv* env) {
				env->CallStaticVoidMethod(*m_emulationActivityClass, m_hideSoftwareKeyboardMethodID);
			});
		}
	};
} // namespace NativeSwkbd

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSwkbd_initializeSwkbd([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	static std::shared_ptr<swkbd::swkbdCallbacks> swkbdCallbacks = nullptr;

	if (swkbdCallbacks != nullptr)
		return;

	swkbdCallbacks = std::make_shared<NativeSwkbd::AndroidSwkbdCallbacks>();
	swkbd::setSwkbdCallbacks(swkbdCallbacks);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSwkbd_onTextChanged([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jstring j_text)
{
	if (!NativeSwkbd::s_isOpen)
		return;

	std::string text = JNIUtils::FromJString(env, j_text);
	auto stringDiff = NativeSwkbd::GetStringDiffs(text, NativeSwkbd::s_currentInputText);
	for (size_t i = 0; i < stringDiff.numberOfCharacterToDelete; i++)
		swkbd::keyInput(swkbd::BACKSPACE_KEYCODE);
	for (size_t i = stringDiff.newTextStartIndex; i < text.length(); i++)
		swkbd::keyInput(text.at(i));
	NativeSwkbd::s_currentInputText = std::move(text);
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeSwkbd_onFinishedInputEdit([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	if (!NativeSwkbd::s_isOpen)
		return;

	swkbd::keyInput(swkbd::RETURN_KEYCODE);
}
