#include "gui/components/wxGameList.h"

#include "gui/helpers/wxCustomData.h"
#include "util/helpers/helpers.h"
#include "gui/GameProfileWindow.h"

#include <numeric>

#include <wx/wupdlock.h>
#include <wx/menu.h>
#include <wx/mstream.h>
#include <wx/imaglist.h>
#include <wx/textdlg.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/wfstream.h>
#include <wx/imagpng.h>
#include <wx/string.h>
#include <wx/utils.h>

#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>

#include "config/ActiveSettings.h"
#include "config/LaunchSettings.h"
#include "Cafe/TitleList/GameInfo.h"
#include "Cafe/TitleList/TitleList.h"

#include "gui/CemuApp.h"
#include "gui/helpers/wxHelpers.h"
#include "gui/MainWindow.h"

#include "../wxHelper.h"

#include "Cafe/IOSU/PDM/iosu_pdm.h" // for last played and play time

#if BOOST_OS_WINDOWS
// for shortcut creation
#include <windows.h>
#include <winnls.h>
#include <shobjidl.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <shlobj.h>
#endif

// public events
wxDEFINE_EVENT(wxEVT_OPEN_SETTINGS, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_GAMELIST_BEGIN_UPDATE, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_GAMELIST_END_UPDATE, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_OPEN_GRAPHIC_PACK, wxTitleIdEvent);

// internal events
wxDEFINE_EVENT(wxEVT_GAME_ENTRY_ADDED_OR_REMOVED, wxTitleIdEvent);

constexpr uint64 kDefaultEntryData = 0x1337;

void _stripPathFilename(fs::path& path)
{
	if (path.has_extension())
		path = path.parent_path();
}

std::list<fs::path> _getCachesPaths(const TitleId& titleId)
{
	std::list<fs::path> cachePaths{
		ActiveSettings::GetCachePath(L"shaderCache/driver/vk/{:016x}.bin", titleId),
		ActiveSettings::GetCachePath(L"shaderCache/precompiled/{:016x}_spirv.bin", titleId),
		ActiveSettings::GetCachePath(L"shaderCache/precompiled/{:016x}_gl.bin", titleId),
		ActiveSettings::GetCachePath(L"shaderCache/transferable/{:016x}_shaders.bin", titleId),
		ActiveSettings::GetCachePath(L"shaderCache/transferable/{:016x}_vkpipeline.bin", titleId)};

	cachePaths.remove_if(
		[](const fs::path& cachePath)
		{
			std::error_code ec;
			return !fs::exists(cachePath, ec);
		});

	return cachePaths;
}

wxGameList::wxGameList(wxWindow* parent, wxWindowID id)
	: wxListCtrl(parent, id, wxDefaultPosition, wxDefaultSize, GetStyleFlags(Style::kList)), m_style(Style::kList)
{
	const auto& config = GetConfig();

	InsertColumn(ColumnHiddenName, "", wxLIST_FORMAT_LEFT, 0);
	InsertColumn(ColumnIcon, "", wxLIST_FORMAT_LEFT, kListIconWidth);
	InsertColumn(ColumnName, _("Game"), wxLIST_FORMAT_LEFT, config.column_width.name);
	InsertColumn(ColumnVersion, _("Version"), wxLIST_FORMAT_RIGHT, config.column_width.version);
	InsertColumn(ColumnDLC, _("DLC"), wxLIST_FORMAT_RIGHT, config.column_width.dlc);
	InsertColumn(ColumnGameTime, _("You've played"), wxLIST_FORMAT_LEFT, config.column_width.game_time);
	InsertColumn(ColumnGameStarted, _("Last played"), wxLIST_FORMAT_LEFT, config.column_width.game_started);
	InsertColumn(ColumnRegion, _("Region"), wxLIST_FORMAT_LEFT, config.column_width.region);
    InsertColumn(ColumnTitleID, _("Title ID"), wxLIST_FORMAT_LEFT, config.column_width.title_id);

	const char transparent_bitmap[kIconWidth * kIconWidth * 4] = {0};
	wxBitmap blank(transparent_bitmap, kIconWidth, kIconWidth);
	blank.UseAlpha(true);

	m_image_list = new wxImageList(kIconWidth, kIconWidth);
	m_image_list->Add(blank);
	wxListCtrl::SetImageList(m_image_list, wxIMAGE_LIST_NORMAL);

	m_image_list_small = new wxImageList(kListIconWidth, kListIconWidth);
	wxBitmap::Rescale(blank, {kListIconWidth, kListIconWidth});
	m_image_list_small->Add(blank);
	wxListCtrl::SetImageList(m_image_list_small, wxIMAGE_LIST_SMALL);

	m_tooltip_window = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
	auto* tooltip_sizer = new wxBoxSizer(wxVERTICAL);
	tooltip_sizer->Add(new wxStaticText(m_tooltip_window, wxID_ANY, _("This game entry seems to be either an update or the base game was merged with update data\nBroken game dumps cause various problems during emulation and may even stop working at all in future Cemu versions\nPlease make sure the base game is intact and install updates only with the File->Install Update/DLC option")), 0, wxALL, 5);
	m_tooltip_window->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
	m_tooltip_window->SetSizerAndFit(tooltip_sizer);
	m_tooltip_window->Hide();

	m_tooltip_timer = new wxTimer(this);

	Bind(wxEVT_CLOSE_WINDOW, &wxGameList::OnClose, this);
	Bind(wxEVT_MOTION, &wxGameList::OnMouseMove, this);
	Bind(wxEVT_LIST_KEY_DOWN, &wxGameList::OnKeyDown, this);
	Bind(wxEVT_CONTEXT_MENU, &wxGameList::OnContextMenu, this);
	Bind(wxEVT_LIST_ITEM_ACTIVATED, &wxGameList::OnItemActivated, this);
	Bind(wxEVT_GAME_ENTRY_ADDED_OR_REMOVED, &wxGameList::OnGameEntryUpdatedByTitleId, this);
	Bind(wxEVT_TIMER, &wxGameList::OnTimer, this);
	Bind(wxEVT_LEAVE_WINDOW, &wxGameList::OnLeaveWindow, this);

	Bind(wxEVT_LIST_COL_CLICK, &wxGameList::OnColumnClick, this);
	Bind(wxEVT_LIST_COL_BEGIN_DRAG, &wxGameList::OnColumnBeginResize, this);
	Bind(wxEVT_LIST_COL_END_DRAG, &wxGameList::OnColumnResize, this);
	Bind(wxEVT_LIST_COL_RIGHT_CLICK, &wxGameList::OnColumnRightClick, this);
	Bind(wxEVT_SIZE, &wxGameList::OnGameListSize, this);

	m_callbackIdTitleList = CafeTitleList::RegisterCallback([](CafeTitleListCallbackEvent* evt, void* ctx) { ((wxGameList*)ctx)->HandleTitleListCallback(evt); }, this);

	// start async worker (for icon loading)
	m_async_worker_active = true;
	m_async_worker_thread = std::thread(&wxGameList::AsyncWorkerThread, this);
}

