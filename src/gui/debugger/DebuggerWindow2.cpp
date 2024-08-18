#include "gui/wxgui.h"
#include "gui/debugger/DebuggerWindow2.h"

#include <filesystem>

#include "config/ActiveSettings.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cafe/OS/RPL/rpl_debug_symbols.h"

#include "gui/debugger/RegisterWindow.h"
#include "gui/debugger/DumpWindow.h"

#include "Cafe/HW/Espresso/Debugger/Debugger.h"

#include "Cafe/OS/RPL/rpl.h"
#include "gui/debugger/DisasmCtrl.h"
#include "gui/debugger/SymbolWindow.h"
#include "gui/debugger/BreakpointWindow.h"
#include "gui/debugger/ModuleWindow.h"
#include "util/helpers/helpers.h"

#include "resource/embedded/resources.h"

enum
{
	// file
	MENU_ID_FILE_EXIT = wxID_HIGHEST + 8000,
	// settings
	MENU_ID_OPTIONS_PIN_TO_MAINWINDOW,
	MENU_ID_OPTIONS_BREAK_ON_START,
	// window
	MENU_ID_WINDOW_REGISTERS,
	MENU_ID_WINDOW_DUMP,
	MENU_ID_WINDOW_STACK,
	MENU_ID_WINDOW_BREAKPOINTS,
	MENU_ID_WINDOW_MODULE,
	MENU_ID_WINDOW_SYMBOL,

	// tool
	TOOL_ID_GOTO,
	TOOL_ID_BP,
	TOOL_ID_PAUSE,
	TOOL_ID_STEP_OVER,
	TOOL_ID_STEP_INTO,
};

wxDEFINE_EVENT(wxEVT_DEBUGGER_CLOSE, wxCloseEvent);
wxDEFINE_EVENT(wxEVT_UPDATE_VIEW, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_BREAKPOINT_CHANGE, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_BREAKPOINT_HIT, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_MOVE_IP, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_RUN, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_NOTIFY_MODULE_LOADED, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_NOTIFY_MODULE_UNLOADED, wxCommandEvent);

wxBEGIN_EVENT_TABLE(DebuggerWindow2, wxFrame)
	EVT_SHOW(DebuggerWindow2::OnShow)
	EVT_CLOSE(DebuggerWindow2::OnClose)
	EVT_COMMAND(wxID_ANY, wxEVT_UPDATE_VIEW, DebuggerWindow2::OnUpdateView)
	EVT_COMMAND(wxID_ANY, wxEVT_BREAKPOINT_CHANGE, DebuggerWindow2::OnBreakpointChange)
	EVT_COMMAND(wxID_ANY, wxEVT_MOVE_IP, DebuggerWindow2::OnMoveIP)
	EVT_COMMAND(wxID_ANY, wxEVT_COMMAND_TOOL_CLICKED, DebuggerWindow2::OnToolClicked)
	EVT_COMMAND(wxID_ANY, wxEVT_BREAKPOINT_HIT, DebuggerWindow2::OnBreakpointHit)
	EVT_COMMAND(wxID_ANY, wxEVT_RUN, DebuggerWindow2::OnRunProgram)
	EVT_COMMAND(wxID_ANY, wxEVT_NOTIFY_MODULE_LOADED, DebuggerWindow2::OnNotifyModuleLoaded)
	EVT_COMMAND(wxID_ANY, wxEVT_NOTIFY_MODULE_UNLOADED, DebuggerWindow2::OnNotifyModuleUnloaded)
	// file menu
	EVT_MENU(MENU_ID_FILE_EXIT, DebuggerWindow2::OnExit)
	// window
	EVT_MENU_RANGE(MENU_ID_WINDOW_REGISTERS, MENU_ID_WINDOW_MODULE, DebuggerWindow2::OnWindowMenu)
wxEND_EVENT_TABLE()

DebuggerWindow2* g_debugger_window;

