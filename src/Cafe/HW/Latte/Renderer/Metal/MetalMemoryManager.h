#pragma once

#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"

//const uint32 bufferAllocatorIndexShift = 24;

struct MetalBufferAllocation
{
    void* data;
    uint32 bufferIndex;
    size_t bufferOffset = INVALID_OFFSET;

    bool IsValid() const
    {
        return bufferOffset != INVALID_OFFSET;
    }
};

struct MetalBufferRange
{
    uint32 bufferIndex;
    size_t offset;
    size_t size;
};

class MetalBufferAllocator
{
public:
    MetalBufferAllocator(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}
    ~MetalBufferAllocator();

    void ResetTemporaryBuffers()
    {
        m_freeBufferRanges.clear();

        // Register the free ranges
        for (uint32 i = 0; i < m_buffers.size(); i++)
        {
            m_freeBufferRanges.push_back({i, 0, m_buffers[i]->length()});
        }
    }

    MTL::Buffer* GetBuffer(uint32 bufferIndex)
    {
        return m_buffers[bufferIndex];
    }

    MetalBufferAllocation GetBufferAllocation(size_t size, size_t alignment);

private:
    class MetalRenderer* m_mtlr;

    std::vector<MTL::Buffer*> m_buffers;
    std::vector<MetalBufferRange> m_freeBufferRanges;
};

struct MetalRestridedBufferRange
{
    MTL::Buffer* buffer;
    size_t offset;
};

// TODO: use one big buffer for all the restrided vertex buffers?
struct MetalRestrideInfo
{
    bool memoryInvalidated = true;
    size_t lastStride = 0;
    MetalBufferAllocation allocation{};
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

    MetalVertexBufferCache(class MetalRenderer* metalRenderer, MetalBufferAllocator* bufferAllocator) : m_mtlr{metalRenderer}, m_bufferAllocator{bufferAllocator} {}
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
        range.offset = INVALID_OFFSET;
    }

    MetalRestridedBufferRange RestrideBufferIfNeeded(MTL::Buffer* bufferCache, uint32 bufferIndex, size_t stride);

private:
    class MetalRenderer* m_mtlr;
    MetalBufferAllocator* m_bufferAllocator;

    class MetalHybridComputePipeline* m_restrideBufferPipeline = nullptr;

    MetalVertexBufferRange m_bufferRanges[LATTE_MAX_VERTEX_BUFFERS] = {};

    void MemoryRangeChanged(size_t offset, size_t size);
};

class MetalMemoryManager
{
public:
    MetalMemoryManager(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer}, m_bufferAllocator(metalRenderer), m_vertexBufferCache(metalRenderer, &m_bufferAllocator) {}
    ~MetalMemoryManager();

    // Pipelines
    void SetRestrideBufferPipeline(class MetalHybridComputePipeline* restrideBufferPipeline)
    {
        m_vertexBufferCache.SetRestrideBufferPipeline(restrideBufferPipeline);
    }

    void ResetTemporaryBuffers()
    {
        m_bufferAllocator/*s[m_bufferAllocatorIndex]*/.ResetTemporaryBuffers();
        //m_bufferAllocatorIndex = (m_bufferAllocatorIndex + 1) % 2;
    }

    MTL::Buffer* GetBuffer(uint32 bufferIndex)
    {
        //uint32 bufferAllocatorIndex = (bufferIndex >> bufferAllocatorIndexShift);

        return m_bufferAllocator/*s[bufferAllocatorIndex]*/.GetBuffer(bufferIndex);
    }

    MetalBufferAllocation GetBufferAllocation(size_t size, size_t alignment)
    {
        auto allocation = m_bufferAllocator/*s[m_bufferAllocatorIndex]*/.GetBufferAllocation(size, alignment);
        //allocation.bufferIndex |= (m_bufferAllocatorIndex << bufferAllocatorIndexShift);

        return allocation;
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

    MetalBufferAllocator m_bufferAllocator;//s[2];
    //uint8 m_bufferAllocatorIndex = 0;
    MetalVertexBufferCache m_vertexBufferCache;

    MTL::Buffer* m_bufferCache = nullptr;
};
