#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Common/precompiled.h"
#include "Metal/MTLResource.hpp"

struct MetalBufferRange
{
    size_t offset;
    size_t size;
};

constexpr size_t BASE_ALLOCATION_SIZE = 8 * 1024 * 1024; // 8 MB
constexpr size_t MAX_ALLOCATION_SIZE = 64 * 1024 * 1024; // 64 MB

template<typename BufferT>
class MetalBufferAllocator
{
public:
    struct Buffer
    {
        MTL::Buffer* m_buffer;
        std::vector<MetalBufferRange> m_freeRanges;
        BufferT m_data;
    };

    MetalBufferAllocator(class MetalRenderer* metalRenderer, MTL::ResourceOptions storageMode) : m_mtlr{metalRenderer} {
        m_isCPUAccessible = (storageMode == MTL::ResourceStorageModeShared) || (storageMode == MTL::ResourceStorageModeManaged);

        m_options = storageMode;
        if (m_isCPUAccessible)
            m_options |= MTL::ResourceCPUCacheModeWriteCombined;
    }

    ~MetalBufferAllocator()
    {
        for (auto buffer : m_buffers)
        {
            buffer.m_buffer->release();
        }
    }

    void ResetAllocations()
    {
        for (uint32 i = 0; i < m_buffers.size(); i++)
            FreeBuffer(i);
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
        for (uint32 i = 0; i < m_buffers.size(); i++)
        {
            auto& buffer = m_buffers[i];
            for (uint32 j = 0; j < buffer.m_freeRanges.size(); j++)
            {
                auto& range = buffer.m_freeRanges[j];
                if (size <= range.size)
                {
                    MetalBufferAllocation allocation;
                    allocation.bufferIndex = i;
                    allocation.offset = range.offset;
                    allocation.size = size;
                    allocation.data = (m_isCPUAccessible ? (uint8*)buffer.m_buffer->contents() + range.offset : nullptr);

                    range.offset += size;
                    range.size -= size;

                    if (range.size == 0)
                    {
                        buffer.m_freeRanges.erase(buffer.m_freeRanges.begin() + j);
                    }

                    return allocation;
                }
            }
        }

        // If no free range was found, allocate a new buffer
        size_t allocationSize = BASE_ALLOCATION_SIZE * (1u << m_buffers.size());
        allocationSize = std::min(allocationSize, MAX_ALLOCATION_SIZE); // Limit the allocation size
        allocationSize = std::max(allocationSize, size);
        MTL::Buffer* mtlBuffer = m_mtlr->GetDevice()->newBuffer(allocationSize, m_options);
    #ifdef CEMU_DEBUG_ASSERT
        mtlBuffer->setLabel(GetLabel("Buffer from buffer allocator", mtlBuffer));
    #endif

        MetalBufferAllocation allocation;
        allocation.bufferIndex = m_buffers.size();
        allocation.offset = 0;
        allocation.size = size;
        allocation.data = (m_isCPUAccessible ? mtlBuffer->contents() : nullptr);

        m_buffers.push_back({mtlBuffer});
        auto& buffer = m_buffers.back();

        // If the buffer is larger than the requested size, add the remaining space to the free buffer ranges
        if (size < allocationSize)
        {
            MetalBufferRange range;
            range.offset = size;
            range.size = allocationSize - size;

            buffer.m_freeRanges.push_back(range);
        }

        // Debug
        m_mtlr->GetPerformanceMonitor().m_bufferAllocatorMemory += allocationSize;

        return allocation;
    }

    void FreeAllocation(MetalBufferAllocation& allocation)
    {
        MetalBufferRange range;
        range.offset = allocation.offset;
        range.size = allocation.size;

        allocation.offset = INVALID_OFFSET;

        // Find the correct position to insert the free range
        auto& buffer = m_buffers[allocation.bufferIndex];
        for (uint32 i = 0; i < buffer.m_freeRanges.size(); i++)
        {
            auto& freeRange = buffer.m_freeRanges[i];
            if (freeRange.offset + freeRange.size == range.offset)
            {
                freeRange.size += range.size;
                return;
            }
        }

        buffer.m_freeRanges.push_back(range);
    }

protected:
    class MetalRenderer* m_mtlr;
    bool m_isCPUAccessible;
    MTL::ResourceOptions m_options;

    std::vector<Buffer> m_buffers;

    void FreeBuffer(uint32 bufferIndex)
    {
        auto& buffer = m_buffers[bufferIndex];
        buffer.m_freeRanges.clear();
        buffer.m_freeRanges.reserve(1);
        buffer.m_freeRanges.push_back({0, m_buffers[bufferIndex].m_buffer->length()});
    }
};

struct Empty {};
typedef MetalBufferAllocator<Empty> MetalDefaultBufferAllocator;

struct MetalSyncedBuffer
{
    std::vector<MTL::CommandBuffer*> m_commandBuffers;
    uint32 m_lock = 0;

    bool IsLocked() const
    {
        return (m_lock != 0);
    }
};

constexpr uint16 BUFFER_RELEASE_FRAME_TRESHOLD = 1024;

