#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "GX2.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"
#include "Cafe/CafeSystem.h"
#include "GX2_Query.h"

#define LATTE_GC_NUM_RB							2
#define _QUERY_REG_COUNT						8 // each reg/result is 64bits, little endian

namespace GX2
{
	struct GX2Query
	{
		// 4*2 sets of uint64 results
		uint32 reg[_QUERY_REG_COUNT * 2];
	};

	static_assert(sizeof(GX2Query) == 0x40);

	void _BeginOcclusionQuery(GX2Query* queryInfo, bool isGPUQuery)
	{
		if (isGPUQuery)
		{
			uint64 titleId = CafeSystem::GetForegroundTitleId();
			if (titleId == 0x00050000101c4c00ULL || titleId == 0x00050000101c4d00 || titleId == 0x0005000010116100) // XCX EU, US, JPN
			{
				// in XCX queries are used to determine if certain objects are visible
				// if we are not setting the result fast enough and the query still holds a value of 0 (which is the default for GPU queries)
				// then XCX will not render affected objects, causing flicker
				// note: This is a very old workaround. It may no longer be necessary since the introduction of full sync. Investigate
				*(uint64*)(queryInfo->reg + 2) = 0x100000;
			}
			else
			{
				GX2ReserveCmdSpace(5 * _QUERY_REG_COUNT);
				MPTR queryInfoPhys = memory_virtualToPhysical(memory_getVirtualOffsetFromPointer(queryInfo));
				for (sint32 i = 0; i < _QUERY_REG_COUNT; i++)
				{
					gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_MEM_WRITE, 4));
					gx2WriteGather_submitU32AsBE((queryInfoPhys + i * 8) | 0x2);
					gx2WriteGather_submitU32AsBE(0x20000); // 0x20000 -> ?
					uint32 v = 0;
					if (i >= LATTE_GC_NUM_RB * 2)
						v |= 0x80000000;
					gx2WriteGather_submitU32AsBE(0);
					gx2WriteGather_submitU32AsBE(v);
				}
			}
		}
		else
		{
			memset(queryInfo, 0, 0x10); // size maybe GPU7_GC_NUM_RB*2*4 ?
			queryInfo->reg[LATTE_GC_NUM_RB * 4 + 0] = 0;
			queryInfo->reg[LATTE_GC_NUM_RB * 4 + 1] = _swapEndianU32('OCPU');
		}
		// todo: Set mmDB_RENDER_CONTROL
	}

	void GX2QueryBegin(uint32 queryType, GX2Query* query)
	{
		if (queryType == GX2_QUERY_TYPE_OCCLUSION_CPU)
		{
			_BeginOcclusionQuery(query, false);
		}
		else if (queryType == GX2_QUERY_TYPE_OCCLUSION_GPU)
		{
			_BeginOcclusionQuery(query, true);
		}
		else
		{
			debug_printf("GX2QueryBegin(): Unsupported type %d\n", queryType);
			debugBreakpoint();
			return;
		}
		// HLE packet
		GX2ReserveCmdSpace(2);
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_BEGIN_OCCLUSION_QUERY, 1));
		gx2WriteGather_submitU32AsBE(MEMPTR<GX2Query>(query).GetMPTR());
	}

	void GX2QueryEnd(uint32 queryType, GX2Query* query)
	{
		GX2ReserveCmdSpace(2);
		if (queryType == GX2_QUERY_TYPE_OCCLUSION_CPU || queryType == GX2_QUERY_TYPE_OCCLUSION_GPU)
		{
			// HLE packet
			gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_END_OCCLUSION_QUERY, 1));
			gx2WriteGather_submitU32AsBE(MEMPTR<GX2Query>(query).GetMPTR());
		}
		else
		{
			debug_printf("GX2QueryBegin(): Unsupported %d\n", queryType);
			debugBreakpoint();
			return;
		}
	}

	uint32 GX2QueryGetOcclusionResult(GX2Query* query, uint64be* resultOut)
	{
		if (query->reg[LATTE_GC_NUM_RB * 4 + 1] == _swapEndianU32('OCPU') && query->reg[LATTE_GC_NUM_RB * 4 + 0] == 0)
		{
			// CPU query result not ready
			return GX2_FALSE;
		}

		uint64 startValue = *(uint64*)(query->reg + 0);
		uint64 endValue = *(uint64*)(query->reg + 2);
		if ((startValue & 0x8000000000000000ULL) || (endValue & 0x8000000000000000ULL))
		{
			return GX2_FALSE;
		}
		*resultOut = endValue - startValue;
		return GX2_TRUE;
	}

	void GX2QueryBeginConditionalRender(uint32 queryType, GX2Query* query, uint32 dontWaitBool, uint32 pixelsMustPassBool)
	{
		GX2ReserveCmdSpace(3);

		uint32 flags = 0;
		if (pixelsMustPassBool)
			flags |= (1<<31);
		if (queryType == GX2_QUERY_TYPE_OCCLUSION_GPU)
			flags |= (1 << 13);
		else
			flags |= (2 << 13);

		flags |= ((dontWaitBool != 0) << 19);

		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_PREDICATION, 2));
		gx2WriteGather_submitU32AsBE(memory_virtualToPhysical(MEMPTR<GX2Query>(query).GetMPTR()));
		gx2WriteGather_submitU32AsBE(flags);
	}

	void GX2QueryEndConditionalRender()
	{
		GX2ReserveCmdSpace(3);

		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_PREDICATION, 2));
		gx2WriteGather_submitU32AsBE(MPTR_NULL);
		gx2WriteGather_submitU32AsBE(0); // unknown / todo
	}

	void GX2QueryInit()
	{
		cafeExportRegister("gx2", GX2QueryBegin, LogType::GX2);
		cafeExportRegister("gx2", GX2QueryEnd, LogType::GX2);
		cafeExportRegister("gx2", GX2QueryGetOcclusionResult, LogType::GX2);
		cafeExportRegister("gx2", GX2QueryBeginConditionalRender, LogType::GX2);
		cafeExportRegister("gx2", GX2QueryEndConditionalRender, LogType::GX2);
	}
};