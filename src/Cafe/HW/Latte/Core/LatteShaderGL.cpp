#include "Common/GLInclude/GLInclude.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/RendererShaderGL.h"


void LatteShader_prepareSeparableUniforms(LatteDecompilerShader* shader)
{
	if(shader->hasError)
		return;

	auto shaderGL = (RendererShaderGL*)shader->shader;
	// setup uniform info
	if (shader->shaderType == LatteConst::ShaderType::Vertex)
	{
		shader->uniform.loc_remapped = glGetUniformLocation(shaderGL->GetProgram(), "uf_remappedVS");
		shader->uniform.loc_uniformRegister = glGetUniformLocation(shaderGL->GetProgram(), "uf_uniformRegisterVS");
	}
	else if (shader->shaderType == LatteConst::ShaderType::Geometry)
	{
		shader->uniform.loc_remapped = glGetUniformLocation(shaderGL->GetProgram(), "uf_remappedGS");
		shader->uniform.loc_uniformRegister = glGetUniformLocation(shaderGL->GetProgram(), "uf_uniformRegisterGS");
	}
	else if (shader->shaderType == LatteConst::ShaderType::Pixel)
	{
		shader->uniform.loc_remapped = glGetUniformLocation(shaderGL->GetProgram(), "uf_remappedPS");
		shader->uniform.loc_uniformRegister = glGetUniformLocation(shaderGL->GetProgram(), "uf_uniformRegisterPS");
	}
	catchOpenGLError();
	shader->uniform.loc_windowSpaceToClipSpaceTransform = glGetUniformLocation(shaderGL->GetProgram(), "uf_windowSpaceToClipSpaceTransform");
	shader->uniform.loc_alphaTestRef = glGetUniformLocation(shaderGL->GetProgram(), "uf_alphaTestRef");
	shader->uniform.loc_pointSize = glGetUniformLocation(shaderGL->GetProgram(), "uf_pointSize");
	shader->uniform.loc_fragCoordScale = glGetUniformLocation(shaderGL->GetProgram(), "uf_fragCoordScale");
	cemu_assert_debug(shader->uniform.list_ufTexRescale.empty());
	for (sint32 t = 0; t < LATTE_NUM_MAX_TEX_UNITS; t++)
	{
		char ufName[64];
		sprintf(ufName, "uf_tex%dScale", t);
		GLint uniformLocation = glGetUniformLocation(shaderGL->GetProgram(), ufName);
		if (uniformLocation >= 0)
		{
			LatteUniformTextureScaleEntry_t entry = { 0 };
			entry.texUnit = t;
			entry.uniformLocation = uniformLocation;
			shader->uniform.list_ufTexRescale.push_back(entry);
		}
	}
}
