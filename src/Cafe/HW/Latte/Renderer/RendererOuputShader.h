#pragma once

#include "Cafe/HW/Latte/Renderer/RendererShader.h"
#include "util/math/vector2.h"

#include "Cafe/HW/Latte/Core/LatteTexture.h"

class RendererOutputShader
{
public:
	enum Shader
	{
		kCopy,
		kBicubic,
		kHermit,
	};
	RendererOutputShader(const std::string& vertex_source, const std::string& fragment_source);
	virtual ~RendererOutputShader() = default;

	void SetUniformParameters(const LatteTextureView& texture_view, const Vector2i& output_res) const;

	RendererShader* GetVertexShader() const
	{
		return m_vertex_shader;
	}

	RendererShader* GetFragmentShader() const
	{
		return m_fragment_shader;
	}

	static void InitializeStatic();

	static RendererOutputShader* s_copy_shader;
	static RendererOutputShader* s_copy_shader_ud;

	static RendererOutputShader* s_bicubic_shader;
	static RendererOutputShader* s_bicubic_shader_ud;

	static RendererOutputShader* s_hermit_shader;
	static RendererOutputShader* s_hermit_shader_ud;

	static std::string GetVulkanVertexSource(bool render_upside_down);
	static std::string GetOpenGlVertexSource(bool render_upside_down);

	static std::string PrependFragmentPreamble(const std::string& shaderSrc);

protected:
	RendererShader* m_vertex_shader;
	RendererShader* m_fragment_shader;

	struct UniformLocations
	{
		sint32 m_loc_textureSrcResolution = -1;
		sint32 m_loc_nativeResolution = -1;
		sint32 m_loc_outputResolution = -1;
	} m_uniformLocations[2]{};

private:
	static const std::string s_copy_shader_source;
	static const std::string s_bicubic_shader_source;
	static const std::string s_hermite_shader_source;

	static const std::string s_bicubic_shader_source_vk;
	static const std::string s_hermite_shader_source_vk;
};
