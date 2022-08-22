#pragma once
#include "fsa_types.h"

namespace iosu
{
	namespace fsa
	{
		struct FSAIpcCommand
		{
			union
			{
				struct
				{
					uint32 ukn0000;
					uint32 destBufferMPTR; // used as fileHandle for FSSetFilePosAsync
					uint32 ukn0008; // used as filePos for FSSetFilePosAsync
					uint32 ukn000C;
					uint32 transferFilePos; // used as filePos for read/write operation
					uint32 fileHandle;
					uint32 cmdFlag;
					uint32 ukn001C;
					uint8 ukn0020[0x10];
					uint8 ukn0030[0x10];
					uint8 ukn0040[0x10];
					uint8 ukn0050[0x10];
					uint8 ukn0060[0x10];
					uint8 ukn0070[0x10];
					uint8 ukn0080[0x10];
					uint8 ukn0090[0x10];
					uint8 ukn00A0[0x10];
					uint8 ukn00B0[0x10];
					uint8 ukn00C0[0x10];
					uint8 ukn00D0[0x10];
					uint8 ukn00E0[0x10];
					uint8 ukn00F0[0x10];
					uint8 ukn0100[0x100];
					uint8 ukn0200[0x100];
					uint8 ukn0300[0x100];
					uint8 ukn0400[0x100];
					uint8 ukn0500[0x100];
					uint8 ukn0600[0x100];
				}cmdDefault;
				struct
				{
					uint32 ukn0000;
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
					uint8 mode[12]; // +0x284 note: code seems to access this value like it has a size of 0x10 but the actual struct element is only 12 bytes? Maybe a typo (10 instead of 0x10 in the struct def)
					uint32 createMode; // +0x290
					uint32 openFlags; // +0x294
					uint32 preallocSize; // +0x298
					uint8 ukn[0x2E8]; // +0x29C
					// output
					uint32be fileHandleOutput; // +0x584 used to return file handle on success
				}cmdOpenFile;
				struct
				{
					uint32 ukn0000; // +0x000
					uint32be fileHandle; // +0x004
				}cmdCloseFile;
				struct
				{
					uint32 ukn0000;
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
					uint32 ukn0284;
					uint8 ukn0288[0x80 - 8];
					uint8 ukn0300[0x100];
					uint8 ukn0400[0x100];
					uint32 ukn0500;
				}cmdRemove;
				struct
				{
					uint32 ukn0000;
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
					uint8 ukn0284[12]; // +0x284
					uint32 ukn0290; // +0x290
					uint32 ukn0294; // +0x294
					uint32 ukn0298; // +0x298
					uint8 ukn[0x2E8]; // +0x29C
					// output
					uint32be dirHandleOutput; // +0x584 used to return dir handle on success
				}cmdOpenDir;
				struct
				{
					uint32 ukn0000;
					betype<uint32> dirHandle;
				}cmdReadDir;
				struct
				{
					uint32 ukn0000;
					betype<uint32> dirHandle;
				}cmdCloseDir;
				struct
				{
					uint32 ukn0000;
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
					uint32 uknParam;
					uint8 ukn0288[0x80 - 8];
					uint8 ukn0300[0x100];
					uint8 ukn0400[0x100];
					uint32 ukn0500;
				}cmdMakeDir;
				struct
				{
					uint32 ukn0000;
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
					uint8 ukn0284[0x80 - 4];
					uint8 ukn0300[0x100];
					uint8 ukn0400[0x100];
					uint32 ukn0500;
				}cmdChangeDir;
				struct
				{
					uint32 ukn0000;
					uint8 query[FSA_CMD_PATH_MAX_LENGTH];
					uint32 queryType;
					uint8 ukn0288[0x80 - 8];
					uint8 ukn0300[0x100];
					uint8 ukn0400[0x100];
					uint32 ukn0500;
				}cmdQueryInfo;
				struct
				{
					uint32 ukn0000;
					uint8 srcPath[FSA_CMD_PATH_MAX_LENGTH];
					uint8 dstPath[FSA_CMD_PATH_MAX_LENGTH];
				}cmdRename;
				struct
				{
					uint32 ukn0000;
					uint32 size;
					uint32 count;
					uint32 fileHandle;
					uint32 uknParam;
				}cmdAppendFile;
				struct
				{
					uint32 ukn0000;
					betype<uint32> fileHandle;
					uint32be ukn0008;
				}cmdTruncateFile;
				struct
				{
					uint32 ukn0000;
					betype<uint32> fileHandle;
				}cmdGetStatFile;
				struct
				{
					uint32 ukn0000;
					uint8 path[FSA_CMD_PATH_MAX_LENGTH];
				}cmdFlushQuota;
			};
			uint8 ukn0700[0x100];
			uint8 ukn0800[0x10];
			uint8 ukn0810[0x10];
		};

		static_assert(sizeof(FSAIpcCommand) == 0x820); // exact size of this is not known

		void Initialize();
		void Shutdown();
	}
}
