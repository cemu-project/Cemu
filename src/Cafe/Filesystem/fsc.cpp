#include "Cafe/Filesystem/fsc.h"
#include "Cafe/Filesystem/FST/fstUtil.h"

struct FSCMountPathNode
{
	std::string path;
	std::vector<FSCMountPathNode*> subnodes;
	FSCMountPathNode* parent;
	// associated device target and path
	fscDeviceC* device{ nullptr };
	void* ctx{ nullptr };
	std::string deviceTargetPath; // the destination base path for the device, utf8
	// priority
	sint32 priority{};

	FSCMountPathNode(FSCMountPathNode* parent) : parent(parent)
	{
	}

    void AssignDevice(fscDeviceC* device, void* ctx, std::string_view deviceBasePath)
    {
        this->device = device;
        this->ctx = ctx;
        this->deviceTargetPath = deviceBasePath;
    }

    void UnassignDevice()
    {
        this->device = nullptr;
        this->ctx = nullptr;
        this->deviceTargetPath.clear();
    }

    bool IsRootNode() const
    {
        return !parent;
    }

	~FSCMountPathNode()
	{
		for (auto& itr : subnodes)
			delete itr;
		subnodes.clear();
	}
};

// compare two file or directory names using FSA rules
bool FSA_CompareNodeName(std::string_view a, std::string_view b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); i++)
	{
		uint8 ac = (uint8)a[i];
		uint8 bc = (uint8)b[i];
		// lower case compare
		if (ac >= (uint8)'A' && ac <= (uint8)'Z')
			ac -= ((uint8)'A' - (uint8)'a');
		if (bc >= (uint8)'A' && bc <= (uint8)'Z')
			bc -= ((uint8)'A' - (uint8)'a');
		if (ac != bc)
			return false;
	}
	return true;
}

FSCMountPathNode* s_fscRootNodePerPrio[FSC_PRIORITY_COUNT]{};

std::recursive_mutex s_fscMutex;

#define fscEnter() s_fscMutex.lock();
#define fscLeave() s_fscMutex.unlock();

FSCMountPathNode* fsc_lookupPathVirtualNode(const char* path, sint32 priority = FSC_PRIORITY_BASE);

void fsc_reset()
{
	// delete existing nodes
	for (auto& itr : s_fscRootNodePerPrio)
	{
		delete itr;
		itr = nullptr;
	}
	// init root node for each priority
	for (sint32 i = 0; i < FSC_PRIORITY_COUNT; i++)
		s_fscRootNodePerPrio[i] = new FSCMountPathNode(nullptr);
}

/*
 * Creates a node chain for the given mount path. Returns the bottom node.
 * If the path already exists for the given priority, NULL is returned (we can't mount two devices to the same path with the same priority)
 * But we can map devices to subdirectories. Something like this is possible:
 * /vol/content		 -> Map to WUD (includes all subdirectories except /data, which is handled by the entry below. This exclusion rule applies only if the priority of both mount entries is the same)
 * /vol/content/data -> Map to HostFS
 * If overlapping paths with different priority are created, then the higher priority one will be checked first
 */
FSCMountPathNode* fsc_createMountPath(const FSCPath& mountPath, sint32 priority)
{
	cemu_assert(priority >= 0 && priority < FSC_PRIORITY_COUNT);
	fscEnter();
	FSCMountPathNode* nodeParent = s_fscRootNodePerPrio[priority];
	for (size_t i=0; i< mountPath.GetNodeCount(); i++)
	{
		// search for subdirectory
		FSCMountPathNode* nodeSub = nullptr; // set if we found a subnode with a matching name, else this is used to store the new nodes
		for (auto& nodeItr : nodeParent->subnodes)
		{
			if (mountPath.MatchNodeName(i, nodeItr->path))
			{
				// subnode found
				nodeSub = nodeItr;
				break;
			}
		}
		if (nodeSub)
		{
			// traverse subnode
			nodeParent = nodeSub;
			continue;
		}
		// no matching subnode, add new entry
		nodeSub = new FSCMountPathNode(nodeParent);
		nodeSub->path = mountPath.GetNodeName(i);
		nodeSub->priority = priority;
		nodeParent->subnodes.emplace_back(nodeSub);
		if (i == (mountPath.GetNodeCount() - 1))
		{
			// last node
			fscLeave();
			return nodeSub;
		}
		// traverse subnode
		nodeParent = nodeSub;
	}
	// path is empty or already mounted
	fscLeave();
	if (mountPath.GetNodeCount() == 0)
		return nodeParent;
	return nullptr;
}

