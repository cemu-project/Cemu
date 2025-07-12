#pragma once

#include <wx/dialog.h>

class wxComboBox;

class SaveTransfer : public wxDialog
{
public:
	SaveTransfer(wxWindow* parent, uint64 title_id, const wxString& source_account, uint32 source_id);

	void EndModal(int retCode) override;

	uint32 GetTargetPersistentId() const { return m_target_id; }
private:
	void OnTransfer(wxCommandEvent& event);

	wxComboBox* m_target_selection;

	uint32 m_target_id = 0;
	const uint64 m_title_id;
	const uint32 m_source_id;
	int m_return_code = wxCANCEL;
};
