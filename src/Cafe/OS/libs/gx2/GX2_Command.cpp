#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"
#include "Cafe/OS/libs/coreinit/coreinit.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/libs/TCL/TCL.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "GX2.h"
#include "GX2_Command.h"
#include "GX2_Shader.h"
#include "GX2_Misc.h"
#include "OS/libs/coreinit/coreinit_MEM.h"

namespace GX2
{
	GX2PerCoreCBState s_perCoreCBState[Espresso::CORE_COUNT];
}

void gx2WriteGather_submitU32AsBE(uint32 v)
{
	uint32 coreIndex = PPCInterpreter_getCoreIndex(PPCInterpreter_getCurrentInstance());
	if (GX2::s_perCoreCBState[coreIndex].currentWritePtr == nullptr)
		return;
	*(uint32*)(GX2::s_perCoreCBState[coreIndex].currentWritePtr) = _swapEndianU32(v);
	GX2::s_perCoreCBState[coreIndex].currentWritePtr++;
	cemu_assert_debug(GX2::s_perCoreCBState[coreIndex].currentWritePtr <= (GX2::s_perCoreCBState[coreIndex].bufferPtr + GX2::s_perCoreCBState[coreIndex].bufferSizeInU32s));
}

void gx2WriteGather_submitU32AsLE(uint32 v)
{
	uint32 coreIndex = PPCInterpreter_getCoreIndex(PPCInterpreter_getCurrentInstance());
	if (GX2::s_perCoreCBState[coreIndex].currentWritePtr == nullptr)
		return;
	*(uint32*)(GX2::s_perCoreCBState[coreIndex].currentWritePtr) = v;
	GX2::s_perCoreCBState[coreIndex].currentWritePtr++;
	cemu_assert_debug(GX2::s_perCoreCBState[coreIndex].currentWritePtr <= (GX2::s_perCoreCBState[coreIndex].bufferPtr + GX2::s_perCoreCBState[coreIndex].bufferSizeInU32s));
}

void gx2WriteGather_submitU32AsLEArray(uint32* v, uint32 numValues)
{
	uint32 coreIndex = PPCInterpreter_getCoreIndex(PPCInterpreter_getCurrentInstance());
	if (GX2::s_perCoreCBState[coreIndex].currentWritePtr == nullptr)
		return;
	memcpy_dwords(GX2::s_perCoreCBState[coreIndex].currentWritePtr, v, numValues);
	GX2::s_perCoreCBState[coreIndex].currentWritePtr += numValues;
	cemu_assert_debug(GX2::s_perCoreCBState[coreIndex].currentWritePtr <= (GX2::s_perCoreCBState[coreIndex].bufferPtr + GX2::s_perCoreCBState[coreIndex].bufferSizeInU32s));
}

namespace GX2
{

	struct GX2CommandState // mapped to PPC space since the GPU writes here
    {
    	// command pool
		MEMPTR<uint32be> commandPoolBase;
    	uint32 commandPoolSizeInU32s;
		MEMPTR<uint32be> gpuCommandReadPtr;
		// timestamp
		uint64be lastSubmissionTime;
    };

	SysAllocator<GX2CommandState> s_commandState;
	GX2PerCoreCBState s_mainCoreLastCommandState;
	bool s_cbBufferIsInternallyAllocated;

	void GX2Command_StartNewCommandBuffer(uint32 numU32s);

