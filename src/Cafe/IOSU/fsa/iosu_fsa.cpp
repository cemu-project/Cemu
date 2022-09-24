#include "fsa_types.h"
#include "iosu_fsa.h"
#include "Cafe/IOSU/kernel/iosu_kernel.h"
#include "Cafe/Filesystem/fsc.h"
#include "util/helpers/helpers.h"

#include "Cafe/OS/libs/coreinit/coreinit_FS.h" // get rid of this dependency, requires reworking some of the IPC stuff. See locations where we use coreinit::FSCmdBlockBody_t
#include "Cafe/HW/Latte/Core/LatteBufferCache.h" // also remove this dependency

#include "Cafe/HW/MMU/MMU.h"

using namespace iosu::kernel;

namespace iosu
{
	namespace fsa
	{
		IOSMsgQueueId sFSAIoMsgQueue;
		SysAllocator<iosu::kernel::IOSMessage, 352> _m_sFSAIoMsgQueueMsgBuffer;
		std::thread sFSAIoThread;

		struct FSAClient // IOSU's counterpart to the coreinit FSClient struct 
		{
			std::string workingDirectory;
			bool isAllocated{ false };

			void AllocateAndInitialize()
			{
				isAllocated = true;
				workingDirectory = std::string("/");
			}

			void ReleaseAndCleanup()
			{
				isAllocated = false;
			}
		};

		std::array<FSAClient, 624> sFSAClientArray;

		IOS_ERROR FSAAllocateClient(sint32& indexOut)
		{
			for (size_t i = 0; i < sFSAClientArray.size(); i++)
			{
				if(sFSAClientArray[i].isAllocated)
					continue;
				sFSAClientArray[i].AllocateAndInitialize();
				indexOut = (sint32)i;
				return IOS_ERROR_OK;
			}
			return (IOS_ERROR)0xFFFCFFEE;
		}

		sint32 FSA_convertFSCtoFSStatus(sint32 fscError)
		{
			if (fscError == FSC_STATUS_OK)
				return (FSStatus)FS_RESULT::SUCCESS;
			else if (fscError == FSC_STATUS_FILE_NOT_FOUND)
				return (sint32)FS_RESULT::NOT_FOUND;
			else if (fscError == FSC_STATUS_ALREADY_EXISTS)
				return (sint32)FS_RESULT::ALREADY_EXISTS;
			cemu_assert_unimplemented();
			return -1;
		}

		std::string __FSATranslatePath(FSAClient* fsaClient, std::string_view input, bool endWithSlash = false)
		{
			std::string tmp;
			if (input.empty())
			{
				tmp.assign(fsaClient->workingDirectory);
				if (endWithSlash)
					tmp.push_back('/');
				return tmp;
			}
			char c = input.front();
			cemu_assert_debug(c != '\\'); // how to handle backward slashes?

			if (c == '/')
			{
				// absolute path
				tmp.assign("/"); // keep the leading slash
			}
			else if (c == '~')
			{
				cemu_assert_debug(false);
				input.remove_prefix(1);
				tmp.assign("/");
			}
			else
			{
				// in all other cases the path is relative to the working directory
				tmp.assign(fsaClient->workingDirectory);
			}
			// parse path
			size_t idx = 0;
			while (idx < input.size())
			{
				c = input[idx];
				if (c == '/')
				{
					idx++;
					continue; // filter leading and repeated slashes
				}

				if (c == '.')
				{
					if ((input.size() - idx) >= 3 && input[idx + 1] == '.' && input[idx + 2] == '/')
					{
						// "../"
						cemu_assert_unimplemented();
						idx += 3;
						continue;
					}
					else if ((input.size() - idx) == 2 && input[idx + 1] == '.')
					{
						// ".." at the end
						cemu_assert_unimplemented();
						idx += 2;
						continue;
					}
					else if ((input.size() - idx) >= 2 && input[idx + 1] == '/')
					{
						// "./"
						idx += 2;
						continue;
					}
				}
				if (!tmp.empty() && tmp.back() != '/')
					tmp.push_back('/');

				while (idx < input.size())
				{
					c = input[idx];
					if (c == '/')
						break;
					tmp.push_back(c);
					idx++;
				}
			}
			if (endWithSlash)
			{
				if (tmp.empty() || tmp.back() != '/')
					tmp.push_back('/');
			}
			return tmp;
		}

		FSCVirtualFile* __FSAOpenNode(FSAClient* client, std::string_view path, FSC_ACCESS_FLAG accessFlags, sint32& fscStatus)
		{
			std::string translatedPath = __FSATranslatePath(client, path);
			return fsc_open(translatedPath.c_str(), accessFlags, &fscStatus);
		}

