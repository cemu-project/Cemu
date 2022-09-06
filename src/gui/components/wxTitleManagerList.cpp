#include "gui/components/wxTitleManagerList.h"
#include "gui/helpers/wxHelpers.h"
#include "util/helpers/SystemException.h"
#include "Cafe/TitleList/GameInfo.h"
#include "Cafe/TitleList/TitleInfo.h"
#include "Cafe/TitleList/TitleList.h"
#include "gui/components/wxGameList.h"
#include "gui/helpers/wxCustomEvents.h"
#include "gui/helpers/wxHelpers.h"

#include <wx/imaglist.h>
#include <wx/wupdlock.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/timer.h>
#include <wx/panel.h>
#include <wx/progdlg.h>
#include "../wxHelper.h"

#include <functional>

#include "config/ActiveSettings.h"
#include "gui/ChecksumTool.h"
#include "gui/MainWindow.h"
#include "Cafe/TitleList/TitleId.h"
#include "Cafe/TitleList/SaveList.h"
#include "Cafe/TitleList/TitleList.h"

#include <zarchive/zarchivewriter.h>
#include <zarchive/zarchivereader.h>

#include "Common/filestream.h"

wxDEFINE_EVENT(wxEVT_TITLE_FOUND, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_TITLE_REMOVED, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_REMOVE_ENTRY, wxCommandEvent);

wxTitleManagerList::wxTitleManagerList(wxWindow* parent, wxWindowID id)
	: wxListCtrl(parent, id, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL)
{
	AddColumns();

	// tooltip TODO: extract class mb wxPanelTooltip
	m_tooltip_window = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
	auto* tooltip_sizer = new wxBoxSizer(wxVERTICAL);
	m_tooltip_text = new wxStaticText(m_tooltip_window, wxID_ANY, wxEmptyString);
	tooltip_sizer->Add(m_tooltip_text , 0, wxALL, 5);
	m_tooltip_window->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
	m_tooltip_window->SetSizerAndFit(tooltip_sizer);
	m_tooltip_window->Hide();
	m_tooltip_timer = new wxTimer(this);

	Bind(wxEVT_LIST_COL_CLICK, &wxTitleManagerList::OnColumnClick, this);
	Bind(wxEVT_CONTEXT_MENU, &wxTitleManagerList::OnContextMenu, this);
	Bind(wxEVT_LIST_ITEM_SELECTED, &wxTitleManagerList::OnItemSelected, this);
	Bind(wxEVT_TIMER, &wxTitleManagerList::OnTimer, this);
	Bind(wxEVT_REMOVE_ITEM, &wxTitleManagerList::OnRemoveItem, this);
	Bind(wxEVT_REMOVE_ENTRY, &wxTitleManagerList::OnRemoveEntry, this);
	Bind(wxEVT_TITLE_FOUND, &wxTitleManagerList::OnTitleDiscovered, this);
	Bind(wxEVT_TITLE_REMOVED, &wxTitleManagerList::OnTitleRemoved, this);
	Bind(wxEVT_CLOSE_WINDOW, &wxTitleManagerList::OnClose, this);

	m_callbackIdTitleList = CafeTitleList::RegisterCallback([](CafeTitleListCallbackEvent* evt, void* ctx) { ((wxTitleManagerList*)ctx)->HandleTitleListCallback(evt); }, this);
	m_callbackIdSaveList = CafeSaveList::RegisterCallback([](CafeSaveListCallbackEvent* evt, void* ctx) { ((wxTitleManagerList*)ctx)->HandleSaveListCallback(evt); }, this);
}

wxTitleManagerList::~wxTitleManagerList()
{
	CafeSaveList::UnregisterCallback(m_callbackIdSaveList);
	CafeTitleList::UnregisterCallback(m_callbackIdTitleList);
}

boost::optional<const wxTitleManagerList::TitleEntry&> wxTitleManagerList::GetSelectedTitleEntry() const
{
	const auto selection = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selection != wxNOT_FOUND)
	{
		const auto tmp = GetTitleEntry(selection);
		if (tmp.has_value())
			return tmp.value();
	}

	return {};
}

boost::optional<wxTitleManagerList::TitleEntry&> wxTitleManagerList::GetSelectedTitleEntry()
{
	const auto selection = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selection != wxNOT_FOUND)
	{
		const auto tmp = GetTitleEntry(selection);
		if (tmp.has_value())
			return tmp.value();
	}

	return {};
}

//boost::optional<wxTitleManagerList::TitleEntry&> wxTitleManagerList::GetTitleEntry(EntryType type, uint64 titleid)
//{
//	for(const auto& v : m_data)
//	{
//		if (v->entry.title_id == titleid && v->entry.type == type)
//			return v->entry;
//	}
//
//	return {};
//}

boost::optional<wxTitleManagerList::TitleEntry&> wxTitleManagerList::GetTitleEntryByUID(uint64 uid)
{
	for (const auto& v : m_data)
	{
		if (v->entry.location_uid == uid)
			return v->entry;
	}
	return {};
}

