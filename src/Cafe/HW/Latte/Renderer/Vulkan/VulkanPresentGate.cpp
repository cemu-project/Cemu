// Fork-only: the present gate (cemu-gamepad project, TASKS.md 7). Delays the TV picture by K whole frames so
// it lines up with the GamePad screen, which trails because of readback, encode, radio and decode.
//
// Runs in SwapBuffer(mainWindow) just before the frame is submitted and presented. The finished swapchain
// image is copied into a ring of recent frames (with their flip times), and the newest frame that is at
// least `hold` old (minus half a frame) is copied back into the swapchain image, which is then presented as
// usual. Selection is by TIME, not frame count, so the hold stays right when the game's frame rate changes.
// No extra thread, no queue sharing, no swapchain-image hoarding, no throughput loss; precision is whole
// frames (+-8 ms at 60 Hz), which is what FIFO vsync displays at anyway. Extra GPU work: two image copies
// per TV frame while the gate is active.
//
// With the sync test pattern on, the ring is filled with the CPU-drawn pattern instead of the game picture
// (and blitted to the swapchain), so the pattern goes through exactly the same delay.
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/SwapchainInfoVk.h"
#include "Cafe/GamePad/GamePadSink.h"
#include "Cafe/GamePad/SyncPattern.h"

namespace
{
	constexpr uint32 kPatternW = 864, kPatternH = 480;

	struct GateSlot
	{
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		// pattern mode only: CPU-drawn pattern, uploaded into `image`
		VkBuffer staging = VK_NULL_HANDLE;
		VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
		uint8* stagingPtr = nullptr;
		uint64 stagingCmdBufferId = 0; // staging may be rewritten once this command buffer has finished
		bool hasContent = false;
		uint64 counter = 0;
		sint64 tFlipNs = 0;
	};
}

struct PresentGateState
{
	std::vector<GateSlot> slots; // GamePadSink::kGateRingSize
	sint64 holdNs = 0;
	bool patternMode = false;
	VkExtent2D extent{};
	VkFormat format = VK_FORMAT_UNDEFINED;
	uint32 head = 0;
	bool loggedUnsupported = false;
};

static void GateBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to)
{
	VkImageMemoryBarrier b{};
	b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	b.oldLayout = from;
	b.newLayout = to;
	b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = image;
	b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
						 nullptr, 1, &b);
}

void VulkanRenderer::PresentGate_Release()
{
	if (!m_presentGate)
		return;
	WaitDeviceIdle();
	for (auto& s : m_presentGate->slots)
	{
		if (s.image)
			vkDestroyImage(m_logicalDevice, s.image, nullptr);
		if (s.memory)
			vkFreeMemory(m_logicalDevice, s.memory, nullptr);
		if (s.staging)
		{
			vkUnmapMemory(m_logicalDevice, s.stagingMemory);
			vkDestroyBuffer(m_logicalDevice, s.staging, nullptr);
			vkFreeMemory(m_logicalDevice, s.stagingMemory, nullptr);
		}
	}
	m_presentGate->slots.clear();
	m_presentGate->holdNs = 0;
}

