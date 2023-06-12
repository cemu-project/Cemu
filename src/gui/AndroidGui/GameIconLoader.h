#pragma once

#include "Cafe/TitleList/TitleId.h"
#include "Image.h"

class GameIconLoadedCallback {
public:
    virtual void onIconLoaded(TitleId titleId, const Image &iconData) = 0;
};

class GameIconLoader {
    std::atomic_bool m_continueLoading = true;
    std::condition_variable m_condVar;
    std::thread m_loaderThread;
    std::mutex m_threadMutex;
    std::deque <TitleId> m_iconsToLoad;
    std::map <TitleId, Image> m_iconCache;
    std::shared_ptr <GameIconLoadedCallback> m_onIconLoaded = nullptr;
public:
    GameIconLoader();

    ~GameIconLoader();

    void setOnIconLoaded(const std::shared_ptr <GameIconLoadedCallback> &onIconLoaded);

    void requestIcon(TitleId titleId);

private:
    void loadGameIcons();
};
