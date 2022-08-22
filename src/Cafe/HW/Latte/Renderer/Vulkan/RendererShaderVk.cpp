#include "Cafe/HW/Latte/Renderer/Vulkan/RendererShaderVk.h"

#if __has_include(<glslang/build_info.h>)
#include <glslang/build_info.h>
#else
#define GLSLANG_VERSION_LESS_OR_EQUAL_TO (false)
#endif

#include <glslang/Public/ShaderLang.h>
#if GLSLANG_VERSION_LESS_OR_EQUAL_TO(11, 0, 0)
#include <glslang/SPIRV/GlslangToSpv.h>
#else
#include <SPIRV/Logger.h>
#include <SPIRV/GlslangToSpv.h>
#endif


#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"

#include <thread>

#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"
#include "util/helpers/ConcurrentQueue.h"

#include "Cemu/FileCache/FileCache.h"

bool s_isLoadingShadersVk{ false };
class FileCache* s_spirvCache{nullptr};

extern std::atomic_int g_compiled_shaders_total;
extern std::atomic_int g_compiled_shaders_async;

const TBuiltInResource DefaultTBuiltInResource = {
	/* .MaxLights = */ 32,
	/* .MaxClipPlanes = */ 6,
	/* .MaxTextureUnits = */ 32,
	/* .MaxTextureCoords = */ 32,
	/* .MaxVertexAttribs = */ 64,
	/* .MaxVertexUniformComponents = */ 4096,
	/* .MaxVaryingFloats = */ 64,
	/* .MaxVertexTextureImageUnits = */ 32,
	/* .MaxCombinedTextureImageUnits = */ 80,
	/* .MaxTextureImageUnits = */ 32,
	/* .MaxFragmentUniformComponents = */ 4096,
	/* .MaxDrawBuffers = */ 32,
	/* .MaxVertexUniformVectors = */ 128,
	/* .MaxVaryingVectors = */ 8,
	/* .MaxFragmentUniformVectors = */ 16,
	/* .MaxVertexOutputVectors = */ 16,
	/* .MaxFragmentInputVectors = */ 15,
	/* .MinProgramTexelOffset = */ -8,
	/* .MaxProgramTexelOffset = */ 7,
	/* .MaxClipDistances = */ 8,
	/* .MaxComputeWorkGroupCountX = */ 65535,
	/* .MaxComputeWorkGroupCountY = */ 65535,
	/* .MaxComputeWorkGroupCountZ = */ 65535,
	/* .MaxComputeWorkGroupSizeX = */ 1024,
	/* .MaxComputeWorkGroupSizeY = */ 1024,
	/* .MaxComputeWorkGroupSizeZ = */ 64,
	/* .MaxComputeUniformComponents = */ 1024,
	/* .MaxComputeTextureImageUnits = */ 16,
	/* .MaxComputeImageUniforms = */ 8,
	/* .MaxComputeAtomicCounters = */ 8,
	/* .MaxComputeAtomicCounterBuffers = */ 1,
	/* .MaxVaryingComponents = */ 60,
	/* .MaxVertexOutputComponents = */ 64,
	/* .MaxGeometryInputComponents = */ 64,
	/* .MaxGeometryOutputComponents = */ 128,
	/* .MaxFragmentInputComponents = */ 128,
	/* .MaxImageUnits = */ 8,
	/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/* .MaxCombinedShaderOutputResources = */ 8,
	/* .MaxImageSamples = */ 0,
	/* .MaxVertexImageUniforms = */ 0,
	/* .MaxTessControlImageUniforms = */ 0,
	/* .MaxTessEvaluationImageUniforms = */ 0,
	/* .MaxGeometryImageUniforms = */ 0,
	/* .MaxFragmentImageUniforms = */ 8,
	/* .MaxCombinedImageUniforms = */ 8,
	/* .MaxGeometryTextureImageUnits = */ 16,
	/* .MaxGeometryOutputVertices = */ 256,
	/* .MaxGeometryTotalOutputComponents = */ 1024,
	/* .MaxGeometryUniformComponents = */ 1024,
	/* .MaxGeometryVaryingComponents = */ 64,
	/* .MaxTessControlInputComponents = */ 128,
	/* .MaxTessControlOutputComponents = */ 128,
	/* .MaxTessControlTextureImageUnits = */ 16,
	/* .MaxTessControlUniformComponents = */ 1024,
	/* .MaxTessControlTotalOutputComponents = */ 4096,
	/* .MaxTessEvaluationInputComponents = */ 128,
	/* .MaxTessEvaluationOutputComponents = */ 128,
	/* .MaxTessEvaluationTextureImageUnits = */ 16,
	/* .MaxTessEvaluationUniformComponents = */ 1024,
	/* .MaxTessPatchComponents = */ 120,
	/* .MaxPatchVertices = */ 32,
	/* .MaxTessGenLevel = */ 64,
	/* .MaxViewports = */ 16,
	/* .MaxVertexAtomicCounters = */ 0,
	/* .MaxTessControlAtomicCounters = */ 0,
	/* .MaxTessEvaluationAtomicCounters = */ 0,
	/* .MaxGeometryAtomicCounters = */ 0,
	/* .MaxFragmentAtomicCounters = */ 8,
	/* .MaxCombinedAtomicCounters = */ 8,
	/* .MaxAtomicCounterBindings = */ 1,
	/* .MaxVertexAtomicCounterBuffers = */ 0,
	/* .MaxTessControlAtomicCounterBuffers = */ 0,
	/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
	/* .MaxGeometryAtomicCounterBuffers = */ 0,
	/* .MaxFragmentAtomicCounterBuffers = */ 1,
	/* .MaxCombinedAtomicCounterBuffers = */ 1,
	/* .MaxAtomicCounterBufferSize = */ 16384,
	/* .MaxTransformFeedbackBuffers = */ 4,
	/* .MaxTransformFeedbackInterleavedComponents = */ 64,
	/* .MaxCullDistances = */ 8,
	/* .MaxCombinedClipAndCullDistances = */ 8,
	/* .MaxSamples = */ 4,
	
	/* .maxMeshOutputVerticesNV = */ 256,
	/* .maxMeshOutputPrimitivesNV = */ 512,
	/* .maxMeshWorkGroupSizeX_NV = */ 32,
	/* .maxMeshWorkGroupSizeY_NV = */ 1,
	/* .maxMeshWorkGroupSizeZ_NV = */ 1,
	/* .maxTaskWorkGroupSizeX_NV = */ 32,
	/* .maxTaskWorkGroupSizeY_NV = */ 1,
	/* .maxTaskWorkGroupSizeZ_NV = */ 1,
	/* .maxMeshViewCountNV = */ 4,

#if GLSLANG_VERSION_LESS_OR_EQUAL_TO(11, 0, 0)
	/* .maxDualSourceDrawBuffersEXT = */ 1,
#endif

	/* .limits = */ {
	/* .nonInductiveForLoops = */ 1,
	/* .whileLoops = */ 1,
	/* .doWhileLoops = */ 1,
	/* .generalUniformIndexing = */ 1,
	/* .generalAttributeMatrixVectorIndexing = */ 1,
	/* .generalVaryingIndexing = */ 1,
	/* .generalSamplerIndexing = */ 1,
	/* .generalVariableIndexing = */ 1,
	/* .generalConstantMatrixVectorIndexing = */ 1,
}
};