void wxTitleManagerList::AddColumns()
{
	wxListItem col0;
	col0.SetId(ColumnTitleId);
	col0.SetText(_("Title ID"));
	col0.SetWidth(120);
	InsertColumn(ColumnTitleId, col0);

	wxListItem col1;
	col1.SetId(ColumnName);
	col1.SetText(_("Name"));
	col1.SetWidth(435);
	InsertColumn(ColumnName, col1);

	wxListItem col2;
	col2.SetId(ColumnType);
	col2.SetText(_("Type"));
	col2.SetWidth(65);
	InsertColumn(ColumnType, col2);

	wxListItem col3;
	col3.SetId(ColumnVersion);
	col3.SetText(_("Version"));
	col3.SetWidth(40);
	InsertColumn(ColumnVersion, col3);

	wxListItem col4;
	col4.SetId(ColumnRegion);
	col4.SetText(_("Region"));
	col4.SetWidth(60);
	InsertColumn(ColumnRegion, col4);

	wxListItem col5;
	col5.SetId(ColumnFormat);
	col5.SetText(_("Format"));
	col5.SetWidth(63);
	InsertColumn(ColumnFormat, col5);
}

wxString wxTitleManagerList::OnGetItemText(long item, long column) const
{
	if (item >= GetItemCount())
		return wxEmptyString;

	const auto entry = GetTitleEntry(item);
	if (entry.has_value())
		return GetTitleEntryText(entry.value(), (ItemColumn)column);

	return wxEmptyString;
}

wxItemAttr* wxTitleManagerList::OnGetItemAttr(long item) const
{
	const auto entry = GetTitleEntry(item);
	const wxColour kSecondColor{ 0xFDF9F2 };
	static wxListItemAttr s_coloured_attr(GetTextColour(), kSecondColor, GetFont());
	return item % 2 == 0 ? nullptr : &s_coloured_attr;
}

boost::optional<wxTitleManagerList::TitleEntry&> wxTitleManagerList::GetTitleEntry(long item)
{
	long counter = 0;
	for (const auto& data : m_sorted_data)
	{
		if (!data.get().visible)
			continue;

		if (item != counter++)
			continue;

		return data.get().entry;
	}
	
	return {};
}

boost::optional<const wxTitleManagerList::TitleEntry&> wxTitleManagerList::GetTitleEntry(const fs::path& path) const
{
	const auto tmp = path.generic_u8string();
	for (const auto& data : m_data)
	{
		if (boost::iequals(data->entry.path.generic_u8string(), tmp))
			return data->entry;
	}

	return {};
}
boost::optional<wxTitleManagerList::TitleEntry&> wxTitleManagerList::GetTitleEntry(const fs::path& path)
{
	const auto tmp = path.generic_u8string();
	for (const auto& data : m_data)
	{
		if (boost::iequals(data->entry.path.generic_u8string(), tmp))
			return data->entry;
	}

	return {};
}

boost::optional<const wxTitleManagerList::TitleEntry&> wxTitleManagerList::GetTitleEntry(long item) const
{
	long counter = 0;
	for (const auto& data : m_sorted_data)
	{
		if (!data.get().visible)
			continue;

		if (item != counter++)
			continue;

		return data.get().entry;
	}

	return {};
}

