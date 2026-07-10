#pragma once

#include "FdStream.h"

class FileStreamSAF
{
  public:
	static FileStreamSAF* OpenFile(const fs::path& path);

	// size and seek
	void SetPosition(uint64 pos);

	uint64 GetSize();
	bool SetEndOfFile();
	void extract(std::vector<uint8>& data);

	// reading
	uint32 readData(void* data, uint32 length);
	bool readU64(uint64& v);
	bool readU32(uint32& v);
	bool readU8(uint8& v);
	bool readLine(std::string& line);

	// writing (not supported)
	sint32 writeData(const void* data, sint32 length);
	void writeU64(uint64 v);
	void writeU32(uint32 v);
	void writeU8(uint8 v);
	void writeStringFmt(const char* format, ...);
	void writeString(const char* str);
	void writeLine(const char* str);
	void Flush();

  private:
	FileStreamSAF(const fs::path& uri);
	FdStream m_stream;
	bool m_isValid = false;
};