// Map a virtual FSC directory to a device. targetPath points to the destination base directory within the device
sint32 fsc_mount(std::string_view mountPath, std::string_view targetPath, fscDeviceC* fscDevice, void* ctx, sint32 priority)
{
	cemu_assert(fscDevice);
	std::string mountPathTmp(mountPath);
	// make sure the target path ends with a slash
	std::string targetPathWithSlash(targetPath);
	if (!targetPathWithSlash.empty() && (targetPathWithSlash.back() != '/' && targetPathWithSlash.back() != '\\'))
		targetPathWithSlash.push_back('/');

	FSCPath parsedMountPath(mountPathTmp);
	// register path
	fscEnter();
	FSCMountPathNode* node = fsc_createMountPath(parsedMountPath, priority);
	if( !node )
	{
		// path empty, invalid or already used
		cemuLog_log(LogType::Force, "fsc_mount failed (virtual path: {})", mountPath);
		fscLeave();
		return FSC_STATUS_INVALID_PATH;
	}
    node->AssignDevice(fscDevice, ctx, targetPathWithSlash);
	fscLeave();
	return FSC_STATUS_OK;
}

bool fsc_unmount(std::string_view mountPath, sint32 priority)
{
	std::string _tmp(mountPath);
	fscEnter();
	FSCMountPathNode* mountPathNode = fsc_lookupPathVirtualNode(_tmp.c_str(), priority);
	if (!mountPathNode)
	{
		fscLeave();
		return false;
	}
	cemu_assert(mountPathNode->priority == priority);
	cemu_assert(mountPathNode->device);
    // unassign device
    mountPathNode->UnassignDevice();
	// prune empty branch
	while (mountPathNode && !mountPathNode->IsRootNode() && mountPathNode->subnodes.empty() && !mountPathNode->device)
	{
		FSCMountPathNode* parent = mountPathNode->parent;
        std::erase(parent->subnodes, mountPathNode);
		delete mountPathNode;
		mountPathNode = parent;
	}
	fscLeave();
	return true;
}

void fsc_unmountAll()
{
	fscEnter();
	fsc_reset();
	fscLeave();
}

// lookup virtual path and find mounted device and relative device directory
bool fsc_lookupPath(const char* path, std::string& devicePathOut, fscDeviceC** fscDeviceOut, void** ctxOut, sint32 priority = FSC_PRIORITY_BASE)
{
	FSCPath parsedPath(path);
	FSCMountPathNode* nodeParent = s_fscRootNodePerPrio[priority];
	size_t i;
	fscEnter();
	for (i = 0; i < parsedPath.GetNodeCount(); i++)
	{
		// search for subdirectory
		FSCMountPathNode* nodeSub = nullptr;
		for(auto& nodeItr : nodeParent->subnodes)
		{			
			if (parsedPath.MatchNodeName(i, nodeItr->path))
			{
				nodeSub = nodeItr;
				break;
			}
		}
		if (nodeSub)
		{
			nodeParent = nodeSub;
			continue;
		}
		// no matching subnode
		break;
	}
	// if the found node is not a device mount point, then travel back towards the root until we find one
	while (nodeParent)
	{
		if (nodeParent->device)
		{
			devicePathOut = nodeParent->deviceTargetPath;
			for (size_t f = i; f < parsedPath.GetNodeCount(); f++)
			{
				auto nodeName = parsedPath.GetNodeName(f);
				devicePathOut.append(nodeName);
				if (f < (parsedPath.GetNodeCount() - 1))
					devicePathOut.push_back('/');
			}
			*fscDeviceOut = nodeParent->device;
			*ctxOut = nodeParent->ctx;
			fscLeave();
			return true;
		}
		nodeParent = nodeParent->parent;
		i--;
	}
	fscLeave();
	return false;
}

