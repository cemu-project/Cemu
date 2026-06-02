#include "wxgui/DownloadCustomGraphicPackWindow.h"
#include "Cafe/CafeSystem.h"
#include "config/ActiveSettings.h"

#include <cstring>
#include <filesystem>
#include <string>

#include <wx/event.h>
#include <wx/gauge.h>
#include <wx/button.h>
#include <wx/gdicmn.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/timer.h>

#include <curl/curl.h>
#include <zip.h>

static size_t curlDownloadFile_writeData(void *ptr, size_t size, size_t nmemb, DownloadCustomGraphicPackWindow::curlDownloadFileState_t* downloadState)
{
	const size_t writeSize = size * nmemb;
	const size_t currentSize = downloadState->fileData.size();
	const size_t newSize = currentSize + writeSize;
	auto* bytePtr = static_cast<const uint8*>(ptr);
	downloadState->fileData.insert(downloadState->fileData.end(), bytePtr, bytePtr + writeSize);
	return writeSize;
}

static int progress_callback(DownloadCustomGraphicPackWindow::curlDownloadFileState_t* downloadState, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    if (downloadState->isCanceled)
        return 1;
    
	if (dltotal > 1.0)
		downloadState->progress = dlnow / dltotal;
	else
		downloadState->progress = 0.0;
	return 0;
}

static bool curlDownloadFile(const char *url, DownloadCustomGraphicPackWindow::curlDownloadFileState_t* downloadState)
{
	CURL* curl = curl_easy_init();
	if (curl == nullptr)
		return false;
	
	downloadState->progress = 0.0;
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlDownloadFile_writeData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, downloadState);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, downloadState);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 15L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, BUILD_VERSION_WITH_NAME_STRING);
	downloadState->fileData.resize(0);
	const CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	return res == CURLE_OK;
}

