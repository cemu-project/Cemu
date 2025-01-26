#include "Common/precompiled.h"
#include "Common/FileStream.h"
#include "Cemu/ncrypto/ncrypto.h"
#include "Cafe/Filesystem/WUD/wud.h"
#include "util/crypto/aes128.h"
#include "openssl/sha.h" /* SHA1 / SHA256 */
#include "fstUtil.h"

#include "FST.h"
#include "KeyCache.h"

#include "boost/range/adaptor/reversed.hpp"

#define SET_FST_ERROR(__code) 	if (errorCodeOut) *errorCodeOut = ErrorCode::__code

class FSTDataSource
{
public:
	virtual uint64 readData(uint16 clusterIndex, uint64 clusterOffset, uint64 offset, void* data, uint64 size) = 0;
	virtual ~FSTDataSource() {};

protected:
	FSTDataSource() {};

	bool m_isOpen;
};

class FSTDataSourceWUD : public FSTDataSource
{
public:

	static FSTDataSourceWUD* Open(const fs::path& path)
	{
		wud_t* wudFile = wud_open(path);
		if (!wudFile)
			return nullptr;
		FSTDataSourceWUD* ds = new FSTDataSourceWUD();
		ds->m_wudFile = wudFile;
		return ds;
	}

	void SetBaseOffset(uint64 baseOffset)
	{
		m_baseOffset = baseOffset;
	}

	uint64 GetBaseOffset() const
	{
		return m_baseOffset;
	}

	uint64 readData(uint16 clusterIndex, uint64 clusterOffset, uint64 offset, void* data, uint64 size) override
	{
		cemu_assert_debug(size <= 0xFFFFFFFF);
		return wud_readData(m_wudFile, data, (uint32)size, clusterOffset + offset + m_baseOffset);
	}

	~FSTDataSourceWUD() override
	{
		if(m_wudFile)
			wud_close(m_wudFile);
	}

protected:
	FSTDataSourceWUD() {}	
	wud_t* m_wudFile;
	uint64 m_baseOffset{};
	std::vector<uint64> m_clusterOffset;
};

class FSTDataSourceApp : public FSTDataSource
{
public:
	static FSTDataSourceApp* Open(fs::path path, NCrypto::TMDParser& tmd)
	{
		std::vector<std::unique_ptr<FileStream>> clusterFile;
		uint32 maxIndex = 0;
		for (auto& itr : tmd.GetContentList())
			maxIndex = std::max(maxIndex, (uint32)itr.index);
		clusterFile.resize(maxIndex + 1);
		// open all the app files
		for (auto& itr : tmd.GetContentList())
		{
			FileStream* appFile = FileStream::openFile2(path / fmt::format("{:08x}.app", itr.contentId));
			if (!appFile)
				return nullptr;
			clusterFile[itr.index].reset(appFile);
		}
		// construct FSTDataSourceApp
		FSTDataSourceApp* dsApp = new FSTDataSourceApp(std::move(clusterFile));
		return dsApp;
	}

	uint64 readData(uint16 clusterIndex, uint64 clusterOffset, uint64 offset, void* data, uint64 size) override
	{
		// ignore clusterOffset for .app files since each file is already relative to the cluster base
		cemu_assert_debug(clusterIndex < m_clusterFile.size());
		cemu_assert_debug(m_clusterFile[clusterIndex].get());
		cemu_assert_debug(size <= 0xFFFFFFFF);
		if (!m_clusterFile[clusterIndex].get())
			return 0;
		m_clusterFile[clusterIndex].get()->SetPosition(offset);
		return m_clusterFile[clusterIndex].get()->readData(data, (uint32)size);
	}

	~FSTDataSourceApp() override
	{
	}

private:
	FSTDataSourceApp(std::vector<std::unique_ptr<FileStream>>&& clusterFiles)
	{
		m_clusterFile = std::move(clusterFiles);
	}

	std::vector<std::unique_ptr<FileStream>> m_clusterFile;
};

constexpr size_t DISC_SECTOR_SIZE = 0x8000;

struct DiscHeaderA
{
	// header in first sector (0x0)
	uint8 productCode[22]; // ?
};

struct DiscHeaderB
{
	// header at 0x10000
	static constexpr uint32 MAGIC_VALUE = 0xCC549EB9;

	/* +0x00 */ uint32be magic;
};

static_assert(sizeof(DiscHeaderB) == 0x04);

struct DiscPartitionTableHeader
{
	// header at 0x18000, encrypted
	static constexpr uint32 MAGIC_VALUE = 0xCCA6E67B;

	/* +0x00 */ uint32be magic;
	/* +0x04 */ uint32be blockSize; // must be 0x8000?
	/* +0x08 */ uint8 partitionTableHash[20]; // hash of the data range at +0x800 to end of sector (0x8000)
	/* +0x1C */ uint32be numPartitions;
};

static_assert(sizeof(DiscPartitionTableHeader) == 0x20);

struct DiscPartitionTableEntry
{
	/* +0x00 */ uint8be partitionName[31];
	/* +0x1F */ uint8be numAddresses; // ?
	/* +0x20 */ uint32be partitionAddress; // this is an array?
	/* +0x24 */ uint8 padding[0x80 - 0x24];
};

static_assert(sizeof(DiscPartitionTableEntry) == 0x80);

struct DiscPartitionHeader
{
	// header at the beginning of each partition
	static constexpr uint32 MAGIC_VALUE = 0xCC93A4F5;

	/* +0x00 */ uint32be magic;
	/* +0x04 */ uint32be sectorSize; // must match DISC_SECTOR_SIZE for hashed blocks

	/* +0x08 */ uint32be ukn008;
	/* +0x0C */ uint32be ukn00C; // h3 array size?
	/* +0x10 */ uint32be h3HashNum;
	/* +0x14 */ uint32be fstSize; // in bytes
	/* +0x18 */ uint32be fstSector; // relative to partition start
	/* +0x1C */ uint32be ukn01C;
	/* +0x20 */ uint32be ukn020;

	// the hash and encryption mode for the FST cluster
	/* +0x24 */ uint8 fstHashType;
	/* +0x25 */ uint8 fstEncryptionType; // purpose of this isn't really understood. Maybe it controls which key is being used? (1 -> disc key, 2 -> partition key)

	/* +0x26 */ uint8be versionA;
	/* +0x27 */ uint8be ukn027; // also a version field?

	// there is an array at +0x40 ? Related to H3 list. Also related to value at +0x0C and h3HashNum
	/* +0x28 */ uint8be _uknOrPadding028[0x18];
	/* +0x40 */ uint8be h3HashArray[32]; // dynamic size. Only present if fstHashType != 0
};

static_assert(sizeof(DiscPartitionHeader) == 0x40+0x20);

