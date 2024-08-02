#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalLayer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"

#include "Cafe/HW/Latte/Renderer/Metal/ShaderSourcePresent.h"

#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteIndices.h"
#include "Cemu/Logging/CemuDebugLogging.h"
#include "Foundation/NSTypes.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "gui/guiWrapper.h"

extern bool hasValidFramebufferAttached;

float supportBufferData[512 * 4];

MetalRenderer::MetalRenderer()
{
    m_device = MTL::CreateSystemDefaultDevice();
    m_commandQueue = m_device->newCommandQueue();

    MTL::SamplerDescriptor* samplerDescriptor = MTL::SamplerDescriptor::alloc()->init();
    m_nearestSampler = m_device->newSamplerState(samplerDescriptor);

    m_memoryManager = new MetalMemoryManager(this);

    // Initialize state
    for (uint32 i = 0; i < (uint32)LatteConst::ShaderType::TotalCount; i++)
    {
        for (uint32 j = 0; j < MAX_MTL_BUFFERS; j++)
        {
            m_state.uniformBufferOffsets[i][j] = INVALID_OFFSET;
        }
    }
}

MetalRenderer::~MetalRenderer()
{
    delete m_memoryManager;

    m_commandQueue->release();
    m_device->release();
}

// TODO: don't ignore "mainWindow" argument
void MetalRenderer::InitializeLayer(const Vector2i& size, bool mainWindow)
{
    const auto& windowInfo = gui_getWindowInfo().window_main;

    m_metalLayer = (CA::MetalLayer*)CreateMetalLayer(windowInfo.handle);
    m_metalLayer->setDevice(m_device);

    // Present pipeline
    NS::Error* error = nullptr;
	MTL::Library* presentLibrary = m_device->newLibrary(NS::String::string(presentLibrarySource, NS::ASCIIStringEncoding), nullptr, &error);
	if (error)
    {
        debug_printf("failed to create present library (error: %s)\n", error->localizedDescription()->utf8String());
        error->release();
        throw;
        return;
    }
    MTL::Function* presentVertexFunction = presentLibrary->newFunction(NS::String::string("presentVertex", NS::ASCIIStringEncoding));
    MTL::Function* presentFragmentFunction = presentLibrary->newFunction(NS::String::string("presentFragment", NS::ASCIIStringEncoding));
    presentLibrary->release();

    MTL::RenderPipelineDescriptor* renderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    renderPipelineDescriptor->setVertexFunction(presentVertexFunction);
    renderPipelineDescriptor->setFragmentFunction(presentFragmentFunction);
    renderPipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(m_metalLayer->pixelFormat());
    m_presentPipeline = m_device->newRenderPipelineState(renderPipelineDescriptor, &error);
    presentVertexFunction->release();
    presentFragmentFunction->release();
    if (error)
    {
        debug_printf("failed to create present pipeline (error: %s)\n", error->localizedDescription()->utf8String());
        error->release();
        throw;
        return;
    }
}

void MetalRenderer::Initialize()
{
}

void MetalRenderer::Shutdown()
{
    debug_printf("MetalRenderer::Shutdown not implemented\n");
}

bool MetalRenderer::IsPadWindowActive()
{
    debug_printf("MetalRenderer::IsPadWindowActive not implemented\n");

    return false;
}

bool MetalRenderer::GetVRAMInfo(int& usageInMB, int& totalInMB) const
{
    debug_printf("MetalRenderer::GetVRAMInfo not implemented\n");

    usageInMB = 1024;
    totalInMB = 1024;

    return false;
}

void MetalRenderer::ClearColorbuffer(bool padView)
{
    debug_printf("MetalRenderer::ClearColorbuffer not implemented\n");
}

void MetalRenderer::DrawEmptyFrame(bool mainWindow)
{
    debug_printf("MetalRenderer::DrawEmptyFrame not implemented\n");
}

void MetalRenderer::SwapBuffers(bool swapTV, bool swapDRC)
{
    EndEncoding();

    if (m_drawable)
    {
        EnsureCommandBuffer();
        m_commandBuffer->presentDrawable(m_drawable);
    } else
    {
        debug_printf("skipped present!\n");
    }
    m_drawable = nullptr;

    CommitCommandBuffer();
}

