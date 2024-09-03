#include "GameTitleLoader.h"

std::optional<TitleInfo> getFirstTitleInfoByTitleId(TitleId titleId)
{
	TitleInfo titleInfo;
	if (CafeTitleList::GetFirstByTitleId(titleId, titleInfo))
		return titleInfo;
	return {};
}

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

void GameTitleLoader::setOnTitleLoaded(const std::shared_ptr<GameTitleLoadedCallback>& gameTitleLoadedCallback)
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
		CafeTitleList::UnregisterCallback(m_callbackIdTitleList.value());
	}
	m_gameInfos.clear();
	CafeTitleList::ClearScanPaths();
	for (auto&& gamePath : g_config.data().game_paths)
		CafeTitleList::AddScanPath(gamePath);
	CafeTitleList::Refresh();
	m_callbackIdTitleList = CafeTitleList::RegisterCallback([](CafeTitleListCallbackEvent* evt, void* ctx) { static_cast<GameTitleLoader*>(ctx)->HandleTitleListCallback(evt); }, this);
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

	Game& game = m_gameInfos[baseTitleId];
	std::optional<TitleInfo> titleInfo = getFirstTitleInfoByTitleId(titleId);
	game.titleId = baseTitleId;
	game.name = getNameByTitleId(baseTitleId, titleInfo);
	game.version = gameInfo.GetVersion();
	game.region = gameInfo.GetRegion();
	std::shared_ptr<Image> icon = loadIcon(baseTitleId, titleInfo);
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
			m_gameTitleLoadedCallback->onTitleLoaded(game, icon);
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
		TitleId titleId;
		{
			std::unique_lock lock(m_threadMutex);
			m_condVar.wait(lock, [this] { return (!m_titlesToLoad.empty()) || !m_continueLoading; });
			if (!m_continueLoading)
				return;
			titleId = m_titlesToLoad.front();
			m_titlesToLoad.pop_front();
		}
		titleRefresh(titleId);
	}
}
std::string GameTitleLoader::getNameByTitleId(TitleId titleId, const std::optional<TitleInfo>& titleInfo)
{
	auto it = m_name_cache.find(titleId);
	if (it != m_name_cache.end())
		return it->second;
	if (!titleInfo.has_value())
		return "Unknown title";
	std::string name;
	if (!GetConfig().GetGameListCustomName(titleId, name))
		name = titleInfo.value().GetMetaTitleName();
	m_name_cache.emplace(titleId, name);
	return name;
}

std::shared_ptr<Image> GameTitleLoader::loadIcon(TitleId titleId, const std::optional<TitleInfo>& titleInfo)
{
	if (auto iconIt = m_iconCache.find(titleId); iconIt != m_iconCache.end())
		return iconIt->second;
	std::string tempMountPath = TitleInfo::GetUniqueTempMountingPath();
	if (!titleInfo.has_value())
		return {};
	auto titleInfoValue = titleInfo.value();
	if (!titleInfoValue.Mount(tempMountPath, "", FSC_PRIORITY_BASE))
		return {};
	auto tgaData = fsc_extractFile((tempMountPath + "/meta/iconTex.tga").c_str());
	if (!tgaData || tgaData->size() <= 16)
	{
		cemuLog_log(LogType::Force, "Failed to load icon for title {:016x}", titleId);
		titleInfoValue.Unmount(tempMountPath);
		return {};
	}
	auto image = std::make_shared<Image>(tgaData.value());
	titleInfoValue.Unmount(tempMountPath);
	if (!image->isOk())
		return {};
	m_iconCache.emplace(titleId, image);
	return image;
}

void GameTitleLoader::HandleTitleListCallback(CafeTitleListCallbackEvent* evt)
{
	if (evt->eventType == CafeTitleListCallbackEvent::TYPE::TITLE_DISCOVERED || evt->eventType == CafeTitleListCallbackEvent::TYPE::TITLE_REMOVED)
	{
		queueTitle(evt->titleInfo->GetAppTitleId());
	}
}
