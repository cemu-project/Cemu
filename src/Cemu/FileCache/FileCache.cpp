#include "FileCache.h"
#include "util/helpers/helpers.h"

#include <mutex>
#include <condition_variable>
#include "zlib.h"
#include "Common/FileStream.h"

struct FileCacheAsyncJob
{
	FileCache* fileCache;
	uint64 name1;
	uint64 name2;
	std::vector<uint8> fileData;
};

struct _FileCacheAsyncWriter
{
	_FileCacheAsyncWriter()
	{
		m_isRunning.store(true);
		m_fileCacheThread = std::thread(&_FileCacheAsyncWriter::FileCacheThread, this);
	}

	~_FileCacheAsyncWriter()
	{
		if (m_isRunning.load())
		{
			m_isRunning.store(false);
			m_fileCacheCondVar.notify_one();
			m_fileCacheThread.join();
		}
	}

	void AddJob(FileCache* fileCache, const FileCache::FileName& name, const uint8* fileData, sint32 fileSize)
	{
		FileCacheAsyncJob async;
		async.fileCache = fileCache;
		async.name1 = name.name1;
		async.name2 = name.name2;
		async.fileData = { fileData, fileData + fileSize };

		std::unique_lock lock(m_fileCacheMutex);
		m_writeRequests.emplace_back(std::move(async));

		lock.unlock();
		m_fileCacheCondVar.notify_one();
	}

private:
	void FileCacheThread()
	{
		SetThreadName("fileCache");
		while (true)
		{
			std::unique_lock lock(m_fileCacheMutex);
			while (m_writeRequests.empty())
			{
				m_fileCacheCondVar.wait(lock);
				if (!m_isRunning.load(std::memory_order::relaxed))
					return;
			}

			std::vector<FileCacheAsyncJob> requestsCopy;
			requestsCopy.swap(m_writeRequests); // fast copy & clear
			lock.unlock();

			for (const auto& entry : requestsCopy)
			{
				entry.fileCache->AddFile({ entry.name1, entry.name2 }, entry.fileData.data(), (sint32)entry.fileData.size());
			}
		}
	}

	std::thread m_fileCacheThread;
	std::mutex m_fileCacheMutex;
	std::condition_variable m_fileCacheCondVar;
	std::vector<FileCacheAsyncJob> m_writeRequests;
	std::atomic_bool m_isRunning;
}FileCacheAsyncWriter;

#define FILECACHE_MAGIC_V1					0x8371b694 // used prior to Cemu 1.7.4, only supported caches up to 4GB
#define FILECACHE_MAGIC_V2					0x8371b695 // added support for large caches
#define FILECACHE_MAGIC_V3					0x8371b696 // introduced in Cemu 1.16.0 (non-WIP). Adds zlib compression
#define FILECACHE_HEADER_RESV				128 // number of bytes reserved for the header
#define FILECACHE_FILETABLE_NAME1			0xEFEFEFEFEFEFEFEFULL
#define FILECACHE_FILETABLE_NAME2			0xFEFEFEFEFEFEFEFEULL
#define FILECACHE_FILETABLE_FREE_NAME		0ULL

FileCache* FileCache::Create(const fs::path& path, uint32 extraVersion)
{
	FileStream* fs = FileStream::createFile2(path);
	if (!fs)
	{
		cemuLog_log(LogType::Force, "Failed to create cache file \"{}\"", _pathToUtf8(path));
		return nullptr;
	}
	// init file cache
	auto* fileCache = new FileCache();
	fileCache->fileStream = fs;
	fileCache->dataOffset = FILECACHE_HEADER_RESV;
	fileCache->fileTableEntryCount = 32;
	fileCache->fileTableOffset = 0;
	fileCache->fileTableSize = sizeof(FileTableEntry) * fileCache->fileTableEntryCount;
	fileCache->fileTableEntries = (FileTableEntry*)malloc(fileCache->fileTableSize);
	fileCache->extraVersion = extraVersion;
	memset(fileCache->fileTableEntries, 0, fileCache->fileTableSize);
	// file table stores info about itself
	fileCache->fileTableEntries[0].name1 = FILECACHE_FILETABLE_NAME1;
	fileCache->fileTableEntries[0].name2 = FILECACHE_FILETABLE_NAME2;
	fileCache->fileTableEntries[0].fileOffset = fileCache->fileTableOffset;
	fileCache->fileTableEntries[0].fileSize = fileCache->fileTableSize;
	// write header
	
	fs->writeU32(FILECACHE_MAGIC_V3);
	fs->writeU32(fileCache->extraVersion);
	fs->writeU64(fileCache->dataOffset);
	fs->writeU64(fileCache->fileTableOffset);
	fs->writeU32(fileCache->fileTableSize);
	// write file table
	fs->SetPosition(fileCache->dataOffset+fileCache->fileTableOffset);
	fs->writeData(fileCache->fileTableEntries, fileCache->fileTableSize);
	// done
	return fileCache;
}

