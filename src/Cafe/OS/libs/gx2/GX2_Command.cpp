#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"
#include "Cafe/OS/libs/coreinit/coreinit.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "GX2.h"
#include "GX2_Command.h"
#include "GX2_Shader.h"
#include "GX2_Misc.h"

extern uint8* gxRingBufferReadPtr;

GX2WriteGatherPipeState gx2WriteGatherPipe = { };

void gx2WriteGather_submitU32AsBE(uint32 v)
{
	gx2WriteGatherPipe.accessData([&](GX2WriteGatherPipeStateData& data) {
		uint32 coreIndex = PPCInterpreter_getCoreIndex(PPCInterpreter_getCurrentInstance());
		if (data.writeGatherPtrWrite[coreIndex] == NULL)
			return;
		*(uint32*)(*data.writeGatherPtrWrite[coreIndex]) = _swapEndianU32(v);
		(*data.writeGatherPtrWrite[coreIndex]) += 4;
	});
}

void gx2WriteGather_submitU32AsLE(uint32 v)
{
	gx2WriteGatherPipe.accessData([&](GX2WriteGatherPipeStateData& data) {
		uint32 coreIndex = PPCInterpreter_getCoreIndex(PPCInterpreter_getCurrentInstance());
		if (data.writeGatherPtrWrite[coreIndex] == NULL)
			return;
		*(uint32*)(*data.writeGatherPtrWrite[coreIndex]) = v;
		(*data.writeGatherPtrWrite[coreIndex]) += 4;
	});
}

void gx2WriteGather_submitU32AsLEArray(uint32* v, uint32 numValues)
{
	gx2WriteGatherPipe.accessData([&](GX2WriteGatherPipeStateData& data) {
		uint32 coreIndex = PPCInterpreter_getCoreIndex(PPCInterpreter_getCurrentInstance());
		if (data.writeGatherPtrWrite[coreIndex] == NULL)
			return;
		memcpy_dwords((*data.writeGatherPtrWrite[coreIndex]), v, numValues);
		(*data.writeGatherPtrWrite[coreIndex]) += 4 * numValues;
	});
}

namespace GX2
{
	sint32 gx2WriteGatherCurrentMainCoreIndex = -1;
	bool gx2WriteGatherInited = false;

    void GX2WriteGather_ResetToDefaultState()
    {
        gx2WriteGatherCurrentMainCoreIndex = -1;
        gx2WriteGatherInited = false;
    }

	void GX2Init_writeGather() // init write gather, make current core 
	{
		gx2WriteGatherPipe.accessData([&](GX2WriteGatherPipeStateData& data) {
			if (data.gxRingBuffer == NULL)
				data.gxRingBuffer = (uint8*)malloc(GX2_COMMAND_RING_BUFFER_SIZE);
			if (gx2WriteGatherCurrentMainCoreIndex == sGX2MainCoreIndex)
				return; // write gather already configured for same core
			for (sint32 i = 0; i < PPC_CORE_COUNT; i++)
			{
				if (i == sGX2MainCoreIndex)
				{
					data.writeGatherPtrGxBuffer[i] = data.gxRingBuffer;
					data.writeGatherPtrWrite[i] = &data.writeGatherPtrGxBuffer[i];
				}
				else
				{
					data.writeGatherPtrGxBuffer[i] = NULL;
					data.writeGatherPtrWrite[i] = NULL;
				}
				data.displayListStart[i] = MPTR_NULL;
				data.writeGatherPtrDisplayList[i] = NULL;
				data.displayListMaxSize[i] = 0;
			}
			gx2WriteGatherCurrentMainCoreIndex = sGX2MainCoreIndex;
			gx2WriteGatherInited = true;
		});
	}