DownloadCustomGraphicPackWindow::DownloadCustomGraphicPackWindow(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("Download Graphic Pack from URL"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxMINIMIZE_BOX | wxSYSTEM_MENU | wxTAB_TRAVERSAL | wxCLOSE_BOX),
    m_stage(StageDone), m_currentStage(StageDone)
{
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    
    m_urlField = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
    m_urlField->SetHint(_("Enter download URL..."));
    sizer->Add(m_urlField, 0, wxALL | wxEXPAND, 5);
    
    m_processBar = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(500, 20), wxGA_HORIZONTAL);
    m_processBar->SetValue(0);
    m_processBar->SetRange(100);
    sizer->Add(m_processBar, 0, wxALL | wxEXPAND, 5);
    
    auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    
    m_statusText = new wxStaticText(this, wxID_ANY, _("Ready..."));
    buttonSizer->Add(m_statusText, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    
    buttonSizer->AddStretchSpacer(1);
    
    auto* m_closeButton = new wxButton(this, wxID_ANY, _("Close"));
    m_closeButton->Bind(wxEVT_BUTTON, &DownloadCustomGraphicPackWindow::OnCancelButton, this);
    buttonSizer->Add(m_closeButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    
    m_downloadButton = new wxButton(this, wxID_ANY, _("Download"));
    m_downloadButton->Bind(wxEVT_BUTTON, &DownloadCustomGraphicPackWindow::OnDownloadButton, this);
    buttonSizer->Add(m_downloadButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    
    sizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 5);
    
    this->SetSizer(sizer);
    this->Centre(wxBOTH);
    
    wxWindowBase::Layout();
    wxWindowBase::Fit();
    
    m_timer = new wxTimer(this);
    this->Bind(wxEVT_TIMER, &DownloadCustomGraphicPackWindow::OnUpdate, this);
    this->Bind(wxEVT_CLOSE_WINDOW, &DownloadCustomGraphicPackWindow::OnClose, this);
    m_timer->Start(100);
    
    m_downloadState = std::make_unique<curlDownloadFileState_t>();
}

DownloadCustomGraphicPackWindow::~DownloadCustomGraphicPackWindow()
{
    if (m_downloadState)
        m_downloadState->isCanceled = true;
    
    m_timer->Stop();
    if (m_thread.joinable())
        m_thread.join();
}

int DownloadCustomGraphicPackWindow::ShowModal()
{
	wxDialog::ShowModal();
	return wxID_OK;
}

void DownloadCustomGraphicPackWindow::OnClose(wxCloseEvent& event)
{
    if (m_downloadState)
    {
        m_downloadState->isCanceled = true; 
    }
    
    m_timer->Stop();
    if (m_thread.joinable())
        m_thread.join();
    
    event.Skip();
}

void DownloadCustomGraphicPackWindow::OnUpdate(const wxTimerEvent& event)
{
    if (m_currentStage >= StageDone)
    {
        m_downloadButton->Enable();
        m_urlField->Enable();
    }
    else
    {
        m_downloadButton->Disable();
        m_urlField->Disable();
    }
    
    if (m_currentStage == StageDownloading)
    {
        const sint32 processedSize = (sint32)(m_downloadState->progress * 100.0f);
		if (m_processBar->GetValue() != processedSize)
			m_processBar->SetValue(processedSize);
    }
   	else if (m_currentStage == StageExtracting)
	{
		const sint32 processedSize = (sint32)(m_extractionProgress * 100.0f);
		if (m_processBar->GetValue() != processedSize)
			m_processBar->SetValue(processedSize);
	}
    
    if (m_currentStage != m_stage)
    {
        wxString status = "...";
        const wxColour* colour = wxWHITE;
        
        switch (m_stage)
        {
        case StageDownloading:
            status = "Downloading...";
            colour = wxWHITE;
            break;
        case StageVerifying:
            status = "Verifying...";
            colour = wxWHITE;
            break;
        case StageExtracting:
            status = "Extracting...";
            colour = wxWHITE;
            break;
        case StageDone:
            status = "Done!";
            colour = wxGREEN;
            m_processBar->SetValue(100.0);
            break;
        case StageErrConnectFailed:
            if (m_urlField->GetValue().empty())
            {
                status = "Please enter a valid URL.";
                colour = wxWHITE;
            }
            else
            {
                status = "ERROR: Connection failed.";
                colour = wxRED;
            }
            m_processBar->SetValue(0.0);
            break;
        case StageErrInvalidPack:
            status = "ERROR: Invalid pack.";
            colour = wxRED;
            m_processBar->SetValue(0.0);
            break;
        case StageErrSourceFailed:
            status = "ERROR: Failed to create ZIP source.";
            colour = wxRED;
            m_processBar->SetValue(0.0);
            break;
        case StageErrOpenFailed:
            status = "ERROR: Failed to open downloaded ZIP.";
            colour = wxRED;
            m_processBar->SetValue(0.0);
            break;
        case StageErrConflict:
            status = "ERROR: File conflict. Pack already installed?";
            colour = wxRED;
            m_processBar->SetValue(0.0);
            break;
        }
        
        m_currentStage = m_stage;
        m_statusText->SetLabel(status);
        m_statusText->SetForegroundColour(*colour);
    }
}

void DownloadCustomGraphicPackWindow::OnCancelButton(const wxCommandEvent& event)
{
    Close();
}

void DownloadCustomGraphicPackWindow::OnDownloadButton(const wxCommandEvent& event)
{
    m_downloadButton->Disable();
    m_urlField->Disable();
    
    if (m_thread.joinable())
        m_thread.join();
    
    m_thread = std::thread(&DownloadCustomGraphicPackWindow::UpdateThread, this);
}

void DownloadCustomGraphicPackWindow::UpdateThread()
{
    m_stage = StageDownloading;
    if (curlDownloadFile(m_urlField->GetValue(), m_downloadState.get()) == false)
    {
        m_stage = StageErrConnectFailed;
		return;
    }
    
    m_extractionProgress = 0.0;
    m_stage = StageExtracting;
    
    zip_source_t *src;
    zip_t *za;
    zip_error_t error;
    
    // init zip source
	zip_error_init(&error);
	if ((src = zip_source_buffer_create(m_downloadState->fileData.data(), m_downloadState->fileData.size(), 0, &error)) == NULL)
	{
		zip_error_fini(&error);
		m_stage = StageErrSourceFailed;
		return;
	}

	// open zip from source
	if ((za = zip_open_from_source(src, 0, &error)) == NULL)
	{
		zip_source_free(src);
		zip_error_fini(&error);
		m_stage = StageErrOpenFailed;
		return;
	}
    
    auto path = ActiveSettings::GetUserDataPath("graphicPacks/customGraphicPacks");
    fs::create_directories(path);
    
    // check if zip root directly contains a rules.txt
    zip_stat_t zs;
    zip_stat_init(&zs);
    bool hasRootRulesTxt = (zip_stat(za, "rules.txt", 0, &zs) == 0);

    fs::path extractionRoot = path;

    if (hasRootRulesTxt)
    {
        // make a folder and extract into that
        wxString urlStr = m_urlField->GetValue();
        std::string folderName = "NewCustomPack"; // fallback
        
        int lastSlash = urlStr.Find('/', true);
        wxString fileNameBase = (lastSlash != wxNOT_FOUND) ? urlStr.Mid(lastSlash + 1) : urlStr;
        
        int lastDot = fileNameBase.Find('.', true);
        if (lastDot != wxNOT_FOUND)
        {
            fileNameBase = fileNameBase.Left(lastDot);
        }
        
        fileNameBase.Trim(true).Trim(false);
        if (!fileNameBase.IsEmpty())
        {
            folderName = fileNameBase.ToStdString();
        }
        
        extractionRoot = path / folderName;
        fs::create_directories(extractionRoot);
    }
    else
    {
        // not a gfxpack
        m_stage = StageErrInvalidPack;
        zip_close(za);
        return;
    }

    // extract
    bool err = false;
    zip_int64_t numEntries = zip_get_num_entries(za, 0);
    for (zip_int64_t i = 0; i < numEntries; i++) 
    {
        m_extractionProgress = (double)i / (double)numEntries;
        
        if (m_downloadState->isCanceled)
        {
            err = true;
        }
        
        if (err)
        {
            break;
        }
        
        zip_stat_t sb = { 0 };
        zip_stat_init(&sb);
        if (zip_stat_index(za, i, 0, &sb) != 0)
        {
            assert_dbg();
            continue;
        }
        
        std::string fileName = sb.name;
        if (std::strstr(sb.name, "../") != nullptr ||
            std::strstr(sb.name, "..\\") != nullptr ||
            (!fileName.empty() && (fileName[0] == '/' || fileName[0] == '\\')))
        {
            // bad path, CommunityGraphicPacks silently continues
            // but this is a low-trust environment, so we halt
            m_stage = StageErrInvalidPack;
            err = true;
            break;
        }
        
        fs::path targetDest = extractionRoot / fileName;

        if (!fileName.empty() && fileName.back() == '/') 
        {
            fs::create_directories(targetDest);
            continue;
        }
        
        if (fs::exists(targetDest) && fs::is_regular_file(targetDest))
        {
            m_stage = StageErrConflict;
            err = true;
            break;
        }

        fs::create_directories(targetDest.parent_path());

        zip_file_t* zf = zip_fopen_index(za, i, 0);
        if (!zf)
        {
            continue;
        }
        
        if (sb.size > 1000000000) // 1GB suspicious limit per file
        {
            m_stage = StageErrInvalidPack;
            err = true;
            break;
        }

        std::vector<uint8> fileBuffer(sb.size);
        
        if (zip_fread(zf, fileBuffer.data(), sb.size) == sb.size)
        {
            std::unique_ptr<FileStream> fs(FileStream::createFile2(targetDest));
            if (fs)
            {
                fs->writeData(fileBuffer.data(), sb.size);
            }
        }
        else
        {
            err = true;
            m_stage = StageErrInvalidPack;
        }
        
        zip_fclose(zf);
    }

    zip_discard(za);
    zip_error_fini(&error);
    
    if (!err)
    {
        m_stage = StageDone;
    }
}
