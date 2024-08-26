#include "gui/EmulatedUSBDevices/EmulatedUSBDeviceFrame.h"

#include <algorithm>

#include "config/CemuConfig.h"
#include "gui/helpers/wxHelpers.h"
#include "gui/wxHelper.h"
#include "util/helpers/helpers.h"

#include "Cafe/OS/libs/nsyshid/nsyshid.h"

#include "Common/FileStream.h"

#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/stream.h>
#include <wx/textctrl.h>
#include <wx/textentry.h>
#include <wx/valnum.h>
#include <wx/wfstream.h>

#include "resource/embedded/resources.h"
#include "EmulatedUSBDeviceFrame.h"

EmulatedUSBDeviceFrame::EmulatedUSBDeviceFrame(wxWindow* parent)
	: wxFrame(parent, wxID_ANY, _("Emulated USB Devices"), wxDefaultPosition,
			  wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL)
{
	SetIcon(wxICON(X_BOX));

	auto& config = GetConfig();

	auto* sizer = new wxBoxSizer(wxVERTICAL);
	auto* notebook = new wxNotebook(this, wxID_ANY);

	notebook->AddPage(AddSkylanderPage(notebook), _("Skylanders Portal"));
	notebook->AddPage(AddInfinityPage(notebook), _("Infinity Base"));

	sizer->Add(notebook, 1, wxEXPAND | wxALL, 2);

	SetSizerAndFit(sizer);
	Layout();
	Centre(wxBOTH);
}

EmulatedUSBDeviceFrame::~EmulatedUSBDeviceFrame() {}

wxPanel* EmulatedUSBDeviceFrame::AddSkylanderPage(wxNotebook* notebook)
{
	auto* panel = new wxPanel(notebook);
	auto* panelSizer = new wxBoxSizer(wxVERTICAL);
	auto* box = new wxStaticBox(panel, wxID_ANY, _("Skylanders Manager"));
	auto* boxSizer = new wxStaticBoxSizer(box, wxVERTICAL);

	auto* row = new wxBoxSizer(wxHORIZONTAL);

	m_emulatePortal =
		new wxCheckBox(box, wxID_ANY, _("Emulate Skylander Portal"));
	m_emulatePortal->SetValue(
		GetConfig().emulated_usb_devices.emulate_skylander_portal);
	m_emulatePortal->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
		GetConfig().emulated_usb_devices.emulate_skylander_portal =
			m_emulatePortal->IsChecked();
		g_config.Save();
	});
	row->Add(m_emulatePortal, 1, wxEXPAND | wxALL, 2);
	boxSizer->Add(row, 1, wxEXPAND | wxALL, 2);
	for (int i = 0; i < nsyshid::MAX_SKYLANDERS; i++)
	{
		boxSizer->Add(AddSkylanderRow(i, box), 1, wxEXPAND | wxALL, 2);
	}
	panelSizer->Add(boxSizer, 1, wxEXPAND | wxALL, 2);
	panel->SetSizerAndFit(panelSizer);

	return panel;
}

