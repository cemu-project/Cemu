#pragma once
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"

namespace coreinit
{
	struct OSMessage
	{
		MPTR           message;
		uint32         data0;
		uint32         data1;
		uint32         data2;
	};

	struct OSMessageQueue
	{
		/* +0x00 */ uint32be				magic;
		/* +0x04 */ MEMPTR<void>			userData;
		/* +0x08 */ uint32be				ukn08;
		/* +0x0C */ OSThreadQueue			threadQueueSend;
		/* +0x1C */ OSThreadQueue			threadQueueReceive;
		/* +0x2C */ MEMPTR<OSMessage>		msgArray;
		/* +0x30 */ uint32be				msgCount;
		/* +0x34 */ uint32be				firstIndex;
		/* +0x38 */ uint32be				usedCount;
	};

	static_assert(sizeof(OSMessageQueue) == 0x3C);

	// flags
	#define OS_MESSAGE_BLOCK				1 // blocking send/receive
	#define OS_MESSAGE_HIGH_PRIORITY		2 // put message in front of all queued messages

	void OSInitMessageQueueEx(OSMessageQueue* msgQueue, OSMessage* msgArray, uint32 msgCount, void* userData);
	void OSInitMessageQueue(OSMessageQueue* msgQueue, OSMessage* msgArray, uint32 msgCount);
	bool OSReceiveMessage(OSMessageQueue* msgQueue, OSMessage* msg, uint32 flags);
	bool OSPeekMessage(OSMessageQueue* msgQueue, OSMessage* msg);
	sint32 OSSendMessage(OSMessageQueue* msgQueue, OSMessage* msg, uint32 flags);

	void InitializeMessageQueue();
};