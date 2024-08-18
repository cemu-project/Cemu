#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalLayer.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalMemoryManager.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalPipelineCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalDepthStencilCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalSamplerCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureReadbackMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalHybridComputePipeline.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"

#include "Cafe/HW/Latte/Renderer/Metal/UtilityShaderSource.h"

#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteIndices.h"
#include "Cemu/Logging/CemuDebugLogging.h"
#include "Common/precompiled.h"
#include "gui/guiWrapper.h"

#define COMMIT_TRESHOLD 256

extern bool hasValidFramebufferAttached;

float supportBufferData[512 * 4];

MetalRenderer::MetalRenderer()
{
    m_device = MTL::CreateSystemDefaultDevice();
    m_commandQueue = m_device->newCommandQueue();

    // Resources
    MTL::SamplerDescriptor* samplerDescriptor = MTL::SamplerDescriptor::alloc()->init();
#ifdef CEMU_DEBUG_ASSERT
    samplerDescriptor->setLabel(GetLabel("Nearest sampler state", samplerDescriptor));
#endif
    m_nearestSampler = m_device->newSamplerState(samplerDescriptor);

    samplerDescriptor->setMinFilter(MTL::SamplerMinMagFilterLinear);
    samplerDescriptor->setMagFilter(MTL::SamplerMinMagFilterLinear);
#ifdef CEMU_DEBUG_ASSERT
    samplerDescriptor->setLabel(GetLabel("Linear sampler state", samplerDescriptor));
#endif
    m_linearSampler = m_device->newSamplerState(samplerDescriptor);
    samplerDescriptor->release();

    // Null resources
    MTL::TextureDescriptor* textureDescriptor = MTL::TextureDescriptor::alloc()->init();
    textureDescriptor->setTextureType(MTL::TextureType1D);
    textureDescriptor->setWidth(4);
    m_nullTexture1D = m_device->newTexture(textureDescriptor);
#ifdef CEMU_DEBUG_ASSERT
    m_nullTexture1D->setLabel(GetLabel("Null texture 1D", m_nullTexture1D));
#endif

    textureDescriptor->setTextureType(MTL::TextureType2D);
    textureDescriptor->setHeight(4);
    m_nullTexture2D = m_device->newTexture(textureDescriptor);
#ifdef CEMU_DEBUG_ASSERT
    m_nullTexture2D->setLabel(GetLabel("Null texture 2D", m_nullTexture2D));
#endif
    textureDescriptor->release();

    m_memoryManager = new MetalMemoryManager(this);
    m_pipelineCache = new MetalPipelineCache(this);
    m_depthStencilCache = new MetalDepthStencilCache(this);
    m_samplerCache = new MetalSamplerCache(this);

    // Texture readback
    m_readbackBuffer = m_device->newBuffer(TEXTURE_READBACK_SIZE, MTL::StorageModeShared);
#ifdef CEMU_DEBUG_ASSERT
    m_readbackBuffer->setLabel(GetLabel("Texture readback buffer", m_readbackBuffer));
#endif

    // Transform feedback
    m_xfbRingBuffer = m_device->newBuffer(LatteStreamout_GetRingBufferSize(), MTL::StorageModeShared);
#ifdef CEMU_DEBUG_ASSERT
    m_xfbRingBuffer->setLabel(GetLabel("Transform feedback buffer", m_xfbRingBuffer));
#endif

    // Initialize state
    for (uint32 i = 0; i < METAL_SHADER_TYPE_TOTAL; i++)
    {
        for (uint32 j = 0; j < MAX_MTL_BUFFERS; j++)
            m_state.m_uniformBufferOffsets[i][j] = INVALID_OFFSET;
    }

    // Utility shader library

    // Process the source first
    std::string processedUtilityShaderSource = utilityShaderSource;
    processedUtilityShaderSource.pop_back();
    processedUtilityShaderSource.erase(processedUtilityShaderSource.begin());
    processedUtilityShaderSource = "#include <metal_stdlib>\nusing namespace metal;\n#define GET_BUFFER_BINDING(index) (27 + index)\n#define GET_TEXTURE_BINDING(index) (29 + index)\n#define GET_SAMPLER_BINDING(index) (14 + index)\n" + processedUtilityShaderSource;

    // Create the library
    NS::Error* error = nullptr;
	MTL::Library* utilityLibrary = m_device->newLibrary(ToNSString(processedUtilityShaderSource.c_str()), nullptr, &error);
	if (error)
    {
        debug_printf("failed to create utility library (error: %s)\n", error->localizedDescription()->utf8String());
        error->release();
        throw;
        return;
    }

    // Present pipeline
    MTL::Function* presentVertexFunction = utilityLibrary->newFunction(ToNSString("vertexFullscreen"));
    MTL::Function* presentFragmentFunction = utilityLibrary->newFunction(ToNSString("fragmentPresent"));

    MTL::RenderPipelineDescriptor* renderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    renderPipelineDescriptor->setVertexFunction(presentVertexFunction);
    renderPipelineDescriptor->setFragmentFunction(presentFragmentFunction);
    presentVertexFunction->release();
    presentFragmentFunction->release();

    error = nullptr;
    renderPipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
#ifdef CEMU_DEBUG_ASSERT
    renderPipelineDescriptor->setLabel(GetLabel("Present pipeline linear", renderPipelineDescriptor));
#endif
    m_presentPipelineLinear = m_device->newRenderPipelineState(renderPipelineDescriptor, &error);
    if (error)
    {
        debug_printf("failed to create linear present pipeline (error: %s)\n", error->localizedDescription()->utf8String());
        error->release();
    }

    error = nullptr;
    renderPipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA8Unorm_sRGB);
