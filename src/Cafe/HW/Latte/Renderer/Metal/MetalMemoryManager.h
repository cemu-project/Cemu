#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalBufferAllocator.h"

#include "GameProfile/GameProfile.h"

class MetalMemoryManager
{
public:
    MetalMemoryManager(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer}, m_stagingAllocator(m_mtlr, m_mtlr->GetOptimalBufferStorageMode(), 32u * 1024 * 1024), m_indexAllocator(m_mtlr, m_mtlr->GetOptimalBufferStorageMode(), 4u * 1024 * 1024) {}
    ~MetalMemoryManager();

    MetalSynchronizedRingAllocator& GetStagingAllocator()
    {
        return m_stagingAllocator;
    }

    MetalSynchronizedHeapAllocator& GetIndexAllocator()
    {
        return m_indexAllocator;
    }

    MTL::Buffer* GetBufferCache()
    {
        return m_bufferCache;
    }

    void CleanupBuffers(MTL::CommandBuffer* latestFinishedCommandBuffer)
    {
        m_stagingAllocator.CleanupBuffer(latestFinishedCommandBuffer);
        m_indexAllocator.CleanupBuffer(latestFinishedCommandBuffer);
    }

    // Texture upload buffer
    void* AcquireTextureUploadBuffer(size_t size);
    void ReleaseTextureUploadBuffer(uint8* mem);

    // Buffer cache
    void InitBufferCache(size_t size);
    void UploadToBufferCache(const void* data, size_t offset, size_t size);
    void CopyBufferCache(size_t srcOffset, size_t dstOffset, size_t size);

    // Getters
    bool UseHostMemoryForCache() const
    {
        return (m_metalBufferCacheMode == MetalBufferCacheMode::Host);
    }

    bool NeedsReducedLatency() const
    {
        return (m_metalBufferCacheMode == MetalBufferCacheMode::DeviceShared || m_metalBufferCacheMode == MetalBufferCacheMode::Host);
    }

    MPTR GetImportedMemBaseAddress() const
    {
        return m_importedMemBaseAddress;
    }

    size_t GetHostAllocationSize() const
    {
        return m_hostAllocationSize;
    }

private:
    class MetalRenderer* m_mtlr;

    std::vector<uint8> m_textureUploadBuffer;

    MetalSynchronizedRingAllocator m_stagingAllocator;
    MetalSynchronizedHeapAllocator m_indexAllocator;

    MTL::Buffer* m_bufferCache = nullptr;
    MetalBufferCacheMode m_metalBufferCacheMode;
    MPTR m_importedMemBaseAddress;
    size_t m_hostAllocationSize = 0;
};
