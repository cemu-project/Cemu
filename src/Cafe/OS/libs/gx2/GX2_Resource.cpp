#include "Cafe/HW/Latte/Core/LatteBufferCache.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"
#include "Cafe/OS/common/OSCommon.h"
#include "GX2.h"
#include "GX2_Command.h"
#include "GX2_Resource.h"
#include "GX2_Streamout.h"
#include "GX2_Draw.h"

namespace GX2
{

	MPTR GX2RAllocateFunc = MPTR_NULL;
	MPTR GX2RFreeFunc = MPTR_NULL;

	void GX2RSetAllocator(MPTR funcAllocMPTR, MPTR funcFreeMPR)
	{
		GX2RAllocateFunc = funcAllocMPTR;
		GX2RFreeFunc = funcFreeMPR;
	}

	uint32 GX2RGetBufferAllocationSize(GX2RBuffer* buffer)
	{
		return (buffer->GetSize() + 0x3F) & ~0x3F; // pad to 64 byte alignment
	}

	uint32 GX2RGetBufferAlignment(uint32 resFlags)
	{
		if ((resFlags & GX2R_RESFLAG_USAGE_STREAM_OUTPUT) != 0)
			return 0x100;
		if ((resFlags & GX2R_RESFLAG_USAGE_UNIFORM_BLOCK) != 0)
			return 0x100;
		if ((resFlags & GX2R_RESFLAG_USAGE_SHADER_PROGRAM) != 0)
			return 0x100;
		if ((resFlags & GX2R_RESFLAG_USAGE_GS_RINGBUFFER) != 0)
			return 0x100;

		if ((resFlags & GX2R_RESFLAG_USAGE_VERTEX_BUFFER) != 0)
			return 0x40;
		if ((resFlags & GX2R_RESFLAG_USAGE_INDEX_BUFFER) != 0)
			return 0x40;
		if ((resFlags & GX2R_RESFLAG_USAGE_DISPLAY_LIST) != 0)
			return 0x40;

		return 0x100;
	}

	bool GX2RCreateBuffer(GX2RBuffer* buffer)
	{
		uint32 bufferAlignment = GX2RGetBufferAlignment(buffer->resFlags);
		uint32 bufferSize = GX2RGetBufferAllocationSize(buffer);
		MPTR allocResult = PPCCoreCallback(GX2RAllocateFunc, (uint32)buffer->resFlags, bufferSize, bufferAlignment);
		buffer->ptr = allocResult;
		buffer->resFlags &= ~GX2R_RESFLAG_LOCKED;
		buffer->resFlags |= GX2R_RESFLAG_ALLOCATED_BY_GX2R;
		// todo: invalidation
		return allocResult != MPTR_NULL;
	}

	bool GX2RCreateBufferUserMemory(GX2RBuffer* buffer, void* ptr, uint32 unusedSizeParameter)
	{
		buffer->ptr = ptr;
		buffer->resFlags &= ~GX2R_RESFLAG_LOCKED;
		buffer->resFlags &= ~GX2R_RESFLAG_ALLOCATED_BY_GX2R;
		// todo: invalidation
		return true;
	}

	void GX2RDestroyBufferEx(GX2RBuffer* buffer, uint32 resFlags)
	{
		if ((buffer->resFlags & GX2R_RESFLAG_ALLOCATED_BY_GX2R) == 0)
		{
			// this buffer is user-allocated
			buffer->ptr = nullptr;
			return;
		}
		PPCCoreCallback(GX2RFreeFunc, (uint32)buffer->resFlags, buffer->GetPtr());
		buffer->ptr = nullptr;
	}

	bool GX2RBufferExists(GX2RBuffer* buffer)
	{
		if (!buffer)
			return false;
		if (!buffer->GetPtr())
			return false;
		return true;
	}

	void* GX2RLockBufferEx(GX2RBuffer* buffer, uint32 resFlags)
	{
		return buffer->GetPtr();
	}

	void GX2RUnlockBufferEx(GX2RBuffer* buffer, uint32 resFlags)
	{
		// todo - account for flags, not all buffer types need flushing
		LatteBufferCache_notifyDCFlush(buffer->GetVirtualAddr(), buffer->GetSize());
	}

