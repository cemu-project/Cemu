#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gauge.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/textctrl.h>

struct curlDownloadFileState_t;

class DownloadCustomGraphicPackWindow : public wxDialog
{
public:
    DownloadCustomGraphicPackWindow(wxWindow* parent);
    ~DownloadCustomGraphicPackWindow() override;
    
    int ShowModal() override;
    void OnClose(wxCloseEvent& event);
    void OnUpdate(const wxTimerEvent& event);
    void OnCancelButton(const wxCommandEvent& event);
    void OnDownloadButton(const wxCommandEvent& event);

public:
    struct curlDownloadFileState_t
    {
    	std::vector<uint8> fileData;
    	std::atomic<double> progress = 0.0;
        std::atomic<bool> isCanceled = false;
    };
    
private:
    void UpdateThread();
	
	enum DownloadStage_t
	{
		StageDownloading,
		StageExtracting,
		StageVerifying,
		StageDone,
		
		// Error stages
		StageErrConnectFailed,
		StageErrSourceFailed,
		StageErrInvalidPack,
		StageErrOpenFailed,
		StageErrConflict,
	};
    
	std::unique_ptr<curlDownloadFileState_t> m_downloadState;
	std::atomic<DownloadStage_t> m_stage;
	DownloadStage_t m_currentStage;
	std::atomic<double> m_extractionProgress;
    
    wxTimer* m_timer;
    wxTextCtrl* m_urlField;
    wxGauge* m_processBar;
    wxStaticText* m_statusText;
    wxButton* m_downloadButton;
    std::thread m_thread;
};
