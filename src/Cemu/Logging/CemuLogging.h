#pragma once

enum class LogType : sint32
{
	Placeholder = -2,
	None = -1,
	Force = 0, // this logging type is always on
	File = 1, // coreinit file?
	GX2 = 2,
	UnsupportedAPI = 3,
	ThreadSync = 4,
	SoundAPI = 5, // any audio related API
	Input = 6, // any input related API
	Socket = 7,
	Save = 8,
	CoreinitMem = 9, // coreinit memory functions
	H264 = 10,
	OpenGL = 11, // OpenGL debug logging
	TextureCache = 12, // texture cache warnings and info
	VulkanValidation = 13, // Vulkan validation layer
	nn_nfp = 14, // nn_nfp (Amiibo) API
	Patches = 15,
	CoreinitMP = 16,
	CoreinitThread = 17,
	CoreinitLogging = 18, // OSReport, OSConsoleWrite etc.

	PPC_IPC = 20,
	NN_AOC = 21,
	NN_PDM = 22,

	ProcUi = 40,

	APIErrors = 0, // alias for 0. Logs bad parameters or other API errors in OS libs
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
bool cemuLog_log(LogType type, std::basic_string<T> format, TArgs&&... args)
{
	if (!cemuLog_isLoggingEnabled(type))
		return false;

	const auto format_view = fmt::basic_string_view<T>(format);
	const auto text = fmt::vformat(format_view, fmt::make_format_args<fmt::buffer_context<T>>(ForwardEnum(args)...));
	cemuLog_log(type, std::basic_string_view(text.data(), text.size()));
	return true;
}
 
template<typename T, typename ... TArgs>
bool cemuLog_log(LogType type, const T* format, TArgs&&... args)
{
	auto format_str=std::basic_string<T>(format);
	return cemuLog_log(type, format_str, std::forward<TArgs>(args)...);
}

// same as cemuLog_log, but only outputs in debug/release mode
template<typename TFmt, typename ... TArgs>
bool cemuLog_logDebug(LogType type, TFmt format, TArgs&&... args)
{
#ifndef PUBLIC_RELEASE
	return cemuLog_log(type, format, std::forward<TArgs>(args)...);
#else
	return false;
#endif
}

// cafe lib calls
bool cemuLog_advancedPPCLoggingEnabled();

uint64 cemuLog_getFlag(LogType type);
void cafeLog_setLoggingFlagEnable(sint32 loggingType, bool isEnable);
bool cafeLog_isLoggingFlagEnabled(sint32 loggingType);

void cemuLog_createLogFile(bool triggeredByCrash);

[[nodiscard]] std::unique_lock<std::recursive_mutex> cafeLog_acquire();

// legacy methods
void cafeLog_log(uint32 type, const char* format, ...);
void cafeLog_logW(uint32 type, const wchar_t* format, ...);
void cafeLog_logLine(uint32 type, const char* line);

// legacy log types, deprecated. Use LogType instead
#define LOG_TYPE_FORCE				(0) // this logging type is always on
#define LOG_TYPE_FILE				(1)
#define LOG_TYPE_GX2				(2)
#define LOG_TYPE_UNSUPPORTED_API	(3)
#define LOG_TYPE_THREADSYNC			(4)
#define LOG_TYPE_SNDAPI				(5) // any audio related API
#define LOG_TYPE_INPUT				(6) // any input related API
#define LOG_TYPE_SOCKET				(7) // anything from nn_sysnet
#define LOG_TYPE_SAVE				(8) // anything from nn_save
#define LOG_TYPE_COREINIT_MEM		(9)	// coreinit memory functions
#define LOG_TYPE_H264				(10)
#define LOG_TYPE_OPENGL				(11) // OpenGL debug logging
#define LOG_TYPE_TEXTURE_CACHE		(12) // texture cache warnings and info
#define LOG_TYPE_VULKAN_VALIDATION	(13) // Vulkan validation layer
#define LOG_TYPE_NFP				(14) // nn_nfp (Amiibo) API
#define LOG_TYPE_PATCHES			(15) // logging for graphic pack patches
#define LOG_TYPE_COREINIT_MP		(16)
#define LOG_TYPE_COREINIT_THREAD	(17)
#define LOG_TYPE_COREINIT_LOGGING	(18)
#define LOG_TYPE_LAST				(64) // set to 64 so we can easily add more log types in the future without destroying settings file compatibility

// force log. Redundant -> replace with _log(LogType::Force, ...) call
inline bool cemuLog_force(std::string_view msg)
{
	return cemuLog_log(LogType::Force, msg);
}

inline bool cemuLog_force(std::wstring_view msg)
{
	return cemuLog_log(LogType::Force, msg);
}

template<typename TFmt, typename ... TArgs>
void cemuLog_force(TFmt format, TArgs&&... args)
{
	cemuLog_log(LogType::Force, format, std::forward<TArgs>(args)...);
}

// these are deprecated. Superseded by cemuLog_log(type, ...)
#define forceLog_printf(...)		cafeLog_log(LOG_TYPE_FORCE, __VA_ARGS__);
#define forceLog_printfW(...)		cafeLog_logW(LOG_TYPE_FORCE, __VA_ARGS__);
#define gx2Log_printf(...)			if( cafeLog_isLoggingFlagEnabled(LOG_TYPE_GX2) ) cafeLog_log(LOG_TYPE_GX2, __VA_ARGS__);
#define coreinitMemLog_printf(...)	if( cafeLog_isLoggingFlagEnabled(LOG_TYPE_COREINIT_MEM) ) cafeLog_log(LOG_TYPE_COREINIT_MEM, __VA_ARGS__);
#define sndApiLog_printf(...)		if( cafeLog_isLoggingFlagEnabled(LOG_TYPE_SNDAPI) ) cafeLog_log(LOG_TYPE_SNDAPI, __VA_ARGS__);
#define socketLog_printf(...)		if( cafeLog_isLoggingFlagEnabled(LOG_TYPE_SOCKET) ) cafeLog_log(LOG_TYPE_SOCKET, __VA_ARGS__);
#define inputLog_printf(...)		if( cafeLog_isLoggingFlagEnabled(LOG_TYPE_INPUT) ) cafeLog_log(LOG_TYPE_INPUT, __VA_ARGS__);
#define saveLog_printf(...)			if( cafeLog_isLoggingFlagEnabled(LOG_TYPE_SAVE) ) cafeLog_log(LOG_TYPE_SAVE, __VA_ARGS__);
#define nfpLog_printf(...)			if( cafeLog_isLoggingFlagEnabled(LOG_TYPE_NFP) ) cafeLog_log(LOG_TYPE_NFP, __VA_ARGS__);

#ifndef PUBLIC_RELEASE
#define forceLogDebug_printf(...)		cafeLog_log(LOG_TYPE_FORCE, __VA_ARGS__);
#define forceLogDebug_printfW(...)		cafeLog_logW(LOG_TYPE_FORCE, __VA_ARGS__);

 // use this for temporary debug logging. Will trigger compilation error in release builds
#define forceLogRemoveMe_printf(...)	cafeLog_log(LOG_TYPE_FORCE, __VA_ARGS__);
#else
#define forceLogDebug_printf(...)	;
#define forceLogDebug_printfW(...)	;
#endif
