#include "gui/wxgui.h"

#include "gui/MemorySearcherTool.h"

#include <vector>
#include <sstream>
#include <thread>

#include "config/ActiveSettings.h"
#include "gui/helpers/wxHelpers.h"
#include "Common/FileStream.h"
#include "util/IniParser/IniParser.h"
#include "util/helpers/StringHelpers.h"
#include "Cafe/CafeSystem.h"

enum
{
	COMBOBOX_DATATYPE = wxID_HIGHEST + 1,
	TEXT_VALUE,
	BUTTON_START,
	BUTTON_FILTER,
	LIST_RESULTS,
	LIST_ENTRYTABLE,
	TIMER_REFRESH,
	LIST_ENTRY_ADD,
	LIST_ENTRY_REMOVE,
};

wxDEFINE_EVENT(wxEVT_SEARCH_FINISHED, wxCommandEvent);

wxBEGIN_EVENT_TABLE(MemorySearcherTool, wxFrame)
EVT_CLOSE(MemorySearcherTool::OnClose)
EVT_BUTTON(BUTTON_START, MemorySearcherTool::OnSearch)
EVT_BUTTON(BUTTON_FILTER, MemorySearcherTool::OnFilter)
EVT_TIMER(TIMER_REFRESH, MemorySearcherTool::OnTimerTick)
wxEND_EVENT_TABLE()

constexpr auto kMaxResultCount = 5000;

const wxString kDatatypeFloat = "float";
const wxString kDatatypeDouble = "double";
const wxString kDatatypeString = "string";
const wxString kDatatypeInt8 = "int8";
const wxString kDatatypeInt16 = "int16";
const wxString kDatatypeInt32 = "int32";
const wxString kDatatypeInt64 = "int64";
const wxString kDataTypeNames[] = {kDatatypeFloat,kDatatypeDouble,/*DATATYPE_STRING,*/kDatatypeInt8,kDatatypeInt16,kDatatypeInt32,kDatatypeInt64};

