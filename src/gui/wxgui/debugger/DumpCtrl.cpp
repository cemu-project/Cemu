#include "wxgui/wxgui.h"
#include "wxgui/debugger/DumpCtrl.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"
#include "util/helpers/helpers.h"

#include "Cemu/ExpressionParser/ExpressionParser.h"


#define OFFSET_ADDRESS (60)
#define OFFSET_ADDRESS_RELATIVE (90)
#define OFFSET_MEMORY (450)

enum {
	ID_WRITE_U8 = wxID_HIGHEST + 1,
	ID_WRITE_U16,
	ID_WRITE_U32,
	ID_WRITE_FLOAT,
	ID_WRITE_STRING
};

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

	Bind(wxEVT_MENU, &DumpCtrl::OnMenuSelected, this);
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
	RPLModule* currentCodeRPL = RPLLoader_FindModuleByCodeAddr(LineToOffset(start));
	RPLModule* currentDataRPL = RPLLoader_FindModuleByDataAddr(LineToOffset(start));
	for (sint32 i = 0; i <= count; i++)
	{
		const uint32 virtual_address = LineToOffset(start + i);

		dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
		dc.DrawText(wxString::Format("%08x", virtual_address), position);
		position.x += OFFSET_ADDRESS;

		dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
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
			dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
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
	dc.SetPen(wxSystemSettings::GetColour(wxSYS_COLOUR_INACTIVECAPTIONTEXT));
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

uint32 DumpCtrl::PositionToAddress(const wxPoint& position, uint32 line)
{
    wxPoint pos = position;

    if (pos.x <= OFFSET_ADDRESS + OFFSET_ADDRESS_RELATIVE)
        return MPTR_NULL;

    pos.x -= OFFSET_ADDRESS + OFFSET_ADDRESS_RELATIVE;

    if (pos.x > OFFSET_MEMORY)
        return MPTR_NULL;

    const uint32 byteIndex = (pos.x / m_char_width) / 3;
    return LineToOffset(line) + byteIndex;
}

void DumpCtrl::OnMouseDClick(const wxPoint& position, uint32 line)
{
	uint32 address = PositionToAddress(position, line);

	if (address == MPTR_NULL)
		return;

	if (!memory_isAddressRangeAccessible(address, 1))
		return;

	if (WriteNumericDialog<uint8>(address))
	{
		wxRect updateRect(0, line * m_line_height, GetSize().x, m_line_height);
		RefreshControl(&updateRect);
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

void DumpCtrl::OnContextMenu(const wxPoint& position, uint32 line)
{
	wxPoint pos = position;

	if (pos.x <= OFFSET_ADDRESS + OFFSET_ADDRESS_RELATIVE)
		return;

	pos.x -= OFFSET_ADDRESS + OFFSET_ADDRESS_RELATIVE;

	if (pos.x > OFFSET_MEMORY)
		return;

	const uint32 byteIndex = (pos.x / m_char_width) / 3;
	const uint32 address = LineToOffset(line) + byteIndex;

	if (!memory_isAddressRangeAccessible(address, 1))
		return;

	m_writerContextAddress = address;
	m_writerContextLine = line;

	wxMenu menu;

	menu.Append(ID_WRITE_U8, "Write Byte");
	menu.Append(ID_WRITE_U16, "Write Int16");
	menu.Append(ID_WRITE_U32, "Write Int32");
	menu.Append(ID_WRITE_FLOAT, "Write Float");
	menu.Append(ID_WRITE_STRING, "Write String");

	PopupMenu(&menu);
}

void DumpCtrl::OnMenuSelected(wxCommandEvent& event)
{
	bool update = false;
	switch (event.GetId())
	{
		case ID_WRITE_U8:
			update = WriteNumericDialog<uint8>(m_writerContextAddress);
			break;
		case ID_WRITE_U16:
			update = WriteNumericDialog<uint16>(m_writerContextAddress);
			break;
		case ID_WRITE_U32:
			update = WriteNumericDialog<uint32>(m_writerContextAddress);
			break;
		case ID_WRITE_FLOAT:
			update = WriteNumericDialog<float>(m_writerContextAddress);
			break;
		case ID_WRITE_STRING:
			update = WriteString(m_writerContextAddress);
			break;
	}

	if (update)
	{
		wxRect updateRect(0, m_writerContextLine * m_line_height, GetSize().x, m_line_height);
		RefreshControl(&updateRect);
	}
}

template <typename T>
bool DumpCtrl::WriteNumericDialog(uint32 address)
{
	static_assert(
		std::is_same<T, uint8>::value  ||
		std::is_same<T, uint16>::value ||
		std::is_same<T, uint32>::value ||
		std::is_same<T, float>::value,
		"Unsupported type"
	);

	T value;
	const char* dataType;
	wxString label;

	if constexpr (std::is_same<T, uint8>::value)
	{
		value = memory_readU8(address);
		dataType = "byte";
		label = wxString::Format("0x%02x", value);
	}
	else if constexpr (std::is_same<T, uint16>::value)
	{
		value = memory_readU16(address);
		dataType = "int16";
		label = wxString::Format("0x%04x", value);
	}
	else if constexpr (std::is_same<T, uint32>::value)
	{
		value = memory_readU32(address);
		dataType = "int32";
		label = wxString::Format("0x%08x", value);
	}
	else if constexpr (std::is_same<T, float>::value)
	{
		value = memory_readFloat(address);
		dataType = "float";
		label = wxString::Format("%f", value);
	}

	wxTextEntryDialog dialog(
		this,
		_("Enter a new value."),
		wxString::Format(_("Write %s at address 0x%08x"), dataType, address),
		label
	);

	if (dialog.ShowModal() != wxID_OK)
		return false;
	
	
	const T newValue = ConvertString<T>(dialog.GetValue().ToStdString());

	if constexpr (std::is_same<T, uint8>::value)
		memory_writeU8(address, newValue);
	else if constexpr (std::is_same<T, uint16>::value)
		memory_writeU16(address, newValue);
	else if constexpr (std::is_same<T, uint32>::value)
		memory_writeU32(address, newValue);
	else if constexpr (std::is_same<T, float>::value)
		memory_writeFloat(address, newValue);
	
	return true;
}

bool DumpCtrl::WriteString(uint32 address)
{
	wxTextEntryDialog dialog(
		this,
		_("Enter string"),
		wxString::Format(_("Write string at address 0x%08x"), address),
		""
	);

	if (dialog.ShowModal() != wxID_OK)
		return false;

	std::string text = dialog.GetValue().ToStdString();

	if (text.empty())
		return false;

    for (size_t i = 0; i < text.size(); i++)
        memory_writeU8(address + i, static_cast<uint8>(text[i]));

	// null-terminator
    memory_writeU8(address + text.size(), 0);

	return true;
}
