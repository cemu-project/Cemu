#include "gui/wxgui.h"
#include "gui/GameUpdateWindow.h"
#include "util/helpers/helpers.h"

#include <filesystem>
#include <sstream>
#include "util/helpers/SystemException.h"
#include "gui/CemuApp.h"
#include "Cafe/TitleList/GameInfo.h"
#include "gui/helpers/wxHelpers.h"
#include "wxHelper.h"

std::string _GetTitleIdTypeStr(TitleId titleId)
{
	TitleIdParser tip(titleId);
	switch (tip.GetType())
	{
	case TitleIdParser::TITLE_TYPE::AOC:
		return _("DLC").ToStdString();
	case TitleIdParser::TITLE_TYPE::BASE_TITLE:
		return _("Base game").ToStdString();
	case TitleIdParser::TITLE_TYPE::BASE_TITLE_DEMO:
		return _("Demo").ToStdString();
	case TitleIdParser::TITLE_TYPE::SYSTEM_TITLE:
	case TitleIdParser::TITLE_TYPE::SYSTEM_OVERLAY_TITLE:
		return _("System title").ToStdString();
	case TitleIdParser::TITLE_TYPE::SYSTEM_DATA:
		return _("System data title").ToStdString();
	case TitleIdParser::TITLE_TYPE::BASE_TITLE_UPDATE:
		return _("Update").ToStdString();
	default:
		break;
	}
	return "Unknown";
}

bool IsRunningInWine();

