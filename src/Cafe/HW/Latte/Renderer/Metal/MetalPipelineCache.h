#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalPipelineCompiler.h"
#include "util/helpers/ConcurrentQueue.h"
#include "util/helpers/fspinlock.h"
#include "util/math/vector2.h"

class MetalPipelineCache
{
public:
	static MetalPipelineCache& GetInstance();

    MetalPipelineCache(class MetalRenderer* metalRenderer);
    ~MetalPipelineCache();

    PipelineObject* GetRenderPipelineState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, const class MetalAttachmentsInfo& lastUsedAttachmentsInfo, const class MetalAttachmentsInfo& activeAttachmentsInfo, Vector2i extend, uint32 indexCount, const LatteContextRegister& lcr);

    // Cache loading
	uint32 BeginLoading(uint64 cacheTitleId); // returns count of pipelines stored in cache
	bool UpdateLoading(uint32& pipelinesLoadedTotal, uint32& pipelinesMissingShaders);
	void EndLoading();
	void LoadPipelineFromCache(std::span<uint8> fileData);
       void Close(); // called on title exit

    // Debug
    size_t GetPipelineCacheSize() const { return m_pipelineCache.size(); }

private:
    class MetalRenderer* m_mtlr;

    std::map<uint64, PipelineObject*> m_pipelineCache;
    FSpinlock m_pipelineCacheLock;

	std::thread* m_pipelineCacheStoreThread;

	class FileCache* s_cache;

	std::atomic_uint32_t m_numCompilationThreads{ 0 };
	ConcurrentQueue<std::vector<uint8>> m_compilationQueue;
	std::atomic_uint32_t m_compilationCount;

    static uint64 CalculatePipelineHash(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, const class MetalAttachmentsInfo& lastUsedAttachmentsInfo, const class MetalAttachmentsInfo& activeAttachmentsInfo, const LatteContextRegister& lcr);

    void AddCurrentStateToCache(uint64 pipelineStateHash, const class MetalAttachmentsInfo& lastUsedAttachmentsInfo);

	// pipeline serialization for file
	bool SerializePipeline(class MemStreamWriter& memWriter, struct CachedPipeline& cachedPipeline);
	bool DeserializePipeline(class MemStreamReader& memReader, struct CachedPipeline& cachedPipeline);

    int CompilerThread();
	void WorkerThread();
};
