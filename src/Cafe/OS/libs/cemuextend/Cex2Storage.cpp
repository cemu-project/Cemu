#include "Common/precompiled.h"

#include "Cafe/OS/libs/cemuextend/Cex2Storage.h"

#include "config/ActiveSettings.h"

#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <random>

#ifndef _WIN32
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#else
#include <Windows.h>
#include <winternl.h>
#endif

namespace cemuextend_hle {
namespace {

namespace fs = std::filesystem;
using namespace cemuextend::wire;

constexpr std::uint64_t kNamespaceBytes = 64ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMaximumFileBytes = 16ULL * 1024ULL * 1024ULL;
constexpr std::uint32_t kMaximumFiles = 4096;
constexpr std::size_t kMaximumConfigEntries = 1024;
constexpr std::size_t kMaximumConfigBytes = 1024 * 1024;
constexpr std::size_t kMaximumValueBytes = 64 * 1024;
constexpr std::size_t kMaximumPageEntries = 128;

std::mutex s_storageMutex;

std::string Hex(std::span<const unsigned char> bytes)
{
	constexpr char digits[] = "0123456789abcdef";
	std::string result(bytes.size() * 2, '0');
	for (std::size_t i = 0; i < bytes.size(); ++i)
	{
		result[i * 2] = digits[bytes[i] >> 4];
		result[i * 2 + 1] = digits[bytes[i] & 15];
	}
	return result;
}

std::string PrincipalHash(std::string_view principal)
{
	std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
	SHA256(reinterpret_cast<const unsigned char*>(principal.data()), principal.size(), digest.data());
	return Hex(digest);
}

fs::path NamespaceRoot(std::uint64_t titleId, std::string_view principal, std::string_view kind)
{
#ifdef CEMU_CEX2_TESTING
	auto root = fs::temp_directory_path() / "cemuextend-cex2-tests";
#else
	auto root = ActiveSettings::GetUserDataPath("cemuextend/abi2");
#endif
	return root / kind / fmt::format("{:016x}", titleId) / PrincipalHash(principal);
}

bool IsValidUtf8(std::string_view text)
{
	for (std::size_t index = 0; index < text.size();)
	{
		const auto lead = static_cast<std::uint8_t>(text[index]);
		if (lead < 0x80) { ++index; continue; }
		const std::size_t count = (lead & 0xe0) == 0xc0 ? 1 : (lead & 0xf0) == 0xe0 ? 2 :
			(lead & 0xf8) == 0xf0 ? 3 : 0;
		if (!count || index + count >= text.size()) return false;
		std::uint32_t cp = lead & (0x7fU >> count);
		for (std::size_t offset = 1; offset <= count; ++offset)
		{
			const auto next = static_cast<std::uint8_t>(text[index + offset]);
			if ((next & 0xc0) != 0x80) return false;
			cp = (cp << 6) | (next & 0x3f);
		}
		if ((count == 1 && cp < 0x80) || (count == 2 && cp < 0x800) ||
			(count == 3 && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
			return false;
		index += count + 1;
	}
	return true;
}

std::optional<std::vector<std::string>> SplitPath(std::string_view path, bool allowRoot)
{
	if (!IsValidUtf8(path) || path.find('\0') != std::string_view::npos || path.starts_with('/'))
		return std::nullopt;
	if (allowRoot && (path.empty() || path == ".")) return std::vector<std::string>{};
	if (path.empty()) return std::nullopt;
	std::vector<std::string> result;
	for (std::size_t begin = 0; begin <= path.size();)
	{
		const auto end = path.find('/', begin);
		const auto part = path.substr(begin, end == std::string_view::npos ? path.size() - begin : end - begin);
		if (part.empty() || part == "." || part == ".." || part.size() > 255) return std::nullopt;
		result.emplace_back(part);
		if (end == std::string_view::npos) break;
		begin = end + 1;
	}
	return result;
}

#ifndef _WIN32
class Fd
{
public:
	Fd() = default;
	explicit Fd(int value) : value(value) {}
	~Fd() { if (value >= 0) ::close(value); }
	Fd(Fd&& other) noexcept : value(std::exchange(other.value, -1)) {}
	Fd& operator=(Fd&& other) noexcept { if (this != &other) { if (value >= 0) ::close(value); value = std::exchange(other.value, -1); } return *this; }
	Fd(const Fd&) = delete;
	int get() const { return value; }
	explicit operator bool() const { return value >= 0; }
private:
	int value{-1};
};

Fd OpenRoot(const fs::path& root)
{
	std::error_code error;
	fs::create_directories(root, error);
	if (error) return {};
	return Fd(::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
}

Fd OpenParent(int root, const std::vector<std::string>& parts, bool create)
{
	Fd current(::dup(root));
	if (!current) return {};
	for (std::size_t index = 0; index + 1 < parts.size(); ++index)
	{
		int next = ::openat(current.get(), parts[index].c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		if (next < 0 && create && errno == ENOENT)
		{
			if (::mkdirat(current.get(), parts[index].c_str(), 0700) != 0 && errno != EEXIST) return {};
			next = ::openat(current.get(), parts[index].c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		}
		if (next < 0) return {};
		current = Fd(next);
	}
	return current;
}

struct Usage { std::uint64_t bytes{}; std::uint32_t files{}; };

bool Measure(int directory, Usage& usage)
{
	DIR* stream = ::fdopendir(::dup(directory));
	if (!stream) return false;
	while (auto* entry = ::readdir(stream))
	{
		if (!std::strcmp(entry->d_name, ".") || !std::strcmp(entry->d_name, "..")) continue;
		struct stat stat{};
		if (::fstatat(directory, entry->d_name, &stat, AT_SYMLINK_NOFOLLOW) != 0 || S_ISLNK(stat.st_mode))
		{ ::closedir(stream); return false; }
		if (S_ISREG(stat.st_mode))
		{
			if (stat.st_size < 0 || usage.bytes > kNamespaceBytes - std::min<std::uint64_t>(stat.st_size, kNamespaceBytes))
			{ ::closedir(stream); return false; }
			usage.bytes += stat.st_size; ++usage.files;
		}
		else if (S_ISDIR(stat.st_mode))
		{
			Fd child(::openat(directory, entry->d_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
			if (!child || !Measure(child.get(), usage)) { ::closedir(stream); return false; }
		}
		else { ::closedir(stream); return false; }
	}
	return ::closedir(stream) == 0;
}

Status ErrnoStatus(int value)
{
	if (value == ENOENT) return Status::NotFound;
	if (value == EEXIST || value == ENOTEMPTY) return Status::Busy;
	if (value == ELOOP || value == ENOTDIR || value == EPERM || value == EACCES) return Status::PermissionDenied;
	return Status::IoError;
}
#else
class WinHandle
{
public:
	WinHandle() = default;
	explicit WinHandle(HANDLE value) : value(value) {}
	~WinHandle() { if (value != INVALID_HANDLE_VALUE) ::CloseHandle(value); }
	WinHandle(WinHandle&& other) noexcept : value(std::exchange(other.value, INVALID_HANDLE_VALUE)) {}
	WinHandle& operator=(WinHandle&& other) noexcept { if (this != &other) { if (*this) ::CloseHandle(value); value = std::exchange(other.value, INVALID_HANDLE_VALUE); } return *this; }
	WinHandle(const WinHandle&) = delete;
	HANDLE get() const { return value; }
	explicit operator bool() const { return value != INVALID_HANDLE_VALUE && value != nullptr; }
private:
	HANDLE value{INVALID_HANDLE_VALUE};
};

using NtCreateFileFn = NTSTATUS (NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES,
	PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
using RtlNtStatusToDosErrorFn = ULONG (NTAPI*)(NTSTATUS);

constexpr ULONG kFileOpen = 1;
constexpr ULONG kFileCreate = 2;
constexpr ULONG kFileOpenIf = 3;
constexpr ULONG kFileDirectoryFile = 0x00000001;
constexpr ULONG kFileSynchronousIoNonalert = 0x00000020;
constexpr ULONG kFileNonDirectoryFile = 0x00000040;
constexpr ULONG kFileOpenReparsePoint = 0x00200000;

std::wstring Wide(std::string_view value)
{
	if (value.empty()) return {};
	const auto size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), nullptr, 0);
	if (size <= 0) return {};
	std::wstring result(size, L'\0');
	if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), result.data(), size) != size) return {};
	return result;
}

std::string Utf8(std::wstring_view value)
{
	if (value.empty()) return {};
	const auto size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
	if (size <= 0) return {};
	std::string result(size, '\0');
	if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), result.data(), size, nullptr, nullptr) != size) return {};
	return result;
}

Status WinStatus(DWORD value)
{
	if (value == ERROR_FILE_NOT_FOUND || value == ERROR_PATH_NOT_FOUND) return Status::NotFound;
	if (value == ERROR_ALREADY_EXISTS || value == ERROR_FILE_EXISTS || value == ERROR_DIR_NOT_EMPTY ||
		value == ERROR_SHARING_VIOLATION) return Status::Busy;
	if (value == ERROR_ACCESS_DENIED || value == ERROR_CANT_ACCESS_FILE || value == ERROR_REPARSE_TAG_INVALID ||
		value == ERROR_NOT_A_REPARSE_POINT) return Status::PermissionDenied;
	return Status::IoError;
}

DWORD NtError(NTSTATUS status)
{
	static const auto convert = reinterpret_cast<RtlNtStatusToDosErrorFn>(
		::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "RtlNtStatusToDosError"));
	return convert ? convert(status) : ERROR_GEN_FAILURE;
}

bool SafeHandle(HANDLE handle, bool directory)
{
	FILE_ATTRIBUTE_TAG_INFO tag{};
	FILE_STANDARD_INFO standard{};
	return ::GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag)) &&
		(tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
		::GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, sizeof(standard)) &&
		standard.Directory == directory;
}

