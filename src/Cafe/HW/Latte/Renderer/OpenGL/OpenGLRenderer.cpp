#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"
#include "gui/guiWrapper.h"

#include "Cafe/HW/Latte/Core/LatteRingBuffer.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"

#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureViewGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/RendererShaderGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/CachedFBOGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLTextureReadback.h"

#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"

#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_extension.h"

#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/OS/libs/gx2/GX2.h"

#include "gui/canvas/OpenGLCanvas.h"

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

namespace CemuGL
{
#define GLFUNC(__type, __name)	__type __name;
#define EGLFUNC(__type, __name)	__type __name;
#include "Common/GLInclude/glFunctions.h"
#undef GLFUNC
#undef EGLFUNC
}

#include "config/ActiveSettings.h"
#include "config/LaunchSettings.h"

static const int TEXBUFFER_SIZE = 1024 * 1024 * 32; // 32MB

struct
{
	// options
	bool useTextureUploadBuffer;
	// texture upload
	GLuint uploadBuffer;
	sint32 uploadIndex;
	void* uploadBufferPtr;
	LatteRingBuffer_t* uploadRingBuffer;
	// current texture work buffer (subrange of uploadRingBuffer)
	uint8* texWorkBuffer;
	sint32 texWorkBufferSize;
	// texture upload buffer (when not using persistent buffer)
	std::vector<uint8> texUploadBuffer;
	// FBO for fast clearing (on Nvidia or if glClearTexSubImage is not supported)
	GLuint clearFBO;
}glRendererState;

static const GLenum glDepthFuncTable[] =
{
	GL_NEVER,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS
};

static const GLenum glAlphaTestFunc[] =
{
	GL_NEVER,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS
};

OpenGLRenderer::OpenGLRenderer()
{
	glRendererState.useTextureUploadBuffer = false;
	if (glRendererState.useTextureUploadBuffer)
	{
		glCreateBuffers(1, &glRendererState.uploadBuffer);
		glNamedBufferStorage(glRendererState.uploadBuffer, TEXBUFFER_SIZE, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
		void* buffer = glMapNamedBufferRange(glRendererState.uploadBuffer, 0, TEXBUFFER_SIZE, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
		if (buffer == nullptr)
		{
			cemuLog_log(LogType::Force, "Failed to allocate GL texture upload buffer. Using traditional API instead");
			cemu_assert_debug(false);
		}
		glRendererState.uploadBufferPtr = buffer;
		glRendererState.uploadRingBuffer = LatteRingBuffer_create((uint8*)buffer, TEXBUFFER_SIZE);
		glRendererState.uploadIndex = 0;
	}

#if BOOST_OS_WINDOWS
	try
	{
		m_dxgi_wrapper = std::make_unique<DXGIWrapper>();
	}
	catch (const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "Unable to create dxgi wrapper: {} (VRAM overlay stat won't be available)", ex.what());
	}
#endif
}

OpenGLRenderer::~OpenGLRenderer()
{
	if(m_pipeline != 0)
		glDeleteProgramPipelines(1, &m_pipeline);
}

OpenGLRenderer* OpenGLRenderer::GetInstance()
{
	cemu_assert_debug(g_renderer && dynamic_cast<OpenGLRenderer*>(g_renderer.get()));
	return (OpenGLRenderer*)g_renderer.get();
}

bool OpenGLRenderer::ImguiBegin(bool mainWindow)
{
	if (!mainWindow)
	{
		GLCanvas_MakeCurrent(true);
		m_isPadViewContext = true;
	}

	if(!Renderer::ImguiBegin(mainWindow))
		return false;
	
	renderstate_resetColorControl();
	renderstate_resetDepthControl();
	renderstate_resetStencilMask();

	if (glClipControl)
		glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_UpdateWindowInformation(mainWindow);
	ImGui::NewFrame();
	return true;
}

void OpenGLRenderer::ImguiEnd()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (m_isPadViewContext)
	{
		GLCanvas_MakeCurrent(false);
		m_isPadViewContext = false;
	}

	if (glClipControl)
		glClipControl(GL_UPPER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
}

ImTextureID OpenGLRenderer::GenerateTexture(const std::vector<uint8>& data, const Vector2i& size)
{
	GLuint textureId;
	glGenTextures(1, &textureId);

	glBindTexture(GL_TEXTURE_2D, textureId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glActiveTexture(GL_TEXTURE0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, size.x, size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
	return (ImTextureID)(uintptr_t)textureId;
}

void OpenGLRenderer::DeleteTexture(ImTextureID id)
{
	if (id)
	{
		GLuint textureId = (GLuint)(uintptr_t)id;
		glDeleteTextures(1, &textureId);
	}
}

void OpenGLRenderer::DeleteFontTextures()
{
	ImGui_ImplOpenGL3_DestroyFontsTexture();
}

typedef void(*GL_IMPORT)();

#if BOOST_OS_WINDOWS
GL_IMPORT _GetOpenGLFunction(HMODULE hLib, const char* name)
{
	GL_IMPORT r = (GL_IMPORT)wglGetProcAddress(name);
	if (r == nullptr)
		r = (GL_IMPORT)GetProcAddress(hLib, name);
	return r;
}

void LoadOpenGLImports()
{
	HMODULE hLib = LoadLibraryA("opengl32.dll");
#define GLFUNC(__type, __name)	__name = (__type)_GetOpenGLFunction(hLib, STRINGIFY(__name));
#include "Common/GLInclude/glFunctions.h"
#undef GLFUNC
}
#elif BOOST_OS_LINUX
GL_IMPORT _GetOpenGLFunction(void* hLib, PFNGLXGETPROCADDRESSPROC func, const char* name)
{
	GL_IMPORT r = (GL_IMPORT)func((const GLubyte*)name);
	return r;
}

#include <dlfcn.h>
// #define RTLD_NOW        	0x00002
// #define RTLD_GLOBAL        	0x00100

void LoadOpenGLImports()
{
	PFNGLXGETPROCADDRESSPROC _glXGetProcAddress = nullptr;
	void* libGL = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
	_glXGetProcAddress = (PFNGLXGETPROCADDRESSPROC)dlsym(libGL, "glXGetProcAddressARB");
	if(!_glXGetProcAddress)
	{
		libGL = dlopen("libGL.so", RTLD_NOW | RTLD_GLOBAL);
		_glXGetProcAddress = (PFNGLXGETPROCADDRESSPROC)dlsym(libGL, "glXGetProcAddressARB");
	}

	void* libEGL = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL);
	if(!libEGL)
	{
		libGL = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
	}

#define GLFUNC(__type, __name)	__name = (__type)_GetOpenGLFunction(libGL, _glXGetProcAddress, STRINGIFY(__name));
#define EGLFUNC(__type, __name)	__name = (__type)dlsym(libEGL, STRINGIFY(__name));
#include "Common/GLInclude/glFunctions.h"
#undef GLFUNC
#undef EGLFUNC
}

#if BOOST_OS_LINUX
// dummy function for all code that is statically linked with cemu and attempts to use eglSwapInterval
// used to suppress wxWidgets calls to eglSwapInterval
extern "C"
EGLAPI EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
	return EGL_TRUE;
}
#endif

#elif BOOST_OS_MACOS
void LoadOpenGLImports()
{
	cemu_assert_unimplemented();
}
#endif

void OpenGLRenderer::Initialize()
{
	Renderer::Initialize();
	auto lock = cemuLog_acquire();
	cemuLog_log(LogType::Force, "------- Init OpenGL graphics backend -------");

	GLCanvas_MakeCurrent(false);
	LoadOpenGLImports();
	GetVendorInformation();	

#if BOOST_OS_WINDOWS
	if (wglSwapIntervalEXT)
		wglSwapIntervalEXT(0); // disable V-Sync per default
#endif

	if (glMaxShaderCompilerThreadsARB)
		glMaxShaderCompilerThreadsARB(0xFFFFFFFF);

	cemuLog_log(LogType::Force, "OpenGL extensions:");
	cemuLog_log(LogType::Force, "ARB_clip_control: {}", glClipControl ? "available" : "not supported");
	cemuLog_log(LogType::Force, "ARB_get_program_binary: {}", (glGetProgramBinary != NULL && glProgramBinary != NULL) ? "available" : "not supported");
	cemuLog_log(LogType::Force, "ARB_clear_texture: {}", (glClearTexImage != NULL) ? "available" : "not supported");
	cemuLog_log(LogType::Force, "ARB_copy_image: {}", (glCopyImageSubData != NULL) ? "available" : "not supported");
	cemuLog_log(LogType::Force, "NV_depth_buffer_float: {}", (glDepthRangedNV != NULL) ? "available" : "not supported");

	// enable framebuffer SRGB support
	glEnable(GL_FRAMEBUFFER_SRGB);

	if (this->m_vendor != GfxVendor::AMD)
	{
		// don't enable this for AMD because GL_PROGRAM_POINT_SIZE breaks shader binaries for some reason
		// it also seems like AMD ignores GL_POINT_SPRITE and gl_PointCoord is always 0.0/0.0

		// point size is always defined via shader
		glEnable(GL_PROGRAM_POINT_SIZE);

		// since we are using compatibility profile point sprites are disabled by default (e.g. gl_PointCoord wont work)
		glEnable(GL_POINT_SPRITE);
	}

	// check if clip control is available
	if (glClipControl)
	{
		glClipControl(GL_UPPER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
	}
	else
	{
		cemuLog_log(LogType::Force, "ARB_CLIP_CONTROL not supported by graphics driver. This will lead to crashes or graphical artifacts.");
	}

	// set pixel unpack alignment to 1 byte
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// on NVIDIA rendering to SNORM textures is clamped to [0.0,1.0] range for non-core profiles
	// we can still disable the clamping using an older function (note that AMD and Intel don't need this, which is technically incorrect according to the compatibility profile spec)
	if (m_vendor == GfxVendor::Nvidia)
		glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);


	glEnable(GL_PRIMITIVE_RESTART);
	glPrimitiveRestartIndex(0xFFFFFFFF);

	glGenProgramPipelines(1, &m_pipeline);
	glBindProgramPipeline(m_pipeline);

	lock.unlock();

	// create framebuffer for fast clearing (avoid glClearTexSubImage on Nvidia)
	if (glCreateFramebuffers)
		glCreateFramebuffers(1, &glRendererState.clearFBO);
	else
	{
		glGenFramebuffers(1, &glRendererState.clearFBO);
		// bind to initialize
		glBindFramebuffer(GL_FRAMEBUFFER_EXT, glRendererState.clearFBO);
		glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
	}

	draw_init();

	catchOpenGLError();

	glGenBuffers(1, &glStreamoutCacheRingBuffer);
	glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, glStreamoutCacheRingBuffer);
	glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, LatteStreamout_GetRingBufferSize(), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

	catchOpenGLError();

	// imgui
	ImGui_ImplOpenGL3_Init("#version 130");
}