MemorySearcherTool::MemorySearcherTool(wxFrame* parent)
	: wxFrame(parent, wxID_ANY, _("Memory Searcher"), wxDefaultPosition, wxSize(600, 540), wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL)
{
	this->SetSizeHints(wxDefaultSize, wxDefaultSize);
	this->wxWindowBase::SetBackgroundColour(*wxWHITE);
	this->wxTopLevelWindowBase::SetMinSize(wxSize(600, 540));

	auto* sizer = new wxBoxSizer(wxVERTICAL);

	auto* row1 = new wxFlexGridSizer(0, 4, 0, 0);
	row1->AddGrowableCol(1);

	m_cbDataType = new wxComboBox(this, COMBOBOX_DATATYPE, kDatatypeFloat, wxDefaultPosition, wxDefaultSize, std::size(kDataTypeNames), kDataTypeNames, wxCB_READONLY);
	m_textValue = new wxTextCtrl(this, TEXT_VALUE);
	m_buttonStart = new wxButton(this, BUTTON_START, _("Search"));
	m_buttonFilter = new wxButton(this, BUTTON_FILTER, _("Filter"));
	m_buttonFilter->Disable();

	row1->Add(m_cbDataType, 0, wxALL, 5);
	row1->Add(m_textValue, 0, wxALL | wxEXPAND, 5);
	row1->Add(m_buttonStart, 0, wxALL, 5);
	row1->Add(m_buttonFilter, 0, wxALL, 5);

	sizer->Add(row1, 0, wxEXPAND, 5);

	auto* row2 = new wxFlexGridSizer(0, 1, 0, 0);
	row2->AddGrowableCol(0);

	m_gauge = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL);
	m_gauge->SetValue(0);
	m_gauge->Enable(false);

	m_textEntryTable = new wxStaticText(this, wxID_ANY, _("Results"));
	m_listResults = new wxListCtrl(this, LIST_RESULTS, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SORT_ASCENDING);
	m_listResults->Bind(wxEVT_LEFT_DCLICK, &MemorySearcherTool::OnResultListClick, this);
	{
		wxListItem col0;
		col0.SetId(0);
		col0.SetText(_("Address"));
		col0.SetWidth(100);
		m_listResults->InsertColumn(0, col0);
		wxListItem col1;
		col1.SetId(1);
		col1.SetText(_("Value"));
		col1.SetWidth(250);
		m_listResults->InsertColumn(1, col1);
	}

	auto textEntryTable = new wxStaticText(this, wxID_ANY, _("Stored Entries"));
	m_listEntryTable = new wxDataViewListCtrl(this, LIST_ENTRYTABLE, wxDefaultPosition, wxSize(420, 200), wxDV_HORIZ_RULES);
	m_listEntryTable->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &MemorySearcherTool::OnEntryListRightClick, this);
	m_listEntryTable->Bind(wxEVT_COMMAND_DATAVIEW_ITEM_EDITING_DONE, &MemorySearcherTool::OnItemEdited, this);
	{
		m_listEntryTable->AppendTextColumn(_("Description"), wxDATAVIEW_CELL_EDITABLE, 150, wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE);
		m_listEntryTable->AppendTextColumn(_("Address"), wxDATAVIEW_CELL_INERT, 100, wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE);
		m_listEntryTable->AppendTextColumn(_("Type"));
		m_listEntryTable->AppendTextColumn(_("Value"), wxDATAVIEW_CELL_EDITABLE);
		m_listEntryTable->AppendToggleColumn(_("Freeze"), wxDATAVIEW_CELL_ACTIVATABLE, 50, wxALIGN_LEFT, 0);
	}

	row2->AddGrowableRow(3);
	row2->AddGrowableRow(5);

	row2->Add(m_gauge, 0, wxALL | wxEXPAND, 5);
	row2->Add(0, 10, 1, wxEXPAND, 5);
	row2->Add(m_textEntryTable, 0, wxALL, 5);
	row2->Add(m_listResults, 1, wxALL | wxEXPAND, 5);
	row2->Add(textEntryTable, 0, wxALL, 5);
	row2->Add(m_listEntryTable, 1, wxALL | wxEXPAND, 5);

	sizer->Add(row2, 1, wxEXPAND, 5);

	// load stored entries
	Load();

	this->Bind(wxEVT_SEARCH_FINISHED, &MemorySearcherTool::OnSearchFinished, this);
	this->Bind(wxEVT_SET_GAUGE_VALUE, &MemorySearcherTool::OnUpdateGauge, this);

	m_refresh_timer = new wxTimer(this, TIMER_REFRESH);
	m_refresh_timer->Start(250);
	
	this->SetSizer(sizer);
	this->wxWindowBase::Layout();

	this->Centre(wxBOTH);
}

MemorySearcherTool::~MemorySearcherTool()
{
	m_refresh_timer->Stop();
	
	m_running = false;
	if (m_worker.joinable())
		m_worker.join();
}

void MemorySearcherTool::OnTimerTick(wxTimerEvent& event)
{
	RefreshResultList();
	RefreshStashList();
}

void MemorySearcherTool::OnClose(wxCloseEvent& event)
{
	Save();
	event.Skip();
}

void MemorySearcherTool::OnSearchFinished(wxCommandEvent&)
{
	FillResultList();
	m_search_running = false;
	m_buttonStart->Enable();
	m_buttonFilter->Enable();
}

void MemorySearcherTool::OnUpdateGauge(wxSetGaugeValue& event)
{
	auto* gauge = event.GetGauge();
	const auto value = event.GetValue();
	if (event.GetRange() != 0)
	{
		gauge->SetRange(event.GetRange());
		gauge->SetValue(value);
		return;
	}
	
	debug_printf("update gauge: %d + %d = %d (/%d)\n", gauge->GetValue(), value, gauge->GetValue() + value, gauge->GetRange());
	gauge->SetValue(gauge->GetValue() + value);
}