FileCache* FileCache::_OpenExisting(const fs::path& path, bool compareExtraVersion, uint32 extraVersion)
{
	FileStream* fs = FileStream::openFile2(path, true);
	if (!fs)
		return nullptr;
	// read header
	uint32 headerMagic = 0;
	fs->readU32(headerMagic);
	bool isV2 = false;
	if (headerMagic != FILECACHE_MAGIC_V1 && headerMagic != FILECACHE_MAGIC_V2 && headerMagic != FILECACHE_MAGIC_V3)
	{
		delete fs;
		return nullptr;
	}
	if (headerMagic == FILECACHE_MAGIC_V1)
	{
		// support for V1 file format removed with the addition of V3
		delete fs;
		return nullptr;
	}
	if (headerMagic == FILECACHE_MAGIC_V2)
		isV2 = true;
	uint32 headerExtraVersion = 0xFFFFFFFF;
	fs->readU32(headerExtraVersion);
	if (compareExtraVersion && headerExtraVersion != extraVersion)
	{
		delete fs;
		return nullptr;
	}
	if (!compareExtraVersion)
	{
		extraVersion = headerExtraVersion;
	}

	uint64 headerDataOffset = 0;
	uint64 headerFileTableOffset = 0;
	uint32 headerFileTableSize = 0;
	fs->readU64(headerDataOffset);
	fs->readU64(headerFileTableOffset);
	if (!fs->readU32(headerFileTableSize))
	{
		cemuLog_log(LogType::Force, "\"{}\" is corrupted", _pathToUtf8(path));
		delete fs;
		return nullptr;
	}
	uint32 fileTableEntryCount = 0;
	bool invalidFileTableSize = false;
	// V2 and V3
	fileTableEntryCount = headerFileTableSize / sizeof(FileTableEntry);
	invalidFileTableSize = (headerFileTableSize % sizeof(FileTableEntry)) != 0;
	if (invalidFileTableSize)
	{
		cemuLog_log(LogType::Force, "\"{}\" is corrupted", _pathToUtf8(path));
		delete fs;
		return nullptr;
	}
	// init struct
	auto* fileCache = new FileCache();
	fileCache->fileStream = fs;
	fileCache->extraVersion = extraVersion;
	fileCache->dataOffset = headerDataOffset;
	fileCache->fileTableEntryCount = fileTableEntryCount;
	fileCache->fileTableOffset = headerFileTableOffset;
	fileCache->fileTableSize = fileTableEntryCount * sizeof(FileTableEntry);
	fileCache->fileTableEntries = (FileTableEntry*)malloc(fileTableEntryCount * sizeof(FileTableEntry));
	memset(fileCache->fileTableEntries, 0, fileTableEntryCount * sizeof(FileTableEntry));
	// read file table
	fileCache->fileStream->SetPosition(fileCache->dataOffset + fileCache->fileTableOffset);
	bool incompleteFileTable = false;
	if (isV2)
	{
		// read file table entries in old format
		incompleteFileTable = fileCache->fileStream->readData(fileCache->fileTableEntries, fileCache->fileTableSize) != fileCache->fileTableSize;
		// in V2 the extra field wasn't guaranteed to have well defined values
		for (uint32 i = 0; i < fileTableEntryCount; i++)
		{
			fileCache->fileTableEntries[i].flags = FileTableEntry::FLAGS::FLAG_NONE;
			fileCache->fileTableEntries[i].extraReserved1 = 0;
			fileCache->fileTableEntries[i].extraReserved2 = 0;
			fileCache->fileTableEntries[i].extraReserved3 = 0;
		}
	}
	else
	{
		incompleteFileTable = fileCache->fileStream->readData(fileCache->fileTableEntries, fileCache->fileTableSize) != fileCache->fileTableSize;
	}
	if (incompleteFileTable)
	{
		cemuLog_log(LogType::Force, "\"{}\" is corrupted (incomplete file table)", _pathToUtf8(path));
		delete fileCache;
		return nullptr;
	}
	return fileCache;
}

