#pragma once

#include <wx/wx.h>
#include "config/CemuConfig.h"
#include "input/api/Controller.h"

class HotkeyEntry;

class HotkeySettings : public wxFrame
{
public:
	static void Init(wxFrame* mainWindowFrame);

	static void CaptureInput(wxKeyEvent& event);

	HotkeySettings(wxWindow* parent);
	~HotkeySettings();

private:
	inline static wxFrame* s_mainWindow = nullptr;
	static const std::unordered_map<sHotkeyCfg*, std::function<void(void)>> s_cfgHotkeyToFuncMap;
	inline static std::unordered_map<uint16, std::function<void(void)>> s_keyboardHotkeyToFuncMap{};
	inline static auto& s_cfgHotkeys = GetConfig().hotkeys;

	wxPanel* m_panel;
	wxFlexGridSizer* m_sizer;
	wxButton* m_activeInputButton{ nullptr };
	const wxSize m_minButtonSize{ 250, 45 };
	const wxString m_disabledHotkeyText{ _("----") };
	const wxString m_editModeHotkeyText{ _("") };

	std::vector<HotkeyEntry> m_hotkeys;
	bool m_needToSave = false;

	/* helpers */
	void CreateColumnHeaders(void);
	void CreateHotkeyRow(const wxString& label, sHotkeyCfg& cfgHotkey);
	wxString To_wxString(uKeyboardHotkey hotkey);
	bool IsValidKeycodeUp(int keycode);

	template<typename T>
	void FinalizeInput(wxButton* inputButton);

	template <typename T>
	void RestoreInputButton(void);

	/* events */
	void OnKeyboardHotkeyInputLeftClick(wxCommandEvent& event);
	void OnKeyboardHotkeyInputRightClick(wxMouseEvent& event);
	void OnKeyUp(wxKeyEvent& event);
};