bool GameUpdateWindow::ParseUpdate(const fs::path& metaPath)
{
	m_title_info = TitleInfo(metaPath);
	if (!m_title_info.IsValid())
		return false;
	fs::path target_location = ActiveSettings::GetMlcPath(m_title_info.GetInstallPath());
	std::error_code ec;
	if (fs::exists(target_location, ec))
	{
		try
		{
			const TitleInfo tmp(target_location);
			if (!tmp.IsValid())
			{
				// does not exist / is not valid. We allow to overwrite it
			}
			else
			{
				TitleIdParser tip(m_title_info.GetAppTitleId());
				TitleIdParser tipOther(tmp.GetAppTitleId());

				if (tip.GetType() != tipOther.GetType())
				{
					std::string typeStrToInstall = _GetTitleIdTypeStr(m_title_info.GetAppTitleId());
					std::string typeStrCurrentlyInstalled = _GetTitleIdTypeStr(tmp.GetAppTitleId());

					std::string wxMsg = wxHelper::MakeUTF8(_("It seems that there is already a title installed at the target location but it has a different type.\nCurrently installed: \'{}\' Installing: \'{}\'\n\nThis can happen for titles which were installed with very old Cemu versions.\nDo you still want to continue with the installation? It will replace the currently installed title."));
					wxMessageDialog dialog(this, fmt::format(fmt::runtime(wxMsg), typeStrCurrentlyInstalled, typeStrToInstall), _("Warning"), wxCENTRE | wxYES_NO | wxICON_EXCLAMATION);
					if (dialog.ShowModal() != wxID_YES)
						return false;
				}
				else if (tmp.GetAppTitleVersion() == m_title_info.GetAppTitleVersion())
				{
					wxMessageDialog dialog(this, _("It seems that the selected title is already installed, do you want to reinstall it?"), _("Warning"), wxCENTRE | wxYES_NO);
					if (dialog.ShowModal() != wxID_YES)
						return false;
				}
				else if (tmp.GetAppTitleVersion() > m_title_info.GetAppTitleVersion())
				{
					wxMessageDialog dialog(this, _("It seems that a newer version is already installed, do you still want to install the older version?"), _("Warning"), wxCENTRE | wxYES_NO);
					if (dialog.ShowModal() != wxID_YES)
						return false;
				}
			}

			// temp rename until done
			m_backup_folder = target_location;
			m_backup_folder.replace_extension(".backup");

			std::error_code ec;
			while (fs::exists(m_backup_folder, ec) || ec)
			{
				fs::remove_all(m_backup_folder, ec);

				if (ec)
				{
					const auto error_msg = wxStringFormat2(_("Error when trying to move former title installation:\n{}"), GetSystemErrorMessage(ec));
					wxMessageBox(error_msg, _("Error"), wxOK | wxCENTRE, this);
					return false;
				}

				// wait so filesystem doesnt 
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			fs::rename(target_location, m_backup_folder);
		}
		catch (const std::exception& ex)
		{
			forceLog_printf("GameUpdateWindow::ParseUpdate exist-error: %s at %s", ex.what(), target_location.generic_u8string().c_str());
		}
	}

	m_target_path = target_location;

	fs::path source(metaPath);

	m_source_paths =
	{
		fs::path(source).append("content"),
		fs::path(source).append("code"),
		fs::path(source).append("meta")
	};

	m_required_size = 0;
	for (auto& path : m_source_paths)
	{
		for (const fs::directory_entry& f : fs::recursive_directory_iterator(path))
		{
			if (is_regular_file(f.path()))
				m_required_size += file_size(f.path());
		}
	}

	// checking size is buggy on Wine (on Steam Deck this would return values too small to install bigger updates) - we therefore skip this step
	if(!IsRunningInWine())
	{
		const fs::space_info targetSpace = fs::space(target_location.root_path());
		if (targetSpace.free <= m_required_size)
		{
			auto string = wxStringFormat(_("Not enough space available.\nRequired: {0} MB\nAvailable: {1} MB"), L"%lld %lld", (m_required_size / 1024 / 1024), (targetSpace.free / 1024 / 1024));
			throw std::runtime_error(string);
		}
	}

	return true;
}

GameUpdateWindow::GameUpdateWindow(wxWindow& parent, const fs::path& filePath)
	: wxDialog(&parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxCAPTION | wxMINIMIZE_BOX | wxSYSTEM_MENU | wxTAB_TRAVERSAL | wxCLOSE_BOX),
	  m_thread_state(ThreadRunning)
{
	try
	{
		#if BOOST_OS_WINDOWS
		SetLastError(0);
		#endif
		if(!ParseUpdate(filePath))
			throw AbortException();
	}
	catch (const std::runtime_error& ex)
	{
		throw SystemException(ex);
	}
	
	auto sizer = new wxBoxSizer(wxVERTICAL);

	TitleIdParser tip(GetTitleId());

	if (tip.GetType() == TitleIdParser::TITLE_TYPE::AOC)
		SetTitle(_("Installing DLC..."));
	else if (tip.GetType() == TitleIdParser::TITLE_TYPE::BASE_TITLE_UPDATE)
		SetTitle(_("Installing update..."));
	else if (tip.IsSystemTitle())
		SetTitle(_("Installing system title..."));
	else
		SetTitle(_("Installing title..."));

	m_processBar = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(500, 20), wxGA_HORIZONTAL);
	m_processBar->SetValue(0);
	m_processBar->SetRange((sint32)(m_required_size / 1000));
	sizer->Add(m_processBar, 0, wxALL | wxEXPAND, 5);

	wxButton* m_cancelButton = new wxButton(this, wxID_ANY, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0);
	m_cancelButton->Bind(wxEVT_BUTTON, &GameUpdateWindow::OnCancelButton, this);
	sizer->Add(m_cancelButton, 0, wxALIGN_RIGHT | wxALL, 5);

	this->SetSizer(sizer);
	this->Centre(wxBOTH);

	wxWindowBase::Layout();
	wxWindowBase::Fit();

	m_timer = new wxTimer(this);
	this->Bind(wxEVT_TIMER, &GameUpdateWindow::OnUpdate, this);
	this->Bind(wxEVT_CLOSE_WINDOW, &GameUpdateWindow::OnClose, this);
	m_timer->Start(250);

	m_thread_state = ThreadRunning;
	m_thread = std::thread(&GameUpdateWindow::ThreadWork, this);
}

