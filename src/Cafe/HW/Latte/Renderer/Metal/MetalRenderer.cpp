#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalMemoryManager.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalOutputShaderCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalPipelineCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalDepthStencilCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalSamplerCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureReadbackMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalVoidVertexPipeline.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalQuery.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/UtilityShaderSource.h"

#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteIndices.h"
#include "Cemu/Logging/CemuDebugLogging.h"
#include "Cemu/Logging/CemuLogging.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalLayerHandle.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "config/CemuConfig.h"

#define IMGUI_IMPL_METAL_CPP
#include "imgui/imgui_extension.h"
#include "imgui/imgui_impl_metal.h"

#define DEFAULT_COMMIT_TRESHOLD 196
#define OCCLUSION_QUERY_POOL_SIZE 1024

extern bool hasValidFramebufferAttached;

float supportBufferData[512 * 4];

MetalRenderer::MetalRenderer()
{
    m_device = MTL::CreateSystemDefaultDevice();
    m_commandQueue = m_device->newCommandQueue();

    // Feature support
    m_isAppleGPU = m_device->supportsFamily(MTL::GPUFamilyApple1);
    m_hasUnifiedMemory = m_device->hasUnifiedMemory();
    m_supportsMetal3 = m_device->supportsFamily(MTL::GPUFamilyMetal3);
    m_recommendedMaxVRAMUsage = m_device->recommendedMaxWorkingSetSize();
    m_pixelFormatSupport = MetalPixelFormatSupport(m_device);

    CheckForPixelFormatSupport(m_pixelFormatSupport);

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
    textureDescriptor->setWidth(1);
    textureDescriptor->setUsage(MTL::TextureUsageShaderRead);
    m_nullTexture1D = m_device->newTexture(textureDescriptor);
#ifdef CEMU_DEBUG_ASSERT
    m_nullTexture1D->setLabel(GetLabel("Null texture 1D", m_nullTexture1D));
#endif

    textureDescriptor->setTextureType(MTL::TextureType2D);
    textureDescriptor->setHeight(1);
    textureDescriptor->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget);
    m_nullTexture2D = m_device->newTexture(textureDescriptor);
#ifdef CEMU_DEBUG_ASSERT
    m_nullTexture2D->setLabel(GetLabel("Null texture 2D", m_nullTexture2D));
#endif
    textureDescriptor->release();

    m_memoryManager = new MetalMemoryManager(this);
    m_outputShaderCache = new MetalOutputShaderCache(this);
    m_pipelineCache = new MetalPipelineCache(this);
    m_depthStencilCache = new MetalDepthStencilCache(this);
    m_samplerCache = new MetalSamplerCache(this);

    // Occlusion queries
    m_occlusionQuery.m_resultBuffer = m_device->newBuffer(OCCLUSION_QUERY_POOL_SIZE * sizeof(uint64), MTL::ResourceStorageModeShared);
#ifdef CEMU_DEBUG_ASSERT
    m_occlusionQuery.m_resultBuffer->setLabel(GetLabel("Occlusion query result buffer", m_occlusionQuery.m_resultBuffer));
#endif
    m_occlusionQuery.m_resultsPtr = (uint64*)m_occlusionQuery.m_resultBuffer->contents();

    m_occlusionQuery.m_availableIndices.reserve(OCCLUSION_QUERY_POOL_SIZE);
    for (uint32 i = 0; i < OCCLUSION_QUERY_POOL_SIZE; i++)
        m_occlusionQuery.m_availableIndices.push_back(i);

    // Initialize state
    for (uint32 i = 0; i < METAL_SHADER_TYPE_TOTAL; i++)
    {
        for (uint32 j = 0; j < MAX_MTL_BUFFERS; j++)
            m_state.m_uniformBufferOffsets[i][j] = INVALID_OFFSET;
    }

    // Utility shader library

    // Create the library
    NS::Error* error = nullptr;
	MTL::Library* utilityLibrary = m_device->newLibrary(ToNSString(utilityShaderSource), nullptr, &error);
	if (error)
    {
        debug_printf("failed to create utility library (error: %s)\n", error->localizedDescription()->utf8String());
        error->release();
        throw;
        return;
    }

    // Present pipeline
    /*
    MTL::Function* fullscreenVertexFunction = utilityLibrary->newFunction(ToNSString("vertexFullscreen"));
    MTL::Function* presentFragmentFunction = utilityLibrary->newFunction(ToNSString("fragmentPresent"));

    MTL::RenderPipelineDescriptor* renderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    renderPipelineDescriptor->setVertexFunction(fullscreenVertexFunction);
    renderPipelineDescriptor->setFragmentFunction(presentFragmentFunction);
    fullscreenVertexFunction->release();
    presentFragmentFunction->release();

    error = nullptr;
    renderPipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
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
    renderPipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm_sRGB);
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
    */

    // Copy texture pipelines
    auto copyTextureToColorPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();

    // Hybrid pipelines
    if (m_isAppleGPU)
        m_copyBufferToBufferPipeline = new MetalVoidVertexPipeline(this, utilityLibrary, "vertexCopyBufferToBuffer");
    //m_copyTextureToTexturePipeline = new MetalVoidVertexPipeline(this, utilityLibrary, "vertexCopyTextureToTexture");
    //m_restrideBufferPipeline = new MetalVoidVertexPipeline(this, utilityLibrary, "vertexRestrideBuffer");
    utilityLibrary->release();

    //m_memoryManager->SetRestrideBufferPipeline(m_restrideBufferPipeline);
}

MetalRenderer::~MetalRenderer()
{
    if (m_isAppleGPU)
        delete m_copyBufferToBufferPipeline;
    //delete m_copyTextureToTexturePipeline;
    //delete m_restrideBufferPipeline;

    //m_presentPipelineLinear->release();
    //m_presentPipelineSRGB->release();

    delete m_outputShaderCache;
    delete m_pipelineCache;
    delete m_depthStencilCache;
    delete m_samplerCache;
    delete m_memoryManager;

    m_nullTexture1D->release();
    m_nullTexture2D->release();

    m_nearestSampler->release();
    m_linearSampler->release();

    if (m_readbackBuffer)
        m_readbackBuffer->release();

    if (m_xfbRingBuffer)
        m_xfbRingBuffer->release();

    m_occlusionQuery.m_resultBuffer->release();

    m_commandQueue->release();
    m_device->release();
}

void MetalRenderer::InitializeLayer(const Vector2i& size, bool mainWindow)
{
    auto& layer = GetLayer(mainWindow);
    layer = MetalLayerHandle(m_device, size, mainWindow);
    layer.GetLayer()->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
}

void MetalRenderer::ShutdownLayer(bool mainWindow)
{
    GetLayer(mainWindow) = MetalLayerHandle();
}

void MetalRenderer::ResizeLayer(const Vector2i& size, bool mainWindow)
{
    GetLayer(mainWindow).Resize(size);
}

