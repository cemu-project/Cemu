#include "gui/components/wxDownloadManagerList.h"

#include "gui/helpers/wxHelpers.h"
#include "util/helpers/SystemException.h"
#include "Cafe/TitleList/GameInfo.h"
#include "gui/components/wxGameList.h"
#include "gui/helpers/wxCustomEvents.h"

#include <wx/imaglist.h>
#include <wx/wupdlock.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/timer.h>
#include <wx/panel.h>

#include <functional>

#include "config/ActiveSettings.h"
#include "gui/ChecksumTool.h"
#include "Cemu/Tools/DownloadManager/DownloadManager.h"
#include "Cafe/TitleList/TitleId.h"
#include "gui/MainWindow.h"

wxDEFINE_EVENT(wxEVT_REMOVE_ENTRY, wxCommandEvent);


wxDownloadManagerList::wxDownloadManagerList(wxWindow* parent, wxWindowID id)
	: wxListCtrl(parent, id, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL)
{
	AddColumns();

	// tooltip TODO: extract class mb wxPanelTooltip
	m_tooltip_window = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
	auto* tooltip_sizer = new wxBoxSizer(wxVERTICAL);
	m_tooltip_text = new wxStaticText(m_tooltip_window, wxID_ANY, wxEmptyString);
	tooltip_sizer->Add(m_tooltip_text , 0, wxALL, 5);
	m_tooltip_window->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
	m_tooltip_window->SetSizerAndFit(tooltip_sizer);
	m_tooltip_window->Hide();
	m_tooltip_timer = new wxTimer(this);

	Bind(wxEVT_LIST_COL_CLICK, &wxDownloadManagerList::OnColumnClick, this);
	Bind(wxEVT_CONTEXT_MENU, &wxDownloadManagerList::OnContextMenu, this);
	Bind(wxEVT_LIST_ITEM_SELECTED, &wxDownloadManagerList::OnItemSelected, this);
	Bind(wxEVT_TIMER, &wxDownloadManagerList::OnTimer, this);
	Bind(wxEVT_REMOVE_ITEM, &wxDownloadManagerList::OnRemoveItem, this);
	Bind(wxEVT_REMOVE_ENTRY, &wxDownloadManagerList::OnRemoveEntry, this);
	Bind(wxEVT_CLOSE_WINDOW, &wxDownloadManagerList::OnClose, this);
}

boost::optional<const wxDownloadManagerList::TitleEntry&> wxDownloadManagerList::GetSelectedTitleEntry() const
{
	const auto selection = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selection != wxNOT_FOUND)
	{
		const auto tmp = GetTitleEntry(selection);
		if (tmp.has_value())
			return tmp.value();
	}

	return {};
}

boost::optional<wxDownloadManagerList::TitleEntry&> wxDownloadManagerList::GetSelectedTitleEntry()
{
	const auto selection = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selection != wxNOT_FOUND)
	{
		const auto tmp = GetTitleEntry(selection);
		if (tmp.has_value())
			return tmp.value();
	}

	return {};
}

boost::optional<wxDownloadManagerList::TitleEntry&> wxDownloadManagerList::GetTitleEntry(uint64 titleId, uint16 titleVersion)
{
	for(const auto& v : m_data)
	{
		if (v->entry.titleId == titleId && v->entry.version == titleVersion)
			return v->entry;
	}

	return {};
}

void wxDownloadManagerList::AddColumns()
{
	wxListItem col0;
	col0.SetId(ColumnTitleId);
	col0.SetText(_("Title ID"));
	col0.SetWidth(120);
	InsertColumn(ColumnTitleId, col0);

	wxListItem col1;
	col1.SetId(ColumnName);
	col1.SetText(_("Name"));
	col1.SetWidth(260);
	InsertColumn(ColumnName, col1);

	wxListItem col2;
	col2.SetId(ColumnVersion);
	col2.SetText(_("Version"));
	col2.SetWidth(55);
	InsertColumn(ColumnVersion, col2);

	wxListItem col3;
	col3.SetId(ColumnType);
	col3.SetText(_("Type"));
	col3.SetWidth(60);
	InsertColumn(ColumnType, col3);

	wxListItem col4;
	col4.SetId(ColumnProgress);
	col4.SetText(_("Progress"));
	col4.SetWidth(wxLIST_AUTOSIZE_USEHEADER);
	InsertColumn(ColumnProgress, col4);
	
	wxListItem col5;
	col5.SetId(ColumnStatus);
	col5.SetText(_("Status"));
	col5.SetWidth(240);
	InsertColumn(ColumnStatus, col5);
}