wxPanel* EmulatedUSBDeviceFrame::AddInfinityPage(wxNotebook* notebook)
{
	auto* panel = new wxPanel(notebook);
	auto* panelSizer = new wxBoxSizer(wxBOTH);
	auto* box = new wxStaticBox(panel, wxID_ANY, _("Infinity Manager"));
	auto* boxSizer = new wxStaticBoxSizer(box, wxBOTH);

	auto* row = new wxBoxSizer(wxHORIZONTAL);

	m_emulateBase =
		new wxCheckBox(box, wxID_ANY, _("Emulate Infinity Base"));
	m_emulateBase->SetValue(
		GetConfig().emulated_usb_devices.emulate_infinity_base);
	m_emulateBase->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
		GetConfig().emulated_usb_devices.emulate_infinity_base =
			m_emulateBase->IsChecked();
		g_config.Save();
	});
	row->Add(m_emulateBase, 1, wxEXPAND | wxALL, 2);
	boxSizer->Add(row, 1, wxEXPAND | wxALL, 2);
	boxSizer->Add(AddInfinityRow("Play Set/Power Disc", 0, box), 1, wxEXPAND | wxALL, 2);
	boxSizer->Add(AddInfinityRow("Power Disc Two", 1, box), 1, wxEXPAND | wxALL, 2);
	boxSizer->Add(AddInfinityRow("Power Disc Three", 2, box), 1, wxEXPAND | wxALL, 2);
	boxSizer->Add(AddInfinityRow("Player One", 3, box), 1, wxEXPAND | wxALL, 2);
	boxSizer->Add(AddInfinityRow("Player One Ability One", 4, box), 1, wxEXPAND | wxALL, 2);
	boxSizer->Add(AddInfinityRow("Player One Ability Two", 5, box), 1, wxEXPAND | wxALL, 2);
	boxSizer->Add(AddInfinityRow("Player Two", 6, box), 1, wxEXPAND | wxALL, 2);
	boxSizer->Add(AddInfinityRow("Player Two Ability One", 7, box), 1, wxEXPAND | wxALL, 2);
	boxSizer->Add(AddInfinityRow("Player Two Ability Two", 8, box), 1, wxEXPAND | wxALL, 2);

	panelSizer->Add(boxSizer, 1, wxEXPAND | wxALL, 2);
	panel->SetSizerAndFit(panelSizer);

	return panel;
}

wxBoxSizer* EmulatedUSBDeviceFrame::AddSkylanderRow(uint8 rowNumber,
													wxStaticBox* box)
{
	auto* row = new wxBoxSizer(wxHORIZONTAL);

	row->Add(new wxStaticText(box, wxID_ANY,
							  fmt::format("{} {}", _("Skylander").ToStdString(),
										  (rowNumber + 1))),
			 1, wxEXPAND | wxALL, 2);
	m_skylanderSlots[rowNumber] =
		new wxTextCtrl(box, wxID_ANY, _("None"), wxDefaultPosition, wxDefaultSize,
					   wxTE_READONLY);
	m_skylanderSlots[rowNumber]->SetMinSize(wxSize(150, -1));
	m_skylanderSlots[rowNumber]->Disable();
	row->Add(m_skylanderSlots[rowNumber], 1, wxEXPAND | wxALL, 2);
	auto* loadButton = new wxButton(box, wxID_ANY, _("Load"));
	loadButton->Bind(wxEVT_BUTTON, [rowNumber, this](wxCommandEvent&) {
		LoadSkylander(rowNumber);
	});
	auto* createButton = new wxButton(box, wxID_ANY, _("Create"));
	createButton->Bind(wxEVT_BUTTON, [rowNumber, this](wxCommandEvent&) {
		CreateSkylander(rowNumber);
	});
	auto* clearButton = new wxButton(box, wxID_ANY, _("Clear"));
	clearButton->Bind(wxEVT_BUTTON, [rowNumber, this](wxCommandEvent&) {
		ClearSkylander(rowNumber);
	});
	row->Add(loadButton, 1, wxEXPAND | wxALL, 2);
	row->Add(createButton, 1, wxEXPAND | wxALL, 2);
	row->Add(clearButton, 1, wxEXPAND | wxALL, 2);

	return row;
}

