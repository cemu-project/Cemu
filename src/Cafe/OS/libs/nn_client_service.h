#pragma once
#include "Cafe/OS/libs/coreinit/coreinit_IPC.h"
#include <boost/container/static_vector.hpp>

class IPCServiceClient
{
public:
	class IPCServiceCall
	{
		struct IOVectorBuffer
		{
			IOVectorBuffer() = default;
			IOVectorBuffer(uint8* ptr, uint32 size, bool isExternalBuffer = false) : ptr(ptr), size(size), isExternalBuffer(isExternalBuffer) {}

			uint8* ptr;
			uint32 size;
			bool isExternalBuffer{false}; // buffer provided by caller
		};
	public:
		IPCServiceCall(IPCServiceClient& client, uint32 serviceId, uint32 commandId) : m_client(client)
		{
			// allocate a parameter and response buffer
			IPCBuffer* cmdBuf = client.AllocateCommandBuffer();
			m_paramBuffers.emplace_back(cmdBuf->data, sizeof(IPCBuffer));
			cmdBuf = client.AllocateCommandBuffer();
			m_responseBuffers.emplace_back(cmdBuf->data, sizeof(IPCBuffer));
			// write the request header
			WriteParam<uint32be>(0); // ukn00
			WriteParam<uint32be>(serviceId); // serviceId
			WriteParam<uint32be>(0); // ukn08
			WriteParam<uint32be>(commandId); // commandId
		}

		~IPCServiceCall()
		{
			for (auto& buf : m_paramBuffers)
			{
				if (buf.isExternalBuffer)
					continue;
				m_client.ReleaseCommandBuffer((IPCBuffer*)buf.ptr);
			}
			for (auto& buf : m_responseBuffers)
			{
				if (buf.isExternalBuffer)
					continue;
				m_client.ReleaseCommandBuffer((IPCBuffer*)buf.ptr);
			}
		}

		IPCServiceCall(const IPCServiceCall&) = delete;
		IPCServiceCall& operator=(const IPCServiceCall&) = delete;

		template<typename T>
		void WriteParam(const T& value)
		{
			cemu_assert(m_paramWriteIndex + sizeof(T) <= m_paramBuffers[0].size);
			memcpy(m_paramBuffers[0].ptr + m_paramWriteIndex, &value, sizeof(T));
			m_paramWriteIndex += sizeof(T);
		}

		// ptr and size defines an input buffer (PPC->IOSU)
		void WriteParamBuffer(MEMPTR<void> ptr, uint32 size)
		{
			WriteInOutBuffer(MEMPTR<uint8>(ptr), size, false);
		}

		// ptr and size defines an output buffer (IOSU->PPC)
		void WriteResponseBuffer(MEMPTR<void> ptr, uint32 size)
		{
			WriteInOutBuffer(MEMPTR<uint8>(ptr), size, true);
		}

		nnResult Submit()
		{
			StackAllocator<IPCIoctlVector, 16> vectorArray;
			uint32 ioVecCount = m_paramBuffers.size() + m_responseBuffers.size();
			cemu_assert(ioVecCount <= 16);
			// output buffers come first
			for (size_t i = 0; i < m_responseBuffers.size(); ++i)
			{
				vectorArray[i].baseVirt = MEMPTR<uint8>(m_responseBuffers[i].ptr);
				vectorArray[i].basePhys = nullptr;
				vectorArray[i].size = m_responseBuffers[i].size;
			}
			// input buffers
			for (size_t i = 0; i < m_paramBuffers.size(); ++i)
			{
				vectorArray[m_responseBuffers.size() + i].baseVirt = MEMPTR<uint8>(m_paramBuffers[i].ptr);
				vectorArray[m_responseBuffers.size() + i].basePhys = nullptr;
				vectorArray[m_responseBuffers.size() + i].size = m_paramBuffers[i].size;
			}
			IOS_ERROR r = coreinit::IOS_Ioctlv(m_client.GetDevHandle(), 0, m_responseBuffers.size(), m_paramBuffers.size(), vectorArray.GetPointer());
			if ( (r&0x80000000) != 0)
			{
				cemu_assert_unimplemented(); // todo - handle submission errors
			}
			uint32be resultCode = ReadResponse<uint32be>();
			if (!NN_RESULT_IS_FAILURE((uint32)resultCode))
			{
				for (auto& bufCopy : m_bufferCopiesOut)
				{
					memcpy(bufCopy.dst, bufCopy.src, bufCopy.size);
				}
			}
			return static_cast<nnResult>(resultCode);
		}

		template<typename T>
		T ReadResponse()
		{
			cemu_assert(m_responseReadIndex + sizeof(T) <= m_responseBuffers[0].size);
			T value;
			memcpy(&value, m_responseBuffers[0].ptr + m_responseReadIndex, sizeof(T));
			m_responseReadIndex += sizeof(T);
			return value;
		}

	private:
		struct BufferCopyOut
		{
			BufferCopyOut(void* dst, void* src, uint32 size) : dst(dst), src(src), size(size) {}

			void* dst;
			void* src;
			uint32 size;
		};

