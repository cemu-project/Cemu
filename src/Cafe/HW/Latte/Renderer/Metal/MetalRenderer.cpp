#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalLayer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalPipelineCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalDepthStencilCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureReadbackMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"

#include "Cafe/HW/Latte/Renderer/Metal/ShaderSourcePresent.h"

#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteIndices.h"
#include "Cemu/Logging/CemuDebugLogging.h"
#include "HW/Latte/Core/Latte.h"
#include "HW/Latte/ISA/LatteReg.h"
#include "Metal/MTLTypes.hpp"
#include "gui/guiWrapper.h"

extern bool hasValidFramebufferAttached;

float supportBufferData[512 * 4];

MetalRenderer::MetalRenderer()
{
    m_device = MTL::CreateSystemDefaultDevice();
    m_commandQueue = m_device->newCommandQueue();

    MTL::SamplerDescriptor* samplerDescriptor = MTL::SamplerDescriptor::alloc()->init();
    m_nearestSampler = m_device->newSamplerState(samplerDescriptor);
    samplerDescriptor->release();

    m_memoryManager = new MetalMemoryManager(this);
    m_pipelineCache = new MetalPipelineCache(this);
    m_depthStencilCache = new MetalDepthStencilCache(this);

    // Texture readback
    m_readbackBuffer = m_device->newBuffer(TEXTURE_READBACK_SIZE, MTL::StorageModeShared);

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
    delete m_depthStencilCache;
    delete m_pipelineCache;
    delete m_memoryManager;

    m_nearestSampler->release();

    m_readbackBuffer->release();

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
    renderPipelineDescriptor->release();
    presentVertexFunction->release();
    presentFragmentFunction->release();
    if (error)
    {
        debug_printf("failed to create present pipeline (error: %s)\n", error->localizedDescription()->utf8String());
        error->release();
        return;
    }
}

void MetalRenderer::Initialize()
{
    Renderer::Initialize();
}

void MetalRenderer::Shutdown()
{
    Renderer::Shutdown();
    CommitCommandBuffer();
}

// TODO: what should this do?
bool MetalRenderer::IsPadWindowActive()
{
    //debug_printf("MetalRenderer::IsPadWindowActive not implemented\n");

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
    if (!AcquireNextDrawable())
        return;

    ClearColorTextureInternal(m_drawable->texture(), 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
}

void MetalRenderer::DrawEmptyFrame(bool mainWindow)
{
    if (!BeginFrame(mainWindow))
		return;
	SwapBuffers(mainWindow, !mainWindow);
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

    // Reset temporary buffers
    m_memoryManager->ResetTemporaryBuffers();
}

void MetalRenderer::DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter,
								sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight,
								bool padView, bool clearBackground)
{
    if (!AcquireNextDrawable())
        return;

    MTL::Texture* presentTexture = static_cast<LatteTextureViewMtl*>(texView)->GetRGBAView();

    // Create render pass
    MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    renderPassDescriptor->colorAttachments()->object(0)->setTexture(m_drawable->texture());

    MTL::Texture* colorRenderTargets[8] = {nullptr};
    colorRenderTargets[0] = m_drawable->texture();
    // If there was already an encoder with these attachment, we should set the viewport and scissor to default, but that shouldn't happen
    auto renderCommandEncoder = GetRenderCommandEncoder(renderPassDescriptor, colorRenderTargets, nullptr, false, false);
    renderPassDescriptor->release();

    // Draw to Metal layer
    renderCommandEncoder->setRenderPipelineState(m_presentPipeline);
    renderCommandEncoder->setFragmentTexture(presentTexture, 0);
    renderCommandEncoder->setFragmentSamplerState(m_nearestSampler, 0);

    renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
}

bool MetalRenderer::BeginFrame(bool mainWindow)
{
    return AcquireNextDrawable();
}

void MetalRenderer::Flush(bool waitIdle)
{
    // TODO: should we?
    CommitCommandBuffer();
    if (waitIdle)
    {
        // TODO
    }
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
    m_state.viewport = MTL::Viewport{x, y, width, height, nearZ, farZ};
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
    // TODO: should the texture buffer get released?
}

TextureDecoder* MetalRenderer::texture_chooseDecodedFormat(Latte::E_GX2SURFFMT format, bool isDepth, Latte::E_DIM dim, uint32 width, uint32 height)
{
    return GetMtlTextureDecoder(format, isDepth);
}

