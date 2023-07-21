#include "iosu_odm.h"
#include "config/ActiveSettings.h"
#include "Common/FileStream.h"
#include "util/helpers/Semaphore.h"
#include "../kernel/iosu_kernel.h"

namespace iosu
{
	namespace odm
	{
		using namespace iosu::kernel;

		std::string s_devicePath = "/dev/odm";
		std::thread s_serviceThread;
		std::atomic_bool s_requestStop{false};
		std::atomic_bool s_isRunning{false};
		std::atomic_bool s_threadInitialized{ false };

		IOSMsgQueueId s_msgQueueId;
		SysAllocator<iosu::kernel::IOSMessage, 128> _s_msgBuffer;

		enum class ODM_CMD_OPERATION_TYPE
		{
			CHECK_STATE = 4,
			UKN_5 = 5,
		};

		enum class ODM_STATE
		{
			NONE = 0,
			INITIAL = 1,
			AUTHENTICATION = 2,
			WAIT_FOR_DISC_READY = 3,
			CAFE_DISC = 4,
			RVL_DISC = 5,
			CLEANING_DISC = 6,
			INVALID_DISC = 8,
			DIRTY_DISC = 9,
			NO_DISC = 10,
			INVALID_DRIVE = 11,
			FATAL = 12,
			HARD_FATAL = 13,
			SHUTDOWN = 14,
		};

		void ODMHandleCommandIoctl(uint32 clientHandle, IPCCommandBody* cmd, ODM_CMD_OPERATION_TYPE operationId, void* ptrIn, uint32 sizeIn, void* ptrOut, uint32 sizeOut)
		{
			switch(operationId)
			{
				case ODM_CMD_OPERATION_TYPE::CHECK_STATE:
				{
					*(uint32be*)ptrOut = (uint32)ODM_STATE::NO_DISC;
					break;
				}
				case ODM_CMD_OPERATION_TYPE::UKN_5:
				{
					// does this return anything?
					break;
				}
				default:
				{
					cemuLog_log(LogType::Force, "ODMHandleCommandIoctl: Unknown operationId %d\n", (uint32)operationId);
					break;
				}
			}

			IOS_ResourceReply(cmd, IOS_ERROR_OK);
		}

		uint32 CreateClientHandle()
		{
			return 1; // we dont care about handles for now
		}

		void CloseClientHandle(uint32 handle)
		{

		}

		void ODMServiceThread()
		{
			s_msgQueueId = IOS_CreateMessageQueue(_s_msgBuffer.GetPtr(), _s_msgBuffer.GetCount());
			cemu_assert(!IOS_ResultIsError((IOS_ERROR)s_msgQueueId));
			IOS_ERROR r = IOS_RegisterResourceManager(s_devicePath.c_str(), s_msgQueueId);
			cemu_assert(!IOS_ResultIsError(r));
			s_threadInitialized = true;
			while (true)
			{
				IOSMessage msg;
				IOS_ERROR r = IOS_ReceiveMessage(s_msgQueueId, &msg, 0);
				cemu_assert(!IOS_ResultIsError(r));
				if (msg == 0)
				{
					cemu_assert_debug(s_requestStop);
					break;
				}
				IPCCommandBody* cmd = MEMPTR<IPCCommandBody>(msg).GetPtr();
				uint32 clientHandle = (uint32)cmd->devHandle;
				if (cmd->cmdId == IPCCommandId::IOS_OPEN)
				{
					IOS_ResourceReply(cmd, (IOS_ERROR)CreateClientHandle());
					continue;
				}
				else if (cmd->cmdId == IPCCommandId::IOS_CLOSE)
				{
					CloseClientHandle((IOSDevHandle)(uint32)cmd->devHandle);
					IOS_ResourceReply(cmd, IOS_ERROR_OK);
					continue;
				}
				else if (cmd->cmdId == IPCCommandId::IOS_IOCTLV)
				{
					uint32 requestId = cmd->args[0];
					uint32 numIn = cmd->args[1];
					uint32 numOut = cmd->args[2];
					IPCIoctlVector* vec = MEMPTR<IPCIoctlVector>{ cmd->args[3] }.GetPtr();
					IPCIoctlVector* vecIn = vec + numIn;
					IPCIoctlVector* vecOut = vec + 0;
					cemuLog_log(LogType::Force, "{}: Received unsupported Ioctlv cmd", s_devicePath);
					IOS_ResourceReply(cmd, IOS_ERROR_INVALID);
					continue;
				}
				else if (cmd->cmdId == IPCCommandId::IOS_IOCTL)
				{
					ODMHandleCommandIoctl(clientHandle, cmd, (ODM_CMD_OPERATION_TYPE)cmd->args[0].value(), MEMPTR<void>(cmd->args[1]), cmd->args[2], MEMPTR<void>(cmd->args[3]), cmd->args[4]);
				}
				else
				{
					cemuLog_log(LogType::Force, "{}: Unsupported cmdId", s_devicePath);
					cemu_assert_unimplemented();
					IOS_ResourceReply(cmd, IOS_ERROR_INVALID);
				}
			}
			s_threadInitialized = false;
		}

		void Initialize()
		{
			if (s_isRunning.exchange(true))
				return;
			s_threadInitialized = false;
			s_requestStop = false;
			s_serviceThread = std::thread(&ODMServiceThread);
			while (!s_threadInitialized) std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		void Shutdown()
		{
			if (!s_isRunning.exchange(false))
				return;
			s_requestStop = true;
			IOS_SendMessage(s_msgQueueId, 0, 0);
			s_serviceThread.join();
		}
	}
}