bool FSTVolume::FindDiscKey(const fs::path& path, NCrypto::AesKey& discTitleKey)
{
	std::unique_ptr<FSTDataSourceWUD> dataSource(FSTDataSourceWUD::Open(path));
	if (!dataSource)
		return false;

	// read section of header which should only contain zero bytes if decrypted
	uint8 header[16*3];
	if (dataSource->readData(0, 0, 0x18000 + 0x100, header, sizeof(header)) != sizeof(header))
		return false;

	// try all the keys
	uint8 headerDecrypted[sizeof(header)-16];
	for (sint32 i = 0; i < 0x7FFFFFFF; i++)
	{
		uint8* key128 = KeyCache_GetAES128(i);
		if (key128 == NULL)
			break;
		AES128_CBC_decrypt(headerDecrypted, header + 16, sizeof(headerDecrypted), key128, header);
		if (std::all_of(headerDecrypted, headerDecrypted + sizeof(headerDecrypted), [](const uint8 v) {return v == 0; }))
		{
			// key found
			std::memcpy(discTitleKey.b, key128, 16);
			return true;
		}
	}
	return false;
}

// open WUD image using key cache
// if no matching key is found then keyFound will return false
FSTVolume* FSTVolume::OpenFromDiscImage(const fs::path& path, ErrorCode* errorCodeOut)
{
	SET_FST_ERROR(UNKNOWN_ERROR);
	KeyCache_Prepare();
	NCrypto::AesKey discTitleKey;
	if (!FindDiscKey(path, discTitleKey))
	{
		SET_FST_ERROR(DISC_KEY_MISSING);
		return nullptr;
	}
	return OpenFromDiscImage(path, discTitleKey, errorCodeOut);
}

// open WUD image
FSTVolume* FSTVolume::OpenFromDiscImage(const fs::path& path, NCrypto::AesKey& discTitleKey, ErrorCode* errorCodeOut)

{
	// WUD images support multiple partitions, each with their own key and FST
	// the process for loading game data FSTVolume from a WUD image is as follows:
	// 1) parse WUD headers and verify
	// 2) read SI partition FST
	// 3) find main GM partition
	// 4) use SI information to get titleKey for GM partition
	// 5) Load FST for GM
	SET_FST_ERROR(UNKNOWN_ERROR);
	std::unique_ptr<FSTDataSourceWUD> dataSource(FSTDataSourceWUD::Open(path));
	if (!dataSource)
		return nullptr;
	// check HeaderA (only contains product code?)
	DiscHeaderA headerA{};
	if (dataSource->readData(0, 0, 0, &headerA, sizeof(headerA)) != sizeof(headerA))
		return nullptr;
	// check HeaderB
	DiscHeaderB headerB{};
	if (dataSource->readData(0, 0, DISC_SECTOR_SIZE * 2, &headerB, sizeof(headerB)) != sizeof(headerB))
		return nullptr;
	if (headerB.magic != headerB.MAGIC_VALUE)
		return nullptr;

	// read, decrypt and parse partition table
	uint8 partitionSector[DISC_SECTOR_SIZE];
	if (dataSource->readData(0, 0, DISC_SECTOR_SIZE * 3, partitionSector, DISC_SECTOR_SIZE) != DISC_SECTOR_SIZE)
		return nullptr;
	uint8 iv[16]{};
	AES128_CBC_decrypt(partitionSector, partitionSector, DISC_SECTOR_SIZE, discTitleKey.b, iv);
	// parse partition info
	DiscPartitionTableHeader* partitionHeader = (DiscPartitionTableHeader*)partitionSector;
	if (partitionHeader->magic != DiscPartitionTableHeader::MAGIC_VALUE)
	{
		cemuLog_log(LogType::Force, "Disc image rejected because decryption failed");
		return nullptr;
	}
	if (partitionHeader->blockSize != DISC_SECTOR_SIZE)
	{
		cemuLog_log(LogType::Force, "Disc image rejected because partition sector size is invalid");
		return nullptr;
	}
	uint32 numPartitions = partitionHeader->numPartitions;
	if (numPartitions > 30) // there is space for up to 240 partitions but we use a more reasonable limit
	{
		cemuLog_log(LogType::Force, "Disc image rejected due to exceeding the partition limit (has {} partitions)", numPartitions);
		return nullptr;
	}
	DiscPartitionTableEntry* partitionArray = (DiscPartitionTableEntry*)(partitionSector + 0x800);
	// validate partitions and find SI partition
	uint32 siPartitionIndex = std::numeric_limits<uint32>::max();
	uint32 gmPartitionIndex = std::numeric_limits<uint32>::max();
	for (uint32 i = 0; i < numPartitions; i++)
	{
		if (partitionArray[i].numAddresses != 1)
		{
			cemuLog_log(LogType::Force, "Disc image has unsupported partition with {} addresses", (uint32)partitionArray[i].numAddresses);
			return nullptr;
		}
		auto& name = partitionArray[i].partitionName;
		if (name[0] == 'S' && name[1] == 'I')
		{
			if (siPartitionIndex != std::numeric_limits<uint32>::max())
			{
				cemuLog_log(LogType::Force, "Disc image has multiple SI partitions. Not supported");
				return nullptr;
			}
			siPartitionIndex = i;
		}
		if (name[0] == 'G' && name[1] == 'M')
		{
			if (gmPartitionIndex == std::numeric_limits<uint32>::max())
				gmPartitionIndex = i; // we use the first GM partition we find. This is likely not correct but it seems to work for practically all disc images
		}
	}
	if (siPartitionIndex == std::numeric_limits<uint32>::max() || gmPartitionIndex == std::numeric_limits<uint32>::max())
	{
		cemuLog_log(LogType::Force, "Disc image has no SI or GM partition. Cannot read game data");
		return nullptr;
	}

	// read and verify partition headers for SI and GM
	auto readPartitionHeader = [&](DiscPartitionHeader& partitionHeader, uint32 partitionIndex) -> bool
	{
		cemu_assert_debug(dataSource->GetBaseOffset() == 0);
		if (dataSource->readData(0, 0, partitionArray[partitionIndex].partitionAddress * DISC_SECTOR_SIZE, &partitionHeader, sizeof(DiscPartitionHeader)) != sizeof(DiscPartitionHeader))
			return false;
		if (partitionHeader.magic != partitionHeader.MAGIC_VALUE && partitionHeader.sectorSize != DISC_SECTOR_SIZE)
			return false;
		return true;
	};

	// SI partition
	DiscPartitionHeader partitionHeaderSI{};
	if (!readPartitionHeader(partitionHeaderSI, siPartitionIndex))
	{
		cemuLog_log(LogType::Force, "Disc image SI partition header is invalid");
		return nullptr;
	}

	cemu_assert_debug(partitionHeaderSI.fstHashType == 0);
	cemu_assert_debug(partitionHeaderSI.fstEncryptionType == 1);
	// todo - check other fields?

	if(partitionHeaderSI.fstHashType == 0 && partitionHeaderSI.h3HashNum != 0)
		cemuLog_log(LogType::Force, "FST: Partition uses unhashed blocks but stores a non-zero amount of H3 hashes");

	// GM partition
	DiscPartitionHeader partitionHeaderGM{};
	if (!readPartitionHeader(partitionHeaderGM, gmPartitionIndex))
	{
		cemuLog_log(LogType::Force, "Disc image GM partition header is invalid");
		return nullptr;
	}
	cemu_assert_debug(partitionHeaderGM.fstHashType == 1);
	cemu_assert_debug(partitionHeaderGM.fstEncryptionType == 2);

	// if decryption is necessary
	// load SI FST
	dataSource->SetBaseOffset((uint64)partitionArray[siPartitionIndex].partitionAddress * DISC_SECTOR_SIZE);
	auto siFST = OpenFST(dataSource.get(), (uint64)partitionHeaderSI.fstSector * DISC_SECTOR_SIZE, partitionHeaderSI.fstSize, &discTitleKey, static_cast<FSTVolume::ClusterHashMode>(partitionHeaderSI.fstHashType), nullptr);
	if (!siFST)
		return nullptr;
	cemu_assert_debug(!(siFST->HashIsDisabled() && partitionHeaderSI.h3HashNum != 0)); // if hash is disabled, no H3 data may be present
	// load ticket file for partition that we want to decrypt
	NCrypto::ETicketParser ticketParser;
	std::vector<uint8> ticketData = siFST->ExtractFile(fmt::format("{:02x}/title.tik", gmPartitionIndex));
	if (ticketData.empty() || !ticketParser.parse(ticketData.data(), ticketData.size()))
	{
		cemuLog_log(LogType::Force, "Disc image ticket file is invalid");
		return nullptr;
	}
#if 0
	// each SI partition seems to contain a title.tmd that we could parse and which should have information about the associated GM partition
	// but the console seems to ignore this file for disc images, at least when mounting, so we shouldn't rely on it either
	std::vector<uint8> tmdData = siFST->ExtractFile(fmt::format("{:02x}/title.tmd", gmPartitionIndex));
	if (tmdData.empty())
	{
		cemuLog_log(LogType::Force, "Disc image TMD file is missing");
		return nullptr;
	}
	// parse TMD
	NCrypto::TMDParser tmdParser;
	if (!tmdParser.parse(tmdData.data(), tmdData.size()))
	{
		cemuLog_log(LogType::Force, "Disc image TMD file is invalid");
		return nullptr;
	}
#endif
	delete siFST;
	NCrypto::AesKey gmTitleKey;
	ticketParser.GetTitleKey(gmTitleKey);
	// load GM partition
	dataSource->SetBaseOffset((uint64)partitionArray[gmPartitionIndex].partitionAddress * DISC_SECTOR_SIZE);
	FSTVolume* r = OpenFST(std::move(dataSource), (uint64)partitionHeaderGM.fstSector * DISC_SECTOR_SIZE, partitionHeaderGM.fstSize, &gmTitleKey, static_cast<FSTVolume::ClusterHashMode>(partitionHeaderGM.fstHashType), nullptr);
	if (r)
		SET_FST_ERROR(OK);
	cemu_assert_debug(!(r->HashIsDisabled() && partitionHeaderGM.h3HashNum != 0)); // if hash is disabled, no H3 data may be present
	return r;
}