wxGameList::~wxGameList()
{
	CafeTitleList::UnregisterCallback(m_callbackIdTitleList);

	m_tooltip_timer->Stop();

	// shut down async worker
	m_async_worker_active.store(false);
	m_async_task_count.increment();
	m_async_worker_thread.join();

	// destroy image lists
	delete m_image_list;
	delete m_image_list_small;

	// clear image cache
	m_icon_cache_mtx.lock();
	m_icon_cache.clear();
	m_icon_cache_mtx.unlock();
}

void wxGameList::LoadConfig()
{
	const auto& config = GetConfig();
	SetStyle((Style)config.game_list_style, false);

	if (!config.game_list_column_order.empty())
	{
		wxArrayInt order;
		order.reserve(ColumnCounts);

		const auto order_string = std::string_view(config.game_list_column_order).substr(1);

		const boost::char_separator<char> sep(",");
		boost::tokenizer tokens(order_string.begin(), order_string.end(), sep);
		for(const auto& token : tokens)
		{
			order.push_back(ConvertString<int>(token, 10));
		}

#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
		if(order.GetCount() == ColumnCounts)
			SetColumnsOrder(order);
#endif
	}
}

void wxGameList::OnGameListSize(wxSizeEvent &event)
{
	event.Skip();

	// when using a sizer-based layout, do not change the size of the wxComponent in its own wxSizeEvent handler to avoid some UI issues.
	int last_col_index = 0;
	for(int i = GetColumnCount() - 1; i > 0; i--)
	{
#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
		if(GetColumnWidth(GetColumnIndexFromOrder(i)) > 0) 
		{
			last_col_index = GetColumnIndexFromOrder(i);
			break;
		}
#else
		if(GetColumnWidth(i) > 0) 
		{
			last_col_index = i;
			break;
		}
#endif
	}
	wxListEvent column_resize_event(wxEVT_LIST_COL_END_DRAG);
	column_resize_event.SetColumn(last_col_index);
	wxPostEvent(this, column_resize_event);
}

void wxGameList::AdjustLastColumnWidth()
{
	wxWindowUpdateLocker windowlock(this);
	int last_col_index = 0;
	int last_col_width = GetClientSize().GetWidth();
	for (int i = 1; i < GetColumnCount(); i++)
	{
#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
		if (GetColumnWidth(GetColumnIndexFromOrder(i)) > 0)
		{
			last_col_index = GetColumnIndexFromOrder(i);
			last_col_width -= GetColumnWidth(last_col_index);
		}
#else
		if (GetColumnWidth(i) > 0)
		{
			last_col_index = i;
			last_col_width -= GetColumnWidth(i);
		}
#endif
	}
	last_col_width += GetColumnWidth(last_col_index);
	if (last_col_width < GetColumnDefaultWidth(last_col_index)) // keep a minimum width
		last_col_width = GetColumnDefaultWidth(last_col_index);
	SetColumnWidth(last_col_index, last_col_width);
}

// todo: scale all columns using a ratio instead of hardcoding exact widths
int wxGameList::GetColumnDefaultWidth(int column)
{
	switch (column)
	{
	case ColumnIcon:
		return kListIconWidth;
	case ColumnName:
		return DefaultColumnSize::name;
	case ColumnVersion:
		return DefaultColumnSize::version;
	case ColumnDLC:
		return DefaultColumnSize::dlc;
	case ColumnGameTime:
		return DefaultColumnSize::game_time;
	case ColumnGameStarted:
		return DefaultColumnSize::game_started;
	case ColumnRegion:
		return DefaultColumnSize::region;
    case ColumnTitleID:
        return DefaultColumnSize::title_id;
	default:
		return 80;
	}
}

void wxGameList::SaveConfig(bool flush)
{
	auto& config = GetConfig();

	config.game_list_style = (int)m_style;
	#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
	config.game_list_column_order = fmt::format("{}", GetColumnsOrder());
	#endif

	if (flush)
		g_config.Save();
}

bool wxGameList::IsVisible(long item) const
{
	wxRect itemRect;
	GetItemRect(item, itemRect);
	const wxRect clientRect = GetClientRect();
	bool visible = clientRect.Intersects(itemRect);
	return visible;
}

void wxGameList::ReloadGameEntries(bool cached)
{
	wxWindowUpdateLocker windowlock(this);
	DeleteAllItems();
	// tell the game list to rescan
	CafeTitleList::Refresh();
	// resend notifications for all known titles by re-registering the callback
	CafeTitleList::UnregisterCallback(m_callbackIdTitleList);
	m_callbackIdTitleList = CafeTitleList::RegisterCallback([](CafeTitleListCallbackEvent* evt, void* ctx) { ((wxGameList*)ctx)->HandleTitleListCallback(evt); }, this);
}

long wxGameList::FindListItemByTitleId(uint64 title_id) const
{
	for (int i = 0; i < GetItemCount(); ++i)
	{
		const auto id = (uint64)GetItemData(i);
		if (id == title_id)
			return i;
	}

	return wxNOT_FOUND;
}

// get title name with cache
std::string wxGameList::GetNameByTitleId(uint64 titleId)
{
	auto it = m_name_cache.find(titleId);
	if (it != m_name_cache.end())
		return it->second;
	TitleInfo titleInfo;
	if (!CafeTitleList::GetFirstByTitleId(titleId, titleInfo))
		return "Unknown title";
	std::string name;
	if (!GetConfig().GetGameListCustomName(titleId, name))
		name = titleInfo.GetMetaTitleName();
	m_name_cache.emplace(titleId, name);
	return name;
}

