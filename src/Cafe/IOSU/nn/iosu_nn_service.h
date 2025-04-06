#pragma once
#include "Cafe/IOSU/kernel/iosu_kernel.h"
#include "Cafe/IOSU/iosu_ipc_common.h"
#include "Cafe/IOSU/iosu_types_common.h"
#include "Cafe/OS/libs/nn_common.h"

namespace iosu
{
	namespace nn
	{
		// a simple service interface which wraps handle management and Ioctlv/IoctlvAsync (used by /dev/fpd and others are still to be determined)
		class IPCSimpleService
		{
		  public:
			IPCSimpleService(std::string_view devicePath) : m_devicePath(devicePath) {};
			virtual ~IPCSimpleService() {};

			virtual void StartService() {};
			virtual void StopService() {};

			virtual std::string GetThreadName() = 0;

			virtual void* CreateClientObject() = 0;
			virtual void DestroyClientObject(void* clientObject) = 0;
			virtual uint32 ServiceCall(void* clientObject, uint32 requestId, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut) = 0;
			virtual void TimerUpdate() {};

			IPCCommandBody* ServiceCallDelayCurrentResponse();
			static void ServiceCallAsyncRespond(IPCCommandBody* response, uint32 r);

			void Start();
			void Stop();

			void SetTimerUpdate(uint32 milliseconds);

		  private:
			void ServiceThread();

			IOSDevHandle GetFreeHandle()
			{
				while(m_clientObjects.find(m_nextHandle) != m_clientObjects.end() || m_nextHandle == 0)
				{
					m_nextHandle++;
					m_nextHandle &= 0x7FFFFFFF;
				}
				IOSDevHandle newHandle = m_nextHandle;
				m_nextHandle++;
				m_nextHandle &= 0x7FFFFFFF;
				return newHandle;
			}

			void* GetClientObjectByHandle(IOSDevHandle handle) const
			{
				auto it = m_clientObjects.find(handle);
				if(it == m_clientObjects.end())
					return nullptr;
				return it->second;
			}

			std::string m_devicePath;
			std::thread m_serviceThread;
			std::atomic_bool m_requestStop{false};
			std::atomic_bool m_isRunning{false};
			std::atomic_bool m_threadInitialized{ false };
			std::unordered_map<IOSDevHandle, void*> m_clientObjects;
			IOSDevHandle m_nextHandle{1};
			IOSTimerId m_timerId{IOSInvalidTimerId};

			IPCCommandBody* m_activeCmd{nullptr};
			bool m_delayResponse{false};

			IOSMsgQueueId m_msgQueueId;
			SysAllocator<iosu::kernel::IOSMessage, 128> _m_msgBuffer;
		};

		struct IPCServiceRequest
		{
			uint32be ukn00;
			uint32be serviceId;
			uint32be ukn08;
			uint32be commandId;
		};

		static_assert(sizeof(IPCServiceRequest) == 0x10);

		struct IPCServiceResponse
		{
			uint32be nnResultCode;
		};

		// a complex service interface which wraps Ioctlv and adds an additional service channel, used by /dev/act, /dev/acp_main, ?
		class IPCService
		{
		public:
			IPCService(std::string_view devicePath) : m_devicePath(devicePath) {};
			virtual ~IPCService() {};

			virtual IOSDevHandle CreateClientHandle()
			{
				return (IOSDevHandle)0;
			}

			virtual void CloseClientHandle(IOSDevHandle handle)
			{
				
			}

			virtual nnResult ServiceCall(uint32 serviceId, void* request, void* response)
			{
				cemu_assert_unimplemented();
				return 0;
			}

			void Start();
			void Stop();

		private:
			void ServiceThread();

			std::string m_devicePath;
			std::thread m_serviceThread;
			std::atomic_bool m_requestStop{false};
			std::atomic_bool m_isRunning{false};
			std::atomic_bool m_threadInitialized{ false };

			IOSMsgQueueId m_msgQueueId;
			SysAllocator<iosu::kernel::IOSMessage, 128> _m_msgBuffer;
		};

	};
};