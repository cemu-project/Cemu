#pragma once

#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/ISA/LatteReg.h"

//const uint32 bufferAllocatorIndexShift = 24;

struct MetalBufferAllocation
{
    void* data;
    uint32 bufferIndex;
    size_t bufferOffset;
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

    MetalBufferAllocation GetBufferAllocation(size_t size);

private:
    class MetalRenderer* m_mtlr;

    std::vector<MTL::Buffer*> m_buffers;
    std::vector<MetalBufferRange> m_freeBufferRanges;
};

class MetalMemoryManager
{
public:
    MetalMemoryManager(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer}, m_bufferAllocator(metalRenderer) {}

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

    MetalBufferAllocation GetBufferAllocation(size_t size)
    {
        auto allocation = m_bufferAllocator/*s[m_bufferAllocatorIndex]*/.GetBufferAllocation(size);
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

private:
    class MetalRenderer* m_mtlr;

    std::vector<uint8> m_textureUploadBuffer;

    MetalBufferAllocator m_bufferAllocator;//s[2];
    //uint8 m_bufferAllocatorIndex = 0;

    MTL::Buffer* m_bufferCache = nullptr;
};