void MemorySearcherTool::OnSearch(wxCommandEvent&)
{
	if (m_clear_state)
	{
		Reset();
		return;
	}

	if (m_search_running)
		return;

	if (m_textValue->IsEmpty())
		return;

	SetSearchDataType();

	if (!VerifySearchValue())
	{
		wxMessageBox(_("Your entered value is not valid for the selected datatype."), _("Error"), wxICON_ERROR);
		return;
	}
	m_buttonStart->Disable();
	m_buttonFilter->Disable();
	m_cbDataType->Disable();
	m_buttonStart->SetLabelText(_("Clear"));
	m_clear_state = true;

	if (m_worker.joinable())
		m_worker.join();

	m_worker = std::thread([this]()
		{
			m_search_jobs.clear();
			uint32 total_size = 0;
			for (const auto& itr : memory_getMMURanges())
			{
				if (!m_running)
					return;

				if (!itr->isMapped())
					continue;

				void* ptr = itr->getPtr();
				const uint32 size = itr->getSize();

				total_size += (size / kGaugeStep);
				m_search_jobs.emplace_back(std::async(std::launch::async, [this, ptr, size]() { return SearchValues(m_searchDataType, ptr, size); }));
			}

			wxQueueEvent(this, new wxSetGaugeValue(0, total_size, m_gauge));
			
			ListType_t tmp;
			for (auto& it : m_search_jobs)
			{
				const auto result = it.get();
				tmp.insert(tmp.end(), result.cbegin(), result.cend());
			}
		
			std::unique_lock lock(m_mutex);
			m_searchBuffer.swap(tmp);
			lock.unlock();
			
			wxQueueEvent(this, new wxCommandEvent(wxEVT_SEARCH_FINISHED));
		});
}

void MemorySearcherTool::OnFilter(wxCommandEvent& event)
{
	m_buttonStart->Disable();
	m_buttonFilter->Disable();

	if (m_worker.joinable())
		m_worker.join();

	m_worker = std::thread([this]()
		{
			const auto count = (uint32)m_searchBuffer.size();
			wxQueueEvent(this, new wxSetGaugeValue(0, count, m_gauge));
		
			auto tmp = FilterValues(m_searchDataType);

			std::unique_lock lock(m_mutex);
			m_searchBuffer.swap(tmp);
			lock.unlock();

			wxQueueEvent(this, new wxCommandEvent(wxEVT_SEARCH_FINISHED));
		});

	m_gauge->SetValue(0);
}

void MemorySearcherTool::Load()
{
	const auto memorySearcherPath = ActiveSettings::GetUserDataPath("memorySearcher/{:016x}.ini", CafeSystem::GetForegroundTitleId());
	auto memSearcherIniContents = FileStream::LoadIntoMemory(memorySearcherPath);
	if (!memSearcherIniContents)
		return;

	IniParser iniParser(*memSearcherIniContents, _pathToUtf8(memorySearcherPath));
	while (iniParser.NextSection())
	{
		auto option_description = iniParser.FindOption("description");
		auto option_address = iniParser.FindOption("address");
		auto option_type = iniParser.FindOption("type");
		auto option_value = iniParser.FindOption("value");

		if (!option_description || !option_address || !option_type || !option_value)
			continue;

		try
		{
			const auto addr = StringHelpers::ToInt64(*option_address);
			if (!IsAddressValid(addr))
				continue;
		}
		catch (const std::invalid_argument&)
		{
			continue;
		}

		bool found = false;
		for (const auto& entry : kDataTypeNames)
		{
			if (boost::iequals(entry.ToStdString(), *option_type))
			{
				found = true;
				break;
			}
		}

		if (!found && !boost::iequals(kDatatypeString.ToStdString(), *option_type))
			continue;

		wxVector<wxVariant> data;
		data.push_back(std::string(*option_description).c_str());
		data.push_back(std::string(*option_address).c_str());
		data.push_back(std::string(*option_type).c_str());
		data.push_back(std::string(*option_value).c_str());
		data.push_back(!option_value->empty());
		m_listEntryTable->AppendItem(data);
	}
}

