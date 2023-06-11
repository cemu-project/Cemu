#pragma once

#include "Cafe/IOSU/PDM/iosu_pdm.h"
#include "Cafe/TitleList/TitleId.h"
#include "Cafe/TitleList/TitleList.h"

struct Game {
    std::string name;
    uint32 secondsPlayed;
    uint16 dlc;
    uint16 version;
    TitleId titleId;
    CafeConsoleRegion region;
};

class GameTitleLoader {
    std::atomic_bool m_continueLoading = true;
    std::mutex m_threadMutex;
    std::condition_variable m_condVar;
    std::thread m_loaderThread;
    std::deque<TitleId> m_titlesToLoad;
    std::optional<uint64> m_callbackIdTitleList;
    std::vector<TitleId> m_titleIds;
    std::map<TitleId, Game> m_gameInfos;
    std::map<TitleId, std::string> m_name_cache;
    std::function<void(const Game &)> m_onTitleLoaded = nullptr;
public:
    GameTitleLoader();

    void queueTitle(TitleId titleId);

    void setOnTitleLoaded(const std::function<void(const Game &)> &onTitleLoaded);

    void reloadGameTitles();

    ~GameTitleLoader();

    void titleRefresh(TitleId titleId);

private:
    void loadGameTitles();

    std::string GetNameByTitleId(uint64 titleId);

    void registerCallback();

    void HandleTitleListCallback(CafeTitleListCallbackEvent *evt);
};
