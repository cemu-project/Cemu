#include "helpers.h"

#include <algorithm> 
#include <functional> 
#include <cctype>
#include <random>

#include <wx/translation.h>

#include "config/ActiveSettings.h"

#include <boost/random/uniform_int.hpp>

#include <zlib.h>


#if BOOST_OS_WINDOWS
#include <TlHelp32.h>
#endif

std::string& ltrim(std::string& str, const std::string& chars)
{
	str.erase(0, str.find_first_not_of(chars));
	return str;
}

std::string& rtrim(std::string& str, const std::string& chars)
{
	str.erase(str.find_last_not_of(chars) + 1);
	return str;
}

std::string& trim(std::string& str, const std::string& chars)
{
	return ltrim(rtrim(str, chars), chars);
}


std::string_view& ltrim(std::string_view& str, const std::string& chars)
{
	str.remove_prefix(std::min(str.find_first_not_of(chars), str.size()));
	return str;
}
std::string_view& rtrim(std::string_view& str, const std::string& chars)
{
	str.remove_suffix(std::max(str.size() - str.find_last_not_of(chars) - 1, (size_t)0));
	return str;
}
std::string_view& trim(std::string_view& str, const std::string& chars)
{
	return ltrim(rtrim(str, chars), chars);
}

#if BOOST_OS_WINDOWS

std::wstring GetSystemErrorMessageW()
{
	return GetSystemErrorMessageW(GetLastError());
}


std::wstring GetSystemErrorMessageW(DWORD error_code)
{
	if(error_code == ERROR_SUCCESS)
		return {};

	LPWSTR lpMsgBuf = nullptr;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error_code, 0, (LPWSTR)&lpMsgBuf, 0, nullptr);
	if (lpMsgBuf)
	{
		std::wstring str = fmt::format(L"{}: {}", _("Error").ToStdWstring(), lpMsgBuf); // TRANSLATE
		LocalFree(lpMsgBuf);
		return str;
	}

	return fmt::format(L"{}: {:#x}", _("Error code").ToStdWstring(), error_code);
}

std::string GetSystemErrorMessage(DWORD error_code)
{
	if(error_code == ERROR_SUCCESS)
		return {};

	LPSTR lpMsgBuf = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error_code, 0, (LPSTR)&lpMsgBuf, 0, nullptr);
	if (lpMsgBuf)
	{
		std::string str = fmt::format("{}: {}", _("Error").ToStdString(), lpMsgBuf); // TRANSLATE
		LocalFree(lpMsgBuf);
		return str;
	}

	return fmt::format("{}: {:#x}", _("Error code").ToStdString(), error_code);
}

std::string GetSystemErrorMessage()
{
	return GetSystemErrorMessage(GetLastError());
}

#else

std::string GetSystemErrorMessage()
{
	return "";
}

#endif

std::string GetSystemErrorMessage(const std::exception& ex)
{
	const std::string msg = GetSystemErrorMessage();
	if(msg.empty())
		return ex.what();

	return fmt::format("{}\n{}",msg, ex.what());
}

std::string GetSystemErrorMessage(const std::error_code& ec)
{
	const std::string msg = GetSystemErrorMessage();
	if(msg.empty())
		return ec.message();

	return fmt::format("{}\n{}",msg, ec.message());
}

#if BOOST_OS_WINDOWS
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
	DWORD dwType; // Must be 0x1000.
	LPCSTR szName; // Pointer to name (in user addr space).
	DWORD dwThreadID; // Thread ID (-1=caller thread).
	DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
#endif

