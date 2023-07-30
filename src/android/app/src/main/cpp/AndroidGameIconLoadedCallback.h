#pragma once

#include "GameIconLoader.h"
#include "JNIUtils.h"

class AndroidGameIconLoadedCallback : public GameIconLoadedCallback
{
	jmethodID m_onGameIconLoadedMID;
	JNIUtils::Scopedjobject m_gameTitleLoadedCallbackObj;

   public:
	AndroidGameIconLoadedCallback(jmethodID onGameIconLoadedMID,
	                              jobject gameTitleLoadedCallbackObj)
		: m_onGameIconLoadedMID(onGameIconLoadedMID),
		  m_gameTitleLoadedCallbackObj(gameTitleLoadedCallbackObj) {}

	void onIconLoaded(TitleId titleId, int* colors, int width, int height) override
	{
		JNIUtils::ScopedJNIENV env;
		jintArray jIconData = env->NewIntArray(width * height);
		env->SetIntArrayRegion(jIconData, 0, width * height, reinterpret_cast<const jint*>(colors));
		env->CallVoidMethod(*m_gameTitleLoadedCallbackObj, m_onGameIconLoadedMID, *reinterpret_cast<const jlong*>(&titleId), jIconData, width, height);
		env->DeleteLocalRef(jIconData);
	}
};