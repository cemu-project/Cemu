#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/RendererShaderGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/CachedFBOGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLSurfaceCopy.h"

#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureViewGL.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"

#include "util/helpers/StringBuf.h"

void LatteDraw_resetAttributePointerCache();

void _setDepthCompareMode(LatteTextureViewGL* textureView, uint8 depthCompareMode)
{
	if (depthCompareMode != textureView->samplerState.depthCompareMode)
	{
		if (depthCompareMode != 0)
			glTexParameteri(textureView->glTexTarget, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		else
			glTexParameteri(textureView->glTexTarget, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		textureView->samplerState.depthCompareMode = depthCompareMode;
	}
}

void OpenGLRenderer::surfaceCopy_copySurfaceWithFormatConversion(LatteTexture* sourceTexture, sint32 srcMip, sint32 srcSlice, LatteTexture* destinationTexture, sint32 dstMip, sint32 dstSlice, sint32 width, sint32 height)
{
	// scale copy size to effective size
	sint32 effectiveCopyWidth = width;
	sint32 effectiveCopyHeight = height;
	LatteTexture_scaleToEffectiveSize(sourceTexture, &effectiveCopyWidth, &effectiveCopyHeight, 0);
	sint32 sourceEffectiveWidth, sourceEffectiveHeight;
	sourceTexture->GetEffectiveSize(sourceEffectiveWidth, sourceEffectiveHeight, srcMip);
	// reset everything
	renderstate_resetColorControl();
	renderstate_resetDepthControl();
	attributeStream_unbindVertexBuffer();

	SetArrayElementBuffer(0);
	LatteDraw_resetAttributePointerCache();
	SetAttributeArrayState(0, true, -1);
	SetAttributeArrayState(1, true, -1);
	for (uint32 i = 2; i < GPU_GL_MAX_NUM_ATTRIBUTE; i++)
		SetAttributeArrayState(i, false, -1);
	catchOpenGLError();
	// set viewport
	g_renderer->renderTarget_setViewport(0, 0, (float)effectiveCopyWidth, (float)effectiveCopyHeight, 0.0f, 1.0f);
	catchOpenGLError();
	// get a view of the copied slice/mip in the source and destination texture
	LatteTextureView* sourceView = sourceTexture->GetOrCreateView(srcMip, 1, srcSlice, 1);
	LatteTextureView* destinationView = destinationTexture->GetOrCreateView(dstMip, 1, dstSlice, 1);

	texture_bindAndActivate(sourceView, 0);
	catchOpenGLError();
	// setup texture attributes
	_setDepthCompareMode((LatteTextureViewGL*)sourceView, 0);
	catchOpenGLError();
	// bind framebuffer
	if (destinationTexture->isDepth)
		LatteMRT::BindDepthBufferOnly(destinationView);
	else
		LatteMRT::BindColorBufferOnly(destinationView);
	catchOpenGLError();
	// enable depth writes if the destination is a depth texture
	if (destinationTexture->isDepth)
		renderstate_setAlwaysWriteDepth();
	// bind format specific copy shader
	LatteGLDefaultShader_t* copyShader = LatteGLDefaultShader_getPixelCopyShader_depthToColor();
	if (destinationTexture->isDepth)
		copyShader = LatteGLDefaultShader_getPixelCopyShader_colorToDepth();
	glUseProgram(copyShader->glProgamId);
	catchOpenGLError();
	// setup uniforms
	glUniform1i(copyShader->copyShaderUniforms.uniformLoc_textureSrc, 0);
	catchOpenGLError();

	float vertexOffsets[4 * 4];
	float srcCopyWidth = (float)width / (float)sourceTexture->width;
	float srcCopyHeight = (float)height / (float)sourceTexture->height;
	// q0 vertex
	vertexOffsets[0] = -1.0f;
	vertexOffsets[1] = 1.0f;
	// q0 uv
	vertexOffsets[2] = 0.0f;
	vertexOffsets[3] = 0.0f;
	// q1
	vertexOffsets[4] = 1.0f;
	vertexOffsets[5] = 1.0f;
	// q1 uv
	vertexOffsets[6] = srcCopyWidth;
	vertexOffsets[7] = 0.0f;
	// q2
	vertexOffsets[8] = -1.0f;
	vertexOffsets[9] = -1.0f;
	// q2 uv
	vertexOffsets[10] = 0.0f;
	vertexOffsets[11] = srcCopyHeight;
	// q3
	vertexOffsets[12] = 1.0f;
	vertexOffsets[13] = -1.0f;
	// q3 uv
	vertexOffsets[14] = srcCopyWidth;
	vertexOffsets[15] = srcCopyHeight;

	glUniform4fv(copyShader->copyShaderUniforms.uniformLoc_vertexOffsets, 4, vertexOffsets);
	catchOpenGLError();

	// draw
	uint16 indexData[6] = { 0,1,3,0,2,3 };
	glDrawRangeElements(GL_TRIANGLES, 0, 5, 6, GL_UNSIGNED_SHORT, indexData);
	catchOpenGLError();

	LatteGPUState.repeatTextureInitialization = true;
	glUseProgram(0);
}

LatteGLDefaultShader_t* _copyShader_depthToColor;
LatteGLDefaultShader_t* _copyShader_colorToDepth;

void LatteGLDefaultShader_pixelCopyShader_generateVSBody(StringBuf* vs)
{
	vs->add("#version 420\r\n");
	vs->add("out vec2 passUV;\r\n");
	vs->add("uniform vec4 uf_vertexOffsets[4];\r\n");
	vs->add("\r\n");
	vs->add("void main(){\r\n");
	vs->add("int vID = gl_VertexID;\r\n");
	vs->add("passUV = uf_vertexOffsets[vID].zw;\r\n");
	vs->add("gl_Position = vec4(uf_vertexOffsets[vID].xy, 0.0, 1.0);\r\n");
	vs->add("}\r\n");
}

GLuint gxShaderDepr_compileRaw(StringBuf* strSourceVS, StringBuf* strSourceFS);
GLuint gxShaderDepr_compileRaw(const std::string& vertex_source, const std::string& fragment_source);

LatteGLDefaultShader_t* LatteGLDefaultShader_getPixelCopyShader_depthToColor()
{
	if (_copyShader_depthToColor != 0)
		return _copyShader_depthToColor;
	catchOpenGLError();
	LatteGLDefaultShader_t* defaultShader = (LatteGLDefaultShader_t*)malloc(sizeof(LatteGLDefaultShader_t));
	memset(defaultShader, 0, sizeof(LatteGLDefaultShader_t));

	StringBuf fCStr_vertexShader(1024 * 16);
	LatteGLDefaultShader_pixelCopyShader_generateVSBody(&fCStr_vertexShader);

	StringBuf fCStr_defaultFragShader(1024 * 16);
	fCStr_defaultFragShader.add("#version 420\r\n");
	fCStr_defaultFragShader.add("in vec2 passUV;\r\n");
	fCStr_defaultFragShader.add("uniform sampler2D textureSrc;\r\n");
	fCStr_defaultFragShader.add("layout(location = 0) out vec4 colorOut0;\r\n");
	fCStr_defaultFragShader.add("\r\n");
	fCStr_defaultFragShader.add("void main(){\r\n");
	fCStr_defaultFragShader.add("colorOut0 = vec4(texture(textureSrc, passUV).r,0.0,0.0,1.0);\r\n");
	fCStr_defaultFragShader.add("}\r\n");

	defaultShader->glProgamId = gxShaderDepr_compileRaw(&fCStr_vertexShader, &fCStr_defaultFragShader);
	catchOpenGLError();

	defaultShader->copyShaderUniforms.uniformLoc_textureSrc = glGetUniformLocation(defaultShader->glProgamId, "textureSrc");
	defaultShader->copyShaderUniforms.uniformLoc_vertexOffsets = glGetUniformLocation(defaultShader->glProgamId, "uf_vertexOffsets");

	_copyShader_depthToColor = defaultShader;
	catchOpenGLError();
	return defaultShader;
}

LatteGLDefaultShader_t* LatteGLDefaultShader_getPixelCopyShader_colorToDepth()
{
	if (_copyShader_colorToDepth != 0)
		return _copyShader_colorToDepth;
	catchOpenGLError();
	LatteGLDefaultShader_t* defaultShader = (LatteGLDefaultShader_t*)malloc(sizeof(LatteGLDefaultShader_t));
	memset(defaultShader, 0, sizeof(LatteGLDefaultShader_t));

	StringBuf fCStr_vertexShader(1024 * 16);
	LatteGLDefaultShader_pixelCopyShader_generateVSBody(&fCStr_vertexShader);

	StringBuf fCStr_defaultFragShader(1024 * 16);
	fCStr_defaultFragShader.add("#version 420\r\n");
	fCStr_defaultFragShader.add("in vec2 passUV;\r\n");
	fCStr_defaultFragShader.add("uniform sampler2D textureSrc;\r\n");
	fCStr_defaultFragShader.add("layout(location = 0) out vec4 colorOut0;\r\n");
	fCStr_defaultFragShader.add("\r\n");
	fCStr_defaultFragShader.add("void main(){\r\n");
	fCStr_defaultFragShader.add("gl_FragDepth = texture(textureSrc, passUV).r;\r\n");
	fCStr_defaultFragShader.add("}\r\n");


	defaultShader->glProgamId = gxShaderDepr_compileRaw(&fCStr_vertexShader, &fCStr_defaultFragShader);
	defaultShader->copyShaderUniforms.uniformLoc_textureSrc = glGetUniformLocation(defaultShader->glProgamId, "textureSrc");
	defaultShader->copyShaderUniforms.uniformLoc_vertexOffsets = glGetUniformLocation(defaultShader->glProgamId, "uf_vertexOffsets");

	_copyShader_colorToDepth = defaultShader;
	catchOpenGLError();
	return defaultShader;
}