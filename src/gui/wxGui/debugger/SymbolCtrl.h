#pragma once

#include <wx/listctrl.h>

class SymbolListCtrl : public wxListCtrl
{
public:
	SymbolListCtrl(wxWindow* parent, const wxWindowID& id, const wxPoint& pos, const wxSize& size);
	void OnGameLoaded();

	void ChangeListFilter(std::string filter);
private:
	struct SymbolItem {
        SymbolItem() = default;
        SymbolItem(wxString name, wxString libName, wxString searchName, bool visible) : name(name), libName(libName), searchName(searchName), visible(visible) {}


		wxString name;
		wxString libName;
		wxString searchName;
		bool visible;
	};

	std::map<MPTR, SymbolItem> m_data;
	wxString m_list_filter;

	wxString OnGetItemText(long item, long column) const;
	void OnLeftDClick(wxListEvent& event);
	void OnRightClick(wxListEvent& event);
};
