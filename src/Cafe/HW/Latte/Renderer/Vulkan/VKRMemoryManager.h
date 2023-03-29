#pragma once
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "util/ChunkedHeap/ChunkedHeap.h"

struct VkImageMemAllocation
{
	VkImageMemAllocation(uint32 _typeFilter, CHAddr _mem, uint32 _allocationSize) : typeFilter(_typeFilter), mem(_mem), allocationSize(_allocationSize) { };

	uint32 typeFilter;
	CHAddr mem;
	uint32 allocationSize;

	uint32 getAllocationSize() { return allocationSize; }
};

class VkTextureChunkedHeap : private ChunkedHeap
{
public:
	VkTextureChunkedHeap(class VKRMemoryManager* memoryManager, uint32 typeFilter, VkDevice device) : m_vkrMemoryManager(memoryManager), m_typeFilter(typeFilter), m_device(device) { };

	struct ChunkInfo
	{
		VkDeviceMemory mem;
	};

	uint32 allocateNewChunk(uint32 chunkIndex, uint32 minimumAllocationSize) override;

	CHAddr allocMem(uint32 size, uint32 alignment)
	{
		if (alignment < 4)
			alignment = 4;
		if ((alignment & (alignment - 1)) != 0)
		{
			// not a power of two
			cemuLog_log(LogType::Force, "VkTextureChunkedHeap: Invalid alignment {}", alignment);
		}
		return this->alloc(size, alignment);
	}

	void freeMem(CHAddr addr)
	{
		this->free(addr);
	}

	void setDevice(VkDevice dev)
	{
		m_device = dev;
	}

	VkDeviceMemory getChunkMem(uint32 index)
	{
		if (index >= m_list_chunkInfo.size())
			return VK_NULL_HANDLE;
		return m_list_chunkInfo[index].mem;
	}

	void getStatistics(uint32& totalHeapSize, uint32& allocatedBytes) const
	{
		totalHeapSize = numHeapBytes;
		allocatedBytes = numAllocatedBytes;
	}

	VkDevice m_device;
	uint32 m_typeFilter{ 0xFFFFFFFF };
	class VKRMemoryManager* m_vkrMemoryManager;
	std::vector<ChunkInfo> m_list_chunkInfo;
};

// a circular ring-buffer which tracks and releases memory per command-buffer
class VKRSynchronizedRingAllocator
{
public:
	enum class BUFFER_TYPE
	{
		STAGING, // staging upload buffer
		INDEX, // buffer for index data
		STRIDE, // buffer for stride-adjusted vertex data
	};

	VKRSynchronizedRingAllocator(class VulkanRenderer* vkRenderer, class VKRMemoryManager* vkMemoryManager, BUFFER_TYPE bufferType, uint32 minimumBufferAllocSize) : m_vkr(vkRenderer), m_vkrMemMgr(vkMemoryManager), m_bufferType(bufferType), m_minimumBufferAllocSize(minimumBufferAllocSize) {};
	VKRSynchronizedRingAllocator(const VKRSynchronizedRingAllocator&) = delete; // disallow copy

	struct BufferSyncPoint_t
	{
		// todo - modularize sync point
		uint64 commandBufferId;
		uint32 offset;

		BufferSyncPoint_t(uint64 _commandBufferId, uint32 _offset) : commandBufferId(_commandBufferId), offset(_offset) {};
	};

	struct AllocatorBuffer_t
	{
		VkBuffer vk_buffer;
		VkDeviceMemory vk_mem;
		uint8* basePtr;
		uint32 size;
		uint32 writeIndex;
		std::queue<BufferSyncPoint_t> queue_syncPoints;
		uint64 lastSyncpointCmdBufferId{ 0xFFFFFFFFFFFFFFFFull };
		uint32 index;
		uint32 cleanupCounter{ 0 }; // increased by one every time CleanupBuffer() is called if there is no sync point. If it reaches 300 then the buffer is released
	};

	struct AllocatorReservation_t
	{
		VkBuffer vkBuffer;
		VkDeviceMemory vkMem;
		uint8* memPtr;
		uint32 bufferOffset;
		uint32 size;
		uint32 bufferIndex;
	};

