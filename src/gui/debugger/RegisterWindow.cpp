#include "gui/wxgui.h"
#include "gui/debugger/RegisterWindow.h"

#include <sstream>

#include "gui/debugger/DebuggerWindow2.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cafe/HW/Espresso/EspressoISA.h"

enum
{
	// REGISTER
	kRegisterValueR0 = wxID_HIGHEST + 8400,
	kRegisterLabelR0 = kRegisterValueR0 + 32,
	kRegisterLabelLR = kRegisterLabelR0 + 32,
	kRegisterValueLR = kRegisterLabelLR + 1,
	kRegisterValueFPR0_0 = kRegisterValueLR + 32,
	kRegisterValueFPR1_0 = kRegisterValueFPR0_0 + 32,
	kRegisterValueCR0 = kRegisterValueFPR1_0 + 32,
	kContextMenuZero,
	kContextMenuInc,
	kContextMenuDec,
	kContextMenuCopy,
	kContextMenuGotoDisasm,
	kContextMenuGotoDump,
};

RegisterWindow::RegisterWindow(DebuggerWindow2& parent, const wxPoint& main_position, const wxSize& main_size)
	: wxFrame(&parent, wxID_ANY, _("Registers"), wxDefaultPosition, wxSize(400, 975), wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxRESIZE_BORDER | wxFRAME_FLOAT_ON_PARENT),
	m_prev_snapshot({}), m_show_double_values(true), m_context_ctrl(nullptr)
{
	SetSizeHints(wxDefaultSize, wxDefaultSize);
	SetMaxSize({ 400, 975 });
	wxWindowBase::SetBackgroundColour(*wxWHITE);
	
	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

	auto scrolled_win = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

	auto gpr_sizer = new wxFlexGridSizer(0, 3, 0, 0);
	
	// GPRs
	for(sint32 i = 0; i < 32; ++i)
	{
		gpr_sizer->Add(new wxStaticText(scrolled_win, wxID_ANY, wxString::Format("R%d", i)), 0, wxLEFT, 5);

		auto value = new wxTextCtrl(scrolled_win, kRegisterValueR0 + i, wxString::Format("%08x", 0), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxNO_BORDER);
		value->SetBackgroundColour(*wxWHITE);
		value->Bind(wxEVT_LEFT_DCLICK, &RegisterWindow::OnMouseDClickEvent, this);
		//value->Bind(wxEVT_CONTEXT_MENU, &RegisterWindow::OnValueContextMenu, this);
		gpr_sizer->Add(value, 0, wxLEFT|wxRIGHT, 5);

		auto label = new wxTextCtrl(scrolled_win, kRegisterLabelR0 + i, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxNO_BORDER);
		label->SetMinSize(wxSize(500, -1));
		label->SetBackgroundColour(*wxWHITE);
		gpr_sizer->Add(label, 0, wxEXPAND);
	}

	{
		// LR
		gpr_sizer->Add(new wxStaticText(scrolled_win, wxID_ANY, wxString::Format("LR")), 0, wxLEFT, 5);
		auto value = new wxTextCtrl(scrolled_win, kRegisterValueLR, wxString::Format("%08x", 0), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxNO_BORDER);
		value->SetBackgroundColour(*wxWHITE);
		value->Bind(wxEVT_LEFT_DCLICK, &RegisterWindow::OnMouseDClickEvent, this);
		//value->Bind(wxEVT_CONTEXT_MENU, &RegisterWindow::OnValueContextMenu, this);
		gpr_sizer->Add(value, 0, wxLEFT | wxRIGHT, 5);
		auto label = new wxTextCtrl(scrolled_win, kRegisterLabelLR, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxNO_BORDER);
		label->SetMinSize(wxSize(500, -1));
		label->SetBackgroundColour(*wxWHITE);
		gpr_sizer->Add(label, 0, wxEXPAND);
	}

	sizer->Add(gpr_sizer, 1, wxEXPAND);

	auto button = new wxButton(scrolled_win, wxID_ANY, _("FP view mode"));
	button->Bind(wxEVT_BUTTON, &RegisterWindow::OnFPViewModePress, this);
	sizer->Add(button, 0, wxALL, 5);

	auto fp_sizer = new wxFlexGridSizer(0, 3, 0, 0);
	for (sint32 i = 0; i < 32; ++i)
	{
		fp_sizer->Add(new wxStaticText(scrolled_win, wxID_ANY, wxString::Format("FP%d", i)), 0, wxLEFT, 5);

		auto value0 = new wxTextCtrl(scrolled_win, kRegisterValueFPR0_0 + i, wxString::Format("%lf", 0.0), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxNO_BORDER);
		value0->SetBackgroundColour(*wxWHITE);
		value0->Bind(wxEVT_LEFT_DCLICK, &RegisterWindow::OnMouseDClickEvent, this);
		fp_sizer->Add(value0, 0, wxLEFT | wxRIGHT, 5);

		auto value1 = new wxTextCtrl(scrolled_win, kRegisterValueFPR1_0 + i, wxString::Format("%lf", 0.0), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxNO_BORDER);
		value1->SetBackgroundColour(*wxWHITE);
		value1->Bind(wxEVT_LEFT_DCLICK, &RegisterWindow::OnMouseDClickEvent, this);
		fp_sizer->Add(value1, 0, wxLEFT | wxRIGHT, 5);
	}

	sizer->Add(fp_sizer, 0, wxEXPAND);
	auto cr_sizer = new wxFlexGridSizer(0, 2, 0, 0);

	// CRs
	for (sint32 i = 0; i < 8; ++i)
	{
		cr_sizer->Add(new wxStaticText(scrolled_win, wxID_ANY, wxString::Format("CR%d", i)), 0, wxLEFT, 5);
		auto value = new wxTextCtrl(scrolled_win, kRegisterValueCR0 + i, "-", wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxNO_BORDER);
		value->SetBackgroundColour(*wxWHITE);
		//value->Bind(wxEVT_CONTEXT_MENU, &RegisterWindow::OnValueContextMenu, this);
		cr_sizer->Add(value, 0, wxRIGHT, 5);
	}

	sizer->Add(cr_sizer, 0, wxEXPAND);
	scrolled_win->SetSizerAndFit(sizer);
	scrolled_win->SetScrollRate(0, GetCharWidth());

	main_sizer->Add(scrolled_win, 1, wxEXPAND);
	SetSizer(main_sizer);
	Layout();

	if (parent.GetConfig().data().pin_to_main)
		OnMainMove(main_position, main_size);

	//Bind(wxEVT_COMMAND_MENU_SELECTED, &RegisterWindow::OnValueContextMenuSelected, this, kContextMenuZero, kContextMenuGotoDump);
}


