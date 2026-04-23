#pragma once

#include "Cafe/IOSU/PDM/iosu_pdm.h"
#include "Cafe/TitleList/TitleId.h"
#include "Cafe/TitleList/TitleList.h"
#include "Image.h"

struct Game
{
	std::string name;
	std::optional<fs::path> path;
	bool isFavorite;
	uint16 version;
	uint16 dlc;
	TitleId titleId;
	std::optional<std::chrono::year_month_day> lastPlayed;
	uint32 minutesPlayed;
	CafeConsoleRegion region;
};

class GameTitleLoadedCallback
{
  public:
	virtual void OnTitleLoaded(const Game& game, const std::shared_ptr<Image>& icon) = 0;
};

class GameTitleLoader
{
	std::mutex m_threadMutex;
	std::condition_variable m_condVar;
	std::thread m_loaderThread;
	std::atomic_bool m_continueLoading = true;
	std::deque<TitleId> m_titlesToLoad;
	std::optional<uint64> m_callbackIdTitleList;
	std::map<TitleId, Game> m_gameInfos;
	std::map<TitleId, std::shared_ptr<Image>> m_iconCache;
	std::map<TitleId, std::string> m_name_cache;
	std::shared_ptr<GameTitleLoadedCallback> m_gameTitleLoadedCallback = nullptr;

  public:
	GameTitleLoader();
	void QueueTitle(TitleId titleId);
	void SetOnTitleLoaded(std::shared_ptr<GameTitleLoadedCallback> gameTitleLoadedCallback);
	void ReloadGameTitles();
	~GameTitleLoader();
	void TitleRefresh(TitleId titleId);

  private:
	void LoadGameTitles();
	std::string GetNameByTitleId(TitleId titleId, const std::optional<TitleInfo>& titleInfo);
	std::shared_ptr<Image> LoadIcon(TitleId titleId, const std::optional<TitleInfo>& titleInfo);
	void HandleTitleListCallback(CafeTitleListCallbackEvent* evt);
};
