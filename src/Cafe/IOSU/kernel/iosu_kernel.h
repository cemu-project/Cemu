#pragma once
#include "Cafe/IOSU/iosu_ipc_common.h"
#include "Cafe/IOSU/iosu_types_common.h"

namespace iosu
{
namespace kernel
{
using IOSMessage = uint32;

IOSMsgQueueId IOS_CreateMessageQueue(IOSMessage* messageArray, uint32 messageCount);
IOS_ERROR IOS_SendMessage(IOSMsgQueueId msgQueueId, IOSMessage message, uint32 flags);
IOS_ERROR IOS_ReceiveMessage(IOSMsgQueueId msgQueueId, IOSMessage* messageOut, uint32 flags);

IOS_ERROR IOS_RegisterResourceManager(const char* devicePath, IOSMsgQueueId msgQueueId);
IOS_ERROR IOS_DeviceAssociateId(const char* devicePath, uint32 id);
IOS_ERROR IOS_ResourceReply(IPCCommandBody* cmd, IOS_ERROR result);

void IPCSubmitFromCOS(uint32 ppcCoreIndex, IPCCommandBody* cmd);

void Initialize();
} // namespace kernel
} // namespace iosu