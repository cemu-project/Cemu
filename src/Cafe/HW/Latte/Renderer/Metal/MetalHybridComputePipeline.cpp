#include "Cafe/HW/Latte/Renderer/Metal/MetalHybridComputePipeline.h"

MetalHybridComputePipeline::MetalHybridComputePipeline(class MetalRenderer* mtlRenderer, MTL::Library* library, const char* vertexFunctionName, const char* kernelFunctionName)
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
        printf("error creating hybrid render pipeline state: %s\n", error->localizedDescription()->utf8String());
        error->release();
    }

    // Compute pipeline state
    // TODO
}

MetalHybridComputePipeline::~MetalHybridComputePipeline()
{
    m_renderPipelineState->release();
    // TODO: uncomment
    //m_computePipelineState->release();
}
