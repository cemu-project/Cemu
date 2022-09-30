#include "CemuLogging.h"
#include "config/CemuConfig.h"
#include "gui/LoggingWindow.h"
#include "config/ActiveSettings.h"
#include "util/helpers/helpers.h"

#include <mutex>
#include <condition_variable>
#include <chrono>

#include <fmt/printf.h>

struct _LogContext
{
	std::condition_variable_any log_condition;
	std::recursive_mutex log_mutex;
	std::ofstream file_stream;
	std::vector<std::string> text_cache;
	std::thread log_writer;
	std::atomic<bool> threadRunning = false;

	~_LogContext()
	{
		// safely shut down the log writer thread
		threadRunning.store(false);
		if (log_writer.joinable())
		{
			log_condition.notify_one();
			log_writer.join();
		}
	}
}LogContext;

const std::map<LogType, std::string> g_logging_window_mapping
{
	{LogType::File, "Coreinit File-Access"},
	{LogType::GX2, "GX2"},
	{LogType::ThreadSync, "Coreinit Thread-Synchronization"},
	{LogType::SoundAPI, "Audio"},
	{LogType::Input, "Input"},
	{LogType::Socket, "Socket"},
	{LogType::Save, "Save"},
	{LogType::CoreinitMem, "Coreinit Memory"},
	{LogType::H264, "H264"},
	{LogType::OpenGL, "OpenGL"},
	{LogType::TextureCache, "Texture Cache"},
	{LogType::nn_nfp, "NFP"},
};

uint64 cemuLog_getFlag(LogType type)
{
	return type <= LogType::Force ? 0 : (1ULL << ((uint64)type - 1));
}

bool cemuLog_advancedPPCLoggingEnabled()
{
	return GetConfig().advanced_ppc_logging;
}

bool cemuLog_isLoggingEnabled(LogType type)
{
	if (type == LogType::Placeholder)
		return false;

	if (type == LogType::None)
		return false;
	
	return (type == LogType::Force)	|| ((GetConfig().log_flag.GetValue() & cemuLog_getFlag(type)) != 0);
}

void cemuLog_thread()
{
	SetThreadName("cemuLog_thread");
	while (true)
	{
		std::unique_lock lock(LogContext.log_mutex);
		while (LogContext.text_cache.empty())
		{
			LogContext.log_condition.wait(lock);
			if (!LogContext.threadRunning.load(std::memory_order::relaxed))
				return;
		}

		std::vector<std::string> cache_copy;
		cache_copy.swap(LogContext.text_cache); // fast copy & clear
		lock.unlock();

		for (const auto& entry : cache_copy)
			LogContext.file_stream.write(entry.data(), entry.size());

		LogContext.file_stream.flush();
	}
}

void cemuLog_createLogFile(bool triggeredByCrash)
{
	std::unique_lock lock(LogContext.log_mutex);
	if (LogContext.file_stream.is_open())
		return;

	const auto path = ActiveSettings::GetUserDataPath("log.txt");
	LogContext.file_stream.open(path, std::ios::out);
	if (LogContext.file_stream.fail())
	{
		cemu_assert_debug(false);
		return;
	}

	LogContext.threadRunning.store(true);
	LogContext.log_writer = std::thread(cemuLog_thread);
	lock.unlock();
}

void cemuLog_writeLineToLog(std::string_view text, bool date, bool new_line)
{
	std::unique_lock lock(LogContext.log_mutex);

	if (date)
	{
		const auto now = std::chrono::system_clock::now();
		const auto temp_time = std::chrono::system_clock::to_time_t(now);
		const auto& time = *std::localtime(&temp_time);

		auto time_str = fmt::format("[{:02d}:{:02d}:{:02d}.{:03d}] ", time.tm_hour, time.tm_min, time.tm_sec,
			std::chrono::duration_cast<std::chrono::milliseconds>(now - std::chrono::time_point_cast<std::chrono::seconds>(now)).count());

		LogContext.text_cache.emplace_back(std::move(time_str));
	}

	LogContext.text_cache.emplace_back(text);

	if (new_line)
		LogContext.text_cache.emplace_back("\n");

	lock.unlock();
	LogContext.log_condition.notify_one();
}