wxBoxSizer* EmulatedUSBDeviceFrame::AddInfinityRow(wxString name, uint8 rowNumber, wxStaticBox* box)
{
	auto* row = new wxBoxSizer(wxHORIZONTAL);

	row->Add(new wxStaticText(box, wxID_ANY, name), 1, wxEXPAND | wxALL, 2);
	m_infinitySlots[rowNumber] =
		new wxTextCtrl(box, wxID_ANY, _("None"), wxDefaultPosition, wxDefaultSize,
					   wxTE_READONLY);
	m_infinitySlots[rowNumber]->SetMinSize(wxSize(150, -1));
	m_infinitySlots[rowNumber]->Disable();
	row->Add(m_infinitySlots[rowNumber], 1, wxALL | wxEXPAND, 5);
	auto* loadButton = new wxButton(box, wxID_ANY, _("Load"));
	loadButton->Bind(wxEVT_BUTTON, [rowNumber, this](wxCommandEvent&) {
		LoadFigure(rowNumber);
	});
	auto* createButton = new wxButton(box, wxID_ANY, _("Create"));
	createButton->Bind(wxEVT_BUTTON, [rowNumber, this](wxCommandEvent&) {
		CreateFigure(rowNumber);
	});
	auto* clearButton = new wxButton(box, wxID_ANY, _("Clear"));
	clearButton->Bind(wxEVT_BUTTON, [rowNumber, this](wxCommandEvent&) {
		ClearFigure(rowNumber);
	});
	row->Add(loadButton, 1, wxEXPAND | wxALL, 2);
	row->Add(createButton, 1, wxEXPAND | wxALL, 2);
	row->Add(clearButton, 1, wxEXPAND | wxALL, 2);

	return row;
}

