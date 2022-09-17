#pragma once
#include "Common/precompiled.h"

class FileStream
{
 public:
	static FileStream* openFile(std::string_view path);
	static FileStream* openFile(const wchar_t* path, bool allowWrite = false);
	static FileStream* openFile2(const fs::path& path, bool allowWrite = false);

	static FileStream* createFile(const wchar_t* path);
	static FileStream* createFile(std::string_view path);
	static FileStream* createFile2(const fs::path& path);

	// helper function to load a file into memory
	static std::optional<std::vector<uint8>> LoadIntoMemory(const fs::path& path);

	// size and seek
	void SetPosition(uint64 pos);

	uint64 GetSize();
	bool SetEndOfFile();
	void extract(std::vector<uint8>& data);

	// reading
	uint32 readData(void* data, uint32 length);
	bool readU64(uint64& v);
	bool readU32(uint32& v);
	bool readU16(uint16& v);
	bool readU8(uint8& v);
	bool readLine(std::string& line);

	// writing (binary)
	sint32 writeData(const void* data, sint32 length);
	void writeU64(uint64 v);
	void writeU32(uint32 v);
	void writeU16(uint16 v);
	void writeU8(uint8 v);

	// writing (strings)
	void writeStringFmt(const char* format, ...);
	void writeString(const char* str);
	void writeLine(const char* str);

	~FileStream();
	FileStream() = default;

 private:
	FileStream(HANDLE hFile);

	bool m_isValid{};
	HANDLE m_hFile;
};
