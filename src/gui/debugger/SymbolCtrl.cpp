#include "gui/debugger/SymbolCtrl.h"
#include "gui/guiWrapper.h"
#include "Cafe/OS/RPL/rpl_symbol_storage.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"

enum ItemColumns
{
	ColumnName = 0,
	ColumnAddress,
	ColumnModule,
};

SymbolListCtrl::SymbolListCtrl(wxWindow* parent, const wxWindowID& id, const wxPoint& pos, const wxSize& size) :
	wxListCtrl(parent, id, pos, size, wxLC_REPORT | wxLC_VIRTUAL)
{    
	wxListItem col0;
	col0.SetId(ColumnName);
	col0.SetText(_("Name"));
	col0.SetWidth(200);
	InsertColumn(ColumnName, col0);

	wxListItem col1;
	col1.SetId(ColumnAddress);
	col1.SetText(_("Address"));
	col1.SetWidth(75);
	col1.SetAlign(wxLIST_FORMAT_RIGHT);
	InsertColumn(ColumnAddress, col1);

	wxListItem col2;
	col2.SetId(ColumnModule);
	col2.SetText(_("Module"));
	col2.SetWidth(75);
	col2.SetAlign(wxLIST_FORMAT_RIGHT);
	InsertColumn(ColumnModule, col2);

	Bind(wxEVT_LIST_ITEM_ACTIVATED, &SymbolListCtrl::OnLeftDClick, this);
	Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &SymbolListCtrl::OnRightClick, this);

	m_list_filter.Clear();

	OnGameLoaded();

	SetItemCount(m_data.size());
}

void SymbolListCtrl::OnGameLoaded()
{
	m_data.clear();
	const auto symbol_map = rplSymbolStorage_lockSymbolMap();
	for (auto const& [address, symbol_info] : symbol_map)
	{
		if (symbol_info == nullptr || symbol_info->symbolName == nullptr)
			continue;

		wxString libNameWX = wxString::FromAscii((const char*)symbol_info->libName);
		wxString symbolNameWX = wxString::FromAscii((const char*)symbol_info->symbolName);
		wxString searchNameWX = libNameWX + symbolNameWX;
		searchNameWX.MakeLower();

		auto new_entry = m_data.try_emplace(
			symbol_info->address,
            symbolNameWX,
			libNameWX,
            searchNameWX,
			false
		);

		if (m_list_filter.IsEmpty())
			new_entry.first->second.visible = true;
		else if (new_entry.first->second.searchName.Contains(m_list_filter))
			new_entry.first->second.visible = true;
	}
	rplSymbolStorage_unlockSymbolMap();

	SetItemCount(m_data.size());
	if (m_data.size() > 0)
		RefreshItems(GetTopItem(), std::min<long>(m_data.size() - 1, GetTopItem() + GetCountPerPage() + 1));
}

wxString SymbolListCtrl::OnGetItemText(long item, long column) const
{
	if (item >= GetItemCount())
		return wxEmptyString;

	long visible_idx = 0;
	for (const auto& [address, symbol] : m_data)
	{
		if (!symbol.visible)
			continue;

		if (item == visible_idx)
		{
			if (column == ColumnName)
				return wxString(symbol.name);
			if (column == ColumnAddress)
				return wxString::Format("%08x", address);
			if (column == ColumnModule)
				return wxString(symbol.libName);
		}
		visible_idx++;
	}

	return wxEmptyString;
}


void SymbolListCtrl::OnLeftDClick(wxListEvent& event)
{
	long selected = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected == -1)
		return;
	const auto text = GetItemText(selected, ColumnAddress);
	const auto address = std::stoul(text.ToStdString(), nullptr, 16);
	if (address == 0)
		return;
	debuggerState.debugSession.instructionPointer = address;
	debuggerWindow_moveIP();
}

void SymbolListCtrl::OnRightClick(wxListEvent& event)
{
	long selected = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected == -1)
		return;
	auto text = GetItemText(selected, ColumnAddress);
	text = "0x" + text;
#if BOOST_OS_WINDOWS
	if (OpenClipboard(nullptr))
	{
		EmptyClipboard();
		const HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.size()+1);
		if (hGlobal)
		{
			memcpy(GlobalLock(hGlobal), text.c_str(), text.size() + 1);
			GlobalUnlock(hGlobal);

			SetClipboardData(CF_TEXT, hGlobal);
			GlobalFree(hGlobal);
		}
		CloseClipboard();
	}
#endif
}

void SymbolListCtrl::ChangeListFilter(std::string filter)
{
	m_list_filter = wxString(filter).MakeLower();

	size_t visible_entries = m_data.size();
	for (auto& [address, symbol] : m_data)
	{
		if (m_list_filter.IsEmpty())
			symbol.visible = true;
		else if (symbol.searchName.Contains(m_list_filter))
			symbol.visible = true;
		else
		{
			visible_entries--;
			symbol.visible = false;
		}
	}
	SetItemCount(visible_entries);
	if (visible_entries > 0)
		RefreshItems(GetTopItem(), std::min<long>(visible_entries - 1, GetTopItem() + GetCountPerPage() + 1));
}