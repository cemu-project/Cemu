#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"

#include "Cemu/FileCache/FileCache.h"
#include "config/ActiveSettings.h"
#include "Cemu/Logging/CemuLogging.h"
#include "Common/precompiled.h"
#include "GameProfile/GameProfile.h"
#include "util/helpers/helpers.h"

#define METAL_AIR_CACHE_NAME "Cemu_AIR_cache"
#define METAL_AIR_CACHE_PATH "/Volumes/" METAL_AIR_CACHE_NAME
#define METAL_AIR_CACHE_SIZE (512 * 1024 * 1024)
#define METAL_AIR_CACHE_BLOCK_COUNT (METAL_AIR_CACHE_SIZE / 512)

static bool s_isLoadingShadersMtl{false};
static std::atomic<bool> s_hasRAMFilesystem{false};
class FileCache* s_airCache{nullptr};

extern std::atomic_int g_compiled_shaders_total;
extern std::atomic_int g_compiled_shaders_async;

class ShaderMtlThreadPool
{
public:
	void StartThreads()
	{
		if (m_threadsActive.exchange(true))
			return;
		// create thread pool
		const uint32 threadCount = 2;
		for (uint32 i = 0; i < threadCount; ++i)
			s_threads.emplace_back(&ShaderMtlThreadPool::CompilerThreadFunc, this);
	}

	void StopThreads()
	{
		if (!m_threadsActive.exchange(false))
			return;
		for (uint32 i = 0; i < s_threads.size(); ++i)
			s_compilationQueueCount.increment();
		for (auto& it : s_threads)
			it.join();
		s_threads.clear();
	}

	~ShaderMtlThreadPool()
	{
		StopThreads();
	}

	void CompilerThreadFunc()
	{
		SetThreadName("mtlShaderComp");
		while (m_threadsActive.load(std::memory_order::relaxed))
		{
			s_compilationQueueCount.decrementWithWait();
			s_compilationQueueMutex.lock();
			if (s_compilationQueue.empty())
			{
				// queue empty again, shaders compiled synchronously via PreponeCompilation()
				s_compilationQueueMutex.unlock();
				continue;
			}
			RendererShaderMtl* job = s_compilationQueue.front();
			s_compilationQueue.pop_front();
			// set compilation state
			cemu_assert_debug(job->m_compilationState.getValue() == RendererShaderMtl::COMPILATION_STATE::QUEUED);
			job->m_compilationState.setValue(RendererShaderMtl::COMPILATION_STATE::COMPILING);
			s_compilationQueueMutex.unlock();
			// compile
			job->CompileInternal();
			if (job->ShouldCountCompilation())
			    ++g_compiled_shaders_async;
			// mark as compiled
			cemu_assert_debug(job->m_compilationState.getValue() == RendererShaderMtl::COMPILATION_STATE::COMPILING);
			job->m_compilationState.setValue(RendererShaderMtl::COMPILATION_STATE::DONE);
		}
	}

	bool HasThreadsRunning() const { return m_threadsActive; }

public:
	std::vector<std::thread> s_threads;

	std::deque<RendererShaderMtl*> s_compilationQueue;
	CounterSemaphore s_compilationQueueCount;
	std::mutex s_compilationQueueMutex;

private:
	std::atomic<bool> m_threadsActive;
} shaderMtlThreadPool;

// TODO: find out if it would be possible to cache compiled Metal shaders
void RendererShaderMtl::ShaderCacheLoading_begin(uint64 cacheTitleId)
{
    s_isLoadingShadersMtl = true;

    // Open AIR cache
    if (s_airCache)
	{
		delete s_airCache;
		s_airCache = nullptr;
	}
	uint32 airCacheMagic = GeneratePrecompiledCacheId();
	const std::string cacheFilename = fmt::format("{:016x}_air.bin", cacheTitleId);
	const fs::path cachePath = ActiveSettings::GetCachePath("shaderCache/precompiled/{}", cacheFilename);
	s_airCache = FileCache::Open(cachePath, true, airCacheMagic);
	if (!s_airCache)
		cemuLog_log(LogType::Force, "Unable to open AIR cache {}", cacheFilename);

    // Maximize shader compilation speed
    static_cast<MetalRenderer*>(g_renderer.get())->SetShouldMaximizeConcurrentCompilation(true);
}

void RendererShaderMtl::ShaderCacheLoading_end()
{
    s_isLoadingShadersMtl = false;

    // Close the AIR cache
    if (s_airCache)
    {
        delete s_airCache;
        s_airCache = nullptr;
    }

    // Close RAM filesystem
    if (s_hasRAMFilesystem)
    {
        executeCommand("diskutil eject {}", METAL_AIR_CACHE_PATH);
        s_hasRAMFilesystem = false;
    }

    // Reset shader compilation speed
    static_cast<MetalRenderer*>(g_renderer.get())->SetShouldMaximizeConcurrentCompilation(false);
}

void RendererShaderMtl::ShaderCacheLoading_Close()
{
    // Do nothing
}

void RendererShaderMtl::Initialize()
{
    shaderMtlThreadPool.StartThreads();
}

void RendererShaderMtl::Shutdown()
{
    shaderMtlThreadPool.StopThreads();
}