WinHandle Duplicate(HANDLE source)
{
	HANDLE result{INVALID_HANDLE_VALUE};
	if (!::DuplicateHandle(::GetCurrentProcess(), source, ::GetCurrentProcess(), &result,
		0, FALSE, DUPLICATE_SAME_ACCESS)) return {};
	return WinHandle(result);
}

WinHandle OpenRelative(HANDLE parent, std::string_view name, bool directory, bool create,
	ACCESS_MASK access, DWORD& error, bool exclusive = false)
{
	static const auto createFile = reinterpret_cast<NtCreateFileFn>(
		::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "NtCreateFile"));
	const auto wide = Wide(name);
	if (!createFile || wide.empty() || wide.size() > std::numeric_limits<USHORT>::max() / sizeof(wchar_t))
	{ error = ERROR_INVALID_NAME; return {}; }
	UNICODE_STRING unicode{};
	unicode.Buffer = const_cast<PWSTR>(wide.data());
	unicode.Length = static_cast<USHORT>(wide.size() * sizeof(wchar_t));
	unicode.MaximumLength = unicode.Length;
	OBJECT_ATTRIBUTES attributes{};
	attributes.Length = sizeof(attributes);
	attributes.RootDirectory = parent;
	attributes.ObjectName = &unicode;
	attributes.Attributes = OBJ_CASE_INSENSITIVE;
	IO_STATUS_BLOCK statusBlock{};
	HANDLE handle{INVALID_HANDLE_VALUE};
	const auto options = (directory ? kFileDirectoryFile : kFileNonDirectoryFile) |
		kFileSynchronousIoNonalert | kFileOpenReparsePoint;
	const auto status = createFile(&handle, access | SYNCHRONIZE, &attributes, &statusBlock, nullptr,
		FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		create ? (exclusive ? kFileCreate : kFileOpenIf) : kFileOpen, options, nullptr, 0);
	if (status < 0)
	{ error = NtError(status); return {}; }
	WinHandle result(handle);
	if (!SafeHandle(handle, directory))
	{ error = ERROR_CANT_ACCESS_FILE; return {}; }
	error = ERROR_SUCCESS;
	return result;
}

