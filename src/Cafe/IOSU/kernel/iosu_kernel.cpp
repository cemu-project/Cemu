#include "iosu_kernel.h"
#include "util/helpers/fspinlock.h"
#include "Cafe/OS/libs/coreinit/coreinit_IPC.h"

namespace iosu
{
	namespace kernel
	{
		std::mutex sInternalMutex;

		static void _assume_lock()
		{
#ifdef CEMU_DEBUG_ASSERT
			cemu_assert_debug(!sInternalMutex.try_lock());
#endif
		}

		/* message queue */

		struct IOSMessageQueue 
		{
			// placeholder
			/* +0x00 */ uint32be ukn00;
			/* +0x04 */ uint32be ukn04;
			/* +0x08 */ uint32be numQueuedMessages;
			/* +0x0C */ uint32be readIndex;
			/* +0x10 */ uint32be msgArraySize; // 0 if queue is not allocated
			/* +0x14 */ MEMPTR<betype<IOSMessage>> msgArray;
			/* +0x18 */ IOSMsgQueueId queueHandle;
			/* +0x1C */ uint32be ukn1C;

			uint32 GetWriteIndex()
			{
				uint32 idx = readIndex + numQueuedMessages;
				if (idx >= msgArraySize)
					idx -= msgArraySize;
				return idx;
			}

			/* HLE extension */
			std::condition_variable cv_send;
			std::condition_variable cv_recv;
		};

		std::array<IOSMessageQueue, 750> sMsgQueuePool;

		IOS_ERROR _IOS_GetMessageQueue(IOSMsgQueueId queueHandle, IOSMessageQueue*& queueOut)
		{
			_assume_lock();
			uint32 index = (queueHandle & 0xFFF);
			if (index >= sMsgQueuePool.size())
				return IOS_ERROR_INVALID;
			IOSMessageQueue& q = sMsgQueuePool.at(index);
			if(q.queueHandle != queueHandle)
				return IOS_ERROR_INVALID;
			queueOut = &q;
			return IOS_ERROR_OK;
		}

		IOSMsgQueueId IOS_CreateMessageQueue(IOSMessage* messageArray, uint32 messageCount)
		{
			std::unique_lock _l(sInternalMutex);
			cemu_assert(messageCount != 0);
			auto it = std::find_if(sMsgQueuePool.begin(), sMsgQueuePool.end(), [](const IOSMessageQueue& q) { return q.msgArraySize == 0; });
			if (it == sMsgQueuePool.end())
			{
				cemu_assert_suspicious();
				return IOS_ERROR_MAXIMUM_REACHED;
			}
			size_t index = std::distance(sMsgQueuePool.begin(), it);
			IOSMessageQueue& msgQueue = sMsgQueuePool.at(index);
			// create queue handle
			static uint32 sQueueHandleCounter = 0;
			uint32 queueHandle = (uint32)index | ((sQueueHandleCounter<<12)&0x7FFFFFFF);
			sQueueHandleCounter++;

			msgQueue.queueHandle = queueHandle;
			msgQueue.msgArraySize = messageCount;
			msgQueue.msgArray = (betype<IOSMessage>*)messageArray;

			msgQueue.numQueuedMessages = 0;
			msgQueue.readIndex = 0;

			return queueHandle;
		}

		IOS_ERROR IOS_SendMessage(IOSMsgQueueId msgQueueId, IOSMessage message, uint32 flags)
		{
			std::unique_lock _l(sInternalMutex);
			cemu_assert_debug(flags == 0 || flags == 1);
			bool dontBlock = (flags & 1) != 0;
			IOSMessageQueue* msgQueue = nullptr;
			IOS_ERROR r = _IOS_GetMessageQueue(msgQueueId, msgQueue);
			if (r != IOS_ERROR_OK)
				return r;
			while (true)
			{
				if (msgQueue->numQueuedMessages == msgQueue->msgArraySize)
				{
					if (dontBlock)
						return IOS_ERROR_WOULD_BLOCK;
				}
				else
					break;
				msgQueue->cv_send.wait(_l);
				// after returning from wait, make sure the queue handle is unchanged
				if (msgQueue->queueHandle != msgQueueId)
					return IOS_ERROR_INVALID;
			}
			uint32 writeIndex = msgQueue->GetWriteIndex();
			msgQueue->msgArray[writeIndex] = message;
			msgQueue->numQueuedMessages += 1;
			msgQueue->cv_recv.notify_one();
			return IOS_ERROR_OK;
		}

