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
    virtual void SetPosition(uint64 pos) = 0;

    virtual uint64 GetSize() = 0;
    virtual bool SetEndOfFile() = 0;
    virtual void extract(std::vector<uint8>& data) = 0;

    // reading
    virtual uint32 readData(void* data, uint32 length) = 0;
    virtual bool readU64(uint64& v) = 0;
    virtual bool readU32(uint32& v) = 0;
    virtual bool readU8(uint8& v) = 0;
    virtual bool readLine(std::string& line) = 0;

    // writing (binary)
    virtual sint32 writeData(const void* data, sint32 length) = 0;
    virtual void writeU64(uint64 v) = 0;
    virtual void writeU32(uint32 v) = 0;
    virtual void writeU8(uint8 v) = 0;

    // writing (strings)
    virtual void writeStringFmt(const char* format, ...) = 0;
    virtual void writeString(const char* str) = 0;
    virtual void writeLine(const char* str) = 0;

    virtual ~FileStream() = default;
};
