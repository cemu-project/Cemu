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