FSTVolume* FSTVolume::OpenFromContentFolder(fs::path folderPath, ErrorCode* errorCodeOut)
{
	SET_FST_ERROR(UNKNOWN_ERROR);
	// load TMD
	FileStream* tmdFile = FileStream::openFile2(folderPath / "title.tmd");
	if (!tmdFile)
		return nullptr;
	std::vector<uint8> tmdData;
	tmdFile->extract(tmdData);
	delete tmdFile;
	NCrypto::TMDParser tmdParser;
	if (!tmdParser.parse(tmdData.data(), tmdData.size()))
	{
		SET_FST_ERROR(BAD_TITLE_TMD);
		return nullptr;
	}
	// load ticket
	FileStream* ticketFile = FileStream::openFile2(folderPath / "title.tik");
	if (!ticketFile)
	{
		SET_FST_ERROR(TITLE_TIK_MISSING);
		return nullptr;
	}
	std::vector<uint8> ticketData;
	ticketFile->extract(ticketData);
	delete ticketFile;
	NCrypto::ETicketParser ticketParser;
	if (!ticketParser.parse(ticketData.data(), ticketData.size()))
	{
		SET_FST_ERROR(BAD_TITLE_TIK);
		return nullptr;
	}
	NCrypto::AesKey titleKey;
	ticketParser.GetTitleKey(titleKey);
	// open data source
	std::unique_ptr<FSTDataSource> dataSource(FSTDataSourceApp::Open(folderPath, tmdParser));
	if (!dataSource)
		return nullptr;
	// get info about FST from first cluster (todo - is this correct or does the TMD store info about the fst?)
	ClusterHashMode fstHashMode = ClusterHashMode::RAW;
	uint32 fstSize = 0;
	for (auto& itr : tmdParser.GetContentList())
	{
		if (itr.index == 0)
		{
			if (HAS_FLAG(itr.contentFlags, NCrypto::TMDParser::TMDContentFlags::FLAG_HASHED_CONTENT))
				fstHashMode = ClusterHashMode::HASH_INTERLEAVED;
			cemu_assert_debug(itr.size <= 0xFFFFFFFF);
			fstSize = (uint32)itr.size;
		}
	}
	// load FST
	// fstSize = size of first cluster?
	FSTVolume* fstVolume = FSTVolume::OpenFST(std::move(dataSource), 0, fstSize, &titleKey, fstHashMode, &tmdParser);
	if (fstVolume)
		SET_FST_ERROR(OK);
	return fstVolume;
}

