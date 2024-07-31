#pragma once

#include <Foundation/Foundation.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalMemoryManager.h"
#include "Metal/MTLComputeCommandEncoder.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPass.hpp"
#include "Metal/MTLRenderPipeline.hpp"

#define MAX_MTL_BUFFERS 31
#define GET_MTL_VERTEX_BUFFER_INDEX(index) (MAX_MTL_BUFFERS - index - 2)
// TODO: don't harcdode the support buffer binding
#define MTL_SUPPORT_BUFFER_BINDING 30

#define MAX_MTL_TEXTURES 31

struct MetalBoundBuffer
{
    bool needsRebind = false;
    sint32 offset = -1;
};

struct MetalState
{
    bool skipDrawSequence = false;
    class CachedFBOMtl* activeFBO = nullptr;
    MetalBoundBuffer vertexBuffers[MAX_MTL_BUFFERS] = {{}};
    class LatteTextureViewMtl* textures[MAX_MTL_TEXTURES] = {nullptr};
    MTL::Texture* colorRenderTargets[8] = {nullptr};
    MTL::Texture* depthRenderTarget = nullptr;
    MTL::Viewport viewport = {0, 0, 0, 0, 0, 0};
    MTL::ScissorRect scissor = {0, 0, 0, 0};
};

enum class MetalEncoderType
{
    None,
    Render,
    Compute,
    Blit,
};

class MetalRenderer : public Renderer
{
public:
    MetalRenderer();
	~MetalRenderer() override;

	RendererAPI GetType() override
	{
	    return RendererAPI::Metal;
	}

	static MetalRenderer* GetInstance() {
	    return static_cast<MetalRenderer*>(g_renderer.get());
	}

	// Helper functions
	MTL::Device* GetDevice() const {
        return m_device;
    }

	void InitializeLayer(const Vector2i& size, bool mainWindow);

	void Initialize() override;
	void Shutdown() override;
	bool IsPadWindowActive() override;

	bool GetVRAMInfo(int& usageInMB, int& totalInMB) const override;

	void ClearColorbuffer(bool padView) override;
	void DrawEmptyFrame(bool mainWindow) override;
	void SwapBuffers(bool swapTV, bool swapDRC) override;

	void HandleScreenshotRequest(LatteTextureView* texView, bool padView) override {
	    cemuLog_log(LogType::MetalLogging, "Screenshots are not yet supported on Metal");
	}

	void DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter,
									sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight,
									bool padView, bool clearBackground) override;
	bool BeginFrame(bool mainWindow) override;

	// flush control
	void Flush(bool waitIdle = false) override;		// called when explicit flush is required (e.g. by imgui)
	void NotifyLatteCommandProcessorIdle() override; // called when command processor has no more commands available or when stalled

	// imgui
	bool ImguiBegin(bool mainWindow) override {
	    cemuLog_log(LogType::MetalLogging, "Imgui is not yet supported on Metal");

		return false;
	};

	void ImguiEnd() override {
	    cemuLog_log(LogType::MetalLogging, "Imgui is not yet supported on Metal");
	};

	ImTextureID GenerateTexture(const std::vector<uint8>& data, const Vector2i& size) override {
	    cemuLog_log(LogType::MetalLogging, "Imgui is not yet supported on Metal");

	    return nullptr;
	};

	void DeleteTexture(ImTextureID id) override {
	    cemuLog_log(LogType::MetalLogging, "Imgui is not yet supported on Metal");
	};

	void DeleteFontTextures() override {
	    cemuLog_log(LogType::MetalLogging, "Imgui is not yet supported on Metal");
	};

	void AppendOverlayDebugInfo() override;

	// rendertarget
	void renderTarget_setViewport(float x, float y, float width, float height, float nearZ, float farZ, bool halfZ = false) override;
	void renderTarget_setScissor(sint32 scissorX, sint32 scissorY, sint32 scissorWidth, sint32 scissorHeight) override;

	LatteCachedFBO* rendertarget_createCachedFBO(uint64 key) override;
	void rendertarget_deleteCachedFBO(LatteCachedFBO* fbo) override;
	void rendertarget_bindFramebufferObject(LatteCachedFBO* cfbo) override;

	// texture functions
	void* texture_acquireTextureUploadBuffer(uint32 size) override;
	void texture_releaseTextureUploadBuffer(uint8* mem) override;

	TextureDecoder* texture_chooseDecodedFormat(Latte::E_GX2SURFFMT format, bool isDepth, Latte::E_DIM dim, uint32 width, uint32 height) override;

	void texture_clearSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex) override;
	void texture_loadSlice(LatteTexture* hostTexture, sint32 width, sint32 height, sint32 depth, void* pixelData, sint32 sliceIndex, sint32 mipIndex, uint32 compressedImageSize) override;
	void texture_clearColorSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex, float r, float g, float b, float a) override;
	void texture_clearDepthSlice(LatteTexture* hostTexture, uint32 sliceIndex, sint32 mipIndex, bool clearDepth, bool clearStencil, float depthValue, uint32 stencilValue) override;

	LatteTexture* texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth) override;

	void texture_setLatteTexture(LatteTextureView* textureView, uint32 textureUnit) override;
	void texture_copyImageSubData(LatteTexture* src, sint32 srcMip, sint32 effectiveSrcX, sint32 effectiveSrcY, sint32 srcSlice, LatteTexture* dst, sint32 dstMip, sint32 effectiveDstX, sint32 effectiveDstY, sint32 dstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight, sint32 srcDepth) override;

	LatteTextureReadbackInfo* texture_createReadback(LatteTextureView* textureView) override;

	// surface copy
	void surfaceCopy_copySurfaceWithFormatConversion(LatteTexture* sourceTexture, sint32 srcMip, sint32 srcSlice, LatteTexture* destinationTexture, sint32 dstMip, sint32 dstSlice, sint32 width, sint32 height) override;

	// buffer cache
	void bufferCache_init(const sint32 bufferSize) override;
	void bufferCache_upload(uint8* buffer, sint32 size, uint32 bufferOffset) override;
	void bufferCache_copy(uint32 srcOffset, uint32 dstOffset, uint32 size) override;
	void bufferCache_copyStreamoutToMainBuffer(uint32 srcOffset, uint32 dstOffset, uint32 size) override;

	void buffer_bindVertexBuffer(uint32 bufferIndex, uint32 offset, uint32 size) override;
	void buffer_bindUniformBuffer(LatteConst::ShaderType shaderType, uint32 bufferIndex, uint32 offset, uint32 size) override;

	// shader
	RendererShader* shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool compileAsync, bool isGfxPackSource) override;

	// streamout
	void streamout_setupXfbBuffer(uint32 bufferIndex, sint32 ringBufferOffset, uint32 rangeAddr, uint32 rangeSize) override;
	void streamout_begin() override;
	void streamout_rendererFinishDrawcall() override;

	// core drawing logic
	void draw_beginSequence() override;
	void draw_execute(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, bool isFirst) override;
	void draw_endSequence() override;

	// index
	void* indexData_reserveIndexMemory(uint32 size, uint32& offset, uint32& bufferIndex) override;
	void indexData_uploadIndexMemory(uint32 offset, uint32 size) override;

	// occlusion queries
	LatteQueryObject* occlusionQuery_create() override {
	    cemuLog_log(LogType::MetalLogging, "Occlusion queries are not yet supported on Metal");

		return nullptr;
	}

	void occlusionQuery_destroy(LatteQueryObject* queryObj) override {
	    cemuLog_log(LogType::MetalLogging, "Occlusion queries are not yet supported on Metal");
	}

	void occlusionQuery_flush() override {
	    cemuLog_log(LogType::MetalLogging, "Occlusion queries are not yet supported on Metal");
	}

	void occlusionQuery_updateState() override {
	    cemuLog_log(LogType::MetalLogging, "Occlusion queries are not yet supported on Metal");
	}


