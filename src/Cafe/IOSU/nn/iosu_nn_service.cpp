#include "iosu_nn_service.h"
#include "../kernel/iosu_kernel.h"

using namespace iosu::kernel;

namespace iosu
{
	namespace nn
	{
		void IPCService::Start()
		{
			if (m_isRunning.exchange(true))
				return;
			m_threadInitialized = false;
			m_requestStop = false;
			m_serviceThread = std::thread(&IPCService::ServiceThread, this);
			while (!m_threadInitialized) std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		void IPCService::Stop()
		{
			if (!m_isRunning.exchange(false))
				return;
			m_requestStop = true;
			IOS_SendMessage(m_msgQueueId, 0, 0); // wake up thread
			m_serviceThread.join();
		}

		void IPCService::ServiceThread()
		{
			m_msgQueueId = IOS_CreateMessageQueue(_m_msgBuffer.GetPtr(), _m_msgBuffer.GetCount());
			cemu_assert(!IOS_ResultIsError((IOS_ERROR)m_msgQueueId));
			IOS_ERROR r = IOS_RegisterResourceManager(m_devicePath.c_str(), m_msgQueueId);
			cemu_assert(!IOS_ResultIsError(r));
			m_threadInitialized = true;
			while (true)
			{
				IOSMessage msg;
				IOS_ERROR r = IOS_ReceiveMessage(m_msgQueueId, &msg, 0);
				cemu_assert(!IOS_ResultIsError(r));
				if (msg == 0)
				{
					cemu_assert_debug(m_requestStop);
					break;
				}
				IPCCommandBody* cmd = MEMPTR<IPCCommandBody>(msg).GetPtr();
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

					cemu_assert(vecIn->size >= 80 && !vecIn->basePhys.IsNull());

					IPCServiceRequest* serviceRequest = MEMPTR<IPCServiceRequest>(vecIn->basePhys).GetPtr();
					IPCServiceResponse* serviceResponse = MEMPTR<IPCServiceResponse>(vecOut->basePhys).GetPtr();

					serviceResponse->nnResultCode = (uint32)ServiceCall(serviceRequest->serviceId, nullptr, nullptr);
					IOS_ResourceReply(cmd, IOS_ERROR_OK);
					continue;
				}
				else
				{
					cemuLog_log(LogType::Force, "{}: Unsupported cmdId", m_devicePath);
					cemu_assert_unimplemented();
					IOS_ResourceReply(cmd, IOS_ERROR_INVALID);
				}
			}
			m_threadInitialized = false;
		}
	};
};