RendererShaderMtl::RendererShaderMtl(MetalRenderer* mtlRenderer, ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader, const std::string& mslCode)
	: RendererShader(type, baseHash, auxHash, isGameShader, isGfxPackShader), m_mtlr{mtlRenderer}, m_mslCode{mslCode}
{
	// start async compilation
	shaderMtlThreadPool.s_compilationQueueMutex.lock();
	m_compilationState.setValue(COMPILATION_STATE::QUEUED);
	shaderMtlThreadPool.s_compilationQueue.push_back(this);
	shaderMtlThreadPool.s_compilationQueueCount.increment();
	shaderMtlThreadPool.s_compilationQueueMutex.unlock();
	cemu_assert_debug(shaderMtlThreadPool.HasThreadsRunning()); // make sure .StartThreads() was called
}

RendererShaderMtl::~RendererShaderMtl()
{
    if (m_function)
        m_function->release();
}

void RendererShaderMtl::PreponeCompilation(bool isRenderThread)
{
	shaderMtlThreadPool.s_compilationQueueMutex.lock();
	bool isStillQueued = m_compilationState.hasState(COMPILATION_STATE::QUEUED);
	if (isStillQueued)
	{
		// remove from queue
		shaderMtlThreadPool.s_compilationQueue.erase(std::remove(shaderMtlThreadPool.s_compilationQueue.begin(), shaderMtlThreadPool.s_compilationQueue.end(), this), shaderMtlThreadPool.s_compilationQueue.end());
		m_compilationState.setValue(COMPILATION_STATE::COMPILING);
	}
	shaderMtlThreadPool.s_compilationQueueMutex.unlock();
	if (!isStillQueued)
	{
		m_compilationState.waitUntilValue(COMPILATION_STATE::DONE);
		if (ShouldCountCompilation())
		    --g_compiled_shaders_async; // compilation caused a stall so we don't consider this one async
		return;
	}
	else
	{
		// compile synchronously
		CompileInternal();
		m_compilationState.setValue(COMPILATION_STATE::DONE);
	}
}

bool RendererShaderMtl::IsCompiled()
{
	return m_compilationState.hasState(COMPILATION_STATE::DONE);
};

bool RendererShaderMtl::WaitForCompiled()
{
	m_compilationState.waitUntilValue(COMPILATION_STATE::DONE);
	return true;
}

bool RendererShaderMtl::ShouldCountCompilation() const
{
    return !s_isLoadingShadersMtl && m_isGameShader;
}

void RendererShaderMtl::CompileInternal()
{
    // First, try to retrieve the compiled shader from the AIR cache
    if (s_isLoadingShadersMtl && (m_isGameShader && !m_isGfxPackShader) && s_airCache)
    {
        cemu_assert_debug(m_baseHash != 0);
		uint64 h1, h2;
		GenerateShaderPrecompiledCacheFilename(m_type, m_baseHash, m_auxHash, h1, h2);
		std::vector<uint8> cacheFileData;
		if (s_airCache->GetFile({ h1, h2 }, cacheFileData))
		{
			CompileFromAIR(std::span<uint8>(cacheFileData.data(), cacheFileData.size()));
			FinishCompilation();
		}
		else
		{
		    // Ensure that RAM filesystem exists
			if (!s_hasRAMFilesystem)
            {
                s_hasRAMFilesystem = true;
                executeCommand("diskutil erasevolume HFS+ {} $(hdiutil attach -nomount ram://{})", METAL_AIR_CACHE_NAME, METAL_AIR_CACHE_BLOCK_COUNT);
            }

		    // The shader is not in the cache, compile it
			std::string filename = fmt::format("{}_{}", h1, h2);
			// TODO: store the source
			executeCommand("xcrun -sdk macosx metal -o {}.ir -c {}.metal", filename, filename);
			executeCommand("xcrun -sdk macosx metallib -o {}.metallib {}.ir", filename, filename);
			// TODO: clean up

			// Load from the newly Generated AIR
			// std::span<uint8> airData = ;
			//CompileFromAIR(std::span<uint8>((uint8*)cacheFileData.data(), cacheFileData.size() / sizeof(uint8)));
			FinishCompilation();

			// Store in the cache
			uint64 h1, h2;
			GenerateShaderPrecompiledCacheFilename(m_type, m_baseHash, m_auxHash, h1, h2);
			//s_airCache->AddFile({ h1, h2 }, airData.data(), airData.size());
		}

		return;
    }

    // Compile from source
    MTL::CompileOptions* options = MTL::CompileOptions::alloc()->init();
    // TODO: always disable fast math for problematic shaders
    if (g_current_game_profile->GetFastMath())
        options->setFastMathEnabled(true);
    if (g_current_game_profile->GetPositionInvariance())
        options->setPreserveInvariance(true);

    NS::Error* error = nullptr;
	MTL::Library* library = m_mtlr->GetDevice()->newLibrary(ToNSString(m_mslCode), options, &error);
	options->release();
	if (error)
    {
        cemuLog_log(LogType::Force, "failed to create library: {} -> {}", error->localizedDescription()->utf8String(), m_mslCode.c_str());
        FinishCompilation();
        return;
    }
    m_function = library->newFunction(ToNSString("main0"));
    library->release();

    FinishCompilation();

	// Count shader compilation
	if (ShouldCountCompilation())
	    g_compiled_shaders_total++;
}

void RendererShaderMtl::CompileFromAIR(std::span<uint8> data)
{
    // TODO: implement this
    printf("LOADING SHADER FROM AIR CACHE\n");
}

void RendererShaderMtl::FinishCompilation()
{
    m_mslCode.clear();
    m_mslCode.shrink_to_fit();
}
