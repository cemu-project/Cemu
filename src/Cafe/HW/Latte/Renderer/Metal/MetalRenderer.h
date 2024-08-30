#pragma once

#include "Cafe/HW/Latte/Renderer/Renderer.h"

#include "Cafe/HW/Latte/Renderer/Metal/MetalLayerHandle.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalPerformanceMonitor.h"

struct MetalBufferAllocation
{
    void* data;
    uint32 bufferIndex;
    size_t offset = INVALID_OFFSET;
    size_t size;

    bool IsValid() const
    {
        return offset != INVALID_OFFSET;
    }
};

struct MetalRestrideInfo
{
    bool memoryInvalidated = true;
    size_t lastStride = 0;
    MetalBufferAllocation allocation{};
};

struct MetalBoundBuffer
{
    size_t offset = INVALID_OFFSET;
    size_t size = 0;
    // Memory manager will write restride info to this variable
    MetalRestrideInfo restrideInfo;
};

enum MetalGeneralShaderType
{
    METAL_GENERAL_SHADER_TYPE_VERTEX,
    METAL_GENERAL_SHADER_TYPE_GEOMETRY,
    METAL_GENERAL_SHADER_TYPE_FRAGMENT,

    METAL_GENERAL_SHADER_TYPE_TOTAL
};

inline MetalGeneralShaderType GetMtlGeneralShaderType(LatteConst::ShaderType shaderType)
{
    switch (shaderType)
    {
    case LatteConst::ShaderType::Vertex:
        return METAL_GENERAL_SHADER_TYPE_VERTEX;
    case LatteConst::ShaderType::Geometry:
        return METAL_GENERAL_SHADER_TYPE_GEOMETRY;
    case LatteConst::ShaderType::Pixel:
        return METAL_GENERAL_SHADER_TYPE_FRAGMENT;
    default:
        return METAL_GENERAL_SHADER_TYPE_TOTAL;
    }
}

enum MetalShaderType
{
    METAL_SHADER_TYPE_VERTEX,
    METAL_SHADER_TYPE_OBJECT,
    METAL_SHADER_TYPE_MESH,
    METAL_SHADER_TYPE_FRAGMENT,

    METAL_SHADER_TYPE_TOTAL
};

inline MetalShaderType GetMtlShaderType(LatteConst::ShaderType shaderType, bool usesGeometryShader)
{
    switch (shaderType)
    {
    case LatteConst::ShaderType::Vertex:
        if (usesGeometryShader)
            return METAL_SHADER_TYPE_OBJECT;
        else
            return METAL_SHADER_TYPE_VERTEX;
    case LatteConst::ShaderType::Geometry:
        return METAL_SHADER_TYPE_MESH;
    case LatteConst::ShaderType::Pixel:
        return METAL_SHADER_TYPE_FRAGMENT;
    default:
        return METAL_SHADER_TYPE_TOTAL;
    }
}

struct MetalEncoderState
{
    MTL::RenderPipelineState* m_renderPipelineState = nullptr;
    MTL::DepthStencilState* m_depthStencilState = nullptr;
    MTL::CullMode m_cullMode = MTL::CullModeNone;
    MTL::Winding m_frontFaceWinding = MTL::WindingClockwise;
    MTL::Viewport m_viewport;
    MTL::ScissorRect m_scissor;
    uint32 m_stencilRefFront = 0;
    uint32 m_stencilRefBack = 0;
    uint32 m_depthBias = 0;
   	uint32 m_depthSlope = 0;
   	uint32 m_depthClamp = 0;
    bool m_depthClipEnable = true;
    struct {
        MTL::Buffer* m_buffer;
        size_t m_offset;
    } m_buffers[METAL_SHADER_TYPE_TOTAL][MAX_MTL_BUFFERS];
    MTL::Texture* m_textures[METAL_SHADER_TYPE_TOTAL][MAX_MTL_TEXTURES];
    MTL::SamplerState* m_samplers[METAL_SHADER_TYPE_TOTAL][MAX_MTL_SAMPLERS];
};

struct MetalStreamoutState
{
	struct
	{
		bool enabled;
		uint32 ringBufferOffset;
	} buffers[LATTE_NUM_STREAMOUT_BUFFER];
	sint32 verticesPerInstance;
};

struct MetalState
{
    MetalEncoderState m_encoderState{};

    bool m_usesSRGB = false;

    bool m_skipDrawSequence = false;
    bool m_isFirstDrawInRenderPass = true;

    class CachedFBOMtl* m_activeFBO = nullptr;
    // If the FBO changes, but it's the same FBO as the last one with some omitted attachments, this FBO doesn't change'
    class CachedFBOMtl* m_lastUsedFBO = nullptr;