void MetalRenderer::texture_clearSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex)
{
    if (hostTexture->isDepth)
    {
        texture_clearDepthSlice(hostTexture, sliceIndex, mipIndex, true, hostTexture->hasStencil, 0.0f, 0);
    }
    else
    {
        texture_clearColorSlice(hostTexture, sliceIndex, mipIndex, 0.0f, 0.0f, 0.0f, 0.0f);
    }
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
    auto mtlTexture = static_cast<LatteTextureMtl*>(hostTexture)->GetTexture();

    ClearColorTextureInternal(mtlTexture, sliceIndex, mipIndex, r, g, b, a);
}

void MetalRenderer::texture_clearDepthSlice(LatteTexture* hostTexture, uint32 sliceIndex, sint32 mipIndex, bool clearDepth, bool clearStencil, float depthValue, uint32 stencilValue)
{
    auto mtlTexture = static_cast<LatteTextureMtl*>(hostTexture)->GetTexture();

    MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    if (clearDepth)
    {
        auto depthAttachment = renderPassDescriptor->depthAttachment();
        depthAttachment->setTexture(mtlTexture);
        depthAttachment->setClearDepth(depthValue);
        depthAttachment->setLoadAction(MTL::LoadActionClear);
        depthAttachment->setStoreAction(MTL::StoreActionStore);
        depthAttachment->setSlice(sliceIndex);
        depthAttachment->setLevel(mipIndex);
    }
    if (clearStencil)
    {
        auto stencilAttachment = renderPassDescriptor->stencilAttachment();
        stencilAttachment->setTexture(mtlTexture);
        stencilAttachment->setClearStencil(stencilValue);
        stencilAttachment->setLoadAction(MTL::LoadActionClear);
        stencilAttachment->setStoreAction(MTL::StoreActionStore);
        stencilAttachment->setSlice(sliceIndex);
        stencilAttachment->setLevel(mipIndex);
    }

    MTL::Texture* colorRenderTargets[8] = {nullptr};
    GetRenderCommandEncoder(renderPassDescriptor, colorRenderTargets, mtlTexture, true);
    renderPassDescriptor->release();
}

LatteTexture* MetalRenderer::texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth)
{
    return new LatteTextureMtl(this, dim, physAddress, physMipAddress, format, width, height, depth, pitch, mipLevels, swizzle, tileMode, isDepth);
}

void MetalRenderer::texture_setLatteTexture(LatteTextureView* textureView, uint32 textureUnit)
{
    m_state.textures[textureUnit] = static_cast<LatteTextureViewMtl*>(textureView);
}

void MetalRenderer::texture_copyImageSubData(LatteTexture* src, sint32 srcMip, sint32 effectiveSrcX, sint32 effectiveSrcY, sint32 srcSlice, LatteTexture* dst, sint32 dstMip, sint32 effectiveDstX, sint32 effectiveDstY, sint32 dstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight, sint32 srcDepth_)
{
    auto blitCommandEncoder = GetBlitCommandEncoder();

    auto mtlSrc = static_cast<LatteTextureMtl*>(src)->GetTexture();
    auto mtlDst = static_cast<LatteTextureMtl*>(dst)->GetTexture();

    uint32 srcBaseLayer = 0;
    uint32 dstBaseLayer = 0;
    uint32 srcOffsetZ = 0;
    uint32 dstOffsetZ = 0;
    uint32 srcLayerCount = 1;
    uint32 dstLayerCount = 1;
    uint32 srcDepth = 1;
    uint32 dstDepth = 1;

	if (src->Is3DTexture())
	{
		srcOffsetZ = srcSlice;
		srcDepth = srcDepth_;
	}
	else
	{
		srcBaseLayer = srcSlice;
		srcLayerCount = srcDepth_;
	}

	if (dst->Is3DTexture())
	{
		dstOffsetZ = dstSlice;
		dstDepth = srcDepth_;
	}
	else
	{
		dstBaseLayer = dstSlice;
		dstLayerCount = srcDepth_;
	}

	// If copying whole textures, we can do a more efficient copy
    if (effectiveSrcX == 0 && effectiveSrcY == 0 && effectiveDstX == 0 && effectiveDstY == 0 &&
        effectiveCopyWidth == src->GetMipWidth(srcMip) && effectiveCopyHeight == src->GetMipHeight(srcMip) &&
        effectiveCopyWidth == dst->GetMipWidth(dstMip) && effectiveCopyHeight == dst->GetMipHeight(dstMip) &&
        srcLayerCount == dstLayerCount)
    {
        blitCommandEncoder->copyFromTexture(mtlSrc, srcSlice, srcMip, mtlDst, dstSlice, dstMip, srcLayerCount, 1);
    }
    else
    {
        if (srcLayerCount == dstLayerCount)
        {
            for (uint32 i = 0; i < srcLayerCount; i++)
            {
                blitCommandEncoder->copyFromTexture(mtlSrc, srcSlice + i, srcMip, MTL::Origin(effectiveSrcX, effectiveSrcY, srcOffsetZ), MTL::Size(effectiveCopyWidth, effectiveCopyHeight, 1), mtlDst, dstSlice + i, dstMip, MTL::Origin(effectiveDstX, effectiveDstY, dstOffsetZ));
            }
        }
        else
        {
            for (uint32 i = 0; i < std::max(srcLayerCount, dstLayerCount); i++)
            {
                if (srcLayerCount == 1)
                    srcOffsetZ++;
                else
                    srcSlice++;

                if (dstLayerCount == 1)
                    dstOffsetZ++;
                else
                    dstSlice++;

                blitCommandEncoder->copyFromTexture(mtlSrc, srcSlice, srcMip, MTL::Origin(effectiveSrcX, effectiveSrcY, srcOffsetZ), MTL::Size(effectiveCopyWidth, effectiveCopyHeight, 1), mtlDst, dstSlice, dstMip, MTL::Origin(effectiveDstX, effectiveDstY, dstOffsetZ));
            }
        }
    }
}

