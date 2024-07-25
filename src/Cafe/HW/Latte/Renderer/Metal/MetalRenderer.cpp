#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"

void MetalRenderer::InitializeLayer(const Vector2i& size, bool mainWindow) {
    /*
    const auto& windowInfo = gui_getWindowInfo().window_main;

    NSView* view = (NS::View*)handle;

	MetalView* childView = [[MetalView alloc] initWithFrame:view.bounds];
	childView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
	childView.wantsLayer = YES;

	[view addSubview:childView];

	VkMetalSurfaceCreateInfoEXT surface;
	surface.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
	surface.pNext = NULL;
	surface.flags = 0;
	surface.pLayer = (CAMetalLayer*)childView.layer;
	*/
}

void MetalRenderer::Initialize() {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::Shutdown() {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

bool MetalRenderer::IsPadWindowActive() {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

bool MetalRenderer::GetVRAMInfo(int& usageInMB, int& totalInMB) const {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::ClearColorbuffer(bool padView) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::DrawEmptyFrame(bool mainWindow) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::SwapBuffers(bool swapTV, bool swapDRC) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter,
								sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight,
								bool padView, bool clearBackground) {
	cemuLog_logDebug(LogType::Force, "not implemented");
}
bool MetalRenderer::BeginFrame(bool mainWindow) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::Flush(bool waitIdle) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::NotifyLatteCommandProcessorIdle() {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::AppendOverlayDebugInfo() {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::renderTarget_setViewport(float x, float y, float width, float height, float nearZ, float farZ, bool halfZ) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::renderTarget_setScissor(sint32 scissorX, sint32 scissorY, sint32 scissorWidth, sint32 scissorHeight) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

LatteCachedFBO* MetalRenderer::rendertarget_createCachedFBO(uint64 key) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::rendertarget_deleteCachedFBO(LatteCachedFBO* fbo) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::rendertarget_bindFramebufferObject(LatteCachedFBO* cfbo) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void* MetalRenderer::texture_acquireTextureUploadBuffer(uint32 size) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::texture_releaseTextureUploadBuffer(uint8* mem) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

TextureDecoder* MetalRenderer::texture_chooseDecodedFormat(Latte::E_GX2SURFFMT format, bool isDepth, Latte::E_DIM dim, uint32 width, uint32 height) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::texture_clearSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::texture_loadSlice(LatteTexture* hostTexture, sint32 width, sint32 height, sint32 depth, void* pixelData, sint32 sliceIndex, sint32 mipIndex, uint32 compressedImageSize) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::texture_clearColorSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex, float r, float g, float b, float a) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::texture_clearDepthSlice(LatteTexture* hostTexture, uint32 sliceIndex, sint32 mipIndex, bool clearDepth, bool clearStencil, float depthValue, uint32 stencilValue) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

LatteTexture* MetalRenderer::texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::texture_setLatteTexture(LatteTextureView* textureView, uint32 textureUnit) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::texture_copyImageSubData(LatteTexture* src, sint32 srcMip, sint32 effectiveSrcX, sint32 effectiveSrcY, sint32 srcSlice, LatteTexture* dst, sint32 dstMip, sint32 effectiveDstX, sint32 effectiveDstY, sint32 dstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight, sint32 srcDepth) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

LatteTextureReadbackInfo* MetalRenderer::texture_createReadback(LatteTextureView* textureView) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::surfaceCopy_copySurfaceWithFormatConversion(LatteTexture* sourceTexture, sint32 srcMip, sint32 srcSlice, LatteTexture* destinationTexture, sint32 dstMip, sint32 dstSlice, sint32 width, sint32 height) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::bufferCache_init(const sint32 bufferSize) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::bufferCache_upload(uint8* buffer, sint32 size, uint32 bufferOffset) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::bufferCache_copy(uint32 srcOffset, uint32 dstOffset, uint32 size) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::bufferCache_copyStreamoutToMainBuffer(uint32 srcOffset, uint32 dstOffset, uint32 size) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::buffer_bindVertexBuffer(uint32 bufferIndex, uint32 offset, uint32 size) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::buffer_bindUniformBuffer(LatteConst::ShaderType shaderType, uint32 bufferIndex, uint32 offset, uint32 size) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

RendererShader* MetalRenderer::shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool compileAsync, bool isGfxPackSource) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::streamout_setupXfbBuffer(uint32 bufferIndex, sint32 ringBufferOffset, uint32 rangeAddr, uint32 rangeSize) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::streamout_begin() {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::streamout_rendererFinishDrawcall() {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::draw_beginSequence() {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::draw_execute(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, bool isFirst) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::draw_endSequence() {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void* MetalRenderer::indexData_reserveIndexMemory(uint32 size, uint32& offset, uint32& bufferIndex) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}

void MetalRenderer::indexData_uploadIndexMemory(uint32 offset, uint32 size) {
    cemuLog_logDebug(LogType::Force, "not implemented");
}