void wxGameList::SetStyle(Style style, bool save)
{
	if (m_style == style)
		return;

	wxWindowUpdateLocker updatelock(this);

	m_style = style;
	SetWindowStyleFlag(GetStyleFlags(m_style));

	uint64 selected_title_id = 0;
	auto selection = GetNextItem(wxNOT_FOUND, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selection != wxNOT_FOUND)
	{
		selected_title_id = (uint64)GetItemData(selection);
		selection = wxNOT_FOUND;
	}

	switch(style)
	{
	case Style::kIcons:
		wxListCtrl::SetImageList(m_image_list, wxIMAGE_LIST_NORMAL);
		break;
	case Style::kSmallIcons:
		wxListCtrl::SetImageList(m_image_list_small, wxIMAGE_LIST_NORMAL);
		break;
	}

	ReloadGameEntries();
	SortEntries();
	UpdateItemColors();

	if(selection != wxNOT_FOUND)
	{
		SetItemState(selection, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
		EnsureVisible(selection);
	}

	if(save)
	{
		GetConfig().game_list_style = (int)m_style;
		g_config.Save();
	}

	if (style == Style::kList)
		ApplyGameListColumnWidths();
}

long wxGameList::GetStyleFlags(Style style) const
{
	switch (style)
	{
	case Style::kList:
		return (wxLC_SINGLE_SEL | wxLC_REPORT);
	case Style::kIcons:
		return (wxLC_SINGLE_SEL | wxLC_ICON);
	case Style::kSmallIcons:
		return (wxLC_SINGLE_SEL | wxLC_ICON);
	default:
		wxASSERT(false);
		return (wxLC_SINGLE_SEL | wxLC_REPORT);
	}
}

void wxGameList::UpdateItemColors(sint32 startIndex)
{
    wxWindowUpdateLocker lock(this);

	wxColour bgColourPrimary = GetBackgroundColour();
	wxColour bgColourSecondary = wxHelper::CalculateAccentColour(bgColourPrimary);

    for (int i = startIndex; i < GetItemCount(); ++i)
    {
        const auto titleId = (uint64)GetItemData(i);
		if (GetConfig().IsGameListFavorite(titleId))
		{
			SetItemBackgroundColour(i, kFavoriteColor);
			SetItemTextColour(i, 0x000000UL);
		}
		else if ((i&1) != 0)
		{
            SetItemBackgroundColour(i, bgColourPrimary);
            SetItemTextColour(i, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
		}
		else
		{
            SetItemBackgroundColour(i, bgColourSecondary);
            SetItemTextColour(i, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
		}
	}
}

static inline int order_to_int(const std::weak_ordering &wo)
{
	// no easy conversion seems to exists in C++20
	if (wo == std::weak_ordering::less)
		return -1;
	else if (wo == std::weak_ordering::greater)
		return 1;
	return 0;
}

int wxGameList::SortComparator(uint64 titleId1, uint64 titleId2, SortData* sortData)
{
	const auto isFavoriteA = GetConfig().IsGameListFavorite(titleId1);
	const auto isFavoriteB = GetConfig().IsGameListFavorite(titleId2);
	const auto& name1 = GetNameByTitleId(titleId1);
	const auto& name2 = GetNameByTitleId(titleId2);

	if(sortData->dir > 0)
		return order_to_int(std::tie(isFavoriteB, name1) <=> std::tie(isFavoriteA, name2));
	else
		return order_to_int(std::tie(isFavoriteB, name2) <=> std::tie(isFavoriteA, name1));
}

int wxGameList::SortFunction(wxIntPtr item1, wxIntPtr item2, wxIntPtr sortData)
{
	const auto sort_data = (SortData*)sortData;
	const int dir = sort_data->dir;

	return sort_data->thisptr->SortComparator((uint64)item1, (uint64)item2, sort_data);
}

void wxGameList::SortEntries(int column)
{
	if (column == -1)
		column = s_last_column;
	else
	{
		if (s_last_column == column)
		{
			s_last_column = 0;
			s_direction = -1;
		}
		else
		{
			s_last_column = column;
			s_direction = 1;
		}
	}

	switch (column)
	{
	case ColumnName:
	case ColumnGameTime:
	case ColumnGameStarted:
	case ColumnRegion:
	{
		SortData data{ this, column, s_direction };
		SortItems(SortFunction, (wxIntPtr)&data);
		break;
	}
	}
}

void wxGameList::OnKeyDown(wxListEvent& event)
{
	event.Skip();
	if (m_style != Style::kList)
		return;

	const auto keycode = std::tolower(event.m_code);
	if (keycode == WXK_LEFT)
	{
		const auto item_count = GetItemCount();
		if (item_count > 0)
		{
			auto selection = (int)GetNextItem(wxNOT_FOUND, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
			if (selection == wxNOT_FOUND)
				selection = 0;
			else
				selection = std::max(0, selection - GetCountPerPage());

			SetItemState(wxNOT_FOUND, 0, wxLIST_STATE_SELECTED);
			SetItemState(selection, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
			EnsureVisible(selection);
		}
	}
	else if (keycode == WXK_RIGHT)
	{
		const auto item_count = GetItemCount();
		if (item_count > 0)
		{
			auto selection = (int)GetNextItem(wxNOT_FOUND, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
			if (selection == wxNOT_FOUND)
				selection = 0;

			selection = std::min(item_count - 1, selection + GetCountPerPage());

			SetItemState(wxNOT_FOUND, 0, wxLIST_STATE_SELECTED);
			SetItemState(selection, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
			EnsureVisible(selection);
		}
	}
}


enum ContextMenuEntries
{
	kContextMenuRefreshGames = wxID_HIGHEST + 1,

	kContextMenuStart,
	kWikiPage,
	kContextMenuFavorite,
	kContextMenuEditName,

	kContextMenuGameFolder,
	kContextMenuSaveFolder,
	kContextMenuUpdateFolder,
	kContextMenuDLCFolder,
	kContextMenuEditGraphicPacks,
	kContextMenuEditGameProfile,

	kContextMenuRemoveCache,

	kContextMenuStyleList,
	kContextMenuStyleIcon,
	kContextMenuStyleIconSmall,
    kContextMenuCreateShortcut
};
void wxGameList::OnContextMenu(wxContextMenuEvent& event)
{
	auto& config = GetConfig();

	wxMenu menu;
	menu.Bind(wxEVT_COMMAND_MENU_SELECTED, &wxGameList::OnContextMenuSelected, this);

	const auto selection = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selection != wxNOT_FOUND)
	{
		const auto title_id = (uint64)GetItemData(selection);
		GameInfo2 gameInfo = CafeTitleList::GetGameInfo(title_id);
		if (gameInfo.IsValid())
		{
			menu.SetClientData((void*)title_id);

			menu.Append(kContextMenuStart, _("&Start"));

			bool isFavorite = GetConfig().IsGameListFavorite(title_id);
			std::error_code ec;

			menu.AppendSeparator();
			menu.AppendCheckItem(kContextMenuFavorite, _("&Favorite"))->Check(isFavorite);
			menu.Append(kContextMenuEditName, _("&Edit name"));

			menu.AppendSeparator();
			menu.Append(kWikiPage, _("&Wiki page"));
			menu.Append(kContextMenuGameFolder, _("&Game directory"));
			menu.Append(kContextMenuSaveFolder, _("&Save directory"))->Enable(fs::is_directory(gameInfo.GetSaveFolder(), ec));
			menu.Append(kContextMenuUpdateFolder, _("&Update directory"))->Enable(gameInfo.HasUpdate());
			menu.Append(kContextMenuDLCFolder, _("&DLC directory"))->Enable(gameInfo.HasAOC());

			menu.AppendSeparator();
			menu.Append(kContextMenuRemoveCache, _("&Remove shader caches"))->Enable(!_getCachesPaths(gameInfo.GetBaseTitleId()).empty());

			menu.AppendSeparator();
			menu.Append(kContextMenuEditGraphicPacks, _("&Edit graphic packs"));
			menu.Append(kContextMenuEditGameProfile, _("&Edit game profile"));

            menu.AppendSeparator();
#if BOOST_OS_LINUX || BOOST_OS_WINDOWS
            menu.Append(kContextMenuCreateShortcut, _("&Create shortcut"));
#endif
			menu.AppendSeparator();
		}
	}

	menu.Append(kContextMenuRefreshGames, _("&Refresh game list"))->Enable(!CafeTitleList::IsScanning());
	menu.AppendSeparator();
	menu.AppendRadioItem(kContextMenuStyleList, _("Style: &List"))->Check(m_style == Style::kList);
	menu.AppendRadioItem(kContextMenuStyleIcon, _("Style: &Icons"))->Check(m_style == Style::kIcons);
	menu.AppendRadioItem(kContextMenuStyleIconSmall, _("Style: &Small Icons"))->Check(m_style == Style::kSmallIcons);
	PopupMenu(&menu);
}

void wxGameList::OnContextMenuSelected(wxCommandEvent& event)
{
	const auto title_id = (uint64)((wxMenu*)event.GetEventObject())->GetClientData();
	if (title_id)
	{
		GameInfo2 gameInfo = CafeTitleList::GetGameInfo(title_id);
		if (gameInfo.IsValid())
		{
			switch (event.GetId())
			{
			case kContextMenuStart:
			{
				MainWindow::RequestLaunchGame(gameInfo.GetBase().GetPath(), wxLaunchGameEvent::INITIATED_BY::GAME_LIST);
				break;
			}
			case kContextMenuFavorite:
				GetConfig().SetGameListFavorite(title_id, !GetConfig().IsGameListFavorite(title_id));
				SortEntries();
				UpdateItemColors();
				//SaveConfig();
				break;
			case kContextMenuEditName:
			{
				std::string customName = "";
				if (!GetConfig().GetGameListCustomName(title_id, customName))
					customName.clear();
				wxTextEntryDialog dialog(this, wxEmptyString, _("Enter a custom game title"), wxHelper::FromUtf8(customName));
				if(dialog.ShowModal() == wxID_OK)
				{
					const auto custom_name = dialog.GetValue();
					GetConfig().SetGameListCustomName(title_id, custom_name.utf8_string());
					m_name_cache.clear();
					g_config.Save();
					// update list entry
					for (int i = 0; i < GetItemCount(); ++i)
					{
						const auto id = (uint64)GetItemData(i);
						if (id == title_id)
						{
							if (m_style == Style::kList)
								SetItem(i, ColumnName, wxHelper::FromUtf8(GetNameByTitleId(title_id)));
							break;
						}
					}
					SortEntries();
					UpdateItemColors();
				}
				break;
			}
			case kContextMenuGameFolder:
				{
				fs::path path(gameInfo.GetBase().GetPath());
				_stripPathFilename(path);
				wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("file:{}", _pathToUtf8(path))));
				break;
				}
			case kWikiPage:
				{
					// https://wiki.cemu.info/wiki/GameIDs
					// WUP-P-ALZP
				if (gameInfo.GetBase().ParseXmlInfo())
				{
					std::string productCode = gameInfo.GetBase().GetMetaInfo()->GetProductCode();
					const auto tokens = TokenizeView(productCode, '-');
					wxASSERT(!tokens.empty());
					const std::string company_code = gameInfo.GetBase().GetMetaInfo()->GetCompanyCode();
					wxASSERT(company_code.size() >= 2);
					wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("https://wiki.cemu.info/wiki/{}{}", *tokens.rbegin(), company_code.substr(company_code.size() - 2).c_str())));
				}
				break;
				}

			case kContextMenuSaveFolder:
			{
				wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("file:{}", _pathToUtf8(gameInfo.GetSaveFolder()))));
				break;
			}
			case kContextMenuUpdateFolder:
			{
				fs::path path(gameInfo.GetUpdate().GetPath());
				_stripPathFilename(path);
				wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("file:{}", _pathToUtf8(path))));
				break;
			}
			case kContextMenuDLCFolder:
			{
				fs::path path(gameInfo.GetAOC().front().GetPath());
				_stripPathFilename(path);
				wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("file:{}", _pathToUtf8(path))));
				break;
			}
			case kContextMenuRemoveCache:
			{
				RemoveCache(_getCachesPaths(gameInfo.GetBaseTitleId()), gameInfo.GetTitleName());
				break;
			}
			case kContextMenuEditGraphicPacks:
			{
				wxTitleIdEvent open_event(wxEVT_OPEN_GRAPHIC_PACK, title_id);
				ProcessWindowEvent(open_event);
				break;
			}
			case kContextMenuEditGameProfile:
			{
				(new GameProfileWindow(GetParent(), title_id))->Show();
				break;
			}
            case kContextMenuCreateShortcut:
#if BOOST_OS_LINUX || BOOST_OS_WINDOWS
                 CreateShortcut(gameInfo);
#endif
                break;
			}
		}
	}

	switch (event.GetId())
	{
	case kContextMenuRefreshGames:
		ReloadGameEntries(false);
		break;
	case kContextMenuStyleList:
		SetStyle(Style::kList);
		break;
	case kContextMenuStyleIcon:
		SetStyle(Style::kIcons);
		break;
	case kContextMenuStyleIconSmall:
		SetStyle(Style::kSmallIcons);
		break;
	}
}

void wxGameList::OnColumnClick(wxListEvent& event)
{
	const int column = event.GetColumn();
	SortEntries(column);
	UpdateItemColors();
	event.Skip();
}

void wxGameList::OnColumnRightClick(wxListEvent& event)
{
	enum ItemIds
	{
		ResetWidth = wxID_HIGHEST + 1,
		ResetOrder,

		ShowName,
		ShowVersion,
		ShowDlc,
		ShowGameTime,
		ShowLastPlayed,
		ShowRegion,
        ShowTitleId
	};
	const int column = event.GetColumn();
	wxMenu menu;
	menu.SetClientObject(new wxCustomData(column));

	menu.Append(ResetWidth, _("Reset &width"));
	menu.Append(ResetOrder, _("Reset &order"))	;

	menu.AppendSeparator();
	menu.AppendCheckItem(ShowName, _("Show &name"))->Check(GetColumnWidth(ColumnName) > 0);
	menu.AppendCheckItem(ShowVersion, _("Show &version"))->Check(GetColumnWidth(ColumnVersion) > 0);
	menu.AppendCheckItem(ShowDlc, _("Show &dlc"))->Check(GetColumnWidth(ColumnDLC) > 0);
	menu.AppendCheckItem(ShowGameTime, _("Show &game time"))->Check(GetColumnWidth(ColumnGameTime) > 0);
	menu.AppendCheckItem(ShowLastPlayed, _("Show &last played"))->Check(GetColumnWidth(ColumnGameStarted) > 0);
	menu.AppendCheckItem(ShowRegion, _("Show &region"))->Check(GetColumnWidth(ColumnRegion) > 0);
    menu.AppendCheckItem(ShowTitleId, _("Show &title ID"))->Check(GetColumnWidth(ColumnTitleID) > 0);

	menu.Bind(wxEVT_COMMAND_MENU_SELECTED,
		[this](wxCommandEvent& event) {
			event.Skip();

			const auto menu = dynamic_cast<wxMenu*>(event.GetEventObject());
			const int column = dynamic_cast<wxCustomData<int>*>(menu->GetClientObject())->GetData();
			auto& config = GetConfig();

			switch (event.GetId())
			{
			case ShowName:
				config.column_width.name = menu->IsChecked(ShowName) ? DefaultColumnSize::name : 0;
				break;
			case ShowVersion:
				config.column_width.version = menu->IsChecked(ShowVersion) ? DefaultColumnSize::version : 0;
				break;
			case ShowDlc:
				config.column_width.dlc = menu->IsChecked(ShowDlc) ? DefaultColumnSize::dlc : 0;
				break;
			case ShowGameTime:
				config.column_width.game_time = menu->IsChecked(ShowGameTime) ? DefaultColumnSize::game_time : 0;
				break;
			case ShowLastPlayed:
				config.column_width.game_started = menu->IsChecked(ShowLastPlayed) ? DefaultColumnSize::game_started : 0;
				break;
			case ShowRegion:
				config.column_width.region = menu->IsChecked(ShowRegion) ? DefaultColumnSize::region : 0;
				break;
            case ShowTitleId:
                config.column_width.title_id = menu->IsChecked(ShowTitleId) ? DefaultColumnSize::title_id : 0;
                break;
			case ResetWidth:
			{
				switch (column)
				{
				case ColumnIcon:
					break;
				case ColumnName:
					config.column_width.name = DefaultColumnSize::name;
					break;
				case ColumnVersion:
					config.column_width.version = DefaultColumnSize::version;
					break;
				case ColumnDLC:
					config.column_width.dlc = DefaultColumnSize::dlc;
					break;
				case ColumnGameTime:
					config.column_width.game_time = DefaultColumnSize::game_time;
					break;
				case ColumnGameStarted:
					config.column_width.game_started = DefaultColumnSize::game_started;
					break;
				case ColumnRegion:
					config.column_width.region = DefaultColumnSize::region;
					break;
                case ColumnTitleID:
                    config.column_width.title_id = DefaultColumnSize::title_id;
				default:
					return;
				}

				break;
			}
			case ResetOrder:
			{
				config.game_list_column_order.clear();
				wxArrayInt order(ColumnCounts);
				std::iota(order.begin(), order.end(), 0);
				#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
				SetColumnsOrder(order);
				#endif
				//ApplyGameListColumnWidths();
				//Refresh();
				//return;
			}
			}

			g_config.Save();
			ApplyGameListColumnWidths();
		});

	PopupMenu(&menu);
	event.Skip();
}