#ifdef CEMU_DEBUG_ASSERT
    renderPipelineDescriptor->setLabel(GetLabel("Present pipeline sRGB", renderPipelineDescriptor));
#endif
    m_presentPipelineSRGB = m_device->newRenderPipelineState(renderPipelineDescriptor, &error);
    renderPipelineDescriptor->release();
    if (error)
    {
        debug_printf("failed to create sRGB present pipeline (error: %s)\n", error->localizedDescription()->utf8String());
        error->release();
    }

    // Hybrid pipelines
    m_copyTextureToTexturePipeline = new MetalHybridComputePipeline(this, utilityLibrary, "vertexCopyTextureToTexture", "kernelCopyTextureToTexture");
    m_restrideBufferPipeline = new MetalHybridComputePipeline(this, utilityLibrary, "vertexRestrideBuffer", "kernelRestrideBuffer");
    utilityLibrary->release();

    m_memoryManager->SetRestrideBufferPipeline(m_restrideBufferPipeline);
}

MetalRenderer::~MetalRenderer()
{
    delete m_copyTextureToTexturePipeline;
    delete m_restrideBufferPipeline;

    m_presentPipelineLinear->release();
    m_presentPipelineSRGB->release();

    delete m_pipelineCache;
    delete m_depthStencilCache;
    delete m_samplerCache;
    delete m_memoryManager;

    m_nullTexture1D->release();
    m_nullTexture2D->release();

    m_nearestSampler->release();
    m_linearSampler->release();

    m_readbackBuffer->release();

    m_commandQueue->release();
    m_device->release();
}

// TODO: don't ignore "mainWindow" argument
void MetalRenderer::InitializeLayer(const Vector2i& size, bool mainWindow)
{
    const auto& windowInfo = gui_getWindowInfo().window_main;

    m_metalLayer = (CA::MetalLayer*)CreateMetalLayer(windowInfo.handle, m_layerScaleX, m_layerScaleY);
    m_metalLayer->setDevice(m_device);
    m_metalLayer->setDrawableSize(CGSize{(float)size.x * m_layerScaleX, (float)size.y * m_layerScaleY});
}

// TODO: don't ignore "mainWindow" argument
void MetalRenderer::ResizeLayer(const Vector2i& size, bool mainWindow)
{
    m_metalLayer->setDrawableSize(CGSize{(float)size.x * m_layerScaleX, (float)size.y * m_layerScaleY});
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
    if (!AcquireNextDrawable(!padView))
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

    if (m_drawable)
    {
        auto commandBuffer = GetCommandBuffer();
        commandBuffer->presentDrawable(m_drawable);
    }
    else
    {
        debug_printf("skipped present!\n");
    }
    m_drawable = nullptr;

    // Release all the command buffers
    CommitCommandBuffer();
    for (uint32 i = 0; i < m_commandBuffers.size(); i++)
        m_commandBuffers[i].m_commandBuffer->release();
    m_commandBuffers.clear();

    // Release frame persistent buffers
    m_memoryManager->GetFramePersistentBufferAllocator().ResetAllocations();
}

// TODO: use `shader` for drawing
void MetalRenderer::DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter,
								sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight,
								bool padView, bool clearBackground)
{
    if (!AcquireNextDrawable(!padView))
        return;

    MTL::Texture* presentTexture = static_cast<LatteTextureViewMtl*>(texView)->GetRGBAView();

    // Create render pass
    MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    auto colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
    colorAttachment->setTexture(m_drawable->texture());
    // TODO: shouldn't it be LoadActionLoad when not clearing?
    colorAttachment->setLoadAction(clearBackground ? MTL::LoadActionClear : MTL::LoadActionDontCare);
    colorAttachment->setStoreAction(MTL::StoreActionStore);

    auto renderCommandEncoder = GetTemporaryRenderCommandEncoder(renderPassDescriptor);
    renderPassDescriptor->release();

    // Draw to Metal layer
    renderCommandEncoder->setRenderPipelineState(m_state.m_usesSRGB ? m_presentPipelineSRGB : m_presentPipelineLinear);
    renderCommandEncoder->setFragmentTexture(presentTexture, 0);
    renderCommandEncoder->setFragmentSamplerState((useLinearTexFilter ? m_linearSampler : m_nearestSampler), 0);

    renderCommandEncoder->setViewport(MTL::Viewport{(double)imageX, (double)imageY, (double)imageWidth, (double)imageHeight, 0.0, 1.0});
    renderCommandEncoder->setScissorRect(MTL::ScissorRect{(uint32)imageX, (uint32)imageY, (uint32)imageWidth, (uint32)imageHeight});

    renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

    EndEncoding();
}

bool MetalRenderer::BeginFrame(bool mainWindow)
{
    return AcquireNextDrawable(mainWindow);
}

void MetalRenderer::Flush(bool waitIdle)
{
    // TODO: commit if commit on idle is requested
    if (m_recordedDrawcalls > 0)
        CommitCommandBuffer();
    if (waitIdle)
    {
        // TODO: shouldn't we wait for all command buffers?
        WaitForCommandBufferCompletion(GetCurrentCommandBuffer());
    }
}

void MetalRenderer::NotifyLatteCommandProcessorIdle()
{
    // TODO: commit if commit on idle is requested
    //CommitCommandBuffer();
}

void MetalRenderer::AppendOverlayDebugInfo()
{
    debug_printf("MetalRenderer::AppendOverlayDebugInfo not implemented\n");
}