bool OpenGLRenderer::IsPadWindowActive()
{
	return GLCanvas_HasPadViewOpen();
}

void OpenGLRenderer::Flush(bool waitIdle)
{
	glFlush();
	if (waitIdle)
		glFinish();
}

void OpenGLRenderer::NotifyLatteCommandProcessorIdle()
{
	glFlush();
}

void OpenGLRenderer::GetVendorInformation()
{
	// example vendor strings:
	// ATI Technologies Inc.
	// NVIDIA Corporation 
	// Intel
	char* glVendorString = (char*)glGetString(GL_VENDOR);
	char* glRendererString = (char*)glGetString(GL_RENDERER);
	char* glVersionString = (char*)glGetString(GL_VERSION);

	cemuLog_log(LogType::Force, "GL_VENDOR: {}", glVendorString ? glVendorString : "unknown");
	cemuLog_log(LogType::Force, "GL_RENDERER: {}", glRendererString ? glRendererString : "unknown");
	cemuLog_log(LogType::Force, "GL_VERSION: {}", glVersionString ? glVersionString : "unknown");

	if(glVersionString && boost::icontains(glVersionString, "Mesa"))
	{
		m_vendor = GfxVendor::Mesa;
		return;
	}

	if (glVendorString)
	{
		if ((toupper(glVendorString[0]) == 'A' && toupper(glVendorString[1]) == 'T' && toupper(glVendorString[2]) == 'I') ||
			(toupper(glVendorString[0]) == 'A' && toupper(glVendorString[1]) == 'M' && toupper(glVendorString[2]) == 'D'))
		{
			m_vendor = GfxVendor::AMD;
			return;
		}
		else if (memcmp(glVendorString, "NVIDIA", 6) == 0)
		{
			m_vendor = GfxVendor::Nvidia;
			return;
		}
		else if (memcmp(glVendorString, "Intel", 5) == 0)
		{
			m_vendor = GfxVendor::Intel;
			return;
		}
	}

	m_vendor = GfxVendor::Generic;
}

void _glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
	if (LatteGPUState.glVendor == GLVENDOR_NVIDIA && strstr(message, "Buffer"))
		return;
	if (LatteGPUState.glVendor == GLVENDOR_NVIDIA && strstr(message, "performance warning"))
		return;
	if (LatteGPUState.glVendor == GLVENDOR_NVIDIA && strstr(message, "Dithering is enabled"))
		return;
	if (LatteGPUState.glVendor == GLVENDOR_NVIDIA && strstr(message, "Blending is enabled, but is not supported for integer framebuffers"))
		return;
	if (LatteGPUState.glVendor == GLVENDOR_NVIDIA && strstr(message, "does not have a defined base level"))
		return;
	if(LatteGPUState.glVendor == GLVENDOR_NVIDIA && strstr(message, "has depth comparisons disabled, with a texture object"))
		return;

	cemuLog_log(LogType::Force, "GLDEBUG: {}", message);

	cemu_assert_debug(false);
}

void OpenGLRenderer::EnableDebugMode()
{
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(_glDebugCallback, NULL);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true);
}

void OpenGLRenderer::SwapBuffers(bool swapTV, bool swapDRC)
{
	GLCanvas_SwapBuffers(swapTV, swapDRC);

	if (swapTV)
		cleanupAfterFrame();
}

bool OpenGLRenderer::BeginFrame(bool mainWindow)
{
	if (!mainWindow && !IsPadWindowActive())
		return false;

	GLCanvas_MakeCurrent(!mainWindow);

	ClearColorbuffer(!mainWindow);
	return true;
}

void OpenGLRenderer::DrawEmptyFrame(bool mainWindow)
{
	if (!BeginFrame(mainWindow))
		return;
	
	SwapBuffers(mainWindow, !mainWindow);
}

void OpenGLRenderer::ClearColorbuffer(bool padView)
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}


void OpenGLRenderer::HandleScreenshotRequest(LatteTextureView* texView, bool padView)
{
	const bool hasScreenshotRequest = gui_hasScreenshotRequest();
	if(!hasScreenshotRequest && m_screenshot_state == ScreenshotState::None)
		return;

	if (IsPadWindowActive())
	{
		// we already took a pad view screenshow and want a main window screenshot
		if (m_screenshot_state == ScreenshotState::Main && padView)
			return;

		if (m_screenshot_state == ScreenshotState::Pad && !padView)
			return;

		// remember which screenshot is left to take
		if (m_screenshot_state == ScreenshotState::None)
			m_screenshot_state = padView ? ScreenshotState::Main : ScreenshotState::Pad;
		else
			m_screenshot_state = ScreenshotState::None;
	}
	else
		m_screenshot_state = ScreenshotState::None;

	int screenshotWidth, screenshotHeight;
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	texture_bindAndActivate(texView, 0);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &screenshotWidth);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &screenshotHeight);
	glPixelStorei(GL_PACK_ALIGNMENT, 1); // set alignment to 1

	const sint32 pixelDataSize = screenshotWidth * screenshotHeight * 3;
	std::vector<uint8> rgb_data(pixelDataSize);

	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_data.data());
	texture_bindAndActivate(nullptr, 0);
	
	const bool srcUsesSRGB = HAS_FLAG(texView->format, Latte::E_GX2SURFFMT::FMT_BIT_SRGB);
	const bool dstUsesSRGB = (!padView && LatteGPUState.tvBufferUsesSRGB) || (padView && LatteGPUState.drcBufferUsesSRGB);
	if((srcUsesSRGB && !dstUsesSRGB) || (!srcUsesSRGB && dstUsesSRGB))
	{
		for (sint32 iy = 0; iy < screenshotHeight; ++iy)
		{
			for (sint32 ix = 0; ix < screenshotWidth; ++ix)
			{
				uint8* pData = rgb_data.data() + (ix + iy * screenshotWidth) * 3;
				if (srcUsesSRGB && !dstUsesSRGB)
				{
					// SRGB -> RGB
					pData[0] = SRGBComponentToRGB(pData[0]);
					pData[1] = SRGBComponentToRGB(pData[1]);
					pData[2] = SRGBComponentToRGB(pData[2]);
				}
				else if (!srcUsesSRGB && dstUsesSRGB)
				{
					// RGB -> SRGB
					pData[0] = RGBComponentToSRGB(pData[0]);
					pData[1] = RGBComponentToSRGB(pData[1]);
					pData[2] = RGBComponentToSRGB(pData[2]);
				}
			}
		}
	}

	SaveScreenshot(rgb_data, screenshotWidth, screenshotHeight, !padView);
}

void OpenGLRenderer::DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter, sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight, bool padView, bool clearBackground)
{
	if (padView && !IsPadWindowActive())
		return;

	catchOpenGLError();
	GLCanvas_MakeCurrent(padView);

	renderstate_resetColorControl();
	renderstate_resetDepthControl();
	attributeStream_reset();

	// bind back buffer
	rendertarget_bindFramebufferObject(nullptr);

	if (clearBackground)
	{
		int windowWidth, windowHeight;
		if (padView)
			gui_getPadWindowPhysSize(windowWidth, windowHeight);
		else
			gui_getWindowPhysSize(windowWidth, windowHeight);
		g_renderer->renderTarget_setViewport(0, 0, windowWidth, windowHeight, 0.0f, 1.0f);
		g_renderer->ClearColorbuffer(padView);
	}

	shader_unbind(RendererShader::ShaderType::kGeometry);
	shader_bind(shader->GetVertexShader());
	shader_bind(shader->GetFragmentShader());
	shader->SetUniformParameters(*texView, {imageWidth, imageHeight});

	// set viewport
	glViewportIndexedf(0, imageX, imageY, imageWidth, imageHeight);

	LatteTextureViewGL* texViewGL = (LatteTextureViewGL*)texView;
	texture_bindAndActivate(texView, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	texViewGL->samplerState.clampS = texViewGL->samplerState.clampT = 0xFF;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, useLinearTexFilter ? GL_LINEAR : GL_NEAREST);
	texViewGL->samplerState.filterMin = 0xFFFFFFFF;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, useLinearTexFilter ? GL_LINEAR : GL_NEAREST);
	texViewGL->samplerState.filterMag = 0xFFFFFFFF;

	if ((!padView && !LatteGPUState.tvBufferUsesSRGB) || (padView && !LatteGPUState.drcBufferUsesSRGB))
		glDisable(GL_FRAMEBUFFER_SRGB);

	uint16 indexData[6] = { 0,1,2,3,4,5 };
	glDrawRangeElements(GL_TRIANGLES, 0, 5, 6, GL_UNSIGNED_SHORT, indexData);

	if ((!padView && !LatteGPUState.tvBufferUsesSRGB) || (padView && !LatteGPUState.drcBufferUsesSRGB))
		glEnable(GL_FRAMEBUFFER_SRGB);

	// unbind texture
	texture_bindAndActivate(nullptr, 0);

	catchOpenGLError();

	// restore viewport
	glViewportIndexedf(0, prevViewportX, prevViewportY, prevViewportWidth, prevViewportHeight);

	// switch back to TV context
	if (padView)
		GLCanvas_MakeCurrent(false);
}

