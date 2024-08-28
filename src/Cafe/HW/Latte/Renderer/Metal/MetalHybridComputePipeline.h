#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Metal/MTLLibrary.hpp"
#include "Metal/MTLRenderPipeline.hpp"

// TODO: rename to MetalVoidVertexPipeline
class MetalHybridComputePipeline
{
public:
    MetalHybridComputePipeline(class MetalRenderer* mtlRenderer, MTL::Library* library, const std::string& vertexFunctionName/*, const std::string& kernelFunctionName*/);
    ~MetalHybridComputePipeline();

    MTL::RenderPipelineState* GetRenderPipelineState() const { return m_renderPipelineState; }

    //MTL::RenderPipelineState* GetComputePipelineState() const { return m_computePipelineState; }

private:
    MTL::RenderPipelineState* m_renderPipelineState;
    //MTL::RenderPipelineState* m_computePipelineState;
};
