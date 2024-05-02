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

template<bool File>
WUHBReader::EntryType<File> WUHBReader::GetEntry(uint32_t offset)
{
	const char* typeName = File ? "fentry" : "direntry";
	EntryType<File> ret;
	if (offset >= (File ? m_header.file_table_size : m_header.dir_table_size))
	{
		cemuLog_log(LogType::Force, "WUHB {} offset exceeds table size declared in header", typeName);
		cemu_assert_suspicious();
	}

	// read the entry
	m_fileIn->SetPosition((File ? m_header.file_table_ofs : m_header.dir_table_ofs) + offset);
	auto read = m_fileIn->readData(&ret, offsetof(EntryType<File>, name));
	if (read != offsetof(EntryType<File>, name))
	{
		cemuLog_log(LogType::Force, "failed to read WUHB {} at offset: {}", typeName, offset);
		cemu_assert_error();
	}

	// read the name
	ret.name.resize(ret.name_size);
	read = m_fileIn->readData(ret.name.data(), ret.name_size);
	if (read != ret.name_size)
	{
		cemuLog_log(LogType::Force, "failed to read WUHB {} name", typeName);
		cemu_assert_error();
	}

	return ret;
}

romfs_direntry_t WUHBReader::GetDirEntry(uint32_t offset)
{
	return GetEntry<false>(offset);
}
romfs_fentry_t WUHBReader::GetFileEntry(uint32_t offset)
{
	return GetEntry<true>(offset);
}

uint64_t WUHBReader::GetFileSize(uint32_t entryOffset)
{
	return GetFileEntry(entryOffset).size;
}

uint64_t WUHBReader::ReadFromFile(uint32_t entryOffset, uint64_t fileOffset, uint64_t length, void* buffer)
{
	const auto fileEntry = GetFileEntry(entryOffset);
	if (fileOffset >= fileEntry.size)
		return 0;
	const auto readAmount = std::min(length, fileEntry.size - fileOffset);
	const auto wuhbOffset = m_header.file_partition_ofs + fileEntry.offset + fileOffset;
	m_fileIn->SetPosition(wuhbOffset);
	return m_fileIn->readData(buffer, readAmount);
}

uint32_t WUHBReader::GetHashTableEntryOffset(uint32_t hash, bool isFile)
{
	const auto hash_table_size = (isFile ? m_header.file_hash_table_size : m_header.dir_hash_table_size);
	const auto hash_table_ofs = (isFile ? m_header.file_hash_table_ofs : m_header.dir_hash_table_ofs);

	const uint64_t hash_table_entry_count = hash_table_size / sizeof(uint32_t);
	const auto hash_table_entry_offset = hash_table_ofs + (hash % hash_table_entry_count) * sizeof(uint32_t);

	m_fileIn->SetPosition(hash_table_entry_offset);
	uint32_t tableOffset;
	if (!m_fileIn->readU32(tableOffset))
	{
		cemuLog_log(LogType::Force, "failed to read WUHB hash table entry at file offset: {}", hash_table_entry_offset);
		cemu_assert_error();
	}

	return uint32be::from_bevalue(tableOffset);
}

template<bool T>
bool WUHBReader::SearchHashList(uint32_t& entryOffset, const fs::path& targetName)
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

uint32_t WUHBReader::Lookup(const std::filesystem::path& path, bool isFile)
{
	uint32_t currentEntryOffset = 0;
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
bool WUHBReader::CheckMagicValue()
{
	uint8_t magic[4];
	m_fileIn->SetPosition(0);
	int read = m_fileIn->readData(magic, 4);
	if (read != 4)
	{
		cemuLog_log(LogType::Force, "Failed to read WUHB magic numbers");
		return false;
	}
	static_assert(sizeof(magic) == headerMagicValue.size());
	return std::memcmp(&magic, headerMagicValue.data(), sizeof(magic)) == 0;
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
uint32_t WUHBReader::CalcPathHash(uint32_t parent, const char* path, uint32_t start, size_t path_len)
{
	cemu_assert(path != nullptr || path_len == 0);
	uint32_t hash = parent ^ 123456789;
	for (uint32_t i = 0; i < path_len; i++)
	{
		hash = (hash >> 5) | (hash << 27);
		hash ^= NormalizeChar(path[start + i]);
	}

	return hash;
}