LatteTextureReadbackInfo* MetalRenderer::texture_createReadback(LatteTextureView* textureView)
{
    size_t uploadSize = static_cast<LatteTextureMtl*>(textureView->baseTexture)->GetTexture()->allocatedSize();

    if ((m_readbackBufferWriteOffset + uploadSize) > TEXTURE_READBACK_SIZE)
	{
		m_readbackBufferWriteOffset = 0;
	}

    auto* result = new LatteTextureReadbackInfoMtl(this, textureView, m_readbackBufferWriteOffset);
    m_readbackBufferWriteOffset += uploadSize;

	return result;
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
    cemu_assert_debug(bufferIndex < LATTE_MAX_VERTEX_BUFFERS);
    auto& buffer = m_state.vertexBuffers[bufferIndex];
	if (buffer.offset == offset && buffer.size == size)
		return;

	if (buffer.offset != INVALID_OFFSET)
	{
	    m_memoryManager->UntrackVertexBuffer(bufferIndex);
	}

	buffer.needsRebind = true;
	buffer.offset = offset;
	buffer.size = size;
	buffer.restrideInfo = {};

	m_memoryManager->TrackVertexBuffer(bufferIndex, offset, size, buffer.restrideInfo);
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
            colorRenderTargets[i] = colorTexture->GetRGBAView();
        }
    }
    auto depthTexture = static_cast<LatteTextureViewMtl*>(m_state.activeFBO->depthBuffer.texture);
    if (depthTexture)
    {
        depthRenderTarget = depthTexture->GetRGBAView();
    }
	auto renderCommandEncoder = GetRenderCommandEncoder(renderPassDescriptor, colorRenderTargets, depthRenderTarget);

	// Shaders
	LatteDecompilerShader* vertexShader = LatteSHRC_GetActiveVertexShader();
	LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
	if (!vertexShader || !static_cast<RendererShaderMtl*>(vertexShader->shader)->GetFunction())
	{
        debug_printf("no vertex function, skipping draw\n");
	    return;
	}

	auto fetchShader = vertexShader->compatibleFetchShader;

	// Render pipeline state
	MTL::RenderPipelineState* renderPipelineState = m_pipelineCache->GetPipelineState(fetchShader, vertexShader, pixelShader, m_state.activeFBO, LatteGPUState.contextNew);
	renderCommandEncoder->setRenderPipelineState(renderPipelineState);

	// Depth stencil state
	MTL::DepthStencilState* depthStencilState = m_depthStencilCache->GetDepthStencilState(LatteGPUState.contextNew);
	renderCommandEncoder->setDepthStencilState(depthStencilState);

	// Stencil reference
	bool stencilEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ENABLE();
	if (stencilEnable)
	{
	    bool backStencilEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_BACK_STENCIL_ENABLE();
		uint32 stencilRefFront = LatteGPUState.contextNew.DB_STENCILREFMASK.get_STENCILREF_F();
    	uint32 stencilRefBack = LatteGPUState.contextNew.DB_STENCILREFMASK_BF.get_STENCILREF_B();
	    if (backStencilEnable)
			renderCommandEncoder->setStencilReferenceValues(stencilRefFront, stencilRefBack);
		else
		    renderCommandEncoder->setStencilReferenceValue(stencilRefFront);
	}

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
        if (vertexBufferRange.offset != INVALID_OFFSET)
        {
            // Restride
            uint32 bufferBaseRegisterIndex = mmSQ_VTX_ATTRIBUTE_BLOCK_START + i * 7;
            uint32 bufferStride = (LatteGPUState.contextNew.GetRawView()[bufferBaseRegisterIndex + 2] >> 11) & 0xFFFF;

            auto restridedBuffer = m_memoryManager->RestrideBufferIfNeeded(i, bufferStride);

            // Bind
            if (vertexBufferRange.needsRebind)
            {
                renderCommandEncoder->setVertexBuffer(restridedBuffer.buffer, restridedBuffer.offset, GET_MTL_VERTEX_BUFFER_INDEX(i));
                vertexBufferRange.needsRebind = false;
            }
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
		renderCommandEncoder->drawIndexedPrimitives(mtlPrimitiveType, hostIndexCount, mtlIndexType, indexBuffer, indexBufferOffset, instanceCount, baseVertex, baseInstance);
	} else
	{
		renderCommandEncoder->drawPrimitives(mtlPrimitiveType, baseVertex, count, instanceCount, baseInstance);
	}
}

