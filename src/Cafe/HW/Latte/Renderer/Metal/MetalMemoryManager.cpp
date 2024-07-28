#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"

void* MetalMemoryManager::GetTextureUploadBuffer(size_t size)
{
    if (m_textureUploadBuffer.size() < size)
    {
        m_textureUploadBuffer.resize(size);
    }

    return m_textureUploadBuffer.data();
}

// TODO: optimize this
MetalBufferAllocation MetalMemoryManager::GetBufferAllocation(size_t size)
{
    MTL::Buffer* buffer = m_mtlr->GetDevice()->newBuffer(size, MTL::ResourceStorageModeShared);

    MetalBufferAllocation allocation;
    allocation.bufferIndex = m_buffers.size();
    allocation.bufferOffset = 0;
    allocation.data = buffer->contents();

    m_buffers.push_back(buffer);

    return allocation;
}

void MetalMemoryManager::InitBufferCache(size_t size)
{
    if (m_bufferCache)
    {
        printf("MetalMemoryManager::InitBufferCache: buffer cache already initialized\n");
        return;
    }

    m_bufferCache = m_mtlr->GetDevice()->newBuffer(size, MTL::ResourceStorageModeShared);
}

void MetalMemoryManager::UploadToBufferCache(const void* data, size_t offset, size_t size)
{
    if (!m_bufferCache)
    {
        printf("MetalMemoryManager::UploadToBufferCache: buffer cache not initialized\n");
        return;
    }

    memcpy((uint8*)m_bufferCache->contents() + offset, data, size);
}

void MetalMemoryManager::CopyBufferCache(size_t srcOffset, size_t dstOffset, size_t size)
{
    if (!m_bufferCache)
    {
        printf("MetalMemoryManager::CopyBufferCache: buffer cache not initialized\n");
        return;
    }

    memcpy((uint8*)m_bufferCache->contents() + dstOffset, (uint8*)m_bufferCache->contents() + srcOffset, size);
}
