#pragma once

#include "gui/helpers/wxCustomData.h"
#include "config/CemuConfig.h"

#include <wx/listctrl.h>

#include <boost/optional.hpp> // std::optional doesn't support optional reference inner types yet
#include <utility>
#include <vector>

class wxTitleManagerList : public wxListCtrl
{
	friend class TitleManager;
public:
	wxTitleManagerList(wxWindow* parent, wxWindowID id = wxID_ANY);
	~wxTitleManagerList();

	enum ItemColumn
	{
		ColumnTitleId = 0,
		ColumnName,
		ColumnType,
		ColumnVersion,
		ColumnRegion,
		ColumnFormat,
		ColumnLocation,

		ColumnMAX,
	};

	enum class EntryType
	{
		Base,
		Update,
		Dlc,
		Save,
		System,
	};

	enum class EntryFormat
	{
		Folder,
		WUD,
		WUA,
	};

	// sort by column, if -1 will sort by last column or default (=titleid)
	void SortEntries(int column);
	void RefreshPage();
	void Filter(const wxString& filter);
	[[nodiscard]] size_t GetCountByType(EntryType type) const;
	void ClearItems();
	void AutosizeColumns();

	struct TitleEntry
	{
		TitleEntry(const EntryType& type, const EntryFormat& format, fs::path path)
			: type(type), format(format), path(std::move(path)) {}

		EntryType type;
		EntryFormat format;
		fs::path path;

		int icon = -1;
		uint64 location_uid;
		uint64 title_id;
		wxString name;
		uint32_t version = 0;
		CafeConsoleRegion region;

		std::vector<uint32> persistent_ids; // only used for save
	};
	boost::optional<const TitleEntry&> GetSelectedTitleEntry() const;

private:
	void AddColumns();
	int Filter(const wxString& filter, const wxString& prefix, ItemColumn column);
	boost::optional<TitleEntry&> GetSelectedTitleEntry();
	boost::optional<TitleEntry&> GetTitleEntryByUID(uint64 uid);

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

	void OnTitleDiscovered(wxCommandEvent& event);
	void OnTitleRemoved(wxCommandEvent& event);
	void HandleTitleListCallback(struct CafeTitleListCallbackEvent* evt);
	void HandleSaveListCallback(struct CafeSaveListCallbackEvent* evt);

	using TitleEntryData_t = wxCustomData<TitleEntry>;
	void AddTitle(TitleEntryData_t* obj);
	int AddImage(const wxImage& image) const;
	wxString OnGetItemText(long item, long column) const override;
	wxItemAttr* OnGetItemAttr(long item) const override;
	[[nodiscard]] boost::optional<const TitleEntry&> GetTitleEntry(long item) const;
	[[nodiscard]] boost::optional<TitleEntry&> GetTitleEntry(long item);
	[[nodiscard]] boost::optional<const TitleEntry&> GetTitleEntry(const fs::path& path) const;
	[[nodiscard]] boost::optional<TitleEntry&> GetTitleEntry(const fs::path& path);
	
	bool VerifyEntryFiles(TitleEntry& entry);
	void OnConvertToCompressedFormat(uint64 titleId);
	bool DeleteEntry(long index, const TitleEntry& entry);

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

	int m_last_column_sorted = -1;
	bool m_sort_less = true;
	using Type_t = std::reference_wrapper<const ItemData>;
	bool SortFunc(int column, const Type_t& v1, const Type_t& v2);

	static wxString GetTitleEntryText(const TitleEntry& entry, ItemColumn column);
	std::future<bool> m_context_worker;

	uint64 m_callbackIdTitleList;
	uint64 m_callbackIdSaveList;
};

template <>
struct fmt::formatter<wxTitleManagerList::EntryType> : formatter<string_view>
{
	using base = fmt::formatter<fmt::string_view>;
	template <typename FormatContext>
	auto format(const wxTitleManagerList::EntryType& type, FormatContext& ctx)
	{
		switch (type)
		{
		case wxTitleManagerList::EntryType::Base:
			return base::format("base", ctx);
		case wxTitleManagerList::EntryType::Update:
			return base::format("update", ctx);
		case wxTitleManagerList::EntryType::Dlc:
			return base::format("DLC", ctx);
		case wxTitleManagerList::EntryType::Save:
			return base::format("save", ctx);
		case wxTitleManagerList::EntryType::System:
			return base::format("system", ctx);
		}
		return base::format(std::to_string(static_cast<std::underlying_type_t<wxTitleManagerList::EntryType>>(type)), ctx);
	}
};