wxString wxDownloadManagerList::OnGetItemText(long item, long column) const
{
	if (item >= GetItemCount())
		return wxEmptyString;

	const auto entry = GetTitleEntry(item);
	if (entry.has_value())
		return GetTitleEntryText(entry.value(), (ItemColumn)column);

	return wxEmptyString;
}

wxItemAttr* wxDownloadManagerList::OnGetItemAttr(long item) const
{
	const auto entry = GetTitleEntry(item);
	if (entry.has_value())
	{
		auto& entryData = entry.value();
		if (entryData.status == TitleDownloadStatus::Downloading ||
			entryData.status == TitleDownloadStatus::Verifying ||
			entryData.status == TitleDownloadStatus::Installing)
		{
			const wxColour kActiveColor{ 0xFFE0E0 };
			static wxListItemAttr s_error_attr(GetTextColour(), kActiveColor, GetFont());
			return &s_error_attr;
		}
		else if (entryData.status == TitleDownloadStatus::Installed && entryData.isPackage)
		{
			const wxColour kActiveColor{ 0xE0FFE0 };
			static wxListItemAttr s_error_attr(GetTextColour(), kActiveColor, GetFont());
			return &s_error_attr;
		}
		else if (entryData.status == TitleDownloadStatus::Error)
		{
			const wxColour kActiveColor{ 0xCCCCF2 };
			static wxListItemAttr s_error_attr(GetTextColour(), kActiveColor, GetFont());
			return &s_error_attr;
		}
	}

	const wxColour kSecondColor{ 0xFDF9F2 };
	static wxListItemAttr s_coloured_attr(GetTextColour(), kSecondColor, GetFont());
	return item % 2 == 0 ? nullptr : &s_coloured_attr;
}

boost::optional<wxDownloadManagerList::TitleEntry&> wxDownloadManagerList::GetTitleEntry(long item)
{
	long counter = 0;
	for (const auto& data : m_sorted_data)
	{
		if (!data.get().visible)
			continue;

		if (item != counter++)
			continue;

		return data.get().entry;
	}
	
	return {};
}

boost::optional<const wxDownloadManagerList::TitleEntry&> wxDownloadManagerList::GetTitleEntry(long item) const
{
	long counter = 0;
	for (const auto& data : m_sorted_data)
	{
		if (!data.get().visible)
			continue;

		if (item != counter++)
			continue;

		return data.get().entry;
	}

	return {};
}

void wxDownloadManagerList::OnClose(wxCloseEvent& event)
{
	event.Skip();
	// wait until all tasks are complete
	if (m_context_worker.valid())
		m_context_worker.get();
	g_mainFrame->RequestGameListRefresh(); // todo: add games instead of fully refreshing game list if a game is downloaded
}

void wxDownloadManagerList::OnColumnClick(wxListEvent& event)
{
	const int column = event.GetColumn();

	if (column == m_sort_by_column)
	{
		m_sort_less = !m_sort_less;
	}
	else
	{
		m_sort_by_column = column;
		m_sort_less = true;
	}
	SortEntries();
	event.Skip();
}

void wxDownloadManagerList::RemoveItem(long item)
{
	const int item_count = GetItemCount();

	const ItemData* ref = nullptr;
	long counter = 0;
	for(auto it = m_sorted_data.begin(); it != m_sorted_data.end(); ++it)
	{
		if (!it->get().visible)
			continue;

		if (item != counter++)
			continue;

		ref = &(it->get());
		m_sorted_data.erase(it);
		break;
	}

	// shouldn't happen
	if (ref == nullptr)
		return;
	
	for(auto it = m_data.begin(); it != m_data.end(); ++it)
	{
		if (ref != (*it).get())
			continue;
		
		m_data.erase(it);
		break;
	}

	SetItemCount(std::max(0, item_count - 1));
	RefreshPage();
}

