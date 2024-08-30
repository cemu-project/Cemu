#pragma once

#include <Metal/Metal.hpp>

#include "HW/Latte/ISA/LatteReg.h"
#include "HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"

class MetalPipelineCache
{
public:
    static void ShaderCacheLoading_begin(uint64 cacheTitleId);
    static void ShaderCacheLoading_end();
    static void ShaderCacheLoading_Close();

    MetalPipelineCache(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}
    ~MetalPipelineCache();

    MTL::RenderPipelineState* GetRenderPipelineState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr);

    MTL::RenderPipelineState* GetMeshPipelineState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr, Renderer::INDEX_TYPE hostIndexType);

    // Debug
    size_t GetPipelineCacheSize() const { return m_pipelineCache.size(); }

private:
    class MetalRenderer* m_mtlr;

    std::map<uint64, MTL::RenderPipelineState*> m_pipelineCache;

    NS::URL* m_binaryArchiveURL;
    MTL::BinaryArchive* m_binaryArchive;

    uint64 CalculateRenderPipelineHash(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, const LatteContextRegister& lcr);

    void TryLoadBinaryArchive();
};
