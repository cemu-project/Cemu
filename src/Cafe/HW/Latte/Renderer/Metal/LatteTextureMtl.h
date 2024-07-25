#pragma once

#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "util/ChunkedHeap/ChunkedHeap.h"

class LatteTextureMtl : public LatteTexture
{
public:
	LatteTextureMtl(class MetalRenderer* vkRenderer, Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels,
		uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth);
	~LatteTextureMtl();

	MTL::Texture* GetTexture() const {
	    return m_texture;
	}

	void AllocateOnHost() override;

protected:
	LatteTextureView* CreateView(Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount) override;

public:
	uint64 m_vkFlushIndex{}; // used to track read-write dependencies within the same renderpass

	uint64 m_vkFlushIndex_read{};
	uint64 m_vkFlushIndex_write{};

	uint32 m_collisionCheckIndex{}; // used to track if texture is being both sampled and output to during drawcall

private:
	class MetalRenderer* m_mtlr;

	MTL::Texture* m_texture;
};
