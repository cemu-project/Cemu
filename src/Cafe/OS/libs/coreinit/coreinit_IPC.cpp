#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/IOSU/kernel/iosu_kernel.h"
#include "Cafe/HW/Espresso/Const.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "coreinit_MessageQueue.h"
#include "coreinit_IPC.h"

namespace coreinit
{
	static constexpr inline size_t IPC_NUM_RESOURCE_BUFFERS = 0x30;
	
	struct IPCResourceBuffer
	{
		IPCCommandBody commandBody;
		uint8 bufferData[0x80 - 0x48];
	};
	
	static_assert(sizeof(IPCCommandBody) == 0x48);
	static_assert(sizeof(IPCResourceBuffer) == 0x80);

	struct IPCResourceBufferDescriptor
	{
		/* +0x00 */ uint32be IsAllocated;
		/* +0x04 */ MEMPTR<OSMessageQueue> asyncMsgQueue; // optional, if set a message will be sent to this queue...
		/* +0x08 */ MEMPTR<void> asyncResultFunc; // ...otherwise this is checked and delegated to the IPC threads. If false, only eventSynchronousIPC will be signaled. If true, a message will be sent to the per-core ipc queue
		/* +0x0C */ MEMPTR<void> asyncResultUserParam;
		/* +0x10 */ uint32 ukn10;
		/* +0x14 */ MEMPTR<IPCResourceBuffer> resourcePtr;
		/* +0x18 */ OSEvent eventSynchronousIPC;
	};

	static_assert(sizeof(IPCResourceBufferDescriptor) == 0x3C);

	struct IPCBufferFIFO
	{
		sint32be writeIndex;
		sint32be readIndex;
		sint32be numQueuedEntries;
		sint32be mostQueuedEntries;
		MEMPTR<IPCResourceBufferDescriptor> ringbufferArray[IPC_NUM_RESOURCE_BUFFERS];

		void Init()
		{
			writeIndex = 0;
			readIndex = -1;
			numQueuedEntries = 0;
			mostQueuedEntries = 0;
			for (size_t i = 0; i < IPC_NUM_RESOURCE_BUFFERS; i++)
				ringbufferArray[i] = nullptr;
		}

		void Push(IPCResourceBufferDescriptor* descriptor)
		{
			cemu_assert(readIndex != writeIndex); // if equal, fifo is full (should not happen as there not more buffers than ringbuffer entries)
			ringbufferArray[writeIndex] = descriptor;
			if (readIndex < 0)
				readIndex = writeIndex;
			writeIndex = (writeIndex + 1) % IPC_NUM_RESOURCE_BUFFERS;
			++numQueuedEntries;
			if (numQueuedEntries > mostQueuedEntries)
				mostQueuedEntries = numQueuedEntries;
		}

		IPCResourceBufferDescriptor* Pop()
		{
			if (numQueuedEntries == 0)
				return nullptr;
			IPCResourceBufferDescriptor* r = ringbufferArray[readIndex].GetPtr();
			--numQueuedEntries;
			if (numQueuedEntries == 0)
				readIndex = -1;
			else
				readIndex = (readIndex + 1) % IPC_NUM_RESOURCE_BUFFERS;
			return r;
		}
	};

	struct IPCDriverCOSKernelCommunicationArea
	{
		uint32be numAvailableResponses;
		MEMPTR<IPCCommandBody> responseArray[11];
	};

	static_assert(sizeof(IPCDriverCOSKernelCommunicationArea) == 0x30);

	struct alignas(64) IPCDriver
	{
		betype<IPCDriverState> state;
		uint32 ukn04;
		uint32 coreIndex;
		uint32 writeIndexCmd020;
		MEMPTR<IPCResourceBuffer> resourceBuffers;

		/* 0x16C */ IPCBufferFIFO fifoFreeBuffers;
		/* 0x23C */ IPCBufferFIFO fifoBuffersInFlight;

		/* 0x334 */ uint32 resourceBuffersInitialized;
		/* 0x338 */ IPCDriverCOSKernelCommunicationArea kernelSharedArea; // this is passed to system call 0x1E00 (IPCOpen)

		/* 0x3FC */ IPCResourceBufferDescriptor resBufferDescriptor[IPC_NUM_RESOURCE_BUFFERS];
	};

	//static_assert(sizeof(IPCDriverInstance) == 0x1740);

