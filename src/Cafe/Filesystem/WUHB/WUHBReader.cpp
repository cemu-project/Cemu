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

romfs_direntry_t WUHBReader::GetDirEntry(uint32_t offset)
{
	romfs_direntry_t ret;
	if (offset >= m_header.dir_table_size)
	{
		cemuLog_log(LogType::Force, "WUHB direntry offset exceeds table size declared in header");
		cemu_assert_suspicious();
	}

	// read the direntry
	m_fileIn->SetPosition(m_header.dir_table_ofs + offset);
	auto read = m_fileIn->readData(&ret, sizeof(romfs_direntry_t));
	if (read != sizeof(romfs_direntry_t))
	{
		cemuLog_log(LogType::Force, "failed to read WUHB direntry at offset: {}", offset);
		cemu_assert_error();
	}

	// read the name
	ret.name.resize(ret.name_size);
	read = m_fileIn->readData(ret.name.data(), ret.name_size);
	if (read != ret.name_size)
	{
		cemuLog_log(LogType::Force, "failed to read WUHB direntry name");
		cemu_assert_error();
	}

	return ret;
}
romfs_fentry_t WUHBReader::GetFileEntry(uint32_t offset)
{
	romfs_fentry_t ret;
	if (offset >= m_header.file_table_size)
	{
		cemuLog_log(LogType::Force, "WUHB fentry offset exceeds table size declared in header");
		cemu_assert_suspicious();
	}

	// read the fentry
	m_fileIn->SetPosition(m_header.file_table_ofs + offset);
	auto read = m_fileIn->readData(&ret, sizeof(romfs_fentry_t));
	if (read != sizeof(romfs_fentry_t))
	{
		cemuLog_log(LogType::Force, "failed to read WUHB fentry at offset: {}", offset);
		cemu_assert_error();
	}

	// read the name
	ret.name.resize(ret.name_size);
	read = m_fileIn->readData(ret.name.data(), ret.name_size);
	if (read != ret.name_size)
	{
		cemuLog_log(LogType::Force, "failed to read WUHB fentry name");
		cemu_assert_error();
	}

	return ret;
}

uint32_t WUHBReader::Lookup(std::string_view path)
{

	auto currentParent = CalcPathHash(0, 0, 1, 0);
	if(path == "/")
		return currentParent;
	for (const auto part : path | std::views::split('/'))
	{
		std::string test;
		for(auto& i : part)
			test.push_back(i);

		std::cout << test << std::endl;
	}
	return 0;
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
	return std::memcmp(&magic, "WUHB", headerMagicValue.size()) == 0;
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
WUHBReader::~WUHBReader()
{
	delete m_fileIn;
}
unsigned char WUHBReader::NormalizeChar(unsigned char c)
{
	if (c >= 'a' && c <= 'z') {
		return c + 'A' - 'a';
	} else {
		return c;
	}
}
uint32_t WUHBReader::CalcPathHash(uint32_t parent, const unsigned char* path, uint32_t start, size_t path_len)
{
	cemu_assert(path != nullptr || path_len == 0);
	uint32_t hash = parent ^ 123456789;
	for (uint32_t i = 0; i < path_len; i++) {
		hash = (hash >> 5) | (hash << 27);
		hash ^= NormalizeChar(path[start + i]);
	}

	return hash;
}