void DebuggerConfig::Load(XMLConfigParser& parser)
{
	pin_to_main = parser.get("PinToMainWindow", true);
	break_on_start = parser.get("break_on_start", true);

	auto window_parser = parser.get("Windows");
	show_register = window_parser.get("Registers", true);
	show_dump = window_parser.get("MemoryDump", true);
	show_stack = window_parser.get("Stack", true);
	show_breakpoints = window_parser.get("Breakpoints", true);
	show_modules = window_parser.get("Modules", true);
	show_symbols = window_parser.get("Symbols", true);
}

void DebuggerConfig::Save(XMLConfigParser& parser)
{
	parser.set("PinToMainWindow", pin_to_main);
	parser.set("break_on_start", break_on_start);
	
	auto window_parser = parser.set("Windows");
	window_parser.set("Registers", show_register);
	window_parser.set("MemoryDump", show_dump);
	window_parser.set("Stack", show_stack);
	window_parser.set("Breakpoints", show_breakpoints);
	window_parser.set("Modules", show_modules);
	window_parser.set("Symbols", show_symbols);
}

void DebuggerModuleStorage::Load(XMLConfigParser& parser)
{
	auto breakpoints_parser = parser.get("Breakpoints");
	for (auto element = breakpoints_parser.get("Entry"); element.valid(); element = breakpoints_parser.get("Entry", element))
	{
		const auto address_string = element.get("Address", "");
		if (*address_string == '\0')
			continue;

		uint32 relative_address = std::stoul(address_string, nullptr, 16);
		if (relative_address == 0)
			continue;

		auto type = element.get("Type", 0);
		auto enabled = element.get("Enabled", true);
		const auto comment = element.get("Comment", "");

		// calculate absolute address
		uint32 module_base_address = (type == DEBUGGER_BP_T_NORMAL || type == DEBUGGER_BP_T_LOGGING) ? this->rpl_module->regionMappingBase_text.GetMPTR() : this->rpl_module->regionMappingBase_data;
		uint32 address = module_base_address + relative_address;

		// don't change anything if there's already a breakpoint
		if (debugger_getFirstBP(address) != nullptr)
			continue;

		// register breakpoints in debugger
		if (type == DEBUGGER_BP_T_NORMAL)
			debugger_createCodeBreakpoint(address, DEBUGGER_BP_T_NORMAL);
		else if (type == DEBUGGER_BP_T_LOGGING)
			debugger_createCodeBreakpoint(address, DEBUGGER_BP_T_LOGGING);
		else if (type == DEBUGGER_BP_T_MEMORY_READ)
			debugger_createMemoryBreakpoint(address, true, false);
		else if (type == DEBUGGER_BP_T_MEMORY_WRITE)
			debugger_createMemoryBreakpoint(address, false, true);
		else
			continue;

		DebuggerBreakpoint* debugBreakpoint = debugger_getFirstBP(address);

		if (!enabled)
			debugger_toggleBreakpoint(address, false, debugBreakpoint);

		debugBreakpoint->comment = boost::nowide::widen(comment);
	}

	auto comments_parser = parser.get("Comments");
	for (XMLConfigParser element = comments_parser.get("Entry"); element.valid(); element = comments_parser.get("Entry", element))
	{
		const auto address_string = element.get("Address", "");
		if (*address_string == '\0')
			continue;

		uint32 address = std::stoul(address_string, nullptr, 16);
		if (address == 0)
			continue;

		const auto comment = element.get("Comment", "");
		if (*comment == '\0')
			continue;

		rplDebugSymbol_createComment(address, boost::nowide::widen(comment).c_str());
	}
}