void wxTitleManagerList::OnConvertToCompressedFormat(uint64 titleId)
{
	TitleInfo titleInfo_base;
	TitleInfo titleInfo_update;
	TitleInfo titleInfo_aoc;

	titleId = TitleIdParser::MakeBaseTitleId(titleId); // if the titleId of a separate update is selected, this converts it back to the base titleId
	TitleIdParser titleIdParser(titleId);
	bool hasBaseTitleId = titleIdParser.GetType() != TitleIdParser::TITLE_TYPE::AOC;
	bool hasUpdateTitleId = titleIdParser.CanHaveSeparateUpdateTitleId();
	TitleId updateTitleId = hasUpdateTitleId ? titleIdParser.GetSeparateUpdateTitleId() : 0;

	// todo - AOC titleIds might differ from the base/update game in other bits than the type. We have to use the meta data from the base/update to match aoc to the base title id
	// for now we just assume they match
	TitleId aocTitleId;
	if (hasBaseTitleId)
		aocTitleId = (titleId & (uint64)~0xFF00000000) | (uint64)0xC00000000;
	else
		aocTitleId = titleId;

	// find base and update
	for (const auto& data : m_data)
	{
		if (hasBaseTitleId && data->entry.title_id == titleId)
		{
			if (!titleInfo_base.IsValid())
			{
				titleInfo_base = TitleInfo(data->entry.path);
			}
			else
			{
				// duplicate entry
			}
		}
		if (hasUpdateTitleId && data->entry.title_id == updateTitleId)
		{
			if (!titleInfo_update.IsValid())
			{
				titleInfo_update = TitleInfo(data->entry.path);
			}
			else
			{
				// duplicate entry
			}
		}
	}
	// find AOC
	for (const auto& data : m_data)
	{
		if (data->entry.title_id == aocTitleId)
		{
			titleInfo_aoc = TitleInfo(data->entry.path);
		}
	}

	std::string msg = wxHelper::MakeUTF8(_("The following content will be converted to a compressed Wii U archive file (.wua):\n \n"));
	
	if (titleInfo_base.IsValid())
		msg.append(fmt::format(fmt::runtime(wxHelper::MakeUTF8(_("Base game: {}"))), _utf8Wrapper(titleInfo_base.GetPath())));
	else
		msg.append(fmt::format(fmt::runtime(wxHelper::MakeUTF8(_("Base game: Not installed")))));

	msg.append("\n");

	if (titleInfo_update.IsValid())
		msg.append(fmt::format(fmt::runtime(wxHelper::MakeUTF8(_("Update: {}"))), _utf8Wrapper(titleInfo_update.GetPath())));
	else
		msg.append(fmt::format(fmt::runtime(wxHelper::MakeUTF8(_("Update: Not installed")))));

	msg.append("\n");

	if (titleInfo_aoc.IsValid())
		msg.append(fmt::format(fmt::runtime(wxHelper::MakeUTF8(_("DLC: {}"))), _utf8Wrapper(titleInfo_aoc.GetPath())));
	else
		msg.append(fmt::format(fmt::runtime(wxHelper::MakeUTF8(_("DLC: Not installed")))));

	const int answer = wxMessageBox(wxString::FromUTF8(msg), _("Confirmation"), wxOK | wxCANCEL | wxCENTRE | wxICON_QUESTION, this);
	if (answer != wxOK)
		return;
	std::vector<TitleInfo*> titlesToConvert;
	if (titleInfo_base.IsValid())
		titlesToConvert.emplace_back(&titleInfo_base);
	if (titleInfo_update.IsValid())
		titlesToConvert.emplace_back(&titleInfo_update);
	if (titleInfo_aoc.IsValid())
		titlesToConvert.emplace_back(&titleInfo_aoc);
	if (titlesToConvert.empty())
		return;
	// get short name
	CafeConsoleLanguage languageId = CafeConsoleLanguage::EN; // todo - use user's locale
	std::string shortName;
	if (titleInfo_base.IsValid())
		shortName = titleInfo_base.GetMetaInfo()->GetShortName(languageId);
	else if (titleInfo_update.IsValid())
		shortName = titleInfo_update.GetMetaInfo()->GetShortName(languageId);
	else if (titleInfo_aoc.IsValid())
		shortName = titleInfo_aoc.GetMetaInfo()->GetShortName(languageId);

	if (!shortName.empty())
	{
		boost::replace_all(shortName, ":", "");
	}
	// for the default output directory we use the first game path configured by the user
	std::wstring defaultDir = L"";
	if (!GetConfig().game_paths.empty())
		defaultDir = GetConfig().game_paths.front();
	// get the short name, which we will use as a suggested default file name
	std::string defaultFileName = shortName;
	boost::replace_all(defaultFileName, "/", "");
	boost::replace_all(defaultFileName, "\\", "");

	CafeConsoleRegion region = CafeConsoleRegion::Auto;
	if (titleInfo_base.IsValid() && titleInfo_base.HasValidXmlInfo())
		region = titleInfo_base.GetMetaInfo()->GetRegion();
	else if (titleInfo_update.IsValid() && titleInfo_update.HasValidXmlInfo())
		region = titleInfo_update.GetMetaInfo()->GetRegion();

	if (region == CafeConsoleRegion::JPN)
		defaultFileName.append(" (JP)");
	else if (region == CafeConsoleRegion::EUR)
		defaultFileName.append(" (EU)");
	else if (region == CafeConsoleRegion::USA)
		defaultFileName.append(" (US)");
	if (titleInfo_update.IsValid())
	{
		defaultFileName.append(fmt::format(" (v{})", titleInfo_update.GetAppTitleVersion()));
	}
	defaultFileName.append(".wua");

	// ask the user to provide a path for the output file
	wxFileDialog saveFileDialog(this, _("Save Wii U game archive file"), defaultDir, wxHelper::FromUtf8(defaultFileName),
			"WUA files (*.wua)|*.wua", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
	if (saveFileDialog.ShowModal() == wxID_CANCEL)
		return;
	fs::path outputPath(wxHelper::MakeFSPath(saveFileDialog.GetPath()));
	fs::path outputPathTmp(wxHelper::MakeFSPath(saveFileDialog.GetPath().append("__tmp")));
	struct ZArchiveWriterContext
	{
		static void NewOutputFile(const int32_t partIndex, void* _ctx)
		{
			ZArchiveWriterContext* ctx = (ZArchiveWriterContext*)_ctx;
			ctx->fs = FileStream::createFile2(ctx->outputPath);
			if (!ctx->fs)
				ctx->isValid = false;
		}

		static void WriteOutputData(const void* data, size_t length, void* _ctx)
		{
			ZArchiveWriterContext* ctx = (ZArchiveWriterContext*)_ctx;
			if (ctx->fs)
				ctx->fs->writeData(data, length);
		}

		bool RecursivelyCountFiles(const std::string& fscPath)
		{
			sint32 fscStatus;
			std::unique_ptr<FSCVirtualFile> vfDir(fsc_openDirIterator(fscPath.c_str(), &fscStatus));
			if (!vfDir)
				return false;
			if (cancelled)
				return false;
			FSCDirEntry dirEntry;
			while (fsc_nextDir(vfDir.get(), &dirEntry))
			{
				if (dirEntry.isFile)
				{
					totalInputFileSize += (uint64)dirEntry.fileSize;
					totalFileCount++;
				}
				else if (dirEntry.isDirectory)
				{
					if (!RecursivelyCountFiles(fmt::format("{}{}/", fscPath, dirEntry.path)))
					{
						return false;
					}
				}
				else
					cemu_assert_unimplemented();
			}
			return true;
		}

		bool RecursivelyAddFiles(std::string archivePath, std::string fscPath)
		{
			sint32 fscStatus;
			std::unique_ptr<FSCVirtualFile> vfDir(fsc_openDirIterator(fscPath.c_str(), &fscStatus));
			if (!vfDir)
				return false;
			if (cancelled)
				return false;
			zaWriter->MakeDir(archivePath.c_str(), false);
			FSCDirEntry dirEntry;
			while (fsc_nextDir(vfDir.get(), &dirEntry))
			{
				if (dirEntry.isFile)
				{
					zaWriter->StartNewFile((archivePath + dirEntry.path).c_str());
					std::unique_ptr<FSCVirtualFile> vFile(fsc_open((fscPath + dirEntry.path).c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus));
					if (!vFile)
						return false;
					transferBuffer.resize(32 * 1024); // 32KB
					uint32 readBytes;
					while (true)
					{
                        readBytes = vFile->fscReadData(transferBuffer.data(), transferBuffer.size());
                        if(readBytes == 0)
                            break;
						zaWriter->AppendData(transferBuffer.data(), readBytes);
						if (cancelled)
							return false;
						transferredInputBytes += readBytes;
					}
					currentFileIndex++;
				}
				else if (dirEntry.isDirectory)
				{
					if (!RecursivelyAddFiles(fmt::format("{}{}/", archivePath, dirEntry.path), fmt::format("{}{}/", fscPath, dirEntry.path)))
						return false;
				}
				else
					cemu_assert_unimplemented();
			}
			return true;
		}

		bool LoadTitleMetaAndCountFiles(TitleInfo* titleInfo)
		{
			std::string temporaryMountPath = TitleInfo::GetUniqueTempMountingPath();
			titleInfo->Mount(temporaryMountPath.c_str(), "", FSC_PRIORITY_BASE);
			bool r = RecursivelyCountFiles(temporaryMountPath);
			titleInfo->Unmount(temporaryMountPath.c_str());
			return r;
		}

		bool StoreTitle(TitleInfo* titleInfo)
		{
			std::string temporaryMountPath = TitleInfo::GetUniqueTempMountingPath();
			titleInfo->Mount(temporaryMountPath.c_str(), "", FSC_PRIORITY_BASE);
			bool r = RecursivelyAddFiles(fmt::format("{:016x}_v{}/", titleInfo->GetAppTitleId(), titleInfo->GetAppTitleVersion()), temporaryMountPath.c_str());
			titleInfo->Unmount(temporaryMountPath.c_str());
			return r;
		}

		bool AddTitles(TitleInfo** titles, size_t count)
		{
			currentFileIndex = 0;
			totalFileCount = 0;
			// count files
			for (size_t i = 0; i < count; i++)
			{
				if (!LoadTitleMetaAndCountFiles(titles[i]))
					return false;
				if (cancelled)
					return false;
			}
			// store files
			for (size_t i = 0; i < count; i++)
			{
				if (!StoreTitle(titles[i]))
					return false;
			}
			return true;
		}

		~ZArchiveWriterContext()
		{
			delete fs;
			delete zaWriter;
		};

		fs::path outputPath;
		FileStream* fs;
		ZArchiveWriter* zaWriter{};
		bool isValid{false};
		std::vector<uint8> transferBuffer;
		std::atomic_bool cancelled{false};
		// progress
		std::atomic_uint32_t totalFileCount{};
		std::atomic_uint32_t currentFileIndex{};
		std::atomic_uint64_t totalInputFileSize{};
		std::atomic_uint64_t transferredInputBytes{};
	}writerContext;

	// mount and store
	writerContext.isValid = true;
	writerContext.outputPath = outputPathTmp;
	writerContext.zaWriter = new ZArchiveWriter(&ZArchiveWriterContext::NewOutputFile, &ZArchiveWriterContext::WriteOutputData, &writerContext);
	if (!writerContext.isValid)
	{
		// failed to create file
		wxMessageBox(_("Unable to create file"), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		return;
	}
	// open progress dialog
	wxGenericProgressDialog progressDialog("Converting to .wua",
		_("Counting files..."),
		100,    // range
		this,   // parent
		wxPD_CAN_ABORT
	);
	progressDialog.Show();

	auto asyncWorker = std::async(std::launch::async, &ZArchiveWriterContext::AddTitles, &writerContext, titlesToConvert.data(), titlesToConvert.size());
	while (!future_is_ready(asyncWorker))
	{
		if (writerContext.cancelled)
		{
			progressDialog.Update(0, _("Stopping..."));
		}
		else if (writerContext.currentFileIndex != 0)
		{
			uint64 numSizeCompleted = writerContext.transferredInputBytes;
			uint64 numSizeTotal = writerContext.totalInputFileSize;
			uint32 pct = (uint32)(numSizeCompleted * (uint64)100 / numSizeTotal);
			pct = std::min(pct, (uint32)100);
			if (pct >= 100)
				pct = 99; // never set it to 100 as progress == total will make .Update() call ShowModal() and lock up this loop
			std::string textSuffix = fmt::format(" ({}MiB/{}MiB)", numSizeCompleted / 1024 / 1024, numSizeTotal / 1024 / 1024);
			progressDialog.Update(pct, _("Converting files...") + textSuffix);
		}
		else
		{
			progressDialog.Update(0, _("Collecting list of files..." + fmt::format(" ({})", writerContext.totalFileCount.load())));
		}
		if (progressDialog.WasCancelled())
			writerContext.cancelled.store(true);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	progressDialog.Update(100-1, _("Finalizing..."));
	bool r = asyncWorker.get();
	if (!r)
	{
		delete writerContext.fs;
		writerContext.fs = nullptr;
		std::error_code ec;
		fs::remove(outputPathTmp, ec);
		return;
	}
	writerContext.zaWriter->Finalize();
	delete writerContext.fs;
	writerContext.fs = nullptr;
	// verify the created WUA file
	ZArchiveReader* zreader = ZArchiveReader::OpenFromFile(outputPathTmp);
	if (!zreader)
	{
		wxMessageBox(_("Conversion failed\n"), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		std::error_code ec;
		fs::remove(outputPathTmp, ec);
		return;
	}
	// todo - do a quick verification here
	delete zreader;
	// finish
	progressDialog.Hide();
	fs::rename(outputPathTmp, outputPath);

	// ask user if they want to delete the original titles
	// todo


	CafeTitleList::Refresh();
	wxMessageBox(_("Conversion finished\n"), _("Complete"), wxOK | wxCENTRE | wxICON_INFORMATION, this);
}

void wxTitleManagerList::OnClose(wxCloseEvent& event)
{
	event.Skip();
	// wait until all tasks are complete
	if (m_context_worker.valid())
		m_context_worker.get();
}

void wxTitleManagerList::OnColumnClick(wxListEvent& event)
{
	const int column = event.GetColumn();
	SortEntries(column);
	event.Skip();
}

void wxTitleManagerList::RemoveItem(long item)
{
	const int item_count = GetItemCount();

	const ItemData* ref = nullptr;
	long counter = 0;
	for(auto it = m_sorted_data.begin(); it != m_sorted_data.end(); ++it)
	{
		if (!it->get().visible)
			continue;

		if (item != counter++)
			continue;

		ref = &(it->get());
		m_sorted_data.erase(it);
		break;
	}

	// shouldn't happen
	if (ref == nullptr)
		return;
	
	for(auto it = m_data.begin(); it != m_data.end(); ++it)
	{
		if (ref != (*it).get())
			continue;
		
		m_data.erase(it);
		break;
	}

	SetItemCount(std::max(0, item_count - 1));
	RefreshPage();
}

void wxTitleManagerList::RemoveItem(const TitleEntry& entry)
{
	const int item_count = GetItemCount();

	const TitleEntry* ref = &entry;
	for (auto it = m_sorted_data.begin(); it != m_sorted_data.end(); ++it)
	{
		if (ref != &it->get().entry)
			continue;

		m_sorted_data.erase(it);
		break;
	}

	for (auto it = m_data.begin(); it != m_data.end(); ++it)
	{
		if (ref != &(*it).get()->entry)
			continue;

		m_data.erase(it);
		break;
	}

	SetItemCount(std::max(0, item_count - 1));
	RefreshPage();
}

void wxTitleManagerList::OnItemSelected(wxListEvent& event)
{
	event.Skip();
	m_tooltip_timer->Stop();
	const auto selection = event.GetIndex();

	if (selection == wxNOT_FOUND)
	{
		m_tooltip_window->Hide();
		return;
	}

	const auto entry = GetTitleEntry(selection);
	if (!entry.has_value())
	{
		m_tooltip_window->Hide();
		return;
	}

	m_tooltip_window->Hide();
	return;

	//const auto mouse_position = wxGetMousePosition() - GetScreenPosition();
	//m_tooltip_window->SetPosition(wxPoint(mouse_position.x + 15, mouse_position.y + 15));

	//wxString msg;
	//switch(entry->error)
	//{
	//case TitleError::WrongBaseLocation:
	//	msg = _("This base game is installed at the wrong location.");
	//	break;
	//case TitleError::WrongUpdateLocation:
	//	msg = _("This update is installed at the wrong location.");
	//	break;
	//case TitleError::WrongDlcLocation:
	//	msg = _("This DLC is installed at the wrong location.");
	//	break;
	//default:
	//	return;;
	//}

	//m_tooltip_text->SetLabel(wxStringFormat2("{}\n{}", msg, _("You can use the context menu to fix it.")));
	//m_tooltip_window->Fit();
	//m_tooltip_timer->StartOnce(250);
}

enum ContextMenuEntries
{
	kContextMenuOpenDirectory = wxID_HIGHEST + 1,
	kContextMenuDelete,
	kContextMenuLaunch,
	kContextMenuVerifyGameFiles,
	kContextMenuConvertToWUA,
};
void wxTitleManagerList::OnContextMenu(wxContextMenuEvent& event)
{
	// still doing work
	if (m_context_worker.valid() && !future_is_ready(m_context_worker))
		return;
	
	wxMenu menu;
	menu.Bind(wxEVT_COMMAND_MENU_SELECTED, &wxTitleManagerList::OnContextMenuSelected, this);

	const auto selection = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selection == wxNOT_FOUND)
		return;

	const auto entry = GetTitleEntry(selection);
	if (!entry.has_value())
		return;

	if(entry->type == EntryType::Base)
		menu.Append(kContextMenuLaunch, _("&Launch title"));
	
	menu.Append(kContextMenuOpenDirectory, _("&Open directory"));
	if (entry->type != EntryType::Save)
		menu.Append(kContextMenuVerifyGameFiles, _("&Verify integrity of game files"));

	menu.AppendSeparator();

	if (entry->type != EntryType::Save && entry->format != EntryFormat::WUA)
	{
		menu.Append(kContextMenuConvertToWUA, _("Convert to compressed Wii U archive (.wua)"));

		menu.AppendSeparator();
	}
	menu.Append(kContextMenuDelete, _("&Delete"));	

	PopupMenu(&menu);
	
	// TODO: fix tooltip position
}

bool wxTitleManagerList::DeleteEntry(long index, const TitleEntry& entry)
{
	wxDTorFunc reset_text(wxQueueEvent, this, new wxSetStatusBarTextEvent(wxEmptyString, 1));
	wxQueueEvent(this, new wxSetStatusBarTextEvent("Deleting entry...", 1));
	
	wxString msg;
	const bool is_directory = fs::is_directory(entry.path);
	if(is_directory)
		msg = wxStringFormat2(_("Are you really sure that you want to delete the following folder:\n{}"), wxHelper::FromUtf8(_utf8Wrapper(entry.path)));
	else
		msg = wxStringFormat2(_("Are you really sure that you want to delete the following file:\n{}"), wxHelper::FromUtf8(_utf8Wrapper(entry.path)));
	
	const auto result = wxMessageBox(msg, _("Warning"), wxYES_NO | wxCENTRE | wxICON_EXCLAMATION, this);
	if (result == wxNO)
		return false;
				
	std::error_code ec;
	if (is_directory)
	{
		if (entry.type != EntryType::Save)
		{
			// delete content, meta, code folders first
			const auto content = entry.path / "content";
			fs::remove_all(content, ec);

			const auto meta = entry.path / "meta";
			fs::remove_all(meta, ec);

			const auto code = entry.path / "code";
			fs::remove_all(code, ec);
		}
		else
		{
			// delete meta, user folders first
			const auto meta = entry.path / "meta";
			fs::remove_all(meta, ec);
			
			const auto user = entry.path / "user";
			fs::remove_all(user, ec);
		}
		

		// check if folder is empty
		if(fs::is_empty(entry.path, ec))
			fs::remove_all(entry.path, ec);
	}	
	else // simply remove file
		fs::remove(entry.path, ec);
	
	if(ec)
	{
		const auto error_msg = wxStringFormat2(_("Error when trying to delete the entry:\n{}"), GetSystemErrorMessage(ec));
		wxMessageBox(error_msg, _("Error"), wxOK|wxCENTRE, this);
		return false;
	}

	// thread safe request to delete entry
	const auto evt = new wxCommandEvent(wxEVT_REMOVE_ITEM);
	evt->SetInt(index);
	wxQueueEvent(this, evt);
	return true;
}

void wxTitleManagerList::OnContextMenuSelected(wxCommandEvent& event)
{
	// still doing work
	if (m_context_worker.valid() && !future_is_ready(m_context_worker))
		return;
	
	const auto selection = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selection == wxNOT_FOUND)
		return;

	const auto entry = GetTitleEntry(selection);
	if (!entry.has_value())
		return;
	
	switch (event.GetId())
	{
	case kContextMenuOpenDirectory:
		{
			const auto path = fs::is_directory(entry->path) ? entry->path : entry->path.parent_path();
			wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("file:{}", _utf8Wrapper(path))));
		}
		break;
	case kContextMenuDelete:
		m_context_worker = std::async(std::launch::async, &wxTitleManagerList::DeleteEntry, this, selection, entry.value());
		break;
	case kContextMenuLaunch:
		{
			try
			{
				MainWindow::RequestLaunchGame(entry->path, wxLaunchGameEvent::INITIATED_BY::TITLE_MANAGER);
				Close();
			}
			catch (const std::exception& ex)
			{
				forceLog_printf("wxTitleManagerList::OnContextMenuSelected: can't launch title: %s", ex.what());
			}
		}
		break;
	case kContextMenuVerifyGameFiles:
		(new ChecksumTool(this, entry.value()))->Show();
		break;
	case kContextMenuConvertToWUA:
		
		OnConvertToCompressedFormat(entry.value().title_id);
		break;
	}
}

