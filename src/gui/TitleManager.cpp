#include "gui/TitleManager.h"

#include "gui/helpers/wxCustomEvents.h"
#include "gui/helpers/wxCustomData.h"
#include "Cafe/TitleList/GameInfo.h"
#include "util/helpers/helpers.h"
#include "gui/helpers/wxHelpers.h"
#include "gui/wxHelper.h"
#include "gui/components/wxTitleManagerList.h"
#include "gui/components/wxDownloadManagerList.h"
#include "gui/GameUpdateWindow.h"
#include "gui/dialogs/SaveImport/SaveTransfer.h"

#include <wx/listbase.h>
#include <wx/sizer.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/statbmp.h>
#include <wx/panel.h>
#include <wx/bmpbuttn.h>
#include <wx/choice.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>

#include <pugixml.hpp>
#include <zip.h>
#include <wx/dirdlg.h>
#include <wx/notebook.h>

#include "config/ActiveSettings.h"
#include "gui/dialogs/SaveImport/SaveImportWindow.h"
#include "Cafe/Account/Account.h"
#include "Cemu/Tools/DownloadManager/DownloadManager.h"
#include "gui/CemuApp.h"

#include "Cafe/TitleList/TitleList.h"

#if BOOST_OS_LINUX || BOOST_OS_MACOS
#include "resource/embedded/resources.h"
#endif
#include "Cafe/TitleList/SaveList.h"

wxDEFINE_EVENT(wxEVT_TITLE_FOUND, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_TITLE_SEARCH_COMPLETE, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_DL_TITLE_UPDATE, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_DL_DISCONNECT_COMPLETE, wxCommandEvent);