void DebuggerModuleStorage::Save(XMLConfigParser& parser)
{
	auto breakpoints_parser = parser.set("Breakpoints");
	for (const auto& bp : debuggerState.breakpoints)
	{
		// check breakpoint type
		if (bp->dbType != DEBUGGER_BP_T_DEBUGGER)
			continue;

		// check whether the breakpoint is part of the current module being saved
		RPLModule* address_module;
		if (bp->bpType == DEBUGGER_BP_T_NORMAL || bp->bpType == DEBUGGER_BP_T_LOGGING) address_module = RPLLoader_FindModuleByCodeAddr(bp->address);
		else if (bp->isMemBP()) address_module = RPLLoader_FindModuleByDataAddr(bp->address);
		else continue;

		if (!address_module || !(address_module->moduleName2 == this->module_name && address_module->patchCRC == this->crc_hash))
			continue;

		uint32_t relative_address = bp->address - (bp->isMemBP() ? address_module->regionMappingBase_data : address_module->regionMappingBase_text.GetMPTR());
		auto entry = breakpoints_parser.set("Entry");
		entry.set("Address", fmt::format("{:#10x}", relative_address));
		entry.set("Comment", boost::nowide::narrow(bp->comment).c_str());
		entry.set("Type", bp->bpType);
		entry.set("Enabled", bp->enabled);

		if (this->delete_breakpoints_after_saving) debugger_deleteBreakpoint(bp);
		this->delete_breakpoints_after_saving = false;
	}

	auto comments_parser = parser.set("Comments");
	for (const auto& comment_entry : rplDebugSymbol_getSymbols())
	{
		// check comment type
		const auto comment_address = comment_entry.first;
		const auto comment = static_cast<rplDebugSymbolComment*>(comment_entry.second);
		if (!comment || comment->type != RplDebugSymbolComment)
			continue;

		// check whether it's part of the current module being saved
		RPLModule* address_module = RPLLoader_FindModuleByCodeAddr(comment_entry.first);
		if (!address_module || !(address_module->moduleName2 == module_name && address_module->patchCRC == this->crc_hash))
			continue;

		uint32_t relative_address = comment_address - address_module->regionMappingBase_text.GetMPTR();
		auto entry = comments_parser.set("Entry");
		entry.set("Address", fmt::format("{:#10x}", relative_address));
		entry.set("Comment", boost::nowide::narrow(comment->comment).c_str());
	}
}

void DebuggerWindow2::CreateToolBar() 
{
	m_toolbar = wxFrame::CreateToolBar(wxTB_HORIZONTAL, wxID_ANY);
	m_toolbar->SetToolBitmapSize(wxSize(16, 16));

	m_toolbar->AddTool(TOOL_ID_GOTO, wxEmptyString, wxBITMAP_PNG_FROM_DATA(DEBUGGER_GOTO), wxNullBitmap, wxITEM_NORMAL, _("GoTo (CTRL + G)"), "test", NULL);
	m_toolbar->AddSeparator();

	m_toolbar->AddTool(TOOL_ID_BP, wxEmptyString, wxBITMAP_PNG_FROM_DATA(DEBUGGER_BP_RED), wxNullBitmap, wxITEM_NORMAL, _("Toggle Breakpoint (F9)"), wxEmptyString, NULL);
	m_toolbar->AddSeparator();

	m_pause = wxBITMAP_PNG_FROM_DATA(DEBUGGER_PAUSE);
	m_run = wxBITMAP_PNG_FROM_DATA(DEBUGGER_PLAY);
	m_toolbar->AddTool(TOOL_ID_PAUSE, wxEmptyString, m_pause, wxNullBitmap, wxITEM_NORMAL, _("Break (F5)"), wxEmptyString, NULL);
	
	m_toolbar->AddTool(TOOL_ID_STEP_INTO, wxEmptyString, wxBITMAP_PNG_FROM_DATA(DEBUGGER_STEP_INTO), wxNullBitmap, wxITEM_NORMAL, _("Step Into (F11)"), wxEmptyString, NULL);
	m_toolbar->AddTool(TOOL_ID_STEP_OVER, wxEmptyString, wxBITMAP_PNG_FROM_DATA(DEBUGGER_STEP_OVER), wxNullBitmap, wxITEM_NORMAL, _("Step Over (F10)"), wxEmptyString, NULL);
	m_toolbar->AddSeparator();

	m_toolbar->Realize();

	m_toolbar->EnableTool(TOOL_ID_STEP_INTO, false);
	m_toolbar->EnableTool(TOOL_ID_STEP_OVER, false);

}