void MetalRenderer::draw_endSequence()
{
    LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
	// post-drawcall logic
	if (pixelShader)
		LatteRenderTarget_trackUpdates();
	bool hasReadback = LatteTextureReadback_Update();
	//m_recordedDrawcalls++;
	//if (m_recordedDrawcalls >= m_submitThreshold || hasReadback)
	//{
	//	SubmitCommandBuffer();
	//}
}

void* MetalRenderer::indexData_reserveIndexMemory(uint32 size, uint32& offset, uint32& bufferIndex)
{
    auto allocation = m_memoryManager->GetBufferAllocation(size, 4);
	offset = allocation.bufferOffset;
	bufferIndex = allocation.bufferIndex;

	return allocation.data;
}

void MetalRenderer::indexData_uploadIndexMemory(uint32 offset, uint32 size)
{
    // Do nothing, since the buffer has shared storage mode
}

void MetalRenderer::EnsureCommandBuffer()
{
    if (!m_commandBuffer)
	{
        // Debug
        m_commandQueue->insertDebugCaptureBoundary();

	    m_commandBuffer = m_commandQueue->commandBuffer();
	}
}

// Some render passes clear the attachments, forceRecreate is supposed to be used in those cases
MTL::RenderCommandEncoder* MetalRenderer::GetRenderCommandEncoder(MTL::RenderPassDescriptor* renderPassDescriptor, MTL::Texture* colorRenderTargets[8], MTL::Texture* depthRenderTarget, bool forceRecreate, bool rebindStateIfNewEncoder)
{
    EnsureCommandBuffer();

    // Check if we need to begin a new render pass
    if (m_commandEncoder)
    {
        if (!forceRecreate)
        {
            if (m_encoderType == MetalEncoderType::Render)
            {
                bool needsNewRenderPass = false;
                for (uint8 i = 0; i < 8; i++)
                {
                    if (colorRenderTargets[i] && (colorRenderTargets[i] != m_state.colorRenderTargets[i]))
                    {
                        needsNewRenderPass = true;
                        break;
                    }
                }

                if (!needsNewRenderPass)
                {
                    if (depthRenderTarget && (depthRenderTarget != m_state.depthRenderTarget))
                    {
                        needsNewRenderPass = true;
                    }
                }

                if (!needsNewRenderPass)
                {
                    return (MTL::RenderCommandEncoder*)m_commandEncoder;
                }
            }
        }

        EndEncoding();
    }

    // Update state
    for (uint8 i = 0; i < 8; i++)
    {
        m_state.colorRenderTargets[i] = colorRenderTargets[i];
    }
    m_state.depthRenderTarget = depthRenderTarget;

    auto renderCommandEncoder = m_commandBuffer->renderCommandEncoder(renderPassDescriptor);
    m_commandEncoder = renderCommandEncoder;
    m_encoderType = MetalEncoderType::Render;

    if (rebindStateIfNewEncoder)
    {
        // Rebind all the render state
        RebindRenderState(renderCommandEncoder);
    }

    return renderCommandEncoder;
}