void MemorySearcherTool::Save()
{
	const auto memorySearcherPath = ActiveSettings::GetUserDataPath("memorySearcher/{:016x}.ini", CafeSystem::GetForegroundTitleId());
	FileStream* fs = FileStream::createFile2(memorySearcherPath);
	if (fs)
	{
		for (int i = 0; i < m_listEntryTable->GetItemCount(); ++i)
		{
			fs->writeLine("[Entry]");

			std::string tmp = "description=" + std::string(m_listEntryTable->GetTextValue(i, 0).mbc_str());
			fs->writeLine(tmp.c_str());

			tmp = "address=" + std::string(m_listEntryTable->GetTextValue(i, 1).mbc_str());
			fs->writeLine(tmp.c_str());

			tmp = "type=" + std::string(m_listEntryTable->GetTextValue(i, 2).mbc_str());
			fs->writeLine(tmp.c_str());

			// only save value when FREEZE is active
			if (m_listEntryTable->GetToggleValue(i, 4))
			{
				tmp = "value=" + std::string(m_listEntryTable->GetTextValue(i, 3).mbc_str());
			}
			else
			{
				tmp = "value=";
			}
			fs->writeLine(tmp.c_str());

			fs->writeLine("");
		}
		delete fs;
	}
}

bool MemorySearcherTool::IsAddressValid(uint32 addr)
{
	for (const auto& itr : memory_getMMURanges())
	{
		if (!itr->isMapped())
			continue;

		void* ptr = itr->getPtr();
		const uint32 size = itr->getSize();

		MEMPTR start((uint8*)ptr);
		MEMPTR end((uint8*)ptr + size);
		if (start.GetMPTR() <= addr && addr < end.GetMPTR())
			return true;
	}

	return false;
}

void MemorySearcherTool::OnEntryListRightClick(wxDataViewEvent& event)
{
	//void *data = reinterpret_cast<void *>(event.GetItem().GetData());
	wxMenu mnu;
	//mnu.SetClientData(data);
	mnu.Append(LIST_ENTRY_ADD, _("&Add new entry"))->Enable(false);
	mnu.Append(LIST_ENTRY_REMOVE, _("&Remove entry"));
	mnu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MemorySearcherTool::OnPopupClick), nullptr, this);
	PopupMenu(&mnu);
}

