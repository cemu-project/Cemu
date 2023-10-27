#pragma once

#include <array>

#include <wx/dialog.h>
#include <wx/frame.h>

#include "Cafe/OS/libs/nsyshid/Infinity.h"
#include "Cafe/OS/libs/nsyshid/Skylander.h"

class wxBoxSizer;
class wxCheckBox;
class wxFlexGridSizer;
class wxNotebook;
class wxPanel;
class wxStaticBox;
class wxString;
class wxTextCtrl;

class EmulatedUSBDeviceFrame : public wxFrame {
  public:
	EmulatedUSBDeviceFrame(wxWindow* parent);
	~EmulatedUSBDeviceFrame();

  private:
	wxCheckBox* m_emulatePortal;
	wxCheckBox* m_emulateBase;
	std::array<wxTextCtrl*, nsyshid::MAX_SKYLANDERS> m_skylanderSlots;
	std::array<wxTextCtrl*, nsyshid::MAX_FIGURES> m_infinitySlots;
	std::array<std::optional<std::tuple<uint8, uint16, uint16>>, nsyshid::MAX_SKYLANDERS> m_skySlots;

	wxPanel* AddSkylanderPage(wxNotebook* notebook);
	wxPanel* AddInfinityPage(wxNotebook* notebook);
	wxBoxSizer* AddSkylanderRow(uint8 row_number, wxStaticBox* box);
	wxBoxSizer* AddInfinityRow(wxString name, uint8 row_number, wxStaticBox* box);
	void LoadSkylander(uint8 slot);
	void LoadSkylanderPath(uint8 slot, wxString path);
	void CreateSkylander(uint8 slot);
	void ClearSkylander(uint8 slot);
	void LoadFigure(uint8 slot);
	void LoadFigurePath(uint8 slot, wxString path);
	void CreateFigure(uint8 slot);
	void ClearFigure(uint8 slot);
	void UpdateSkylanderEdits();
};
class CreateSkylanderDialog : public wxDialog {
  public:
	explicit CreateSkylanderDialog(wxWindow* parent, uint8 slot);
	wxString GetFilePath() const;

  protected:
	wxString m_filePath;
};

class CreateInfinityFigureDialog : public wxDialog {
  public:
	explicit CreateInfinityFigureDialog(wxWindow* parent, uint8 slot);
	wxString GetFilePath() const;

  protected:
	wxString m_filePath;
};