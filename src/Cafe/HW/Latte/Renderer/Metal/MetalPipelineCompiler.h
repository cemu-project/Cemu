#pragma once

#include <Metal/Metal.hpp>

#include "Foundation/NSObject.hpp"
#include "HW/Latte/ISA/LatteReg.h"
#include "HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"

class MetalPipelineCompiler
{
public:
    MetalPipelineCompiler(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}
    ~MetalPipelineCompiler();

    void InitFromState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr);

    MTL::RenderPipelineState* Compile(bool forceCompile, bool isRenderThread);

private:
    class MetalRenderer* m_mtlr;

    bool m_usesGeometryShader;

    /*
    std::map<uint64, MTL::RenderPipelineState*> m_pipelineCache;

    NS::URL* m_binaryArchiveURL;
    MTL::BinaryArchive* m_binaryArchive;
    */
    NS::Object* m_pipelineDescriptor;

    void InitFromStateRender(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr);

    void InitFromStateMesh(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, class CachedFBOMtl* lastUsedFBO, class CachedFBOMtl* activeFBO, const LatteContextRegister& lcr);

    //void TryLoadBinaryArchive();
};
