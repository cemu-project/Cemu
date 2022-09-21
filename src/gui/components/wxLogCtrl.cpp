#include "gui/components/wxLogCtrl.h"

#include <boost/algorithm/string.hpp>

wxDEFINE_EVENT(EVT_ON_LIST_UPDATED, wxEvent);

wxLogCtrl::wxLogCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: TextList(parent, id, pos, size, style)
{
	m_timer = new wxTimer(this);
	this->Bind(wxEVT_TIMER, &wxLogCtrl::OnTimer, this);
	this->Bind(EVT_ON_LIST_UPDATED, &wxLogCtrl::OnActiveListUpdated, this);
	m_timer->Start(250);
}

wxLogCtrl::~wxLogCtrl()
{
	this->Unbind(wxEVT_TIMER, &wxLogCtrl::OnTimer, this);
	this->Unbind(EVT_ON_LIST_UPDATED, &wxLogCtrl::OnActiveListUpdated, this);

	if(m_timer)
		m_timer->Stop();

	if(m_update_worker.joinable())
		m_update_worker.join();
}

void wxLogCtrl::SetActiveFilter(const std::string& active_filter)
{
	if(m_active_filter == active_filter)
		return;

	m_active_filter = active_filter;

	if(m_update_worker.joinable())
		m_update_worker.join();

	m_update_worker = std::thread(&wxLogCtrl::UpdateActiveEntries, this);
}

const std::string& wxLogCtrl::GetActiveFilter() const
{
	return m_active_filter;
}

void wxLogCtrl::SetFilterMessage(bool state)
{
	if(m_filter_messages == state)
		return;

	m_filter_messages = state;

	if(m_update_worker.joinable())
		m_update_worker.join();

	m_update_worker = std::thread(&wxLogCtrl::UpdateActiveEntries, this);
}

bool wxLogCtrl::GetFilterMessage() const
{
	return m_filter_messages;
}

void wxLogCtrl::PushEntry(const wxString& filter, const wxString& message)
{
	std::unique_lock lock(m_mutex);
	m_log_entries.emplace_back(filter, message);
	ListIt_t it = m_log_entries.back();
	lock.unlock();

	if(m_active_filter.empty() || filter == m_active_filter || (m_filter_messages && boost::icontains(message.ToStdString(), m_active_filter)))
	{
		std::unique_lock active_lock(m_active_mutex);
		m_active_entries.emplace_back(std::cref(it));
		const auto entry_count = m_active_entries.size();
		active_lock.unlock();
		
		if(entry_count <= m_elements_visible)
		{
			RefreshLine(entry_count - 1);
		}
	}
}

void wxLogCtrl::OnDraw(wxDC& dc, sint32 start, sint32 count, const wxPoint& start_position)
{
	wxPoint position = start_position;

	std::scoped_lock lock(m_active_mutex);
	auto it = std::next(m_active_entries.cbegin(), start);
	if(it == m_active_entries.cend())
		return;

	for (sint32 i = 0; i <= count && it != m_active_entries.cend(); ++i, ++it)
	{
		wxColour background_colour;
		if((start + i) % 2 == 0)
			background_colour = COLOR_WHITE;
		else
			background_colour = 0xFFFDF9F2;

		DrawLineBackground(dc, position, background_colour);

		dc.SetTextForeground(COLOR_BLACK);
		dc.DrawText(it->get().second, position);

		NextLine(position, &start_position);
	}
}

void wxLogCtrl::OnTimer(wxTimerEvent& event)
{
	std::unique_lock lock(m_active_mutex);
	const auto count = m_active_entries.size();
	if(count == m_element_count)
		return;

	lock.unlock();
	SetElementCount(count);

	if(m_scrolled_to_end)
	{
		Scroll(0, count);
		RefreshControl();
	}
}

void wxLogCtrl::OnActiveListUpdated(wxEvent& event)
{
	std::unique_lock lock(m_active_mutex);
	const auto count = m_active_entries.size();
	lock.unlock();

	SetElementCount(count);
	RefreshControl();
}

void wxLogCtrl::UpdateActiveEntries()
{
	{
		std::scoped_lock lock(m_mutex, m_active_mutex);
		m_active_entries.clear();
		if(m_active_filter.empty())
		{
			for (const auto& it : m_log_entries)
				m_active_entries.emplace_back(it);
		}
		else
		{
			for (const auto& it : m_log_entries)
			{
				if(it.first == m_active_filter ||
                              (m_filter_messages && boost::icontains(it.second.ToStdString(), m_active_filter)) )
					m_active_entries.emplace_back(it);
			}
		}
	}

	wxQueueEvent(this, new wxCommandEvent(EVT_ON_LIST_UPDATED));
}

