#pragma once

enum class LogType : sint32
{
	// note: IDs must not exceed 63
	Placeholder = -2,
	None = -1,
	Force = 0, // this logging type is always on
	CoreinitFile = 1,
	GX2 = 2,
	UnsupportedAPI = 3,
	ThreadSync = 4,
	SoundAPI = 5, // any audio related API
	InputAPI = 6, // any input related API
	Socket = 7,
	Save = 8,
	CoreinitMem = 9, // coreinit memory functions
	H264 = 10,
	OpenGLLogging = 11, // OpenGL debug logging
	TextureCache = 12, // texture cache warnings and info
	VulkanValidation = 13, // Vulkan validation layer
	nn_nfp = 14, // nn_nfp (Amiibo) API
	Patches = 15,
	CoreinitMP = 16,
	CoreinitThread = 17,
	CoreinitLogging = 18, // OSReport, OSConsoleWrite etc.
	CoreinitMemoryMapping = 19, // OSGetAvailPhysAddrRange, OSAllocVirtAddr, OSMapMemory etc.
	CoreinitAlarm = 23,

	PPC_IPC = 20,
	NN_AOC = 21,
	NN_PDM = 22,
	
	TextureReadback = 30,

	ProcUi = 40,

	APIErrors = 0, // alias for Force. Logs bad parameters or other API errors in OS libs

	
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

bool cemuLog_isLoggingEnabled(LogType type);

bool cemuLog_log(LogType type, std::string_view text);
bool cemuLog_log(LogType type, std::u8string_view text);
bool cemuLog_log(LogType type, std::wstring_view text);
void cemuLog_waitForFlush(); // wait until all log lines are written

template <typename T>
auto ForwardEnum(T t) 
{
  if constexpr (std::is_enum_v<T>)
    return fmt::underlying(t);
  else
    return std::forward<T>(t);
}

template <typename... TArgs>
auto ForwardEnum(std::tuple<TArgs...> t) 
{ 
	return std::apply([](auto... x) { return std::make_tuple(ForwardEnum(x)...); }, t);
}

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
		const auto text = fmt::vformat(format_view, fmt::make_format_args<fmt::buffer_context<T>>(ForwardEnum(args)...));
		cemuLog_log(type, std::basic_string_view(text.data(), text.size()));
	}
	return true;
}
 
template<typename T, typename ... TArgs>
bool cemuLog_log(LogType type, const T* format, TArgs&&... args)
{
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
void cemuLog_setFlag(LogType type, bool enabled);

fs::path cemuLog_GetLogFilePath();
void cemuLog_createLogFile(bool triggeredByCrash);
[[nodiscard]] std::unique_lock<std::recursive_mutex> cemuLog_acquire(); // used for logging multiple lines at once
