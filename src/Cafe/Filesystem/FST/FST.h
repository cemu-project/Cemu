#pragma once
#include "Cemu/ncrypto/ncrypto.h"

struct FSTFileHandle
{
	friend class FSTVolume;
private:
	uint32 m_fstIndex;
};

struct FSTDirectoryIterator
{
	friend class FSTVolume;

	const FSTFileHandle& GetDirHandle() const
	{
		return dirHandle;
	}
private:
    FSTFileHandle dirHandle;
	uint32 startIndex;
	uint32 endIndex;
	uint32 currentIndex;
};

class FSTVolume
{
public:
	enum class ErrorCode
  	{
		OK = 0,
		UNKNOWN_ERROR = 1,
	  	DISC_KEY_MISSING = 2,
	  	TITLE_TIK_MISSING = 3,
	    BAD_TITLE_TMD = 4,
	    BAD_TITLE_TIK = 5,
  	};

	static bool FindDiscKey(const fs::path& path, NCrypto::AesKey& discTitleKey);

	static FSTVolume* OpenFromDiscImage(const fs::path& path, NCrypto::AesKey& discTitleKey, ErrorCode* errorCodeOut = nullptr);
	static FSTVolume* OpenFromDiscImage(const fs::path& path, ErrorCode* errorCodeOut = nullptr);
	static FSTVolume* OpenFromContentFolder(fs::path folderPath, ErrorCode* errorCodeOut = nullptr);

	~FSTVolume();

	uint32 GetFileCount() const;

	bool OpenFile(std::string_view path, FSTFileHandle& fileHandleOut, bool openOnlyFiles = false);

	// file and directory functions
	bool IsDirectory(const FSTFileHandle& fileHandle) const;
	bool IsFile(const FSTFileHandle& fileHandle) const;
	bool HasLinkFlag(const FSTFileHandle& fileHandle) const;

	std::string_view GetName(const FSTFileHandle& fileHandle) const;
	std::string GetPath(const FSTFileHandle& fileHandle) const;

	// file functions
	uint32 GetFileSize(const FSTFileHandle& fileHandle) const;
	uint32 ReadFile(FSTFileHandle& fileHandle, uint32 offset, uint32 size, void* dataOut);

	// directory iterator
	bool OpenDirectoryIterator(std::string_view path, FSTDirectoryIterator& directoryIteratorOut);
	bool Next(FSTDirectoryIterator& directoryIterator, FSTFileHandle& fileHandleOut);

	// helper function to read whole file
	std::vector<uint8> ExtractFile(std::string_view path, bool* success = nullptr)
	{
		if (success)
			*success = false;
		std::vector<uint8> fileData;
		FSTFileHandle fileHandle;
		if (!OpenFile(path, fileHandle, true))
			return fileData;
		fileData.resize(GetFileSize(fileHandle));
		ReadFile(fileHandle, 0, (uint32)fileData.size(), fileData.data());
		if (success)
			*success = true;
		return fileData;
	}

private:

	/* FST data (in memory) */
	enum class ClusterHashMode : uint8
	{
		RAW = 0, // raw data + encryption, no hashing?
		RAW2 = 1, // raw data + encryption, with hash stored in tmd?
		HASH_INTERLEAVED = 2, // hashes + raw interleaved in 0x10000 blocks (0x400 bytes of hashes at the beginning, followed by 0xFC00 bytes of data)
	};

	struct FSTCluster
	{
		uint32 offset;
		uint32 size;
		ClusterHashMode hashMode;
	};

	struct FSTEntry
	{
		enum class TYPE : uint8
		{
			FILE,
			DIRECTORY,
		};

		enum FLAGS : uint8
		{
			FLAG_NONE = 0x0,
			FLAG_LINK = 0x1,
			FLAG_UKN02 = 0x2, // seen in Super Mario Galaxy. Used for vWii files?
		};

		uint32 nameOffset;
		uint32 parentDirIndex; // index of parent directory
		uint16 nameHash;
		uint8 typeAndFlags;

		TYPE GetType() const
		{
			return (TYPE)(typeAndFlags & 0xF);
		}

		void SetType(TYPE t)
		{
			typeAndFlags &= ~0x0F;
			typeAndFlags |= ((uint8)t);
		}

		FLAGS GetFlags() const
		{
			return (FLAGS)(typeAndFlags >> 4);
		}

		void SetFlags(FLAGS flags)
		{
			typeAndFlags &= ~0xF0;
			typeAndFlags |= ((uint8)flags << 4);
		}

		// note: The root node is not considered a valid parent
		bool HasNonRootNodeParent() const
		{
			return parentDirIndex != 0;
		}

		union
		{
			struct
			{
				uint32 endIndex;
			}dirInfo;
			struct
			{
				uint32 fileOffset;
				uint32 fileSize;
				uint16 clusterIndex;
			}fileInfo;
		};
	};

	class FSTDataSource* m_dataSource;
	bool m_sourceIsOwned{};
	uint32 m_sectorSize{}; // for cluster offsets
	uint32 m_offsetFactor{}; // for file offsets
	std::vector<FSTCluster> m_cluster;
	std::vector<FSTEntry> m_entries;
	std::vector<char> m_nameStringTable;
	NCrypto::AesKey m_partitionTitlekey;

	/* Cache for decrypted hashed blocks */
	std::unordered_map<uint64, struct FSTCachedHashedBlock*> m_cacheDecryptedHashedBlocks;
	uint64 m_cacheAccessCounter{};

	struct FSTCachedHashedBlock* GetDecryptedHashedBlock(uint32 clusterIndex, uint32 blockIndex);