WinHandle OpenRoot(const fs::path& root)
{
	std::error_code error;
	fs::create_directories(root, error);
	if (error) return {};
	WinHandle result(::CreateFileW(root.c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES |
		FILE_TRAVERSE | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY | FILE_DELETE_CHILD | SYNCHRONIZE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
	return result && SafeHandle(result.get(), true) ? std::move(result) : WinHandle{};
}

WinHandle OpenParent(HANDLE root, const std::vector<std::string>& parts, bool create, DWORD& error)
{
	auto current = Duplicate(root);
	if (!current) { error = ::GetLastError(); return {}; }
	for (std::size_t index = 0; index + 1 < parts.size(); ++index)
	{
		auto next = OpenRelative(current.get(), parts[index], true, create,
			FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | FILE_TRAVERSE |
			(create ? FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY | FILE_DELETE_CHILD : 0), error);
		if (!next) return {};
		current = std::move(next);
	}
	return current;
}

WinHandle OpenAny(HANDLE parent, std::string_view name, ACCESS_MASK access, bool& directory,
	DWORD& error)
{
	directory = false;
	auto result = OpenRelative(parent, name, false, false, access, error);
	if (result) return result;
	directory = true;
	return OpenRelative(parent, name, true, false, access, error);
}

struct Usage { std::uint64_t bytes{}; std::uint32_t files{}; };
struct WinEntry { std::string name; bool directory{}; std::uint64_t size{}; };

bool Entries(HANDLE directory, std::vector<WinEntry>& entries)
{
	std::array<std::byte, 64 * 1024> buffer{};
	bool restart = true;
	for (;;)
	{
		const auto infoClass = restart ? FileIdBothDirectoryRestartInfo : FileIdBothDirectoryInfo;
		if (!::GetFileInformationByHandleEx(directory, infoClass, buffer.data(), static_cast<DWORD>(buffer.size())))
		{
			return ::GetLastError() == ERROR_NO_MORE_FILES;
		}
		restart = false;
		for (auto offset = std::size_t{};;)
		{
			const auto* info = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(buffer.data() + offset);
			const std::wstring_view wide(info->FileName, info->FileNameLength / sizeof(wchar_t));
			if (wide != L"." && wide != L"..")
			{
				if (info->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) return false;
				const auto directoryEntry = (info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
				const auto name = Utf8(wide);
				if (name.empty()) return false;
				entries.push_back({name, directoryEntry,
					directoryEntry ? 0 : static_cast<std::uint64_t>(info->EndOfFile.QuadPart)});
			}
			if (info->NextEntryOffset == 0) break;
			offset += info->NextEntryOffset;
			if (offset >= buffer.size()) return false;
		}
	}
}

bool Measure(HANDLE directory, Usage& usage)
{
	std::vector<WinEntry> entries;
	if (!Entries(directory, entries)) return false;
	for (const auto& entry : entries)
	{
		if (entry.directory)
		{
			DWORD error{};
			auto child = OpenRelative(directory, entry.name, true, false,
				FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | FILE_TRAVERSE, error);
			if (!child || !Measure(child.get(), usage)) return false;
		}
		else
		{
			if (entry.size > kNamespaceBytes || usage.bytes > kNamespaceBytes - entry.size) return false;
			usage.bytes += entry.size;
			if (++usage.files > kMaximumFiles) return false;
		}
	}
	return true;
}
#endif

bool ValidConfigValue(ValueType type, std::span<const std::byte> value)
{
	if (value.size() > kMaximumValueBytes) return false;
	switch (type)
	{
	case ValueType::Boolean: return value.size() == 1 && std::to_integer<unsigned>(value[0]) <= 1;
	case ValueType::SignedInteger:
	case ValueType::UnsignedInteger:
	case ValueType::Float: return value.size() == 8;
	case ValueType::String:
		return IsValidUtf8({reinterpret_cast<const char*>(value.data()), value.size()});
	case ValueType::Bytes: return true;
	default: return false;
	}
}

using ConfigMap = std::map<std::string, std::pair<ValueType, std::vector<std::byte>>>;
using ConfigCacheKey = std::pair<std::uint64_t, std::string>;
constexpr std::size_t kMaximumCachedNamespaces = 64;
std::map<ConfigCacheKey, ConfigMap> s_configCache;

std::array<unsigned char, SHA256_DIGEST_LENGTH> Digest(std::span<const std::byte> bytes)
{
	std::array<unsigned char, SHA256_DIGEST_LENGTH> result{};
	SHA256(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), result.data());
	return result;
}

Cex2StorageResult LoadConfig(const fs::path& root, ConfigMap& values)
{
	std::vector<std::byte> bytes;
#ifndef _WIN32
	Fd directory = OpenRoot(root);
	if (!directory) return {Status::IoError};
	Fd input(::openat(directory.get(), "config.cxc2", O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
	if (!input) return errno == ENOENT ? Cex2StorageResult{Status::Ok} : Cex2StorageResult{Status::IoError};
	struct stat stat{};
	if (::fstat(input.get(), &stat) != 0 || !S_ISREG(stat.st_mode) || stat.st_size < 0 ||
		static_cast<std::uint64_t>(stat.st_size) > kMaximumConfigBytes + 48) return {Status::IoError};
	bytes.resize(static_cast<std::size_t>(stat.st_size));
	std::size_t read{};
	while (read < bytes.size())
	{
		const auto count = ::pread(input.get(), bytes.data() + read, bytes.size() - read, read);
		if (count <= 0) return {Status::IoError};
		read += count;
	}
#else
	auto directory = OpenRoot(root);
	if (!directory) return {Status::IoError};
	DWORD error{};
	auto input = OpenRelative(directory.get(), "config.cxc2", false, false,
		GENERIC_READ | FILE_READ_ATTRIBUTES, error);
	if (!input) return error == ERROR_FILE_NOT_FOUND ? Cex2StorageResult{Status::Ok} : Cex2StorageResult{Status::IoError};
	FILE_STANDARD_INFO standard{};
	if (!::GetFileInformationByHandleEx(input.get(), FileStandardInfo, &standard, sizeof(standard)) ||
		standard.EndOfFile.QuadPart < 0 ||
		static_cast<std::uint64_t>(standard.EndOfFile.QuadPart) > kMaximumConfigBytes + 48)
		return {Status::IoError};
	bytes.resize(static_cast<std::size_t>(standard.EndOfFile.QuadPart));
	DWORD read{};
	if (!bytes.empty() && (!::ReadFile(input.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) ||
		read != bytes.size())) return {Status::IoError};
#endif
	if (bytes.size() < 48 || std::memcmp(bytes.data(), "CEX2CFG\0", 8) != 0) return {Status::IoError};
	Decoder header({bytes.data() + 8, 8});
	std::uint16_t version{}, endian{}; std::uint32_t size{};
	if (!header.U16(version) || !header.U16(endian) || !header.U32(size) || version != 1 || endian != 0x4245 ||
		size != bytes.size() - 48) return {Status::IoError};
	const auto digest = Digest({bytes.data() + 48, size});
	if (std::memcmp(digest.data(), bytes.data() + 16, digest.size()) != 0) return {Status::IoError};
	Decoder decoder({bytes.data() + 48, size});
	std::uint32_t count{};
	if (!decoder.U32(count) || count > kMaximumConfigEntries) return {Status::IoError};
	std::size_t total{};
	for (std::uint32_t i = 0; i < count; ++i)
	{
		std::string key; std::uint8_t type{}; std::uint32_t valueSize{}; std::span<const std::byte> value;
		if (!decoder.String(key) || !decoder.U8(type) || !decoder.U32(valueSize) || !decoder.Bytes(valueSize, value) ||
			key.empty() || key.size() > 256 || !IsValidUtf8(key) || values.contains(key) ||
			!ValidConfigValue(static_cast<ValueType>(type), value)) return {Status::IoError};
		total += key.size() + value.size();
		if (total > kMaximumConfigBytes) return {Status::IoError};
		values.emplace(std::move(key), std::pair{static_cast<ValueType>(type), std::vector<std::byte>(value.begin(), value.end())});
	}
	return decoder.remaining() ? Cex2StorageResult{Status::IoError} : Cex2StorageResult{Status::Ok};
}

Cex2StorageResult SaveConfig(const fs::path& root, const ConfigMap& values)
{
	Encoder payload; payload.U32(static_cast<std::uint32_t>(values.size()));
	for (const auto& [key, typed] : values)
	{
		payload.String(key); payload.U8(static_cast<std::uint8_t>(typed.first));
		payload.U32(static_cast<std::uint32_t>(typed.second.size())); payload.Bytes(typed.second);
	}
	Encoder file;
	file.Bytes({reinterpret_cast<const std::byte*>("CEX2CFG\0"), 8}); file.U16(1); file.U16(0x4245);
	file.U32(static_cast<std::uint32_t>(payload.data().size()));
	const auto digest = Digest(payload.data());
	file.Bytes({reinterpret_cast<const std::byte*>(digest.data()), digest.size()}); file.Bytes(payload.data());
	std::error_code error; fs::create_directories(root, error); if (error) return {Status::IoError};
#ifndef _WIN32
	Fd directory(::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	if (!directory) return {Status::IoError};
	std::random_device random;
	const auto temporary = fmt::format(".config-{:08x}.tmp", random());
	Fd output(::openat(directory.get(), temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
	if (!output) return {Status::IoError};
	std::size_t written{};
	while (written < file.data().size())
	{
		const auto count = ::write(output.get(), file.data().data() + written, file.data().size() - written);
		if (count <= 0) { ::unlinkat(directory.get(), temporary.c_str(), 0); return {Status::IoError}; }
		written += count;
	}
	if (::fsync(output.get()) != 0 || ::renameat(directory.get(), temporary.c_str(), directory.get(), "config.cxc2") != 0 ||
		::fsync(directory.get()) != 0)
	{ ::unlinkat(directory.get(), temporary.c_str(), 0); return {Status::IoError}; }
#else
	auto directory = OpenRoot(root);
	if (!directory) return {Status::IoError};
	const auto temporary = fmt::format(".config-{:08x}.tmp", std::random_device{}());
	DWORD winError{};
	auto output = OpenRelative(directory.get(), temporary, false, true,
		GENERIC_READ | GENERIC_WRITE | DELETE | FILE_READ_ATTRIBUTES, winError, true);
	if (!output) return {Status::IoError};
	DWORD written{};
	if (!::WriteFile(output.get(), file.data().data(), static_cast<DWORD>(file.data().size()),
		&written, nullptr) || written != file.data().size() || !::FlushFileBuffers(output.get()))
		return {Status::IoError};
	const std::wstring destination = L"config.cxc2";
	const auto renameBytes = offsetof(FILE_RENAME_INFO, FileName) + destination.size() * sizeof(wchar_t);
	std::vector<std::byte> renameStorage(renameBytes);
	auto* rename = reinterpret_cast<FILE_RENAME_INFO*>(renameStorage.data());
	rename->ReplaceIfExists = TRUE;
	rename->RootDirectory = directory.get();
	rename->FileNameLength = static_cast<DWORD>(destination.size() * sizeof(wchar_t));
	std::memcpy(rename->FileName, destination.data(), rename->FileNameLength);
	if (!::SetFileInformationByHandle(output.get(), FileRenameInfo, rename,
		static_cast<DWORD>(renameBytes))) return {Status::IoError};
#endif
	return {Status::Ok};
}

Cex2StorageResult Config(std::uint64_t title, std::string_view principal, std::uint16_t operation,
	std::span<const std::byte> payload)
{
	Decoder decoder(payload); std::string key;
	if (!decoder.String(key) || key.size() > 256 || !IsValidUtf8(key) ||
		(key.empty() && operation != static_cast<std::uint16_t>(ConfigurationOperation::List))) return {Status::InvalidArgument};
	const auto root = NamespaceRoot(title, principal, "config");
	const ConfigCacheKey cacheKey{title, PrincipalHash(principal)};
	auto cached = s_configCache.find(cacheKey);
	if (cached == s_configCache.end())
	{
		ConfigMap values;
		if (auto loaded = LoadConfig(root, values); loaded.status != Status::Ok) return loaded;
		if (s_configCache.size() >= kMaximumCachedNamespaces)
			s_configCache.erase(s_configCache.begin());
		cached = s_configCache.emplace(cacheKey, std::move(values)).first;
	}
	auto& values = cached->second;
	if (operation == static_cast<std::uint16_t>(ConfigurationOperation::Get))
	{
		if (decoder.remaining()) return {Status::InvalidArgument};
		const auto found = values.find(key); if (found == values.end()) return {Status::NotFound};
		Encoder out; out.U8(static_cast<std::uint8_t>(found->second.first)); out.U32(found->second.second.size()); out.Bytes(found->second.second);
		return {Status::Ok, out.Take()};
	}
	if (operation == static_cast<std::uint16_t>(ConfigurationOperation::Set))
	{
		std::uint8_t type{}; std::uint32_t size{}; std::span<const std::byte> value;
		if (!decoder.U8(type) || !decoder.U32(size) || !decoder.Bytes(size, value) || decoder.remaining() ||
			!ValidConfigValue(static_cast<ValueType>(type), value)) return {Status::InvalidArgument};
		if (!values.contains(key) && values.size() >= kMaximumConfigEntries) return {Status::TooLarge};
		std::size_t total = key.size() + value.size(); for (const auto& [other, item] : values) if (other != key) total += other.size() + item.second.size();
		if (total > kMaximumConfigBytes) return {Status::TooLarge};
		const auto existing = values.find(key);
		const bool replacing = existing != values.end();
		std::optional<std::pair<ValueType, std::vector<std::byte>>> previous;
		if (replacing) previous = existing->second;
		values[key] = {static_cast<ValueType>(type), {value.begin(), value.end()}};
		auto saved = SaveConfig(root, values);
		if (saved.status != Status::Ok)
		{
			if (previous) values[key] = std::move(*previous);
			else values.erase(key);
		}
		return saved;
	}
	if (operation == static_cast<std::uint16_t>(ConfigurationOperation::Delete))
	{
		if (decoder.remaining()) return {Status::InvalidArgument};
		auto removed = values.extract(key);
		if (removed.empty()) return {Status::NotFound};
		auto saved = SaveConfig(root, values);
		if (saved.status != Status::Ok) values.insert(std::move(removed));
		return saved;
	}
	if (operation != static_cast<std::uint16_t>(ConfigurationOperation::List)) return {Status::NotSupported};
	std::uint16_t maximum{}; std::uint32_t tokenSize{}; std::span<const std::byte> token;
	if (!decoder.U16(maximum) || !decoder.U32(tokenSize) || !decoder.Bytes(tokenSize, token) || decoder.remaining() ||
		maximum == 0 || maximum > kMaximumPageEntries || tokenSize > 256) return {Status::InvalidArgument};
	const std::string after(reinterpret_cast<const char*>(token.data()), token.size());
	Encoder out; std::vector<ConfigMap::const_iterator> page;
	for (auto it = after.empty() ? values.lower_bound(key) : values.upper_bound(after); it != values.end() && it->first.starts_with(key); ++it)
	{ if (page.size() == maximum) break; page.push_back(it); }
	bool truncated = false; if (!page.empty()) { auto next = std::next(page.back()); truncated = next != values.end() && next->first.starts_with(key); }
	out.U32(page.size()); for (auto it : page) { out.String(it->first); out.U8(static_cast<std::uint8_t>(it->second.first)); }
	out.U8(truncated); const auto nextToken = truncated ? page.back()->first : std::string{}; out.U32(nextToken.size()); out.Bytes({reinterpret_cast<const std::byte*>(nextToken.data()), nextToken.size()});
	return {Status::Ok, out.Take()};
}

#ifndef _WIN32
Cex2StorageResult File(std::uint64_t title, std::string_view principal, std::uint16_t operation,
	std::span<const std::byte> payload)
{
	Decoder decoder(payload); std::string pathText;
	if (!decoder.String(pathText)) return {Status::InvalidArgument};
	const bool list = operation == static_cast<std::uint16_t>(FileOperation::List);
	const auto parts = SplitPath(pathText, list); if (!parts) return {Status::PermissionDenied};
	Fd root = OpenRoot(NamespaceRoot(title, principal, "files")); if (!root) return {Status::IoError};
	if (operation == static_cast<std::uint16_t>(FileOperation::Mkdir))
	{
		if (decoder.remaining() || parts->empty()) return {Status::InvalidArgument};
		Fd current(::dup(root.get()));
		for (const auto& part : *parts) { if (::mkdirat(current.get(), part.c_str(), 0700) != 0 && errno != EEXIST) return {ErrnoStatus(errno)}; Fd next(::openat(current.get(), part.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)); if (!next) return {ErrnoStatus(errno)}; current = std::move(next); }
		return {Status::Ok};
	}
	if (parts->empty() && !list) return {Status::PermissionDenied};
	Fd parent = parts->empty() ? Fd(::dup(root.get())) : OpenParent(root.get(), *parts,
		operation == static_cast<std::uint16_t>(FileOperation::Write));
	if (!parent) return {ErrnoStatus(errno)};
	const char* name = parts->empty() ? "." : parts->back().c_str();
	if (operation == static_cast<std::uint16_t>(FileOperation::Stat))
	{
		if (decoder.remaining()) return {Status::InvalidArgument}; struct stat stat{};
		if (::fstatat(parent.get(), name, &stat, AT_SYMLINK_NOFOLLOW) != 0) return {ErrnoStatus(errno)};
		if (!S_ISREG(stat.st_mode) && !S_ISDIR(stat.st_mode)) return {Status::PermissionDenied};
		FileStatPayload out{};
		out.size = S_ISREG(stat.st_mode) ? static_cast<std::uint64_t>(stat.st_size) : 0;
#if defined(__APPLE__)
		const auto modified = stat.st_mtimespec;
#else
		const auto modified = stat.st_mtim;
#endif
		if (modified.tv_sec >= 0 && modified.tv_nsec >= 0 && modified.tv_nsec < 1'000'000'000L &&
			static_cast<std::uint64_t>(modified.tv_sec) <=
				(std::numeric_limits<std::uint64_t>::max() - modified.tv_nsec) / 1'000'000'000ULL)
			out.modifiedTimeNs = static_cast<std::uint64_t>(modified.tv_sec) * 1'000'000'000ULL +
				static_cast<std::uint64_t>(modified.tv_nsec);
		out.mode = S_ISDIR(stat.st_mode) ? 0700 : 0600;
		out.type = S_ISDIR(stat.st_mode) ? 2 : 1;
		return {Status::Ok, {reinterpret_cast<std::byte*>(&out), reinterpret_cast<std::byte*>(&out) + sizeof(out)}};
	}
	if (list)
	{
		std::uint16_t maximum{}; std::uint32_t tokenSize{}; std::span<const std::byte> token;
		if (!decoder.U16(maximum) || !decoder.U32(tokenSize) || !decoder.Bytes(tokenSize, token) || decoder.remaining() || maximum == 0 || maximum > kMaximumPageEntries || tokenSize > 255) return {Status::InvalidArgument};
		Fd directory(parts->empty() ? ::dup(root.get()) : ::openat(parent.get(), name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)); if (!directory) return {ErrnoStatus(errno)};
		DIR* stream = ::fdopendir(::dup(directory.get())); if (!stream) return {Status::IoError};
		struct Entry { std::string name; std::uint8_t type; std::uint64_t size; }; std::vector<Entry> entries;
		while (auto* item = ::readdir(stream)) { if (!std::strcmp(item->d_name, ".") || !std::strcmp(item->d_name, "..")) continue; struct stat stat{}; if (::fstatat(directory.get(), item->d_name, &stat, AT_SYMLINK_NOFOLLOW) != 0 || (!S_ISREG(stat.st_mode) && !S_ISDIR(stat.st_mode))) { ::closedir(stream); return {Status::IoError}; } entries.push_back({item->d_name, S_ISDIR(stat.st_mode) ? std::uint8_t{2} : std::uint8_t{1}, S_ISREG(stat.st_mode) ? static_cast<std::uint64_t>(stat.st_size) : 0}); }
		if (::closedir(stream) != 0) return {Status::IoError}; std::ranges::sort(entries, {}, &Entry::name);
		const std::string after(reinterpret_cast<const char*>(token.data()), token.size()); auto begin = std::ranges::upper_bound(entries, after, {}, &Entry::name); const auto count = std::min<std::size_t>(maximum, entries.end() - begin); const bool truncated = count < static_cast<std::size_t>(entries.end() - begin);
		Encoder out; out.U32(count); for (auto it = begin; it != begin + count; ++it) { out.String(it->name); out.U8(it->type); out.U64(it->size); } out.U8(truncated); const auto next = truncated ? (begin + count - 1)->name : std::string{}; out.U32(next.size()); out.Bytes({reinterpret_cast<const std::byte*>(next.data()), next.size()}); return {Status::Ok, out.Take()};
	}
	if (operation == static_cast<std::uint16_t>(FileOperation::Read))
	{
		std::uint64_t offset{}; std::uint32_t size{}; if (!decoder.U64(offset) || !decoder.U32(size) || decoder.remaining() || size > kMaximumValueBytes || offset > kMaximumFileBytes) return {Status::InvalidArgument};
		Fd file(::openat(parent.get(), name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW)); if (!file) return {ErrnoStatus(errno)}; struct stat stat{}; if (::fstat(file.get(), &stat) != 0 || !S_ISREG(stat.st_mode)) return {Status::PermissionDenied}; if (offset > static_cast<std::uint64_t>(stat.st_size)) return {Status::InvalidArgument}; std::vector<std::byte> out(std::min<std::uint64_t>(size, stat.st_size - offset)); const auto count = ::pread(file.get(), out.data(), out.size(), static_cast<off_t>(offset)); if (count < 0) return {Status::IoError}; out.resize(count); return {Status::Ok, std::move(out)};
	}
	if (operation == static_cast<std::uint16_t>(FileOperation::Write))
	{
		std::uint64_t offset{}; if (!decoder.U64(offset) || decoder.remaining() > kMaximumValueBytes || offset > kMaximumFileBytes || decoder.remaining() > kMaximumFileBytes - offset) return {Status::InvalidArgument}; std::span<const std::byte> data; decoder.Bytes(decoder.remaining(), data);
		Usage usage{}; if (!Measure(root.get(), usage)) return {Status::IoError}; struct stat before{}; const bool exists = ::fstatat(parent.get(), name, &before, AT_SYMLINK_NOFOLLOW) == 0; if (exists && (!S_ISREG(before.st_mode) || before.st_size < 0)) return {Status::PermissionDenied}; const std::uint64_t oldSize = exists ? static_cast<std::uint64_t>(before.st_size) : 0; const auto newSize = std::max<std::uint64_t>(oldSize, offset + data.size()); if (offset > oldSize) return {Status::InvalidArgument};
		if (oldSize > usage.bytes) return {Status::IoError};
		if ((!exists && usage.files >= kMaximumFiles) || newSize > kMaximumFileBytes ||
			usage.bytes - oldSize > kNamespaceBytes - newSize) return {Status::TooLarge};
		Fd file(::openat(parent.get(), name, O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600)); if (!file) return {ErrnoStatus(errno)}; std::size_t done{}; while (done < data.size()) { const auto count = ::pwrite(file.get(), data.data() + done, data.size() - done, static_cast<off_t>(offset + done)); if (count <= 0) return {Status::IoError}; done += count; } return ::fsync(file.get()) == 0 ? Cex2StorageResult{Status::Ok} : Cex2StorageResult{Status::IoError};
	}
	if (operation == static_cast<std::uint16_t>(FileOperation::Remove))
	{
		if (decoder.remaining()) return {Status::InvalidArgument}; struct stat stat{}; if (::fstatat(parent.get(), name, &stat, AT_SYMLINK_NOFOLLOW) != 0) return {ErrnoStatus(errno)}; if (!S_ISREG(stat.st_mode) && !S_ISDIR(stat.st_mode)) return {Status::PermissionDenied}; if (::unlinkat(parent.get(), name, S_ISDIR(stat.st_mode) ? AT_REMOVEDIR : 0) != 0) return {ErrnoStatus(errno)}; return {Status::Ok};
	}
	if (operation == static_cast<std::uint16_t>(FileOperation::Rename))
	{
		std::string destinationText; if (!decoder.String(destinationText) || decoder.remaining()) return {Status::InvalidArgument}; const auto destination = SplitPath(destinationText, false); if (!destination) return {Status::PermissionDenied}; Fd destinationParent = OpenParent(root.get(), *destination, true); if (!destinationParent) return {ErrnoStatus(errno)};
#ifdef SYS_renameat2
		if (::syscall(SYS_renameat2, parent.get(), name, destinationParent.get(), destination->back().c_str(), 1U) != 0) return {ErrnoStatus(errno)};
#elif defined(__APPLE__)
		if (::renameatx_np(parent.get(), name, destinationParent.get(),
			destination->back().c_str(), RENAME_EXCL) != 0) return {ErrnoStatus(errno)};
#else
		return {Status::NotSupported};
#endif
		return {Status::Ok};
	}
	return {Status::NotSupported};
}
#else
Cex2StorageResult File(std::uint64_t title, std::string_view principal, std::uint16_t operation,
	std::span<const std::byte> payload)
{
	Decoder decoder(payload);
	std::string pathText;
	if (!decoder.String(pathText)) return {Status::InvalidArgument};
	const bool list = operation == static_cast<std::uint16_t>(FileOperation::List);
	const auto parts = SplitPath(pathText, list);
	if (!parts) return {Status::PermissionDenied};
	auto root = OpenRoot(NamespaceRoot(title, principal, "files"));
	if (!root) return {Status::IoError};
	DWORD error{};
	if (operation == static_cast<std::uint16_t>(FileOperation::Mkdir))
	{
		if (decoder.remaining() || parts->empty()) return {Status::InvalidArgument};
		auto current = Duplicate(root.get());
		for (const auto& part : *parts)
		{
			auto next = OpenRelative(current.get(), part, true, true,
				FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | FILE_TRAVERSE | FILE_ADD_SUBDIRECTORY,
				error);
			if (!next) return {WinStatus(error)};
			current = std::move(next);
		}
		return {Status::Ok};
	}
	if (parts->empty() && !list) return {Status::PermissionDenied};
	auto parent = parts->empty() ? Duplicate(root.get()) : OpenParent(root.get(), *parts,
		operation == static_cast<std::uint16_t>(FileOperation::Write), error);
	if (!parent) return {WinStatus(error)};
	const auto name = parts->empty() ? std::string_view{} : std::string_view(parts->back());
	if (operation == static_cast<std::uint16_t>(FileOperation::Stat))
	{
		if (decoder.remaining()) return {Status::InvalidArgument};
		bool directory{};
		auto item = OpenAny(parent.get(), name, FILE_READ_ATTRIBUTES, directory, error);
		if (!item) return {WinStatus(error)};
		FILE_STANDARD_INFO standard{};
		if (!::GetFileInformationByHandleEx(item.get(), FileStandardInfo, &standard, sizeof(standard)))
			return {Status::IoError};
		FILE_BASIC_INFO basic{};
		if (!::GetFileInformationByHandleEx(item.get(), FileBasicInfo, &basic, sizeof(basic)))
			return {Status::IoError};
		FileStatPayload out{};
		out.size = directory ? 0 : static_cast<std::uint64_t>(standard.EndOfFile.QuadPart);
		constexpr std::uint64_t kWindowsToUnixEpoch100ns = 116'444'736'000'000'000ULL;
		const auto ticks = basic.LastWriteTime.QuadPart > 0 ?
			static_cast<std::uint64_t>(basic.LastWriteTime.QuadPart) : 0;
		if (ticks >= kWindowsToUnixEpoch100ns &&
			ticks - kWindowsToUnixEpoch100ns <= std::numeric_limits<std::uint64_t>::max() / 100ULL)
			out.modifiedTimeNs = (ticks - kWindowsToUnixEpoch100ns) * 100ULL;
		out.mode = directory ? 0700 : 0600;
		out.type = directory ? 2 : 1;
		return {Status::Ok, {reinterpret_cast<std::byte*>(&out),
			reinterpret_cast<std::byte*>(&out) + sizeof(out)}};
	}
	if (list)
	{
		std::uint16_t maximum{};
		std::uint32_t tokenSize{};
		std::span<const std::byte> token;
		if (!decoder.U16(maximum) || !decoder.U32(tokenSize) || !decoder.Bytes(tokenSize, token) ||
			decoder.remaining() || maximum == 0 || maximum > kMaximumPageEntries || tokenSize > 255)
			return {Status::InvalidArgument};
		auto directory = parts->empty() ? Duplicate(root.get()) : OpenRelative(parent.get(), name,
			true, false, FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | FILE_TRAVERSE, error);
		if (!directory) return {WinStatus(error)};
		std::vector<WinEntry> entries;
		if (!Entries(directory.get(), entries)) return {Status::IoError};
		std::ranges::sort(entries, {}, &WinEntry::name);
		const std::string after(reinterpret_cast<const char*>(token.data()), token.size());
		auto begin = std::ranges::upper_bound(entries, after, {}, &WinEntry::name);
		const auto count = std::min<std::size_t>(maximum, entries.end() - begin);
		const bool truncated = count < static_cast<std::size_t>(entries.end() - begin);
		Encoder out;
		out.U32(count);
		for (auto it = begin; it != begin + count; ++it)
		{ out.String(it->name); out.U8(it->directory ? 2 : 1); out.U64(it->size); }
		out.U8(truncated);
		const auto next = truncated ? (begin + count - 1)->name : std::string{};
		out.U32(next.size());
		out.Bytes({reinterpret_cast<const std::byte*>(next.data()), next.size()});
		return {Status::Ok, out.Take()};
	}
	if (operation == static_cast<std::uint16_t>(FileOperation::Read))
	{
		std::uint64_t offset{};
		std::uint32_t size{};
		if (!decoder.U64(offset) || !decoder.U32(size) || decoder.remaining() ||
			size > kMaximumValueBytes || offset > kMaximumFileBytes ||
			offset > static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max()))
			return {Status::InvalidArgument};
		auto file = OpenRelative(parent.get(), name, false, false,
			GENERIC_READ | FILE_READ_ATTRIBUTES, error);
		if (!file) return {WinStatus(error)};
		FILE_STANDARD_INFO standard{};
		if (!::GetFileInformationByHandleEx(file.get(), FileStandardInfo, &standard, sizeof(standard)) ||
			offset > static_cast<std::uint64_t>(standard.EndOfFile.QuadPart)) return {Status::InvalidArgument};
		LARGE_INTEGER position{}; position.QuadPart = static_cast<LONGLONG>(offset);
		if (!::SetFilePointerEx(file.get(), position, nullptr, FILE_BEGIN)) return {Status::IoError};
		std::vector<std::byte> output(std::min<std::uint64_t>(size,
			static_cast<std::uint64_t>(standard.EndOfFile.QuadPart) - offset));
		DWORD read{};
		if (!output.empty() && !::ReadFile(file.get(), output.data(), static_cast<DWORD>(output.size()), &read, nullptr))
			return {Status::IoError};
		output.resize(read);
		return {Status::Ok, std::move(output)};
	}
	if (operation == static_cast<std::uint16_t>(FileOperation::Write))
	{
		std::uint64_t offset{};
		if (!decoder.U64(offset) || decoder.remaining() > kMaximumValueBytes ||
			offset > kMaximumFileBytes || decoder.remaining() > kMaximumFileBytes - offset)
			return {Status::InvalidArgument};
		std::span<const std::byte> data;
		decoder.Bytes(decoder.remaining(), data);
		Usage usage{};
		if (!Measure(root.get(), usage)) return {Status::IoError};
		auto existing = OpenRelative(parent.get(), name, false, false, FILE_READ_ATTRIBUTES, error);
		const bool existed = static_cast<bool>(existing);
		if (!existed && error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
			return {WinStatus(error)};
		if (!existed && usage.files >= kMaximumFiles) return {Status::TooLarge};
		auto file = OpenRelative(parent.get(), name, false, true,
			GENERIC_READ | GENERIC_WRITE | FILE_READ_ATTRIBUTES, error);
		if (!file) return {WinStatus(error)};
		FILE_STANDARD_INFO standard{};
		if (!::GetFileInformationByHandleEx(file.get(), FileStandardInfo, &standard, sizeof(standard)) ||
			standard.EndOfFile.QuadPart < 0) return {Status::IoError};
		const auto oldSize = static_cast<std::uint64_t>(standard.EndOfFile.QuadPart);
		const auto newSize = std::max<std::uint64_t>(oldSize, offset + data.size());
		if (offset > oldSize || oldSize > usage.bytes || newSize > kMaximumFileBytes ||
			usage.bytes - oldSize > kNamespaceBytes - newSize) return {Status::TooLarge};
		// Re-measure after opening: this also identifies newly created files and
		// prevents the file-count admission from racing our own namespace state.
		Usage afterOpen{};
		if (!Measure(root.get(), afterOpen) || afterOpen.files > kMaximumFiles) return {Status::TooLarge};
		LARGE_INTEGER position{}; position.QuadPart = static_cast<LONGLONG>(offset);
		if (!::SetFilePointerEx(file.get(), position, nullptr, FILE_BEGIN)) return {Status::IoError};
		std::size_t done{};
		while (done < data.size())
		{
			DWORD written{};
			const auto chunk = static_cast<DWORD>(std::min<std::size_t>(data.size() - done,
				std::numeric_limits<DWORD>::max()));
			if (!::WriteFile(file.get(), data.data() + done, chunk, &written, nullptr) || written == 0)
				return {Status::IoError};
			done += written;
		}
		return ::FlushFileBuffers(file.get()) ? Cex2StorageResult{Status::Ok} :
			Cex2StorageResult{Status::IoError};
	}
	if (operation == static_cast<std::uint16_t>(FileOperation::Remove))
	{
		if (decoder.remaining()) return {Status::InvalidArgument};
		bool directory{};
		auto item = OpenAny(parent.get(), name, DELETE | FILE_READ_ATTRIBUTES, directory, error);
		if (!item) return {WinStatus(error)};
		FILE_DISPOSITION_INFO disposition{TRUE};
		if (!::SetFileInformationByHandle(item.get(), FileDispositionInfo, &disposition, sizeof(disposition)))
			return {WinStatus(::GetLastError())};
		return {Status::Ok};
	}
	if (operation == static_cast<std::uint16_t>(FileOperation::Rename))
	{
		std::string destinationText;
		if (!decoder.String(destinationText) || decoder.remaining()) return {Status::InvalidArgument};
		const auto destination = SplitPath(destinationText, false);
		if (!destination) return {Status::PermissionDenied};
		auto destinationParent = OpenParent(root.get(), *destination, true, error);
		if (!destinationParent) return {WinStatus(error)};
		bool directory{};
		auto source = OpenAny(parent.get(), name, DELETE | FILE_READ_ATTRIBUTES, directory, error);
		if (!source) return {WinStatus(error)};
		const auto wideName = Wide(destination->back());
		if (wideName.empty()) return {Status::InvalidArgument};
		const auto bytes = offsetof(FILE_RENAME_INFO, FileName) + wideName.size() * sizeof(wchar_t);
		std::vector<std::byte> storage(bytes);
		auto* rename = reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
		rename->ReplaceIfExists = FALSE;
		rename->RootDirectory = destinationParent.get();
		rename->FileNameLength = static_cast<DWORD>(wideName.size() * sizeof(wchar_t));
		std::memcpy(rename->FileName, wideName.data(), rename->FileNameLength);
		if (!::SetFileInformationByHandle(source.get(), FileRenameInfo, rename, static_cast<DWORD>(bytes)))
			return {WinStatus(::GetLastError())};
		return {Status::Ok};
	}
	return {Status::NotSupported};
}
#endif

} // namespace

bool Cex2Storage::ValidateConfigurationFormat(std::span<const std::byte> bytes)
{
	if (bytes.size() < 48 || bytes.size() > kMaximumConfigBytes + 48 ||
		std::memcmp(bytes.data(), "CEX2CFG\0", 8) != 0)
		return false;
	Decoder header({bytes.data() + 8, 8});
	std::uint16_t version{}, endian{};
	std::uint32_t size{};
	if (!header.U16(version) || !header.U16(endian) || !header.U32(size) || version != 1 ||
		endian != 0x4245 || size != bytes.size() - 48)
		return false;
	const auto digest = Digest({bytes.data() + 48, size});
	if (std::memcmp(digest.data(), bytes.data() + 16, digest.size()) != 0)
		return false;
	Decoder decoder({bytes.data() + 48, size});
	std::uint32_t count{};
	if (!decoder.U32(count) || count > kMaximumConfigEntries)
		return false;
	ConfigMap values;
	std::size_t total{};
	for (std::uint32_t index = 0; index < count; ++index)
	{
		std::string key;
		std::uint8_t type{};
		std::uint32_t valueSize{};
		std::span<const std::byte> value;
		if (!decoder.String(key) || !decoder.U8(type) || !decoder.U32(valueSize) ||
			!decoder.Bytes(valueSize, value) || key.empty() || key.size() > 256 ||
			!IsValidUtf8(key) || values.contains(key) ||
			!ValidConfigValue(static_cast<ValueType>(type), value))
			return false;
		total += key.size() + value.size();
		if (total > kMaximumConfigBytes)
			return false;
		values.emplace(std::move(key),
			std::pair{static_cast<ValueType>(type), std::vector<std::byte>(value.begin(), value.end())});
	}
	return decoder.remaining() == 0;
}

Cex2StorageResult Cex2Storage::Dispatch(std::uint64_t titleId, std::string_view principal,
	ServiceId service, std::uint16_t operation, std::span<const std::byte> payload)
{
	std::lock_guard lock(s_storageMutex);
	if (service == ServiceId::Configuration) return Config(titleId, principal, operation, payload);
	if (service == ServiceId::File) return File(titleId, principal, operation, payload);
	return {Status::NotSupported};
}

Status Cex2Storage::ImportLegacy(std::uint64_t titleId, std::string_view principal)
{
#ifdef CEMU_CEX2_TESTING
	return Status::NotSupported;
#else
	if (titleId == 0 || principal.empty()) return Status::InvalidArgument;
	std::lock_guard lock(s_storageMutex);
	const auto oldFiles = ActiveSettings::GetUserDataPath("cemuextend/files/{:016x}", titleId);
	const auto oldConfig = ActiveSettings::GetUserDataPath("cemuextend/config/{:016x}.bin", titleId);
	const auto newFiles = NamespaceRoot(titleId, principal, "files");
	const auto newConfig = NamespaceRoot(titleId, principal, "config");
	std::error_code error;
	const bool hasFiles = fs::exists(oldFiles, error) && !error;
	error.clear();
	const bool hasConfig = fs::exists(oldConfig, error) && !error;
	if (!hasFiles && !hasConfig) return Status::NotFound;
	if (fs::exists(newFiles, error) ||
		(fs::exists(newConfig / "config.cxc2", error))) return Status::Busy;

	ConfigMap values;
	if (hasConfig)
	{
		std::ifstream input(oldConfig, std::ios::binary);
		std::uint32_t magic{}, count{};
		input.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		input.read(reinterpret_cast<char*>(&count), sizeof(count));
		if (!input || magic != 0x43455843U || count > kMaximumConfigEntries)
			return Status::IoError;
		std::size_t total{};
		for (std::uint32_t index = 0; index < count; ++index)
		{
			std::uint8_t type{};
			std::uint16_t keySize{};
			std::uint32_t valueSize{};
			input.read(reinterpret_cast<char*>(&type), sizeof(type));
			input.read(reinterpret_cast<char*>(&keySize), sizeof(keySize));
			input.read(reinterpret_cast<char*>(&valueSize), sizeof(valueSize));
			if (!input || keySize == 0 || keySize > 256 || valueSize > kMaximumValueBytes ||
				keySize + static_cast<std::size_t>(valueSize) > kMaximumConfigBytes - total)
				return Status::IoError;
			std::string key(keySize, '\0');
			std::vector<std::byte> value(valueSize);
			input.read(key.data(), key.size());
			input.read(reinterpret_cast<char*>(value.data()), value.size());
			const auto valueType = static_cast<ValueType>(type);
			if (!input || key.empty() || !IsValidUtf8(key) || values.contains(key) ||
				!ValidConfigValue(valueType, value)) return Status::IoError;
			total += key.size() + value.size();
			values.emplace(std::move(key), std::pair{valueType, std::move(value)});
		}
		if (input.peek() != std::char_traits<char>::eof()) return Status::IoError;
	}

	fs::path temporary;
	if (hasFiles)
	{
		fs::create_directories(newFiles.parent_path(), error);
		if (error) return Status::IoError;
		temporary = newFiles.parent_path() /
			fmt::format(".import-{:08x}.tmp", std::random_device{}());
		if (!fs::create_directory(temporary, error) || error) return Status::IoError;
		Usage usage{};
		for (fs::recursive_directory_iterator iterator(oldFiles,
			fs::directory_options::none, error), end; !error && iterator != end; ++iterator)
		{
			const auto status = iterator->symlink_status(error);
			if (error || fs::is_symlink(status)) break;
			const auto relative = iterator->path().lexically_relative(oldFiles);
			const auto generic = relative.generic_string();
			if (!SplitPath(generic, false)) { error = std::make_error_code(std::errc::permission_denied); break; }
			const auto destination = temporary / relative;
			if (fs::is_directory(status))
				fs::create_directory(destination, error);
			else if (fs::is_regular_file(status))
			{
				const auto size = fs::file_size(iterator->path(), error);
				if (!error && (size > kMaximumFileBytes || usage.files >= kMaximumFiles ||
					usage.bytes > kNamespaceBytes - size))
					error = std::make_error_code(std::errc::file_too_large);
				if (!error && (!fs::copy_file(iterator->path(), destination,
					fs::copy_options::none, error) || error)) break;
				usage.bytes += size; ++usage.files;
			}
			else { error = std::make_error_code(std::errc::permission_denied); break; }
		}
		if (error)
		{
			std::error_code cleanup; fs::remove_all(temporary, cleanup);
			return Status::IoError;
		}
		fs::rename(temporary, newFiles, error);
		if (error) { std::error_code cleanup; fs::remove_all(temporary, cleanup); return Status::IoError; }
	}
	if (hasConfig)
	{
		const auto saved = SaveConfig(newConfig, values);
		if (saved.status != Status::Ok)
		{
			std::error_code cleanup;
			if (hasFiles) fs::remove_all(newFiles, cleanup);
			cleanup.clear();
			fs::remove_all(newConfig, cleanup);
			return saved.status;
		}
	}
	s_configCache.erase(ConfigCacheKey{titleId, PrincipalHash(principal)});
	return Status::Ok;
#endif
}

} // namespace cemuextend_hle
