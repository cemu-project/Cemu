#pragma once

#include "Cafe/TitleList/TitleId.h"
#include "Image.h"

class GameIconLoadedCallback
{
   public:
	virtual void onIconLoaded(TitleId titleId, int *colors, int width, int height) = 0;
};

class GameIconLoader
{
	std::condition_variable m_condVar;
	std::atomic_bool m_continueLoading = true;
	std::thread m_loaderThread;
	std::mutex m_threadMutex;
	std::deque<TitleId> m_iconsToLoad;
	std::map<TitleId, Image> m_iconCache;
	std::shared_ptr<GameIconLoadedCallback> m_onIconLoaded = nullptr;

   public:
	GameIconLoader();

	~GameIconLoader();

	const Image &getGameIcon(TitleId titleId);

	void setOnIconLoaded(const std::shared_ptr<GameIconLoadedCallback> &onIconLoaded);

	void requestIcon(TitleId titleId);

   private:
	void loadGameIcons();
};