MTL::ComputeCommandEncoder* MetalRenderer::GetComputeCommandEncoder()
{
    if (m_commandEncoder)
    {
        if (m_encoderType == MetalEncoderType::Compute)
        {
            return (MTL::ComputeCommandEncoder*)m_commandEncoder;
        }

        EndEncoding();
    }

    auto computeCommandEncoder = m_commandBuffer->computeCommandEncoder();
    m_commandEncoder = computeCommandEncoder;
    m_encoderType = MetalEncoderType::Compute;

    return computeCommandEncoder;
}

MTL::BlitCommandEncoder* MetalRenderer::GetBlitCommandEncoder()
{
    if (m_commandEncoder)
    {
        if (m_encoderType == MetalEncoderType::Blit)
        {
            return (MTL::BlitCommandEncoder*)m_commandEncoder;
        }

        EndEncoding();
    }

    auto blitCommandEncoder = m_commandBuffer->blitCommandEncoder();
    m_commandEncoder = blitCommandEncoder;
    m_encoderType = MetalEncoderType::Blit;

    return blitCommandEncoder;
}

void MetalRenderer::EndEncoding()
{
    if (m_commandEncoder)
    {
        m_commandEncoder->endEncoding();
        m_commandEncoder->release();
        m_commandEncoder = nullptr;
    }
}

void MetalRenderer::CommitCommandBuffer()
{
    EndEncoding();

    if (m_commandBuffer)
    {
        m_commandBuffer->commit();
        m_commandBuffer->release();
        m_commandBuffer = nullptr;

        // TODO: where should this be called?
        LatteTextureReadback_UpdateFinishedTransfers(false);

        // Debug
        m_commandQueue->insertDebugCaptureBoundary();
    }
}

