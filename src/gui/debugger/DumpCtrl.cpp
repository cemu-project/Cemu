#include "gui/wxgui.h"
#include "gui/debugger/DumpCtrl.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"
#include "util/helpers/helpers.h"

#include "Cemu/ExpressionParser/ExpressionParser.h"

#define COLOR_BLACK				0xFF000000
#define COLOR_GREY				0xFFA0A0A0
#define COLOR_WHITE				0xFFFFFFFF

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

#define OFFSET_ADDRESS (60)
#define OFFSET_ADDRESS_RELATIVE (90)
#define OFFSET_MEMORY (450)

#define OFFSET_DISASSEMBLY_OPERAND (80)


DumpCtrl::DumpCtrl(wxWindow* parent, const wxWindowID& id, const wxPoint& pos, const wxSize& size, long style)
	: TextList(parent, id, pos, size, style)
{
	MMURange* range = memory_getMMURangeByAddress(0x10000000);
	if (range)
	{
		m_memoryRegion.baseAddress = range->getBase();
		m_memoryRegion.size = range->getSize();
		Init();
	}
	else
	{
		m_memoryRegion.baseAddress = 0x10000000;
		m_memoryRegion.size = 0x1000;
		Init();
	}
}

void DumpCtrl::Init()
{
	uint32 element_count = m_memoryRegion.size;
	this->SetElementCount(element_count / 0x10);
	Scroll(0, 0);
	RefreshControl();
}

void DumpCtrl::OnDraw(wxDC& dc, sint32 start, sint32 count, const wxPoint& start_position)
{
	wxPoint position = start_position;
	uint32 endAddr = m_memoryRegion.baseAddress + m_memoryRegion.size;
	RPLModule* currentCodeRPL = RPLLoader_FindModuleByCodeAddr(m_memoryRegion.baseAddress + start);
	RPLModule* currentDataRPL = RPLLoader_FindModuleByDataAddr(m_memoryRegion.baseAddress + start);
	for (sint32 i = 0; i <= count; i++)
	{
		const uint32 virtual_address = m_memoryRegion.baseAddress + (start + i) * 0x10;

		dc.SetTextForeground(wxColour(COLOR_BLACK));
		dc.DrawText(wxString::Format("%08x", virtual_address), position);
		position.x += OFFSET_ADDRESS;

		dc.SetTextForeground(wxColour(COLOR_GREY));
		if (currentCodeRPL)
		{
			dc.DrawText(wxString::Format("+0x%-8x", virtual_address - currentCodeRPL->regionMappingBase_text.GetMPTR()), position);
		}
		else if (currentDataRPL)
		{
			dc.DrawText(wxString::Format("+0x%-8x", virtual_address - currentDataRPL->regionMappingBase_data), position);
		}
		else
		{
			dc.DrawText("???", position);
		}

		position.x += OFFSET_ADDRESS_RELATIVE;

		sint32 start_width = position.x;
		
		if (!memory_isAddressRangeAccessible(virtual_address, 0x10))
		{
			for (sint32 f=0; f<0x10; f++)
			{
				wxPoint p(position);
				WriteText(dc, wxString::Format("?? "), p);
				position.x += (m_char_width * 3);
			}
		}
		else
		{
			std::array<uint8, 0x10> data;
			memory_readBytes(virtual_address, data);
			for (auto b : data)
			{
				wxPoint p(position);
				WriteText(dc, wxString::Format("%02x ", b), p);
				position.x += (m_char_width * 3);
			}
			position.x = start_width = OFFSET_MEMORY;
			dc.SetTextForeground(wxColour(COLOR_BLACK));
			for (auto b : data)
			{
				if (isprint(b))
					dc.DrawText(wxString::Format("%c ", b), position);
				else
					dc.DrawText(".", position);

				position.x += m_char_width;
			}
		}

		// display goto indicator
		if (m_lastGotoOffset >= virtual_address && m_lastGotoOffset < (virtual_address + 16))
		{
			sint32 indicatorX = start_position.x + OFFSET_ADDRESS + OFFSET_ADDRESS_RELATIVE + (m_lastGotoOffset - virtual_address) * m_char_width * 3;
			wxPoint line1(start_position.x + indicatorX - 2, position.y);
			wxPoint line2(line1.x, line1.y + m_line_height);
			dc.SetPen(*wxRED_PEN);
			dc.DrawLine(line1, line2);
			dc.DrawLine(line1, wxPoint(line1.x + 3, line1.y));
			dc.DrawLine(line2, wxPoint(line2.x + 3, line2.y));
		}

		NextLine(position, &start_position);
	}

	// draw vertical separator lines for 4 byte blocks
	sint32 cursorOffsetHexBytes = start_position.x + OFFSET_ADDRESS + OFFSET_ADDRESS_RELATIVE;
	wxPoint line_from(
		cursorOffsetHexBytes + m_char_width * (3 * 4 - 1) + m_char_width / 2,
		start_position.y
	);
	wxPoint line_to(line_from.x, line_from.y + m_line_height * (count + 1));
	dc.SetPen(*wxLIGHT_GREY_PEN);
	for (sint32 i = 0; i < 3; i++)
	{
		dc.DrawLine(line_from, line_to);
		line_from.x += m_char_width * (3 * 4);
		line_to.x += m_char_width * (3 * 4);
	}
}

