#include "wxgui/wxgui.h"
#include "wxgui/debugger/DisasmCtrl.h"

#include "wxHelper.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_symbol_storage.h"
#include "Cafe/OS/RPL/rpl_debug_symbols.h"
#include "Cemu/PPCAssembler/ppcAssembler.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"
#include "wxgui/debugger/DebuggerWindow2.h"
#include "util/helpers/helpers.h"

#include "Cemu/ExpressionParser/ExpressionParser.h"
#include "Cafe/HW/Espresso/Debugger/DebugSymbolStorage.h"

wxDEFINE_EVENT(wxEVT_DISASMCTRL_NOTIFY_GOTO_ADDRESS, wxCommandEvent);

#define MAX_SYMBOL_LEN			(120)


constexpr uint8 arrowRightPNG[] =
{
	0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x0B,
	0x08, 0x03, 0x00, 0x00, 0x00, 0x41, 0x3C, 0xFD, 0x0B, 0x00, 0x00, 0x00,
	0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00,
	0x00, 0x04, 0x67, 0x41, 0x4D, 0x41, 0x00, 0x00, 0xB1, 0x8F, 0x0B, 0xFC,
	0x61, 0x05, 0x00, 0x00, 0x00, 0x06, 0x50, 0x4C, 0x54, 0x45, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xA5, 0x67, 0xB9, 0xCF, 0x00, 0x00, 0x00, 0x02,
	0x74, 0x52, 0x4E, 0x53, 0xFF, 0x00, 0xE5, 0xB7, 0x30, 0x4A, 0x00, 0x00,
	0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0E, 0xC3, 0x00, 0x00,
	0x0E, 0xC3, 0x01, 0xC7, 0x6F, 0xA8, 0x64, 0x00, 0x00, 0x00, 0x2C, 0x49,
	0x44, 0x41, 0x54, 0x18, 0x57, 0x63, 0x60, 0x84, 0x03, 0x08, 0x13, 0x59,
	0x00, 0xCC, 0x46, 0x11, 0x00, 0x71, 0x80, 0x24, 0x32, 0xC0, 0x10, 0x60,
	0xC0, 0x10, 0xC0, 0x00, 0x58, 0xCC, 0x80, 0xD8, 0x00, 0x02, 0x60, 0x3E,
	0x7E, 0x77, 0x00, 0x31, 0x23, 0x23, 0x00, 0x21, 0x95, 0x00, 0x5B, 0x20,
	0x73, 0x8D, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE,
	0x42, 0x60, 0x82
};
std::optional<wxBitmap> g_ipArrowBitmap;


static wxColour theme_textForeground;
static wxColour theme_textForegroundMuted;
static wxColour theme_background;

// background colors for lines in the disassembly view
static wxColour theme_lineBreakpointAndCurrentInstruction;
static wxColour theme_lineCurrentInstruction;
static wxColour theme_lineBreakpointSet;
static wxColour theme_lineLoggingBreakpointSet;
static wxColour theme_lineLastGotoAddress;

// text colors for addresses in the disassembly view
static wxColour theme_syntaxGPR;
static wxColour theme_syntaxFPR;
static wxColour theme_syntaxSPR;
static wxColour theme_syntaxCR;
static wxColour theme_syntaxIMM;
static wxColour theme_syntaxIMMOffset;
static wxColour theme_syntaxCallIMM;
static wxColour theme_syntaxPseudoOrUnknown;
static wxColour theme_syntaxSymbol;

static wxColour theme_opCode;
static wxColour theme_typeData;
static wxColour theme_patchedOpCode;
static wxColour theme_patchedData;


