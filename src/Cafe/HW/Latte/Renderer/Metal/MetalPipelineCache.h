#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalPipelineCompiler.h"
#include "util/helpers/ConcurrentQueue.h"
#include "util/helpers/fspinlock.h"

// TODO: binary archives
class MetalPipelineCache
{
public:
    struct PipelineHash
	{
		PipelineHash(uint64 h0, uint64 h1) : h0(h0), h1(h1) {};

		uint64 h0;
		uint64 h1;

		bool operator==(const PipelineHash& r) const
		{
			return h0 == r.h0 && h1 == r.h1;
		}

		struct HashFunc
		{
			size_t operator()(const PipelineHash& v) const
			{
				static_assert(sizeof(uint64) == sizeof(size_t));
				return v.h0 ^ v.h1;
			}
		};
	};

    MetalPipelineCache(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}
    ~MetalPipelineCache();

    MTL::RenderPipelineState* GetRenderPipelineState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr);

    // Cache loading
	uint32 BeginLoading(uint64 cacheTitleId); // returns count of pipelines stored in cache
	bool UpdateLoading(uint32& pipelinesLoadedTotal, uint32& pipelinesMissingShaders);
	void EndLoading();
	void LoadPipelineFromCache(std::span<uint8> fileData);
       void Close(); // called on title exit

	bool HasPipelineCached(uint64 baseHash, uint64 pipelineStateHash);
	void AddCurrentStateToCache(uint64 baseHash, uint64 pipelineStateHash);

	// pipeline serialization for file
	bool SerializePipeline(class MemStreamWriter& memWriter, struct CachedPipeline& cachedPipeline);
	bool DeserializePipeline(class MemStreamReader& memReader, struct CachedPipeline& cachedPipeline);

    // Debug
    size_t GetPipelineCacheSize() const { return m_pipelineCache.size(); }

private:
    class MetalRenderer* m_mtlr;

    std::map<uint64, MTL::RenderPipelineState*> m_pipelineCache;

	std::thread* m_pipelineCacheStoreThread;

	std::unordered_set<PipelineHash, PipelineHash::HashFunc> m_pipelineIsCached;
	FSpinlock m_pipelineIsCachedLock;
	class FileCache* s_cache;

	std::atomic_uint32_t m_numCompilationThreads{ 0 };
	ConcurrentQueue<std::vector<uint8>> m_compilationQueue;
	std::atomic_uint32_t m_compilationCount;

    static uint64 CalculatePipelineHash(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr);

    int CompilerThread();
	void WorkerThread();
};