void OpenGLRenderer::renderTarget_setViewport(float x, float y, float width, float height, float nearZ, float farZ, bool halfZ /*= false*/)
{
	if (prevNearZ != nearZ || prevFarZ != farZ || _prevHalfZ != halfZ)
	{
		if (*(uint32*)&farZ == 0x3f7fffff)
			*(uint32*)&farZ = 0x3f800000;
		if (glDepthRangedNV)
			glDepthRangedNV(nearZ, farZ);
		else
			glDepthRange(nearZ, farZ);
		prevNearZ = nearZ;
		prevFarZ = farZ;
	}

	bool invertY = false;
	if (height < 0.0)
	{
		invertY = true;
		y += height;
		height = -height;
	}

	if (glClipControl && (_prevInvertY != invertY || _prevHalfZ != halfZ))
	{
		GLenum clipDepth = halfZ ? GL_ZERO_TO_ONE : GL_NEGATIVE_ONE_TO_ONE;
		if (invertY)
			glClipControl(GL_LOWER_LEFT, clipDepth); // OpenGL style
		else
			glClipControl(GL_UPPER_LEFT, clipDepth); // DX style (default for GX2)
		_prevInvertY = invertY;
		_prevHalfZ = halfZ;
	}

	if (prevViewportX == x && prevViewportY == y && prevViewportWidth == width && prevViewportHeight == height)
		return; // viewport did not change
	glViewportIndexedf(0, x, y, width, height);
	prevViewportX = x;
	prevViewportY = y;
	prevViewportWidth = width;
	prevViewportHeight = height;
}

void OpenGLRenderer::renderTarget_setScissor(sint32 scissorX, sint32 scissorY, sint32 scissorWidth, sint32 scissorHeight)
{
	if (prevScissorEnable != true)
	{
		// enable scissor box
		glEnable(GL_SCISSOR_TEST);
		prevScissorEnable = true;
	}
	glScissor(scissorX, scissorY, scissorWidth, scissorHeight);
}

LatteCachedFBO* OpenGLRenderer::rendertarget_createCachedFBO(uint64 key)
{
	return new CachedFBOGL(key);
}

void OpenGLRenderer::rendertarget_deleteCachedFBO(LatteCachedFBO* cfbo)
{
	auto cfboGL = (CachedFBOGL*)cfbo;
	if (prevBoundFBO == cfboGL->glId_fbo)
	{
		glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
		prevBoundFBO = 0;
	}
	glDeleteFramebuffers(1, &cfboGL->glId_fbo);
}

// set active FBO
void OpenGLRenderer::rendertarget_bindFramebufferObject(LatteCachedFBO* cfbo)
{
	GLuint fboid;
	if (cfbo)
	{
		const auto cfboGL = (CachedFBOGL*)cfbo;
		fboid = cfboGL->glId_fbo;
	}
	else
		fboid = 0;

	if (prevBoundFBO != fboid)
	{
		glBindFramebuffer(GL_FRAMEBUFFER_EXT, fboid);
		prevBoundFBO = fboid;
	}
}

void OpenGLRenderer::renderstate_setChannelTargetMask(uint32 renderTargetMask)
{
	if (renderTargetMask != prevTargetColorMask)
	{
		for (sint32 i = 0; i < 8; i++)
		{
			uint32 targetRGBAMask = ((renderTargetMask >> (i * 4)) & 0xF);
			if (targetRGBAMask != ((prevTargetColorMask >> (i * 4)) & 0xF))
			{
				// update color mask
				glColorMaski(i, (targetRGBAMask & 1) ? GL_TRUE : GL_FALSE, (targetRGBAMask & 2) ? GL_TRUE : GL_FALSE, (targetRGBAMask & 4) ? GL_TRUE : GL_FALSE, (targetRGBAMask & 8) ? GL_TRUE : GL_FALSE);
			}
		}
		prevTargetColorMask = renderTargetMask;
	}
}

void OpenGLRenderer::renderstate_setAlwaysWriteDepth()
{
	if (prevDepthEnable == 0)
	{
		glEnable(GL_DEPTH_TEST);
		prevDepthEnable = 1;
	}
	glDepthFunc(GL_ALWAYS);
	prevDepthFunc = Latte::LATTE_DB_DEPTH_CONTROL::E_ZFUNC::ALWAYS;
}

static const GLuint table_glBlendSrcDst[] =
{
	/* 0x00 */ GL_ZERO,
	/* 0x01 */ GL_ONE,
	/* 0x02 */ GL_SRC_COLOR,
	/* 0x03 */ GL_ONE_MINUS_SRC_COLOR,
	/* 0x04 */ GL_SRC_ALPHA,
	/* 0x05 */ GL_ONE_MINUS_SRC_ALPHA,
	/* 0x06 */ GL_DST_ALPHA,
	/* 0x07 */ GL_ONE_MINUS_DST_ALPHA,
	/* 0x08 */ GL_DST_COLOR,
	/* 0x09 */ GL_ONE_MINUS_DST_COLOR,
	/* 0x0A */ GL_SRC_ALPHA_SATURATE,
	/* 0x0B */ 0xFFFFFFFF,
	/* 0x0C */ 0xFFFFFFFF,
	/* 0x0D */ GL_CONSTANT_COLOR,
	/* 0x0E */ GL_ONE_MINUS_CONSTANT_COLOR,
	/* 0x0F */ GL_SRC1_COLOR,
	/* 0x10 */ GL_ONE_MINUS_SRC1_COLOR,
	/* 0x11 */ GL_SRC1_ALPHA,
	/* 0x12 */ GL_ONE_MINUS_SRC1_ALPHA,
	/* 0x13 */ GL_CONSTANT_ALPHA,
	/* 0x14 */ GL_ONE_MINUS_CONSTANT_ALPHA
};

static GLuint GetGLBlendFactor(Latte::LATTE_CB_BLENDN_CONTROL::E_BLENDFACTOR blendFactor)
{
	uint32 blendFactorU = (uint32)blendFactor;
	if (blendFactorU >= 0xF && blendFactorU <= 0x12)
	{
		debug_printf("Unsupported dual-source blending used\n");
		cemu_assert_debug(false); // dual-source blending
		return GL_ZERO;
	}
	if (blendFactorU >= (sizeof(table_glBlendSrcDst) / sizeof(table_glBlendSrcDst[0])))
	{
		debug_printf("GetGLBlendFactor: Constant 0x%x out of range\n", blendFactor);
		return GL_ZERO;
	}
	if (table_glBlendSrcDst[blendFactorU] == -1)
	{
		debug_printf("GetGLBlendFactor: Constant 0x%x is invalid\n", blendFactor);
		cemu_assert_debug(false);
		return GL_ZERO;
	}
	return table_glBlendSrcDst[blendFactorU];
}

static const GLuint table_glBlendCombine[] =
{
	GL_FUNC_ADD,
	GL_FUNC_SUBTRACT,
	GL_MIN,
	GL_MAX,
	GL_FUNC_REVERSE_SUBTRACT
};

GLuint GetGLBlendCombineFunc(Latte::LATTE_CB_BLENDN_CONTROL::E_COMBINEFUNC combineFunc)
{
	uint32 combineFuncU = (uint32)combineFunc;
	if (combineFuncU >= (sizeof(table_glBlendCombine) / sizeof(table_glBlendCombine[0])))
	{
		cemu_assert_suspicious();
		return GL_FUNC_ADD;
	}
	return table_glBlendCombine[combineFuncU];
}


void* OpenGLRenderer::texture_acquireTextureUploadBuffer(uint32 size)
{
	if (glRendererState.useTextureUploadBuffer)
	{
		glRendererState.texWorkBuffer = LatteRingBuffer_allocate(glRendererState.uploadRingBuffer, size, 1024);
		glRendererState.texWorkBufferSize = size;
		return glRendererState.texWorkBuffer;
	}
	// static memory buffer
	if (glRendererState.texUploadBuffer.size() < size)
	{
		glRendererState.texUploadBuffer.resize(size);
	}
	return glRendererState.texUploadBuffer.data();
}

void OpenGLRenderer::texture_releaseTextureUploadBuffer(uint8* mem)
{
	// do nothing
}

