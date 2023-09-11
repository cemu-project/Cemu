#pragma once

#include "gui/helpers/wxCustomData.h"
#include "config/CemuConfig.h"

#include <wx/listctrl.h>

#include <boost/optional.hpp> // std::optional doesn't support optional reference inner types yet
#include <utility>
#include <vector>

class wxDownloadManagerList : public wxListCtrl
{
	friend class TitleManager;
public:
	wxDownloadManagerList(wxWindow* parent, wxWindowID id = wxID_ANY);
	
	enum ItemColumn
	{
		ColumnTitleId = 0,
		ColumnName,
		ColumnVersion,
		ColumnType,
		ColumnProgress,
		ColumnStatus,

		ColumnMAX,
	};

	enum class EntryType
	{
		Base,
		Update,
		DLC,
	};

	enum class TitleDownloadStatus
	{
		None,
		Available, // available for download
		Error, // error state
		Queued, // queued for download
		Initializing, // downloading/parsing TMD
		Checking, // checking for previously downloaded files
		Downloading,
		Verifying, // verifying downloaded files
		Installing,
		Installed,
		// error state?
	};

	void SortEntries();
	void RefreshPage();
	void Filter(const wxString& filter);
	void Filter2(bool showTitles, bool showUpdates, bool showInstalled);
	[[nodiscard]] size_t GetCountByType(EntryType type) const;
	void ClearItems();
	void AutosizeColumns();

	struct TitleEntry
	{
		TitleEntry(const EntryType& type, bool isPackage, uint64 titleId, uint16 version, bool isPaused)
			: type(type), isPackage(isPackage), titleId(titleId), version(version), isPaused(isPaused) {}

		EntryType type;

		bool isPaused{};
		int icon = -1;
		bool isPackage;
		uint64 titleId;
		wxString name;
		uint32_t version{ 0 };
		uint32 progress; // downloading: in 1/10th of a percent, installing: number of files
		uint32 progressMax{ 0 };
		CafeConsoleRegion region;

		TitleDownloadStatus status = TitleDownloadStatus::None;
		std::string errorMsg;

		bool operator==(const TitleEntry& e) const
		{
			return type == e.type && titleId == e.titleId && version == e.version;
		}
		bool operator!=(const TitleEntry& e) const { return !(*this == e); }
	};
	boost::optional<const TitleEntry&> GetSelectedTitleEntry() const;

private:
	void AddColumns();
	int Filter(const wxString& filter, const wxString& prefix, ItemColumn column);
	boost::optional<TitleEntry&> GetSelectedTitleEntry();
	boost::optional<TitleEntry&> GetTitleEntry(uint64 titleId, uint16 titleVersion);
	
	class wxPanel* m_tooltip_window;
	class wxStaticText* m_tooltip_text;
	class wxTimer* m_tooltip_timer;

	void OnClose(wxCloseEvent& event);
	void OnColumnClick(wxListEvent& event);
	void OnContextMenu(wxContextMenuEvent& event);
	void OnItemSelected(wxListEvent& event);
	void OnContextMenuSelected(wxCommandEvent& event);
	void OnTimer(class wxTimerEvent& event);
	void OnRemoveItem(wxCommandEvent& event);
	void OnRemoveEntry(wxCommandEvent& event);

	using TitleEntryData_t = wxCustomData<TitleEntry>;
	void AddOrUpdateTitle(TitleEntryData_t* obj);
	void UpdateTitleStatusDepr(TitleEntryData_t* obj, const wxString& text);
	int AddImage(const wxImage& image) const;
	wxString OnGetItemText(long item, long column) const override;
	wxItemAttr* OnGetItemAttr(long item) const override;
	[[nodiscard]] boost::optional<const TitleEntry&> GetTitleEntry(long item) const;
	[[nodiscard]] boost::optional<TitleEntry&> GetTitleEntry(long item);
	//[[nodiscard]] boost::optional<const TitleEntry&> GetTitleEntry(const fs::path& path) const;
	//[[nodiscard]] boost::optional<TitleEntry&> GetTitleEntry(const fs::path& path);
	
	void SetCurrentDownloadMgr(class DownloadManager* dlMgr);

	//bool FixEntry(TitleEntry& entry);
	//bool VerifyEntryFiles(TitleEntry& entry);
	bool StartDownloadEntry(const TitleEntry& entry);
	bool RetryDownloadEntry(const TitleEntry& entry);
	bool PauseDownloadEntry(const TitleEntry& entry);

	void RemoveItem(long item);
	void RemoveItem(const TitleEntry& entry);
	
	struct ItemData
	{
		ItemData(bool visible, const TitleEntry& entry)
			: visible(visible), entry(entry) {}
		
		bool visible;
		TitleEntry entry;
	};
	using ItemDataPtr = std::unique_ptr<ItemData>;
	std::vector<ItemDataPtr> m_data;
	std::vector<std::reference_wrapper<ItemData>> m_sorted_data;

	int m_sort_by_column = ItemColumn::ColumnName;
	bool m_sort_less = true;

	bool m_filterShowTitles = true;
	bool m_filterShowUpdates = true;
	bool m_filterShowInstalled = true;
	DownloadManager* m_dlMgr{};
	std::mutex m_dlMgrMutex;
	using Type_t = std::reference_wrapper<const ItemData>;
	bool SortFunc(std::span<int> sortColumnOrder, const Type_t& v1, const Type_t& v2);

	static wxString GetTitleEntryText(const TitleEntry& entry, ItemColumn column);
	static wxString GetTranslatedTitleEntryType(EntryType entryType);
	std::future<bool> m_context_worker;
};