		IOS_ERROR IOS_ReceiveMessage(IOSMsgQueueId msgQueueId, IOSMessage* messageOut, uint32 flags)
		{
			std::unique_lock _l(sInternalMutex);
			cemu_assert_debug(flags == 0 || flags == 1);
			bool dontBlock = (flags & 1) != 0;
			IOSMessageQueue* msgQueue = nullptr;
			IOS_ERROR r = _IOS_GetMessageQueue(msgQueueId, msgQueue);
			if (r != IOS_ERROR_OK)
				return r;
			while (true)
			{
				if (msgQueue->numQueuedMessages == 0)
				{
					if (dontBlock)
						return IOS_ERROR_NONE_AVAILABLE;
				}
				else
					break;
				msgQueue->cv_recv.wait(_l);
				// after returning from wait, make sure the queue handle is unchanged
				if (msgQueue->queueHandle != msgQueueId)
					return IOS_ERROR_INVALID;
			}
			*messageOut = msgQueue->msgArray[(uint32)msgQueue->readIndex];
			msgQueue->readIndex = msgQueue->readIndex + 1;
			if (msgQueue->readIndex >= msgQueue->msgArraySize)
				msgQueue->readIndex -= msgQueue->msgArraySize;
			msgQueue->numQueuedMessages -= 1;
			msgQueue->cv_send.notify_one();
			return IOS_ERROR_OK;
		}

		/* devices and IPC */

		struct IOSResourceManager
		{
			bool isSet{false};
			std::string path;
			IOSMsgQueueId msgQueueId;
		};

		std::array<IOSResourceManager, 512> sDeviceResources;
		
		IOSResourceManager* _IOS_FindResourceManager(const char* devicePath)
		{
			_assume_lock();
			std::string_view devicePathSV{ devicePath };
			for (auto& it : sDeviceResources)
			{
				if (it.isSet && it.path == devicePathSV)
					return &it;
			}
			return nullptr;
		}

		IOSResourceManager* _IOS_CreateNewResourceManager(const char* devicePath, IOSMsgQueueId msgQueueId)
		{
			_assume_lock();
			std::string_view devicePathSV{ devicePath };
			for (auto& it : sDeviceResources)
			{
				if (!it.isSet)
				{
					it.isSet = true;
					it.path = devicePath;
					it.msgQueueId = msgQueueId;
					return &it;
				}
			}
			return nullptr;
		}

		IOS_ERROR IOS_RegisterResourceManager(const char* devicePath, IOSMsgQueueId msgQueueId)
		{
			std::unique_lock _lock(sInternalMutex);
			if (_IOS_FindResourceManager(devicePath))
			{
				cemu_assert_suspicious();
				return IOS_ERROR_INVALID; // correct error code?
			}

			// verify if queue is valid
			IOSMessageQueue* msgQueue;
			IOS_ERROR r = _IOS_GetMessageQueue(msgQueueId, msgQueue);
			if (r != IOS_ERROR_OK)
				return r;

			// create resource manager
			IOSResourceManager* resourceMgr = _IOS_CreateNewResourceManager(devicePath, msgQueueId);
			if (!resourceMgr)
				return IOS_ERROR_MAXIMUM_REACHED;

			return IOS_ERROR_OK;
		}

		IOS_ERROR IOS_DeviceAssociateId(const char* devicePath, uint32 id)
		{
			// not yet implemented
			return IOS_ERROR_OK;
		}

		/* IPC */
		