void EmulatedUSBDeviceFrame::LoadSkylander(uint8 slot)
{
	wxFileDialog openFileDialog(this, _("Open Skylander dump"), "", "",
								"Skylander files (*.sky;*.bin;*.dump;*.dmp)|*.sky;*.bin;*.dump;*.dmp",
								wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if (openFileDialog.ShowModal() != wxID_OK || openFileDialog.GetPath().empty())
		return;

	LoadSkylanderPath(slot, openFileDialog.GetPath());
}

void EmulatedUSBDeviceFrame::LoadSkylanderPath(uint8 slot, wxString path)
{
	std::unique_ptr<FileStream> skyFile(FileStream::openFile2(_utf8ToPath(path.utf8_string()), true));
	if (!skyFile)
	{
		wxMessageDialog open_error(this, "Error Opening File: " + path.c_str());
		open_error.ShowModal();
		return;
	}

	std::array<uint8, nsyshid::SKY_FIGURE_SIZE> fileData;
	if (skyFile->readData(fileData.data(), fileData.size()) != fileData.size())
	{
		wxMessageDialog open_error(this, "Failed to read file! File was too small");
		open_error.ShowModal();
		return;
	}
	ClearSkylander(slot);

	uint16 skyId = uint16(fileData[0x11]) << 8 | uint16(fileData[0x10]);
	uint16 skyVar = uint16(fileData[0x1D]) << 8 | uint16(fileData[0x1C]);

	uint8 portalSlot = nsyshid::g_skyportal.LoadSkylander(fileData.data(),
														  std::move(skyFile));
	m_skySlots[slot] = std::tuple(portalSlot, skyId, skyVar);
	UpdateSkylanderEdits();
}

void EmulatedUSBDeviceFrame::CreateSkylander(uint8 slot)
{
	CreateSkylanderDialog create_dlg(this, slot);
	create_dlg.ShowModal();
	if (create_dlg.GetReturnCode() == 1)
	{
		LoadSkylanderPath(slot, create_dlg.GetFilePath());
	}
}

void EmulatedUSBDeviceFrame::ClearSkylander(uint8 slot)
{
	if (auto slotInfos = m_skySlots[slot])
	{
		auto [curSlot, id, var] = slotInfos.value();
		nsyshid::g_skyportal.RemoveSkylander(curSlot);
		m_skySlots[slot] = {};
		UpdateSkylanderEdits();
	}
}

CreateSkylanderDialog::CreateSkylanderDialog(wxWindow* parent, uint8 slot)
	: wxDialog(parent, wxID_ANY, _("Skylander Figure Creator"), wxDefaultPosition, wxSize(500, 150))
{
	auto* sizer = new wxBoxSizer(wxVERTICAL);

	auto* comboRow = new wxBoxSizer(wxHORIZONTAL);

	auto* comboBox = new wxComboBox(this, wxID_ANY);
	comboBox->Append("---Select---", reinterpret_cast<void*>(0xFFFFFFFF));
	wxArrayString filterlist;
	for (const auto& it : nsyshid::g_skyportal.GetListSkylanders())
	{
		const uint32 variant = uint32(uint32(it.first.first) << 16) | uint32(it.first.second);
		comboBox->Append(it.second, reinterpret_cast<void*>(variant));
		filterlist.Add(it.second);
	}
	comboBox->SetSelection(0);
	bool enabled = comboBox->AutoComplete(filterlist);
	comboRow->Add(comboBox, 1, wxEXPAND | wxALL, 2);

	auto* idVarRow = new wxBoxSizer(wxHORIZONTAL);

	wxIntegerValidator<uint32> validator;

	auto* labelId = new wxStaticText(this, wxID_ANY, "ID:");
	auto* labelVar = new wxStaticText(this, wxID_ANY, "Variant:");
	auto* editId = new wxTextCtrl(this, wxID_ANY, _("0"), wxDefaultPosition, wxDefaultSize, 0, validator);
	auto* editVar = new wxTextCtrl(this, wxID_ANY, _("0"), wxDefaultPosition, wxDefaultSize, 0, validator);

	idVarRow->Add(labelId, 1, wxALL, 5);
	idVarRow->Add(editId, 1, wxALL, 5);
	idVarRow->Add(labelVar, 1, wxALL, 5);
	idVarRow->Add(editVar, 1, wxALL, 5);

	auto* buttonRow = new wxBoxSizer(wxHORIZONTAL);

	auto* createButton = new wxButton(this, wxID_ANY, _("Create"));
	createButton->Bind(wxEVT_BUTTON, [editId, editVar, this](wxCommandEvent&) {
		long longSkyId;
		if (!editId->GetValue().ToLong(&longSkyId) || longSkyId > 0xFFFF)
		{
			wxMessageDialog idError(this, "Error Converting ID!", "ID Entered is Invalid");
			idError.ShowModal();
			return;
		}
		long longSkyVar;
		if (!editVar->GetValue().ToLong(&longSkyVar) || longSkyVar > 0xFFFF)
		{
			wxMessageDialog idError(this, "Error Converting Variant!", "Variant Entered is Invalid");
			idError.ShowModal();
			return;
		}
		uint16 skyId = longSkyId & 0xFFFF;
		uint16 skyVar = longSkyVar & 0xFFFF;
		wxString predefName = nsyshid::g_skyportal.FindSkylander(skyId, skyVar) + ".sky";
		wxFileDialog
			saveFileDialog(this, _("Create Skylander file"), "", predefName,
						   "SKY files (*.sky)|*.sky", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

		if (saveFileDialog.ShowModal() == wxID_CANCEL)
			return;

		m_filePath = saveFileDialog.GetPath();
		
		if(!nsyshid::g_skyportal.CreateSkylander(_utf8ToPath(m_filePath.utf8_string()), skyId, skyVar))
		{
			wxMessageDialog errorMessage(this, "Failed to create file");
			errorMessage.ShowModal();
			this->EndModal(0);
			return;
		}

		this->EndModal(1);
	});
	auto* cancelButton = new wxButton(this, wxID_ANY, _("Cancel"));
	cancelButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		this->EndModal(0);
	});

	comboBox->Bind(wxEVT_COMBOBOX, [comboBox, editId, editVar, this](wxCommandEvent&) {
		const uint64 sky_info = reinterpret_cast<uint64>(comboBox->GetClientData(comboBox->GetSelection()));
		if (sky_info != 0xFFFFFFFF)
		{
			const uint16 skyId = sky_info >> 16;
			const uint16 skyVar = sky_info & 0xFFFF;

			editId->SetValue(wxString::Format(wxT("%i"), skyId));
			editVar->SetValue(wxString::Format(wxT("%i"), skyVar));
		}
	});

	buttonRow->Add(createButton, 1, wxALL, 5);
	buttonRow->Add(cancelButton, 1, wxALL, 5);

	sizer->Add(comboRow, 1, wxEXPAND | wxALL, 2);
	sizer->Add(idVarRow, 1, wxEXPAND | wxALL, 2);
	sizer->Add(buttonRow, 1, wxEXPAND | wxALL, 2);

	this->SetSizer(sizer);
	this->Centre(wxBOTH);
}

