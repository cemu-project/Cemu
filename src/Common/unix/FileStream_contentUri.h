#pragma once

#include "Common/FileStream.h"
#include "Common/unix/ContentUriIStream.h"

class FileStreamContentUri : public FileStream
{
   public:
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

    // writing (not supported)
    sint32 writeData(const void* data, sint32 length) override;
    void writeU64(uint64 v) override;
    void writeU32(uint32 v) override;
    void writeU8(uint8 v) override;
    void writeStringFmt(const char* format, ...) override;
    void writeString(const char* str) override;
    void writeLine(const char* str) override;

   private:
    friend class FileStream;
    FileStreamContentUri(const std::string& uri);
    ContentUriIStream m_contentUriIStream;
    bool m_isValid = false;
};
