#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "GX2.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"

#include "Cafe/OS/common/OSCommon.h"

#include "GX2_Command.h"
#include "GX2_Draw.h"

namespace GX2
{
	void GX2SetAttribBuffer(uint32 bufferIndex, uint32 sizeInBytes, uint32 stride, void* data)
	{
		GX2ReserveCmdSpace(9);
		MPTR physicalAddress = memory_virtualToPhysical(memory_getVirtualOffsetFromPointer(data));
		// write PM4 command
		gx2WriteGather_submit(
			pm4HeaderType3(IT_SET_RESOURCE, 8),
			0x8C0 + bufferIndex * 7,
			physicalAddress,
			sizeInBytes - 1, // size
			(stride & 0xFFFF) << 11, // stride
			0, // ukn
			0, // ukn
			0, // ukn
			0xC0000000); // ukn
	}

	void GX2DrawIndexedEx(GX2PrimitiveMode2 primitiveMode, uint32 count, GX2IndexType indexType, void* indexData, uint32 baseVertex, uint32 numInstances)
	{
		GX2ReserveCmdSpace(3 + 3 + 2 + 2 + 6);
		gx2WriteGather_submit(
			// IT_SET_CTL_CONST
			pm4HeaderType3(IT_SET_CTL_CONST, 2), 0,
			baseVertex,
			// IT_SET_CONFIG_REG
			pm4HeaderType3(IT_SET_CONFIG_REG, 2), Latte::REGADDR::VGT_PRIMITIVE_TYPE - 0x2000,
			(uint32)primitiveMode,
			// IT_INDEX_TYPE
			pm4HeaderType3(IT_INDEX_TYPE, 1),
			(uint32)indexType,
			// IT_NUM_INSTANCES
			pm4HeaderType3(IT_NUM_INSTANCES, 1),
			numInstances,
			// IT_DRAW_INDEX_2
			pm4HeaderType3(IT_DRAW_INDEX_2, 5) | 0x00000001,
			-1,
			memory_virtualToPhysical(memory_getVirtualOffsetFromPointer(indexData)),
			0,
			count,
			0);
		GX2::GX2WriteGather_checkAndInsertWrapAroundMark();
	}

	void GX2DrawIndexedEx2(GX2PrimitiveMode2 primitiveMode, uint32 count, GX2IndexType indexType, void* indexData, uint32 baseVertex, uint32 numInstances, uint32 baseInstance)
	{
		GX2ReserveCmdSpace(3 + 3 + 2 + 2 + 6);
		gx2WriteGather_submit(
			// IT_SET_CTL_CONST
			pm4HeaderType3(IT_SET_CTL_CONST, 2), 0,
			baseVertex,
			// set base instance
			pm4HeaderType3(IT_SET_CTL_CONST, 2), 1,
			baseInstance,
			// IT_SET_CONFIG_REG
			pm4HeaderType3(IT_SET_CONFIG_REG, 2), Latte::REGADDR::VGT_PRIMITIVE_TYPE - 0x2000,
			(uint32)primitiveMode,
			// IT_INDEX_TYPE
			pm4HeaderType3(IT_INDEX_TYPE, 1),
			(uint32)indexType,
			// IT_NUM_INSTANCES
			pm4HeaderType3(IT_NUM_INSTANCES, 1),
			numInstances,
			// IT_DRAW_INDEX_2
			pm4HeaderType3(IT_DRAW_INDEX_2, 5) | 0x00000001,
			-1,
			memory_virtualToPhysical(memory_getVirtualOffsetFromPointer(indexData)),
			0,
			count,
			0,
			// reset base instance
			pm4HeaderType3(IT_SET_CTL_CONST, 2), 1,
			0 // baseInstance
		);
		GX2::GX2WriteGather_checkAndInsertWrapAroundMark();
	}

	void GX2DrawEx(GX2PrimitiveMode2 primitiveMode, uint32 count, uint32 baseVertex, uint32 numInstances)
	{
		GX2ReserveCmdSpace(3 + 3 + 2 + 2 + 6);
		gx2WriteGather_submit(
			// IT_SET_CTL_CONST
			pm4HeaderType3(IT_SET_CTL_CONST, 2), 0,
			baseVertex,
			// IT_SET_CONFIG_REG
			pm4HeaderType3(IT_SET_CONFIG_REG, 2), Latte::REGADDR::VGT_PRIMITIVE_TYPE - 0x2000,
			(uint32)primitiveMode,
			// IT_INDEX_TYPE
			pm4HeaderType3(IT_INDEX_TYPE, 1),
			(uint32)GX2IndexType::U32_BE,
			// IT_NUM_INSTANCES
			pm4HeaderType3(IT_NUM_INSTANCES, 1),
			numInstances,
			// IT_DRAW_INDEX_2
			pm4HeaderType3(IT_DRAW_INDEX_AUTO, 2) | 0x00000001,
			count,
			0 // DRAW_INITIATOR
		);
		GX2::GX2WriteGather_checkAndInsertWrapAroundMark();
	}