void MetalRenderer::DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter,
								sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight,
								bool padView, bool clearBackground)
{
    // Acquire drawable
    m_drawable = m_metalLayer->nextDrawable();
    if (!m_drawable)
    {
        return;
    }

    MTL::Texture* presentTexture = static_cast<LatteTextureViewMtl*>(texView)->GetTexture();

    // Create render pass
    MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    renderPassDescriptor->colorAttachments()->object(0)->setTexture(m_drawable->texture());

    MTL::Texture* colorRenderTargets[8] = {nullptr};
    colorRenderTargets[0] = m_drawable->texture();
    // If there was already an encoder with these attachment, we should set the viewport and scissor to default, but that shouldn't happen
    auto renderCommandEncoder = GetRenderCommandEncoder(renderPassDescriptor, colorRenderTargets, nullptr, false);

    // Draw to Metal layer
    renderCommandEncoder->setRenderPipelineState(m_presentPipeline);
    renderCommandEncoder->setFragmentTexture(presentTexture, 0);
    renderCommandEncoder->setFragmentSamplerState(m_nearestSampler, 0);

    renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
}

bool MetalRenderer::BeginFrame(bool mainWindow)
{
    // TODO
    return false;
}

void MetalRenderer::Flush(bool waitIdle)
{
    debug_printf("MetalRenderer::Flush not implemented\n");
}

void MetalRenderer::NotifyLatteCommandProcessorIdle()
{
    // TODO: should we?
    CommitCommandBuffer();
}

void MetalRenderer::AppendOverlayDebugInfo()
{
    debug_printf("MetalRenderer::AppendOverlayDebugInfo not implemented\n");
}

void MetalRenderer::renderTarget_setViewport(float x, float y, float width, float height, float nearZ, float farZ, bool halfZ)
{
    m_state.viewport = MTL::Viewport{x, y + height, width, -height, nearZ, farZ};
    if (m_encoderType == MetalEncoderType::Render)
    {
        static_cast<MTL::RenderCommandEncoder*>(m_commandEncoder)->setViewport(m_state.viewport);
    }
}

void MetalRenderer::renderTarget_setScissor(sint32 scissorX, sint32 scissorY, sint32 scissorWidth, sint32 scissorHeight)
{
    m_state.scissor = MTL::ScissorRect{NS::UInteger(scissorX), NS::UInteger(scissorY), NS::UInteger(scissorWidth), NS::UInteger(scissorHeight)};
    if (m_encoderType == MetalEncoderType::Render)
    {
        static_cast<MTL::RenderCommandEncoder*>(m_commandEncoder)->setScissorRect(m_state.scissor);
    }
}

LatteCachedFBO* MetalRenderer::rendertarget_createCachedFBO(uint64 key)
{
	return new CachedFBOMtl(key);
}

void MetalRenderer::rendertarget_deleteCachedFBO(LatteCachedFBO* cfbo)
{
	if (cfbo == (LatteCachedFBO*)m_state.activeFBO)
		m_state.activeFBO = nullptr;
}

void MetalRenderer::rendertarget_bindFramebufferObject(LatteCachedFBO* cfbo)
{
	m_state.activeFBO = (CachedFBOMtl*)cfbo;
}

void* MetalRenderer::texture_acquireTextureUploadBuffer(uint32 size)
{
    return m_memoryManager->GetTextureUploadBuffer(size);
}

void MetalRenderer::texture_releaseTextureUploadBuffer(uint8* mem)
{
    debug_printf("MetalRenderer::texture_releaseTextureUploadBuffer not implemented\n");
}

