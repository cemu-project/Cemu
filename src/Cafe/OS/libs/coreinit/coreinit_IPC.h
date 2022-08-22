#include "Cafe/IOSU/iosu_ipc_common.h"
#include "Cafe/IOSU/iosu_types_common.h"

namespace coreinit
{
	void IPCDriver_NotifyResponses(uint32 ppcCoreIndex, IPCCommandBody** responseArray, uint32 numResponses);

	IOSDevHandle IOS_Open(const char* devicePath, uint32 flags);
	IOS_ERROR IOS_Close(IOSDevHandle devHandle);
	IOS_ERROR IOS_Ioctl(IOSDevHandle devHandle, uint32 requestId, void* ptrIn, uint32 sizeIn, void* ptrOut, uint32 sizeOut);
	IOS_ERROR IOS_IoctlAsync(IOSDevHandle devHandle, uint32 requestId, void* ptrIn, uint32 sizeIn, void* ptrOut, uint32 sizeOut, MEMPTR<void> asyncResultFunc, MEMPTR<void> asyncResultUserParam);
	IOS_ERROR IOS_Ioctlv(IOSDevHandle devHandle, uint32 requestId, uint32 numIn, uint32 numOut, IPCIoctlVector* vec);
	IOS_ERROR IOS_IoctlvAsync(IOSDevHandle devHandle, uint32 requestId, uint32 numIn, uint32 numOut, IPCIoctlVector* vec, MEMPTR<void> asyncResultFunc, MEMPTR<void> asyncResultUserParam);

	void InitializeIPC();
};