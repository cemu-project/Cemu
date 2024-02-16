#pragma once

#include <thread>
#include <atomic>

#include <wx/frame.h>
#include <wx/button.h>

#include "Cemu/Tools/DownloadManager/DownloadManager.h"

class wxCheckBox;
class wxStaticText;
class wxListEvent;
class wxSetStatusBarTextEvent;
class wxTitleManagerList;
class wxDownloadManagerList;
class wxTextCtrl;
class wxStatusBar;
class wxImageList;
class wxBitmapButton;
class wxPanel;
class wxChoice;
class wxNotebook;

enum class TitleManagerPage
{
	TitleManager = 0,
	DownloadManager = 1
};

enum class DLMGR_STATUS_CODE;

class TitleManager : public wxFrame
{
public:
	TitleManager(wxWindow* parent, TitleManagerPage default_page = TitleManagerPage::TitleManager);
	~TitleManager();

	void SetFocusAndTab(TitleManagerPage page);

	void SetDownloadStatusText(const wxString& text);
	
private:
	wxPanel* CreateTitleManagerPage();
	wxPanel* CreateDownloadManagerPage();
	
	// title manager
	void OnTitleFound(wxCommandEvent& event);
	void OnTitleSearchComplete(wxCommandEvent& event);
	void OnSetStatusBarText(wxSetStatusBarTextEvent& event);
	void OnFilterChanged(wxCommandEvent& event);
	void OnRefreshButton(wxCommandEvent& event);
	void OnInstallTitle(wxCommandEvent& event);
	void OnTitleSelected(wxListEvent& event);
	void OnSaveOpenDirectory(wxCommandEvent& event);
	void OnSaveDelete(wxCommandEvent& event);
	void OnSaveTransfer(wxCommandEvent& event);
	void OnSaveAccountSelected(wxCommandEvent& event);
	void OnSaveExport(wxCommandEvent& event);
	void OnSaveImport(wxCommandEvent& event);

	void HandleTitleListCallback(struct CafeTitleListCallbackEvent* evt);

	wxNotebook* m_notebook;
	
	uint32 m_callbackId;

	// title manager
	wxTextCtrl* m_filter;
	wxTitleManagerList* m_title_list;
	wxStatusBar* m_status_bar;
	wxBitmapButton* m_refresh_button;
	wxPanel* m_save_panel;
	wxChoice* m_save_account_list;
	wxButton* m_save_import;

	bool m_isScanning{ true }; // set when CafeTitleList is scanning

	std::atomic_bool m_running = true;

	// download manager
	void InitiateConnect();
	void OnConnect(wxCommandEvent& event);
	void OnSetStatusText(wxCommandEvent& event);
	void OnDownloadableTitleUpdate(wxCommandEvent& event);
	void OnDisconnect(wxCommandEvent& event);

	void OnDlFilterCheckbox(wxCommandEvent& event);

	void SetConnected(bool state);

	static void Callback_ConnectStatusUpdate(std::string statusText, DLMGR_STATUS_CODE statusCode);
	static void Callback_AddDownloadableTitle(const struct DlMgrTitleReport& titleInfo);
	static void Callback_RemoveDownloadableTitle(uint64 titleId, uint16 version);

	wxChoice* m_account;
	wxButton* m_connect;
	wxStaticText* m_status_text;
	wxCheckBox *m_show_titles, *m_show_updates, *m_show_installed;
	wxDownloadManagerList* m_download_list;
	bool m_connectRequested{false}; // connect was clicked before m_foundTitles was available
};