void wxGameList::ApplyGameListColumnWidths()
{
	const auto& config = GetConfig();
	wxWindowUpdateLocker lock(this);
	SetColumnWidth(ColumnIcon, kListIconWidth);
	SetColumnWidth(ColumnName, config.column_width.name);
	SetColumnWidth(ColumnVersion, config.column_width.version);
	SetColumnWidth(ColumnDLC, config.column_width.dlc);
	SetColumnWidth(ColumnGameTime, config.column_width.game_time);
	SetColumnWidth(ColumnGameStarted, config.column_width.game_started);
	SetColumnWidth(ColumnRegion, config.column_width.region);
    SetColumnWidth(ColumnTitleID, config.column_width.title_id);

	AdjustLastColumnWidth();
}

void wxGameList::OnColumnBeginResize(wxListEvent& event)
{
	const int column = event.GetColumn();
	const int width = GetColumnWidth(column);
	int last_col_index = 0;
	for(int i = GetColumnCount() - 1; i > 0; i--)
	{
#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
		if(GetColumnWidth(GetColumnIndexFromOrder(i)) > 0) 
		{
			last_col_index = GetColumnIndexFromOrder(i);
			break;
		}
#else
		if(GetColumnWidth(i) > 0) 
		{
			last_col_index = i;
			break;
		}
#endif
	}
	if (width == 0 || column == ColumnIcon || column == last_col_index) // dont resize hidden name, icon, and last column
		event.Veto();
	else
		event.Skip();
}

