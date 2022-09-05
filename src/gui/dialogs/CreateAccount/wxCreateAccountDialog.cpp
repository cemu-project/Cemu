#include "gui/dialogs/CreateAccount/wxCreateAccountDialog.h"
#include "Cafe/Account/Account.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/msgdlg.h>
#include "util/helpers/helpers.h"

wxCreateAccountDialog::wxCreateAccountDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("Create new account"))
{
	auto* main_sizer = new wxFlexGridSizer(0, 2, 0, 0);
	main_sizer->AddGrowableCol(1);
	main_sizer->SetFlexibleDirection(wxBOTH);
	main_sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

	main_sizer->Add(new wxStaticText(this, wxID_ANY, wxT("PersistentId")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	
	m_persistent_id = new wxTextCtrl(this, wxID_ANY, fmt::format("{:x}", Account::GetNextPersistentId()));
	m_persistent_id->SetToolTip(_("The persistent id is the internal folder name used for your saves. Only change this if you are importing saves from a Wii U with a specific id"));
	main_sizer->Add(m_persistent_id, 1, wxALL | wxEXPAND, 5);

	main_sizer->Add(new wxStaticText(this, wxID_ANY, wxT("Mii name")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	m_mii_name = new wxTextCtrl(this, wxID_ANY);
	m_mii_name->SetFocus();
	m_mii_name->SetMaxLength(10);
	main_sizer->Add(m_mii_name, 1, wxALL | wxEXPAND, 5);

	main_sizer->Add(0, 0, 1, wxEXPAND, 5);
	{
		auto* button_sizer = new wxBoxSizer(wxHORIZONTAL);

		m_ok_button = new wxButton(this, wxID_ANY, _("OK"));
		m_ok_button->Bind(wxEVT_BUTTON, &wxCreateAccountDialog::OnOK, this);
		button_sizer->Add(m_ok_button, 0, wxALL, 5);

		m_cancel_buton = new wxButton(this, wxID_ANY, _("Cancel"));
		m_cancel_buton->Bind(wxEVT_BUTTON, &wxCreateAccountDialog::OnCancel, this);
		button_sizer->Add(m_cancel_buton, 0, wxALL, 5);

		main_sizer->Add(button_sizer, 1, wxALIGN_RIGHT, 5);
	}

	this->SetSizerAndFit(main_sizer);
	this->wxWindowBase::Layout();
}

uint32 wxCreateAccountDialog::GetPersistentId() const
{
	const std::string id_string = m_persistent_id->GetValue().c_str().AsChar();
	return ConvertString<uint32>(id_string, 16);
}

wxString wxCreateAccountDialog::GetMiiName() const
{
	return m_mii_name->GetValue();
}

void wxCreateAccountDialog::OnOK(wxCommandEvent& event)
{
	if(m_persistent_id->IsEmpty())
	{
		wxMessageBox(_("No persistent id entered!"), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		return;
	}

	const auto id = GetPersistentId();
	if(id < Account::kMinPersistendId)
	{
		wxMessageBox(fmt::format(fmt::runtime(_("The persistent id must be greater than {:x}!").ToStdString()), Account::kMinPersistendId),
			_("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		return;
	}
	
	const auto& account = Account::GetAccount(id);
	if(account.GetPersistentId() == id)
	{
		const std::wstring msg = fmt::format(fmt::runtime(_("The persistent id {:x} is already in use by account {}!").ToStdWstring()), 
			account.GetPersistentId(), std::wstring{ account.GetMiiName() });
		wxMessageBox(msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		return;
	}
	
	if(m_mii_name->IsEmpty())
	{
		wxMessageBox(_("Account name may not be empty!"), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		return;
	}
	
	EndModal(wxID_OK);
}

void wxCreateAccountDialog::OnCancel(wxCommandEvent& event)
{
	EndModal(wxID_CANCEL);
}