	SysAllocator<IPCResourceBuffer, IPC_NUM_RESOURCE_BUFFERS * Espresso::CORE_COUNT, 0x40> s_ipcResourceBuffers;
	SysAllocator<IPCDriver, Espresso::CORE_COUNT, 0x40> s_ipcDriver;

	IPCDriver& IPCDriver_GetByCore(uint32 coreIndex)
	{
		cemu_assert_debug(coreIndex >= 0 && coreIndex < (uint32)Espresso::CORE_COUNT);
		return s_ipcDriver[coreIndex];
	}

	void IPCDriver_InitForCore(uint32 coreIndex)
	{
		IPCDriver& ipcDriver = IPCDriver_GetByCore(coreIndex);
		ipcDriver.coreIndex = coreIndex;
		ipcDriver.state = IPCDriverState::INITIALIZED;
		ipcDriver.resourceBuffers = s_ipcResourceBuffers.GetPtr() + IPC_NUM_RESOURCE_BUFFERS * coreIndex;
		ipcDriver.resourceBuffersInitialized = 0;
		// setup resource descriptors
		for (size_t i = 0; i < IPC_NUM_RESOURCE_BUFFERS; i++)
		{
			ipcDriver.resBufferDescriptor[i].resourcePtr = ipcDriver.resourceBuffers.GetPtr() + i;
			ipcDriver.resBufferDescriptor[i].asyncResultFunc = nullptr;
			ipcDriver.resBufferDescriptor[i].asyncResultUserParam = nullptr;
		}
		ipcDriver.resourceBuffersInitialized = 1;
		// setup resource buffer FIFOs
		ipcDriver.fifoFreeBuffers.Init();
		ipcDriver.fifoBuffersInFlight.Init();
		for (size_t i = 0; i < IPC_NUM_RESOURCE_BUFFERS; i++)
			ipcDriver.fifoFreeBuffers.Push(ipcDriver.resBufferDescriptor + i);
	}

	IPCResourceBufferDescriptor* IPCDriver_AllocateResource(IPCDriver* ipcDriver, IOSDevHandle devHandle, IPCCommandId cmdId, OSMessageQueue* asyncMessageQueue, MEMPTR<void> asyncResultFunc, MEMPTR<void> asyncResultUserParam)
	{
		cemu_assert_debug(ipcDriver->coreIndex == OSGetCoreId());
		IPCResourceBufferDescriptor* descriptor = nullptr;
		while (true) 
		{
			descriptor = ipcDriver->fifoFreeBuffers.Pop();
			if (!descriptor)
			{
				cemuLog_log(LogType::Force, "IPCDriver: Exceeded free resources");
				OSYieldThread();
				cemu_assert_unimplemented(); // we should wait for an event instead of busylooping
			}
			else
				break;
		}
		cemu_assert_debug(descriptor >= ipcDriver->resBufferDescriptor && descriptor < ipcDriver->resBufferDescriptor + IPC_NUM_RESOURCE_BUFFERS);
		cemu_assert_debug(descriptor->resourcePtr.GetPtr() >= ipcDriver->resourceBuffers.GetPtr() && descriptor->resourcePtr.GetPtr() < (ipcDriver->resourceBuffers.GetPtr() + IPC_NUM_RESOURCE_BUFFERS));
		IPCResourceBuffer* res = descriptor->resourcePtr;
		IPCCommandBody& cmdBody = res->commandBody;

		descriptor->IsAllocated = 1;
		descriptor->asyncMsgQueue = asyncMessageQueue;
		descriptor->asyncResultFunc = asyncResultFunc;
		descriptor->asyncResultUserParam = asyncResultUserParam;

		cmdBody.cmdId = cmdId;
		cmdBody.ukn0C = 0;
		cmdBody.ukn14 = 0;
		cmdBody.result = 0;
		cmdBody.devHandle = devHandle;

		return descriptor;
	}

	void IPCDriver_ReleaseResource(IPCDriver* ipcDriver, IPCResourceBufferDescriptor* requestDescriptor)
	{
		requestDescriptor->IsAllocated = 0;
		ipcDriver->fifoFreeBuffers.Push(requestDescriptor);
	}

	/* IPC threads */

