#pragma once
#include "Common/precompiled.h"
#include "Common/FileStream.h"

class FileStreamWin32 : public FileStream
{
 public:
	// helper function to load a file into memory
	static std::optional<std::vector<uint8>> LoadIntoMemory(const fs::path& path);

	// size and seek
	void SetPosition(uint64 pos) override;

	uint64 GetSize() override;
	bool SetEndOfFile() override;
	void extract(std::vector<uint8>& data) override;

	// reading
	uint32 readData(void* data, uint32 length) override;
	bool readU64(uint64& v) override;
	bool readU32(uint32& v) override;
	bool readU8(uint8& v) override;
	bool readLine(std::string& line) override;

	// writing (binary)
	sint32 writeData(const void* data, sint32 length) override;
	void writeU64(uint64 v) override;
	void writeU32(uint32 v) override;
	void writeU8(uint8 v) override;

	// writing (strings)
	void writeStringFmt(const char* format, ...) override;
	void writeString(const char* str) override;
	void writeLine(const char* str) override;

	virtual ~FileStreamWin32();
	FileStreamWin32() = default;

 private:
    friend class FileStream;
	FileStreamWin32(HANDLE hFile);

	bool m_isValid{};
	HANDLE m_hFile;
};