	AllocatorReservation_t AllocateBufferMemory(uint32 size, uint32 alignment);
	void FlushReservation(AllocatorReservation_t& uploadReservation);
	void CleanupBuffer(uint64 latestFinishedCommandBufferId);
	VkBuffer GetBufferByIndex(uint32 index) const;

	void GetStats(uint32& numBuffers, size_t& totalBufferSize, size_t& freeBufferSize) const;

private:
	void allocateAdditionalUploadBuffer(uint32 sizeRequiredForAlloc);
	void addUploadBufferSyncPoint(AllocatorBuffer_t& buffer, uint32 offset);

	const class VulkanRenderer* m_vkr;
	const class VKRMemoryManager* m_vkrMemMgr;
	const BUFFER_TYPE m_bufferType;
	const uint32 m_minimumBufferAllocSize;

	std::vector<AllocatorBuffer_t> m_buffers;

};

void LatteIndices_invalidateAll();

class VKRMemoryManager
{
	friend class VKRSynchronizedRingAllocator;
public:
	VKRMemoryManager(class VulkanRenderer* renderer) :
			m_stagingBuffer(renderer, this, VKRSynchronizedRingAllocator::BUFFER_TYPE::STAGING, 32u * 1024 * 1024),
			m_indexBuffer(renderer, this, VKRSynchronizedRingAllocator::BUFFER_TYPE::INDEX, 4u * 1024 * 1024),
			m_vertexStrideMetalBuffer(renderer, this, VKRSynchronizedRingAllocator::BUFFER_TYPE::STRIDE, 4u * 1024 * 1024)
	{
		m_vkr = renderer;
	}

	// texture memory management
	std::unordered_map<uint32, VkTextureChunkedHeap*> map_textureHeap; // one heap per memory type
	std::vector<uint8> m_textureUploadBuffer;

	// texture upload buffer
	void* TextureUploadBufferAcquire(uint32 size)
	{
		if (m_textureUploadBuffer.size() < size)
			m_textureUploadBuffer.resize(size);

		return m_textureUploadBuffer.data();
	}

	void TextureUploadBufferRelease(uint8* mem)
	{
		cemu_assert_debug(m_textureUploadBuffer.data() == mem);
		m_textureUploadBuffer.clear();
	}

	VKRSynchronizedRingAllocator& getStagingAllocator() { return m_stagingBuffer; }; // allocator for texture/attribute/uniform uploads
	VKRSynchronizedRingAllocator& getIndexAllocator() { return m_indexBuffer; }; // allocator for index data
	VKRSynchronizedRingAllocator& getMetalStrideWorkaroundAllocator() { return m_vertexStrideMetalBuffer; }; // allocator for stride-adjusted vertex data

	void cleanupBuffers(uint64 latestFinishedCommandBufferId)
	{
		LatteIndices_invalidateAll();
		m_stagingBuffer.CleanupBuffer(latestFinishedCommandBufferId);
		m_indexBuffer.CleanupBuffer(latestFinishedCommandBufferId);
		m_vertexStrideMetalBuffer.CleanupBuffer(latestFinishedCommandBufferId);
	}

	// memory helpers
	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
	bool FindMemoryType2(uint32 typeFilter, VkMemoryPropertyFlags properties, uint32& memoryIndex) const; // searches for exact properties. Can gracefully fail without throwing exception (returns false)
	std::vector<uint32> FindMemoryTypes(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

	// image memory allocation
	void imageMemoryFree(VkImageMemAllocation* imageMemAllocation);
	VkImageMemAllocation* imageMemoryAllocate(VkImage image);

	// buffer management
	size_t GetTotalMemoryForBufferType(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, size_t minimumBufferSize = 16 * 1024 * 1024);

	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;
	bool CreateBuffer2(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const; // same as CreateBuffer but doesn't throw exception on failure
	bool CreateBufferFromHostMemory(void* hostPointer, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;

	void DeleteBuffer(VkBuffer& buffer, VkDeviceMemory& deviceMem) const;

	// overlay info
	void appendOverlayHeapDebugInfo();

	private:
		class VulkanRenderer* m_vkr;
		VKRSynchronizedRingAllocator m_stagingBuffer;
		VKRSynchronizedRingAllocator m_indexBuffer;
		VKRSynchronizedRingAllocator m_vertexStrideMetalBuffer;
};
