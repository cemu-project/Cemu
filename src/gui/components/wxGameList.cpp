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

wxGameList::wxGameList(wxWindow* parent, wxWindowID id)
	: wxListCtrl(parent, id, wxDefaultPosition, wxDefaultSize, GetStyleFlags(Style::kList)), m_style(Style::kList)
{
	CreateListColumns();

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
		order.reserve(ColumnFavorite);
		
		const auto order_string = std::string_view(config.game_list_column_order).substr(1);
		
		const boost::char_separator<char> sep(",");
		boost::tokenizer tokens(order_string.begin(), order_string.end(), sep);
		for(const auto& token : tokens)
		{
			order.push_back(ConvertString<int>(token, 10));
		}
		
		#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
		if(order.GetCount() == ColumnFavorite)
			SetColumnsOrder(order);
		#endif
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
		name = titleInfo.GetTitleName();
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
	for (int i = startIndex; i < GetItemCount(); ++i)
	{
		const auto titleId = (uint64)GetItemData(i);
		if (GetConfig().IsGameListFavorite(titleId))//entry->favorite)
			SetItemBackgroundColour(i, kFavoriteColor);
		else if ((i&1) != 0)
			SetItemBackgroundColour(i, kSecondColor);
		else
			SetItemBackgroundColour(i, 0xFFFFFFUL);
	}
}