	SysAllocator<OSThread_t, Espresso::CORE_COUNT> gIPCThread;
	SysAllocator<uint8, 0x4000 * Espresso::CORE_COUNT> _gIPCThreadStack;
	SysAllocator<uint8, 0x18 * Espresso::CORE_COUNT> _gIPCThreadNameStorage;
	SysAllocator<OSMessageQueue, Espresso::CORE_COUNT> gIPCThreadMsgQueue;
	SysAllocator<OSMessage, Espresso::CORE_COUNT * IPC_NUM_RESOURCE_BUFFERS> _gIPCThreadSemaphoreStorage;

	// handler thread for asynchronous callbacks for IPC responses
	void __IPCDriverThreadFunc(PPCInterpreter_t* hCPU)
	{
		uint32 coreIndex = OSGetCoreId();
		while (true)
		{
			OSMessage msg;
			OSReceiveMessage(gIPCThreadMsgQueue.GetPtr() + coreIndex, &msg, OS_MESSAGE_BLOCK);
			cemu_assert(msg.data2 == 1); // type must be callback
			MEMPTR<void> cbFunc{ msg.message };
			cemu_assert(cbFunc != nullptr);
			PPCCoreCallback(cbFunc.GetPtr(), (uint32)msg.data0, (uint32)msg.data1);
		}
		osLib_returnFromFunction(hCPU, 0);
	}

	void IPCDriver_InitIPCThread(uint32 coreIndex)
	{
		// create a thread with 0x4000 stack space
		// and a message queue large enough to hold the maximum number of commands (IPC_NUM_RESOURCE_BUFFERS)
		OSInitMessageQueue(gIPCThreadMsgQueue.GetPtr() + coreIndex, _gIPCThreadSemaphoreStorage.GetPtr() + coreIndex * IPC_NUM_RESOURCE_BUFFERS, IPC_NUM_RESOURCE_BUFFERS);
		OSThread_t* ipcThread = gIPCThread.GetPtr() + coreIndex;
		__OSCreateThreadType(ipcThread, PPCInterpreter_makeCallableExportDepr(__IPCDriverThreadFunc), 0, nullptr, _gIPCThreadStack.GetPtr() + 0x4000 * coreIndex + 0x4000, 0x4000, 15, (1 << coreIndex), OSThread_t::THREAD_TYPE::TYPE_DRIVER);
		sprintf((char*)_gIPCThreadNameStorage.GetPtr()+coreIndex*0x18, "{SYS IPC Core %d}", coreIndex);
		OSSetThreadName(ipcThread, (char*)_gIPCThreadNameStorage.GetPtr() + coreIndex * 0x18);
		OSResumeThread(ipcThread);
	}

	/* coreinit IOS_* API */

	void _IPCDriver_SubmitCmdAllQueued(IPCDriver& ipcDriver)
	{
		// on COS, submitted commands first go to the COS kernel via syscall 0x2000, where they are processed, copied and queued again
		// we skip all of this and just pass our IPC commands directly to the IOSU kernel handler HLE function
		// important: IOSU needs to know which PPC core sent the command, so that it can also notify the same core about the result
		ipcDriver.state = IPCDriverState::SUBMITTING;
		while (true)
		{
			IPCResourceBufferDescriptor* res = ipcDriver.fifoBuffersInFlight.Pop();
			if (!res)
				break;
			// resolve pointers
			switch (res->resourcePtr->commandBody.cmdId)
			{
			case IPCCommandId::IOS_OPEN:
				res->resourcePtr->commandBody.args[0] = res->resourcePtr->commandBody.ppcVirt0.GetMPTR();
				break;
			case IPCCommandId::IOS_CLOSE:
				break;
			case IPCCommandId::IOS_IOCTL:
				res->resourcePtr->commandBody.args[1] = res->resourcePtr->commandBody.ppcVirt0.GetMPTR();
				res->resourcePtr->commandBody.args[3] = res->resourcePtr->commandBody.ppcVirt1.GetMPTR();
				break;
			case IPCCommandId::IOS_IOCTLV:
				res->resourcePtr->commandBody.args[3] = res->resourcePtr->commandBody.ppcVirt0.GetMPTR();
				break;
			default:
				cemu_assert_unimplemented();
				break;
			}
			iosu::kernel::IPCSubmitFromCOS(ipcDriver.coreIndex, &res->resourcePtr->commandBody);
		}
		ipcDriver.state = IPCDriverState::READY;
	}

