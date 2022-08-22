#pragma once

// basic resource flags
#define GX2_RESFLAG_USAGE_TEXTURE					(1<<0)
#define GX2_RESFLAG_USAGE_COLOR_BUFFER				(1<<1)
#define GX2_RESFLAG_USAGE_DEPTH_BUFFER				(1<<2)
#define GX2_RESFLAG_USAGE_SCAN_BUFFER				(1<<3)

// extended resource flags used by GX2R API
#define GX2R_RESFLAG_USAGE_VERTEX_BUFFER			(1<<4)
#define GX2R_RESFLAG_USAGE_INDEX_BUFFER				(1<<5)
#define GX2R_RESFLAG_USAGE_UNIFORM_BLOCK			(1<<6)
#define GX2R_RESFLAG_USAGE_SHADER_PROGRAM			(1<<7)
#define GX2R_RESFLAG_USAGE_STREAM_OUTPUT			(1<<8)
#define GX2R_RESFLAG_USAGE_DISPLAY_LIST				(1<<9)
#define GX2R_RESFLAG_USAGE_GS_RINGBUFFER			(1<<10)

#define GX2R_RESFLAG_USAGE_CPU_READ					(1<<11)
#define GX2R_RESFLAG_USAGE_CPU_WRITE				(1<<12)
#define GX2R_RESFLAG_USAGE_GPU_READ					(1<<13)
#define GX2R_RESFLAG_USAGE_GPU_WRITE				(1<<14)

#define GX2R_RESFLAG_USE_MEM1						(1<<17)

#define GX2R_RESFLAG_UKN_BIT_19						(1<<19)
#define GX2R_RESFLAG_UKN_BIT_20						(1<<20)
#define GX2R_RESFLAG_UKN_BIT_21						(1<<21)
#define GX2R_RESFLAG_UKN_BIT_22						(1<<22)
#define GX2R_RESFLAG_UKN_BIT_23						(1<<23)

#define GX2R_RESFLAG_ALLOCATED_BY_GX2R				(1<<29)
#define GX2R_RESFLAG_LOCKED							(1<<30)

struct GX2RBuffer
{
	/* +0x00 */ uint32be		resFlags;
	/* +0x04 */ uint32be		elementSize;
	/* +0x08 */ uint32be		elementCount;
	/* +0x0C */ MEMPTR<void>	ptr;

	uint32 GetSize() const
	{
		return (uint32)elementSize * (uint32)elementCount;
	}

	MPTR GetVirtualAddr() const
	{
		return ptr.GetMPTR();
	}

	void* GetPtr() const
	{
		return ptr.GetPtr();
	}
};

static_assert(sizeof(GX2RBuffer) == 0x10);

namespace GX2
{
	void GX2ResourceInit();

	void GX2RSetAllocator(MPTR funcAllocMPTR, MPTR funcFreeMPR);
};