bool cemuLog_log(LogType type, std::string_view text)
{
	if (!cemuLog_isLoggingEnabled(type))
		return false;

	cemuLog_writeLineToLog(text);

	const auto it = std::find_if(g_logging_window_mapping.cbegin(), g_logging_window_mapping.cend(),
		[type](const auto& entry) { return entry.first == type; });
	if (it == g_logging_window_mapping.cend())
		LoggingWindow::Log(text);
	else
		LoggingWindow::Log(it->second, text);

	return true;
}

bool cemuLog_log(LogType type, std::u8string_view text)
{
	std::basic_string_view<char> s((char*)text.data(), text.size());	
	return cemuLog_log(type, s);
}

bool cemuLog_log(LogType type, std::wstring_view text)
{
	if (!cemuLog_isLoggingEnabled(type))
		return false;

	return cemuLog_log(type, boost::nowide::narrow(text.data(), text.size()));
}

void cemuLog_waitForFlush()
{
	cemuLog_createLogFile(false);
	std::unique_lock lock(LogContext.log_mutex);
	while(!LogContext.text_cache.empty())
	{
		lock.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		std::this_thread::yield();
		lock.lock();
	}
	
}

// used to atomically write multiple lines to the log
std::unique_lock<decltype(LogContext.log_mutex)> cafeLog_acquire()
{
	return std::unique_lock(LogContext.log_mutex);
}

void cafeLog_log(uint32 type, const char* format, ...)
{
	const auto logType = (LogType)type;
	if (!cemuLog_isLoggingEnabled(logType))
		return;
	
	char logTempStr[2048];
	va_list(args);
	va_start(args, format);
#if BOOST_OS_WINDOWS
	vsprintf_s(logTempStr, format, args);
#else
	vsprintf(logTempStr, format, args);
#endif
	va_end(args);

	cemuLog_writeLineToLog(logTempStr);
	
	const auto it = std::find_if(g_logging_window_mapping.cbegin(), g_logging_window_mapping.cend(),
		[logType](const auto& entry) { return entry.first == logType; });
	if (it == g_logging_window_mapping.cend())
		LoggingWindow::Log(logTempStr);
	else
		LoggingWindow::Log(it->second, logTempStr);
}

void cafeLog_logW(uint32 type, const wchar_t* format, ...)
{
	const auto logType = (LogType)type;
	if (!cemuLog_isLoggingEnabled(logType))
		return;
	
	wchar_t logTempStr[2048];
	va_list(args);
	va_start(args, format);
#if BOOST_OS_WINDOWS
	vswprintf_s(logTempStr, format, args);
#else
	vswprintf(logTempStr, 2048, format, args);
#endif
	va_end(args);
	
	cemuLog_log(logType, logTempStr);

	const auto it = std::find_if(g_logging_window_mapping.cbegin(), g_logging_window_mapping.cend(),
		[logType](const auto& entry) { return entry.first == logType; });
	if (it == g_logging_window_mapping.cend())
		LoggingWindow::Log(logTempStr);
	else
		LoggingWindow::Log(it->second, logTempStr);
}

void cemuLog_log()
{
	typedef void(*VoidFunc)();
	const VoidFunc func = (VoidFunc)cafeLog_log;
	func();
}

void cafeLog_logLine(uint32 type, const char* line)
{
	if (!cemuLog_isLoggingEnabled((LogType)type))
		return;
	
	cemuLog_writeLineToLog(line);
}

void cafeLog_setLoggingFlagEnable(sint32 loggingType, bool isEnable)
{
	if (isEnable)
		GetConfig().log_flag = GetConfig().log_flag.GetValue() | cemuLog_getFlag((LogType)loggingType);
	else
		GetConfig().log_flag = GetConfig().log_flag.GetValue() & ~cemuLog_getFlag((LogType)loggingType);
	
	g_config.Save();
}

bool cafeLog_isLoggingFlagEnabled(sint32 loggingType)
{
	return cemuLog_isLoggingEnabled((LogType)loggingType);
}
