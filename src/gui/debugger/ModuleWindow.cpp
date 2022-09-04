#include "gui/wxgui.h"
#include "gui/guiWrapper.h"
#include "gui/debugger/ModuleWindow.h"

#include <sstream>

#include "gui/debugger/DebuggerWindow2.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"

#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_structs.h"

#include "Cafe/GraphicPack/GraphicPack2.h"

enum ItemColumns
{
	ColumnName = 0,
	ColumnAddress,
	ColumnSize,
};

ModuleWindow::ModuleWindow(DebuggerWindow2& parent, const wxPoint& main_position, const wxSize& main_size)
	: wxFrame(&parent, wxID_ANY, _("Modules"), wxDefaultPosition, wxSize(420, 250), wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxRESIZE_BORDER | wxFRAME_FLOAT_ON_PARENT)
{
	this->SetSizeHints(wxDefaultSize, wxDefaultSize);

	this->wxWindowBase::SetBackgroundColour(*wxWHITE);

	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

	m_modules = new wxListView(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);

	wxListItem col0;
	col0.SetId(ColumnName);
	col0.SetText(_("Name"));
	col0.SetWidth(125);
	m_modules->InsertColumn(ColumnName, col0);

	wxListItem col1;
	col1.SetId(ColumnAddress);
	col1.SetText(_("Address"));
	col1.SetWidth(75);
	col1.SetAlign(wxLIST_FORMAT_RIGHT);
	m_modules->InsertColumn(ColumnAddress, col1);

	wxListItem col2;
	col2.SetId(ColumnSize);
	col2.SetText(_("Size"));
	col2.SetWidth(75);
	col2.SetAlign(wxLIST_FORMAT_RIGHT);
	m_modules->InsertColumn(ColumnSize, col2);

	main_sizer->Add(m_modules, 1, wxEXPAND);

	this->SetSizer(main_sizer);
	this->wxWindowBase::Layout();

	this->Centre(wxBOTH);

	if (parent.GetConfig().data().pin_to_main)
		OnMainMove(main_position, main_size);

	m_modules->Bind(wxEVT_LEFT_DCLICK, &ModuleWindow::OnLeftDClick, this);

	OnGameLoaded();
}

void ModuleWindow::OnMainMove(const wxPoint& main_position, const wxSize& main_size)
{
	wxSize size(420, 250);
	this->SetSize(size);

	wxPoint position = main_position;
	position.x -= 420;
	position.y += main_size.y;
	this->SetPosition(position);
}


void ModuleWindow::OnGameLoaded()
{
	Freeze();

	m_modules->DeleteAllItems();

	const auto module_count = RPLLoader_GetModuleCount();
	const auto module_list = RPLLoader_GetModuleList();
	for (int i = 0; i < module_count; i++)
	{
		const auto module = module_list[i];
		if (module)
		{
			wxListItem item;
			item.SetId(i);
			item.SetText(module->moduleName2.c_str());

			const auto index = m_modules->InsertItem(item);
			m_modules->SetItem(index, ColumnAddress, wxString::Format("%08x", module->regionMappingBase_text.GetMPTR()));
			m_modules->SetItem(index, ColumnSize, wxString::Format("%x", module->regionSize_text));
		}
	}

	sint32 patch_count = 0;
	for (auto& gfx_pack : GraphicPack2::GetGraphicPacks())
	{
		for (auto& patch_group : gfx_pack->GetPatchGroups())
		{
			if (patch_group->isApplied())
			{
				wxListItem item;
				item.SetId(module_count + patch_count);
				item.SetText(std::string(patch_group->getName()));

				const auto index = m_modules->InsertItem(item);
				m_modules->SetItem(index, ColumnAddress, wxString::Format("%08x", patch_group->getCodeCaveBase()));
				m_modules->SetItem(index, ColumnSize, wxString::Format("%x", patch_group->getCodeCaveSize()));
				patch_count++;
			}
		}
	}

	Thaw();
}


void ModuleWindow::OnLeftDClick(wxMouseEvent& event)
{
	long selected = m_modules->GetFirstSelected();
	if (selected == -1)
		return;
	const auto text = m_modules->GetItemText(selected, ColumnAddress);
	const auto address = std::stoul(text.ToStdString(), nullptr, 16);
	if (address == 0)
		return;
	debuggerState.debugSession.instructionPointer = address;
	debuggerWindow_moveIP();
}
