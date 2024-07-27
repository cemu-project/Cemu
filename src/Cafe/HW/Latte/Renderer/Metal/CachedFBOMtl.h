#pragma once

#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Core/LatteCachedFBO.h"

class CachedFBOMtl : public LatteCachedFBO
{
public:
	CachedFBOMtl(uint64 key) : LatteCachedFBO(key)
	{
		CreateRenderPass();
	}

	~CachedFBOMtl();

	MTL::RenderPassDescriptor* GetRenderPassDescriptor()
	{
	    return m_renderPassDescriptor;
	}

private:
    MTL::RenderPassDescriptor* m_renderPassDescriptor = nullptr;

    void CreateRenderPass();
};
