#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/GameProfile/GameProfile.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"

#include "util/containers/IntervalBucketContainer.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Core/LatteRingBuffer.h"
#include "Cafe/HW/Latte/Core/LatteBufferCache.h"

struct
{
	sint32 currentRingbufferOffset;
	VirtualBufferHeap_t* mainBufferHeap;
}streamoutManager;

sint32 LatteStreamout_GetRingBufferSize()
{
	return 8 * 1024 * 1024; // 8MB
}

sint32 LatteStreamout_allocateGPURingbufferMem(sint32 size)
{
	// pad size to 256 byte alignment
	size = (size + 255)&~255;
	// get next offset
	if ((streamoutManager.currentRingbufferOffset + size) > LatteStreamout_GetRingBufferSize())
	{
		streamoutManager.currentRingbufferOffset = 0;
	}
	sint32 allocOffset = streamoutManager.currentRingbufferOffset;
	streamoutManager.currentRingbufferOffset += size;
	return allocOffset;
}

void LatteStreamout_InitCache()
{
	streamoutManager.currentRingbufferOffset = 0;
	streamoutManager.mainBufferHeap = nullptr;
}

bool _transformFeedbackIsActive = false;
struct
{
	uint32 vertexCount;
	uint32 instanceCount;
	uint32 streamoutWriteMask;
	struct
	{
		bool isActive;
		sint32 ringBufferOffset;
		uint32 rangeAddr;
		uint32 rangeSize; // size of written streamout data, bounded by buffer size
	}streamoutBufferWrite[LATTE_NUM_STREAMOUT_BUFFER];
}activeStreamoutOperation;

uint32 LatteStreamout_getNumberOfWrittenVertices()
{
	// todo: Currently we only handle GX2_POINTS
	return activeStreamoutOperation.vertexCount * activeStreamoutOperation.instanceCount;
}

// returns the number of bytes that are written into the buffer by the current draw operation (ignoring buffer maximum size)
uint32 LatteStreamout_getBufferWriteRangeSize(uint32 streamoutBufferIndex)
{
	uint32 bufferStride = LatteGPUState.contextRegister[mmVGT_STRMOUT_VTX_STRIDE_0 + streamoutBufferIndex * 4] << 2;
	uint32 bufferSize = LatteGPUState.contextRegister[mmVGT_STRMOUT_BUFFER_SIZE_0 + streamoutBufferIndex * 4] << 2;
	uint32 writeSize = LatteStreamout_getNumberOfWrittenVertices() * bufferStride;
	if (bufferSize < writeSize)
		writeSize = bufferSize;
	return writeSize;
}

