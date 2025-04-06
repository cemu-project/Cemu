#include "TitleList.h"
#include "Common/FileStream.h"

#include "util/helpers/helpers.h"

#include <zarchive/zarchivereader.h>

bool sTLInitialized{ false };
fs::path sTLCacheFilePath;

// lists for tracking known titles
// note: The list may only contain titles with valid meta data (except for certain system titles). Entries loaded from the cache may not have been parsed yet, but they will use a cached value for titleId and titleVersion
std::mutex sTLMutex;
std::vector<TitleInfo*> sTLList;
std::vector<TitleInfo*> sTLListPending;
std::unordered_multimap<uint64, TitleInfo*> sTLMap;
bool sTLCacheDirty{false};

// paths
fs::path sTLMLCPath;
std::vector<fs::path> sTLScanPaths;

// worker
std::thread sTLRefreshWorker;
bool sTLRefreshWorkerActive{false};
std::atomic_uint32_t sTLRefreshRequests{};
std::atomic_bool sTLIsScanMandatory{ false };

// callback list
struct TitleListCallbackEntry 
{
	TitleListCallbackEntry(void(*cb)(CafeTitleListCallbackEvent* evt, void* ctx), void* ctx, uint64 uniqueId) :
		cb(cb), ctx(ctx), uniqueId(uniqueId) {};
	void (*cb)(CafeTitleListCallbackEvent* evt, void* ctx);
	void* ctx;
	uint64 uniqueId;
};

std::vector<TitleListCallbackEntry> sTLCallbackList;

void CafeTitleList::Initialize(const fs::path cacheXmlFile)
{
	std::unique_lock _lock(sTLMutex);
	sTLInitialized = true;
	sTLCacheFilePath = cacheXmlFile;
	LoadCacheFile();
}

void CafeTitleList::LoadCacheFile()
{
	sTLIsScanMandatory = true;
	cemu_assert_debug(sTLInitialized);
	cemu_assert_debug(sTLList.empty());
	auto xmlData = FileStream::LoadIntoMemory(sTLCacheFilePath);
	if (!xmlData)
		return;
	pugi::xml_document doc;
	if (!doc.load_buffer_inplace(xmlData->data(), xmlData->size(), pugi::parse_default, pugi::xml_encoding::encoding_utf8))
		return;
	auto titleListNode = doc.child("title_list");
	pugi::xml_node itNode = titleListNode.first_child();
	for (const auto& titleInfoNode : doc.child("title_list"))
	{
		TitleId titleId;
		if( !TitleIdParser::ParseFromStr(titleInfoNode.attribute("titleId").as_string(), titleId))
			continue;
		uint16 titleVersion = titleInfoNode.attribute("version").as_uint();
		uint32 sdkVersion = titleInfoNode.attribute("sdk_version").as_uint();
		TitleInfo::TitleDataFormat format = (TitleInfo::TitleDataFormat)ConvertString<uint32>(titleInfoNode.child_value("format"));
		CafeConsoleRegion region = (CafeConsoleRegion)ConvertString<uint32>(titleInfoNode.child_value("region"));
		std::string name = titleInfoNode.child_value("name");
		std::string path = titleInfoNode.child_value("path");
		std::string sub_path = titleInfoNode.child_value("sub_path");
		uint32 group_id = ConvertString<uint32>(titleInfoNode.attribute("group_id").as_string(), 16);
		uint32 app_type = ConvertString<uint32>(titleInfoNode.attribute("app_type").as_string(), 16);

		TitleInfo::CachedInfo cacheEntry;
		cacheEntry.titleId = titleId;
		cacheEntry.titleVersion = titleVersion;
		cacheEntry.sdkVersion = sdkVersion;
		cacheEntry.titleDataFormat = format;
		cacheEntry.region = region;
		cacheEntry.titleName = std::move(name);
		cacheEntry.path = _utf8ToPath(path);
		cacheEntry.subPath = std::move(sub_path);
		cacheEntry.group_id = group_id;
		cacheEntry.app_type = app_type;

		TitleInfo* ti = new TitleInfo(cacheEntry);
		if (!ti->IsValid())
		{
			cemuLog_log(LogType::Force, "Title cache contained invalid title");
			delete ti;
			continue;
		}
		AddTitle(ti);
	}
	sTLIsScanMandatory = false;
}