void wxTitleManagerList::OnTimer(wxTimerEvent& event)
{
	if(event.GetTimer().GetId() != m_tooltip_timer->GetId())
	{
		event.Skip();
		return;
	}

	m_tooltip_window->Show();
}

void wxTitleManagerList::OnRemoveItem(wxCommandEvent& event)
{
	RemoveItem(event.GetInt());
}

void wxTitleManagerList::OnRemoveEntry(wxCommandEvent& event)
{
	wxASSERT(event.GetClientData() != nullptr);
	RemoveItem(*(TitleEntry*)event.GetClientData());
}

wxString wxTitleManagerList::GetTitleEntryText(const TitleEntry& entry, ItemColumn column)
{
	switch (column)
	{
	case ColumnTitleId:
		return wxStringFormat2("{:08x}-{:08x}", (uint32)(entry.title_id >> 32), (uint32)(entry.title_id & 0xFFFFFFFF));
	case ColumnName:
		return entry.name;
	case ColumnType:
		return wxStringFormat2("{}", entry.type);
	case ColumnVersion:
		return wxStringFormat2("{}", entry.version);
	case ColumnRegion:
		return wxStringFormat2("{}", entry.region); // TODO its a flag so formatter is currently not correct
	case ColumnFormat:
	{
		if (entry.type == EntryType::Save)
			return _("Save folder");
		switch (entry.format)
		{
		case wxTitleManagerList::EntryFormat::Folder:
			return _("Folder");
		case wxTitleManagerList::EntryFormat::WUD:
			return _("WUD");
		case wxTitleManagerList::EntryFormat::WUA:
			return _("WUA");
		}
		return "";
		//return wxStringFormat2("{}", entry.format);
	}
	default:
		UNREACHABLE;
	}
	
	return wxEmptyString;
}