void wxGameList::OnColumnResize(wxListEvent& event)
{
	event.Skip();

	if(m_style != Style::kList)
		return;

	const int column = event.GetColumn();
	const int width = GetColumnWidth(column);

	auto& config = GetConfig();
	switch (column)
	{
	case ColumnName:
		config.column_width.name = width;
		break;
	case ColumnVersion:
		config.column_width.version = width;
		break;
	case ColumnDLC:
		config.column_width.dlc = width;
		break;
	case ColumnGameTime:
		config.column_width.game_time = width;
		break;
	case ColumnGameStarted:
		config.column_width.game_started = width;
		break;
	case ColumnRegion:
		config.column_width.region = width;
		break;
	default:
		break;
	}

	g_config.Save();
	AdjustLastColumnWidth();
}

void wxGameList::OnClose(wxCloseEvent& event)
{
	event.Skip();
	m_exit = true;
}

int wxGameList::FindInsertPosition(TitleId titleId)
{
	SortData data{ this, s_last_column, s_direction };
	const auto itemCount = GetItemCount();
	if (itemCount == 0)
		return 0;
	// todo - optimize this with binary search

	for (int i = 0; i < itemCount; i++)
	{
		if (SortComparator(titleId, (uint64)GetItemData(i), &data) <= 0)
			return i;
	}
	return itemCount;
}

void wxGameList::OnGameEntryUpdatedByTitleId(wxTitleIdEvent& event)
{
	const auto titleId = event.GetTitleId();
	GameInfo2 gameInfo = CafeTitleList::GetGameInfo(titleId);
	if (!gameInfo.IsValid() || gameInfo.IsSystemDataTitle())
	{
		// entry no longer exists or is not a valid game
		// we dont need to remove list entries here because all delete operations should trigger a full list refresh
		return;
	}
	TitleId baseTitleId = gameInfo.GetBaseTitleId();
	bool isNewEntry = false;

	auto index = FindListItemByTitleId(baseTitleId);
	if(index == wxNOT_FOUND)
	{
		// entry doesn't exist
		index = InsertItem(FindInsertPosition(baseTitleId), wxHelper::FromUtf8(GetNameByTitleId(baseTitleId)));
		SetItemPtrData(index, baseTitleId);
		isNewEntry = true;
	}

	int icon = 0; /* 0 is the default empty icon */
	int icon_small = 0; /* 0 is the default empty icon */
	QueryIconForTitle(baseTitleId, icon, icon_small);

	if (m_style == Style::kList)
	{
		SetItemColumnImage(index, ColumnIcon, icon_small);

		SetItem(index, ColumnName, wxHelper::FromUtf8(GetNameByTitleId(baseTitleId)));

		SetItem(index, ColumnVersion, fmt::format("{}", gameInfo.GetVersion()));

		if(gameInfo.HasAOC())
			SetItem(index, ColumnDLC, fmt::format("{}", gameInfo.GetAOCVersion()));
		else
			SetItem(index, ColumnDLC, wxString());

		if (isNewEntry)
		{
			iosu::pdm::GameListStat playTimeStat;
			if (iosu::pdm::GetStatForGamelist(baseTitleId, playTimeStat))
			{
				// time played
				uint32 minutesPlayed = playTimeStat.numMinutesPlayed;
				if (minutesPlayed == 0)
					SetItem(index, ColumnGameTime, wxEmptyString);
				else if (minutesPlayed < 60)
					SetItem(index, ColumnGameTime, formatWxString(wxPLURAL("{} minute", "{} minutes", minutesPlayed), minutesPlayed));
				else
				{
					uint32 hours = minutesPlayed / 60;
					uint32 minutes = minutesPlayed % 60;
					wxString hoursText = formatWxString(wxPLURAL("{} hour", "{} hours", hours), hours);
					wxString minutesText = formatWxString(wxPLURAL("{} minute", "{} minutes", minutes), minutes);
					SetItem(index, ColumnGameTime, hoursText + " " + minutesText);
				}
				
				// last played
				if (playTimeStat.last_played.year != 0)
				{
					const wxDateTime tmp((wxDateTime::wxDateTime_t)playTimeStat.last_played.day, (wxDateTime::Month)playTimeStat.last_played.month, (wxDateTime::wxDateTime_t)playTimeStat.last_played.year, 0, 0, 0, 0);
					SetItem(index, ColumnGameStarted, tmp.FormatDate());
				}
				else
					SetItem(index, ColumnGameStarted, _("never"));
			}
			else
			{
				SetItem(index, ColumnGameTime, wxEmptyString);
				SetItem(index, ColumnGameStarted, _("never"));
			}
		}


		const auto region_text = fmt::format("{}", gameInfo.GetRegion());
		SetItem(index, ColumnRegion, wxGetTranslation(region_text));
        SetItem(index, ColumnTitleID, fmt::format("{:016x}", titleId));
	}
	else if (m_style == Style::kIcons)
	{
		SetItemImage(index, icon);
	}
	else if (m_style == Style::kSmallIcons)
	{
		SetItemImage(index, icon_small);
	}
	if (isNewEntry)
		UpdateItemColors(index);
}