void GameUpdateWindow::ThreadWork()
{
	fs::directory_entry currentDirEntry;
	try
	{
		// create base directories
		for (auto& path : m_source_paths)
		{
			if (!path.has_stem())
				continue;

			fs::path targetDir = fs::path(m_target_path) / path.stem();
			create_directories(targetDir);
		}

		const auto target_path = fs::path(m_target_path);
		for (auto& path : m_source_paths)
		{
			if (m_thread_state == ThreadCanceled)
				break;

			if (!path.has_parent_path())
				continue;

			const auto len = path.parent_path().string().size() + 1;
			for (const fs::directory_entry& f : fs::recursive_directory_iterator {path})
			{
				if (m_thread_state == ThreadCanceled)
					break;

				currentDirEntry = f;
				fs::path relative(f.path().string().substr(len));
				fs::path target = fs::path(m_target_path) / relative;
				if (is_directory(f))
				{
					create_directories(target);
					continue;
				}

				copy(f, target, fs::copy_options::overwrite_existing);
				if (is_regular_file(f.path()))
				{
					m_processed_size += file_size(f.path());
				}
			}
		}
	}
	catch (const std::exception& ex)
	{
		std::stringstream error_msg;
		error_msg << GetSystemErrorMessage(ex);

		if(currentDirEntry != fs::directory_entry{})
			error_msg << fmt::format("\n{}\n{}",_("Current file:").ToStdString(), _utf8Wrapper(currentDirEntry.path()));

		m_thread_exception = error_msg.str();
		m_thread_state = ThreadCanceled;
	}

	if (m_thread_state == ThreadCanceled)
	{
		if(fs::exists(m_target_path))
			fs::remove_all(m_target_path);
	}
	else
		m_thread_state = ThreadFinished;
}

GameUpdateWindow::~GameUpdateWindow()
{
	m_timer->Stop();
	if (m_thread.joinable())
		m_thread.join();
}

int GameUpdateWindow::ShowModal()
{
	wxDialog::ShowModal();
	return m_thread_state == ThreadCanceled ? wxID_CANCEL : wxID_OK;
}

void GameUpdateWindow::OnClose(wxCloseEvent& event)
{
	if (m_thread_state == ThreadRunning)
	{
		wxMessageDialog dialog(this, _("Do you really want to cancel the installation process?\n\nCanceling the process will delete the applied files."), _("Info"), wxCENTRE | wxYES_NO);
		if (dialog.ShowModal() != wxID_YES)
			return;

		m_thread_state = ThreadCanceled;
	}

	m_timer->Stop();
	if (m_thread.joinable())
		m_thread.join();

	if(!m_backup_folder.empty())
	{
		if(m_thread_state == ThreadCanceled)
		{
			// restore backup
			try
			{
				if(fs::exists(m_target_path))
					fs::remove_all(m_target_path);

				fs::rename(m_backup_folder, m_target_path);
			}
			catch (const std::exception& ex)
			{
				forceLogDebug_printf("can't restore update backup: %s",ex.what());
			}
		}
		else
		{
			// delete backup
			try
			{
				if(fs::exists(m_backup_folder))
					fs::remove_all(m_backup_folder);
			}
			catch (const std::exception& ex)
			{
				forceLogDebug_printf("can't delete update backup: %s",ex.what());
			}
		}
		
		m_backup_folder.clear();
	}

	event.Skip();
}

void GameUpdateWindow::OnUpdate(const wxTimerEvent& event)
{
	if (m_thread_state != ThreadRunning)
	{
		Close();
		return;
	}

	const auto processedSize = (sint32)(m_processed_size / 1000);
	if (m_processBar->GetValue() != processedSize)
		m_processBar->SetValue(processedSize);
}

void GameUpdateWindow::OnCancelButton(const wxCommandEvent& event)
{
	Close();
}