	void GX2WriteGather_beginDisplayList(PPCInterpreter_t* hCPU, MPTR buffer, uint32 maxSize)
	{
		gx2WriteGatherPipe.accessData([&](GX2WriteGatherPipeStateData& data) {
			uint32 coreIndex = PPCInterpreter_getCoreIndex(hCPU);
			data.displayListStart[coreIndex] = buffer;
			data.displayListMaxSize[coreIndex] = maxSize;
			// set new write gather ptr
			data.writeGatherPtrDisplayList[coreIndex] = memory_getPointerFromVirtualOffset(data.displayListStart[coreIndex]);
			data.writeGatherPtrWrite[coreIndex] = &data.writeGatherPtrDisplayList[coreIndex];
		});
	}

	uint32 GX2WriteGather_getDisplayListWriteDistance(sint32 coreIndex)
	{
		return gx2WriteGatherPipe.accessDataRet<uint32>([&](GX2WriteGatherPipeStateData& data) {
			return (uint32)(*data.writeGatherPtrWrite[coreIndex] - memory_getPointerFromVirtualOffset(data.displayListStart[coreIndex]));
		});
	}

	uint32 GX2WriteGather_getFifoWriteDistance(uint32 coreIndex)
	{
		return gx2WriteGatherPipe.accessDataRet<uint32>([&](GX2WriteGatherPipeStateData& data) {
			uint32 writeDistance = (uint32)(data.writeGatherPtrGxBuffer[coreIndex] - data.gxRingBuffer);
			return writeDistance;
		});
	}

	uint32 GX2WriteGather_endDisplayList(PPCInterpreter_t* hCPU, MPTR buffer)
	{
		return gx2WriteGatherPipe.accessDataRet<uint32>([&](GX2WriteGatherPipeStateData& data) {
			uint32 coreIndex = PPCInterpreter_getCoreIndex(hCPU);
			if (data.displayListStart[coreIndex] != MPTR_NULL)
			{
				uint32 currentWriteSize = GX2WriteGather_getDisplayListWriteDistance(coreIndex);
				// pad to 32 byte
				if (data.displayListMaxSize[coreIndex] >= ((data.displayListMaxSize[coreIndex] + 0x1F) & ~0x1F))
				{
					while ((currentWriteSize & 0x1F) != 0)
					{
						gx2WriteGather_submitU32AsBE(pm4HeaderType2Filler());
						currentWriteSize += 4;
					}
				}
				// get size of written data
				currentWriteSize = GX2WriteGather_getDisplayListWriteDistance(coreIndex);
				// disable current display list and restore write gather ptr
				data.displayListStart[coreIndex] = MPTR_NULL;
				if (sGX2MainCoreIndex == coreIndex)
					data.writeGatherPtrWrite[coreIndex] = &data.writeGatherPtrGxBuffer[coreIndex];
				else
					data.writeGatherPtrWrite[coreIndex] = NULL;
				// return size of (written) display list
				return currentWriteSize;
			}
			else
			{
				// no active display list
				// return a size of 0
				return 0u;
			}
		});
	}

	bool GX2GetCurrentDisplayList(betype<MPTR>* displayListAddr, uint32be* displayListSize)
	{
		return gx2WriteGatherPipe.accessDataRet<bool>([&](GX2WriteGatherPipeStateData& data) {
			uint32 coreIndex = coreinit::OSGetCoreId();
			if (data.displayListStart[coreIndex] == MPTR_NULL)
				return false;

			if (displayListAddr)
				*displayListAddr = data.displayListStart[coreIndex];
			if (displayListSize)
				*displayListSize = data.displayListMaxSize[coreIndex];

			return true;
		});
	}

	bool GX2GetDisplayListWriteStatus()
	{
		return gx2WriteGatherPipe.accessDataRet<bool>([&](GX2WriteGatherPipeStateData& data) {
			// returns true if we are writing to a display list
			uint32 coreIndex = coreinit::OSGetCoreId();
			return data.displayListStart[coreIndex] != MPTR_NULL;
		});
	}

	bool GX2WriteGather_isDisplayListActive()
	{
		return gx2WriteGatherPipe.accessDataRet<bool>([&](GX2WriteGatherPipeStateData& data) {
			uint32 coreIndex = coreinit::OSGetCoreId();
			if (data.displayListStart[coreIndex] != MPTR_NULL)
				return true;
			return false;
		});
	}