TextureDecoder* MetalRenderer::texture_chooseDecodedFormat(Latte::E_GX2SURFFMT format, bool isDepth, Latte::E_DIM dim, uint32 width, uint32 height)
{
    // TODO: move to LatteToMtl
    if (isDepth)
    {
    	switch (format)
    	{
    	case Latte::E_GX2SURFFMT::D24_S8_UNORM:
    		return TextureDecoder_D24_S8::getInstance();
    	case Latte::E_GX2SURFFMT::D24_S8_FLOAT:
    		return TextureDecoder_NullData64::getInstance();
    	case Latte::E_GX2SURFFMT::D32_FLOAT:
    		return TextureDecoder_R32_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::D16_UNORM:
    		return TextureDecoder_R16_UNORM::getInstance();
    	case Latte::E_GX2SURFFMT::D32_S8_FLOAT:
    		return TextureDecoder_D32_S8_UINT_X24::getInstance();
     	default:
    		debug_printf("invalid depth texture format %u\n", (uint32)format);
    		cemu_assert_debug(false);
    		return nullptr;
    	}
    } else
    {
        switch (format)
        {
    	case Latte::E_GX2SURFFMT::R32_G32_B32_A32_FLOAT:
    		return TextureDecoder_R32_G32_B32_A32_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R32_G32_B32_A32_UINT:
    		return TextureDecoder_R32_G32_B32_A32_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT:
    		return TextureDecoder_R16_G16_B16_A16_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_B16_A16_UINT:
    		return TextureDecoder_R16_G16_B16_A16_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM:
    		return TextureDecoder_R16_G16_B16_A16::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_B16_A16_SNORM:
    		return TextureDecoder_R16_G16_B16_A16::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM:
    		return TextureDecoder_R8_G8_B8_A8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_B8_A8_SNORM:
    		return TextureDecoder_R8_G8_B8_A8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB:
    		return TextureDecoder_R8_G8_B8_A8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_B8_A8_UINT:
    		return TextureDecoder_R8_G8_B8_A8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_B8_A8_SINT:
    		return TextureDecoder_R8_G8_B8_A8::getInstance();
    	case Latte::E_GX2SURFFMT::R32_G32_FLOAT:
    		return TextureDecoder_R32_G32_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R32_G32_UINT:
    		return TextureDecoder_R32_G32_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_UNORM:
    		return TextureDecoder_R16_G16::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_FLOAT:
    		return TextureDecoder_R16_G16_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_UNORM:
    		return TextureDecoder_R8_G8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_SNORM:
    		return TextureDecoder_R8_G8::getInstance();
    	case Latte::E_GX2SURFFMT::R4_G4_UNORM:
    		return TextureDecoder_R4_G4::getInstance();
    	case Latte::E_GX2SURFFMT::R32_FLOAT:
    		return TextureDecoder_R32_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R32_UINT:
    		return TextureDecoder_R32_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_FLOAT:
    		return TextureDecoder_R16_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_UNORM:
    		return TextureDecoder_R16_UNORM::getInstance();
    	case Latte::E_GX2SURFFMT::R16_SNORM:
    		return TextureDecoder_R16_SNORM::getInstance();
    	case Latte::E_GX2SURFFMT::R16_UINT:
    		return TextureDecoder_R16_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R8_UNORM:
    		return TextureDecoder_R8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_SNORM:
    		return TextureDecoder_R8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_UINT:
    		return TextureDecoder_R8_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R5_G6_B5_UNORM:
    		return TextureDecoder_R5_G6_B5_swappedRB::getInstance();
    	case Latte::E_GX2SURFFMT::R5_G5_B5_A1_UNORM:
    		return TextureDecoder_R5_G5_B5_A1_UNORM_swappedRB::getInstance();
    	case Latte::E_GX2SURFFMT::A1_B5_G5_R5_UNORM:
    		return TextureDecoder_A1_B5_G5_R5_UNORM_vulkan::getInstance();
    	case Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT:
    		return TextureDecoder_R11_G11_B10_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R4_G4_B4_A4_UNORM:
    		return TextureDecoder_R4_G4_B4_A4_UNORM::getInstance();
    	case Latte::E_GX2SURFFMT::R10_G10_B10_A2_UNORM:
    		return TextureDecoder_R10_G10_B10_A2_UNORM::getInstance();
    	case Latte::E_GX2SURFFMT::R10_G10_B10_A2_SNORM:
    		return TextureDecoder_R10_G10_B10_A2_SNORM_To_RGBA16::getInstance();
    	case Latte::E_GX2SURFFMT::R10_G10_B10_A2_SRGB:
    		return TextureDecoder_R10_G10_B10_A2_UNORM::getInstance();
    	case Latte::E_GX2SURFFMT::BC1_SRGB:
    		return TextureDecoder_BC1::getInstance();
    	case Latte::E_GX2SURFFMT::BC1_UNORM:
    		return TextureDecoder_BC1::getInstance();
    	case Latte::E_GX2SURFFMT::BC2_UNORM:
    		return TextureDecoder_BC2::getInstance();
    	case Latte::E_GX2SURFFMT::BC2_SRGB:
    		return TextureDecoder_BC2::getInstance();
    	case Latte::E_GX2SURFFMT::BC3_UNORM:
    		return TextureDecoder_BC3::getInstance();
    	case Latte::E_GX2SURFFMT::BC3_SRGB:
    		return TextureDecoder_BC3::getInstance();
    	case Latte::E_GX2SURFFMT::BC4_UNORM:
    		return TextureDecoder_BC4::getInstance();
    	case Latte::E_GX2SURFFMT::BC4_SNORM:
    		return TextureDecoder_BC4::getInstance();
    	case Latte::E_GX2SURFFMT::BC5_UNORM:
    		return TextureDecoder_BC5::getInstance();
    	case Latte::E_GX2SURFFMT::BC5_SNORM:
    		return TextureDecoder_BC5::getInstance();
    	case Latte::E_GX2SURFFMT::R24_X8_UNORM:
    		return TextureDecoder_R24_X8::getInstance();
    	case Latte::E_GX2SURFFMT::X24_G8_UINT:
    		return TextureDecoder_X24_G8_UINT::getInstance(); // todo - verify
    	default:
    		debug_printf("invalid color texture format %u\n", (uint32)format);
    		cemu_assert_debug(false);
    		return nullptr;
    	}
    }
}

void MetalRenderer::texture_clearSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex)
{
    debug_printf("MetalRenderer::texture_clearSlice not implemented\n");
}

