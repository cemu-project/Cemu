#include "wxgui/wxgui.h"
#include "wxgui/debugger/BreakpointWindow.h"

#include <sstream>

#include "wxgui/debugger/DebuggerWindow2.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"

#include "Cemu/ExpressionParser/ExpressionParser.h"

enum
{
	MENU_ID_CREATE_CODE_BP_EXECUTION = 1,
	MENU_ID_CREATE_CODE_BP_LOGGING,
	MENU_ID_CREATE_MEM_BP_READ,
	MENU_ID_CREATE_MEM_BP_WRITE,
	MENU_ID_DELETE_BP,
};

enum ItemColumns
{
	ColumnEnabled = 0,
	ColumnAddress,
	ColumnType,
	ColumnComment,
};

BreakpointWindow::BreakpointWindow(DebuggerWindow2& parent, const wxPoint& main_position, const wxSize& main_size)
	: wxFrame(&parent, wxID_ANY, _("Breakpoints"), wxDefaultPosition, wxSize(420, 250), wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxRESIZE_BORDER | wxFRAME_FLOAT_ON_PARENT)
{
	this->SetSizeHints(wxDefaultSize, wxDefaultSize);
	
	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

	m_breakpoints = new wxListView(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
	m_breakpoints->EnableCheckBoxes(true);
	
	wxListItem col0;
	col0.SetId(ColumnEnabled);
	col0.SetText(_("On"));
	col0.SetWidth(32);
	m_breakpoints->InsertColumn(ColumnEnabled, col0);

	wxListItem col1;
	col1.SetId(ColumnAddress);
	col1.SetText(_("Address"));
	col1.SetWidth(75);
	m_breakpoints->InsertColumn(ColumnAddress, col1);

	wxListItem col2;
	col2.SetId(ColumnType);
	col2.SetText(_("Type"));
	col2.SetWidth(42);
	m_breakpoints->InsertColumn(ColumnType, col2);

	wxListItem col3;
	col3.SetId(ColumnComment);
	col3.SetText(_("Comment"));
	col3.SetWidth(250);
	m_breakpoints->InsertColumn(ColumnComment, col3);

	main_sizer->Add(m_breakpoints, 1, wxEXPAND);

	this->SetSizer(main_sizer);
	this->wxWindowBase::Layout();

	this->Centre(wxBOTH);

	if (parent.GetConfig().data().pin_to_main)
		OnMainMove(main_position, main_size);

	m_breakpoints->Bind(wxEVT_LIST_ITEM_CHECKED, &BreakpointWindow::OnBreakpointToggled, this);
	m_breakpoints->Bind(wxEVT_LIST_ITEM_UNCHECKED, &BreakpointWindow::OnBreakpointToggled, this);
	m_breakpoints->Bind(wxEVT_LEFT_DCLICK, &BreakpointWindow::OnLeftDClick, this);
	m_breakpoints->Bind(wxEVT_RIGHT_DOWN, &BreakpointWindow::OnRightDown, this);

	OnUpdateView();
}

BreakpointWindow::~BreakpointWindow()
{
	m_breakpoints->Unbind(wxEVT_LIST_ITEM_CHECKED, &BreakpointWindow::OnBreakpointToggled, this);
	m_breakpoints->Unbind(wxEVT_LIST_ITEM_UNCHECKED, &BreakpointWindow::OnBreakpointToggled, this);
	m_breakpoints->Unbind(wxEVT_LEFT_DCLICK, &BreakpointWindow::OnLeftDClick, this);
	m_breakpoints->Unbind(wxEVT_RIGHT_DOWN, &BreakpointWindow::OnRightDown, this);
}

void BreakpointWindow::OnMainMove(const wxPoint& main_position, const wxSize& main_size)
{
	wxSize size(420, 250);
	this->SetSize(size);

	wxPoint position = main_position;
	position.x -= 420;
	position.y += main_size.y - 250;
	this->SetPosition(position);
}

void BreakpointWindow::OnUpdateView()
{
	Freeze();

	m_breakpoints->DeleteAllItems();

	if (!debuggerState.breakpoints.empty())
	{
		uint32_t i = 0;
		for (const auto bpBase : debuggerState.breakpoints)
		{
			DebuggerBreakpoint* bp = bpBase;
			while (bp)
			{
				wxListItem item = {};
				item.SetId(i++);

				const auto index = m_breakpoints->InsertItem(item);
				m_breakpoints->SetItem(index, ColumnAddress, wxString::Format("%08x", bp->address));
				const char* typeName = "UKN";
				if (bp->bpType == DEBUGGER_BP_T_NORMAL)
					typeName = "X";
				else if (bp->bpType == DEBUGGER_BP_T_LOGGING)
					typeName = "LOG";
				else if (bp->bpType == DEBUGGER_BP_T_ONE_SHOT)
					typeName = "XS";
				else if (bp->bpType == DEBUGGER_BP_T_MEMORY_READ)
					typeName = "R";
				else if (bp->bpType == DEBUGGER_BP_T_MEMORY_WRITE)
					typeName = "W";

				m_breakpoints->SetItem(index, ColumnType, typeName);
				m_breakpoints->SetItem(index, ColumnComment, bp->comment);
				m_breakpoints->CheckItem(index, bp->enabled);
				m_breakpoints->SetItemPtrData(index, (wxUIntPtr)bp);

				bp = bp->next;
			}
		}
	}
	
	Thaw();
}

void BreakpointWindow::OnGameLoaded()
{
	OnUpdateView();
}

void BreakpointWindow::OnBreakpointToggled(wxListEvent& event)
{
	const int32_t index = event.GetIndex();
	if (0 <= index && index < m_breakpoints->GetItemCount())
	{
		const bool state = m_breakpoints->IsItemChecked(index);
		wxString line = m_breakpoints->GetItemText(index, ColumnAddress);
		DebuggerBreakpoint* bp = (DebuggerBreakpoint*)m_breakpoints->GetItemData(index);
		const uint32 address = std::stoul(line.c_str().AsChar(), nullptr, 16);
		debugger_toggleBreakpoint(address, state, bp);
		m_breakpoints->CheckItem(index, state);
	}
}

void BreakpointWindow::OnLeftDClick(wxMouseEvent& event)
{
	const auto position = event.GetPosition();
	const sint32 index = (position.y / m_breakpoints->GetCharHeight()) - 2;
	if (index < 0 || index >= m_breakpoints->GetItemCount())
		return;
	
	sint32 x = position.x;
	const auto enabled_width = m_breakpoints->GetColumnWidth(ColumnEnabled);
	if (x <= enabled_width)
		return;

	x -= enabled_width;
	const auto address_width = m_breakpoints->GetColumnWidth(ColumnAddress);
	if(x <= address_width)
	{
		const auto item = m_breakpoints->GetItemText(index, ColumnAddress);
		const auto address = std::stoul(item.ToStdString(), nullptr, 16);
		debuggerState.debugSession.instructionPointer = address;
		g_debuggerDispatcher.MoveIP();
		return;
	}

	x -= address_width;
	const auto type_width =  m_breakpoints->GetColumnWidth(ColumnType);
	if (x <= type_width)
		return;

	x -= type_width;
	const auto comment_width = m_breakpoints->GetColumnWidth(ColumnComment);
	if(x <= comment_width)
	{
		if (index >= debuggerState.breakpoints.size())
			return;

		const auto item = m_breakpoints->GetItemText(index, ColumnAddress);
		const auto address = std::stoul(item.ToStdString(), nullptr, 16);
		
		auto it = debuggerState.breakpoints.begin();
		std::advance(it, index);

		const wxString dialogTitle = (*it)->bpType == DEBUGGER_BP_T_LOGGING ? _("Enter a new logging message") : _("Enter a new comment");
		const wxString dialogMessage = (*it)->bpType == DEBUGGER_BP_T_LOGGING ? _("Set logging message when code at address %08x is ran.\nUse placeholders like {r3} or {f3} to log register values") : _("Set comment for breakpoint at address %08x");
		wxTextEntryDialog set_comment_dialog(this, dialogMessage, dialogTitle, (*it)->comment);
		if (set_comment_dialog.ShowModal() == wxID_OK)
		{
			(*it)->comment = set_comment_dialog.GetValue().ToStdWstring();
			m_breakpoints->SetItem(index, ColumnComment, set_comment_dialog.GetValue());
		}
	}
}

void BreakpointWindow::OnRightDown(wxMouseEvent& event)
{
	const auto position = event.GetPosition();
	const sint32 index = (position.y / m_breakpoints->GetCharHeight()) - 2;
	if (index < 0 || index >= m_breakpoints->GetItemCount())
	{
		wxMenu menu;
		menu.Append(MENU_ID_CREATE_CODE_BP_EXECUTION, _("Create execution breakpoint"));
		menu.Append(MENU_ID_CREATE_CODE_BP_LOGGING, _("Create logging breakpoint"));
		menu.Append(MENU_ID_CREATE_MEM_BP_READ, _("Create memory breakpoint (read)"));
		menu.Append(MENU_ID_CREATE_MEM_BP_WRITE, _("Create memory breakpoint (write)"));

		menu.Bind(wxEVT_COMMAND_MENU_SELECTED, &BreakpointWindow::OnContextMenuClick, this);
		PopupMenu(&menu);
	}
	else
	{
		m_breakpoints->Focus(index);
		m_breakpoints->Select(index);

		wxMenu menu;
		menu.Append(MENU_ID_DELETE_BP, _("Delete breakpoint"));

		menu.Bind(wxEVT_COMMAND_MENU_SELECTED, &BreakpointWindow::OnContextMenuClickSelected, this);
		PopupMenu(&menu);
	}
}

void BreakpointWindow::OnContextMenuClickSelected(wxCommandEvent& evt)
{
	if (evt.GetId() == MENU_ID_DELETE_BP)
	{
		long sel = m_breakpoints->GetFirstSelected();
		if (sel != wxNOT_FOUND)
		{
			if (sel >= debuggerState.breakpoints.size())
				return;

			auto it = debuggerState.breakpoints.begin();
			std::advance(it, sel);

			debugger_deleteBreakpoint(*it);

			wxCommandEvent evt(wxEVT_BREAKPOINT_CHANGE);
			wxPostEvent(this->m_parent, evt);
		}
	}
}

void BreakpointWindow::OnContextMenuClick(wxCommandEvent& evt)
{
	wxTextEntryDialog goto_dialog(this, _("Enter a memory address"), _("Set breakpoint"), wxEmptyString);
	if (goto_dialog.ShowModal() == wxID_OK)
	{
		ExpressionParser parser;

		auto value = goto_dialog.GetValue().ToStdString();
		std::transform(value.begin(), value.end(), value.begin(), tolower);

		uint32_t newBreakpointAddress = 0;
		try
		{
			debugger_addParserSymbols(parser);
			newBreakpointAddress = parser.IsConstantExpression("0x"+value) ? (uint32)parser.Evaluate("0x"+value) : (uint32)parser.Evaluate(value);
		}
		catch (const std::exception& ex)
		{
			wxMessageBox(ex.what(), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
			return;
		}

		switch (evt.GetId())
		{
		case MENU_ID_CREATE_CODE_BP_EXECUTION:
			debugger_createCodeBreakpoint(newBreakpointAddress, DEBUGGER_BP_T_NORMAL);
			break;
		case MENU_ID_CREATE_CODE_BP_LOGGING:
			debugger_createCodeBreakpoint(newBreakpointAddress, DEBUGGER_BP_T_LOGGING);
			break;
		case MENU_ID_CREATE_MEM_BP_READ:
			debugger_createMemoryBreakpoint(newBreakpointAddress, true, false);
			break;
		case MENU_ID_CREATE_MEM_BP_WRITE:
			debugger_createMemoryBreakpoint(newBreakpointAddress, false, true);
			break;
		}

		this->OnUpdateView();
	}
}