void wxTitleManagerList::HandleTitleListCallback(CafeTitleListCallbackEvent* evt)
{
	if (evt->eventType != CafeTitleListCallbackEvent::TYPE::TITLE_DISCOVERED &&
		evt->eventType != CafeTitleListCallbackEvent::TYPE::TITLE_REMOVED)
		return;

	auto& titleInfo = *evt->titleInfo;
	wxTitleManagerList::EntryType entryType;
	switch (titleInfo.GetTitleType())
	{
	case TitleIdParser::TITLE_TYPE::BASE_TITLE_UPDATE:
		entryType = EntryType::Update;
		break;
	case TitleIdParser::TITLE_TYPE::AOC:
		entryType = EntryType::Dlc;
		break;
	case TitleIdParser::TITLE_TYPE::SYSTEM_DATA:
	case TitleIdParser::TITLE_TYPE::SYSTEM_OVERLAY_TITLE:
	case TitleIdParser::TITLE_TYPE::SYSTEM_TITLE:
		entryType = EntryType::System;
		break;
	default:
		entryType = EntryType::Base;
	}

	wxTitleManagerList::EntryFormat entryFormat;
	switch (titleInfo.GetFormat())
	{
	case TitleInfo::TitleDataFormat::HOST_FS:
	default:
		entryFormat = EntryFormat::Folder;
		break;
	case TitleInfo::TitleDataFormat::WUD:
		entryFormat = EntryFormat::WUD;
		break;
	case TitleInfo::TitleDataFormat::WIIU_ARCHIVE:
		entryFormat = EntryFormat::WUA;
		break;
	}

	if (evt->eventType == CafeTitleListCallbackEvent::TYPE::TITLE_DISCOVERED)
	{
		if (titleInfo.IsCached())
			return; // the title list only displays non-cached entries
		wxTitleManagerList::TitleEntry entry(entryType, entryFormat, titleInfo.GetPath());

		ParsedMetaXml* metaInfo = titleInfo.GetMetaInfo();

		entry.location_uid = titleInfo.GetUID();
		entry.title_id = titleInfo.GetAppTitleId();
		std::string name = metaInfo->GetLongName(GetConfig().console_language.GetValue());
		const auto nl = name.find(L'\n');
		if (nl != std::string::npos)
			name.replace(nl, 1, " - ");
		entry.name = wxString::FromUTF8(name);
		entry.version = titleInfo.GetAppTitleVersion();
		entry.region = metaInfo->GetRegion();

		auto* cmdEvt = new wxCommandEvent(wxEVT_TITLE_FOUND);
		cmdEvt->SetClientObject(new wxCustomData(entry));
		wxQueueEvent(this, cmdEvt);
	}
	else if (evt->eventType == CafeTitleListCallbackEvent::TYPE::TITLE_REMOVED)
	{
		wxTitleManagerList::TitleEntry entry(entryType, entryFormat, titleInfo.GetPath());
		entry.location_uid = titleInfo.GetUID();
		entry.title_id = titleInfo.GetAppTitleId();

		auto* cmdEvt = new wxCommandEvent(wxEVT_TITLE_REMOVED);
		cmdEvt->SetClientObject(new wxCustomData(entry));
		wxQueueEvent(this, cmdEvt);
	}
}