void wxDownloadManagerList::RemoveItem(const TitleEntry& entry)
{
	const int item_count = GetItemCount();

	const TitleEntry* ref = &entry;
	for (auto it = m_sorted_data.begin(); it != m_sorted_data.end(); ++it)
	{
		if (ref != &it->get().entry)
			continue;

		m_sorted_data.erase(it);
		break;
	}

	for (auto it = m_data.begin(); it != m_data.end(); ++it)
	{
		if (ref != &(*it).get()->entry)
			continue;

		m_data.erase(it);
		break;
	}

	SetItemCount(std::max(0, item_count - 1));
	RefreshPage();
}

void wxDownloadManagerList::OnItemSelected(wxListEvent& event)
{
	event.Skip();
	m_tooltip_timer->Stop();
	const auto selection = event.GetIndex();

	if (selection == wxNOT_FOUND)
	{
		m_tooltip_window->Hide();
		return;
	}

}

enum ContextMenuEntries
{
	kContextMenuRetry = wxID_HIGHEST + 1,
	kContextMenuDownload,
	kContextMenuPause,
	kContextMenuResume,
};
void wxDownloadManagerList::OnContextMenu(wxContextMenuEvent& event)
{
	// still doing work
	if (m_context_worker.valid() && !future_is_ready(m_context_worker))
		return;
	
	wxMenu menu;
	menu.Bind(wxEVT_COMMAND_MENU_SELECTED, &wxDownloadManagerList::OnContextMenuSelected, this);

	const auto selection = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selection == wxNOT_FOUND)
		return;

	const auto entry = GetTitleEntry(selection);
	if (!entry.has_value())
		return;

	if (entry->isPaused)
		menu.Append(kContextMenuResume, _("&Resume"));
	else if (entry->status == TitleDownloadStatus::Error)
		menu.Append(kContextMenuRetry, _("&Retry"));
	else if(entry->status == TitleDownloadStatus::Available)
		menu.Append(kContextMenuDownload, _("&Download"));
	//else if(entry->status == TitleDownloadStatus::Downloading || entry->status == TitleDownloadStatus::Initializing)
	//	menu.Append(kContextMenuPause, _("&Pause")); buggy!

	PopupMenu(&menu);
	
	// TODO: fix tooltip position
}

void wxDownloadManagerList::SetCurrentDownloadMgr(DownloadManager* dlMgr)
{
	std::unique_lock<std::mutex> _l(m_dlMgrMutex);
	m_dlMgr = dlMgr;
}

bool wxDownloadManagerList::StartDownloadEntry(const TitleEntry& entry)
{
	std::unique_lock<std::mutex> _l(m_dlMgrMutex);
	if (m_dlMgr)
		m_dlMgr->initiateDownload(entry.titleId, entry.version);
	return true;
}

bool wxDownloadManagerList::RetryDownloadEntry(const TitleEntry& entry)
{
	StartDownloadEntry(entry);
	return true;
}

bool wxDownloadManagerList::PauseDownloadEntry(const TitleEntry& entry)
{
	std::unique_lock<std::mutex> _l(m_dlMgrMutex);
	if (m_dlMgr)
		m_dlMgr->pauseDownload(entry.titleId, entry.version);
	return true;
}

void wxDownloadManagerList::OnContextMenuSelected(wxCommandEvent& event)
{
	// still doing work
	if (m_context_worker.valid() && !future_is_ready(m_context_worker))
		return;
	
	const auto selection = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selection == wxNOT_FOUND)
		return;

	const auto entry = GetTitleEntry(selection);
	if (!entry.has_value())
		return;
	
	switch (event.GetId())
	{
	case kContextMenuDownload:
		StartDownloadEntry(entry.value());
		break;
	case kContextMenuRetry:
		RetryDownloadEntry(entry.value());
		break;
	case kContextMenuResume:
		StartDownloadEntry(entry.value());
		break;
	case kContextMenuPause:
		PauseDownloadEntry(entry.value());
		break;
	}
}

