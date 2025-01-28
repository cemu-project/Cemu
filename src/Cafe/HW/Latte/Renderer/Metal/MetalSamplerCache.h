#pragma once

#include <Metal/Metal.hpp>

#include "HW/Latte/Core/LatteConst.h"
#include "HW/Latte/ISA/LatteReg.h"

class MetalSamplerCache
{
public:
    MetalSamplerCache(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}
    ~MetalSamplerCache();

    MTL::SamplerState* GetSamplerState(const LatteContextRegister& lcr, LatteConst::ShaderType shaderType, uint32 stageSamplerIndex, const _LatteRegisterSetSampler* samplerWords);

private:
    class MetalRenderer* m_mtlr;

    std::map<uint64, MTL::SamplerState*> m_samplerCache;

    uint64 CalculateSamplerHash(const LatteContextRegister& lcr, LatteConst::ShaderType shaderType, uint32 stageSamplerIndex, const _LatteRegisterSetSampler* samplerWords);
};
