#pragma once

#include "Cafe/HW/Latte/Renderer/RendererShader.h"
#include "util/helpers/ConcurrentQueue.h"

#include <vulkan/vulkan_core.h>
#include "util/helpers/Semaphore.h"
#include "util/helpers/fspinlock.h"

class RendererShaderVk : public RendererShader
{
	friend class VulkanRenderer;
	friend class _ShaderVkThreadPool;

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

	RendererShaderVk(ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader, const std::string& glslCode);
	virtual ~RendererShaderVk();

	sint32 GetUniformLocation(const char* name) override;
	void SetUniform1iv(sint32 location, void* data, sint32 count) override;
	void SetUniform2fv(sint32 location, void* data, sint32 count) override;
	void SetUniform4iv(sint32 location, void* data, sint32 count) override;
	VkShaderModule& GetShaderModule() { return m_shader_module; }

	static inline FSpinlock s_dependencyLock;

	void TrackDependency(class PipelineInfo* p)
	{
		s_dependencyLock.lock();
		list_pipelineInfo.emplace_back(p);
		s_dependencyLock.unlock();
	}

	void RemoveDependency(class PipelineInfo* p)
	{
		s_dependencyLock.lock();
		vectorRemoveByValue(list_pipelineInfo, p);
		s_dependencyLock.unlock();
	}

	void PreponeCompilation(bool isRenderThread) override;
	bool IsCompiled() override;
	bool WaitForCompiled() override;

private:
	void CompileInternal(bool isRenderThread);

	void FinishCompilation();

	VkShaderModule m_shader_module = nullptr;

	StateSemaphore<COMPILATION_STATE> m_compilationState{ COMPILATION_STATE::NONE };

	std::string m_glslCode;

	void CreateVkShaderModule(std::span<uint32> spirvBuffer);

	// pipeline infos
	std::vector<class PipelineInfo*> list_pipelineInfo;
};
