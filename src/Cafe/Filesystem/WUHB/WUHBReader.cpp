#include "WUHBReader.h"
WUHBReader* WUHBReader::FromPath(const fs::path& path)
{
	FileStream* fileIn{FileStream::openFile2(path)};
	if (!fileIn)
		return nullptr;

	WUHBReader* ret = new WUHBReader(fileIn);
	if (!ret->CheckMagicValue())
	{
		delete ret;
		return nullptr;
	}

	if (!ret->ReadHeader())
	{
		delete ret;
		return nullptr;
	}

	return ret;
}

static const romfs_direntry_t fallbackDirEntry{
	.parent = ROMFS_ENTRY_EMPTY,
	.listNext = ROMFS_ENTRY_EMPTY,
	.dirListHead = ROMFS_ENTRY_EMPTY,
	.fileListHead = ROMFS_ENTRY_EMPTY,
	.hash = ROMFS_ENTRY_EMPTY,
	.name_size = 0,
	.name = ""
};
static const romfs_fentry_t fallbackFileEntry{
	.parent = ROMFS_ENTRY_EMPTY,
	.listNext = ROMFS_ENTRY_EMPTY,
	.offset = 0,
	.size = 0,
	.hash = ROMFS_ENTRY_EMPTY,
	.name_size = 0,
	.name = ""
};
template<bool File>
const WUHBReader::EntryType<File>& WUHBReader::GetFallback()
{
	if constexpr (File)
		return fallbackFileEntry;
	else
		return fallbackDirEntry;
}

template<bool File>
WUHBReader::EntryType<File> WUHBReader::GetEntry(uint32 offset) const
{
	auto fallback = GetFallback<File>();
	if(offset == ROMFS_ENTRY_EMPTY)
		return fallback;

	const char* typeName = File ? "fentry" : "direntry";
	EntryType<File> ret;
	if (offset >= (File ? m_header.file_table_size : m_header.dir_table_size))
	{
		cemuLog_log(LogType::Force, "WUHB {} offset exceeds table size declared in header", typeName);
		return fallback;
	}

	// read the entry
	m_fileIn->SetPosition((File ? m_header.file_table_ofs : m_header.dir_table_ofs) + offset);
	auto read = m_fileIn->readData(&ret, offsetof(EntryType<File>, name));
	if (read != offsetof(EntryType<File>, name))
	{
		cemuLog_log(LogType::Force, "failed to read WUHB {} at offset: {}", typeName, offset);
		return fallback;
	}

	// read the name
	ret.name.resize(ret.name_size);
	read = m_fileIn->readData(ret.name.data(), ret.name_size);
	if (read != ret.name_size)
	{
		cemuLog_log(LogType::Force, "failed to read WUHB {} name", typeName);
		return fallback;
	}

	return ret;
}

romfs_direntry_t WUHBReader::GetDirEntry(uint32 offset) const
{
	return GetEntry<false>(offset);
}
romfs_fentry_t WUHBReader::GetFileEntry(uint32 offset) const
{
	return GetEntry<true>(offset);
}

uint64 WUHBReader::GetFileSize(uint32 entryOffset) const
{
	return GetFileEntry(entryOffset).size;
}

uint64 WUHBReader::ReadFromFile(uint32 entryOffset, uint64 fileOffset, uint64 length, void* buffer) const
{
	const auto fileEntry = GetFileEntry(entryOffset);
	if (fileOffset >= fileEntry.size)
		return 0;
	const uint64 readAmount = std::min(length, fileEntry.size - fileOffset);
	const uint64 wuhbOffset = m_header.file_partition_ofs + fileEntry.offset + fileOffset;
	m_fileIn->SetPosition(wuhbOffset);
	return m_fileIn->readData(buffer, readAmount);
}