bool MetalRenderer::AcquireNextDrawable()
{
    if (m_drawable)
    {
        // TODO: should this be true?
        return true;
    }

    m_drawable = m_metalLayer->nextDrawable();
    if (!m_drawable)
    {
        printf("failed to acquire next drawable\n");
        return false;
    }

    return true;
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
		switch (shader->shaderType)
		{
		case LatteConst::ShaderType::Vertex:
			hostTextureUnit += LATTE_CEMU_VS_TEX_UNIT_BASE;
			texUnitRegIndex += Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_VS;
			break;
		case LatteConst::ShaderType::Pixel:
			hostTextureUnit += LATTE_CEMU_PS_TEX_UNIT_BASE;
			texUnitRegIndex += Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_PS;
			break;
		case LatteConst::ShaderType::Geometry:
			hostTextureUnit += LATTE_CEMU_GS_TEX_UNIT_BASE;
			texUnitRegIndex += Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_GS;
			break;
		default:
			UNREACHABLE;
		}

		auto textureView = m_state.textures[hostTextureUnit];
		if (!textureView)
		{
		    debug_printf("invalid bound texture view %u\n", hostTextureUnit);
			continue;
		}

		LatteTexture* baseTexture = textureView->baseTexture;
		// get texture register word 0
		uint32 word4 = LatteGPUState.contextRegister[texUnitRegIndex + 4];

		// TODO: wht
		//auto imageViewObj = textureView->GetSamplerView(word4);
		//info.imageView = imageViewObj->m_textureImageView;

		// TODO: uncomment
		uint32 binding = shader->resourceMapping.getTextureBaseBindingPoint() + i;//shader->resourceMapping.textureUnitToBindingPoint[hostTextureUnit];
		//uint32 textureBinding = binding % MAX_MTL_TEXTURES;
		//uint32 samplerBinding = binding % MAX_MTL_SAMPLERS;
		if (binding >= MAX_MTL_TEXTURES)
		{
		    debug_printf("invalid texture binding %u\n", binding);
            continue;
		}

		uint32 stageSamplerIndex = shader->textureUnitSamplerAssignment[relative_textureUnit];
		if (stageSamplerIndex != LATTE_DECOMPILER_SAMPLER_NONE)
		{
    		uint32 samplerIndex = stageSamplerIndex + LatteDecompiler_getTextureSamplerBaseIndex(shader->shaderType);
    		const _LatteRegisterSetSampler* samplerWords = LatteGPUState.contextNew.SQ_TEX_SAMPLER + samplerIndex;

        	// TODO: cache this instead
            MTL::SamplerDescriptor* samplerDescriptor = MTL::SamplerDescriptor::alloc()->init();

    		// lod
    		uint32 iMinLOD = samplerWords->WORD1.get_MIN_LOD();
    		uint32 iMaxLOD = samplerWords->WORD1.get_MAX_LOD();
    		sint32 iLodBias = samplerWords->WORD1.get_LOD_BIAS();

    		// apply relative lod bias from graphic pack
    		if (baseTexture->overwriteInfo.hasRelativeLodBias)
    			iLodBias += baseTexture->overwriteInfo.relativeLodBias;
    		// apply absolute lod bias from graphic pack
    		if (baseTexture->overwriteInfo.hasLodBias)
    			iLodBias = baseTexture->overwriteInfo.lodBias;

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

    		if (baseTexture->overwriteInfo.anisotropicLevel >= 0)
    			maxAniso = baseTexture->overwriteInfo.anisotropicLevel;

    		if (maxAniso > 0)
    		{
               	samplerDescriptor->setMaxAnisotropy(1 << maxAniso);
    		}

        	// TODO: set lod bias
    		//samplerInfo.mipLodBias = (float)iLodBias / 64.0f;

    		// depth compare
    		uint8 depthCompareMode = shader->textureUsesDepthCompare[relative_textureUnit] ? 1 : 0;
    		if (depthCompareMode == 1)
    		{
                // TODO: is it okay to just cast?
    			samplerDescriptor->setCompareFunction(GetMtlCompareFunc((Latte::E_COMPAREFUNC)samplerWords->WORD0.get_DEPTH_COMPARE_FUNCTION()));
    		}

    		// border
    		auto borderType = samplerWords->WORD0.get_BORDER_COLOR_TYPE();

    		if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::TRANSPARENT_BLACK)
                samplerDescriptor->setBorderColor(MTL::SamplerBorderColorTransparentBlack);
    		else if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::OPAQUE_BLACK)
                samplerDescriptor->setBorderColor(MTL::SamplerBorderColorOpaqueBlack);
    		else if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::OPAQUE_WHITE)
                samplerDescriptor->setBorderColor(MTL::SamplerBorderColorOpaqueWhite);
    		else
    		{
               	// Metal doesn't support custom border color
                samplerDescriptor->setBorderColor(MTL::SamplerBorderColorOpaqueBlack);
    		}

            MTL::SamplerState* sampler = m_device->newSamplerState(samplerDescriptor);
            samplerDescriptor->release();

			switch (shader->shaderType)
			{
			case LatteConst::ShaderType::Vertex:
			{
				renderCommandEncoder->setVertexSamplerState(sampler, binding);
				break;
			}
			case LatteConst::ShaderType::Pixel:
			{
			    renderCommandEncoder->setFragmentSamplerState(sampler, binding);
				break;
			}
			default:
				UNREACHABLE;
			}
			sampler->release();
		}

		MTL::Texture* mtlTexture = textureView->GetSwizzledView(word4);
		switch (shader->shaderType)
		{
		case LatteConst::ShaderType::Vertex:
		{
			renderCommandEncoder->setVertexTexture(mtlTexture, binding);
			break;
		}
		case LatteConst::ShaderType::Pixel:
		{
		    renderCommandEncoder->setFragmentTexture(mtlTexture, binding);
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

	if (shader->resourceMapping.uniformVarsBufferBindingPoint >= 0)
	{
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
	}

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
    		size_t offset = m_state.uniformBufferOffsets[(uint32)shader->shaderType][i];
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

	// Storage buffer
	if (shader->resourceMapping.tfStorageBindingPoint >= 0)
	{
		debug_printf("storage buffer not implemented, index: %i\n", shader->resourceMapping.tfStorageBindingPoint);
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
            vertexBufferRange.needsRebind = true;
	}
}

void MetalRenderer::ClearColorTextureInternal(MTL::Texture* mtlTexture, sint32 sliceIndex, sint32 mipIndex, float r, float g, float b, float a)
{
    MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    auto colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
    colorAttachment->setTexture(mtlTexture);
    colorAttachment->setClearColor(MTL::ClearColor(r, g, b, a));
    colorAttachment->setLoadAction(MTL::LoadActionClear);
    colorAttachment->setStoreAction(MTL::StoreActionStore);
    colorAttachment->setSlice(sliceIndex);
    colorAttachment->setLevel(mipIndex);

    MTL::Texture* colorRenderTargets[8] = {nullptr};
    colorRenderTargets[0] = mtlTexture;
    GetRenderCommandEncoder(renderPassDescriptor, colorRenderTargets, nullptr, true);
    renderPassDescriptor->release();
}
