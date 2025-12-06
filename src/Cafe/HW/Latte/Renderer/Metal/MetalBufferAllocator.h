#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Metal/MTLResource.hpp"
#include "util/ChunkedHeap/ChunkedHeap.h"
#include "util/helpers/MemoryPool.h"

#include <utility>

inline MTL::ResourceOptions GetResourceOptions(MTL::ResourceOptions options)
{
    if (options & MTL::ResourceStorageModeShared || options & MTL::ResourceStorageModeManaged)
        options |= MTL::ResourceCPUCacheModeWriteCombined;

    return options;
}

class MetalBufferChunkedHeap : private ChunkedHeap<>
{
  public:
	MetalBufferChunkedHeap(const class MetalRenderer* mtlRenderer, MTL::ResourceOptions options, size_t minimumBufferAllocationSize) : m_mtlr(mtlRenderer), m_options(GetResourceOptions(options)), m_minimumBufferAllocationSize(minimumBufferAllocationSize) { };
	~MetalBufferChunkedHeap();

	using ChunkedHeap::alloc;
	using ChunkedHeap::free;

	uint8* GetChunkPtr(uint32 index) const
	{
		if (index >= m_chunkBuffers.size())
			return nullptr;

		return (uint8*)m_chunkBuffers[index]->contents();
	}

	MTL::Buffer* GetBufferByIndex(uint32 index) const
    {
        cemu_assert_debug(index < m_chunkBuffers.size());

        return m_chunkBuffers[index];
    }

    bool RequiresFlush() const
    {
        return m_options & MTL::ResourceStorageModeManaged;
    }

	void GetStats(uint32& numBuffers, size_t& totalBufferSize, size_t& freeBufferSize) const
	{
		numBuffers = m_chunkBuffers.size();
		totalBufferSize = m_numHeapBytes;
		freeBufferSize = m_numHeapBytes - m_numAllocatedBytes;
	}

  private:
	uint32 allocateNewChunk(uint32 chunkIndex, uint32 minimumAllocationSize) override;

	const class MetalRenderer* m_mtlr;

	MTL::ResourceOptions m_options;
	size_t m_minimumBufferAllocationSize;

	std::vector<MTL::Buffer*> m_chunkBuffers;
};

// a circular ring-buffer which tracks and releases memory per command-buffer
class MetalSynchronizedRingAllocator
{
public:
	MetalSynchronizedRingAllocator(class MetalRenderer* mtlRenderer, MTL::ResourceOptions options, uint32 minimumBufferAllocSize) : m_mtlr(mtlRenderer), m_options(GetResourceOptions(options)), m_minimumBufferAllocSize(minimumBufferAllocSize) {};
	MetalSynchronizedRingAllocator(const MetalSynchronizedRingAllocator&) = delete; // disallow copy

	struct BufferSyncPoint_t
	{
		// todo - modularize sync point
		MTL::CommandBuffer* commandBuffer;
		uint32 offset;

		BufferSyncPoint_t(MTL::CommandBuffer* _commandBuffer, uint32 _offset) : commandBuffer(_commandBuffer), offset(_offset) {};
	};

	struct AllocatorBuffer_t
	{
		MTL::Buffer* mtlBuffer;
		uint8* basePtr;
		uint32 size;
		uint32 writeIndex;
		std::queue<BufferSyncPoint_t> queue_syncPoints;
		MTL::CommandBuffer* lastSyncpointCommandBuffer{ nullptr };
		uint32 index;
		uint32 cleanupCounter{ 0 }; // increased by one every time CleanupBuffer() is called if there is no sync point. If it reaches 300 then the buffer is released
	};

	struct AllocatorReservation_t
	{
		MTL::Buffer* mtlBuffer;
		uint8* memPtr;
		uint32 bufferOffset;
		uint32 size;
		uint32 bufferIndex;
	};

	AllocatorReservation_t AllocateBufferMemory(uint32 size, uint32 alignment);
	void FlushReservation(AllocatorReservation_t& uploadReservation);
	void CleanupBuffer(MTL::CommandBuffer* latestFinishedCommandBuffer);
	MTL::Buffer* GetBufferByIndex(uint32 index) const;

    bool RequiresFlush() const
    {
        return m_options & MTL::ResourceStorageModeManaged;
    }

	void GetStats(uint32& numBuffers, size_t& totalBufferSize, size_t& freeBufferSize) const;

private:
	void allocateAdditionalUploadBuffer(uint32 sizeRequiredForAlloc);
	void addUploadBufferSyncPoint(AllocatorBuffer_t& buffer, uint32 offset);

	const class MetalRenderer* m_mtlr;

	MTL::ResourceOptions m_options;
	const uint32 m_minimumBufferAllocSize;

	std::vector<AllocatorBuffer_t> m_buffers;
};

// heap style allocator with released memory being freed after the current command buffer finishes
class MetalSynchronizedHeapAllocator
{
	struct TrackedAllocation
	{
		TrackedAllocation(CHAddr allocation) : allocation(allocation) {};
		CHAddr allocation;
	};

  public:
	MetalSynchronizedHeapAllocator(class MetalRenderer* mtlRenderer, MTL::ResourceOptions options, size_t minimumBufferAllocSize) : m_mtlr(mtlRenderer), m_chunkedHeap(m_mtlr, options, minimumBufferAllocSize) {}
	MetalSynchronizedHeapAllocator(const MetalSynchronizedHeapAllocator&) = delete; // disallow copy

	struct AllocatorReservation
	{
		MTL::Buffer* mtlBuffer;
		uint8* memPtr;
		uint32 bufferOffset;
		uint32 size;
		uint32 bufferIndex;
	};

	AllocatorReservation* AllocateBufferMemory(uint32 size, uint32 alignment);
	void FreeReservation(AllocatorReservation* uploadReservation);
	void FlushReservation(AllocatorReservation* uploadReservation);

	void CleanupBuffer(MTL::CommandBuffer* latestFinishedCommandBuffer);

	void GetStats(uint32& numBuffers, size_t& totalBufferSize, size_t& freeBufferSize) const;
  private:
	const class MetalRenderer* m_mtlr;
	MetalBufferChunkedHeap m_chunkedHeap;
	// allocations
	std::vector<TrackedAllocation> m_activeAllocations;
	MemoryPool<AllocatorReservation> m_poolAllocatorReservation{32};
	// release queue
	std::unordered_map<MTL::CommandBuffer*, std::vector<CHAddr>> m_releaseQueue;
};
