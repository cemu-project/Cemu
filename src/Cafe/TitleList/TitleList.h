#pragma once

#include "TitleInfo.h"
#include "GameInfo.h"

struct CafeTitleListCallbackEvent
{
	enum class TYPE
	{
		TITLE_DISCOVERED,
		TITLE_REMOVED,
		SCAN_FINISHED,
	};
	TYPE eventType;
	TitleInfo* titleInfo;
};

class CafeTitleList
{
public:

	static void Initialize(const fs::path cacheXmlFile);
	static void LoadCacheFile();
	static void StoreCacheFile();

	static void ClearScanPaths();
	static void AddScanPath(fs::path path);
	static void SetMLCPath(fs::path path);
	static void Refresh(); // scan all paths
	static bool IsScanning(); // returns true if async refresh is currently active
	static void WaitForMandatoryScan(); // wait for current scan result if no cached info is available
	static void AddTitleFromPath(fs::path path);

	static uint64 RegisterCallback(void(*cb)(CafeTitleListCallbackEvent* evt, void* ctx), void* ctx); // on register, the callback will be invoked for every already known title
	static void UnregisterCallback(uint64 id);

	// utility functions
	static bool HasTitle(TitleId titleId, uint16& versionOut);
	static bool HasTitleAndVersion(TitleId titleId, uint16 version);
	static std::vector<TitleId> GetAllTitleIds();
	static bool GetFirstByTitleId(TitleId titleId, TitleInfo& titleInfoOut);
	static bool FindBaseTitleId(TitleId titleId, TitleId& titleIdBaseOut);

	static std::span<TitleInfo*> AcquireInternalList();
	static void ReleaseInternalList();

	static GameInfo2 GetGameInfo(TitleId titleId);
	static TitleInfo GetTitleInfoByUID(uint64 uid);

private:
	static bool RefreshWorkerThread();
	static void ScanGamePath(const fs::path& path);
	static void ScanMLCPath(const fs::path& path);

	static void AddDiscoveredTitle(TitleInfo* titleInfo);
	static void AddTitle(TitleInfo* titleInfo);
};