		struct IOSDispatchableCommand
		{
			// stores a copy of incoming IPC requests with some extra information required for replies
			IPCCommandBody body; // our dispatchable copy
			IPCIoctlVector vecCopy[8]; // our copy of the Ioctlv vector array
			IPCCommandBody* originalBody; // the original command that was sent to us
			uint32 ppcCoreIndex;
			IOSDevHandle replyHandle; // handle for outgoing replies
			bool isAllocated{false};
		};

		SysAllocator<IOSDispatchableCommand, 96> sIPCDispatchableCommandPool;
		std::queue<IOSDispatchableCommand*> sIPCFreeDispatchableCommands;
		FSpinlock sIPCDispatchableCommandPoolLock;

		void _IPCInitDispatchablePool()
		{
			sIPCDispatchableCommandPoolLock.acquire();
			while (!sIPCFreeDispatchableCommands.empty())
				sIPCFreeDispatchableCommands.pop();
			for (size_t i = 0; i < sIPCDispatchableCommandPool.GetCount(); i++)
				sIPCFreeDispatchableCommands.push(sIPCDispatchableCommandPool.GetPtr()+i);
			sIPCDispatchableCommandPoolLock.release();
		}

		IOSDispatchableCommand* _IPCAllocateDispatchableCommand()
		{
			sIPCDispatchableCommandPoolLock.acquire();
			if (sIPCFreeDispatchableCommands.empty())
			{
				cemuLog_log(LogType::Force, "IOS: Exhausted pool of dispatchable commands");
				sIPCDispatchableCommandPoolLock.release();
				return nullptr;
			}
			IOSDispatchableCommand* cmd = sIPCFreeDispatchableCommands.front();
			sIPCFreeDispatchableCommands.pop();
			cemu_assert_debug(!cmd->isAllocated);
			cmd->isAllocated = true;
			sIPCDispatchableCommandPoolLock.release();
			return cmd;
		}

		void _IPCReleaseDispatchableCommand(IOSDispatchableCommand* cmd)
		{
			sIPCDispatchableCommandPoolLock.acquire();
			cemu_assert_debug(cmd->isAllocated);
			cmd->isAllocated = false;
			sIPCFreeDispatchableCommands.push(cmd);
			sIPCDispatchableCommandPoolLock.release();
		}

		static constexpr size_t MAX_NUM_ACTIVE_DEV_HANDLES = 96; // per process

		struct IPCActiveDeviceHandle
		{
			bool isSet{false};
			uint32 handleCheckValue{0};
			std::string path;
			IOSMsgQueueId msgQueueId;
			// dispatch target handle (retrieved via IOS_OPEN command to dispatch target)
			bool hasDispatchTargetHandle{false};
			IOSDevHandle dispatchTargetHandle;
		};

		IPCActiveDeviceHandle sActiveDeviceHandles[MAX_NUM_ACTIVE_DEV_HANDLES];

		IOS_ERROR _IPCCreateResourceHandle(const char* devicePath, IOSDevHandle& handleOut)
		{
			std::unique_lock _lock(sInternalMutex);
			static uint32 sHandleCreationCounter = 1;
			// find resource manager for device
			IOSResourceManager* resMgr = _IOS_FindResourceManager(devicePath);
			if (!resMgr)
			{
				cemuLog_log(LogType::Force, "IOSU-Kernel: IOS_Open() could not open {}", devicePath);
				return IOS_ERROR_INVALID;
			}
			IOSMsgQueueId msgQueueId = resMgr->msgQueueId;
			_lock.unlock();
			// create new handle
			sint32 deviceHandleIndex = -1;
			for (size_t i = 0; i < MAX_NUM_ACTIVE_DEV_HANDLES; i++)
			{
				if (!sActiveDeviceHandles[i].isSet)
				{
					deviceHandleIndex = (sint32)i;
					break;
				}
			}
			cemu_assert_debug(deviceHandleIndex >= 0);
			if (deviceHandleIndex < 0)
				return IOS_ERROR_MAXIMUM_REACHED;
			// calc handle
			uint32 devHandle = deviceHandleIndex | ((sHandleCreationCounter << 12) & 0x7FFFFFFF);
			sHandleCreationCounter++;
			// init handle instance
			sActiveDeviceHandles[deviceHandleIndex].isSet = true;
			sActiveDeviceHandles[deviceHandleIndex].handleCheckValue = devHandle;
			sActiveDeviceHandles[deviceHandleIndex].path = devicePath;
			sActiveDeviceHandles[deviceHandleIndex].msgQueueId = msgQueueId;
			sActiveDeviceHandles[deviceHandleIndex].hasDispatchTargetHandle = false;			
			handleOut = devHandle;
			return IOS_ERROR_OK;
		}

