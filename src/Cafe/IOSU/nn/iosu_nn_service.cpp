#include "iosu_nn_service.h"
#include "../kernel/iosu_kernel.h"
#include "util/helpers/helpers.h"

using namespace iosu::kernel;

namespace iosu
{
	namespace nn
	{
		/* IPCSimpleService */
		void IPCSimpleService::Start()
		{
			if (m_isRunning.exchange(true))
				return;
			m_threadInitialized = false;
			m_requestStop = false;
			m_serviceThread = std::thread(&IPCSimpleService::ServiceThread, this);
			while (!m_threadInitialized) std::this_thread::sleep_for(std::chrono::milliseconds(10));
			StartService();
		}

		void IPCSimpleService::Stop()
		{
			if (!m_isRunning.exchange(false))
				return;
			m_requestStop = true;
			StopService();
			if(m_timerId != IOSInvalidTimerId)
				IOS_DestroyTimer(m_timerId);
			m_timerId = IOSInvalidTimerId;
			IOS_SendMessage(m_msgQueueId, 0, 0); // wake up thread
			m_serviceThread.join();
		}

		void IPCSimpleService::ServiceThread()
		{
			if(!GetThreadName().empty())
				SetThreadName(GetThreadName().c_str());
			m_msgQueueId = IOS_CreateMessageQueue(_m_msgBuffer.GetPtr(), _m_msgBuffer.GetCount());
			cemu_assert(!IOS_ResultIsError((IOS_ERROR)m_msgQueueId));
			IOS_ERROR r = IOS_RegisterResourceManager(m_devicePath.c_str(), m_msgQueueId);
			cemu_assert(!IOS_ResultIsError(r));
			m_threadInitialized = true;
			while (true)
			{
				IOSMessage msg;
				r = IOS_ReceiveMessage(m_msgQueueId, &msg, 0);
				cemu_assert(!IOS_ResultIsError(r));
				if (msg == 0)
				{
					cemu_assert_debug(m_requestStop);
					break;
				}
				else if(msg == 1)
				{
					TimerUpdate();
					continue;
				}
				IPCCommandBody* cmd = MEMPTR<IPCCommandBody>(msg).GetPtr();
				if (cmd->cmdId == IPCCommandId::IOS_OPEN)
				{
					void* clientObject = CreateClientObject();
					if(clientObject == nullptr)
					{
						cemuLog_log(LogType::Force, "IPCSimpleService[{}]: Maximum handle count reached or handle rejected", m_devicePath);
						IOS_ResourceReply(cmd, IOS_ERROR_MAXIMUM_REACHED);
						continue;
					}
					IOSDevHandle newHandle = GetFreeHandle();
					m_clientObjects[newHandle] = clientObject;
					IOS_ResourceReply(cmd, (IOS_ERROR)newHandle);
					continue;
				}
				else if (cmd->cmdId == IPCCommandId::IOS_CLOSE)
				{
					void* clientObject = GetClientObjectByHandle(cmd->devHandle);
					if (clientObject)
						DestroyClientObject(clientObject);
					IOS_ResourceReply(cmd, IOS_ERROR_OK);
					continue;
				}
				else if (cmd->cmdId == IPCCommandId::IOS_IOCTLV)
				{
					void* clientObject = GetClientObjectByHandle(cmd->devHandle);
					if (!clientObject)
					{
						cemuLog_log(LogType::Force, "IPCSimpleService[{}]: Invalid IPC handle", m_devicePath);
						IOS_ResourceReply(cmd, IOS_ERROR_INVALID);
						continue;
					}
					uint32 requestId = cmd->args[0];
					uint32 numIn = cmd->args[1];
					uint32 numOut = cmd->args[2];
					IPCIoctlVector* vec = MEMPTR<IPCIoctlVector>{ cmd->args[3] }.GetPtr();
					IPCIoctlVector* vecIn = vec + 0; // the ordering of vecIn/vecOut differs from IPCService
					IPCIoctlVector* vecOut = vec + numIn;
					m_delayResponse = false;
					m_activeCmd = cmd;
					uint32 result = ServiceCall(clientObject, requestId, vecIn, numIn, vecOut, numOut);
					if (!m_delayResponse)
						IOS_ResourceReply(cmd, (IOS_ERROR)result);
					m_activeCmd = nullptr;
					continue;
				}
				else
				{
					cemuLog_log(LogType::Force, "IPCSimpleService[{}]: Unsupported IPC cmdId {}", m_devicePath, (uint32)cmd->cmdId.value());
					cemu_assert_unimplemented();
					IOS_ResourceReply(cmd, IOS_ERROR_INVALID);
				}
			}
			IOS_DestroyMessageQueue(m_msgQueueId);
			m_threadInitialized = false;
		}

		void IPCSimpleService::SetTimerUpdate(uint32 milliseconds)
		{
			if(m_timerId != IOSInvalidTimerId)
				IOS_DestroyTimer(m_timerId);
			m_timerId = IOS_CreateTimer(milliseconds * 1000, milliseconds * 1000, m_msgQueueId, 1);
		}

		IPCCommandBody* IPCSimpleService::ServiceCallDelayCurrentResponse()
		{
			cemu_assert_debug(m_activeCmd);
			m_delayResponse = true;
			return m_activeCmd;
		}

		void IPCSimpleService::ServiceCallAsyncRespond(IPCCommandBody* response, uint32 r)
		{
			IOS_ResourceReply(response, (IOS_ERROR)r);
		}

		/* IPCService */
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
			std::string serviceName = m_devicePath.substr(m_devicePath.find_last_of('/') == std::string::npos ? 0 : m_devicePath.find_last_of('/') + 1);
			serviceName.insert(0, "NNsvc_");
			SetThreadName(serviceName.c_str());
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
			IOS_DestroyMessageQueue(m_msgQueueId);
			m_threadInitialized = false;
		}
	};
};
