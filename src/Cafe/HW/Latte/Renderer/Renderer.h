#pragma once

#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteCachedFBO.h"
#include "Cafe/HW/Latte/Renderer/RendererShader.h"
#include "Cafe/HW/Latte/Core/LatteTextureLoader.h"
#include "Cafe/HW/Latte/Core/LatteTextureReadbackInfo.h"
#include "Cafe/HW/Latte/Core/LatteQueryObject.h"
#include "Cafe/HW/Latte/Renderer/RendererOuputShader.h"

#if BOOST_OS_WINDOWS
#include "util/DXGIWrapper/DXGIWrapper.h"
#endif

// imgui forward declarations
struct ImFontAtlas;
struct ImGuiContext;

enum class GfxVendor
{
	Generic,

	AMD,
	Intel,
	Nvidia,
	Apple,
	Mesa,

	MAX
};

enum class RendererAPI
{
	OpenGL,
	Vulkan,

	MAX
};

using ImTextureID = void*;

class Renderer
{
public:
	enum class INDEX_TYPE
	{
		NONE,
		U16,
		U32
	};

	virtual ~Renderer() = default;

	virtual RendererAPI GetType() = 0;

	virtual void Initialize();
	virtual void Shutdown();
	virtual bool IsPadWindowActive() = 0;

	virtual bool GetVRAMInfo(int& usageInMB, int& totalInMB) const;

	virtual void EnableDebugMode() {}

	virtual void ClearColorbuffer(bool padView) = 0;
	virtual void DrawEmptyFrame(bool mainWindow) = 0;
	virtual void SwapBuffers(bool swapTV, bool swapDRC) = 0;

	virtual void HandleScreenshotRequest(LatteTextureView* texView, bool padView){}
	