wxString CreateSkylanderDialog::GetFilePath() const
{
	return m_filePath;
}

CreateInfinityFigureDialog::CreateInfinityFigureDialog(wxWindow* parent, uint8 slot)
	: wxDialog(parent, wxID_ANY, _("Infinity Figure Creator"), wxDefaultPosition, wxSize(500, 150))
{
	auto* sizer = new wxBoxSizer(wxVERTICAL);

	auto* comboRow = new wxBoxSizer(wxHORIZONTAL);

	auto* comboBox = new wxComboBox(this, wxID_ANY);
	comboBox->Append("---Select---", reinterpret_cast<void*>(0xFFFFFF));
	wxArrayString filterlist;
	for (const auto& it : nsyshid::g_infinitybase.GetFigureList())
	{
		const uint32 figure = it.first;
		if ((slot == 0 &&
			 ((figure > 0x1E8480 && figure < 0x2DC6BF) || (figure > 0x3D0900 && figure < 0x4C4B3F))) ||
			((slot == 1 || slot == 2) && (figure > 0x3D0900 && figure < 0x4C4B3F)) ||
			((slot == 3 || slot == 6) && figure < 0x1E847F) ||
			((slot == 4 || slot == 5 || slot == 7 || slot == 8) &&
			 (figure > 0x2DC6C0 && figure < 0x3D08FF)))
		{
			comboBox->Append(it.second.second, reinterpret_cast<void*>(figure));
			filterlist.Add(it.second.second);
		}
	}
	comboBox->SetSelection(0);
	bool enabled = comboBox->AutoComplete(filterlist);
	comboRow->Add(comboBox, 1, wxEXPAND | wxALL, 2);

	auto* figNumRow = new wxBoxSizer(wxHORIZONTAL);

	wxIntegerValidator<uint32> validator;

	auto* labelFigNum = new wxStaticText(this, wxID_ANY, "Figure Number:");
	auto* editFigNum = new wxTextCtrl(this, wxID_ANY, _("0"), wxDefaultPosition, wxDefaultSize, 0, validator);

	figNumRow->Add(labelFigNum, 1, wxALL, 5);
	figNumRow->Add(editFigNum, 1, wxALL, 5);

	auto* buttonRow = new wxBoxSizer(wxHORIZONTAL);

	auto* createButton = new wxButton(this, wxID_ANY, _("Create"));
	createButton->Bind(wxEVT_BUTTON, [editFigNum, this](wxCommandEvent&) {
		long longFigNum;
		if (!editFigNum->GetValue().ToLong(&longFigNum))
		{
			wxMessageDialog idError(this, "Error Converting Figure Number!", "Number Entered is Invalid");
			idError.ShowModal();
			this->EndModal(0);
		}
		uint32 figNum = longFigNum & 0xFFFFFFFF;
		auto figure = nsyshid::g_infinitybase.FindFigure(figNum);
		wxString predefName = figure.second + ".bin";
		wxFileDialog
			saveFileDialog(this, _("Create Infinity Figure file"), "", predefName,
						   "BIN files (*.bin)|*.bin", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

		if (saveFileDialog.ShowModal() == wxID_CANCEL)
			this->EndModal(0);

		m_filePath = saveFileDialog.GetPath();

		nsyshid::g_infinitybase.CreateFigure(_utf8ToPath(m_filePath.utf8_string()), figNum, figure.first);

		this->EndModal(1);
	});
	auto* cancelButton = new wxButton(this, wxID_ANY, _("Cancel"));
	cancelButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		this->EndModal(0);
	});

	comboBox->Bind(wxEVT_COMBOBOX, [comboBox, editFigNum, this](wxCommandEvent&) {
		const uint64 fig_info = reinterpret_cast<uint64>(comboBox->GetClientData(comboBox->GetSelection()));
		if (fig_info != 0xFFFFFF)
		{
			const uint32 figNum = fig_info & 0xFFFFFFFF;

			editFigNum->SetValue(wxString::Format(wxT("%i"), figNum));
		}
	});

	buttonRow->Add(createButton, 1, wxALL, 5);
	buttonRow->Add(cancelButton, 1, wxALL, 5);

	sizer->Add(comboRow, 1, wxEXPAND | wxALL, 2);
	sizer->Add(figNumRow, 1, wxEXPAND | wxALL, 2);
	sizer->Add(buttonRow, 1, wxEXPAND | wxALL, 2);

	this->SetSizer(sizer);
	this->Centre(wxBOTH);
}

