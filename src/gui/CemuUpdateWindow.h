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

	static std::future<bool> IsUpdateAvailable();

private:
	wxStaticText* m_text;
	wxGauge* m_gauge;
	wxButton* m_cancel_button, *m_update_button;
	wxHyperlinkCtrl* m_changelog;

	void OnUpdateButton(const wxCommandEvent& event);
	void OnCancelButton(const wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);
	void OnResult(wxCommandEvent& event);
	void OnGaugeUpdate(wxCommandEvent& event);

	static size_t WriteStringCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
	static bool GetServerVersion(uint64& version, std::string& filename, std::string& changelog_filename);
	static bool CheckVersion();
	static bool IsUpdateAvailable(uint64 latest_version);

	static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
	bool DownloadCemuZip(const std::string& url, const fs::path& filename);
	bool ExtractUpdate(const fs::path& zipname, const fs::path& targetpath);

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

	uint64 m_version = 0;
	std::string m_filename, m_changelog_filename;
	int m_gauge_max_value = 0;

	std::thread m_thread;
	fs::path m_restart_file;
	bool m_restart_required = false;
};