	void GX2RInvalidateBuffer(GX2RBuffer* buffer, uint32 resFlags)
	{
		// todo - account for flags, not all buffer types need flushing
		LatteBufferCache_notifyDCFlush(buffer->GetVirtualAddr(), buffer->GetSize());
	}

	void GX2RSetAttributeBuffer(GX2RBuffer* buffer, uint32 bufferIndex, uint32 stride, uint32 offset)
	{
		uint32 bufferSize = buffer->GetSize();
		if (offset > bufferSize)
			cemuLog_log(LogType::Force, "GX2RSetAttributeBuffer(): Offset exceeds buffer size");
		GX2SetAttribBuffer(bufferIndex, bufferSize - offset, stride, ((uint8be*)buffer->GetPtr()) + offset);
	}

	void GX2RSetStreamOutBuffer(uint32 bufferIndex, GX2StreamOutBuffer* soBuffer)
	{
		// seen in CoD: Ghosts and CoD: Black Ops 2
		GX2SetStreamOutBuffer(bufferIndex, soBuffer);
	}

	bool GX2RCreateSurface(GX2Surface* surface, uint32 resFlags)
	{
		// seen in Transformers Prime
		surface->resFlag = resFlags;
		GX2CalcSurfaceSizeAndAlignment(surface);
		surface->resFlag &= ~GX2R_RESFLAG_LOCKED;
		surface->resFlag |= GX2R_RESFLAG_ALLOCATED_BY_GX2R;
		MPTR allocResult = PPCCoreCallback(GX2RAllocateFunc, (uint32)surface->resFlag, (uint32)surface->imageSize + (uint32)surface->mipSize, (uint32)surface->alignment);
		surface->imagePtr = allocResult;
		if (surface->imagePtr != MPTR_NULL && surface->mipSize > 0)
		{
			surface->mipPtr = (uint32)surface->imagePtr + surface->imageSize;
		}
		else
		{
			surface->mipPtr = MPTR_NULL;
		}
		// todo: Cache invalidation based on resourceFlags?
		return allocResult != MPTR_NULL;
	}

	bool GX2RCreateSurfaceUserMemory(GX2Surface* surface, void* imagePtr, void* mipPtr, uint32 resFlags)
	{
		surface->resFlag = resFlags;
		surface->resFlag &= ~(GX2R_RESFLAG_LOCKED | GX2R_RESFLAG_ALLOCATED_BY_GX2R);
		GX2CalcSurfaceSizeAndAlignment(surface);
		surface->imagePtr = memory_getVirtualOffsetFromPointer(imagePtr);
		surface->mipPtr = memory_getVirtualOffsetFromPointer(mipPtr);
		if (surface->resFlag & 0x14000)
		{
			// memory invalidate
		}
		return true;
	}

	void GX2RDestroySurfaceEx(GX2Surface* surface, uint32 resFlags)
	{
		if ((surface->resFlag & GX2R_RESFLAG_ALLOCATED_BY_GX2R) == 0)
		{
			// this surface is user-allocated
			surface->imagePtr = MPTR_NULL;
			return;
		}
		resFlags &= (GX2R_RESFLAG_UKN_BIT_19 | GX2R_RESFLAG_UKN_BIT_20 | GX2R_RESFLAG_UKN_BIT_21 | GX2R_RESFLAG_UKN_BIT_22 | GX2R_RESFLAG_UKN_BIT_23);
		PPCCoreCallback(GX2RFreeFunc, (uint32)surface->resFlag | resFlags, (uint32)surface->imagePtr);
		surface->imagePtr = MPTR_NULL;
	}

	bool GX2RSurfaceExists(GX2Surface* surface)
	{
		if (!surface)
			return false;
		if (surface->imagePtr == MPTR_NULL)
			return false;
		if ((surface->resFlag & (GX2R_RESFLAG_USAGE_CPU_READ | GX2R_RESFLAG_USAGE_CPU_WRITE | GX2R_RESFLAG_USAGE_GPU_READ | GX2R_RESFLAG_USAGE_GPU_WRITE)) == 0)
			return false;
		return true;
	}