FSTVolume* FSTVolume::OpenFST(FSTDataSource* dataSource, uint64 fstOffset, uint32 fstSize, NCrypto::AesKey* partitionTitleKey, ClusterHashMode fstHashMode, NCrypto::TMDParser* optionalTMD)
{
	cemu_assert_debug(fstHashMode != ClusterHashMode::RAW || fstHashMode != ClusterHashMode::RAW_STREAM);
	if (fstSize < sizeof(FSTHeader))
		return nullptr;
	constexpr uint64 FST_CLUSTER_OFFSET = 0;
	uint32 fstSizePadded = (fstSize + 15) & ~15; // pad to AES block size
	// read FST data and decrypt
	std::vector<uint8> fstData(fstSizePadded);
	if (dataSource->readData(0, FST_CLUSTER_OFFSET, fstOffset, fstData.data(), fstSizePadded) != fstSizePadded)
		return nullptr;
	uint8 iv[16]{};
	AES128_CBC_decrypt(fstData.data(), fstData.data(), fstSizePadded, partitionTitleKey->b, iv);
	// validate header
	FSTHeader* fstHeader = (FSTHeader*)fstData.data();
	const void* fstEnd = fstData.data() + fstSize;
	if (fstHeader->magic != 0x46535400 || fstHeader->numCluster >= 0x1000)
	{
		cemuLog_log(LogType::Force, "FST has invalid header");
		return nullptr;
	}
	// load cluster table
	uint32 numCluster = fstHeader->numCluster;
	FSTHeader_ClusterEntry* clusterDataTable = (FSTHeader_ClusterEntry*)(fstData.data() + sizeof(FSTHeader));
	if ((clusterDataTable + numCluster) > fstEnd)
		return nullptr;
	std::vector<FSTCluster> clusterTable;
	clusterTable.resize(numCluster);
	for (size_t i = 0; i < numCluster; i++)
	{
		clusterTable[i].offset = clusterDataTable[i].offset;
		clusterTable[i].size = clusterDataTable[i].size;
		clusterTable[i].hashMode = static_cast<FSTVolume::ClusterHashMode>((uint8)clusterDataTable[i].hashMode);
		clusterTable[i].hasContentHash = false; // from the TMD file (H4?)
	}
	// if the TMD is available (when opening .app files) we can use the extra info from it to validate unhashed clusters
	// each content entry in the TMD corresponds to one cluster used by the FST
	if(optionalTMD)
	{
		if(numCluster != optionalTMD->GetContentList().size())
		{
			cemuLog_log(LogType::Force, "FST: Number of clusters does not match TMD content list");
			return nullptr;
		}
		auto& contentList = optionalTMD->GetContentList();
		for(size_t i=0; i<contentList.size(); i++)
		{
			auto& cluster = clusterTable[i];
			auto& content = contentList[i];
			cluster.hasContentHash = true;
			cluster.contentHashIsSHA1 = HAS_FLAG(contentList[i].contentFlags, NCrypto::TMDParser::TMDContentFlags::FLAG_SHA1);
			cluster.contentSize = content.size;
			static_assert(sizeof(content.hash32) == sizeof(cluster.contentHash32));
			memcpy(cluster.contentHash32, content.hash32, sizeof(cluster.contentHash32));
			// if unhashed mode, then initialize the hash context
			if(cluster.hashMode == ClusterHashMode::RAW || cluster.hashMode == ClusterHashMode::RAW_STREAM)
			{
				cluster.singleHashCtx.reset(EVP_MD_CTX_new());
				EVP_DigestInit_ex(cluster.singleHashCtx.get(), cluster.contentHashIsSHA1 ? EVP_sha1() : EVP_sha256(), nullptr);
			}
		}
	}
	// preprocess FST table
	FSTHeader_FileEntry* fileTable = (FSTHeader_FileEntry*)(clusterDataTable + numCluster);
	if ((fileTable + 1) > fstEnd)
		return nullptr;
	if (fileTable[0].GetType() != FSTHeader_FileEntry::TYPE::DIRECTORY)
		return nullptr;
	uint32 numFileEntries = fileTable[0].size;
	if (numFileEntries == 0 || (fileTable + numFileEntries) > fstEnd)
		return nullptr;
	// load name string table
	ptrdiff_t nameLookupTableSize = ((const uint8*)fstEnd - (const uint8*)(fileTable + numFileEntries));
	if (nameLookupTableSize < 1)
		return nullptr;
	std::vector<char> nameStringTable(nameLookupTableSize);
	std::memcpy(nameStringTable.data(), (fileTable + numFileEntries), nameLookupTableSize);
	// process FST
	std::vector<FSTEntry> fstEntries;
	if (!ProcessFST(fileTable, numFileEntries, numCluster, nameStringTable, fstEntries))
		return nullptr;
	// construct FSTVolume from the processed data
	FSTVolume* fstVolume = new FSTVolume();
	fstVolume->m_dataSource = dataSource;
	fstVolume->m_offsetFactor = fstHeader->offsetFactor;
	fstVolume->m_sectorSize = DISC_SECTOR_SIZE;
	fstVolume->m_partitionTitlekey = *partitionTitleKey;
	fstVolume->m_hashIsDisabled = fstHeader->hashIsDisabled != 0;
	fstVolume->m_cluster = std::move(clusterTable);
	fstVolume->m_entries = std::move(fstEntries);
	fstVolume->m_nameStringTable = std::move(nameStringTable);
	return fstVolume;
}

FSTVolume* FSTVolume::OpenFST(std::unique_ptr<FSTDataSource> dataSource, uint64 fstOffset, uint32 fstSize, NCrypto::AesKey* partitionTitleKey, ClusterHashMode fstHashMode, NCrypto::TMDParser* optionalTMD)
{
	FSTDataSource* ds = dataSource.release();
	FSTVolume* fstVolume = OpenFST(ds, fstOffset, fstSize, partitionTitleKey, fstHashMode, optionalTMD);
	if (!fstVolume)
	{
		delete ds;
		return nullptr;
	}
	fstVolume->m_sourceIsOwned = true;
	return fstVolume;
}

bool FSTVolume::ProcessFST(FSTHeader_FileEntry* fileTable, uint32 numFileEntries, uint32 numCluster, std::vector<char>& nameStringTable, std::vector<FSTEntry>& fstEntries)
{
	struct DirHierachyInfo
	{
		DirHierachyInfo(uint32 parentIndex, uint32 endIndex) : parentIndex(parentIndex), endIndex(endIndex) {};

		uint32 parentIndex;
		uint32 endIndex;
	};
	std::vector<DirHierachyInfo> currentDirEnd;
	currentDirEnd.reserve(32);
	currentDirEnd.emplace_back(0, numFileEntries); // create a fake parent for the root directory, the root's parent index is zero (referencing itself)
	uint32 currentIndex = 0;
	FSTHeader_FileEntry* pFileIn = fileTable + currentIndex;
	fstEntries.resize(numFileEntries);
	FSTEntry* pFileOut = fstEntries.data();
	// validate root directory
	if (pFileIn->GetType() != FSTHeader_FileEntry::TYPE::DIRECTORY || pFileIn->GetDirectoryEndIndex() != numFileEntries || pFileIn->GetDirectoryParent() != 0)
	{
		cemuLog_log(LogType::Force, "FSTVolume::ProcessFST() - root node is invalid");
		return false;
	}
	for (; currentIndex < numFileEntries; currentIndex++)
	{
		while (currentIndex >= currentDirEnd.back().endIndex)
			currentDirEnd.pop_back();
		// process entry name
		uint32 nameOffset = pFileIn->GetNameOffset();
		uint32 pos = nameOffset;
		while (true)
		{
			if (pos >= nameStringTable.size())
				return false; // name exceeds string table
			if (nameStringTable[pos] == '\0')
				break;
			pos++;
		}
		uint32 nameLen = pos - nameOffset;
		pFileOut->nameOffset = nameOffset;
		pFileOut->nameHash = _QuickNameHash(nameStringTable.data() + nameOffset, nameLen);
		// parent directory index
		pFileOut->parentDirIndex = currentDirEnd.back().parentIndex;
		//if (currentDirEnd.back().parentIndex == 0)
		//	pFileOut->parentDirIndex = std::numeric_limits<uint32>::max();
		//else
		//	pFileOut->parentDirIndex = currentDirEnd.back().parentIndex;
		// process type specific data
		auto entryType = pFileIn->GetType();

		uint8 flags = 0;
		if (pFileIn->HasFlagLink())
			flags |= FSTEntry::FLAG_LINK;
		if (pFileIn->HasUknFlag02())
			flags |= FSTEntry::FLAG_UKN02;
		pFileOut->SetFlags((FSTEntry::FLAGS)flags);

		if (entryType == FSTHeader_FileEntry::TYPE::FILE)
		{
			bool isSysLink = entryType == FSTHeader_FileEntry::TYPE::FILE;
			if (pFileIn->clusterIndex >= numCluster)
			{
				cemuLog_log(LogType::Force, "FST: File references cluster out of range");
				return false;
			}
			cemu_assert_debug(pFileIn->flagsOrPermissions != 0x4004);
			pFileOut->SetType(FSTEntry::TYPE::FILE);
			pFileOut->fileInfo.fileOffset = pFileIn->offset;
			pFileOut->fileInfo.fileSize = pFileIn->size;
			pFileOut->fileInfo.clusterIndex = pFileIn->clusterIndex;
		}
		else if (entryType == FSTHeader_FileEntry::TYPE::DIRECTORY)
		{
			cemu_assert_debug(pFileIn->flagsOrPermissions != 0x4004);
			pFileOut->SetType(FSTEntry::TYPE::DIRECTORY);
			uint32 endIndex = pFileIn->GetDirectoryEndIndex();
			uint32 parentIndex = pFileIn->GetDirectoryParent();
			if (endIndex < currentIndex || endIndex > currentDirEnd.back().endIndex)
			{
				cemuLog_log(LogType::Force, "FST: Directory range out of bounds");
				return false; // dir index out of range
			}
			if (parentIndex != currentDirEnd.back().parentIndex)
			{
				cemuLog_log(LogType::Force, "FST: Parent index does not match");
				cemu_assert_debug(false);
				return false;
			}
			currentDirEnd.emplace_back(currentIndex, endIndex);
			pFileOut->dirInfo.endIndex = endIndex;
		}
		else
		{
			cemuLog_log(LogType::Force, "FST: Encountered node with unknown type");
			cemu_assert_debug(false);
			return false;
		}
		pFileIn++;
		pFileOut++;
	}
	// end remaining directory hierarchy with final index
	cemu_assert_debug(currentIndex == numFileEntries);
	while (!currentDirEnd.empty() && currentIndex >= currentDirEnd.back().endIndex)
		currentDirEnd.pop_back();
	cemu_assert_debug(currentDirEnd.empty()); // no entries should remain
	return true;
}