static inline int strongorder_to_int(const std::strong_ordering &wo)
{
	/* No easy conversion seems to exists in C++20 */
	if (wo < 0)
		return -1;
	else if (wo > 0)
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
		return strongorder_to_int(std::tie(isFavoriteB, name1) <=> std::tie(isFavoriteA, name2));
	else
		return strongorder_to_int(std::tie(isFavoriteB, name2) <=> std::tie(isFavoriteA, name1));
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

void wxGameList::CreateListColumns()
{
	DeleteAllColumns();
	
	const auto& config = GetConfig();
	wxListItem col0;
	col0.SetId(ColumnHiddenName);
	col0.SetWidth(0);
	InsertColumn(ColumnHiddenName, col0);

	wxListItem col1;
	col1.SetId(ColumnIcon);
	col1.SetWidth(kListIconWidth);
	InsertColumn(ColumnIcon, col1);

	wxListItem col2;
	col2.SetId(ColumnName);
	col2.SetText(_("Game"));
	col2.SetWidth(config.column_width.name);
	InsertColumn(ColumnName, col2);

	wxListItem col3;
	col3.SetId(ColumnVersion);
	col3.SetText(_("Version"));
	col3.SetWidth(config.column_width.version);
	col3.SetAlign(wxLIST_FORMAT_RIGHT);
	InsertColumn(ColumnVersion, col3);

	wxListItem col4;
	col4.SetId(ColumnDLC);
	col4.SetText(_("DLC"));
	col4.SetWidth(config.column_width.dlc);
	col4.SetAlign(wxLIST_FORMAT_RIGHT);
	InsertColumn(ColumnDLC, col4);

	wxListItem col5;
	col5.SetId(ColumnGameTime);
	col5.SetText(_("You've played"));
	col5.SetWidth(config.column_width.game_time);
	InsertColumn(ColumnGameTime, col5);

	wxListItem col6;
	col6.SetId(ColumnGameStarted);
	col6.SetText(_("Last played"));
	col6.SetWidth(config.column_width.game_started);
	InsertColumn(ColumnGameStarted, col6);
	
	wxListItem col7;
	col7.SetId(ColumnRegion);
	col7.SetText(_("Region"));
	col7.SetWidth(config.column_width.region);
	InsertColumn(ColumnRegion, col7);
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

	kContextMenuStyleList,
	kContextMenuStyleIcon,
	kContextMenuStyleIconSmall,
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
			menu.Append(kContextMenuEditGraphicPacks, _("&Edit graphic packs"));
			menu.Append(kContextMenuEditGameProfile, _("&Edit game profile"));
			
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
				wxTextEntryDialog dialog(this, wxEmptyString, L"Enter a custom game title", customName);
				if(dialog.ShowModal() == wxID_OK)
				{
					const auto custom_name = dialog.GetValue();
					GetConfig().SetGameListCustomName(title_id, wxHelper::MakeUTF8(custom_name));
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
				wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("file:{}", _utf8Wrapper(path))));
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
				wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("file:{}", _utf8Wrapper(gameInfo.GetSaveFolder()))));
				break;
			}
			case kContextMenuUpdateFolder:
			{
				fs::path path(gameInfo.GetUpdate().GetPath());
				_stripPathFilename(path);
				wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("file:{}", _utf8Wrapper(path))));
				break;
			}
			case kContextMenuDLCFolder:
			{
				fs::path path(gameInfo.GetAOC().front().GetPath());
				_stripPathFilename(path);
				wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("file:{}", _utf8Wrapper(path))));
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
	menu.AppendCheckItem(ColumnRegion, _("Show &region"))->Check(GetColumnWidth(ColumnRegion) > 0);
	//menu.AppendSeparator();
	//menu.Append(ResetOrder, _("&Reset order"));

	menu.Bind(wxEVT_COMMAND_MENU_SELECTED,
		[this](wxCommandEvent& event) {
			event.Skip();

			const auto menu = dynamic_cast<wxMenu*>(event.GetEventObject());
			const int column = dynamic_cast<wxCustomData<int>*>(menu->GetClientObject())->GetData();
			auto& config = GetConfig();

			switch (event.GetId())
			{
			case ShowName:
				config.column_width.name = menu->IsChecked(ShowName) ? 500 : 0;
				break;
			case ShowVersion:
				config.column_width.version = menu->IsChecked(ShowVersion) ? 60 : 0;
				break;
			case ShowDlc:
				config.column_width.dlc = menu->IsChecked(ShowDlc) ? 50 : 0;
				break;
			case ShowGameTime:
				config.column_width.game_time = menu->IsChecked(ShowGameTime) ? 140 : 0;
				break;
			case ShowLastPlayed:
				config.column_width.game_started = menu->IsChecked(ShowLastPlayed) ? 160 : 0;
				break;
			case ColumnRegion:
				config.column_width.region = menu->IsChecked(ColumnRegion) ? 80 : 0;
				break;
			case ResetWidth:
			{
				switch (column)
				{
				case ColumnName:
					config.column_width.name = 500;
					break;
				case ColumnVersion:
					config.column_width.version = 60;
					break;
				case ColumnDLC:
					config.column_width.dlc = 50;
					break;
				case ColumnGameTime:
					config.column_width.game_time = 140;
					break;
				case ColumnGameStarted:
					config.column_width.game_started = 160;
					break;
				case ColumnRegion:
					config.column_width.region = 80;
					break;
				default:
					return;
				}

				break;
			}
			case ResetOrder:
			{
				config.game_list_column_order.clear();
				wxArrayInt order(ColumnFavorite);
				std::iota(order.begin(), order.end(), 0);
				#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
				SetColumnsOrder(order);
				#endif
				Refresh();
				return;
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
	auto set_width = [this](int id, int width)
	{
		if (width == -3)
			wxAutosizeColumn(this, id);
		else
			this->SetColumnWidth(id, width);
	};
	
	const auto& config = GetConfig();
	wxWindowUpdateLocker lock(this);
	set_width(ColumnName, config.column_width.name);
	set_width(ColumnVersion, config.column_width.version);
	set_width(ColumnDLC, config.column_width.dlc);
	set_width(ColumnGameTime, config.column_width.game_time);
	set_width(ColumnGameStarted, config.column_width.game_started);
	set_width(ColumnRegion, config.column_width.region);
}

void wxGameList::OnColumnBeginResize(wxListEvent& event)
{
	const int column = event.GetColumn();
	const int width = GetColumnWidth(column);
	if (width == 0)
		event.Veto();
	else
		event.Skip();
}
void wxGameList::OnColumnResize(wxListEvent& event)
{
	event.Skip();

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
	default:
		return;
	}

	g_config.Save();
}

void wxGameList::OnColumnDrag(wxListEvent& event)
{
	const auto column = event.GetColumn();
	const auto width = GetColumnWidth(column);
	if (column == ColumnHiddenName || width == 0)
		event.Veto();
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
	// get GameInfo from title list
	GameInfo2 gameInfo = CafeTitleList::GetGameInfo(titleId);

	if (!gameInfo.IsValid())
	{
		// entry no longer exists
		// we dont need to do anything here because all delete operations should trigger a full list refresh
		return;
	}
	TitleId baseTitleId = gameInfo.GetBaseTitleId();
	bool isNewEntry = false;

	auto index = FindListItemByTitleId(baseTitleId);
	if(index == wxNOT_FOUND)
	{
		// entry doesn't exist
		index = InsertItem(FindInsertPosition(baseTitleId), wxHelper::FromUtf8(gameInfo.GetTitleName()));
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
				uint32 timePlayed = playTimeStat.numMinutesPlayed * 60;
				if (timePlayed == 0)
					SetItem(index, ColumnGameTime, wxEmptyString);
				else if (timePlayed < 60)
					SetItem(index, ColumnGameTime, fmt::format("{} seconds", timePlayed));
				else if (timePlayed < 60 * 60)
					SetItem(index, ColumnGameTime, fmt::format("{} minutes", timePlayed / 60));
				else
					SetItem(index, ColumnGameTime, fmt::format("{} hours {} minutes", timePlayed / 3600, (timePlayed / 60) % 60));
				// last played
				if (playTimeStat.last_played.year != 0)
				{
					const wxDateTime tmp((wxDateTime::wxDateTime_t)playTimeStat.last_played.day, (wxDateTime::Month)playTimeStat.last_played.month, (wxDateTime::wxDateTime_t)playTimeStat.last_played.year, 0, 0, 0, 0);
					SetItem(index, ColumnGameStarted, tmp.FormatISODate());
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
		SetItem(index, ColumnRegion, _(region_text));
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
