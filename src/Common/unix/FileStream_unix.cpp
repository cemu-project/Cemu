#include "Common/unix/FileStream_unix.h"
#include <cstdarg>

fs::path findPathCI(const fs::path& path)
{
	if (fs::exists(path)) return path;

	fs::path fName = path.filename();
	fs::path parentPath = path.parent_path();
	if (!fs::exists(parentPath))
	{
		auto CIParent = findPathCI(parentPath);
		if (fs::exists(CIParent))
			return findPathCI(CIParent / fName);
	}

	std::error_code listErr;
	for (auto&& dirEntry : fs::directory_iterator(parentPath, listErr))
		if (boost::iequals(dirEntry.path().filename().string(), fName.string()))
			return dirEntry;

	return parentPath / fName;
}

void FileStreamUnix::SetPosition(uint64 pos)
{
	cemu_assert(m_isValid);
	if (m_prevOperationWasWrite)
		m_fileStream.seekp((std::streampos)pos);
	else
		m_fileStream.seekg((std::streampos)pos);
}

uint64 FileStreamUnix::GetSize()
{
	cemu_assert(m_isValid);
	auto currentPos = m_fileStream.tellg();
	m_fileStream.seekg(0, std::ios::end);
	auto fileSize = m_fileStream.tellg();
	m_fileStream.seekg(currentPos, std::ios::beg);
	uint64 fs = (uint64)fileSize;
	return fs;
}

bool FileStreamUnix::SetEndOfFile()
{
	assert_dbg();
	return true;
	//return ::SetEndOfFile(m_hFile) != 0;
}

void FileStreamUnix::extract(std::vector<uint8>& data)
{
	uint64 fileSize = GetSize();
	SetPosition(0);
	data.resize(fileSize);
	readData(data.data(), fileSize);
}

uint32 FileStreamUnix::readData(void* data, uint32 length)
{
	SyncReadWriteSeek(false);
	m_fileStream.read((char*)data, length);
	size_t bytesRead = m_fileStream.gcount();
	return (uint32)bytesRead;
}

bool FileStreamUnix::readU64(uint64& v)
{
	return readData(&v, sizeof(uint64)) == sizeof(uint64);
}

bool FileStreamUnix::readU32(uint32& v)
{
	return readData(&v, sizeof(uint32)) == sizeof(uint32);
}

bool FileStreamUnix::readU8(uint8& v)
{
	return readData(&v, sizeof(uint8)) == sizeof(uint8);
}

bool FileStreamUnix::readLine(std::string& line)
{
	line.clear();
	uint8 c;
	bool isEOF = true;
	while (readU8(c))
	{
		isEOF = false;
		if (c == '\r')
			continue;
		if (c == '\n')
			break;
		line.push_back((char)c);
	}
	return !isEOF;
}

sint32 FileStreamUnix::writeData(const void* data, sint32 length)
{
	SyncReadWriteSeek(true);
	m_fileStream.write((const char*)data, length);
	return length;
}

void FileStreamUnix::writeU64(uint64 v)
{
	writeData(&v, sizeof(uint64));
}

void FileStreamUnix::writeU32(uint32 v)
{
	writeData(&v, sizeof(uint32));
}

void FileStreamUnix::writeU8(uint8 v)
{
	writeData(&v, sizeof(uint8));
}

void FileStreamUnix::writeStringFmt(const char* format, ...)
{
	char buffer[2048];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	writeData(buffer, (sint32)strlen(buffer));
}

void FileStreamUnix::writeString(const char* str)
{
	writeData(str, (sint32)strlen(str));
}

void FileStreamUnix::writeLine(const char* str)
{
	writeData(str, (sint32)strlen(str));
	writeData("\r\n", 2);
}

FileStreamUnix::~FileStreamUnix()
{
	if (m_isValid)
	{
		m_fileStream.close();
	}
	//	CloseHandle(m_hFile);
}

FileStreamUnix::FileStreamUnix(const fs::path& path, bool isOpen, bool isWriteable)
{
	fs::path CIPath = findPathCI(path);
	if (isOpen)
	{
		m_fileStream.open(CIPath, isWriteable ? (std::ios_base::in | std::ios_base::out | std::ios_base::binary) : (std::ios_base::in | std::ios_base::binary));
		m_isValid = m_fileStream.is_open();
	}
	else
	{
		m_fileStream.open(CIPath, std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
		m_isValid = m_fileStream.is_open();
	}
	if(m_isValid && fs::is_directory(path))
	{
		m_isValid = false;
		m_fileStream.close();
	}
}

void FileStreamUnix::SyncReadWriteSeek(bool nextOpIsWrite)
{
	// nextOpIsWrite == false -> read. Otherwise write
	if (nextOpIsWrite == m_prevOperationWasWrite)
		return;
	if (nextOpIsWrite)
		m_fileStream.seekp(m_fileStream.tellg(), std::ios::beg);
	else
		m_fileStream.seekg(m_fileStream.tellp(), std::ios::beg);

	m_prevOperationWasWrite = nextOpIsWrite;
}
