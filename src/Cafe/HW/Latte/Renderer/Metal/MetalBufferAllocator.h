#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Common/precompiled.h"
#include "Metal/MTLResource.hpp"

struct MetalBufferRange
{
    uint32 bufferIndex;
    size_t offset;
    size_t size;
};

template<typename BufferT>
class MetalBufferAllocator
{
public:
    MetalBufferAllocator(class MetalRenderer* metalRenderer, MTL::ResourceOptions storageMode) : m_mtlr{metalRenderer}, m_storageMode{storageMode} {}

    ~MetalBufferAllocator()
    {
        for (auto buffer : m_buffers)
        {
            buffer.m_buffer->release();
        }
    }

    void ResetAllocations()
    {
        m_freeBufferRanges.clear();
        for (uint32_t i = 0; i < m_buffers.size(); i++)
            m_freeBufferRanges.push_back({i, 0, m_buffers[i].m_buffer->length()});
    }

    MTL::Buffer* GetBuffer(uint32 bufferIndex)
    {
        return m_buffers[bufferIndex].m_buffer;
    }

    MetalBufferAllocation GetBufferAllocation(size_t size)
    {
        // Align the size
        size = Align(size, 128);

        // First, try to find a free range
        for (uint32 i = 0; i < m_freeBufferRanges.size(); i++)
        {
            auto& range = m_freeBufferRanges[i];
            if (size <= range.size)
            {
                auto& buffer = m_buffers[range.bufferIndex];

                MetalBufferAllocation allocation;
                allocation.bufferIndex = range.bufferIndex;
                allocation.offset = range.offset;
                allocation.size = size;
                allocation.data = (uint8*)buffer.m_buffer->contents() + range.offset;

                range.offset += size;
                range.size -= size;

                if (range.size == 0)
                {
                    m_freeBufferRanges.erase(m_freeBufferRanges.begin() + i);
                }

                return allocation;
            }
        }

        // If no free range was found, allocate a new buffer
        m_allocationSize = std::max(m_allocationSize, size);
        MTL::Buffer* buffer = m_mtlr->GetDevice()->newBuffer(m_allocationSize, m_storageMode | MTL::ResourceCPUCacheModeWriteCombined);
    #ifdef CEMU_DEBUG_ASSERT
        buffer->setLabel(GetLabel("Buffer from buffer allocator", buffer));
    #endif

        MetalBufferAllocation allocation;
        allocation.bufferIndex = m_buffers.size();
        allocation.offset = 0;
        allocation.size = size;
        allocation.data = buffer->contents();

        m_buffers.push_back({buffer});

        // If the buffer is larger than the requested size, add the remaining space to the free buffer ranges
        if (size < m_allocationSize)
        {
            MetalBufferRange range;
            range.bufferIndex = allocation.bufferIndex;
            range.offset = size;
            range.size = m_allocationSize - size;

            m_freeBufferRanges.push_back(range);
        }

        // Debug
        m_mtlr->GetPerformanceMonitor().m_bufferAllocatorMemory += m_allocationSize;

        // Increase the allocation size for the next buffer
        if (m_allocationSize < 128 * 1024 * 1024)
            m_allocationSize *= 2;

        return allocation;
    }

    void FreeAllocation(MetalBufferAllocation& allocation)
    {
        MetalBufferRange range;
        range.bufferIndex = allocation.bufferIndex;
        range.offset = allocation.offset;
        range.size = allocation.size;

        allocation.offset = INVALID_OFFSET;

        // Find the correct position to insert the free range
        for (uint32 i = 0; i < m_freeBufferRanges.size(); i++)
        {
            auto& freeRange = m_freeBufferRanges[i];
            if (freeRange.bufferIndex == range.bufferIndex && freeRange.offset + freeRange.size == range.offset)
            {
                freeRange.size += range.size;
                return;
            }
        }

        m_freeBufferRanges.push_back(range);
    }

protected:
    class MetalRenderer* m_mtlr;
    MTL::ResourceOptions m_storageMode;

    size_t m_allocationSize = 8 * 1024 * 1024;

    std::vector<BufferT> m_buffers;
    std::vector<MetalBufferRange> m_freeBufferRanges;
};

struct MetalBuffer
{
    MTL::Buffer* m_buffer;
};

typedef MetalBufferAllocator<MetalBuffer> MetalDefaultBufferAllocator;

struct MetalSyncedBuffer
{
    MTL::Buffer* m_buffer;
    std::vector<MTL::CommandBuffer*> m_commandBuffers;
    uint32 m_lock = 0;

    bool IsLocked() const
    {
        return (m_lock != 0);
    }
};

constexpr uint16 MAX_COMMAND_BUFFER_FRAMES = 8;

class MetalTemporaryBufferAllocator : public MetalBufferAllocator<MetalSyncedBuffer>
{
public:
    MetalTemporaryBufferAllocator(class MetalRenderer* metalRenderer) : MetalBufferAllocator<MetalSyncedBuffer>(metalRenderer, MTL::ResourceStorageModeShared) {}

    void LockBuffer(uint32 bufferIndex)
    {
        m_buffers[bufferIndex].m_lock++;
    }

    void UnlockBuffer(uint32 bufferIndex)
    {
        auto& buffer = m_buffers[bufferIndex];

        buffer.m_lock--;

        // TODO: is this really necessary?
        // Release the buffer if it wasn't released due to the lock
        if (!buffer.IsLocked() && buffer.m_commandBuffers.empty())
            FreeBuffer(bufferIndex);
    }

