#include "SaveTransfer.h"

#include "pugixml.hpp"
#include <wx/sizer.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/frame.h>
#include <wx/msgdlg.h>

#include "Cafe/Account/Account.h"
#include "config/ActiveSettings.h"
#include "util/helpers/helpers.h"
#include "gui/helpers/wxHelpers.h"

SaveTransfer::SaveTransfer(wxWindow* parent, uint64 title_id, const wxString& source_account, uint32 source_id)
: wxDialog(parent, wxID_ANY, _("Save transfer"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxFRAME_TOOL_WINDOW | wxSYSTEM_MENU | wxTAB_TRAVERSAL | wxCLOSE_BOX),
	m_title_id(title_id), m_source_id(source_id)
{
	auto* sizer = new wxBoxSizer(wxVERTICAL);

	{
		auto* row1 = new wxFlexGridSizer(0, 2, 0, 0);
		row1->AddGrowableCol(1);

		row1->Add(new wxStaticText(this, wxID_ANY, _("Source")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		auto* source_choice = new wxChoice(this, wxID_ANY);
		source_choice->SetMinSize({170, -1});
		source_choice->Append(source_account);
		source_choice->SetSelection(0);
		row1->Add(source_choice, 1, wxALL | wxEXPAND, 5);

		row1->Add(new wxStaticText(this, wxID_ANY, _("Target")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		const auto& accounts = Account::GetAccounts();
		m_target_selection = new wxComboBox(this, wxID_ANY);
		for (const auto& account : accounts)
		{
			if (account.GetPersistentId() == m_source_id)
				continue;

			m_target_selection->Append(fmt::format("{:x} ({})", account.GetPersistentId(),
			                           boost::nowide::narrow(account.GetMiiName().data(), account.GetMiiName().size())),
			               (void*)(uintptr_t)account.GetPersistentId());
		}
		row1->Add(m_target_selection, 1, wxALL | wxEXPAND, 5);

		sizer->Add(row1, 0, wxEXPAND, 5);
	}

	{
		auto* row2 = new wxFlexGridSizer(0, 2, 0, 0);
		row2->AddGrowableCol(1);
		row2->SetFlexibleDirection(wxBOTH);
		row2->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

		auto* ok_button = new wxButton(this, wxID_ANY, _("OK"), wxDefaultPosition, wxDefaultSize, 0);
		ok_button->Bind(wxEVT_BUTTON, &SaveTransfer::OnTransfer, this);
		row2->Add(ok_button, 0, wxALL, 5);

		auto* cancel_button = new wxButton(this, wxID_ANY, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0);
		cancel_button->Bind(wxEVT_BUTTON, [this](auto&)
		{
			m_return_code = wxCANCEL;
			Close();
		});
		row2->Add(cancel_button, 0, wxALIGN_RIGHT | wxALL, 5);

		sizer->Add(row2, 1, wxEXPAND, 5);
	}

	this->SetSizerAndFit(sizer);
	this->Centre(wxBOTH);
}

void SaveTransfer::EndModal(int retCode)
{
	wxDialog::EndModal(retCode);
	SetReturnCode(m_return_code);
}

void SaveTransfer::OnTransfer(wxCommandEvent& event)
{
	uint32 target_id = 0;
	const auto selection = m_target_selection->GetCurrentSelection();
	if(selection != wxNOT_FOUND)
		target_id = (uint32)(uintptr_t)m_target_selection->GetClientData(selection);
		
	if (target_id == 0)
	{
		target_id = ConvertString<uint32>(m_target_selection->GetValue().ToStdString(), 16);
		if(target_id < Account::kMinPersistendId)
		{
			const auto msg = formatWxString(_("The given account id is not valid!\nIt must be a hex number bigger or equal than {:08x}"), Account::kMinPersistendId);
			wxMessageBox(msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
			return;
		}
			
	}
	
	const fs::path source_path = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/user/{:08x}", (uint32)(m_title_id >> 32), (uint32)(m_title_id & 0xFFFFFFFF), m_source_id);
	if (!fs::exists(source_path) || !fs::is_directory(source_path))
		return;
	
	const fs::path target_path = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/user/{:08x}", (uint32)(m_title_id >> 32), (uint32)(m_title_id & 0xFFFFFFFF), target_id);
	if (fs::exists(target_path))
	{
		if(!fs::is_directory(target_path))
		{
			const auto msg = formatWxString(_("There's already a file at the target directory:\n{}"), _pathToUtf8(target_path));
			wxMessageBox(msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
			m_return_code = wxCANCEL;
			Close();
			return;
		}

		const auto msg = _("There's already a save game available for the target account, do you want to overwrite it?\nThis will delete the existing save files for the account and replace them.");
		const auto result = wxMessageBox(msg, _("Error"), wxYES_NO | wxCENTRE | wxICON_WARNING, this);
		if(result == wxNO)
		{
			m_return_code = wxCANCEL;
			Close();
			return;
		}

		std::error_code ec;
		while (fs::exists(target_path, ec) || ec)
		{
			fs::remove_all(target_path, ec);

			if (ec)
			{
				const auto error_msg = formatWxString(_("Error when trying to delete the former save game:\n{}"), GetSystemErrorMessage(ec));
				wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE, this);
				return;
			}

			// wait so filesystem doesnt 
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	// edit meta saveinfo.xml
	bool meta_file_edited = false;
	const fs::path saveinfo = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/meta/saveinfo.xml", (uint32)(m_title_id >> 32), (uint32)(m_title_id & 0xFFFFFFFF));
	if (fs::exists(saveinfo) || fs::is_regular_file(saveinfo))
	{
		pugi::xml_document doc;
		if (doc.load_file(saveinfo.c_str()))
		{
			auto info_node = doc.child("info");
			if (info_node)
			{
				// delete old node if available
				auto old_persistend_id_string = fmt::format("{:08x}", target_id);
				const auto delete_entry = info_node.find_child([&old_persistend_id_string](const pugi::xml_node& node)
					{
						return boost::iequals(node.attribute("persistentId").as_string(), old_persistend_id_string);
					});
				if (delete_entry)
					info_node.remove_child(delete_entry);
					

				// move new node 
				auto new_persistend_id_string = fmt::format("{:08x}", m_source_id);
				auto move_entry = info_node.find_child([&new_persistend_id_string](const pugi::xml_node& node)
					{
						return boost::iequals(node.attribute("persistentId").as_string(), new_persistend_id_string);
					});
				if(move_entry)
					move_entry.attribute("persistentId").set_value(old_persistend_id_string.c_str());
				else
				{
					// TODO: create if not found! (shouldnt happen though)
				}
				
				meta_file_edited = doc.save_file(saveinfo.c_str());
			}
		}
	}

	if (!meta_file_edited)
		cemuLog_log(LogType::Force, "SaveTransfer::OnTransfer: couldn't update save entry in saveinfo.xml: {}", _pathToUtf8(saveinfo));

	std::error_code ec;
	fs::rename(source_path, target_path, ec);
	if (ec)
	{
		const auto error_msg = formatWxString(_("Error when trying to move the save game:\n{}"), GetSystemErrorMessage(ec));
		wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE, this);
		return;
	}

	m_target_id = target_id;
	m_return_code = wxOK;
	Close();
}
