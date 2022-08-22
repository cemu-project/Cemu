#pragma once
#include "GX2_Resource.h"

#define GX2_MAX_STREAMOUT_BUFFERS 4

namespace GX2
{
	struct GX2StreamOutBuffer
	{
		/* +0x00 */ uint32be size; // size of buffer (if dataPtr is not NULL)
		/* +0x04 */ MEMPTR<void> dataPtr;
		/* +0x08 */ uint32be vertexStride;
		/* +0x0C */ GX2RBuffer rBuffer; // if dataPtr is NULL, use this as the buffer and size
		/* +0x1C */ MEMPTR<void> ctxPtr; // stream out context
	};

	static_assert(sizeof(GX2StreamOutBuffer) == 0x20, "GX2StreamOutBuffer_t has invalid size");

	void GX2SetStreamOutBuffer(uint32 bufferIndex, GX2StreamOutBuffer* streamOutBuffer);
	void GX2StreamoutInit();
}