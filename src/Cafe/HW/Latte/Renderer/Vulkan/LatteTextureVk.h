#pragma once

#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "util/ChunkedHeap/ChunkedHeap.h"

#include "Cafe/HW/Latte/Renderer/Vulkan/VKRBase.h"

class LatteTextureVk : public LatteTexture
{
public:
	LatteTextureVk(class VulkanRenderer* vkRenderer, Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels,
		uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth);

	~LatteTextureVk();

	void AllocateOnHost() override;

	VKRObjectTexture* GetImageObj() const { return vkObjTex; };
	
	VkFormat GetFormat() const { return vkObjTex->m_format; }
	VkImageAspectFlags GetImageAspect() const { return vkObjTex->m_imageAspect; }

	VkImageLayout GetImageLayout(VkImageSubresource& subresource)
	{
		cemu_assert_debug(subresource.mipLevel < m_layoutsMips);
		cemu_assert_debug(subresource.arrayLayer < m_layoutsDepth);
		if (Is3DTexture())
			return m_layouts[subresource.mipLevel];
		return m_layouts[subresource.mipLevel * m_layoutsDepth + subresource.arrayLayer];
	}

	VkImageLayout GetImageLayout(VkImageSubresourceRange& subresource)
	{
		cemu_assert_debug(subresource.baseMipLevel < m_layoutsMips);
		cemu_assert_debug(subresource.baseArrayLayer < m_layoutsDepth);
		cemu_assert_debug(subresource.levelCount == 1);
		if (Is3DTexture())
			return m_layouts[subresource.baseMipLevel];
		cemu_assert_debug(subresource.layerCount > 0);
		if (subresource.layerCount > 1)
		{
			VkImageLayout imgLayout = m_layouts[subresource.baseMipLevel * m_layoutsDepth + subresource.baseArrayLayer];
			for (uint32 i = 1; i < subresource.layerCount; i++)
			{
				cemu_assert_debug(m_layouts[subresource.baseMipLevel * m_layoutsDepth + subresource.baseArrayLayer + i] == imgLayout);
			}
			return imgLayout;
		}
		return m_layouts[subresource.baseMipLevel * m_layoutsDepth + subresource.baseArrayLayer];
	}

	void SetImageLayout(VkImageSubresource& subresource, VkImageLayout newLayout)
	{
		cemu_assert_debug(subresource.mipLevel < m_layoutsMips);
		cemu_assert_debug(subresource.arrayLayer < m_layoutsDepth);
		if (Is3DTexture())
			m_layouts[subresource.mipLevel] = newLayout;
		else
			m_layouts[subresource.mipLevel * m_layoutsDepth + subresource.arrayLayer] = newLayout;
	}

	void SetImageLayout(VkImageSubresourceRange& subresource, VkImageLayout newLayout)
	{
		cemu_assert_debug(subresource.baseMipLevel < m_layoutsMips);
		cemu_assert_debug(subresource.baseArrayLayer < m_layoutsDepth);
		cemu_assert_debug(subresource.levelCount == 1);
		if (Is3DTexture())
			m_layouts[subresource.baseMipLevel] = newLayout;
		else
		{
			for(uint32 i=0; i<subresource.layerCount; i++)
				m_layouts[subresource.baseMipLevel * m_layoutsDepth + subresource.baseArrayLayer + i] = newLayout;
		}
	}

protected:
	LatteTextureView* CreateView(Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount) override;

public:
	uint64 m_vkFlushIndex{}; // used to track read-write dependencies within the same renderpass

	uint64 m_vkFlushIndex_read{};
	uint64 m_vkFlushIndex_write{};

	uint32 m_collisionCheckIndex{}; // used to track if texture is being both sampled and output to during drawcall

private:
	class VulkanRenderer* m_vkr;

	VKRObjectTexture* vkObjTex{};
	std::vector<VkImageLayout> m_layouts;
	uint32 m_layoutsMips;
	uint32 m_layoutsDepth;
};