// lookup path and find virtual device node
FSCMountPathNode* fsc_lookupPathVirtualNode(const char* path, sint32 priority)
{
	FSCPath parsedPath(path);
	FSCMountPathNode* nodeCurrentDir = s_fscRootNodePerPrio[priority];
	fscEnter();
	for (size_t i = 0; i < parsedPath.GetNodeCount(); i++)
	{
		// search for subdirectory
		FSCMountPathNode* nodeSub = nullptr;
		for (auto& nodeItr : nodeCurrentDir->subnodes)
		{
			if (parsedPath.MatchNodeName(i, nodeItr->path))
			{
				nodeSub = nodeItr;
				break;
			}
		}
		if (nodeSub)
		{
			// traverse subdirectory
			nodeCurrentDir = nodeSub;
			continue;
		}
		fscLeave();
		return nullptr;
	}
	fscLeave();
	return nodeCurrentDir;
}

// this wraps multiple iterated directories from different devices into one unified virtual representation
class FSCVirtualFileDirectoryIterator : public FSCVirtualFile
{
public:
	sint32 fscGetType() override
	{
		return FSC_TYPE_DIRECTORY;
	}

	FSCVirtualFileDirectoryIterator(std::string_view path, std::span<FSCVirtualFile*> mappedFolders)
		: m_path(path), m_folders(mappedFolders.begin(), mappedFolders.end())
	{
		dirIterator = nullptr;
	}

	~FSCVirtualFileDirectoryIterator()
	{
		// dirIterator is deleted in base constructor
		for (auto& itr : m_folders)
			delete itr;
	}

	bool fscDirNext(FSCDirEntry* dirEntry) override
	{
		if (!dirIterator)
		{
			// lazily populate list only if directory is actually iterated
			PopulateIterationList();
			cemu_assert_debug(dirIterator);
		}
		if (dirIterator->index >= dirIterator->dirEntries.size())
			return false;
		*dirEntry = dirIterator->dirEntries[dirIterator->index];
		dirIterator->index++;
		return true;
	}

	bool fscRewindDir() override
	{
		if (!dirIterator)
			return true;

		dirIterator->index  = 0;
		return true;
	}

	void addUniqueDirEntry(const FSCDirEntry& dirEntry)
	{
		// skip if already in list
		for (auto& itr : dirIterator->dirEntries)
		{
			if (FSA_CompareNodeName(dirEntry.path, itr.path))
				return;
		}
		dirIterator->dirEntries.emplace_back(dirEntry);
	}

private:
	void PopulateIterationList()
	{
		cemu_assert_debug(!dirIterator);
		dirIterator = new FSCVirtualFile::FSCDirIteratorState();
		FSCDirEntry dirEntry;
		fscEnter();
		for (auto& itr : m_folders)
		{
			while (itr->fscDirNext(&dirEntry))
				addUniqueDirEntry(dirEntry);
		}
		for (sint32 prio = FSC_PRIORITY_COUNT - 1; prio >= 0; prio--)
		{
			FSCMountPathNode* nodeVirtualPath = fsc_lookupPathVirtualNode(m_path.c_str(), prio);
			if (nodeVirtualPath)
			{
				for (auto& itr : nodeVirtualPath->subnodes)
				{
					dirEntry = {};
					dirEntry.isDirectory = true;
					strncpy(dirEntry.path, itr->path.c_str(), sizeof(dirEntry.path) - 1);
					dirEntry.path[sizeof(dirEntry.path) - 1] = '\0';
					dirEntry.fileSize = 0;
					addUniqueDirEntry(dirEntry);
				}
			}
		}
		fscLeave();
	}

private:
	std::string m_path;
	std::vector<FSCVirtualFile*> m_folders; // list of all folders mapped to the same directory (at different priorities)
};