		IOS_ERROR _IPCDestroyResourceHandle(IOSDevHandle devHandle)
		{
			std::unique_lock _lock(sInternalMutex);
			uint32 index = devHandle & 0xFFF;
			cemu_assert(index < MAX_NUM_ACTIVE_DEV_HANDLES);
			if (!sActiveDeviceHandles[index].isSet)
			{
				cemuLog_log(LogType::Force, "_IPCDispatchToResourceManager(): Resource manager destroyed before all IPC commands were processed");
				return IOS_ERROR_INVALID;
			}
			if (devHandle != sActiveDeviceHandles[index].handleCheckValue)
			{
				cemuLog_log(LogType::Force, "_IPCDispatchToResourceManager(): Mismatching handle");
				return IOS_ERROR_INVALID;
			}
			sActiveDeviceHandles[index].isSet = false;
			sActiveDeviceHandles[index].handleCheckValue = 0;
			sActiveDeviceHandles[index].hasDispatchTargetHandle = false;
			_lock.unlock();
			return IOS_ERROR_OK;
		}

		IOS_ERROR _IPCAssignDispatchTargetHandle(IOSDevHandle devHandle, IOSDevHandle internalHandle)
		{
			std::unique_lock _lock(sInternalMutex);
			uint32 index = devHandle & 0xFFF;
			cemu_assert(index < MAX_NUM_ACTIVE_DEV_HANDLES);
			if (!sActiveDeviceHandles[index].isSet)
			{
				cemuLog_log(LogType::Force, "_IPCDispatchToResourceManager(): Resource manager destroyed before all IPC commands were processed");
				return IOS_ERROR_INVALID;
			}
			if (devHandle != sActiveDeviceHandles[index].handleCheckValue)
			{
				cemuLog_log(LogType::Force, "_IPCDispatchToResourceManager(): Mismatching handle");
				return IOS_ERROR_INVALID;
			}
			cemu_assert_debug(!sActiveDeviceHandles[index].hasDispatchTargetHandle);
			sActiveDeviceHandles[index].hasDispatchTargetHandle = true;
			sActiveDeviceHandles[index].dispatchTargetHandle = internalHandle;
			_lock.unlock();
			return IOS_ERROR_OK;
		}

		IOS_ERROR _IPCDispatchToResourceManager(IOSDevHandle devHandle, IOSDispatchableCommand* dispatchCmd)
		{
			std::unique_lock _lock(sInternalMutex);
			uint32 index = devHandle & 0xFFF;
			cemu_assert(index < MAX_NUM_ACTIVE_DEV_HANDLES);
			if (!sActiveDeviceHandles[index].isSet)
			{
				cemuLog_log(LogType::Force, "_IPCDispatchToResourceManager(): Resource manager destroyed before all IPC commands were processed");
				return IOS_ERROR_INVALID;
			}
			if (devHandle != sActiveDeviceHandles[index].handleCheckValue)
			{
				cemuLog_log(LogType::Force, "_IPCDispatchToResourceManager(): Mismatching handle");
				return IOS_ERROR_INVALID;
			}
			IOSMsgQueueId msgQueueId = sActiveDeviceHandles[index].msgQueueId;
			if (dispatchCmd->body.cmdId == IPCCommandId::IOS_OPEN)
			{
				cemu_assert(!sActiveDeviceHandles[index].hasDispatchTargetHandle);
				dispatchCmd->body.devHandle = 0;
			}
			else
			{
				cemu_assert(sActiveDeviceHandles[index].hasDispatchTargetHandle);
				dispatchCmd->body.devHandle = sActiveDeviceHandles[index].dispatchTargetHandle;
			}
			_lock.unlock();
			MEMPTR<IOSDispatchableCommand> msgVal{ dispatchCmd };
			IOS_ERROR r = IOS_SendMessage(msgQueueId, msgVal.GetMPTR(), 1);
			if(r != IOS_ERROR_OK)
				cemuLog_log(LogType::Force, "_IPCDispatchToResourceManager(): SendMessage returned {}", (sint32)r);
			return r;
		}

