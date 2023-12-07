#pragma once

class RendererShader
{
public:
	enum class ShaderType
	{
		kVertex,
		kFragment,
		kGeometry
	};

	virtual ~RendererShader() = default;

	ShaderType GetType() const { return m_type; }
	
	virtual void PreponeCompilation(bool isRenderThread) = 0; // if shader not yet compiled, compile it synchronously (if possible) or alternatively wait for compilation. After this function IsCompiled() is guaranteed to be true
	virtual bool IsCompiled() = 0;
	virtual bool WaitForCompiled() = 0;

	virtual sint32 GetUniformLocation(const char* name) = 0;

	virtual void SetUniform2fv(sint32 location, void* data, sint32 count) = 0;
	virtual void SetUniform4iv(sint32 location, void* data, sint32 count) = 0;

protected:
	// if isGameShader is true, then baseHash and auxHash are valid
	RendererShader(ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader)
		: m_type(type), m_baseHash(baseHash), m_auxHash(auxHash), m_isGameShader(isGameShader), m_isGfxPackShader(isGfxPackShader) {}

	static uint32 GeneratePrecompiledCacheId();
	static void GenerateShaderPrecompiledCacheFilename(ShaderType type, uint64 baseHash, uint64 auxHash, uint64& h1, uint64& h2);

protected:
	ShaderType m_type;
	uint64 m_baseHash;	
	uint64 m_auxHash;
	bool m_isGameShader;
	bool m_isGfxPackShader;
};