	// called from GX2Init. Allocates a 4MB memory chunk from which command buffers are suballocated from
	void GX2Init_commandBufferPool(void* bufferBase, uint32 bufferSize)
	{
		cemu_assert_debug(!s_commandState->commandPoolBase); // should not be allocated already
    	// setup command buffer pool. If not provided allocate a 4MB or custom size buffer
		uint32 poolSize = bufferSize ? bufferSize : 0x400000; // 4MB (can be overwritten by custom GX2Init parameters?)
		if (bufferBase)
		{
			s_commandState->commandPoolBase = (uint32be*)bufferBase;
			s_cbBufferIsInternallyAllocated = false;
		}
		else
		{
			s_commandState->commandPoolBase = (uint32be*)coreinit::_weak_MEMAllocFromDefaultHeapEx(poolSize, 0x100);
			s_cbBufferIsInternallyAllocated = true;
		}
		if (!s_commandState->commandPoolBase)
		{
			cemuLog_log(LogType::Force, "GX2: Failed to allocate command buffer pool");
		}
		s_commandState->commandPoolSizeInU32s = poolSize / sizeof(uint32be);
		s_commandState->gpuCommandReadPtr = s_commandState->commandPoolBase;
		// init per-core command buffer state
		for (uint32 i = 0; i < Espresso::CORE_COUNT; i++)
		{
			s_perCoreCBState[i].bufferPtr = nullptr;
			s_perCoreCBState[i].bufferSizeInU32s = 0;
			s_perCoreCBState[i].currentWritePtr = nullptr;
		}
		// start first command buffer for main core
		GX2Command_StartNewCommandBuffer(0x100);
	}

	void GX2Shutdown_commandBufferPool()
	{
		if (!s_commandState->commandPoolBase)
			return;
		if (s_cbBufferIsInternallyAllocated)
			coreinit::_weak_MEMFreeToDefaultHeap(s_commandState->commandPoolBase.GetPtr());
		s_cbBufferIsInternallyAllocated = false;
		s_commandState->commandPoolBase = nullptr;
		s_commandState->commandPoolSizeInU32s = 0;
		s_commandState->gpuCommandReadPtr = nullptr;
	}

	// current position of where the GPU is reading from. Updated via a memory write command submitted to the GPU
	uint32 GX2Command_GetPoolGPUReadIndex()
	{
		stdx::atomic_ref<MEMPTR<uint32be>> _readPtr(s_commandState->gpuCommandReadPtr);
		MEMPTR<uint32be> currentReadPtr = _readPtr.load();
		cemu_assert_debug(currentReadPtr);
		return (uint32)(currentReadPtr.GetPtr() - s_commandState->commandPoolBase.GetPtr());
	}

	void GX2Command_WaitForNextBufferRetired()
	{
		uint64 retiredTimeStamp = GX2GetRetiredTimeStamp();
		retiredTimeStamp += 1;
		// but cant be higher than the submission timestamp
		stdx::atomic_ref<uint64be> _lastSubmissionTime(s_commandState->lastSubmissionTime);
		uint64 submissionTimeStamp = _lastSubmissionTime.load();
		if (retiredTimeStamp > submissionTimeStamp)
			retiredTimeStamp = submissionTimeStamp;
		GX2WaitTimeStamp(retiredTimeStamp);
	}

	void GX2Command_SetupCoreCommandBuffer(uint32be* buffer, uint32 sizeInU32s, bool isDisplayList)
	{
		uint32 coreIndex = coreinit::OSGetCoreId();
		auto& coreCBState = s_perCoreCBState[coreIndex];
		coreCBState.bufferPtr = buffer;
		coreCBState.bufferSizeInU32s = sizeInU32s;
		coreCBState.currentWritePtr = buffer;
		coreCBState.isDisplayList = isDisplayList;
	}