uint32 WUHBReader::GetHashTableEntryOffset(uint32 hash, bool isFile) const
{
	const uint64 hash_table_size = (isFile ? m_header.file_hash_table_size : m_header.dir_hash_table_size);
	const uint64 hash_table_ofs = (isFile ? m_header.file_hash_table_ofs : m_header.dir_hash_table_ofs);

	const uint64 hash_table_entry_count = hash_table_size / sizeof(uint32);
	const uint64 hash_table_entry_offset = hash_table_ofs + (hash % hash_table_entry_count) * sizeof(uint32);

	m_fileIn->SetPosition(hash_table_entry_offset);
	uint32 tableOffset;
	if (!m_fileIn->readU32(tableOffset))
	{
		cemuLog_log(LogType::Force, "failed to read WUHB hash table entry at file offset: {}", hash_table_entry_offset);
		return ROMFS_ENTRY_EMPTY;
	}

	return uint32be::from_bevalue(tableOffset);
}

template<bool T>
bool WUHBReader::SearchHashList(uint32& entryOffset, const fs::path& targetName) const
{
	for (;;)
	{
		if (entryOffset == ROMFS_ENTRY_EMPTY)
			return false;
		auto entry = GetEntry<T>(entryOffset);

		if (entry.name == targetName)
			return true;
		entryOffset = entry.hash;
	}
	return false;
}

uint32 WUHBReader::Lookup(const std::filesystem::path& path, bool isFile) const
{
	uint32 currentEntryOffset = 0;
	auto look = [&](const fs::path& part, bool lookInFileHT) {
		const auto partString = part.string();
		currentEntryOffset = GetHashTableEntryOffset(CalcPathHash(currentEntryOffset, partString.c_str(), 0, partString.size()), lookInFileHT);
		if (lookInFileHT)
			return SearchHashList<true>(currentEntryOffset, part);
		else
			return SearchHashList<false>(currentEntryOffset, part);
	};
	// look for the root entry
	if (!look("", false))
		return ROMFS_ENTRY_EMPTY;

	auto it = path.begin();
	while (it != path.end())
	{
		fs::path part = *it;
		++it;
		// no need to recurse after trailing forward slash (e.g. directory/)
		if (part.empty() && !isFile)
			break;
		// skip leading forward slash
		if (part == "/")
			continue;

		// if the lookup target is a file and this is the last iteration, look in the file hash table instead.
		if (!look(part, it == path.end() && isFile))
			return ROMFS_ENTRY_EMPTY;
	}
	return currentEntryOffset;
}
bool WUHBReader::CheckMagicValue() const
{
	uint8 magic[4];
	m_fileIn->SetPosition(0);
	int read = m_fileIn->readData(magic, 4);
	if (read != 4)
	{
		cemuLog_log(LogType::Force, "Failed to read WUHB magic numbers");
		return false;
	}
	static_assert(sizeof(magic) == s_headerMagicValue.size());
	return std::memcmp(&magic, s_headerMagicValue.data(), sizeof(magic)) == 0;
}
bool WUHBReader::ReadHeader()
{
	m_fileIn->SetPosition(0);
	auto read = m_fileIn->readData(&m_header, sizeof(m_header));
	auto readSuccess = read == sizeof(m_header);
	if (!readSuccess)
		cemuLog_log(LogType::Force, "Failed to read WUHB header");
	return readSuccess;
}
unsigned char WUHBReader::NormalizeChar(unsigned char c)
{
	if (c >= 'a' && c <= 'z')
	{
		return c + 'A' - 'a';
	}
	else
	{
		return c;
	}
}
uint32 WUHBReader::CalcPathHash(uint32 parent, const char* path, uint32 start, size_t path_len)
{
	cemu_assert(path != nullptr || path_len == 0);
	uint32 hash = parent ^ 123456789;
	for (uint32 i = 0; i < path_len; i++)
	{
		hash = (hash >> 5) | (hash << 27);
		hash ^= NormalizeChar(path[start + i]);
	}

	return hash;
}