class _ShaderVkThreadPool
{
public:
	void StartThreads()
	{
		if (s_threads.empty())
		{
			// create thread pool
			m_shutdownThread.store(false);
			const uint32 threadCount = 2;
			for (uint32 i = 0; i < threadCount; ++i)
				s_threads.emplace_back(&_ShaderVkThreadPool::CompilerThreadFunc, this);
		}
	}

	~_ShaderVkThreadPool()
	{
		m_shutdownThread.store(true);
		for (uint32 i = 0; i < s_threads.size(); ++i)
			s_compilationQueueCount.increment();
		for (auto& it : s_threads)
			it.join();
		s_threads.clear();
	}

	void CompilerThreadFunc()
	{
		while (!m_shutdownThread.load(std::memory_order::relaxed))
		{
			s_compilationQueueCount.decrementWithWait();
			s_compilationQueueMutex.lock();
			if (s_compilationQueue.empty())
			{
				// queue empty again, shaders compiled synchronously via PreponeCompilation()
				s_compilationQueueMutex.unlock();
				continue;
			}
			RendererShaderVk* job = s_compilationQueue.front();
			s_compilationQueue.pop_front();
			// set compilation state
			cemu_assert_debug(job->m_compilationState.getValue() == RendererShaderVk::COMPILATION_STATE::QUEUED);
			job->m_compilationState.setValue(RendererShaderVk::COMPILATION_STATE::COMPILING);
			s_compilationQueueMutex.unlock();
			// compile
			job->CompileInternal(false);
			++g_compiled_shaders_async;
			// mark as compiled
			cemu_assert_debug(job->m_compilationState.getValue() == RendererShaderVk::COMPILATION_STATE::COMPILING);
			job->m_compilationState.setValue(RendererShaderVk::COMPILATION_STATE::DONE);
		}
	}

public:
	std::vector<std::thread> s_threads;