	void GX2Command_StartNewCommandBuffer(uint32 numU32s)
	{
		uint32 coreIndex = coreinit::OSGetCoreId();
		auto& coreCBState = s_perCoreCBState[coreIndex];
		numU32s = std::max<uint32>(numU32s, 0x100);
		// grab space from command buffer pool and if necessary wait for it
		uint32be* bufferPtr = nullptr;
		uint32 bufferSizeInU32s = 0;
		uint32 readIndex;
		while (true)
		{
			// try to grab buffer data from first available spot:
			// 1. At the current write location up to the end of the buffer (avoiding an overlap with the read location)
			// 2. From the start of the buffer up to the read location
			readIndex = GX2Command_GetPoolGPUReadIndex();
			uint32be* nextWritePos = coreCBState.bufferPtr ? coreCBState.bufferPtr + coreCBState.bufferSizeInU32s : s_commandState->commandPoolBase.GetPtr();
			uint32 writeIndex = nextWritePos - s_commandState->commandPoolBase;
			uint32 poolSizeInU32s = s_commandState->commandPoolSizeInU32s;
			// readIndex == writeIndex can mean either buffer full or buffer empty
			// we could use GX2GetRetiredTimeStamp() == GX2GetLastSubmittedTimeStamp() to determine if the buffer is truly empty
			// but this can have false negatives since the last submission timestamp is updated independently of the read index
			// so instead we just avoid ever filling the buffer completely
			cemu_assert_debug(readIndex < poolSizeInU32s);
			cemu_assert_debug(writeIndex < poolSizeInU32s);
			if (writeIndex < readIndex)
			{
				// writeIndex has wrapped around
				uint32 wordsAvailable = readIndex - writeIndex;
				if (wordsAvailable > 0)
					wordsAvailable--; // avoid writeIndex becoming equal to readIndex
				if (wordsAvailable >= numU32s)
				{
					bufferPtr = s_commandState->commandPoolBase + writeIndex;
					bufferSizeInU32s = wordsAvailable;
					break;
				}
			}
			else
			{
				uint32 wordsAvailable = poolSizeInU32s - writeIndex;
				if (wordsAvailable > 0)
					wordsAvailable--; // avoid writeIndex becoming equal to readIndex
				if (wordsAvailable >= numU32s)
				{
					bufferPtr = nextWritePos;
					bufferSizeInU32s = wordsAvailable;
					break;
				}
				// not enough space at end of buffer, try to grab from the beginning of the buffer
				wordsAvailable = readIndex;
				if (wordsAvailable > 0)
					wordsAvailable--; // avoid writeIndex becoming equal to readIndex
				if (wordsAvailable >= numU32s)
				{
					bufferPtr = s_commandState->commandPoolBase;
					bufferSizeInU32s = wordsAvailable;
					break;
				}
			}
			GX2Command_WaitForNextBufferRetired();
		}
		cemu_assert_debug(bufferPtr);
		bufferSizeInU32s = std::min<uint32>(numU32s, 0x20000); // size cap
#ifdef CEMU_DEBUG_ASSERT
		uint32 newWriteIndex = ((bufferPtr - s_commandState->commandPoolBase) + bufferSizeInU32s) % s_commandState->commandPoolSizeInU32s;
		cemu_assert_debug(newWriteIndex != readIndex);
#endif
		// setup buffer and make it the current write gather target
		cemu_assert_debug(bufferPtr >= s_commandState->commandPoolBase && (bufferPtr + bufferSizeInU32s) <= s_commandState->commandPoolBase + s_commandState->commandPoolSizeInU32s);
		GX2Command_SetupCoreCommandBuffer(bufferPtr, bufferSizeInU32s, false);
	}

	void GX2Command_SubmitCommandBuffer(uint32be* buffer, uint32 sizeInU32s, MEMPTR<uint32be>* completionGPUReadPointer, bool triggerMarkerInterrupt)
	{
		uint32be cmd[10];
		uint32 cmdLen = 4;
		cmd[0] = pm4HeaderType3(IT_INDIRECT_BUFFER_PRIV, 3);
		cmd[1] = memory_virtualToPhysical(MEMPTR<void>(buffer).GetMPTR());
		cmd[2] = 0x00000000; // address high bits
		cmd[3] = sizeInU32s;
		if (completionGPUReadPointer)
		{
			// append command to update completionGPUReadPointer after the GPU is done with the command buffer
			cmd[4] = pm4HeaderType3(IT_MEM_WRITE, 4);
			cmd[5] = memory_virtualToPhysical(MEMPTR<void>(completionGPUReadPointer).GetMPTR()) | 2;
			cmd[6] = 0x40000;
			cmd[7] = MEMPTR<void>(buffer + sizeInU32s).GetMPTR(); // value to write
			cmd[8] = 0x00000000;
			cmdLen = 9;
		}

		betype<TCL::TCLSubmissionFlag> submissionFlags{};
		if (!triggerMarkerInterrupt)
			submissionFlags |= TCL::TCLSubmissionFlag::NO_MARKER_INTERRUPT;
		submissionFlags |= TCL::TCLSubmissionFlag::USE_RETIRED_MARKER;

		TCL::TCLSubmitToRing(cmd, cmdLen, &submissionFlags, &s_commandState->lastSubmissionTime);
	}

