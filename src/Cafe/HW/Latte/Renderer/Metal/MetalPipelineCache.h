#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalPipelineCompiler.h"

// TODO: binary archives
class MetalPipelineCache
{
public:
    static uint64 CalculatePipelineHash(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr);

    MetalPipelineCache(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}
    ~MetalPipelineCache();

    MTL::RenderPipelineState* GetRenderPipelineState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr);

    // Debug
    size_t GetPipelineCacheSize() const { return m_pipelineCache.size(); }

private:
    class MetalRenderer* m_mtlr;

    std::map<uint64, MTL::RenderPipelineState*> m_pipelineCache;
};
