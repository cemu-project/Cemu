#include "CompressTitleCallbacks.h"

CompressTitleCallbacks::CompressTitleCallbacks(jobject compressTitleCallbacks)
	: m_compressTitleCallbacks{compressTitleCallbacks}
{
	JNIEnv* env = JNIUtils::GetEnv();
	JNIUtils::Scopedjclass compressTitleCallbacksClass("info/cemu/cemu/nativeinterface/NativeGameTitles$TitleCompressCallbacks");
	m_onFinishedMID = env->GetMethodID(*compressTitleCallbacksClass, "onFinished", "()V");
	m_onErrorMID = env->GetMethodID(*compressTitleCallbacksClass, "onError", "()V");
}

void CompressTitleCallbacks::OnFinished()
{
	JNIUtils::GetEnv()->CallVoidMethod(*m_compressTitleCallbacks, m_onFinishedMID);
}

void CompressTitleCallbacks::OnError()
{
	JNIUtils::GetEnv()->CallVoidMethod(*m_compressTitleCallbacks, m_onErrorMID);
}