FileCache* FileCache::Open(const fs::path& path, bool allowCreate, uint32 extraVersion)
{
	FileCache* fileCache = _OpenExisting(path, true, extraVersion);
	if (fileCache)
		return fileCache;
	if (!allowCreate)
		return nullptr;
	return Create(path, extraVersion);
}

FileCache* FileCache::Open(const fs::path& path)
{
	return _OpenExisting(path, false, 0);
}

FileCache::~FileCache()
{
	free(this->fileTableEntries);
	delete fileStream;
}

void FileCache::fileCache_updateFiletable(sint32 extraEntriesToAllocate)
{
	// recreate file table with bigger size (optional)
	this->fileTableEntries[0].name1 = FILECACHE_FILETABLE_FREE_NAME;
	this->fileTableEntries[0].name2 = FILECACHE_FILETABLE_FREE_NAME;
	sint32 newFileTableEntryCount = this->fileTableEntryCount + extraEntriesToAllocate;
	this->fileTableEntries = (FileTableEntry*)realloc(this->fileTableEntries, sizeof(FileTableEntry)*newFileTableEntryCount);
	for (sint32 f = this->fileTableEntryCount; f < newFileTableEntryCount; f++)
	{
		this->fileTableEntries[f].name1 = FILECACHE_FILETABLE_FREE_NAME;
		this->fileTableEntries[f].name2 = FILECACHE_FILETABLE_FREE_NAME;
		this->fileTableEntries[f].fileOffset = 0;
		this->fileTableEntries[f].fileSize = 0;
		this->fileTableEntries[f].flags = FileTableEntry::FLAGS::FLAG_NONE;
		this->fileTableEntries[f].extraReserved1 = 0;
		this->fileTableEntries[f].extraReserved2 = 0;
		this->fileTableEntries[f].extraReserved3 = 0;
	}
	this->fileTableEntryCount = newFileTableEntryCount;
	this->_addFileInternal(FILECACHE_FILETABLE_NAME1, FILECACHE_FILETABLE_NAME2, (uint8*)this->fileTableEntries, sizeof(FileTableEntry)*newFileTableEntryCount, true);
	// update file table info in struct
	if (this->fileTableEntries[0].name1 != FILECACHE_FILETABLE_NAME1 || this->fileTableEntries[0].name2 != FILECACHE_FILETABLE_NAME2)
	{
		cemuLog_log(LogType::Force, "Corruption in cache file detected");
		assert_dbg();
	}
	this->fileTableOffset = this->fileTableEntries[0].fileOffset;
	this->fileTableSize = this->fileTableEntries[0].fileSize;
	// update header
	fileStream->SetPosition(0);
	fileStream->writeU32(FILECACHE_MAGIC_V3);
	fileStream->writeU32(this->extraVersion);
	fileStream->writeU64(this->dataOffset);
	fileStream->writeU64(this->fileTableOffset);
	fileStream->writeU32(this->fileTableSize);
}

uint8* _fileCache_compressFileData(const uint8* fileData, uint32 fileSize, sint32& compressedSize)
{
	// compress data using zlib deflate
	// stores the size of the uncompressed file in the first 4 bytes
	Bytef* uncompressedInput = (Bytef*)fileData;
	uLongf uncompressedLen = fileSize;
	uLongf compressedLen = compressBound(fileSize);
	Bytef* compressedData = (Bytef*)malloc(4 + compressedLen);
	int zret = compress2(compressedData + 4, &compressedLen, uncompressedInput, uncompressedLen, 4); // level 4 has good compression to performance ratio
	if (zret != Z_OK)
	{
		free(compressedData);
		return nullptr;
	}
	compressedData[0] = ((uint32)fileSize >> 24) & 0xFF;
	compressedData[1] = ((uint32)fileSize >> 16) & 0xFF;
	compressedData[2] = ((uint32)fileSize >> 8) & 0xFF;
	compressedData[3] = ((uint32)fileSize >> 0) & 0xFF;
	compressedSize = 4 + compressedLen;
	return compressedData;
}

