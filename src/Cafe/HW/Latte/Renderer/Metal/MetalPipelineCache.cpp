#include "Cafe/HW/Latte/Renderer/Metal/MetalPipelineCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "HW/Latte/Core/FetchShader.h"
#include "HW/Latte/ISA/RegDefines.h"
#include "HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"

MetalPipelineCache::~MetalPipelineCache()
{
    for (auto& pair : m_pipelineCache)
    {
        pair.second->release();
    }
    m_pipelineCache.clear();
}

MTL::RenderPipelineState* MetalPipelineCache::GetPipelineState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* pixelShader, CachedFBOMtl* activeFBO, const LatteContextRegister& lcr)
{
    uint64 stateHash = CalculatePipelineHash(fetchShader, vertexShader, pixelShader, activeFBO, lcr);
    auto& pipeline = m_pipelineCache[stateHash];
    if (pipeline)
    {
        return pipeline;
    }

	// Vertex descriptor
	MTL::VertexDescriptor* vertexDescriptor = MTL::VertexDescriptor::alloc()->init();
	for (auto& bufferGroup : fetchShader->bufferGroups)
	{
		std::optional<LatteConst::VertexFetchType2> fetchType;

		for (sint32 j = 0; j < bufferGroup.attribCount; ++j)
		{
			auto& attr = bufferGroup.attrib[j];

			uint32 semanticId = vertexShader->resourceMapping.attributeMapping[attr.semanticId];
			if (semanticId == (uint32)-1)
				continue; // attribute not used?

			auto attribute = vertexDescriptor->attributes()->object(semanticId);
			attribute->setOffset(attr.offset);
			// Bind from the end to not conflict with uniform buffers
			attribute->setBufferIndex(GET_MTL_VERTEX_BUFFER_INDEX(attr.attributeBufferIndex));
			attribute->setFormat(GetMtlVertexFormat(attr.format));

			if (fetchType.has_value())
				cemu_assert_debug(fetchType == attr.fetchType);
			else
				fetchType = attr.fetchType;

			if (attr.fetchType == LatteConst::INSTANCE_DATA)
			{
				cemu_assert_debug(attr.aluDivisor == 1); // other divisor not yet supported
			}
		}

		uint32 bufferIndex = bufferGroup.attributeBufferIndex;
		uint32 bufferBaseRegisterIndex = mmSQ_VTX_ATTRIBUTE_BLOCK_START + bufferIndex * 7;
		uint32 bufferStride = (LatteGPUState.contextNew.GetRawView()[bufferBaseRegisterIndex + 2] >> 11) & 0xFFFF;

		uint32 strideRemainder = bufferStride % 4;
		if (strideRemainder != 0)
		{
		    debug_printf("vertex stride must be a multiple of 4, remainder: %u\n", strideRemainder);
		}

		auto layout = vertexDescriptor->layouts()->object(GET_MTL_VERTEX_BUFFER_INDEX(bufferIndex));
		layout->setStride(bufferStride);
		if (!fetchType.has_value() || fetchType == LatteConst::VertexFetchType2::VERTEX_DATA)
			layout->setStepFunction(MTL::VertexStepFunctionPerVertex);
		else if (fetchType == LatteConst::VertexFetchType2::INSTANCE_DATA)
			layout->setStepFunction(MTL::VertexStepFunctionPerInstance);
		else
		{
		    debug_printf("unimplemented vertex fetch type %u\n", (uint32)fetchType.value());
			cemu_assert(false);
		}
	}

	// Render pipeline state
	MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
	desc->setVertexFunction(static_cast<RendererShaderMtl*>(vertexShader->shader)->GetFunction());
	desc->setFragmentFunction(static_cast<RendererShaderMtl*>(pixelShader->shader)->GetFunction());
	// TODO: don't always set the vertex descriptor
	desc->setVertexDescriptor(vertexDescriptor);
	for (uint8 i = 0; i < 8; i++)
	{
	    const auto& colorBuffer = activeFBO->colorBuffer[i];
		auto texture = static_cast<LatteTextureViewMtl*>(colorBuffer.texture);
		if (!texture)
		{
		    continue;
		}
		auto colorAttachment = desc->colorAttachments()->object(i);
		colorAttachment->setPixelFormat(texture->GetRGBAView()->pixelFormat());

		// Blending
		const Latte::LATTE_CB_COLOR_CONTROL& colorControlReg = LatteGPUState.contextNew.CB_COLOR_CONTROL;
		uint32 blendEnableMask = colorControlReg.get_BLEND_MASK();
		uint32 renderTargetMask = LatteGPUState.contextNew.CB_TARGET_MASK.get_MASK();

		bool blendEnabled = ((blendEnableMask & (1 << i))) != 0;
		if (blendEnabled)
		{
    		colorAttachment->setBlendingEnabled(true);

    		const auto& blendControlReg = LatteGPUState.contextNew.CB_BLENDN_CONTROL[i];

    		auto rgbBlendOp = GetMtlBlendOp(blendControlReg.get_COLOR_COMB_FCN());
    		auto srcRgbBlendFactor = GetMtlBlendFactor(blendControlReg.get_COLOR_SRCBLEND());
    		auto dstRgbBlendFactor = GetMtlBlendFactor(blendControlReg.get_COLOR_DSTBLEND());

    		colorAttachment->setWriteMask((renderTargetMask >> (i * 4)) & 0xF);
    		colorAttachment->setRgbBlendOperation(rgbBlendOp);
    		colorAttachment->setSourceRGBBlendFactor(srcRgbBlendFactor);
    		colorAttachment->setDestinationRGBBlendFactor(dstRgbBlendFactor);
    		if (blendControlReg.get_SEPARATE_ALPHA_BLEND())
    		{
    			colorAttachment->setAlphaBlendOperation(GetMtlBlendOp(blendControlReg.get_ALPHA_COMB_FCN()));
      		    colorAttachment->setSourceAlphaBlendFactor(GetMtlBlendFactor(blendControlReg.get_ALPHA_SRCBLEND()));
      		    colorAttachment->setDestinationAlphaBlendFactor(GetMtlBlendFactor(blendControlReg.get_ALPHA_DSTBLEND()));
    		}
    		else
    		{
        		colorAttachment->setAlphaBlendOperation(rgbBlendOp);
        		colorAttachment->setSourceAlphaBlendFactor(srcRgbBlendFactor);
        		colorAttachment->setDestinationAlphaBlendFactor(dstRgbBlendFactor);
    		}
		}
	}
	if (activeFBO->depthBuffer.texture)
	{
	    auto texture = static_cast<LatteTextureViewMtl*>(activeFBO->depthBuffer.texture);
        desc->setDepthAttachmentPixelFormat(texture->GetRGBAView()->pixelFormat());
        // TODO: stencil pixel format
	}

	NS::Error* error = nullptr;
	pipeline = m_mtlr->GetDevice()->newRenderPipelineState(desc, &error);
	desc->release();
	vertexDescriptor->release();
	if (error)
	{
	    debug_printf("error creating render pipeline state: %s\n", error->localizedDescription()->utf8String());
		error->release();
		return nullptr;
	}

	return pipeline;
}

uint64 MetalPipelineCache::CalculatePipelineHash(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr)
{
    // Hash
    uint64 stateHash = 0;
	for (auto& group : fetchShader->bufferGroups)
	{
		uint32 bufferStride = group.getCurrentBufferStride(lcr.GetRawView());
		stateHash = std::rotl<uint64>(stateHash, 7);
		stateHash += bufferStride * 3;
	}

	stateHash += fetchShader->getVkPipelineHashFragment();
	stateHash = std::rotl<uint64>(stateHash, 7);

	stateHash += lcr.GetRawView()[mmVGT_PRIMITIVE_TYPE];
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

	return stateHash;
}