		class _FSAHandleTable 
		{
			struct _FSAHandleResource
			{
				bool isAllocated{ false };
				FSCVirtualFile* fscFile;
				uint16 handleCheckValue;
			};
		public:
			FSA_RESULT AllocateHandle(FSResHandle& handleOut, FSCVirtualFile* fscFile)
			{
				for (size_t i = 0; i < m_handleTable.size(); i++)
				{
					auto& it = m_handleTable.at(i);
					if(it.isAllocated)
						continue;
					uint16 checkValue = (uint16)m_currentCounter;
					m_currentCounter++;
					it.handleCheckValue = checkValue;
					it.fscFile = fscFile;
					it.isAllocated = true;
					uint32 handleVal = ((uint32)i << 16) | (uint32)checkValue;
					handleOut = (FSResHandle)handleVal;
					return FSA_RESULT::SUCCESS;
				}
				cemuLog_log(LogType::Force, "FSA: Ran out of file handles");
				return FSA_RESULT::FATAL_ERROR;
			}

			FSA_RESULT ReleaseHandle(FSResHandle handle)
			{
				uint16 index = (uint16)((uint32)handle >> 16);
				uint16 checkValue = (uint16)(handle & 0xFFFF);
				if (index >= m_handleTable.size())
					return FSA_RESULT::INVALID_HANDLE_UKN38;
				auto& it = m_handleTable.at(index);
				if(!it.isAllocated)
					return FSA_RESULT::INVALID_HANDLE_UKN38;
				if(it.handleCheckValue != checkValue)
					return FSA_RESULT::INVALID_HANDLE_UKN38;
				it.fscFile = nullptr;
				it.isAllocated = false;
				return FSA_RESULT::SUCCESS;
			}

			FSCVirtualFile* GetByHandle(FSResHandle handle)
			{
				uint16 index = (uint16)((uint32)handle >> 16);
				uint16 checkValue = (uint16)(handle & 0xFFFF);
				if (index >= m_handleTable.size())
					return nullptr;
				auto& it = m_handleTable.at(index);
				if (!it.isAllocated)
					return nullptr;
				if (it.handleCheckValue != checkValue)
					return nullptr;
				return it.fscFile;
			}
		
		private:
			uint32 m_currentCounter = 1;
			std::array<_FSAHandleResource, 0x3C0> m_handleTable;
		};

		_FSAHandleTable sFileHandleTable;
		_FSAHandleTable sDirHandleTable;