void DebuggerWindow2::SaveModuleStorage(const RPLModule* module, bool delete_breakpoints)
{
	auto path = GetModuleStoragePath(module->moduleName2, module->patchCRC);
	for (auto& module_storage : m_modules_storage)
	{
		if (module_storage->data().module_name == module->moduleName2 && module_storage->data().crc_hash == module->patchCRC)
		{
			module_storage->data().delete_breakpoints_after_saving = delete_breakpoints;
			module_storage->Save(path);
			if (delete_breakpoints) m_modules_storage.erase(std::find(m_modules_storage.begin(), m_modules_storage.end(), module_storage));
		}
	}

}

void DebuggerWindow2::LoadModuleStorage(const RPLModule* module)
{
	auto path = GetModuleStoragePath(module->moduleName2, module->patchCRC);
	bool already_loaded = std::any_of(m_modules_storage.begin(), m_modules_storage.end(), [path](const std::unique_ptr<XMLDebuggerModuleConfig>& debug) { return debug->GetFilename() == path; });
	if (!path.empty() && !already_loaded)
	{
		m_modules_storage.emplace_back(new XMLDebuggerModuleConfig(path, { module->moduleName2, module->patchCRC, module, false }))->Load();
	}
}

DebuggerWindow2::DebuggerWindow2(wxFrame& parent, const wxRect& display_size)
	: wxFrame(&parent, wxID_ANY, _("PPC Debugger"), wxDefaultPosition, wxSize(1280, 300), wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN | wxRESIZE_BORDER | wxFRAME_FLOAT_ON_PARENT),
		m_module_address(0)
{
	this->wxWindowBase::SetBackgroundColour(*wxWHITE);

	const auto file = ActiveSettings::GetConfigPath("debugger/config.xml");
	m_config.SetFilename(file.generic_wstring());
	m_config.Load();

	debuggerState.breakOnEntry = m_config.data().break_on_start;

	m_main_position = parent.GetPosition();
	m_main_size = parent.GetSize();

	auto y = std::max<uint32>(300, (display_size.GetHeight() - 500 - 300) * 0.8);
	this->SetSize(1280, y);

	CreateMenuBar();
	CreateToolBar();

	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

	// load configs for already loaded modules
	const auto module_count = RPLLoader_GetModuleCount();
	const auto module_list = RPLLoader_GetModuleList();
	for (sint32 i = 0; i < module_count; i++)
	{
		const auto module = module_list[i];
		LoadModuleStorage(module);
	}

	wxString label_text = _("> no modules loaded");
	if (module_count != 0)
	{
		RPLModule* current_rpl_module = RPLLoader_FindModuleByCodeAddr(MEMORY_CODEAREA_ADDR);
		if (current_rpl_module)
			label_text = wxString::Format("> %s", current_rpl_module->moduleName2.c_str());
		else
			label_text = _("> unknown module");
	}

	m_module_label = new wxStaticText(this, wxID_ANY, label_text);
	m_module_label->SetBackgroundColour(*wxWHITE);
	m_module_label->SetForegroundColour(wxColour(0xFFbf52fe));
	main_sizer->Add(m_module_label, 0, wxEXPAND | wxALL, 5);

	m_disasm_ctrl = new DisasmCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxScrolledWindowStyle);
	main_sizer->Add(m_disasm_ctrl, 1, wxEXPAND);

	this->SetSizer(main_sizer);
	this->wxWindowBase::Layout();

	m_register_window = new RegisterWindow(*this, m_main_position, m_main_size);
	m_dump_window = new DumpWindow(*this, m_main_position, m_main_size);
	m_breakpoint_window = new BreakpointWindow(*this, m_main_position, m_main_size);
	m_module_window = new ModuleWindow(*this, m_main_position, m_main_size);
	m_symbol_window = new SymbolWindow(*this, m_main_position, m_main_size);

	const bool value = m_config.data().pin_to_main;
	m_config.data().pin_to_main = true;
	OnParentMove(m_main_position, m_main_size);
	m_config.data().pin_to_main = value;

	g_debugger_window = this;
}