void RegisterWindow::OnMainMove(const wxPoint& main_position, const wxSize& main_size)
{
	wxSize size(400, 255 + main_size.y + 250);
	this->SetSize(size);

	wxPoint position = main_position;
	position.x += main_size.x;
	position.y -= 255;
	this->SetPosition(position);
}

void RegisterWindow::UpdateIntegerRegister(wxTextCtrl* label, wxTextCtrl* value, uint32 registerValue, bool hasChanged)
{
	//const auto value = dynamic_cast<wxTextCtrl*>(GetWindowChild(kRegisterValueR0 + i));
	//wxASSERT(value);

	//const bool has_changed = register_value != m_prev_snapshot.gpr[i];
	if (hasChanged)
		value->SetForegroundColour(COLOR_RED);
	else if (value->GetForegroundColour() != COLOR_BLACK)
		value->SetForegroundColour(COLOR_BLACK);

	value->ChangeValue(wxString::Format("%08x", registerValue));

	//const auto label = dynamic_cast<wxTextCtrl*>(GetWindowChild(kRegisterLabelR0 + i));
	//wxASSERT(label);
	label->SetForegroundColour(hasChanged ? COLOR_RED : COLOR_BLACK);

	// check if address is a string
	if (registerValue >= MEMORY_DATA_AREA_ADDR && registerValue < (MEMORY_DATA_AREA_ADDR + MEMORY_DATA_AREA_SIZE))
	{
		bool is_valid_string = true;
		std::stringstream buffer;

		uint32 string_offset = registerValue;
		while (string_offset < (MEMORY_DATA_AREA_ADDR + MEMORY_DATA_AREA_SIZE))
		{
			const uint8 c = memory_readU8(string_offset++);
			if (isprint(c))
				buffer << c;
			else if (c == '\0')
			{
				buffer << c;
				break;
			}
			else
			{
				is_valid_string = false;
				break;
			}
		}

		if (is_valid_string && buffer.tellp() > 1)
		{
			label->ChangeValue(wxString::Format("\"%s\"", buffer.str().c_str()));
			return;
		}

		// check for widestring
		is_valid_string = true;
		string_offset = registerValue;
		std::wstringstream wbuffer;
		while (string_offset < (MEMORY_DATA_AREA_ADDR + MEMORY_DATA_AREA_SIZE - 1))
		{
			const uint16 c = memory_readU16(string_offset);
			string_offset += 2;

			if (iswprint(c))
				wbuffer << c;
			else if (c == L'\0')
			{
				wbuffer << c;
				break;
			}
			else
			{
				is_valid_string = false;
				break;
			}
		}

		if (is_valid_string && buffer.tellp() > 1)
		{
			label->ChangeValue(wxString::Format(L"ws\"%s\"", wbuffer.str().c_str()));
			return;
		}
	}

	// check if address is a code offset
	RPLModule* code_module = RPLLoader_FindModuleByCodeAddr(registerValue);
	if (code_module)
	{
		label->ChangeValue(wxString::Format("<%s> + %x", code_module->moduleName2.c_str(), registerValue - code_module->regionMappingBase_text.GetMPTR()));
		return;
	}

	label->ChangeValue(wxEmptyString);
}