void MetalRenderer::Initialize()
{
    Renderer::Initialize();
    RendererShaderMtl::Initialize();
}

void MetalRenderer::Shutdown()
{
    // TODO: should shutdown both layers
    ImGui_ImplMetal_Shutdown();
    CommitCommandBuffer();
    Renderer::Shutdown();
    RendererShaderMtl::Shutdown();
}

bool MetalRenderer::IsPadWindowActive()
{
    return (GetLayer(false).GetLayer() != nullptr);
}

bool MetalRenderer::GetVRAMInfo(int& usageInMB, int& totalInMB) const
{
    usageInMB = m_device->currentAllocatedSize() / 1024 / 1024;
    totalInMB = m_recommendedMaxVRAMUsage / 1024 / 1024;

    return true;
}

void MetalRenderer::ClearColorbuffer(bool padView)
{
    if (!AcquireDrawable(!padView))
        return;

    ClearColorTextureInternal(GetLayer(!padView).GetDrawable()->texture(), 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
}

void MetalRenderer::DrawEmptyFrame(bool mainWindow)
{
    if (!BeginFrame(mainWindow))
		return;
	SwapBuffers(mainWindow, !mainWindow);
}

void MetalRenderer::SwapBuffers(bool swapTV, bool swapDRC)
{
    if (swapTV)
        SwapBuffer(true);
    if (swapDRC)
        SwapBuffer(false);

    // Reset the command buffers (they are released by TemporaryBufferAllocator)
    CommitCommandBuffer();
    m_commandBuffers.clear();

    // Release frame persistent buffers
    m_memoryManager->GetFramePersistentBufferAllocator().ResetAllocations();

    // Unlock all temporary buffers
    m_memoryManager->GetTemporaryBufferAllocator().EndFrame();

    // Debug
    m_performanceMonitor.ResetPerFrameData();
}

void MetalRenderer::DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter,
								sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight,
								bool padView, bool clearBackground)
{
    if (!AcquireDrawable(!padView))
        return;

    MTL::Texture* presentTexture = static_cast<LatteTextureViewMtl*>(texView)->GetRGBAView();

    // Create render pass
    auto& layer = GetLayer(!padView);

    MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    auto colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
    colorAttachment->setTexture(layer.GetDrawable()->texture());
    colorAttachment->setLoadAction(clearBackground ? MTL::LoadActionClear : MTL::LoadActionLoad);
    colorAttachment->setStoreAction(MTL::StoreActionStore);

    auto renderCommandEncoder = GetTemporaryRenderCommandEncoder(renderPassDescriptor);
    renderPassDescriptor->release();

    // Get a render pipeline

    // Find out which shader we are using
    uint8 shaderIndex = 255;
    if (shader == RendererOutputShader::s_copy_shader) shaderIndex = 0;
    else if (shader == RendererOutputShader::s_bicubic_shader) shaderIndex = 1;
    else if (shader == RendererOutputShader::s_hermit_shader) shaderIndex = 2;
    else if (shader == RendererOutputShader::s_copy_shader_ud) shaderIndex = 3;
    else if (shader == RendererOutputShader::s_bicubic_shader_ud) shaderIndex = 4;
    else if (shader == RendererOutputShader::s_hermit_shader_ud) shaderIndex = 5;

    uint8 shaderType = shaderIndex % 3;

    // Get the render pipeline state
    auto renderPipelineState = m_outputShaderCache->GetPipeline(shader, shaderIndex, m_state.m_usesSRGB);

    // Draw to Metal layer
    renderCommandEncoder->setRenderPipelineState(renderPipelineState);
    renderCommandEncoder->setFragmentTexture(presentTexture, 0);
    renderCommandEncoder->setFragmentSamplerState((useLinearTexFilter ? m_linearSampler : m_nearestSampler), 0);

    // Set uniforms
    float outputSize[2] = {(float)imageWidth, (float)imageHeight};
    switch (shaderType)
    {
    case 2:
        renderCommandEncoder->setFragmentBytes(outputSize, sizeof(outputSize), 0);
        break;
    default:
        break;
    }

    renderCommandEncoder->setViewport(MTL::Viewport{(double)imageX, (double)imageY, (double)imageWidth, (double)imageHeight, 0.0, 1.0});
    renderCommandEncoder->setScissorRect(MTL::ScissorRect{(uint32)imageX, (uint32)imageY, (uint32)imageWidth, (uint32)imageHeight});

    renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

    EndEncoding();
}

bool MetalRenderer::BeginFrame(bool mainWindow)
{
    return AcquireDrawable(mainWindow);
}

void MetalRenderer::Flush(bool waitIdle)
{
    if (m_recordedDrawcalls > 0 || waitIdle)
        CommitCommandBuffer();
    if (waitIdle)
    {
        for (auto commandBuffer : m_commandBuffers)
        {
            cemu_assert_debug(commandBuffer.m_commited);

            commandBuffer.m_commandBuffer->waitUntilCompleted();
        }
    }
}

void MetalRenderer::NotifyLatteCommandProcessorIdle()
{
    //if (m_commitOnIdle)
    //    CommitCommandBuffer();
}

bool MetalRenderer::ImguiBegin(bool mainWindow)
{
    if (!Renderer::ImguiBegin(mainWindow))
		return false;

	if (!AcquireDrawable(mainWindow))
		return false;

	EnsureImGuiBackend();

	// Check if the font texture needs to be built
	ImGuiIO& io = ImGui::GetIO();
    if (!io.Fonts->IsBuilt())
        ImGui_ImplMetal_CreateFontsTexture(m_device);

	auto& layer = GetLayer(mainWindow);

	// Render pass descriptor
	MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    auto colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
    colorAttachment->setTexture(layer.GetDrawable()->texture());
    colorAttachment->setLoadAction(MTL::LoadActionLoad);
    colorAttachment->setStoreAction(MTL::StoreActionStore);

    // New frame
	ImGui_ImplMetal_NewFrame(renderPassDescriptor);
	ImGui_UpdateWindowInformation(mainWindow);
	ImGui::NewFrame();

	if (m_encoderType != MetalEncoderType::Render)
	    GetTemporaryRenderCommandEncoder(renderPassDescriptor);
	renderPassDescriptor->release();

	return true;
}

void MetalRenderer::ImguiEnd()
{
    EnsureImGuiBackend();

    if (m_encoderType != MetalEncoderType::Render)
    {
        debug_printf("no render command encoder, cannot draw ImGui\n");
        return;
    }

    ImGui::Render();
	ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), GetCurrentCommandBuffer(), (MTL::RenderCommandEncoder*)m_commandEncoder);
	//ImGui::EndFrame();

	EndEncoding();
}