DebuggerWindow2::~DebuggerWindow2()
{
	debuggerState.breakOnEntry = false;
	g_debugger_window = nullptr;

	// save configs for all modules that are still loaded
	// doesn't delete breakpoints since that should (in the future) be done by unloading the rpl modules when exiting the current game
	const auto module_count = RPLLoader_GetModuleCount();
	const auto module_list = RPLLoader_GetModuleList();
	for (sint32 i = 0; i < module_count; i++)
	{
		const auto module = module_list[i];
		if (module)
			SaveModuleStorage(module, false);
	}

	if (m_register_window && m_register_window->IsShown())
		m_register_window->Close(true);

	if (m_dump_window && m_dump_window->IsShown())
		m_dump_window->Close(true);

	if (m_breakpoint_window && m_breakpoint_window->IsShown())
		m_breakpoint_window->Close(true);

	if (m_module_window && m_module_window->IsShown())
		m_module_window->Close(true);

	if (m_symbol_window && m_symbol_window->IsShown())
		m_symbol_window->Close(true);

	m_config.Save();
}

void DebuggerWindow2::OnClose(wxCloseEvent& event)
{
	debuggerState.breakOnEntry = false;
	
	const wxCloseEvent parentEvent(wxEVT_DEBUGGER_CLOSE);
	wxPostEvent(m_parent, parentEvent);

	event.Skip();
}

void DebuggerWindow2::OnMoveIP(wxCommandEvent& event)
{
	const auto ip = debuggerState.debugSession.instructionPointer;
	UpdateModuleLabel(ip);
	m_disasm_ctrl->CenterOffset(ip);
}

void DebuggerWindow2::OnParentMove(const wxPoint& main_position, const wxSize& main_size)
{
	m_main_position = main_position;
	m_main_size = main_size;

	if (!m_config.data().pin_to_main)
		return;

	wxSize size(m_main_size.x, GetSize().GetHeight());
	SetSize(size);

	wxPoint position = m_main_position;
	position.y -= size.y;
	SetPosition(position);

	m_register_window->OnMainMove(main_position, main_size);
	m_dump_window->OnMainMove(main_position, main_size);
	m_breakpoint_window->OnMainMove(main_position, main_size);
	m_module_window->OnMainMove(main_position, main_size);
	m_symbol_window->OnMainMove(main_position, main_size);
}

void DebuggerWindow2::OnNotifyModuleLoaded(wxCommandEvent& event)
{
	RPLModule* module = (RPLModule*)event.GetClientData();
	LoadModuleStorage(module);
	m_module_window->OnGameLoaded();
	m_symbol_window->OnGameLoaded();
	m_disasm_ctrl->Init();
}

void DebuggerWindow2::OnNotifyModuleUnloaded(wxCommandEvent& event)
{
	RPLModule* module = (RPLModule*)event.GetClientData();
	SaveModuleStorage(module, true);
	m_module_window->OnGameLoaded();
	m_symbol_window->OnGameLoaded();
	m_disasm_ctrl->Init();
}

void DebuggerWindow2::OnGameLoaded()
{
	m_disasm_ctrl->Init();

	m_dump_window->OnGameLoaded();
	m_module_window->OnGameLoaded();
	m_breakpoint_window->OnGameLoaded();
	m_symbol_window->OnGameLoaded();

	RPLModule* current_rpl_module = RPLLoader_FindModuleByCodeAddr(MEMORY_CODEAREA_ADDR);
	if(current_rpl_module)
		m_module_label->SetLabel(wxString::Format("> %s", current_rpl_module->moduleName2.c_str()));	

	this->SendSizeEvent();
}

XMLDebuggerConfig& DebuggerWindow2::GetConfig()
{
	return m_config;
}

