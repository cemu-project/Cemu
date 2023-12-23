#include "FileStream_contentUri.h"

#include "unix/ContentUriIStream.h"

void ThrowWriteNotSupportedError()
{
    throw std::logic_error("write operation not supported");
}

void FileStreamContentUri::SetPosition(uint64 pos)
{
    if (!m_isValid)
        return;
    m_contentUriIStream.seekg((std::streampos)pos);
}

uint64 FileStreamContentUri::GetSize()
{
    cemu_assert(m_isValid);
    auto currentPos = m_contentUriIStream.tellg();
    m_contentUriIStream.seekg(0, std::ios::end);
    auto fileSize = m_contentUriIStream.tellg();
    m_contentUriIStream.seekg(currentPos, std::ios::beg);
    uint64 fs = (uint64)fileSize;
    return fs;
}

bool FileStreamContentUri::SetEndOfFile()
{
    return true;
}

void FileStreamContentUri::extract(std::vector<uint8>& data)
{
    if (!m_isValid)
        return;
    uint64 fileSize = GetSize();
    SetPosition(0);
    data.resize(fileSize);
    readData(data.data(), fileSize);
}

uint32 FileStreamContentUri::readData(void* data, uint32 length)
{
    m_contentUriIStream.read((char*)data, length);
    size_t bytesRead = m_contentUriIStream.gcount();
    return (uint32)bytesRead;
}

bool FileStreamContentUri::readU64(uint64& v)
{
    return readData(&v, sizeof(uint64)) == sizeof(uint64);
}

bool FileStreamContentUri::readU32(uint32& v)
{
    return readData(&v, sizeof(uint32)) == sizeof(uint32);
}

bool FileStreamContentUri::readU8(uint8& v)
{
    return readData(&v, sizeof(uint8)) == sizeof(uint8);
}

bool FileStreamContentUri::readLine(std::string& line)
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

sint32 FileStreamContentUri::writeData(const void* data, sint32 length)
{
    ThrowWriteNotSupportedError();
    return -1;
}

void FileStreamContentUri::writeU64(uint64 v)
{
    ThrowWriteNotSupportedError();
}

void FileStreamContentUri::writeU32(uint32 v)
{
    ThrowWriteNotSupportedError();
}

void FileStreamContentUri::writeU8(uint8 v)
{
    ThrowWriteNotSupportedError();
}

void FileStreamContentUri::writeStringFmt(const char* format, ...)
{
    ThrowWriteNotSupportedError();
}

void FileStreamContentUri::writeString(const char* str)
{
    ThrowWriteNotSupportedError();
}

void FileStreamContentUri::writeLine(const char* str)
{
    ThrowWriteNotSupportedError();
}

FileStreamContentUri::FileStreamContentUri(const std::string& uri)
    : m_contentUriIStream(uri)
{
    m_isValid = m_contentUriIStream.isOpen();
}
