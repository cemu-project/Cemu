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

std::array<unsigned char, SHA256_DIGEST_LENGTH> Digest(std::span<const std::byte> bytes)
{
	std::array<unsigned char, SHA256_DIGEST_LENGTH> result{};
	SHA256(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), result.data());
	return result;
}

Cex2StorageResult LoadConfig(const fs::path& root, ConfigMap& values)
{
	const auto path = root / "config.cxc2";
	std::ifstream input(path, std::ios::binary);
	if (!input) return fs::exists(path) ? Cex2StorageResult{Status::IoError} : Cex2StorageResult{Status::Ok};
	input.seekg(0, std::ios::end);
	const auto fileSize = input.tellg();
	if (fileSize < 0 || static_cast<std::uint64_t>(fileSize) > kMaximumConfigBytes + 48)
		return {Status::IoError};
	std::vector<std::byte> bytes(static_cast<std::size_t>(fileSize));
	input.seekg(0, std::ios::beg);
	input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if (!input) return {Status::IoError};
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
	const auto temporary = root / fmt::format(".config-{:08x}.tmp", std::random_device{}());
	std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char*>(file.data().data()), file.data().size()); output.flush(); output.close();
	if (!output || !MoveFileExW(temporary.c_str(), (root / "config.cxc2").c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{ fs::remove(temporary, error); return {Status::IoError}; }
#endif
	return {Status::Ok};
}

Cex2StorageResult Config(std::uint64_t title, std::string_view principal, std::uint16_t operation,
	std::span<const std::byte> payload)
{
	Decoder decoder(payload); std::string key;
	if (!decoder.String(key) || key.size() > 256 || !IsValidUtf8(key) ||
		(key.empty() && operation != static_cast<std::uint16_t>(ConfigurationOperation::List))) return {Status::InvalidArgument};
	const auto root = NamespaceRoot(title, principal, "config"); ConfigMap values;
	if (auto loaded = LoadConfig(root, values); loaded.status != Status::Ok) return loaded;
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
		values[key] = {static_cast<ValueType>(type), {value.begin(), value.end()}}; return SaveConfig(root, values);
	}
	if (operation == static_cast<std::uint16_t>(ConfigurationOperation::Delete))
	{
		if (decoder.remaining()) return {Status::InvalidArgument};
		if (!values.erase(key)) return {Status::NotFound}; return SaveConfig(root, values);
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
		FileStatPayload out{}; out.size = S_ISREG(stat.st_mode) ? stat.st_size : 0; out.mode = stat.st_mode & 0777; out.type = S_ISDIR(stat.st_mode) ? 2 : 1;
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
#else
		return {Status::NotSupported};
#endif
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
#ifndef _WIN32
	if (service == ServiceId::File) return File(titleId, principal, operation, payload);
#else
	if (service == ServiceId::File) return {Status::NotSupported};
#endif
	return {Status::NotSupported};
}

} // namespace cemuextend_hle