    void UnlockAllBuffers()
    {
        for (uint32_t i = 0; i < m_buffers.size(); i++)
        {
            auto& buffer = m_buffers[i];

            if (buffer.m_lock != 0)
            {
                if (buffer.m_commandBuffers.empty())
                    FreeBuffer(i);

                buffer.m_lock = 0;
            }
        }

        /*
        auto it = m_commandBuffersFrames.begin();
        while (it != m_commandBuffersFrames.end())
        {
            it->second++;

            if (it->second > MAX_COMMAND_BUFFER_FRAMES)
            {
                debug_printf("command buffer %p remained unfinished for more than %u frames\n", it->first, MAX_COMMAND_BUFFER_FRAMES);

                // Pretend like the command buffer has finished
                CommandBufferFinished(it->first, false);

                it = m_commandBuffersFrames.erase(it);
            }
            else
            {
                it++;
            }
        }
        */
    }

    void SetActiveCommandBuffer(MTL::CommandBuffer* commandBuffer)
    {
        m_activeCommandBuffer = commandBuffer;

        //if (commandBuffer)
        //    m_commandBuffersFrames[commandBuffer] = 0;
    }

    void CheckForCompletedCommandBuffers(/*MTL::CommandBuffer* commandBuffer, bool erase = true*/)
    {
        for (uint32_t i = 0; i < m_buffers.size(); i++)
        {
            auto& buffer = m_buffers[i];
            for (uint32_t j = 0; j < buffer.m_commandBuffers.size(); j++)
            {
                if (m_mtlr->CommandBufferCompleted(buffer.m_commandBuffers[j]))
                {
                    if (buffer.m_commandBuffers.size() == 1)
                    {
                        if (!buffer.IsLocked())
                        {
                            // All command buffers using it have finished execution, we can use it again
                            FreeBuffer(i);
                        }

                        buffer.m_commandBuffers.clear();
                        break;
                    }
                    else
                    {
                        buffer.m_commandBuffers.erase(buffer.m_commandBuffers.begin() + j);
                        j--;
                    }
                }
            }
        }

        //if (erase)
        //    m_commandBuffersFrames.erase(commandBuffer);
    }

    MTL::Buffer* GetBuffer(uint32 bufferIndex)
    {
        cemu_assert_debug(m_activeCommandBuffer);

        auto& buffer = m_buffers[bufferIndex];
        if (buffer.m_commandBuffers.empty() || buffer.m_commandBuffers.back() != m_activeCommandBuffer/*std::find(buffer.m_commandBuffers.begin(), buffer.m_commandBuffers.end(), m_activeCommandBuffer) == buffer.m_commandBuffers.end()*/)
            buffer.m_commandBuffers.push_back(m_activeCommandBuffer);

        return buffer.m_buffer;
    }

    MTL::Buffer* GetBufferOutsideOfCommandBuffer(uint32 bufferIndex)
    {
        return m_buffers[bufferIndex].m_buffer;
    }

    /*
    MetalBufferAllocation GetBufferAllocation(size_t size)
    {
        if (!m_activeCommandBuffer)
            throw std::runtime_error("No active command buffer when allocating a buffer!");

        auto allocation = MetalBufferAllocator<MetalSyncedBuffer>::GetBufferAllocation(size);

        auto& buffer = m_buffers[allocation.bufferIndex];
        if (buffer.m_commandBuffers.empty() || buffer.m_commandBuffers.back() != m_activeCommandBuffer)
            buffer.m_commandBuffers.push_back(m_activeCommandBuffer);

        return allocation;
    }
    */

    /*
    void LogInfo()
    {
        debug_printf("BUFFERS:\n");
        for (auto& buffer : m_buffers)
        {
            debug_printf("  %p -> size: %lu, command buffers: %zu\n", buffer.m_buffer, buffer.m_buffer->length(), buffer.m_commandBuffers.size());
            uint32 same = 0;
            uint32 completed = 0;
            for (uint32 i = 0; i < buffer.m_commandBuffers.size(); i++)
            {
                if (m_mtlr->CommandBufferCompleted(buffer.m_commandBuffers[i]))
                    completed++;
                for (uint32 j = 0; j < buffer.m_commandBuffers.size(); j++)
                {
                    if (i != j && buffer.m_commandBuffers[i] == buffer.m_commandBuffers[j])
                        same++;
                }
            }
            debug_printf("  same: %u\n", same);
            debug_printf("  completed: %u\n", completed);
        }

        debug_printf("FREE RANGES:\n");
        for (auto& range : m_freeBufferRanges)
        {
            debug_printf("  %u -> offset: %zu, size: %zu\n", range.bufferIndex, range.offset, range.size);
        }
    }
    */

private:
    MTL::CommandBuffer* m_activeCommandBuffer = nullptr;

    //std::map<MTL::CommandBuffer*, uint16> m_commandBuffersFrames;

    void FreeBuffer(uint32 bufferIndex)
    {
        // First remove any free ranges that use this buffer
        for (uint32 k = 0; k < m_freeBufferRanges.size(); k++)
        {
            if (m_freeBufferRanges[k].bufferIndex == bufferIndex)
            {
                m_freeBufferRanges.erase(m_freeBufferRanges.begin() + k);
                k--;
            }
        }

        m_freeBufferRanges.push_back({bufferIndex, 0, m_buffers[bufferIndex].m_buffer->length()});
    }
};