	std::deque<RendererShaderVk*> s_compilationQueue;
	CounterSemaphore s_compilationQueueCount;
	std::mutex s_compilationQueueMutex;

private:
	std::atomic<bool> m_shutdownThread;
}ShaderVkThreadPool;

RendererShaderVk::RendererShaderVk(ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader, const std::string& glslCode)
	: RendererShader(type, baseHash, auxHash, isGameShader, isGfxPackShader), m_glslCode(glslCode)
{
	// start async compilation
	ShaderVkThreadPool.s_compilationQueueMutex.lock();
	m_compilationState.setValue(COMPILATION_STATE::QUEUED);
	ShaderVkThreadPool.s_compilationQueue.push_back(this);
	ShaderVkThreadPool.s_compilationQueueCount.increment();
	ShaderVkThreadPool.StartThreads();
	ShaderVkThreadPool.s_compilationQueueMutex.unlock();
}

RendererShaderVk::~RendererShaderVk()
{
	VulkanRenderer::GetInstance()->destroyShader(this);
}

sint32 RendererShaderVk::GetUniformLocation(const char* name)
{
	cemu_assert_suspicious();
	return 0;
}

void RendererShaderVk::SetUniform1iv(sint32 location, void* data, sint32 count)
{
	cemu_assert_suspicious();
}

void RendererShaderVk::SetUniform2fv(sint32 location, void* data, sint32 count)
{
	cemu_assert_suspicious();
}

void RendererShaderVk::SetUniform4iv(sint32 location, void* data, sint32 count)
{
	cemu_assert_suspicious();
}

void RendererShaderVk::CreateVkShaderModule(std::span<uint32> spirvBuffer)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = spirvBuffer.size_bytes();
	createInfo.pCode = spirvBuffer.data();

	VulkanRenderer* vkr = (VulkanRenderer*)g_renderer.get();

	VkDevice m_device = vkr->GetLogicalDevice();

	VkResult result = vkCreateShaderModule(m_device, &createInfo, nullptr, &m_shader_module);
	if (result != VK_SUCCESS)
	{
		forceLog_printf("Vulkan: Shader error");
		throw std::runtime_error(fmt::format("Failed to create shader module: {}", result));
	}

	// set debug name
	if (vkr->IsDebugUtilsEnabled() && vkSetDebugUtilsObjectNameEXT)
	{
		VkDebugUtilsObjectNameInfoEXT objName{};
		objName.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		objName.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
		objName.pNext = nullptr;
		objName.objectHandle = (uint64_t)m_shader_module;
		auto objNameStr = fmt::format("shader_{:016x}_{:016x}", m_baseHash, m_auxHash);
		objName.pObjectName = objNameStr.c_str();
		vkSetDebugUtilsObjectNameEXT(vkr->GetLogicalDevice(), &objName);
	}
}