ImTextureID MetalRenderer::GenerateTexture(const std::vector<uint8>& data, const Vector2i& size)
{
    try
	{
		std::vector <uint8> tmp(size.x * size.y * 4);
		for (size_t i = 0; i < data.size() / 3; ++i)
		{
			tmp[(i * 4) + 0] = data[(i * 3) + 0];
			tmp[(i * 4) + 1] = data[(i * 3) + 1];
			tmp[(i * 4) + 2] = data[(i * 3) + 2];
			tmp[(i * 4) + 3] = 0xFF;
		}

		MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
		desc->setTextureType(MTL::TextureType2D);
		desc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
		desc->setWidth(size.x);
		desc->setHeight(size.y);
		desc->setStorageMode(m_isAppleGPU ? MTL::StorageModeShared : MTL::StorageModeManaged);
		desc->setUsage(MTL::TextureUsageShaderRead);

		MTL::Texture* texture = m_device->newTexture(desc);
		desc->release();

		// TODO: do a GPU copy?
		texture->replaceRegion(MTL::Region(0, 0, size.x, size.y), 0, 0, tmp.data(), size.x * 4, 0);

		return (ImTextureID)texture;
	}
	catch (const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "can't generate imgui texture: {}", ex.what());
		return nullptr;
	}
}

void MetalRenderer::DeleteTexture(ImTextureID id)
{
    EnsureImGuiBackend();

    ((MTL::Texture*)id)->release();
}

void MetalRenderer::DeleteFontTextures()
{
    EnsureImGuiBackend();

    ImGui_ImplMetal_DestroyFontsTexture();
}

void MetalRenderer::AppendOverlayDebugInfo()
{
    ImGui::Text("--- GPU info ---");
    ImGui::Text("Is Apple GPU              %s", (m_isAppleGPU ? "yes" : "no"));
    ImGui::Text("Has unified memory        %s", (m_hasUnifiedMemory ? "yes" : "no"));
    ImGui::Text("Supports Metal3           %s", (m_supportsMetal3 ? "yes" : "no"));

    ImGui::Text("--- Metal info ---");
    ImGui::Text("Render pipeline states    %zu", m_pipelineCache->GetPipelineCacheSize());
    ImGui::Text("Buffer allocator memory   %zuMB", m_performanceMonitor.m_bufferAllocatorMemory / 1024 / 1024);

    ImGui::Text("--- Metal info (per frame) ---");
    ImGui::Text("Command buffers           %zu", m_commandBuffers.size());
    ImGui::Text("Render passes             %u", m_performanceMonitor.m_renderPasses);
    ImGui::Text("Clears                    %u", m_performanceMonitor.m_clears);
    ImGui::Text("Manual vertex fetch draws %u (mesh draws: %u)", m_performanceMonitor.m_manualVertexFetchDraws, m_performanceMonitor.m_meshDraws);
    ImGui::Text("Triangle fans             %u", m_performanceMonitor.m_triangleFans);
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
	return new CachedFBOMtl(this, key);
}

void MetalRenderer::rendertarget_deleteCachedFBO(LatteCachedFBO* cfbo)
{
	if (cfbo == (LatteCachedFBO*)m_state.m_activeFBO.m_fbo)
	    m_state.m_activeFBO = {nullptr};
}

void MetalRenderer::rendertarget_bindFramebufferObject(LatteCachedFBO* cfbo)
{
	m_state.m_activeFBO = {(CachedFBOMtl*)cfbo, MetalAttachmentsInfo((CachedFBOMtl*)cfbo)};
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
    return GetMtlPixelFormatInfo(format, isDepth).textureDecoder;
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

// TODO: do a cpu copy on Apple Silicon?
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
    // No need to set bytesPerImage for 3D textures, since we always load just one slice
    //size_t bytesPerImage = GetMtlTextureBytesPerImage(textureMtl->GetFormat(), textureMtl->IsDepth(), height, bytesPerRow);
    //if (m_isAppleGPU)
    //{
    //    textureMtl->GetTexture()->replaceRegion(MTL::Region(0, 0, offsetZ, width, height, 1), mipIndex, sliceIndex, pixelData, bytesPerRow, 0);
    //}
    //else
    //{
    auto blitCommandEncoder = GetBlitCommandEncoder();

    // Allocate a temporary buffer
    auto& bufferAllocator = m_memoryManager->GetTemporaryBufferAllocator();
    auto allocation = bufferAllocator.GetBufferAllocation(compressedImageSize);
    auto buffer = bufferAllocator.GetBuffer(allocation.bufferIndex);

    // Copy the data to the temporary buffer
    memcpy(allocation.data, pixelData, compressedImageSize);
    //buffer->didModifyRange(NS::Range(allocation.offset, allocation.size));

    // Copy the data from the temporary buffer to the texture
    blitCommandEncoder->copyFromBuffer(buffer, allocation.offset, bytesPerRow, 0, MTL::Size(width, height, 1), textureMtl->GetTexture(), sliceIndex, mipIndex, MTL::Origin(0, 0, offsetZ));
    //}
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

    // Debug
    m_performanceMonitor.m_clears++;
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
    // scale copy size to effective size
	sint32 effectiveCopyWidth = width;
	sint32 effectiveCopyHeight = height;
	LatteTexture_scaleToEffectiveSize(sourceTexture, &effectiveCopyWidth, &effectiveCopyHeight, 0);
	//sint32 sourceEffectiveWidth, sourceEffectiveHeight;
	//sourceTexture->GetEffectiveSize(sourceEffectiveWidth, sourceEffectiveHeight, srcMip);

    texture_copyImageSubData(sourceTexture, srcMip, 0, 0, srcSlice, destinationTexture, dstMip, 0, 0, dstSlice, effectiveCopyWidth, effectiveCopyHeight, 1);

    /*
	sint32 texSrcMip = srcMip;
	sint32 texSrcSlice = srcSlice;
	sint32 texDstMip = dstMip;
	sint32 texDstSlice = dstSlice;

	// Create texture views
	LatteTextureViewMtl* srcTextureMtl = static_cast<LatteTextureViewMtl*>(sourceTexture->GetOrCreateView(srcMip, 1, srcSlice, 1));
	LatteTextureViewMtl* dstTextureMtl = static_cast<LatteTextureViewMtl*>(destinationTexture->GetOrCreateView(dstMip, 1, dstSlice, 1));

	// check if texture rescale ratios match
	// todo - if not, we have to use drawcall based copying
	if (!LatteTexture_doesEffectiveRescaleRatioMatch(sourceTexture, texSrcMip, destinationTexture, texDstMip))
	{
		cemuLog_logDebug(LogType::Force, "surfaceCopy_copySurfaceWithFormatConversion(): Mismatching dimensions");
		return;
	}

	// check if bpp size matches
	if (sourceTexture->GetBPP() != destinationTexture->GetBPP())
	{
		cemuLog_logDebug(LogType::Force, "surfaceCopy_copySurfaceWithFormatConversion(): Mismatching BPP");
		return;
	}

	if (m_encoderType == MetalEncoderType::Render)
	{
	    auto renderCommandEncoder = static_cast<MTL::RenderCommandEncoder*>(m_commandEncoder);

		renderCommandEncoder->setRenderPipelineState(m_copyTextureToTexturePipeline->GetRenderPipelineState());
		m_state.m_encoderState.m_renderPipelineState = m_copyTextureToTexturePipeline->GetRenderPipelineState();

		SetTexture(renderCommandEncoder, METAL_SHADER_TYPE_VERTEX, srcTextureMtl->GetRGBAView(), GET_HELPER_TEXTURE_BINDING(0));
		SetTexture(renderCommandEncoder, METAL_SHADER_TYPE_VERTEX, dstTextureMtl->GetRGBAView(), GET_HELPER_TEXTURE_BINDING(1));
		renderCommandEncoder->setVertexBytes(&effectiveCopyWidth, sizeof(effectiveCopyWidth), GET_HELPER_BUFFER_BINDING(0));
		m_state.m_encoderState.m_buffers[METAL_SHADER_TYPE_VERTEX][GET_HELPER_BUFFER_BINDING(0)] = {nullptr};

		renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
	}
	else
	{
	    bool copyingToWholeRegion = ((effectiveCopyWidth == dstTextureMtl->GetMipWidth(dstMip) && effectiveCopyHeight == dstTextureMtl->GetMipHeight(dstMip)));

	    auto renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
		auto colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
		colorAttachment->setTexture(dstTextureMtl->GetTexture());
		// We don't care about previous contents if we are about to overwrite the whole region
		colorAttachment->setLoadAction(copyingToWholeRegion ? MTL::LoadActionDontCare : MTL::LoadActionLoad);
		colorAttachment->setStoreAction(MTL::StoreActionStore);
		colorAttachment->setSlice(dstSlice);
		colorAttachment->setLevel(dstMip);

		auto renderCommandEncoder = GetTemporaryRenderCommandEncoder(renderPassDescriptor);

		auto pipeline = (srcTextureMtl->IsDepth() ? m_copyTextureToColorPipeline : m_copyTextureToDepthPipeline);
		renderCommandEncoder->setRenderPipelineState(pipeline);

		renderCommandEncoder->setFragmentTexture(srcTextureMtl->GetTexture(), GET_HELPER_TEXTURE_BINDING(0));
		renderCommandEncoder->setFragmentBytes(&effectiveCopyWidth, offsetof(effectiveCopyWidth), GET_HELPER_BUFFER_BINDING(0));

		renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

		EndEncoding();

		debug_printf("surface copy with no render command encoder, skipping copy\n");
	}
	*/
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
    CopyBufferToBuffer(GetXfbRingBuffer(), srcOffset, m_memoryManager->GetBufferCache(), dstOffset, size, MTL::RenderStageVertex | MTL::RenderStageMesh, ALL_MTL_RENDER_STAGES);
}

