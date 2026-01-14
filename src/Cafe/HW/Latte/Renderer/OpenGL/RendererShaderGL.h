#pragma once

#include "Cafe/HW/Latte/Renderer/RendererShader.h"
#include "Common/GLInclude/GLInclude.h"

class RendererShaderGL : public RendererShader
{
public:
	RendererShaderGL(ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader, const std::string& glslSource);

	virtual ~RendererShaderGL();

	void PreponeCompilation(bool isRenderThread) override;
	bool IsCompiled() override;
	bool WaitForCompiled() override;

	GLuint GetProgram() const { cemu_assert_debug(m_isCompiled); return m_program; }
	GLuint GetShaderObject() const { cemu_assert_debug(m_isCompiled); return m_shader_object; }

	sint32 GetUniformLocation(const char* name);

	void SetUniform1i(sint32 location, sint32 value);
	void SetUniform1f(sint32 location, float value);
	void SetUniform2fv(sint32 location, void* data, sint32 count);
	void SetUniform4iv(sint32 location, void* data, sint32 count);

	static void ShaderCacheLoading_begin(uint64 cacheTitleId);
	static void ShaderCacheLoading_end();
    static void ShaderCacheLoading_Close();

private:
	GLuint m_program;
	GLuint m_shader_object;

	bool loadBinary();
	void storeBinary();

	std::string m_glslSource;

	bool m_shader_attached{ false };
	bool m_isCompiled{ false };

	static class FileCache* s_programBinaryCache;
};

