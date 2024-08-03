#include "Cafe/HW/Latte/Renderer/Metal/MetalMemoryManager.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"

const size_t BUFFER_ALLOCATION_SIZE = 8 * 1024 * 1024;

MetalBufferAllocation MetalBufferAllocator::GetBufferAllocation(size_t size, size_t alignment)
{
    // Align the size
    size = (size + alignment - 1) & ~(alignment - 1);

    // First, try to find a free range
    for (uint32 i = 0; i < m_freeBufferRanges.size(); i++)
    {
        auto& range = m_freeBufferRanges[i];
        if (size <= range.size)
        {
            MetalBufferAllocation allocation;
            allocation.bufferIndex = range.bufferIndex;
            allocation.bufferOffset = range.offset;
            allocation.data = (uint8*)m_buffers[range.bufferIndex]->contents() + range.offset;

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
    MTL::Buffer* buffer = m_mtlr->GetDevice()->newBuffer(std::max(size, BUFFER_ALLOCATION_SIZE), MTL::ResourceStorageModeShared);

    MetalBufferAllocation allocation;
    allocation.bufferIndex = m_buffers.size();
    allocation.bufferOffset = 0;
    allocation.data = buffer->contents();

    m_buffers.push_back(buffer);

    // If the buffer is larger than the requested size, add the remaining space to the free buffer ranges
    if (size < BUFFER_ALLOCATION_SIZE)
    {
        MetalBufferRange range;
        range.bufferIndex = allocation.bufferIndex;
        range.offset = size;
        range.size = BUFFER_ALLOCATION_SIZE - size;

        m_freeBufferRanges.push_back(range);
    }

    return allocation;
}

void* MetalMemoryManager::GetTextureUploadBuffer(size_t size)
{
    if (m_textureUploadBuffer.size() < size)
    {
        m_textureUploadBuffer.resize(size);
    }

    return m_textureUploadBuffer.data();
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
    if ((offset + size) > m_bufferCache->length())
    {
        throw std::runtime_error(std::to_string(offset) + " + " + std::to_string(size) + " > " + std::to_string(m_bufferCache->length()));
    }

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