// TODO: halfZ
void MetalRenderer::renderTarget_setViewport(float x, float y, float width, float height, float nearZ, float farZ, bool halfZ)
{
    m_state.m_viewport = MTL::Viewport{x, y, width, height, nearZ, farZ};
}

void MetalRenderer::renderTarget_setScissor(sint32 scissorX, sint32 scissorY, sint32 scissorWidth, sint32 scissorHeight)
{
    m_state.m_scissor = MTL::ScissorRect{(uint32)scissorX, (uint32)scissorY, (uint32)scissorWidth, (uint32)scissorHeight};
}

LatteCachedFBO* MetalRenderer::rendertarget_createCachedFBO(uint64 key)
{
	return new CachedFBOMtl(key);
}

void MetalRenderer::rendertarget_deleteCachedFBO(LatteCachedFBO* cfbo)
{
	if (cfbo == (LatteCachedFBO*)m_state.m_activeFBO)
	m_state.m_activeFBO = nullptr;
}

void MetalRenderer::rendertarget_bindFramebufferObject(LatteCachedFBO* cfbo)
{
	m_state.m_activeFBO = (CachedFBOMtl*)cfbo;
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
    auto textureMtl = (LatteTextureMtl*)hostTexture;

    uint32 offsetZ = 0;
    if (textureMtl->Is3DTexture())
    {
        offsetZ = sliceIndex;
        sliceIndex = 0;
    }

    size_t bytesPerRow = GetMtlTextureBytesPerRow(textureMtl->GetFormat(), textureMtl->IsDepth(), width);
    // No need to calculate bytesPerImage for 3D textures, since we always load just one slice
    //size_t bytesPerImage = GetMtlTextureBytesPerImage(textureMtl->GetFormat(), textureMtl->IsDepth(), height, bytesPerRow);
    textureMtl->GetTexture()->replaceRegion(MTL::Region(0, 0, offsetZ, width, height, 1), mipIndex, sliceIndex, pixelData, bytesPerRow, 0);
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
    if (clearStencil && GetMtlPixelFormatInfo(hostTexture->format, true).hasStencil)
    {
        auto stencilAttachment = renderPassDescriptor->stencilAttachment();
        stencilAttachment->setTexture(mtlTexture);
        stencilAttachment->setClearStencil(stencilValue);
        stencilAttachment->setLoadAction(MTL::LoadActionClear);
        stencilAttachment->setStoreAction(MTL::StoreActionStore);
        stencilAttachment->setSlice(sliceIndex);
        stencilAttachment->setLevel(mipIndex);
    }

    GetTemporaryRenderCommandEncoder(renderPassDescriptor);
    renderPassDescriptor->release();
    EndEncoding();
}

LatteTexture* MetalRenderer::texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth)
{
    return new LatteTextureMtl(this, dim, physAddress, physMipAddress, format, width, height, depth, pitch, mipLevels, swizzle, tileMode, isDepth);
}