		void WriteInOutBuffer(MEMPTR<uint8> ptr, uint32 size, bool isOutput)
		{
			uint32 headSize = (0x40 - (ptr.GetMPTR()&0x3F))&0x3F;
			headSize = std::min<uint32>(headSize, size);
			uint32 alignedSize = size - headSize;
			uint32 tailSize = alignedSize - (alignedSize&~0x3F);
			alignedSize -= tailSize;
			// verify
			cemu_assert_debug(headSize + alignedSize + tailSize == size);
			cemu_assert_debug(alignedSize == 0 || ((ptr.GetMPTR()+headSize)&0x3F) == 0);
			cemu_assert_debug(tailSize == 0 || ((ptr.GetMPTR()+headSize+alignedSize)&0x3F) == 0);

			if (isOutput)
				cemu_assert(m_responseBuffers.size()+2 <= m_responseBuffers.capacity());
			else
				cemu_assert(m_paramBuffers.size()+2 <= m_paramBuffers.capacity());
			IOVectorBuffer alignedBuffer;
			alignedBuffer.ptr = ptr + headSize;
			alignedBuffer.size = alignedSize;
			alignedBuffer.isExternalBuffer = true;
			if (isOutput)
				m_responseBuffers.emplace_back(alignedBuffer);
			else
				m_paramBuffers.emplace_back(alignedBuffer);
			IPCBuffer* headAndTailBuffer = m_client.AllocateCommandBuffer();
			IOVectorBuffer headAndTail;
			headAndTail.ptr = headAndTailBuffer->data + (0x40 - headSize);
			headAndTail.size = 128 - (0x40 - headSize);
			headAndTail.isExternalBuffer = false;
			if (headSize > 0)
			{
				if (isOutput)
					m_bufferCopiesOut.emplace_back(ptr, headAndTailBuffer->data + 0x40 - headSize, headSize);
				else
					memcpy(headAndTailBuffer->data + 0x40 - headSize, ptr, headSize);
			}
			if (tailSize > 0)
			{
				if (isOutput)
					m_bufferCopiesOut.emplace_back(ptr + headSize + alignedSize, headAndTailBuffer->data + 0x40, tailSize);
				else
					memcpy(headAndTailBuffer->data + 0x40, ptr + headSize + alignedSize, tailSize);
			}
			if (isOutput)
				m_responseBuffers.emplace_back(headAndTail);
			else
				m_paramBuffers.emplace_back(headAndTail);
			// serialize into parameter stream
			WriteParam<uint32be>(alignedSize);
			WriteParam<uint8be>(0); // ukn4
			WriteParam<uint8be>(0); // ukn5
			WriteParam<uint8be>((uint8)headSize);
			WriteParam<uint8be>((uint8)tailSize);
		}

		IPCServiceClient& m_client;
		boost::container::static_vector<IOVectorBuffer, 8> m_paramBuffers;
		boost::container::static_vector<IOVectorBuffer, 8> m_responseBuffers;
		sint32 m_paramWriteIndex{0};
		sint32 m_responseReadIndex{0};
		boost::container::static_vector<BufferCopyOut, 16> m_bufferCopiesOut;
	};

    IPCServiceClient()
    {
    }

    ~IPCServiceClient()
    {
    	Shutdown();
    }

	void Initialize(std::string_view devicePath, uint8_t* buffer, uint32_t bufferSize)
    {
    	m_devicePath = devicePath;
    	m_buffer = buffer;
    	m_bufferSize = bufferSize;

    	static_assert(sizeof(IPCBuffer) == 256);
    	size_t numCommandBuffers = m_bufferSize / sizeof(IPCBuffer);

    	m_commandBuffersFree.resize(numCommandBuffers);
    	for (size_t i = 0; i < numCommandBuffers; ++i)
    	{
    		m_commandBuffersFree[i] = reinterpret_cast<IPCBuffer*>(m_buffer + i * sizeof(IPCBuffer));
    	}
    	m_clientHandle = coreinit::IOS_Open(m_devicePath.c_str(), 0);
    }

    void Shutdown()
    {
        if (m_clientHandle != 0)
        {
	        coreinit::IOS_Close(m_clientHandle);
            m_clientHandle = 0;
        }
    }

    IPCServiceCall Begin(uint32_t serviceId, uint32_t commandId)
    {
        return IPCServiceCall(*this, serviceId, commandId);
    }

	IOSDevHandle GetDevHandle() const
	{
    	cemu_assert(m_clientHandle != 0);
		return m_clientHandle;
	}
private:
    struct IPCBuffer
    {
		uint8 data[256];
    };

    IPCBuffer* AllocateCommandBuffer()
    {
        cemu_assert(m_commandBuffersFree.size() > 0);
    	IPCBuffer* buf = m_commandBuffersFree.back();
    	m_commandBuffersFree.pop_back();
        return buf;
    }

    void ReleaseCommandBuffer(IPCBuffer* buffer)
    {
    	m_commandBuffersFree.emplace_back(buffer);
    }

private:
    std::string m_devicePath;
    IOSDevHandle m_clientHandle{0};
    uint8_t* m_buffer{nullptr};
    uint32_t m_bufferSize{0};
    std::vector<IPCBuffer*> m_commandBuffersFree;
};
