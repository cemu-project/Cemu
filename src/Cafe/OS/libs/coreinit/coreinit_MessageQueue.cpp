#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_MessageQueue.h"

namespace coreinit
{
	void UpdateSystemMessageQueue();
	void HandleReceivedSystemMessage(OSMessage* msg);

	SysAllocator<OSMessageQueue> g_systemMessageQueue;
	SysAllocator<OSMessage, 16> _systemMessageQueueArray;

	void OSInitMessageQueueEx(OSMessageQueue* msgQueue, OSMessage* msgArray, uint32 msgCount, void* userData)
	{
		msgQueue->magic = 'mSgQ';
		msgQueue->userData = userData;
		msgQueue->msgArray = msgArray;
		msgQueue->msgCount = msgCount;
		msgQueue->firstIndex = 0;
		msgQueue->usedCount = 0;
		msgQueue->ukn08 = 0;
		OSInitThreadQueueEx(&msgQueue->threadQueueReceive, msgQueue);
		OSInitThreadQueueEx(&msgQueue->threadQueueSend, msgQueue);
	}

	void OSInitMessageQueue(OSMessageQueue* msgQueue, OSMessage* msgArray, uint32 msgCount)
	{
		OSInitMessageQueueEx(msgQueue, msgArray, msgCount, nullptr);
	}

	bool OSReceiveMessage(OSMessageQueue* msgQueue, OSMessage* msg, uint32 flags)
	{
		bool isSystemMessageQueue = (msgQueue == g_systemMessageQueue);
		if(isSystemMessageQueue)
			UpdateSystemMessageQueue();
		__OSLockScheduler(msgQueue);
		while (msgQueue->usedCount == (uint32be)0)
		{
			if ((flags & OS_MESSAGE_BLOCK))
			{
				msgQueue->threadQueueReceive.queueAndWait(OSGetCurrentThread());
			}
			else
			{
				__OSUnlockScheduler(msgQueue);
				return false;
			}
		}
		// copy message
		sint32 messageIndex = msgQueue->firstIndex;
		OSMessage* readMsg = &(msgQueue->msgArray[messageIndex]);
		memcpy(msg, readMsg, sizeof(OSMessage));
		msgQueue->firstIndex = ((uint32)msgQueue->firstIndex + 1) % (uint32)(msgQueue->msgCount);
		msgQueue->usedCount = (uint32)msgQueue->usedCount - 1;
		// wake up any thread waiting to add a message
		if (!msgQueue->threadQueueSend.isEmpty())
			msgQueue->threadQueueSend.wakeupSingleThreadWaitQueue(true);
		__OSUnlockScheduler(msgQueue);
		if(isSystemMessageQueue)
			HandleReceivedSystemMessage(msg);
		return true;
	}

	bool OSPeekMessage(OSMessageQueue* msgQueue, OSMessage* msg)
	{
		__OSLockScheduler(msgQueue);
		if ((msgQueue->usedCount == (uint32be)0))
		{
			__OSUnlockScheduler(msgQueue);
			return false;
		}
		// copy message
		sint32 messageIndex = msgQueue->firstIndex;
		if (msg)
		{
			OSMessage* readMsg = &(msgQueue->msgArray[messageIndex]);
			memcpy(msg, readMsg, sizeof(OSMessage));
		}
		__OSUnlockScheduler(msgQueue);
		return true;
	}

	sint32 OSSendMessage(OSMessageQueue* msgQueue, OSMessage* msg, uint32 flags)
	{
		__OSLockScheduler();
		while (msgQueue->usedCount >= msgQueue->msgCount)
		{
			if ((flags & OS_MESSAGE_BLOCK))
			{
				msgQueue->threadQueueSend.queueAndWait(OSGetCurrentThread());																  
			}
			else
			{
				__OSUnlockScheduler();
				return 0;
			}
		}
		// add message
		if ((flags & OS_MESSAGE_HIGH_PRIORITY))
		{
			// decrease firstIndex
			sint32 newFirstIndex = (sint32)((sint32)msgQueue->firstIndex + (sint32)msgQueue->msgCount - 1) % (sint32)msgQueue->msgCount;
			msgQueue->firstIndex = newFirstIndex;
			// insert message at new first index
			msgQueue->usedCount = (uint32)msgQueue->usedCount + 1;
			OSMessage* newMsg = &(msgQueue->msgArray[newFirstIndex]);
			memcpy(newMsg, msg, sizeof(OSMessage));
		}
		else
		{
			sint32 messageIndex = (uint32)(msgQueue->firstIndex + msgQueue->usedCount) % (uint32)msgQueue->msgCount;
			msgQueue->usedCount = (uint32)msgQueue->usedCount + 1;
			OSMessage* newMsg = &(msgQueue->msgArray[messageIndex]);
			memcpy(newMsg, msg, sizeof(OSMessage));
		}
		// wake up any thread waiting to read a message
		if (!msgQueue->threadQueueReceive.isEmpty())
			msgQueue->threadQueueReceive.wakeupSingleThreadWaitQueue(true);
		__OSUnlockScheduler();
		return 1;
	}

	OSMessageQueue* OSGetSystemMessageQueue()
	{
		return g_systemMessageQueue.GetPtr();
	}

	void InitializeMessageQueue()
	{
		OSInitMessageQueue(g_systemMessageQueue.GetPtr(), _systemMessageQueueArray.GetPtr(), _systemMessageQueueArray.GetCount());

		cafeExportRegister("coreinit", OSInitMessageQueueEx, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSInitMessageQueue, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSReceiveMessage, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSPeekMessage, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSSendMessage, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSGetSystemMessageQueue, LogType::CoreinitThread);
	}
};