    MetalBoundBuffer m_vertexBuffers[MAX_MTL_BUFFERS] = {{}};
    // TODO: find out what is the max number of bound textures on the Wii U
    class LatteTextureViewMtl* m_textures[64] = {nullptr};
    size_t m_uniformBufferOffsets[METAL_GENERAL_SHADER_TYPE_TOTAL][MAX_MTL_BUFFERS];

    MTL::Viewport m_viewport;
    MTL::ScissorRect m_scissor;

    MetalStreamoutState m_streamoutState;
};

struct MetalCommandBuffer
{
    MTL::CommandBuffer* m_commandBuffer;
    bool m_commited = false;
};

enum class MetalEncoderType
{
    None,
    Render,
    Compute,
    Blit,
};

// HACK: Dummy occlusion query object for Metal
class LatteQueryObjectMtl : public LatteQueryObject
{
public:
	LatteQueryObjectMtl(class MetalRenderer* mtlRenderer) : m_mtlr{mtlRenderer} {}

	bool getResult(uint64& numSamplesPassed) override
	{
	    cemuLog_log(LogType::MetalLogging, "LatteQueryObjectMtl::getResult: occlusion queries are not yet supported on Metal");
        return true;
	}

	void begin() override
	{
	    cemuLog_log(LogType::MetalLogging, "LatteQueryObjectMtl::begin: occlusion queries are not yet supported on Metal");
	}

	void end() override
	{
	    cemuLog_log(LogType::MetalLogging, "LatteQueryObjectMtl::end: occlusion queries are not yet supported on Metal");
	}

private:
	class MetalRenderer* m_mtlr;
};

class MetalRenderer : public Renderer
{
public:
    static const inline int TEXTURE_READBACK_SIZE = 32 * 1024 * 1024; // 32 MB

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
	void ShutdownLayer(bool mainWindow);
	void ResizeLayer(const Vector2i& size, bool mainWindow);

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
	bool ImguiBegin(bool mainWindow) override;
	void ImguiEnd() override;
	ImTextureID GenerateTexture(const std::vector<uint8>& data, const Vector2i& size) override;
	void DeleteTexture(ImTextureID id) override;
	void DeleteFontTextures() override;

	bool UseTFViaSSBO() const override { return true; }
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
	void indexData_uploadIndexMemory(uint32 bufferIndex, uint32 offset, uint32 size) override;

	// occlusion queries
	LatteQueryObject* occlusionQuery_create() override {
	    cemuLog_log(LogType::MetalLogging, "MetalRenderer::occlusionQuery_create: Occlusion queries are not yet supported on Metal");

		return new LatteQueryObjectMtl(this);
	}

	void occlusionQuery_destroy(LatteQueryObject* queryObj) override {
	    cemuLog_log(LogType::MetalLogging, "MetalRenderer::occlusionQuery_destroy: occlusion queries are not yet supported on Metal");
	}

	void occlusionQuery_flush() override {
	    cemuLog_log(LogType::MetalLogging, "MetalRenderer::occlusionQuery_flush: occlusion queries are not yet supported on Metal");
	}

	void occlusionQuery_updateState() override {
	    cemuLog_log(LogType::MetalLogging, "MetalRenderer::occlusionQuery_updateState: occlusion queries are not yet supported on Metal");
	}

	// Helpers
	MetalPerformanceMonitor& GetPerformanceMonitor() { return m_performanceMonitor; }

	MTL::CommandBuffer* GetCurrentCommandBuffer()
    {
        cemu_assert_debug(m_commandBuffers.size() != 0);

        return m_commandBuffers[m_commandBuffers.size() - 1].m_commandBuffer;
    }

    MTL::CommandEncoder* GetCommandEncoder()
    {
        return m_commandEncoder;
    }

    MetalEncoderType GetEncoderType()
    {
        return m_encoderType;
    }

    void ResetEncoderState()
    {
        m_state.m_encoderState = {};

        // TODO: set viewport and scissor to render target dimensions if render commands

        for (uint32 i = 0; i < METAL_SHADER_TYPE_TOTAL; i++)
        {
            for (uint32 j = 0; j < MAX_MTL_BUFFERS; j++)
                m_state.m_encoderState.m_buffers[i][j] = {nullptr};
            for (uint32 j = 0; j < MAX_MTL_TEXTURES; j++)
                m_state.m_encoderState.m_textures[i][j] = nullptr;
            for (uint32 j = 0; j < MAX_MTL_SAMPLERS; j++)
                m_state.m_encoderState.m_samplers[i][j] = nullptr;
        }
    }

    MetalEncoderState& GetEncoderState()
    {
        return m_state.m_encoderState;
    }

