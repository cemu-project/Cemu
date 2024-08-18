#pragma once
#include <IOSU/iosu_ipc_common.h>
#include "fsa_types.h"

namespace iosu
{
	namespace fsa
	{

		struct FSARequest
		{
			uint32be ukn0;
			union
			{
				uint8 ukn04[0x51C];
				struct
				{
					MEMPTR<void> dest;
					uint32be size;
					uint32be count;
					uint32be filePos;
					uint32be fileHandle;
					uint32be flag;
				} cmdReadFile;
				struct
				{
					MEMPTR<void> dest;
					uint32be size;
					uint32be count;
					uint32be filePos;
					uint32be fileHandle;
					uint32be flag;
				} cmdWriteFile;
				struct
				{
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
					uint8 mode[12];		   // +0x284 note: code seems to access this value like it has a size of 0x10 but the actual struct element is only 12 bytes? Maybe a typo (10 instead of 0x10 in the struct def)
					uint32be createMode;   // +0x290
					uint32be openFlags;	   // +0x294
					uint32be preallocSize; // +0x298
				} cmdOpenFile;
				struct
				{
					uint32be fileHandle;
				} cmdCloseFile;
				struct
				{
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
				} cmdRemove;
				struct
				{
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
					uint8 ukn0284[12]; // +0x284
				} cmdOpenDir;
				struct
				{
					betype<uint32> dirHandle;
				} cmdReadDir;
				struct
				{
					betype<uint32> dirHandle;
				} cmdCloseDir;
				struct
				{
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
					uint32be uknParam;
				} cmdMakeDir;
				struct
				{
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
				} cmdChangeDir;
				struct
				{
					uint8 query[FSA_CMD_PATH_MAX_LENGTH];
					uint32be queryType;
				} cmdQueryInfo;
				struct
				{
					uint8 srcPath[FSA_CMD_PATH_MAX_LENGTH];
					uint8 dstPath[FSA_CMD_PATH_MAX_LENGTH];
				} cmdRename;
				struct
				{
					uint32be size;
					uint32be count;
					uint32be fileHandle;
					uint32be uknParam;
				} cmdAppendFile;
				struct
				{
					uint32be fileHandle;
				} cmdTruncateFile;
				struct
				{
					uint32be fileHandle;
				} cmdGetStatFile;
				struct
				{
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
				} cmdFlushQuota;
				struct
				{
					uint32be fileHandle;
					uint32be filePos;
				} cmdSetPosFile;
				struct
				{
					uint32be fileHandle;
				} cmdGetPosFile;
				struct
				{
					uint32be fileHandle;
				} cmdIsEof;
				struct
				{
					uint32be dirHandle;
				} cmdRewindDir;
				struct
				{
					uint32be fileHandle;
				} cmdFlushFile;
				struct
				{
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
					uint32be mode1;
					uint32be mode2;
				} cmdChangeMode;
			};
		};
		static_assert(sizeof(FSARequest) == 0x520);

#pragma pack(1)
		struct FSAResponse
		{
			uint32be ukn0;
			union
			{
				uint8 ukn04[0x28F];
				struct
				{
					uint32be fileHandleOutput; // +0x584 used to return file handle on success
				} cmdOpenFile;
				struct
				{
					uint32be dirHandleOutput; // +0x584 used to return dir handle on success
				} cmdOpenDir;
				struct
				{
					uint32be filePos;
				} cmdGetPosFile;
				struct
				{
					FSStat_t statOut;
				} cmdStatFile;
				struct
				{
					FSDirEntry_t dirEntry;
				} cmdReadDir;
				struct
				{
					char path[FSA_CMD_PATH_MAX_LENGTH];
				} cmdGetCWD;
				struct
				{
					union
					{
						uint8 ukn04[0x64];
						struct
						{
							uint64be freespace;
						} queryFreeSpace;
						struct
						{
							FSStat_t stat;
						} queryStat;
						struct
						{
							FSADeviceInfo_t info;
						} queryDeviceInfo;
					};
				} cmdQueryInfo;
			};
		};
		static_assert(sizeof(FSAResponse) == 0x293);
#pragma pack()

		struct FSAShimBuffer
		{
			FSARequest request;
			uint8 ukn0520[0x60];
			FSAResponse response;
			uint8 ukn0813[0x6D];
			IPCIoctlVector ioctlvVec[3];
			uint8 ukn08A4[0x5C];
			/* +0x0900 */ uint32be operationType;
			betype<IOSDevHandle> fsaDevHandle;
			/* +0x0908 */ uint16be ipcReqType; // 0 -> IoctlAsync, 1 -> IoctlvAsync
			uint8 ioctlvVecIn;
			uint8 ioctlvVecOut;
			uint32 ukn090C;
			uint32 ukn0910;
			uint32 ukn0914;
			uint32 ukn0918;
			uint32 ukn091C;
			uint32 ukn0920;
			uint32 ukn0924;
			uint32 ukn0928;
			uint32 ukn092C;
			uint32 ukn0930;
			uint32 ukn0934;
		};
		static_assert(sizeof(FSAShimBuffer) == 0x938); // exact size of this is not known

		void Initialize();
		void Shutdown();
	} // namespace fsa
} // namespace iosu