	uint32 GX2WriteGather_getReadWriteDistance()
	{
		return gx2WriteGatherPipe.accessDataRet<bool>([&](GX2WriteGatherPipeStateData& data) {
			uint32 coreIndex = sGX2MainCoreIndex;
			uint32 writeDistance = (uint32)(data.writeGatherPtrGxBuffer[coreIndex] + GX2_COMMAND_RING_BUFFER_SIZE - gxRingBufferReadPtr);
			writeDistance %= GX2_COMMAND_RING_BUFFER_SIZE;
			return writeDistance;
		});
	}

	void GX2WriteGather_checkAndInsertWrapAroundMark()
	{
		gx2WriteGatherPipe.accessData([&](GX2WriteGatherPipeStateData& data) {
			uint32 coreIndex = coreinit::OSGetCoreId();
			if (coreIndex != sGX2MainCoreIndex) // only if main gx2 core
				return;
			if (data.displayListStart[coreIndex] != MPTR_NULL)
				return;
			uint32 writeDistance = GX2WriteGather_getFifoWriteDistance(coreIndex);
			if (writeDistance >= (GX2_COMMAND_RING_BUFFER_SIZE * 3 / 5))
			{
				gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_FIFO_WRAP_AROUND, 1));
				gx2WriteGather_submitU32AsBE(0); // empty word since we can't send commands with zero data words
				data.writeGatherPtrGxBuffer[coreIndex] = data.gxRingBuffer;
			}
		});
	}

	void GX2BeginDisplayList(MEMPTR<void> displayListAddr, uint32 size)
	{
		GX2WriteGather_beginDisplayList(PPCInterpreter_getCurrentInstance(), displayListAddr.GetMPTR(), size);
	}

	void GX2BeginDisplayListEx(MEMPTR<void> displayListAddr, uint32 size, bool profiling)
	{
		GX2WriteGather_beginDisplayList(PPCInterpreter_getCurrentInstance(), displayListAddr.GetMPTR(), size);
	}

	uint32 GX2EndDisplayList(MEMPTR<void> displayListAddr)
	{
		cemu_assert_debug(displayListAddr != nullptr);
		uint32 displayListSize = GX2WriteGather_endDisplayList(PPCInterpreter_getCurrentInstance(), displayListAddr.GetMPTR());
		return displayListSize;
	}

	void GX2CallDisplayList(MPTR addr, uint32 size)
	{
		cemu_assert_debug((size&3) == 0);
		// write PM4 command
		GX2ReserveCmdSpace(4);
		gx2WriteGather_submit(pm4HeaderType3(IT_INDIRECT_BUFFER_PRIV, 3),
			memory_virtualToPhysical(addr),
			0, // high address bits
			size / 4);
		GX2::GX2WriteGather_checkAndInsertWrapAroundMark();
	}

	void GX2DirectCallDisplayList(void* addr, uint32 size)
	{
		gx2WriteGatherPipe.accessData([&](GX2WriteGatherPipeStateData& data) {
			// this API submits to TCL directly and bypasses write-gatherer
			// its basically a way to manually submit a command buffer to the GPU
			// as such it also affects the submission and retire timestamps

			uint32 coreIndex = PPCInterpreter_getCoreIndex(PPCInterpreter_getCurrentInstance());
			cemu_assert_debug(coreIndex == sGX2MainCoreIndex);
			coreIndex = sGX2MainCoreIndex; // always submit to main queue which is owned by GX2 main core (TCLSubmitToRing does not need this workaround)

			uint32be* cmdStream = (uint32be*)(data.writeGatherPtrGxBuffer[coreIndex]);
			cmdStream[0] = pm4HeaderType3(IT_INDIRECT_BUFFER_PRIV, 3);
			cmdStream[1] = memory_virtualToPhysical(MEMPTR<void>(addr).GetMPTR());
			cmdStream[2] = 0;
			cmdStream[3] = size / 4;
			data.writeGatherPtrGxBuffer[coreIndex] += 16;

			// update submission timestamp and retired timestamp
			_GX2SubmitToTCL();
		});
	}

	void GX2CopyDisplayList(MEMPTR<uint32be*> addr, uint32 size)
	{
		// copy display list to write gather
		uint32* displayListDWords = (uint32*)addr.GetPtr();
		uint32 dwordCount = size / 4;
		if (dwordCount > 0)
		{
			GX2ReserveCmdSpace(dwordCount);
			gx2WriteGather_submitU32AsLEArray(displayListDWords, dwordCount);
		}
	}

	enum class GX2_PATCH_TYPE : uint32
	{
		FETCH_SHADER			= 1,
		VERTEX_SHADER			= 2,
		GEOMETRY_COPY_SHADER	= 3,
		GEOMETRY_SHADER			= 4,
		PIXEL_SHADER			= 5,
		COMPUTE_SHADER			= 6
	};

	void GX2PatchDisplayList(uint32be* displayData, GX2_PATCH_TYPE patchType, uint32 patchOffset, void* obj)
	{
		cemu_assert_debug((patchOffset & 3) == 0);

		if (patchType == GX2_PATCH_TYPE::VERTEX_SHADER)
		{
			GX2VertexShader* vertexShader = (GX2VertexShader*)obj;
			displayData[patchOffset / 4 + 2] = memory_virtualToPhysical(vertexShader->GetProgramAddr()) >> 8;
		}
		else if (patchType == GX2_PATCH_TYPE::PIXEL_SHADER)
		{
			GX2PixelShader_t* pixelShader = (GX2PixelShader_t*)obj;
			displayData[patchOffset / 4 + 2] = memory_virtualToPhysical(pixelShader->GetProgramAddr()) >> 8;
		}
		else if (patchType == GX2_PATCH_TYPE::FETCH_SHADER)
		{
			GX2FetchShader* fetchShader = (GX2FetchShader*)obj;
			displayData[patchOffset / 4 + 2] = memory_virtualToPhysical(fetchShader->GetProgramAddr()) >> 8;
		}
		else if (patchType == GX2_PATCH_TYPE::GEOMETRY_COPY_SHADER)
		{
			GX2GeometryShader_t* geometryShader = (GX2GeometryShader_t*)obj;
			displayData[patchOffset / 4 + 2] = memory_virtualToPhysical(geometryShader->GetCopyProgramAddr()) >> 8;
		}
		else if (patchType == GX2_PATCH_TYPE::GEOMETRY_SHADER)
		{
			GX2GeometryShader_t* geometryShader = (GX2GeometryShader_t*)obj;
			displayData[patchOffset / 4 + 2] = memory_virtualToPhysical(geometryShader->GetGeometryProgramAddr()) >> 8;
		}
		else
		{
			cemuLog_log(LogType::Force, "GX2PatchDisplayList(): unsupported patchType {}", (uint32)patchType);
			cemu_assert_debug(false);
		}
	}

	void GX2CommandInit()
	{
		cafeExportRegister("gx2", GX2BeginDisplayList, LogType::GX2);
		cafeExportRegister("gx2", GX2BeginDisplayListEx, LogType::GX2);
		cafeExportRegister("gx2", GX2EndDisplayList, LogType::GX2);

		cafeExportRegister("gx2", GX2GetCurrentDisplayList, LogType::GX2);
		cafeExportRegister("gx2", GX2GetDisplayListWriteStatus, LogType::GX2);


		cafeExportRegister("gx2", GX2CallDisplayList, LogType::GX2);
		cafeExportRegister("gx2", GX2DirectCallDisplayList, LogType::GX2);
		cafeExportRegister("gx2", GX2CopyDisplayList, LogType::GX2);

		cafeExportRegister("gx2", GX2PatchDisplayList, LogType::GX2);
	}

    void GX2CommandResetToDefaultState()
    {
        GX2WriteGather_ResetToDefaultState();
    }

}
