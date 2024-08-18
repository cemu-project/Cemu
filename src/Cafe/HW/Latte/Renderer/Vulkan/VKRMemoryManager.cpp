#include "Cafe/HW/Latte/Renderer/Vulkan/VKRMemoryManager.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include <imgui.h>

/* VKRSynchronizedMemoryBuffer */

void VKRSynchronizedRingAllocator::addUploadBufferSyncPoint(AllocatorBuffer_t& buffer, uint32 offset)
{
	auto cmdBufferId = m_vkr->GetCurrentCommandBufferId();
	if (cmdBufferId == buffer.lastSyncpointCmdBufferId)
		return;
	buffer.lastSyncpointCmdBufferId = cmdBufferId;
	buffer.queue_syncPoints.emplace(cmdBufferId, offset);
}

void VKRSynchronizedRingAllocator::allocateAdditionalUploadBuffer(uint32 sizeRequiredForAlloc)
{
	// calculate buffer size, should be a multiple of bufferAllocSize that is at least as large as sizeRequiredForAlloc
	uint32 bufferAllocSize = m_minimumBufferAllocSize;
	while (bufferAllocSize < sizeRequiredForAlloc)
		bufferAllocSize += m_minimumBufferAllocSize;

	AllocatorBuffer_t newBuffer{};
	newBuffer.writeIndex = 0;
	newBuffer.basePtr = nullptr;
	if (m_bufferType == BUFFER_TYPE::STAGING)
		m_vkrMemMgr->CreateBuffer(bufferAllocSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, newBuffer.vk_buffer, newBuffer.vk_mem);
	else if (m_bufferType == BUFFER_TYPE::INDEX)
		m_vkrMemMgr->CreateBuffer(bufferAllocSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, newBuffer.vk_buffer, newBuffer.vk_mem);
	else if (m_bufferType == BUFFER_TYPE::STRIDE)
		m_vkrMemMgr->CreateBuffer(bufferAllocSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, newBuffer.vk_buffer, newBuffer.vk_mem);
	else
		cemu_assert_debug(false);

	void* bufferPtr = nullptr;
	vkMapMemory(m_vkr->GetLogicalDevice(), newBuffer.vk_mem, 0, VK_WHOLE_SIZE, 0, &bufferPtr);
	newBuffer.basePtr = (uint8*)bufferPtr;
	newBuffer.size = bufferAllocSize;
	newBuffer.index = (uint32)m_buffers.size();
	m_buffers.push_back(newBuffer);
}

VKRSynchronizedRingAllocator::AllocatorReservation_t VKRSynchronizedRingAllocator::AllocateBufferMemory(uint32 size, uint32 alignment)
{
	if (alignment < 128)
		alignment = 128;
	size = (size + 127) & ~127;

	for (auto& itr : m_buffers)
	{
		// align pointer
		uint32 alignmentPadding = (alignment - (itr.writeIndex % alignment)) % alignment;
		uint32 distanceToSyncPoint;
		if (!itr.queue_syncPoints.empty())
		{
			if(itr.queue_syncPoints.front().offset < itr.writeIndex)
				distanceToSyncPoint = 0xFFFFFFFF;
			else
				distanceToSyncPoint = itr.queue_syncPoints.front().offset - itr.writeIndex;
		}
		else
			distanceToSyncPoint = 0xFFFFFFFF;
		uint32 spaceNeeded = alignmentPadding + size;
		if (spaceNeeded > distanceToSyncPoint)
			continue; // not enough space in current buffer
		if ((itr.writeIndex + spaceNeeded) > itr.size)
		{
			// wrap-around
			spaceNeeded = size;
			alignmentPadding = 0;
			// check if there is enough space in current buffer after wrap-around
			if (!itr.queue_syncPoints.empty())
			{
				distanceToSyncPoint = itr.queue_syncPoints.front().offset - 0;
				if (spaceNeeded > distanceToSyncPoint)
					continue;
			}
			else if (spaceNeeded > itr.size)
				continue;
			itr.writeIndex = 0;
		}
		addUploadBufferSyncPoint(itr, itr.writeIndex);
		itr.writeIndex += alignmentPadding;
		uint32 offset = itr.writeIndex;
		itr.writeIndex += size;
		itr.cleanupCounter = 0;
		VKRSynchronizedRingAllocator::AllocatorReservation_t res;
		res.vkBuffer = itr.vk_buffer;
		res.vkMem = itr.vk_mem;
		res.memPtr = itr.basePtr + offset;
		res.bufferOffset = offset;
		res.size = size;
		res.bufferIndex = itr.index;
		return res;
	}
	// allocate new buffer
	allocateAdditionalUploadBuffer(size);
	return AllocateBufferMemory(size, alignment);
}

