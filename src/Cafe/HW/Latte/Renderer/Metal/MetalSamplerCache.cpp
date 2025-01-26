#include "Cafe/HW/Latte/Renderer/Metal/MetalSamplerCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "Metal/MTLSampler.hpp"

inline const char* BorderColorToStr(MTL::SamplerBorderColor borderColor)
{
    switch (borderColor)
    {
    case MTL::SamplerBorderColorTransparentBlack:
        return "transparent black";
    case MTL::SamplerBorderColorOpaqueBlack:
        return "opaque black";
    case MTL::SamplerBorderColorOpaqueWhite:
        return "opaque white";
    }
}

MetalSamplerCache::~MetalSamplerCache()
{
    for (auto& pair : m_samplerCache)
    {
        pair.second->release();
    }
    m_samplerCache.clear();
}

MTL::SamplerState* MetalSamplerCache::GetSamplerState(const LatteContextRegister& lcr, LatteConst::ShaderType shaderType, uint32 stageSamplerIndex)
{
    uint32 samplerIndex = stageSamplerIndex + LatteDecompiler_getTextureSamplerBaseIndex(shaderType);

    uint64 stateHash = CalculateSamplerHash(lcr, shaderType, stageSamplerIndex, samplerIndex);
    auto& samplerState = m_samplerCache[stateHash];
    if (samplerState)
        return samplerState;

	// Sampler state
	const _LatteRegisterSetSampler* samplerWords = lcr.SQ_TEX_SAMPLER + samplerIndex;

    NS_STACK_SCOPED MTL::SamplerDescriptor* samplerDescriptor = MTL::SamplerDescriptor::alloc()->init();

    // lod
    uint32 iMinLOD = samplerWords->WORD1.get_MIN_LOD();
    uint32 iMaxLOD = samplerWords->WORD1.get_MAX_LOD();
    sint32 iLodBias = samplerWords->WORD1.get_LOD_BIAS();

    // TODO: uncomment
    // apply relative lod bias from graphic pack
    //if (baseTexture->overwriteInfo.hasRelativeLodBias)
    //    iLodBias += baseTexture->overwriteInfo.relativeLodBias;
    // apply absolute lod bias from graphic pack
    //if (baseTexture->overwriteInfo.hasLodBias)
    //    iLodBias = baseTexture->overwriteInfo.lodBias;

    auto filterMip = samplerWords->WORD0.get_MIP_FILTER();
    if (filterMip == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_Z_FILTER::NONE)
    {
        samplerDescriptor->setMipFilter(MTL::SamplerMipFilterNearest);
        samplerDescriptor->setLodMinClamp(0.0f);
        samplerDescriptor->setLodMaxClamp(0.25f);
    }
    else if (filterMip == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_Z_FILTER::POINT)
    {
        samplerDescriptor->setMipFilter(MTL::SamplerMipFilterNearest);
        samplerDescriptor->setLodMinClamp((float)iMinLOD / 64.0f);
        samplerDescriptor->setLodMaxClamp((float)iMaxLOD / 64.0f);
    }
    else if (filterMip == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_Z_FILTER::LINEAR)
    {
        samplerDescriptor->setMipFilter(MTL::SamplerMipFilterLinear);
        samplerDescriptor->setLodMinClamp((float)iMinLOD / 64.0f);
        samplerDescriptor->setLodMaxClamp((float)iMaxLOD / 64.0f);
    }
    else
    {
        // fallback for invalid constants
        samplerDescriptor->setMipFilter(MTL::SamplerMipFilterLinear);
        samplerDescriptor->setLodMinClamp((float)iMinLOD / 64.0f);
        samplerDescriptor->setLodMaxClamp((float)iMaxLOD / 64.0f);
    }

    auto filterMin = samplerWords->WORD0.get_XY_MIN_FILTER();
    cemu_assert_debug(filterMin != Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::BICUBIC); // todo
    samplerDescriptor->setMinFilter((filterMin == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::POINT || filterMin == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::ANISO_POINT) ? MTL::SamplerMinMagFilterNearest : MTL::SamplerMinMagFilterLinear);

    auto filterMag = samplerWords->WORD0.get_XY_MAG_FILTER();
    samplerDescriptor->setMagFilter((filterMag == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::POINT || filterMin == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::ANISO_POINT) ? MTL::SamplerMinMagFilterNearest : MTL::SamplerMinMagFilterLinear);

    auto filterZ = samplerWords->WORD0.get_Z_FILTER();
    // todo: z-filter for texture array samplers is customizable for GPU7 but OpenGL/Vulkan doesn't expose this functionality?

    auto clampX = samplerWords->WORD0.get_CLAMP_X();
    auto clampY = samplerWords->WORD0.get_CLAMP_Y();
    auto clampZ = samplerWords->WORD0.get_CLAMP_Z();

    samplerDescriptor->setSAddressMode(GetMtlSamplerAddressMode(clampX));
    samplerDescriptor->setTAddressMode(GetMtlSamplerAddressMode(clampY));
    samplerDescriptor->setRAddressMode(GetMtlSamplerAddressMode(clampZ));

    auto maxAniso = samplerWords->WORD0.get_MAX_ANISO_RATIO();

    // TODO: uncomment
    //if (baseTexture->overwriteInfo.anisotropicLevel >= 0)
    //    maxAniso = baseTexture->overwriteInfo.anisotropicLevel;

    if (maxAniso > 0)
        samplerDescriptor->setMaxAnisotropy(1 << maxAniso);

    // TODO: set lod bias
    //samplerInfo.mipLodBias = (float)iLodBias / 64.0f;

    // depth compare
    //uint8 depthCompareMode = shader->textureUsesDepthCompare[relative_textureUnit] ? 1 : 0;
    // TODO: is it okay to just cast?
    samplerDescriptor->setCompareFunction(GetMtlCompareFunc((Latte::E_COMPAREFUNC)samplerWords->WORD0.get_DEPTH_COMPARE_FUNCTION()));

    // border
    auto borderType = samplerWords->WORD0.get_BORDER_COLOR_TYPE();

    MTL::SamplerBorderColor borderColor;
    if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::TRANSPARENT_BLACK)
        borderColor = MTL::SamplerBorderColorTransparentBlack;
    else if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::OPAQUE_BLACK)
        borderColor = MTL::SamplerBorderColorOpaqueBlack;
    else if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::OPAQUE_WHITE)
        borderColor = MTL::SamplerBorderColorOpaqueWhite;
    else [[unlikely]]
    {
        _LatteRegisterSetSamplerBorderColor* borderColorReg;
		if (shaderType == LatteConst::ShaderType::Vertex)
			borderColorReg = LatteGPUState.contextNew.TD_VS_SAMPLER_BORDER_COLOR + stageSamplerIndex;
		else if (shaderType == LatteConst::ShaderType::Pixel)
			borderColorReg = LatteGPUState.contextNew.TD_PS_SAMPLER_BORDER_COLOR + stageSamplerIndex;
		else // geometry
			borderColorReg = LatteGPUState.contextNew.TD_GS_SAMPLER_BORDER_COLOR + stageSamplerIndex;
		float r = borderColorReg->red.get_channelValue();
		float g = borderColorReg->green.get_channelValue();
		float b = borderColorReg->blue.get_channelValue();
		float a = borderColorReg->alpha.get_channelValue();

		// Metal doesn't support custom border color
		// Let's find the best match
		bool opaque = (a == 1.0f);
		bool white = (r == 1.0f);
		if (opaque)
		{
		    if (white)
                borderColor = MTL::SamplerBorderColorOpaqueWhite;
            else
                borderColor = MTL::SamplerBorderColorOpaqueBlack;
		}
		else
		{
		    borderColor = MTL::SamplerBorderColorTransparentBlack;
		}

        cemuLog_log(LogType::Force, "Custom border color ({}, {}, {}, {}) is not supported on Metal, using {} instead", r, g, b, a, BorderColorToStr(borderColor));
    }
    samplerDescriptor->setBorderColor(borderColor);

    samplerState = m_mtlr->GetDevice()->newSamplerState(samplerDescriptor);

    return samplerState;
}

uint64 MetalSamplerCache::CalculateSamplerHash(const LatteContextRegister& lcr, LatteConst::ShaderType shaderType, uint32 stageSamplerIndex, uint32 samplerIndex)
{
    const _LatteRegisterSetSampler* samplerWords = lcr.SQ_TEX_SAMPLER + samplerIndex;

    // TODO: check this
	return *((uint64*)samplerWords);
}
