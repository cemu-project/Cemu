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

private:
	wxPanel* CreatePage1();
	wxPanel* CreatePage2();
	void ApplySettings();
	void UpdateWindowSize();

	void OnClose(wxCloseEvent& event);
	void OnConfigureGPs(wxCommandEvent& event);
	void OnInputSettings(wxCommandEvent& event);

	wxSimplebook* m_notebook;

	struct
	{
		// header
		wxStaticText* staticText11{};
		wxStaticText* portableModeInfoText{};

		// game path box
		wxStaticBoxSizer* gamePathBoxSizer{};
		wxStaticText* gamePathText{};
		wxStaticText* gamePathText2{};
		wxDirPickerCtrl* gamePathPicker{};
	}m_page1;

	struct
	{
		wxCheckBox* fullscreenCheckbox;
		wxCheckBox* separateCheckbox;
		wxCheckBox* updateCheckbox;
	}m_page2;
};