void VKRSynchronizedRingAllocator::FlushReservation(AllocatorReservation_t& uploadReservation)
{
	cemu_assert_debug(m_bufferType == BUFFER_TYPE::STAGING); // only the staging buffer isn't coherent
	// todo - use nonCoherentAtomSize for flush size (instead of hardcoded constant)
	VkMappedMemoryRange flushedRange{};
	flushedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	flushedRange.memory = uploadReservation.vkMem;
	flushedRange.offset = uploadReservation.bufferOffset;
	flushedRange.size = uploadReservation.size;
	vkFlushMappedMemoryRanges(m_vkr->GetLogicalDevice(), 1, &flushedRange);
}

void VKRSynchronizedRingAllocator::CleanupBuffer(uint64 latestFinishedCommandBufferId)
{
	if (latestFinishedCommandBufferId > 1)
		latestFinishedCommandBufferId -= 1;

	for (auto& itr : m_buffers)
	{
		while (!itr.queue_syncPoints.empty() && latestFinishedCommandBufferId > itr.queue_syncPoints.front().commandBufferId)
		{
			itr.queue_syncPoints.pop();
		}
		if (itr.queue_syncPoints.empty())
			itr.cleanupCounter++;
	}

	// check if last buffer is available for deletion
	if (m_buffers.size() >= 2)
	{
		auto& lastBuffer = m_buffers.back();
		if (lastBuffer.cleanupCounter >= 1000)
		{
			// release buffer
			vkUnmapMemory(m_vkr->GetLogicalDevice(), lastBuffer.vk_mem);
			m_vkrMemMgr->DeleteBuffer(lastBuffer.vk_buffer, lastBuffer.vk_mem);
			m_buffers.pop_back();
		}
	}
}

VkBuffer VKRSynchronizedRingAllocator::GetBufferByIndex(uint32 index) const
{
	return m_buffers[index].vk_buffer;
}

void VKRSynchronizedRingAllocator::GetStats(uint32& numBuffers, size_t& totalBufferSize, size_t& freeBufferSize) const
{
	numBuffers = (uint32)m_buffers.size();
	totalBufferSize = 0;
	freeBufferSize = 0;
	for (auto& itr : m_buffers)
	{
		totalBufferSize += itr.size;
		// calculate free space in buffer
		uint32 distanceToSyncPoint;
		if (!itr.queue_syncPoints.empty())
		{
			if (itr.queue_syncPoints.front().offset < itr.writeIndex)
				distanceToSyncPoint = (itr.size - itr.writeIndex) + itr.queue_syncPoints.front().offset; // size with wrap-around
			else
				distanceToSyncPoint = itr.queue_syncPoints.front().offset - itr.writeIndex;
		}
		else
			distanceToSyncPoint = itr.size;
		freeBufferSize += distanceToSyncPoint;
	}
}

/* VkTextureChunkedHeap */