	void GX2DrawIndexedImmediateEx(GX2PrimitiveMode2 primitiveMode, uint32 count, GX2IndexType indexType, void* indexData, uint32 baseVertex, uint32 numInstances)
	{
		uint32* indexDataU32 = (uint32*)indexData;
		uint32 numIndexU32s;
		bool use32BitIndices = false;
		if (indexType == GX2IndexType::U16_BE || indexType == GX2IndexType::U16_LE)
		{
			// 16bit indices
			numIndexU32s = (count + 1) / 2;
		}
		else if (indexType == GX2IndexType::U32_BE || indexType == GX2IndexType::U32_LE)
		{
			// 32bit indices
			numIndexU32s = count;
			use32BitIndices = true;
		}
		else
		{
			cemu_assert_unimplemented();
		}

		GX2ReserveCmdSpace(3 + 3 + 3 + 2 + 2 + 6 + 3 + numIndexU32s);

		if (numIndexU32s > 0x4000 - 2)
		{
			cemuLog_log(LogType::Force, "GX2DrawIndexedImmediateEx(): Draw exceeds maximum PM4 command size. Keep index size below 16KiB minus 8 byte");
			return;
		}

		// set base vertex
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CTL_CONST, 2));
		gx2WriteGather_submitU32AsBE(0);
		gx2WriteGather_submitU32AsBE(baseVertex);
		// set primitive mode
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONFIG_REG, 2));
		gx2WriteGather_submitU32AsBE(Latte::REGADDR::VGT_PRIMITIVE_TYPE - 0x2000);
		gx2WriteGather_submitU32AsBE((uint32)primitiveMode);
		// set index type
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_INDEX_TYPE, 1));
		gx2WriteGather_submitU32AsBE((uint32)indexType);
		// set number of instances
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_NUM_INSTANCES, 1));
		gx2WriteGather_submitU32AsBE((uint32)numInstances);
		// request indexed draw with indices embedded into command buffer
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_DRAW_INDEX_IMMD, 2 + numIndexU32s) | 0x00000001);
		gx2WriteGather_submitU32AsBE(count);
		gx2WriteGather_submitU32AsBE(0); // ukn
		if (use32BitIndices)
		{
			for (uint32 i = 0; i < numIndexU32s; i++)
			{
				gx2WriteGather_submitU32AsLE(indexDataU32[i]);
			}
		}
		else
		{
			for (uint32 i = 0; i < numIndexU32s; i++)
			{
				uint32 indexPair = indexDataU32[i];
				// swap index pair
				indexPair = (indexPair >> 16) | (indexPair << 16);
				gx2WriteGather_submitU32AsLE(indexPair);
			}
		}

		GX2::GX2WriteGather_checkAndInsertWrapAroundMark();
	}

	struct GX2DispatchComputeParam
	{
		/* +0x00 */ uint32be worksizeX;
		/* +0x04 */ uint32be worksizeY;
		/* +0x08 */ uint32be worksizeZ;
	};

	void GX2DispatchCompute(GX2DispatchComputeParam* dispatchParam)
	{
		GX2ReserveCmdSpace(9 + 10);

		gx2WriteGather_submit(pm4HeaderType3(IT_SET_RESOURCE, 8),
			(mmSQ_CS_DISPATCH_PARAMS - mmSQ_TEX_RESOURCE_WORD0),
			memory_virtualToPhysical(MEMPTR<GX2DispatchComputeParam>(dispatchParam).GetMPTR()),
			0xF,
			0x862000,
			1,
			0xABCD1234,
			0xABCD1234,
			0xC0000000);

		// IT_EVENT_WRITE with RST_VTX_CNT?

		// set primitive mode
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONFIG_REG, 2));
		gx2WriteGather_submitU32AsBE(Latte::REGADDR::VGT_PRIMITIVE_TYPE - 0x2000);
		gx2WriteGather_submitU32AsBE(1); // mode
		// set number of instances
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_NUM_INSTANCES, 1));
		gx2WriteGather_submitU32AsBE(1); // numInstances

		uint32 workCount = (uint32)dispatchParam->worksizeX * (uint32)dispatchParam->worksizeY * (uint32)dispatchParam->worksizeZ;

		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_DRAW_INDEX_AUTO, 2) | 0x00000001);
		gx2WriteGather_submitU32AsBE(workCount);
		gx2WriteGather_submitU32AsBE(0); // DRAW_INITIATOR (has source select for index generator + other unknown info)
	}

	void GX2DrawInit()
	{
		cafeExportRegister("gx2", GX2SetAttribBuffer, LogType::GX2);
		cafeExportRegister("gx2", GX2DrawIndexedEx, LogType::GX2);
		cafeExportRegister("gx2", GX2DrawIndexedEx2, LogType::GX2);
		cafeExportRegister("gx2", GX2DrawEx, LogType::GX2);
		cafeExportRegister("gx2", GX2DrawIndexedImmediateEx, LogType::GX2);
		cafeExportRegister("gx2", GX2DispatchCompute, LogType::GX2);
	}

}
