#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalLayer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"

#include "HW/Latte/Core/LatteShader.h"
#include "gui/guiWrapper.h"

extern bool hasValidFramebufferAttached;

MetalRenderer::MetalRenderer()
{
    m_device = MTL::CreateSystemDefaultDevice();
    m_commandQueue = m_device->newCommandQueue();
}

MetalRenderer::~MetalRenderer()
{
    m_commandQueue->release();
    m_device->release();
}

// TODO: don't ignore "mainWindow" argument
void MetalRenderer::InitializeLayer(const Vector2i& size, bool mainWindow)
{
    const auto& windowInfo = gui_getWindowInfo().window_main;

    m_metalLayer = (CA::MetalLayer*)CreateMetalLayer(windowInfo.handle);
    m_metalLayer->setDevice(m_device);
}

void MetalRenderer::Initialize()
{
}

void MetalRenderer::Shutdown()
{
    printf("MetalRenderer::Shutdown not implemented\n");
}

bool MetalRenderer::IsPadWindowActive()
{
    printf("MetalRenderer::IsPadWindowActive not implemented\n");

    return false;
}

bool MetalRenderer::GetVRAMInfo(int& usageInMB, int& totalInMB) const
{
    printf("MetalRenderer::GetVRAMInfo not implemented\n");

    usageInMB = 1024;
    totalInMB = 1024;

    return false;
}

void MetalRenderer::ClearColorbuffer(bool padView)
{
    printf("MetalRenderer::ClearColorbuffer not implemented\n");
}

void MetalRenderer::DrawEmptyFrame(bool mainWindow)
{
    printf("MetalRenderer::DrawEmptyFrame not implemented\n");
}

void MetalRenderer::SwapBuffers(bool swapTV, bool swapDRC)
{

    CA::MetalDrawable* drawable = m_metalLayer->nextDrawable();
    if (drawable)
    {
        ensureCommandBuffer();
        m_commandBuffer->presentDrawable(drawable);
    } else
    {
        printf("skipped present!\n");
    }

    if (m_commandBuffer)
    {
        m_commandBuffer->commit();

        m_commandBuffer->release();
        m_commandBuffer = nullptr;

        // Debug
        m_commandQueue->insertDebugCaptureBoundary();
    }
}

void MetalRenderer::DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter,
								sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight,
								bool padView, bool clearBackground)
{
	printf("MetalRenderer::DrawBackbufferQuad not implemented\n");
}

bool MetalRenderer::BeginFrame(bool mainWindow)
{
    // TODO
    return false;
}

void MetalRenderer::Flush(bool waitIdle)
{
    printf("MetalRenderer::Flush not implemented\n");
}

void MetalRenderer::NotifyLatteCommandProcessorIdle()
{
    printf("MetalRenderer::NotifyLatteCommandProcessorIdle not implemented\n");
}

void MetalRenderer::AppendOverlayDebugInfo()
{
    printf("MetalRenderer::AppendOverlayDebugInfo not implemented\n");
}

void MetalRenderer::renderTarget_setViewport(float x, float y, float width, float height, float nearZ, float farZ, bool halfZ)
{
    printf("MetalRenderer::renderTarget_setViewport not implemented\n");
}

void MetalRenderer::renderTarget_setScissor(sint32 scissorX, sint32 scissorY, sint32 scissorWidth, sint32 scissorHeight)
{
    printf("MetalRenderer::renderTarget_setScissor not implemented\n");
}

LatteCachedFBO* MetalRenderer::rendertarget_createCachedFBO(uint64 key)
{
    printf("MetalRenderer::rendertarget_createCachedFBO not implemented\n");

    return nullptr;
}

void MetalRenderer::rendertarget_deleteCachedFBO(LatteCachedFBO* fbo)
{
    printf("MetalRenderer::rendertarget_deleteCachedFBO not implemented\n");
}

void MetalRenderer::rendertarget_bindFramebufferObject(LatteCachedFBO* cfbo)
{
    printf("MetalRenderer::rendertarget_bindFramebufferObject not implemented\n");
}

void* MetalRenderer::texture_acquireTextureUploadBuffer(uint32 size)
{
    return m_memoryManager.GetTextureUploadBuffer(size);
}

void MetalRenderer::texture_releaseTextureUploadBuffer(uint8* mem)
{
    printf("MetalRenderer::texture_releaseTextureUploadBuffer not implemented\n");
}

TextureDecoder* MetalRenderer::texture_chooseDecodedFormat(Latte::E_GX2SURFFMT format, bool isDepth, Latte::E_DIM dim, uint32 width, uint32 height)
{
    printf("decoding format %u\n", (uint32)format);
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
    		printf("invalid depth texture format %u\n", (uint32)format);
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
    		printf("invalid color texture format %u\n", (uint32)format);
    		cemu_assert_debug(false);
    		return nullptr;
    	}
    }
}

void MetalRenderer::texture_clearSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex)
{
    printf("MetalRenderer::texture_clearSlice not implemented\n");
}

void MetalRenderer::texture_loadSlice(LatteTexture* hostTexture, sint32 width, sint32 height, sint32 depth, void* pixelData, sint32 sliceIndex, sint32 mipIndex, uint32 compressedImageSize)
{
    std::cout << "TEXTURE LOAD SLICE" << std::endl;
    auto mtlTexture = (LatteTextureMtl*)hostTexture;

    size_t bytesPerRow = GetMtlTextureBytesPerRow(mtlTexture->GetFormat(), width);
    size_t bytesPerImage = GetMtlTextureBytesPerImage(mtlTexture->GetFormat(), height, bytesPerRow);
    mtlTexture->GetTexture()->replaceRegion(MTL::Region(0, 0, width, height), mipIndex, sliceIndex, pixelData, bytesPerRow, bytesPerImage);
}