bool _uncompressFileData(const uint8* rawData, size_t rawSize, std::vector<uint8>& dataOut)
{
	if (rawSize < 4)
	{
		dataOut.clear();
		return false;
	}
	// get size of uncompressed file
	uint32 fileSize = 0;
	fileSize |= ((uint32)rawData[0] << 24);
	fileSize |= ((uint32)rawData[1] << 16);
	fileSize |= ((uint32)rawData[2] << 8);
	fileSize |= ((uint32)rawData[3] << 0);
	// allocate buffer
	Bytef* compressedInput = (Bytef*)rawData + 4;
	uLongf compressedLen = (uLongf)(rawSize - 4);
	uLongf uncompressedLen = fileSize;
	dataOut.resize(fileSize);	
	int zret = uncompress2(dataOut.data(), &uncompressedLen, compressedInput, &compressedLen);
	if (zret != Z_OK)
	{
		dataOut.clear();
		return false;
	}
	if (uncompressedLen != fileSize || compressedLen != (rawSize - 4))
	{
		// uncompressed size does not match stored size
		dataOut.clear();
		return false;
	}
	return true;
}

void FileCache::_addFileInternal(uint64 name1, uint64 name2, const uint8* fileData, sint32 fileSize, bool noCompression)
{
	if (fileSize < 0)
		return;
	if (!enableCompression)
		noCompression = true;
	// compress data
	sint32 rawSize = 0;
	uint8* rawData = nullptr;
	bool isCompressed = false;
	if (noCompression)
	{
		rawData = (uint8*)fileData;
		rawSize = fileSize;
	}
	else
	{
		// compress file
		rawData = _fileCache_compressFileData(fileData, fileSize, rawSize);
		if (rawData)
		{
			isCompressed = true;
		}
		else
		{
			rawData = (uint8*)fileData;
			rawSize = fileSize;
		}
	}
	std::unique_lock lock(this->mutex);
	// find free entry in file table
	sint32 entryIndex = -1;
	// scan for already existing entry
	for (sint32 i = 0; i < this->fileTableEntryCount; i++)
	{
		if (this->fileTableEntries[i].name1 == name1 && this->fileTableEntries[i].name2 == name2)
		{
			entryIndex = i;
			break;
		}
	}
	if (entryIndex == -1)
	{
		while (true)
		{
			// if no entry exists, search for empty one
			for (sint32 i = 0; i < this->fileTableEntryCount; i++)
			{
				if (this->fileTableEntries[i].name1 == FILECACHE_FILETABLE_FREE_NAME && this->fileTableEntries[i].name2 == FILECACHE_FILETABLE_FREE_NAME)
				{
					entryIndex = i;
					break;
				}
			}
			if (entryIndex == -1)
			{
				if (name1 == FILECACHE_FILETABLE_NAME1 && name2 == FILECACHE_FILETABLE_NAME2)
				{
					cemuLog_log(LogType::Force, "Error in cache file");
					cemu_assert_debug(false);
				}
				// no free entry, recreate file table with larger size
				fileCache_updateFiletable(64);
				// try again
				continue;
			}
			else
				break;
		}
	}
	// find free space
	sint64 currentStartOffset = 0;
	while (true)
	{
		bool hasCollision = false;
		sint64 currentEndOffset = currentStartOffset + rawSize;
		FileTableEntry* entry = this->fileTableEntries;
		FileTableEntry* entryLast = this->fileTableEntries + this->fileTableEntryCount;
		while (entry < entryLast)
		{
			if (entry->name1 == FILECACHE_FILETABLE_FREE_NAME && entry->name2 == FILECACHE_FILETABLE_FREE_NAME)
			{
				entry++;
				continue;
			}
			if (currentEndOffset >= (sint64)entry->fileOffset && currentStartOffset < (sint64)(entry->fileOffset + entry->fileSize))
			{
				currentStartOffset = entry->fileOffset + entry->fileSize;
				hasCollision = true;
				break;
			}
			entry++;
		}
		// optimized logic to speed up scanning for free offsets
		// assumes that most of the time entries are stored in direct succession (holds true more often than not)
		if (hasCollision && (entry + 1) < entryLast)
		{
			entry++;
			while (entry < entryLast)
			{
				if (entry->name1 == FILECACHE_FILETABLE_FREE_NAME && entry->name2 == FILECACHE_FILETABLE_FREE_NAME)
				{
					entry++;
					continue;
				}
				if (entry->fileOffset == currentStartOffset)
				{
					currentStartOffset = entry->fileOffset + entry->fileSize;
					entry++;
					continue;
				}
				break;
			}
		}
		// retry in case of collision
		if (hasCollision == false)
			break;
	}
	// update file table entry
	this->fileTableEntries[entryIndex].name1 = name1;
	this->fileTableEntries[entryIndex].name2 = name2;
	this->fileTableEntries[entryIndex].fileOffset = currentStartOffset;
	this->fileTableEntries[entryIndex].fileSize = rawSize;
	this->fileTableEntries[entryIndex].flags = isCompressed ? FileTableEntry::FLAGS::FLAG_COMPRESSED : FileTableEntry::FLAGS::FLAG_NONE;
	this->fileTableEntries[entryIndex].extraReserved1 = 0;
	this->fileTableEntries[entryIndex].extraReserved2 = 0;
	this->fileTableEntries[entryIndex].extraReserved3 = 0;
	// write file data
	fileStream->SetPosition(this->dataOffset + currentStartOffset);
	fileStream->writeData(rawData, rawSize);
	// write file table entry
	fileStream->SetPosition(this->dataOffset + this->fileTableOffset + (uint64)(sizeof(FileTableEntry)*entryIndex));
	fileStream->writeData(this->fileTableEntries + entryIndex, sizeof(FileTableEntry));
	if (isCompressed)
		free(rawData);
}

