#include "FileStream_saf.h"
#include "FilesystemAndroid.h"

FileStreamSAF* FileStreamSAF::OpenFile(const fs::path& path)
{
	auto fileStream = new FileStreamSAF(path);
	if (fileStream->m_isValid)
	{
		return fileStream;
	}

	delete fileStream;
	return nullptr;
}

void FileStreamSAF::SetPosition(uint64 pos)
{
	if (!m_isValid)
		return;
	m_stream.seekg((std::streampos)pos);
}

uint64 FileStreamSAF::GetSize()
{
	cemu_assert(m_isValid);
	auto currentPos = m_stream.tellg();
	m_stream.seekg(0, std::ios::end);
	auto fileSize = m_stream.tellg();
	m_stream.seekg(currentPos, std::ios::beg);
	uint64 fs = (uint64)fileSize;
	return fs;
}

bool FileStreamSAF::SetEndOfFile()
{
	return true;
}

void FileStreamSAF::extract(std::vector<uint8>& data)
{
	if (!m_isValid)
		return;
	uint64 fileSize = GetSize();
	SetPosition(0);
	data.resize(fileSize);
	readData(data.data(), fileSize);
}

uint32 FileStreamSAF::readData(void* data, uint32 length)
{
	m_stream.read((char*)data, length);
	size_t bytesRead = m_stream.gcount();
	return (uint32)bytesRead;
}

bool FileStreamSAF::readU64(uint64& v)
{
	return readData(&v, sizeof(uint64)) == sizeof(uint64);
}

bool FileStreamSAF::readU32(uint32& v)
{
	return readData(&v, sizeof(uint32)) == sizeof(uint32);
}

bool FileStreamSAF::readU8(uint8& v)
{
	return readData(&v, sizeof(uint8)) == sizeof(uint8);
}

bool FileStreamSAF::readLine(std::string& line)
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

sint32 FileStreamSAF::writeData(const void* data, sint32 length)
{
	throw std::runtime_error("write operation not supported");
}

void FileStreamSAF::writeU64(uint64 v)
{
	throw std::runtime_error("write operation not supported");
}

void FileStreamSAF::writeU32(uint32 v)
{
	throw std::runtime_error("write operation not supported");
}

void FileStreamSAF::writeU8(uint8 v)
{
	throw std::runtime_error("write operation not supported");
}

void FileStreamSAF::writeStringFmt(const char* format, ...)
{
	throw std::runtime_error("write operation not supported");
}

void FileStreamSAF::writeString(const char* str)
{
	throw std::runtime_error("write operation not supported");
}

void FileStreamSAF::writeLine(const char* str)
{
	throw std::runtime_error("write operation not supported");
}

void FileStreamSAF::Flush()
{
	throw std::runtime_error("write operation not supported");
}

FileStreamSAF::FileStreamSAF(const fs::path& uri) : m_stream(FilesystemAndroid::OpenContentUri(uri))
{
	m_isValid = m_stream.IsValid();
}