void MetalRenderer::texture_loadSlice(LatteTexture* hostTexture, sint32 width, sint32 height, sint32 depth, void* pixelData, sint32 sliceIndex, sint32 mipIndex, uint32 compressedImageSize)
{
    auto mtlTexture = (LatteTextureMtl*)hostTexture;

    size_t bytesPerRow = GetMtlTextureBytesPerRow(mtlTexture->GetFormat(), mtlTexture->IsDepth(), width);
    size_t bytesPerImage = GetMtlTextureBytesPerImage(mtlTexture->GetFormat(), mtlTexture->IsDepth(), height, bytesPerRow);
    mtlTexture->GetTexture()->replaceRegion(MTL::Region(0, 0, width, height), mipIndex, sliceIndex, pixelData, bytesPerRow, bytesPerImage);
}

void MetalRenderer::texture_clearColorSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex, float r, float g, float b, float a)
{
    debug_printf("MetalRenderer::texture_clearColorSlice not implemented\n");
}

void MetalRenderer::texture_clearDepthSlice(LatteTexture* hostTexture, uint32 sliceIndex, sint32 mipIndex, bool clearDepth, bool clearStencil, float depthValue, uint32 stencilValue)
{
    debug_printf("MetalRenderer::texture_clearDepthSlice not implemented\n");
}

LatteTexture* MetalRenderer::texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth)
{
    return new LatteTextureMtl(this, dim, physAddress, physMipAddress, format, width, height, depth, pitch, mipLevels, swizzle, tileMode, isDepth);
}

void MetalRenderer::texture_setLatteTexture(LatteTextureView* textureView, uint32 textureUnit)
{
    m_state.textures[textureUnit] = static_cast<LatteTextureViewMtl*>(textureView);
}

void MetalRenderer::texture_copyImageSubData(LatteTexture* src, sint32 srcMip, sint32 effectiveSrcX, sint32 effectiveSrcY, sint32 srcSlice, LatteTexture* dst, sint32 dstMip, sint32 effectiveDstX, sint32 effectiveDstY, sint32 dstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight, sint32 srcDepth)
{
    debug_printf("MetalRenderer::texture_copyImageSubData not implemented\n");
}

LatteTextureReadbackInfo* MetalRenderer::texture_createReadback(LatteTextureView* textureView)
{
    debug_printf("MetalRenderer::texture_createReadback not implemented\n");

    return nullptr;
}

void MetalRenderer::surfaceCopy_copySurfaceWithFormatConversion(LatteTexture* sourceTexture, sint32 srcMip, sint32 srcSlice, LatteTexture* destinationTexture, sint32 dstMip, sint32 dstSlice, sint32 width, sint32 height)
{
    debug_printf("MetalRenderer::surfaceCopy_copySurfaceWithFormatConversion not implemented\n");
}

void MetalRenderer::bufferCache_init(const sint32 bufferSize)
{
    m_memoryManager->InitBufferCache(bufferSize);
}

void MetalRenderer::bufferCache_upload(uint8* buffer, sint32 size, uint32 bufferOffset)
{
    m_memoryManager->UploadToBufferCache(buffer, bufferOffset, size);
}

void MetalRenderer::bufferCache_copy(uint32 srcOffset, uint32 dstOffset, uint32 size)
{
    m_memoryManager->CopyBufferCache(srcOffset, dstOffset, size);
}

void MetalRenderer::bufferCache_copyStreamoutToMainBuffer(uint32 srcOffset, uint32 dstOffset, uint32 size)
{
    debug_printf("MetalRenderer::bufferCache_copyStreamoutToMainBuffer not implemented\n");
}

void MetalRenderer::buffer_bindVertexBuffer(uint32 bufferIndex, uint32 offset, uint32 size)
{
	if (m_state.vertexBuffers[bufferIndex].offset == offset)
		return;
	cemu_assert_debug(bufferIndex < LATTE_MAX_VERTEX_BUFFERS);
	m_state.vertexBuffers[bufferIndex].needsRebind = true;
	m_state.vertexBuffers[bufferIndex].offset = offset;
}

void MetalRenderer::buffer_bindUniformBuffer(LatteConst::ShaderType shaderType, uint32 bufferIndex, uint32 offset, uint32 size)
{
    m_state.uniformBufferOffsets[(uint32)shaderType][bufferIndex] = offset;
}

RendererShader* MetalRenderer::shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool isGameShader, bool isGfxPackShader)
{
    return new RendererShaderMtl(this, type, baseHash, auxHash, isGameShader, isGfxPackShader, source);
}

void MetalRenderer::streamout_setupXfbBuffer(uint32 bufferIndex, sint32 ringBufferOffset, uint32 rangeAddr, uint32 rangeSize)
{
    debug_printf("MetalRenderer::streamout_setupXfbBuffer not implemented\n");
}

