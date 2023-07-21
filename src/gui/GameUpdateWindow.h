#pragma once

#include "Cafe/TitleList/GameInfo.h"

#include <wx/dialog.h>
#include <wx/timer.h>
#include <wx/gauge.h>

#include <atomic>
#include <string>
#include <array>
#include <memory>

// thrown if users doesn't wish to reinstall update/dlc
class AbortException : public std::exception {};

class GameUpdateWindow : public wxDialog
{
public:

	GameUpdateWindow(wxWindow& parent, const fs::path& metaPath);
	~GameUpdateWindow();

	uint64 GetTitleId() const { return m_title_info.GetAppTitleId(); }
	bool HasException() const { return !m_thread_exception.empty(); }
	//bool IsDLC() const { return m_game_info->IsDLC(); }
	//bool IsUpdate() const { return m_game_info->IsUpdate(); }
	const std::string& GetExceptionMessage() const { return m_thread_exception; }
	const std::string GetGameName() const { return m_title_info.GetMetaTitleName(); }
	uint32 GetTargetVersion() const { return m_title_info.GetAppTitleVersion(); }
	fs::path GetTargetPath() const { return fs::path(m_target_path); }

	int ShowModal() override;
	void OnClose(wxCloseEvent& event);

	void OnUpdate(const wxTimerEvent& event);
	void OnCancelButton(const wxCommandEvent& event);

	//uint64 GetUpdateTitleId() const { return m_title_info->GetUpdateTitleId(); }
	//uint64 GetDLCTitleId() const { return m_game_info->GetDLCTitleId(); }
	
private:
	//std::unique_ptr<GameInfoDEPRECATED> m_game_info;
	TitleInfo m_title_info;
	enum ThreadState_t
	{
		ThreadRunning,
		ThreadCanceled,
		ThreadFinished,
	};

	uint64_t m_required_size;
	fs::path m_target_path;
	std::array<fs::path, 3> m_source_paths;
	bool ParseUpdate(const fs::path& metaPath);

	std::atomic<uint64> m_processed_size = 0;
	std::atomic<ThreadState_t> m_thread_state;
	std::string m_thread_exception;
	std::thread m_thread;
	void ThreadWork();

	fs::path m_backup_folder; // for prev update data

	wxGauge* m_processBar;
	wxTimer* m_timer;
};
