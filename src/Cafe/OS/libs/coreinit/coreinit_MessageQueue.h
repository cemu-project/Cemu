#pragma once
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"

namespace coreinit
{
	enum class SysMessageId : uint32
	{
		MsgAcquireForeground = 0xFACEF000,
		MsgReleaseForeground = 0xFACEBACC,
		MsgExit = 0xD1E0D1E0,
		HomeButtonDenied = 0xCCC0FFEE,
		NetIoStartOrStop = 0xAAC0FFEE,
	};

	struct OSMessage
	{
		uint32be		message;
		uint32be		data0;
		uint32be		data1;
		uint32be		data2;
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

	OSMessageQueue* OSGetSystemMessageQueue();

	void InitializeMessageQueue();
};