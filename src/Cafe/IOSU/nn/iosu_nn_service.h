#pragma once
#include "Cafe/IOSU/kernel/iosu_kernel.h"
#include "Cafe/IOSU/iosu_ipc_common.h"
#include "Cafe/IOSU/iosu_types_common.h"
#include "Cafe/OS/libs/nn_common.h"

namespace iosu
{
	namespace nn
	{
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