uint32 FSTVolume::GetFileCount() const
{
	uint32 fileCount = 0;
	for (auto& itr : m_entries)
	{
		if (itr.GetType() == FSTEntry::TYPE::FILE)
			fileCount++;
	}
	return fileCount;
}

bool FSTVolume::OpenFile(std::string_view path, FSTFileHandle& fileHandleOut, bool openOnlyFiles)
{
	FSCPath fscPath(path);
	if (fscPath.GetNodeCount() == 0)
	{
		// empty path pointers to root directory
		if(openOnlyFiles)
			return false;
		fileHandleOut.m_fstIndex = 0;
		return true;
	}

	// scan directory and find sub folder or file
	// skips iterating subdirectories
	auto findSubentry = [this](size_t firstIndex, size_t lastIndex, std::string_view nodeName) -> sint32
	{
		uint16 nodeHash = _QuickNameHash(nodeName.data(), nodeName.size());
		size_t index = firstIndex;
		while (index < lastIndex)
		{
			if (m_entries[index].nameHash == nodeHash && MatchFSTEntryName(m_entries[index], nodeName))
				return (sint32)index;
			if (m_entries[index].GetType() == FSTEntry::TYPE::DIRECTORY)
				index = m_entries[index].dirInfo.endIndex;
			else
				index++;
		}
		return -1;
	};

	// current FST range we iterate, starting with root directory which covers all entries
	uint32 parentIndex = std::numeric_limits<uint32>::max();
	size_t curDirStart = 1; // skip root directory
	size_t curDirEnd = m_entries[0].dirInfo.endIndex;

	// find the subdirectory
	for (size_t nodeIndex = 0; nodeIndex < fscPath.GetNodeCount() - 1; nodeIndex++)
	{
		// get hash of node name
		sint32 fstIndex = findSubentry(curDirStart, curDirEnd, fscPath.GetNodeName(nodeIndex));
		if (fstIndex < 0)
			return false;
		if (m_entries[fstIndex].GetType() != FSTEntry::TYPE::DIRECTORY)
			return false;
		parentIndex = fstIndex;
		curDirStart = fstIndex + 1;
		curDirEnd = m_entries[fstIndex].dirInfo.endIndex;
	}
	// find the entry
	sint32 fstIndex = findSubentry(curDirStart, curDirEnd, fscPath.GetNodeName(fscPath.GetNodeCount() - 1));
	if (fstIndex < 0)
		return false;
	if (openOnlyFiles && m_entries[fstIndex].GetType() != FSTEntry::TYPE::FILE)
		return false;
	fileHandleOut.m_fstIndex = fstIndex;
	return true;
}

bool FSTVolume::IsDirectory(const FSTFileHandle& fileHandle) const
{
	cemu_assert_debug(fileHandle.m_fstIndex < m_entries.size());
	return m_entries[fileHandle.m_fstIndex].GetType() == FSTEntry::TYPE::DIRECTORY;
};

bool FSTVolume::IsFile(const FSTFileHandle& fileHandle) const
{
	cemu_assert_debug(fileHandle.m_fstIndex < m_entries.size());
	return m_entries[fileHandle.m_fstIndex].GetType() == FSTEntry::TYPE::FILE;
};

bool FSTVolume::HasLinkFlag(const FSTFileHandle& fileHandle) const
{
	cemu_assert_debug(fileHandle.m_fstIndex < m_entries.size());
	return HAS_FLAG(m_entries[fileHandle.m_fstIndex].GetFlags(), FSTEntry::FLAGS::FLAG_LINK);
};

std::string_view FSTVolume::GetName(const FSTFileHandle& fileHandle) const
{
	if (fileHandle.m_fstIndex > m_entries.size())
		return "";
	const char* entryName = m_nameStringTable.data() + m_entries[fileHandle.m_fstIndex].nameOffset;
	return entryName;
}

std::string FSTVolume::GetPath(const FSTFileHandle& fileHandle) const
{
	std::string path;
	auto& entry = m_entries[fileHandle.m_fstIndex];
	// get parent chain
	boost::container::small_vector<uint32, 8> parentChain;
	if (entry.HasNonRootNodeParent())
	{
		parentChain.emplace_back(entry.parentDirIndex);
		auto* parentItr = &m_entries[entry.parentDirIndex];
		while (parentItr->HasNonRootNodeParent())
		{
			cemu_assert_debug(parentItr->GetType() == FSTEntry::TYPE::DIRECTORY);
			parentChain.emplace_back(parentItr->parentDirIndex);
			parentItr = &m_entries[parentItr->parentDirIndex];
		}
	}
	// build path
	cemu_assert_debug(parentChain.size() <= 1); // test this case
	for (auto& itr : parentChain | boost::adaptors::reversed)
	{
		const char* name = m_nameStringTable.data() + m_entries[itr].nameOffset;
		path.append(name);
		path.push_back('/');
	}
	// append node name
	const char* name = m_nameStringTable.data() + entry.nameOffset;
	path.append(name);
	return path;
}

uint32 FSTVolume::GetFileSize(const FSTFileHandle& fileHandle) const
{
	if (m_entries[fileHandle.m_fstIndex].GetType() != FSTEntry::TYPE::FILE)
		return 0;
	return m_entries[fileHandle.m_fstIndex].fileInfo.fileSize;
}

uint32 FSTVolume::ReadFile(FSTFileHandle& fileHandle, uint32 offset, uint32 size, void* dataOut)
{
	FSTEntry& entry = m_entries[fileHandle.m_fstIndex];
	if (entry.GetType() != FSTEntry::TYPE::FILE)
		return 0;
	cemu_assert_debug(!HAS_FLAG(entry.GetFlags(), FSTEntry::FLAGS::FLAG_LINK));
	FSTCluster& cluster = m_cluster[entry.fileInfo.clusterIndex];
	if (cluster.hashMode == ClusterHashMode::RAW || cluster.hashMode == ClusterHashMode::RAW_STREAM)
		return ReadFile_HashModeRaw(entry.fileInfo.clusterIndex, entry, offset, size, dataOut);
	else if (cluster.hashMode == ClusterHashMode::HASH_INTERLEAVED)
		return ReadFile_HashModeHashed(entry.fileInfo.clusterIndex, entry, offset, size, dataOut);
	cemu_assert_debug(false);
	return 0;
}