// Open file or directory from virtual file system
FSCVirtualFile* fsc_open(const char* path, FSC_ACCESS_FLAG accessFlags, sint32* fscStatus, sint32 maxPriority)
{
	cemu_assert_debug(HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_FILE) || HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_DIR)); // must open either file or directory
	FSCVirtualFile* dirList[FSC_PRIORITY_COUNT];
	uint8 dirListCount = 0;
	std::string devicePath;
	fscDeviceC* fscDevice = NULL;
	*fscStatus = FSC_STATUS_UNDEFINED;
	void* ctx;
	fscEnter();
	for (sint32 prio = maxPriority; prio >= 0; prio--)
	{
		if (fsc_lookupPath(path, devicePath, &fscDevice, &ctx, prio))
		{
			FSCVirtualFile* fscVirtualFile = fscDevice->fscDeviceOpenByPath(devicePath, accessFlags, ctx, fscStatus);
			if (fscVirtualFile)
			{
				if (fscVirtualFile->fscGetType() == FSC_TYPE_DIRECTORY)
				{
					cemu_assert_debug(HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_DIR));
					// collect all folders 
					dirList[dirListCount] = fscVirtualFile;
					dirListCount++;
				}
				else
				{
					// return first found file
					cemu_assert_debug(HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_FILE));
					fscVirtualFile->m_isAppend = HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::IS_APPEND);
					fscLeave();
					return fscVirtualFile;
				}				
			}
		}
	}
	// for directories we create a virtual representation of the enumerated files of all priorities as well as the FSC folder structure itself
	if (HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_DIR))
	{
		// create a virtual directory VirtualFile that represents all the mounted folders as well as the virtual FSC folder structure
		bool folderExists = dirListCount > 0;
		for (sint32 prio = FSC_PRIORITY_COUNT - 1; prio >= 0; prio--)
		{
			if (folderExists)
				break;
			folderExists |= (fsc_lookupPathVirtualNode(path, prio) != 0);
		}
		if (folderExists)
		{
			FSCVirtualFileDirectoryIterator* dirIteratorFile = new FSCVirtualFileDirectoryIterator(path, { dirList, dirListCount});
			*fscStatus = FSC_STATUS_OK;
			fscLeave();
			return dirIteratorFile;
		}
	}
	else
	{
		cemu_assert_debug(dirListCount == 0);
	}
	fscLeave();
	*fscStatus = FSC_STATUS_FILE_NOT_FOUND;
	return nullptr;
}

/*
 * Open file using virtual path
 */
FSCVirtualFile* fsc_openDirIterator(const char* path, sint32* fscStatus)
{
	return fsc_open(path, FSC_ACCESS_FLAG::OPEN_DIR, fscStatus);
}

/*
* Iterate next node in directory
* Returns false if there is no node left
*/
bool fsc_nextDir(FSCVirtualFile* fscFile, FSCDirEntry* dirEntry)
{
	fscEnter();
	if (fscFile->fscGetType() != FSC_TYPE_DIRECTORY)
	{
		cemu_assert_suspicious();
		fscLeave();
		return false;
	}
	bool r = fscFile->fscDirNext(dirEntry);
	fscLeave();
	return r;
}

/*
 * Create directory
 */
bool fsc_createDir(const char* path, sint32* fscStatus)
{
	fscDeviceC* fscDevice = NULL;
	*fscStatus = FSC_STATUS_UNDEFINED;
	void* ctx;
	std::string devicePath;
	fscEnter();
	if( fsc_lookupPath(path, devicePath, &fscDevice, &ctx) )
	{
		sint32 status = fscDevice->fscDeviceCreateDir(devicePath, ctx, fscStatus);
		fscLeave();
		return status;
	}
	fscLeave();
	return false;
}

