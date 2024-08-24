#pragma once

#include <QuartzCore/QuartzCore.hpp>

#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "QuartzCore/CAMetalDrawable.hpp"
#include "QuartzCore/CAMetalLayer.hpp"
#include "util/math/vector2.h"

class MetalLayerHandle
{
public:
    MetalLayerHandle() = default;
    MetalLayerHandle(MTL::Device* device, const Vector2i& size);

    ~MetalLayerHandle();

    void Resize(const Vector2i& size);

    bool AcquireDrawable();

    void CreateRenderPassDescriptor(bool clear);

    void PresentDrawable(MTL::CommandBuffer* commandBuffer);

    CA::MetalLayer* GetLayer() const { return m_layer; }

    CA::MetalDrawable* GetDrawable() const { return m_drawable; }

    MTL::RenderPassDescriptor* GetRenderPassDescriptor() const { return m_renderPassDescriptor; }

private:
    CA::MetalLayer* m_layer = nullptr;
    float m_layerScaleX, m_layerScaleY;

    CA::MetalDrawable* m_drawable = nullptr;
    MTL::RenderPassDescriptor* m_renderPassDescriptor = nullptr;
};