	void _IPCDriver_SubmitCmd(IPCDriver& ipcDriver, IPCResourceBufferDescriptor* requestDescriptor)
	{
		if (requestDescriptor->asyncResultFunc == nullptr)
			OSInitEvent(&requestDescriptor->eventSynchronousIPC, OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, OSEvent::EVENT_MODE::MODE_AUTO);
		ipcDriver.fifoBuffersInFlight.Push(requestDescriptor);
		cemu_assert_debug(ipcDriver.state == IPCDriverState::READY || ipcDriver.state == IPCDriverState::INITIALIZED);
		_IPCDriver_SubmitCmdAllQueued(ipcDriver);
	}

	uint32 _IPCDriver_WaitForResultAndRelease(IPCDriver& ipcDriver, IPCResourceBufferDescriptor* requestDescriptor)
	{
		OSWaitEvent(&requestDescriptor->eventSynchronousIPC);
		uint32 r = requestDescriptor->resourcePtr->commandBody.result;
		IPCDriver_ReleaseResource(&ipcDriver, requestDescriptor);
		return r;
	}

	void IPCDriver_HandleResponse(IPCDriver& ipcDriver, IPCCommandBody* res, uint32 ppcCoreIndex)
	{
		size_t index = (IPCResourceBuffer*)res - ipcDriver.resourceBuffers.GetPtr();
		cemu_assert(index < IPC_NUM_RESOURCE_BUFFERS);
		IPCResourceBufferDescriptor* descriptor = ipcDriver.resBufferDescriptor + index;
		cemu_assert(descriptor->IsAllocated != 0);
		if (descriptor->asyncMsgQueue != nullptr)
		{
			OSMessage msg;
			msg.message = 0;
			msg.data0 = res->result;
			msg.data1 = descriptor->asyncResultUserParam.GetMPTR();
			msg.data2 = 0;
			sint32 r = OSSendMessage(descriptor->asyncMsgQueue.GetPtr(), &msg, 0);
			cemu_assert(r != 0);
			IPCDriver_ReleaseResource(&ipcDriver, descriptor);
			return;
		}
		if (descriptor->asyncResultFunc != nullptr)
		{
			OSMessage msg;
			msg.message = descriptor->asyncResultFunc.GetMPTR();
			msg.data0 = res->result;
			msg.data1 = descriptor->asyncResultUserParam.GetMPTR();
			msg.data2 = 1;
			sint32 r = OSSendMessage(gIPCThreadMsgQueue.GetPtr() + ppcCoreIndex, &msg, 0);
			cemu_assert(r != 0);
			IPCDriver_ReleaseResource(&ipcDriver, descriptor);
			return;
		}
		// signal event for synchronous IPC
		OSSignalEvent(&descriptor->eventSynchronousIPC);
	}

	// handles responses queued in shared region
	void IPCDriver_KernelCallback(IPCDriver& ipcDriver)
	{
		cemu_assert_debug(ipcDriver.kernelSharedArea.numAvailableResponses != 0);
		for (uint32 i = 0; i < ipcDriver.kernelSharedArea.numAvailableResponses; i++)
			IPCDriver_HandleResponse(ipcDriver, ipcDriver.kernelSharedArea.responseArray[i], ipcDriver.coreIndex);
		ipcDriver.kernelSharedArea.numAvailableResponses = 0;
	}

	// called by our HLE'd IOSU directly
	void IPCDriver_NotifyResponses(uint32 ppcCoreIndex, IPCCommandBody** responseArray, uint32 numResponses)
	{
		cemu_assert(numResponses < 11);
		IPCDriver& ipcDriver = IPCDriver_GetByCore(ppcCoreIndex);
		ipcDriver.kernelSharedArea.numAvailableResponses = numResponses;
		for (uint32 i = 0; i < numResponses; i++)
			ipcDriver.kernelSharedArea.responseArray[i] = responseArray[i];
		IPCDriver_KernelCallback(ipcDriver);
	}

	void _IPCDriver_SetupCmd_IOSOpen(IPCDriver& ipcDriver, IPCResourceBufferDescriptor* requestDescriptor, const char* devicePath, uint32 flags)
	{
		// store the path in the buffer after the command body
		IPCResourceBuffer* resBuffer = requestDescriptor->resourcePtr;
		IPCCommandBody& cmdBody = resBuffer->commandBody;

		uint8* buffer = resBuffer->bufferData;
		size_t pathLen = strlen(devicePath);
		if (pathLen > 31)
		{
			cemuLog_log(LogType::Force, "IOS_Open(): Device path must not exceed 31 characters");
			cemu_assert_error();
		}
		memcpy(buffer, devicePath, pathLen + 1);

		cmdBody.ppcVirt0 = MEMPTR<void>(buffer).GetMPTR();
		cmdBody.args[0] = 0;
		cmdBody.args[1] = (uint32)(pathLen + 1);
		cmdBody.args[2] = flags;
	}

