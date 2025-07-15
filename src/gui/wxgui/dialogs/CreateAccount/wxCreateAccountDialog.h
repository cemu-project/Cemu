#pragma once
#include <wx/dialog.h>

class wxCreateAccountDialog : public wxDialog
{
public:
	wxCreateAccountDialog(wxWindow* parent);

	[[nodiscard]] uint32 GetPersistentId() const;
	[[nodiscard]] wxString GetMiiName() const;
private:
	class wxTextCtrl* m_persistent_id;
	class wxTextCtrl* m_mii_name;
	class wxButton* m_ok_button, * m_cancel_buton;

	void OnOK(wxCommandEvent& event);
	void OnCancel(wxCommandEvent& event);
};