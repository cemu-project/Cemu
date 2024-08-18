#include "SaveImportWindow.h"

#include "pugixml.hpp"
#include <wx/sizer.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/frame.h>
#include <wx/filepicker.h>
#include <wx/msgdlg.h>
#include "zip.h"

#include "Cafe/Account/Account.h"
#include "config/ActiveSettings.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "util/helpers/helpers.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "gui/helpers/wxHelpers.h"

SaveImportWindow::SaveImportWindow(wxWindow* parent, uint64 title_id)
	: wxDialog(parent, wxID_ANY, _("Import save entry"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxFRAME_TOOL_WINDOW | wxSYSTEM_MENU | wxTAB_TRAVERSAL | wxCLOSE_BOX),
	m_title_id(title_id)
{
	auto* sizer = new wxBoxSizer(wxVERTICAL);

	{
		auto* row1 = new wxFlexGridSizer(0, 2, 0, 0);
		row1->AddGrowableCol(1);

		row1->Add(new wxStaticText(this, wxID_ANY, _("Source")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_source_selection = new wxFilePickerCtrl(this, wxID_ANY, wxEmptyString,
			_("Select a zipped save file"),
			formatWxString("{}|*.zip", _("Save entry (*.zip)")));
		m_source_selection->SetMinSize({ 270, -1 });
		row1->Add(m_source_selection, 1, wxALL | wxEXPAND, 5);

		row1->Add(new wxStaticText(this, wxID_ANY, _("Target")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		const auto& accounts = Account::GetAccounts();
		m_target_selection = new wxComboBox(this, wxID_ANY);
		for (const auto& account : accounts)
		{
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
		ok_button->Bind(wxEVT_BUTTON, &SaveImportWindow::OnImport, this);
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

void SaveImportWindow::EndModal(int retCode)
{
	wxDialog::EndModal(retCode);
	SetReturnCode(m_return_code);
}

void SaveImportWindow::OnImport(wxCommandEvent& event)
{
	const auto source_path = m_source_selection->GetPath();
	if (source_path.empty())
		return;

	// try to find cemu_meta file to verify targetid
	const std::string zipfile = source_path.ToUTF8().data();
	
	int ziperr;
	auto* zip = zip_open(zipfile.c_str(), ZIP_RDONLY, &ziperr);
	if (zip)
	{
		const sint32 numEntries = zip_get_num_entries(zip, 0);
		for (sint32 i = 0; i < numEntries; i++)
		{
			zip_stat_t sb{};
			if (zip_stat_index(zip, i, 0, &sb) != 0)
				continue;

			if (!boost::equals(sb.name, "cemu_meta"))
				continue;

			auto* zip_file = zip_fopen_index(zip, i, 0);
			if (zip_file == nullptr)
				continue;

			auto buffer = std::make_unique<char[]>(sb.size);
			if (zip_fread(zip_file, buffer.get(), sb.size) == sb.size)
			{
				std::string_view str{ buffer.get(), sb.size };
				// titleId = {:#016x}
				if(boost::starts_with(str, "titleId = "))
				{
					const uint64_t titleId = ConvertString<uint64>(str.substr(sizeof("titleId = ") + 1), 16);
					if(titleId != 0 && titleId != m_title_id)
					{
						const auto msg = formatWxString(_("You are trying to import a savegame for a different title than your currently selected one: {:016x} vs {:016x}\nAre you sure that you want to continue?"), titleId, m_title_id);
						const auto res = wxMessageBox(msg, _("Error"), wxYES_NO | wxCENTRE | wxICON_WARNING, this);
						if(res == wxNO)
						{
							m_return_code = wxCANCEL;
							Close();
							return;
						}
					}
				}
			}

			zip_fclose(zip_file);

			break;
		}

		zip_close(zip);
	}

	// unzip to tmp folder -> using target path directly now
	std::error_code ec;
	//auto tmp_source = fs::temp_directory_path(ec);
	//if(ec)
	//{
	//	const auto error_msg = formatWxString(_("Error when getting the temp directory path:\n{}"), GetSystemErrorMessage(ec));
	//	wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
	//	return;
	//}
	
	uint32 target_id = 0;
	const auto selection = m_target_selection->GetCurrentSelection();
	if (selection != wxNOT_FOUND)
		target_id = (uint32)(uintptr_t)m_target_selection->GetClientData(selection);

	if (target_id == 0)
	{
		target_id = ConvertString<uint32>(m_target_selection->GetValue().ToStdString(), 16);
		if (target_id < Account::kMinPersistendId)
		{
			const auto msg = formatWxString(_("The given account id is not valid!\nIt must be a hex number bigger or equal than {:08x}"), Account::kMinPersistendId);
			wxMessageBox(msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
			return;
		}

	}

	fs::path target_path = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/user/{:08x}", (uint32)(m_title_id >> 32), (uint32)(m_title_id & 0xFFFFFFFF), target_id);
	if (fs::exists(target_path))
	{
		if (!fs::is_directory(target_path))
		{
			const auto msg = formatWxString(_("There's already a file at the target directory:\n{}"), _pathToUtf8(target_path));
			wxMessageBox(msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
			m_return_code = wxCANCEL;
			Close();
			return;
		}

		const auto msg = _("There's already a save game available for the target account, do you want to overwrite it?\nThis will delete the existing save files for the account and replace them.");
		const auto result = wxMessageBox(msg, _("Error"), wxYES_NO | wxCENTRE | wxICON_WARNING, this);
		if (result == wxNO)
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

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	// extract zip file

	//tmp_source.append("Cemu").append(fmt::format("{}", rand()));

	//tmp_source = getMlcPath(L"usr/save");
	//tmp_source /= fmt::format("{:08x}/{:08x}/user/{:08x}_tmp", (uint32)(m_title_id >> 32), (uint32)(m_title_id & 0xFFFFFFFF), target_id);
	auto tmp_source = target_path;
	
	fs::create_directories(tmp_source, ec);
	if (ec)
	{
		const auto error_msg = formatWxString(_("Error when creating the extraction path:\n{}"), GetSystemErrorMessage(ec));
		wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		return;
	}

	zip = zip_open(zipfile.c_str(), ZIP_RDONLY, &ziperr);
	if (!zip)
	{
		const auto error_msg = formatWxString(_("Error when opening the import zip file:\n{}"), GetSystemErrorMessage(ec));
		wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		return;
	}

	sint32 numEntries = zip_get_num_entries(zip, 0);
	for (sint32 i = 0; i < numEntries; i++)
	{
		zip_stat_t sb{};
		if (zip_stat_index(zip, i, 0, &sb) != 0)
		{
			cemuLog_log(LogType::Force, "zip stat index failed on {} entry", i);
			continue;
		}

		if (boost::starts_with(sb.name, "../") || boost::starts_with(sb.name, "..\\"))
			continue; // bad path

		if (boost::equals(sb.name, "cemu_meta"))
			continue;

		size_t sb_name_len = strlen(sb.name);
		if (sb_name_len == 0)
			continue;

		const auto path = fs::path(tmp_source).append(sb.name);
		if (sb.name[sb_name_len - 1] == '/')
		{
			fs::create_directories(path, ec);
			if (ec)
				cemuLog_log(LogType::Force, "can't create directory {}: {}", sb.name, ec.message());

			continue;
		}

		if (sb.size == 0)
			continue;

		if (sb.size > (1024 * 1024 * 128))
			continue; // skip unusually huge files

		auto* zip_file = zip_fopen_index(zip, i, 0);
		if (zip_file == nullptr)
			continue;

		auto buffer = std::make_unique<char[]>(sb.size);
		if (zip_fread(zip_file, buffer.get(), sb.size) == sb.size)
		{
			std::ofstream file(path, std::ios::out | std::ios::binary);
			if (file.is_open())
				file.write(buffer.get(), sb.size);
		}

		zip_fclose(zip_file);
	}

	zip_close(zip);
	// extracted all files to tmp_source

	// edit meta saveinfo.xml
	fs::path saveinfo = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/meta/saveinfo.xml", (uint32)(m_title_id >> 32), (uint32)(m_title_id & 0xFFFFFFFF));
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
				const auto node_entry = info_node.find_child([&old_persistend_id_string](const pugi::xml_node& node)
					{
						return boost::iequals(node.attribute("persistentId").as_string(), old_persistend_id_string);
					});
				// add save-entry node if not available yet
				if (!node_entry)
				{
					// add new node 
					auto new_persistend_id_string = fmt::format("{:08x}", target_id);
					auto new_node = info_node.append_child("account");
					new_node.append_attribute("persistentId").set_value(new_persistend_id_string.c_str());
					auto timestamp = new_node.append_child("timestamp");
					timestamp.text().set(fmt::format("{:016x}", coreinit::coreinit_getOSTime() / ESPRESSO_TIMER_CLOCK).c_str()); // TODO time not initialized yet?
					
					if(!doc.save_file(saveinfo.c_str()))
						cemuLog_log(LogType::Force, "couldn't insert save entry in saveinfo.xml: {}", _pathToUtf8(saveinfo));
				}
			}
		}
	} // todo create saveinfo if doesnt exist

	
	/*wxMessageBox(fmt::format("{}\n{}", tmp_source.generic_u8string(), target_path.generic_u8string()), _("Error"), wxOK | wxCENTRE, this);

	fs::rename(tmp_source, target_path, ec);
	if (ec)
	{
		const auto error_msg = formatWxString(_("Error when trying to move the extracted save game:\n{}"), GetSystemErrorMessage(ec));
		wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE, this);
		return;
	}*/

	m_target_id = target_id;
	m_return_code = wxOK;
	Close();
}
