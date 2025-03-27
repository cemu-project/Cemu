#pragma once

#include <wx/wx.h>
#include "config/CemuConfig.h"

class HotkeyEntry;

class HotkeySettings : public wxFrame
{
public:
	static void Init(wxFrame* mainWindowFrame);
	inline static std::unordered_map<int, std::function<void(void)>> s_hotkeyToFuncMap{};

	HotkeySettings(wxWindow* parent);
	~HotkeySettings();

private:
	inline static wxFrame* s_mainWindow = nullptr;
	inline static auto& s_cfgHotkeys = GetConfig().hotkeys;
	static const std::unordered_map<uHotkey*, std::function<void(void)>> s_cfgHotkeyToFuncMap;

	wxPanel* m_panel;
	wxFlexGridSizer* m_sizer;
	std::vector<HotkeyEntry> m_hotkeys;
	wxButton* m_activeInputButton = nullptr;
	bool m_needToSave = false;

	/* helpers */
	void CreateHotkey(const wxString& label, uHotkey& hotkey);
	wxString To_wxString(uHotkey hotkey);
	bool IsValidKeycodeUp(int keycode);
	void FinalizeInput(wxButton* inputButton);

	/* events */
	void OnHotkeyInputLeftClick(wxCommandEvent& event);
	void OnHotkeyInputRightClick(wxMouseEvent& event);
	void OnKeyUp(wxKeyEvent& event);
};