void MemorySearcherTool::OnResultListClick(wxMouseEvent& event)
{
	long selectedIndex = -1;

	while (true)
	{
		selectedIndex = m_listResults->GetNextItem(selectedIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (selectedIndex == -1)
			break;

		long address = m_listResults->GetItemData(selectedIndex);
		auto currValue = m_listResults->GetItemText(selectedIndex, 1);

		char addressString[256];
		sprintf(addressString, "0x%08lx", address);

		// description, address, type, value, freeze
		wxVector<wxVariant> data;
		data.push_back("");
		data.push_back(addressString);
		data.push_back(m_cbDataType->GetValue());
		data.push_back(currValue);
		data.push_back(false);
		m_listEntryTable->AppendItem(data);
	}
}

void MemorySearcherTool::Reset()
{
	m_searchBuffer.clear();
	m_buttonStart->SetLabelText(_("Search"));
	m_textEntryTable->SetLabelText(_("Results"));
	m_buttonFilter->Disable();
	m_cbDataType->Enable();
	m_textValue->SetValue("");
	m_listResults->DeleteAllItems();
	m_clear_state = false;
}

bool MemorySearcherTool::VerifySearchValue() const
{
	const auto s1 = m_textValue->GetValue();
	const auto s2 = s1.c_str();
	const auto inputString = s2.AsChar();

	switch (m_searchDataType)
	{
	case SearchDataType_String:
		return true;
	case SearchDataType_Float:
		{
			float value;
			return ConvertStringToType(inputString, value);
		}
	case SearchDataType_Double:
		{
			double value;
			return ConvertStringToType(inputString, value);
		}
	case SearchDataType_Int8:
		{
			sint8 value;
			return ConvertStringToType(inputString, value);
		}
	case SearchDataType_Int16:
		{
			sint16 value;
			return ConvertStringToType(inputString, value);
		}
	case SearchDataType_Int32:
		{
			sint32 value;
			return ConvertStringToType(inputString, value);
		}
	case SearchDataType_Int64:
		{
			sint64 value;
			return ConvertStringToType(inputString, value);
		}
	default:
		return false;
	}
}

void MemorySearcherTool::FillResultList()
{
	auto text = formatWxString(_("Results ({0})"), m_searchBuffer.size());
	m_textEntryTable->SetLabelText(text);

	m_listResults->DeleteAllItems();

	if (m_searchBuffer.empty() || m_searchBuffer.size() > kMaxResultCount)
		return;

	for (const auto& address : m_searchBuffer)
	{
		const auto index = m_listResults->InsertItem(0, fmt::format("{:#08x}", address.GetMPTR()));
		m_listResults->SetItemData(index, address.GetMPTR());
		m_listResults->SetItem(index, 1, m_textValue->GetValue());
	}
}

void MemorySearcherTool::RefreshResultList()
{
	std::unique_lock lock(m_mutex);
	if (m_searchBuffer.empty() || m_searchBuffer.size() > kMaxResultCount)
		return;

	for (int i = 0; i < m_listResults->GetItemCount(); ++i)
	{
		const auto addr = m_listResults->GetItemData(i);
		switch (m_searchDataType)
		{
		case SearchDataType_String:
			{
				// TODO Peter
				break;
			}
		case SearchDataType_Float:
			{
			const auto value = memory_read<float>(addr);
				m_listResults->SetItem(i, 1, fmt::format("{}", value));
				break;
			}
		case SearchDataType_Double:
			{
			const auto value = memory_read<double>(addr);
				m_listResults->SetItem(i, 1, fmt::format("{}", value));
				break;
			}
		case SearchDataType_Int8:
			{
			const auto value = memory_read<sint8>(addr);
				m_listResults->SetItem(i, 1, fmt::format("{}", value));
				break;
			}
		case SearchDataType_Int16:
			{
			const auto value = memory_read<sint16>(addr);
				m_listResults->SetItem(i, 1, fmt::format("{}", value));
				break;
			}
		case SearchDataType_Int32:
			{
				const auto value = memory_read<sint32>(addr);
				m_listResults->SetItem(i, 1, fmt::format("{}", value));
				break;
			}
		case SearchDataType_Int64:
			{
			const auto value = memory_read<sint64>(addr);
				m_listResults->SetItem(i, 1, fmt::format("{}", value));
				break;
			}
		}
	}
}

void MemorySearcherTool::RefreshStashList()
{
	for (int i = 0; i < m_listEntryTable->GetItemCount(); ++i)
	{
		auto freeze = m_listEntryTable->GetToggleValue(i, 4);

		auto addressText = std::string(m_listEntryTable->GetTextValue(i, 1).mbc_str());
		auto type = std::string(m_listEntryTable->GetTextValue(i, 2).mbc_str());

		auto address = stol(addressText, nullptr, 16);

		// if freeze is activated, set the value instead of refreshing it
		if (freeze)
		{
			wxVariant var;
			m_listEntryTable->GetValue(var, i, 3);
			if (type == kDatatypeFloat)
			{
				auto value = (float)var.GetDouble();
				memory_writeFloat(address, value);
			}
			else if (type == kDatatypeDouble)
			{
				auto value = var.GetDouble();
				memory_writeDouble(address, value);
			}
			else if (type == kDatatypeInt8)
			{
				auto value = var.GetInteger();
				memory_writeU8(address, value);
			}
			else if (type == kDatatypeInt16)
			{
				auto value = var.GetInteger();
				memory_writeU16(address, value);
			}
			else if (type == kDatatypeInt32)
			{
				auto value = var.GetInteger();
				memory_writeU32(address, value);
			}
			else if (type == kDatatypeInt64)
			{
				auto valueText = std::string(var.GetString().mbc_str());
				auto value = stoull(valueText);
				memory_writeU64(address, value);
			}
			else if (type == kDatatypeString)
			{
				auto valueText = std::string(var.GetString().mbc_str());
				for (int i = 0; i < valueText.size(); ++i)
				{
					memory_writeU8(address + i, valueText[i]);
				}
				memory_writeU8(address + valueText.size(), 0x00);
			}
		}
		else
		{
			if (type == kDatatypeFloat)
			{
				auto value = memory_readFloat(address);
				m_listEntryTable->SetValue(fmt::format("{}", value), i, 3);
			}
			else if (type == kDatatypeDouble)
			{
				auto value = memory_readDouble(address);
				m_listEntryTable->SetValue(fmt::format("{}", value), i, 3);
			}
			else if (type == kDatatypeInt8)
			{
				auto value = (sint8)memory_readU8(address);
				m_listEntryTable->SetValue(fmt::format("{}", (int)value), i, 3);
			}
			else if (type == kDatatypeInt16)
			{
				auto value = (sint16)memory_readU16(address);
				m_listEntryTable->SetValue(fmt::format("{}", value), i, 3);
			}
			else if (type == kDatatypeInt32)
			{
				auto value = (sint32)memory_readU32(address);
				m_listEntryTable->SetValue(fmt::format("{}", value), i, 3);
			}
			else if (type == kDatatypeInt64)
			{
				auto value = (sint64)memory_readU64(address);
				m_listEntryTable->SetValue(fmt::format("{}", value), i, 3);
			}
			else if (type == kDatatypeString)
			{
				// TODO Peter
			}
		}
	}
}

void MemorySearcherTool::SetSearchDataType()
{
	const auto type = m_cbDataType->GetStringSelection();
	if (type == kDatatypeFloat)
		m_searchDataType = SearchDataType_Float;
	else if (type == kDatatypeDouble)
		m_searchDataType = SearchDataType_Double;
	else if (type == kDatatypeInt8)
		m_searchDataType = SearchDataType_Int8;
	else if (type == kDatatypeInt16)
		m_searchDataType = SearchDataType_Int16;
	else if (type == kDatatypeInt32)
		m_searchDataType = SearchDataType_Int32;
	else if (type == kDatatypeInt64)
		m_searchDataType = SearchDataType_Int64;
	else if (type == kDatatypeString)
		m_searchDataType = SearchDataType_String;
	else
		m_searchDataType = SearchDataType_None;
}

template <>
bool MemorySearcherTool::ConvertStringToType<signed char>(const char* inValue, sint8& outValue) const
{
	sint16 tmp;
	std::istringstream iss(inValue);
	iss >> std::noskipws >> tmp;
	if (iss && iss.eof())
	{
		if (SCHAR_MIN <= tmp && tmp <= SCHAR_MAX)
		{
			outValue = tmp;
			return true;
		}
	}
	return false;
}

void MemorySearcherTool::OnPopupClick(wxCommandEvent& event)
{
	if (event.GetId() == LIST_ENTRY_REMOVE)
	{
		const int row = m_listEntryTable->GetSelectedRow();
		if (row == -1)
			return;
		
		m_listEntryTable->DeleteItem(row);
	}
}

void MemorySearcherTool::OnItemEdited(wxDataViewEvent& event)
{
	auto column = event.GetColumn();
	// Edit description
	if (column == 0) { }
		// Edit value
	else if (column == 3)
	{
		auto row = m_listEntryTable->GetSelectedRow();
		if (row == -1)
			return;

		auto addressText = std::string(m_listEntryTable->GetTextValue(row, 1).mbc_str());
		uint32 address = stoul(addressText, nullptr, 16);

		auto type = m_listEntryTable->GetTextValue(row, 2);
		if (type == kDatatypeFloat)
		{
			auto value = (float)event.GetValue().GetDouble();
			memory_writeFloat(address, value);
		}
		else if (type == kDatatypeDouble)
		{
			auto value = event.GetValue().GetDouble();
			memory_writeDouble(address, value);
		}
		else if (type == kDatatypeInt8)
		{
			auto value = event.GetValue().GetInteger();
			memory_writeU8(address, value);
		}
		else if (type == kDatatypeInt16)
		{
			auto value = event.GetValue().GetInteger();
			memory_writeU16(address, value);
		}
		else if (type == kDatatypeInt32)
		{
			auto value = event.GetValue().GetInteger();
			memory_writeU32(address, value);
		}
		else if (type == kDatatypeInt64)
		{
			auto valueText = std::string(event.GetValue().GetString().mbc_str());
			auto value = stoull(valueText);
			memory_writeU64(address, value);
		}
		else if (type == kDatatypeString)
		{
			auto valueText = std::string(event.GetValue().GetString().mbc_str());
			for (int i = 0; i < valueText.size(); ++i)
			{
				memory_writeU8(address + i, valueText[i]);
			}
			memory_writeU8(address + valueText.size(), 0x00);
		}
	}
}