void wxGameList::OnItemActivated(wxListEvent& event)
{
	event.Skip();

	const auto selection = event.GetIndex();
	if (selection == wxNOT_FOUND)
		return;

	const auto item_data = (uint64)GetItemData(selection);
	if(item_data == kDefaultEntryData)
	{
		const wxCommandEvent open_settings_event(wxEVT_OPEN_SETTINGS);
		wxPostEvent(this, open_settings_event);
		return;
	}

	TitleInfo titleInfo;
	if (!CafeTitleList::GetFirstByTitleId(item_data, titleInfo))
		return;
	if (!titleInfo.IsValid())
		return;

	MainWindow::RequestLaunchGame(titleInfo.GetPath(), wxLaunchGameEvent::INITIATED_BY::GAME_LIST);
}

void wxGameList::OnTimer(wxTimerEvent& event)
{
	const auto& obj = event.GetTimer().GetId();
	if(obj == m_tooltip_timer->GetId())
	{
		m_tooltip_window->Hide();

		auto flag = wxLIST_HITTEST_ONITEM;
		const auto item = this->HitTest(m_mouse_position, flag);
		if(item != wxNOT_FOUND )
		{
			//const auto title_id = (uint64_t)GetItemData(item);
			//auto entry = GetGameEntry(title_id);
			//if (entry && entry->is_update)
			//{
			//	m_tooltip_window->SetPosition(wxPoint(m_mouse_position.x + 15, m_mouse_position.y + 15));
			//	m_tooltip_window->SendSizeEvent();
			//	m_tooltip_window->Show();
			//}
		}
	}

}

void wxGameList::OnMouseMove(wxMouseEvent& event)
{
	m_tooltip_timer->Stop();
	m_tooltip_timer->StartOnce(250);
	m_mouse_position = event.GetPosition();
}

void wxGameList::OnLeaveWindow(wxMouseEvent& event)
{
	m_tooltip_timer->Stop();
	m_tooltip_window->Hide();
}

void wxGameList::HandleTitleListCallback(CafeTitleListCallbackEvent* evt)
{
	if (evt->eventType == CafeTitleListCallbackEvent::TYPE::TITLE_DISCOVERED ||
		evt->eventType == CafeTitleListCallbackEvent::TYPE::TITLE_REMOVED)
	{
		wxQueueEvent(this, new wxTitleIdEvent(wxEVT_GAME_ENTRY_ADDED_OR_REMOVED, evt->titleInfo->GetAppTitleId()));
	}
}

void wxGameList::RemoveCache(const std::list<fs::path>& cachePaths, const std::string& titleName)
{
	wxMessageDialog dialog(this, formatWxString(_("Remove the shader caches for {}?"), titleName), _("Remove shader caches"), wxCENTRE | wxYES_NO | wxICON_EXCLAMATION);
	dialog.SetYesNoLabels(_("Yes"), _("No"));

	const auto dialogResult = dialog.ShowModal();
	if (dialogResult != wxID_YES)
		return;
	std::list<std::string> errs;
	for (const fs::path& cachePath : cachePaths)
	{
		if (std::error_code ec; !fs::remove(cachePath, ec))
			errs.emplace_back(fmt::format("{} : {}", cachePath.string(), ec.message()));
	}
	if (errs.empty())
		wxMessageDialog(this, _("The shader caches were removed!"), _("Shader caches removed"), wxCENTRE | wxOK | wxICON_INFORMATION).ShowModal();
	else
		wxMessageDialog(this, formatWxString(_("Failed to remove the shader caches:\n{}"), fmt::join(errs, "\n")), _("Error"), wxCENTRE | wxOK | wxICON_ERROR).ShowModal();
}

void wxGameList::AsyncWorkerThread()
{
	while (m_async_worker_active)
	{
		m_async_task_count.decrementWithWait();
		// get next titleId to load (if any)
		m_async_worker_mutex.lock();
		bool hasJob = !m_icon_load_queue.empty();
		TitleId titleId = 0;
		if (hasJob)
		{
			titleId = m_icon_load_queue.front();
			m_icon_load_queue.erase(m_icon_load_queue.begin());
		}
		m_async_worker_mutex.unlock();
		if (!hasJob)
			continue;
		if(m_icon_loaded.find(titleId) != m_icon_loaded.end())
			continue;
		m_icon_loaded.emplace(titleId);
		// load and process icon
		TitleInfo titleInfo;
		if( !CafeTitleList::GetFirstByTitleId(titleId, titleInfo) )
			continue;
		std::string tempMountPath = TitleInfo::GetUniqueTempMountingPath();
		if(!titleInfo.Mount(tempMountPath, "", FSC_PRIORITY_BASE))
			continue;
		auto tgaData = fsc_extractFile((tempMountPath + "/meta/iconTex.tga").c_str());
		bool iconSuccessfullyLoaded = false;
		if (tgaData && tgaData->size() > 16)
		{
			wxMemoryInputStream tmp_stream(tgaData->data(), tgaData->size());
			const wxImage image(tmp_stream);
			// todo - is wxImageList thread safe?
			int icon = m_image_list->Add(image.Scale(kIconWidth, kIconWidth, wxIMAGE_QUALITY_BICUBIC));
			int icon_small = m_image_list_small->Add(image.Scale(kListIconWidth, kListIconWidth, wxIMAGE_QUALITY_BICUBIC));
			// store in cache
			m_icon_cache_mtx.lock();
			m_icon_cache.try_emplace(titleId, icon, icon_small);
			m_icon_cache_mtx.unlock();
			iconSuccessfullyLoaded = true;
		}
		else
		{
			cemuLog_log(LogType::Force, "Failed to load icon for title {:016x}", titleId);
		}
		titleInfo.Unmount(tempMountPath);
		// notify UI about loaded icon
		if(iconSuccessfullyLoaded)
			wxQueueEvent(this, new wxTitleIdEvent(wxEVT_GAME_ENTRY_ADDED_OR_REMOVED, titleId));
	}
}

void wxGameList::RequestLoadIconAsync(TitleId titleId)
{
	m_async_worker_mutex.lock();
	m_icon_load_queue.push_back(titleId);
	m_async_worker_mutex.unlock();
	m_async_task_count.increment();
}

// returns icons if cached, otherwise an async request to load them is made
bool wxGameList::QueryIconForTitle(TitleId titleId, int& icon, int& iconSmall)
{
	m_icon_cache_mtx.lock();
	auto it = m_icon_cache.find(titleId);
	if (it == m_icon_cache.end())
	{
		m_icon_cache_mtx.unlock();
		RequestLoadIconAsync(titleId);
		return false;
	}
	icon = it->second.first;
	iconSmall = it->second.second;
	m_icon_cache_mtx.unlock();
	return true;
}

