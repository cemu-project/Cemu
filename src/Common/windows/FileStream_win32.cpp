#include "Common/windows/FileStream_win32.h"

FileStream* FileStream::openFile(std::string_view path)
{
	HANDLE hFile = CreateFileW(boost::nowide::widen(path.data(), path.size()).c_str(), FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return nullptr;
	return new FileStream(hFile);
}

FileStream* FileStream::openFile(const wchar_t* path, bool allowWrite)
{
	HANDLE hFile = CreateFileW(path, allowWrite ? (FILE_GENERIC_READ | FILE_GENERIC_WRITE) : FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return nullptr;
	return new FileStream(hFile);
}

FileStream* FileStream::openFile2(const fs::path& path, bool allowWrite)
{
	return openFile(path.generic_wstring().c_str(), allowWrite);
}

FileStream* FileStream::createFile(const wchar_t* path)
{
	HANDLE hFile = CreateFileW(path, FILE_GENERIC_READ | FILE_GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return nullptr;
	return new FileStream(hFile);
}

FileStream* FileStream::createFile(std::string_view path)
{
	auto w = boost::nowide::widen(path.data(), path.size());
	HANDLE hFile = CreateFileW(w.c_str(), FILE_GENERIC_READ | FILE_GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return nullptr;
	return new FileStream(hFile);
}

FileStream* FileStream::createFile2(const fs::path& path)
{
	return createFile(path.generic_wstring().c_str());
}

std::optional<std::vector<uint8>> FileStream::LoadIntoMemory(const fs::path& path)
{
	FileStream* fs = openFile2(path);
	if (!fs)
		return std::nullopt;
	uint64 fileSize = fs->GetSize();
	if(fileSize > 0xFFFFFFFFull)
	{
		delete fs;
		return std::nullopt;
	}
	std::optional<std::vector<uint8>> v(fileSize);
	if (fs->readData(v->data(), (uint32)fileSize) != (uint32)fileSize)
	{
		delete fs;
		return std::nullopt;
	}
	delete fs;
	return v;
}

void FileStream::SetPosition(uint64 pos)
{
	LONG posHigh = (LONG)(pos >> 32);
	LONG posLow = (LONG)(pos);
	SetFilePointer(m_hFile, posLow, &posHigh, FILE_BEGIN);
}

uint64 FileStream::GetSize()
{
	DWORD fileSizeHigh = 0;
	DWORD fileSizeLow = 0;
	fileSizeLow = GetFileSize(m_hFile, &fileSizeHigh);
	return ((uint64)fileSizeHigh << 32) | (uint64)fileSizeLow;
}

bool FileStream::SetEndOfFile()
{
	return ::SetEndOfFile(m_hFile) != 0;
}

void FileStream::extract(std::vector<uint8>& data)
{
	DWORD fileSize = GetFileSize(m_hFile, nullptr);
	data.resize(fileSize);
	SetFilePointer(m_hFile, 0, 0, FILE_BEGIN);
	DWORD bt;
	ReadFile(m_hFile, data.data(), fileSize, &bt, nullptr);
}

uint32 FileStream::readData(void* data, uint32 length)
{
	DWORD bytesRead = 0;
	ReadFile(m_hFile, data, length, &bytesRead, NULL);
	return bytesRead;
}

bool FileStream::readU64(uint64& v)
{
	return readData(&v, sizeof(uint64)) == sizeof(uint64);
}

bool FileStream::readU32(uint32& v)
{
	return readData(&v, sizeof(uint32)) == sizeof(uint32);
}

bool FileStream::readU8(uint8& v)
{
	return readData(&v, sizeof(uint8)) == sizeof(uint8);
}

bool FileStream::readLine(std::string& line)
{
	line.clear();
	uint8 c;
	bool isEOF = true;
	while (readU8(c))
	{
		isEOF = false;
		if(c == '\r')
			continue;
		if (c == '\n')
			break;
		line.push_back((char)c);
	}
	return !isEOF;
}

sint32 FileStream::writeData(const void* data, sint32 length)
{
	DWORD bytesWritten = 0;
	WriteFile(m_hFile, data, length, &bytesWritten, NULL);
	return bytesWritten;
}

void FileStream::writeU64(uint64 v)
{
	writeData(&v, sizeof(uint64));
}

void FileStream::writeU32(uint32 v)
{
	writeData(&v, sizeof(uint32));
}

void FileStream::writeU8(uint8 v)
{
	writeData(&v, sizeof(uint8));
}

void FileStream::writeStringFmt(const char* format, ...)
{
	char buffer[2048];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	writeData(buffer, (sint32)strlen(buffer));
}

void FileStream::writeString(const char* str)
{
	writeData(str, (sint32)strlen(str));
}

void FileStream::writeLine(const char* str)
{
	writeData(str, (sint32)strlen(str));
	writeData("\r\n", 2);
}

FileStream::~FileStream()
{
	if(m_isValid)
		CloseHandle(m_hFile);
}

FileStream::FileStream(HANDLE hFile)
{
	m_hFile = hFile;
	m_isValid = true;
}
