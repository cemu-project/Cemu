#pragma once

#include "Cafe/TitleList/TitleId.h"
#include "Image.h"

class GameIconLoader {
    std::atomic_bool m_continueLoading = true;
    std::condition_variable m_condVar;
    std::thread m_loaderThread;
    std::mutex m_threadMutex;
    std::deque <TitleId> m_iconsToLoad;
    std::map <TitleId, Image> m_iconCache;
    std::function<void(TitleId, const Image &)> m_onIconLoaded = nullptr;

    void loadGameIcons();

public:
    GameIconLoader();

    ~GameIconLoader();

    void setOnIconLoaded(const std::function<void(TitleId, const Image &)> &onIconLoaded);

    void requestIcon(TitleId titleId);
};