void MetalRenderer::texture_setLatteTexture(LatteTextureView* textureView, uint32 textureUnit)
{
    m_state.m_textures[textureUnit] = static_cast<LatteTextureViewMtl*>(textureView);
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
        srcOffsetZ == 0 && dstOffsetZ == 0 &&
        effectiveCopyWidth == src->GetMipWidth(srcMip) && effectiveCopyHeight == src->GetMipHeight(srcMip) && srcDepth == src->GetMipDepth(srcMip) &&
        effectiveCopyWidth == dst->GetMipWidth(dstMip) && effectiveCopyHeight == dst->GetMipHeight(dstMip) && dstDepth == dst->GetMipDepth(dstMip) &&
        srcLayerCount == dstLayerCount)
    {
        blitCommandEncoder->copyFromTexture(mtlSrc, srcBaseLayer, srcMip, mtlDst, dstBaseLayer, dstMip, srcLayerCount, 1);
    }
    else
    {
        if (srcLayerCount == dstLayerCount)
        {
            for (uint32 i = 0; i < srcLayerCount; i++)
            {
                blitCommandEncoder->copyFromTexture(mtlSrc, srcBaseLayer + i, srcMip, MTL::Origin(effectiveSrcX, effectiveSrcY, srcOffsetZ), MTL::Size(effectiveCopyWidth, effectiveCopyHeight, srcDepth), mtlDst, dstBaseLayer + i, dstMip, MTL::Origin(effectiveDstX, effectiveDstY, dstOffsetZ));
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

                blitCommandEncoder->copyFromTexture(mtlSrc, srcBaseLayer, srcMip, MTL::Origin(effectiveSrcX, effectiveSrcY, srcOffsetZ), MTL::Size(effectiveCopyWidth, effectiveCopyHeight, 1), mtlDst, dstBaseLayer, dstMip, MTL::Origin(effectiveDstX, effectiveDstY, dstOffsetZ));
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
    GetCommandBuffer();

    // scale copy size to effective size
	sint32 effectiveCopyWidth = width;
	sint32 effectiveCopyHeight = height;
	LatteTexture_scaleToEffectiveSize(sourceTexture, &effectiveCopyWidth, &effectiveCopyHeight, 0);
	sint32 sourceEffectiveWidth, sourceEffectiveHeight;
	sourceTexture->GetEffectiveSize(sourceEffectiveWidth, sourceEffectiveHeight, srcMip);

	sint32 texSrcMip = srcMip;
	sint32 texSrcSlice = srcSlice;
	sint32 texDstMip = dstMip;
	sint32 texDstSlice = dstSlice;

	LatteTextureMtl* srcTextureMtl = static_cast<LatteTextureMtl*>(sourceTexture);
	LatteTextureMtl* dstTextureMtl = static_cast<LatteTextureMtl*>(destinationTexture);

	// check if texture rescale ratios match
	// todo - if not, we have to use drawcall based copying
	if (!LatteTexture_doesEffectiveRescaleRatioMatch(srcTextureMtl, texSrcMip, dstTextureMtl, texDstMip))
	{
		cemuLog_logDebug(LogType::Force, "surfaceCopy_copySurfaceWithFormatConversion(): Mismatching dimensions");
		return;
	}

	// check if bpp size matches
	if (srcTextureMtl->GetBPP() != dstTextureMtl->GetBPP())
	{
		cemuLog_logDebug(LogType::Force, "surfaceCopy_copySurfaceWithFormatConversion(): Mismatching BPP");
		return;
	}

	MTL::Texture* textures[] = {srcTextureMtl->GetTexture(), dstTextureMtl->GetTexture()};

	struct CopyParams
	{
	    uint32 width;
		uint32 srcMip;
		uint32 srcSlice;
		uint32 dstMip;
		uint32 dstSlice;
	} params{(uint32)effectiveCopyWidth, (uint32)texSrcMip, (uint32)texSrcSlice, (uint32)texDstMip, (uint32)texDstSlice};

	if (m_encoderType == MetalEncoderType::Render)
	{
	    auto renderCommandEncoder = static_cast<MTL::RenderCommandEncoder*>(m_commandEncoder);

		renderCommandEncoder->setRenderPipelineState(m_copyTextureToTexturePipeline->GetRenderPipelineState());
		m_state.m_encoderState.m_renderPipelineState = m_copyTextureToTexturePipeline->GetRenderPipelineState();

		renderCommandEncoder->setVertexTextures(textures, NS::Range(GET_HELPER_TEXTURE_BINDING(0), 2));
		m_state.m_encoderState.m_textures[METAL_SHADER_TYPE_VERTEX][GET_HELPER_TEXTURE_BINDING(0)] = {(LatteTextureViewMtl*)textures[0]};
		m_state.m_encoderState.m_textures[METAL_SHADER_TYPE_VERTEX][GET_HELPER_TEXTURE_BINDING(1)] = {(LatteTextureViewMtl*)textures[1]};
		renderCommandEncoder->setVertexBytes(&params, sizeof(params), GET_HELPER_BUFFER_BINDING(0));
		m_state.m_encoderState.m_uniformBufferOffsets[METAL_SHADER_TYPE_VERTEX][GET_HELPER_BUFFER_BINDING(0)] = INVALID_OFFSET;

		renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
	}
	else
	{
	    // TODO: do the copy in a compute shader
		debug_printf("surfaceCopy_copySurfaceWithFormatConversion: no active render command encoder, skipping copy\n");
	}
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
    auto& buffer = m_state.m_vertexBuffers[bufferIndex];
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

	m_memoryManager->TrackVertexBuffer(bufferIndex, offset, size, &buffer.restrideInfo);
}

void MetalRenderer::buffer_bindUniformBuffer(LatteConst::ShaderType shaderType, uint32 bufferIndex, uint32 offset, uint32 size)
{
    m_state.m_uniformBufferOffsets[GetMtlShaderType(shaderType)][bufferIndex] = offset;
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
    m_state.m_skipDrawSequence = false;

    // update shader state
	LatteSHRC_UpdateActiveShaders();
	if (LatteGPUState.activeShaderHasError)
	{
		debug_printf("Skipping drawcalls due to shader error\n");
		m_state.m_skipDrawSequence = true;
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
			m_state.m_skipDrawSequence = true;
			return; // no render target
		}

		if (!hasValidFramebufferAttached)
		{
			debug_printf("Drawcall with no color buffer or depth buffer attached\n");
			m_state.m_skipDrawSequence = true;
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
		m_state.m_skipDrawSequence = true;

	// TODO: is this even needed?
	if (!m_state.m_activeFBO)
	    m_state.m_skipDrawSequence = true;
}

void MetalRenderer::draw_execute(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, bool isFirst)
{
    // TODO: uncomment
    //if (m_state.m_skipDrawSequence)
	//{
	//  LatteGPUState.drawCallCounter++;
	//	return;
	//}

	auto& encoderState = m_state.m_encoderState;

	// Render pass
	auto renderCommandEncoder = GetRenderCommandEncoder();

	// Shaders
	LatteDecompilerShader* vertexShader = LatteSHRC_GetActiveVertexShader();
	LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
	if (!vertexShader || !static_cast<RendererShaderMtl*>(vertexShader->shader)->GetFunction())
	{
        debug_printf("no vertex function, skipping draw\n");
	    return;
	}
	const auto fetchShader = LatteSHRC_GetActiveFetchShader();

	// Depth stencil state
	// TODO: implement this somehow
	//auto depthControl = LatteGPUState.contextNew.DB_DEPTH_CONTROL;

	// Disable depth write when there is no depth attachment
	//if (!m_state.m_lastUsedFBO->depthBuffer.texture)
	//    depthControl.set_Z_WRITE_ENABLE(false);

	MTL::DepthStencilState* depthStencilState = m_depthStencilCache->GetDepthStencilState(LatteGPUState.contextNew);
	if (depthStencilState != encoderState.m_depthStencilState)
	{
	    renderCommandEncoder->setDepthStencilState(depthStencilState);
		encoderState.m_depthStencilState = depthStencilState;
	}

	// Stencil reference
	bool stencilEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ENABLE();
	if (stencilEnable)
	{
	    bool backStencilEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_BACK_STENCIL_ENABLE();
		uint32 stencilRefFront = LatteGPUState.contextNew.DB_STENCILREFMASK.get_STENCILREF_F();
    	uint32 stencilRefBack;
        if (backStencilEnable)
            stencilRefBack = LatteGPUState.contextNew.DB_STENCILREFMASK_BF.get_STENCILREF_B();
        else
            stencilRefBack = stencilRefFront;

	    if (stencilRefFront != encoderState.m_stencilRefFront || stencilRefBack != encoderState.m_stencilRefBack)
		{
		    renderCommandEncoder->setStencilReferenceValues(stencilRefFront, stencilRefBack);

			encoderState.m_stencilRefFront = stencilRefFront;
			encoderState.m_stencilRefBack = stencilRefBack;
		}
	}

    // Primitive type
    const LattePrimitiveMode primitiveMode = static_cast<LattePrimitiveMode>(LatteGPUState.contextRegister[mmVGT_PRIMITIVE_TYPE]);
    auto mtlPrimitiveType = GetMtlPrimitiveType(primitiveMode);
    bool isPrimitiveRect = (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::RECTS);

	// Blend color
	float* blendColorConstant = (float*)LatteGPUState.contextRegister + Latte::REGADDR::CB_BLEND_RED;
	renderCommandEncoder->setBlendColor(blendColorConstant[0], blendColorConstant[1], blendColorConstant[2], blendColorConstant[3]);

	// polygon control
	const auto& polygonControlReg = LatteGPUState.contextNew.PA_SU_SC_MODE_CNTL;
	const auto frontFace = polygonControlReg.get_FRONT_FACE();
	uint32 cullFront = polygonControlReg.get_CULL_FRONT();
	uint32 cullBack = polygonControlReg.get_CULL_BACK();
	uint32 polyOffsetFrontEnable = polygonControlReg.get_OFFSET_FRONT_ENABLED();

	// TODO
	//cemu_assert_debug(LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_ZCLIP_NEAR_DISABLE() == LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_ZCLIP_FAR_DISABLE()); // near or far clipping can be disabled individually
	//bool zClipEnable = LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_ZCLIP_FAR_DISABLE() == false;

	if (polyOffsetFrontEnable)
	{
    	uint32 frontScaleU32 = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_SCALE.getRawValue();
    	uint32 frontOffsetU32 = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_OFFSET.getRawValue();
    	uint32 offsetClampU32 = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_CLAMP.getRawValue();

        if (frontOffsetU32 != encoderState.m_depthBias || frontScaleU32 != encoderState.m_depthSlope || offsetClampU32 != encoderState.m_depthClamp)
        {
           	float frontScale = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_SCALE.get_SCALE();
           	float frontOffset = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_OFFSET.get_OFFSET();
           	float offsetClamp = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_CLAMP.get_CLAMP();

           	frontScale /= 16.0f;

            renderCommandEncoder->setDepthBias(frontOffset, frontScale, offsetClamp);

            encoderState.m_depthBias = frontOffsetU32;
            encoderState.m_depthSlope = frontScaleU32;
            encoderState.m_depthClamp = offsetClampU32;
        }
	}
	else
	{
	    if (0 != encoderState.m_depthBias || 0 != encoderState.m_depthSlope || 0 != encoderState.m_depthClamp)
		{
	        renderCommandEncoder->setDepthBias(0.0f, 0.0f, 0.0f);

			encoderState.m_depthBias = 0;
			encoderState.m_depthSlope = 0;
			encoderState.m_depthClamp = 0;
		}
	}

	// todo - how does culling behave with rects?
	// right now we just assume that their winding is always CW
	if (isPrimitiveRect)
	{
		if (frontFace == Latte::LATTE_PA_SU_SC_MODE_CNTL::E_FRONTFACE::CW)
			cullFront = cullBack;
		else
			cullBack = cullFront;
	}

	// Cull mode
	if (cullFront && cullBack)
		return; // We can just skip the draw (TODO: can we?)

    MTL::CullMode cullMode;
   	if (cullFront)
  		cullMode = MTL::CullModeFront;
   	else if (cullBack)
  		cullMode = MTL::CullModeBack;
   	else
  		cullMode = MTL::CullModeNone;

    if (cullMode != encoderState.m_cullMode)
   	{
   	    renderCommandEncoder->setCullMode(cullMode);
  		encoderState.m_cullMode = cullMode;
   	}

	// Front face
	MTL::Winding frontFaceWinding;
	if (frontFace == Latte::LATTE_PA_SU_SC_MODE_CNTL::E_FRONTFACE::CCW)
		frontFaceWinding = MTL::WindingCounterClockwise;
	else
		frontFaceWinding = MTL::WindingClockwise;

    if (frontFaceWinding != encoderState.m_frontFaceWinding)
   	{
   	    renderCommandEncoder->setFrontFacingWinding(frontFaceWinding);
  		encoderState.m_frontFaceWinding = frontFaceWinding;
   	}

    // Viewport
    if (m_state.m_viewport.originX != encoderState.m_viewport.originX ||
        m_state.m_viewport.originY != encoderState.m_viewport.originY ||
        m_state.m_viewport.width != encoderState.m_viewport.width ||
        m_state.m_viewport.height != encoderState.m_viewport.height ||
        m_state.m_viewport.znear != encoderState.m_viewport.znear ||
        m_state.m_viewport.zfar != encoderState.m_viewport.zfar)
    {
        renderCommandEncoder->setViewport(m_state.m_viewport);

        encoderState.m_viewport = m_state.m_viewport;
    }

    // Scissor
    if (m_state.m_scissor.x != encoderState.m_scissor.x ||
        m_state.m_scissor.y != encoderState.m_scissor.y ||
        m_state.m_scissor.width != encoderState.m_scissor.width ||
        m_state.m_scissor.height != encoderState.m_scissor.height)
    {
        encoderState.m_scissor = m_state.m_scissor;

        // TODO: clamp scissor to render target dimensions
        //scissor.width = ;
        //scissor.height = ;
        renderCommandEncoder->setScissorRect(encoderState.m_scissor);
    }

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
        auto& vertexBufferRange = m_state.m_vertexBuffers[i];
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

	// Render pipeline state
	MTL::RenderPipelineState* renderPipelineState = m_pipelineCache->GetPipelineState(fetchShader, vertexShader, pixelShader, m_state.m_lastUsedFBO, LatteGPUState.contextNew);
	if (renderPipelineState != encoderState.m_renderPipelineState)
   	{
        renderCommandEncoder->setRenderPipelineState(renderPipelineState);
  		encoderState.m_renderPipelineState = renderPipelineState;
   	}

	// Uniform buffers, textures and samplers
	BindStageResources(renderCommandEncoder, vertexShader);
	BindStageResources(renderCommandEncoder, pixelShader);

	// Draw
	if (hostIndexType != INDEX_TYPE::NONE)
	{
	    auto mtlIndexType = GetMtlIndexType(hostIndexType);
		MTL::Buffer* indexBuffer = m_memoryManager->GetTemporaryBufferAllocator().GetBuffer(indexBufferIndex);
		renderCommandEncoder->drawIndexedPrimitives(mtlPrimitiveType, hostIndexCount, mtlIndexType, indexBuffer, indexBufferOffset, instanceCount, baseVertex, baseInstance);
	} else
	{
		renderCommandEncoder->drawPrimitives(mtlPrimitiveType, baseVertex, count, instanceCount, baseInstance);
	}

	LatteGPUState.drawCallCounter++;
}

void MetalRenderer::draw_endSequence()
{
    LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
	// post-drawcall logic
	if (pixelShader)
		LatteRenderTarget_trackUpdates();
	bool hasReadback = LatteTextureReadback_Update();
	m_recordedDrawcalls++;
	// The number of draw calls needs to twice as big, since we are interrupting the render pass
	if (m_recordedDrawcalls >= COMMIT_TRESHOLD * 2 || hasReadback)
	{
		CommitCommandBuffer();

        // TODO: where should this be called?
        LatteTextureReadback_UpdateFinishedTransfers(false);
	}
}

void* MetalRenderer::indexData_reserveIndexMemory(uint32 size, uint32& offset, uint32& bufferIndex)
{
    auto allocation = m_memoryManager->GetTemporaryBufferAllocator().GetBufferAllocation(size);
	offset = allocation.offset;
	bufferIndex = allocation.bufferIndex;

	return allocation.data;
}

void MetalRenderer::indexData_uploadIndexMemory(uint32 offset, uint32 size)
{
    // Do nothing, since the buffer has shared storage mode
}

MTL::CommandBuffer* MetalRenderer::GetCommandBuffer()
{
    bool needsNewCommandBuffer = (m_commandBuffers.empty() || m_commandBuffers.back().m_commited);
    if (needsNewCommandBuffer)
	{
        // Debug
        //m_commandQueue->insertDebugCaptureBoundary();

	    MTL::CommandBuffer* mtlCommandBuffer = m_commandQueue->commandBuffer();
		m_commandBuffers.push_back({mtlCommandBuffer});

		// Notify memory manager about the new command buffer
        m_memoryManager->GetTemporaryBufferAllocator().SetActiveCommandBuffer(mtlCommandBuffer);

		return mtlCommandBuffer;
	}
	else
	{
	    return m_commandBuffers.back().m_commandBuffer;
	}
}

bool MetalRenderer::CommandBufferCompleted(MTL::CommandBuffer* commandBuffer)
{
    return commandBuffer->status() == MTL::CommandBufferStatusCompleted;
}

void MetalRenderer::WaitForCommandBufferCompletion(MTL::CommandBuffer* commandBuffer)
{
    commandBuffer->waitUntilCompleted();
}

MTL::RenderCommandEncoder* MetalRenderer::GetTemporaryRenderCommandEncoder(MTL::RenderPassDescriptor* renderPassDescriptor)
{
    EndEncoding();

    auto commandBuffer = GetCommandBuffer();

    auto renderCommandEncoder = commandBuffer->renderCommandEncoder(renderPassDescriptor);
#ifdef CEMU_DEBUG_ASSERT
    renderCommandEncoder->setLabel(GetLabel("Temporary render command encoder", renderCommandEncoder));
#endif
    m_commandEncoder = renderCommandEncoder;
    m_encoderType = MetalEncoderType::Render;

    return renderCommandEncoder;
}

// Some render passes clear the attachments, forceRecreate is supposed to be used in those cases
MTL::RenderCommandEncoder* MetalRenderer::GetRenderCommandEncoder(bool forceRecreate, bool rebindStateIfNewEncoder)
{
    // Check if we need to begin a new render pass
    if (m_commandEncoder)
    {
        if (!forceRecreate)
        {
            if (m_encoderType == MetalEncoderType::Render)
            {
                bool needsNewRenderPass = (m_state.m_lastUsedFBO == nullptr);
                if (!needsNewRenderPass)
                {
                    for (uint8 i = 0; i < 8; i++)
                    {
                        if (m_state.m_activeFBO->colorBuffer[i].texture && m_state.m_activeFBO->colorBuffer[i].texture != m_state.m_lastUsedFBO->colorBuffer[i].texture)
                        {
                            needsNewRenderPass = true;
                            break;
                        }
                    }
                }

                if (!needsNewRenderPass)
                {
                    if (m_state.m_activeFBO->depthBuffer.texture && (m_state.m_activeFBO->depthBuffer.texture != m_state.m_lastUsedFBO->depthBuffer.texture || ( m_state.m_activeFBO->depthBuffer.hasStencil && !m_state.m_lastUsedFBO->depthBuffer.hasStencil)))
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

    auto commandBuffer = GetCommandBuffer();

    // Update state
    m_state.m_lastUsedFBO = m_state.m_activeFBO;

    auto renderCommandEncoder = commandBuffer->renderCommandEncoder(m_state.m_activeFBO->GetRenderPassDescriptor());
#ifdef CEMU_DEBUG_ASSERT
    renderCommandEncoder->setLabel(GetLabel("Render command encoder", renderCommandEncoder));
#endif
    m_commandEncoder = renderCommandEncoder;
    m_encoderType = MetalEncoderType::Render;

    ResetEncoderState();

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

    auto commandBuffer = GetCommandBuffer();

    auto computeCommandEncoder = commandBuffer->computeCommandEncoder();
    m_commandEncoder = computeCommandEncoder;
    m_encoderType = MetalEncoderType::Compute;

    ResetEncoderState();

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

    auto commandBuffer = GetCommandBuffer();

    auto blitCommandEncoder = commandBuffer->blitCommandEncoder();
    m_commandEncoder = blitCommandEncoder;
    m_encoderType = MetalEncoderType::Blit;

    ResetEncoderState();

    return blitCommandEncoder;
}

void MetalRenderer::EndEncoding()
{
    if (m_commandEncoder)
    {
        m_commandEncoder->endEncoding();
        m_commandEncoder->release();
        m_commandEncoder = nullptr;
        m_encoderType = MetalEncoderType::None;

        // Commit the command buffer if enough draw calls have been recorded
        if (m_recordedDrawcalls >= COMMIT_TRESHOLD)
            CommitCommandBuffer();
    }
}

void MetalRenderer::CommitCommandBuffer()
{
    m_recordedDrawcalls = 0;

    if (m_commandBuffers.size() != 0)
    {
        EndEncoding();

        auto& commandBuffer = m_commandBuffers.back();
        if (!commandBuffer.m_commited)
        {
            commandBuffer.m_commandBuffer->addCompletedHandler(^(MTL::CommandBuffer* cmd) {
                m_memoryManager->GetTemporaryBufferAllocator().CommandBufferFinished(commandBuffer.m_commandBuffer);
            });

            commandBuffer.m_commandBuffer->commit();
            commandBuffer.m_commited = true;

            // Debug
            //m_commandQueue->insertDebugCaptureBoundary();
        }
    }
}

bool MetalRenderer::AcquireNextDrawable(bool mainWindow)
{
    const bool latteBufferUsesSRGB = mainWindow ? LatteGPUState.tvBufferUsesSRGB : LatteGPUState.drcBufferUsesSRGB;
    if (latteBufferUsesSRGB != m_state.m_usesSRGB)
    {
        m_metalLayer->setPixelFormat(latteBufferUsesSRGB ? MTL::PixelFormatRGBA8Unorm_sRGB : MTL::PixelFormatRGBA8Unorm);
        m_state.m_usesSRGB = latteBufferUsesSRGB;
    }

    if (m_drawable)
    {
        // TODO: should this be true?
        return true;
    }

    m_drawable = m_metalLayer->nextDrawable();
    if (!m_drawable)
    {
        debug_printf("failed to acquire next drawable\n");
        return false;
    }

    return true;
}

void MetalRenderer::BindStageResources(MTL::RenderCommandEncoder* renderCommandEncoder, LatteDecompilerShader* shader)
{
    auto mtlShaderType = GetMtlShaderType(shader->shaderType);

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

		// TODO: uncomment
		uint32 binding = shader->resourceMapping.getTextureBaseBindingPoint() + i;//shader->resourceMapping.textureUnitToBindingPoint[hostTextureUnit];
		if (binding >= MAX_MTL_TEXTURES)
		{
		    debug_printf("invalid texture binding %u\n", binding);
            continue;
		}

		auto textureView = m_state.m_textures[hostTextureUnit];
		if (!textureView)
		{
		    // TODO: don't bind if already bound
            if (textureDim == Latte::E_DIM::DIM_1D)
           	{
          		switch (shader->shaderType)
               	{
               	case LatteConst::ShaderType::Vertex:
               	{
              		renderCommandEncoder->setVertexTexture(m_nullTexture1D, binding);
              		renderCommandEncoder->setVertexSamplerState(m_nearestSampler, binding);
              		break;
               	}
               	case LatteConst::ShaderType::Pixel:
               	{
               	    renderCommandEncoder->setFragmentTexture(m_nullTexture1D, binding);
                    renderCommandEncoder->setVertexSamplerState(m_nearestSampler, binding);
              		break;
               	}
               	default:
              		UNREACHABLE;
               	}
           	}
           	else
           	{
          		switch (shader->shaderType)
               	{
               	case LatteConst::ShaderType::Vertex:
               	{
              		renderCommandEncoder->setVertexTexture(m_nullTexture2D, binding);
              		renderCommandEncoder->setVertexSamplerState(m_nearestSampler, binding);
              		break;
               	}
               	case LatteConst::ShaderType::Pixel:
               	{
               	    renderCommandEncoder->setFragmentTexture(m_nullTexture2D, binding);
                    renderCommandEncoder->setVertexSamplerState(m_nearestSampler, binding);
              		break;
               	}
               	default:
              		UNREACHABLE;
               	}
           	}
            continue;
		}

		if (textureDim == Latte::E_DIM::DIM_1D && (textureView->dim != Latte::E_DIM::DIM_1D))
		{
		    switch (shader->shaderType)
           	{
           	case LatteConst::ShaderType::Vertex:
           	{
          		renderCommandEncoder->setVertexTexture(m_nullTexture1D, binding);
          		renderCommandEncoder->setVertexSamplerState(m_nearestSampler, binding);
          		break;
           	}
           	case LatteConst::ShaderType::Pixel:
           	{
           	    renderCommandEncoder->setFragmentTexture(m_nullTexture1D, binding);
                renderCommandEncoder->setVertexSamplerState(m_nearestSampler, binding);
          		break;
           	}
           	default:
          		UNREACHABLE;
           	}
			continue;
		}
		else if (textureDim == Latte::E_DIM::DIM_2D && (textureView->dim != Latte::E_DIM::DIM_2D && textureView->dim != Latte::E_DIM::DIM_2D_MSAA))
		{
		    switch (shader->shaderType)
           	{
           	case LatteConst::ShaderType::Vertex:
           	{
          		renderCommandEncoder->setVertexTexture(m_nullTexture2D, binding);
          		renderCommandEncoder->setVertexSamplerState(m_nearestSampler, binding);
          		break;
           	}
           	case LatteConst::ShaderType::Pixel:
           	{
           	    renderCommandEncoder->setFragmentTexture(m_nullTexture2D, binding);
                renderCommandEncoder->setVertexSamplerState(m_nearestSampler, binding);
          		break;
           	}
           	default:
          		UNREACHABLE;
           	}
			continue;
		}

		LatteTexture* baseTexture = textureView->baseTexture;

		uint32 stageSamplerIndex = shader->textureUnitSamplerAssignment[relative_textureUnit];
		MTL::SamplerState* sampler;
		if (stageSamplerIndex != LATTE_DECOMPILER_SAMPLER_NONE)
		{
    		uint32 samplerIndex = stageSamplerIndex + LatteDecompiler_getTextureSamplerBaseIndex(shader->shaderType);
    		sampler = m_samplerCache->GetSamplerState(LatteGPUState.contextNew, samplerIndex);
		}
		else
		{
		    sampler = m_nearestSampler;
		}

        auto& boundSampler = m_state.m_encoderState.m_samplers[mtlShaderType][binding];
        if (sampler != boundSampler)
        {
            boundSampler = sampler;

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
        }

		// get texture register word 0
		uint32 word4 = LatteGPUState.contextRegister[texUnitRegIndex + 4];
		auto& boundTexture = m_state.m_encoderState.m_textures[mtlShaderType][binding];
		if (textureView == boundTexture.m_textureView && word4 == boundTexture.m_word4)
		    continue;

		boundTexture = {textureView, word4};

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
		// TODO: uncomment
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

		auto& bufferAllocator = m_memoryManager->GetTemporaryBufferAllocator();
		size_t size = shader->uniform.uniformRangeSize;
		auto supportBuffer = bufferAllocator.GetBufferAllocation(size);
		memcpy(supportBuffer.data, supportBufferData, size);

		switch (shader->shaderType)
		{
		case LatteConst::ShaderType::Vertex:
		{
			renderCommandEncoder->setVertexBuffer(bufferAllocator.GetBuffer(supportBuffer.bufferIndex), supportBuffer.offset, MTL_SUPPORT_BUFFER_BINDING);
			//renderCommandEncoder->setVertexBytes(supportBufferData, sizeof(supportBufferData), MTL_SUPPORT_BUFFER_BINDING);
			break;
		}
		case LatteConst::ShaderType::Pixel:
		{
		    renderCommandEncoder->setFragmentBuffer(bufferAllocator.GetBuffer(supportBuffer.bufferIndex), supportBuffer.offset, MTL_SUPPORT_BUFFER_BINDING);
			//renderCommandEncoder->setFragmentBytes(supportBufferData, sizeof(supportBufferData), MTL_SUPPORT_BUFFER_BINDING);
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
    		    debug_printf("invalid buffer binding%u\n", binding);
    			continue;
    		}

    		size_t offset = m_state.m_uniformBufferOffsets[mtlShaderType][i];
    		if (offset == INVALID_OFFSET)
                continue;

            auto& boundOffset = m_state.m_encoderState.m_uniformBufferOffsets[mtlShaderType][binding];
            if (offset == boundOffset)
                continue;

            boundOffset = offset;

            // TODO: only set the offset if already bound
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

	// Storage buffer
	if (shader->resourceMapping.tfStorageBindingPoint >= 0)
	{
	   switch (shader->shaderType)
        {
        case LatteConst::ShaderType::Vertex:
        {
     			renderCommandEncoder->setVertexBuffer(m_xfbRingBuffer, 0, shader->resourceMapping.tfStorageBindingPoint);
     			break;
        }
        case LatteConst::ShaderType::Pixel:
        {
            renderCommandEncoder->setFragmentBuffer(m_xfbRingBuffer, 0, shader->resourceMapping.tfStorageBindingPoint);
     			break;
        }
        default:
     			UNREACHABLE;
        }
	}
}

void MetalRenderer::RebindRenderState(MTL::RenderCommandEncoder* renderCommandEncoder)
{
    // Vertex buffers
	for (uint8 i = 0; i < MAX_MTL_BUFFERS; i++)
	{
	    auto& vertexBufferRange = m_state.m_vertexBuffers[i];
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
    GetTemporaryRenderCommandEncoder(renderPassDescriptor);
    renderPassDescriptor->release();
    EndEncoding();
}
