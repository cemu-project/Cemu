#include "GameIconLoader.h"

#include "Cafe/TitleList/TitleInfo.h"
#include "Cafe/TitleList/TitleList.h"

GameIconLoader::GameIconLoader()
{
	m_loaderThread = std::thread(&GameIconLoader::loadGameIcons, this);
}

GameIconLoader::~GameIconLoader()
{
	m_continueLoading = false;
	m_condVar.notify_one();
	m_loaderThread.join();
}

const Image &GameIconLoader::getGameIcon(TitleId titleId)
{
	return m_iconCache.at(titleId);
}

void GameIconLoader::requestIcon(TitleId titleId)
{
	{
		std::lock_guard lock(m_threadMutex);
		m_iconsToLoad.emplace_front(titleId);
	}
	m_condVar.notify_one();
}

void GameIconLoader::loadGameIcons()
{
	while (m_continueLoading)
	{
		TitleId titleId = 0;
		{
			std::unique_lock lock(m_threadMutex);
			m_condVar.wait(lock, [this]
			               { return !m_iconsToLoad.empty() || !m_continueLoading; });
			if (!m_continueLoading)
				return;
			titleId = m_iconsToLoad.front();
			m_iconsToLoad.pop_front();
		}
		TitleInfo titleInfo;
		if (!CafeTitleList::GetFirstByTitleId(titleId, titleInfo))
			continue;

		if (auto iconIt = m_iconCache.find(titleId); iconIt != m_iconCache.end())
		{
			auto &icon = iconIt->second;
			if (m_onIconLoaded)
				m_onIconLoaded->onIconLoaded(titleId, icon.intColors(), icon.m_width, icon.m_height);
			continue;
		}

		std::string tempMountPath = TitleInfo::GetUniqueTempMountingPath();
		if (!titleInfo.Mount(tempMountPath, "", FSC_PRIORITY_BASE))
			continue;
		auto tgaData = fsc_extractFile((tempMountPath + "/meta/iconTex.tga").c_str());
		if (tgaData && tgaData->size() > 16)
		{
			Image image(tgaData.value());
			if (image.isOk())
			{
				if (m_onIconLoaded)
					m_onIconLoaded->onIconLoaded(titleId, image.intColors(), image.m_width, image.m_height);
				m_iconCache.emplace(titleId, std::move(image));
			}
		}
		else
		{
			cemuLog_log(LogType::Force, "Failed to load icon for title {:016x}", titleId);
		}
		titleInfo.Unmount(tempMountPath);
	}
}

void GameIconLoader::setOnIconLoaded(const std::shared_ptr<GameIconLoadedCallback> &onIconLoaded)
{
	{
		std::lock_guard lock(m_threadMutex);
		m_onIconLoaded = onIconLoaded;
	}
	m_condVar.notify_one();
};
