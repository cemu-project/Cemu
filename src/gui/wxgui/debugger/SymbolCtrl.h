#pragma once

#include <wx/listctrl.h>

class SymbolListCtrl : public wxListView
{
public:
	SymbolListCtrl(wxWindow* parent, const wxWindowID& id, const wxPoint& pos, const wxSize& size);
	void OnGameLoaded();

	void ChangeListFilter(wxString filter);

private:
	struct SymbolItem {
		SymbolItem() = default;
		SymbolItem(const wxString& name, const wxString& libName, const wxString& searchName) : name(name), libName(libName), searchName(searchName) {}

		wxString name;
		wxString libName;
		wxString searchName;
	};
	using SymbolMap = std::map<MPTR, SymbolItem>;

	SymbolMap m_data;
	wxString m_list_filter;
	std::vector<SymbolMap::const_iterator> m_visible_items;

	wxString OnGetItemText(long item, long column) const override;
	void RebuildVisibleItems();
	void OnLeftDClick(wxListEvent& event);
	void OnRightClick(wxListEvent& event);
};
