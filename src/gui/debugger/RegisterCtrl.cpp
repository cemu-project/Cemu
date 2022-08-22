#include "gui/wxgui.h"
#include "gui/debugger/RegisterCtrl.h"

#include <sstream>

#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"
#include "gui/debugger/DebuggerWindow2.h"

#define COLOR_DEBUG_ACTIVE_BP	0xFFFFA0FF
#define COLOR_DEBUG_ACTIVE		0xFFFFA080
#define COLOR_DEBUG_BP			0xFF8080FF

#define SYNTAX_COLOR_GPR		0xFF000066
#define SYNTAX_COLOR_FPR		0xFF006666
#define SYNTAX_COLOR_SPR		0xFF666600
#define SYNTAX_COLOR_CR			0xFF666600
#define SYNTAX_COLOR_IMM		0xFF006600
#define SYNTAX_COLOR_IMM_OFFSET	0xFF006600
#define SYNTAX_COLOR_CIMM		0xFF880000
#define SYNTAX_COLOR_PSEUDO		0xFFA0A0A0 // color for pseudo code
#define SYNTAX_COLOR_SYMBOL		0xFF0000A0 // color for function symbol

#define OFFSET_REGISTER (60)
#define OFFSET_REGISTER_LABEL (100)
#define OFFSET_FPR (120)

RegisterCtrl::RegisterCtrl(wxWindow* parent, const wxWindowID& id, const wxPoint& pos, const wxSize& size, long style)
	: TextList(parent, id, pos, size, style), m_prev_snapshot({})
{
	this->SetElementCount(32 + 1 + 32);
	RefreshControl();
}

