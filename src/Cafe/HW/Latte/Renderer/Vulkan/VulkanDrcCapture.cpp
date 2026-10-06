// Fork-only: asynchronous readback of the DRC (GamePad) framebuffer for the GamePad bridge.
//
// Unlike HandleScreenshotRequest (submit + wait, i.e. a full GPU stall), this records the copy into the
// current command buffer and harvests the pixels on a later DRC flip once that command buffer has
// finished. Cost: about one frame of latency instead of a render-thread stall. The bridge measures it
// as "cemu flip->submit" (docs/MEASUREMENTS.md).
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/LatteTextureViewVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/LatteTextureVk.h"
#include "Cafe/GamePad/GamePadSink.h"

namespace
{
	constexpr int kReadbackSlots = 3;

	struct DrcReadback
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		uint8* mapped = nullptr;
		VkDeviceSize capacity = 0;
		uint32 width = 0, height = 0;
		bool srgb = false; // bytes are sRGB-encoded: decode like the display shader / screenshots do
		bool usesConvImage = false;
		uint64 commandBufferId = 0;
		sint64 tFlipNs = 0;
		uint64 counter = 0; // GamePadSink::FrameCounter() at capture
		bool pending = false;
	};
}

struct DrcCaptureState
{
	DrcReadback slots[kReadbackSlots];
	// Conversion target for framebuffers that aren't RGBA8 (blit to RGBA8 UNORM).
	VkImage convImage = VK_NULL_HANDLE;
	VkDeviceMemory convMemory = VK_NULL_HANDLE;
	uint32 convW = 0, convH = 0;
	uint8 srgbToLinear[256];
};

static void DrcHarvest(VulkanRenderer* r, DrcCaptureState& st)
{
	// Oldest first, so frames reach the bridge in order.
	for (;;)
	{
		DrcReadback* next = nullptr;
		for (auto& e : st.slots)
			if (e.pending && r->HasCommandBufferFinished(e.commandBufferId) && (!next || e.commandBufferId < next->commandBufferId))
				next = &e;
		if (!next)
			return;
		next->pending = false;
		GamePadSink::FrameTarget target;
		if (!GamePadSink::BeginFrame(next->width, next->height, target))
			continue;
		const size_t rowBytes = size_t(next->width) * 4;
		for (uint32 y = 0; y < next->height; y++)
		{
			const uint8* src = next->mapped + y * rowBytes;
			uint8* dst = target.dst + size_t(y) * target.stride;
			if (!next->srgb)
			{
				memcpy(dst, src, rowBytes);
				continue;
			}
			for (size_t x = 0; x < rowBytes; x += 4)
			{
				dst[x + 0] = st.srgbToLinear[src[x + 0]];
				dst[x + 1] = st.srgbToLinear[src[x + 1]];
				dst[x + 2] = st.srgbToLinear[src[x + 2]];
				dst[x + 3] = src[x + 3];
			}
		}
		GamePadSink::EndFrame(target, next->width, next->height, next->tFlipNs, next->counter);
	}
}

