#pragma once

#include <wx/dialog.h>
#include <wx/stattext.h>
#include <wx/gauge.h>
#include <wx/timer.h>
#include <wx/hyperlink.h>
#include <wx/checkbox.h>

#include <curl/system.h>

class CemuUpdateWindow : public wxDialog
{
public:
	CemuUpdateWindow(wxWindow* parent);
	~CemuUpdateWindow();

	static std::future<bool> IsUpdateAvailableAsync();

private:
	wxStaticText* m_text;
	wxGauge* m_gauge;
	wxButton* m_cancelButton, *m_updateButton;
	wxHyperlinkCtrl* m_changelog;

	void OnUpdateButton(const wxCommandEvent& event);
	void OnCancelButton(const wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);
	void OnResult(wxCommandEvent& event);
	void OnGaugeUpdate(wxCommandEvent& event);

	static size_t WriteStringCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
	static bool QueryUpdateInfo(std::string& downloadUrlOut, std::string& changelogUrlOut);
	static bool CheckVersion();

	static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
	bool DownloadCemuZip(const std::string& url, const fs::path& filename);
	bool ExtractUpdate(const fs::path& zipname, const fs::path& targetpath, std::string& cemuFolderName);

	enum class WorkerOrder
	{
		Idle,
		Exit,
		CheckVersion,
		UpdateVersion,
	};
	enum class Result
	{
		NoUpdateAvailable,
		UpdateAvailable,
		UpdateDownloaded,
		UpdateDownloadError,
		ExtractSuccess,
		ExtractError,
		Success,
		Error
	};
	std::mutex m_mutex;
	std::condition_variable m_condition;
	WorkerOrder m_order = WorkerOrder::CheckVersion;
	void WorkerThread();

	std::string m_downloadUrl, m_changelogUrl;
	int m_gaugeMaxValue = 0;

	std::thread m_thread;
	fs::path m_restartFile;
	bool m_restartRequired = false;
};