	IOS_ERROR _IPCDriver_SetupCmd_IOSIoctl(IPCDriver& ipcDriver, IPCResourceBufferDescriptor* requestDescriptor, uint32 requestId, void* ptrIn, uint32 sizeIn, void* ptrOut, uint32 sizeOut)
	{
		IPCCommandBody& cmdBody = requestDescriptor->resourcePtr->commandBody;
		cmdBody.args[0] = requestId;
		cmdBody.args[1] = MPTR_NULL; // set to ppcVirt0 later
		cmdBody.args[2] = sizeIn;
		cmdBody.args[3] = MPTR_NULL; // set to ppcVirt1 later
		cmdBody.args[4] = sizeOut;
		cmdBody.ppcVirt0 = MEMPTR<void>(ptrIn).GetMPTR();
		cmdBody.ppcVirt1 = MEMPTR<void>(ptrOut).GetMPTR();
		return IOS_ERROR_OK;
	}

	IOS_ERROR _IPCDriver_SetupCmd_IOSIoctlv(IPCDriver& ipcDriver, IPCResourceBufferDescriptor* requestDescriptor, uint32 requestId, uint32 numIn, uint32 numOut, IPCIoctlVector* vec)
	{
		IPCCommandBody& cmdBody = requestDescriptor->resourcePtr->commandBody;
		// set args
		cmdBody.ppcVirt0 = MEMPTR<void>(vec).GetMPTR();
		cmdBody.args[0] = requestId;
		cmdBody.args[1] = numIn;
		cmdBody.args[2] = numOut;
		cmdBody.args[3] = 0; // set to ppcVirt0 later
		return IOS_ERROR_OK;
	}

	IOSDevHandle IOS_Open(const char* devicePath, uint32 flags)
	{
		IPCDriver& ipcDriver = IPCDriver_GetByCore(OSGetCoreId());
		IPCResourceBufferDescriptor* ipcDescriptor = IPCDriver_AllocateResource(&ipcDriver, 0, IPCCommandId::IOS_OPEN, nullptr, nullptr, nullptr);
		_IPCDriver_SetupCmd_IOSOpen(ipcDriver, ipcDescriptor, devicePath, flags);
		_IPCDriver_SubmitCmd(ipcDriver, ipcDescriptor);
		uint32 r = _IPCDriver_WaitForResultAndRelease(ipcDriver, ipcDescriptor);
		return r;
	}

	IOS_ERROR IOS_Close(IOSDevHandle devHandle)
	{
		IPCDriver& ipcDriver = IPCDriver_GetByCore(OSGetCoreId());
		IPCResourceBufferDescriptor* ipcDescriptor = IPCDriver_AllocateResource(&ipcDriver, devHandle, IPCCommandId::IOS_CLOSE, nullptr, nullptr, nullptr);
		_IPCDriver_SubmitCmd(ipcDriver, ipcDescriptor);
		IOS_ERROR r = (IOS_ERROR)_IPCDriver_WaitForResultAndRelease(ipcDriver, ipcDescriptor);
		return r;
	}

	IOS_ERROR IOS_Ioctl(IOSDevHandle devHandle, uint32 requestId, void* ptrIn, uint32 sizeIn, void* ptrOut, uint32 sizeOut)
	{
		IPCDriver& ipcDriver = IPCDriver_GetByCore(OSGetCoreId());
		IPCResourceBufferDescriptor* ipcDescriptor = IPCDriver_AllocateResource(&ipcDriver, devHandle, IPCCommandId::IOS_IOCTL, nullptr, nullptr, nullptr);
		IOS_ERROR r = _IPCDriver_SetupCmd_IOSIoctl(ipcDriver, ipcDescriptor, requestId, ptrIn, sizeIn, ptrOut, sizeOut);
		if (r != IOS_ERROR_OK)
		{
			cemuLog_log(LogType::Force, "IOS_Ioctl failed due to bad parameters");
			IPCDriver_ReleaseResource(&ipcDriver, ipcDescriptor);
			return r;
		}
		_IPCDriver_SubmitCmd(ipcDriver, ipcDescriptor);
		r = (IOS_ERROR)_IPCDriver_WaitForResultAndRelease(ipcDriver, ipcDescriptor);
		return r;
	}