		void _IPCReplyAndRelease(IOSDispatchableCommand* dispatchCmd, uint32 result)
		{
			cemu_assert(dispatchCmd >= sIPCDispatchableCommandPool.GetPtr() && dispatchCmd < sIPCDispatchableCommandPool.GetPtr() + sIPCDispatchableCommandPool.GetCount());	
			dispatchCmd->originalBody->result = result;
			// submit to COS
			IPCCommandBody* responseArray[1];
			responseArray[0] = dispatchCmd->originalBody;
			coreinit::IPCDriver_NotifyResponses(dispatchCmd->ppcCoreIndex, responseArray, 1);
			_IPCReleaseDispatchableCommand(dispatchCmd);
		}

		IOS_ERROR _IPCHandlerIn_IOS_Open(IOSDispatchableCommand* dispatchCmd)
		{
			IPCCommandBody& cmd = dispatchCmd->body;
			const char* name = MEMPTR<const char>(cmd.args[0]).GetPtr();
			uint32 nameLenPlusOne = cmd.args[1];
			cemu_assert(nameLenPlusOne > 0);
			uint32 flags = cmd.args[2];
			cemu_assert_debug(flags == 0);

			std::string devicePath{ name, nameLenPlusOne - 1 };

			IOSDevHandle handle;
			IOS_ERROR r = _IPCCreateResourceHandle(devicePath.c_str(), handle);
			if (r != IOS_ERROR_OK)
				return r;
			dispatchCmd->replyHandle = handle;
			dispatchCmd->body.devHandle = 0;
			r = _IPCDispatchToResourceManager(handle, dispatchCmd);
			return r;
		}

		IOS_ERROR _IPCHandlerIn_IOS_Close(IOSDispatchableCommand* dispatchCmd)
		{
			IPCCommandBody& cmd = dispatchCmd->body;
			IOS_ERROR r = _IPCDispatchToResourceManager(dispatchCmd->body.devHandle, dispatchCmd);
			return r;
		}

		IOS_ERROR _IPCHandlerIn_IOS_Ioctl(IOSDispatchableCommand* dispatchCmd)
		{
			IPCCommandBody& cmd = dispatchCmd->body;
			IOS_ERROR r = _IPCDispatchToResourceManager(dispatchCmd->body.devHandle, dispatchCmd);
			return r;
		}

		IOS_ERROR _IPCHandlerIn_IOS_Ioctlv(IOSDispatchableCommand* dispatchCmd)
		{
			IPCCommandBody& cmd = dispatchCmd->body;
			uint32 requestId = dispatchCmd->body.args[0];
			uint32 numIn = dispatchCmd->body.args[1];
			uint32 numOut = dispatchCmd->body.args[2];
			IPCIoctlVector* vec = MEMPTR<IPCIoctlVector>(cmd.args[3]).GetPtr();

			// copy the vector array
			uint32 numVec = numIn + numOut;
			if (numVec <= 8)
			{
				std::copy(vec, vec + numVec, dispatchCmd->vecCopy);
				dispatchCmd->body.args[3] = MEMPTR<IPCIoctlVector>(vec).GetMPTR();
			}
			else
			{
				// reuse the original vector pointer
				cemuLog_log(LogType::Force, "Info: Ioctlv command with more than 8 vectors");
			}
			IOS_ERROR r = _IPCDispatchToResourceManager(dispatchCmd->body.devHandle, dispatchCmd);
			return r;
		}

