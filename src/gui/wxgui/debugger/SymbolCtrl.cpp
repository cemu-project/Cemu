#include "wxgui/debugger/SymbolCtrl.h"
#include "Cafe/OS/RPL/rpl_symbol_storage.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"
#include <wx/listctrl.h>

enum ItemColumns
{
	ColumnName = 0,
	ColumnAddress,
	ColumnModule,
};

SymbolListCtrl::SymbolListCtrl(wxWindow* parent, const wxWindowID& id, const wxPoint& pos, const wxSize& size) : wxListView(parent, id, pos, size, wxLC_REPORT | wxLC_VIRTUAL)
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
}

void SymbolListCtrl::OnGameLoaded()
{
	m_data.clear();
	m_visible_items.clear();
	const auto symbol_map = rplSymbolStorage_lockSymbolMap();
	for (auto const& [address, symbol_info] : symbol_map)
	{
		if (symbol_info == nullptr || symbol_info->symbolName == nullptr)
			continue;

		wxString libNameWX = wxString::FromAscii((const char*)symbol_info->libName);
		wxString symbolNameWX = wxString::FromAscii((const char*)symbol_info->symbolName);
		wxString searchNameWX = libNameWX + symbolNameWX;
		searchNameWX.MakeLower();

		m_data.try_emplace(
			address,
			symbolNameWX,
			libNameWX,
			searchNameWX
		);
	}
	rplSymbolStorage_unlockSymbolMap();

	RebuildVisibleItems();
}

wxString SymbolListCtrl::OnGetItemText(long item, long column) const
{
	if (item < 0 || item >= static_cast<long>(m_visible_items.size()))
		return wxEmptyString;

	const auto& [address, symbol] = *m_visible_items[item];
	if (column == ColumnName)
		return symbol.name;
	if (column == ColumnAddress)
		return wxString::Format("%08x", address);
	if (column == ColumnModule)
		return symbol.libName;

	return wxEmptyString;
}


void SymbolListCtrl::OnLeftDClick(wxListEvent& event)
{
	long selected = GetFirstSelected();
	if (selected == wxNOT_FOUND || selected >= static_cast<long>(m_visible_items.size()))
		return;
	const auto address = m_visible_items[selected]->first;
	if (address == 0)
		return;
	debugger_jumpToAddressInDisasm(address);
}

void SymbolListCtrl::OnRightClick(wxListEvent& event)
{
	long selected = GetFirstSelected();
	if (selected == wxNOT_FOUND || selected >= static_cast<long>(m_visible_items.size()))
		return;
	auto text = wxString::Format("0x%08x", m_visible_items[selected]->first);
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

void SymbolListCtrl::RebuildVisibleItems()
{
	m_visible_items.clear();
	m_visible_items.reserve(m_data.size());
	const bool has_filter = !m_list_filter.IsEmpty();
	for (auto it = m_data.cbegin(); it != m_data.cend(); ++it)
	{
		if (has_filter && !it->second.searchName.Contains(m_list_filter))
			continue;

		m_visible_items.emplace_back(it);
	}

	SetItemCount(static_cast<long>(m_visible_items.size()));
	if (!m_visible_items.empty())
	{
		const auto top_item = std::min<long>(GetTopItem(), static_cast<long>(m_visible_items.size() - 1));
		RefreshItems(top_item, std::min<long>(static_cast<long>(m_visible_items.size() - 1), top_item + GetCountPerPage() + 1));
	}
	else
		Refresh();
}

void SymbolListCtrl::ChangeListFilter(wxString filter)
{
	m_list_filter = filter.MakeLower();
	RebuildVisibleItems();
}
