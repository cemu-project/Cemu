#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalBufferAllocator.h"
#include "Metal/MTLResource.hpp"

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
    MetalMemoryManager(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer}, m_bufferAllocator(metalRenderer, m_mtlr->GetOptimalBufferStorageMode()), m_framePersistentBufferAllocator(metalRenderer, MTL::ResourceStorageModeShared), m_tempBufferAllocator(metalRenderer), m_vertexBufferCache(metalRenderer, m_framePersistentBufferAllocator) {}
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