void wxDownloadManagerList::OnTimer(wxTimerEvent& event)
{
	if(event.GetTimer().GetId() != m_tooltip_timer->GetId())
	{
		event.Skip();
		return;
	}

	m_tooltip_window->Show();
}

void wxDownloadManagerList::OnRemoveItem(wxCommandEvent& event)
{
	RemoveItem(event.GetInt());
}

void wxDownloadManagerList::OnRemoveEntry(wxCommandEvent& event)
{
	wxASSERT(event.GetClientData() != nullptr);
	RemoveItem(*(TitleEntry*)event.GetClientData());
}

wxString wxDownloadManagerList::GetTitleEntryText(const TitleEntry& entry, ItemColumn column)
{
	switch (column)
	{
	case ColumnTitleId:
		return formatWxString("{:08x}-{:08x}", (uint32) (entry.titleId >> 32), (uint32) (entry.titleId & 0xFFFFFFFF));
	case ColumnName:
		return entry.name;
	case ColumnType:
		return GetTranslatedTitleEntryType(entry.type);
	case ColumnVersion:
	{
		// dont show version for base game unless it is not v0
		if (entry.type == EntryType::Base && entry.version == 0)
			return "";
		if (entry.type == EntryType::DLC && entry.version == 0)
			return "";
		return formatWxString("v{}", entry.version);
	}
	case ColumnProgress:
	{
		if (entry.status == TitleDownloadStatus::Downloading)
		{
			if (entry.progress >= 1000)
				return "100%";
			return formatWxString("{:.1f}%", (float) entry.progress / 10.0f); // one decimal
		}
		else if (entry.status == TitleDownloadStatus::Installing || entry.status == TitleDownloadStatus::Checking || entry.status == TitleDownloadStatus::Verifying)
		{
			return formatWxString("{0}/{1}", entry.progress, entry.progressMax); // number of processed files/content files
		}
		return "";
	}
	case ColumnStatus:
	{
		if (entry.isPaused)
			return _("Paused");
		else if (entry.status == TitleDownloadStatus::Available)
		{
			if (entry.progress == 1)
				return _("Not installed (Partially downloaded)");
			if (entry.progress == 2)
				return _("Update available");
			return _("Not installed");
		}
		else if (entry.status == TitleDownloadStatus::Initializing)
			return _("Initializing");
		else if (entry.status == TitleDownloadStatus::Checking)
			return _("Checking");
		else if (entry.status == TitleDownloadStatus::Queued)
			return _("Queued");
		else if (entry.status == TitleDownloadStatus::Downloading)
			return _("Downloading");
		else if (entry.status == TitleDownloadStatus::Verifying)
			return _("Verifying");
		else if (entry.status == TitleDownloadStatus::Installing)
			return _("Installing");
		else if (entry.status == TitleDownloadStatus::Installed)
			return _("Installed");
		else if (entry.status == TitleDownloadStatus::Error)
		{
			wxString errorStatusMsg = _("Error:");
			errorStatusMsg.append(" ");
			errorStatusMsg.append(entry.errorMsg);
			return errorStatusMsg;
		}
		else
			return "Unknown status";
	}
	}
	
	return wxEmptyString;
}

wxString wxDownloadManagerList::GetTranslatedTitleEntryType(EntryType type)
{
	switch (type)
	{
		case EntryType::Base:
			return _("base");
		case EntryType::Update:
			return _("update");
		case EntryType::DLC:
			return _("DLC");
		default:
			return std::to_string(static_cast<std::underlying_type_t<EntryType>>(type));
	}
}

void wxDownloadManagerList::AddOrUpdateTitle(TitleEntryData_t* obj)
{
	const auto& data = obj->GetData();
	// if already in list only update
	auto entry = GetTitleEntry(data.titleId, data.version);
	if (entry.has_value())
	{
		// update item
		entry.value() = data;
		RefreshPage(); // more efficient way to do this?
		return;
	}

	m_data.emplace_back(std::make_unique<ItemData>(true, data));
	m_sorted_data.emplace_back(*m_data[m_data.size() - 1]);
	SetItemCount(m_data.size());

	// reapply sort
	Filter2(m_filterShowTitles, m_filterShowUpdates, m_filterShowInstalled);
	SortEntries();
}