constexpr size_t BLOCK_SIZE = 0x10000;
constexpr size_t BLOCK_HASH_SIZE = 0x0400;
constexpr size_t BLOCK_FILE_SIZE = 0xFC00;

struct FSTRawBlock
{
	std::vector<uint8> rawData; // unhashed block size depends on sector size field in partition header
};

struct FSTHashedBlock
{
	uint8 rawData[BLOCK_SIZE];

	uint8* getHashData()
	{
		return rawData;
	}

	uint8* getH0Hash(uint32 index)
	{
		cemu_assert_debug(index < 16);
		return getHashData() + 20 * index;
	}

	uint8* getH1Hash(uint32 index)
	{
		cemu_assert_debug(index < 16);
		return getHashData() + (20 * 16) * 1 + 20 * index;
	}

	uint8* getH2Hash(uint32 index)
	{
		cemu_assert_debug(index < 16);
		return getHashData() + (20 * 16) * 2 + 20 * index;
	}

	uint8* getFileData()
	{
		return rawData + BLOCK_HASH_SIZE;
	}

	uint8* getH0Hash(size_t index)
	{
		cemu_assert_debug(index < 16);
		return rawData + index * 20;
	}
};

static_assert(sizeof(FSTHashedBlock) == BLOCK_SIZE);

struct FSTCachedRawBlock
{
	FSTRawBlock blockData;
	uint8 ivForNextBlock[16];
	uint64 lastAccess;
};

struct FSTCachedHashedBlock 
{
	FSTHashedBlock blockData;
	uint64 lastAccess;
};

// Checks cache fill state and if necessary drops least recently accessed block from the cache. Optionally allows to recycle the released cache entry to cut down cost of memory allocation and clearing
void FSTVolume::TrimCacheIfRequired(FSTCachedRawBlock** droppedRawBlock, FSTCachedHashedBlock** droppedHashedBlock)
{
	// calculate size used by cache
	size_t cacheSize = 0;
	for (auto& itr : m_cacheDecryptedRawBlocks)
		cacheSize += itr.second->blockData.rawData.size();
	for (auto& itr : m_cacheDecryptedHashedBlocks)
		cacheSize += sizeof(FSTCachedHashedBlock) + sizeof(FSTHashedBlock);
	// only trim if cache is full (larger than 2MB)
	if (cacheSize < 2*1024*1024) // 2MB
		return;
	// scan both cache lists to find least recently accessed block to drop
	auto dropRawItr = std::min_element(m_cacheDecryptedRawBlocks.begin(), m_cacheDecryptedRawBlocks.end(), [](const auto& a, const auto& b) -> bool
										{ return a.second->lastAccess < b.second->lastAccess; });
	auto dropHashedItr = std::min_element(m_cacheDecryptedHashedBlocks.begin(), m_cacheDecryptedHashedBlocks.end(), [](const auto& a, const auto& b) -> bool
										{ return a.second->lastAccess < b.second->lastAccess; });
	uint64 lastAccess = std::numeric_limits<uint64>::max();
	if(dropRawItr != m_cacheDecryptedRawBlocks.end())
		lastAccess = dropRawItr->second->lastAccess;
	if(dropHashedItr != m_cacheDecryptedHashedBlocks.end())
		lastAccess = std::min<uint64>(lastAccess, dropHashedItr->second->lastAccess);
	if(dropRawItr != m_cacheDecryptedRawBlocks.end() && dropRawItr->second->lastAccess == lastAccess)
	{
		if (droppedRawBlock)
			*droppedRawBlock = dropRawItr->second;
		else
			delete dropRawItr->second;
		m_cacheDecryptedRawBlocks.erase(dropRawItr);
		return;
	}
	else if(dropHashedItr != m_cacheDecryptedHashedBlocks.end() && dropHashedItr->second->lastAccess == lastAccess)
	{
		if (droppedHashedBlock)
			*droppedHashedBlock = dropHashedItr->second;
		else
			delete dropHashedItr->second;
		m_cacheDecryptedHashedBlocks.erase(dropHashedItr);
	}
}

void FSTVolume::DetermineUnhashedBlockIV(uint32 clusterIndex, uint32 blockIndex, uint8 ivOut[16])
{
	memset(ivOut, 0, sizeof(ivOut));
	if(blockIndex == 0)
	{
		ivOut[0] = (uint8)(clusterIndex >> 8);
		ivOut[1] = (uint8)(clusterIndex >> 0);
	}
	else
	{
		// the last 16 encrypted bytes of the previous block are the IV (AES CBC)
		// if the previous block is cached we can grab the IV from there. Otherwise we have to read the 16 bytes from the data source
		uint32 prevBlockIndex = blockIndex - 1;
		uint64 cacheBlockId = ((uint64)clusterIndex << (64 - 16)) | (uint64)prevBlockIndex;
		auto itr = m_cacheDecryptedRawBlocks.find(cacheBlockId);
		if (itr != m_cacheDecryptedRawBlocks.end())
		{
			memcpy(ivOut, itr->second->ivForNextBlock, 16);
		}
		else
		{
			cemu_assert(m_sectorSize >= 16);
			uint64 clusterOffset = (uint64)m_cluster[clusterIndex].offset * m_sectorSize;
			uint8 prevIV[16];
			if (m_dataSource->readData(clusterIndex, clusterOffset, blockIndex * m_sectorSize - 16, prevIV, 16) != 16)
			{
				cemuLog_log(LogType::Force, "Failed to read IV for raw FST block");
				m_detectedCorruption = true;
				return;
			}
			memcpy(ivOut, prevIV, 16);
		}
	}
}

