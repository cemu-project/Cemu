#pragma once

#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/ISA/LatteReg.h"

struct MetalBufferAllocation
{
    void* data;
    uint32 bufferIndex;
    size_t bufferOffset;
};

class MetalMemoryManager
{
public:
    MetalMemoryManager(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}

    MTL::Buffer* GetBuffer(uint32 bufferIndex)
    {
        return m_buffers[bufferIndex];
    }

    MTL::Buffer* GetBufferCache()
    {
        return m_bufferCache;
    }

    void* GetTextureUploadBuffer(size_t size);

    MetalBufferAllocation GetBufferAllocation(size_t size);

    // Buffer cache
    void InitBufferCache(size_t size);
    void UploadToBufferCache(const void* data, size_t offset, size_t size);
    void CopyBufferCache(size_t srcOffset, size_t dstOffset, size_t size);

private:
    class MetalRenderer* m_mtlr;

    std::vector<uint8> m_textureUploadBuffer;
    std::vector<MTL::Buffer*> m_buffers;
    MTL::Buffer* m_bufferCache = nullptr;
};