void RegisterWindow::OnUpdateView()
{
	// m_register_ctrl->RefreshControl();
	for (int i = 0; i < 32; ++i)
	{
		const uint32 registerValue = debuggerState.debugSession.ppcSnapshot.gpr[i];
		const bool hasChanged = registerValue != m_prev_snapshot.gpr[i];
		const auto value = dynamic_cast<wxTextCtrl*>(FindWindow(kRegisterValueR0 + i));
		wxASSERT(value);
		const auto label = dynamic_cast<wxTextCtrl*>(FindWindow(kRegisterLabelR0 + i));
		wxASSERT(label);

		UpdateIntegerRegister(label, value, registerValue, hasChanged);
	}

	// update LR
	{
		const uint32 registerValue = debuggerState.debugSession.ppcSnapshot.spr_lr;
		const bool hasChanged = registerValue != m_prev_snapshot.spr_lr;
		const auto value = dynamic_cast<wxTextCtrl*>(FindWindow(kRegisterValueLR));
		wxASSERT(value);
		const auto label = dynamic_cast<wxTextCtrl*>(FindWindow(kRegisterLabelLR));
		wxASSERT(label);
		UpdateIntegerRegister(label, value, registerValue, hasChanged);
	}

	for (int i = 0; i < 32; ++i)
	{
		const uint64_t register_value = debuggerState.debugSession.ppcSnapshot.fpr[i].fp0int;

		const auto value = dynamic_cast<wxTextCtrl*>(FindWindow(kRegisterValueFPR0_0 + i));
		wxASSERT(value);

		const bool has_changed = register_value != m_prev_snapshot.fpr[i].fp0int;
		if (has_changed)
			value->SetForegroundColour(COLOR_RED);
		else if (value->GetForegroundColour() != COLOR_BLACK)
			value->SetForegroundColour(COLOR_BLACK);
		else
			continue;

		if(m_show_double_values)
			value->ChangeValue(wxString::Format("%lf", debuggerState.debugSession.ppcSnapshot.fpr[i].fp0));
		else
			value->ChangeValue(wxString::Format("%016llx", register_value));
	}

	for (int i = 0; i < 32; ++i)
	{
		const uint64_t register_value = debuggerState.debugSession.ppcSnapshot.fpr[i].fp1int;

		const auto value = dynamic_cast<wxTextCtrl*>(FindWindow(kRegisterValueFPR1_0 + i));
		wxASSERT(value);

		const bool has_changed = register_value != m_prev_snapshot.fpr[i].fp1int;
		if (has_changed)
			value->SetForegroundColour(COLOR_RED);
		else if (value->GetForegroundColour() != COLOR_BLACK)
			value->SetForegroundColour(COLOR_BLACK);
		else
			continue;

		if (m_show_double_values)
			value->ChangeValue(wxString::Format("%lf", debuggerState.debugSession.ppcSnapshot.fpr[i].fp1));
		else
			value->ChangeValue(wxString::Format("%016llx", register_value));
	}

	// update CRs
	for (int i = 0; i < 8; ++i)
	{
		const auto value = dynamic_cast<wxTextCtrl*>(FindWindow(kRegisterValueCR0 + i));
		wxASSERT(value);
		
		auto cr_bits_ptr = debuggerState.debugSession.ppcSnapshot.cr + i * 4;
		auto cr_bits_ptr_cmp = m_prev_snapshot.cr + i * 4;

		const bool has_changed = !std::equal(cr_bits_ptr, cr_bits_ptr + 4, cr_bits_ptr_cmp);
		if (has_changed)
			value->SetForegroundColour(COLOR_RED);
		else if (value->GetForegroundColour() != COLOR_BLACK)
			value->SetForegroundColour(COLOR_BLACK);
		else
			continue;

		std::vector<std::string> joinArray = {};
		if (cr_bits_ptr[Espresso::CR_BIT_INDEX_LT] != 0)
			joinArray.emplace_back("LT");
		if (cr_bits_ptr[Espresso::CR_BIT_INDEX_GT] != 0)
			joinArray.emplace_back("GT");
		if (cr_bits_ptr[Espresso::CR_BIT_INDEX_EQ] != 0)
			joinArray.emplace_back("EQ");
		if (cr_bits_ptr[Espresso::CR_BIT_INDEX_SO] != 0)
			joinArray.emplace_back("SO");

		if (joinArray.empty())
			value->ChangeValue("-");
		else
			value->ChangeValue(fmt::format("{}", fmt::join(joinArray, ", ")));
	}

	memcpy(&m_prev_snapshot, &debuggerState.debugSession.ppcSnapshot, sizeof(m_prev_snapshot));
}