void wxTitleManagerList::HandleSaveListCallback(struct CafeSaveListCallbackEvent* evt)
{
	if (evt->eventType == CafeSaveListCallbackEvent::TYPE::SAVE_DISCOVERED)
	{
		ParsedMetaXml* metaInfo = evt->saveInfo->GetMetaInfo();
		if (!metaInfo)
			return;
		auto& saveInfo = *evt->saveInfo;
		wxTitleManagerList::TitleEntry entry(EntryType::Save, EntryFormat::Folder, saveInfo.GetPath());
		entry.location_uid = std::hash<uint64>() ( metaInfo->GetTitleId() );
		entry.title_id = metaInfo->GetTitleId();
		std::string name = metaInfo->GetLongName(GetConfig().console_language.GetValue());
		const auto nl = name.find(L'\n');
		if (nl != std::string::npos)
			name.replace(nl, 1, " - ");
		entry.name = wxString::FromUTF8(name);
		entry.version = metaInfo->GetTitleVersion();
		entry.region = metaInfo->GetRegion();

		auto* cmdEvt = new wxCommandEvent(wxEVT_TITLE_FOUND);
		cmdEvt->SetClientObject(new wxCustomData(entry));
		wxQueueEvent(this, cmdEvt);
	}
}

void wxTitleManagerList::OnTitleDiscovered(wxCommandEvent& event)
{
	auto* obj = dynamic_cast<wxTitleManagerList::TitleEntryData_t*>(event.GetClientObject());
	wxASSERT(obj);
	AddTitle(obj);
}

