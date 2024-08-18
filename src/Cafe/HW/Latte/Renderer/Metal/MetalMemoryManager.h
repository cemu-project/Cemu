#pragma once

#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"

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
    MetalBufferAllocator(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}

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
        size = Align(size, 16);

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
        MTL::Buffer* buffer = m_mtlr->GetDevice()->newBuffer(m_allocationSize, MTL::ResourceStorageModeShared);
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
};

class MetalTemporaryBufferAllocator : public MetalBufferAllocator<MetalSyncedBuffer>
{
public:
    MetalTemporaryBufferAllocator(class MetalRenderer* metalRenderer) : MetalBufferAllocator<MetalSyncedBuffer>(metalRenderer) {}

    void SetActiveCommandBuffer(MTL::CommandBuffer* commandBuffer)
    {
        m_activeCommandBuffer = commandBuffer;
    }

    void CommandBufferFinished(MTL::CommandBuffer* commandBuffer)
    {
        for (uint32_t i = 0; i < m_buffers.size(); i++)
        {
            auto& buffer = m_buffers[i];
            for (uint32_t j = 0; j < buffer.m_commandBuffers.size(); j++)
            {
                if (commandBuffer == buffer.m_commandBuffers[j])
                {
                    if (buffer.m_commandBuffers.size() == 1)
                    {
                        // All command buffers using it have finished execution, we can use it again
                        m_freeBufferRanges.push_back({i, 0, buffer.m_buffer->length()});

                        buffer.m_commandBuffers.clear();
                    }
                    else
                    {
                        buffer.m_commandBuffers.erase(buffer.m_commandBuffers.begin() + j);
                    }
                    break;
                }
            }
        }
    }

    // TODO: should this be here? It's just to ensure safety
    MTL::Buffer* GetBuffer(uint32 bufferIndex)
    {
        auto& buffer = m_buffers[bufferIndex];
        if (buffer.m_commandBuffers.back() != m_activeCommandBuffer)
            buffer.m_commandBuffers.push_back(m_activeCommandBuffer);

        return buffer.m_buffer;
    }

    MetalBufferAllocation GetBufferAllocation(size_t size)
    {
        // TODO: remove this
        if (!m_activeCommandBuffer)
            throw std::runtime_error("No active command buffer when allocating a buffer!");

        auto allocation = MetalBufferAllocator<MetalSyncedBuffer>::GetBufferAllocation(size);

        auto& buffer = m_buffers[allocation.bufferIndex];
        if (buffer.m_commandBuffers.empty() || buffer.m_commandBuffers.back() != m_activeCommandBuffer)
            buffer.m_commandBuffers.push_back(m_activeCommandBuffer);

        return allocation;
    }

private:
    MTL::CommandBuffer* m_activeCommandBuffer = nullptr;
};

struct MetalRestridedBufferRange
{
    MTL::Buffer* buffer;
    size_t offset;
};

struct MetalVertexBufferRange
{
    size_t offset = INVALID_OFFSET;
    size_t size;
    MetalRestrideInfo* restrideInfo;
};

class MetalVertexBufferCache
{
public:
    friend class MetalMemoryManager;

    MetalVertexBufferCache(class MetalRenderer* metalRenderer, MetalDefaultBufferAllocator& bufferAllocator) : m_mtlr{metalRenderer}, m_bufferAllocator{bufferAllocator} {}
    ~MetalVertexBufferCache();

    void SetRestrideBufferPipeline(class MetalHybridComputePipeline* restrideBufferPipeline)
    {
        m_restrideBufferPipeline = restrideBufferPipeline;
    }

    void TrackVertexBuffer(uint32 bufferIndex, size_t offset, size_t size, MetalRestrideInfo* restrideInfo)
    {
        m_bufferRanges[bufferIndex] = MetalVertexBufferRange{offset, size, restrideInfo};
    }

    void UntrackVertexBuffer(uint32 bufferIndex)
    {
        auto& range = m_bufferRanges[bufferIndex];
        //if (range.restrideInfo->allocation.offset != INVALID_OFFSET)
        //    m_bufferAllocator.FreeAllocation(range.restrideInfo->allocation);
        range.offset = INVALID_OFFSET;
    }

    MetalRestridedBufferRange RestrideBufferIfNeeded(MTL::Buffer* bufferCache, uint32 bufferIndex, size_t stride);

private:
    class MetalRenderer* m_mtlr;
    MetalDefaultBufferAllocator& m_bufferAllocator;

    class MetalHybridComputePipeline* m_restrideBufferPipeline = nullptr;

    MetalVertexBufferRange m_bufferRanges[LATTE_MAX_VERTEX_BUFFERS] = {};

    void MemoryRangeChanged(size_t offset, size_t size);
};

class MetalMemoryManager
{
public:
    MetalMemoryManager(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer}, m_bufferAllocator(metalRenderer), m_framePersistentBufferAllocator(metalRenderer), m_tempBufferAllocator(metalRenderer), m_vertexBufferCache(metalRenderer, m_framePersistentBufferAllocator) {}
    ~MetalMemoryManager();

    // Pipelines
    void SetRestrideBufferPipeline(class MetalHybridComputePipeline* restrideBufferPipeline)
    {
        m_vertexBufferCache.SetRestrideBufferPipeline(restrideBufferPipeline);
    }

    MetalDefaultBufferAllocator& GetBufferAllocator()
    {
        return m_bufferAllocator;
    }

    MetalDefaultBufferAllocator& GetFramePersistentBufferAllocator()
    {
        return m_framePersistentBufferAllocator;
    }

    MetalTemporaryBufferAllocator& GetTemporaryBufferAllocator()
    {
        return m_tempBufferAllocator;
    }

    MTL::Buffer* GetBufferCache()
    {
        return m_bufferCache;
    }

    void* GetTextureUploadBuffer(size_t size);

    // Buffer cache
    void InitBufferCache(size_t size);
    void UploadToBufferCache(const void* data, size_t offset, size_t size);
    void CopyBufferCache(size_t srcOffset, size_t dstOffset, size_t size);

    // Vertex buffer cache
    void TrackVertexBuffer(uint32 bufferIndex, size_t offset, size_t size, MetalRestrideInfo* restrideInfo)
    {
        m_vertexBufferCache.TrackVertexBuffer(bufferIndex, offset, size, restrideInfo);
    }

    void UntrackVertexBuffer(uint32 bufferIndex)
    {
        m_vertexBufferCache.UntrackVertexBuffer(bufferIndex);
    }

    MetalRestridedBufferRange RestrideBufferIfNeeded(uint32 bufferIndex, size_t stride)
    {
        return m_vertexBufferCache.RestrideBufferIfNeeded(m_bufferCache, bufferIndex, stride);
    }

private:
    class MetalRenderer* m_mtlr;

    std::vector<uint8> m_textureUploadBuffer;

    MetalDefaultBufferAllocator m_bufferAllocator;
    MetalDefaultBufferAllocator m_framePersistentBufferAllocator;
    MetalTemporaryBufferAllocator m_tempBufferAllocator;
    MetalVertexBufferCache m_vertexBufferCache;

    MTL::Buffer* m_bufferCache = nullptr;
};