	virtual void DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter, 
												sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight, 
												bool padView, bool clearBackground) = 0;
	virtual bool BeginFrame(bool mainWindow) = 0;

	// flush control
	virtual void Flush(bool waitIdle = false) = 0; // called when explicit flush is required (e.g. by imgui)
	virtual void NotifyLatteCommandProcessorIdle() = 0; // called when command processor has no more commands available or when stalled

	// imgui
	virtual bool ImguiBegin(bool mainWindow);
	virtual void ImguiEnd() = 0;
	virtual ImTextureID GenerateTexture(const std::vector<uint8>& data, const Vector2i& size) = 0;
	virtual void DeleteTexture(ImTextureID id) = 0;
	virtual void DeleteFontTextures() = 0;

	GfxVendor GetVendor() const { return m_vendor; }
	virtual void AppendOverlayDebugInfo() = 0;

	// rendertarget
	virtual void renderTarget_setViewport(float x, float y, float width, float height, float nearZ, float farZ, bool halfZ = false) = 0;
	virtual void renderTarget_setScissor(sint32 scissorX, sint32 scissorY, sint32 scissorWidth, sint32 scissorHeight) = 0;

	virtual LatteCachedFBO* rendertarget_createCachedFBO(uint64 key) = 0;
	virtual void rendertarget_deleteCachedFBO(LatteCachedFBO* fbo) = 0;
	virtual void rendertarget_bindFramebufferObject(LatteCachedFBO* cfbo) = 0;

	// texture functions
	virtual void* texture_acquireTextureUploadBuffer(uint32 size) = 0;
	virtual void texture_releaseTextureUploadBuffer(uint8* mem) = 0;

	virtual TextureDecoder* texture_chooseDecodedFormat(Latte::E_GX2SURFFMT format, bool isDepth, Latte::E_DIM dim, uint32 width, uint32 height) = 0;

	virtual void texture_clearSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex) = 0;
	virtual void texture_loadSlice(LatteTexture* hostTexture, sint32 width, sint32 height, sint32 depth, void* pixelData, sint32 sliceIndex, sint32 mipIndex, uint32 compressedImageSize) = 0;
	virtual void texture_clearColorSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex, float r, float g, float b, float a) = 0;
	virtual void texture_clearDepthSlice(LatteTexture* hostTexture, uint32 sliceIndex, sint32 mipIndex, bool clearDepth, bool clearStencil, float depthValue, uint32 stencilValue) = 0;

	virtual LatteTexture* texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth) = 0;

	virtual void texture_setLatteTexture(LatteTextureView* textureView, uint32 textureUnit) = 0;
	virtual void texture_copyImageSubData(LatteTexture* src, sint32 srcMip, sint32 effectiveSrcX, sint32 effectiveSrcY, sint32 srcSlice, LatteTexture* dst, sint32 dstMip, sint32 effectiveDstX, sint32 effectiveDstY, sint32 dstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight, sint32 srcDepth) = 0;

	virtual LatteTextureReadbackInfo* texture_createReadback(LatteTextureView* textureView) = 0;

	// surface copy
	virtual void surfaceCopy_copySurfaceWithFormatConversion(LatteTexture* sourceTexture, sint32 srcMip, sint32 srcSlice, LatteTexture* destinationTexture, sint32 dstMip, sint32 dstSlice, sint32 width, sint32 height) = 0;

	// buffer cache
	virtual void bufferCache_init(const sint32 bufferSize) = 0;
	virtual void bufferCache_upload(uint8* buffer, sint32 size, uint32 bufferOffset) = 0;
	virtual void bufferCache_copy(uint32 srcOffset, uint32 dstOffset, uint32 size) = 0;
	virtual void bufferCache_copyStreamoutToMainBuffer(uint32 srcOffset, uint32 dstOffset, uint32 size) = 0;

	virtual void buffer_bindVertexBuffer(uint32 bufferIndex, uint32 offset, uint32 size) = 0;
	virtual void buffer_bindUniformBuffer(LatteConst::ShaderType shaderType, uint32 bufferIndex, uint32 offset, uint32 size) = 0;

	// shader
	virtual RendererShader* shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool compileAsync, bool isGfxPackSource) = 0;

	// streamout
	virtual void streamout_setupXfbBuffer(uint32 bufferIndex, sint32 ringBufferOffset, uint32 rangeAddr, uint32 rangeSize) = 0;
	virtual void streamout_begin() = 0;
	virtual void streamout_rendererFinishDrawcall() = 0;

	// core drawing logic
	virtual void draw_beginSequence() = 0;
	virtual void draw_execute(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, bool isFirst) = 0;
	virtual void draw_endSequence() = 0;

	// index
	virtual void* indexData_reserveIndexMemory(uint32 size, uint32& offset, uint32& bufferIndex) = 0;
	virtual void indexData_uploadIndexMemory(uint32 offset, uint32 size) = 0;

	// occlusion queries
	virtual LatteQueryObject* occlusionQuery_create() = 0;
	virtual void occlusionQuery_destroy(LatteQueryObject* queryObj) = 0;
	virtual void occlusionQuery_flush() = 0;
	virtual void occlusionQuery_updateState() = 0;

protected:
	virtual void GetVendorInformation() { }
	GfxVendor m_vendor = GfxVendor::Generic;

	static uint8 SRGBComponentToRGB(uint8 ci);
	static uint8 RGBComponentToSRGB(uint8 cli);

	enum class ScreenshotState
	{
		None,
		Main,
		Pad,
	};
	ScreenshotState m_screenshot_state = ScreenshotState::None;
	void SaveScreenshot(const std::vector<uint8>& rgb_data, int width, int height, bool mainWindow) const;


	ImFontAtlas* imguiFontAtlas{};
	ImGuiContext* imguiTVContext{};
	ImGuiContext* imguiPadContext{};

#if BOOST_OS_WINDOWS
	std::unique_ptr<DXGIWrapper> m_dxgi_wrapper{};
#endif
};

extern std::unique_ptr<Renderer> g_renderer;
