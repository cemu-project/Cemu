#pragma once
#include <Common/FileStream.h>
#include "RomFSStructs.h"
class WUHBReader {
  public:
	static WUHBReader* FromPath(const fs::path& path);

	bool CheckMagicValue();

	romfs_direntry_t GetDirEntry(uint32_t offset);
	romfs_fentry_t GetFileEntry(uint32_t offset);

	uint32_t Lookup(std::string_view path);

	~WUHBReader();
  private:
	WUHBReader(FileStream* file) : m_fileIn(file) {cemu_assert_debug(file != nullptr);};
	WUHBReader() = delete;

	romfs_header_t m_header;
	FileStream* m_fileIn;
	constexpr static std::string_view headerMagicValue = "WUHB";
	bool ReadHeader();

	static inline unsigned char NormalizeChar(unsigned char c);
	static uint32_t CalcPathHash(uint32_t parent, const char* path, uint32_t start, size_t path_len);

	uint32_t GetHashTableEntryOffset(uint32_t hash, bool isFile);

	template <typename EntryType>
	std::optional<
	    std::enable_if_t<
	        std::disjunction_v<
				std::is_same<EntryType,romfs_fentry_t>,
				std::is_same<EntryType,romfs_direntry_t>
			>,
			EntryType
		>
	> LookupHashEntry(uint32_t hash)
	{
		constexpr bool isFile = std::is_same_v<EntryType, romfs_fentry_t>;

		uint32_t tableOffset = GetHashTableEntryOffset(hash, isFile);

		if(tableOffset == ROMFS_ENTRY_EMPTY)
			return {};

		if constexpr (isFile)
		{
			return GetFileEntry(tableOffset);
		} else {
			return GetDirEntry(tableOffset);
		}
	}
};
