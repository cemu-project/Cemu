#include "Cafe/OS/libs/coreinit/coreinit_IOS.h"
#include "Cafe/IOSU/legacy/iosu_ioctl.h"

// superseded by coreinit_IPC.cpp/h

sint32 __depr__IOS_Open(char* path, uint32 mode)
{
	sint32 iosDevice = 0;
	if (path == nullptr)
	{
		iosDevice = 0;
	}
	else
	{
		if (strcmp(path, IOS_PATH_ODM) == 0)
			iosDevice = IOS_DEVICE_ODM;
		else if (strcmp(path, IOS_PATH_SOCKET) == 0)
			iosDevice = IOS_DEVICE_SOCKET;
		else if (strcmp(path, IOS_PATH_ACT) == 0)
			iosDevice = IOS_DEVICE_ACT;
		else if (strcmp(path, IOS_PATH_FPD) == 0)
			iosDevice = IOS_DEVICE_FPD;
		else if (strcmp(path, IOS_PATH_ACP_MAIN) == 0)
			iosDevice = IOS_DEVICE_ACP_MAIN;
		else if (strcmp(path, IOS_PATH_MCP) == 0)
			iosDevice = IOS_DEVICE_MCP;
		else if (strcmp(path, IOS_PATH_BOSS) == 0)
			iosDevice = IOS_DEVICE_BOSS;
		else if (strcmp(path, IOS_PATH_NIM) == 0)
			iosDevice = IOS_DEVICE_NIM;
		else if (strcmp(path, IOS_PATH_IOSUHAX) == 0)
			return -1;
		else
			iosDevice = IOS_DEVICE_UKN;
	}
	return iosDevice;
}

sint32 __depr__IOS_Ioctl(uint32 fd, uint32 request, void* inBuffer, uint32 inSize, void* outBuffer, uint32 outSize)
{
	switch (fd)
	{
		case IOS_DEVICE_ODM:
		{
			// Home Menu uses ioctl cmd 5 on startup and then repeats cmd 4 every frame
			if (request == 4)
			{
				// check drive state
				debug_printf("checkDriveState()\n");
				*(uint32be*)outBuffer = 0xA;
			}
			else
			{
				debug_printf("odm unsupported ioctl %d\n", request);
			}
			break;
		}
		default:
		{
			// todo
			cemuLog_logDebug(LogType::Force, "Unsupported Ioctl command");
		}
	}
	return 0;
}

sint32 __depr__IOS_Ioctlv(uint32 fd, uint32 request, uint32 countIn, uint32 countOut, ioBufferVector_t* ioBufferVectors)
{
	StackAllocator<ioQueueEntry_t> _queueEntryBuf;
	ioQueueEntry_t* queueEntry = _queueEntryBuf.GetPointer();

	queueEntry->isIoctlv = true;
	queueEntry->isAsync = false;
	queueEntry->request = request;
	queueEntry->countIn = countIn;
	queueEntry->countOut = countOut;
	queueEntry->bufferVectors = ioBufferVectors;

	queueEntry->ppcThread = nullptr;
	queueEntry->returnValue = 0;
	queueEntry->isCompleted = false;

	sint32 r = iosuIoctl_pushAndWait(fd, queueEntry);

	return r;
}

sint32 __depr__IOS_Close(uint32 fd)
{
	return 0;
}
