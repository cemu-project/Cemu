#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"

constexpr uint8 METAL_SHADER_TYPE_COUNT = 6;
constexpr uint8 METAL_OUTPUT_SHADER_CACHE_SIZE = 2 * METAL_SHADER_TYPE_COUNT;

class MetalOutputShaderCache
{
public:
    MetalOutputShaderCache(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}
    ~MetalOutputShaderCache();

    MTL::RenderPipelineState* GetPipeline(RendererOutputShader* shader, uint8 shaderIndex, bool usesSRGB);

private:
    class MetalRenderer* m_mtlr;

    MTL::RenderPipelineState* m_cache[METAL_OUTPUT_SHADER_CACHE_SIZE] = {nullptr};
};
