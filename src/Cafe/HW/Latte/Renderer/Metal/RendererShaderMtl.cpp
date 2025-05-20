#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"

//#include "Cemu/FileCache/FileCache.h"
//#include "config/ActiveSettings.h"
#include "Cemu/Logging/CemuLogging.h"
#include "Common/precompiled.h"
#include "GameProfile/GameProfile.h"
#include "util/helpers/helpers.h"

#define METAL_AIR_CACHE_NAME "Cemu_AIR_cache"
#define METAL_AIR_CACHE_PATH "/Volumes/" METAL_AIR_CACHE_NAME
#define METAL_AIR_CACHE_SIZE (16 * 1024 * 1024)
#define METAL_AIR_CACHE_BLOCK_COUNT (METAL_AIR_CACHE_SIZE / 512)

static bool s_isLoadingShadersMtl{false};
//static bool s_hasRAMFilesystem{false};
//class FileCache* s_airCache{nullptr};

extern std::atomic_int g_compiled_shaders_total;
extern std::atomic_int g_compiled_shaders_async;

class ShaderMtlThreadPool
{
public:
	void StartThreads()
	{
		if (m_threadsActive.exchange(true))
			return;

		// Create thread pool
		const uint32 threadCount = 2;
		for (uint32 i = 0; i < threadCount; ++i)
			s_threads.emplace_back(&ShaderMtlThreadPool::CompilerThreadFunc, this);

		// Create AIR cache thread
		/*
	    s_airCacheThread = new std::thread(&ShaderMtlThreadPool::AIRCacheThreadFunc, this);

		// Set priority
		sched_param schedParam;
        schedParam.sched_priority = 20;
        if (pthread_setschedparam(s_airCacheThread->native_handle(), SCHED_FIFO, &schedParam) != 0) {
            cemuLog_log(LogType::Force, "failed to set FIFO thread priority");
        }

        if (pthread_setschedparam(s_airCacheThread->native_handle(), SCHED_RR, &schedParam) != 0) {
            cemuLog_log(LogType::Force, "failed to set RR thread priority");
        }
        */
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

		/*
		if (s_airCacheThread)
		{
            s_airCacheQueueCount.increment();
    		s_airCacheThread->join();
    		delete s_airCacheThread;
		}
		*/
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

	/*
	void AIRCacheThreadFunc()
    {
        SetThreadName("mtlAIRCache");
        while (m_threadsActive.load(std::memory_order::relaxed))
        {
            s_airCacheQueueCount.decrementWithWait();
            s_airCacheQueueMutex.lock();
            if (s_airCacheQueue.empty())
            {
                s_airCacheQueueMutex.unlock();
                continue;
            }

            // Create RAM filesystem
            if (!s_hasRAMFilesystem)
            {
                executeCommand("diskutil erasevolume HFS+ {} $(hdiutil attach -nomount ram://{})", METAL_AIR_CACHE_NAME, METAL_AIR_CACHE_BLOCK_COUNT);
                s_hasRAMFilesystem = true;
            }

            RendererShaderMtl* job = s_airCacheQueue.front();
            s_airCacheQueue.pop_front();
            s_airCacheQueueMutex.unlock();
            // compile
            job->CompileToAIR();
        }
    }
    */

	bool HasThreadsRunning() const { return m_threadsActive; }

public:
	std::vector<std::thread> s_threads;
	//std::thread* s_airCacheThread{nullptr};

	std::deque<RendererShaderMtl*> s_compilationQueue;
	CounterSemaphore s_compilationQueueCount;
	std::mutex s_compilationQueueMutex;

	/*
	std::deque<RendererShaderMtl*> s_airCacheQueue;
	CounterSemaphore s_airCacheQueueCount;
	std::mutex s_airCacheQueueMutex;
	*/

private:
	std::atomic<bool> m_threadsActive;
} shaderMtlThreadPool;

// TODO: find out if it would be possible to cache compiled Metal shaders
void RendererShaderMtl::ShaderCacheLoading_begin(uint64 cacheTitleId)
{
    s_isLoadingShadersMtl = true;

    // Open AIR cache
    /*
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
	*/

    // Maximize shader compilation speed
    static_cast<MetalRenderer*>(g_renderer.get())->SetShouldMaximizeConcurrentCompilation(true);
}

void RendererShaderMtl::ShaderCacheLoading_end()
{
    s_isLoadingShadersMtl = false;

    // Reset shader compilation speed
    static_cast<MetalRenderer*>(g_renderer.get())->SetShouldMaximizeConcurrentCompilation(false);
}

void RendererShaderMtl::ShaderCacheLoading_Close()
{
    // Close the AIR cache
    /*
    if (s_airCache)
    {
        delete s_airCache;
        s_airCache = nullptr;
    }

    // Close RAM filesystem
    if (s_hasRAMFilesystem)
        executeCommand("diskutil eject {}", METAL_AIR_CACHE_PATH);
    */
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

MTL::Library* RendererShaderMtl::LibraryFromSource()
{
    // Compile from source
    NS_STACK_SCOPED MTL::CompileOptions* options = MTL::CompileOptions::alloc()->init();
    if (g_current_game_profile->GetShaderFastMath())
        options->setFastMathEnabled(true);

    if (m_mtlr->GetPositionInvariance())
    {
        // TODO: filter out based on GPU state
        options->setPreserveInvariance(true);
    }

    NS::Error* error = nullptr;
	MTL::Library* library = m_mtlr->GetDevice()->newLibrary(ToNSString(m_mslCode), options, &error);
	if (error)
    {
        cemuLog_log(LogType::Force, "failed to create library from source: {} -> {}", error->localizedDescription()->utf8String(), m_mslCode.c_str());
        return nullptr;
    }

    return library;
}

/*
MTL::Library* RendererShaderMtl::LibraryFromAIR(std::span<uint8> data)
{
    dispatch_data_t dispatchData = dispatch_data_create(data.data(), data.size(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    NS::Error* error = nullptr;
	MTL::Library* library = m_mtlr->GetDevice()->newLibrary(dispatchData, &error);
	if (error)
    {
        cemuLog_log(LogType::Force, "failed to create library from AIR: {}", error->localizedDescription()->utf8String());
        return nullptr;
    }

    return library;
}
*/

void RendererShaderMtl::CompileInternal()
{
    MTL::Library* library = nullptr;

    // First, try to retrieve the compiled shader from the AIR cache
    /*
    if (s_isLoadingShadersMtl && (m_isGameShader && !m_isGfxPackShader) && s_airCache)
    {
        cemu_assert_debug(m_baseHash != 0);
		uint64 h1, h2;
		GenerateShaderPrecompiledCacheFilename(m_type, m_baseHash, m_auxHash, h1, h2);
		std::vector<uint8> cacheFileData;
		if (s_airCache->GetFile({ h1, h2 }, cacheFileData))
		{
			library = LibraryFromAIR(std::span<uint8>(cacheFileData.data(), cacheFileData.size()));
			FinishCompilation();
		}
    }
    */

    // Not in the cache, compile from source
    if (!library)
    {
        // Compile from source
        library = LibraryFromSource();
        FinishCompilation();
        if (!library)
            return;

        // Store in the AIR cache
        /*
        shaderMtlThreadPool.s_airCacheQueueMutex.lock();
        shaderMtlThreadPool.s_airCacheQueue.push_back(this);
        shaderMtlThreadPool.s_airCacheQueueCount.increment();
        shaderMtlThreadPool.s_airCacheQueueMutex.unlock();
        */
    }

    m_function = library->newFunction(ToNSString("main0"));
    library->release();

	// Count shader compilation
	if (ShouldCountCompilation())
	    g_compiled_shaders_total++;
}

/*
void RendererShaderMtl::CompileToAIR()
{
    uint64 h1, h2;
	GenerateShaderPrecompiledCacheFilename(m_type, m_baseHash, m_auxHash, h1, h2);

    // The shader is not in the cache, compile it
	std::string baseFilename = fmt::format("{}/{}_{}", METAL_AIR_CACHE_PATH, h1, h2);

	// Source
	std::ofstream mslFile;
    mslFile.open(fmt::format("{}.metal", baseFilename));
    mslFile << m_mslCode;
    mslFile.close();

    // Compile
	if (!executeCommand("xcrun -sdk macosx metal -o {}.ir -c {}.metal -w", baseFilename, baseFilename))
	    return;
	if (!executeCommand("xcrun -sdk macosx metallib -o {}.metallib {}.ir", baseFilename, baseFilename))
        return;

	// Clean up
	executeCommand("rm {}.metal", baseFilename);
	executeCommand("rm {}.ir", baseFilename);

	// Load from the newly generated AIR
	MemoryMappedFile airFile(fmt::format("{}.metallib", baseFilename));
	std::span<uint8> airData = std::span<uint8>(airFile.data(), airFile.size());
	//library = LibraryFromAIR(std::span<uint8>(airData.data(), airData.size()));

	// Store in the cache
	s_airCache->AddFile({ h1, h2 }, airData.data(), airData.size());

	// Clean up
	executeCommand("rm {}.metallib", baseFilename);

	FinishCompilation();
}
*/

void RendererShaderMtl::FinishCompilation()
{
    m_mslCode.clear();
    m_mslCode.shrink_to_fit();
}