uint32 VkTextureChunkedHeap::allocateNewChunk(uint32 chunkIndex, uint32 minimumAllocationSize)
{
	cemu_assert_debug(m_list_chunkInfo.size() == chunkIndex);
	m_list_chunkInfo.resize(m_list_chunkInfo.size() + 1);

	// pad minimumAllocationSize to 32KB alignment
	minimumAllocationSize = (minimumAllocationSize + (32*1024-1)) & ~(32 * 1024 - 1);

	uint32 allocationSize = 1024 * 1024 * 128;
	if (chunkIndex == 0)
	{
		// make the first allocation smaller, this decreases wasted memory when there are textures that require specific flags (and thus separate heaps)
		allocationSize = 1024 * 1024 * 16;
	}
	if (allocationSize < minimumAllocationSize)
		allocationSize = minimumAllocationSize;
	// get available memory types/heaps
	std::vector<uint32> deviceLocalMemoryTypeIndices = m_vkrMemoryManager->FindMemoryTypes(m_typeFilter, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	std::vector<uint32> hostLocalMemoryTypeIndices = m_vkrMemoryManager->FindMemoryTypes(m_typeFilter, 0);
	// remove device local memory types from host local vector
	auto pred = [&deviceLocalMemoryTypeIndices](const uint32& v) ->bool
	{
		return std::find(deviceLocalMemoryTypeIndices.begin(), deviceLocalMemoryTypeIndices.end(), v) != deviceLocalMemoryTypeIndices.end();
	};
	hostLocalMemoryTypeIndices.erase(std::remove_if(hostLocalMemoryTypeIndices.begin(), hostLocalMemoryTypeIndices.end(), pred), hostLocalMemoryTypeIndices.end());
	// allocate chunk memory
	for (sint32 t = 0; t < 3; t++)
	{
		// attempt to allocate from device local memory first
		for (auto memType : deviceLocalMemoryTypeIndices)
		{
			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = allocationSize;
			allocInfo.memoryTypeIndex = memType;

			VkDeviceMemory imageMemory;
			VkResult r = vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory);
			if (r != VK_SUCCESS)
				continue;
			m_list_chunkInfo[chunkIndex].mem = imageMemory;
			return allocationSize;
		}
		// attempt to allocate from host-local memory
		for (auto memType : hostLocalMemoryTypeIndices)
		{
			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = allocationSize;
			allocInfo.memoryTypeIndex = memType;

			VkDeviceMemory imageMemory;
			VkResult r = vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory);
			if (r != VK_SUCCESS)
				continue;
			m_list_chunkInfo[chunkIndex].mem = imageMemory;
			return allocationSize;
		}
		// retry with smaller size if possible
		allocationSize /= 2;
		if (allocationSize < minimumAllocationSize)
			break;
		cemuLog_log(LogType::Force, "Failed to allocate texture memory chunk with size {}MB. Trying again with smaller allocation size", allocationSize / 1024 / 1024);
	}
	cemuLog_log(LogType::Force, "Unable to allocate image memory chunk ({} heaps)", deviceLocalMemoryTypeIndices.size());
	throw std::runtime_error("failed to allocate image memory!");
	return 0;
}

uint32_t VKRMemoryManager::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_vkr->GetPhysicalDevice(), &memProperties);

	for (uint32 i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) != 0 && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			return i;
	}
	m_vkr->UnrecoverableError(fmt::format("failed to find suitable memory type ({0:#08x} {1:#08x})", typeFilter, properties).c_str());
	return 0;
}

bool VKRMemoryManager::FindMemoryType2(uint32 typeFilter, VkMemoryPropertyFlags properties, uint32& memoryIndex) const
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_vkr->GetPhysicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if (typeFilter & (1 << i) && memProperties.memoryTypes[i].propertyFlags == properties)
		{
			memoryIndex = i;
			return true;
		}
	}
	return false;
}

std::vector<uint32> VKRMemoryManager::FindMemoryTypes(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
	std::vector<uint32> memoryTypes;
	memoryTypes.clear();
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_vkr->GetPhysicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			memoryTypes.emplace_back(i);
	}

	if (memoryTypes.empty())
		m_vkr->UnrecoverableError(fmt::format("Failed to find suitable memory type ({0:#08x} {1:#08x})", typeFilter, properties).c_str());

	return memoryTypes;
}