void wxTitleManagerList::OnTitleRemoved(wxCommandEvent& event)
{
	auto* obj = dynamic_cast<wxTitleManagerList::TitleEntryData_t*>(event.GetClientObject());
	wxASSERT(obj);
	for (auto& itr : m_data)
	{
		if (itr.get()->entry.location_uid == obj->get().location_uid)
		{
			RemoveItem(itr.get()->entry);
			break;
		}
	}
}

void wxTitleManagerList::AddTitle(TitleEntryData_t* obj)
{
	const auto& data = obj->GetData();
	if (GetTitleEntryByUID(data.location_uid).has_value())
		return; // already in list
	m_data.emplace_back(std::make_unique<ItemData>(true, data));
	m_sorted_data.emplace_back(*m_data[m_data.size() - 1]);
	SetItemCount(m_data.size());
}

int wxTitleManagerList::AddImage(const wxImage& image) const
{
	return -1; // m_image_list->Add(image.Scale(kListIconWidth, kListIconWidth, wxIMAGE_QUALITY_BICUBIC));
}

// return <
bool wxTitleManagerList::SortFunc(int column, const Type_t& v1, const Type_t& v2)
{
	// last sort option
	if (column == -1)
		return v1.get().entry.path.compare(v2.get().entry.path) < 0;

	// visible have always priority
	if (!v1.get().visible && v2.get().visible)
		return false;
	else if (v1.get().visible && !v2.get().visible)
		return true;

	const auto& entry1 = v1.get().entry;
	const auto& entry2 = v2.get().entry;
	
	// check column: title id -> type -> path
	if (column == ColumnTitleId)
	{
		// ensure strong ordering -> use type since only one entry should be now (should be changed if every save for every user is displayed spearately?)
		if (entry1.title_id == entry2.title_id)
			return SortFunc(ColumnType, v1, v2);
		
		return entry1.title_id < entry2.title_id;
	}
	else if (column == ColumnName)
	{
		const int tmp = entry1.name.CmpNoCase(entry2.name);
		if(tmp == 0)
			return SortFunc(ColumnTitleId, v1, v2);
			
		return tmp < 0;
	}
	else if (column == ColumnType)
	{
		if(std::underlying_type_t<EntryType>(entry1.type) == std::underlying_type_t<EntryType>(entry2.type))
			return SortFunc(-1, v1, v2);
		
		return std::underlying_type_t<EntryType>(entry1.type) < std::underlying_type_t<EntryType>(entry2.type);
	}
	else if (column == ColumnVersion)
	{
		if(entry1.version == entry2.version)
			return SortFunc(ColumnTitleId, v1, v2);
		
		return std::underlying_type_t<EntryType>(entry1.version) < std::underlying_type_t<EntryType>(entry2.version);
	}
	else if (column == ColumnRegion)
	{
		if(entry1.region == entry2.region)
			return SortFunc(ColumnTitleId, v1, v2);
		
		return std::underlying_type_t<EntryType>(entry1.region) < std::underlying_type_t<EntryType>(entry2.region);
	}
		
	return false;
}
void wxTitleManagerList::SortEntries(int column)
{
	if(column == -1)
	{
		column = m_last_column_sorted;
		m_last_column_sorted = -1;
		if (column == -1)
			column = ColumnTitleId;
	}
	
	if (column != ColumnTitleId && column != ColumnName && column != ColumnType && column != ColumnVersion && column != ColumnRegion && column != ColumnFormat)
		return;

	if (m_last_column_sorted != column)
	{
		m_last_column_sorted = column;
		m_sort_less = true;
	}
	else
		m_sort_less = !m_sort_less;
		
	std::sort(m_sorted_data.begin(), m_sorted_data.end(),
		[this, column](const Type_t& v1, const Type_t& v2) -> bool
		{
			const bool result = SortFunc(column, v1, v2);
			return m_sort_less ? result : !result;
		});
	
	RefreshPage();
}

