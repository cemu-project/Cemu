#pragma once

#include <Metal/Metal.hpp>

#include "HW/Latte/ISA/LatteReg.h"
#include "HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"

class MetalPipelineCache
{
public:
    static void ShaderCacheLoading_begin(uint64 cacheTitleId);
    static void ShaderCacheLoading_end();
    static void ShaderCacheLoading_Close();

    MetalPipelineCache(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}
    ~MetalPipelineCache();

    MTL::RenderPipelineState* GetPipelineState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr);

private:
    class MetalRenderer* m_mtlr;

    std::map<uint64, MTL::RenderPipelineState*> m_pipelineCache;

    NS::URL* m_binaryArchiveURL;
    MTL::BinaryArchive* m_binaryArchive;

    uint64 CalculatePipelineHash(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr);

    void TryLoadBinaryArchive();

    void LoadBinary(MTL::RenderPipelineDescriptor* desc);

    void SaveBinary(MTL::RenderPipelineDescriptor* desc);
};
