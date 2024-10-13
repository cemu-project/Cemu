#include "Cafe/HW/Latte/Renderer/Metal/MetalPipelineCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"

#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"

uint64 MetalPipelineCache::CalculatePipelineHash(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr)
{
    // Hash
    uint64 stateHash = 0;
    for (int i = 0; i < Latte::GPU_LIMITS::NUM_COLOR_ATTACHMENTS; ++i)
	{
		auto textureView = static_cast<LatteTextureViewMtl*>(lastUsedFBO->colorBuffer[i].texture);
		if (!textureView)
		    continue;

		stateHash += textureView->GetRGBAView()->pixelFormat() + i * 31;
		stateHash = std::rotl<uint64>(stateHash, 7);

		if (activeFBO->colorBuffer[i].texture)
		{
            stateHash += 1;
		    stateHash = std::rotl<uint64>(stateHash, 1);
		}
	}

	if (lastUsedFBO->depthBuffer.texture)
	{
	    auto textureView = static_cast<LatteTextureViewMtl*>(lastUsedFBO->depthBuffer.texture);
		stateHash += textureView->GetRGBAView()->pixelFormat();
		stateHash = std::rotl<uint64>(stateHash, 7);

		if (activeFBO->depthBuffer.texture)
		{
            stateHash += 1;
		    stateHash = std::rotl<uint64>(stateHash, 1);
		}
	}

	for (auto& group : fetchShader->bufferGroups)
	{
		uint32 bufferStride = group.getCurrentBufferStride(lcr.GetRawView());
		stateHash = std::rotl<uint64>(stateHash, 7);
		stateHash += bufferStride * 3;
	}

	stateHash += fetchShader->getVkPipelineHashFragment();
	stateHash = std::rotl<uint64>(stateHash, 7);

	stateHash += lcr.GetRawView()[mmVGT_STRMOUT_EN];
	stateHash = std::rotl<uint64>(stateHash, 7);

	if(lcr.PA_CL_CLIP_CNTL.get_DX_RASTERIZATION_KILL())
		stateHash += 0x333333;

	stateHash = (stateHash >> 8) + (stateHash * 0x370531ull) % 0x7F980D3BF9B4639Dull;

	uint32* ctxRegister = lcr.GetRawView();

	if (vertexShader)
		stateHash += vertexShader->baseHash;

	stateHash = std::rotl<uint64>(stateHash, 13);

	if (pixelShader)
		stateHash += pixelShader->baseHash + pixelShader->auxHash;

	stateHash = std::rotl<uint64>(stateHash, 13);

	uint32 polygonCtrl = lcr.PA_SU_SC_MODE_CNTL.getRawValue();
	stateHash += polygonCtrl;
	stateHash = std::rotl<uint64>(stateHash, 7);

	stateHash += ctxRegister[Latte::REGADDR::PA_CL_CLIP_CNTL];
	stateHash = std::rotl<uint64>(stateHash, 7);

	const auto colorControlReg = ctxRegister[Latte::REGADDR::CB_COLOR_CONTROL];
	stateHash += colorControlReg;

	stateHash += ctxRegister[Latte::REGADDR::CB_TARGET_MASK];

	const uint32 blendEnableMask = (colorControlReg >> 8) & 0xFF;
	if (blendEnableMask)
	{
		for (auto i = 0; i < 8; ++i)
		{
			if (((blendEnableMask & (1 << i))) == 0)
				continue;
			stateHash = std::rotl<uint64>(stateHash, 7);
			stateHash += ctxRegister[Latte::REGADDR::CB_BLEND0_CONTROL + i];
		}
	}

	// Mesh pipeline
	const LattePrimitiveMode primitiveMode = static_cast<LattePrimitiveMode>(LatteGPUState.contextRegister[mmVGT_PRIMITIVE_TYPE]);
    bool isPrimitiveRect = (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::RECTS);

    bool usesGeometryShader = (geometryShader != nullptr || isPrimitiveRect);

    if (usesGeometryShader)
    {
        stateHash += lcr.GetRawView()[mmVGT_PRIMITIVE_TYPE];
        stateHash = std::rotl<uint64>(stateHash, 7);
    }

	return stateHash;
}

MetalPipelineCache::~MetalPipelineCache()
{
    for (auto& [key, value] : m_pipelineCache)
    {
        value->release();
    }
}

MTL::RenderPipelineState* MetalPipelineCache::GetRenderPipelineState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr)
{
    auto& pipeline = m_pipelineCache[CalculatePipelineHash(fetchShader, vertexShader, geometryShader, pixelShader, lastUsedFBO, activeFBO, lcr)];
    if (pipeline)
        return pipeline;

    MetalPipelineCompiler compiler(m_mtlr);
    compiler.InitFromState(fetchShader, vertexShader, geometryShader, pixelShader, lastUsedFBO, activeFBO, lcr);
    pipeline = compiler.Compile(false, true);

    return pipeline;
}
