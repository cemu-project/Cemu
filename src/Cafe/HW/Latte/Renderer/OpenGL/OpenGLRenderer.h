#pragma once
#include "Common/GLInclude/GLInclude.h"

#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Renderer/RendererOuputShader.h"

#define GPU_GL_MAX_NUM_ATTRIBUTE		(16) // Wii U GPU supports more than 16 but not all desktop GPUs do. Have to keep this at 16 until we find a better solution

class OpenGLRenderer : public Renderer
{
	friend class OpenGLCanvas;
public:
	OpenGLRenderer();
	~OpenGLRenderer();

	RendererAPI GetType() override { return RendererAPI::OpenGL; }

	static OpenGLRenderer* GetInstance();

	// imgui
	bool ImguiBegin(bool mainWindow) override;
	void ImguiEnd() override;
	ImTextureID GenerateTexture(const std::vector<uint8>& data, const Vector2i& size) override;
	void DeleteTexture(ImTextureID id) override;
	void DeleteFontTextures() override;

	void Initialize() override;
	bool IsPadWindowActive() override;

	void Flush(bool waitIdle = false) override;
	void NotifyLatteCommandProcessorIdle() override;

	void EnableDebugMode() override;
	void SwapBuffers(bool swapTV = true, bool swapDRC = true) override;

	bool BeginFrame(bool mainWindow) override;
	void DrawEmptyFrame(bool mainWindow) override;
	void ClearColorbuffer(bool padView) override;
	void HandleScreenshotRequest(LatteTextureView* texView, bool padView) override;

	void DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter, sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight, bool padView, bool clearBackground) override;

	void AppendOverlayDebugInfo() override
	{
		// does nothing currently
	}

	// rendertarget
	void renderTarget_setViewport(float x, float y, float width, float height, float nearZ, float farZ, bool halfZ = false) override;
	void renderTarget_setScissor(sint32 scissorX, sint32 scissorY, sint32 scissorWidth, sint32 scissorHeight) override;

	LatteCachedFBO* rendertarget_createCachedFBO(uint64 key) override;
	void rendertarget_deleteCachedFBO(LatteCachedFBO* cfbo) override;
	void rendertarget_bindFramebufferObject(LatteCachedFBO* cfbo) override;

	// renderstate functions
	void renderstate_setChannelTargetMask(uint32 renderTargetMask);
	void renderstate_setAlwaysWriteDepth();
	void renderstate_updateBlendingAndColorControl();
	void renderstate_resetColorControl();
	void renderstate_resetDepthControl();
	void renderstate_resetStencilMask();

	void renderstate_updateTextureSettingsGL(LatteDecompilerShader* shaderContext, LatteTextureView* _hostTextureView, uint32 hostTextureUnit, const Latte::LATTE_SQ_TEX_RESOURCE_WORD4_N texUnitWord4, uint32 texUnitIndex, bool isDepthSampler);

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
	void texture_bindAndActivate(LatteTextureView* textureView, uint32 textureUnit);
	void texture_copyImageSubData(LatteTexture* src, sint32 srcMip, sint32 effectiveSrcX, sint32 effectiveSrcY, sint32 srcSlice, LatteTexture* dst, sint32 dstMip, sint32 effectiveDstX, sint32 effectiveDstY, sint32 dstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight, sint32 srcDepth) override;

	void texture_notifyDelete(LatteTextureView* textureView);

	LatteTextureReadbackInfo* texture_createReadback(LatteTextureView* textureView) override;

	void surfaceCopy_copySurfaceWithFormatConversion(LatteTexture* sourceTexture, sint32 srcMip, sint32 srcSlice, LatteTexture* destinationTexture, sint32 dstMip, sint32 dstSlice, sint32 width, sint32 height) override;

	void attributeStream_reset();
	void bufferCache_init(const sint32 bufferSize) override;
	void bufferCache_upload(uint8* buffer, sint32 size, uint32 bufferOffset) override;
	void bufferCache_copy(uint32 srcOffset, uint32 dstOffset, uint32 size) override;

	void buffer_bindVertexBuffer(uint32 bufferIndex, uint32 offset, uint32 size) override;
	void buffer_bindUniformBuffer(LatteConst::ShaderType shaderType, uint32 bufferIndex, uint32 offset, uint32 size) override;

	void _setupVertexAttributes();

	void attributeStream_bindVertexCacheBuffer();
	void attributeStream_unbindVertexBuffer();

	static void SetAttributeArrayState(uint32 index, bool isEnabled, sint32 aluDivisor);
	static void SetArrayElementBuffer(GLuint arrayElementBuffer);

	// index
	void* indexData_reserveIndexMemory(uint32 size, uint32& offset, uint32& bufferIndex) override
	{
		assert_dbg();
		return nullptr;
	}

	void indexData_uploadIndexMemory(uint32 offset, uint32 size) override
	{
		assert_dbg();
	}

	// uniform
	void uniformData_update();

	// shader
	RendererShader* shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool isGameShader, bool isGfxPackShader) override;
	void shader_bind(RendererShader* shader);
	void shader_unbind(RendererShader::ShaderType shaderType);

	// streamout
	void streamout_setupXfbBuffer(uint32 bufferIndex, sint32 ringBufferOffset, uint32 rangeAddr, uint32 rangeSize) override;
	void streamout_begin() override;
	void bufferCache_copyStreamoutToMainBuffer(uint32 srcOffset, uint32 dstOffset, uint32 size) override;
	void streamout_rendererFinishDrawcall() override;

	// draw
	void draw_init();

	void draw_beginSequence() override;
	void draw_execute(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, bool isFirst) override;
	void draw_endSequence() override;

	template<bool TIsMinimal, bool THasProfiling>
	void draw_genericDrawHandler(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType);

	// resource garbage collection
	void cleanupAfterFrame();
	void ReleaseBufferCacheEntries();

	// occlusion queries
	LatteQueryObject* occlusionQuery_create() override;
	void occlusionQuery_destroy(LatteQueryObject* queryObj) override;
	void occlusionQuery_flush() override;
	void occlusionQuery_updateState() override {};

