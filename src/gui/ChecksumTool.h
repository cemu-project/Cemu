#pragma once
#include <wx/dialog.h>
#include "gui/components/wxTitleManagerList.h"
#include "Cafe/TitleList/TitleInfo.h"
#include <rapidjson/document.h>

class wxSetGaugeValue;

class ChecksumTool : public wxDialog
{
public:
	ChecksumTool(wxWindow* parent, wxTitleManagerList::TitleEntry& entry);
	~ChecksumTool();
	
private:
	std::future<void> m_online_ready;
	void LoadOnlineData() const;
	void VerifyJsonEntry(const rapidjson::Document& doc);

	void OnSetGaugevalue(wxSetGaugeValue& event);
	void OnExportChecksums(wxCommandEvent& event);
	void OnVerifyOnline(wxCommandEvent& event);
	void OnVerifyLocal(wxCommandEvent& event);
	
	void DoWork();
	std::atomic_bool m_running = true;
	std::thread m_worker;
	
	class wxGauge* m_progress;
	class wxStaticText* m_status;
	class wxButton *m_verify_online, *m_verify_local, *m_export_button;
	int m_enable_verify_button = 0;

	TitleInfo m_info;
	wxTitleManagerList::TitleEntry m_entry;
	wxColour m_default_color;
	
	struct JsonEntry
	{
		uint64 title_id;
		uint32 version;
		CafeConsoleRegion region;

		std::string wud_hash;
		std::map<std::string, std::string> file_hashes;
	};
	JsonEntry m_json_entry;
	inline static std::mutex s_mutex{};
	inline static std::vector<JsonEntry> s_entries{};

};
