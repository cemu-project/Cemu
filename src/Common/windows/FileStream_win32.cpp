#include "Common/windows/FileStream_win32.h"

void FileStreamWin32::SetPosition(uint64 pos)
{
	LONG posHigh = (LONG)(pos >> 32);
	LONG posLow = (LONG)(pos);
	SetFilePointer(m_hFile, posLow, &posHigh, FILE_BEGIN);
}

uint64 FileStreamWin32::GetSize()
{
	DWORD fileSizeHigh = 0;
	DWORD fileSizeLow = 0;
	fileSizeLow = GetFileSize(m_hFile, &fileSizeHigh);
	return ((uint64)fileSizeHigh << 32) | (uint64)fileSizeLow;
}

bool FileStreamWin32::SetEndOfFile()
{
	return ::SetEndOfFile(m_hFile) != 0;
}

void FileStreamWin32::extract(std::vector<uint8>& data)
{
	DWORD fileSize = GetFileSize(m_hFile, nullptr);
	data.resize(fileSize);
	SetFilePointer(m_hFile, 0, 0, FILE_BEGIN);
	DWORD bt;
	ReadFile(m_hFile, data.data(), fileSize, &bt, nullptr);
}

uint32 FileStreamWin32::readData(void* data, uint32 length)
{
	DWORD bytesRead = 0;
	ReadFile(m_hFile, data, length, &bytesRead, NULL);
	return bytesRead;
}

bool FileStreamWin32::readU64(uint64& v)
{
	return readData(&v, sizeof(uint64)) == sizeof(uint64);
}

bool FileStreamWin32::readU32(uint32& v)
{
	return readData(&v, sizeof(uint32)) == sizeof(uint32);
}

bool FileStreamWin32::readU8(uint8& v)
{
	return readData(&v, sizeof(uint8)) == sizeof(uint8);
}

bool FileStreamWin32::readLine(std::string& line)
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

sint32 FileStreamWin32::writeData(const void* data, sint32 length)
{
	DWORD bytesWritten = 0;
	WriteFile(m_hFile, data, length, &bytesWritten, NULL);
	return bytesWritten;
}

void FileStreamWin32::writeU64(uint64 v)
{
	writeData(&v, sizeof(uint64));
}

void FileStreamWin32::writeU32(uint32 v)
{
	writeData(&v, sizeof(uint32));
}

void FileStreamWin32::writeU8(uint8 v)
{
	writeData(&v, sizeof(uint8));
}

void FileStreamWin32::writeStringFmt(const char* format, ...)
{
	char buffer[2048];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	writeData(buffer, (sint32)strlen(buffer));
}

void FileStreamWin32::writeString(const char* str)
{
	writeData(str, (sint32)strlen(str));
}

void FileStreamWin32::writeLine(const char* str)
{
	writeData(str, (sint32)strlen(str));
	writeData("\r\n", 2);
}

FileStreamWin32::~FileStreamWin32()
{
	if(m_isValid)
		CloseHandle(m_hFile);
}

FileStreamWin32::FileStreamWin32(HANDLE hFile)
{
	m_hFile = hFile;
	m_isValid = true;
}