		// called by COS directly
		void IPCSubmitFromCOS(uint32 ppcCoreIndex, IPCCommandBody* cmd)
		{
			// create a copy of the cmd
			IOSDispatchableCommand* dispatchCmd = _IPCAllocateDispatchableCommand();
			dispatchCmd->body = *cmd;
			dispatchCmd->originalBody = cmd;
			dispatchCmd->ppcCoreIndex = ppcCoreIndex;
			dispatchCmd->replyHandle = cmd->devHandle;
			// forward command to device
			IOS_ERROR r = IOS_ERROR_INVALID;
			switch ((IPCCommandId)cmd->cmdId)
			{
			case IPCCommandId::IOS_OPEN:
				dispatchCmd->replyHandle = 0;
				r = _IPCHandlerIn_IOS_Open(dispatchCmd);
				break;
			case IPCCommandId::IOS_CLOSE:
				r = _IPCHandlerIn_IOS_Close(dispatchCmd);
				break;
			case IPCCommandId::IOS_IOCTL:
				r = _IPCHandlerIn_IOS_Ioctl(dispatchCmd);
				break;
			case IPCCommandId::IOS_IOCTLV:
				r = _IPCHandlerIn_IOS_Ioctlv(dispatchCmd);
				break;
			default:
				cemuLog_log(LogType::Force, "Invalid IPC command {}", (uint32)(IPCCommandId)cmd->cmdId);
				break;
			}
			if (r < 0)
			{
				cemuLog_log(LogType::Force, "Error occurred while trying to dispatch IPC");			
				_IPCReplyAndRelease(dispatchCmd, r);
				// in non-error case the device handler will send the result asynchronously via IOS_ResourceReply
			}
		}

		IOS_ERROR IOS_ResourceReply(IPCCommandBody* cmd, IOS_ERROR result)
		{
			IOSDispatchableCommand* dispatchCmd = (IOSDispatchableCommand*)cmd;
			cemu_assert(dispatchCmd >= sIPCDispatchableCommandPool.GetPtr() && dispatchCmd < sIPCDispatchableCommandPool.GetPtr() + sIPCDispatchableCommandPool.GetCount());
			cemu_assert_debug(dispatchCmd->isAllocated);
			dispatchCmd->originalBody->result = result;
			if (dispatchCmd->originalBody->cmdId == IPCCommandId::IOS_OPEN)
			{
				IOSDevHandle devHandle = dispatchCmd->replyHandle;
				if (IOS_ResultIsError(result))
				{
					cemuLog_log(LogType::Force, "IOS_ResourceReply(): Target device triggered an error on IOS_OPEN");
					// dispatch target returned error, destroy our device handle again
					IOS_ERROR r = _IPCDestroyResourceHandle(devHandle);
					cemu_assert(r == IOS_ERROR_OK);
				}
				else
				{
					cemu_assert(_IPCAssignDispatchTargetHandle(devHandle, (IOSDevHandle)result) == IOS_ERROR_OK);
					result = (IOS_ERROR)(uint32)devHandle;
				}
			}
			else if (dispatchCmd->originalBody->cmdId == IPCCommandId::IOS_CLOSE)
			{
				if (IOS_ResultIsError(result))
				{
					cemuLog_log(LogType::Force, "IOS_ResourceReply(): Target device triggered an error on IOS_CLOSE");
				}
				// reply, then destroy handle
				IOSDevHandle devHandle = dispatchCmd->replyHandle;
				_IPCReplyAndRelease(dispatchCmd, result);
				IOS_ERROR r = _IPCDestroyResourceHandle(devHandle);
				cemu_assert_debug(r == IOS_ERROR::IOS_ERROR_OK);
				return IOS_ERROR_OK;
			}
			_IPCReplyAndRelease(dispatchCmd, result);
			return IOS_ERROR_OK;
		}

		void Initialize()
		{
			_IPCInitDispatchablePool();
		}

	}
}