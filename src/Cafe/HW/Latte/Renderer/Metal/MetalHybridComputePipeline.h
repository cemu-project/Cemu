#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Metal/MTLLibrary.hpp"
#include "Metal/MTLRenderPipeline.hpp"

class MetalHybridComputePipeline
{
public:
    MetalHybridComputePipeline(class MetalRenderer* mtlRenderer, MTL::Library* library, const char* vertexFunctionName, const char* kernelFunctionName);
    ~MetalHybridComputePipeline();

    MTL::RenderPipelineState* GetRenderPipelineState() const { return m_renderPipelineState; }

    MTL::RenderPipelineState* GetComputePipelineState() const { return m_computePipelineState; }

private:
    MTL::RenderPipelineState* m_renderPipelineState;
    MTL::RenderPipelineState* m_computePipelineState;
};
