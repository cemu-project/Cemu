#pragma once
#include <Common/FileStream.h>
#include "RomFSStructs.h"
class WUHBReader {
  public:
	static WUHBReader* FromPath(const fs::path& path);

	bool CheckMagicValue();

	romfs_direntry_t GetDirEntry(uint32_t offset);
	romfs_fentry_t GetFileEntry(uint32_t offset);

	uint64_t GetFileSize(uint32_t entryOffset);

	uint64_t ReadFromFile(uint32_t entryOffset, uint64_t fileOffset, uint64_t length, void* buffer);

	uint32_t Lookup(const std::filesystem::path& path);

  private:
	WUHBReader(FileStream* file) : m_fileIn(file) {cemu_assert_debug(file != nullptr);};
	WUHBReader() = delete;

	romfs_header_t m_header;
	std::unique_ptr<FileStream> m_fileIn;
	constexpr static std::string_view headerMagicValue = "WUHB";
	bool ReadHeader();

	static inline unsigned char NormalizeChar(unsigned char c);
	static uint32_t CalcPathHash(uint32_t parent, const char* path, uint32_t start, size_t path_len);

	uint32_t GetHashTableEntryOffset(uint32_t hash, bool isFile);
};
