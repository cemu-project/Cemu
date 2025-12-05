#pragma once

#include "config/CemuConfig.h"
#include "Cafe/TitleList/TitleId.h"

#include <mutex>

#include <wx/listctrl.h>
#include <wx/timer.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <Cafe/TitleList/GameInfo.h>

#include "wxHelper.h"
#include "util/helpers/Semaphore.h"

class wxTitleIdEvent : public wxCommandEvent
{
public:
	wxTitleIdEvent(int type, uint64 title_id)
		: wxCommandEvent(type), m_title_id(title_id) {}

	[[nodiscard]] uint64 GetTitleId() const { return m_title_id; }

private:
	uint64 m_title_id;
};

wxDECLARE_EVENT(wxEVT_OPEN_SETTINGS, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_OPEN_GRAPHIC_PACK, wxTitleIdEvent);
wxDECLARE_EVENT(wxEVT_GAMELIST_BEGIN_UPDATE, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_GAMELIST_END_UPDATE, wxCommandEvent);

class wxGameList : public wxListView
{
	friend class MainWindow;
public:
	enum class Style
	{
		kList,
		kIcons,
		kSmallIcons
	};

	wxGameList(wxWindow* parent, wxWindowID id = wxID_ANY);
	~wxGameList();

	void SetStyle(Style style, bool save = true);

	void LoadConfig();
	void SaveConfig(bool flush = false);
	bool IsVisible(long item) const; // only available in wxwidgets 3.1.3

	void ReloadGameEntries();
	void DeleteCachedStrings();

    void CreateShortcut(GameInfo2& gameInfo);

	long FindListItemByTitleId(uint64 title_id) const;
	void OnClose(wxCloseEvent& event);

private:
	std::atomic_bool m_exit = false;
	Style m_style;
	long GetStyleFlags(Style style) const;

	const wxColour kUpdateColor{ wxSystemSettings::SelectLightDark(wxColour(195, 57, 57), wxColour(84, 29, 29)) };
	const wxColour kFavoriteColor{ wxSystemSettings::SelectLightDark(wxColour(253, 246, 211), wxColour(82, 84, 48)) };
	const wxColour kPrimaryColor = GetBackgroundColour();
	const wxColour kAlternateColor = wxHelper::CalculateAccentColour(kPrimaryColor);
	void UpdateItemColors(sint32 startIndex = 0);

	enum ItemColumns : int
	{
		ColumnHiddenName = 0,
		ColumnIcon,
		ColumnName,
		ColumnVersion,
		ColumnDLC,
		ColumnGameTime,
		ColumnGameStarted,
		ColumnRegion,
        ColumnTitleID,
        //ColumnFavorite,
		ColumnCounts,
	};

	void SortEntries(int column = -1);
	struct SortData
	{
		wxGameList* thisptr;
		ItemColumns column;
		int dir;
	};

	int FindInsertPosition(TitleId titleId);
	std::weak_ordering SortComparator(uint64 titleId1, uint64 titleId2, SortData* sortData);
	static int SortFunction(wxIntPtr item1, wxIntPtr item2, wxIntPtr sortData);

	wxTimer* m_tooltip_timer;
	wxToolTip* m_tool_tip;
	wxPanel* m_tooltip_window;
	wxPoint m_mouse_position;

	std::mutex m_async_worker_mutex;
	std::thread m_async_worker_thread;
	CounterSemaphore m_async_task_count;
	std::atomic_bool m_async_worker_active{false};

	std::vector<TitleId> m_icon_load_queue;

	uint64 m_callbackIdTitleList;

	std::string GetNameByTitleId(uint64 titleId);

	void HandleTitleListCallback(struct CafeTitleListCallbackEvent* evt);

	void RemoveCache(const std::vector<fs::path>& cachePath, const std::string& titleName);

	void AsyncWorkerThread();
	void RequestLoadIconAsync(TitleId titleId);
	bool QueryIconForTitle(TitleId titleId, int& icon, int& iconSmall);

	inline static constexpr int kListIconWidth = 64;
	inline static constexpr int kIconWidth = 128;
	wxImageList m_image_list_data = wxImageList(kIconWidth, kIconWidth, false, 1);
	wxImageList m_image_list_small_data = wxImageList(kListIconWidth, kListIconWidth, false, 1);

	std::mutex m_icon_cache_mtx;
	std::set<TitleId> m_icon_loaded;
	std::map<TitleId, std::pair<int, int>> m_icon_cache; // pair contains icon and small icon

	std::map<TitleId, std::string> m_name_cache;

	// list mode
	void CreateListColumns();

	void OnColumnClick(wxListEvent& event);
	void OnColumnRightClick(wxListEvent& event);
	void ApplyGameListColumnWidths();
	void OnColumnBeginResize(wxListEvent& event);
	void OnColumnResize(wxListEvent& event);
	void OnColumnDrag(wxListEvent& event);
	
	// generic events
	void OnKeyDown(wxListEvent& event);
	void OnContextMenu(wxContextMenuEvent& event);
	void OnContextMenuSelected(wxCommandEvent& event);
	void OnGameEntryUpdatedByTitleId(wxTitleIdEvent& event);
	void OnItemActivated(wxListEvent& event);
	void OnTimer(wxTimerEvent& event);
	void OnMouseMove(wxMouseEvent& event);
	void OnLeaveWindow(wxMouseEvent& event);

	void OnGameListSize(wxSizeEvent& event);
	void AdjustLastColumnWidth();
	int GetColumnDefaultWidth(int column);

	static inline std::once_flag s_launch_file_once;
};