TextureDecoder* OpenGLRenderer::texture_chooseDecodedFormat(Latte::E_GX2SURFFMT format, bool isDepth, Latte::E_DIM dim, uint32 width, uint32 height)
{
	TextureDecoder* texDecoder = nullptr;
	if (isDepth)
	{
		if (format == Latte::E_GX2SURFFMT::R32_FLOAT)
		{
			return TextureDecoder_R32_FLOAT::getInstance();
		}
		if (format == Latte::E_GX2SURFFMT::D24_S8_UNORM)
		{
			return TextureDecoder_D24_S8::getInstance();
		}
		else if (format == Latte::E_GX2SURFFMT::D24_S8_FLOAT)
		{
			return TextureDecoder_NullData64::getInstance();
		}
		else if (format == Latte::E_GX2SURFFMT::D32_S8_FLOAT)
		{
			return TextureDecoder_D32_S8_UINT_X24::getInstance();
		}
		else if (format == Latte::E_GX2SURFFMT::R32_FLOAT)
		{
			return TextureDecoder_R32_FLOAT::getInstance();
		}
		else if (format == Latte::E_GX2SURFFMT::R16_UNORM)
		{
			return TextureDecoder_R16_FLOAT::getInstance();
		}
		return nullptr;
	}
	if (format == Latte::E_GX2SURFFMT::R4_G4_UNORM)
		texDecoder = TextureDecoder_R4_G4_UNORM_To_RGBA4::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R4_G4_B4_A4_UNORM)
		texDecoder = TextureDecoder_R4_G4_B4_A4_UNORM::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT)
		texDecoder = TextureDecoder_R16_G16_B16_A16_FLOAT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R16_G16_FLOAT)
		texDecoder = TextureDecoder_R16_G16_FLOAT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R16_SNORM)
		texDecoder = TextureDecoder_R16_SNORM::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R16_FLOAT)
		texDecoder = TextureDecoder_R16_FLOAT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R32_FLOAT)
		texDecoder = TextureDecoder_R32_FLOAT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::BC1_UNORM)
		texDecoder = TextureDecoder_BC1::getInstance();
	else if (format == Latte::E_GX2SURFFMT::BC1_SRGB)
		texDecoder = TextureDecoder_BC1::getInstance();
	else if (format == Latte::E_GX2SURFFMT::BC2_UNORM)
		texDecoder = TextureDecoder_BC2_UNORM_uncompress::getInstance();
	else if (format == Latte::E_GX2SURFFMT::BC2_SRGB)
		texDecoder = TextureDecoder_BC2_SRGB_uncompress::getInstance();
	else if (format == Latte::E_GX2SURFFMT::BC3_UNORM)
		texDecoder = TextureDecoder_BC3::getInstance();
	else if (format == Latte::E_GX2SURFFMT::BC3_SRGB)
		texDecoder = TextureDecoder_BC3::getInstance();
	else if (format == Latte::E_GX2SURFFMT::BC4_UNORM)
	{
		if (dim != Latte::E_DIM::DIM_2D && dim != Latte::E_DIM::DIM_2D_ARRAY)
			texDecoder = TextureDecoder_BC4_UNORM_uncompress::getInstance();
		else
			texDecoder = TextureDecoder_BC4::getInstance();
	}
	else if (format == Latte::E_GX2SURFFMT::BC4_SNORM)
	{
		if (dim != Latte::E_DIM::DIM_2D && dim != Latte::E_DIM::DIM_2D_ARRAY)
			texDecoder = TextureDecoder_BC4::getInstance();
		else
			texDecoder = TextureDecoder_BC4_UNORM_uncompress::getInstance();
	}
	else if (format == Latte::E_GX2SURFFMT::BC5_UNORM)
		texDecoder = TextureDecoder_BC5::getInstance();
	else if (format == Latte::E_GX2SURFFMT::BC5_SNORM)
		texDecoder = TextureDecoder_BC5::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM)
		texDecoder = TextureDecoder_R8_G8_B8_A8::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_SNORM)
		texDecoder = TextureDecoder_R8_G8_B8_A8::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB)
		texDecoder = TextureDecoder_R8_G8_B8_A8::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R8_UNORM)
		texDecoder = TextureDecoder_R8::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R8_SNORM)
		texDecoder = TextureDecoder_R8::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R8_G8_UNORM)
		texDecoder = TextureDecoder_R8_G8::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R8_G8_SNORM)
		texDecoder = TextureDecoder_R8_G8::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R16_UNORM)
		texDecoder = TextureDecoder_R16_UNORM::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM)
		texDecoder = TextureDecoder_R16_G16_B16_A16::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_SNORM)
		texDecoder = TextureDecoder_R16_G16_B16_A16::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R16_G16_UNORM)
		texDecoder = TextureDecoder_R16_G16::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R5_G6_B5_UNORM)
		texDecoder = TextureDecoder_R5_G6_B5::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R5_G5_B5_A1_UNORM)
		texDecoder = TextureDecoder_R5_G5_B5_A1_UNORM_swappedOpenGL::getInstance();
	else if (format == Latte::E_GX2SURFFMT::A1_B5_G5_R5_UNORM)
		texDecoder = TextureDecoder_A1_B5_G5_R5_UNORM::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R32_G32_FLOAT)
		texDecoder = TextureDecoder_R32_G32_FLOAT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R32_G32_UINT)
		texDecoder = TextureDecoder_R32_G32_UINT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R32_UINT)
		texDecoder = TextureDecoder_R32_UINT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R16_UINT)
		texDecoder = TextureDecoder_R16_UINT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R8_UINT)
		texDecoder = TextureDecoder_R8_UINT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R32_G32_B32_A32_FLOAT)
		texDecoder = TextureDecoder_R32_G32_B32_A32_FLOAT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R10_G10_B10_A2_UNORM)
		texDecoder = TextureDecoder_R10_G10_B10_A2_UNORM::getInstance();
	else if (format == Latte::E_GX2SURFFMT::A2_B10_G10_R10_UNORM)
		texDecoder = TextureDecoder_A2_B10_G10_R10_UNORM_To_RGBA16::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R10_G10_B10_A2_SNORM)
		texDecoder = TextureDecoder_R10_G10_B10_A2_SNORM_To_RGBA16::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R10_G10_B10_A2_SRGB)
		texDecoder = TextureDecoder_R10_G10_B10_A2_UNORM::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT)
		texDecoder = TextureDecoder_R11_G11_B10_FLOAT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R32_G32_B32_A32_UINT)
		texDecoder = TextureDecoder_R32_G32_B32_A32_UINT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_UINT)
		texDecoder = TextureDecoder_R16_G16_B16_A16_UINT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UINT)
		texDecoder = TextureDecoder_R8_G8_B8_A8_UINT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::R24_X8_UNORM)
		texDecoder = TextureDecoder_R24_X8::getInstance();
	else if (format == Latte::E_GX2SURFFMT::X24_G8_UINT)
		texDecoder = TextureDecoder_X24_G8_UINT::getInstance();
	else if (format == Latte::E_GX2SURFFMT::D32_S8_FLOAT)
		texDecoder = TextureDecoder_D32_S8_UINT_X24::getInstance();
	else
		cemu_assert_debug(false);

	cemu_assert_debug(!isDepth);

	return texDecoder;
}

// use standard API to upload texture data
void OpenGLRenderer_texture_loadSlice_normal(LatteTexture* hostTextureGeneric, sint32 width, sint32 height, sint32 depth, void* pixelData, sint32 sliceIndex, sint32 mipIndex, uint32 imageSize)
{
	auto hostTexture = (LatteTextureGL*)hostTextureGeneric;
	sint32 effectiveWidth = width;
	sint32 effectiveHeight = height;
	sint32 effectiveDepth = depth;
	cemu_assert_debug(hostTexture->overwriteInfo.hasResolutionOverwrite == false); // not supported in _loadSlice
	cemu_assert_debug(hostTexture->overwriteInfo.hasFormatOverwrite == false); // not supported in _loadSlice
	// get format info
	LatteTextureGL::FormatInfoGL glFormatInfo;
	LatteTextureGL::GetOpenGLFormatInfo(hostTexture->isDepth, hostTexture->overwriteInfo.hasFormatOverwrite ? (Latte::E_GX2SURFFMT)hostTexture->overwriteInfo.format : hostTexture->format, hostTexture->dim, &glFormatInfo);
	// upload slice
	catchOpenGLError();
	if (mipIndex >= hostTexture->maxPossibleMipLevels)
	{
		cemuLog_logDebug(LogType::Force, "2D texture mip level allocated out of range");
		return;
	}
	if (hostTexture->dim == Latte::E_DIM::DIM_2D || hostTexture->dim == Latte::E_DIM::DIM_2D_MSAA)
	{
		if (glFormatInfo.glIsCompressed)
			glCompressedTextureSubImage2DWrapper(hostTexture->glTexTarget, hostTexture->glId_texture, mipIndex, 0, 0, effectiveWidth, effectiveHeight, glFormatInfo.glInternalFormat, imageSize, pixelData);
		else
			glTextureSubImage2DWrapper(hostTexture->glTexTarget, hostTexture->glId_texture, mipIndex, 0, 0, effectiveWidth, effectiveHeight, glFormatInfo.glSuppliedFormat, glFormatInfo.glSuppliedFormatType, pixelData);
	}
	else if (hostTexture->dim == Latte::E_DIM::DIM_1D)
	{
		if (glFormatInfo.glIsCompressed)
			glCompressedTextureSubImage1DWrapper(hostTexture->glTexTarget, hostTexture->glId_texture, mipIndex, 0, width, glFormatInfo.glInternalFormat, imageSize, pixelData);
		else
			glTextureSubImage1DWrapper(hostTexture->glTexTarget, hostTexture->glId_texture, mipIndex, 0, width, glFormatInfo.glSuppliedFormat, glFormatInfo.glSuppliedFormatType, pixelData);
	}
	else if (hostTexture->dim == Latte::E_DIM::DIM_2D_ARRAY || hostTexture->dim == Latte::E_DIM::DIM_2D_ARRAY_MSAA ||
			 hostTexture->dim == Latte::E_DIM::DIM_3D ||
			 hostTexture->dim == Latte::E_DIM::DIM_CUBEMAP)
	{
		if (glFormatInfo.glIsCompressed)
			glCompressedTextureSubImage3DWrapper(hostTexture->glTexTarget, hostTexture->glId_texture, mipIndex, 0, 0, sliceIndex, effectiveWidth, effectiveHeight, 1, glFormatInfo.glInternalFormat, imageSize, pixelData);
		else
			glTextureSubImage3DWrapper(hostTexture->glTexTarget, hostTexture->glId_texture, mipIndex, 0, 0, sliceIndex, effectiveWidth, effectiveHeight, 1, glFormatInfo.glSuppliedFormat, glFormatInfo.glSuppliedFormatType, pixelData);
	}
	catchOpenGLError();
}