wxString CreateInfinityFigureDialog::GetFilePath() const
{
	return m_filePath;
}

void EmulatedUSBDeviceFrame::LoadFigure(uint8 slot)
{
	wxFileDialog openFileDialog(this, _("Open Infinity Figure dump"), "", "",
								"BIN files (*.bin)|*.bin",
								wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if (openFileDialog.ShowModal() != wxID_OK || openFileDialog.GetPath().empty())
	{
		wxMessageDialog errorMessage(this, "File Okay Error");
		errorMessage.ShowModal();
		return;
	}

	LoadFigurePath(slot, openFileDialog.GetPath());
}

void EmulatedUSBDeviceFrame::LoadFigurePath(uint8 slot, wxString path)
{
	std::unique_ptr<FileStream> infFile(FileStream::openFile2(_utf8ToPath(path.utf8_string()), true));
	if (!infFile)
	{
		wxMessageDialog errorMessage(this, "File Open Error");
		errorMessage.ShowModal();
		return;
	}

	std::array<uint8, nsyshid::INF_FIGURE_SIZE> fileData;
	if (infFile->readData(fileData.data(), fileData.size()) != fileData.size())
	{
		wxMessageDialog open_error(this, "Failed to read file! File was too small");
		open_error.ShowModal();
		return;
	}
	ClearFigure(slot);

	uint32 number = nsyshid::g_infinitybase.LoadFigure(fileData, std::move(infFile), slot);
	m_infinitySlots[slot]->ChangeValue(nsyshid::g_infinitybase.FindFigure(number).second);
}

void EmulatedUSBDeviceFrame::CreateFigure(uint8 slot)
{
	cemuLog_log(LogType::Force, "Create Figure: {}", slot);
	CreateInfinityFigureDialog create_dlg(this, slot);
	create_dlg.ShowModal();
	if (create_dlg.GetReturnCode() == 1)
	{
		LoadFigurePath(slot, create_dlg.GetFilePath());
	}
}

void EmulatedUSBDeviceFrame::ClearFigure(uint8 slot)
{
	m_infinitySlots[slot]->ChangeValue("None");
	nsyshid::g_infinitybase.RemoveFigure(slot);
}

void EmulatedUSBDeviceFrame::UpdateSkylanderEdits()
{
	for (auto i = 0; i < nsyshid::MAX_SKYLANDERS; i++)
	{
		std::string displayString;
		if (auto sd = m_skySlots[i])
		{
			auto [portalSlot, skyId, skyVar] = sd.value();
			displayString = nsyshid::g_skyportal.FindSkylander(skyId, skyVar);
		}
		else
		{
			displayString = "None";
		}

		m_skylanderSlots[i]->ChangeValue(displayString);
	}
}