void MetalRenderer::streamout_begin()
{
    debug_printf("MetalRenderer::streamout_begin not implemented\n");
}

void MetalRenderer::streamout_rendererFinishDrawcall()
{
    debug_printf("MetalRenderer::streamout_rendererFinishDrawcall not implemented\n");
}

void MetalRenderer::draw_beginSequence()
{
    m_state.skipDrawSequence = false;

    // update shader state
	LatteSHRC_UpdateActiveShaders();
	if (LatteGPUState.activeShaderHasError)
	{
		debug_printf("Skipping drawcalls due to shader error\n");
		m_state.skipDrawSequence = true;
		cemu_assert_debug(false);
		return;
	}

	// update render target and texture state
	LatteGPUState.requiresTextureBarrier = false;
	while (true)
	{
		LatteGPUState.repeatTextureInitialization = false;
		if (!LatteMRT::UpdateCurrentFBO())
		{
			debug_printf("Rendertarget invalid\n");
			m_state.skipDrawSequence = true;
			return; // no render target
		}

		if (!hasValidFramebufferAttached)
		{
			debug_printf("Drawcall with no color buffer or depth buffer attached\n");
			m_state.skipDrawSequence = true;
			return; // no render target
		}
		LatteTexture_updateTextures();
		if (!LatteGPUState.repeatTextureInitialization)
			break;
	}

	// apply render target
	LatteMRT::ApplyCurrentState();

	// viewport and scissor box
	LatteRenderTarget_updateViewport();
	LatteRenderTarget_updateScissorBox();

	// check for conditions which would turn the drawcalls into no-ops
	bool rasterizerEnable = LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_DX_RASTERIZATION_KILL() == false;

	// GX2SetSpecialState(0, true) enables DX_RASTERIZATION_KILL, but still expects depth writes to happen? -> Research which stages are disabled by DX_RASTERIZATION_KILL exactly
	// for now we use a workaround:
	if (!LatteGPUState.contextNew.PA_CL_VTE_CNTL.get_VPORT_X_OFFSET_ENA())
		rasterizerEnable = true;

	if (!rasterizerEnable == false)
		m_state.skipDrawSequence = true;
}