void CafeTitleList::StoreCacheFile()
{
	cemu_assert_debug(sTLInitialized);
	if (sTLCacheFilePath.empty())
		return;
	std::unique_lock _lock(sTLMutex);

	pugi::xml_document doc;
	auto declarationNode = doc.append_child(pugi::node_declaration);
	declarationNode.append_attribute("version") = "1.0";
	declarationNode.append_attribute("encoding") = "UTF-8";
	auto title_list_node = doc.append_child("title_list");

	for (auto& tiIt : sTLList)
	{
		TitleInfo::CachedInfo info = tiIt->MakeCacheEntry();
		auto titleInfoNode = title_list_node.append_child("title");
		titleInfoNode.append_attribute("titleId").set_value(fmt::format("{:016x}", info.titleId).c_str());
		titleInfoNode.append_attribute("version").set_value(fmt::format("{:}", info.titleVersion).c_str());
		titleInfoNode.append_attribute("sdk_version").set_value(fmt::format("{:}", info.sdkVersion).c_str());
		titleInfoNode.append_attribute("group_id").set_value(fmt::format("{:08x}", info.group_id).c_str());
		titleInfoNode.append_attribute("app_type").set_value(fmt::format("{:08x}", info.app_type).c_str());
		titleInfoNode.append_child("region").append_child(pugi::node_pcdata).set_value(fmt::format("{}", (uint32)info.region).c_str());
		titleInfoNode.append_child("name").append_child(pugi::node_pcdata).set_value(info.titleName.c_str());
		titleInfoNode.append_child("format").append_child(pugi::node_pcdata).set_value(fmt::format("{}", (uint32)info.titleDataFormat).c_str());
		titleInfoNode.append_child("path").append_child(pugi::node_pcdata).set_value(_pathToUtf8(info.path).c_str());
		if(!info.subPath.empty())
			titleInfoNode.append_child("sub_path").append_child(pugi::node_pcdata).set_value(_pathToUtf8(info.subPath).c_str());
	}

	fs::path tmpPath = fs::path(sTLCacheFilePath.parent_path()).append(fmt::format("{}__tmp", _pathToUtf8(sTLCacheFilePath.filename())));
	std::ofstream fileOut(tmpPath, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!fileOut.is_open())
	{
		cemuLog_log(LogType::Force, "Unable to store title list in {}", _pathToUtf8(tmpPath));
		return;
	}
	doc.save(fileOut, " ", 1, pugi::xml_encoding::encoding_utf8);
	fileOut.flush();
	fileOut.close();

	std::error_code ec;
	fs::rename(tmpPath, sTLCacheFilePath, ec);
}

void CafeTitleList::ClearScanPaths()
{
	std::unique_lock _lock(sTLMutex);
	sTLScanPaths.clear();
}

void CafeTitleList::AddScanPath(fs::path path)
{	
	std::unique_lock _lock(sTLMutex);
	sTLScanPaths.emplace_back(path);
}

void CafeTitleList::SetMLCPath(fs::path path)
{
	std::unique_lock _lock(sTLMutex);
	std::error_code ec;
	if (!fs::is_directory(path, ec))
	{
		cemuLog_log(LogType::Force, "MLC set to invalid path: {}", _pathToUtf8(path));
		return;
	}
	sTLMLCPath = path;
}

void CafeTitleList::Refresh()
{
	std::unique_lock _lock(sTLMutex);
	cemu_assert_debug(sTLInitialized);
	sTLRefreshRequests++;
	if (!sTLRefreshWorkerActive)
	{
		if (sTLRefreshWorker.joinable())
			sTLRefreshWorker.join();
		sTLRefreshWorkerActive = true;
		sTLRefreshWorker = std::thread(RefreshWorkerThread);
	}
	sTLIsScanMandatory = false;
}

bool CafeTitleList::IsScanning()
{
	std::unique_lock _lock(sTLMutex);
	return sTLRefreshWorkerActive;
}

