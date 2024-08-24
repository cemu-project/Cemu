#include "Cafe/HW/Latte/Renderer/Metal/MetalLayerHandle.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalLayer.h"

#include "gui/guiWrapper.h"

MetalLayerHandle::MetalLayerHandle(MTL::Device* device, const Vector2i& size)
{
    const auto& windowInfo = gui_getWindowInfo().window_main;

    m_layer = (CA::MetalLayer*)CreateMetalLayer(windowInfo.handle, m_layerScaleX, m_layerScaleY);
    m_layer->setDevice(device);
    m_layer->setDrawableSize(CGSize{(float)size.x * m_layerScaleX, (float)size.y * m_layerScaleY});
}

MetalLayerHandle::~MetalLayerHandle()
{
    if (m_layer)
        m_layer->release();
    if (m_renderPassDescriptor)
        m_renderPassDescriptor->release();
}

void MetalLayerHandle::Resize(const Vector2i& size)
{
    m_layer->setDrawableSize(CGSize{(float)size.x * m_layerScaleX, (float)size.y * m_layerScaleY});
}

bool MetalLayerHandle::AcquireDrawable()
{
    if (m_drawable)
        return true;

    m_drawable = m_layer->nextDrawable();
    if (!m_drawable)
    {
        debug_printf("failed to acquire next drawable\n");
        return false;
    }

    if (m_renderPassDescriptor)
    {
        m_renderPassDescriptor->release();
        m_renderPassDescriptor = nullptr;
    }

    return true;
}

void MetalLayerHandle::CreateRenderPassDescriptor(bool clear)
{
    if (m_renderPassDescriptor)
        m_renderPassDescriptor->release();

    m_renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    auto colorAttachment = m_renderPassDescriptor->colorAttachments()->object(0);
    colorAttachment->setTexture(m_drawable->texture());
    colorAttachment->setLoadAction(clear ? MTL::LoadActionClear : MTL::LoadActionLoad);
    colorAttachment->setStoreAction(MTL::StoreActionStore);
}

void MetalLayerHandle::PresentDrawable(MTL::CommandBuffer* commandBuffer)
{
    commandBuffer->presentDrawable(m_drawable);
    m_drawable = nullptr;
}
