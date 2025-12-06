#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"

class MetalAttachmentsInfo
{
public:
    MetalAttachmentsInfo() = default;
    MetalAttachmentsInfo(class CachedFBOMtl* fbo);
    MetalAttachmentsInfo(const LatteContextRegister& lcr, const class LatteDecompilerShader* pixelShader);

    Latte::E_GX2SURFFMT colorFormats[LATTE_NUM_COLOR_TARGET] = {Latte::E_GX2SURFFMT::INVALID_FORMAT};
    Latte::E_GX2SURFFMT depthFormat = Latte::E_GX2SURFFMT::INVALID_FORMAT;
    bool hasStencil = false;
};