void wxTitleManagerList::RefreshPage()
{
	long item_count = GetItemCount();

	if (item_count > 0)
		RefreshItems(GetTopItem(), std::min(item_count - 1, GetTopItem() + GetCountPerPage() + 1));
}

int wxTitleManagerList::Filter(const wxString& filter, const wxString& prefix, ItemColumn column)
{
	if (prefix.empty())
		return -1;
	
	if (!filter.StartsWith(prefix))
		return -1;

	int counter = 0;
	const auto tmp_filter = filter.substr(prefix.size() - 1).Trim(false);
	for (auto&& data : m_data)
	{
		if (GetTitleEntryText(data->entry, column).Upper().Contains(tmp_filter))
		{
			data->visible = true;
			++counter;
		}
		else
			data->visible = false;
	}
	return counter;
}

void wxTitleManagerList::Filter(const wxString& filter)
{
	if(filter.empty())
	{
		std::for_each(m_data.begin(), m_data.end(), [](ItemDataPtr& data) { data->visible = true; });
		SetItemCount(m_data.size());
		RefreshPage();
		return;
	}

	const auto filter_upper = filter.Upper().Trim(false).Trim(true);
	int counter = 0;
	
	if (const auto result = Filter(filter_upper, "TITLEID:", ColumnTitleId) != -1)
		counter = result;
	else if (const auto result = Filter(filter_upper, "NAME:", ColumnName) != -1)
		counter = result;
	else if (const auto result = Filter(filter_upper, "TYPE:", ColumnType) != -1)
		counter = result;
	else if (const auto result = Filter(filter_upper, "REGION:", ColumnRegion) != -1)
		counter = result;
	else if (const auto result = Filter(filter_upper, "VERSION:", ColumnVersion) != -1)
		counter = result;
	else if (const auto result = Filter(filter_upper, "FORMAT:", ColumnFormat) != -1)
		counter = result;
	else if(filter_upper == "ERROR")
	{
		for (auto&& data : m_data)
		{
			bool visible = true;
			data->visible = visible;
			if (visible)
				++counter;
		}
	}
	else
	{
		for (auto&& data : m_data)
		{
			bool visible = false;
			if (data->entry.name.Upper().Contains(filter_upper))
				visible = true;
			else if (GetTitleEntryText(data->entry, ColumnTitleId).Upper().Contains(filter_upper))
				visible = true;
			else if (GetTitleEntryText(data->entry, ColumnType).Upper().Contains(filter_upper))
				visible = true;

			data->visible = visible;
			if (visible)
				++counter;
		}
	}
	
	SetItemCount(counter);
	RefreshPage();
}

size_t wxTitleManagerList::GetCountByType(EntryType type) const
{
	size_t result = 0;
	for(const auto& data : m_data)
	{
		if (data->entry.type == type)
			++result;
	}
	return result;
}

void wxTitleManagerList::ClearItems()
{
	m_sorted_data.clear();
	m_data.clear();
	SetItemCount(0);
	RefreshPage();
}

void wxTitleManagerList::AutosizeColumns()
{
	wxAutosizeColumns(this, ColumnTitleId, ColumnMAX - 1);
}