FSTCachedRawBlock* FSTVolume::GetDecryptedRawBlock(uint32 clusterIndex, uint32 blockIndex)
{
	FSTCluster& cluster = m_cluster[clusterIndex];
	uint64 clusterOffset = (uint64)cluster.offset * m_sectorSize;
	// generate id for cache
	uint64 cacheBlockId = ((uint64)clusterIndex << (64 - 16)) | (uint64)blockIndex;
	// lookup block in cache
	FSTCachedRawBlock* block = nullptr;
	auto itr = m_cacheDecryptedRawBlocks.find(cacheBlockId);
	if (itr != m_cacheDecryptedRawBlocks.end())
	{
		block = itr->second;
		block->lastAccess = ++m_cacheAccessCounter;
		return block;
	}
	// if cache already full, drop least recently accessed block and recycle FSTCachedRawBlock object if possible
	TrimCacheIfRequired(&block, nullptr);
	if (!block)
		block = new FSTCachedRawBlock();
	block->blockData.rawData.resize(m_sectorSize);
	// block not cached, read new
	block->lastAccess = ++m_cacheAccessCounter;
	if (m_dataSource->readData(clusterIndex, clusterOffset, blockIndex * m_sectorSize, block->blockData.rawData.data(), m_sectorSize) != m_sectorSize)
	{
		cemuLog_log(LogType::Force, "Failed to read raw FST block");
		delete block;
		m_detectedCorruption = true;
		return nullptr;
	}
	// decrypt hash data
	uint8 iv[16]{};
	DetermineUnhashedBlockIV(clusterIndex, blockIndex, iv);
	memcpy(block->ivForNextBlock, block->blockData.rawData.data() + m_sectorSize - 16, 16);
	AES128_CBC_decrypt(block->blockData.rawData.data(), block->blockData.rawData.data(), m_sectorSize, m_partitionTitlekey.b, iv);
	// if this is the next block, then hash it
	if(cluster.hasContentHash)
	{
		if(cluster.singleHashNumBlocksHashed == blockIndex)
		{
			cemu_assert_debug(!(cluster.contentSize % m_sectorSize)); // size should be multiple of sector size? Regardless, the hashing code below can handle non-aligned sizes
			bool isLastBlock = blockIndex == (std::max<uint32>(cluster.contentSize / m_sectorSize, 1) - 1);
			uint32 hashSize = m_sectorSize;
			if(isLastBlock)
				hashSize = cluster.contentSize - (uint64)blockIndex*m_sectorSize;
			EVP_DigestUpdate(cluster.singleHashCtx.get(), block->blockData.rawData.data(), hashSize);
			cluster.singleHashNumBlocksHashed++;
			if(isLastBlock)
			{
				uint8 hash[32];
				EVP_DigestFinal_ex(cluster.singleHashCtx.get(), hash, nullptr);
				if(memcmp(hash, cluster.contentHash32, cluster.contentHashIsSHA1 ? 20 : 32) != 0)
				{
					cemuLog_log(LogType::Force, "FST: Raw section hash mismatch");
					delete block;
					m_detectedCorruption = true;
					return nullptr;
				}
			}
		}
	}
	// register in cache
	m_cacheDecryptedRawBlocks.emplace(cacheBlockId, block);
	return block;
}

FSTCachedHashedBlock* FSTVolume::GetDecryptedHashedBlock(uint32 clusterIndex, uint32 blockIndex)
{
	const FSTCluster& cluster = m_cluster[clusterIndex];
	uint64 clusterOffset = (uint64)cluster.offset * m_sectorSize;
	// generate id for cache
	uint64 cacheBlockId = ((uint64)clusterIndex << (64 - 16)) | (uint64)blockIndex;
	// lookup block in cache
	FSTCachedHashedBlock* block = nullptr;
	auto itr = m_cacheDecryptedHashedBlocks.find(cacheBlockId);
	if (itr != m_cacheDecryptedHashedBlocks.end())
	{
		block = itr->second;
		block->lastAccess = ++m_cacheAccessCounter;
		return block;
	}
	// if cache already full, drop least recently accessed block and recycle FSTCachedHashedBlock object if possible
	TrimCacheIfRequired(nullptr, &block);
	if (!block)
		block = new FSTCachedHashedBlock();
	// block not cached, read new
	block->lastAccess = ++m_cacheAccessCounter;
	if (m_dataSource->readData(clusterIndex, clusterOffset, blockIndex * BLOCK_SIZE, block->blockData.rawData, BLOCK_SIZE) != BLOCK_SIZE)
	{
		cemuLog_log(LogType::Force, "Failed to read hashed FST block");
		delete block;
		m_detectedCorruption = true;
		return nullptr;
	}
	// decrypt hash data
	uint8 iv[16]{};
	AES128_CBC_decrypt(block->blockData.getHashData(), block->blockData.getHashData(), BLOCK_HASH_SIZE, m_partitionTitlekey.b, iv);
	// decrypt file data
	AES128_CBC_decrypt(block->blockData.getFileData(), block->blockData.getFileData(), BLOCK_FILE_SIZE, m_partitionTitlekey.b, block->blockData.getH0Hash(blockIndex%16));
	// compare with H0 to verify data integrity
	NCrypto::CHash160 h0;
	SHA1(block->blockData.getFileData(), BLOCK_FILE_SIZE, h0.b);
	uint32 h0Index = (blockIndex % 4096);
	if (memcmp(h0.b, block->blockData.getH0Hash(h0Index & 0xF), sizeof(h0.b)) != 0)
	{
		cemuLog_log(LogType::Force, "FST: Hash H0 mismatch in hashed block (section {} index {})", clusterIndex, blockIndex);
		delete block;
		m_detectedCorruption = true;
		return nullptr;
	}
	// register in cache
	m_cacheDecryptedHashedBlocks.emplace(cacheBlockId, block);
	return block;
}

uint32 FSTVolume::ReadFile_HashModeRaw(uint32 clusterIndex, FSTEntry& entry, uint32 readOffset, uint32 readSize, void* dataOut)
{
	uint8* dataOutU8 = (uint8*)dataOut;
	if (readOffset >= entry.fileInfo.fileSize)
		return 0;
	else if ((readOffset + readSize) >= entry.fileInfo.fileSize)
		readSize = (entry.fileInfo.fileSize - readOffset);
	uint64 absFileOffset = entry.fileInfo.fileOffset * m_offsetFactor + readOffset;
	uint32 remainingReadSize = readSize;
	while (remainingReadSize > 0)
	{
		const FSTCachedRawBlock* rawBlock = this->GetDecryptedRawBlock(clusterIndex, absFileOffset/m_sectorSize);
		if (!rawBlock)
			break;
		uint32 blockOffset = (uint32)(absFileOffset % m_sectorSize);
		uint32 bytesToRead = std::min<uint32>(remainingReadSize, m_sectorSize - blockOffset);
		std::memcpy(dataOutU8, rawBlock->blockData.rawData.data() + blockOffset, bytesToRead);
		dataOutU8 += bytesToRead;
		remainingReadSize -= bytesToRead;
		absFileOffset += bytesToRead;
	}
	return readSize - remainingReadSize;
}

uint32 FSTVolume::ReadFile_HashModeHashed(uint32 clusterIndex, FSTEntry& entry, uint32 readOffset, uint32 readSize, void* dataOut)
{
	/*
		Data is divided into 0x10000 (64KiB) blocks
		Layout:
		+0x0000		Hash20[16]		H0 hashes
		+0x0140		Hash20[16]		H1 hashes
		+0x0240		Hash20[16]		H2 hashes
		+0x03C0		uint8[64]		padding
		+0x0400		uint8[0xFC00]	fileData
	
		The hash part (0-0x3FF) uses AES-CBC with IV initialized to zero
		The file part (0x400 - 0xFFFF) uses AES-CBC with IV initialized to block->h0Hash[blockIndex % 16]

		The hash data itself is calculated over 4096 blocks. Where each individual H0 entry hashes a single 0xFC00 file data block (unencrypted)
		Each H1 hash is calculated from 16 H0 hashes
		Each H2 hash is calculated from 16 H1 hashes.
		The  H3 hash is calculated from 16 H2 hashes.
		Thus for each 4096 block group we end up with:
		4096 H0 hashes
		256  H1 hashes
		16	 H2 hashes
		1    H3 hash

		The embedded H0/H1 hashes per block are only a slice of the larger array. Whereas H2 always get embedded as a whole, due to only having 16 hashes in total
		There is also a H4 hash that covers all H3 hashes and is stored in the TMD

	*/

	const FSTCluster& cluster = m_cluster[clusterIndex];
	uint64 fileReadOffset = entry.fileInfo.fileOffset * m_offsetFactor + readOffset;
	uint32 blockIndex = (uint32)(fileReadOffset / BLOCK_FILE_SIZE);
	uint32 bytesRemaining = readSize;
	uint32 offsetWithinBlock = (uint32)(fileReadOffset % BLOCK_FILE_SIZE);
	while (bytesRemaining > 0)
	{
		FSTCachedHashedBlock* block = GetDecryptedHashedBlock(clusterIndex, blockIndex);
		if (!block)
			return 0;
		uint32 bytesToRead = std::min(bytesRemaining, (uint32)BLOCK_FILE_SIZE - offsetWithinBlock);
		std::memcpy(dataOut, block->blockData.getFileData() + offsetWithinBlock, bytesToRead);
		dataOut = (uint8*)dataOut + bytesToRead;
		bytesRemaining -= bytesToRead;
		blockIndex++;
		offsetWithinBlock = 0;
	}
	return readSize - bytesRemaining;
}