void RendererShaderVk::FinishCompilation()
{
	m_glslCode.clear();
	m_glslCode.shrink_to_fit();
}

void RendererShaderVk::CompileInternal(bool isRenderThread)
{
	// try to retrieve SPIR-V module from cache
	if (s_isLoadingShadersVk && (m_isGameShader && !m_isGfxPackShader) && s_spirvCache)
	{
		cemu_assert_debug(m_baseHash != 0);
		uint64 h1, h2;
		GenerateShaderPrecompiledCacheFilename(m_type, m_baseHash, m_auxHash, h1, h2);
		std::vector<uint8> cacheFileData;
		if (s_spirvCache->GetFile({ h1, h2 }, cacheFileData))
		{
			// generate shader from cached SPIR-V buffer
			CreateVkShaderModule(std::span<uint32>((uint32*)cacheFileData.data(), cacheFileData.size() / sizeof(uint32)));
			FinishCompilation();
			return;
		}
	}

	EShLanguage state;
	switch (GetType())
	{
	case ShaderType::kVertex:
		state = EShLangVertex;
		break;
	case ShaderType::kFragment:
		state = EShLangFragment;
		break;
	case ShaderType::kGeometry:
		state = EShLangGeometry;
		break;
	default:
		cemu_assert_debug(false);
	}

	glslang::TShader Shader(state);
	const char* cstr = m_glslCode.c_str();
	Shader.setStrings(&cstr, 1);
	Shader.setEnvInput(glslang::EShSourceGlsl, state, glslang::EShClientVulkan, 100);
	Shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetClientVersion::EShTargetVulkan_1_1);

	Shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetLanguageVersion::EShTargetSpv_1_3);

	TBuiltInResource Resources = DefaultTBuiltInResource;
	std::string PreprocessedGLSL;

	VulkanRenderer* vkr = (VulkanRenderer*)g_renderer.get();

	EShMessages messagesPreprocess;
	if (vkr->IsDebugUtilsEnabled() && vkSetDebugUtilsObjectNameEXT)
		messagesPreprocess = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgDebugInfo);
	else
		messagesPreprocess = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

	glslang::TShader::ForbidIncluder Includer;
	if (!Shader.preprocess(&Resources, 450, ENoProfile, false, false, messagesPreprocess, &PreprocessedGLSL, Includer))
	{
		cemuLog_log(LogType::Force, fmt::format("GLSL Preprocessing Failed For {:016x}_{:016x}: \"{}\"", m_baseHash, m_auxHash, Shader.getInfoLog()));
		FinishCompilation();
		return;
	}

	EShMessages messagesParseLink;
	if (vkr->IsDebugUtilsEnabled() && vkSetDebugUtilsObjectNameEXT)
		messagesParseLink = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgDebugInfo);
	else
		messagesParseLink = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

	const char* PreprocessedCStr = PreprocessedGLSL.c_str();
	Shader.setStrings(&PreprocessedCStr, 1);
	if (!Shader.parse(&Resources, 100, false, messagesParseLink))
	{
		cemuLog_log(LogType::Force, fmt::format("GLSL parsing failed for {:016x}_{:016x}: \"{}\"", m_baseHash, m_auxHash, Shader.getInfoLog()));
		cemu_assert_debug(false);
		FinishCompilation();
		return;
	}

	glslang::TProgram Program;
	Program.addShader(&Shader);

	if (!Program.link(messagesParseLink))
	{
		cemuLog_log(LogType::Force, fmt::format("GLSL linking failed for {:016x}_{:016x}: \"{}\"", m_baseHash, m_auxHash, Program.getInfoLog()));
		cemu_assert_debug(false);
		FinishCompilation();
		return;
	}

	if (!Program.mapIO())
	{
		cemuLog_log(LogType::Force, fmt::format("GLSL linking failed for {:016x}_{:016x}: \"{}\"", m_baseHash, m_auxHash, Program.getInfoLog()));
		FinishCompilation();
		return;
	}

	// temp storage for SPIR-V after translation 
	std::vector<uint32> spirvBuffer;
	spv::SpvBuildLogger logger;

	glslang::SpvOptions spvOptions;
	spvOptions.disableOptimizer = false;
	spvOptions.generateDebugInfo = (vkr->IsDebugUtilsEnabled() && vkSetDebugUtilsObjectNameEXT);
	spvOptions.validate = false;
	spvOptions.optimizeSize = true;

	//auto beginTime = benchmarkTimer_start();

	GlslangToSpv(*Program.getIntermediate(state), spirvBuffer, &logger, &spvOptions);

	//double timeDur = benchmarkTimer_stop(beginTime);
	//forceLogRemoveMe_printf("Shader GLSL-to-SPIRV compilation took %lfms Size %08x", timeDur, spirvBuffer.size()*4);
	
	if (s_spirvCache && m_isGameShader && m_isGfxPackShader == false)
	{
		uint64 h1, h2;
		GenerateShaderPrecompiledCacheFilename(m_type, m_baseHash, m_auxHash, h1, h2);
		s_spirvCache->AddFile({ h1, h2 }, (const uint8*)spirvBuffer.data(), spirvBuffer.size() * sizeof(uint32));
	}

	CreateVkShaderModule(spirvBuffer);

	// count compiled shader
	if (!s_isLoadingShadersVk)
	{
		if( m_isGameShader )
			++g_compiled_shaders_total;
	}

	FinishCompilation();
}