	void* GX2RLockSurfaceEx(GX2Surface* surface, uint32 mipLevel, uint32 resFlags)
	{
		// todo: handle invalidation
		surface->resFlag |= GX2R_RESFLAG_LOCKED;
		return memory_getPointerFromVirtualOffset(surface->imagePtr);
	}

	void GX2RUnlockSurfaceEx(GX2Surface* surface, uint32 mipLevel, uint32 resFlags)
	{
		// todo: handle invalidation
		surface->resFlag &= ~GX2R_RESFLAG_LOCKED;
	}

	void GX2RBeginDisplayListEx(GX2RBuffer* buffer, bool ukn, uint32 resFlags)
	{
		// todo: handle invalidation
		GX2::GX2BeginDisplayList(buffer->GetPtr(), buffer->GetSize());
	}

	uint32 GX2REndDisplayList(GX2RBuffer* buffer)
	{
		return GX2::GX2EndDisplayList(buffer->GetPtr());
	}

	void GX2RCallDisplayList(GX2RBuffer* buffer, uint32 size)
	{
		GX2::GX2CallDisplayList(buffer->GetVirtualAddr(), size);
	}

	void GX2RDirectCallDisplayList(GX2RBuffer* buffer, uint32 size)
	{
		GX2::GX2DirectCallDisplayList(buffer->GetPtr(), size);
	}

	void GX2RDrawIndexed(GX2PrimitiveMode2 primitiveMode, GX2RBuffer* indexBuffer, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, uint32 count, uint32 baseIndex, uint32 baseVertex, uint32 numInstances)
	{
		GX2DrawIndexedEx(primitiveMode, count, indexType, (uint8be*)indexBuffer->GetPtr() + (baseIndex * (uint32)indexBuffer->elementSize), baseVertex, numInstances);
	}

	void GX2ResourceInit()
	{
		cafeExportRegister("gx2", GX2RSetAllocator, LogType::GX2);
		cafeExportRegister("gx2", GX2RGetBufferAllocationSize, LogType::GX2);
		cafeExportRegister("gx2", GX2RGetBufferAlignment, LogType::GX2);

		cafeExportRegister("gx2", GX2RCreateBuffer, LogType::GX2);
		cafeExportRegister("gx2", GX2RCreateBufferUserMemory, LogType::GX2);
		cafeExportRegister("gx2", GX2RDestroyBufferEx, LogType::GX2);
		cafeExportRegister("gx2", GX2RBufferExists, LogType::GX2);
		cafeExportRegister("gx2", GX2RLockBufferEx, LogType::GX2);
		cafeExportRegister("gx2", GX2RUnlockBufferEx, LogType::GX2);
		cafeExportRegister("gx2", GX2RInvalidateBuffer, LogType::GX2);

		cafeExportRegister("gx2", GX2RSetAttributeBuffer, LogType::GX2);
		cafeExportRegister("gx2", GX2RSetStreamOutBuffer, LogType::GX2);

		cafeExportRegister("gx2", GX2RCreateSurface, LogType::GX2);
		cafeExportRegister("gx2", GX2RCreateSurfaceUserMemory, LogType::GX2);
		cafeExportRegister("gx2", GX2RDestroySurfaceEx, LogType::GX2);
		cafeExportRegister("gx2", GX2RSurfaceExists, LogType::GX2);
		cafeExportRegister("gx2", GX2RLockSurfaceEx, LogType::GX2);
		cafeExportRegister("gx2", GX2RUnlockSurfaceEx, LogType::GX2);

		cafeExportRegister("gx2", GX2RBeginDisplayListEx, LogType::GX2);
		cafeExportRegister("gx2", GX2REndDisplayList, LogType::GX2);
		cafeExportRegister("gx2", GX2RCallDisplayList, LogType::GX2);
		cafeExportRegister("gx2", GX2RDirectCallDisplayList, LogType::GX2);

		cafeExportRegister("gx2", GX2RDrawIndexed, LogType::GX2);

		GX2RAllocateFunc = MPTR_NULL;
		GX2RFreeFunc = MPTR_NULL;
	}
};