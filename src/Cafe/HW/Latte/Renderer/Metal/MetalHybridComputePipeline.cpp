#include "Cafe/HW/Latte/Renderer/Metal/MetalHybridComputePipeline.h"

MetalHybridComputePipeline::MetalHybridComputePipeline(class MetalRenderer* mtlRenderer, MTL::Library* library, const std::string& vertexFunctionName/*, const std::string& kernelFunctionName*/)
{
    // Render pipeline state
    MTL::Function* vertexFunction = library->newFunction(ToNSString(vertexFunctionName));

    MTL::RenderPipelineDescriptor* renderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    renderPipelineDescriptor->setVertexFunction(vertexFunction);
    renderPipelineDescriptor->setRasterizationEnabled(false);

    NS::Error* error = nullptr;
    m_renderPipelineState = mtlRenderer->GetDevice()->newRenderPipelineState(renderPipelineDescriptor, &error);
    renderPipelineDescriptor->release();
    vertexFunction->release();
    if (error)
    {
        cemuLog_log(LogType::Force, "error creating hybrid render pipeline state: {}", error->localizedDescription()->utf8String());
        error->release();
    }

    // Compute pipeline state
}

MetalHybridComputePipeline::~MetalHybridComputePipeline()
{
    m_renderPipelineState->release();
    //m_computePipelineState->release();
}
