#pragma once

#include <atomic>
#include <thread>
#include <string>
#include <memory>

#include <wx/timer.h>
#include <wx/gauge.h>


class DownloadGraphicPacksWindow : public wxDialog
{
public:
	DownloadGraphicPacksWindow(wxWindow* parent);
	~DownloadGraphicPacksWindow();

	const std::string& GetException() const;

	int ShowModal() override;
	void OnClose(wxCloseEvent& event);

	void OnUpdate(const wxTimerEvent& event);
	void OnCancelButton(const wxCommandEvent& event);

private:
	void UpdateThread();

	enum ThreadState_t
	{
		ThreadRunning,
		ThreadCanceled,
		ThreadError,
		ThreadFinished,
	};

	enum DownloadStage_t
	{
		StageCheckVersion,
		StageDownloading,
		StageExtracting
	};

	std::atomic<ThreadState_t> m_threadState;
	std::atomic<DownloadStage_t> m_stage;
	std::atomic<double> m_extractionProgress;
	std::string m_threadException;
	std::thread m_thread;

	DownloadStage_t m_currentStage;
	wxGauge* m_processBar;
	wxTimer* m_timer;

	struct curlDownloadFileState_t;
	std::unique_ptr<curlDownloadFileState_t> m_downloadState;

	static size_t curlDownloadFile_writeData(void* ptr, size_t size, size_t nmemb, curlDownloadFileState_t* downloadState);
	static int progress_callback(curlDownloadFileState_t* downloadState, double dltotal, double dlnow, double ultotal, double ulnow);
	static bool curlDownloadFile(const char* url, curlDownloadFileState_t* downloadState);
};
