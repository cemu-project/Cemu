#pragma once

#include <mutex>
#include <thread>
#include <atomic>

#include "Cafe/HW/MMU/MMU.h"
#include "util/helpers/helpers.h"
#include "gui/helpers/wxCustomEvents.h"

enum SearchDataType
{
	SearchDataType_None,
	SearchDataType_String,
	SearchDataType_Float,
	SearchDataType_Double,
	SearchDataType_Int8,
	SearchDataType_Int16,
	SearchDataType_Int32,
	SearchDataType_Int64,
};

struct TableEntry_t
{
	uint32 address;
	const char description[32];
	SearchDataType type;
	bool freeze;
	uint16 dataLen;
	uint8* data;
};

class MemorySearcherTool : public wxFrame
{
public:
	MemorySearcherTool(wxFrame* parent);
	virtual ~MemorySearcherTool();

	void OnTimerTick(wxTimerEvent& event);
	void OnClose(wxCloseEvent&);
	void OnSearch(wxCommandEvent&);
	void OnSearchFinished(wxCommandEvent& event);
	void OnUpdateGauge(wxSetGaugeValue& event);
	void OnFilter(wxCommandEvent& event);
	void OnResultListClick(wxMouseEvent& event);
	void OnEntryListRightClick(wxDataViewEvent& event);
	//void OnEntryListRightClick2(wxContextMenuEvent& event);
	void OnPopupClick(wxCommandEvent& event);

	void OnItemEdited(wxDataViewEvent& event);

private:
	void Reset();
	bool VerifySearchValue() const;
	void FillResultList();
	void RefreshResultList();
	void RefreshStashList();
	void SetSearchDataType();

	void Load();
	void Save();

	static bool IsAddressValid(uint32 addr);

	template <typename T>
	bool ConvertStringToType(const char* inValue, T& outValue) const
	{
		std::istringstream iss(inValue);
		iss >> std::noskipws >> outValue;
		return iss && iss.eof();
	}
	
	using ListType_t = std::vector<MEMPTR<void>>;
	ListType_t SearchValues(SearchDataType type, void* ptr, uint32 size)
	{
		/*if (type == SearchDataType_String)
			return SearchValues((const char* )ptr, size);
		else */if (type == SearchDataType_Float)
			return SearchValues((float*)ptr, size);
		else if (type == SearchDataType_Double)
			return SearchValues((double*)ptr, size);
		else if (type == SearchDataType_Int8)
			return SearchValues((sint8*)ptr, size);
		else if (type == SearchDataType_Int16)
			return SearchValues((sint16*)ptr, size);
		else if (type == SearchDataType_Int32)
			return SearchValues((sint32*)ptr, size);
		else if (type == SearchDataType_Int64)
			return SearchValues((sint64*)ptr, size);
		return {};
	}
	
	ListType_t FilterValues(SearchDataType type)
	{
		/*if (type == SearchDataType_String)
			return FilterValues<const char*>();
		else */if (type == SearchDataType_Float)
			return FilterValues<float>();
		else if (type == SearchDataType_Double)
			return FilterValues<double>();
		else if (type == SearchDataType_Int8)
			return FilterValues<sint8>();
		else if (type == SearchDataType_Int16)
			return FilterValues<sint16>();
		else if (type == SearchDataType_Int32)
			return FilterValues<sint32>();
		else if (type == SearchDataType_Int64)
			return FilterValues<sint64>();
		return {};
	}

	constexpr static int kGaugeStep = 0x10000;

	template <typename T>
	ListType_t SearchValues(T* ptr, uint32 size)
	{
		const auto value = m_textValue->GetValue();
		const auto* string_value = value.c_str().AsChar();
		const auto search_value = ConvertString<T>(string_value);

		const auto* end = (T*)((uint8*)ptr + size - sizeof(T));

		uint32 counter = 0;
		std::vector<MEMPTR<void>> result;
		for (; ptr < end; ++ptr)
		{
			if (!m_running)
				return result;

			const auto tmp = (betype<T>*)ptr;
			if (equals(search_value, tmp->value()))
				result.emplace_back(ptr);

			counter += sizeof(T);
			if(counter >= kGaugeStep)
			{
				wxQueueEvent(this, new wxSetGaugeValue(1, m_gauge));
				counter -= kGaugeStep;
			}
		}

		return result;
	}

	template <typename T>
	ListType_t FilterValues()
	{
		const auto value = m_textValue->GetValue();
		const auto* string_value = value.c_str().AsChar();
		const auto search_value = ConvertString<T>(string_value);

		ListType_t newSearchBuffer;
		newSearchBuffer.reserve(m_searchBuffer.size());

		for (const auto& it : m_searchBuffer)
		{
			if (!m_running)
				return newSearchBuffer;

			const auto tmp = (betype<T>*)it.GetPtr();
			if (equals(search_value , tmp->value()))
				newSearchBuffer.emplace_back(it);
			
			wxQueueEvent(this, new wxSetGaugeValue(1, m_gauge));
		}

		return newSearchBuffer;
	}

wxDECLARE_EVENT_TABLE();

	SearchDataType m_searchDataType = SearchDataType_None;
	wxComboBox* m_cbDataType;
	wxTextCtrl* m_textValue;
	wxButton *m_buttonStart, *m_buttonFilter;
	wxListCtrl* m_listResults;
	wxDataViewListCtrl* m_listEntryTable;
	wxStaticText* m_textEntryTable;
	wxGauge* m_gauge;
	wxTimer* m_refresh_timer;

	ListType_t m_searchBuffer;
	std::vector<TableEntry_t> m_tableEntries;
	
	std::mutex m_mutex;
	std::thread m_worker;

	std::atomic_bool m_running = true;
	std::atomic_bool m_search_running = false;
	std::vector<std::future<ListType_t>> m_search_jobs;

	bool m_clear_state = false;
};

template <>
bool MemorySearcherTool::ConvertStringToType(const char* inValue, sint8& outValue) const;



//
//template <typename T>
//void MemorySearcherTool::FilterValues()
//{
//	auto s1 = m_textValue->GetValue();
//	auto s2 = s1.c_str();
//	auto stringValue = s2.AsChar();
//
//	T filterValue;
//	ConvertStringToType(stringValue, filterValue);
//
//	std::vector<uint32> newSearchBuffer;
//	newSearchBuffer.reserve(m_searchBuffer.size());
//
//	uint32 gaugeValue = 0;
//	uint32 count = m_searchBuffer.size();
//	uint32 counter = 0;
//
//	for (auto it = m_searchBuffer.begin(); it != m_searchBuffer.end(); ++it)
//	{
//		if (m_running)
//			return;
//
//		T value = memory_read<T>(*it);
//		if constexpr (std::is_same<T, float>::value)
//		{
//			//float value1 = filterValue * 0.95f;
//			float diff = filterValue - value;
//			diff = fabs(diff);
//			if(diff < value*0.05f)
//			{
//				newSearchBuffer.push_back(*it);
//			}
//
//		}
//		else
//		{
//			if (value == filterValue)
//			{
//				newSearchBuffer.push_back(*it);
//			}
//		}
//
//		++counter;
//		uint32 newValue = counter * 100 / count;
//		if (newValue > gaugeValue + GAUGE_STEP_SIZE)
//		{
//			gaugeValue = newValue;
//			m_gaugeValue = newValue;
//		}
//	}
//
//	m_mutex.lock();
//	m_searchBuffer = newSearchBuffer;
//	m_gaugeValue = 100;
//	m_mutex.unlock();
//	m_isSearchFinished = true;
//}
//