	void GX2Command_PadCurrentBuffer()
	{
		uint32 coreIndex = coreinit::OSGetCoreId();
		auto& coreCBState = s_perCoreCBState[coreIndex];
		if (!coreCBState.currentWritePtr)
			return;
		uint32 writeDistance = (uint32)(coreCBState.currentWritePtr - coreCBState.bufferPtr);
		if ((writeDistance&7) != 0)
		{
			uint32 distanceToPad = 0x8 - (writeDistance & 0x7);
			while (distanceToPad)
			{
				*coreCBState.currentWritePtr = pm4HeaderType2Filler();
				coreCBState.currentWritePtr++;
				distanceToPad--;
			}
		}
	}

	void GX2Command_Flush(uint32 numU32sForNextBuffer, bool triggerMarkerInterrupt)
	{
		uint32 coreIndex = coreinit::OSGetCoreId();
		auto& coreCBState = s_perCoreCBState[coreIndex];
		if (coreCBState.isDisplayList)
		{
			// display list
			cemu_assert_debug((uint32)(coreCBState.currentWritePtr - coreCBState.bufferPtr) < coreCBState.bufferSizeInU32s);
			cemuLog_logDebugOnce(LogType::Force, "GX2 flush called on display list");
		}
		else
		{
			// command buffer
			if (coreCBState.currentWritePtr != coreCBState.bufferPtr)
			{
				// pad the command buffer to 32 byte alignment
				GX2Command_PadCurrentBuffer();
				// submit it to the GPU
				uint32 bufferLength = (uint32)(coreCBState.currentWritePtr - coreCBState.bufferPtr);
				cemu_assert_debug(bufferLength <= coreCBState.bufferSizeInU32s);
				GX2Command_SubmitCommandBuffer(coreCBState.bufferPtr, bufferLength, &s_commandState->gpuCommandReadPtr, triggerMarkerInterrupt);
				GX2Command_StartNewCommandBuffer(numU32sForNextBuffer);
			}
			else
			{
				// current buffer is empty so we dont need to queue it
				if (numU32sForNextBuffer > s_commandState->commandPoolSizeInU32s)
					GX2Command_StartNewCommandBuffer(numU32sForNextBuffer);
			}
		}
	}

	void GX2Flush()
	{
		GX2Command_Flush(256, true);
	}

	uint64 GX2GetLastSubmittedTimeStamp()
	{
		stdx::atomic_ref<uint64be> _lastSubmissionTime(s_commandState->lastSubmissionTime);
		return _lastSubmissionTime.load();
	}

	uint64 GX2GetRetiredTimeStamp()
	{
		uint64be ts = 0;
		TCL::TCLTimestamp(TCL::TCLTimestampId::TIMESTAMP_LAST_BUFFER_RETIRED, &ts);
		return ts;
	}

	bool GX2WaitTimeStamp(uint64 tsWait)
	{
		// handle GPU timeout here? But for now we timeout after 60 seconds
		TCL::TCLWaitTimestamp(TCL::TCLTimestampId::TIMESTAMP_LAST_BUFFER_RETIRED, tsWait, Espresso::TIMER_CLOCK * 60);
		return true;
	}

	/*
	 * Guarantees that the requested amount of space is available on the current command buffer
	 * If the space is not available, the current command buffer is pushed to the GPU and a new one is allocated
	 */
	void GX2ReserveCmdSpace(uint32 reservedFreeSpaceInU32)
	{
		uint32 coreIndex = coreinit::OSGetCoreId();
		auto& coreCBState = s_perCoreCBState[coreIndex];
		if (coreCBState.currentWritePtr == nullptr)
			return;
		uint32 writeDistance = (uint32)(coreCBState.currentWritePtr - coreCBState.bufferPtr);
		if (writeDistance + reservedFreeSpaceInU32 > coreCBState.bufferSizeInU32s)
		{
			GX2Command_Flush(reservedFreeSpaceInU32, true);
		}
	}

