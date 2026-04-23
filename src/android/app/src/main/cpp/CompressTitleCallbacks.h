#pragma once

#include "JNIUtils.h"

class CompressTitleCallbacks
{
	jmethodID m_onFinishedMID;
	jmethodID m_onErrorMID;
	JNIUtils::Scopedjobject m_compressTitleCallbacks;

  public:
	explicit CompressTitleCallbacks(jobject compressTitleCallbacks);

	void OnFinished();

	void OnError();
};