	IOS_ERROR IOS_IoctlAsync(IOSDevHandle devHandle, uint32 requestId, void* ptrIn, uint32 sizeIn, void* ptrOut, uint32 sizeOut, MEMPTR<void> asyncResultFunc, MEMPTR<void> asyncResultUserParam)
	{
		IPCDriver& ipcDriver = IPCDriver_GetByCore(OSGetCoreId());
		IPCResourceBufferDescriptor* ipcDescriptor = IPCDriver_AllocateResource(&ipcDriver, devHandle, IPCCommandId::IOS_IOCTL, nullptr, asyncResultFunc, asyncResultUserParam);
		IOS_ERROR r = _IPCDriver_SetupCmd_IOSIoctl(ipcDriver, ipcDescriptor, requestId, ptrIn, sizeIn, ptrOut, sizeOut);
		if (r != IOS_ERROR_OK)
		{
			cemuLog_log(LogType::Force, "IOS_Ioctl failed due to bad parameters");
			IPCDriver_ReleaseResource(&ipcDriver, ipcDescriptor);
			return r;
		}
		_IPCDriver_SubmitCmd(ipcDriver, ipcDescriptor);
		return r;
	}

	IOS_ERROR IOS_Ioctlv(IOSDevHandle devHandle, uint32 requestId, uint32 numIn, uint32 numOut, IPCIoctlVector* vec)
	{
		IPCDriver& ipcDriver = IPCDriver_GetByCore(OSGetCoreId());
		IPCResourceBufferDescriptor* ipcDescriptor = IPCDriver_AllocateResource(&ipcDriver, devHandle, IPCCommandId::IOS_IOCTLV, nullptr, nullptr, nullptr);
		IOS_ERROR r = _IPCDriver_SetupCmd_IOSIoctlv(ipcDriver, ipcDescriptor, requestId, numIn, numOut, vec);
		if (r != IOS_ERROR_OK)
		{
			cemuLog_log(LogType::Force, "IOS_Ioctlv failed due to bad parameters");
			IPCDriver_ReleaseResource(&ipcDriver, ipcDescriptor);
			return r;
		}
		_IPCDriver_SubmitCmd(ipcDriver, ipcDescriptor);
		r = (IOS_ERROR)_IPCDriver_WaitForResultAndRelease(ipcDriver, ipcDescriptor);
		return r;
	}

	IOS_ERROR IOS_IoctlvAsync(IOSDevHandle devHandle, uint32 requestId, uint32 numIn, uint32 numOut, IPCIoctlVector* vec, MEMPTR<void> asyncResultFunc, MEMPTR<void> asyncResultUserParam)
	{
		IPCDriver& ipcDriver = IPCDriver_GetByCore(OSGetCoreId());
		IPCResourceBufferDescriptor* ipcDescriptor = IPCDriver_AllocateResource(&ipcDriver, devHandle, IPCCommandId::IOS_IOCTLV, nullptr, asyncResultFunc, asyncResultUserParam);
		IOS_ERROR r = _IPCDriver_SetupCmd_IOSIoctlv(ipcDriver, ipcDescriptor, requestId, numIn, numOut, vec);
		if (r != IOS_ERROR_OK)
		{
			cemuLog_log(LogType::Force, "IOS_Ioctlv failed due to bad parameters");
			IPCDriver_ReleaseResource(&ipcDriver, ipcDescriptor);
			return r;
		}
		_IPCDriver_SubmitCmd(ipcDriver, ipcDescriptor);
		return r;
	}

	void InitializeIPC()
	{
		for (uint32 i = 0; i < Espresso::CORE_COUNT; i++)
		{
			IPCDriver_InitForCore(i);
			IPCDriver_InitIPCThread(i);
		}

		// register API
		cafeExportRegister("coreinit", IOS_Open, LogType::PPC_IPC);
		cafeExportRegister("coreinit", IOS_Close, LogType::PPC_IPC);
		cafeExportRegister("coreinit", IOS_Ioctl, LogType::PPC_IPC);
		cafeExportRegister("coreinit", IOS_IoctlAsync, LogType::PPC_IPC);
		cafeExportRegister("coreinit", IOS_Ioctlv, LogType::PPC_IPC);
		cafeExportRegister("coreinit", IOS_IoctlvAsync, LogType::PPC_IPC);
	}

};
