#pragma once

#include "GameTitleLoader.h"

class AndroidGameTitleLoadedCallback : public GameTitleLoadedCallback {
	jmethodID m_onGameTitleLoadedMID;
	JNIUtils::Scopedjobject m_gameTitleLoadedCallbackObj;

  public:
	AndroidGameTitleLoadedCallback(jmethodID onGameTitleLoadedMID, jobject gameTitleLoadedCallbackObj)
		: m_onGameTitleLoadedMID(onGameTitleLoadedMID),
		  m_gameTitleLoadedCallbackObj(gameTitleLoadedCallbackObj) {}

	void onTitleLoaded(const Game& game) override
	{
		JNIUtils::ScopedJNIENV env;
		jstring name = env->NewStringUTF(game.name.c_str());
		jlong titleId = static_cast<const jlong>(game.titleId);
		env->CallVoidMethod(*m_gameTitleLoadedCallbackObj, m_onGameTitleLoadedMID, titleId, name);
		env->DeleteLocalRef(name);
	}
};
