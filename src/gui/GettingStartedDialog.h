#pragma once

#include <wx/dialog.h>
#include <wx/checkbox.h>
#include <wx/filepicker.h>
#include <wx/statbmp.h>
#include <wx/simplebook.h>
#include <wx/stattext.h>
#include <wx/panel.h>

class GettingStartedDialog : public wxDialog
{
public:
	GettingStartedDialog(wxWindow* parent = nullptr);

	[[nodiscard]] bool HasGamePathChanged() const { return m_game_path_changed; }
	[[nodiscard]] bool HasMLCChanged() const { return m_mlc_changed; }

private:
	wxPanel* CreatePage1();
	wxPanel* CreatePage2();
	void ApplySettings();
	void UpdateWindowSize();

	void OnClose(wxCloseEvent& event);
	void OnDownloadGPs(wxCommandEvent& event);
	void OnInputSettings(wxCommandEvent& event);
	void OnMLCPathChar(wxKeyEvent& event);

	wxSimplebook* m_notebook;
	wxCheckBox* m_fullscreen;
	wxCheckBox* m_separate;
	wxCheckBox* m_update;
	wxCheckBox* m_dont_show;

	wxStaticBoxSizer* m_mlc_box_sizer;
	wxStaticText* m_prev_mlc_warning;
	wxDirPickerCtrl* m_mlc_folder;
	wxDirPickerCtrl* m_game_path;

	bool m_game_path_changed = false;
	bool m_mlc_changed = false;
};