void FileCache::AddFile(const FileName&& name, const uint8* fileData, sint32 fileSize)
{
	this->_addFileInternal(name.name1, name.name2, fileData, fileSize, false);
}

bool FileCache::DeleteFile(const FileName&& name)
{
	if( name.name1 == FILECACHE_FILETABLE_NAME1 && name.name2 == FILECACHE_FILETABLE_NAME2 )
		return false; // prevent filetable from being deleted
	std::unique_lock lock(this->mutex);
	FileTableEntry* entry = this->fileTableEntries;
	FileTableEntry* entryLast = this->fileTableEntries+this->fileTableEntryCount;
	while( entry < entryLast )
	{
		if( entry->name1 == name.name1 && entry->name2 == name.name2 )
		{
			entry->name1 = FILECACHE_FILETABLE_FREE_NAME;
			entry->name2 = FILECACHE_FILETABLE_FREE_NAME;
			entry->fileOffset = 0;
			entry->fileSize = 0;
			// store updated entry to file cache
			size_t entryIndex = entry - this->fileTableEntries;
			fileStream->SetPosition(this->dataOffset+this->fileTableOffset+(uint64)(sizeof(FileTableEntry)*entryIndex));
			fileStream->writeData(this->fileTableEntries+entryIndex, sizeof(FileTableEntry));
			return true;
		}
		entry++;
	}
	return false;
}

void FileCache::AddFileAsync(const FileName& name, const uint8* fileData, sint32 fileSize)
{
	FileCacheAsyncWriter.AddJob(this, name, fileData, fileSize);
}

bool FileCache::_getFileDataInternal(const FileTableEntry* entry, std::vector<uint8>& dataOut)
{
	std::vector<uint8> rawData(entry->fileSize);

	fileStream->SetPosition(this->dataOffset + entry->fileOffset);
	fileStream->readData(rawData.data(), entry->fileSize);

	if ((entry->flags&FileTableEntry::FLAG_COMPRESSED) == 0)
	{
		// uncompressed
		std::swap(rawData, dataOut);
		return true;
	}
	// decompress
	sint32 uncompressedSize = 0;
	if (!_uncompressFileData(rawData.data(), rawData.size(), dataOut))
	{
		dataOut.clear();
		return false;
	}
	return true;
}

bool FileCache::GetFile(const FileName&& name, std::vector<uint8>& dataOut)
{
	std::unique_lock lock(this->mutex);
	FileTableEntry* entry = this->fileTableEntries;
	FileTableEntry* entryLast = this->fileTableEntries+this->fileTableEntryCount;
	while( entry < entryLast )
	{
		if( entry->name1 == name.name1 && entry->name2 == name.name2 )
		{
			return _getFileDataInternal(entry, dataOut);
		}
		entry++;
	}
	dataOut.clear();
	return false;
}

