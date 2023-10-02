#pragma once

#include "config/CemuConfig.h"
#include "TitleInfo.h"
#include "config/ActiveSettings.h"

class GameInfo2
{
public:
	~GameInfo2()
	{
		m_base.UnmountAll();
		m_update.UnmountAll();
		for (auto& it : m_aoc)
			it.UnmountAll();
	}

	bool IsValid() const
	{
		return m_base.IsValid(); // at least the base must be valid for this to be a runnable title
	}

	bool IsSystemDataTitle() const
	{
		return m_base.IsSystemDataTitle();
	}

	void SetBase(const TitleInfo& titleInfo)
	{
		if (IsPrioritizedVersionOrFormat(m_base, titleInfo))
			m_base = titleInfo;
	}

	void SetUpdate(const TitleInfo& titleInfo)
	{
		if (IsPrioritizedVersionOrFormat(m_update, titleInfo))
			m_update = titleInfo;
	}

	bool HasUpdate() const
	{
		return m_update.IsValid();
	}

	void AddAOC(const TitleInfo& titleInfo)
	{
		TitleId aocTitleId = titleInfo.GetAppTitleId();
		uint16 aocVersion = titleInfo.GetAppTitleVersion();
		auto it = std::find_if(m_aoc.begin(), m_aoc.end(), [aocTitleId](const TitleInfo& rhs) { return rhs.GetAppTitleId() == aocTitleId; });
		if (it != m_aoc.end())
		{
			if (!IsPrioritizedVersionOrFormat(*it, titleInfo))
				return;
			m_aoc.erase(it);
		}
		m_aoc.emplace_back(titleInfo);
	}

	bool HasAOC() const
	{
		return !m_aoc.empty();
	}

	TitleInfo& GetBase()
	{
		return m_base;
	}

	TitleInfo& GetUpdate()
	{
		return m_update;
	}

	std::span<TitleInfo> GetAOC()
	{
		return m_aoc;
	}

	TitleId GetBaseTitleId()
	{
		cemu_assert_debug(m_base.IsValid());
		return m_base.GetAppTitleId();
	}

	std::string GetTitleName()
	{
		cemu_assert_debug(m_base.IsValid());
		return m_base.GetMetaTitleName(); // long name
	}

	uint16 GetVersion() const
	{
		if (m_update.IsValid())
			return m_update.GetAppTitleVersion();
		return m_base.GetAppTitleVersion();
	}

	uint32 GetSDKVersion() const
	{
		if (m_update.IsValid())
			return m_update.GetAppSDKVersion();
		return m_base.GetAppSDKVersion();
	}

	CafeConsoleRegion GetRegion() const
	{
		if (m_update.IsValid())
			return m_update.GetMetaRegion();
		return m_base.GetMetaRegion();
	}

	uint16 GetAOCVersion() const
	{
		if (m_aoc.empty())
			return 0;
		return m_aoc.front().GetAppTitleVersion();
	}

	fs::path GetSaveFolder()
	{
		return ActiveSettings::GetMlcPath(fmt::format("usr/save/{:08x}/{:08x}", (GetBaseTitleId() >> 32), GetBaseTitleId() & 0xFFFFFFFF));
	}

private:
  	bool IsPrioritizedVersionOrFormat(const TitleInfo& currentTitle, const TitleInfo& newTitle)
	{
		if (!currentTitle.IsValid())
			return true; // always prefer a valid title over an invalid one
		// always prefer higher version
		if (newTitle.GetAppTitleVersion() > currentTitle.GetAppTitleVersion())
			return true;
		// never prefer lower version
		if (newTitle.GetAppTitleVersion() < currentTitle.GetAppTitleVersion())
			return false;
		// for users which have both NUS and non-NUS titles in their games folder we want to prioritize non-NUS formats
		// this is to stay consistent with previous Cemu versions which did not support NUS format at all
		TitleInfo::TitleDataFormat currentFormat = currentTitle.GetFormat();
		TitleInfo::TitleDataFormat newFormat = newTitle.GetFormat();
		if (currentFormat != TitleInfo::TitleDataFormat::NUS && newFormat == TitleInfo::TitleDataFormat::NUS)
			return false;
		return true;
	};

	TitleInfo m_base;
	TitleInfo m_update;
	std::vector<TitleInfo> m_aoc;
};