	void GX2WriteGather_beginDisplayList(PPCInterpreter_t* hCPU, MPTR buffer, uint32 maxSize)
	{
		uint32 coreIndex = PPCInterpreter_getCoreIndex(hCPU);
		if (coreIndex == sGX2MainCoreIndex)
		{
			GX2Command_PadCurrentBuffer();
			cemu_assert_debug(!s_perCoreCBState[coreIndex].isDisplayList);
			s_mainCoreLastCommandState = s_perCoreCBState[coreIndex];
		}
		GX2Command_SetupCoreCommandBuffer(MEMPTR<uint32be>(buffer), maxSize/4, true);
	}

	uint32 GX2WriteGather_getDisplayListWriteDistance(sint32 coreIndex)
	{
		auto& coreCBState = s_perCoreCBState[coreIndex];
		cemu_assert_debug(coreCBState.isDisplayList);
		if (coreCBState.currentWritePtr == nullptr)
			return 0;
		return (uint32)(coreCBState.currentWritePtr - coreCBState.bufferPtr) * 4;
	}

	uint32 GX2WriteGather_endDisplayList(PPCInterpreter_t* hCPU, MPTR buffer)
	{
		uint32 coreIndex = coreinit::OSGetCoreId();
		auto& coreCBState = s_perCoreCBState[coreIndex];
		GX2Command_PadCurrentBuffer();
		uint32 finalWriteIndex = coreCBState.currentWritePtr - coreCBState.bufferPtr;
		cemu_assert_debug(finalWriteIndex <= coreCBState.bufferSizeInU32s);
		// if we are on the main GX2 core then restore the GPU command buffer
		if (coreIndex == sGX2MainCoreIndex)
		{
  			coreCBState = s_mainCoreLastCommandState;
		}
		else
		{
			coreCBState.bufferPtr = nullptr;
			coreCBState.currentWritePtr = nullptr;
			coreCBState.bufferSizeInU32s = 0;
			coreCBState.isDisplayList = false;
		}
		return finalWriteIndex * 4;
	}

	bool GX2GetCurrentDisplayList(MEMPTR<uint32be>* displayListAddr, uint32be* displayListSize)
	{
		uint32 coreIndex = coreinit::OSGetCoreId();
		auto& coreCBState = s_perCoreCBState[coreIndex];
		if (!coreCBState.isDisplayList)
			return false;
		if (displayListAddr)
			*displayListAddr = coreCBState.bufferPtr;
		if (displayListSize)
			*displayListSize = coreCBState.bufferSizeInU32s * sizeof(uint32be);
		return true;
	}

	// returns true if we are writing to a display list
	bool GX2GetDisplayListWriteStatus()
	{
		uint32 coreIndex = coreinit::OSGetCoreId();
		return s_perCoreCBState[coreIndex].isDisplayList;
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
	}

	void GX2DirectCallDisplayList(void* addr, uint32 size)
	{
		// this API submits to TCL directly and bypasses write-gatherer
		// its basically a way to manually submit a command buffer to the GPU
		uint32 coreIndex = coreinit::OSGetCoreId();
		if (coreIndex != sGX2MainCoreIndex)
		{
			cemuLog_logDebugOnce(LogType::Force, "GX2DirectCallDisplayList() called on non-main GX2 core");
		}
		if (!s_perCoreCBState[coreIndex].isDisplayList)
		{
			// make sure any preceeding commands are submitted first
			GX2Command_Flush(0x100, false);
		}
		GX2Command_SubmitCommandBuffer(static_cast<uint32be*>(addr), size / 4, nullptr, false);
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
		cafeExportRegister("gx2", GX2Flush, LogType::GX2);

		cafeExportRegister("gx2", GX2GetLastSubmittedTimeStamp, LogType::GX2);
		cafeExportRegister("gx2", GX2GetRetiredTimeStamp, LogType::GX2);
		cafeExportRegister("gx2", GX2WaitTimeStamp, LogType::GX2);

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
		s_commandState->commandPoolBase = nullptr;
		s_commandState->commandPoolSizeInU32s = 0;
		s_commandState->gpuCommandReadPtr = nullptr;
		s_cbBufferIsInternallyAllocated = false;
    }

}