bool FileCache::GetFileByIndex(sint32 index, uint64* name1, uint64* name2, std::vector<uint8>& dataOut)
{
	if (index < 0 || index >= this->fileTableEntryCount)
		return false;
	FileTableEntry* entry = this->fileTableEntries + index;
	if (this->fileTableEntries == nullptr)
	{
		cemuLog_log(LogType::Force, "GetFileByIndex() fileTable is NULL");
		return false;
	}
	if (entry->name1 == FILECACHE_FILETABLE_FREE_NAME && entry->name2 == FILECACHE_FILETABLE_FREE_NAME)
		return false;
	if (entry->name1 == FILECACHE_FILETABLE_NAME1 && entry->name2 == FILECACHE_FILETABLE_NAME2)
		return false;

	std::unique_lock lock(this->mutex);
	if(name1)
		*name1 = entry->name1;
	if(name2)
		*name2 = entry->name2;
	return _getFileDataInternal(entry, dataOut);
}

bool FileCache::HasFile(const FileName&& name)
{
	std::unique_lock lock(this->mutex);
	FileTableEntry* entry = this->fileTableEntries;
	FileTableEntry* entryLast = this->fileTableEntries + this->fileTableEntryCount;
	while (entry < entryLast)
	{
		if (entry->name1 == name.name1 && entry->name2 == name.name2)
			return true;
		entry++;
	}
	return false;
}

sint32 FileCache::GetMaximumFileIndex()
{
	return this->fileTableEntryCount;
}

sint32 FileCache::GetFileCount()
{
	std::unique_lock lock(this->mutex);
	sint32 fileCount = 0;
	FileTableEntry* entry = this->fileTableEntries;
	FileTableEntry* entryLast = this->fileTableEntries+this->fileTableEntryCount;
	while( entry < entryLast )
	{
		if( entry->name1 == FILECACHE_FILETABLE_FREE_NAME && entry->name2 == FILECACHE_FILETABLE_FREE_NAME )
		{
			entry++;
			continue;
		}
		if( entry->name1 == FILECACHE_FILETABLE_NAME1 && entry->name2 == FILECACHE_FILETABLE_NAME2 )
		{
			entry++;
			continue;
		}
		fileCount++;
		entry++;
	}

	return fileCount;
}

void fileCache_test()
{
	FileCache* fc = FileCache::Create("testCache.bin", 0);
	uint32 time1 = GetTickCount();

	char* testString1 = (char*)malloc(1024 * 1024 * 8);
	char* testString2 = (char*)malloc(1024 * 1024 * 8);
	char* testString3 = (char*)malloc(1024 * 1024 * 8);
	for (sint32 f = 0; f < 1024 * 1024 * 8; f++)
	{
		testString1[f] = 'a' + (f & 7);
		testString2[f] = 'd' + (f & 3);
		testString3[f] = 'f' + (f & 3);
	}

	for(sint32 i=0; i<2200; i++)
	{
		fc->AddFile({ 0x1000001ULL, (uint64)i }, (uint8*)testString1, 1024 * 1024 * 1);
		fc->AddFile({ 0x1000002ULL, (uint64)i }, (uint8*)testString2, 1024 * 1024 * 1);
		fc->AddFile({ 0x1000003ULL, (uint64)i }, (uint8*)testString3, 1024 * 1024 * 1);
	}
	uint32 time2 = GetTickCount();
	debug_printf("Writing took %dms\n", time2-time1);
	delete fc;
	// verify if all entries are still valid
	FileCache* fcRead = FileCache::Open("testCache.bin", 0);
	uint32 time3 = GetTickCount();
	for(sint32 i=0; i<2200; i++)
	{
		std::vector<uint8> fileData;
		bool r = fcRead->GetFile({ 0x1000001ULL, (uint64)i }, fileData);
		if (!r || fileData.size() != 1024 * 1024 * 1 || memcmp(fileData.data(), testString1, 1024 * 1024 * 1) != 0)
			cemu_assert_debug(false);
		r = fcRead->GetFile({ 0x1000002ULL, (uint64)i }, fileData);
		if( !r || fileData.size() != 1024 * 1024 * 1 || memcmp(fileData.data(), testString2, 1024 * 1024 * 1) != 0 )
			cemu_assert_debug(false);
		r = fcRead->GetFile({ 0x1000003ULL, (uint64)i }, fileData);
		if( !r || fileData.size() != 1024 * 1024 * 1 || memcmp(fileData.data(), testString3, 1024 * 1024 * 1) != 0 )
			cemu_assert_debug(false);
	}
	uint32 time4 = GetTickCount();
	debug_printf("Reading took %dms\n", time4-time3);
	delete fcRead;
	cemu_assert_debug(false);
	exit(0);
}