// use persistent buffers to upload data
void OpenGLRenderer_texture_loadSlice_viaBuffers(LatteTexture* hostTexture, sint32 width, sint32 height, sint32 depth, void* pixelData, sint32 sliceIndex, sint32 mipIndex, uint32 imageSize)
{
	// bind buffer
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, glRendererState.uploadBuffer);
	catchOpenGLError();
	// upload data to buffer
	cemu_assert(imageSize <= TEXBUFFER_SIZE);

	cemu_assert_debug(glRendererState.texWorkBufferSize == imageSize);
	glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, (GLintptr)(glRendererState.texWorkBuffer - (uint8*)glRendererState.uploadBufferPtr), imageSize);
	catchOpenGLError();
	OpenGLRenderer_texture_loadSlice_normal(hostTexture, width, height, depth, (void*)(glRendererState.texWorkBuffer - (uint8*)glRendererState.uploadBufferPtr), sliceIndex, mipIndex, imageSize);
	// unbind buffer and sync
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	catchOpenGLError();
}

void OpenGLRenderer::texture_loadSlice(LatteTexture* hostTexture, sint32 width, sint32 height, sint32 depth, void* pixelData, sint32 sliceIndex, sint32 mipIndex, uint32 imageSize)
{
	if (glRendererState.useTextureUploadBuffer)
		OpenGLRenderer_texture_loadSlice_viaBuffers(hostTexture, width, height, depth, pixelData, sliceIndex, mipIndex, imageSize);
	else
		OpenGLRenderer_texture_loadSlice_normal(hostTexture, width, height, depth, pixelData, sliceIndex, mipIndex, imageSize);
}

void OpenGLRenderer::texture_clearColorSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex, float r, float g, float b, float a)
{
	LatteTextureGL* texGL = (LatteTextureGL*)hostTexture;
	cemu_assert_debug(!texGL->isDepth);

	sint32 eWidth, eHeight;
	hostTexture->GetEffectiveSize(eWidth, eHeight, mipIndex);
	renderstate_resetColorControl();
	renderTarget_setViewport(0, 0, eWidth, eHeight, 0.0f, 1.0f);
	LatteMRT::BindColorBufferOnly(hostTexture->GetOrCreateView(mipIndex, 1, sliceIndex, 1));
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderer::texture_clearDepthSlice(LatteTexture* hostTexture, uint32 sliceIndex, sint32 mipIndex, bool clearDepth, bool clearStencil, float depthValue, uint32 stencilValue)
{
	LatteTextureGL* texGL = (LatteTextureGL*)hostTexture;
	cemu_assert_debug(texGL->isDepth);

	sint32 eWidth, eHeight;
	hostTexture->GetEffectiveSize(eWidth, eHeight, mipIndex);
	renderstate_resetColorControl();
	renderstate_resetDepthControl();
	renderTarget_setViewport(0, 0, eWidth, eHeight, 0.0f, 1.0f);
	LatteMRT::BindDepthBufferOnly(hostTexture->GetOrCreateView(mipIndex, 1, sliceIndex, 1));

	if (!hostTexture->hasStencil)
		clearStencil = false;

	if (clearDepth)
		glClearDepth(clearDepth);
	if (clearStencil)
	{
		renderstate_resetStencilMask();
		glClearStencil((GLint)stencilValue);
	}
	glClear((clearDepth ? GL_DEPTH_BUFFER_BIT : 0) | (clearStencil ? GL_STENCIL_BUFFER_BIT : 0));
	catchOpenGLError();
}


void OpenGLRenderer::texture_clearSlice(LatteTexture* hostTextureGeneric, sint32 sliceIndex, sint32 mipIndex)
{
	auto hostTexture = (LatteTextureGL*)hostTextureGeneric;
	// get OpenGL format info
	LatteTextureGL::FormatInfoGL formatInfoGL;
	LatteTextureGL::GetOpenGLFormatInfo(hostTexture->isDepth, hostTexture->format, hostTexture->dim, &formatInfoGL);
	// get effective size of mip
	sint32 effectiveWidth, effectiveHeight;
	hostTexture->GetEffectiveSize(effectiveWidth, effectiveHeight, mipIndex);

	// on Nvidia glClearTexImage and glClearTexSubImage has bad performance (clearing a 4K texture takes up to 50ms)
	// clearing with glTextureSubImage2D from a CPU RAM buffer is only slightly slower
	// clearing with glTextureSubImage2D from a OpenGL buffer is 10-20% faster than glClearTexImage
	// clearing with FBO and glClear is orders of magnitude faster than the other methods
	// (these are results from 2018, may be different now)

	if (this->m_vendor == GfxVendor::Nvidia || glClearTexSubImage == nullptr)
	{
		if (formatInfoGL.glIsCompressed)
		{
			cemuLog_logDebug(LogType::Force, "Unsupported clear for compressed texture");
			return; // todo - create integer texture view to clear compressed textures
		}
		if (hostTextureGeneric->isDepth)
		{
			cemuLog_logDebug(LogType::Force, "Unsupported clear for depth texture");
			return; // todo - use depth clear
		}

		glBindFramebuffer(GL_FRAMEBUFFER_EXT, glRendererState.clearFBO);
		// set attachment
		if (hostTexture->dim == Latte::E_DIM::DIM_2D)
			glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hostTexture->glId_texture, mipIndex);
		else
			glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, hostTexture->glId_texture, mipIndex, sliceIndex);

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glBindFramebuffer(GL_FRAMEBUFFER_EXT, prevBoundFBO);
		return;
	}
	if (glClearTexSubImage == nullptr)
		return;
	glClearTexSubImage(hostTexture->glId_texture, mipIndex, 0, 0, sliceIndex, effectiveWidth, effectiveHeight, 1, formatInfoGL.glSuppliedFormat, formatInfoGL.glSuppliedFormatType, NULL);
}

LatteTexture* OpenGLRenderer::texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels,
	uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth)
{
	return new LatteTextureGL(dim, physAddress, physMipAddress, format, width, height, depth, pitch, mipLevels, swizzle, tileMode, isDepth);
}

void OpenGLRenderer::texture_setActiveTextureUnit(sint32 index)
{
	if (activeTextureUnit != index)
	{
		glActiveTexture(GL_TEXTURE0 + index);
		activeTextureUnit = index;
	}
}

void OpenGLRenderer::texture_bindAndActivate(LatteTextureView* textureView, uint32 textureUnit)
{
	const auto textureViewGL = (LatteTextureViewGL*)textureView;
	// don't call glBindTexture if the texture is already bound
	if (m_latteBoundTextures[textureUnit] == textureViewGL)
	{
		texture_setActiveTextureUnit(textureUnit);
		return; // already bound
	}
	// bind
	m_latteBoundTextures[textureUnit] = textureViewGL;
	texture_setActiveTextureUnit(textureUnit);
	if (textureViewGL)
	{
		glBindTexture(textureViewGL->glTexTarget, textureViewGL->glTexId);
	}
}

void OpenGLRenderer::texture_notifyDelete(LatteTextureView* textureView)
{
	for (uint32 i = 0; i < Latte::GPU_LIMITS::NUM_TEXTURES_PER_STAGE * 3; i++)
	{
		if (m_latteBoundTextures[i] == textureView)
			m_latteBoundTextures[i] = nullptr;
	}
}

// set Latte texture, on the OpenGL renderer this behaves like _bindAndActivate() but doesn't call _setActiveTextureUnit() if the texture is already bound
void OpenGLRenderer::texture_setLatteTexture(LatteTextureView* textureView1, uint32 textureUnit)
{
	auto textureView = ((LatteTextureViewGL*)textureView1);

	cemu_assert_debug(textureUnit < Latte::GPU_LIMITS::NUM_TEXTURES_PER_STAGE * 3);
	if (m_latteBoundTextures[textureUnit] == textureView)
		return;
	if (textureView == nullptr)
		return;
	// bind
	if (glBindTextureUnit)
	{
		glBindTextureUnit(textureUnit, textureView->glTexId);
		m_latteBoundTextures[textureUnit] = textureView;
		activeTextureUnit = -1;
	}
	else
	{
		texture_setActiveTextureUnit(textureUnit);
		glBindTexture(textureView->glTexTarget, textureView->glTexId);
		m_latteBoundTextures[textureUnit] = textureView;
	}
}

void OpenGLRenderer::texture_copyImageSubData(LatteTexture* src, sint32 srcMip, sint32 effectiveSrcX, sint32 effectiveSrcY, sint32 srcSlice, LatteTexture* dst, sint32 dstMip, sint32 effectiveDstX, sint32 effectiveDstY,
	sint32 dstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight, sint32 srcDepth)
{
	auto srcGL = (LatteTextureGL*)src;
	auto dstGL = (LatteTextureGL*)dst;
	if ((srcGL->isAlternativeFormat || dstGL->isAlternativeFormat) && (srcGL->glInternalFormat != dstGL->glInternalFormat))
	{
		if (srcGL->format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_UINT && dstGL->format == Latte::E_GX2SURFFMT::BC4_UNORM)
		{
			cemu_assert_debug(dstGL->dim != Latte::E_DIM::DIM_2D);
			// special case where BC4 format is replaced with R16F for array/3d-textures (since OpenGL's BC4 compression only supports 2D textures)
			texture_syncSliceSpecialBC4(srcGL, srcSlice, srcMip, dstGL, dstSlice, dstMip);
			return;
		}
		else
		{
			cemuLog_logDebug(LogType::Force, "_syncSlice() called with unhandled alternative format");
			return;
		}
	}

	if (srcGL->format == Latte::E_GX2SURFFMT::R32_G32_B32_A32_UINT && dstGL->format == Latte::E_GX2SURFFMT::BC3_UNORM)
	{
		if ((dstGL->width >> dstMip) < 4 ||	(dstGL->height >> dstMip) < 4)
		{
			texture_syncSliceSpecialIntegerToBC3(srcGL, srcSlice, srcMip, dstGL, dstSlice, dstMip);
			return;

		}
	}
	catchOpenGLError();
	glCopyImageSubData(srcGL->glId_texture, srcGL->glTexTarget, srcMip, effectiveSrcX, effectiveSrcY, srcSlice, dstGL->glId_texture, dstGL->glTexTarget, dstMip, effectiveDstX, effectiveDstY, dstSlice, effectiveCopyWidth, effectiveCopyHeight, srcDepth);
	catchOpenGLError();
}