static void InitSyntaxColors()
{
	theme_textForeground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
	theme_textForegroundMuted = wxSystemSettings::GetAppearance().IsDark() ? wxColour(140,142,145) : wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
	theme_background = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

	// line background highlights
	theme_lineBreakpointSet = wxSystemSettings::SelectLightDark(wxColour(0xFF,0x80,0x80), wxColour(0x84,0x21,0x21)); // red for a non-current breakpoint
	theme_lineBreakpointAndCurrentInstruction = wxSystemSettings::SelectLightDark(wxColour(0xD2,0x7E,0xD2), wxColour(0x91,0x44,0xA4)); // pink for a current breakpoint
	theme_lineCurrentInstruction = wxSystemSettings::SelectLightDark(wxColour(0xFF,0xA0,0x80), wxColour(0x87,0x53,0x1A)); // light orange
	theme_lineLoggingBreakpointSet = wxSystemSettings::SelectLightDark(wxColour(0xAB,0xED,0xEE), wxColour(0x25,0x5D,0x6D)); // light blue
	theme_lineLastGotoAddress = wxSystemSettings::SelectLightDark(wxColour(0xE0,0xE0,0xE0), wxColour(0x40,0x40,0x40)); // very light gray to indicate the line that was last jumped to

	// disassembly syntax colors
	theme_syntaxGPR = wxSystemSettings::SelectLightDark(wxColour(0x66,0x00,0x00), wxColour(0xE0,0x6C,0x75));
	theme_syntaxFPR = wxSystemSettings::SelectLightDark(wxColour(0x66,0x66,0x00), wxColour(0xD1,0x9A,0x66));
	theme_syntaxSPR = wxSystemSettings::SelectLightDark(wxColour(0x00,0x66,0x66), wxColour(0x56,0xB6,0xC2));
	theme_syntaxCR = wxSystemSettings::SelectLightDark(wxColour(0x00,0x66,0x66), wxColour(0x56,0xB6,0xC2));
	theme_syntaxIMM = wxSystemSettings::SelectLightDark(wxColour(0x00,0x66,0x00), wxColour(0x9D,0xDE,0x6F));
	theme_syntaxIMMOffset = wxSystemSettings::SelectLightDark(wxColour(0x00,0x66,0x00), wxColour(0x9D,0xDE,0x6F));
	theme_syntaxCallIMM = wxSystemSettings::SelectLightDark(wxColour(0x00,0x00,0x88), wxColour(0x61,0xAF,0xEF));
	theme_syntaxPseudoOrUnknown = wxSystemSettings::SelectLightDark(wxColour(0xA0,0xA0,0xA0), wxColour(0x5C,0x63,0x70));
	theme_syntaxSymbol = wxSystemSettings::SelectLightDark(wxColour(0xA0,0x00,0x00), wxColour(0xE0,0x6C,0x75));

	// opcode & data highlighting
	theme_opCode = wxSystemSettings::SelectLightDark(wxColour(0xFF,0x00,0x40), wxColour(0xFF,0x70,0x7B));
	theme_patchedOpCode = wxSystemSettings::SelectLightDark(wxColour(0xFF,0x20,0x20), wxColour(0xCC,0x52,0xF5));
	theme_typeData = wxSystemSettings::SelectLightDark(wxColour(0xFF,0x20,0x20), wxColour(0xCE,0x91,0x78));
	theme_patchedData = wxSystemSettings::SelectLightDark(wxColour(0xFF,0x20,0x20), wxColour(0xF4,0x43,0x36));

	// theme the current instruction pointer arrow
	g_ipArrowBitmap = wxHelper::LoadThemedBitmapFromPNG(arrowRightPNG, sizeof(arrowRightPNG), wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
}


#define OFFSET_ADDRESS (60)
#define OFFSET_ADDRESS_RELATIVE (90)
#define OFFSET_DISASSEMBLY (300)

#define OFFSET_DISASSEMBLY_OPERAND (80)

DisasmCtrl::DisasmCtrl(wxWindow* parent, const wxWindowID& id, const wxPoint& pos, const wxSize& size, long style)
	: TextList(parent, id, pos, size, style), m_mouse_line(-1), m_mouse_line_drawn(-1), m_active_line(-1)
{
	Init();

	if (!g_ipArrowBitmap.has_value())
	{
		InitSyntaxColors();
	}

	auto tooltip_sizer = new wxBoxSizer(wxVERTICAL);
	tooltip_sizer->Add(new wxStaticText(m_tooltip_window, wxID_ANY, wxEmptyString), 0, wxALL, 5);
	m_tooltip_window->SetSizer(tooltip_sizer);

	Bind(wxEVT_MENU, &DisasmCtrl::OnContextMenuEntryClicked, this, IDContextMenu_ToggleBreakpoint, IDContextMenu_Last);
}

void DisasmCtrl::Init()
{
	SelectCodeRegion(mmuRange_TEXT_AREA.getBase());
}

void DisasmCtrl::SelectCodeRegion(uint32 newAddress)
{
	if (newAddress >= mmuRange_TEXT_AREA.getBase() && newAddress < mmuRange_TEXT_AREA.getEnd())
	{
		currentCodeRegionStart = MEMORY_CODEAREA_ADDR;
		currentCodeRegionEnd = RPLLoader_GetMaxCodeOffset();
		currentCodeRegionEnd = std::max(currentCodeRegionEnd, currentCodeRegionStart + 0x1000);
	}
	MMURange* mmuRange = memory_getMMURangeByAddress(newAddress);
	if (mmuRange)
	{
		currentCodeRegionStart = mmuRange->getBase();
		currentCodeRegionEnd = mmuRange->getEnd();
	}
	else
	{
		currentCodeRegionStart = 0;
		currentCodeRegionEnd = 0;
	}

	// update line tracking
	sint32 element_count = currentCodeRegionEnd - currentCodeRegionStart;
	// if (element_count <= 0x00010000)
	// 	element_count = 0x00010000;

	if (this->SetElementCount(element_count / 4))
	{
		Scroll(0, 0);
		RefreshControl();
	}
}

void DisasmCtrl::DrawDisassemblyLine(wxDC& dc, const wxPoint& linePosition, MPTR virtualAddress, RPLModule* rplModule)
{
	wxPoint position = linePosition;

	bool hasPatch = debugger_hasPatch(virtualAddress);

	PPCDisassembledInstruction disasmInstr;

	const DebuggerBreakpoint* bp = debugger_getFirstBP(virtualAddress);
	while (bp)
	{
		if (bp->isExecuteBP() && bp->enabled)
			break;
		bp = bp->next;
	}

	uint32 opcode;

	if (bp)
		opcode = bp->originalOpcodeValue;
	else
		opcode = memory_readU32(virtualAddress);

	ppcAssembler_disassemble(virtualAddress, opcode, &disasmInstr);

	const bool is_active_bp = debuggerState.debugSession.isTrapped && debuggerState.debugSession.instructionPointer == virtualAddress;

	// write virtual address
	wxColour background_colour;
	if (is_active_bp && bp != nullptr)
		background_colour = theme_lineBreakpointAndCurrentInstruction;
	else if (is_active_bp)
		background_colour = theme_lineCurrentInstruction;
	else if (bp != nullptr)
		background_colour = bp->bpType == DEBUGGER_BP_T_NORMAL ? theme_lineBreakpointSet : theme_lineLoggingBreakpointSet;
	else if(virtualAddress == m_lastGotoTarget)
		background_colour = theme_lineLastGotoAddress;
	else
		background_colour = theme_background;

	DrawLineBackground(dc, position, background_colour);

	dc.SetTextForeground(theme_textForeground);
	dc.DrawText(wxString::Format("%08x", virtualAddress), position);
	position.x += OFFSET_ADDRESS;

	dc.SetTextForeground(theme_textForegroundMuted);
	if (rplModule)
		dc.DrawText(wxString::Format("+0x%-8x", virtualAddress - rplModule->regionMappingBase_text.GetMPTR()), position);
	else
		dc.DrawText("???", position);

	position.x += OFFSET_ADDRESS_RELATIVE;

	// draw arrow to clearly indicate instruction pointer
	if(is_active_bp)
		dc.DrawBitmap(*g_ipArrowBitmap, wxPoint(position.x - 24, position.y + 2), false);

	// handle data symbols
	auto debugSymbolDataType = DebugSymbolStorage::GetDataType(virtualAddress);
	if (debugSymbolDataType == DEBUG_SYMBOL_TYPE::FLOAT)
	{
		dc.SetTextForeground(hasPatch ? theme_patchedData : theme_typeData);
		dc.DrawText(fmt::format(".float"), position);

		position.x += OFFSET_DISASSEMBLY_OPERAND;
		dc.SetTextForeground(hasPatch ? theme_patchedData : theme_syntaxIMM);
		dc.DrawText(fmt::format("{}", memory_readFloat(virtualAddress)), position);

		return;
	}
	else if (debugSymbolDataType == DEBUG_SYMBOL_TYPE::U32)
	{
		dc.SetTextForeground(hasPatch ? theme_patchedData : theme_typeData);
		dc.DrawText(fmt::format(".uint"), position);

		position.x += OFFSET_DISASSEMBLY_OPERAND;
		dc.SetTextForeground(hasPatch ? theme_patchedData : theme_syntaxIMM);
		dc.DrawText(fmt::format("{}", memory_readU32(virtualAddress)), position);

		return;
	}

	sint32 start_width = position.x;
	dc.SetTextForeground(hasPatch ? theme_patchedOpCode : theme_opCode);
	char opName[32];
	strcpy(opName, ppcAssembler_getInstructionName(disasmInstr.ppcAsmCode));
	std::transform(opName, opName + sizeof(opName), opName, tolower);
	dc.DrawText(wxString::Format("%-12s", opName), position);
	position.x += OFFSET_DISASSEMBLY_OPERAND;

	bool isRLWINM = disasmInstr.ppcAsmCode == PPCASM_OP_RLWINM || disasmInstr.ppcAsmCode == PPCASM_OP_RLWINM_ ||
		disasmInstr.ppcAsmCode == PPCASM_OP_CLRLWI || disasmInstr.ppcAsmCode == PPCASM_OP_CLRLWI_ ||
		disasmInstr.ppcAsmCode == PPCASM_OP_CLRRWI || disasmInstr.ppcAsmCode == PPCASM_OP_CLRRWI_ ||
		disasmInstr.ppcAsmCode == PPCASM_OP_EXTLWI || disasmInstr.ppcAsmCode == PPCASM_OP_EXTLWI_ ||
		disasmInstr.ppcAsmCode == PPCASM_OP_EXTRWI || disasmInstr.ppcAsmCode == PPCASM_OP_EXTRWI_ ||
		disasmInstr.ppcAsmCode == PPCASM_OP_SLWI || disasmInstr.ppcAsmCode == PPCASM_OP_SLWI_ ||
		disasmInstr.ppcAsmCode == PPCASM_OP_SRWI || disasmInstr.ppcAsmCode == PPCASM_OP_SRWI_ ||
		disasmInstr.ppcAsmCode == PPCASM_OP_ROTRWI || disasmInstr.ppcAsmCode == PPCASM_OP_ROTRWI_ ||
		disasmInstr.ppcAsmCode == PPCASM_OP_ROTLWI || disasmInstr.ppcAsmCode == PPCASM_OP_ROTLWI_;
	bool forceDecDisplay = isRLWINM;

	if (disasmInstr.ppcAsmCode == PPCASM_OP_UKN)
	{
		// show raw bytes
		WriteText(dc, wxString::Format("%02x %02x %02x %02x", (opcode >> 24) & 0xFF, (opcode >> 16) & 0xFF, (opcode >> 8) & 0xFF, (opcode >> 0) & 0xFF), position, theme_syntaxPseudoOrUnknown);
	}

	bool is_first_operand = true;
	for (sint32 o = 0; o < PPCASM_OPERAND_COUNT; o++)
	{
		if (((disasmInstr.operandMask >> o) & 1) == 0)
			continue;

		if (!is_first_operand)
			WriteText(dc, ", ", position, theme_textForeground);

		is_first_operand = false;
		switch (disasmInstr.operand[o].type)
		{
		case PPCASM_OPERAND_TYPE_GPR:
			WriteText(dc, wxString::Format("r%d", disasmInstr.operand[o].registerIndex), position, theme_syntaxGPR);
			break;

		case PPCASM_OPERAND_TYPE_FPR:
			WriteText(dc, wxString::Format("f%d", disasmInstr.operand[o].registerIndex), position, theme_syntaxFPR);
			break;

		case PPCASM_OPERAND_TYPE_SPR:
			WriteText(dc, wxString::Format("spr%d", disasmInstr.operand[o].registerIndex), position, theme_syntaxSPR);
			break;

		case PPCASM_OPERAND_TYPE_CR:
			WriteText(dc, wxString::Format("cr%d", disasmInstr.operand[o].registerIndex), position, theme_syntaxCR);
			break;

		case PPCASM_OPERAND_TYPE_IMM:
		{
			wxString string;
			if (disasmInstr.operand[o].isSignedImm)
			{
				sint32 sImm = disasmInstr.operand[o].immS32;
				if (disasmInstr.operand[o].immWidth == 16 && (sImm & 0x8000))
					sImm |= (sint32)0xFFFF0000;

				if ((sImm > -10 && sImm < 10) || forceDecDisplay)
					string = wxString::Format("%d", sImm);
				else
				{
					if (sImm < 0)
						string = wxString::Format("-0x%x", -sImm);
					else
						string = wxString::Format("0x%x", sImm);
				}
			}
			else
			{
				uint32 uImm = disasmInstr.operand[o].immS32;
				if ((uImm >= 0 && uImm < 10) || forceDecDisplay)
					string = wxString::Format("%u", uImm);
				else
					string = wxString::Format("0x%x", uImm);
			}

			WriteText(dc, string, position, theme_syntaxIMM);
			break;
		}
		case PPCASM_OPERAND_TYPE_PSQMODE:
		{
			if (disasmInstr.operand[o].immS32)
				WriteText(dc, "single", position, theme_syntaxIMM);
			else
				WriteText(dc, "paired", position, theme_syntaxIMM);
			break;
		}
		case PPCASM_OPERAND_TYPE_CIMM:
		{
			wxString string;
			// use symbol for function calls if available 
			uint32 callDest = disasmInstr.operand[o].immU32;
			RPLStoredSymbol* storedSymbol = nullptr;
			if (disasmInstr.ppcAsmCode == PPCASM_OP_BL || disasmInstr.ppcAsmCode == PPCASM_OP_BLA)
				storedSymbol = rplSymbolStorage_getByAddress(callDest);

			if (storedSymbol)
			{
				// if symbol is within same module then don't display libname prefix
				RPLModule* rplModuleCurrent = RPLLoader_FindModuleByCodeAddr(virtualAddress); // cache this
				if (rplModuleCurrent && callDest >= rplModuleCurrent->regionMappingBase_text.GetMPTR() && callDest < (rplModuleCurrent->regionMappingBase_text.GetMPTR() + rplModuleCurrent->regionSize_text))
					string = wxString((char*)storedSymbol->symbolName);
				else
					string = wxString::Format("%s.%s", (char*)storedSymbol->libName, (char*)storedSymbol->symbolName);

				// truncate name after 36 characters
				if (string.Length() >= 36)
				{
					string.Truncate(34);
					string.Append("..");
				}
			}
			else
			{
				string = wxString::Format("0x%08x", disasmInstr.operand[o].immU32);
			}

			WriteText(dc, string, position, theme_syntaxCallIMM);

			if (disasmInstr.ppcAsmCode != PPCASM_OP_BL)
			{
				wxString x = wxString(" ");
				if (callDest <= virtualAddress)
					x.Append(wxUniChar(0x2191)); // arrow up
				else
					x.Append(wxUniChar(0x2193)); // arrow down

				WriteText(dc, x, position, theme_textForeground);
			}

			break;
		}
		case PPCASM_OPERAND_TYPE_MEM:
		{
			// offset
			wxString string;
			if (disasmInstr.operand[o].isSignedImm && disasmInstr.operand[o].immS32 >= 0)
				string = wxString::Format("+0x%x", disasmInstr.operand[o].immS32);
			else
				string = wxString::Format("-0x%x", -disasmInstr.operand[o].immS32);

			WriteText(dc, string, position, theme_syntaxIMMOffset);
			WriteText(dc, "(", position, theme_textForeground);

			// register
			WriteText(dc, wxString::Format("r%d", disasmInstr.operand[o].registerIndex), position, theme_syntaxGPR);
			WriteText(dc, ")", position, theme_textForeground);
			break;
		}
		default:
			// TODO
			WriteText(dc, "<TODO>", position, wxColour(0xFF444444));
		}
	}

	position.x = start_width + OFFSET_DISASSEMBLY;
	const auto comment = static_cast<rplDebugSymbolComment*>(rplDebugSymbol_getForAddress(virtualAddress));
	if (comment && comment->type == RplDebugSymbolComment)
		WriteText(dc, comment->comment, position, theme_textForeground);
	else if (isRLWINM)
	{
		sint32 rS, rA, SH, MB, ME;
		rS = (opcode >> 21) & 0x1f;
		rA = (opcode >> 16) & 0x1f;
		SH = (opcode >> 11) & 0x1f;
		MB = (opcode >> 6) & 0x1f;
		ME = (opcode >> 1) & 0x1f;

		uint32 mask = ppcAssembler_generateMaskRLW(MB, ME);

		wxString string;
		if (SH == 0)
			string = wxString::Format("r%d=r%d&0x%x", rA, rS, mask);
		else if ((0xFFFFFFFF << (uint32)disasmInstr.operand[2].immS32) == mask)
			string = wxString::Format("r%d=r%d<<%d", rA, rS, SH);
		else if ((0xFFFFFFFF >> (32 - SH) == mask))
			string = wxString::Format("r%d=r%d>>%d", rA, rS, 32 - SH);
		else
			string = wxString::Format("r%d=(r%d<<<%d)&0x%x", rA, rS, SH, mask);
		WriteText(dc, string, position, theme_textForegroundMuted);
	}
	else if (disasmInstr.ppcAsmCode == PPCASM_OP_SUBF || disasmInstr.ppcAsmCode == PPCASM_OP_SUBF_)
	{
		sint32 rD, rA, rB;
		rD = (opcode >> 21) & 0x1f;
		rA = (opcode >> 16) & 0x1f;
		rB = (opcode >> 11) & 0x1f;

		wxString string;
		string = wxString::Format("r%d=r%d-r%d", rD, rB, rA);
		WriteText(dc, string, position, theme_textForegroundMuted);
	}
}

void DisasmCtrl::DrawLabelName(wxDC& dc, const wxPoint& linePosition, MPTR virtualAddress, RPLStoredSymbol* storedSymbol)
{
	wxString symbol_string = wxString::Format("%s:", (char*)storedSymbol->symbolName);
	if (symbol_string.Length() > MAX_SYMBOL_LEN)
	{
		symbol_string.Truncate(MAX_SYMBOL_LEN - 3);
		symbol_string.Append("..:");
	}
	wxPoint tmpPos(linePosition);
	WriteText(dc, symbol_string, tmpPos, theme_syntaxSymbol);
}

void DisasmCtrl::OnDraw(wxDC& dc, sint32 start, sint32 count, const wxPoint& start_position)
{
	wxPoint position(0, 0);

	RPLModule* current_rpl_module = RPLLoader_FindModuleByCodeAddr(GetViewBaseAddress());
	
	if (currentCodeRegionStart == currentCodeRegionEnd)
		return;

	sint32 viewFirstLine = GetViewStart().y;
	sint32 lineOffset = start - viewFirstLine;

	cemu_assert_debug(lineOffset >= 0);

	sint32 instructionIndex = 0;
	sint32 numLinesToUpdate = lineOffset + count;
	numLinesToUpdate = std::min(numLinesToUpdate, (sint32)m_elements_visible);

	if(m_lineToAddress.size() != m_elements_visible)
		m_lineToAddress.resize(m_elements_visible);

	sint32 lineIndex = 0;
	while(lineIndex < numLinesToUpdate)
	{
		const uint32 virtualAddress = GetViewBaseAddress() + instructionIndex * 4;
		instructionIndex++;

		if (virtualAddress < currentCodeRegionStart ||
			virtualAddress >= currentCodeRegionEnd)
		{
			NextLine(position, &start_position);
			m_lineToAddress[lineIndex] = std::nullopt;
			lineIndex++;
			continue;	
		}

		// check if valid memory address
		if (!memory_isAddressRangeAccessible(virtualAddress, 4))
		{
			NextLine(position, &start_position);
			m_lineToAddress[lineIndex] = std::nullopt;
			lineIndex++;
			continue;
		}

		// draw symbol as label
		RPLStoredSymbol* storedSymbol = rplSymbolStorage_getByAddress(virtualAddress);
		if (storedSymbol)
		{
			if(lineIndex >= lineOffset)
				DrawLabelName(dc, position, virtualAddress, storedSymbol);
			m_lineToAddress[lineIndex] = virtualAddress;
			lineIndex++;
			if (lineIndex >= numLinesToUpdate)
				break;
			NextLine(position, &start_position);
		}
		m_lineToAddress[lineIndex] = virtualAddress;
		if (lineIndex >= lineOffset)
			DrawDisassemblyLine(dc, position, virtualAddress, current_rpl_module);
		NextLine(position, &start_position);
		lineIndex++;
	}

	// draw vertical separator lines: offset | disassembly | comment
	dc.SetPen(*wxLIGHT_GREY_PEN);

	wxPoint line_from = start_position;
	line_from.x = OFFSET_ADDRESS + OFFSET_ADDRESS_RELATIVE - 5;
	wxPoint line_to = line_from;
	line_to.y += (count + 1) * m_line_height;
	dc.DrawLine(line_from, line_to);

	line_from.x += OFFSET_DISASSEMBLY;
	line_to.x += OFFSET_DISASSEMBLY;
	dc.DrawLine(line_from, line_to);
}

void DisasmCtrl::OnMouseMove(const wxPoint& start_position, uint32 line)
{
	/*m_mouse_line = line;
	if (m_mouse_line_drawn != -1)
		RefreshLine(m_mouse_line_drawn);
	if (m_mouse_line != -1)
		RefreshLine(m_mouse_line);*/

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

	// disassembly code
	if (position.x <= OFFSET_DISASSEMBLY)
	{
		if(m_mouse_down)
		{
			
		}

		if (position.x <= OFFSET_DISASSEMBLY_OPERAND)
		{
			return;
		}

		position.x -= OFFSET_DISASSEMBLY_OPERAND;
	}
}

void DisasmCtrl::OnKeyPressed(sint32 key_code, const wxPoint& position)
{
	auto optVirtualAddress = LinePixelPosToAddress(position.y);
	switch (key_code)
	{
		case WXK_F9:
		{
			if (optVirtualAddress)
			{
				debugger_toggleExecuteBreakpoint(*optVirtualAddress);

				wxCommandEvent evt(wxEVT_BREAKPOINT_CHANGE);
				wxPostEvent(this->m_parent, evt);
			}
			return;
		}
		case 'G':
		{
			if(IsKeyDown(WXK_CONTROL))
			{
				GoToAddressDialog();
				return;
			}
		}
	}


	// debugger currently in break state
	if (debuggerState.debugSession.isTrapped)
	{
		switch (key_code)
		{
		case WXK_F5:
			{
				debuggerState.debugSession.run = true;
				return;
			}
		case WXK_F10:
			{
				debuggerState.debugSession.stepOver = true;
				return;
			}
		case WXK_F11:
			{
				debuggerState.debugSession.stepInto = true;
			}
		}
	}
	else
	{
		switch (key_code)
		{
		case WXK_F5:
			{
				debuggerState.debugSession.shouldBreak = true;
			}
		}
	}
}

void DisasmCtrl::OnMouseDClick(const wxPoint& position, uint32 line)
{
	wxPoint pos = position;
	auto optVirtualAddress = LinePixelPosToAddress(position.y - GetViewStart().y * m_line_height);
	if (!optVirtualAddress)
		return;
	MPTR virtualAddress = *optVirtualAddress;

	// address
	if (pos.x <= OFFSET_ADDRESS + OFFSET_ADDRESS_RELATIVE)
	{
		// address + address relative
		debugger_toggleExecuteBreakpoint(virtualAddress);
		RefreshLine(line);
		wxCommandEvent evt(wxEVT_BREAKPOINT_CHANGE);
		wxPostEvent(this->m_parent, evt);
		return;
	}
	else if (pos.x <= OFFSET_ADDRESS + OFFSET_ADDRESS_RELATIVE + OFFSET_DISASSEMBLY)
	{
		// double-clicked on disassembly (operation and operand data)
		wxString currentInstruction = wxEmptyString;
		wxTextEntryDialog set_value_dialog(this, _("Enter a new instruction."), wxString::Format(_("Overwrite instruction at address %08x"), virtualAddress), currentInstruction);
		if (set_value_dialog.ShowModal() == wxID_OK)
		{
			PPCAssemblerInOut ctx = { 0 };
			ctx.virtualAddress = virtualAddress;
			if (ppcAssembler_assembleSingleInstruction(set_value_dialog.GetValue().c_str(), &ctx))
			{
				debugger_createPatch(virtualAddress, { ctx.outputData.data(), ctx.outputData.size() });
				RefreshLine(line);
			}
		}
		return;
	}
	else
	{
		// comment
		const auto comment = static_cast<rplDebugSymbolComment*>(rplDebugSymbol_getForAddress(virtualAddress));

		wxString old_comment = wxEmptyString;
		if (comment && comment->type == RplDebugSymbolComment)
			old_comment = comment->comment;

		wxTextEntryDialog set_value_dialog(this, _("Enter a new comment."), wxString::Format(_("Create comment at address %08x"), virtualAddress), old_comment);
		if (set_value_dialog.ShowModal() == wxID_OK)
		{
			rplDebugSymbol_createComment(virtualAddress, set_value_dialog.GetValue().wc_str());
			RefreshLine(line);
		}
		return;
	}
}

void DisasmCtrl::CopyToClipboard(std::string text)
{
#if BOOST_OS_WINDOWS
	if (OpenClipboard(nullptr))
	{
		EmptyClipboard();
		const HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
		if (hGlobal)
		{
			memcpy(GlobalLock(hGlobal), text.c_str(), text.size() + 1);
			GlobalUnlock(hGlobal);
			SetClipboardData(CF_TEXT, hGlobal);
		}
		CloseClipboard();
	}
#endif
}

static uint32 GetUnrelocatedAddress(MPTR address)
{
	RPLModule* rplModule = RPLLoader_FindModuleByCodeAddr(address);
	if (!rplModule)
		return 0;
	if (address >= rplModule->regionMappingBase_text.GetMPTR() && address < (rplModule->regionMappingBase_text.GetMPTR() + rplModule->regionSize_text))
		return 0x02000000 + (address - rplModule->regionMappingBase_text.GetMPTR());
	return 0;
}

void DisasmCtrl::OnContextMenu(const wxPoint& position, uint32 line)
{
	auto optVirtualAddress = LinePixelPosToAddress(position.y - GetViewStart().y * m_line_height);
	if (!optVirtualAddress)
		return;
	MPTR virtualAddress = *optVirtualAddress;
	m_contextMenuAddress = virtualAddress;
	// show dialog
	wxMenu menu;
	menu.Append(IDContextMenu_ToggleBreakpoint, _("Toggle breakpoint"));
	menu.Append(IDContextMenu_ToggleLoggingBreakpoint, _("Toggle logging point"));
	if(debugger_hasPatch(virtualAddress))
		menu.Append(IDContextMenu_RestoreOriginalInstructions, _("Restore original instructions"));
	menu.AppendSeparator();
	menu.Append(IDContextMenu_CopyAddress, _("Copy address"));
	uint32 unrelocatedAddress = GetUnrelocatedAddress(virtualAddress);
	if (unrelocatedAddress && unrelocatedAddress != virtualAddress)
		menu.Append(IDContextMenu_CopyUnrelocatedAddress, _("Copy virtual address (for IDA/Ghidra)"));
	PopupMenu(&menu);
}

void DisasmCtrl::OnContextMenuEntryClicked(wxCommandEvent& event)
{
	switch(event.GetId())
	{
		case IDContextMenu_ToggleBreakpoint:
		{
			debugger_toggleExecuteBreakpoint(m_contextMenuAddress);
			wxCommandEvent evt(wxEVT_BREAKPOINT_CHANGE);
			wxPostEvent(this->m_parent, evt);
			break;
		}
		case IDContextMenu_ToggleLoggingBreakpoint:
		{
			debugger_toggleLoggingBreakpoint(m_contextMenuAddress);
			wxCommandEvent evt(wxEVT_BREAKPOINT_CHANGE);
			wxPostEvent(this->m_parent, evt);
			break;
		}
		case IDContextMenu_RestoreOriginalInstructions:
		{
			debugger_removePatch(m_contextMenuAddress);
			wxCommandEvent evt(wxEVT_BREAKPOINT_CHANGE); // This also refreshes the disassembly view
			wxPostEvent(this->m_parent, evt);
			break;
		}
		case IDContextMenu_CopyAddress:
		{
			CopyToClipboard(fmt::format("{:#10x}", m_contextMenuAddress));
			break;
		}
		case IDContextMenu_CopyUnrelocatedAddress:
		{
			uint32 unrelocatedAddress = GetUnrelocatedAddress(m_contextMenuAddress);
			CopyToClipboard(fmt::format("{:#10x}", unrelocatedAddress));
			break;
		}
		default:
			UNREACHABLE;
	}
}

bool DisasmCtrl::OnShowTooltip(const wxPoint& position, uint32 line)
{
	return false;
}

void DisasmCtrl::ScrollWindow(int dx, int dy, const wxRect* prect)
{
	// scroll and repaint everything
	RefreshControl(nullptr);
	TextList::ScrollWindow(dx, dy, nullptr);
}

wxSize DisasmCtrl::DoGetBestSize() const
{
	return TextList::DoGetBestSize();
}

void DisasmCtrl::OnUpdateView()
{
	RefreshControl();
}

uint32 DisasmCtrl::GetViewBaseAddress() const
{
	if (GetViewStart().y < 0)
		return MPTR_NULL;
	return currentCodeRegionStart + GetViewStart().y * 4;
}

std::optional<MPTR> DisasmCtrl::LinePixelPosToAddress(sint32 posY)
{
	if (posY < 0)
		return std::nullopt;

	sint32 lineIndex = posY / m_line_height;
	if (lineIndex >= m_lineToAddress.size())
		return std::nullopt;

	return m_lineToAddress[lineIndex];
}

uint32 DisasmCtrl::AddressToScrollPos(uint32 offset) const
{
	return (offset - currentCodeRegionStart) / 4;
}

void DisasmCtrl::CenterOffset(uint32 offset)
{
	if (offset < currentCodeRegionStart || offset >= currentCodeRegionEnd)
		SelectCodeRegion(offset);

	const sint32 line = AddressToScrollPos(offset);
	if (line < 0 || line >= (sint32)m_element_count)
		return;

	if (m_active_line != -1)
		RefreshLine(m_active_line);

	DoScroll(0, std::max(0, line - (sint32)(m_elements_visible / 2)));

	m_active_line = line;
	RefreshLine(m_active_line);
}

void DisasmCtrl::GoToAddressDialog()
{
	wxTextEntryDialog goto_dialog(this, _("Enter a target address."), _("GoTo address"), wxEmptyString);
	if (goto_dialog.ShowModal() == wxID_OK)
	{
		ExpressionParser parser;

		auto value = goto_dialog.GetValue().ToStdString();
		std::transform(value.begin(), value.end(), value.begin(), tolower);

		// trim any leading spaces
		while(!value.empty() && value[0] == ' ')
			value.erase(value.begin());

		debugger_addParserSymbols(parser);

		// try to parse expression as hex value first (it should interpret 1234 as 0x1234, not 1234)
		if (parser.IsConstantExpression("0x"+value))
		{
			const auto result = (uint32)parser.Evaluate("0x"+value);
			m_lastGotoTarget = result;
			CenterOffset(result);
			wxCommandEvent evt(wxEVT_DISASMCTRL_NOTIFY_GOTO_ADDRESS);
			evt.SetExtraLong(static_cast<long>(result));
			wxPostEvent(GetParent(), evt);
		}
		else if (parser.IsConstantExpression(value))
		{
			const auto result = (uint32)parser.Evaluate(value);
			m_lastGotoTarget = result;
			CenterOffset(result);
			wxCommandEvent evt(wxEVT_DISASMCTRL_NOTIFY_GOTO_ADDRESS);
			evt.SetExtraLong(static_cast<long>(result));
			wxPostEvent(GetParent(), evt);
		}
		else
		{
			try
			{
				// if not a constant expression (i.e. relying on unknown variables), then evaluating will throw an exception with a detailed error message
				const auto _ = (uint32)parser.Evaluate(value);
			}
			catch (const std::exception& ex)
			{
				wxMessageBox(ex.what(), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
			}
		}
	}
}
