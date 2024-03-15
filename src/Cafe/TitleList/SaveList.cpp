#include "SaveList.h"
#include <charconv>
#include <util/helpers/helpers.h>

std::mutex sSLMutex;
fs::path sSLMLCPath;

std::vector<SaveInfo*> sSLList;

// callback list
struct SaveListCallbackEntry
{
	SaveListCallbackEntry(void(*cb)(CafeSaveListCallbackEvent* evt, void* ctx), void* ctx, uint64 uniqueId) :
		cb(cb), ctx(ctx), uniqueId(uniqueId) {};
	void (*cb)(CafeSaveListCallbackEvent* evt, void* ctx);
	void* ctx;
	uint64 uniqueId;
};
std::vector<SaveListCallbackEntry> sSLCallbackList;

// worker thread
std::atomic_bool sSLWorkerThreadActive{false};


void CafeSaveList::Initialize()
{

}

void CafeSaveList::SetMLCPath(fs::path mlcPath)
{
	std::unique_lock _lock(sSLMutex);
	sSLMLCPath = mlcPath;
}

void CafeSaveList::Refresh()
{
	std::unique_lock _lock(sSLMutex);
	if (sSLWorkerThreadActive)
		return;
	sSLWorkerThreadActive = true;
	std::thread t(RefreshThreadWorker);
	t.detach();
}

void CafeSaveList::RefreshThreadWorker()
{
	SetThreadName("SaveListWorker");
	// clear save list
	for (auto& itSaveInfo : sSLList)
	{
		for (auto& it : sSLCallbackList)
		{
			CafeSaveListCallbackEvent evt;
			evt.eventType = CafeSaveListCallbackEvent::TYPE::SAVE_REMOVED;
			evt.saveInfo = itSaveInfo;
			it.cb(&evt, it.ctx);
		}
		delete itSaveInfo;
	}
	sSLList.clear();

	sSLMutex.lock();
	fs::path mlcPath = sSLMLCPath;
	sSLMutex.unlock();
	std::error_code ec;
	for (auto it_titleHigh : fs::directory_iterator(mlcPath / "usr/save", ec))
	{
		if(!it_titleHigh.is_directory(ec))
			continue;
		std::string dirName = _pathToUtf8(it_titleHigh.path().filename());
		if(dirName.empty())
			continue;		
		uint32 titleIdHigh;
		std::from_chars_result r = std::from_chars(dirName.data(), dirName.data() + dirName.size(), titleIdHigh, 16);
		if (r.ec != std::errc())
			continue;
		fs::path tmp = it_titleHigh.path();
		for (auto it_titleLow : fs::directory_iterator(tmp, ec))
		{
			if (!it_titleLow.is_directory(ec))
				continue;
			dirName = _pathToUtf8(it_titleLow.path().filename());
			if (dirName.empty())
				continue;
			uint32 titleIdLow;
			std::from_chars_result r = std::from_chars(dirName.data(), dirName.data() + dirName.size(), titleIdLow, 16);
			if (r.ec != std::errc())
				continue;
			// found save
			TitleId titleId = (uint64)titleIdHigh << 32 | (uint64)titleIdLow;
			SaveInfo* saveInfo = new SaveInfo(titleId);
			if (saveInfo->IsValid())
				DiscoveredSave(saveInfo);
			else
				delete saveInfo;
		}
	}
	sSLMutex.lock();
	sSLWorkerThreadActive = false;
	sSLMutex.unlock();
	// send notification about finished scan
	for (auto& it : sSLCallbackList)
	{
		CafeSaveListCallbackEvent evt;
		evt.eventType = CafeSaveListCallbackEvent::TYPE::SCAN_FINISHED;
		evt.saveInfo = nullptr;
		it.cb(&evt, it.ctx);
	}
}

void CafeSaveList::DiscoveredSave(SaveInfo* saveInfo)
{
	if (!saveInfo->ParseMetaData())
	{
		delete saveInfo;
		return;
	}
	std::unique_lock _lock(sSLMutex);
	auto it = std::find_if(sSLList.begin(), sSLList.end(), [saveInfo](const SaveInfo* rhs) { return saveInfo->GetTitleId() == rhs->GetTitleId(); });
	if (it != sSLList.end())
	{
		// save already known
		delete saveInfo;
		return;
	}
	sSLList.emplace_back(saveInfo);
	// send notification
	for (auto& it : sSLCallbackList)
	{
		CafeSaveListCallbackEvent evt;
		evt.eventType = CafeSaveListCallbackEvent::TYPE::SAVE_DISCOVERED;
		evt.saveInfo = saveInfo;
		it.cb(&evt, it.ctx);
	}
}

uint64 CafeSaveList::RegisterCallback(void(*cb)(CafeSaveListCallbackEvent* evt, void* ctx), void* ctx)
{
	static std::atomic<uint64_t> sCallbackIdGen = 1;
	uint64 id = sCallbackIdGen.fetch_add(1);
	std::unique_lock _lock(sSLMutex);
	sSLCallbackList.emplace_back(cb, ctx, id);
	// immediately notify of all known titles
	for (auto& it : sSLList)
	{
		CafeSaveListCallbackEvent evt;
		evt.eventType = CafeSaveListCallbackEvent::TYPE::SAVE_DISCOVERED;
		evt.saveInfo = it;
		cb(&evt, ctx);
	}
	// if not scanning then send out scan finished notification
	if (!sSLWorkerThreadActive)
	{
		CafeSaveListCallbackEvent evt;
		evt.eventType = CafeSaveListCallbackEvent::TYPE::SCAN_FINISHED;
		evt.saveInfo = nullptr;
		for (auto& it : sSLCallbackList)
			it.cb(&evt, it.ctx);
	}
	return id;
}

void CafeSaveList::UnregisterCallback(uint64 id)
{
	std::unique_lock _lock(sSLMutex);
	auto it = std::find_if(sSLCallbackList.begin(), sSLCallbackList.end(), [id](auto& e) { return e.uniqueId == id; });
	cemu_assert(it != sSLCallbackList.end());
	sSLCallbackList.erase(it);
}

SaveInfo CafeSaveList::GetSaveByTitleId(TitleId titleId)
{
	std::unique_lock _lock(sSLMutex);
	for (auto& it : sSLList)
		if (it->GetTitleId() == titleId)
			return *it;
	return {};
}