size_t VKRMemoryManager::GetTotalMemoryForBufferType(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, size_t minimumBufferSize)
{
	VkDevice logicalDevice = m_vkr->GetLogicalDevice();
	// create temporary buffer object to get memory type
	VkBuffer temporaryBuffer;
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.usage = usage;
	bufferInfo.size = minimumBufferSize; // the buffer size can theoretically influence the memory type, is there a better way to handle this?
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &temporaryBuffer) != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Vulkan: GetTotalMemoryForBufferType() failed to create temporary buffer");
		return 0;
	}

	// get memory requirements for buffer
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(logicalDevice, temporaryBuffer, &memRequirements);
	uint32 typeFilter = memRequirements.memoryTypeBits;
	// destroy temporary buffer
	vkDestroyBuffer(logicalDevice, temporaryBuffer, nullptr);
	// get list of all suitable heaps
	std::unordered_set<uint32> list_heapIndices;
	VkPhysicalDeviceMemoryProperties memProperties{};
	vkGetPhysicalDeviceMemoryProperties(m_vkr->GetPhysicalDevice(), &memProperties);
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			list_heapIndices.emplace(memProperties.memoryTypes[i].heapIndex);
	}
	// sum up size of heaps
	size_t total = 0;
	for (auto heapIndex : list_heapIndices)
	{
		if (heapIndex > memProperties.memoryHeapCount)
			continue;
		total += memProperties.memoryHeaps[heapIndex].size;
	}

	return total;
}

