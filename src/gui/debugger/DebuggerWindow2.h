#pragma once

#include "gui/debugger/DisasmCtrl.h"
#include "config/XMLConfig.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/HW/Espresso/Debugger/GDBStub.h"

#include <wx/bitmap.h>
#include <wx/frame.h>

class BreakpointWindow;
class RegisterWindow;
class DumpWindow;
class ModuleWindow;
class SymbolWindow;
class wxStaticText;

wxDECLARE_EVENT(wxEVT_UPDATE_VIEW, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_BREAKPOINT_HIT, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_RUN, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_BREAKPOINT_CHANGE, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_MOVE_IP, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_NOTIFY_MODULE_LOADED, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_NOTIFY_MODULE_UNLOADED, wxCommandEvent);

struct DebuggerConfig
{
	DebuggerConfig()
	: pin_to_main(true), break_on_start(true), show_register(true), show_dump(true), show_stack(true), show_breakpoints(true), show_modules(true), show_symbols(true) {}
	
	bool pin_to_main;
	bool break_on_start;

	bool show_register;
	bool show_dump;
	bool show_stack;
	bool show_breakpoints;
	bool show_modules;
	bool show_symbols;

	void Load(XMLConfigParser& parser);
	void Save(XMLConfigParser& parser);
};
typedef XMLDataConfig<DebuggerConfig, &DebuggerConfig::Load, &DebuggerConfig::Save> XMLDebuggerConfig;

struct DebuggerModuleStorage
{
	std::string module_name;
	uint32_t crc_hash;
	const RPLModule* rpl_module;
	bool delete_breakpoints_after_saving;

	void Load(XMLConfigParser& parser);
	void Save(XMLConfigParser& parser);
};
typedef XMLDataConfig<DebuggerModuleStorage, &DebuggerModuleStorage::Load, &DebuggerModuleStorage::Save> XMLDebuggerModuleConfig;

class DebuggerWindow2 : public wxFrame
{
public:
	void CreateToolBar();
	void LoadModuleStorage(const RPLModule* module);
	void SaveModuleStorage(const RPLModule* module, bool delete_breakpoints);
	DebuggerWindow2(wxFrame& parent, const wxRect& display_size);
	~DebuggerWindow2();

	void OnParentMove(const wxPoint& position, const wxSize& size);
	void OnGameLoaded();

	XMLDebuggerConfig& GetConfig();

	bool Show(bool show = true) override;
	std::wstring GetModuleStoragePath(std::string module_name, uint32_t crc_hash) const;
private:
	void OnBreakpointHit(wxCommandEvent& event);
	void OnRunProgram(wxCommandEvent& event);
	void OnToolClicked(wxCommandEvent& event);
	void OnBreakpointChange(wxCommandEvent& event);
	void OnOptionsInput(wxCommandEvent& event);
	void OnWindowMenu(wxCommandEvent& event);
	void OnUpdateView(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
	void OnShow(wxShowEvent& event);
	void OnClose(wxCloseEvent& event);
	void OnMoveIP(wxCommandEvent& event);
	void OnNotifyModuleLoaded(wxCommandEvent& event);
	void OnNotifyModuleUnloaded(wxCommandEvent& event);

	void CreateMenuBar();
	void UpdateModuleLabel(uint32 address = 0);

	XMLDebuggerConfig m_config;
	std::vector<std::unique_ptr<XMLDebuggerModuleConfig>> m_modules_storage;

	wxPoint m_main_position;
	wxSize m_main_size;
	
	RegisterWindow* m_register_window;
	DumpWindow* m_dump_window;
	BreakpointWindow* m_breakpoint_window;
	ModuleWindow* m_module_window;
	SymbolWindow* m_symbol_window;

	DisasmCtrl* m_disasm_ctrl;

	wxToolBar* m_toolbar;
	wxBitmap m_run, m_pause;

	uint32 m_module_address;
	wxStaticText* m_module_label;


wxDECLARE_EVENT_TABLE();
};