bool DebuggerWindow2::Show(bool show)
{
	const bool result = wxFrame::Show(show);

	if (show)
	{
		m_register_window->Show(m_config.data().show_register);
		m_dump_window->Show(m_config.data().show_dump);
		m_breakpoint_window->Show(m_config.data().show_breakpoints);
		m_module_window->Show(m_config.data().show_modules);
		m_symbol_window->Show(m_config.data().show_symbols);
	}
	else
	{
		m_register_window->Show(false);
		m_dump_window->Show(false);
		m_breakpoint_window->Show(false);
		m_module_window->Show(false);
		m_symbol_window->Show(false);
	}

	return result;
}

std::wstring DebuggerWindow2::GetModuleStoragePath(std::string module_name, uint32_t crc_hash) const
{
	if (module_name.empty() || crc_hash == 0) return {};
	return ActiveSettings::GetConfigPath("debugger/{}_{:#10x}.xml", module_name, crc_hash).generic_wstring();
}

void DebuggerWindow2::OnBreakpointHit(wxCommandEvent& event)
{
	const auto ip = debuggerState.debugSession.instructionPointer;
	UpdateModuleLabel(ip);

	m_toolbar->SetToolShortHelp(TOOL_ID_PAUSE, _("Run (F5)"));
	m_toolbar->SetToolNormalBitmap(TOOL_ID_PAUSE, m_run);

	m_toolbar->EnableTool(TOOL_ID_STEP_INTO, true);
	m_toolbar->EnableTool(TOOL_ID_STEP_OVER, true);

	m_disasm_ctrl->CenterOffset(ip);
}

void DebuggerWindow2::OnRunProgram(wxCommandEvent& event)
{
	m_toolbar->SetToolShortHelp(TOOL_ID_PAUSE, _("Break (F5)"));
	m_toolbar->SetToolNormalBitmap(TOOL_ID_PAUSE, m_pause);

	m_toolbar->EnableTool(TOOL_ID_STEP_INTO, false);
	m_toolbar->EnableTool(TOOL_ID_STEP_OVER, false);
}

void DebuggerWindow2::OnToolClicked(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case TOOL_ID_GOTO:
		m_disasm_ctrl->GoToAddressDialog();
		break;
	case TOOL_ID_PAUSE:
		if (debugger_isTrapped())
			debugger_resume();
		else
			debugger_forceBreak();
		break;
	case TOOL_ID_STEP_INTO:
		if (debugger_isTrapped())
			debuggerState.debugSession.stepInto = true;
		break;
	case TOOL_ID_STEP_OVER:
		if (debugger_isTrapped())
			debuggerState.debugSession.stepOver = true;
		break;
	}
}

void DebuggerWindow2::OnBreakpointChange(wxCommandEvent& event)
{
	m_breakpoint_window->OnUpdateView();
	m_disasm_ctrl->RefreshControl();
	UpdateModuleLabel();
}

void DebuggerWindow2::OnOptionsInput(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case MENU_ID_OPTIONS_PIN_TO_MAINWINDOW:
	{
		const bool value = !m_config.data().pin_to_main;
		m_config.data().pin_to_main = value;
		if (value)
			OnParentMove(m_main_position, m_main_size);

		break;
	}
	case MENU_ID_OPTIONS_BREAK_ON_START:
	{
		const bool value = !m_config.data().break_on_start;
		m_config.data().break_on_start = value;
		debuggerState.breakOnEntry = value;
		break;
	}
	default:
		return;
	}

	m_config.Save();
}

void DebuggerWindow2::OnWindowMenu(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case MENU_ID_WINDOW_REGISTERS:
	{
		const bool value = !m_config.data().show_register;
		m_config.data().show_register = value;
		m_register_window->Show(value);
		break;
	}
	case MENU_ID_WINDOW_DUMP:
	{
		const bool value = !m_config.data().show_dump;
		m_config.data().show_dump = value;
		m_dump_window->Show(value);
		break;
	}
	case MENU_ID_WINDOW_BREAKPOINTS:
	{
		const bool value = !m_config.data().show_breakpoints;
		m_config.data().show_breakpoints = value;
		m_breakpoint_window->Show(value);
		break;
	}
	case MENU_ID_WINDOW_MODULE:
	{
		const bool value = !m_config.data().show_modules;
		m_config.data().show_modules = value;
		m_module_window->Show(value);
		break;
	}
	case MENU_ID_WINDOW_SYMBOL:
	{
		const bool value = !m_config.data().show_symbols;
		m_config.data().show_symbols = value;
		m_symbol_window->Show(value);
		break;
	}
	default:
		return;
	}

	m_config.Save();
}