void wxDownloadManagerList::UpdateTitleStatusDepr(TitleEntryData_t* obj, const wxString& text)
{
	const auto& data = obj->GetData();
	const auto entry = GetTitleEntry(data.titleId, data.version);
	// check if already added to list
	if (!entry.has_value())
		return;

	// update gamelist text
	for(size_t i = 0; i < m_sorted_data.size(); ++i)
	{
		if (m_sorted_data[i].get().entry == data)
		{
			SetItem(i, ColumnStatus, text);
			return;
		}
	}

	cemuLog_logDebug(LogType::Force, "cant update title status of {:x}", data.titleId);
}

int wxDownloadManagerList::AddImage(const wxImage& image) const
{
	return -1; // m_image_list->Add(image.Scale(kListIconWidth, kListIconWidth, wxIMAGE_QUALITY_BICUBIC));
}

// return <
bool wxDownloadManagerList::SortFunc(std::span<int> sortColumnOrder, const Type_t& v1, const Type_t& v2)
{
	cemu_assert_debug(sortColumnOrder.size() == 5);

	// visible have always priority
	if (!v1.get().visible && v2.get().visible)
		return false;
	else if (v1.get().visible && !v2.get().visible)
		return true;

	const auto& entry1 = v1.get().entry;
	const auto& entry2 = v2.get().entry;
	
	for (size_t i = 0; i < sortColumnOrder.size(); i++)
	{
		int sortByColumn = sortColumnOrder[i];
		if (sortByColumn == ColumnTitleId)
		{
			// ensure strong ordering -> use type since only one entry should be now (should be changed if every save for every user is displayed spearately?)
			if (entry1.titleId != entry2.titleId)
				return entry1.titleId < entry2.titleId;
		}
		else if (sortByColumn == ColumnName)
		{
			const int tmp = entry1.name.CmpNoCase(entry2.name);
			if (tmp != 0)
				return tmp < 0;
		}
		else if (sortByColumn == ColumnType)
		{
			if (std::underlying_type_t<EntryType>(entry1.type) != std::underlying_type_t<EntryType>(entry2.type))
				return std::underlying_type_t<EntryType>(entry1.type) < std::underlying_type_t<EntryType>(entry2.type);
		}
		else if (sortByColumn == ColumnVersion)
		{
			if (entry1.version != entry2.version)
				return entry1.version < entry2.version;
		}
		else if (sortByColumn == ColumnProgress)
		{
			if (entry1.progress != entry2.progress)
			return entry1.progress < entry2.progress;
		}
		else
		{
			cemu_assert_debug(false);
			return (uintptr_t)&entry1 < (uintptr_t)&entry2;
		}

	}

	return false;
}

#include <boost/container/small_vector.hpp>

void wxDownloadManagerList::SortEntries()
{
	boost::container::small_vector<int, 12> s_SortColumnOrder{ ColumnName, ColumnType, ColumnVersion, ColumnTitleId, ColumnProgress };

	if (m_sort_by_column != -1)
	{
		// prioritize column by moving it to first position in the column sort order list
		s_SortColumnOrder.erase(std::remove(s_SortColumnOrder.begin(), s_SortColumnOrder.end(), m_sort_by_column), s_SortColumnOrder.end());
		s_SortColumnOrder.insert(s_SortColumnOrder.begin(), m_sort_by_column);
	}

	std::sort(m_sorted_data.begin(), m_sorted_data.end(),
		[this, &s_SortColumnOrder](const Type_t& v1, const Type_t& v2) -> bool
		{
			const bool result = SortFunc({ s_SortColumnOrder.data(), s_SortColumnOrder.size() }, v1, v2);
			return m_sort_less ? result : !result;
		});
	
	RefreshPage();
}