void wxGameList::DeleteCachedStrings() 
{
	m_name_cache.clear();
}

#if BOOST_OS_LINUX || BOOST_OS_WINDOWS
void wxGameList::CreateShortcut(GameInfo2& gameInfo) {
    const auto title_id = gameInfo.GetBaseTitleId();
    const auto title_name = gameInfo.GetTitleName();
    auto exe_path = ActiveSettings::GetExecutablePath();
    const char *flatpak_id = getenv("FLATPAK_ID");

    // GetExecutablePath returns the AppImage's temporary mount location, instead of its actual path
    wxString appimage_path;
    if (wxGetEnv(("APPIMAGE"), &appimage_path)) {
        exe_path = appimage_path.utf8_string();
    }

#if BOOST_OS_LINUX
    const wxString desktop_entry_name = wxString::Format("%s.desktop", title_name);
    wxFileDialog entry_dialog(this, _("Choose desktop entry location"), "~/.local/share/applications", desktop_entry_name,
                              "Desktop file (*.desktop)|*.desktop", wxFD_SAVE | wxFD_CHANGE_DIR | wxFD_OVERWRITE_PROMPT);
#elif BOOST_OS_WINDOWS
    // Get '%APPDATA%\Microsoft\Windows\Start Menu\Programs' path
    PWSTR user_shortcut_folder;
    SHGetKnownFolderPath(FOLDERID_Programs, 0, NULL, &user_shortcut_folder);
    const wxString shortcut_name = wxString::Format("%s.lnk", title_name);
    wxFileDialog entry_dialog(this, _("Choose shortcut location"), _pathToUtf8(user_shortcut_folder), shortcut_name,
                              "Shortcut (*.lnk)|*.lnk", wxFD_SAVE | wxFD_CHANGE_DIR | wxFD_OVERWRITE_PROMPT);
#endif
    const auto result = entry_dialog.ShowModal();
    if (result == wxID_CANCEL)
        return;
    const auto output_path = entry_dialog.GetPath();

#if BOOST_OS_LINUX
    std::optional<fs::path> icon_path;
    // Obtain and convert icon
    {
        m_icon_cache_mtx.lock();
        const auto icon_iter = m_icon_cache.find(title_id);
        const auto result_index = (icon_iter != m_icon_cache.cend()) ? std::optional<int>(icon_iter->second.first) : std::nullopt;
        m_icon_cache_mtx.unlock();

        // In most cases it should find it
        if (!result_index){
            wxMessageBox(_("Icon is yet to load, so will not be used by the shortcut"), _("Warning"), wxOK | wxCENTRE | wxICON_WARNING);
        }
        else {
            const fs::path out_icon_dir = ActiveSettings::GetUserDataPath("icons");

            if (!fs::exists(out_icon_dir) && !fs::create_directories(out_icon_dir)){
                wxMessageBox(_("Cannot access the icon directory, the shortcut will have no icon"), _("Warning"), wxOK | wxCENTRE | wxICON_WARNING);
            }
            else {
                icon_path = out_icon_dir / fmt::format("{:016x}.png", gameInfo.GetBaseTitleId());

                auto image = m_image_list->GetIcon(result_index.value()).ConvertToImage();

                wxFileOutputStream png_file(_pathToUtf8(icon_path.value()));
                wxPNGHandler pngHandler;
                if (!pngHandler.SaveFile(&image, png_file, false)) {
                    icon_path = std::nullopt;
                    wxMessageBox(_("The icon was unable to be saved, the shortcut will have no icon"), _("Warning"), wxOK | wxCENTRE | wxICON_WARNING);
                }
            }
        }
    }

    std::string desktop_exec_entry;
    if (flatpak_id)
        desktop_exec_entry = fmt::format("/usr/bin/flatpak run {0} --title-id {1:016x}", flatpak_id, title_id);
    else
        desktop_exec_entry = fmt::format("{0:?} --title-id {1:016x}", _pathToUtf8(exe_path), title_id);

    // 'Icon' accepts spaces in file name, does not accept quoted file paths
    // 'Exec' does not accept non-escaped spaces, and can accept quoted file paths
    auto desktop_entry_string =
            fmt::format("[Desktop Entry]\n"
                        "Name={0}\n"
                        "Comment=Play {0} on Cemu\n"
                        "Exec={1}\n"
                        "Icon={2}\n"
                        "Terminal=false\n"
                        "Type=Application\n"
                        "Categories=Game;\n",
                        title_name,
                        desktop_exec_entry,
                        _pathToUtf8(icon_path.value_or("")));

    if (flatpak_id)
        desktop_entry_string += fmt::format("X-Flatpak={}\n", flatpak_id);

    std::ofstream output_stream(output_path);
    if (!output_stream.good())
    {
        auto errorMsg = formatWxString(_("Failed to save desktop entry to {}"), output_path.utf8_string());
        wxMessageBox(errorMsg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
        return;
    }
    output_stream << desktop_entry_string;

#elif BOOST_OS_WINDOWS
    IShellLinkW *shell_link;
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, reinterpret_cast<LPVOID*>(&shell_link));
    if (SUCCEEDED(hres))
    {
        const auto description = wxString::Format("Play %s on Cemu", title_name);
        const auto args = wxString::Format("-t %016llx", title_id);

        shell_link->SetPath(exe_path.wstring().c_str());
        shell_link->SetDescription(description.wc_str());
        shell_link->SetArguments(args.wc_str());
        shell_link->SetWorkingDirectory(exe_path.parent_path().wstring().c_str());
        // Use icon from Cemu exe for now since we can't embed icons into the shortcut
        // in the future we could convert and store icons in AppData or ProgramData
        shell_link->SetIconLocation(exe_path.wstring().c_str(), 0);

        IPersistFile *shell_link_file;
        // save the shortcut
        hres = shell_link->QueryInterface(IID_IPersistFile, reinterpret_cast<LPVOID*>(&shell_link_file));
        if (SUCCEEDED(hres))
        {
            hres = shell_link_file->Save(output_path.wc_str(), TRUE);
            shell_link_file->Release();
        }
        shell_link->Release();
    }
#endif
}
#endif