LatteTextureReadbackInfo* OpenGLRenderer::texture_createReadback(LatteTextureView* textureView)
{
	return new LatteTextureReadbackInfoGL(textureView);
}

void LatteDraw_resetAttributePointerCache();

void OpenGLRenderer::attributeStream_reset()
{
	// reset attribute state
	attributeStream_unbindVertexBuffer();
	// setup vertices
	SetArrayElementBuffer(0);
	LatteDraw_resetAttributePointerCache();
	SetAttributeArrayState(0, true, -1);
	SetAttributeArrayState(1, true, -1);
	for (uint32 i = 0; i < GPU_GL_MAX_NUM_ATTRIBUTE; i++)
		SetAttributeArrayState(i, false, -1);
}

void OpenGLRenderer::bufferCache_init(const sint32 bufferSize)
{
	glGenBuffers(1, &glAttributeCacheAB);
	glBindBuffer(GL_ARRAY_BUFFER, glAttributeCacheAB);
	glBufferData(GL_ARRAY_BUFFER, bufferSize, NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLRenderer::attributeStream_bindVertexCacheBuffer()
{
	if (_boundArrayBuffer == glAttributeCacheAB)
		return;
	_boundArrayBuffer = glAttributeCacheAB;
	glBindBuffer(GL_ARRAY_BUFFER, glAttributeCacheAB);
}

void OpenGLRenderer::attributeStream_unbindVertexBuffer()
{
	if (_boundArrayBuffer == 0)
		return;
	_boundArrayBuffer = 0;
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

RendererShader* OpenGLRenderer::shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool isGameShader, bool isGfxPackShader)
{
	return new RendererShaderGL(type, baseHash, auxHash, isGameShader, isGfxPackShader, source);
}

void OpenGLRenderer::shader_bind(RendererShader* shader)
{
	auto shaderGL = (RendererShaderGL*)shader;
	GLbitfield shaderBit;
	const auto program = shaderGL->GetProgram();
	switch(shader->GetType())
	{
	case RendererShader::ShaderType::kVertex:
		if (program == prevVertexShaderProgram)
			return;
		shaderBit = GL_VERTEX_SHADER_BIT;
		prevVertexShaderProgram = program;
		break;
	case RendererShader::ShaderType::kFragment:
		if (program == prevPixelShaderProgram)
			return;
		shaderBit = GL_FRAGMENT_SHADER_BIT;
		prevPixelShaderProgram = program;
		break;
	case RendererShader::ShaderType::kGeometry:
		if (program == prevGeometryShaderProgram)
			return;
		shaderBit = GL_GEOMETRY_SHADER_BIT;
		prevGeometryShaderProgram = program;
		break;
	default:
		UNREACHABLE;
	}

	catchOpenGLError();
	glUseProgramStages(m_pipeline, shaderBit, program);
	catchOpenGLError();
}

void OpenGLRenderer::shader_unbind(RendererShader::ShaderType shaderType)
{
	switch (shaderType) { 
		case RendererShader::ShaderType::kVertex:
		glUseProgramStages(m_pipeline, GL_VERTEX_SHADER_BIT, 0);
		prevVertexShaderProgram = -1;
		break;
	case RendererShader::ShaderType::kFragment: 
		glUseProgramStages(m_pipeline, GL_FRAGMENT_SHADER_BIT, 0);
		prevPixelShaderProgram = -1;
		break;
	case RendererShader::ShaderType::kGeometry: 
		glUseProgramStages(m_pipeline, GL_GEOMETRY_SHADER_BIT, 0);
		prevGeometryShaderProgram = -1;
		break;
	default: 
		UNREACHABLE;
	}
}

void decodeBC4Block_UNORM(uint8* blockStorage, float* rOutput);

void OpenGLRenderer::texture_syncSliceSpecialBC4(LatteTexture* srcTexture, sint32 srcSliceIndex, sint32 srcMipIndex, LatteTexture* dstTexture, sint32 dstSliceIndex, sint32 dstMipIndex)
{
	auto srcTextureGL = (LatteTextureGL*)srcTexture;
	auto dstTextureGL = (LatteTextureGL*)dstTexture;

	sint32 sourceTexWidth = std::max(srcTexture->width >> srcMipIndex, 1);
	sint32 sourceTexHeight = std::max(srcTexture->height >> srcMipIndex, 1);
	sint32 destTexWidth = std::max(dstTexture->width >> dstMipIndex, 1);
	sint32 destTexHeight = std::max(dstTexture->height >> dstMipIndex, 1);

	sint32 compressedCopyWidth = std::min(sourceTexWidth, std::max(1, destTexWidth / 4));
	sint32 compressedCopyHeight = std::min(sourceTexHeight, std::max(1, destTexHeight / 4));

	uint8* texelData = (uint8*)malloc(compressedCopyWidth*compressedCopyHeight * 8);
	float* pixelRGBA16fData = (float*)malloc(destTexWidth*destTexHeight * sizeof(float) * 2);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	if (glGetTextureSubImage)
		glGetTextureSubImage(srcTextureGL->glId_texture, 0, 0, 0, srcSliceIndex, compressedCopyWidth, compressedCopyHeight, 1, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, compressedCopyWidth * compressedCopyHeight * 8, texelData);
	for (sint32 bx = 0; bx < compressedCopyWidth; bx++)
	{
		for (sint32 by = 0; by < compressedCopyHeight; by++)
		{
			float rBlock[4 * 4];
			decodeBC4Block_UNORM(texelData + (bx + by * compressedCopyWidth) * 8, rBlock);
			for (sint32 sy = 0; sy < std::min(4, destTexHeight - by * 4); sy++)
			{
				for (sint32 sx = 0; sx < std::min(4, destTexWidth - bx * 4); sx++)
				{
					sint32 pixelIndex = (bx * 4 + sx) + (by * 4 + sy)*destTexWidth;
					pixelRGBA16fData[pixelIndex * 2] = rBlock[sx + sy * 4];
					pixelRGBA16fData[pixelIndex * 2 + 1] = rBlock[sx + sy * 4];
				}
			}
		}
	}
	// upload mip
	if (glGetTextureSubImage && glTextureSubImage3D)
		glTextureSubImage3D(dstTextureGL->glId_texture, dstMipIndex, 0, 0, dstSliceIndex, destTexWidth, destTexHeight, 1, GL_RG, GL_FLOAT, pixelRGBA16fData);
	free(pixelRGBA16fData);
	free(texelData);
	catchOpenGLError();
}

void OpenGLRenderer::texture_syncSliceSpecialIntegerToBC3(LatteTexture* srcTexture, sint32 srcSliceIndex, sint32 srcMipIndex, LatteTexture* dstTexture, sint32 dstSliceIndex, sint32 dstMipIndex)
{
	auto srcTextureGL = (LatteTextureGL*)srcTexture;
	auto dstTextureGL = (LatteTextureGL*)dstTexture;

	sint32 sourceTexWidth = std::max(srcTexture->width >> srcMipIndex, 1);
	sint32 sourceTexHeight = std::max(srcTexture->height >> srcMipIndex, 1);
	sint32 destTexWidth = std::max(dstTexture->width >> dstMipIndex, 1);
	sint32 destTexHeight = std::max(dstTexture->height >> dstMipIndex, 1);

	sint32 compressedCopyWidth = std::min(sourceTexWidth, std::max(1, destTexWidth / 4));
	sint32 compressedCopyHeight = std::min(sourceTexHeight, std::max(1, destTexHeight / 4));

	uint8* texelData = (uint8*)malloc(compressedCopyWidth*compressedCopyHeight * 16);

	catchOpenGLError();
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	catchOpenGLError();
	if (glGetTextureSubImage)
		glGetTextureSubImage(srcTextureGL->glId_texture, 0, 0, 0, srcSliceIndex, compressedCopyWidth, compressedCopyHeight, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, compressedCopyWidth * compressedCopyHeight * 16, texelData);
	//float* pixelRGBA16fData = (float*)malloc(destTexWidth*destTexHeight * sizeof(float) * 2);
	//for (sint32 bx = 0; bx < compressedCopyWidth; bx++)
	//{
	//	for (sint32 by = 0; by < compressedCopyHeight; by++)
	//	{
	//		float rBlock[4 * 4];
	//		decodeBC4Block_UNORM(texelData + (bx + by * compressedCopyWidth) * 8, rBlock);
	//		for (sint32 sy = 0; sy < min(4, destTexHeight - by * 4); sy++)
	//		{
	//			for (sint32 sx = 0; sx < min(4, destTexWidth - bx * 4); sx++)
	//			{
	//				sint32 pixelIndex = (bx * 4 + sx) + (by * 4 + sy)*destTexWidth;
	//				pixelRGBA16fData[pixelIndex * 2] = rBlock[sx + sy * 4];
	//				pixelRGBA16fData[pixelIndex * 2 + 1] = rBlock[sx + sy * 4];
	//			}
	//		}
	//	}
	//}
	// upload mip
	catchOpenGLError();
	if (glGetTextureSubImage && glCompressedTextureSubImage3D)
		glCompressedTextureSubImage3D(dstTextureGL->glId_texture, dstMipIndex, 0, 0, dstSliceIndex, destTexWidth, destTexHeight, 1, dstTextureGL->glInternalFormat, compressedCopyWidth * compressedCopyHeight * 16, texelData);
	free(texelData);
	catchOpenGLError();
}

void OpenGLRenderer::renderstate_updateBlendingAndColorControl()
{
	catchOpenGLError();
	const auto& colorControlReg = LatteGPUState.contextNew.CB_COLOR_CONTROL;

	const auto specialOp = colorControlReg.get_SPECIAL_OP();
	const uint32 blendEnableMask = colorControlReg.get_BLEND_MASK();
	const auto logicOp = colorControlReg.get_ROP();

	cemu_assert_debug(!colorControlReg.get_MULTIWRITE_ENABLE()); // not supported

	uint32 renderTargetMask = LatteGPUState.contextNew.CB_TARGET_MASK.get_MASK();
	if (specialOp == Latte::LATTE_CB_COLOR_CONTROL::E_SPECIALOP::DISABLE)
	{
		renderTargetMask = 0;
	}
	OpenGLRenderer::renderstate_setChannelTargetMask(renderTargetMask);
	catchOpenGLError();

	// handle depth and stencil control
	// get depth control parameters
	bool depthEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_Z_ENABLE();
	auto depthFunc = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_Z_FUNC();
	bool depthWriteEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_Z_WRITE_ENABLE();

	// get stencil control parameters
	bool stencilEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ENABLE();
	bool backStencilEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_BACK_STENCIL_ENABLE();
	auto frontStencilFunc = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_FUNC_F();
	auto frontStencilZPass = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ZPASS_F();
	auto frontStencilZFail = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ZFAIL_F();
	auto frontStencilFail = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_FAIL_F();
	auto backStencilFunc = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_FUNC_B();
	auto backStencilZPass = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ZPASS_B();
	auto backStencilZFail = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ZFAIL_B();
	auto backStencilFail = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_FAIL_B();

	// get stencil control parameters
	uint32 stencilCompareMaskFront = LatteGPUState.contextNew.DB_STENCILREFMASK.get_STENCILMASK_F();
	uint32 stencilWriteMaskFront = LatteGPUState.contextNew.DB_STENCILREFMASK.get_STENCILWRITEMASK_F();
	uint32 stencilRefFront = LatteGPUState.contextNew.DB_STENCILREFMASK.get_STENCILREF_F();
	uint32 stencilCompareMaskBack = LatteGPUState.contextNew.DB_STENCILREFMASK_BF.get_STENCILMASK_B();
	uint32 stencilWriteMaskBack = LatteGPUState.contextNew.DB_STENCILREFMASK_BF.get_STENCILWRITEMASK_B();
	uint32 stencilRefBack = LatteGPUState.contextNew.DB_STENCILREFMASK_BF.get_STENCILREF_B();

	const static GLenum stencilActionGX2ToGL[] =
	{
		GL_KEEP,
		GL_ZERO,
		GL_REPLACE,
		GL_INCR,
		GL_DECR,
		GL_INVERT,
		GL_INCR_WRAP,
		GL_DECR_WRAP
	};

	if (prevStencilEnable != stencilEnable)
	{
		if (stencilEnable)
			glEnable(GL_STENCIL_TEST);
		else
			glDisable(GL_STENCIL_TEST);
		prevStencilEnable = stencilEnable;
	}

	// update stencil parameters only if stencil is enabled
	if (stencilEnable)
	{
		if (!backStencilEnable)
		{
			// if back face stencil is disabled then use front parameters
			backStencilFunc = frontStencilFunc;
			backStencilZPass = frontStencilZPass;
			backStencilZFail = frontStencilZFail;
			backStencilFail = frontStencilFail;
			stencilRefBack = stencilRefFront;
			stencilCompareMaskBack = stencilCompareMaskFront;
			stencilWriteMaskBack = stencilWriteMaskFront;
		}
		// update stencil configuration for front side
		if (prevFrontStencilFail != frontStencilFail || prevFrontStencilZFail != frontStencilZFail || prevFrontStencilZPass != frontStencilZPass)
		{
			glStencilOpSeparate(GL_FRONT, stencilActionGX2ToGL[(size_t)frontStencilFail], stencilActionGX2ToGL[(size_t)frontStencilZFail], stencilActionGX2ToGL[(size_t)frontStencilZPass]);
			prevFrontStencilFail = frontStencilFail;
			prevFrontStencilZFail = frontStencilZFail;
			prevFrontStencilZPass = frontStencilZPass;
		}
		if (prevFrontStencilFunc != frontStencilFunc || prevStencilRefFront != stencilRefFront || prevStencilCompareMaskFront != stencilCompareMaskFront)
		{
			glStencilFuncSeparate(GL_FRONT, glDepthFuncTable[(size_t)frontStencilFunc], stencilRefFront, stencilCompareMaskFront);
			prevFrontStencilFunc = frontStencilFunc;
			prevStencilRefFront = stencilRefFront;
			prevStencilCompareMaskFront = stencilCompareMaskFront;
		}
		if (prevStencilWriteMaskFront != stencilWriteMaskFront)
		{
			glStencilMaskSeparate(GL_FRONT, stencilWriteMaskFront);
			prevStencilWriteMaskFront = stencilWriteMaskFront;
		}
		// update stencil configuration for back side
		if (prevBackStencilFail != backStencilFail || prevBackStencilZFail != backStencilZFail || prevBackStencilZPass != backStencilZPass)
		{
			glStencilOpSeparate(GL_BACK, stencilActionGX2ToGL[(size_t)backStencilFail], stencilActionGX2ToGL[(size_t)backStencilZFail], stencilActionGX2ToGL[(size_t)backStencilZPass]);
			prevBackStencilFail = backStencilFail;
			prevBackStencilZFail = backStencilZFail;
			prevBackStencilZPass = backStencilZPass;
		}
		if (prevBackStencilFunc != backStencilFunc || prevStencilRefBack != stencilRefBack || prevStencilCompareMaskBack != stencilCompareMaskBack)
		{
			glStencilFuncSeparate(GL_BACK, glDepthFuncTable[(size_t)backStencilFunc], stencilRefBack, stencilCompareMaskBack);
			prevBackStencilFunc = backStencilFunc;
			prevStencilRefBack = stencilRefBack;
			prevStencilCompareMaskBack = stencilCompareMaskBack;
		}
		if (prevStencilWriteMaskBack != stencilWriteMaskBack)
		{
			glStencilMaskSeparate(GL_BACK, stencilWriteMaskBack);
			prevStencilWriteMaskBack = stencilWriteMaskBack;
		}
	}

	if (depthEnable != prevDepthEnable)
	{
		if (depthEnable)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		prevDepthEnable = depthEnable;
	}
	if (depthWriteEnable != prevDepthWriteEnable)
	{
		if (depthWriteEnable)
			glDepthMask(GL_TRUE);
		else
			glDepthMask(GL_FALSE);
		prevDepthWriteEnable = depthWriteEnable;
	}
	if (depthFunc != prevDepthFunc)
	{
		glDepthFunc(glDepthFuncTable[(size_t)depthFunc]);
		prevDepthFunc = depthFunc;
	}

	catchOpenGLError();
	uint32 blendChangeMask = blendEnableMask ^ prevBlendMask;
	if (blendChangeMask)
	{
		for (uint32 i = 0; i < 8; i++)
		{
			if ((blendChangeMask & (1 << i)) != 0)
			{
				// bit changed -> blend mode was toggled
				if ((blendEnableMask & (1 << i)) != 0)
					glEnablei(GL_BLEND, i);
				else
					glDisablei(GL_BLEND, i);
			}
		}
		prevBlendMask = blendEnableMask;
	}
	catchOpenGLError();

	uint32* blendColorConstant = LatteGPUState.contextRegister + Latte::REGADDR::CB_BLEND_RED;
	if (blendColorConstant[0] != prevBlendColorConstant[0] ||
		blendColorConstant[1] != prevBlendColorConstant[1] ||
		blendColorConstant[2] != prevBlendColorConstant[2] ||
		blendColorConstant[3] != prevBlendColorConstant[3])
	{
		glBlendColor(*(float*)(blendColorConstant + 0), *(float*)(blendColorConstant + 1), *(float*)(blendColorConstant + 2), *(float*)(blendColorConstant + 3));
		prevBlendColorConstant[0] = blendColorConstant[0];
		prevBlendColorConstant[1] = blendColorConstant[1];
		prevBlendColorConstant[2] = blendColorConstant[2];
		prevBlendColorConstant[3] = blendColorConstant[3];
	}

	for (uint32 i = 0; i < 8; i++)
	{
		const auto& blendControlReg = LatteGPUState.contextNew.CB_BLENDN_CONTROL[i];
		if (blendControlReg.getRawValue() != prevBlendControlReg[i])
		{
			if (blendControlReg.get_SEPARATE_ALPHA_BLEND())
			{
				glBlendFuncSeparatei(i, GetGLBlendFactor(blendControlReg.get_COLOR_SRCBLEND()), GetGLBlendFactor(blendControlReg.get_COLOR_DSTBLEND()), GetGLBlendFactor(blendControlReg.get_ALPHA_SRCBLEND()), GetGLBlendFactor(blendControlReg.get_ALPHA_DSTBLEND()));
				glBlendEquationSeparatei(i, GetGLBlendCombineFunc(blendControlReg.get_COLOR_COMB_FCN()), GetGLBlendCombineFunc(blendControlReg.get_ALPHA_COMB_FCN()));
			}
			else
			{
				auto colorSrc = GetGLBlendFactor(blendControlReg.get_COLOR_SRCBLEND());
				auto colorDst = GetGLBlendFactor(blendControlReg.get_COLOR_DSTBLEND());
				glBlendFuncSeparatei(i, colorSrc, colorDst, colorSrc, colorDst);
				auto combineFunc = GetGLBlendCombineFunc(blendControlReg.get_COLOR_COMB_FCN());
				glBlendEquationSeparatei(i, combineFunc, combineFunc);
			}
			prevBlendControlReg[i] = blendControlReg.getRawValue();
		}
	}
	// set logic op
	uint32 logicOpGL = GL_COPY;
	if (logicOp == Latte::LATTE_CB_COLOR_CONTROL::E_LOGICOP::COPY)
		logicOpGL = GL_COPY;
	else if (logicOp == Latte::LATTE_CB_COLOR_CONTROL::E_LOGICOP::SET)
		logicOpGL = GL_SET;
	else if (logicOp == Latte::LATTE_CB_COLOR_CONTROL::E_LOGICOP::CLEAR)
		logicOpGL = GL_CLEAR;
	else if (logicOp == Latte::LATTE_CB_COLOR_CONTROL::E_LOGICOP::OR)
		logicOpGL = GL_OR;
	else
		cemu_assert_unimplemented();
	if (prevLogicOp != logicOpGL)
	{
		if (logicOpGL != GL_COPY)
			glEnable(GL_COLOR_LOGIC_OP);
		else
			glDisable(GL_COLOR_LOGIC_OP);
		glLogicOp(logicOpGL);
		prevLogicOp = logicOpGL;
	}

	// polygon control
	const auto& polygonControlReg = LatteGPUState.contextNew.PA_SU_SC_MODE_CNTL;
	const auto frontFace = polygonControlReg.get_FRONT_FACE();
	uint32 cullFront = polygonControlReg.get_CULL_FRONT();
	uint32 cullBack = polygonControlReg.get_CULL_BACK();
	// todo - polygon modes
	uint32 lineAndPointOffsetEnabled = polygonControlReg.get_OFFSET_PARA_ENABLED();
	uint32 polyOffsetFrontEnabled = polygonControlReg.get_OFFSET_FRONT_ENABLED();
	uint32 polyOffsetBackEnabled = polygonControlReg.get_OFFSET_BACK_ENABLED();

	uint32 cullEnable = (cullFront || cullBack) ? 1 : 0;
	if (prevCullEnable != cullEnable)
	{
		if (cullEnable)
			glEnable(GL_CULL_FACE);
		else
			glDisable(GL_CULL_FACE);
		prevCullEnable = cullEnable;
	}
	if (prevCullFrontFace != frontFace)
	{
		if (frontFace == Latte::LATTE_PA_SU_SC_MODE_CNTL::E_FRONTFACE::CCW)
			glFrontFace(GL_CCW);
		else if (frontFace == Latte::LATTE_PA_SU_SC_MODE_CNTL::E_FRONTFACE::CW)
			glFrontFace(GL_CW);
		else
			cemu_assert_unimplemented();
		prevCullFrontFace = frontFace;
	}
	if (prevCullFront != cullFront || prevCullBack != cullBack)
	{
		if (cullFront && cullBack)
			glCullFace(GL_FRONT_AND_BACK);
		else if (cullFront == 0 && cullBack)
			glCullFace(GL_BACK);
		else if (cullFront && cullBack == 0)
			glCullFace(GL_FRONT);
		else
			; // front and back disabled, do nothing here since we force disable culling via glDisable(GL_CULL_FACE) above
		prevCullFront = cullFront;
		prevCullBack = cullBack;
	}

	if (polyOffsetFrontEnabled != prevPolygonOffsetFrontEnabled)
	{
		if (polyOffsetFrontEnabled)
			glEnable(GL_POLYGON_OFFSET_FILL);
		else
			glDisable(GL_POLYGON_OFFSET_FILL);
		prevPolygonOffsetFrontEnabled = polyOffsetFrontEnabled;

	}

	if (polyOffsetFrontEnabled)
	{
		// if polygon offset is enabled check if offset/scale register changed
		if (LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_OFFSET.getRawValue() != prevPolygonFrontOffsetU32 || LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_SCALE.getRawValue() != prevPolygonFrontScaleU32 || LatteGPUState.contextNew.PA_SU_POLY_OFFSET_CLAMP.getRawValue() != prevPolygonClampU32)
		{
			float frontScale = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_SCALE.get_SCALE();
			float frontOffset = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_OFFSET.get_OFFSET();
			float offsetClamp = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_CLAMP.get_CLAMP();

			frontScale /= 16.0f;

			//if( glPolygonOffsetClampEXT )
			//	glPolygonOffsetClampEXT(frontOffset, frontScale, offsetClamp);		
			//else
			glPolygonOffset(frontScale, frontOffset);

			prevPolygonFrontOffsetU32 = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_SCALE.getRawValue();
			prevPolygonFrontScaleU32 = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_SCALE.getRawValue();
			prevPolygonClampU32 = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_CLAMP.getRawValue();
		}
	}
	// clip control
	catchOpenGLError();
	cemu_assert_debug(LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_ZCLIP_NEAR_DISABLE() == LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_ZCLIP_FAR_DISABLE()); // near/far clipping disabled individually
	uint32 zClipEnable = LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_ZCLIP_NEAR_DISABLE() == false;
	// todo: Mass Effect 3 uses precompiled display lists which seem to write values to PA_CL_CLIP_CNTL which aren't available via the traditional GX2 API
	if (prevZClipEnable != zClipEnable)
	{
		if (zClipEnable)
		{
			// disable depth clamping and enable clipping
			glDisable(GL_DEPTH_CLAMP);
		}
		else
		{
			// enable depth clamping and disable clipping
			glEnable(GL_DEPTH_CLAMP);
		}
		prevZClipEnable = zClipEnable;
	}
	catchOpenGLError();
	// point size
	const auto& pointSizeReg = LatteGPUState.contextNew.PA_SU_POINT_SIZE;
	if (pointSizeReg.getRawValue() != prevPointSizeReg)
	{
		float pointWidth = (float)pointSizeReg.get_WIDTH() / 8.0f;
		float pointHeight = (float)pointSizeReg.get_HEIGHT() / 8.0f;
		if (pointWidth == 0.0f)
			glPointSize(1.0f / 8.0f); // minimum size
		else
			glPointSize(pointWidth);
		prevPointSizeReg = pointSizeReg.getRawValue();
		catchOpenGLError();
	}
	// primitive restart index
	uint32 primitiveRestartIndex = LatteGPUState.contextNew.VGT_MULTI_PRIM_IB_RESET_INDX.get_RESTART_INDEX();
	if (prevPrimitiveRestartIndex != primitiveRestartIndex)
	{
		glPrimitiveRestartIndex(primitiveRestartIndex);
		prevPrimitiveRestartIndex = primitiveRestartIndex;
	}
}

void OpenGLRenderer::renderstate_resetColorControl()
{
	renderstate_setChannelTargetMask(0xF);
	// disable blending
	uint32 blendEnableMask = 0;
	for (uint32 i = 0; i < 8; i++)
	{
		if (((blendEnableMask^prevBlendMask)&(1 << i)) != 0)
		{
			// bit changed -> blend mode was toggled
			if ((blendEnableMask&(1 << i)) != 0)
				glEnablei(GL_BLEND, i);
			else
				glDisablei(GL_BLEND, i);
		}
	}
	prevBlendMask = blendEnableMask;
	// "forget" blend states
	for (uint32 i = 0; i < 8; i++)
	{
		prevBlendControlReg[i] = 0xFFFFFFFF;
	}
	// disable alpha test
	if (prevAlphaTestEnable != 0)
	{
		glDisable(GL_ALPHA_TEST);
		prevAlphaTestEnable = 0;
	}
	if (prevLogicOp != GL_COPY)
	{
		glDisable(GL_COLOR_LOGIC_OP);
		glLogicOp(GL_COPY);
		prevLogicOp = GL_COPY;
	}
	// disable culling
	uint32 cullEnable = 0;
	if (prevCullEnable != cullEnable)
	{
		glDisable(GL_CULL_FACE);
		prevCullEnable = 0;
	}
	// disable polygon offset
	if (prevPolygonOffsetFrontEnabled != 0)
	{
		glDisable(GL_POLYGON_OFFSET_FILL);
		prevPolygonOffsetFrontEnabled = 0;
	}
	// disable scissor box
	if (prevScissorEnable != false)
	{
		glDisable(GL_SCISSOR_TEST);
		prevScissorEnable = false;
	}
}

void OpenGLRenderer::renderstate_resetDepthControl()
{
	if (prevDepthEnable)
	{
		glDisable(GL_DEPTH_TEST);
		prevDepthEnable = false;
	}
	if (!prevDepthWriteEnable)
	{
		glDepthMask(GL_TRUE);
		prevDepthWriteEnable = true;
	}
	if (prevStencilEnable)
	{
		glDisable(GL_STENCIL_TEST);
		prevStencilEnable = false;
	}
	//if (prevZClipEnable == 0)
	//{
	//	glDisable(GL_DEPTH_CLAMP);
	//	prevZClipEnable = 1;
	//}

	glDisable(GL_DEPTH_CLAMP);
	prevZClipEnable = 1;

	if (prevPrimitiveRestartIndex != 0xFFFFFFFF)
	{
		glPrimitiveRestartIndex(0xFFFFFFFF);
		prevPrimitiveRestartIndex = 0xFFFFFFFF;
	}
}

void OpenGLRenderer::renderstate_resetStencilMask()
{
	uint32 stencilWriteMaskFront = 0xFFFFFFFF; // enable front mask
	uint32 stencilWriteMaskBack = 0xFFFFFFFF; // enable back mask
	if (prevStencilWriteMaskFront != stencilWriteMaskFront)
	{
		glStencilMaskSeparate(GL_FRONT, stencilWriteMaskFront);
		prevStencilWriteMaskFront = stencilWriteMaskFront;
	}
	if (prevStencilWriteMaskBack != stencilWriteMaskBack)
	{
		glStencilMaskSeparate(GL_BACK, stencilWriteMaskBack);
		prevStencilWriteMaskBack = stencilWriteMaskBack;
	}
}

void OpenGLRenderer::cleanupAfterFrame()
{
	ReleaseBufferCacheEntries();
}

void OpenGLRenderer::ReleaseBufferCacheEntries()
{
	for (auto& itr : m_destructionQueues.bufferCacheEntries)
		itr.free();
	m_destructionQueues.bufferCacheEntries.clear();
}
