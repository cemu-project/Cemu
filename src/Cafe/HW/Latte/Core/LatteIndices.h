#pragma once
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/ISA/LatteReg.h"

void LatteIndices_invalidate(const void* memPtr, uint32 size);
void LatteIndices_invalidateAll();
void LatteIndices_decode(const void* indexData, LatteIndexType indexType, uint32 count, LattePrimitiveMode primitiveMode, uint32& indexMin, uint32& indexMax, Renderer::INDEX_TYPE& renderIndexType, uint32& outputCount, uint32& indexBufferOffset, uint32& indexBufferIndex);