void wxDownloadManagerList::RefreshPage()
{
	long item_count = GetItemCount();

	if (item_count > 0)
		RefreshItems(GetTopItem(), std::min(item_count - 1, GetTopItem() + GetCountPerPage() + 1));
}

int wxDownloadManagerList::Filter(const wxString& filter, const wxString& prefix, ItemColumn column)
{
	if (prefix.empty())
		return -1;
	
	if (!filter.StartsWith(prefix))
		return -1;

	int counter = 0;
	const auto tmp_filter = filter.substr(prefix.size() - 1).Trim(false);
	for (auto&& data : m_data)
	{
		if (GetTitleEntryText(data->entry, column).Upper().Contains(tmp_filter))
		{
			data->visible = true;
			++counter;
		}
		else
			data->visible = false;
	}
	return counter;
}

void wxDownloadManagerList::Filter(const wxString& filter)
{
	if(filter.empty())
	{
		std::for_each(m_data.begin(), m_data.end(), [](ItemDataPtr& data) { data->visible = true; });
		SetItemCount(m_data.size());
		RefreshPage();
		return;
	}

	const auto filter_upper = filter.Upper().Trim(false).Trim(true);
	int counter = 0;
	
	if (const auto result = Filter(filter_upper, "TITLEID:", ColumnTitleId) != -1)
		counter = result;
	else if (const auto result = Filter(filter_upper, "NAME:", ColumnName) != -1)
		counter = result;
	else if (const auto result = Filter(filter_upper, "TYPE:", ColumnType) != -1)
		counter = result;
	//else if (const auto result = Filter(filter_upper, "REGION:", ColumnRegion) != -1)
	//	counter = result;
	else if (const auto result = Filter(filter_upper, "VERSION:", ColumnVersion) != -1)
		counter = result;
	else if(filter_upper == "ERROR")
	{
		for (auto&& data : m_data)
		{
			bool visible = false;
			//if (data->entry.error != TitleError::None)
			//	visible = true;

			data->visible = visible;
			if (visible)
				++counter;
		}
	}
	else
	{
		for (auto&& data : m_data)
		{
			bool visible = false;
			if (data->entry.name.Upper().Contains(filter_upper))
				visible = true;
			else if (GetTitleEntryText(data->entry, ColumnTitleId).Upper().Contains(filter_upper))
				visible = true;
			else if (GetTitleEntryText(data->entry, ColumnType).Upper().Contains(filter_upper))
				visible = true;

			data->visible = visible;
			if (visible)
				++counter;
		}
	}
	
	SetItemCount(counter);
	RefreshPage();
}

void wxDownloadManagerList::Filter2(bool showTitles, bool showUpdates, bool showInstalled)
{
	m_filterShowTitles = showTitles;
	m_filterShowUpdates = showUpdates;
	m_filterShowInstalled = showInstalled;
	if (showTitles && showUpdates && showInstalled)
	{
		std::for_each(m_data.begin(), m_data.end(), [](ItemDataPtr& data) { data->visible = true; });
		SetItemCount(m_data.size());
		RefreshPage();
		return;
	}

	size_t counter = 0;

	for (auto&& data : m_data)
	{
		bool visible = false;

		TitleIdParser tParser(data->entry.titleId);
		bool isInstalled = data->entry.status == TitleDownloadStatus::Installed;
		if (tParser.IsBaseTitleUpdate())
		{
			if (showUpdates && (showInstalled || !isInstalled))
				visible = true;
		}
		else
		{
			if (showTitles && (showInstalled || !isInstalled))
				visible = true;
		}
	
		data->visible = visible;
		if (visible)
			++counter;
	}

	SetItemCount(counter);
	RefreshPage();
}

size_t wxDownloadManagerList::GetCountByType(EntryType type) const
{
	size_t result = 0;
	for(const auto& data : m_data)
	{
		if (data->entry.type == type)
			++result;
	}
	return result;
}

void wxDownloadManagerList::ClearItems()
{
	m_sorted_data.clear();
	m_data.clear();
	SetItemCount(0);
	RefreshPage();
}

void wxDownloadManagerList::AutosizeColumns()
{
	wxAutosizeColumns(this, ColumnTitleId, ColumnMAX - 1);
}

