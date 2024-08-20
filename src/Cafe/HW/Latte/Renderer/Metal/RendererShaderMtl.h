#pragma once

#include "Cafe/HW/Latte/Renderer/RendererShader.h"
#include "HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "util/helpers/ConcurrentQueue.h"

#include <Metal/Metal.hpp>

class RendererShaderMtl : public RendererShader
{
	//enum class COMPILATION_STATE : uint32
	//{
	//	NONE,
	//	QUEUED,
	//	COMPILING,
	//	DONE
	//};

public:
	RendererShaderMtl(class MetalRenderer* mtlRenderer, ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader, const std::string& mslCode);
	virtual ~RendererShaderMtl();

	void CompileVertexFunction()
	{
	    Compile(m_mslCode);
	}

	void CompileObjectFunction(const LatteContextRegister& lcr, const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, Renderer::INDEX_TYPE hostIndexType);
	void CompileMeshFunction(const LatteContextRegister& lcr, const LatteFetchShader* fetchShader);
	void CompileFragmentFunction(CachedFBOMtl* activeFBO);

	MTL::Function* GetFunction() const
	{
	    return m_function;
	}

	sint32 GetUniformLocation(const char* name) override
	{
	    cemu_assert_suspicious();
	    return 0;
	}

	void SetUniform2fv(sint32 location, void* data, sint32 count) override
	{
	    cemu_assert_suspicious();
	}

	void SetUniform4iv(sint32 location, void* data, sint32 count) override
	{
	    cemu_assert_suspicious();
	}

	// TODO: implement this
	void PreponeCompilation(bool isRenderThread) override {}
	bool IsCompiled() override { return true; }
	bool WaitForCompiled() override { return true; }

private:
    class MetalRenderer* m_mtlr;

	MTL::Function* m_function = nullptr;

	std::vector<uint8> m_binary;
	std::string m_mslCode;

	void Compile(const std::string& mslCode);
};
