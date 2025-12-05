#include "CemuLogging.h"
#include "Common/precompiled.h"
#include "util/helpers/helpers.h"
#include "config/CemuConfig.h"
#include "config/ActiveSettings.h"
#include "config/LaunchSettings.h"

#include <mutex>
#include <condition_variable>
#include <chrono>

#include <fmt/printf.h>

uint64 s_loggingFlagMask = cemuLog_getFlag(LogType::Force);

class LoggingDispatcher
{
  private:
	static inline class DefaultLoggingCallbacks : public LoggingCallbacks
	{
	} s_defaultCallbacks;

	LoggingCallbacks* m_loggingCallbacks = &s_defaultCallbacks;
	std::shared_mutex m_mutex;

  public:
	void Log(std::string_view filter, std::string_view message)
	{
		std::shared_lock lock(m_mutex);
		m_loggingCallbacks->Log(filter, message);
	}

	void Log(std::string_view message)
	{
		Log("", message);
	}

	void Log(std::string_view filter, std::wstring_view message)
	{
		std::shared_lock lock(m_mutex);
		m_loggingCallbacks->Log(filter, message);
	}

	void Log(std::wstring_view message)
	{
		Log("", message);
	}

	void setCallbacks(LoggingCallbacks* loggingCallbacks)
	{
		std::unique_lock lock(m_mutex);
		cemu_assert_debug(m_loggingCallbacks == &s_defaultCallbacks);
		m_loggingCallbacks = loggingCallbacks;
	}

	void clearCallbacks()
	{
		std::unique_lock lock(m_mutex);
		cemu_assert_debug(m_loggingCallbacks != &s_defaultCallbacks);
		m_loggingCallbacks = &s_defaultCallbacks;
	}
} s_loggingDispatcher;

void cemuLog_setCallbacks(LoggingCallbacks* loggingCallbacks)
{
	s_loggingDispatcher.setCallbacks(loggingCallbacks);
}

void cemuLog_clearCallbacks()
{
	s_loggingDispatcher.clearCallbacks();
}

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
	{LogType::UnsupportedAPI,     "Unsupported API calls"},
	{LogType::APIErrors, 		  "Invalid API usage"},
	{LogType::CoreinitLogging,    "Coreinit Logging"},
	{LogType::CoreinitFile,       "Coreinit File-Access"},
	{LogType::CoreinitThreadSync, "Coreinit Thread-Synchronization"},
	{LogType::CoreinitMem,        "Coreinit Memory"},
	{LogType::CoreinitMP,         "Coreinit MP"},
	{LogType::CoreinitThread,     "Coreinit Thread"},
	{LogType::NN_NFP,             "nn::nfp"},
	{LogType::NN_FP,              "nn::fp"},
	{LogType::NN_BOSS,            "nn::boss"},
	{LogType::GX2,                "GX2"},
	{LogType::SoundAPI,           "Audio"},
	{LogType::InputAPI,           "Input"},
	{LogType::Socket,             "Socket"},
	{LogType::Save,               "Save"},
	{LogType::H264,               "H264"},
	{LogType::NFC,                "NFC"},
	{LogType::NTAG,               "NTAG"},
	{LogType::Patches,            "Graphic pack patches"},
	{LogType::TextureCache,       "Texture cache"},
	{LogType::TextureReadback,    "Texture readback"},
	{LogType::OpenGLLogging,      "OpenGL debug output"},
	{LogType::VulkanValidation,   "Vulkan validation layer"},
};

bool cemuLog_advancedPPCLoggingEnabled()
{
	return GetConfig().advanced_ppc_logging;
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

fs::path cemuLog_GetLogFilePath()
{
    return ActiveSettings::GetUserDataPath("log.txt");
}

void cemuLog_createLogFile(bool triggeredByCrash)
{
	std::unique_lock lock(LogContext.log_mutex);
	if (LogContext.file_stream.is_open())
		return;

	const auto path = cemuLog_GetLogFilePath();
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

	if (LaunchSettings::Verbose())
		std::cout << text << std::endl;

	cemuLog_writeLineToLog(text);

	const auto it = std::find_if(g_logging_window_mapping.cbegin(), g_logging_window_mapping.cend(),
		[type](const auto& entry) { return entry.first == type; });
	if (it == g_logging_window_mapping.cend())
		s_loggingDispatcher.Log(text);
	else
		s_loggingDispatcher.Log(it->second, text);

	return true;
}

bool cemuLog_log(LogType type, std::u8string_view text)
{
	std::basic_string_view<char> s((char*)text.data(), text.size());
	return cemuLog_log(type, s);
}

void cemuLog_logHexDump(LogType type, const void* data, size_t size, size_t lineSize)
{
	const uint8* dataU8 = static_cast<const uint8*>(data);
	std::vector<char> hexLine;
	hexLine.resize(6 + lineSize * 3 + 1, ' ');
	for (size_t i = 0; i < size; i += lineSize)
	{
		sprintf(hexLine.data(), "%04X: ", (int)i);
		size_t remainingSize = std::min<size_t>(lineSize, size - i);
		for (size_t j=0; j<remainingSize; j++)
			sprintf(hexLine.data() + 6 + j * 3, "%02X ", dataU8[j]);
		dataU8 += remainingSize;
		if (remainingSize == lineSize)
			hexLine.resize(6 + remainingSize * 3 + 1);
		cemuLog_log(type, hexLine.data());
	}
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
std::unique_lock<decltype(LogContext.log_mutex)> cemuLog_acquire()
{
	return std::unique_lock(LogContext.log_mutex);
}

void cemuLog_setActiveLoggingFlags(uint64 flagMask)
{
	s_loggingFlagMask = flagMask | cemuLog_getFlag(LogType::Force);
}