bool FSTVolume::OpenDirectoryIterator(std::string_view path, FSTDirectoryIterator& directoryIteratorOut)
{
	FSTFileHandle fileHandle;
	if (!OpenFile(path, fileHandle, false))
		return false;
	if (!IsDirectory(fileHandle))
		return false;
	auto const& fstEntry = m_entries[fileHandle.m_fstIndex];
	directoryIteratorOut.dirHandle = fileHandle;
	directoryIteratorOut.startIndex = fileHandle.m_fstIndex + 1;
	directoryIteratorOut.endIndex = fstEntry.dirInfo.endIndex;
	directoryIteratorOut.currentIndex = directoryIteratorOut.startIndex;
	return true;
}

bool FSTVolume::Next(FSTDirectoryIterator& directoryIterator, FSTFileHandle& fileHandleOut)
{
	if (directoryIterator.currentIndex >= directoryIterator.endIndex)
		return false;
	auto const& fstEntry = m_entries[directoryIterator.currentIndex];
	fileHandleOut.m_fstIndex = directoryIterator.currentIndex;
	if (fstEntry.GetType() == FSTEntry::TYPE::DIRECTORY)
	{
		cemu_assert_debug(fstEntry.dirInfo.endIndex > directoryIterator.currentIndex);
		directoryIterator.currentIndex = fstEntry.dirInfo.endIndex;
	}
	else
		directoryIterator.currentIndex++;
	return true;
}

FSTVolume::~FSTVolume()
{
	for (auto& itr : m_cacheDecryptedRawBlocks)
		delete itr.second;
	for (auto& itr : m_cacheDecryptedHashedBlocks)
		delete itr.second;
	if (m_sourceIsOwned)
		delete m_dataSource;
}

bool FSTVerifier::VerifyContentFile(FileStream* fileContent, const NCrypto::AesKey* key, uint32 contentIndex, uint32 contentSize, uint32 contentSizePadded, bool isSHA1, const uint8* tmdContentHash)
{
	cemu_assert_debug(isSHA1); // test this case
	cemu_assert_debug(((contentSize+0xF)&~0xF) == contentSizePadded);

	std::vector<uint8> buffer;
	buffer.resize(64 * 1024);
	if ((uint32)fileContent->GetSize() != contentSizePadded)
		return false;
	fileContent->SetPosition(0);
	uint8 iv[16]{};
	iv[0] = (contentIndex >> 8) & 0xFF;
	iv[1] = (contentIndex >> 0) & 0xFF;
	// raw content
	uint64 remainingBytes = contentSize;
	uint8 calculatedHash[SHA256_DIGEST_LENGTH];

	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	EVP_DigestInit(ctx, isSHA1 ? EVP_sha1() : EVP_sha256());

	while (remainingBytes > 0)
	{
		uint32 bytesToRead = (uint32)std::min(remainingBytes, (uint64)buffer.size());
		uint32 bytesToReadPadded = ((bytesToRead + 0xF) & ~0xF);
		uint32 bytesRead = fileContent->readData(buffer.data(), bytesToReadPadded);
		if (bytesRead != bytesToReadPadded)
			return false;
		AES128_CBC_decrypt_updateIV(buffer.data(), buffer.data(), bytesToReadPadded, key->b, iv);
		EVP_DigestUpdate(ctx, buffer.data(), bytesToRead);
		remainingBytes -= bytesToRead;
	}
	unsigned int md_len;
	EVP_DigestFinal_ex(ctx, calculatedHash, &md_len);
	EVP_MD_CTX_free(ctx);
	return memcmp(calculatedHash, tmdContentHash, md_len) == 0;
}

bool FSTVerifier::VerifyHashedContentFile(FileStream* fileContent, const NCrypto::AesKey* key, uint32 contentIndex, uint32 contentSize, uint32 contentSizePadded, bool isSHA1, const uint8* tmdContentHash)
{
	if (!isSHA1)
		return false; // not supported
	if ((contentSize % sizeof(FSTHashedBlock)) != 0)
		return false;
	if ((uint32)fileContent->GetSize() != contentSize)
		return false;
	fileContent->SetPosition(0);

	std::vector<NCrypto::CHash160> h0List(4096);

	FSTHashedBlock block;
	uint32 numBlocks = contentSize / sizeof(FSTHashedBlock);
	for (uint32 blockIndex = 0; blockIndex < numBlocks; blockIndex++)
	{
		if (fileContent->readData(&block, sizeof(FSTHashedBlock)) != sizeof(FSTHashedBlock))
			return false;
		uint32 h0Index = (blockIndex % 4096);
		// decrypt hash data and file data
		uint8 iv[16]{};
		AES128_CBC_decrypt(block.getHashData(), block.getHashData(), BLOCK_HASH_SIZE, key->b, iv);
		AES128_CBC_decrypt(block.getFileData(), block.getFileData(), BLOCK_FILE_SIZE, key->b, block.getH0Hash(blockIndex % 16));

		// generate H0 hash and compare
		NCrypto::CHash160 h0;
		SHA1(block.getFileData(), BLOCK_FILE_SIZE, h0.b);
		if (memcmp(h0.b, block.getH0Hash(h0Index & 0xF), sizeof(h0.b)) != 0)
			return false;
		std::memcpy(h0List[h0Index].b, h0.b, sizeof(h0.b));

		// Sixteen H0 hashes become one H1 hash
		if (((h0Index + 1) % 16) == 0 && h0Index > 0)
		{
			uint32 h1Index = ((h0Index - 15) / 16);

			NCrypto::CHash160 h1;
			SHA1((unsigned char *) (h0List.data() + h1Index * 16), sizeof(NCrypto::CHash160) * 16, h1.b);
			if (memcmp(h1.b, block.getH1Hash(h1Index&0xF), sizeof(h1.b)) != 0)
				return false;
		}
		// todo - repeat same for H1 and H2
		//        At the end all H3 hashes are hashed into a single H4 hash which is then compared with the content hash from the TMD

		// Checking only H0 and H1 is sufficient enough for verifying if the file data is intact
		// but if we wanted to be strict and only allow correctly signed data we would have to hash all the way up to H4
	}
	return true;
}

void FSTVolumeTest()
{
	FSTPathUnitTest();
}
