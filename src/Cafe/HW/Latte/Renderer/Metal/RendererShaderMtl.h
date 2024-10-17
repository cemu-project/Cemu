#pragma once

#include "Cafe/HW/Latte/Renderer/RendererShader.h"
#include "HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "util/helpers/ConcurrentQueue.h"
#include "util/helpers/Semaphore.h"

#include <Metal/Metal.hpp>

class RendererShaderMtl : public RendererShader
{
    friend class ShaderMtlThreadPool;

	enum class COMPILATION_STATE : uint32
	{
		NONE,
		QUEUED,
		COMPILING,
		DONE
	};

public:
    static void ShaderCacheLoading_begin(uint64 cacheTitleId);
    static void ShaderCacheLoading_end();
    static void ShaderCacheLoading_Close();

    static void Initialize();
	static void Shutdown();

	RendererShaderMtl(class MetalRenderer* mtlRenderer, ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader, const std::string& mslCode);
	virtual ~RendererShaderMtl();

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

	void PreponeCompilation(bool isRenderThread) override;
	bool IsCompiled() override;
	bool WaitForCompiled() override;

private:
    class MetalRenderer* m_mtlr;

	MTL::Function* m_function = nullptr;

	StateSemaphore<COMPILATION_STATE> m_compilationState{ COMPILATION_STATE::NONE };

	std::string m_mslCode;

	bool ShouldCountCompilation() const;

	void CompileInternal();

	void FinishCompilation();
};