/*
 * Rename file or directory
 */
bool fsc_rename(const char* srcPath, const char* dstPath, sint32* fscStatus)
{
	std::string srcDevicePath;
	std::string dstDevicePath;
	void* srcCtx;
	void* dstCtx;
	fscDeviceC* fscSrcDevice = NULL;
	fscDeviceC* fscDstDevice = NULL;
	*fscStatus = FSC_STATUS_UNDEFINED;
	if( fsc_lookupPath(srcPath, srcDevicePath, &fscSrcDevice, &srcCtx) && fsc_lookupPath(dstPath, dstDevicePath, &fscDstDevice, &dstCtx) )
	{
		if( fscSrcDevice == fscDstDevice )
			return fscSrcDevice->fscDeviceRename(srcDevicePath, dstDevicePath, srcCtx, fscStatus);
	}
	return false;
}

/*
 * Delete file or subdirectory
 */
bool fsc_remove(const char* path, sint32* fscStatus)
{
	std::string devicePath;
	fscDeviceC* fscDevice = NULL;
	*fscStatus = FSC_STATUS_UNDEFINED;
	void* ctx;
	if( fsc_lookupPath(path, devicePath, &fscDevice, &ctx) )
	{
		return fscDevice->fscDeviceRemoveFileOrDir(devicePath, ctx, fscStatus);
	}
	return false;
}

/*
 * Close file handle
 */
void fsc_close(FSCVirtualFile* fscFile)
{
	fscEnter();
	delete fscFile;
	fscLeave();
}

/*
 * Return size of file
 */
uint32 fsc_getFileSize(FSCVirtualFile* fscFile)
{
	return (uint32)fscFile->fscQueryValueU64(FSC_QUERY_SIZE);
}

/*
 * Return file position
 */
uint32 fsc_getFileSeek(FSCVirtualFile* fscFile)
{
	return (uint32)fscFile->fscGetSeek();
}

/*
 * Set file seek
 * For writable files the seek pointer can be set past the end of the file
 */
void fsc_setFileSeek(FSCVirtualFile* fscFile, uint32 newSeek)
{
	fscEnter();
	uint32 fileSize = fsc_getFileSize(fscFile);
	if (fsc_isWritable(fscFile) == false)
		newSeek = std::min(newSeek, fileSize);
	fscFile->fscSetSeek((uint64)newSeek);
	fscLeave();
}

// set file length
void fsc_setFileLength(FSCVirtualFile* fscFile, uint32 newEndOffset)
{
	fscEnter();
	uint32 fileSize = fsc_getFileSize(fscFile);
	if (!fsc_isWritable(fscFile))
	{
		cemuLog_log(LogType::Force, "TruncateFile called on read-only file");
	}
	else
	{
		fscFile->fscSetFileLength((uint64)newEndOffset);
	}
	fscLeave();
}

/*
 * Returns true if the file object is a directory
 */
bool fsc_isDirectory(FSCVirtualFile* fscFile)
{
	return fscFile->fscGetType() == FSC_TYPE_DIRECTORY;
}

/*
 * Returns true if the file object is a file
 */
bool fsc_isFile(FSCVirtualFile* fscFile)
{
	return fscFile->fscGetType() == FSC_TYPE_FILE;
}

/*
 * Returns true if the file is writable
 */
bool fsc_isWritable(FSCVirtualFile* fscFile)
{	
	return fscFile->fscQueryValueU64(FSC_QUERY_WRITEABLE) != 0;
}

/*
 * Read data from file
 * Returns number of bytes successfully read
 */
uint32 fsc_readFile(FSCVirtualFile* fscFile, void* buffer, uint32 size)
{
	fscEnter();
	uint32 fscStatus = fscFile->fscReadData(buffer, size);
	fscLeave();
	return fscStatus;
}

