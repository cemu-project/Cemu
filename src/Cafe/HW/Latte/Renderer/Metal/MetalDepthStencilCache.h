#pragma once

#include <Metal/Metal.hpp>

#include "HW/Latte/ISA/LatteReg.h"

class MetalDepthStencilCache
{
public:
    MetalDepthStencilCache(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}
    ~MetalDepthStencilCache();

    MTL::DepthStencilState* GetDepthStencilState(const LatteContextRegister& lcr);

private:
    class MetalRenderer* m_mtlr;

    std::map<uint64, MTL::DepthStencilState*> m_depthStencilCache;

    uint64 CalculateDepthStencilHash(const LatteContextRegister& lcr);
};
