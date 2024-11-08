#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalMemoryManager.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalVoidVertexPipeline.h"

#include "Cemu/Logging/CemuLogging.h"
#include "Common/precompiled.h"
#include "HW/MMU/MMU.h"

MetalMemoryManager::~MetalMemoryManager()
{
    if (m_bufferCache)
    {
        m_bufferCache->release();
    }
}

void* MetalMemoryManager::AcquireTextureUploadBuffer(size_t size)
{
    if (m_textureUploadBuffer.size() < size)
    {
        m_textureUploadBuffer.resize(size);
    }

    return m_textureUploadBuffer.data();
}

void MetalMemoryManager::ReleaseTextureUploadBuffer(uint8* mem)
{
    cemu_assert_debug(m_textureUploadBuffer.data() == mem);
	m_textureUploadBuffer.clear();
}

void MetalMemoryManager::InitBufferCache(size_t size)
{
    cemu_assert_debug(!m_bufferCache);

    m_bufferCacheMode = g_current_game_profile->GetBufferCacheMode();

    // First, try to import the host memory as a buffer
    if (m_bufferCacheMode == BufferCacheMode::Host)
    {
        if (m_mtlr->HasUnifiedMemory())
        {
            m_importedMemBaseAddress = mmuRange_MEM2.getBase();
           	m_hostAllocationSize = mmuRange_MEM2.getSize();
            m_bufferCache = m_mtlr->GetDevice()->newBuffer(memory_getPointerFromVirtualOffset(m_importedMemBaseAddress), m_hostAllocationSize, MTL::ResourceStorageModeShared, nullptr);
            if (!m_bufferCache)
            {
                cemuLog_log(LogType::Force, "Failed to import host memory as a buffer, using device shared mode instead");
                m_bufferCacheMode = BufferCacheMode::DeviceShared;
            }
        }
        else
        {
            cemuLog_log(LogType::Force, "Host buffer cache mode is only available on unified memory systems, using device shared mode instead");
            m_bufferCacheMode = BufferCacheMode::DeviceShared;
        }
    }

    if (!m_bufferCache)
        m_bufferCache = m_mtlr->GetDevice()->newBuffer(size, (m_bufferCacheMode == BufferCacheMode::DevicePrivate ? MTL::ResourceStorageModePrivate : MTL::ResourceStorageModeShared));

#ifdef CEMU_DEBUG_ASSERT
    m_bufferCache->setLabel(GetLabel("Buffer cache", m_bufferCache));
#endif
}

void MetalMemoryManager::UploadToBufferCache(const void* data, size_t offset, size_t size)
{
    cemu_assert_debug(m_bufferCacheMode != BufferCacheMode::Host);
    cemu_assert_debug(m_bufferCache);
    cemu_assert_debug((offset + size) <= m_bufferCache->length());

    if (m_bufferCacheMode == BufferCacheMode::DevicePrivate)
    {
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
    }
    else
    {
        memcpy((uint8*)m_bufferCache->contents() + offset, data, size);
    }
}

void MetalMemoryManager::CopyBufferCache(size_t srcOffset, size_t dstOffset, size_t size)
{
    cemu_assert_debug(m_bufferCacheMode != BufferCacheMode::Host);
    cemu_assert_debug(m_bufferCache);

    if (m_bufferCacheMode == BufferCacheMode::DevicePrivate)
        m_mtlr->CopyBufferToBuffer(m_bufferCache, srcOffset, m_bufferCache, dstOffset, size, ALL_MTL_RENDER_STAGES, ALL_MTL_RENDER_STAGES);
    else
        memcpy((uint8*)m_bufferCache->contents() + dstOffset, (uint8*)m_bufferCache->contents() + srcOffset, size);
}