void VKRMemoryManager::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.usage = usage;
	bufferInfo.size = size;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(m_vkr->GetLogicalDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		m_vkr->UnrecoverableError("Failed to create buffer");

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_vkr->GetLogicalDevice(), buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(m_vkr->GetLogicalDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		m_vkr->UnrecoverableError("Failed to allocate buffer memory");
	if (vkBindBufferMemory(m_vkr->GetLogicalDevice(), buffer, bufferMemory, 0) != VK_SUCCESS)
		m_vkr->UnrecoverableError("Failed to bind buffer memory");
}

bool VKRMemoryManager::CreateBuffer2(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.usage = usage;
	bufferInfo.size = size;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(m_vkr->GetLogicalDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Failed to create buffer (CreateBuffer2)");
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_vkr->GetLogicalDevice(), buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	if (!FindMemoryType2(memRequirements.memoryTypeBits, properties, allocInfo.memoryTypeIndex))
	{
		vkDestroyBuffer(m_vkr->GetLogicalDevice(), buffer, nullptr);
		return false;
	}
	if (vkAllocateMemory(m_vkr->GetLogicalDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
	{
		vkDestroyBuffer(m_vkr->GetLogicalDevice(), buffer, nullptr);
		return false;
	}
	if (vkBindBufferMemory(m_vkr->GetLogicalDevice(), buffer, bufferMemory, 0) != VK_SUCCESS)
	{
		vkDestroyBuffer(m_vkr->GetLogicalDevice(), buffer, nullptr);
		cemuLog_log(LogType::Force, "Failed to bind buffer (CreateBuffer2)");
		return false;
	}
	return true;
}

bool VKRMemoryManager::CreateBufferFromHostMemory(void* hostPointer, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.usage = usage;
	bufferInfo.size = size;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkExternalMemoryBufferCreateInfo emb{};
	emb.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
	emb.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;

	bufferInfo.pNext = &emb;

	if (vkCreateBuffer(m_vkr->GetLogicalDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Failed to create buffer (CreateBuffer2)");
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_vkr->GetLogicalDevice(), buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;

	VkImportMemoryHostPointerInfoEXT importHostMem{};
	importHostMem.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
	importHostMem.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
	importHostMem.pHostPointer = hostPointer;
	// VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT or 
	// VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT
	// whats the difference ?

	allocInfo.pNext = &importHostMem;

	if (!FindMemoryType2(memRequirements.memoryTypeBits, properties, allocInfo.memoryTypeIndex))
	{
		vkDestroyBuffer(m_vkr->GetLogicalDevice(), buffer, nullptr);
		return false;
	}
	if (vkAllocateMemory(m_vkr->GetLogicalDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
	{
		vkDestroyBuffer(m_vkr->GetLogicalDevice(), buffer, nullptr);
		return false;
	}
	if (vkBindBufferMemory(m_vkr->GetLogicalDevice(), buffer, bufferMemory, 0) != VK_SUCCESS)
	{
		vkDestroyBuffer(m_vkr->GetLogicalDevice(), buffer, nullptr);
		cemuLog_log(LogType::Force, "Failed to bind buffer (CreateBufferFromHostMemory)");
		return false;
	}
	return true;
}

void VKRMemoryManager::DeleteBuffer(VkBuffer& buffer, VkDeviceMemory& deviceMem) const
{
	if (buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(m_vkr->GetLogicalDevice(), buffer, nullptr);
	if (deviceMem != VK_NULL_HANDLE)
		vkFreeMemory(m_vkr->GetLogicalDevice(), deviceMem, nullptr);
	buffer = VK_NULL_HANDLE;
	deviceMem = VK_NULL_HANDLE;
}

VkImageMemAllocation* VKRMemoryManager::imageMemoryAllocate(VkImage image)
{
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_vkr->GetLogicalDevice(), image, &memRequirements);
	uint32 typeFilter = memRequirements.memoryTypeBits;

	// get or create heap for this type filter
	VkTextureChunkedHeap* texHeap;
	auto it = map_textureHeap.find(typeFilter);
	if (it == map_textureHeap.end())
	{
		texHeap = new VkTextureChunkedHeap(this, typeFilter, m_vkr->GetLogicalDevice());
		map_textureHeap.emplace(typeFilter, texHeap);
	}
	else
		texHeap = it->second;

	// alloc mem from heap
	uint32 allocationSize = (uint32)memRequirements.size;

	CHAddr mem = texHeap->allocMem(allocationSize, (uint32)memRequirements.alignment);
	if (!mem.isValid())
	{
		// allocation failed, try to make space by deleting textures
		// todo - improve this algorithm
		std::vector<LatteTexture*> deleteableTextures = LatteTC_GetDeleteableTextures();
		// delete up to 20 textures from the deletable textures list, then retry allocation
		while (!deleteableTextures.empty())
		{
			size_t numDelete = deleteableTextures.size();
			if (numDelete > 20)
				numDelete = 20;
			for (size_t i = 0; i < numDelete; i++)
				LatteTexture_Delete(deleteableTextures[i]);
			deleteableTextures.erase(deleteableTextures.begin(), deleteableTextures.begin() + numDelete);
			mem = texHeap->allocMem(allocationSize, (uint32)memRequirements.alignment);
			if (mem.isValid())
				break;
		}
		if (!mem.isValid())
		{
			m_vkr->UnrecoverableError("Ran out of VRAM for textures");
		}
	}

	vkBindImageMemory(m_vkr->GetLogicalDevice(), image, texHeap->getChunkMem(mem.chunkIndex), mem.offset);

	return new VkImageMemAllocation(typeFilter, mem, allocationSize);
}

void VKRMemoryManager::imageMemoryFree(VkImageMemAllocation* imageMemAllocation)
{
	auto heapItr = map_textureHeap.find(imageMemAllocation->typeFilter);
	if (heapItr == map_textureHeap.end())
	{
		cemuLog_log(LogType::Force, "Internal texture heap error");
		return;
	}
	heapItr->second->freeMem(imageMemAllocation->mem);
	delete imageMemAllocation;
}

void VKRMemoryManager::appendOverlayHeapDebugInfo()
{
	for (auto& itr : map_textureHeap)
	{
		uint32 heapSize;
		uint32 allocatedBytes;
		itr.second->getStatistics(heapSize, allocatedBytes);

		uint32 heapSizeMB = (heapSize / 1024 / 1024);
		uint32 allocatedBytesMB = (allocatedBytes / 1024 / 1024);

		ImGui::Text("%s", fmt::format("{0:#08x} Size: {1}MB/{2}MB", itr.first, allocatedBytesMB, heapSizeMB).c_str());
	}
}
