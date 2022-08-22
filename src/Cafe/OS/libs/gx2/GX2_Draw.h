#pragma once
#include "Cafe/HW/Latte/ISA/LatteReg.h"

namespace GX2
{
	using GX2IndexType = Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE;
	using GX2PrimitiveMode2 = Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE;

	void GX2SetAttribBuffer(uint32 bufferIndex, uint32 sizeInBytes, uint32 stride, void* data);
	void GX2DrawIndexedEx(GX2PrimitiveMode2 primitiveMode, uint32 count, GX2IndexType indexType, void* indexData, uint32 baseVertex, uint32 numInstances);

	void GX2DrawInit();
}