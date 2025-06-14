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

		// a complex service interface which wraps Ioctlv and adds an additional service channel, used by /dev/act, /dev/acp_main, /dev/boss and most nn services
		class IPCService
		{
			struct IPCServiceRequestHeader
			{
				uint32be ukn00;
				uint32be serviceId;
				uint32be ukn08;
				uint32be commandId;
			};

			static_assert(sizeof(IPCServiceRequestHeader) == 0x10);

			struct IPCServiceResponseHeader
			{
				uint32be nnResultCode;
			};
		public:
			class IPCParameterStream // input stream for parameters
			{
			public:
				IPCParameterStream() = default;
				IPCParameterStream(void* data, uint32 size) : m_data((uint8_t*)data), m_size(size) {}

				template<typename T>
				T ReadParameter(bool& hasError)
				{
					hasError = false;
					if (m_readIndex + sizeof(T) > m_size)
					{
						hasError = true;
						return T{};
					}
					T value;
					memcpy(&value, &m_data[m_readIndex], sizeof(T));
					m_readIndex += sizeof(T);
					return value;
				}

				uint8* GetData() { return m_data; }
				uint32 GetSize() const { return m_size; }

			private:
				uint8* m_data{nullptr};
				uint32 m_size{0};
				uint32 m_readIndex{0};
			};

			class IPCResponseStream // output stream for response data
			{
			public:
				IPCResponseStream() = default;
				IPCResponseStream(void* data, uint32 size) : m_data((uint8*)data), m_size(size) {}

				template<typename T>
				void Write(const T& value)
				{
					if (m_writtenSize + sizeof(T) > m_size)
					{
						m_hasError = true;
						return;
					}
					memcpy(&m_data[m_writtenSize], &value, sizeof(T));
					m_writtenSize += sizeof(T);
				}

				uint8* GetData() { return m_data; }
				uint32 GetSize() const { return m_size; }

			private:
				uint8* m_data{nullptr};
				uint32 m_writtenSize{0};
				uint32 m_size{0};
				bool m_hasError{false};
			};

			class IPCServiceCall
			{
				struct LargeBufferHeader
				{
					uint32be alignedSize; // size of the aligned (middle) part of the buffer
					uint8be ukn4;
					uint8be ukn5;
					uint8be headSize;
					uint8be tailSize;
				};
				static_assert(sizeof(LargeBufferHeader) == 8);
			public:
				struct UnalignedBuffer
				{
					UnalignedBuffer(bool isOutput, std::span<uint8> head, std::span<uint8> middle, std::span<uint8> tail) : m_isOutput(isOutput)
					{
						headPtr = head.data();
						headSize = (uint32)head.size();
						middlePtr = middle.data();
						middleSize = (uint32)middle.size();
						tailPtr = tail.data();
						tailSize = (uint32)tail.size();
					};

					template<typename T>
					T ReadType()
					{
						cemu_assert(!m_isOutput);
						T v;
						memcpy((uint8_t*)&v, headPtr, headSize);
						memcpy((uint8_t*)&v + headSize, middlePtr, middleSize);
						memcpy((uint8_t*)&v + headSize + middleSize, tailPtr, tailSize);
						return v;
					}

					void WriteData(void* data, size_t size)
					{
						if (size == 0)
							return;
						cemu_assert(m_isOutput);
						cemu_assert((headSize + middleSize + tailSize) >= size);
						size_t bytesToCopy = std::min<size_t>(size, headSize);
						memcpy(headPtr, data, bytesToCopy);
						size -= bytesToCopy;
						if (size > 0)
						{
							bytesToCopy = std::min<size_t>(size, middleSize);
							memcpy(middlePtr, (uint8_t*)data + headSize, bytesToCopy);
							size -= bytesToCopy;
						}
						if (size > 0)
						{
							bytesToCopy = std::min<size_t>(size, tailSize);
							memcpy(tailPtr, (uint8_t*)data + headSize + middleSize, bytesToCopy);
						}
					}

					size_t GetSize() const
					{
						return headSize + middleSize + tailSize;
					}

				private:
					void* headPtr;
					uint32 headSize;
					void* middlePtr; // aligned
					uint32 middleSize;
					void* tailPtr;
					uint32 tailSize;
					bool m_isOutput;
				};

				IPCServiceCall(IOSDevHandle clientHandle, uint32 serviceId, uint32 commandId) : m_clientHandle(clientHandle), m_serviceId(serviceId), m_commandId(commandId)
				{
				}

				void AddInputStream(void* data, uint32 size)
				{
					cemu_assert(m_paramStreamArraySize < 4);
					m_paramStreamArray[m_paramStreamArraySize++] = IPCParameterStream(data, size);
				}

				void AddOutputStream(void* data, uint32 size)
				{
					cemu_assert(m_responseStreamArraySize < 4);
					m_responseStreamArray[m_responseStreamArraySize++] = IPCResponseStream(data, size);
				}

				uint32 GetServiceId() const
				{
					return m_serviceId;
				}

				uint32 GetCommandId() const
				{
					return m_commandId;
				}

				template<typename T>
				T ReadParameter()
				{
					// read only from stream 0 for now
					return m_paramStreamArray[0].ReadParameter<T>(m_hasError);
				}

				UnalignedBuffer ReadUnalignedInputBufferInfo()
				{
					// how large/unaligned buffers work:
					// Instead of appending the data into the parameter stream, there are two separate buffers created:
					// 1. A ioctl vector that points directly to the aligned part of the original buffer. Both the pointer and the size are aligned
					// 2. A ioctl vector with an allocated up-to-128byte buffer that holds any unaligned head or tail data (e.g. anything that isn't occupying the full 64 byte alignment)
					// The buffer layout is then also serialized into the parameter stream
					LargeBufferHeader header = ReadParameter<LargeBufferHeader>();
					// get aligned buffer part
					void* alignedBuffer = m_paramStreamArray[m_inputBufferIndex+0].GetData();
					cemu_assert(m_paramStreamArray[m_inputBufferIndex+0].GetSize() == header.alignedSize);
					// get head and tail buffer parts
					uint8* unalignedDataBuffer = m_paramStreamArray[m_inputBufferIndex+1].GetData();
					cemu_assert((header.headSize + header.tailSize) <= m_paramStreamArray[m_inputBufferIndex+1].GetSize());
					UnalignedBuffer largeBuffer(false, {(uint8*)unalignedDataBuffer, header.headSize}, {(uint8*)alignedBuffer, header.alignedSize}, {(uint8*)unalignedDataBuffer + header.headSize, header.tailSize});
					m_inputBufferIndex += 2; // if there is no unaligned data then are both buffers still present?
					return largeBuffer;
				}

				UnalignedBuffer ReadUnalignedOutputBufferInfo()
				{
					LargeBufferHeader header = ReadParameter<LargeBufferHeader>();
					// get aligned buffer part
					void* alignedBuffer = m_responseStreamArray[m_outputBufferIndex+0].GetData();
					cemu_assert(m_responseStreamArray[m_outputBufferIndex+0].GetSize() == header.alignedSize);
					// get head and tail buffer parts
					uint8* unalignedDataBuffer = m_responseStreamArray[m_outputBufferIndex+1].GetData();
					cemu_assert((header.headSize + header.tailSize) <= m_responseStreamArray[m_outputBufferIndex+1].GetSize());
					UnalignedBuffer largeBuffer(true, {(uint8*)unalignedDataBuffer, header.headSize}, {(uint8*)alignedBuffer, header.alignedSize}, {(uint8*)unalignedDataBuffer + header.headSize, header.tailSize});
					m_outputBufferIndex += 2; // if there is no unaligned data then are both buffers still present?
					return largeBuffer;
				}

				template<typename T>
				void WriteResponse(const T& value)
				{
					m_responseStreamArray[0].Write<T>(value);
				}

			private:
				IOSDevHandle m_clientHandle;
				uint32 m_serviceId;
				uint32 m_commandId;
				IPCParameterStream m_paramStreamArray[4]{};
				size_t m_paramStreamArraySize{0};
				IPCResponseStream m_responseStreamArray[4]{};
				size_t m_responseStreamArraySize{0};
				bool m_hasError{false};
				sint8 m_inputBufferIndex{1};
				sint8 m_outputBufferIndex{1};
			};

			IPCService(std::string_view devicePath) : m_devicePath(devicePath) {};
			virtual ~IPCService() {};

			virtual IOSDevHandle CreateClientHandle()
			{
				return (IOSDevHandle)0;
			}

			virtual void CloseClientHandle(IOSDevHandle handle)
			{
				
			}

			virtual nnResult ServiceCall(IPCServiceCall& serviceCall)
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