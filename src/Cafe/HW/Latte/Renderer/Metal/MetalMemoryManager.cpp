#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalMemoryManager.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalHybridComputePipeline.h"
#include "Common/precompiled.h"
#include "Foundation/NSRange.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"

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
    size = Align(size, alignment);

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
#ifdef CEMU_DEBUG_ASSERT
    buffer->setLabel(GetLabel("Buffer from buffer allocator", buffer));
#endif

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
}

MetalRestridedBufferRange MetalVertexBufferCache::RestrideBufferIfNeeded(MTL::Buffer* bufferCache, uint32 bufferIndex, size_t stride)
{
    auto vertexBufferRange = m_bufferRanges[bufferIndex];
    auto& restrideInfo = *vertexBufferRange.restrideInfo;

    if (stride % 4 == 0)
    {
        // No restride needed
        return {bufferCache, vertexBufferRange.offset};
    }

    auto buffer = m_bufferAllocator->GetBuffer(restrideInfo.allocation.bufferIndex);
    if (restrideInfo.memoryInvalidated || stride != restrideInfo.lastStride)
    {
        size_t newStride = Align(stride, 4);
        size_t newSize = vertexBufferRange.size / stride * newStride;
        restrideInfo.allocation = m_bufferAllocator->GetBufferAllocation(newSize, 4);

        //uint8* oldPtr = (uint8*)bufferCache->contents() + vertexBufferRange.offset;
        //uint8* newPtr = (uint8*)restrideInfo.buffer->contents();

        //for (size_t elem = 0; elem < vertexBufferRange.size / stride; elem++)
    	//{
    	//	memcpy(newPtr + elem * newStride, oldPtr + elem * stride, stride);
    	//}
        //debug_printf("Restrided vertex buffer (old stride: %zu, new stride: %zu, old size: %zu, new size: %zu)\n", stride, newStride, vertexBufferRange.size, newSize);

        if (m_mtlr->GetEncoderType() == MetalEncoderType::Render)
        {
            auto renderCommandEncoder = static_cast<MTL::RenderCommandEncoder*>(m_mtlr->GetCommandEncoder());

            renderCommandEncoder->setRenderPipelineState(m_restrideBufferPipeline->GetRenderPipelineState());
            m_mtlr->GetEncoderState().m_renderPipelineState = m_restrideBufferPipeline->GetRenderPipelineState();

            MTL::Buffer* buffers[] = {bufferCache, buffer};
            size_t offsets[] = {vertexBufferRange.offset, restrideInfo.allocation.bufferOffset};
            renderCommandEncoder->setVertexBuffers(buffers, offsets, NS::Range(GET_HELPER_BUFFER_BINDING(0), 2));
            m_mtlr->GetEncoderState().m_uniformBufferOffsets[METAL_SHADER_TYPE_VERTEX][GET_HELPER_BUFFER_BINDING(0)] = INVALID_OFFSET;
            m_mtlr->GetEncoderState().m_uniformBufferOffsets[METAL_SHADER_TYPE_VERTEX][GET_HELPER_BUFFER_BINDING(1)] = INVALID_OFFSET;

            struct
            {
                uint32 oldStride;
                uint32 newStride;
            } strideData = {static_cast<uint32>(stride), static_cast<uint32>(newStride)};
            renderCommandEncoder->setVertexBytes(&strideData, sizeof(strideData), GET_HELPER_BUFFER_BINDING(2));
            m_mtlr->GetEncoderState().m_uniformBufferOffsets[METAL_SHADER_TYPE_VERTEX][GET_HELPER_BUFFER_BINDING(2)] = INVALID_OFFSET;

            renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypePoint, NS::UInteger(0), vertexBufferRange.size / stride);

            // TODO: do the barrier in one call?
            MTL::Resource* barrierBuffers[] = {buffer};
            renderCommandEncoder->memoryBarrier(barrierBuffers, 1, MTL::RenderStageVertex, MTL::RenderStageVertex);
        }
        else
        {
            debug_printf("vertex buffer restride needs an active render encoder\n");
            cemu_assert_suspicious();
        }

        restrideInfo.memoryInvalidated = false;
        restrideInfo.lastStride = newStride;
    }

    return {buffer, restrideInfo.allocation.bufferOffset};
}

void MetalVertexBufferCache::MemoryRangeChanged(size_t offset, size_t size)
{
    for (uint32 i = 0; i < LATTE_MAX_VERTEX_BUFFERS; i++)
    {
        auto vertexBufferRange = m_bufferRanges[i];
        if (vertexBufferRange.offset != INVALID_OFFSET)
        {
            if ((offset < vertexBufferRange.offset && (offset + size) < (vertexBufferRange.offset + vertexBufferRange.size)) ||
                (offset > vertexBufferRange.offset && (offset + size) > (vertexBufferRange.offset + vertexBufferRange.size)))
            {
                continue;
            }

            vertexBufferRange.restrideInfo->memoryInvalidated = true;
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
#ifdef CEMU_DEBUG_ASSERT
    m_bufferCache->setLabel(GetLabel("Buffer cache", m_bufferCache));
#endif
}

void MetalMemoryManager::UploadToBufferCache(const void* data, size_t offset, size_t size)
{
    if (!m_bufferCache)
    {
        debug_printf("MetalMemoryManager::UploadToBufferCache: buffer cache not initialized\n");
        return;
    }

    if ((offset + size) > m_bufferCache->length())
    {
        debug_printf("MetalMemoryManager::UploadToBufferCache: out of bounds access (offset: %zu, size: %zu, buffer size: %zu)\n", offset, size, m_bufferCache->length());
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
