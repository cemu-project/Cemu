#pragma once

extern uint64 s_loggingFlagMask;

enum class LogType : sint32
{
	// note: IDs must be in range 1-64
	Force = 63, // always enabled
	Placeholder = 62, // always disabled
	APIErrors = Force, // alias for Force. Logs bad parameters or other API errors in OS libs

	CoreinitFile = 0,
	GX2 = 1,
	UnsupportedAPI = 2,
	SoundAPI = 4, // any audio related API
	InputAPI = 5, // any input related API
	Socket = 6,
	Save = 7,
	H264 = 9,
	OpenGLLogging = 10, // OpenGL debug logging
	TextureCache = 11, // texture cache warnings and info
	VulkanValidation = 12, // Vulkan validation layer
	Patches = 14,
	CoreinitMem = 8, // coreinit memory functions
	CoreinitMP = 15,
	CoreinitThread = 16,
	CoreinitLogging = 17, // OSReport, OSConsoleWrite etc.
	CoreinitMemoryMapping = 18, // OSGetAvailPhysAddrRange, OSAllocVirtAddr, OSMapMemory etc.
	CoreinitAlarm = 22,
	CoreinitThreadSync = 3,

	PPC_IPC = 19,
	NN_AOC = 20,
	NN_PDM = 21,
	NN_OLV = 23,
	NN_NFP = 13,

	TextureReadback = 29,

	ProcUi = 39,

};

template <>
struct fmt::formatter<std::u8string_view> : formatter<string_view> {
	template <typename FormatContext>
	auto format(std::u8string_view v, FormatContext& ctx) 
	{
		string_view s((char*)v.data(), v.size());
		return formatter<string_view>::format(s, ctx);
	}
};

void cemuLog_writeLineToLog(std::string_view text, bool date = true, bool new_line = true);
inline void cemuLog_writePlainToLog(std::string_view text) { cemuLog_writeLineToLog(text, false, false); }

void cemuLog_setActiveLoggingFlags(uint64 flagMask);

inline uint64 cemuLog_getFlag(LogType type)
{
	return 1ULL << (uint64)type;
}

inline bool cemuLog_isLoggingEnabled(LogType type)
{
	return (s_loggingFlagMask & cemuLog_getFlag(type)) != 0;
}

bool cemuLog_log(LogType type, std::string_view text);
bool cemuLog_log(LogType type, std::u8string_view text);
void cemuLog_waitForFlush(); // wait until all log lines are written

template<typename T, typename ... TArgs>
bool cemuLog_log(LogType type, std::basic_string<T> formatStr, TArgs&&... args)
{
	if (!cemuLog_isLoggingEnabled(type))
		return false;
	if constexpr (sizeof...(TArgs) == 0)
	{
		cemuLog_log(type, std::basic_string_view<T>(formatStr.data(), formatStr.size()));
		return true;
	}
	else
	{
		const auto format_view = fmt::basic_string_view<T>(formatStr);
		const auto text = fmt::vformat(format_view, fmt::make_format_args<fmt::buffer_context<T>>(args...));
		cemuLog_log(type, std::basic_string_view(text.data(), text.size()));
	}
	return true;
}
 
template<typename T, typename ... TArgs>
bool cemuLog_log(LogType type, const T* format, TArgs&&... args)
{
	if (!cemuLog_isLoggingEnabled(type))
		return false;
	auto format_str = std::basic_string<T>(format);
	return cemuLog_log(type, format_str, std::forward<TArgs>(args)...);
}

// same as cemuLog_log, but only outputs in debug mode
template<typename TFmt, typename ... TArgs>
bool cemuLog_logDebug(LogType type, TFmt format, TArgs&&... args)
{
#ifdef CEMU_DEBUG_ASSERT
	return cemuLog_log(type, format, std::forward<TArgs>(args)...);
#else
	return false;
#endif
}

// cafe lib calls
bool cemuLog_advancedPPCLoggingEnabled();

uint64 cemuLog_getFlag(LogType type);

fs::path cemuLog_GetLogFilePath();
void cemuLog_createLogFile(bool triggeredByCrash);
[[nodiscard]] std::unique_lock<std::recursive_mutex> cemuLog_acquire(); // used for logging multiple lines at once
