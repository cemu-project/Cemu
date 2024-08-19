#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalMemoryManager.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalHybridComputePipeline.h"
#include "Common/precompiled.h"

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

    MTL::Buffer* buffer;
    if (restrideInfo.memoryInvalidated || stride != restrideInfo.lastStride)
    {
        size_t newStride = Align(stride, 4);
        size_t newSize = vertexBufferRange.size / stride * newStride;
        restrideInfo.allocation = m_bufferAllocator.GetBufferAllocation(newSize);
        buffer = m_bufferAllocator.GetBuffer(restrideInfo.allocation.bufferIndex);

        //uint8* oldPtr = (uint8*)bufferCache->contents() + vertexBufferRange.offset;
        //uint8* newPtr = (uint8*)buffer->contents() + restrideInfo.allocation.bufferOffset;

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
            size_t offsets[] = {vertexBufferRange.offset, restrideInfo.allocation.offset};
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

            // TODO: remove
            uint32 vertexCount = vertexBufferRange.size / stride;
            if (vertexCount * strideData.oldStride > buffers[0]->length() - offsets[0]) {
                throw std::runtime_error("Source buffer overflow (" + std::to_string(vertexCount) + " * " + std::to_string(strideData.oldStride) + " > " + std::to_string(buffers[0]->length()) + " - " + std::to_string(offsets[0]) + ")");
            }
            if (vertexCount * strideData.newStride > buffers[1]->length() - offsets[1]) {
                throw std::runtime_error("Destination buffer overflow (" + std::to_string(vertexCount) + " * " + std::to_string(strideData.newStride) + " > " + std::to_string(buffers[1]->length()) + " - " + std::to_string(offsets[1]) + ")");
            }

            renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), vertexBufferRange.size / stride);

            // TODO: do the barriers in one call?
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
    else
    {
        buffer = m_bufferAllocator.GetBuffer(restrideInfo.allocation.bufferIndex);
    }

    return {buffer, restrideInfo.allocation.offset};
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

    m_bufferCache = m_mtlr->GetDevice()->newBuffer(size, m_mtlr->GetOptimalBufferStorageMode() | MTL::ResourceCPUCacheModeWriteCombined);
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
    if (!m_mtlr->HasUnifiedMemory())
        m_bufferCache->didModifyRange(NS::Range(offset, size));

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
