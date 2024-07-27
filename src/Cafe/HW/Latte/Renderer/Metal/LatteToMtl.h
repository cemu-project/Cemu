#pragma once

#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/ISA/LatteReg.h"

struct Uvec2 {
    uint32 x;
    uint32 y;
};

struct MtlPixelFormatInfo {
    MTL::PixelFormat pixelFormat;
    size_t bytesPerBlock;
    Uvec2 blockTexelSize = {1, 1};
};

const MtlPixelFormatInfo GetMtlPixelFormatInfo(Latte::E_GX2SURFFMT format);

size_t GetMtlTextureBytesPerRow(Latte::E_GX2SURFFMT format, uint32 width);

size_t GetMtlTextureBytesPerImage(Latte::E_GX2SURFFMT format, uint32 height, size_t bytesPerRow);