void MetalRenderer::texture_clearColorSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex, float r, float g, float b, float a)
{
    printf("MetalRenderer::texture_clearColorSlice not implemented\n");
}

void MetalRenderer::texture_clearDepthSlice(LatteTexture* hostTexture, uint32 sliceIndex, sint32 mipIndex, bool clearDepth, bool clearStencil, float depthValue, uint32 stencilValue)
{
    printf("MetalRenderer::texture_clearDepthSlice not implemented\n");
}

LatteTexture* MetalRenderer::texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth)
{
    return new LatteTextureMtl(this, dim, physAddress, physMipAddress, format, width, height, depth, pitch, mipLevels, swizzle, tileMode, isDepth);
}

void MetalRenderer::texture_setLatteTexture(LatteTextureView* textureView, uint32 textureUnit)
{
    printf("MetalRenderer::texture_setLatteTexture not implemented\n");
}

void MetalRenderer::texture_copyImageSubData(LatteTexture* src, sint32 srcMip, sint32 effectiveSrcX, sint32 effectiveSrcY, sint32 srcSlice, LatteTexture* dst, sint32 dstMip, sint32 effectiveDstX, sint32 effectiveDstY, sint32 dstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight, sint32 srcDepth)
{
    printf("MetalRenderer::texture_copyImageSubData not implemented\n");
}

LatteTextureReadbackInfo* MetalRenderer::texture_createReadback(LatteTextureView* textureView)
{
    printf("MetalRenderer::texture_createReadback not implemented\n");

    return nullptr;
}

void MetalRenderer::surfaceCopy_copySurfaceWithFormatConversion(LatteTexture* sourceTexture, sint32 srcMip, sint32 srcSlice, LatteTexture* destinationTexture, sint32 dstMip, sint32 dstSlice, sint32 width, sint32 height)
{
    printf("MetalRenderer::surfaceCopy_copySurfaceWithFormatConversion not implemented\n");
}

void MetalRenderer::bufferCache_init(const sint32 bufferSize)
{
    printf("MetalRenderer::bufferCache_init not implemented\n");
}

void MetalRenderer::bufferCache_upload(uint8* buffer, sint32 size, uint32 bufferOffset)
{
    printf("MetalRenderer::bufferCache_upload not implemented\n");
}

void MetalRenderer::bufferCache_copy(uint32 srcOffset, uint32 dstOffset, uint32 size)
{
    printf("MetalRenderer::bufferCache_copy not implemented\n");
}

void MetalRenderer::bufferCache_copyStreamoutToMainBuffer(uint32 srcOffset, uint32 dstOffset, uint32 size)
{
    printf("MetalRenderer::bufferCache_copyStreamoutToMainBuffer not implemented\n");
}

void MetalRenderer::buffer_bindVertexBuffer(uint32 bufferIndex, uint32 offset, uint32 size)
{
    printf("MetalRenderer::buffer_bindVertexBuffer not implemented\n");
}

void MetalRenderer::buffer_bindUniformBuffer(LatteConst::ShaderType shaderType, uint32 bufferIndex, uint32 offset, uint32 size)
{
    printf("MetalRenderer::buffer_bindUniformBuffer not implemented\n");
}

RendererShader* MetalRenderer::shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool isGameShader, bool isGfxPackShader)
{
    return new RendererShaderMtl(this, type, baseHash, auxHash, isGameShader, isGfxPackShader, source);
}

void MetalRenderer::streamout_setupXfbBuffer(uint32 bufferIndex, sint32 ringBufferOffset, uint32 rangeAddr, uint32 rangeSize)
{
    printf("MetalRenderer::streamout_setupXfbBuffer not implemented\n");
}

void MetalRenderer::streamout_begin()
{
    printf("MetalRenderer::streamout_begin not implemented\n");
}

void MetalRenderer::streamout_rendererFinishDrawcall()
{
    printf("MetalRenderer::streamout_rendererFinishDrawcall not implemented\n");
}

void MetalRenderer::draw_beginSequence()
{
    skipDraws = false;

    // update shader state
	LatteSHRC_UpdateActiveShaders();
	if (LatteGPUState.activeShaderHasError)
	{
		cemuLog_logDebugOnce(LogType::Force, "Skipping drawcalls due to shader error");
		skipDraws = true;
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
			skipDraws = true;
			return; // no render target
		}

		if (!hasValidFramebufferAttached)
		{
			debug_printf("Drawcall with no color buffer or depth buffer attached\n");
			skipDraws = true;
			return; // no render target
		}
		LatteTexture_updateTextures();
		if (!LatteGPUState.repeatTextureInitialization)
			break;
	}

	// apply render target
	// HACK: not implemented yet
	//LatteMRT::ApplyCurrentState();

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
		skipDraws = true;
}

void MetalRenderer::draw_execute(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, bool isFirst)
{
    printf("MetalRenderer::draw_execute not implemented\n");
}

void MetalRenderer::draw_endSequence()
{
    printf("MetalRenderer::draw_endSequence not implemented\n");
}

void* MetalRenderer::indexData_reserveIndexMemory(uint32 size, uint32& offset, uint32& bufferIndex)
{
    printf("MetalRenderer::indexData_reserveIndexMemory not implemented\n");

    return nullptr;
}

void MetalRenderer::indexData_uploadIndexMemory(uint32 offset, uint32 size)
{
    printf("MetalRenderer::indexData_uploadIndexMemory not implemented\n");
}
