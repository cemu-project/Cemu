#include "CemodPermissionDialog.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

namespace
{
	wxString PermissionName(std::size_t index)
	{
		switch (index)
		{
		case 0: return _("Read host state");
		case 1: return _("Write Mod storage/logging");
		case 2: return _("Input injection");
		case 3: return _("Clipboard");
		case 4: return _("Capture");
		default: return {};
		}
	}
}

CemodPermissionDialog::CemodPermissionDialog(wxWindow* parent, const wxString& gameName,
	std::vector<CemodPermissionRequest> requests)
	: wxDialog(parent, wxID_ANY, _("Mod permissions required"), wxDefaultPosition,
		wxSize(720, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	auto* root = new wxBoxSizer(wxVERTICAL);
	root->Add(new wxStaticText(this, wxID_ANY,
		wxString::Format(_("Enabled Mods require permission settings before %s can start."),
			gameName.c_str())),
		0, wxEXPAND | wxALL, 12);
	root->Add(new wxStaticText(this, wxID_ANY,
		_("Unchecked permissions remain denied. The request will be shown again on the next launch while permissions are missing.")),
		0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

	auto* scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxVSCROLL | wxBORDER_NONE);
	scroll->SetScrollRate(0, FromDIP(10));
	auto* mods = new wxBoxSizer(wxVERTICAL);

	for (auto& request : requests)
	{
		const auto mode = request.executionMode == CemodExecutionMode::TrustedNative ?
			_("trusted native") : _("isolated");
		const auto signature = request.signedPackage ? _("signed") : _("unsigned");
		const auto packageLabel = wxString::FromUTF8(request.modId) + " — " + mode + " / " + signature;
		auto* box = new wxStaticBoxSizer(wxVERTICAL, scroll, packageLabel);
		auto* boxParent = box->GetStaticBox();

		if (request.executionMode == CemodExecutionMode::TrustedNative)
		{
			auto* warning = new wxStaticText(boxParent, wxID_ANY,
				_("Warning: this trusted native Mod executes in the game address space and can access all game memory and Cafe/GX2 APIs."));
			warning->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_HOTLIGHT));
			box->Add(warning, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
		}

		ModRow row{std::move(request)};
		wxString missing;
		for (std::size_t index = 0; index < row.permissions.size(); ++index)
		{
			const auto bit = 1U << index;
			if ((row.request.requestedPermissions & bit) == 0) continue;
			auto* checkbox = new wxCheckBox(boxParent, wxID_ANY, PermissionName(index));
			const bool granted = (row.request.grantedPermissions & bit) != 0;
			checkbox->SetValue(granted);
			row.permissions[index] = checkbox;
			box->Add(checkbox, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
			if (!granted)
			{
				if (!missing.empty()) missing += ", ";
				missing += PermissionName(index);
			}
		}
		if (!missing.empty())
		{
			auto* missingText = new wxStaticText(boxParent, wxID_ANY,
				wxString::Format(_("Currently not allowed: %s"), missing.c_str()));
			missingText->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_HOTLIGHT));
			box->Add(missingText, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
		}
		mods->Add(box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
		m_rows.push_back(std::move(row));
	}
	scroll->SetSizer(mods);
	root->Add(scroll, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

	auto* buttons = new wxBoxSizer(wxHORIZONTAL);
	auto* allowAll = new wxButton(this, wxID_ANY, _("Allow all requested"));
	auto* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
	auto* start = new wxButton(this, wxID_OK, _("Save permissions and start game"));
	buttons->Add(allowAll, 0, wxRIGHT, 8);
	buttons->AddStretchSpacer();
	buttons->Add(cancel, 0, wxRIGHT, 8);
	buttons->Add(start, 0);
	root->Add(buttons, 0, wxEXPAND | wxALL, 12);

	allowAll->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AllowAll(); });
	start->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SaveAndClose(); });
	start->SetDefault();
	SetSizer(root);
	SetMinSize(wxSize(620, 440));
	CentreOnParent();
}

void CemodPermissionDialog::AllowAll()
{
	for (auto& row : m_rows)
		for (auto* checkbox : row.permissions)
			if (checkbox) checkbox->SetValue(true);
}

void CemodPermissionDialog::SaveAndClose()
{
	m_selections.clear();
	for (const auto& row : m_rows)
	{
		std::uint32_t granted{};
		for (std::size_t index = 0; index < row.permissions.size(); ++index)
			if (row.permissions[index] && row.permissions[index]->IsChecked())
				granted |= 1U << index;
		m_selections.push_back({row.request.principal, row.request.requestedPermissions,
			granted & row.request.requestedPermissions});
	}
	EndModal(wxID_OK);
}