private:
	CA::MetalLayer* m_metalLayer;

	MetalMemoryManager* m_memoryManager;

	// Metal objects
	MTL::Device* m_device;
	MTL::CommandQueue* m_commandQueue;

	// Pipelines
	MTL::RenderPipelineState* m_presentPipeline;

	// Basic
	MTL::SamplerState* m_nearestSampler;

	// Active objects
	MTL::CommandBuffer* m_commandBuffer = nullptr;
	MetalEncoderType m_encoderType = MetalEncoderType::None;
	MTL::CommandEncoder* m_commandEncoder = nullptr;
	CA::MetalDrawable* m_drawable;

	// State
	MetalState m_state;

	// Helpers
	void EnsureCommandBuffer()
	{
	    if (!m_commandBuffer)
		{
            // Debug
            m_commandQueue->insertDebugCaptureBoundary();

		    m_commandBuffer = m_commandQueue->commandBuffer();
		}
	}

	MTL::RenderCommandEncoder* GetRenderCommandEncoder(MTL::RenderPassDescriptor* renderPassDescriptor, MTL::Texture* colorRenderTargets[8], MTL::Texture* depthRenderTarget)
    {
        EnsureCommandBuffer();

        // Check if we need to begin a new render pass
        if (m_commandEncoder)
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

        // Rebind all the render state
        RebindRenderState(renderCommandEncoder);

        return renderCommandEncoder;
    }

    MTL::ComputeCommandEncoder* GetComputeCommandEncoder()
    {
        if (m_commandEncoder)
        {
            if (m_encoderType != MetalEncoderType::Compute)
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

    MTL::BlitCommandEncoder* GetBlitCommandEncoder()
    {
        if (m_commandEncoder)
        {
            if (m_encoderType != MetalEncoderType::Blit)
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

    void EndEncoding()
    {
        if (m_commandEncoder)
        {
            m_commandEncoder->endEncoding();
            m_commandEncoder->release();
            m_commandEncoder = nullptr;
        }
    }

    void CommitCommandBuffer()
    {
        EndEncoding();

        if (m_commandBuffer)
        {
            m_commandBuffer->commit();
            m_commandBuffer->release();
            m_commandBuffer = nullptr;

            // Reset temporary buffers
            m_memoryManager->ResetTemporaryBuffers();

            // Debug
            m_commandQueue->insertDebugCaptureBoundary();
        }
    }

    void BindStageResources(MTL::RenderCommandEncoder* renderCommandEncoder, LatteDecompilerShader* shader);

    void RebindRenderState(MTL::RenderCommandEncoder* renderCommandEncoder);
};