void LatteStreamout_PrepareDrawcall(uint32 count, uint32 instanceCount)
{
	if (LatteGPUState.contextRegister[mmVGT_STRMOUT_EN] == 0)
	{
		_transformFeedbackIsActive = false;
		return; // streamout inactive
	}
	// get active vertex shader
	LatteDecompilerShader* vertexShader = LatteSHRC_GetActiveVertexShader();
	// if a geometry shader is used calculate how many vertices it outputs
	LatteDecompilerShader* geometryShader = LatteSHRC_GetActiveGeometryShader();
	sint32 maxVerticesInGS = 1;
	if (geometryShader)
	{
		uint32 gsOutPrimType = LatteGPUState.contextRegister[mmVGT_GS_OUT_PRIM_TYPE];
		uint32 bytesPerVertex = LatteGPUState.contextRegister[mmSQ_GS_VERT_ITEMSIZE] * 4;
		maxVerticesInGS = ((LatteGPUState.contextRegister[mmSQ_GSVS_RING_ITEMSIZE] & 0x7FFF) * 4) / bytesPerVertex;
		cemu_assert_debug(maxVerticesInGS > 0);
	}
	// setup active streamout operation struct
	activeStreamoutOperation.vertexCount = count * maxVerticesInGS;
	activeStreamoutOperation.instanceCount = instanceCount;
	// get mask of all written streamout buffers
	uint32 streamoutWriteMask = 0;
	if (geometryShader)
	{
#ifdef CEMU_DEBUG_ASSERT
		cemu_assert_debug(vertexShader->streamoutBufferWriteMask.any() == false);
#endif
		for (sint32 i = 0; i < LATTE_NUM_STREAMOUT_BUFFER; i++)
			if (geometryShader->streamoutBufferWriteMask[i])
				streamoutWriteMask |= (1 << i);
	}
	else
	{
		for (sint32 i = 0; i < LATTE_NUM_STREAMOUT_BUFFER; i++)
			if (vertexShader->streamoutBufferWriteMask[i])
				streamoutWriteMask |= (1 << i);
	}
	activeStreamoutOperation.streamoutWriteMask = streamoutWriteMask;
	// bind streamout buffers
	for (uint32 i = 0; i < LATTE_NUM_STREAMOUT_BUFFER; i++)
	{
		if ((streamoutWriteMask&(1 << i)) == 0)
		{
			activeStreamoutOperation.streamoutBufferWrite[i].isActive = false;
			continue;
		}
		uint32 bufferBaseMPTR = LatteGPUState.contextRegister[mmVGT_STRMOUT_BUFFER_BASE_0 + i * 4] << 8;
		uint32 bufferSize = LatteGPUState.contextRegister[mmVGT_STRMOUT_BUFFER_SIZE_0 + i * 4] << 2;
		uint32 bufferOffset = LatteGPUState.contextRegister[mmVGT_STRMOUT_BUFFER_OFFSET_0 + i * 4];
		uint32 streamoutWriteSize = LatteStreamout_getBufferWriteRangeSize(i);
		uint32 rangeAddr = bufferBaseMPTR + bufferOffset;
		sint32 ringBufferOffset = LatteStreamout_allocateGPURingbufferMem(streamoutWriteSize); // allocate memory for the entire streamout write
		// calculate write size after bounding it to the buffer
		uint32 remainingBytesToWrite = bufferOffset > bufferSize ? 0 : (bufferSize - bufferOffset);
		uint32 rangeSize = std::min(streamoutWriteSize, remainingBytesToWrite);

		activeStreamoutOperation.streamoutBufferWrite[i].isActive = true;
		activeStreamoutOperation.streamoutBufferWrite[i].ringBufferOffset = ringBufferOffset;
		activeStreamoutOperation.streamoutBufferWrite[i].rangeAddr = rangeAddr;
		activeStreamoutOperation.streamoutBufferWrite[i].rangeSize = rangeSize;

		g_renderer->streamout_setupXfbBuffer(i, ringBufferOffset, rangeAddr, rangeSize);
	}
	g_renderer->streamout_begin();
	_transformFeedbackIsActive = true;
}

void LatteStreamout_FinishDrawcall(bool useDirectMemoryMode)
{
	if (_transformFeedbackIsActive)
	{
		_transformFeedbackIsActive = false;
		for (uint32 i = 0; i < LATTE_NUM_STREAMOUT_BUFFER; i++)
		{
			if ((activeStreamoutOperation.streamoutWriteMask&(1 << i)) == 0)
				continue;
			if (activeStreamoutOperation.streamoutBufferWrite[i].rangeSize > 0)
			{
				if(useDirectMemoryMode)
					g_renderer->bufferCache_copyStreamoutToMainBuffer(activeStreamoutOperation.streamoutBufferWrite[i].ringBufferOffset, activeStreamoutOperation.streamoutBufferWrite[i].rangeAddr, activeStreamoutOperation.streamoutBufferWrite[i].rangeSize);
				else
					LatteBufferCache_copyStreamoutDataToCache(activeStreamoutOperation.streamoutBufferWrite[i].rangeAddr, activeStreamoutOperation.streamoutBufferWrite[i].rangeSize, activeStreamoutOperation.streamoutBufferWrite[i].ringBufferOffset);
			}
			// advance streamout offset
			uint32 newOffset = LatteGPUState.contextRegister[mmVGT_STRMOUT_BUFFER_OFFSET_0 + i * 4] + activeStreamoutOperation.streamoutBufferWrite[i].rangeSize;
			LatteGPUState.contextRegister[mmVGT_STRMOUT_BUFFER_OFFSET_0 + i * 4] = newOffset;
		}
		g_renderer->streamout_rendererFinishDrawcall();
	}
}