void RegisterCtrl::OnDraw(wxDC& dc, sint32 start, sint32 count, const wxPoint& start_position)
{
	wxPoint position = start_position;

	//RPLModule* current_rpl_module = rpl3_getModuleByCodeAddr(MEMORY_CODEAREA_ADDR + start);
	for (sint32 i = 0; i <= count; i++)
	{
		const uint32 line = start + i;

		if (line < 32)
		{
			dc.SetTextForeground(COLOR_BLACK);
			dc.DrawText(wxString::Format("R%d", line), position);
			position.x += OFFSET_REGISTER;

			const uint32 register_value = debuggerState.debugSession.ppcSnapshot.gpr[line];
			if(register_value != m_prev_snapshot.gpr[line])
				dc.SetTextForeground(COLOR_RED);
			else
				dc.SetTextForeground(COLOR_BLACK);

			dc.DrawText(wxString::Format("%08x", register_value), position);
			position.x += OFFSET_REGISTER_LABEL;

			// check if address is a code offset
			RPLModule* code_module = RPLLoader_FindModuleByCodeAddr(register_value);
			if (code_module)
				dc.DrawText(wxString::Format("<%s> + %x", code_module->moduleName2.c_str(), register_value - code_module->regionMappingBase_text.GetMPTR()), position);

			// check if address is a string
			uint32 string_offset = register_value;
			if (string_offset >= MEMORY_DATA_AREA_ADDR && string_offset < (MEMORY_DATA_AREA_ADDR+MEMORY_DATA_AREA_SIZE) )
			{
				bool is_valid_string = true;
				std::stringstream buffer;

				uint32 string_offset = register_value;
				while (string_offset < (MEMORY_DATA_AREA_ADDR + MEMORY_DATA_AREA_SIZE))
				{
					const uint8 c = memory_readU8(string_offset++);
					if (isprint(c))
					{
						buffer << c;
					}
					else if( c == '\0' )
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

				if(is_valid_string && buffer.tellp() > 1)
				{
					dc.DrawText(wxString::Format("s \"%s\"", buffer.str().c_str()), position);
				}
				else
				{
					// check for widestring
					is_valid_string = true;
					string_offset = register_value;
					std::wstringstream wbuffer;
					while (string_offset < (MEMORY_DATA_AREA_ADDR + MEMORY_DATA_AREA_SIZE))
					{
						const uint16 c = memory_readU16(string_offset);
						string_offset += 2;

						if (iswprint(c))
						{
							wbuffer << c;
						}
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
						dc.DrawText(wxString::Format(L"ws \"%s\"", wbuffer.str().c_str()), position);
					}
				}
			}
		}
		else if( 32 + 1 <= line && line <= 32 + 1 + 32)
		{
			const uint32 register_index = line - 32 - 1;
			dc.SetTextForeground(COLOR_BLACK);
			dc.DrawText(wxString::Format("F0_%d", register_index), position);
			position.x += OFFSET_REGISTER;

			const uint64 fpr0_value = debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp0int;
			if (fpr0_value != m_prev_snapshot.fpr[register_index].fp0int)
				dc.SetTextForeground(COLOR_RED);
			else
				dc.SetTextForeground(COLOR_BLACK);

			//dc.DrawText(wxString::Format("%016llx", fpr0_value), position);
			dc.DrawText(wxString::Format("%lf", debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp0), position);

			position.x += OFFSET_FPR;

			dc.SetTextForeground(COLOR_BLACK);
			dc.DrawText(wxString::Format("F1_%d", register_index), position);
			position.x += OFFSET_REGISTER;

			const uint64 fpr1_value = debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp1int;
			if (fpr1_value != m_prev_snapshot.fpr[register_index].fp1int)
				dc.SetTextForeground(COLOR_RED);
			else
				dc.SetTextForeground(COLOR_BLACK);

			//dc.DrawText(wxString::Format("%016llx", fpr1_value), position);
			dc.DrawText(wxString::Format("%lf", debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp1), position);
		}
		else
		{
			// empty line
		}
			
		NextLine(position, &start_position);
	}

	memcpy(&m_prev_snapshot, &debuggerState.debugSession.ppcSnapshot, sizeof(m_prev_snapshot));
}

void RegisterCtrl::OnMouseMove(const wxPoint& position, uint32 line)
{
	if (!m_mouse_down)
		return;

	wxPoint pos = position;
	if(pos.x <= OFFSET_REGISTER)
	{

	}

	pos.x -= OFFSET_REGISTER;

	if (pos.x <= OFFSET_REGISTER_LABEL)
	{

	}

	pos.x -= OFFSET_REGISTER_LABEL;
}

void RegisterCtrl::OnMouseDClick(const wxPoint& position, uint32 line)
{
	if (!debuggerState.debugSession.isTrapped)
		return;

	if(line < 32)
	{
		const uint32 register_index = line;
		if (position.x <= OFFSET_REGISTER + OFFSET_REGISTER_LABEL)
		{
			const uint32 register_value = debuggerState.debugSession.ppcSnapshot.gpr[register_index];
			wxTextEntryDialog set_value_dialog(this, _("Enter a new value."), _(wxString::Format("Set R%d value", register_index)), wxString::Format("%08x", register_value));
			if (set_value_dialog.ShowModal() == wxID_OK)
			{
				const uint32 new_value = std::stoul(set_value_dialog.GetValue().ToStdString(), nullptr, 16);
				debuggerState.debugSession.hCPU->gpr[register_index] = new_value;
				debuggerState.debugSession.ppcSnapshot.gpr[register_index] = new_value;

				wxRect update_rect(0, line * m_line_height, GetSize().x, m_line_height);
				RefreshControl(&update_rect);
			}
		}
	}
	// one line empty between = line 32
	else if (32 + 1 <= line && line <= 32 + 1 + 32)
	{
		const uint32 register_index = line - 32 - 1;
		if (position.x <= OFFSET_REGISTER + OFFSET_FPR)
		{
			const double register_value = debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp0;
			wxTextEntryDialog set_value_dialog(this, _("Enter a new value."), _(wxString::Format("Set FP0_%d value", register_index)), wxString::Format("%lf", register_value));
			if (set_value_dialog.ShowModal() == wxID_OK)
			{
				const double new_value = std::stod(set_value_dialog.GetValue().ToStdString());
				debuggerState.debugSession.hCPU->fpr[register_index].fp0 = new_value;
				debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp0 = new_value;

				wxRect update_rect(0, line * m_line_height, GetSize().x, m_line_height);
				RefreshControl(&update_rect);
			}
		}
		else
		{
			const double register_value = debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp1;
			wxTextEntryDialog set_value_dialog(this, _("Enter a new value."), _(wxString::Format("Set FP1_%d value", register_index)), wxString::Format("%lf", register_value));
			if (set_value_dialog.ShowModal() == wxID_OK)
			{
				const double new_value = std::stod(set_value_dialog.GetValue().ToStdString());
				debuggerState.debugSession.hCPU->fpr[register_index].fp1 = new_value;
				debuggerState.debugSession.ppcSnapshot.fpr[register_index].fp1 = new_value;

				wxRect update_rect(0, line * m_line_height, GetSize().x, m_line_height);
				RefreshControl(&update_rect);
			}
		}
	}
}

void RegisterCtrl::OnGameLoaded()
{
	RefreshControl();
}