	/* File reading */
	uint32 ReadFile_HashModeRaw(uint32 clusterIndex, FSTEntry& entry, uint32 readOffset, uint32 readSize, void* dataOut);
	uint32 ReadFile_HashModeHashed(uint32 clusterIndex, FSTEntry& entry, uint32 readOffset, uint32 readSize, void* dataOut);

	/* FST parsing */
	struct FSTHeader
	{
		/* +0x00 */ uint32be magic;
		/* +0x04 */ uint32be offsetFactor;
		/* +0x08 */ uint32be numCluster;
		/* +0x0C */ uint32be ukn0C;
		/* +0x10 */ uint32be ukn10;
		/* +0x14 */ uint32be ukn14;
		/* +0x18 */ uint32be ukn18;
		/* +0x1C */ uint32be ukn1C;
	};

	static_assert(sizeof(FSTHeader) == 0x20);

	struct FSTHeader_ClusterEntry
	{
		/* +0x00 */ uint32be offset;
		/* +0x04 */ uint32be size;
		/* +0x08 */ uint64be ownerTitleId;
		/* +0x10 */ uint32be groupId;
		/* +0x14 */ uint8be  hashMode;
		/* +0x15 */ uint8be  padding[0xB]; // ?
	};
	static_assert(sizeof(FSTHeader_ClusterEntry) == 0x20);

	struct FSTHeader_FileEntry
	{
		enum class TYPE : uint8
		{
			FILE = 0,
			DIRECTORY = 1,
		};

		/* +0x00 */ uint32be typeAndNameOffset;
		/* +0x04 */ uint32be offset; // for directories: parent directory index
		/* +0x08 */ uint32be size; // for directories: end index
		/* +0x0C */ uint16be flagsOrPermissions; // three entries, each one shifted by 4. (so 0xXYZ). Possible bits per value seem to be 0x1 and 0x4 ? These are probably permissions
		/* +0x0E */ uint16be clusterIndex;

		TYPE GetType()
		{
			uint8 v = GetTypeFlagField();
			cemu_assert_debug((v & ~0x83) == 0); // unknown type/flag
			return static_cast<TYPE>(v & 0x01);
		}

		bool HasFlagLink()
		{
			uint8 v = GetTypeFlagField();
			return (v & 0x80) != 0;
		}

		bool HasUknFlag02()
		{
			uint8 v = GetTypeFlagField();
			return (v & 0x02) != 0;
		}

		uint32 GetNameOffset()
		{
			return (uint32)typeAndNameOffset & 0xFFFFFF;
		}

		uint32 GetDirectoryParent()
		{
			return offset;
		}

		uint32 GetDirectoryEndIndex()
		{
			return size;
		}

	private:
		uint8 GetTypeFlagField()
		{
			return static_cast<uint8>((typeAndNameOffset >> 24) & 0xFF);
		}
	};

	static_assert(sizeof(FSTHeader_FileEntry) == 0x10);

	static FSTVolume* OpenFST(FSTDataSource* dataSource, uint64 fstOffset, uint32 fstSize, NCrypto::AesKey* partitionTitleKey, ClusterHashMode fstHashMode);
	static FSTVolume* OpenFST(std::unique_ptr<FSTDataSource> dataSource, uint64 fstOffset, uint32 fstSize, NCrypto::AesKey* partitionTitleKey, ClusterHashMode fstHashMode);
	static bool ProcessFST(FSTHeader_FileEntry* fileTable, uint32 numFileEntries, uint32 numCluster, std::vector<char>& nameStringTable, std::vector<FSTEntry>& fstEntries);

	bool MatchFSTEntryName(FSTEntry& entry, std::string_view comparedName)
	{
		const char* entryName = m_nameStringTable.data() + entry.nameOffset;
		const char* comparedNameCur = comparedName.data();
		const char* comparedNameEnd = comparedName.data() + comparedName.size();
		while (comparedNameCur < comparedNameEnd)
		{
			uint8 c1 = *entryName;
			uint8 c2 = *comparedNameCur;
			if (c1 >= (uint8)'A' && c1 <= (uint8)'Z')
				c1 = c1 - ((uint8)'A' - (uint8)'a');
			if (c2 >= (uint8)'A' && c2 <= (uint8)'Z')
				c2 = c2 - ((uint8)'A' - (uint8)'a');
			if (c1 != c2)
				return false;
			entryName++;
			comparedNameCur++;
		}
		return *entryName == '\0'; // all the characters match, check for same length
	}

	// we utilize hashes to accelerate string comparisons when doing path lookups
	static uint16 _QuickNameHash(const char* fileName, size_t len)
	{
		uint16 v = 0;
		const char* fileNameEnd = fileName + len;
		while (fileName < fileNameEnd)
		{
			uint8 c = (uint8)*fileName;
			if (c >= (uint8)'A' && c <= (uint8)'Z')
				c = c - ((uint8)'A' - (uint8)'a');
			v += (uint16)c;
			v = (v >> 3) | (v << 13);
			fileName++;
		}
		return v;
	}

};

class FSTVerifier
{
public:
	static bool VerifyContentFile(class FileStream* fileContent, const NCrypto::AesKey* key, uint32 contentIndex, uint32 contentSize, uint32 contentSizePadded, bool isSHA1, const uint8* tmdContentHash);
	static bool VerifyHashedContentFile(class FileStream* fileContent, const NCrypto::AesKey* key, uint32 contentIndex, uint32 contentSize, uint32 contentSizePadded, bool isSHA1, const uint8* tmdContentHash);

};