class MetalTemporaryBufferAllocator : public MetalBufferAllocator<MetalSyncedBuffer>
{
public:
    MetalTemporaryBufferAllocator(class MetalRenderer* metalRenderer) : MetalBufferAllocator<MetalSyncedBuffer>(metalRenderer, MTL::ResourceStorageModeShared) {}

    void LockBuffer(uint32 bufferIndex)
    {
        m_buffers[bufferIndex].m_data.m_lock++;
    }

    void UnlockBuffer(uint32 bufferIndex)
    {
        auto& buffer = m_buffers[bufferIndex];

        buffer.m_data.m_lock--;

        // TODO: is this really necessary?
        // Release the buffer if it wasn't released due to the lock
        if (!buffer.m_data.IsLocked() && buffer.m_data.m_commandBuffers.empty())
            FreeBuffer(bufferIndex);
    }

    void EndFrame()
    {
        CheckForCompletedCommandBuffers();

        // Unlock all buffers
        for (uint32_t i = 0; i < m_buffers.size(); i++)
        {
            auto& buffer = m_buffers[i];

            if (buffer.m_data.m_lock != 0)
            {
                if (buffer.m_data.m_commandBuffers.empty())
                    FreeBuffer(i);

                buffer.m_data.m_lock = 0;
            }
        }

        // TODO: do this for other buffer allocators as well?
        // Track how many frames have passed since the last access to the back buffer
        if (!m_buffers.empty())
        {
            auto& backBuffer = m_buffers.back();
            if (backBuffer.m_data.m_commandBuffers.empty())
            {
                // Release the back buffer if it hasn't been accessed for a while
                if (m_framesSinceBackBufferAccess >= BUFFER_RELEASE_FRAME_TRESHOLD)
                {
                    // Debug
                    m_mtlr->GetPerformanceMonitor().m_bufferAllocatorMemory -= backBuffer.m_buffer->length();

                    backBuffer.m_buffer->release();
                    m_buffers.pop_back();

                    m_framesSinceBackBufferAccess = 0;
                }
                else
                {
                    m_framesSinceBackBufferAccess++;
                }
            }
            else
            {
                m_framesSinceBackBufferAccess = 0;
            }
        }
    }

    void SetActiveCommandBuffer(MTL::CommandBuffer* commandBuffer)
    {
        m_activeCommandBuffer = commandBuffer;
    }

    void CheckForCompletedCommandBuffers(/*MTL::CommandBuffer* commandBuffer, bool erase = true*/)
    {
        for (uint32_t i = 0; i < m_buffers.size(); i++)
        {
            auto& buffer = m_buffers[i];
            for (uint32_t j = 0; j < buffer.m_data.m_commandBuffers.size(); j++)
            {
                if (m_mtlr->CommandBufferCompleted(buffer.m_data.m_commandBuffers[j]))
                {
                    if (buffer.m_data.m_commandBuffers.size() == 1)
                    {
                        if (!buffer.m_data.IsLocked())
                        {
                            // All command buffers using it have finished execution, we can use it again
                            FreeBuffer(i);
                        }

                        buffer.m_data.m_commandBuffers.clear();
                        break;
                    }
                    else
                    {
                        buffer.m_data.m_commandBuffers.erase(buffer.m_data.m_commandBuffers.begin() + j);
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
        if (buffer.m_data.m_commandBuffers.empty() || buffer.m_data.m_commandBuffers.back() != m_activeCommandBuffer/*std::find(buffer.m_commandBuffers.begin(), buffer.m_commandBuffers.end(), m_activeCommandBuffer) == buffer.m_commandBuffers.end()*/)
            buffer.m_data.m_commandBuffers.push_back(m_activeCommandBuffer);

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

    // For debugging
    /*
    void LogInfo()
    {
        debug_printf("BUFFERS:\n");
        for (auto& buffer : m_buffers)
        {
            debug_printf("  %p -> size: %lu, command buffers: %zu\n", buffer.m_buffer, buffer.m_buffer->length(), buffer.m_data.m_commandBuffers.size());
            uint32 same = 0;
            uint32 completed = 0;
            for (uint32 i = 0; i < buffer.m_data.m_commandBuffers.size(); i++)
            {
                if (m_mtlr->CommandBufferCompleted(buffer.m_data.m_commandBuffers[i]))
                    completed++;
                for (uint32 j = 0; j < buffer.m_data.m_commandBuffers.size(); j++)
                {
                    if (i != j && buffer.m_data.m_commandBuffers[i] == buffer.m_data.m_commandBuffers[j])
                        same++;
                }
            }
            debug_printf("  same: %u\n", same);
            debug_printf("  completed: %u\n", completed);

            debug_printf("  FREE RANGES:\n");
            for (auto& range : buffer.m_freeRanges)
            {
                debug_printf("    offset: %zu, size: %zu\n", range.offset, range.size);
            }
        }
    }
    */

private:
    MTL::CommandBuffer* m_activeCommandBuffer = nullptr;

    uint16 m_framesSinceBackBufferAccess = 0;
};