wxPanel* TitleManager::CreateTitleManagerPage()
{
	auto* panel = new wxPanel(m_notebook);
	auto* sizer = new wxBoxSizer(wxVERTICAL);
	{
		auto* row = new wxFlexGridSizer(0, 4, 0, 0);
		row->AddGrowableCol(1);

		row->Add(new wxStaticText(panel, wxID_ANY, _("Filter")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		m_filter = new wxTextCtrl(panel, wxID_ANY);
		m_filter->Bind(wxEVT_TEXT, &TitleManager::OnFilterChanged, this);
		row->Add(m_filter, 1, wxALL | wxEXPAND, 5);

		const wxImage refresh = wxBITMAP_PNG(PNG_REFRESH).ConvertToImage();
		m_refresh_button = new wxBitmapButton(panel, wxID_ANY, refresh.Scale(16, 16));
		m_refresh_button->Disable();
		m_refresh_button->Bind(wxEVT_BUTTON, &TitleManager::OnRefreshButton, this);
		m_refresh_button->SetToolTip(_("Refresh"));
		row->Add(m_refresh_button, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		auto* help_button = new wxStaticBitmap(panel, wxID_ANY, wxBITMAP_PNG(PNG_HELP));
		help_button->SetToolTip(wxStringFormat2(_("The following prefixes are supported:\n{0}\n{1}\n{2}\n{3}\n{4}"),
			"titleid:", "name:", "type:", "version:", "region:"));
		row->Add(help_button, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		sizer->Add(row, 0, wxEXPAND, 5);
	}

	sizer->Add(new wxStaticLine(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL), 0, wxEXPAND | wxALL, 5);

	m_title_list = new wxTitleManagerList(panel);
	m_title_list->SetSizeHints(800, 600);
	m_title_list->Bind(wxEVT_LIST_ITEM_SELECTED, &TitleManager::OnTitleSelected, this);
	sizer->Add(m_title_list, 1, wxALL | wxEXPAND, 5);

	sizer->Add(new wxStaticLine(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL), 0, wxEXPAND | wxALL, 5);

	{
		auto* row = new wxFlexGridSizer(0, 3, 0, 0);
		row->AddGrowableCol(2);

		auto* install_button = new wxButton(panel, wxID_ANY, _("Install title"));
		install_button->Bind(wxEVT_BUTTON, &TitleManager::OnInstallTitle, this);
		row->Add(install_button, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		row->Add(new wxStaticLine(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL), 0, wxEXPAND | wxALL, 5);

		{
			m_save_panel = new wxPanel(panel);
			auto* save_sizer = new wxFlexGridSizer(0, 7, 0, 0);

			save_sizer->Add(new wxStaticText(m_save_panel, wxID_ANY, _("Account")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

			m_save_account_list = new wxChoice(m_save_panel, wxID_ANY);
			m_save_account_list->SetMinSize({ 170, -1 });
			m_save_account_list->Bind(wxEVT_CHOICE, &TitleManager::OnSaveAccountSelected, this);
			save_sizer->Add(m_save_account_list, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

			auto* save_open = new wxButton(m_save_panel, wxID_ANY, _("Open directory"));
			save_open->Bind(wxEVT_BUTTON, &TitleManager::OnSaveOpenDirectory, this);
			save_open->SetToolTip(_("Open the directory of the save entry"));
			save_open->Disable();
			save_sizer->Add(save_open, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

			auto* save_transfer = new wxButton(m_save_panel, wxID_ANY, _("Transfer"));
			save_transfer->Bind(wxEVT_BUTTON, &TitleManager::OnSaveTransfer, this);
			save_transfer->SetToolTip(_("Transfers the save entry to another persistent account id"));
			save_transfer->Disable();
			save_sizer->Add(save_transfer, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

			auto* save_delete = new wxButton(m_save_panel, wxID_ANY, _("Delete"));
			save_delete->Bind(wxEVT_BUTTON, &TitleManager::OnSaveDelete, this);
			save_delete->SetToolTip(_("Permanently delete the save entry"));
			save_delete->Disable();
			save_sizer->Add(save_delete, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

			m_save_import = new wxButton(m_save_panel, wxID_ANY, _("Import"));
			m_save_import->Bind(wxEVT_BUTTON, &TitleManager::OnSaveImport, this);
			m_save_import->SetToolTip(_("Imports a zipped save entry"));
			save_sizer->Add(m_save_import, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP | wxBOTTOM, 5);

			auto* export_bttn = new wxButton(m_save_panel, wxID_ANY, _("Export"));
			export_bttn->Bind(wxEVT_BUTTON, &TitleManager::OnSaveExport, this);
			export_bttn->SetToolTip(_("Exports the selected save entry as zip file"));
			export_bttn->Disable();
			save_sizer->Add(export_bttn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxTOP | wxBOTTOM, 5);

			m_save_panel->SetSizerAndFit(save_sizer);
			row->Add(m_save_panel, 1, wxRESERVE_SPACE_EVEN_IF_HIDDEN | wxALIGN_CENTER_VERTICAL, 0);
			m_save_panel->Hide(); // hide by default
		}

		sizer->Add(row, 0, wxEXPAND, 5);
	}

	panel->SetSizerAndFit(sizer);
	return panel;
}

wxPanel* TitleManager::CreateDownloadManagerPage()
{
	auto* panel = new wxPanel(m_notebook);
	auto* sizer = new wxBoxSizer(wxVERTICAL);
	{
		auto* row = new wxBoxSizer(wxHORIZONTAL);

		m_account = new wxChoice(panel, wxID_ANY);
		m_account->SetMinSize({ 250,-1 });
		auto accounts = Account::GetAccounts();
		if (!accounts.empty())
		{
			const auto id = GetConfig().account.m_persistent_id.GetValue();
			for (const auto& a : accounts)
			{
				m_account->Append(a.ToString(), (void*)static_cast<uintptr_t>(a.GetPersistentId()));
				if(a.GetPersistentId() == id)
				{
					m_account->SetSelection(m_account->GetCount() - 1);
				}
			}
		}
	
		row->Add(m_account, 0, wxALL, 5);

		m_connect = new wxButton(panel, wxID_ANY, _("Connect"));
		m_connect->Bind(wxEVT_BUTTON, &TitleManager::OnConnect, this);
		row->Add(m_connect, 0, wxALL, 5);

		sizer->Add(row, 0, wxEXPAND, 5);
	}

	m_status_text = new wxStaticText(panel, wxID_ANY, _("Select an account and press Connect"));
	this->Bind(wxEVT_SET_TEXT, &TitleManager::OnSetStatusText, this);
	sizer->Add(m_status_text, 0, wxALL, 5);
	
	sizer->Add(new wxStaticLine(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL), 0, wxEXPAND | wxALL, 5);

	{
		auto* row = new wxFlexGridSizer(0, 3, 0, 0);
		m_show_titles = new wxCheckBox(panel, wxID_ANY, _("Show available titles"));
		m_show_titles->SetValue(true);
		m_show_titles->Enable(false);
		row->Add(m_show_titles, 0, wxALL, 5);
		m_show_titles->Bind(wxEVT_CHECKBOX, &TitleManager::OnDlFilterCheckbox, this);

		m_show_updates = new wxCheckBox(panel, wxID_ANY, _("Show available updates"));
		m_show_updates->SetValue(true);
		m_show_updates->Enable(false);
		row->Add(m_show_updates, 0, wxALL, 5);
		m_show_updates->Bind(wxEVT_CHECKBOX, &TitleManager::OnDlFilterCheckbox, this);

		m_show_installed = new wxCheckBox(panel, wxID_ANY, _("Show installed"));
		m_show_installed->SetValue(true);
		m_show_installed->Enable(false);
		row->Add(m_show_installed, 0, wxALL, 5);
		m_show_installed->Bind(wxEVT_CHECKBOX, &TitleManager::OnDlFilterCheckbox, this);

		sizer->Add(row, 0, wxEXPAND, 5);
	}
	
	m_download_list = new wxDownloadManagerList(panel);
	m_download_list->SetSizeHints(800, 600);
	m_download_list->Bind(wxEVT_LIST_ITEM_SELECTED, &TitleManager::OnTitleSelected, this);
	sizer->Add(m_download_list, 1, wxALL | wxEXPAND, 5);

	panel->SetSizerAndFit(sizer);
	return panel;
}

TitleManager::TitleManager(wxWindow* parent, TitleManagerPage default_page)
	: wxFrame(parent, wxID_ANY, _("Title manager"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL)
{
	SetIcon(wxICON(X_BOX));
	
	auto* sizer = new wxBoxSizer(wxVERTICAL);
	m_notebook = new wxNotebook(this, wxID_ANY);

	m_notebook->AddPage(CreateTitleManagerPage(), _("Title Manager"), default_page == TitleManagerPage::TitleManager);
	m_notebook->AddPage(CreateDownloadManagerPage(), _("Download Manager"), default_page == TitleManagerPage::DownloadManager);

	sizer->Add(m_notebook, 1, wxEXPAND);

	m_status_bar = CreateStatusBar(2, wxSTB_SIZEGRIP);
	m_status_bar->SetStatusText(_("Searching for titles..."));

	this->SetSizerAndFit(sizer);
	this->Centre(wxBOTH);

	this->Bind(wxEVT_SET_STATUS_BAR_TEXT, &TitleManager::OnSetStatusBarText, this);
	this->Bind(wxEVT_TITLE_FOUND, &TitleManager::OnTitleFound, this);
	this->Bind(wxEVT_TITLE_SEARCH_COMPLETE, &TitleManager::OnTitleSearchComplete, this);
	this->Bind(wxEVT_DL_TITLE_UPDATE, &TitleManager::OnDownloadableTitleUpdate, this);
	this->Bind(wxEVT_DL_DISCONNECT_COMPLETE, &TitleManager::OnDisconnect, this);
	
	// TODO typing on title list should change filter text and filter!

	// if download manager is already active then restore state
	DownloadManager* dlMgr = DownloadManager::GetInstance(false);
	if (dlMgr && dlMgr->IsConnected())
	{
		dlMgr->setUserData(this);
		dlMgr->registerCallbacks(
			TitleManager::Callback_ConnectStatusUpdate,
			TitleManager::Callback_AddDownloadableTitle,
			TitleManager::Callback_RemoveDownloadableTitle);
		SetConnected(true);
	}

	m_callbackId = CafeTitleList::RegisterCallback([](CafeTitleListCallbackEvent* evt, void* ctx) { ((TitleManager*)ctx)->HandleTitleListCallback(evt); }, this);
}

TitleManager::~TitleManager()
{
	CafeTitleList::UnregisterCallback(m_callbackId);

	// unregister callbacks for download manager
	DownloadManager* dlMgr = DownloadManager::GetInstance(false);
	if (dlMgr)
	{
		dlMgr->setUserData(nullptr);
		dlMgr->registerCallbacks(
			nullptr,
			nullptr,
			nullptr);
		// if download manager is still downloading / installing then show a warning
		if (dlMgr->hasActiveDownloads())
		{
			static bool s_showedBGDownloadWarning = false;
			if (!s_showedBGDownloadWarning)
			{
				wxMessageBox(_("Currently active downloads will continue in the background."), _("Information"), wxOK | wxCENTRE, this);
				s_showedBGDownloadWarning = true;
			}
		}
	}

	m_running = false;
}

void TitleManager::SetFocusAndTab(TitleManagerPage page)
{
	m_notebook->SetSelection((int)page);
	this->SetFocus();
}

void TitleManager::SetDownloadStatusText(const wxString& text)
{
	auto* evt = new wxCommandEvent(wxEVT_SET_TEXT);
	evt->SetEventObject(m_status_text);
	evt->SetString(text);
	wxQueueEvent(this, evt);
}

void TitleManager::HandleTitleListCallback(CafeTitleListCallbackEvent* evt)
{
	if (evt->eventType == CafeTitleListCallbackEvent::TYPE::SCAN_FINISHED)
	{
		auto* evt = new wxCommandEvent(wxEVT_TITLE_SEARCH_COMPLETE);
		wxQueueEvent(this, evt);
	}
}

void TitleManager::OnTitleFound(wxCommandEvent& event)
{
	auto* obj = dynamic_cast<wxTitleManagerList::TitleEntryData_t*>(event.GetClientObject());
	wxASSERT(obj);
	m_title_list->AddTitle(obj);
}

void TitleManager::OnTitleSearchComplete(wxCommandEvent& event)
{
	m_isScanning = false;
	if (m_connectRequested)
	{
		InitiateConnect();
		m_connectRequested = false;
	}
	// update status bar text
	m_title_list->SortEntries(-1);
	m_status_bar->SetStatusText(wxStringFormat2(_("Found {0} games, {1} updates, {2} DLCs and {3} save entries"),
		m_title_list->GetCountByType(wxTitleManagerList::EntryType::Base) + m_title_list->GetCountByType(wxTitleManagerList::EntryType::System),
		m_title_list->GetCountByType(wxTitleManagerList::EntryType::Update),
		m_title_list->GetCountByType(wxTitleManagerList::EntryType::Dlc),
		m_title_list->GetCountByType(wxTitleManagerList::EntryType::Save)
	));

	// re-filter if any filter is set
	const auto filter = m_filter->GetValue();
	if (!filter.IsEmpty())
		m_title_list->Filter(m_filter->GetValue());

	m_title_list->AutosizeColumns();
	m_refresh_button->Enable();
}

void TitleManager::OnSetStatusBarText(wxSetStatusBarTextEvent& event)
{
	m_status_bar->SetStatusText(_(event.GetText()), event.GetNumber());
}

void TitleManager::OnFilterChanged(wxCommandEvent& event)
{
	event.Skip();
	m_title_list->Filter(m_filter->GetValue());
}

void TitleManager::OnRefreshButton(wxCommandEvent& event)
{
	m_refresh_button->Disable();
	m_isScanning = true;
	// m_title_list->ClearItems(); -> Dont clear. Refresh() triggers incremental updates via notifications
	m_status_bar->SetStatusText(_("Searching for titles..."));
	CafeTitleList::Refresh();
}

void TitleManager::OnInstallTitle(wxCommandEvent& event)
{
	wxFileDialog openFileDialog(this, _("Select title to install"), "", "", "meta.xml|meta.xml", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if (openFileDialog.ShowModal() == wxID_CANCEL)
		return;

	fs::path filePath(openFileDialog.GetPath().wc_str());
	try
	{
		filePath = filePath.parent_path();
		filePath = filePath.parent_path();
		GameUpdateWindow frame(*this, filePath);
		const int updateResult = frame.ShowModal();

		if (updateResult == wxID_OK)
		{
			CafeTitleList::AddTitleFromPath(frame.GetTargetPath());
		}
		else
		{
			if (frame.GetExceptionMessage().empty())
				wxMessageBox(_("Update installation has been canceled!"));
			else
			{
				throw std::runtime_error(frame.GetExceptionMessage());
			}
		}
	}
	catch (const AbortException&)
	{
		// ignored
	}
	catch (const std::exception& ex)
	{
		wxMessageBox(ex.what(), _("Update error"));
	}
}

static void PopulateSavePersistentIds(wxTitleManagerList::TitleEntry& entry)
{
	if (!entry.persistent_ids.empty())
		return;
	cemu_assert(entry.type == wxTitleManagerList::EntryType::Save);
	SaveInfo saveInfo = CafeSaveList::GetSaveByTitleId(entry.title_id);
	if (!saveInfo.IsValid())
		return;
	fs::path savePath = saveInfo.GetPath();
	savePath /= "user";
	std::error_code ec;
	for (auto it : fs::directory_iterator(savePath, ec))
	{
		if(!it.is_directory(ec))
			continue;
		std::string dirName = it.path().filename().string();
		uint32 persistentId = ConvertString<uint32>(dirName, 16);
		if (persistentId != 0)
			entry.persistent_ids.emplace_back(persistentId);
	}
}

void TitleManager::OnTitleSelected(wxListEvent& event)
{
	event.Skip();

	const auto entry = m_title_list->GetSelectedTitleEntry();
	if(entry.has_value() && entry->type == wxTitleManagerList::EntryType::Save)
	{
		m_save_panel->Show();
		m_save_account_list->Clear();

		PopulateSavePersistentIds(*entry);

		// an account must be selected before any of the control buttons can be used
		for(auto&& v : m_save_panel->GetChildren())
		{
			if (dynamic_cast<wxButton*>(v) && v->GetId() != m_save_import->GetId()) // import is always enabled
				v->Disable();
		}

		const auto& accounts = Account::GetAccounts();
		for (const auto& id : entry->persistent_ids)
		{
			const auto it = std::find_if(accounts.cbegin(), accounts.cend(), [id](const auto& acc) { return acc.GetPersistentId() == id; });
			if(it != accounts.cend())
			{
				m_save_account_list->Append(fmt::format("{:x} ({})", id, 
					boost::nowide::narrow(it->GetMiiName().data(), it->GetMiiName().size())), 
					(void*)(uintptr_t)id);
			}
			else
				m_save_account_list->Append(fmt::format("{:x}", id), (void*)(uintptr_t)id);
		}
	}
	else
	{
		m_save_panel->Hide();
	}
}

void TitleManager::OnSaveOpenDirectory(wxCommandEvent& event)
{
	const auto selection=  m_save_account_list->GetSelection();
	if (selection == wxNOT_FOUND)
		return;

	const auto entry = m_title_list->GetSelectedTitleEntry();
	if (!entry.has_value())
		return;

	const auto persistent_id = (uint32)(uintptr_t)m_save_account_list->GetClientData(selection);

	const fs::path target = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/user/{:08x}", (uint32)(entry->title_id >> 32), (uint32)(entry->title_id & 0xFFFFFFFF), persistent_id);
	if (!fs::exists(target) || !fs::is_directory(target))
		return;

	wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("file:{}", _utf8Wrapper(target))));
}

void TitleManager::OnSaveDelete(wxCommandEvent& event)
{
	const auto selection_index = m_save_account_list->GetSelection();
	if (selection_index == wxNOT_FOUND)
		return;
	
	const auto selection = m_save_account_list->GetStringSelection();
	if (selection.IsEmpty())
		return;
	
	const auto msg = wxStringFormat2(_("Are you really sure that you want to delete the save entry for {}"), selection);
	const auto result = wxMessageBox(msg, _("Warning"), wxYES_NO | wxCENTRE | wxICON_EXCLAMATION, this);
	if (result == wxNO)
		return;

	auto entry = m_title_list->GetSelectedTitleEntry();
	if (!entry.has_value())
		return;

	const auto persistent_id = (uint32)(uintptr_t)m_save_account_list->GetClientData(selection_index);

	const fs::path target = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/user/{:08x}", (uint32)(entry->title_id >> 32), (uint32)(entry->title_id & 0xFFFFFFFF), persistent_id);
	if (!fs::exists(target) || !fs::is_directory(target))
		return;

	// edit meta saveinfo.xml
	bool meta_file_edited = false;
	const fs::path saveinfo = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/meta/saveinfo.xml", (uint32)(entry->title_id >> 32), (uint32)(entry->title_id & 0xFFFFFFFF));
	if (fs::exists(saveinfo) || fs::is_regular_file(saveinfo))
	{
		pugi::xml_document doc;
		if (doc.load_file(saveinfo.c_str()))
		{
			auto info_node = doc.child("info");
			if(info_node)
			{
				auto persistend_id_string = fmt::format(L"{:08x}", persistent_id);
				const auto delete_entry = info_node.find_child([&persistend_id_string](const pugi::xml_node& node)
				{
						return boost::iequals(node.attribute("persistentId").as_string(), persistend_id_string);
				});
				if (delete_entry)
				{
					info_node.remove_child(delete_entry);
					meta_file_edited = doc.save_file(saveinfo.c_str());
				}
			}
		}
	}

	if (!meta_file_edited)
		forceLog_printf("TitleManager::OnSaveDelete: couldn't delete save entry in saveinfo.xml: %s", saveinfo.generic_u8string().c_str());

	// remove from title entry
	auto& persistent_ids = entry->persistent_ids;
	persistent_ids.erase(std::remove(persistent_ids.begin(), persistent_ids.end(), persistent_id), persistent_ids.end());

	std::error_code ec;
	fs::remove_all(target, ec);
	if (ec)
	{
		const auto error_msg = wxStringFormat2(_("Error when trying to delete the save directory:\n{}"), GetSystemErrorMessage(ec));
		wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE, this);
		return;
	}

	m_save_account_list->Delete(selection_index);
}


void TitleManager::OnSaveTransfer(wxCommandEvent& event)
{
	const auto selection_index = m_save_account_list->GetSelection();
	if (selection_index == wxNOT_FOUND)
		return;
	
	const auto selection = m_save_account_list->GetStringSelection();
	if (selection.IsEmpty())
		return;

	auto entry = m_title_list->GetSelectedTitleEntry();
	if (!entry.has_value())
		return;

	const auto persistent_id = (uint32)(uintptr_t)m_save_account_list->GetClientData(selection_index);
	
	SaveTransfer transfer(this, entry->title_id, selection, persistent_id);
	if (transfer.ShowModal() == wxCANCEL)
		return;

	// remove old id entry
	auto& persistent_ids = entry->persistent_ids;
	persistent_ids.erase(std::remove(persistent_ids.begin(), persistent_ids.end(), persistent_id), persistent_ids.end());

	// add new id if not added yet
	const auto new_id = transfer.GetTargetPersistentId();
	if (new_id != 0 && std::find(persistent_ids.cbegin(), persistent_ids.cend(), new_id) == persistent_ids.cend())
	{
		persistent_ids.emplace_back(new_id);

		const auto& account = Account::GetAccount(new_id);
		if(account.GetPersistentId() == new_id)
			m_save_account_list->Append(fmt::format("{:x} ({})", new_id,
				boost::nowide::narrow(account.GetMiiName().data(), account.GetMiiName().size())),
				(void*)(uintptr_t)new_id);
		else
			m_save_account_list->Append(fmt::format("{:x}", new_id), (void*)(uintptr_t)new_id);
	}

	m_save_account_list->Delete(selection_index);
}

void TitleManager::OnSaveAccountSelected(wxCommandEvent& event)
{
	event.Skip();
	for (auto&& v : m_save_panel->GetChildren())
	{
		if (!v->IsEnabled())
			v->Enable();
	}
}

void TitleManager::OnSaveExport(wxCommandEvent& event)
{
	const auto selection_index = m_save_account_list->GetSelection();
	if (selection_index == wxNOT_FOUND)
		return;

	const auto selection = m_save_account_list->GetStringSelection();
	if (selection.IsEmpty())
		return;

	const auto entry = m_title_list->GetSelectedTitleEntry();
	if (!entry.has_value())
		return;

	const auto persistent_id = (uint32)(uintptr_t)m_save_account_list->GetClientData(selection_index);

	wxFileDialog path_dialog(this, _("Select a target file to export the save entry"), entry->path.string(), wxEmptyString, "Exported save entry (*.zip)|*.zip", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
	if (path_dialog.ShowModal() != wxID_OK)
		return;

	const auto path = path_dialog.GetPath();
	if (path.empty())
		return;

	int ze;
	auto* zip = zip_open(path.ToUTF8().data(), ZIP_CREATE | ZIP_TRUNCATE, &ze);
	if (!zip)
	{
		zip_error_t ziperror;
		zip_error_init_with_code(&ziperror, ze);
		const auto error_msg = wxStringFormat2(_("Error when creating the zip for the save entry:\n{}"), zip_error_strerror(&ziperror));
		wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		return;
	}

	// grab all files to zip
	const auto savedir = fs::path(entry->path).append(fmt::format("user/{:08x}", persistent_id));
	const auto savedir_str = savedir.generic_u8string();
	for(const auto& it : fs::recursive_directory_iterator(savedir))
	{
		if (it.path() == "." || it.path() == "..")
			continue;

		const auto entryname = it.path().generic_u8string();
		if(fs::is_directory(it.path()))
		{
			if(zip_dir_add(zip, (const char*)entryname.substr(savedir_str.size() + 1).c_str(), ZIP_FL_ENC_UTF_8) < 0 )
			{
				const auto error_msg = wxStringFormat2(_("Error when trying to add a directory to the zip:\n{}"), zip_strerror(zip));
				wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
			}
		}
		else
		{
			auto* source = zip_source_file(zip, (const char*)entryname.c_str(), 0, 0);
			if(!source)
			{
				const auto error_msg = wxStringFormat2(_("Error when trying to add a file to the zip:\n{}"), zip_strerror(zip));
				wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
			}

			if (zip_file_add(zip, (const char*)entryname.substr(savedir_str.size() + 1).c_str(), source, ZIP_FL_ENC_UTF_8) < 0)
			{
				const auto error_msg = wxStringFormat2(_("Error when trying to add a file to the zip:\n{}"), zip_strerror(zip));
				wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
				
				zip_source_free(source);
			}
		}
	}

	// add own metainfo like file and store title id for verification later
	std::string metacontent = fmt::format("titleId = {:#016x}", entry->title_id);
	auto* metabuff = zip_source_buffer(zip, metacontent.data(), metacontent.size(), 0);
	if(zip_file_add(zip, "cemu_meta", metabuff, ZIP_FL_ENC_UTF_8) < 0)
	{
		const auto error_msg = wxStringFormat2(_("Error when trying to add cemu_meta file to the zip:\n{}"), zip_strerror(zip));
		wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);

		zip_source_free(metabuff);
	}

	zip_close(zip);
}

void TitleManager::OnSaveImport(wxCommandEvent& event)
{
	auto entry = m_title_list->GetSelectedTitleEntry();
	if (!entry.has_value())
		return;
	
	SaveImportWindow save_import(this, entry->title_id);
	if (save_import.ShowModal() == wxCANCEL)
		return;

	// add new id if not added yet
	auto& persistent_ids = entry->persistent_ids;
	const auto new_id = save_import.GetTargetPersistentId();
	if (new_id != 0 && std::find(persistent_ids.cbegin(), persistent_ids.cend(), new_id) == persistent_ids.cend())
	{
		persistent_ids.emplace_back(new_id);

		const auto& account = Account::GetAccount(new_id);
		if (account.GetPersistentId() == new_id)
			m_save_account_list->Append(fmt::format("{:x} ({})", new_id,
				boost::nowide::narrow(account.GetMiiName().data(), account.GetMiiName().size())),
				(void*)(uintptr_t)new_id);
		else
			m_save_account_list->Append(fmt::format("{:x}", new_id), (void*)(uintptr_t)new_id);
	}
	
}

void TitleManager::InitiateConnect()
{
	// init connection to download manager if queued
	uint32 persistentId = (uint32)(uintptr_t)m_account->GetClientData(m_account->GetSelection());
	auto& account = Account::GetAccount(persistentId);

	DownloadManager* dlMgr = DownloadManager::GetInstance();
	dlMgr->reset();
	m_download_list->SetCurrentDownloadMgr(dlMgr);

	std::string deviceCertBase64 = NCrypto::CertECC::GetDeviceCertificate().encodeToBase64();

	if (!NCrypto::SEEPROM_IsPresent())
	{
		SetDownloadStatusText("Dumped online files not found");
		return;
	}

	SetDownloadStatusText("Connecting...");
	// begin async connect
	dlMgr->setUserData(this);
	dlMgr->registerCallbacks(
		TitleManager::Callback_ConnectStatusUpdate,
		TitleManager::Callback_AddDownloadableTitle,
		TitleManager::Callback_RemoveDownloadableTitle);
	dlMgr->connect(account.GetAccountId(), account.GetAccountPasswordCache(), NCrypto::SEEPROM_GetRegion(), NCrypto::GetCountryAsString(account.GetCountry()), NCrypto::GetDeviceId(), NCrypto::GetSerial(), deviceCertBase64);
}

void TitleManager::OnConnect(wxCommandEvent& event)
{
	if (!m_isScanning)
	{
		InitiateConnect();
		SetConnected(true);
	}
	else
	{
		SetDownloadStatusText(_("Getting installed title information..."));
		SetConnected(true);
		m_connectRequested = true;
	}
}

void TitleManager::OnDlFilterCheckbox(wxCommandEvent& event)
{
	m_download_list->Filter2(m_show_titles->GetValue(), m_show_updates->GetValue(), m_show_installed->GetValue());
	m_download_list->SortEntries();
}

void TitleManager::OnSetStatusText(wxCommandEvent& event)
{
	auto* text = wxDynamicCast(event.GetEventObject(), wxControl);
	wxASSERT(text);
	text->SetLabel(event.GetString());
}

void TitleManager::OnDownloadableTitleUpdate(wxCommandEvent& event)
{
	auto* obj = dynamic_cast<wxDownloadManagerList::TitleEntryData_t*>(event.GetClientObject());
	auto entry = obj->GetData();
	m_download_list->AddOrUpdateTitle(obj);
}

void TitleManager::OnDisconnect(wxCommandEvent& event)
{
	SetConnected(false);
}

void TitleManager::SetConnected(bool state)
{
	m_account->Enable(!state);
	m_connect->Enable(!state);

	m_show_titles->Enable(state);
	m_show_updates->Enable(state);
	m_show_installed->Enable(state);
	m_download_list->Enable(state);
}

void TitleManager::Callback_ConnectStatusUpdate(std::string statusText, DLMGR_STATUS_CODE statusCode)
{
	TitleManager* titleManager = static_cast<TitleManager*>(DownloadManager::GetInstance()->getUserData());
	titleManager->SetDownloadStatusText(statusText);
	if (statusCode == DLMGR_STATUS_CODE::FAILED)
	{
		auto* evt = new wxCommandEvent(wxEVT_DL_DISCONNECT_COMPLETE);
		wxQueueEvent(titleManager, evt); // this will set SetConnected() to false
		return;
	}
}

void TitleManager::Callback_AddDownloadableTitle(const DlMgrTitleReport& titleInfo)
{
	TitleManager* titleManager = static_cast<TitleManager*>(DownloadManager::GetInstance()->getUserData());
	wxDownloadManagerList::EntryType type = wxDownloadManagerList::EntryType::Base;
	if (((titleInfo.titleId >> 32) & 0xF) == 0xE)
		type = wxDownloadManagerList::EntryType::Update;
	else if (((titleInfo.titleId >> 32) & 0xF) == 0xC)
		type = wxDownloadManagerList::EntryType::DLC;
	if (titleInfo.status == DlMgrTitleReport::STATUS::INSTALLABLE || titleInfo.status == DlMgrTitleReport::STATUS::INSTALLABLE_UNFINISHED || titleInfo.status == DlMgrTitleReport::STATUS::INSTALLABLE_UPDATE)
	{
		// installable title
		wxDownloadManagerList::TitleEntry titleEntry(type, false, titleInfo.titleId, titleInfo.version, titleInfo.isPaused);
		titleEntry.name = wxString::FromUTF8(titleInfo.name);
		titleEntry.status = wxDownloadManagerList::TitleDownloadStatus::Available;
		titleEntry.progress = 0;
		if (titleInfo.status == DlMgrTitleReport::STATUS::INSTALLABLE_UNFINISHED)
			titleEntry.progress = 1;
		if (titleInfo.status == DlMgrTitleReport::STATUS::INSTALLABLE_UPDATE)
			titleEntry.progress = 2;
		auto* evt = new wxCommandEvent(wxEVT_DL_TITLE_UPDATE);
		evt->SetClientObject(new wxCustomData(titleEntry));
		wxQueueEvent(titleManager, evt);
	}
	else
	{
		// package
		wxDownloadManagerList::TitleEntry titleEntry(type, true, titleInfo.titleId, titleInfo.version, titleInfo.isPaused);
		titleEntry.name = wxString::FromUTF8(titleInfo.name);
		titleEntry.progress = titleInfo.progress;
		titleEntry.progressMax = titleInfo.progressMax;
		switch (titleInfo.status)
		{
		case DlMgrTitleReport::STATUS::INITIALIZING:
			titleEntry.status = wxDownloadManagerList::TitleDownloadStatus::Initializing;
			break;
		case DlMgrTitleReport::STATUS::QUEUED:
			titleEntry.status = wxDownloadManagerList::TitleDownloadStatus::Queued;
			break;
		case DlMgrTitleReport::STATUS::CHECKING:
			titleEntry.status = wxDownloadManagerList::TitleDownloadStatus::Checking;
			break;
		case DlMgrTitleReport::STATUS::DOWNLOADING:
			titleEntry.status = wxDownloadManagerList::TitleDownloadStatus::Downloading;
			titleEntry.progress = titleInfo.progress;
			break;
		case DlMgrTitleReport::STATUS::VERIFYING:
			titleEntry.status = wxDownloadManagerList::TitleDownloadStatus::Verifying;
			break;
		case DlMgrTitleReport::STATUS::INSTALLING:
			titleEntry.status = wxDownloadManagerList::TitleDownloadStatus::Installing;
			break;
		case DlMgrTitleReport::STATUS::INSTALLED:
			titleEntry.status = wxDownloadManagerList::TitleDownloadStatus::Installed;
			break;
		case DlMgrTitleReport::STATUS::HAS_ERROR:
			titleEntry.status = wxDownloadManagerList::TitleDownloadStatus::Error;
			titleEntry.errorMsg = titleInfo.errorMsg;
			break;
		default:
			titleEntry.status = wxDownloadManagerList::TitleDownloadStatus::None;
			break;
		}

		auto* evt = new wxCommandEvent(wxEVT_DL_TITLE_UPDATE);
		evt->SetClientObject(new wxCustomData(titleEntry));
		wxQueueEvent(titleManager, evt);
	}
}

void TitleManager::Callback_RemoveDownloadableTitle(uint64 titleId, uint16 version)
{
	TitleManager* titleManager = static_cast<TitleManager*>(DownloadManager::GetInstance()->getUserData());
	assert_dbg();
}