void RegisterWindow::OnMouseDClickEvent(wxMouseEvent& event)
{
	if (!debuggerState.debugSession.isTrapped)
	{
		event.Skip();
		return;
	}

	const auto id = event.GetId();
	if(kRegisterValueR0 <= id && id < kRegisterValueR0 + 32)
	{
		const uint32 register_index = id - kRegisterValueR0;
		const uint32 register_value = debuggerState.debugSession.ppcSnapshot.gpr[register_index];
		wxTextEntryDialog set_value_dialog(this, _("Enter a new value."), _(wxString::Format("Set R%d value", register_index)), wxString::Format("%08x", register_value));
		if (set_value_dialog.ShowModal() == wxID_OK)
		{
			const uint32 new_value = std::stoul(set_value_dialog.GetValue().ToStdString(), nullptr, 16);
			debuggerState.debugSession.hCPU->gpr[register_index] = new_value;
			debuggerState.debugSession.ppcSnapshot.gpr[register_index] = new_value;
			OnUpdateView();
		}
				
		return;
	}

	if (kRegisterValueFPR0_0 <= id && id < kRegisterValueFPR0_0 + 32)
	{
		const uint32 register_index = id - kRegisterValueFPR0_0;
		const double register_value = debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp0;
		wxTextEntryDialog set_value_dialog(this, _("Enter a new value."), _(wxString::Format("Set FP0_%d value", register_index)), wxString::Format("%lf", register_value));
		if (set_value_dialog.ShowModal() == wxID_OK)
		{
			const double new_value = std::stod(set_value_dialog.GetValue().ToStdString());
			debuggerState.debugSession.hCPU->fpr[register_index].fp0 = new_value;
			debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp0 = new_value;
			OnUpdateView();
		}

		return;
	}

	if (kRegisterValueFPR1_0 <= id && id < kRegisterValueFPR1_0 + 32)
	{
		const uint32 register_index = id - kRegisterValueFPR1_0;
		const double register_value = debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp1;
		wxTextEntryDialog set_value_dialog(this, _("Enter a new value."), _(wxString::Format("Set FP1_%d value", register_index)), wxString::Format("%lf", register_value));
		if (set_value_dialog.ShowModal() == wxID_OK)
		{
			const double new_value = std::stod(set_value_dialog.GetValue().ToStdString());
			debuggerState.debugSession.hCPU->fpr[register_index].fp1 = new_value;
			debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp1 = new_value;
			OnUpdateView();
		}

		return;
	}

	event.Skip();
}

