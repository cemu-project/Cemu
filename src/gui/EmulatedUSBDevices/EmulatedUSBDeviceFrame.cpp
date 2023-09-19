#include "gui/EmulatedUSBDevices/EmulatedUSBDeviceFrame.h"

#include <algorithm>
#include <random>

#include "config/CemuConfig.h"
#include "gui/helpers/wxHelpers.h"
#include "gui/wxHelper.h"
#include "util/helpers/helpers.h"

#include "Cafe/OS/libs/nsyshid/nsyshid.h"
#include "Cafe/OS/libs/nsyshid/Skylander.h"

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
	for (int i = 0; i < 16; i++)
	{
		boxSizer->Add(AddSkylanderRow(i, box), 1, wxEXPAND | wxALL, 2);
	}
	panelSizer->Add(boxSizer, 1, wxEXPAND | wxALL, 2);
	panel->SetSizerAndFit(panelSizer);

	return panel;
}

wxBoxSizer* EmulatedUSBDeviceFrame::AddSkylanderRow(uint8 row_number,
													wxStaticBox* box)
{
	auto* row = new wxBoxSizer(wxHORIZONTAL);

	row->Add(new wxStaticText(box, wxID_ANY,
							  fmt::format("{} {}", _("Skylander").ToStdString(),
										  (row_number + 1))),
			 1, wxEXPAND | wxALL, 2);
	m_skylanderSlots[row_number] =
		new wxTextCtrl(box, wxID_ANY, _("None"), wxDefaultPosition, wxDefaultSize,
					   wxTE_READONLY);
	m_skylanderSlots[row_number]->SetMinSize(wxSize(150, -1));
	m_skylanderSlots[row_number]->Disable();
	row->Add(m_skylanderSlots[row_number], 1, wxEXPAND | wxALL, 2);
	auto* loadButton = new wxButton(box, wxID_ANY, _("Load"));
	loadButton->Bind(wxEVT_BUTTON, [row_number, this](wxCommandEvent&) {
		LoadSkylander(row_number);
	});
	auto* createButton = new wxButton(box, wxID_ANY, _("Create"));
	createButton->Bind(wxEVT_BUTTON, [row_number, this](wxCommandEvent&) {
		CreateSkylander(row_number);
	});
	auto* clearButton = new wxButton(box, wxID_ANY, _("Clear"));
	clearButton->Bind(wxEVT_BUTTON, [row_number, this](wxCommandEvent&) {
		ClearSkylander(row_number);
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

	std::array<uint8, 0x40 * 0x10> fileData;
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
	for (auto it = nsyshid::listSkylanders.begin(); it != nsyshid::listSkylanders.end(); it++)
	{
		const uint32 variant = uint32(uint32(it->first.first) << 16) | uint32(it->first.second);
		comboBox->Append(it->second, reinterpret_cast<void*>(variant));
		filterlist.Add(it->second);
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
			wxMessageDialog id_error(this, "Error Converting ID!", "ID Entered is Invalid");
			id_error.ShowModal();
			return;
		}
		long longSkyVar;
		if (!editVar->GetValue().ToLong(&longSkyVar) || longSkyVar > 0xFFFF)
		{
			wxMessageDialog id_error(this, "Error Converting Variant!", "Variant Entered is Invalid");
			id_error.ShowModal();
			return;
		}
		uint16 skyId = longSkyId & 0xFFFF;
		uint16 skyVar = longSkyVar & 0xFFFF;
		const auto foundSky = nsyshid::listSkylanders.find(std::make_pair(skyId, skyVar));
		wxString predefName;
		if (foundSky != nsyshid::listSkylanders.end())
		{
			predefName = foundSky->second + ".sky";
		}
		else
		{
			predefName = wxString::Format(_("Unknown(%i %i).sky"), skyId, skyVar);
		}
		wxFileDialog
			saveFileDialog(this, _("Create Skylander file"), "", predefName,
						   "SKY files (*.sky)|*.sky", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

		if (saveFileDialog.ShowModal() == wxID_CANCEL)
			return;

		m_filePath = saveFileDialog.GetPath();

		wxFileOutputStream output_stream(saveFileDialog.GetPath());
		if (!output_stream.IsOk())
		{
			wxMessageDialog saveError(this, "Error Creating Skylander File");
			return;
		}

		std::array<uint8, 0x40 * 0x10> data{};

		uint32 first_block = 0x690F0F0F;
		uint32 other_blocks = 0x69080F7F;
		memcpy(&data[0x36], &first_block, sizeof(first_block));
		for (size_t index = 1; index < 0x10; index++)
		{
			memcpy(&data[(index * 0x40) + 0x36], &other_blocks, sizeof(other_blocks));
		}
		std::random_device rd;
		std::mt19937 mt(rd());
		std::uniform_int_distribution<int> dist(0, 255);
		data[0] = dist(mt);
		data[1] = dist(mt);
		data[2] = dist(mt);
		data[3] = dist(mt);
		data[4] = data[0] ^ data[1] ^ data[2] ^ data[3];
		data[5] = 0x81;
		data[6] = 0x01;
		data[7] = 0x0F;

		memcpy(&data[0x10], &skyId, sizeof(skyId));
		memcpy(&data[0x1C], &skyVar, sizeof(skyVar));

		uint16 crc = nsyshid::g_skyportal.SkylanderCRC16(0xFFFF, data.data(), 0x1E);

		memcpy(&data[0x1E], &crc, sizeof(crc));

		output_stream.SeekO(0);
		output_stream.WriteAll(data.data(), data.size());
		output_stream.Close();

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

void EmulatedUSBDeviceFrame::UpdateSkylanderEdits()
{
	for (auto i = 0; i < 16; i++)
	{
		std::string displayString;
		if (auto sd = m_skySlots[i])
		{
			auto [portalSlot, skyId, skyVar] = sd.value();
			auto foundSky = nsyshid::listSkylanders.find(std::make_pair(skyId, skyVar));
			if (foundSky != nsyshid::listSkylanders.end())
			{
				displayString = foundSky->second;
			}
			else
			{
				displayString = fmt::format("Unknown (Id:{} Var:{})", skyId, skyVar);
			}
		}
		else
		{
			displayString = "None";
		}

		m_skylanderSlots[i]->ChangeValue(displayString);
	}
}