void SetThreadName(const char* name)
{
#if BOOST_OS_WINDOWS
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = name;
	info.dwThreadID = GetCurrentThreadId();
	info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
	__try {
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
#pragma warning(pop)
#elif BOOST_OS_MACOS
	pthread_setname_np(name);
#else
	if(std::strlen(name) > 15)
		cemuLog_log(LogType::Force, "Truncating thread name {} because it was longer than 15 characters", name);
	pthread_setname_np(pthread_self(), std::string{name}.substr(0,15).c_str());
#endif
}

#if BOOST_OS_WINDOWS
std::pair<DWORD, DWORD> GetWindowsVersion()
{
	using RtlGetVersion_t = LONG(*)(POSVERSIONINFOEXW);
	static RtlGetVersion_t pRtlGetVersion = nullptr;
	if(!pRtlGetVersion) 
		pRtlGetVersion = (RtlGetVersion_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
	cemu_assert(pRtlGetVersion);

	OSVERSIONINFOEXW version_info{};
	pRtlGetVersion(&version_info);
	return { version_info.dwMajorVersion, version_info.dwMinorVersion };
}

bool IsWindows81OrGreater()
{
	const auto [major, minor] = GetWindowsVersion();
	return major > 6 || (major == 6 && minor >= 3);
}

bool IsWindows10OrGreater()
{
	const auto [major, minor] = GetWindowsVersion();
	return major >= 10;
}
#endif

fs::path GetParentProcess()
{
	fs::path result;
	
#if BOOST_OS_WINDOWS
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(hSnapshot != INVALID_HANDLE_VALUE)
	{
		DWORD pid = GetCurrentProcessId();
		
		PROCESSENTRY32 pe{};
		pe.dwSize = sizeof(pe);
		for(BOOL ret = Process32First(hSnapshot, &pe); ret; ret = Process32Next(hSnapshot, &pe))
		{
			if(pe.th32ProcessID == pid)
			{
				HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ParentProcessID);
				if(hProcess)
				{
					wchar_t tmp[MAX_PATH];
					DWORD size = std::size(tmp);
					if (QueryFullProcessImageNameW(hProcess, 0, tmp, &size) && size > 0)
						result = tmp;
					
					CloseHandle(hProcess);
				}
				break;
			}
		}
		
		CloseHandle(hSnapshot);
	}
#else
	assert_dbg();
#endif

	return result;
}

std::string ltrim_copy(const std::string& s)
{
	std::string result = s;
	ltrim(result);
	return result;
}

std::string rtrim_copy(const std::string& s)
{
	std::string result = s;
	rtrim(result);
	return result;
}

uint32_t GetPhysicalCoreCount()
{
	static uint32_t s_core_count = 0;
	if (s_core_count != 0)
		return s_core_count;
	
#if BOOST_OS_WINDOWS
	auto core_count = std::thread::hardware_concurrency();

	// Get physical cores
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = nullptr;
	DWORD returnLength = 0;
	GetLogicalProcessorInformation(buffer, &returnLength);
	if (returnLength > 0)
	{
		buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);
		if (GetLogicalProcessorInformation(buffer, &returnLength))
		{
			uint32_t counter = 0;
			for (DWORD i = 0; i < returnLength / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i)
			{
				if (buffer[i].Relationship == RelationProcessorCore)
					++counter;
			}

			if (counter > 0 && counter < core_count)
				core_count = counter;
		}

		free(buffer);
	}

	s_core_count = core_count;
	return core_count;
#else
	return std::thread::hardware_concurrency();
#endif
}

bool TestWriteAccess(const fs::path& p)
{
	std::error_code ec;
	// must be path and must exist
	if (!fs::exists(p, ec) || !fs::is_directory(p, ec))
		return false;

	// retry 3 times
	for (int i = 0; i < 3; ++i)
	{
		const auto filename = p / fmt::format("_{}.tmp", GenerateRandomString(8));
		if (fs::exists(filename, ec))
			continue;

		std::ofstream file(filename);
		if (!file.is_open()) // file couldn't be created
			break;
		
		file.close();

		fs::remove(filename, ec);
		return true;
	}
	
	return false;
}

// make path relative to Cemu directory
fs::path MakeRelativePath(const fs::path& base, const fs::path& path)
{
	try
	{
		return fs::relative(path, base);
	}
	catch (const std::exception&)
	{
		return path;
	}
}