void CafeTitleList::WaitForMandatoryScan()
{
	if (!sTLIsScanMandatory)
		return;
	while (IsScanning())
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void _RemoveTitleFromMultimap(TitleInfo* titleInfo)
{
	auto mapRange = sTLMap.equal_range(titleInfo->GetAppTitleId());
	for (auto mapIt = mapRange.first; mapIt != mapRange.second; ++mapIt)
	{
		if (mapIt->second == titleInfo)
		{
			sTLMap.erase(mapIt);
			return;
		}
	}
	cemu_assert_suspicious();
}

// check if path is a valid title and if it is, permanently add it to the title list
// in the special case that path points to a WUA file, all contained titles will be added
void CafeTitleList::AddTitleFromPath(fs::path path)
{
	if (path.has_extension() && boost::iequals(_pathToUtf8(path.extension()), ".wua"))
	{
		ZArchiveReader* zar = ZArchiveReader::OpenFromFile(path);
		if (!zar)
		{
			cemuLog_log(LogType::Force, "Found {} but it is not a valid Wii U archive file", _pathToUtf8(path));
			return;
		}
		// enumerate all contained titles
		ZArchiveNodeHandle rootDir = zar->LookUp("", false, true);
		cemu_assert(rootDir != ZARCHIVE_INVALID_NODE);
		for (uint32 i = 0; i < zar->GetDirEntryCount(rootDir); i++)
		{
			ZArchiveReader::DirEntry dirEntry;
			if( !zar->GetDirEntry(rootDir, i, dirEntry) )
				continue;
			if(!dirEntry.isDirectory)
				continue;
			TitleId parsedId;
			uint16 parsedVersion;
			if (!TitleInfo::ParseWuaTitleFolderName(dirEntry.name, parsedId, parsedVersion))
			{
				cemuLog_log(LogType::Force, "Invalid title directory in {}: \"{}\"", _pathToUtf8(path), dirEntry.name);
				continue;
			}
			// valid subdirectory
			TitleInfo* titleInfo = new TitleInfo(path, dirEntry.name);
			if (titleInfo->IsValid())
				AddDiscoveredTitle(titleInfo);
			else
				delete titleInfo;
		}
		delete zar;
		return;
	}
	TitleInfo* titleInfo = new TitleInfo(path);
	if (titleInfo->IsValid())
		AddDiscoveredTitle(titleInfo);
	else
		delete titleInfo;
}

bool CafeTitleList::RefreshWorkerThread()
{
	SetThreadName("TitleListWorker");
	while (sTLRefreshRequests.load())
	{
		sTLRefreshRequests.store(0);
		// create copies of all the paths
		sTLMutex.lock();
		fs::path mlcPath = sTLMLCPath;
		std::vector<fs::path> gamePaths = sTLScanPaths;
		// remember the current list of known titles
		// during the scanning process we will erase matches from the pending list
		// at the end of scanning, we can then use this list to identify and remove any titles that are no longer discoverable
		sTLListPending = sTLList;
		sTLMutex.unlock();
		// scan game paths
		for (auto& it : gamePaths)
			ScanGamePath(it);
		// scan MLC
		if (!mlcPath.empty())
		{
			std::error_code ec;
			for (auto& it : fs::directory_iterator(mlcPath / "usr/title", ec))
			{
				if (!it.is_directory(ec))
					continue;
				ScanMLCPath(it.path());
			}
			ScanMLCPath(mlcPath / "sys/title/00050010");
			ScanMLCPath(mlcPath / "sys/title/00050030");
		}

		// remove any titles that are still pending
		for (auto& itPending : sTLListPending)
		{
			_RemoveTitleFromMultimap(itPending);
			std::erase(sTLList, itPending);
		}
		// send notifications for removed titles, but only if there exists no other title with the same titleId and version
		if (!sTLListPending.empty())
			sTLCacheDirty = true;
		for (auto& itPending : sTLListPending)
		{
			CafeTitleListCallbackEvent evt;
			evt.eventType = CafeTitleListCallbackEvent::TYPE::TITLE_REMOVED;
			evt.titleInfo = itPending;
			for (auto& it : sTLCallbackList)
				it.cb(&evt, it.ctx);
			delete itPending;
		}
		sTLListPending.clear();
	}
	sTLMutex.lock();
	sTLRefreshWorkerActive = false;
	// send notification that scanning finished
	CafeTitleListCallbackEvent evt;
	evt.eventType = CafeTitleListCallbackEvent::TYPE::SCAN_FINISHED;
	evt.titleInfo = nullptr;
	for (auto& it : sTLCallbackList)
		it.cb(&evt, it.ctx);
	sTLMutex.unlock();
	if (sTLCacheDirty)
	{
		StoreCacheFile();
		sTLCacheDirty = false;
	}
	return true;
}

bool _IsKnownFileNameOrExtension(const fs::path& path)
{
	std::string fileExtension = _pathToUtf8(path.extension());
	for (auto& it : fileExtension)
		it = _ansiToLower(it);
	if(fileExtension == ".tmd")
	{
		// must be "title.tmd"
		std::string fileName = _pathToUtf8(path.filename());
		for (auto& it : fileName)
			it = _ansiToLower(it);
		return fileName == "title.tmd";
	}
	return
		fileExtension == ".wud" ||
		fileExtension == ".wux" ||
		fileExtension == ".iso" ||
		fileExtension == ".wua" ||
		fileExtension == ".wuhb";
	// note: To detect extracted titles with RPX we rely on the presence of the content,code,meta directory structure
}

void CafeTitleList::ScanGamePath(const fs::path& path)
{
	// scan the whole directory first to determine if this is a title folder
	std::vector<fs::path> filesInDirectory;
	std::vector<fs::path> dirsInDirectory;
	bool hasContentFolder = false, hasCodeFolder = false, hasMetaFolder = false;
	std::error_code ec;
	for (auto& it : fs::directory_iterator(path, ec))
	{		
		if (it.is_regular_file(ec))
		{
			filesInDirectory.emplace_back(it.path());
		}
		else if (it.is_directory(ec))
		{
			dirsInDirectory.emplace_back(it.path());
			std::string dirName = _pathToUtf8(it.path().filename());
			if (boost::iequals(dirName, "content"))
				hasContentFolder = true;
			else if (boost::iequals(dirName, "code"))
				hasCodeFolder = true;
			else if (boost::iequals(dirName, "meta"))
				hasMetaFolder = true;
		}
	}
	// always check individual files
	for (auto& it : filesInDirectory)
	{
		// since checking individual files is slow, we limit it to known file names or extensions
		if (!it.has_extension())
			continue;
		if (!_IsKnownFileNameOrExtension(it))
			continue;
		AddTitleFromPath(it);
	}
	// is the current directory a title folder?
	if (hasContentFolder && hasCodeFolder && hasMetaFolder)
	{
		// verify if this folder is a valid title
		TitleInfo* titleInfo = new TitleInfo(path);
		if (titleInfo->IsValid())
			AddDiscoveredTitle(titleInfo);
		else
			delete titleInfo;
		// if there are other folders besides content/code/meta then traverse those
		if (dirsInDirectory.size() > 3)
		{
			for (auto& it : dirsInDirectory)
			{
				std::string dirName = _pathToUtf8(it.filename());
				if (!boost::iequals(dirName, "content") &&
					!boost::iequals(dirName, "code") &&
					!boost::iequals(dirName, "meta"))
					ScanGamePath(it);
			}
		}
	}
	else
	{
		// scan subdirectories
		for (auto& it : dirsInDirectory)
			ScanGamePath(it);
	}
}

void CafeTitleList::ScanMLCPath(const fs::path& path)
{
	std::error_code ec;
	for (auto& it : fs::directory_iterator(path, ec))
	{
		if (!it.is_directory())
			continue;
		// only scan directories which match the title id naming scheme
		std::string dirName = _pathToUtf8(it.path().filename());
		if(dirName.size() != 8)
			continue;
		bool containsNoHexCharacter = false;
		for (auto& it : dirName)
		{
			if(it >= 'A' && it <= 'F' ||
				it >= 'a' && it <= 'f' ||
				it >= '0' && it <= '9')
				continue;
			containsNoHexCharacter = true;
			break;
		}
		if(containsNoHexCharacter)
			continue;

		if (fs::is_directory(it.path() / "code", ec) &&
			fs::is_directory(it.path() / "content", ec) &&
			fs::is_directory(it.path() / "meta", ec))
		{
			TitleInfo* titleInfo = new TitleInfo(it);
			if (titleInfo->IsValid() && titleInfo->ParseXmlInfo())
				AddDiscoveredTitle(titleInfo);
			else
				delete titleInfo;
		}
	}
}

void CafeTitleList::AddDiscoveredTitle(TitleInfo* titleInfo)
{
	cemu_assert_debug(titleInfo->ParseXmlInfo());
	std::unique_lock _lock(sTLMutex);
	// remove from pending list
	auto pendingIt = std::find_if(sTLListPending.begin(), sTLListPending.end(), [titleInfo](const TitleInfo* it) { return it->IsEqualByLocation(*titleInfo); });
	if (pendingIt != sTLListPending.end())
		sTLListPending.erase(pendingIt);
	AddTitle(titleInfo);	
}

void CafeTitleList::AddTitle(TitleInfo* titleInfo)
{
	// check if title is already known
	if (titleInfo->IsCached())
	{
		bool isKnown = std::any_of(sTLList.cbegin(), sTLList.cend(), [&titleInfo](const TitleInfo* ti) { return titleInfo->IsEqualByLocation(*ti); });
		if (isKnown)
		{
			delete titleInfo;
			return;
		}
	}
	else
	{
		auto it = std::find_if(sTLList.begin(), sTLList.end(), [titleInfo](const TitleInfo* it) { return it->IsEqualByLocation(*titleInfo); });
		if (it != sTLList.end())
		{
			if ((*it)->IsCached())
			{
				// replace cached entry with newly parsed title
				TitleInfo* deletedInfo = *it;
				sTLList.erase(it);
				_RemoveTitleFromMultimap(deletedInfo);
				delete deletedInfo;
			}
			else
			{
				// title already known
				delete titleInfo;
				return;
			}
		}
	}
	sTLList.emplace_back(titleInfo);
	sTLMap.emplace(titleInfo->GetAppTitleId(), titleInfo);
	// send out notification
	CafeTitleListCallbackEvent evt;
	evt.eventType = CafeTitleListCallbackEvent::TYPE::TITLE_DISCOVERED;
	evt.titleInfo = titleInfo;
	for (auto& it : sTLCallbackList)
		it.cb(&evt, it.ctx);
	sTLCacheDirty = true;
}

uint64 CafeTitleList::RegisterCallback(void(*cb)(CafeTitleListCallbackEvent* evt, void* ctx), void* ctx)
{
	static std::atomic<uint64_t> sCallbackIdGen = 1;
	uint64 id = sCallbackIdGen.fetch_add(1);
	std::unique_lock _lock(sTLMutex);
	sTLCallbackList.emplace_back(cb, ctx, id);
	// immediately notify of all known titles
	for (auto& it : sTLList)
	{
		CafeTitleListCallbackEvent evt;
		evt.eventType = CafeTitleListCallbackEvent::TYPE::TITLE_DISCOVERED;
		evt.titleInfo = it;
		cb(&evt, ctx);
	}
	// if not scanning then send out scan finished notification
	if (!sTLRefreshWorkerActive)
	{
		CafeTitleListCallbackEvent evt;
		evt.eventType = CafeTitleListCallbackEvent::TYPE::SCAN_FINISHED;
		evt.titleInfo = nullptr;
		for (auto& it : sTLCallbackList)
			it.cb(&evt, it.ctx);
	}
	return id;
}

void CafeTitleList::UnregisterCallback(uint64 id)
{
	std::unique_lock _lock(sTLMutex);
	auto it = std::find_if(sTLCallbackList.begin(), sTLCallbackList.end(), [id](auto& e) { return e.uniqueId == id; });
	cemu_assert(it != sTLCallbackList.end()); // must be a valid callback
	sTLCallbackList.erase(it);
}

bool CafeTitleList::HasTitle(TitleId titleId, uint16& versionOut)
{
	// todo - optimize?
	bool matchFound = false;
	versionOut = 0;
	std::unique_lock _lock(sTLMutex);
	for (auto& it : sTLList)
	{
		if (it->GetAppTitleId() == titleId)
		{
			uint16 titleVersion = it->GetAppTitleVersion();
			if (titleVersion > versionOut)
				versionOut = titleVersion;
			matchFound = true;
		}
	}
	return matchFound;
}

bool CafeTitleList::HasTitleAndVersion(TitleId titleId, uint16 version)
{
	std::unique_lock _lock(sTLMutex);
	for (auto& it : sTLList)
	{
		if (it->GetAppTitleId() == titleId && it->GetAppTitleVersion() == version)
			return true;
	}
	return false;
}

std::vector<TitleId> CafeTitleList::GetAllTitleIds()
{
	std::unordered_set<TitleId> visitedTitleIds;
	std::unique_lock _lock(sTLMutex);
	std::vector<TitleId> titleIds;
	titleIds.reserve(sTLList.size());
	for (auto& it : sTLList)
	{
		TitleId tid = it->GetAppTitleId();
		if (visitedTitleIds.find(tid) != visitedTitleIds.end())
			continue;
		titleIds.emplace_back(tid);
		visitedTitleIds.emplace(tid);
	}
	return titleIds;
}

std::span<TitleInfo*> CafeTitleList::AcquireInternalList()
{
	sTLMutex.lock();
	return { sTLList.data(), sTLList.size() };
}

void CafeTitleList::ReleaseInternalList()
{
	sTLMutex.unlock();
}

bool CafeTitleList::GetFirstByTitleId(TitleId titleId, TitleInfo& titleInfoOut)
{
	std::unique_lock _lock(sTLMutex);
	auto it = sTLMap.find(titleId);
	if (it != sTLMap.end())
	{
		cemu_assert_debug(it->first == titleId);
		titleInfoOut = *it->second;
		return true;
	}
	return false;
}

// takes update or AOC title id and returns the title id of the associated base title
// this can fail if trying to translate an AOC title id without having the base title meta information
bool CafeTitleList::FindBaseTitleId(TitleId titleId, TitleId& titleIdBaseOut)
{
	titleId = TitleIdParser::MakeBaseTitleId(titleId);

	// aoc to base
	// todo - this requires scanning all base titles and their updates to see if they reference this title id
	// for now we assume there is a direct match of ids
	if (((titleId >> 32) & 0xFF) == 0x0C)
	{
		titleId &= ~0xFF00000000;
		titleId |= 0x0000000000;
	}
	titleIdBaseOut = titleId;
	return true;
}

GameInfo2 CafeTitleList::GetGameInfo(TitleId titleId)
{
	GameInfo2 gameInfo;

	// find base title id
	uint64 baseTitleId;
	if (!FindBaseTitleId(titleId, baseTitleId))
	{
		cemu_assert_suspicious();
	}
	// determine if an optional update title id exists
	TitleIdParser tip(baseTitleId);
	bool hasSeparateUpdateTitleId = tip.CanHaveSeparateUpdateTitleId();
	uint64 updateTitleId = 0;
	if (hasSeparateUpdateTitleId)
		updateTitleId = tip.GetSeparateUpdateTitleId();	
	// scan the title list for base and update
	std::unique_lock _lock(sTLMutex);
	for (auto& it : sTLList)
	{
		TitleId appTitleId = it->GetAppTitleId();
		if (appTitleId == baseTitleId)
		{
			gameInfo.SetBase(*it);
		}
		if (hasSeparateUpdateTitleId && appTitleId == updateTitleId)
		{
			gameInfo.SetUpdate(*it);
		}
	}

	// if this title can have AOC content then do a second scan
	// todo - get a list of all AOC title ids from the base/update meta information
	// for now we assume there is a direct match between the base titleId and the aoc titleId
	if (tip.CanHaveSeparateUpdateTitleId())
	{
		uint64 aocTitleId = baseTitleId | 0xC00000000;
		for (auto& it : sTLList)
		{
			TitleId appTitleId = it->GetAppTitleId();
			if (appTitleId == aocTitleId)
			{
				gameInfo.AddAOC(*it); // stores the AOC with the highest title version
			}
		}
	}
	return gameInfo;
}

TitleInfo CafeTitleList::GetTitleInfoByUID(uint64 uid)
{
	TitleInfo titleInfo;
	std::unique_lock _lock(sTLMutex);
	for (auto& it : sTLList)
	{
		if (it->GetUID() == uid)
		{
			titleInfo = *it;
			break;
		}
	}
	return titleInfo;
}