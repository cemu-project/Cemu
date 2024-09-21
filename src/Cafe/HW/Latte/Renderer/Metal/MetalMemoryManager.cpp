#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalMemoryManager.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalHybridComputePipeline.h"
#include "Common/precompiled.h"
#include "HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Metal/MTLResource.hpp"

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

        // HACK: the restriding is done on the CPU, since doing it on the GPU was causing over-synchronization
        uint8* oldPtr = (uint8*)bufferCache->contents() + vertexBufferRange.offset;
        uint8* newPtr = (uint8*)buffer->contents() + restrideInfo.allocation.offset;

        for (size_t elem = 0; elem < vertexBufferRange.size / stride; elem++)
    		memcpy(newPtr + elem * newStride, oldPtr + elem * stride, stride);

        /*
        if (m_mtlr->GetEncoderType() == MetalEncoderType::Render)
        {
            auto renderCommandEncoder = static_cast<MTL::RenderCommandEncoder*>(m_mtlr->GetCommandEncoder());

            renderCommandEncoder->setRenderPipelineState(m_restrideBufferPipeline->GetRenderPipelineState());
            m_mtlr->GetEncoderState().m_renderPipelineState = m_restrideBufferPipeline->GetRenderPipelineState();

            m_mtlr->SetBuffer(renderCommandEncoder, METAL_SHADER_TYPE_VERTEX, bufferCache, vertexBufferRange.offset, GET_HELPER_BUFFER_BINDING(0));
            m_mtlr->SetBuffer(renderCommandEncoder, METAL_SHADER_TYPE_VERTEX, buffer, restrideInfo.allocation.offset, GET_HELPER_BUFFER_BINDING(1));

            struct
            {
                uint32 oldStride;
                uint32 newStride;
            } strideData = {static_cast<uint32>(stride), static_cast<uint32>(newStride)};
            renderCommandEncoder->setVertexBytes(&strideData, sizeof(strideData), GET_HELPER_BUFFER_BINDING(2));
            m_mtlr->GetEncoderState().m_buffers[METAL_SHADER_TYPE_VERTEX][GET_HELPER_BUFFER_BINDING(2)] = {nullptr};

            renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), vertexBufferRange.size / stride);

            // TODO: do the barriers in one call?
            MTL::Resource* barrierBuffers[] = {buffer};
            renderCommandEncoder->memoryBarrier(barrierBuffers, 1, MTL::RenderStageVertex, MTL::RenderStageVertex);
        }
        else
        {
            debug_printf("vertex buffer restride needs an active render command encoder\n");
            cemu_assert_suspicious();
        }
        */

        restrideInfo.memoryInvalidated = false;
        restrideInfo.lastStride = newStride;

        // Debug
        m_mtlr->GetPerformanceMonitor().m_vertexBufferRestrides++;
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
    cemu_assert_debug(!m_bufferCache);

    m_bufferCache = m_mtlr->GetDevice()->newBuffer(size, MTL::ResourceStorageModeShared);
#ifdef CEMU_DEBUG_ASSERT
    m_bufferCache->setLabel(GetLabel("Buffer cache", m_bufferCache));
#endif
}

void MetalMemoryManager::UploadToBufferCache(const void* data, size_t offset, size_t size)
{
    cemu_assert_debug(m_bufferCache);
    cemu_assert_debug((offset + size) <= m_bufferCache->length());

    auto allocation = m_tempBufferAllocator.GetBufferAllocation(size);
    auto buffer = m_tempBufferAllocator.GetBufferOutsideOfCommandBuffer(allocation.bufferIndex);
    memcpy((uint8*)buffer->contents() + allocation.offset, data, size);

    // Lock the buffer to make sure it's not deallocated before the copy is done
    m_tempBufferAllocator.LockBuffer(allocation.bufferIndex);

    m_mtlr->CopyBufferToBuffer(buffer, allocation.offset, m_bufferCache, offset, size, ALL_MTL_RENDER_STAGES, ALL_MTL_RENDER_STAGES);

    // Make sure the buffer has the right command buffer
    m_tempBufferAllocator.GetBuffer(allocation.bufferIndex); // TODO: make a helper function for this

    // We can now safely unlock the buffer
    m_tempBufferAllocator.UnlockBuffer(allocation.bufferIndex);

    // Notify vertex buffer cache about the change
    m_vertexBufferCache.MemoryRangeChanged(offset, size);
}

void MetalMemoryManager::CopyBufferCache(size_t srcOffset, size_t dstOffset, size_t size)
{
    cemu_assert_debug(m_bufferCache);

    m_mtlr->CopyBufferToBuffer(m_bufferCache, srcOffset, m_bufferCache, dstOffset, size, ALL_MTL_RENDER_STAGES, ALL_MTL_RENDER_STAGES);
}