void DebuggerWindow2::OnUpdateView(wxCommandEvent& event)
{
	UpdateModuleLabel();
	m_disasm_ctrl->OnUpdateView();
	m_register_window->OnUpdateView();
	m_breakpoint_window->OnUpdateView();
}

void DebuggerWindow2::OnExit(wxCommandEvent& event)
{
	this->Close();
}

void DebuggerWindow2::OnShow(wxShowEvent& event)
{
	m_register_window->Show();
	m_dump_window->Show();
	m_breakpoint_window->Show();
	m_module_window->Show();
	m_symbol_window->Show();
	event.Skip();
}

void DebuggerWindow2::CreateMenuBar()
{
	auto menu_bar = new wxMenuBar;
	
	// file
	wxMenu* file_menu = new wxMenu;
	file_menu->Append(MENU_ID_FILE_EXIT, _("&Exit"));
	file_menu->Bind(wxEVT_MENU, &DebuggerWindow2::OnExit, this);

	menu_bar->Append(file_menu, _("&File"));

	// options
	wxMenu* options_menu = new wxMenu;
	options_menu->Append(MENU_ID_OPTIONS_PIN_TO_MAINWINDOW, _("&Pin to main window"), wxEmptyString, wxITEM_CHECK)->Check(m_config.data().pin_to_main);
	options_menu->Append(MENU_ID_OPTIONS_BREAK_ON_START, _("Break on &entry point"), wxEmptyString, wxITEM_CHECK)->Check(m_config.data().break_on_start);
	menu_bar->Append(options_menu, _("&Options"));

	// window
	wxMenu* window_menu = new wxMenu;
	window_menu->Append(MENU_ID_WINDOW_REGISTERS, _("&Registers"), wxEmptyString, wxITEM_CHECK)->Check(m_config.data().show_register);
	window_menu->Append(MENU_ID_WINDOW_DUMP, _("&Memory Dump"), wxEmptyString, wxITEM_CHECK)->Check(m_config.data().show_dump);
	window_menu->Append(MENU_ID_WINDOW_BREAKPOINTS, _("&Breakpoints"), wxEmptyString, wxITEM_CHECK)->Check(m_config.data().show_breakpoints);
	window_menu->Append(MENU_ID_WINDOW_MODULE, _("Module&list"), wxEmptyString, wxITEM_CHECK)->Check(m_config.data().show_modules);
	window_menu->Append(MENU_ID_WINDOW_SYMBOL, _("&Symbols"), wxEmptyString, wxITEM_CHECK)->Check(m_config.data().show_symbols);
	
	menu_bar->Append(window_menu, _("&Window"));

	SetMenuBar(menu_bar);

	options_menu->Bind(wxEVT_MENU, &DebuggerWindow2::OnOptionsInput, this);
	window_menu->Bind(wxEVT_MENU, &DebuggerWindow2::OnWindowMenu, this);
}

void DebuggerWindow2::UpdateModuleLabel(uint32 address)
{
	if(address == 0)
		address = m_disasm_ctrl->GetViewBaseAddress();

	RPLModule* module = RPLLoader_FindModuleByCodeAddr(address);
	if (module && m_module_address != module->regionMappingBase_text.GetMPTR())
	{
		m_module_label->SetLabel(wxString::Format("> %s", module->moduleName2.c_str()));
		m_module_address = module->regionMappingBase_text.GetMPTR();
	}
	else if (address >= mmuRange_CODECAVE.getBase() && address < mmuRange_CODECAVE.getEnd())
	{
		m_module_label->SetLabel(wxString::Format("> %s", "Cemu codecave"));
		m_module_address = mmuRange_CODECAVE.getBase();
	}
}