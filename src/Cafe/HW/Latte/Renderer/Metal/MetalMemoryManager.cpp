#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalMemoryManager.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Metal/MTLResource.hpp"

const size_t BUFFER_ALLOCATION_SIZE = 8 * 1024 * 1024;

MetalBufferAllocator::~MetalBufferAllocator()
{
    for (auto buffer : m_buffers)
    {
        buffer->release();
    }
}

MetalBufferAllocation MetalBufferAllocator::GetBufferAllocation(size_t size, size_t alignment)
{
    // Align the size
    size = align(size, alignment);

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

MetalVertexBufferCache::~MetalVertexBufferCache()
{
    for (uint32 i = 0; i < LATTE_MAX_VERTEX_BUFFERS; i++)
    {
        auto vertexBufferRange = m_bufferRanges[i];
        if (vertexBufferRange)
        {
            if (vertexBufferRange->restrideInfo.buffer)
            {
                vertexBufferRange->restrideInfo.buffer->release();
            }
        }
    }
}

MetalRestridedBufferRange MetalVertexBufferCache::RestrideBufferIfNeeded(MTL::Buffer* bufferCache, uint32 bufferIndex, size_t stride)
{
    auto vertexBufferRange = m_bufferRanges[bufferIndex];
    auto& restrideInfo = vertexBufferRange->restrideInfo;

    if (stride % 4 == 0)
    {
        // No restride needed
        return {bufferCache, vertexBufferRange->offset};
    }

    if (restrideInfo.memoryInvalidated || stride != restrideInfo.lastStride)
    {
        // TODO: use compute/void vertex function instead
        size_t newStride = align(stride, 4);
        size_t newSize = vertexBufferRange->size / stride * newStride;
        restrideInfo.buffer = m_mtlr->GetDevice()->newBuffer(newSize, MTL::StorageModeShared);

        uint8* oldPtr = (uint8*)bufferCache->contents() + vertexBufferRange->offset;
        uint8* newPtr = (uint8*)restrideInfo.buffer->contents();

        for (size_t elem = 0; elem < vertexBufferRange->size / stride; elem++)
    	{
    		memcpy(newPtr + elem * newStride, oldPtr + elem * stride, stride);
    	}

        restrideInfo.memoryInvalidated = false;
        restrideInfo.lastStride = newStride;
    }

    return {restrideInfo.buffer, 0};
}

void MetalVertexBufferCache::MemoryRangeChanged(size_t offset, size_t size)
{
    for (uint32 i = 0; i < LATTE_MAX_VERTEX_BUFFERS; i++)
    {
        auto vertexBufferRange = m_bufferRanges[i];
        if (vertexBufferRange)
        {
            if ((offset < vertexBufferRange->offset && (offset + size) < (vertexBufferRange->offset + vertexBufferRange->size)) ||
                (offset > vertexBufferRange->offset && (offset + size) > (vertexBufferRange->offset + vertexBufferRange->size)))
            {
                continue;
            }

            vertexBufferRange->restrideInfo.memoryInvalidated = true;
        }
    }
}

MetalMemoryManager::~MetalMemoryManager()
{
    if (m_bufferCache)
    {
        m_bufferCache->release();
    }
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
        debug_printf("MetalMemoryManager::InitBufferCache: buffer cache already initialized\n");
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
        debug_printf("MetalMemoryManager::UploadToBufferCache: buffer cache not initialized\n");
        return;
    }

    memcpy((uint8*)m_bufferCache->contents() + offset, data, size);

    // Notify vertex buffer cache about the change
    m_vertexBufferCache.MemoryRangeChanged(offset, size);
}

void MetalMemoryManager::CopyBufferCache(size_t srcOffset, size_t dstOffset, size_t size)
{
    if (!m_bufferCache)
    {
        debug_printf("MetalMemoryManager::CopyBufferCache: buffer cache not initialized\n");
        return;
    }

    memcpy((uint8*)m_bufferCache->contents() + dstOffset, (uint8*)m_bufferCache->contents() + srcOffset, size);
}