void MetalRenderer::draw_execute(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, bool isFirst)
{
    //if (m_state.skipDrawSequence)
	//{
	//	return;
	//}

	// Render pass
	if (!m_state.activeFBO)
	{
	    debug_printf("no active FBO, skipping draw\n");
	    return;
	}

	auto renderPassDescriptor = m_state.activeFBO->GetRenderPassDescriptor();
	MTL::Texture* colorRenderTargets[8] = {nullptr};
	MTL::Texture* depthRenderTarget = nullptr;
	for (uint32 i = 0; i < 8; i++)
    {
        auto colorTexture = static_cast<LatteTextureViewMtl*>(m_state.activeFBO->colorBuffer[i].texture);
        if (colorTexture)
        {
            colorRenderTargets[i] = colorTexture->GetTexture();
        }
    }
    auto depthTexture = static_cast<LatteTextureViewMtl*>(m_state.activeFBO->depthBuffer.texture);
    if (depthTexture)
    {
        depthRenderTarget = depthTexture->GetTexture();
    }
	auto renderCommandEncoder = GetRenderCommandEncoder(renderPassDescriptor, colorRenderTargets, depthRenderTarget);

	// Shaders
	LatteSHRC_UpdateActiveShaders();
	LatteDecompilerShader* vertexShader = LatteSHRC_GetActiveVertexShader();
	LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
	if (!vertexShader || !static_cast<RendererShaderMtl*>(vertexShader->shader)->GetFunction())
	{
        debug_printf("no vertex function, skipping draw\n");
	    return;
	}

	auto fetchShader = vertexShader->compatibleFetchShader;

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
		// TODO: is LatteGPUState.contextNew correct?
		uint32 bufferStride = (LatteGPUState.contextNew.GetRawView()[bufferBaseRegisterIndex + 2] >> 11) & 0xFFFF;

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
	MTL::RenderPipelineDescriptor* renderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
	renderPipelineDescriptor->setVertexFunction(static_cast<RendererShaderMtl*>(vertexShader->shader)->GetFunction());
	renderPipelineDescriptor->setFragmentFunction(static_cast<RendererShaderMtl*>(pixelShader->shader)->GetFunction());
	// TODO: don't always set the vertex descriptor
	renderPipelineDescriptor->setVertexDescriptor(vertexDescriptor);
	for (uint8 i = 0; i < 8; i++)
	{
	    const auto& colorBuffer = m_state.activeFBO->colorBuffer[i];
		auto texture = static_cast<LatteTextureViewMtl*>(colorBuffer.texture);
		if (!texture)
		{
		    continue;
		}
		auto colorAttachment = renderPipelineDescriptor->colorAttachments()->object(i);
		colorAttachment->setPixelFormat(texture->GetTexture()->pixelFormat());

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
	if (m_state.activeFBO->depthBuffer.texture)
	{
	    auto texture = static_cast<LatteTextureViewMtl*>(m_state.activeFBO->depthBuffer.texture);
        renderPipelineDescriptor->setDepthAttachmentPixelFormat(texture->GetTexture()->pixelFormat());
	}

	NS::Error* error = nullptr;
	MTL::RenderPipelineState* renderPipelineState = m_device->newRenderPipelineState(renderPipelineDescriptor, &error);
	if (error)
	{
	    debug_printf("error creating render pipeline state: %s\n", error->localizedDescription()->utf8String());
		return;
	}
	renderCommandEncoder->setRenderPipelineState(renderPipelineState);

	// Primitive type
	const LattePrimitiveMode primitiveMode = static_cast<LattePrimitiveMode>(LatteGPUState.contextRegister[mmVGT_PRIMITIVE_TYPE]);
	auto mtlPrimitiveType = GetMtlPrimitiveType(primitiveMode);

	// Resources

	// Index buffer
	Renderer::INDEX_TYPE hostIndexType;
	uint32 hostIndexCount;
	uint32 indexMin = 0;
	uint32 indexMax = 0;
	uint32 indexBufferOffset = 0;
	uint32 indexBufferIndex = 0;
	LatteIndices_decode(memory_getPointerFromVirtualOffset(indexDataMPTR), indexType, count, primitiveMode, indexMin, indexMax, hostIndexType, hostIndexCount, indexBufferOffset, indexBufferIndex);

	// synchronize vertex and uniform cache and update buffer bindings
	LatteBufferCache_Sync(indexMin + baseVertex, indexMax + baseVertex, baseInstance, instanceCount);

	// Vertex buffers
	for (uint8 i = 0; i < MAX_MTL_BUFFERS; i++)
	{
	    auto& vertexBufferRange = m_state.vertexBuffers[i];
	    if (vertexBufferRange.needsRebind)
        {
            renderCommandEncoder->setVertexBuffer(m_memoryManager->GetBufferCache(), vertexBufferRange.offset, GET_MTL_VERTEX_BUFFER_INDEX(i));
            vertexBufferRange.needsRebind = false;
        }
	}

	// Uniform buffers, textures and samplers
	BindStageResources(renderCommandEncoder, vertexShader);
	BindStageResources(renderCommandEncoder, pixelShader);

	// Draw
	if (hostIndexType != INDEX_TYPE::NONE)
	{
	    auto mtlIndexType = GetMtlIndexType(hostIndexType);
		MTL::Buffer* indexBuffer = m_memoryManager->GetBuffer(indexBufferIndex);
		renderCommandEncoder->drawIndexedPrimitives(mtlPrimitiveType, hostIndexCount, mtlIndexType, indexBuffer, 0, instanceCount, baseVertex, baseInstance);
	} else
	{
		renderCommandEncoder->drawPrimitives(mtlPrimitiveType, baseVertex, count, instanceCount, baseInstance);
	}
}

void MetalRenderer::draw_endSequence()
{
    // TODO: do something?
}

void* MetalRenderer::indexData_reserveIndexMemory(uint32 size, uint32& offset, uint32& bufferIndex)
{
    auto allocation = m_memoryManager->GetBufferAllocation(size);
	offset = allocation.bufferOffset;
	bufferIndex = allocation.bufferIndex;

	return allocation.data;
}

void MetalRenderer::indexData_uploadIndexMemory(uint32 offset, uint32 size)
{
    debug_printf("MetalRenderer::indexData_uploadIndexMemory not implemented\n");
}

void MetalRenderer::BindStageResources(MTL::RenderCommandEncoder* renderCommandEncoder, LatteDecompilerShader* shader)
{
    sint32 textureCount = shader->resourceMapping.getTextureCount();

	for (int i = 0; i < textureCount; ++i)
	{
		const auto relative_textureUnit = shader->resourceMapping.getTextureUnitFromBindingPoint(i);
		auto hostTextureUnit = relative_textureUnit;
		auto textureDim = shader->textureUnitDim[relative_textureUnit];
		auto texUnitRegIndex = hostTextureUnit * 7;

		auto textureView = m_state.textures[hostTextureUnit];
		if (!textureView)
		{
		    debug_printf("invalid bound texture view %u\n", hostTextureUnit);
			continue;
		}

		//LatteTexture* baseTexture = textureView->baseTexture;
		// get texture register word 0
		uint32 word4 = LatteGPUState.contextRegister[texUnitRegIndex + 4];

		// TODO: wht
		//auto imageViewObj = textureView->GetSamplerView(word4);
		//info.imageView = imageViewObj->m_textureImageView;

		// HACK
		uint32 textureBinding = (shader->resourceMapping.getTextureBaseBindingPoint() + i) % MAX_MTL_TEXTURES;
		uint32 samplerBinding = textureBinding % MAX_MTL_SAMPLERS;

		uint32 stageSamplerIndex = shader->textureUnitSamplerAssignment[relative_textureUnit];
		if (stageSamplerIndex != LATTE_DECOMPILER_SAMPLER_NONE)
		{
			// TODO: bind the actual sampler
			MTL::SamplerState* sampler = m_nearestSampler;
			switch (shader->shaderType)
			{
			case LatteConst::ShaderType::Vertex:
			{
				renderCommandEncoder->setVertexSamplerState(sampler, samplerBinding);
				break;
			}
			case LatteConst::ShaderType::Pixel:
			{
			    renderCommandEncoder->setFragmentSamplerState(sampler, samplerBinding);
				break;
			}
			default:
				UNREACHABLE;
			}
		}

		switch (shader->shaderType)
		{
		case LatteConst::ShaderType::Vertex:
		{
			renderCommandEncoder->setVertexTexture(textureView->GetTexture(), textureBinding);
			break;
		}
		case LatteConst::ShaderType::Pixel:
		{
		    renderCommandEncoder->setFragmentTexture(textureView->GetTexture(), textureBinding);
			break;
		}
		default:
			UNREACHABLE;
		}
	}

	// Support buffer
	auto GET_UNIFORM_DATA_PTR = [&](size_t index) { return supportBufferData + (index / 4); };

	sint32 shaderAluConst;
	sint32 shaderUniformRegisterOffset;

	switch (shader->shaderType)
	{
	case LatteConst::ShaderType::Vertex:
		shaderAluConst = 0x400;
		shaderUniformRegisterOffset = mmSQ_VTX_UNIFORM_BLOCK_START;
		break;
	case LatteConst::ShaderType::Pixel:
		shaderAluConst = 0;
		shaderUniformRegisterOffset = mmSQ_PS_UNIFORM_BLOCK_START;
		break;
	case LatteConst::ShaderType::Geometry:
		shaderAluConst = 0; // geometry shader has no ALU const
		shaderUniformRegisterOffset = mmSQ_GS_UNIFORM_BLOCK_START;
		break;
	default:
		UNREACHABLE;
	}

	//if (shader->resourceMapping.uniformVarsBufferBindingPoint >= 0)
	//{
		if (shader->uniform.list_ufTexRescale.empty() == false)
		{
			for (auto& entry : shader->uniform.list_ufTexRescale)
			{
				float* xyScale = LatteTexture_getEffectiveTextureScale(shader->shaderType, entry.texUnit);
				memcpy(entry.currentValue, xyScale, sizeof(float) * 2);
				memcpy(GET_UNIFORM_DATA_PTR(entry.uniformLocation), xyScale, sizeof(float) * 2);
			}
		}
		if (shader->uniform.loc_alphaTestRef >= 0)
		{
			*GET_UNIFORM_DATA_PTR(shader->uniform.loc_alphaTestRef) = LatteGPUState.contextNew.SX_ALPHA_REF.get_ALPHA_TEST_REF();
		}
		if (shader->uniform.loc_pointSize >= 0)
		{
			const auto& pointSizeReg = LatteGPUState.contextNew.PA_SU_POINT_SIZE;
			float pointWidth = (float)pointSizeReg.get_WIDTH() / 8.0f;
			if (pointWidth == 0.0f)
				pointWidth = 1.0f / 8.0f; // minimum size
			*GET_UNIFORM_DATA_PTR(shader->uniform.loc_pointSize) = pointWidth;
		}
		if (shader->uniform.loc_remapped >= 0)
		{
			LatteBufferCache_LoadRemappedUniforms(shader, GET_UNIFORM_DATA_PTR(shader->uniform.loc_remapped));
		}
		if (shader->uniform.loc_uniformRegister >= 0)
		{
			uint32* uniformRegData = (uint32*)(LatteGPUState.contextRegister + mmSQ_ALU_CONSTANT0_0 + shaderAluConst);
			memcpy(GET_UNIFORM_DATA_PTR(shader->uniform.loc_uniformRegister), uniformRegData, shader->uniform.count_uniformRegister * 16);
		}
		if (shader->uniform.loc_windowSpaceToClipSpaceTransform >= 0)
		{
			sint32 viewportWidth;
			sint32 viewportHeight;
			LatteRenderTarget_GetCurrentVirtualViewportSize(&viewportWidth, &viewportHeight); // always call after _updateViewport()
			float* v = GET_UNIFORM_DATA_PTR(shader->uniform.loc_windowSpaceToClipSpaceTransform);
			v[0] = 2.0f / (float)viewportWidth;
			v[1] = 2.0f / (float)viewportHeight;
		}
		if (shader->uniform.loc_fragCoordScale >= 0)
		{
			LatteMRT::GetCurrentFragCoordScale(GET_UNIFORM_DATA_PTR(shader->uniform.loc_fragCoordScale));
		}
		// TODO: uncomment?
		/*
		if (shader->uniform.loc_verticesPerInstance >= 0)
		{
			*(int*)(supportBufferData + ((size_t)shader->uniform.loc_verticesPerInstance / 4)) = m_streamoutState.verticesPerInstance;
			for (sint32 b = 0; b < LATTE_NUM_STREAMOUT_BUFFER; b++)
			{
				if (shader->uniform.loc_streamoutBufferBase[b] >= 0)
				{
					*(uint32*)GET_UNIFORM_DATA_PTR(shader->uniform.loc_streamoutBufferBase[b]) = m_streamoutState.buffer[b].ringBufferOffset;
				}
			}
		}
		*/

		switch (shader->shaderType)
		{
		case LatteConst::ShaderType::Vertex:
		{
			renderCommandEncoder->setVertexBytes(supportBufferData, sizeof(supportBufferData), MTL_SUPPORT_BUFFER_BINDING);
			break;
		}
		case LatteConst::ShaderType::Pixel:
		{
		    renderCommandEncoder->setFragmentBytes(supportBufferData, sizeof(supportBufferData), MTL_SUPPORT_BUFFER_BINDING);
			break;
		}
		default:
			UNREACHABLE;
		}
	//}

	// Uniform buffers
	for (sint32 i = 0; i < LATTE_NUM_MAX_UNIFORM_BUFFERS; i++)
	{
		if (shader->resourceMapping.uniformBuffersBindingPoint[i] >= 0)
		{
			uint32 binding = shader->resourceMapping.uniformBuffersBindingPoint[i];
			if (binding >= MAX_MTL_BUFFERS)
			{
			    debug_printf("too big buffer index (%u), skipping binding\n", binding);
				continue;
			}
			size_t offset = m_state.uniformBufferOffsets[(uint32)shader->shaderType][binding];
			if (offset != INVALID_OFFSET)
			{
    			switch (shader->shaderType)
    			{
    			case LatteConst::ShaderType::Vertex:
    			{
    				renderCommandEncoder->setVertexBuffer(m_memoryManager->GetBufferCache(), offset, binding);
    				break;
    			}
    			case LatteConst::ShaderType::Pixel:
    			{
    			    renderCommandEncoder->setFragmentBuffer(m_memoryManager->GetBufferCache(), offset, binding);
    				break;
    			}
    			default:
    				UNREACHABLE;
    			}
			}
		}
	}
}

void MetalRenderer::RebindRenderState(MTL::RenderCommandEncoder* renderCommandEncoder)
{
    // Viewport
    //if (m_state.viewport.width != 0.0)
    //{
    renderCommandEncoder->setViewport(m_state.viewport);
    /*
    }
    else
    {
        // Find the framebuffer dimensions
        uint32 framebufferWidth = 0, framebufferHeight = 0;
        if (m_state.activeFBO->hasDepthBuffer())
        {
            framebufferHeight = m_state.activeFBO->depthBuffer.texture->baseTexture->width;
            framebufferHeight = m_state.activeFBO->depthBuffer.texture->baseTexture->height;
        }
        else
        {
            for (uint8 i = 0; i < 8; i++)
            {
                auto texture = m_state.activeFBO->colorBuffer[i].texture;
                if (texture)
                {
                    framebufferWidth = texture->baseTexture->width;
                    framebufferHeight = texture->baseTexture->height;
                    break;
                }
            }
        }

        MTL::Viewport viewport{0, (double)framebufferHeight, (double)framebufferWidth, -(double)framebufferHeight, 0.0, 1.0};
        renderCommandEncoder->setViewport(viewport);
    }
    */

    // Scissor
    //if (m_state.scissor.width != 0)
    //{
    renderCommandEncoder->setScissorRect(m_state.scissor);
    //}

    // Vertex buffers
	for (uint8 i = 0; i < MAX_MTL_BUFFERS; i++)
	{
	    auto& vertexBufferRange = m_state.vertexBuffers[i];
	    if (vertexBufferRange.offset != INVALID_OFFSET)
        {
            renderCommandEncoder->setVertexBuffer(m_memoryManager->GetBufferCache(), vertexBufferRange.offset, GET_MTL_VERTEX_BUFFER_INDEX(i));
            vertexBufferRange.needsRebind = false;
        }
	}
}