#ifdef HAS_DIRECTINPUT
bool GUIDFromString(const char* string, GUID& guid)
{
	unsigned long p0;
	int p1, p2, p3, p4, p5, p6, p7, p8, p9, p10;
	const sint32 count = sscanf_s(string, "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", &p0, &p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &p9, &p10);
	if (count != 11)
		return false;

	guid.Data1 = p0;
	guid.Data2 = p1;
	guid.Data3 = p2;
	guid.Data4[0] = p3;
	guid.Data4[1] = p4;
	guid.Data4[2] = p5;
	guid.Data4[3] = p6;
	guid.Data4[4] = p7;
	guid.Data4[5] = p8;
	guid.Data4[6] = p9;
	guid.Data4[7] = p10;
	return count == 11;
}

std::string StringFromGUID(const GUID& guid)
{
	char temp[256];
	sprintf(temp, "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	return std::string(temp);
}

std::wstring WStringFromGUID(const GUID& guid)
{
	wchar_t temp[256];
	swprintf_s(temp, L"%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	return std::wstring(temp);
}
#endif

std::vector<std::string_view> TokenizeView(std::string_view str, char delimiter)
{
	std::vector<std::string_view> result;

	size_t last_token_index = 0;
	for (auto index = str.find(delimiter); index != std::string_view::npos; index = str.find(delimiter, index + 1))
	{
		const auto token = str.substr(last_token_index, index - last_token_index);
		result.emplace_back(token);

		last_token_index = index + 1;
	}

	try
	{
		const auto token = str.substr(last_token_index);
		result.emplace_back(token);
	}
	catch (const std::invalid_argument&) {}

	return result;
}

std::vector<std::string> Tokenize(std::string_view str, char delimiter)
{
	std::vector<std::string> result;

	size_t last_token_index = 0;
	for (auto index = str.find(delimiter); index != std::string_view::npos; index = str.find(delimiter, index + 1))
	{
		const auto token = str.substr(last_token_index, index - last_token_index);
		result.emplace_back(token);

		last_token_index = index + 1;
	}

	try
	{
		const auto token = str.substr(last_token_index);
		result.emplace_back(token);
	}
	catch (const std::invalid_argument&) {}

	return result;
}

std::string GenerateRandomString(size_t length)
{
	const std::string kCharacters{
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"1234567890" };
	return GenerateRandomString(length, kCharacters);
}

std::string GenerateRandomString(const size_t length, const std::string_view characters)
{
	assert(!characters.empty());
	std::string result;
	result.resize(length);

	std::random_device rd;
	std::mt19937 gen(rd());
     
        // workaround for static asserts using boost
        boost::random::uniform_int_distribution<decltype(characters.size())> index_dist(0, characters.size() - 1);
	std::generate_n(
		result.begin(),
		length,
		[&] { return characters[index_dist(gen)]; }
	);

	return result;
}

std::optional<std::vector<uint8>> zlibDecompress(const std::vector<uint8>& compressed, size_t sizeHint)
{
	int err;
	std::vector<uint8> decompressed;
	size_t outWritten = 0;
	size_t bytesPerIteration = sizeHint;
	z_stream stream;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.avail_in = compressed.size();
	stream.next_in = (Bytef*)compressed.data();
	err = inflateInit2(&stream, 32); // 32 is a zlib magic value to enable header detection
	if (err != Z_OK)
		return {};

	do
	{
		decompressed.resize(decompressed.size() + bytesPerIteration);
		const auto availBefore = decompressed.size() - outWritten;
		stream.avail_out = availBefore;
		stream.next_out = decompressed.data() + outWritten;
		err = inflate(&stream, Z_NO_FLUSH);
		if (!(err == Z_OK || err == Z_STREAM_END))
		{
			inflateEnd(&stream);
			return {};
		}
		outWritten += availBefore - stream.avail_out;
		bytesPerIteration *= 2;
	}
	while (err != Z_STREAM_END);

	inflateEnd(&stream);
	decompressed.resize(stream.total_out);

	return decompressed;
}
