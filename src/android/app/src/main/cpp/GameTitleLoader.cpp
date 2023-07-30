#include "GameTitleLoader.h"

GameTitleLoader::GameTitleLoader()
{
	m_loaderThread = std::thread(&GameTitleLoader::loadGameTitles, this);
}

void GameTitleLoader::queueTitle(TitleId titleId)
{
	{
		std::lock_guard lock(m_threadMutex);
		m_titlesToLoad.emplace_front(titleId);
	}
	m_condVar.notify_one();
}

void GameTitleLoader::setOnTitleLoaded(const std::shared_ptr<GameTitleLoadedCallback> &gameTitleLoadedCallback)
{
	{
		std::lock_guard lock(m_threadMutex);
		m_gameTitleLoadedCallback = gameTitleLoadedCallback;
	}
	m_condVar.notify_one();
}

void GameTitleLoader::reloadGameTitles()
{
	if (m_callbackIdTitleList.has_value())
	{
		CafeTitleList::Refresh();
		CafeTitleList::UnregisterCallback(m_callbackIdTitleList.value());
	}
	m_gameInfos.clear();
	registerCallback();
}

GameTitleLoader::~GameTitleLoader()
{
	m_continueLoading = false;
	m_condVar.notify_one();
	m_loaderThread.join();
	if (m_callbackIdTitleList.has_value())
		CafeTitleList::UnregisterCallback(m_callbackIdTitleList.value());
}

void GameTitleLoader::titleRefresh(TitleId titleId)
{
	GameInfo2 gameInfo = CafeTitleList::GetGameInfo(titleId);
	if (!gameInfo.IsValid())
	{
		return;
	}
	TitleId baseTitleId = gameInfo.GetBaseTitleId();
	bool isNewEntry = false;
	if (auto gameInfoIt = m_gameInfos.find(baseTitleId); gameInfoIt == m_gameInfos.end())
	{
		isNewEntry = true;
		m_gameInfos[baseTitleId] = Game();
	}
	Game &game = m_gameInfos[baseTitleId];
	game.titleId = baseTitleId;
	game.name = GetNameByTitleId(baseTitleId);
	game.version = gameInfo.GetVersion();
	game.region = gameInfo.GetRegion();
	if (gameInfo.HasAOC())
	{
		game.dlc = gameInfo.GetAOCVersion();
	}
	if (isNewEntry)
	{
		iosu::pdm::GameListStat playTimeStat{};
		if (iosu::pdm::GetStatForGamelist(baseTitleId, playTimeStat))
			game.secondsPlayed = playTimeStat.numMinutesPlayed * 60;
		if (m_gameTitleLoadedCallback)
			m_gameTitleLoadedCallback->onTitleLoaded(game);
	}
	else
	{
		// TODO: Update
	}
}

void GameTitleLoader::loadGameTitles()
{
	while (m_continueLoading)
	{
		TitleId titleId = 0;
		{
			std::unique_lock lock(m_threadMutex);
			m_condVar.wait(lock, [this]
			               { return (!m_titlesToLoad.empty()) ||
				                    !m_continueLoading; });
			if (!m_continueLoading)
				return;
			titleId = m_titlesToLoad.front();
			m_titlesToLoad.pop_front();
		}
		titleRefresh(titleId);
	}
}

std::string GameTitleLoader::GetNameByTitleId(uint64 titleId)
{
	auto it = m_name_cache.find(titleId);
	if (it != m_name_cache.end())
		return it->second;
	TitleInfo titleInfo;
	if (!CafeTitleList::GetFirstByTitleId(titleId, titleInfo))
		return "Unknown title";
	std::string name;
	if (!GetConfig().GetGameListCustomName(titleId, name))
		name = titleInfo.GetTitleName();
	m_name_cache.emplace(titleId, name);
	return name;
}

void GameTitleLoader::registerCallback()
{
	m_callbackIdTitleList = CafeTitleList::RegisterCallback(
		[](CafeTitleListCallbackEvent *evt, void *ctx)
		{
			static_cast<GameTitleLoader *>(ctx)->HandleTitleListCallback(evt);
		},
		this);
}

void GameTitleLoader::HandleTitleListCallback(CafeTitleListCallbackEvent *evt)
{
	if (evt->eventType == CafeTitleListCallbackEvent::TYPE::TITLE_DISCOVERED || evt->eventType == CafeTitleListCallbackEvent::TYPE::TITLE_REMOVED)
	{
		queueTitle(evt->titleInfo->GetAppTitleId());
	}
}

void GameTitleLoader::addGamePath(const fs::path &path)
{
	CafeTitleList::ClearScanPaths();
	CafeTitleList::AddScanPath(path);
	CafeTitleList::Refresh();
}