private:
	void GetVendorInformation() override;

	void texture_setActiveTextureUnit(sint32 index);

	void texture_syncSliceSpecialBC4(LatteTexture* srcTexture, sint32 srcSliceIndex, sint32 srcMipIndex, LatteTexture* dstTexture, sint32 dstSliceIndex, sint32 dstMipIndex);
	void texture_syncSliceSpecialIntegerToBC3(LatteTexture* srcTexture, sint32 srcSliceIndex, sint32 srcMipIndex, LatteTexture* dstTexture, sint32 dstSliceIndex, sint32 dstMipIndex);

	GLuint m_pipeline = 0;

	bool m_isPadViewContext{};

	// rendertarget viewport
	float prevViewportX = 0;
	float prevViewportY = 0;
	float prevViewportWidth = 0;
	float prevViewportHeight = 0;

	float prevNearZ = -999999.0f;
	float prevFarZ = -9999999.0f;
	bool _prevInvertY = false;
	bool _prevHalfZ = false;

	uint32 prevScissorX = 0;
	uint32 prevScissorY = 0;
	uint32 prevScissorWidth = 0;
	uint32 prevScissorHeight = 0;
	bool prevScissorEnable = false;

	bool m_isXfbActive = false;

	sint32 activeTextureUnit = 0;
	void* m_latteBoundTextures[Latte::GPU_LIMITS::NUM_TEXTURES_PER_STAGE * 3]{};

	// attribute stream
	GLuint glAttributeCacheAB{};
	GLuint _boundArrayBuffer{};

	// streamout
	GLuint glStreamoutCacheRingBuffer;

	// cfbo
	GLuint prevBoundFBO = 0;
	GLuint glId_fbo = 0;

	// renderstate
	uint32 prevBlendControlReg[8] = { 0 };
	uint32 prevBlendMask = 0;
	uint32 prevLogicOp = 0;
	uint32 prevBlendColorConstant[4] = { 0 };
	uint8 prevAlphaTestEnable = 0;
	bool prevDepthEnable = 0;
	bool prevDepthWriteEnable = 0;
	Latte::LATTE_DB_DEPTH_CONTROL::E_ZFUNC prevDepthFunc = (Latte::LATTE_DB_DEPTH_CONTROL::E_ZFUNC)-1;
	bool prevStencilEnable = false;
	Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILFUNC prevFrontStencilFunc = (Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILFUNC)-1;
	Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILFUNC prevBackStencilFunc = (Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILFUNC)-1;
	Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION prevFrontStencilZPass = (Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION)-1;
	Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION prevFrontStencilZFail = (Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION)-1;
	Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION prevFrontStencilFail = (Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION)-1;
	Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION prevBackStencilZPass = (Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION)-1;
	Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION prevBackStencilZFail = (Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION)-1;
	Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION prevBackStencilFail = (Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION)-1;
	uint32 prevStencilRefFront = -1;
	uint32 prevStencilCompareMaskFront = -1;
	uint32 prevStencilWriteMaskFront = -1;
	uint32 prevStencilRefBack = -1;
	uint32 prevStencilCompareMaskBack = -1;
	uint32 prevStencilWriteMaskBack = -1;

	uint32 prevTargetColorMask = 0; // RGBA color mask for each render target (4 bit per target, starting from LSB)
	uint32 prevCullEnable = 0;
	uint32 prevCullFront = 0;
	uint32 prevCullBack = 0;
	Latte::LATTE_PA_SU_SC_MODE_CNTL::E_FRONTFACE prevCullFrontFace = Latte::LATTE_PA_SU_SC_MODE_CNTL::E_FRONTFACE::CCW;

	uint32 prevPolygonFrontScaleU32 = 0;
	uint32 prevPolygonFrontOffsetU32 = 0;
	uint32 prevPolygonClampU32 = 0;
	uint32 prevPolygonOffsetFrontEnabled = 0;
	uint32 prevPolygonOffsetBackEnabled = 0;

	uint32 prevZClipEnable = 0;

	uint32 prevPointSizeReg = 0xFFFFFFFF;

	uint32 prevPrimitiveRestartIndex = 0xFFFFFFFF;

	GLuint prevVertexShaderProgram = -1;
	GLuint prevGeometryShaderProgram = -1;
	GLuint prevPixelShaderProgram = -1;

	// occlusion queries
	std::vector<class LatteQueryObjectGL*> list_queryCacheOcclusion; // cache for unused queries

	// resource garbage collection	
	struct BufferCacheReleaseQueueEntry
	{
		BufferCacheReleaseQueueEntry(VirtualBufferHeap_t* heap, VirtualBufferHeapEntry_t* entry) : m_heap(heap), m_entry(entry) {};
		
		void free()
		{
			virtualBufferHeap_free(m_heap, m_entry);
		}

		VirtualBufferHeap_t* m_heap{};
		VirtualBufferHeapEntry_t* m_entry{};
	};

	struct
	{
		sint32 index;
		std::vector<BufferCacheReleaseQueueEntry> bufferCacheEntries;
	}m_destructionQueues;

};
