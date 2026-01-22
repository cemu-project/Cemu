#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteDefaultShaders.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/RendererShaderGL.h"

LatteDefaultShader_t* _copyShader_depthToColor;
LatteDefaultShader_t* _copyShader_colorToDepth;

LatteDefaultShader_t* LatteDefaultShader_getPixelCopyShader_depthToColor()
{
	if (_copyShader_depthToColor != 0)
		return _copyShader_depthToColor;
	catchOpenGLError();
	LatteDefaultShader_t* defaultShader = new LatteDefaultShader_t{};

	std::string defaultFragmentShader =
	R"glsl(#version 420
	in vec2 passUV;
	uniform sampler2D textureSrc;
	layout(location = 0) out vec4 colorOut0;

	void main(){
		colorOut0 = vec4(texture(textureSrc, passUV).r,0.0,0.0,1.0);
	}
	)glsl";

	RendererShaderGL* fragShader = static_cast<RendererShaderGL*>(g_renderer->shader_create(RendererShader::ShaderType::kFragment, 0, 0, std::move(defaultFragmentShader), false, false));
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
	LatteDefaultShader_t* defaultShader = new LatteDefaultShader_t{};

	std::string defaultFragShader = R"glsl(#version 420
		in vec2 passUV;
		uniform sampler2D textureSrc;
		layout(location = 0) out vec4 colorOut0;

		void main() {
			gl_FragDepth = texture(textureSrc, passUV).r;
		}
	)glsl";

	RendererShaderGL* fragShader = static_cast<RendererShaderGL*>(g_renderer->shader_create(RendererShader::ShaderType::kFragment, 0, 0, std::move(defaultFragShader), false, false));
	fragShader->PreponeCompilation();
	defaultShader->glProgamId = fragShader->GetProgram();
	defaultShader->copyShaderUniforms.uniformLoc_textureSrc =  fragShader->GetUniformLocation("textureSrc");
	defaultShader->copyShaderUniforms.uniformLoc_vertexOffsets =  fragShader->GetUniformLocation("uf_vertexOffsets");

	_copyShader_colorToDepth = defaultShader;
	catchOpenGLError();
	return defaultShader;
}