bool VulkanRenderer::DrcCapture(LatteTextureView* texView, sint64 tFlipNs)
{
	if (!m_drcCapture)
	{
		m_drcCapture = new DrcCaptureState();
		for (int i = 0; i < 256; i++)
			m_drcCapture->srgbToLinear[i] = SRGBComponentToRGB(uint8(i));
	}
	DrcCaptureState& st = *m_drcCapture;

	ProcessFinishedCommandBuffers();
	DrcHarvest(this, st);

	auto texViewVk = (LatteTextureViewVk*)texView;
	if (texViewVk->firstMip != 0)
		return false;
	LatteTextureVk* baseTex = texViewVk->GetBaseImage();
	int width, height;
	baseTex->GetEffectiveSize(width, height, 0);
	if (width <= 0 || height <= 0)
		return false;

	DrcReadback* slot = nullptr;
	for (auto& e : st.slots)
		if (!e.pending)
		{
			slot = &e;
			break;
		}
	if (!slot)
		return false; // GPU hasn't finished the previous copies yet: drop, never wait

	const VkDeviceSize size = VkDeviceSize(width) * height * 4;
	if (slot->capacity < size)
	{
		if (slot->buffer)
		{
			vkUnmapMemory(m_logicalDevice, slot->memory);
			vkDestroyBuffer(m_logicalDevice, slot->buffer, nullptr);
			vkFreeMemory(m_logicalDevice, slot->memory, nullptr);
			slot->buffer = VK_NULL_HANDLE;
			slot->capacity = 0;
		}
		if (!memoryManager->CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
										 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
										 slot->buffer, slot->memory))
		{
			cemuLog_log(LogType::Force, "GamePad Bridge: failed to allocate DRC readback buffer ({} bytes)", size);
			return false;
		}
		void* p = nullptr;
		vkMapMemory(m_logicalDevice, slot->memory, 0, VK_WHOLE_SIZE, 0, &p);
		slot->mapped = static_cast<uint8*>(p);
		slot->capacity = size;
	}

	// Transfers are invalid inside a render pass; the game's pass can still be open at flip time.
	// (Cemu does the same before its own copies, e.g. VulkanRenderer.cpp "vkCmdCopyImage must be
	// called outside of a renderpass".) First real run without this read back only black.
	draw_endRenderPass();
	baseTex->GetImageObj()->flagForCurrentCommandBuffer();
	VkImage srcImage = baseTex->GetImageObj()->m_image;
	const VkFormat format = baseTex->GetFormat();
	const bool direct = format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB;

	VkBufferImageCopy region{};
	region.bufferRowLength = width;
	region.bufferImageHeight = height;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = {(uint32)width, (uint32)height, 1};

	if (direct)
	{
		// Same barrier sequence as the screenshot path's no-blit case.
		region.imageSubresource.baseArrayLayer = texViewVk->firstSlice;
		barrier_image<IMAGE_WRITE | TRANSFER_WRITE, TRANSFER_READ>(baseTex, region.imageSubresource, VK_IMAGE_LAYOUT_GENERAL);
		vkCmdCopyImageToBuffer(m_state.currentCommandBuffer, srcImage, VK_IMAGE_LAYOUT_GENERAL, slot->buffer, 1, &region);
		barrier_image<TRANSFER_READ, TRANSFER_WRITE | IMAGE_WRITE>(baseTex, region.imageSubresource, baseTex->GetDefaultLayout());
		slot->srgb = format == VK_FORMAT_R8G8B8A8_SRGB;
		slot->usesConvImage = false;
	}
	else
	{
		// Blit to RGBA8 UNORM: the blit decodes sRGB/float sources to linear values, which is what
		// the display shader shows. (Upstream's screenshot path blits to an sRGB target here; we don't.)
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
		bool canBlit = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0;
		vkGetPhysicalDeviceFormatProperties(m_physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &props);
		canBlit &= (props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0;
		if (!canBlit)
		{
			static bool logged = false;
			if (!logged)
				cemuLog_log(LogType::Force, "GamePad Bridge: DRC framebuffer format {} can't be blitted to RGBA8; no frames", (int)format);
			logged = true;
			return false;
		}

		if (st.convW != (uint32)width || st.convH != (uint32)height)
		{
			for (auto& e : st.slots)
				if (e.pending && e.usesConvImage)
					return false; // in use by an unfinished copy; recreate once it's done
			if (st.convImage)
			{
				vkDestroyImage(m_logicalDevice, st.convImage, nullptr);
				vkFreeMemory(m_logicalDevice, st.convMemory, nullptr);
				st.convImage = VK_NULL_HANDLE;
			}
			VkImageCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			info.format = VK_FORMAT_R8G8B8A8_UNORM;
			info.extent = {(uint32)width, (uint32)height, 1};
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.arrayLayers = 1;
			info.mipLevels = 1;
			info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			info.imageType = VK_IMAGE_TYPE_2D;
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			if (vkCreateImage(m_logicalDevice, &info, nullptr, &st.convImage) != VK_SUCCESS)
				return false;
			VkMemoryRequirements req;
			vkGetImageMemoryRequirements(m_logicalDevice, st.convImage, &req);
			uint32 memIndex;
			VkMemoryAllocateInfo alloc{};
			alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc.allocationSize = req.size;
			if (!memoryManager->FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memIndex))
			{
				vkDestroyImage(m_logicalDevice, st.convImage, nullptr);
				st.convImage = VK_NULL_HANDLE;
				return false;
			}
			alloc.memoryTypeIndex = memIndex;
			if (vkAllocateMemory(m_logicalDevice, &alloc, nullptr, &st.convMemory) != VK_SUCCESS)
			{
				vkDestroyImage(m_logicalDevice, st.convImage, nullptr);
				st.convImage = VK_NULL_HANDLE;
				return false;
			}
			vkBindImageMemory(m_logicalDevice, st.convImage, st.convMemory, 0);
			st.convW = width;
			st.convH = height;
		}

		VkImageSubresourceRange dstRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		barrier_image<TRANSFER_READ, TRANSFER_WRITE>(st.convImage, dstRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		VkImageSubresourceLayers srcLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, (uint32)texViewVk->firstSlice, 1};
		barrier_image<IMAGE_WRITE | TRANSFER_WRITE, SYNC_OP::TRANSFER_READ>(baseTex, srcLayers, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VkImageBlit blit{};
		blit.srcSubresource = srcLayers;
		blit.srcOffsets[1] = {width, height, 1};
		blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		blit.dstOffsets[1] = {width, height, 1};
		vkCmdBlitImage(m_state.currentCommandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, st.convImage,
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

		barrier_image<TRANSFER_WRITE, TRANSFER_READ>(st.convImage, dstRange, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		VkImageSubresourceLayers backLayers{baseTex->GetImageAspect(), 0, (uint32)texViewVk->firstSlice, 1};
		barrier_image<TRANSFER_READ, TRANSFER_WRITE | IMAGE_WRITE>(baseTex, backLayers, baseTex->GetDefaultLayout());

		region.imageSubresource.baseArrayLayer = 0;
		vkCmdCopyImageToBuffer(m_state.currentCommandBuffer, st.convImage, VK_IMAGE_LAYOUT_GENERAL, slot->buffer, 1, &region);
		slot->srgb = false;
		slot->usesConvImage = true;
	}

	slot->width = width;
	slot->height = height;
	slot->tFlipNs = tFlipNs;
	slot->counter = GamePadSink::FrameCounter();
	slot->commandBufferId = GetCurrentCommandBufferId();
	slot->pending = true;
	return true;
}

void VulkanRenderer::DrcCapture_Release()
{
	if (!m_drcCapture)
		return;
	DrcCaptureState& st = *m_drcCapture;
	for (auto& e : st.slots)
	{
		if (!e.buffer)
			continue;
		vkUnmapMemory(m_logicalDevice, e.memory);
		vkDestroyBuffer(m_logicalDevice, e.buffer, nullptr);
		vkFreeMemory(m_logicalDevice, e.memory, nullptr);
	}
	if (st.convImage)
	{
		vkDestroyImage(m_logicalDevice, st.convImage, nullptr);
		vkFreeMemory(m_logicalDevice, st.convMemory, nullptr);
	}
	delete m_drcCapture;
	m_drcCapture = nullptr;
}
