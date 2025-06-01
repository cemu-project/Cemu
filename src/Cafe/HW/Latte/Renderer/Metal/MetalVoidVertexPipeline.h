#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Metal/MTLLibrary.hpp"
#include "Metal/MTLRenderPipeline.hpp"

class MetalVoidVertexPipeline
{
public:
    MetalVoidVertexPipeline(class MetalRenderer* mtlRenderer, MTL::Library* library, const std::string& vertexFunctionName);
    ~MetalVoidVertexPipeline();

    MTL::RenderPipelineState* GetRenderPipelineState() const { return m_renderPipelineState; }

private:
    MTL::RenderPipelineState* m_renderPipelineState;
};