		FSStatus __FSAOpenFile(FSAClient* client, const char* path, const char* accessModifierStr, sint32* fileHandle)
		{
			*fileHandle = FS_INVALID_HANDLE_VALUE;
			FSC_ACCESS_FLAG accessModifier = FSC_ACCESS_FLAG::NONE;
			bool truncateFile = false; // todo: Support for this
			bool isAppend = false; // todo: proper support for this (all write operations should move cursor to the end of the file?)
			if (strcmp(accessModifierStr, "r") == 0 || strcmp(accessModifierStr, "rb") == 0)
				accessModifier = FSC_ACCESS_FLAG::READ_PERMISSION;
			else if (strcmp(accessModifierStr, "r+") == 0)
			{
				// r+ will create a new file if it doesn't exist
				// the cursor will be set to the beginning of the file
				// allows read and write access
				accessModifier = FSC_ACCESS_FLAG::READ_PERMISSION | FSC_ACCESS_FLAG::WRITE_PERMISSION | FSC_ACCESS_FLAG::FILE_ALLOW_CREATE; // create if non exists, read, write
			}
			else if (strcmp(accessModifierStr, "w") == 0)
			{
				accessModifier = FSC_ACCESS_FLAG::READ_PERMISSION | FSC_ACCESS_FLAG::WRITE_PERMISSION | FSC_ACCESS_FLAG::FILE_ALWAYS_CREATE; // create new file & write
				truncateFile = true;
			}
			else if (strcmp(accessModifierStr, "w+") == 0)
			{
				accessModifier = FSC_ACCESS_FLAG::READ_PERMISSION | FSC_ACCESS_FLAG::WRITE_PERMISSION | FSC_ACCESS_FLAG::FILE_ALWAYS_CREATE; // create new file & read & write
				truncateFile = true;
			}
			else if (strcmp(accessModifierStr, "wb") == 0) // used in Super Meat Boy
			{
				// b flag is allowed has no effect
				accessModifier = FSC_ACCESS_FLAG::READ_PERMISSION | FSC_ACCESS_FLAG::WRITE_PERMISSION | FSC_ACCESS_FLAG::FILE_ALWAYS_CREATE; // create new file & write
				truncateFile = true;
			}
			else if (strcmp(accessModifierStr, "a+") == 0)
			{
				cemu_assert_debug(false); // a+ is kind of special. Writing always happens at the end but the read cursor can dynamically move
				// but Cafe OS might not support this. Needs investigation.
				// this also used to be FILE_ALWAYS_CREATE in 1.26.2 and before
				accessModifier = FSC_ACCESS_FLAG::READ_PERMISSION | FSC_ACCESS_FLAG::WRITE_PERMISSION | FSC_ACCESS_FLAG::FILE_ALLOW_CREATE;
				isAppend = true;
			}
			else
				cemu_assert_debug(false);

			accessModifier |= FSC_ACCESS_FLAG::OPEN_DIR | FSC_ACCESS_FLAG::OPEN_FILE;
			sint32 fscStatus;
			FSCVirtualFile* fscFile = __FSAOpenNode(client, path, accessModifier, fscStatus);
			if (!fscFile)
				return (sint32)FS_RESULT::NOT_FOUND;
			if (fscFile->fscGetType() != FSC_TYPE_FILE)
			{
				delete fscFile;
				return (sint32)FS_RESULT::NOT_FILE;
			}
			if (isAppend)
				fsc_setFileSeek(fscFile, fsc_getFileSize(fscFile));
			FSResHandle fsFileHandle;
			FSA_RESULT r = sFileHandleTable.AllocateHandle(fsFileHandle, fscFile);
			if (r != FSA_RESULT::SUCCESS)
			{
				cemuLog_log(LogType::Force, "Exceeded maximum number of FSA file handles");
				delete fscFile;
				return -0x400;
			}
			*fileHandle = fsFileHandle;
			cemuLog_log(LogType::File, "Open file {} (access: {} result: ok handle: 0x{})", path, accessModifierStr, (uint32)*fileHandle);
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus __FSAOpenDirectory(FSAClient* client, std::string_view path, sint32* dirHandle)
		{
			*dirHandle = FS_INVALID_HANDLE_VALUE;
			sint32 fscStatus;
			FSCVirtualFile* fscFile = __FSAOpenNode(client, path, FSC_ACCESS_FLAG::OPEN_DIR | FSC_ACCESS_FLAG::OPEN_FILE, fscStatus);
			if (!fscFile)
				return (FSStatus)FS_RESULT::NOT_FOUND;
			if (fscFile->fscGetType() != FSC_TYPE_DIRECTORY)
			{
				delete fscFile;
				return (FSStatus)(FS_RESULT::NOT_DIR);
			}
			FSResHandle fsDirHandle;
			FSA_RESULT r = sDirHandleTable.AllocateHandle(fsDirHandle, fscFile);
			if (r != FSA_RESULT::SUCCESS)
			{
				delete fscFile;
				return -0x400;
			}
			*dirHandle = fsDirHandle;
			cemuLog_log(LogType::File, "Open directory {} (result: ok handle: 0x{})", path, (uint32)*dirHandle);
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus __FSACloseFile(uint32 fileHandle)
		{
			uint8 handleType = 0;
			FSCVirtualFile* fscFile = sFileHandleTable.GetByHandle(fileHandle);
			if (!fscFile)
			{
				forceLogDebug_printf("__FSACloseFile(): Invalid handle (0x%08x)", fileHandle);
				return (FSStatus)FS_RESULT::ERR_PLACEHOLDER;
			}
			// unregister file
			sFileHandleTable.ReleaseHandle(fileHandle); // todo - use the error code of this
			fsc_close(fscFile);
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus FSAProcessCmd_remove(FSAClient* client, FSAIpcCommand* cmd)
		{
			char* path = (char*)cmd->cmdRemove.path;
			sint32 fscStatus = FSC_STATUS_FILE_NOT_FOUND;
			fsc_remove(path, &fscStatus);
			return FSA_convertFSCtoFSStatus(fscStatus);
		}

		FSStatus FSAProcessCmd_makeDir(FSAClient* client, FSAIpcCommand* cmd)
		{
			char* path = (char*)cmd->cmdMakeDir.path;
			sint32 fscStatus = FSC_STATUS_FILE_NOT_FOUND;
			fsc_createDir(path, &fscStatus);
			return FSA_convertFSCtoFSStatus(fscStatus);
		}

		FSStatus FSAProcessCmd_rename(FSAClient* client, FSAIpcCommand* cmd)
		{
			char* srcPath = (char*)cmd->cmdRename.srcPath;
			char* dstPath = (char*)cmd->cmdRename.dstPath;
			sint32 fscStatus = FSC_STATUS_FILE_NOT_FOUND;
			fsc_rename(srcPath, dstPath, &fscStatus);
			return FSA_convertFSCtoFSStatus(fscStatus);
		}

		bool __FSA_GetStatFromFSCFile(FSCVirtualFile* fscFile, FSStat_t* fsStatOut)
		{
			memset(fsStatOut, 0x00, sizeof(FSStat_t));
			FSFlag statFlag = FSFlag::NONE;
			if (fsc_isDirectory(fscFile))
			{
				// note: Only quota (save) directories have the size field set. For other directories it's zero.
				// Hyrule Warriors relies on the size field being zero for /vol/content/data/. Otherwise it will try to read it like a file and get stuck in an endless loop.
				// Art Academy reads the size for save directories
				statFlag |= FSFlag::IS_DIR;
				fsStatOut->size = 0;
			}
			else if (fsc_isFile(fscFile))
			{
				fsStatOut->size = fsc_getFileSize(fscFile);
			}
			else
			{
				cemu_assert_suspicious();
			}
			fsStatOut->permissions = 0x777; // format matches unix (fstat) permissions?
			fsStatOut->flag = statFlag;
			return true;
		}

		FSStatus __FSA_GetFileStat(FSAClient* client, const char* path, FSStat_t* fsStatOut)
		{
			sint32 fscStatus;
			FSCVirtualFile* fscFile = __FSAOpenNode(client, path, FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::OPEN_DIR, fscStatus);
			if (!fscFile)
				return FSA_convertFSCtoFSStatus(fscStatus);
			__FSA_GetStatFromFSCFile(fscFile, fsStatOut);
			delete fscFile;
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus FSAProcessCmd_queryInfo(FSAClient* client, FSAIpcCommand* cmd)
		{
			coreinit::FSCmdBlockBody_t* fullCmd = (coreinit::FSCmdBlockBody_t*)cmd;

			char* path = (char*)cmd->cmdQueryInfo.query;
			uint32 queryType = _swapEndianU32(cmd->cmdQueryInfo.queryType);
			void* queryResult = memory_getPointerFromVirtualOffset(_swapEndianU32(fullCmd->returnValueMPTR));
			// handle query
			sint32 fscStatus = FSC_STATUS_OK;
			if (queryType == FSA_QUERY_TYPE_STAT)
			{
				FSStat_t* fsStat = (FSStat_t*)queryResult;
				FSStatus fsStatus = __FSA_GetFileStat(client, path, fsStat);
				return fsStatus;
			}
			else if (queryType == FSA_QUERY_TYPE_FREESPACE)
			{
				sint32 fscStatus;
				FSCVirtualFile* fscFile = __FSAOpenNode(client, path, FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::OPEN_DIR, fscStatus);
				if (!fscFile)
					return FSA_convertFSCtoFSStatus(fscStatus);
				betype<uint64>* fsStatSize = (betype<uint64>*)queryResult;
				*fsStatSize = 30ull * 1024 * 1024 * 1024; // placeholder value. How is this determined?
				delete fscFile;
				return (FSStatus)FS_RESULT::SUCCESS;
			}
			else
				cemu_assert_unimplemented();
			return FSA_convertFSCtoFSStatus(fscStatus);
		}

		FSStatus FSAProcessCmd_getStatFile(FSAClient* client, FSAIpcCommand* cmd)
		{
			coreinit::FSCmdBlockBody_t* fullCmd = (coreinit::FSCmdBlockBody_t*)cmd;

			FSFileHandle2 fileHandle = cmd->cmdGetStatFile.fileHandle;
			FSStat_t* statOut = (FSStat_t*)memory_getPointerFromVirtualOffset(_swapEndianU32(fullCmd->returnValueMPTR));
			FSCVirtualFile* fscFile = sFileHandleTable.GetByHandle(fileHandle);
			if (!fscFile)
				return (FSStatus)FS_RESULT::NOT_FOUND;
			cemu_assert_debug(fsc_isFile(fscFile));
			__FSA_GetStatFromFSCFile(fscFile, statOut);
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus FSAProcessCmd_read(FSAClient* client, FSAIpcCommand* cmd)
		{
			coreinit::FSCmdBlockBody_t* fullCmd = (coreinit::FSCmdBlockBody_t*)cmd;

			uint32 filePos = _swapEndianU32(cmd->cmdDefault.transferFilePos);
			uint32 fileHandle = _swapEndianU32(cmd->cmdDefault.fileHandle);
			MPTR destOffset = _swapEndianU32(cmd->cmdDefault.destBufferMPTR);
			void* destPtr = memory_getPointerFromVirtualOffset(destOffset);
			uint32 transferSize = _swapEndianU32(fullCmd->transferSize);
			uint32 transferElementSize = _swapEndianU32(fullCmd->transferElemSize);
			uint32 flags = _swapEndianU32(cmd->cmdDefault.cmdFlag);
			uint32 errHandling = _swapEndianU32(fullCmd->errHandling);
			FSCVirtualFile* fscFile = sFileHandleTable.GetByHandle(fileHandle);
			if (!fscFile)
				return (FSStatus)FS_RESULT::ERR_PLACEHOLDER;
			uint32 elementSize = transferElementSize;
			uint32 elementCount = 0;
			if (transferElementSize != 0)
			{
				elementCount = transferSize / transferElementSize;
				cemu_assert_debug((transferSize % transferElementSize) == 0);
			}
			else
			{
				cemu_assert_debug(transferSize == 0);
			}
			uint32 bytesToRead = transferSize;
			// update file position if flag is set
			if ((flags & FSA_CMD_FLAG_SET_POS) != 0)
				fsc_setFileSeek(fscFile, filePos);
			// todo: File permissions
			uint32 bytesSuccessfullyRead = fsc_readFile(fscFile, destPtr, bytesToRead);
			if (transferElementSize == 0)
				return 0;

			LatteBufferCache_notifyDCFlush(memory_getVirtualOffsetFromPointer(destPtr), bytesToRead);

			return bytesSuccessfullyRead / transferElementSize; // return number of elements read
		}

		FSStatus FSAProcessCmd_write(FSAClient* client, FSAIpcCommand* cmd)
		{
			coreinit::FSCmdBlockBody_t* fullCmd = (coreinit::FSCmdBlockBody_t*)cmd;

			uint32 filePos = _swapEndianU32(cmd->cmdDefault.transferFilePos);
			uint32 fileHandle = _swapEndianU32(cmd->cmdDefault.fileHandle);
			MPTR destOffset = _swapEndianU32(cmd->cmdDefault.destBufferMPTR);
			void* destPtr = memory_getPointerFromVirtualOffset(destOffset);
			uint32 transferSize = _swapEndianU32(fullCmd->transferSize);
			uint32 transferElementSize = _swapEndianU32(fullCmd->transferElemSize);
			uint32 flags = _swapEndianU32(cmd->cmdDefault.cmdFlag);
			uint32 errHandling = _swapEndianU32(fullCmd->errHandling);
			FSCVirtualFile* fscFile = sFileHandleTable.GetByHandle(fileHandle);
			if (!fscFile)
				return (FSStatus)FS_RESULT::ERR_PLACEHOLDER;
			uint32 elementSize = transferElementSize;
			uint32 elementCount = transferSize / transferElementSize;
			cemu_assert_debug((transferSize % transferElementSize) == 0);
			uint32 bytesToWrite = transferSize;
			// check for write permission (should this happen before or after setting file position?)
			if (!fsc_isWritable(fscFile))
			{
				cemu_assert_debug(false);
				return (FSStatus)FS_RESULT::PERMISSION_ERROR;
			}
			// update file position if flag is set
			if ((flags & FSA_CMD_FLAG_SET_POS) != 0)
				fsc_setFileSeek(fscFile, filePos);
			uint32 bytesSuccessfullyWritten = fsc_writeFile(fscFile, destPtr, bytesToWrite);
			debug_printf("FSAProcessCmd_write(): Writing 0x%08x bytes (bytes actually written: 0x%08x)\n", bytesToWrite, bytesSuccessfullyWritten);
			return bytesSuccessfullyWritten / transferElementSize; // return number of elements read
		}

		FSStatus FSAProcessCmd_setPos(FSAClient* client, FSAIpcCommand* cmd)
		{
			uint32 fileHandle = _swapEndianU32(cmd->cmdDefault.destBufferMPTR);
			uint32 filePos = _swapEndianU32(cmd->cmdDefault.ukn0008);
			FSCVirtualFile* fscFile = sFileHandleTable.GetByHandle(fileHandle);
			if (!fscFile)
				return (FSStatus)FS_RESULT::ERR_PLACEHOLDER;
			fsc_setFileSeek(fscFile, filePos);
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus FSAProcessCmd_getPos(FSAClient* client, FSAIpcCommand* cmd)
		{
			coreinit::FSCmdBlockBody_t* fullCmd = (coreinit::FSCmdBlockBody_t*)cmd;

			uint32 fileHandle = _swapEndianU32(cmd->cmdDefault.destBufferMPTR);
			MPTR returnedFilePos = _swapEndianU32(fullCmd->returnValueMPTR);
			FSCVirtualFile* fscFile = sFileHandleTable.GetByHandle(fileHandle);
			if (!fscFile)
				return (FSStatus)FS_RESULT::ERR_PLACEHOLDER;
			uint32 filePos = fsc_getFileSeek(fscFile);
			memory_writeU32(returnedFilePos, filePos);
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus FSAProcessCmd_openFile(FSAClient* client, FSAIpcCommand* cmd)
		{
			coreinit::FSCmdBlockBody_t* fullCmd = (coreinit::FSCmdBlockBody_t*)cmd;
			sint32 fileHandle = 0;
			FSStatus fsStatus = __FSAOpenFile(client, (char*)cmd->cmdOpenFile.path, (char*)cmd->cmdOpenFile.mode, &fileHandle);
			memory_writeU32(_swapEndianU32(fullCmd->returnValueMPTR), fileHandle);
			cmd->cmdOpenFile.fileHandleOutput = fileHandle;
			return fsStatus;
		}

		FSStatus FSAProcessCmd_closeFile(FSAClient* client, FSAIpcCommand* cmd)
		{
			return __FSACloseFile(cmd->cmdCloseFile.fileHandle);
		}

		FSStatus FSAProcessCmd_openDir(FSAClient* client, FSAIpcCommand* cmd)
		{
			coreinit::FSCmdBlockBody_t* fullCmd = (coreinit::FSCmdBlockBody_t*)cmd;
			sint32 dirHandle = 0;
			FSStatus fsStatus = __FSAOpenDirectory(client, (const char*)cmd->cmdOpenFile.path, &dirHandle);
			memory_writeU32(_swapEndianU32(fullCmd->returnValueMPTR), dirHandle);
			cmd->cmdOpenDir.dirHandleOutput = dirHandle;
			return fsStatus;
		}

		FSStatus FSAProcessCmd_readDir(FSAClient* client, FSAIpcCommand* cmd)
		{
			coreinit::FSCmdBlockBody_t* fullCmd = (coreinit::FSCmdBlockBody_t*)cmd;

			FSCVirtualFile* fscFile = sDirHandleTable.GetByHandle((sint32)cmd->cmdReadDir.dirHandle);
			if (!fscFile)
				return (FSStatus)FS_RESULT::ERR_PLACEHOLDER;
			FSDirEntry_t* dirEntryOut = (FSDirEntry_t*)memory_getPointerFromVirtualOffset(_swapEndianU32(fullCmd->returnValueMPTR));
			FSCDirEntry fscDirEntry;
			if (fsc_nextDir(fscFile, &fscDirEntry) == false)
				return (FSStatus)FS_RESULT::END_ITERATION;
			strcpy(dirEntryOut->name, fscDirEntry.path);
			FSFlag statFlag = FSFlag::NONE;
			dirEntryOut->stat.size = 0;
			if (fscDirEntry.isDirectory)
			{
				statFlag |= FSFlag::IS_DIR;
			}
			else if (fscDirEntry.isFile)
			{
				dirEntryOut->stat.size = fscDirEntry.fileSize;
			}
			dirEntryOut->stat.flag = statFlag;
			dirEntryOut->stat.permissions = 0x777;
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus FSAProcessCmd_closeDir(FSAClient* client, FSAIpcCommand* cmd)
		{
			FSCVirtualFile* fscFile = sDirHandleTable.GetByHandle((sint32)cmd->cmdReadDir.dirHandle);
			if (!fscFile)
			{
				forceLogDebug_printf("CloseDir: Invalid handle (0x%08x)", (sint32)cmd->cmdReadDir.dirHandle);
				return (FSStatus)FS_RESULT::ERR_PLACEHOLDER;
			}
			sDirHandleTable.ReleaseHandle(cmd->cmdReadDir.dirHandle);
			fsc_close(fscFile);
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus FSAProcessCmd_flushQuota(FSAClient* client, FSAIpcCommand* cmd)
		{
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus FSAProcessCmd_appendFile(FSAClient* client, FSAIpcCommand* cmd)
		{
			uint32 fileHandle = _swapEndianU32(cmd->cmdDefault.destBufferMPTR);
			FSCVirtualFile* fscFile = sFileHandleTable.GetByHandle(fileHandle);
			if (!fscFile)
				return (FSStatus)FS_RESULT::ERR_PLACEHOLDER;
#ifdef CEMU_DEBUG_ASSERT
			cemuLog_force("FSAProcessCmd_appendFile(): size 0x{:08x} count 0x{:08x} (todo)\n", _swapEndianU32(cmd->cmdAppendFile.size), _swapEndianU32(cmd->cmdAppendFile.count));
#endif
			return _swapEndianU32(cmd->cmdAppendFile.size) * _swapEndianU32(cmd->cmdAppendFile.count);
		}

		FSStatus FSAProcessCmd_truncateFile(FSAClient* client, FSAIpcCommand* cmd)
		{
			FSFileHandle2 fileHandle = cmd->cmdTruncateFile.fileHandle;
			FSCVirtualFile* fscFile = sFileHandleTable.GetByHandle(fileHandle);
			if (!fscFile)
				return (FSStatus)FS_RESULT::ERR_PLACEHOLDER;
			fsc_setFileLength(fscFile, fsc_getFileSeek(fscFile));
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus FSAProcessCmd_isEof(FSAClient* client, FSAIpcCommand* cmd)
		{
			uint32 fileHandle = _swapEndianU32(cmd->cmdDefault.destBufferMPTR);
			FSCVirtualFile* fscFile = sFileHandleTable.GetByHandle(fileHandle);
			if (!fscFile)
				return (FSStatus)FS_RESULT::ERR_PLACEHOLDER;
			uint32 filePos = fsc_getFileSeek(fscFile);
			uint32 fileSize = fsc_getFileSize(fscFile);
			if (filePos >= fileSize)
				return (FSStatus)FS_RESULT::END_ITERATION;
			return (FSStatus)FS_RESULT::SUCCESS;
		}

		FSStatus FSAProcessCmd_getCwd(FSAClient* client, FSAIpcCommand* cmd)
		{
			coreinit::FSCmdBlockBody_t* fullCmd = (coreinit::FSCmdBlockBody_t*)cmd;

			char* pathOutput = (char*)memory_getPointerFromVirtualOffset(_swapEndianU32(fullCmd->returnValueMPTR));
			sint32 pathOutputMaxLen = _swapEndianU32(fullCmd->transferSize);
			cemu_assert(pathOutputMaxLen > 0);
			sint32 fscStatus = FSC_STATUS_OK;
			strncpy(pathOutput, client->workingDirectory.data(), std::min(client->workingDirectory.size() + 1, (size_t)pathOutputMaxLen));
			pathOutput[pathOutputMaxLen - 1] = '\0';
			return FSA_convertFSCtoFSStatus(fscStatus);
		}

		FSStatus FSAProcessCmd_changeDir(FSAClient* client, FSAIpcCommand* cmd)
		{
			const char* path = (const char*)cmd->cmdChangeDir.path;
			cmd->cmdChangeDir.path[sizeof(cmd->cmdChangeDir.path) - 1] = '\0';
			sint32 fscStatus = FSC_STATUS_OK;
			client->workingDirectory.assign(__FSATranslatePath(client, path, true));
			return FSA_convertFSCtoFSStatus(fscStatus);
		}

		void FSAHandleCommandIoctl(FSAClient* client, IPCCommandBody* cmd, uint32 operationId, void* ptrIn, void* ptrOut)
		{
			FSAIpcCommand* fsaCommand = (FSAIpcCommand*)ptrIn;
			FSStatus fsStatus = (FSStatus)(FS_RESULT::FATAL_ERROR);
			if (operationId == FSA_CMD_OPERATION_TYPE_REMOVE)
			{
				fsStatus = FSAProcessCmd_remove(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_MAKEDIR)
			{
				fsStatus = FSAProcessCmd_makeDir(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_RENAME)
			{
				fsStatus = FSAProcessCmd_rename(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_READ)
			{
				fsStatus = FSAProcessCmd_read(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_WRITE)
			{
				fsStatus = FSAProcessCmd_write(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_SETPOS)
			{
				fsStatus = FSAProcessCmd_setPos(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_GETPOS)
			{
				fsStatus = FSAProcessCmd_getPos(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_OPENFILE)
			{
				fsStatus = FSAProcessCmd_openFile(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_CLOSEFILE)
			{
				fsStatus = FSAProcessCmd_closeFile(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_APPENDFILE)
			{
				fsStatus = FSAProcessCmd_appendFile(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_TRUNCATEFILE)
			{
				fsStatus = FSAProcessCmd_truncateFile(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_ISEOF)
			{
				fsStatus = FSAProcessCmd_isEof(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_QUERYINFO)
			{
				fsStatus = FSAProcessCmd_queryInfo(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_GETSTATFILE)
			{
				fsStatus = FSAProcessCmd_getStatFile(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_GETCWD)
			{
				fsStatus = FSAProcessCmd_getCwd(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_CHANGEDIR)
			{
				fsStatus = FSAProcessCmd_changeDir(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_OPENDIR)
			{
				fsStatus = FSAProcessCmd_openDir(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_READDIR)
			{
				fsStatus = FSAProcessCmd_readDir(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_CLOSEDIR)
			{
				fsStatus = FSAProcessCmd_closeDir(client, fsaCommand);
			}
			else if (operationId == FSA_CMD_OPERATION_TYPE_FLUSHQUOTA)
			{
				fsStatus = FSAProcessCmd_flushQuota(client, fsaCommand);
			}
			else
			{
				cemu_assert_unimplemented();
			}
			IOS_ResourceReply(cmd, (IOS_ERROR)fsStatus);
		}

		void FSAIoThread()
		{
			SetThreadName("IOSU-FSA");
			IOSMessage msg;
			while (true)
			{
				IOS_ERROR r = IOS_ReceiveMessage(sFSAIoMsgQueue, &msg, 0);
				cemu_assert(!IOS_ResultIsError(r));
				if (msg == 0)
					return; // shutdown signaled
				IPCCommandBody* cmd = MEMPTR<IPCCommandBody>(msg).GetPtr();
				uint32 clientHandle = (uint32)cmd->devHandle;
				if (cmd->cmdId == IPCCommandId::IOS_OPEN)
				{
					sint32 clientIndex = 0;
					r = FSAAllocateClient(clientIndex);
					if (r != IOS_ERROR_OK)
					{
						IOS_ResourceReply(cmd, r);
						continue;
					}
					IOS_ResourceReply(cmd, (IOS_ERROR)clientIndex);
					continue;
				}
				else if (cmd->cmdId == IPCCommandId::IOS_CLOSE)
				{
					cemu_assert(clientHandle < sFSAClientArray.size());
					sFSAClientArray[clientHandle].ReleaseAndCleanup();
					IOS_ResourceReply(cmd, IOS_ERROR_OK);
					continue;
				}
				else if (cmd->cmdId == IPCCommandId::IOS_IOCTL)
				{
					cemu_assert(clientHandle < sFSAClientArray.size());
					cemu_assert(sFSAClientArray[clientHandle].isAllocated);
					FSAHandleCommandIoctl(sFSAClientArray.data() + clientHandle, cmd, cmd->args[0], MEMPTR<void>(cmd->args[1]), MEMPTR<void>(cmd->args[3]));
				}
				else if (cmd->cmdId == IPCCommandId::IOS_IOCTLV)
				{
					cemu_assert_unimplemented();
					//uint32 requestId = cmd->args[0];
					//uint32 numIn = cmd->args[1];
					//uint32 numOut = cmd->args[2];
					//IPCIoctlVector* vec = MEMPTR<IPCIoctlVector>{ cmd->args[3] }.GetPtr();
					IOS_ResourceReply(cmd, IOS_ERROR_INVALID);
					continue;
				}
				else
				{
					cemuLog_log(LogType::Force, "/dev/fsa: Unsupported IPC cmdId");
					cemu_assert_suspicious();
					IOS_ResourceReply(cmd, IOS_ERROR_INVALID);
				}
			}
		}

		void Initialize()
		{
			for (auto& it : sFSAClientArray)
				it.ReleaseAndCleanup();
			sFSAIoMsgQueue = (IOSMsgQueueId)IOS_CreateMessageQueue(_m_sFSAIoMsgQueueMsgBuffer.GetPtr(), _m_sFSAIoMsgQueueMsgBuffer.GetCount());
			IOS_ERROR r = IOS_RegisterResourceManager("/dev/fsa", sFSAIoMsgQueue);
			IOS_DeviceAssociateId("/dev/fsa", 11);
			cemu_assert(!IOS_ResultIsError(r));
			sFSAIoThread = std::thread(FSAIoThread);
		}

		void Shutdown()
		{
			IOS_SendMessage(sFSAIoMsgQueue, 0, 0);
			sFSAIoThread.join();
		}
	}
}