void MetalRenderer::buffer_bindVertexBuffer(uint32 bufferIndex, uint32 offset, uint32 size)
{
    cemu_assert_debug(bufferIndex < LATTE_MAX_VERTEX_BUFFERS);
    auto& buffer = m_state.m_vertexBuffers[bufferIndex];
	if (buffer.offset == offset && buffer.size == size)
		return;

	//if (buffer.offset != INVALID_OFFSET)
	//{
	//    m_memoryManager->UntrackVertexBuffer(bufferIndex);
	//}

	buffer.offset = offset;
	buffer.size = size;
	//buffer.restrideInfo = {};

	//m_memoryManager->TrackVertexBuffer(bufferIndex, offset, size, &buffer.restrideInfo);
}

void MetalRenderer::buffer_bindUniformBuffer(LatteConst::ShaderType shaderType, uint32 bufferIndex, uint32 offset, uint32 size)
{
    m_state.m_uniformBufferOffsets[GetMtlGeneralShaderType(shaderType)][bufferIndex] = offset;
}

RendererShader* MetalRenderer::shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool isGameShader, bool isGfxPackShader)
{
    return new RendererShaderMtl(this, type, baseHash, auxHash, isGameShader, isGfxPackShader, source);
}

void MetalRenderer::streamout_setupXfbBuffer(uint32 bufferIndex, sint32 ringBufferOffset, uint32 rangeAddr, uint32 rangeSize)
{
    m_state.m_streamoutState.buffers[bufferIndex].enabled = true;
	m_state.m_streamoutState.buffers[bufferIndex].ringBufferOffset = ringBufferOffset;
}

void MetalRenderer::streamout_begin()
{
    // Do nothing
}

void MetalRenderer::streamout_rendererFinishDrawcall()
{
    // Do nothing
}

void MetalRenderer::draw_beginSequence()
{
    m_state.m_skipDrawSequence = false;

    bool streamoutEnable = LatteGPUState.contextRegister[mmVGT_STRMOUT_EN] != 0;

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

		if (!hasValidFramebufferAttached && !streamoutEnable)
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
	bool rasterizerEnable = !LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_DX_RASTERIZATION_KILL();

	// GX2SetSpecialState(0, true) enables DX_RASTERIZATION_KILL, but still expects depth writes to happen? -> Research which stages are disabled by DX_RASTERIZATION_KILL exactly
	// for now we use a workaround:
	if (!LatteGPUState.contextNew.PA_CL_VTE_CNTL.get_VPORT_X_OFFSET_ENA())
		rasterizerEnable = true;

	if (!rasterizerEnable && !streamoutEnable)
		m_state.m_skipDrawSequence = true;
}

