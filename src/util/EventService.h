#pragma once

#include "util/helpers/Singleton.h"

#include <boost/signals2.hpp>
#include <boost/bind/placeholders.hpp>

enum class Events : int32_t
{
	ControllerChanged,
};

using ControllerChangedFunc = void(void);

class EventService : public Singleton<EventService>
{
	friend class Singleton<EventService>;
	EventService() = default;

public:
	template <Events event, typename TFunc, typename TClass>
	boost::signals2::connection connect(TFunc function, TClass thisptr)
	{
		using namespace boost::placeholders;
		if constexpr (event == Events::ControllerChanged)
			return m_controller_changed_signal.connect(boost::bind(function, thisptr));
		else
		{
			cemu_assert_suspicious();
		}
	}

	template <Events event>
	void disconnect(const boost::signals2::connection& slot)
	{
		using namespace boost::placeholders;
		if constexpr (event == Events::ControllerChanged)
			m_controller_changed_signal.disconnect(slot);
		else
		{
			cemu_assert_suspicious();
		}
	}

	template <Events event, typename ... TArgs>
	void signal(TArgs&&... args)
	{
		try
		{
			if constexpr (event == Events::ControllerChanged)
				m_controller_changed_signal(std::forward<TArgs>(args)...);
			else
			{
				cemu_assert_suspicious();
			}
		}
		catch (const std::exception& ex)
		{
			cemuLog_log(LogType::Force, "error when signaling {}: {}", event, ex.what());
		}
	}

private:
	boost::signals2::signal<ControllerChangedFunc> m_controller_changed_signal;

};
