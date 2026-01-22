#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteDefaultShaders.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/RendererShaderGL.h"
#include "util/helpers/StringBuf.h"

LatteDefaultShader_t* _copyShader_depthToColor;
LatteDefaultShader_t* _copyShader_colorToDepth;

LatteDefaultShader_t* LatteDefaultShader_getPixelCopyShader_depthToColor()
{
	if (_copyShader_depthToColor != 0)
		return _copyShader_depthToColor;
	catchOpenGLError();
	LatteDefaultShader_t* defaultShader = (LatteDefaultShader_t*)malloc(sizeof(LatteDefaultShader_t));
	memset(defaultShader, 0, sizeof(LatteDefaultShader_t));

	StringBuf fCStr_defaultFragShader(1024 * 16);
	fCStr_defaultFragShader.add("#version 420\r\n");
	fCStr_defaultFragShader.add("in vec2 passUV;\r\n");
	fCStr_defaultFragShader.add("uniform sampler2D textureSrc;\r\n");
	fCStr_defaultFragShader.add("layout(location = 0) out vec4 colorOut0;\r\n");
	fCStr_defaultFragShader.add("\r\n");
	fCStr_defaultFragShader.add("void main(){\r\n");
	fCStr_defaultFragShader.add("colorOut0 = vec4(texture(textureSrc, passUV).r,0.0,0.0,1.0);\r\n");
	fCStr_defaultFragShader.add("}\r\n");

	RendererShaderGL* fragShader = static_cast<RendererShaderGL*>(g_renderer->shader_create(RendererShader::ShaderType::kFragment, 0, 0, std::string{fCStr_defaultFragShader.c_str()}, false, false));
	fragShader->PreponeCompilation();

	defaultShader->glProgamId = fragShader->GetProgram();
	catchOpenGLError();

	defaultShader->copyShaderUniforms.uniformLoc_textureSrc = fragShader->GetUniformLocation("textureSrc");
	defaultShader->copyShaderUniforms.uniformLoc_vertexOffsets = fragShader->GetUniformLocation("uf_vertexOffsets");

	_copyShader_depthToColor = defaultShader;
	catchOpenGLError();
	return defaultShader;
}

LatteDefaultShader_t* LatteDefaultShader_getPixelCopyShader_colorToDepth()
{
	if (_copyShader_colorToDepth != 0)
		return _copyShader_colorToDepth;
	catchOpenGLError();
	LatteDefaultShader_t* defaultShader = (LatteDefaultShader_t*)malloc(sizeof(LatteDefaultShader_t));
	memset(defaultShader, 0, sizeof(LatteDefaultShader_t));

	StringBuf fCStr_defaultFragShader(1024 * 16);
	fCStr_defaultFragShader.add("#version 420\r\n");
	fCStr_defaultFragShader.add("in vec2 passUV;\r\n");
	fCStr_defaultFragShader.add("uniform sampler2D textureSrc;\r\n");
	fCStr_defaultFragShader.add("layout(location = 0) out vec4 colorOut0;\r\n");
	fCStr_defaultFragShader.add("\r\n");
	fCStr_defaultFragShader.add("void main(){\r\n");
	fCStr_defaultFragShader.add("gl_FragDepth = texture(textureSrc, passUV).r;\r\n");
	fCStr_defaultFragShader.add("}\r\n");


	RendererShaderGL* fragShader = static_cast<RendererShaderGL*>(g_renderer->shader_create(RendererShader::ShaderType::kFragment, 0, 0, std::string{fCStr_defaultFragShader.c_str()}, false, false));
	fragShader->PreponeCompilation();
	defaultShader->glProgamId = fragShader->GetProgram();
	defaultShader->copyShaderUniforms.uniformLoc_textureSrc =  fragShader->GetUniformLocation("textureSrc");
	defaultShader->copyShaderUniforms.uniformLoc_vertexOffsets =  fragShader->GetUniformLocation("uf_vertexOffsets");

	_copyShader_colorToDepth = defaultShader;
	catchOpenGLError();
	return defaultShader;
}