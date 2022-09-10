#include "gui/wxgui.h"
#include "gui/guiWrapper.h"
#include "gui/debugger/SymbolWindow.h"
#include "gui/debugger/DebuggerWindow2.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"
#include "Cafe/OS/RPL/rpl_symbol_storage.h"

enum ItemColumns
{
	ColumnName = 0,
	ColumnAddress,
	ColumnModule,
};

SymbolWindow::SymbolWindow(DebuggerWindow2& parent, const wxPoint& main_position, const wxSize& main_size)
	: wxFrame(&parent, wxID_ANY, _("Symbols"), wxDefaultPosition, wxSize(600, 250), wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxRESIZE_BORDER | wxFRAME_FLOAT_ON_PARENT)
{
	this->wxWindowBase::SetBackgroundColour(*wxWHITE);

	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
	m_symbol_ctrl = new SymbolListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
	main_sizer->Add(m_symbol_ctrl, 1, wxEXPAND);

	m_filter = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_NO_VSCROLL);
	m_filter->Bind(wxEVT_TEXT, &SymbolWindow::OnFilterChanged, this);
	main_sizer->Add(m_filter, 0, wxALL | wxEXPAND, 5);

	this->SetSizer(main_sizer);
	this->wxWindowBase::Layout();

	this->Centre(wxHORIZONTAL);

	if (parent.GetConfig().data().pin_to_main)
		OnMainMove(main_position, main_size);
}

void SymbolWindow::OnMainMove(const wxPoint& main_position, const wxSize& main_size)
{
	wxSize size(420, 250);
	this->SetSize(size);

	wxPoint position = main_position;
	position.x -= 420;
	this->SetPosition(position);
}

void SymbolWindow::OnGameLoaded()
{
	m_symbol_ctrl->OnGameLoaded();
}

void SymbolWindow::OnFilterChanged(wxCommandEvent& event)
{
	m_symbol_ctrl->ChangeListFilter(m_filter->GetValue().ToStdString());
}