void RendererShaderVk::PreponeCompilation(bool isRenderThread)
{
	ShaderVkThreadPool.s_compilationQueueMutex.lock();
	bool isStillQueued = m_compilationState.hasState(COMPILATION_STATE::QUEUED);
	if (isStillQueued)
	{
		// remove from queue
		ShaderVkThreadPool.s_compilationQueue.erase(std::remove(ShaderVkThreadPool.s_compilationQueue.begin(), ShaderVkThreadPool.s_compilationQueue.end(), this), ShaderVkThreadPool.s_compilationQueue.end());
		m_compilationState.setValue(COMPILATION_STATE::COMPILING);
	}
	ShaderVkThreadPool.s_compilationQueueMutex.unlock();
	if (!isStillQueued)
	{
		m_compilationState.waitUntilValue(COMPILATION_STATE::DONE);
		--g_compiled_shaders_async; // compilation caused a stall so we don't consider this one async
		return;
	}
	else
	{
		// compile synchronously
		CompileInternal(isRenderThread);
		m_compilationState.setValue(COMPILATION_STATE::DONE);
	}
}

bool RendererShaderVk::IsCompiled()
{
	return m_compilationState.hasState(COMPILATION_STATE::DONE);
};

bool RendererShaderVk::WaitForCompiled()
{
	m_compilationState.waitUntilValue(COMPILATION_STATE::DONE);
	return true;
}

void RendererShaderVk::ShaderCacheLoading_begin(uint64 cacheTitleId)
{
	if (s_spirvCache)
	{
		delete s_spirvCache;
		s_spirvCache = nullptr;
	}
	uint32 spirvCacheMagic = GeneratePrecompiledCacheId();
	const std::string cacheFilename = fmt::format("{:016x}_spirv.bin", cacheTitleId);
	const std::wstring cachePath = ActiveSettings::GetPath("shaderCache/precompiled/{}", cacheFilename).generic_wstring();
	s_spirvCache = FileCache::Open(cachePath, true, spirvCacheMagic);
	if (s_spirvCache == nullptr)
		cemuLog_log(LogType::Force, "Unable to open SPIR-V cache {}", cacheFilename);
	s_isLoadingShadersVk = true;
}

void RendererShaderVk::ShaderCacheLoading_end()
{
	// keep g_spirvCache open since we will write to it while the game is running
	s_isLoadingShadersVk = false;
}