void RegisterWindow::OnFPViewModePress(wxCommandEvent& event)
{
	m_show_double_values = !m_show_double_values;
	
	for (int i = 0; i < 32; ++i)
	{
		const auto value0 = dynamic_cast<wxTextCtrl*>(FindWindow(kRegisterValueFPR0_0 + i));
		const auto value1 = dynamic_cast<wxTextCtrl*>(FindWindow(kRegisterValueFPR1_0 + i));
		wxASSERT(value0);
		wxASSERT(value1);

		if (m_show_double_values)
		{
			value0->ChangeValue(wxString::Format("%lf", debuggerState.debugSession.ppcSnapshot.fpr[i].fp0));
			value1->ChangeValue(wxString::Format("%lf", debuggerState.debugSession.ppcSnapshot.fpr[i].fp1));
		}
		else
		{
			value0->ChangeValue(wxString::Format("%016llx", debuggerState.debugSession.ppcSnapshot.fpr[i].fp0int));
			value1->ChangeValue(wxString::Format("%016llx", debuggerState.debugSession.ppcSnapshot.fpr[i].fp1int));
		}
	}
}

void RegisterWindow::OnValueContextMenu(wxContextMenuEvent& event)
{
	wxMenu menu;

	menu.Append(kContextMenuZero, _("&Zero"));
	menu.Append(kContextMenuInc, _("&Increment"));
	menu.Append(kContextMenuDec, _("&Decrement"));
	menu.AppendSeparator();
	menu.Append(kContextMenuCopy, _("&Copy"));
	menu.AppendSeparator();
	menu.Append(kContextMenuGotoDisasm, _("&Goto Disasm"));
	menu.Append(kContextMenuGotoDump, _("G&oto Dump"));

	m_context_ctrl = dynamic_cast<wxTextCtrl*>(event.GetEventObject());

	PopupMenu(&menu);
}

void RegisterWindow::OnValueContextMenuSelected(wxCommandEvent& event)
{
	wxASSERT(m_context_ctrl);

	switch (event.GetId()) 
	{
	case kContextMenuZero:
		break;
	case kContextMenuInc:
		break;
	case kContextMenuDec:
		break;
	case kContextMenuCopy:
		break;
	case kContextMenuGotoDisasm:
		break;
	case kContextMenuGotoDump:
		break;
	}
}