/*
 * Write data to file
 * Returns number of bytes successfully written
 */
uint32 fsc_writeFile(FSCVirtualFile* fscFile, void* buffer, uint32 size)
{
	fscEnter();
	if (fsc_isWritable(fscFile) == false)
	{
		fscLeave();
		return 0;
	}
	if (fscFile->m_isAppend)
		fsc_setFileSeek(fscFile, fsc_getFileSize(fscFile));

	uint32 fscStatus = fscFile->fscWriteData(buffer, size);
	fscLeave();
	return fscStatus;
}

// helper function to load a file into memory
uint8* fsc_extractFile(const char* path, uint32* fileSize, sint32 maxPriority)
{
	fscDeviceC* fscDevice = nullptr;
	sint32 fscStatus = FSC_STATUS_UNDEFINED;
	fscEnter();
	FSCVirtualFile* fscFile = fsc_open(path, FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus, maxPriority);
	if( !fscFile )
	{
		*fileSize = 0;
		fscLeave();
		return nullptr;
	}
	uint32 fscFileSize = fsc_getFileSize(fscFile);
	*fileSize = fscFileSize;
	uint8* fileMem = (uint8*)malloc(fscFileSize);
	if( fsc_readFile(fscFile, fileMem, fscFileSize) != fscFileSize )
	{
		free(fileMem);
		fsc_close(fscFile);
		*fileSize = 0;
		fscLeave();
		return nullptr;
	}
	fsc_close(fscFile);
	fscLeave();
	return fileMem;
}

std::optional<std::vector<uint8>> fsc_extractFile(const char* path, sint32 maxPriority)
{
	fscDeviceC* fscDevice = nullptr;
	sint32 fscStatus = FSC_STATUS_UNDEFINED;
	fscEnter();
	FSCVirtualFile* fscFile = fsc_open(path, FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus, maxPriority);
	if (!fscFile)
	{
		fscLeave();
		return std::nullopt;
	}
	std::vector<uint8> fileData;
	uint32 fscFileSize = fsc_getFileSize(fscFile);
	fileData.resize(fscFileSize);

	uint32 readOffset = 0;
	while (readOffset < fscFileSize)
	{
		uint32 stepReadSize = std::min(fscFileSize - readOffset, (uint32)1024 * 1024 * 32);
		uint32 numBytesRead = fsc_readFile(fscFile, fileData.data() + readOffset, stepReadSize);
		if (numBytesRead != stepReadSize)
		{
			fsc_close(fscFile);
			fscLeave();
			return std::nullopt;
		}
		readOffset += stepReadSize;
	}
	fsc_close(fscFile);
	fscLeave();
	return fileData;
}

// helper function to check if a file exists
bool fsc_doesFileExist(const char* path, sint32 maxPriority)
{
	fscDeviceC* fscDevice = nullptr;
	sint32 fscStatus = FSC_STATUS_UNDEFINED;
	fscEnter();
	FSCVirtualFile* fscFile = fsc_open(path, FSC_ACCESS_FLAG::OPEN_FILE, &fscStatus, maxPriority);
	if (!fscFile)
	{
		fscLeave();
		return false;
	}
	fsc_close(fscFile);
	fscLeave();
	return true;
}

// helper function to check if a directory exists
bool fsc_doesDirectoryExist(const char* path, sint32 maxPriority)
{
	fscDeviceC* fscDevice = nullptr;
	sint32 fscStatus = FSC_STATUS_UNDEFINED;
	fscEnter();
	FSCVirtualFile* fscFile = fsc_open(path, FSC_ACCESS_FLAG::OPEN_DIR, &fscStatus, maxPriority);
	if (!fscFile)
	{
		fscLeave();
		return false;
	}
	fsc_close(fscFile);
	fscLeave();
	return true;
}

// initialize Cemu's virtual filesystem
void fsc_init()
{
	fsc_reset();
}
