#pragma once
#include <Common/FileStream.h>
#include "RomFSStructs.h"
class WUHBReader
{
  public:
	static WUHBReader* FromPath(const fs::path& path);

	romfs_direntry_t GetDirEntry(uint32 offset) const;
	romfs_fentry_t GetFileEntry(uint32 offset) const;

	uint64 GetFileSize(uint32 entryOffset) const;

	uint64 ReadFromFile(uint32 entryOffset, uint64 fileOffset, uint64 length, void* buffer) const;

	uint32 Lookup(const std::filesystem::path& path, bool isFile) const;

  private:
	WUHBReader(FileStream* file)
		: m_fileIn(file)
	{
		cemu_assert_debug(file != nullptr);
	};
	WUHBReader() = delete;

	romfs_header_t m_header;
	std::unique_ptr<FileStream> m_fileIn;
	constexpr static std::string_view s_headerMagicValue = "WUHB";
	bool ReadHeader();
	bool CheckMagicValue() const;

	static inline unsigned char NormalizeChar(unsigned char c);
	static uint32 CalcPathHash(uint32 parent, const char* path, uint32 start, size_t path_len);

	template<bool File>
	using EntryType = std::conditional_t<File, romfs_fentry_t, romfs_direntry_t>;
	template<bool File>
	static const EntryType<File>& GetFallback();
	template<bool File>
	EntryType<File> GetEntry(uint32 offset) const;

	template<bool T>
	bool SearchHashList(uint32& entryOffset, const fs::path& targetName) const;
	uint32 GetHashTableEntryOffset(uint32 hash, bool isFile) const;
};