void MetalRenderer::draw_execute(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, bool isFirst)
{
    if (m_state.m_skipDrawSequence)
	{
	    LatteGPUState.drawCallCounter++;
		return;
	}

	// TODO: special state 8 and 5

	auto& encoderState = m_state.m_encoderState;

    // Shaders
    LatteDecompilerShader* vertexShader = LatteSHRC_GetActiveVertexShader();
    LatteDecompilerShader* geometryShader = LatteSHRC_GetActiveGeometryShader();
    LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
    const auto fetchShader = LatteSHRC_GetActiveFetchShader();

    bool neverSkipAccurateBarrier = false;

    // "Accurate barriers" is usually enabled globally but since the CPU cost is substantial we allow users to disable it (debug -> 'Accurate barriers' option)
	// We always force accurate barriers for known problematic shaders
	if (pixelShader)
	{
		if (pixelShader->baseHash == 0x6f6f6e7b9aae57af && pixelShader->auxHash == 0x00078787f9249249) // BotW lava
			neverSkipAccurateBarrier = true;
		if (pixelShader->baseHash == 0x4c0bd596e3aef4a6 && pixelShader->auxHash == 0x003c3c3fc9269249) // BotW foam layer for water on the bottom of waterfalls
			neverSkipAccurateBarrier = true;
	}

	// Check if we need to end the render pass
	if (!m_state.m_isFirstDrawInRenderPass && (GetConfig().vk_accurate_barriers || neverSkipAccurateBarrier))
	{
    	// Fragment shader is most likely to require a render pass flush, so check for it first
    	bool endRenderPass = CheckIfRenderPassNeedsFlush(pixelShader);
    	if (!endRenderPass)
    	    endRenderPass = CheckIfRenderPassNeedsFlush(vertexShader);
    	if (!endRenderPass && geometryShader)
    	    endRenderPass = CheckIfRenderPassNeedsFlush(geometryShader);

    	if (endRenderPass)
    	    EndEncoding();
	}

    // Primitive type
    const LattePrimitiveMode primitiveMode = static_cast<LattePrimitiveMode>(LatteGPUState.contextRegister[mmVGT_PRIMITIVE_TYPE]);
    auto mtlPrimitiveType = GetMtlPrimitiveType(primitiveMode);
    bool isPrimitiveRect = (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::RECTS);

    bool usesGeometryShader = (geometryShader != nullptr || isPrimitiveRect);
    bool fetchVertexManually = (usesGeometryShader || fetchShader->mtlFetchVertexManually);

	// Index buffer
	Renderer::INDEX_TYPE hostIndexType;
	uint32 hostIndexCount;
	uint32 indexMin = 0;
	uint32 indexMax = 0;
	uint32 indexBufferOffset = 0;
	uint32 indexBufferIndex = 0;
	LatteIndices_decode(memory_getPointerFromVirtualOffset(indexDataMPTR), indexType, count, primitiveMode, indexMin, indexMax, hostIndexType, hostIndexCount, indexBufferOffset, indexBufferIndex);

	// synchronize vertex and uniform cache and update buffer bindings
	// We need to call this before getting the render command encoder, since it can cause buffer copies
	LatteBufferCache_Sync(indexMin + baseVertex, indexMax + baseVertex, baseInstance, instanceCount);

	// Render pass
	auto renderCommandEncoder = GetRenderCommandEncoder();

    // Render pipeline state
    MTL::RenderPipelineState* renderPipelineState = m_pipelineCache->GetRenderPipelineState(fetchShader, vertexShader, geometryShader, pixelShader, m_state.m_lastUsedFBO.m_attachmentsInfo, m_state.m_activeFBO.m_attachmentsInfo, LatteGPUState.contextNew);
    if (!renderPipelineState)
        return;

    if (renderPipelineState != encoderState.m_renderPipelineState)
   	{
        renderCommandEncoder->setRenderPipelineState(renderPipelineState);
  		encoderState.m_renderPipelineState = renderPipelineState;
   	}

	// Depth stencil state

	// Disable depth write when there is no depth attachment
	auto& depthControl = LatteGPUState.contextNew.DB_DEPTH_CONTROL;
	bool depthWriteEnable = depthControl.get_Z_WRITE_ENABLE();
	if (!m_state.m_activeFBO.m_fbo->depthBuffer.texture)
	    depthControl.set_Z_WRITE_ENABLE(false);

	MTL::DepthStencilState* depthStencilState = m_depthStencilCache->GetDepthStencilState(LatteGPUState.contextNew);
	if (depthStencilState != encoderState.m_depthStencilState)
	{
	    renderCommandEncoder->setDepthStencilState(depthStencilState);
		encoderState.m_depthStencilState = depthStencilState;
	}

	// Restore the original depth write state
	depthControl.set_Z_WRITE_ENABLE(depthWriteEnable);

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

	// Blend color
	float* blendColorConstant = (float*)LatteGPUState.contextRegister + Latte::REGADDR::CB_BLEND_RED;

	// TODO: only set when changed
	renderCommandEncoder->setBlendColor(blendColorConstant[0], blendColorConstant[1], blendColorConstant[2], blendColorConstant[3]);

	// polygon control
	const auto& polygonControlReg = LatteGPUState.contextNew.PA_SU_SC_MODE_CNTL;
	const auto frontFace = polygonControlReg.get_FRONT_FACE();
	uint32 cullFront = polygonControlReg.get_CULL_FRONT();
	uint32 cullBack = polygonControlReg.get_CULL_BACK();
	uint32 polyOffsetFrontEnable = polygonControlReg.get_OFFSET_FRONT_ENABLED();

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

	// Depth clip mode
	cemu_assert_debug(LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_ZCLIP_NEAR_DISABLE() == LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_ZCLIP_FAR_DISABLE()); // near or far clipping can be disabled individually
	bool zClipEnable = LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_ZCLIP_FAR_DISABLE() == false;

	if (zClipEnable != encoderState.m_depthClipEnable)
	{
	    renderCommandEncoder->setDepthClipMode(zClipEnable ? MTL::DepthClipModeClip : MTL::DepthClipModeClamp);
        encoderState.m_depthClipEnable = zClipEnable;
	}

	// Visibility result mode
	if (m_occlusionQuery.m_activeIndex != encoderState.m_visibilityResultOffset)
	{
	    auto mode = (m_occlusionQuery.m_activeIndex == INVALID_UINT32 ? MTL::VisibilityResultModeDisabled : MTL::VisibilityResultModeCounting);
	    renderCommandEncoder->setVisibilityResultMode(mode, m_occlusionQuery.m_activeIndex * sizeof(uint64));
		encoderState.m_visibilityResultOffset = m_occlusionQuery.m_activeIndex;
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

	// Cull front and back is handled by disabling rasterization
	if (!(cullFront && cullBack))
	{
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

	// Vertex buffers
	//std::vector<MTL::Resource*> barrierBuffers;
    for (uint8 i = 0; i < MAX_MTL_BUFFERS; i++)
    {
        auto& vertexBufferRange = m_state.m_vertexBuffers[i];
        if (vertexBufferRange.offset != INVALID_OFFSET)
        {
            /*
            MTL::Buffer* buffer;
            size_t offset;

            // Restride
            if (usesGeometryShader)
            {
                // Object shaders don't need restriding, since the attributes are fetched in the shader
                buffer = m_memoryManager->GetBufferCache();
                offset = m_state.m_vertexBuffers[i].offset;
            }
            else
            {
                uint32 bufferBaseRegisterIndex = mmSQ_VTX_ATTRIBUTE_BLOCK_START + i * 7;
                uint32 bufferStride = (LatteGPUState.contextNew.GetRawView()[bufferBaseRegisterIndex + 2] >> 11) & 0xFFFF;

                auto restridedBuffer = m_memoryManager->RestrideBufferIfNeeded(i, bufferStride, barrierBuffers);

                buffer = restridedBuffer.buffer;
                offset = restridedBuffer.offset;
            }
            */

            MTL::Buffer* buffer = m_memoryManager->GetBufferCache();
            size_t offset = m_state.m_vertexBuffers[i].offset;

            // Bind
            SetBuffer(renderCommandEncoder, GetMtlShaderType(vertexShader->shaderType, usesGeometryShader), buffer, offset, GET_MTL_VERTEX_BUFFER_INDEX(i));
        }
    }

    //if (!barrierBuffers.empty())
    //{
    //    renderCommandEncoder->memoryBarrier(barrierBuffers.data(), barrierBuffers.size(), MTL::RenderStageVertex, MTL::RenderStageVertex);
    //}

	// Prepare streamout
	m_state.m_streamoutState.verticesPerInstance = count;
	LatteStreamout_PrepareDrawcall(count, instanceCount);

	// Uniform buffers, textures and samplers
	BindStageResources(renderCommandEncoder, vertexShader, usesGeometryShader);
	if (geometryShader)
	    BindStageResources(renderCommandEncoder, geometryShader, usesGeometryShader);
	BindStageResources(renderCommandEncoder, pixelShader, usesGeometryShader);

	// Draw
	MTL::Buffer* indexBuffer = nullptr;
	if (hostIndexType != INDEX_TYPE::NONE)
	{
	    auto& bufferAllocator = m_memoryManager->GetTemporaryBufferAllocator();
	    indexBuffer = bufferAllocator.GetBuffer(indexBufferIndex);

		// We have already retrieved the buffer, no need for it to be locked anymore
		bufferAllocator.UnlockBuffer(indexBufferIndex);
	}

	if (usesGeometryShader)
	{
	    if (indexBuffer)
		    SetBuffer(renderCommandEncoder, METAL_SHADER_TYPE_OBJECT, indexBuffer, indexBufferOffset, vertexShader->resourceMapping.indexBufferBinding);

		uint8 hostIndexTypeU8 = (uint8)hostIndexType;
		renderCommandEncoder->setObjectBytes(&hostIndexTypeU8, sizeof(hostIndexTypeU8), vertexShader->resourceMapping.indexTypeBinding);
        encoderState.m_buffers[METAL_SHADER_TYPE_OBJECT][vertexShader->resourceMapping.indexTypeBinding] = {nullptr};

		uint32 verticesPerPrimitive = 0;
		switch (primitiveMode)
        {
        case LattePrimitiveMode::POINTS:
            verticesPerPrimitive = 1;
            break;
        case LattePrimitiveMode::LINES:
            verticesPerPrimitive = 2;
            break;
        case LattePrimitiveMode::TRIANGLES:
        case LattePrimitiveMode::RECTS:
            verticesPerPrimitive = 3;
            break;
        default:
            debug_printf("invalid primitive mode %u\n", (uint32)primitiveMode);
            break;
        }

		renderCommandEncoder->drawMeshThreadgroups(MTL::Size(count * instanceCount / verticesPerPrimitive, 1, 1), MTL::Size(verticesPerPrimitive, 1, 1), MTL::Size(1, 1, 1));
	}
	else
	{
        if (indexBuffer)
       	{
       	    auto mtlIndexType = GetMtlIndexType(hostIndexType);
      		renderCommandEncoder->drawIndexedPrimitives(mtlPrimitiveType, hostIndexCount, mtlIndexType, indexBuffer, indexBufferOffset, instanceCount, baseVertex, baseInstance);
       	}
       	else
       	{
      		renderCommandEncoder->drawPrimitives(mtlPrimitiveType, baseVertex, count, instanceCount, baseInstance);
       	}
	}

	m_state.m_isFirstDrawInRenderPass = false;

	LatteStreamout_FinishDrawcall(false);

	// Debug
	if (fetchVertexManually)
	    m_performanceMonitor.m_manualVertexFetchDraws++;
	if (usesGeometryShader)
	    m_performanceMonitor.m_meshDraws++;
	if (primitiveMode == LattePrimitiveMode::TRIANGLE_FAN)
	    m_performanceMonitor.m_triangleFans++;

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
	// TODO: ucomment?
	if (m_recordedDrawcalls >= m_commitTreshold * 2/* || hasReadback*/)
	{
		CommitCommandBuffer();

        // TODO: where should this be called?
        LatteTextureReadback_UpdateFinishedTransfers(false);
	}
}

void* MetalRenderer::indexData_reserveIndexMemory(uint32 size, uint32& offset, uint32& bufferIndex)
{
    auto& bufferAllocator = m_memoryManager->GetTemporaryBufferAllocator();
    auto allocation = bufferAllocator.GetBufferAllocation(size);
	offset = allocation.offset;
	bufferIndex = allocation.bufferIndex;

	// Lock the buffer so that it doesn't get released
	bufferAllocator.LockBuffer(allocation.bufferIndex);

	return allocation.data;
}

void MetalRenderer::indexData_uploadIndexMemory(uint32 bufferIndex, uint32 offset, uint32 size)
{
    // Do nothing
    /*
    if (!HasUnifiedMemory())
    {
        auto buffer = m_memoryManager->GetTemporaryBufferAllocator().GetBufferOutsideOfCommandBuffer(bufferIndex);
        buffer->didModifyRange(NS::Range(offset, size));
    }
    */
}

LatteQueryObject* MetalRenderer::occlusionQuery_create() {
	return new LatteQueryObjectMtl(this);
}

void MetalRenderer::occlusionQuery_destroy(LatteQueryObject* queryObj) {
    auto queryObjMtl = static_cast<LatteQueryObjectMtl*>(queryObj);
    delete queryObjMtl;
}

void MetalRenderer::occlusionQuery_flush() {
    // TODO: implement
}

void MetalRenderer::occlusionQuery_updateState() {
    // TODO: implement
}

void MetalRenderer::SetBuffer(MTL::RenderCommandEncoder* renderCommandEncoder, MetalShaderType shaderType, MTL::Buffer* buffer, size_t offset, uint32 index)
{
    auto& boundBuffer = m_state.m_encoderState.m_buffers[shaderType][index];
    if (buffer == boundBuffer.m_buffer && offset == boundBuffer.m_offset)
        return;

    if (buffer == boundBuffer.m_buffer)
    {
        // Update just the offset
        boundBuffer.m_offset = offset;

        switch (shaderType)
        {
        case METAL_SHADER_TYPE_VERTEX:
            renderCommandEncoder->setVertexBufferOffset(offset, index);
            break;
        case METAL_SHADER_TYPE_OBJECT:
            renderCommandEncoder->setObjectBufferOffset(offset, index);
            break;
        case METAL_SHADER_TYPE_MESH:
            renderCommandEncoder->setMeshBufferOffset(offset, index);
            break;
        case METAL_SHADER_TYPE_FRAGMENT:
            renderCommandEncoder->setFragmentBufferOffset(offset, index);
            break;
        }

        return;
    }

    boundBuffer = {buffer, offset};

    switch (shaderType)
    {
    case METAL_SHADER_TYPE_VERTEX:
        renderCommandEncoder->setVertexBuffer(buffer, offset, index);
        break;
    case METAL_SHADER_TYPE_OBJECT:
        renderCommandEncoder->setObjectBuffer(buffer, offset, index);
        break;
    case METAL_SHADER_TYPE_MESH:
        renderCommandEncoder->setMeshBuffer(buffer, offset, index);
        break;
    case METAL_SHADER_TYPE_FRAGMENT:
        renderCommandEncoder->setFragmentBuffer(buffer, offset, index);
        break;
    }
}

void MetalRenderer::SetTexture(MTL::RenderCommandEncoder* renderCommandEncoder, MetalShaderType shaderType, MTL::Texture* texture, uint32 index)
{
    auto& boundTexture = m_state.m_encoderState.m_textures[shaderType][index];
    if (texture == boundTexture)
        return;

    boundTexture = texture;

    switch (shaderType)
    {
    case METAL_SHADER_TYPE_VERTEX:
        renderCommandEncoder->setVertexTexture(texture, index);
        break;
    case METAL_SHADER_TYPE_OBJECT:
        renderCommandEncoder->setObjectTexture(texture, index);
        break;
    case METAL_SHADER_TYPE_MESH:
        renderCommandEncoder->setMeshTexture(texture, index);
        break;
    case METAL_SHADER_TYPE_FRAGMENT:
        renderCommandEncoder->setFragmentTexture(texture, index);
        break;
    }
}

void MetalRenderer::SetSamplerState(MTL::RenderCommandEncoder* renderCommandEncoder, MetalShaderType shaderType, MTL::SamplerState* samplerState, uint32 index)
{
    auto& boundSamplerState = m_state.m_encoderState.m_samplers[shaderType][index];
    if (samplerState == boundSamplerState)
        return;

    boundSamplerState = samplerState;

    switch (shaderType)
    {
    case METAL_SHADER_TYPE_VERTEX:
        renderCommandEncoder->setVertexSamplerState(samplerState, index);
        break;
    case METAL_SHADER_TYPE_OBJECT:
        renderCommandEncoder->setObjectSamplerState(samplerState, index);
        break;
    case METAL_SHADER_TYPE_MESH:
        renderCommandEncoder->setMeshSamplerState(samplerState, index);
        break;
    case METAL_SHADER_TYPE_FRAGMENT:
        renderCommandEncoder->setFragmentSamplerState(samplerState, index);
        break;
    }
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

		m_recordedDrawcalls = 0;
		m_commitTreshold = DEFAULT_COMMIT_TRESHOLD;

		// Notify memory manager about the new command buffer
        m_memoryManager->GetTemporaryBufferAllocator().SetActiveCommandBuffer(mtlCommandBuffer);

		return mtlCommandBuffer;
	}
	else
	{
	    return m_commandBuffers.back().m_commandBuffer;
	}
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

    // Debug
    m_performanceMonitor.m_renderPasses++;

    return renderCommandEncoder;
}

// Some render passes clear the attachments, forceRecreate is supposed to be used in those cases
MTL::RenderCommandEncoder* MetalRenderer::GetRenderCommandEncoder(bool forceRecreate)
{
    // Check if we need to begin a new render pass
    if (m_commandEncoder)
    {
        if (!forceRecreate)
        {
            if (m_encoderType == MetalEncoderType::Render)
            {
                bool needsNewRenderPass = (m_state.m_lastUsedFBO.m_fbo == nullptr);
                if (!needsNewRenderPass)
                {
                    for (uint8 i = 0; i < 8; i++)
                    {
                        if (m_state.m_activeFBO.m_fbo->colorBuffer[i].texture && m_state.m_activeFBO.m_fbo->colorBuffer[i].texture != m_state.m_lastUsedFBO.m_fbo->colorBuffer[i].texture)
                        {
                            needsNewRenderPass = true;
                            break;
                        }
                    }
                }

                if (!needsNewRenderPass)
                {
                    if (m_state.m_activeFBO.m_fbo->depthBuffer.texture && (m_state.m_activeFBO.m_fbo->depthBuffer.texture != m_state.m_lastUsedFBO.m_fbo->depthBuffer.texture || ( m_state.m_activeFBO.m_fbo->depthBuffer.hasStencil && !m_state.m_lastUsedFBO.m_fbo->depthBuffer.hasStencil)))
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

    auto renderCommandEncoder = commandBuffer->renderCommandEncoder(m_state.m_activeFBO.m_fbo->GetRenderPassDescriptor());
#ifdef CEMU_DEBUG_ASSERT
    renderCommandEncoder->setLabel(GetLabel("Render command encoder", renderCommandEncoder));
#endif
    m_commandEncoder = renderCommandEncoder;
    m_encoderType = MetalEncoderType::Render;

    // Update state
    m_state.m_lastUsedFBO = m_state.m_activeFBO;
    m_state.m_isFirstDrawInRenderPass = true;

    ResetEncoderState();

    // Debug
    m_performanceMonitor.m_renderPasses++;

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
        if (m_recordedDrawcalls >= m_commitTreshold)
            CommitCommandBuffer();
    }
}

void MetalRenderer::CommitCommandBuffer()
{
    if (m_commandBuffers.size() != 0)
    {
        EndEncoding();

        auto& commandBuffer = m_commandBuffers.back();
        if (!commandBuffer.m_commited)
        {
            // Handled differently, since it seems like Metal doesn't always call the completion handler
            //commandBuffer.m_commandBuffer->addCompletedHandler(^(MTL::CommandBuffer*) {
            //    m_memoryManager->GetTemporaryBufferAllocator().CommandBufferFinished(commandBuffer.m_commandBuffer);
            //});

            commandBuffer.m_commandBuffer->commit();
            commandBuffer.m_commandBuffer->release();
            commandBuffer.m_commited = true;

            m_memoryManager->GetTemporaryBufferAllocator().SetActiveCommandBuffer(nullptr);

            // Debug
            //m_commandQueue->insertDebugCaptureBoundary();
        }
    }
}

bool MetalRenderer::AcquireDrawable(bool mainWindow)
{
    auto& layer = GetLayer(mainWindow);
    if (!layer.GetLayer())
        return false;

    const bool latteBufferUsesSRGB = mainWindow ? LatteGPUState.tvBufferUsesSRGB : LatteGPUState.drcBufferUsesSRGB;
    if (latteBufferUsesSRGB != m_state.m_usesSRGB)
    {
        layer.GetLayer()->setPixelFormat(latteBufferUsesSRGB ? MTL::PixelFormatBGRA8Unorm_sRGB : MTL::PixelFormatBGRA8Unorm);
        m_state.m_usesSRGB = latteBufferUsesSRGB;
    }

    return layer.AcquireDrawable();
}

bool MetalRenderer::CheckIfRenderPassNeedsFlush(LatteDecompilerShader* shader)
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

		auto textureView = m_state.m_textures[hostTextureUnit];
		if (!textureView)
            continue;

		LatteTexture* baseTexture = textureView->baseTexture;
		if (!m_state.m_isFirstDrawInRenderPass)
		{
		    // If the texture is also used in the current render pass, we need to end the render pass to "flush" the texture
			for (uint8 i = 0; i < LATTE_NUM_COLOR_TARGET; i++)
			{
			    auto colorTarget = m_state.m_activeFBO.m_fbo->colorBuffer[i].texture;
				if (colorTarget && colorTarget->baseTexture == baseTexture)
				    return true;
			}
		}
	}

	return false;
}

void MetalRenderer::BindStageResources(MTL::RenderCommandEncoder* renderCommandEncoder, LatteDecompilerShader* shader, bool usesGeometryShader)
{
    auto mtlShaderType = GetMtlShaderType(shader->shaderType, usesGeometryShader);

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

		// TODO: correct?
		uint32 binding = shader->resourceMapping.getTextureBaseBindingPoint() + i;
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
                SetTexture(renderCommandEncoder, mtlShaderType, m_nullTexture1D, binding);
           	else
                SetTexture(renderCommandEncoder, mtlShaderType, m_nullTexture2D, binding);
            SetSamplerState(renderCommandEncoder, mtlShaderType, m_nearestSampler, binding);
            continue;
		}

		if (textureDim == Latte::E_DIM::DIM_1D && (textureView->dim != Latte::E_DIM::DIM_1D))
		{
		    SetTexture(renderCommandEncoder, mtlShaderType, m_nullTexture1D, binding);
			continue;
		}
		else if (textureDim == Latte::E_DIM::DIM_2D && (textureView->dim != Latte::E_DIM::DIM_2D && textureView->dim != Latte::E_DIM::DIM_2D_MSAA))
		{
		    SetTexture(renderCommandEncoder, mtlShaderType, m_nullTexture2D, binding);
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
        SetSamplerState(renderCommandEncoder, mtlShaderType, sampler, binding);

		// get texture register word 0
		uint32 word4 = LatteGPUState.contextRegister[texUnitRegIndex + 4];
		auto& boundTexture = m_state.m_encoderState.m_textures[mtlShaderType][binding];
		MTL::Texture* mtlTexture = textureView->GetSwizzledView(word4);
		SetTexture(renderCommandEncoder, mtlShaderType, mtlTexture, binding);
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
		if (shader->uniform.loc_verticesPerInstance >= 0)
		{
			*(int*)(supportBufferData + ((size_t)shader->uniform.loc_verticesPerInstance / 4)) = m_state.m_streamoutState.verticesPerInstance;
			for (sint32 b = 0; b < LATTE_NUM_STREAMOUT_BUFFER; b++)
			{
				if (shader->uniform.loc_streamoutBufferBase[b] >= 0)
				{
					*(uint32*)GET_UNIFORM_DATA_PTR(shader->uniform.loc_streamoutBufferBase[b]) = m_state.m_streamoutState.buffers[b].ringBufferOffset;
				}
			}
		}

		auto& bufferAllocator = m_memoryManager->GetTemporaryBufferAllocator();
		size_t size = shader->uniform.uniformRangeSize;
		auto supportBuffer = bufferAllocator.GetBufferAllocation(size);
		memcpy(supportBuffer.data, supportBufferData, size);
		auto buffer = bufferAllocator.GetBuffer(supportBuffer.bufferIndex);
		//if (!HasUnifiedMemory())
		//    buffer->didModifyRange(NS::Range(supportBuffer.offset, size));

		SetBuffer(renderCommandEncoder, mtlShaderType, buffer, supportBuffer.offset, shader->resourceMapping.uniformVarsBufferBindingPoint);
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

    		size_t offset = m_state.m_uniformBufferOffsets[GetMtlGeneralShaderType(shader->shaderType)][i];
    		if (offset == INVALID_OFFSET)
                continue;

            SetBuffer(renderCommandEncoder, mtlShaderType, m_memoryManager->GetBufferCache(), offset, binding);
		}
	}

	// Storage buffer
	if (shader->resourceMapping.tfStorageBindingPoint >= 0)
	{
        SetBuffer(renderCommandEncoder, mtlShaderType, m_xfbRingBuffer, 0, shader->resourceMapping.tfStorageBindingPoint);
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

    // Debug
    m_performanceMonitor.m_clears++;
}

void MetalRenderer::CopyBufferToBuffer(MTL::Buffer* src, uint32 srcOffset, MTL::Buffer* dst, uint32 dstOffset, uint32 size, MTL::RenderStages after, MTL::RenderStages before)
{
    // TODO: uncomment and fix performance issues
    // Do the copy in a vertex shader on Apple GPUs
    /*
    if (m_isAppleGPU && m_encoderType == MetalEncoderType::Render)
    {
        auto renderCommandEncoder = static_cast<MTL::RenderCommandEncoder*>(m_commandEncoder);

        MTL::Resource* barrierBuffers[] = {src};
        renderCommandEncoder->memoryBarrier(barrierBuffers, 1, after, after | MTL::RenderStageVertex);

		renderCommandEncoder->setRenderPipelineState(m_copyBufferToBufferPipeline->GetRenderPipelineState());
		m_state.m_encoderState.m_renderPipelineState = m_copyBufferToBufferPipeline->GetRenderPipelineState();

		SetBuffer(renderCommandEncoder, METAL_SHADER_TYPE_VERTEX, src, srcOffset, GET_HELPER_BUFFER_BINDING(0));
		SetBuffer(renderCommandEncoder, METAL_SHADER_TYPE_VERTEX, dst, dstOffset, GET_HELPER_BUFFER_BINDING(1));

		renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypePoint, NS::UInteger(0), NS::UInteger(size));

		barrierBuffers[0] = dst;
        renderCommandEncoder->memoryBarrier(barrierBuffers, 1, before | MTL::RenderStageVertex, before);
    }
    else
    {
    */
        auto blitCommandEncoder = GetBlitCommandEncoder();

        blitCommandEncoder->copyFromBuffer(src, srcOffset, dst, dstOffset, size);
    //}
}

void MetalRenderer::SwapBuffer(bool mainWindow)
{
    if (!AcquireDrawable(mainWindow))
        return;

    auto commandBuffer = GetCommandBuffer();
    GetLayer(mainWindow).PresentDrawable(commandBuffer);
}

void MetalRenderer::EnsureImGuiBackend()
{
    if (!ImGui::GetIO().BackendRendererUserData)
    {
        ImGui_ImplMetal_Init(m_device);
        //ImGui_ImplMetal_CreateFontsTexture(m_device);
    }
}