void VulkanRenderer::PresentGate_Apply(SwapchainInfoVk& chain)
{
	const sint64 hold = GamePadSink::TvHoldNs();
	const bool pattern = GamePadSink::PatternActive();
	if (hold <= 0 && !pattern)
	{
		if (m_presentGate && !m_presentGate->slots.empty())
			PresentGate_Release(); // gate turned off: free the ring
		return;
	}
	if (!m_presentGate)
		m_presentGate = new PresentGateState();
	PresentGateState& st = *m_presentGate;

	// The swapchain image must allow transfers (SwapchainInfoVk requests it when supported).
	if (!chain.m_transferUsageSupported)
	{
		if (!st.loggedUnsupported)
			cemuLog_log(LogType::Force, "GamePad present gate: swapchain images don't support transfer; TV picture can't be held");
		st.loggedUnsupported = true;
		return;
	}

	const VkExtent2D extent = chain.getExtent();
	const VkFormat swapFormat = chain.m_surfaceFormat.format;
	const VkFormat ringFormat = pattern ? VK_FORMAT_R8G8B8A8_UNORM : swapFormat;
	const VkExtent2D ringExtent = pattern ? VkExtent2D{kPatternW, kPatternH} : extent;
	const uint32 count = hold > 0 ? uint32(GamePadSink::kGateRingSize) : 1;

	if (pattern)
	{
		// The pattern is blitted (scaled, format-converted) onto the swapchain image.
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(m_physicalDevice, swapFormat, &props);
		if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
		{
			if (!st.loggedUnsupported)
				cemuLog_log(LogType::Force, "GamePad present gate: swapchain format {} can't be blitted to; no TV test pattern", (int)swapFormat);
			st.loggedUnsupported = true;
			return;
		}
	}

	// (Re)create the ring when on/off, mode, size or format changes. Rare; a device wait is fine.
	if (st.slots.size() != count || st.patternMode != pattern || st.extent.width != ringExtent.width ||
		st.extent.height != ringExtent.height || st.format != ringFormat)
	{
		PresentGate_Release();
		st.slots.resize(count);
		for (auto& s : st.slots)
		{
			VkImageCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			info.imageType = VK_IMAGE_TYPE_2D;
			info.format = ringFormat;
			info.extent = {ringExtent.width, ringExtent.height, 1};
			info.mipLevels = 1;
			info.arrayLayers = 1;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			uint32 memIndex = 0;
			VkMemoryRequirements req{};
			bool ok = vkCreateImage(m_logicalDevice, &info, nullptr, &s.image) == VK_SUCCESS;
			if (ok)
			{
				vkGetImageMemoryRequirements(m_logicalDevice, s.image, &req);
				ok = memoryManager->FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memIndex);
			}
			if (ok)
			{
				VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, req.size, memIndex};
				ok = vkAllocateMemory(m_logicalDevice, &alloc, nullptr, &s.memory) == VK_SUCCESS;
			}
			if (ok)
				vkBindImageMemory(m_logicalDevice, s.image, s.memory, 0);
			if (ok && pattern)
			{
				ok = memoryManager->CreateBuffer(VkDeviceSize(kPatternW) * kPatternH * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
												 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
												 s.staging, s.stagingMemory);
				if (ok)
				{
					void* p = nullptr;
					vkMapMemory(m_logicalDevice, s.stagingMemory, 0, VK_WHOLE_SIZE, 0, &p);
					s.stagingPtr = static_cast<uint8*>(p);
				}
			}
			if (!ok)
			{
				cemuLog_log(LogType::Force, "GamePad present gate: failed to allocate the frame ring; TV picture not held");
				PresentGate_Release();
				return;
			}
		}
		st.patternMode = pattern;
		st.extent = ringExtent;
		st.format = ringFormat;
		st.head = 0;
		cemuLog_log(LogType::Force, "GamePad present gate: active ({} ms hold{})", hold / 1'000'000, pattern ? ", sync test pattern" : "");
	}

	draw_endRenderPass(); // transfers are invalid inside a render pass
	VkCommandBuffer cmd = m_state.currentCommandBuffer;
	VkImage swapImage = chain.m_swapchainImages[chain.swapchainImageIndex];
	const uint64 counter = GamePadSink::FrameCounter();
	const sint64 tNow = GamePadSink::NowNs();

	// 1) Store this frame (the game picture, or the pattern) in the ring at head.
	GateSlot& cur = st.slots[st.head];
	GateBarrier(cmd, cur.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	if (pattern)
	{
		if (cur.stagingCmdBufferId && !HasCommandBufferFinished(cur.stagingCmdBufferId))
			WaitCommandBufferFinished(cur.stagingCmdBufferId); // K+1 frames old: practically never waits
		SyncPattern::Draw(cur.stagingPtr, kPatternW, kPatternH, kPatternW * 4, counter);
		VkBufferImageCopy region{};
		region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		region.imageExtent = {kPatternW, kPatternH, 1};
		vkCmdCopyBufferToImage(cmd, cur.staging, cur.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		cur.stagingCmdBufferId = GetCurrentCommandBufferId();
	}
	else
	{
		GateBarrier(cmd, swapImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		VkImageCopy copy{};
		copy.srcSubresource = copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copy.extent = {extent.width, extent.height, 1};
		vkCmdCopyImage(cmd, swapImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cur.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	}
	GateBarrier(cmd, cur.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	cur.hasContent = true;
	cur.counter = counter;
	cur.tFlipNs = tNow;

	// 2) Show the newest frame flipped at least `hold` ago (half a frame of slack, so a steady 60 Hz
	//    stream with hold = 33 ms picks exactly 2 frames back rather than jittering between 2 and 3).
	//    If none is that old yet (ring filling, or hold beyond what the ring covers at this rate),
	//    show the oldest frame we have.
	const uint32 n = uint32(st.slots.size());
	const sint64 halfFrame = 8'333'333;
	const sint64 cutoff = tNow - hold + halfFrame;
	uint32 showIndex = st.head;
	sint64 best = INT64_MIN, oldest = INT64_MAX;
	uint32 oldestIndex = st.head;
	for (uint32 i = 0; i < n; i++)
	{
		const GateSlot& sl = st.slots[i];
		if (!sl.hasContent)
			continue;
		if (sl.tFlipNs <= cutoff && sl.tFlipNs > best)
		{
			best = sl.tFlipNs;
			showIndex = i;
		}
		if (sl.tFlipNs < oldest)
		{
			oldest = sl.tFlipNs;
			oldestIndex = i;
		}
	}
	if (best == INT64_MIN)
		showIndex = oldestIndex;
	GateSlot& show = st.slots[showIndex];

	GateBarrier(cmd, swapImage, pattern ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	if (pattern)
	{
		VkImageBlit blit{};
		blit.srcSubresource = blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		blit.srcOffsets[1] = {sint32(kPatternW), sint32(kPatternH), 1};
		blit.dstOffsets[1] = {sint32(extent.width), sint32(extent.height), 1};
		vkCmdBlitImage(cmd, show.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
	}
	else if (showIndex != st.head)
	{
		VkImageCopy copy{};
		copy.srcSubresource = copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copy.extent = {extent.width, extent.height, 1};
		vkCmdCopyImage(cmd, show.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	}
	GateBarrier(cmd, swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	GamePadSink::OnTvPresent(show.counter, show.tFlipNs, tNow);
	st.head = (st.head + 1) % n;
}
