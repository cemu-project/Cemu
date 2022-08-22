#pragma once

#include <wx/dialog.h>

class SaveImportWindow : public wxDialog
{
public:
	SaveImportWindow(wxWindow* parent, uint64 title_id);

	void EndModal(int retCode) override;

	uint32 GetTargetPersistentId() const { return m_target_id; }
private:
	void OnImport(wxCommandEvent& event);

	class wxFilePickerCtrl* m_source_selection;
	class wxComboBox* m_target_selection;

	uint32 m_target_id = 0;
	const uint64 m_title_id;
	const fs::path m_source_file;
	int m_return_code = wxCANCEL;
};