void DumpCtrl::OnMouseMove(const wxPoint& start_position, uint32 line)
{
	wxPoint position = start_position;

	// address
	if (position.x <= OFFSET_ADDRESS)
	{
		return;
	}

	position.x -= OFFSET_ADDRESS;

	// relative offset
	if (position.x <= OFFSET_ADDRESS_RELATIVE)
	{
		return;
	}

	position.x -= OFFSET_ADDRESS_RELATIVE;

	// byte code
	if (position.x <= OFFSET_MEMORY)
	{
		
	}

	// string view
	position.x -= OFFSET_MEMORY;
}

void DumpCtrl::OnMouseDClick(const wxPoint& position, uint32 line)
{
	wxPoint pos = position;
	if (pos.x <= OFFSET_ADDRESS + OFFSET_ADDRESS_RELATIVE)
		return;
	 
	pos.x -= OFFSET_ADDRESS + OFFSET_ADDRESS_RELATIVE;
	if(pos.x <= OFFSET_MEMORY)
	{
		const uint32 byte_index = (pos.x / m_char_width) / 3;
		const uint32 offset = LineToOffset(line) + byte_index;
		const uint8 value = memory_readU8(offset);

		wxTextEntryDialog set_value_dialog(this, _("Enter a new value."), _(wxString::Format("Set byte at address %08x", offset)), wxString::Format("%02x", value));
		if (set_value_dialog.ShowModal() == wxID_OK)
		{
			const uint8 new_value = std::stoul(set_value_dialog.GetValue().ToStdString(), nullptr, 16);
			memory_writeU8(offset, new_value);
			wxRect update_rect(0, line * m_line_height, GetSize().x, m_line_height);
			RefreshControl(&update_rect);
		}

		return;
	}
}

void DumpCtrl::GoToAddressDialog()
{
	wxTextEntryDialog goto_dialog(this, _("Enter a target address."), _("GoTo address"), wxEmptyString);
	if (goto_dialog.ShowModal() == wxID_OK)
	{
		ExpressionParser parser;

		auto value = goto_dialog.GetValue().ToStdString();
		std::transform(value.begin(), value.end(), value.begin(), tolower);

		debugger_addParserSymbols(parser);

		// try to parse expression as hex value first (it should interpret 1234 as 0x1234, not 1234)
		if (parser.IsConstantExpression("0x"+value))
		{
			const auto result = (uint32)parser.Evaluate("0x"+value);
			m_lastGotoOffset = result;
			CenterOffset(result);
		}
		else if (parser.IsConstantExpression(value))
		{
			const auto result = (uint32)parser.Evaluate(value);
			m_lastGotoOffset = result;
			CenterOffset(result);
		}
		else
		{
			try
			{
				const auto _ = (uint32)parser.Evaluate(value);
			}
			catch (const std::exception& ex)
			{
				wxMessageBox(ex.what(), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
			}
		}
	}
}

void DumpCtrl::CenterOffset(uint32 offset)
{
	// check if the offset is valid
	if (!memory_isAddressRangeAccessible(offset, 1))
		return;
	// set region and line
	MMURange* range = memory_getMMURangeByAddress(offset);
	if (m_memoryRegion.baseAddress != range->getBase() || m_memoryRegion.size != range->getSize())
	{
		m_memoryRegion.baseAddress = range->getBase();
		m_memoryRegion.size = range->getSize();
		Init();
	}
	
	const sint32 line = OffsetToLine(offset);
	if (line < 0 || line >= (sint32)m_element_count)
		return;

	DoScroll(0, std::max(0, line - ((sint32)m_elements_visible / 2)));

	RefreshControl();
	//RefreshLine(line);

	debug_printf("scroll to %x\n", debuggerState.debugSession.instructionPointer);
}

uint32 DumpCtrl::LineToOffset(uint32 line)
{
	return m_memoryRegion.baseAddress + line * 0x10;
}

uint32 DumpCtrl::OffsetToLine(uint32 offset)
{
	return (offset - m_memoryRegion.baseAddress) / 0x10;
}


void DumpCtrl::OnKeyPressed(sint32 key_code, const wxPoint& position)
{
	switch (key_code)
	{
	case 'G':
	{
		if (IsKeyDown(WXK_CONTROL))
		{
			GoToAddressDialog();
			return;
		}
	}
	}
}

wxSize DumpCtrl::DoGetBestSize() const
{
	return TextList::DoGetBestSize();
}