    void SetBuffer(MTL::RenderCommandEncoder* renderCommandEncoder, MetalShaderType shaderType, MTL::Buffer* buffer, size_t offset, uint32 index);
    void SetTexture(MTL::RenderCommandEncoder* renderCommandEncoder, MetalShaderType shaderType, MTL::Texture* texture, uint32 index);
    void SetSamplerState(MTL::RenderCommandEncoder* renderCommandEncoder, MetalShaderType shaderType, MTL::SamplerState* samplerState, uint32 index);

	MTL::CommandBuffer* GetCommandBuffer();
	bool CommandBufferCompleted(MTL::CommandBuffer* commandBuffer);
	void WaitForCommandBufferCompletion(MTL::CommandBuffer* commandBuffer);
	MTL::RenderCommandEncoder* GetTemporaryRenderCommandEncoder(MTL::RenderPassDescriptor* renderPassDescriptor);
	MTL::RenderCommandEncoder* GetRenderCommandEncoder(bool forceRecreate = false);
    MTL::ComputeCommandEncoder* GetComputeCommandEncoder();
    MTL::BlitCommandEncoder* GetBlitCommandEncoder();
    void EndEncoding();
    void CommitCommandBuffer();

    bool AcquireDrawable(bool mainWindow);

    bool CheckIfRenderPassNeedsFlush(LatteDecompilerShader* shader);
    void BindStageResources(MTL::RenderCommandEncoder* renderCommandEncoder, LatteDecompilerShader* shader, bool usesGeometryShader);

    void ClearColorTextureInternal(MTL::Texture* mtlTexture, sint32 sliceIndex, sint32 mipIndex, float r, float g, float b, float a);

    void CopyBufferToBuffer(MTL::Buffer* src, uint32 srcOffset, MTL::Buffer* dst, uint32 dstOffset, uint32 size);

    // Getters
    bool IsAppleGPU() const
    {
        return m_isAppleGPU;
    }

    bool HasUnifiedMemory() const
    {
        return m_hasUnifiedMemory;
    }

    bool SupportsMetal3() const
    {
        return m_supportsMetal3;
    }

    const MetalPixelFormatSupport& GetPixelFormatSupport() const
    {
        return m_pixelFormatSupport;
    }

    //MTL::StorageMode GetOptimalTextureStorageMode() const
    //{
    //    return (m_isAppleGPU ? MTL::StorageModeShared : MTL::StorageModePrivate);
    //}

    MTL::ResourceOptions GetOptimalBufferStorageMode() const
    {
        return (m_hasUnifiedMemory ? MTL::ResourceStorageModeShared : MTL::ResourceStorageModeManaged);
    }

    MTL::Buffer* GetTextureReadbackBuffer() const
    {
        return m_readbackBuffer;
    }

private:
	MetalLayerHandle m_mainLayer;
	MetalLayerHandle m_padLayer;

	MetalPerformanceMonitor m_performanceMonitor;

	// Metal objects
	MTL::Device* m_device;
	MTL::CommandQueue* m_commandQueue;

	// Feature support
	bool m_isAppleGPU;
	bool m_hasUnifiedMemory;
	bool m_supportsMetal3;
	uint32 m_recommendedMaxVRAMUsage;
	MetalPixelFormatSupport m_pixelFormatSupport;

	// Managers and caches
	class MetalMemoryManager* m_memoryManager;
	class MetalPipelineCache* m_pipelineCache;
	class MetalDepthStencilCache* m_depthStencilCache;
	class MetalSamplerCache* m_samplerCache;

	// Pipelines
	MTL::RenderPipelineState* m_presentPipelineLinear;
	MTL::RenderPipelineState* m_presentPipelineSRGB;

	// Hybrid pipelines
	class MetalHybridComputePipeline* m_copyBufferToBufferPipeline;
	//class MetalHybridComputePipeline* m_copyTextureToTexturePipeline;
	class MetalHybridComputePipeline* m_restrideBufferPipeline;

	// Resources
	MTL::SamplerState* m_nearestSampler;
	MTL::SamplerState* m_linearSampler;

	// Null resources
	MTL::Texture* m_nullTexture1D;
	MTL::Texture* m_nullTexture2D;

	// Texture readback
	MTL::Buffer* m_readbackBuffer;
	uint32 m_readbackBufferWriteOffset = 0;

	// Transform feedback
	MTL::Buffer* m_xfbRingBuffer;

	// Active objects
	std::vector<MetalCommandBuffer> m_commandBuffers;
	MetalEncoderType m_encoderType = MetalEncoderType::None;
	MTL::CommandEncoder* m_commandEncoder = nullptr;

    uint32 m_recordedDrawcalls = 0;

	// State
	MetalState m_state;

	MetalLayerHandle& GetLayer(bool mainWindow)
	{
	    return (mainWindow ? m_mainLayer : m_padLayer);
	}

	void SwapBuffer(bool mainWindow);

	void EnsureImGuiBackend();
};