#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/OS/libs/gx2/GX2.h" // for write gatherer and special state. Get rid of dependency
#include "Cafe/OS/libs/gx2/GX2_Misc.h" // for GX2::sGX2MainCoreIndex. Legacy dependency
#include "Cafe/OS/libs/gx2/GX2_Event.h" // for notification callbacks
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteAsyncCommands.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Cafe/HW/Latte/Core/LatteIndices.h"
#include "Cafe/HW/Latte/Core/LatteBufferCache.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"

#include "Cafe/OS/libs/coreinit/coreinit_Time.h"

#include "Cafe/CafeSystem.h"

#define CP_TIMER_RECHECK	1024

//#define FAST_DRAW_LOGGING

uint8* gxRingBufferReadPtr; // currently active read pointer (gx2 ring buffer or display list)
uint8* gx2CPParserDisplayListPtr;
uint8* gx2CPParserDisplayListStart; // used for debugging
uint8* gx2CPParserDisplayListEnd;

void LatteThread_HandleOSScreen();

void LatteThread_Exit();

class DrawPassContext
{
public:
	bool isWithinDrawPass() const
	{
		return m_drawPassActive;
	}

	void beginDrawPass()
	{
		m_drawPassActive = true;
		m_isFirstDraw = true;
		m_vertexBufferChanged = true;
		m_uniformBufferChanged = true;
		g_renderer->draw_beginSequence();
	}

	void executeDraw(uint32 count, bool isAutoIndex, MPTR physIndices)
	{
		uint32 baseVertex = LatteGPUState.contextRegister[mmSQ_VTX_BASE_VTX_LOC];
		uint32 baseInstance = LatteGPUState.contextRegister[mmSQ_VTX_START_INST_LOC];
		uint32 numInstances = LatteGPUState.drawContext.numInstances;
		if (numInstances == 0)
			return;

		if (!isAutoIndex)
		{
			cemu_assert_debug(physIndices != MPTR_NULL);
			if (physIndices == MPTR_NULL)
				return;
			auto indexType = LatteGPUState.contextNew.VGT_DMA_INDEX_TYPE.get_INDEX_TYPE();
			g_renderer->draw_execute(baseVertex, baseInstance, numInstances, count, physIndices, indexType, m_isFirstDraw);
		}
		else
		{
			g_renderer->draw_execute(baseVertex, baseInstance, numInstances, count, MPTR_NULL, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE::AUTO, m_isFirstDraw);
		}
		m_isFirstDraw = false;
		m_vertexBufferChanged = false;
		m_uniformBufferChanged = false;
	}

	void endDrawPass()
	{
		g_renderer->draw_endSequence();
		m_drawPassActive = false;
	}

	void notifyModifiedVertexBuffer() 
	{
		m_vertexBufferChanged = true;
	}

	void notifyModifiedUniformBuffer()
	{
		m_uniformBufferChanged = true;
	}

private:
	bool m_drawPassActive{ false };
	bool m_isFirstDraw{false};
	bool m_vertexBufferChanged{ false };
	bool m_uniformBufferChanged{ false };
};

void LatteCP_processCommandBuffer(uint8* cmdBuffer, sint32 cmdSize, DrawPassContext& drawPassCtx);

/*
* Read a U32 from the command buffer
* If no data is available then wait in a busy loop
*/
uint32 LatteCP_readU32Deprc()
{
	uint32 v;
	uint8* gxRingBufferWritePtr;
	sint32 readDistance;
	// no display list active
	while (true)
	{
		gxRingBufferWritePtr = gx2WriteGatherPipe.writeGatherPtrGxBuffer[GX2::sGX2MainCoreIndex];
		readDistance = (sint32)(gxRingBufferWritePtr - gxRingBufferReadPtr);
		if (readDistance != 0)
			break;

		g_renderer->NotifyLatteCommandProcessorIdle(); // let the renderer know in case it wants to flush any commands
		performanceMonitor.gpuTime_idleTime.beginMeasuring();
		// no command data available, spin in a busy loop for a bit then check again
		for (sint32 busy = 0; busy < 80; busy++)
		{
			_mm_pause();
		}
		LatteThread_HandleOSScreen(); // check if new frame was presented via OSScreen API

		readDistance = (sint32)(gxRingBufferWritePtr - gxRingBufferReadPtr);
		if (readDistance != 0)
			break;
		if (Latte_GetStopSignal())
			LatteThread_Exit();

		// still no command data available, do some other tasks
		LatteTiming_HandleTimedVsync();
		LatteAsyncCommands_checkAndExecute();
		std::this_thread::yield();
		performanceMonitor.gpuTime_idleTime.endMeasuring();
	}
	v = *(uint32*)gxRingBufferReadPtr;
	gxRingBufferReadPtr += 4;
#ifdef CEMU_DEBUG_ASSERT
	if (v == 0xcdcdcdcd)
		assert_dbg();
#endif
	v = _swapEndianU32(v);
	return v;
}

void LatteCP_waitForNWords(uint32 numWords)
{
	uint8* gxRingBufferWritePtr;
	sint32 readDistance;
	bool isFlushed = false;
	sint32 waitDistance = numWords * sizeof(uint32be);
	// no display list active
	while (true)
	{
		gxRingBufferWritePtr = gx2WriteGatherPipe.writeGatherPtrGxBuffer[GX2::sGX2MainCoreIndex];
		readDistance = (sint32)(gxRingBufferWritePtr - gxRingBufferReadPtr);
		if (readDistance < 0)
			return; // wrap around means there is at least one full command queued after this
		if (readDistance >= waitDistance)
			break;
		g_renderer->NotifyLatteCommandProcessorIdle(); // let the renderer know in case it wants to flush any commands
		performanceMonitor.gpuTime_idleTime.beginMeasuring();
		// no command data available, spin in a busy loop for a while then check again
		for (sint32 busy = 0; busy < 80; busy++)
		{
			_mm_pause();
		}
		readDistance = (sint32)(gxRingBufferWritePtr - gxRingBufferReadPtr);
		if (readDistance < 0)
			return; // wrap around means there is at least one full command queued after this
		if (readDistance >= waitDistance)
			break;

		if (Latte_GetStopSignal())
			LatteThread_Exit();

		// still no command data available, do some other tasks
		LatteTiming_HandleTimedVsync();
		LatteAsyncCommands_checkAndExecute();
		std::this_thread::yield();
		performanceMonitor.gpuTime_idleTime.endMeasuring();
	}
}

template<uint32 readU32()>
void LatteCP_skipWords(uint32 wordsToSkip)
{
	while (wordsToSkip)
	{
		readU32();
		wordsToSkip--;
	}
}

typedef uint32be* LatteCMDPtr;
#define LatteReadCMD() ((uint32)*(cmd++))
#define LatteSkipCMD(_nWords) cmd += (_nWords)

LatteCMDPtr LatteCP_itSurfaceSync(LatteCMDPtr cmd)
{
	uint32 invalidationFlags = LatteReadCMD();
	uint32 size = LatteReadCMD() << 8;
	MPTR addressPhys = LatteReadCMD() << 8;
	uint32 pollInterval = LatteReadCMD();

	if (addressPhys == MPTR_NULL || size == 0xFFFFFFFF)
		return cmd; // block global invalidations because they are too expensive

	if (invalidationFlags & 0x800000)
	{
		// invalidate uniform or attribute buffer
		LatteBufferCache_invalidate(addressPhys, size);
	}
	return cmd;
}

template<uint32 readU32()>
void LatteCP_itIndirectBufferDepr(uint32 nWords)
{
	cemu_assert_debug(nWords == 3);

	uint32 physicalAddress = readU32();
	uint32 physicalAddressHigh = readU32(); // unused
	uint32 sizeInDWords = readU32();
	uint32 displayListSize = sizeInDWords * 4;
	DrawPassContext drawPassCtx;
	LatteCP_processCommandBuffer(memory_getPointerFromPhysicalOffset(physicalAddress), displayListSize, drawPassCtx);
	if (drawPassCtx.isWithinDrawPass())
		drawPassCtx.endDrawPass();
}

LatteCMDPtr LatteCP_itIndirectBuffer(LatteCMDPtr cmd, uint32 nWords, DrawPassContext& drawPassCtx)
{
	cemu_assert_debug(nWords == 3);
	uint32 physicalAddress = LatteReadCMD();
	uint32 physicalAddressHigh = LatteReadCMD(); // unused
	uint32 sizeInDWords = LatteReadCMD();
	uint32 displayListSize = sizeInDWords * 4;
	cemu_assert_debug(displayListSize >= 4);

	LatteCP_processCommandBuffer(memory_getPointerFromPhysicalOffset(physicalAddress), displayListSize, drawPassCtx);
	return cmd;
}

LatteCMDPtr LatteCP_itStreamoutBufferUpdate(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 5);
	uint32 updateControl = LatteReadCMD();
	uint32 physicalAddressWrite = LatteReadCMD();
	uint32 ukn1 = LatteReadCMD();
	uint32 physicalAddressRead = LatteReadCMD();
	uint32 ukn3 = LatteReadCMD();

	uint32 mode = (updateControl >> 1) & 3;
	uint32 soIndex = (updateControl >> 8) & 3;

	if (mode == 0)
	{
		// reset pointer
		MPTR virtualAddress = memory_physicalToVirtual(physicalAddressRead);
		uint32 bufferOffset = 0;
		LatteGPUState.contextRegister[mmVGT_STRMOUT_BUFFER_OFFSET_0 + 4 * soIndex] = bufferOffset;
	}
	else if (mode == 3)
	{
		// store current offset to memory
		MPTR virtualAddress = memory_physicalToVirtual(physicalAddressWrite);
		uint32 bufferOffset = LatteGPUState.contextRegister[mmVGT_STRMOUT_BUFFER_OFFSET_0 + 4 * soIndex];
		memory_writeU32(virtualAddress + 0x00, bufferOffset);
	}
	else
	{
		cemu_assert_unimplemented();
	}
	return cmd;
}

template<uint32 registerBaseMode>
void LatteCP_itSetRegistersGeneric_handleSpecialRanges(uint32 registerStartIndex, uint32 registerEndIndex)
{
	if constexpr (registerBaseMode == IT_SET_CONTEXT_REG)
	{
		if (registerStartIndex <= mmSQ_VTX_SEMANTIC_CLEAR && registerEndIndex >= mmSQ_VTX_SEMANTIC_CLEAR)
		{
			for (uint32 i = 0; i < 32; i++)
			{
				LatteGPUState.contextRegister[mmSQ_VTX_SEMANTIC_0 + i] = 0xFF;
			}
		}
	}
}

template<uint32 TRegisterBase>
LatteCMDPtr LatteCP_itSetRegistersGeneric(LatteCMDPtr cmd, uint32 nWords)
{
	uint32 registerOffset = LatteReadCMD();
	uint32 registerIndex = TRegisterBase + registerOffset;
	uint32 registerStartIndex = registerIndex;
	uint32 registerEndIndex = registerStartIndex + nWords;
#ifdef CEMU_DEBUG_ASSERT
	cemu_assert_debug((registerIndex + nWords) <= LATTE_MAX_REGISTER);
#endif
	uint32* outputReg = (uint32*)(LatteGPUState.contextRegister + registerIndex);
	if (LatteGPUState.contextControl0 == 0x80000077)
	{
		// state shadowing enabled
		uint32* shadowAddrs = LatteGPUState.contextRegisterShadowAddr + registerIndex;
		sint32 indexCounter = 0;
		while (--nWords)
		{
			uint32 dataWord = LatteReadCMD();
			MPTR regShadowAddr = shadowAddrs[indexCounter];
			if (regShadowAddr)
				*(uint32*)(memory_base + regShadowAddr) = _swapEndianU32(dataWord);
			outputReg[indexCounter] = dataWord;
			indexCounter++;
		}
	}
	else
	{
		// state shadowing disabled
		sint32 indexCounter = 0;
		while (--nWords)
		{
			*outputReg = LatteReadCMD();
			outputReg++;
		}
	}
	// some register writes trigger special behavior
	LatteCP_itSetRegistersGeneric_handleSpecialRanges<TRegisterBase>(registerStartIndex, registerEndIndex);
	return cmd;
}

template<uint32 TRegisterBase, typename TRegRangeCallback>
LatteCMDPtr LatteCP_itSetRegistersGeneric(LatteCMDPtr cmd, uint32 nWords, TRegRangeCallback cbRegRange)
{
	uint32 registerOffset = LatteReadCMD();
	uint32 registerIndex = TRegisterBase + registerOffset;
	uint32 registerStartIndex = registerIndex;
	uint32 registerEndIndex = registerStartIndex + nWords;
#ifdef CEMU_DEBUG_ASSERT
	cemu_assert_debug((registerIndex + nWords) <= LATTE_MAX_REGISTER);
#endif
	cbRegRange(registerStartIndex, registerEndIndex);

	uint32* outputReg = (uint32*)(LatteGPUState.contextRegister + registerIndex);
	if (LatteGPUState.contextControl0 == 0x80000077)
	{
		// state shadowing enabled
		uint32* shadowAddrs = LatteGPUState.contextRegisterShadowAddr + registerIndex;
		sint32 indexCounter = 0;
		while (--nWords)
		{
			uint32 dataWord = LatteReadCMD();
			MPTR regShadowAddr = shadowAddrs[indexCounter];
			if (regShadowAddr)
				*(uint32*)(memory_base + regShadowAddr) = _swapEndianU32(dataWord);
			outputReg[indexCounter] = dataWord;
			indexCounter++;
		}
	}
	else
	{
		// state shadowing disabled
		sint32 indexCounter = 0;
		while (--nWords)
		{
			*outputReg = LatteReadCMD();
			outputReg++;
		}
	}
	// some register writes trigger special behavior
	LatteCP_itSetRegistersGeneric_handleSpecialRanges<TRegisterBase>(registerStartIndex, registerEndIndex);
	return cmd;
}

LatteCMDPtr LatteCP_itIndexType(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 1);
	LatteGPUState.contextNew.VGT_DMA_INDEX_TYPE.set_INDEX_TYPE((Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE)LatteReadCMD());
	return cmd;
}

LatteCMDPtr LatteCP_itNumInstances(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 1);
	LatteGPUState.drawContext.numInstances = LatteReadCMD();
	return cmd;
}

LatteCMDPtr LatteCP_itWaitRegMem(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 6);
	uint32 word0 = LatteReadCMD();
	uint32 word1 = LatteReadCMD();
	uint32 word2 = LatteReadCMD();
	uint32 word3 = LatteReadCMD();
	uint32 word4 = LatteReadCMD();
	uint32 word5 = LatteReadCMD();

	uint32 compareOp = (word0) & 7;
	uint32 physAddr = word1 & ~3;
	cemu_assert_debug((physAddr&3) == 0);
	uint32 fenceValue = word3;
	uint32 fenceMask = word4;

	uint32* fencePtr = (uint32*)memory_getPointerFromPhysicalOffset(physAddr);

	const uint32 GPU7_WAIT_MEM_OP_ALWAYS = 0;
	const uint32 GPU7_WAIT_MEM_OP_LESS = 1;
	const uint32 GPU7_WAIT_MEM_OP_LEQUAL = 2;
	const uint32 GPU7_WAIT_MEM_OP_EQUAL = 3;
	const uint32 GPU7_WAIT_MEM_OP_NOTEQUAL = 4;
	const uint32 GPU7_WAIT_MEM_OP_GEQUAL = 5;
	const uint32 GPU7_WAIT_MEM_OP_GREATER = 6;
	const uint32 GPU7_WAIT_MEM_OP_NEVER = 7;

	bool stalls = false;
	if ((word0 & 0x10) != 0)
	{
		// wait for memory address
		performanceMonitor.gpuTime_fenceTime.beginMeasuring();
		while (true)
		{
			uint32 fenceMemValue = _swapEndianU32(*fencePtr);
			fenceMemValue &= fenceMask;
			if (compareOp == GPU7_WAIT_MEM_OP_GEQUAL)
			{
				// greater or equal
				if (fenceMemValue >= fenceValue)
					break;
			}
			else if (compareOp == GPU7_WAIT_MEM_OP_EQUAL)
			{
				// equal
				if (fenceMemValue == fenceValue)
					break;
			}
			else
				assert_dbg();
			if (!stalls)
			{
				g_renderer->NotifyLatteCommandProcessorIdle();
				stalls = true;
			}

			// check if any GPU events happened
			LatteTiming_HandleTimedVsync();
			LatteAsyncCommands_checkAndExecute();
		}
		performanceMonitor.gpuTime_fenceTime.endMeasuring();
	}
	else
	{
		// wait for register
		debugBreakpoint();
	}
	return cmd;
}

LatteCMDPtr LatteCP_itMemWrite(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 4);
	uint32 word0 = LatteReadCMD();
	uint32 word1 = LatteReadCMD();
	uint32 word2 = LatteReadCMD();
	uint32 word3 = LatteReadCMD();

	MPTR valuePhysAddr = (word0 & ~3);
	if (valuePhysAddr == 0)
	{
		cemuLog_log(LogType::Force, "GPU: Invalid itMemWrite to null pointer");
		return cmd;
	}
	uint32be* memPtr = (uint32be*)memory_getPointerFromPhysicalOffset(valuePhysAddr);

	if (word1 == 0x40000)
	{
		// write U32
		*memPtr = word2;
	}
	else if (word1 == 0x00000)
	{
		// write U64 (as two U32)
		// note: The U32s are swapped
		memPtr[0] = word2;
		memPtr[1] = word3;
	}
	else if (word1 == 0x20000)
	{
		// write U64 (little endian)
		memPtr[0] = _swapEndianU32(word2);
		memPtr[1] = _swapEndianU32(word3);
	}
	else
		cemu_assert_unimplemented();
	return cmd;
}


LatteCMDPtr LatteCP_itMemSemaphore(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 2);
	MPTR semaphorePhysicalAddress = LatteReadCMD();
	uint32 semaphoreControl = LatteReadCMD();
	uint8 SEM_SIGNAL = (semaphoreControl >> 29) & 7;

	std::atomic<uint64le>* semaphoreData = _rawPtrToAtomic((uint64le*)memory_getPointerFromPhysicalOffset(semaphorePhysicalAddress));
	static_assert(sizeof(std::atomic<uint64le>) == sizeof(uint64le));

	if (SEM_SIGNAL == 6)
	{
		// signal
		semaphoreData->fetch_add(1);
	}
	else if(SEM_SIGNAL == 7)
	{
		// wait
		size_t loopCount = 0;
		while (true)
		{
			uint64le oldVal = semaphoreData->load();
			if (oldVal == 0)
			{
				loopCount++;
				if (loopCount > 2000)
					std::this_thread::yield();
				continue;
			}
			if (semaphoreData->compare_exchange_strong(oldVal, oldVal - 1))
				break;
		}
	}
	else
	{
		cemu_assert_debug(false);
	}
	return cmd;
}

LatteCMDPtr LatteCP_itContextControl(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 2);
	uint32 word0 = LatteReadCMD();
	uint32 word1 = LatteReadCMD();
	LatteGPUState.contextControl0 = word0;
	LatteGPUState.contextControl1 = word1;
	return cmd;
}

LatteCMDPtr LatteCP_itLoadReg(LatteCMDPtr cmd, uint32 nWords, uint32 regBase)
{
	if (nWords < 2 || (nWords & 1) != 0)
	{
		cemuLog_logDebug(LogType::Force, "itLoadReg: Invalid nWords value");
		return cmd;
	}
	MPTR physAddressRegArea = LatteReadCMD();
	uint32 waitForIdle = LatteReadCMD();
	uint32 loadEntries = (nWords - 2) / 2;
	uint32 regShadowMemAddr = physAddressRegArea;
	for (uint32 i = 0; i < loadEntries; i++)
	{
		uint32 regOffset = LatteReadCMD();
		uint32 regCount = LatteReadCMD();
		cemu_assert_debug(regCount != 0);
		uint32 regAddr = regBase + regOffset;
		for (uint32 f = 0; f < regCount; f++)
		{
			LatteGPUState.contextRegisterShadowAddr[regAddr] = regShadowMemAddr;
			LatteGPUState.contextRegister[regAddr] = memory_read<uint32>(regShadowMemAddr);
			regAddr++;
			regShadowMemAddr += 4;
		}
	}
	return cmd;
}

bool conditionalRenderActive = false;

LatteCMDPtr LatteCP_itSetPredication(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 2);
	MPTR physQueryInfo = LatteReadCMD();
	uint32 flags = LatteReadCMD();

	uint32 queryTypeFlag = (flags >> 13) & 7;
	uint32 pixelsMustPassFlag = (flags >> 31) & 1;
	uint32 dontWaitFlag = (flags >> 1) & 19;

	if (queryTypeFlag == 0)
	{
		// disable conditional render
		if (conditionalRenderActive == false)
			debug_printf("conditionalRenderActive already inactive\n");
		conditionalRenderActive = false;
	}
	else
	{
		// enable conditonal render
		if (conditionalRenderActive == true)
			debug_printf("conditionalRenderActive already active\n");
		conditionalRenderActive = true;
	}
	return cmd;
}

LatteCMDPtr LatteCP_itDrawIndex2(LatteCMDPtr cmd, uint32 nWords, DrawPassContext& drawPassCtx)
{
	cemu_assert_debug(nWords == 5);
	uint32 ukn1 = LatteReadCMD();
	MPTR physIndices = LatteReadCMD();
	uint32 ukn2 = LatteReadCMD();
	uint32 count = LatteReadCMD();
	uint32 ukn3 = LatteReadCMD();

	performanceMonitor.cycle[performanceMonitor.cycleIndex].drawCallCounter++;

	LatteGPUState.currentDrawCallTick = GetTickCount();
	drawPassCtx.executeDraw(count, false, physIndices);
	return cmd;
}

LatteCMDPtr LatteCP_itDrawIndexAuto(LatteCMDPtr cmd, uint32 nWords, DrawPassContext& drawPassCtx)
{
	cemu_assert_debug(nWords == 2);
	uint32 count = LatteReadCMD();
	uint32 ukn = LatteReadCMD();

	performanceMonitor.cycle[performanceMonitor.cycleIndex].drawCallCounter++;

	if (LatteGPUState.drawContext.numInstances == 0)
		return cmd;
	LatteGPUState.currentDrawCallTick = GetTickCount();
	// todo - better way to identify compute drawcalls
	if ((LatteGPUState.contextRegister[mmSQ_CONFIG] >> 24) == 0xE4)
	{
		uint32 vsProgramCode = ((LatteGPUState.contextRegister[mmSQ_PGM_START_ES] & 0xFFFFFF) << 8);
		uint32 vsProgramSize = LatteGPUState.contextRegister[mmSQ_PGM_START_ES + 1] << 3;
		cemuLog_logDebug(LogType::Force, "Compute {} {:08x} {:08x} (unsupported)", count, vsProgramCode, vsProgramSize);
	}
	else
	{
		drawPassCtx.executeDraw(count, true, MPTR_NULL);
	}
	return cmd;
}

MPTR _tempIndexArrayMPTR = MPTR_NULL;

LatteCMDPtr LatteCP_itDrawImmediate(LatteCMDPtr cmd, uint32 nWords, DrawPassContext& drawPassCtx)
{
	uint32 count = LatteReadCMD();
	uint32 ukn1 = LatteReadCMD();
	// reserve array for index data	
	if (_tempIndexArrayMPTR == MPTR_NULL)
		_tempIndexArrayMPTR = coreinit_allocFromSysArea(0x4000 * sizeof(uint32), 0x4);

	LatteGPUState.currentDrawCallTick = GetTickCount();
	// calculate size of index data in packet and read indices
	uint32 numIndexU32s;
	auto indexType = LatteGPUState.contextNew.VGT_DMA_INDEX_TYPE.get_INDEX_TYPE();
	if (indexType == Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE::U16_BE)
	{
		// 16bit indices
		numIndexU32s = (count + 1) / 2;
		memcpy(memory_getPointerFromVirtualOffset(_tempIndexArrayMPTR), cmd, numIndexU32s * sizeof(uint32));
		LatteSkipCMD(numIndexU32s);
		// swap pairs
		uint32* indexDataU32 = (uint32*)memory_getPointerFromVirtualOffset(_tempIndexArrayMPTR);
		for (uint32 i = 0; i < numIndexU32s; i++)
		{
			indexDataU32[i] = (indexDataU32[i] >> 16) | (indexDataU32[i] << 16);
		}
		LatteIndices_invalidate(memory_getPointerFromVirtualOffset(_tempIndexArrayMPTR), numIndexU32s * sizeof(uint32));
	}
	else if (indexType == Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE::U32_BE)
	{
		// 32bit indices
		cemu_assert_debug(false); // testing needed
		numIndexU32s = count;
		memcpy(memory_getPointerFromVirtualOffset(_tempIndexArrayMPTR), cmd, numIndexU32s * sizeof(uint32));
		LatteSkipCMD(numIndexU32s);
		LatteIndices_invalidate(memory_getPointerFromVirtualOffset(_tempIndexArrayMPTR), numIndexU32s * sizeof(uint32));
	}
	else
	{
		cemuLog_log(LogType::Force, "itDrawImmediate - Unsupported index type");
		return cmd;
	}
	// verify packet size
	if (nWords != (2 + numIndexU32s))
		debugBreakpoint();
	performanceMonitor.cycle[performanceMonitor.cycleIndex].drawCallCounter++;

	uint32 baseVertex = LatteGPUState.contextRegister[mmSQ_VTX_BASE_VTX_LOC];
	uint32 baseInstance = LatteGPUState.contextRegister[mmSQ_VTX_START_INST_LOC];
	uint32 numInstances = LatteGPUState.drawContext.numInstances;

	drawPassCtx.executeDraw(count, false, _tempIndexArrayMPTR);
	return cmd;

}

LatteCMDPtr LatteCP_itHLEFifoWrapAround(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 1);
	uint32 unused = LatteReadCMD();
	gxRingBufferReadPtr = gx2WriteGatherPipe.gxRingBuffer;
	cmd = (LatteCMDPtr)gxRingBufferReadPtr;
	return cmd;
}

LatteCMDPtr LatteCP_itHLESampleTimer(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 1);
	MPTR timerMPTR = (MPTR)LatteReadCMD();
	memory_writeU64(timerMPTR, coreinit::coreinit_getTimerTick());
	return cmd;
}

LatteCMDPtr LatteCP_itHLESpecialState(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 2);
	uint32 stateId = LatteReadCMD();
	uint32 stateValue = LatteReadCMD();
	if (stateId > GX2_SPECIAL_STATE_COUNT)
	{
		cemu_assert_suspicious();
	}
	else
	{
		LatteGPUState.contextNew.GetSpecialStateValues()[stateId] = stateValue;
	}
	return cmd;
}

LatteCMDPtr LatteCP_itHLESetRetirementTimestamp(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 2);
	uint32 timestampHigh = (uint32)LatteReadCMD();
	uint32 timestampLow = (uint32)LatteReadCMD();
	uint64 timestamp = ((uint64)timestampHigh << 32ULL) | (uint64)timestampLow;
	GX2::__GX2NotifyNewRetirementTimestamp(timestamp);
	return cmd;
}

LatteCMDPtr LatteCP_itHLEBeginOcclusionQuery(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 1);
	MPTR queryMPTR = (MPTR)LatteReadCMD();
	LatteQuery_BeginOcclusionQuery(queryMPTR);
	return cmd;
}

LatteCMDPtr LatteCP_itHLEEndOcclusionQuery(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 1);
	MPTR queryMPTR = (MPTR)LatteReadCMD();
	LatteQuery_EndOcclusionQuery(queryMPTR);
	return cmd;
}

LatteCMDPtr LatteCP_itHLEBottomOfPipeCB(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 3);
	MPTR timestampMPTR = (uint32)LatteReadCMD();
	uint32 timestampHigh = (uint32)LatteReadCMD();
	uint32 timestampLow = (uint32)LatteReadCMD();
	uint64 timestamp = ((uint64)timestampHigh << 32ULL) | (uint64)timestampLow;
	// write timestamp
	*(uint32*)memory_getPointerFromPhysicalOffset(timestampMPTR) = _swapEndianU32((uint32)(timestamp >> 32));
	*(uint32*)memory_getPointerFromPhysicalOffset(timestampMPTR + 4) = _swapEndianU32((uint32)timestamp);
	// send event
	GX2::__GX2NotifyEvent(GX2::GX2CallbackEventType::TIMESTAMP_BOTTOM);
	return cmd;
}

// GPU-side handler for GX2CopySurface/GX2CopySurfaceEx and similar
LatteCMDPtr LatteCP_itHLECopySurfaceNew(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 26);
	// src
	MPTR srcPhysAddr = LatteReadCMD();
	MPTR srcMipAddr = LatteReadCMD();
	uint32 srcSwizzle = LatteReadCMD();
	Latte::E_GX2SURFFMT srcSurfaceFormat = (Latte::E_GX2SURFFMT)LatteReadCMD();
	sint32 srcWidth = LatteReadCMD();
	sint32 srcHeight = LatteReadCMD();
	sint32 srcDepth = LatteReadCMD();
	uint32 srcPitch = LatteReadCMD();
	uint32 srcSlice = LatteReadCMD();
	Latte::E_DIM srcDim = (Latte::E_DIM)LatteReadCMD();
	Latte::E_HWTILEMODE srcTilemode = (Latte::E_HWTILEMODE)LatteReadCMD();
	sint32 srcAA = LatteReadCMD();
	sint32 srcLevel = LatteReadCMD();
	// dst
	MPTR dstPhysAddr = LatteReadCMD();
	MPTR dstMipAddr = LatteReadCMD();
	uint32 dstSwizzle = LatteReadCMD();
	Latte::E_GX2SURFFMT dstSurfaceFormat = (Latte::E_GX2SURFFMT)LatteReadCMD();
	sint32 dstWidth = LatteReadCMD();
	sint32 dstHeight = LatteReadCMD();
	sint32 dstDepth = LatteReadCMD();
	uint32 dstPitch = LatteReadCMD();
	uint32 dstSlice = LatteReadCMD();
	Latte::E_DIM dstDim = (Latte::E_DIM)LatteReadCMD();
	Latte::E_HWTILEMODE dstTilemode = (Latte::E_HWTILEMODE)LatteReadCMD();
	sint32 dstAA = LatteReadCMD();
	sint32 dstLevel = LatteReadCMD();

	LatteSurfaceCopy_copySurfaceNew(srcPhysAddr, srcMipAddr, srcSwizzle, srcSurfaceFormat, srcWidth, srcHeight, srcDepth, srcPitch, srcSlice, srcDim, srcTilemode, srcAA, srcLevel, dstPhysAddr, dstMipAddr, dstSwizzle, dstSurfaceFormat, dstWidth, dstHeight, dstDepth, dstPitch, dstSlice, dstDim, dstTilemode, dstAA, dstLevel);
	return cmd;
}

LatteCMDPtr LatteCP_itHLEClearColorDepthStencil(LatteCMDPtr cmd, uint32 nWords)
{
	cemu_assert_debug(nWords == 23);
	uint32 clearMask = LatteReadCMD(); // color (1), depth (2), stencil (4)
	// color buffer
	MPTR colorBufferMPTR = LatteReadCMD(); // MPTR for color buffer (physical address)
	MPTR colorBufferFormat = LatteReadCMD(); // format for color buffer
	Latte::E_HWTILEMODE colorBufferTilemode = (Latte::E_HWTILEMODE)LatteReadCMD();
	uint32 colorBufferWidth = LatteReadCMD();
	uint32 colorBufferHeight = LatteReadCMD();
	uint32 colorBufferPitch = LatteReadCMD();
	uint32 colorBufferViewFirstSlice = LatteReadCMD();
	uint32 colorBufferViewNumSlice = LatteReadCMD();
	// depth buffer
	MPTR depthBufferMPTR = LatteReadCMD(); // MPTR for depth buffer (physical address)
	MPTR depthBufferFormat = LatteReadCMD(); // format for depth buffer
	Latte::E_HWTILEMODE depthBufferTileMode = (Latte::E_HWTILEMODE)LatteReadCMD();
	uint32 depthBufferWidth = LatteReadCMD();
	uint32 depthBufferHeight = LatteReadCMD();
	uint32 depthBufferPitch = LatteReadCMD();
	uint32 depthBufferViewFirstSlice = LatteReadCMD();
	uint32 depthBufferViewNumSlice = LatteReadCMD();

	float r = (float)LatteReadCMD() / 255.0f;
	float g = (float)LatteReadCMD() / 255.0f;
	float b = (float)LatteReadCMD() / 255.0f;
	float a = (float)LatteReadCMD() / 255.0f;

	float clearDepth;
	*(uint32*)&clearDepth = LatteReadCMD();
	uint32 clearStencil = LatteReadCMD();

	LatteRenderTarget_itHLEClearColorDepthStencil(
		clearMask, 
		colorBufferMPTR, colorBufferFormat, colorBufferTilemode, colorBufferWidth, colorBufferHeight, colorBufferPitch, colorBufferViewFirstSlice, colorBufferViewNumSlice, 
		depthBufferMPTR, depthBufferFormat, depthBufferTileMode, depthBufferWidth, depthBufferHeight, depthBufferPitch, depthBufferViewFirstSlice, depthBufferViewNumSlice, 
		r, g, b, a,
		clearDepth, clearStencil);
	return cmd;
}

LatteCMDPtr LatteCP_itHLERequestSwapBuffers(LatteCMDPtr cmd, uint32 nWords)
{
	catchOpenGLError();
	cemu_assert_debug(nWords == 1);
	MPTR reserved1 = LatteReadCMD();
	// request flip counter increase (will be increased on next flip)
	LatteGPUState.flipRequestCount.fetch_add(1);
	return cmd;
}

LatteCMDPtr LatteCP_itHLESwapScanBuffer(LatteCMDPtr cmd, uint32 nWords)
{
	catchOpenGLError();
	cemu_assert_debug(nWords == 1);
	MPTR reserved1 = LatteReadCMD(); // reserved
	LatteRenderTarget_itHLESwapScanBuffer();
	return cmd;
}

LatteCMDPtr LatteCP_itHLEWaitForFlip(LatteCMDPtr cmd, uint32 nWords)
{
	catchOpenGLError();
	cemu_assert_debug(nWords == 1);
	MPTR reserved1 = LatteReadCMD(); // reserved
	// wait for flip
	uint32 currentFlipCount = LatteGPUState.flipCounter;
	while (true)
	{
		_mm_pause();
		if (currentFlipCount != LatteGPUState.flipCounter)
		{
			break;
		}
		// check if any GPU events happened
		LatteTiming_HandleTimedVsync();
		std::this_thread::yield();
	}
	return cmd;
}

LatteCMDPtr LatteCP_itHLECopyColorBufferToScanBuffer(LatteCMDPtr cmd, uint32 nWords)
{
	MPTR colorBufferPtr = LatteReadCMD(); // physical address
	uint32 colorBufferWidth = LatteReadCMD();
	uint32 colorBufferHeight = LatteReadCMD();
	uint32 colorBufferPitch = LatteReadCMD();
	Latte::E_HWTILEMODE colorBufferTilemode = (Latte::E_HWTILEMODE)LatteReadCMD();
	uint32 colorBufferSwizzle = LatteReadCMD();
	uint32 colorBufferSliceIndex = LatteReadCMD();
	uint32 colorBufferFormat = LatteReadCMD();
	uint32 renderTarget = LatteReadCMD();

	LatteRenderTarget_itHLECopyColorBufferToScanBuffer(colorBufferPtr, colorBufferWidth, colorBufferHeight, colorBufferSliceIndex, colorBufferFormat, colorBufferPitch, colorBufferTilemode, colorBufferSwizzle, renderTarget);

	return cmd;
}

void LatteCP_dumpCommandBufferError(LatteCMDPtr cmdStart, LatteCMDPtr cmdEnd, LatteCMDPtr cmdError)
{
	cemuLog_log(LogType::Force, "Detected error in GPU command buffer");
	cemuLog_log(LogType::Force, "Dumping contents and info");
	cemuLog_log(LogType::Force, "Buffer 0x{0:08x} Size 0x{1:08x}", memory_getVirtualOffsetFromPointer(cmdStart), memory_getVirtualOffsetFromPointer(cmdEnd));
	cemuLog_log(LogType::Force, "Error at 0x{0:08x}", memory_getVirtualOffsetFromPointer(cmdError));
	for (LatteCMDPtr p = cmdStart; p < cmdEnd; p += 4)
	{
		if(cmdError >= p && cmdError < (p+4) )
			cemuLog_log(LogType::Force, "0x{0:08x}: {1:08x} {2:08x} {3:08x} {4:08x} <<<<<", memory_getVirtualOffsetFromPointer(p), p[0], p[1], p[2], p[3]);
		else
			cemuLog_log(LogType::Force, "0x{0:08x}: {1:08x} {2:08x} {3:08x} {4:08x}", memory_getVirtualOffsetFromPointer(p), p[0], p[1], p[2], p[3]);
	}
	cemuLog_waitForFlush();
	cemu_assert_debug(false);
}

// any drawcalls issued without changing textures, framebuffers, shader or other complex states can be done quickly without having to reinitialize the entire pipeline state
// we implement this optimization by having an optimized version of LatteCP_processCommandBuffer, called right after drawcalls, which only implements commands that dont interfere with fast drawing. Other commands will cause this function to return to the complex parser
LatteCMDPtr LatteCP_processCommandBuffer_continuousDrawPass(LatteCMDPtr cmd, LatteCMDPtr cmdStart, LatteCMDPtr cmdEnd, DrawPassContext& drawPassCtx)
{
	cemu_assert_debug(drawPassCtx.isWithinDrawPass());
	// quit early if there are parameters set which are generally incompatible with fast drawing
	if (LatteGPUState.contextRegister[mmVGT_STRMOUT_EN] != 0)
	{
		drawPassCtx.endDrawPass();
		return cmd;
	}
	// check for other special states?

	while (cmd < cmdEnd)
	{
		LatteCMDPtr cmdBeforeCommand = cmd;
		uint32 itHeader = LatteReadCMD();
		uint32 itHeaderType = (itHeader >> 30) & 3;
		if (itHeaderType == 3)
		{
			uint32 itCode = (itHeader >> 8) & 0xFF;
			uint32 nWords = ((itHeader >> 16) & 0x3FFF) + 1;
			switch (itCode)
			{
			case IT_SET_RESOURCE: // attribute buffers, uniform buffers or texture units
			{
				cmd = LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_RESOURCE>(cmd, nWords, [&drawPassCtx](uint32 registerStart, uint32 registerEnd)
				{
					if (registerStart >= Latte::REGADDR::SQ_TEX_RESOURCE_WORD_FIRST && registerStart <= Latte::REGADDR::SQ_TEX_RESOURCE_WORD_LAST)
						drawPassCtx.endDrawPass(); // texture updates end the current draw sequence
					else if (registerStart >= mmSQ_VTX_ATTRIBUTE_BLOCK_START && registerEnd <= mmSQ_VTX_ATTRIBUTE_BLOCK_END)
						drawPassCtx.notifyModifiedVertexBuffer();
					else
						drawPassCtx.notifyModifiedUniformBuffer();
				});
				if (!drawPassCtx.isWithinDrawPass())
					return cmd;
				break;
			}
			case IT_SET_ALU_CONST: // uniform register
			{
				cmd = LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_ALU_CONST>(cmd, nWords);
				break;
			}
			case IT_SET_CTL_CONST:
			{
				cmd = LatteCP_itSetRegistersGeneric<mmSQ_VTX_BASE_VTX_LOC>(cmd, nWords);
				break;
			}
			case IT_SET_CONFIG_REG:
			{
				cmd = LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_CONFIG>(cmd, nWords);
				break;
			}
			case IT_INDEX_TYPE:
			{
				cmd = LatteCP_itIndexType(cmd, nWords);
				break;
			}
			case IT_NUM_INSTANCES:
			{
				cmd = LatteCP_itNumInstances(cmd, nWords);
				break;
			}
			case IT_DRAW_INDEX_2:
			{
#ifdef FAST_DRAW_LOGGING
				if(GetAsyncKeyState('A'))
					forceLogRemoveMe_printf("Minimal draw");
#endif
				cmd = LatteCP_itDrawIndex2(cmd, nWords, drawPassCtx);
				break;
			}
			case IT_SET_CONTEXT_REG:
			{
#ifdef FAST_DRAW_LOGGING
				if (GetAsyncKeyState('A'))
					forceLogRemoveMe_printf("[FAST-DRAW] Quit due to command IT_SET_CONTEXT_REG Reg: %04x", (uint32)cmd[0] + 0xA000);
#endif
				drawPassCtx.endDrawPass();
				return cmdBeforeCommand;
			}
			case IT_INDIRECT_BUFFER_PRIV:
			{
				cmd = LatteCP_itIndirectBuffer(cmd, nWords, drawPassCtx);
				if (!drawPassCtx.isWithinDrawPass())
					return cmd;
				break;
			}
			default:
#ifdef FAST_DRAW_LOGGING
				if (GetAsyncKeyState('A'))
					forceLogRemoveMe_printf("[FAST-DRAW] Quit due to command itCode 0x%02x", itCode);
#endif
				drawPassCtx.endDrawPass();
				return cmdBeforeCommand;
			}
		}
		else if (itHeaderType == 2)
		{
			// filler packet
		}
		else
		{
#ifdef FAST_DRAW_LOGGING
			if (GetAsyncKeyState('A'))
				forceLogRemoveMe_printf("[FAST-DRAW] Quit due to unsupported headerType 0x%02x", itHeaderType);
#endif
			drawPassCtx.endDrawPass();
			return cmdBeforeCommand;
		}
	}
	cemu_assert_debug(drawPassCtx.isWithinDrawPass());
	return cmd;
}

void LatteCP_processCommandBuffer(uint8* cmdBuffer, sint32 cmdSize, DrawPassContext& drawPassCtx)
{
	LatteCMDPtr cmd = (LatteCMDPtr)cmdBuffer;
	LatteCMDPtr cmdStart = (LatteCMDPtr)cmdBuffer;
	LatteCMDPtr cmdEnd = (LatteCMDPtr)(cmdBuffer + cmdSize);

	if (drawPassCtx.isWithinDrawPass())
	{
		cmd = LatteCP_processCommandBuffer_continuousDrawPass(cmd, cmdStart, cmdEnd, drawPassCtx);
		cemu_assert_debug(cmd <= cmdEnd);
		if (cmd == cmdEnd)
			return;
		cemu_assert_debug(!drawPassCtx.isWithinDrawPass());
	}

	while (cmd < cmdEnd)
	{
		uint32 itHeader = LatteReadCMD();
		uint32 itHeaderType = (itHeader >> 30) & 3;
		if (itHeaderType == 3)
		{
			uint32 itCode = (itHeader >> 8) & 0xFF;
			uint32 nWords = ((itHeader >> 16) & 0x3FFF) + 1;
#ifdef CEMU_DEBUG_ASSERT
			LatteCMDPtr expectedPostCmd = cmd + nWords;
#endif
			switch (itCode)
			{
			case IT_SET_CONTEXT_REG:
			{
				cmd = LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_CONTEXT>(cmd, nWords);
			}
			break;
			case IT_SET_RESOURCE:
			{
				cmd = LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_RESOURCE>(cmd, nWords);
			}
			break;
			case IT_SET_ALU_CONST:
			{
				cmd = LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_ALU_CONST>(cmd, nWords);
			}
			break;
			case IT_SET_CTL_CONST:
			{
				cmd = LatteCP_itSetRegistersGeneric<mmSQ_VTX_BASE_VTX_LOC>(cmd, nWords);
			}
			break;
			case IT_SET_SAMPLER:
			{
				cmd = LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_SAMPLER>(cmd, nWords);
			}
			break;
			case IT_SET_CONFIG_REG:
			{
				cmd = LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_CONFIG>(cmd, nWords);
			}
			break;
			case IT_SET_LOOP_CONST:
			{
				LatteSkipCMD(nWords);
				// todo
			}
			break;
			case IT_SURFACE_SYNC:
			{
				cmd = LatteCP_itSurfaceSync(cmd);
			}
			break;
			case IT_INDIRECT_BUFFER_PRIV:
			{
				cmd = LatteCP_itIndirectBuffer(cmd, nWords, drawPassCtx);
				if (drawPassCtx.isWithinDrawPass())
				{
					cmd = LatteCP_processCommandBuffer_continuousDrawPass(cmd, cmdStart, cmdEnd, drawPassCtx);
					cemu_assert_debug(cmd <= cmdEnd);
					if (cmd == cmdEnd)
						return;
					cemu_assert_debug(!drawPassCtx.isWithinDrawPass());
				}
#ifdef CEMU_DEBUG_ASSERT
				expectedPostCmd = cmd;
#endif
			}
			break;
			case IT_STRMOUT_BUFFER_UPDATE:
			{
				cmd = LatteCP_itStreamoutBufferUpdate(cmd, nWords);
			}
			break;
			case IT_INDEX_TYPE:
			{
				cmd = LatteCP_itIndexType(cmd, nWords);
			}
			break;
			case IT_NUM_INSTANCES:
			{
				cmd = LatteCP_itNumInstances(cmd, nWords);
			}
			break;
			case IT_DRAW_INDEX_2:
			{
				drawPassCtx.beginDrawPass();
#ifdef FAST_DRAW_LOGGING
				if (GetAsyncKeyState('A'))
					forceLogRemoveMe_printf("[FAST-DRAW] Starting");
#endif
				cmd = LatteCP_itDrawIndex2(cmd, nWords, drawPassCtx);
				cmd = LatteCP_processCommandBuffer_continuousDrawPass(cmd, cmdStart, cmdEnd, drawPassCtx);
				cemu_assert_debug(cmd == cmdEnd || drawPassCtx.isWithinDrawPass() == false); // draw sequence should have ended if we didn't reach the end of the command buffer
#ifdef CEMU_DEBUG_ASSERT
				expectedPostCmd = cmd;
#endif
			}
			break;
			case IT_DRAW_INDEX_AUTO:
			{
				drawPassCtx.beginDrawPass();
				cmd = LatteCP_itDrawIndexAuto(cmd, nWords, drawPassCtx);
				cmd = LatteCP_processCommandBuffer_continuousDrawPass(cmd, cmdStart, cmdEnd, drawPassCtx);
				cemu_assert_debug(cmd == cmdEnd || drawPassCtx.isWithinDrawPass() == false); // draw sequence should have ended if we didn't reach the end of the command buffer
#ifdef CEMU_DEBUG_ASSERT
				expectedPostCmd = cmd;
#endif
#ifdef FAST_DRAW_LOGGING
				if (GetAsyncKeyState('A'))
					forceLogRemoveMe_printf("[FAST-DRAW] Auto-draw");
#endif
			}
			break;
			case IT_DRAW_INDEX_IMMD:
			{
				DrawPassContext drawPassCtx;
				drawPassCtx.beginDrawPass();
				cmd = LatteCP_itDrawImmediate(cmd, nWords, drawPassCtx);
				drawPassCtx.endDrawPass();
				break;
			}
			case IT_WAIT_REG_MEM:
			{
				cmd = LatteCP_itWaitRegMem(cmd, nWords);
				LatteTiming_HandleTimedVsync();
				LatteAsyncCommands_checkAndExecute();
			}
			break;
			case IT_MEM_WRITE:
			{
				cmd = LatteCP_itMemWrite(cmd, nWords);
			}
			break;
			case IT_CONTEXT_CONTROL:
			{
				cmd = LatteCP_itContextControl(cmd, nWords);
			}
			break;
			case IT_MEM_SEMAPHORE:
			{
				cmd = LatteCP_itMemSemaphore(cmd, nWords);
			}
			break;
			case IT_LOAD_CONFIG_REG:
			{
				cmd = LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_CONFIG);
			}
			break;
			case IT_LOAD_CONTEXT_REG:
			{
				cmd = LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_CONTEXT);
			}
			break;
			case IT_LOAD_ALU_CONST:
			{
				cmd = LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_ALU_CONST);
			}
			break;
			case IT_LOAD_LOOP_CONST:
			{
				cmd = LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_LOOP_CONST);
			}
			break;
			case IT_LOAD_RESOURCE:
			{
				cmd = LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_RESOURCE);
			}
			break;
			case IT_LOAD_SAMPLER:
			{
				cmd = LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_SAMPLER);
			}
			break;
			case IT_SET_PREDICATION:
			{
				cmd = LatteCP_itSetPredication(cmd, nWords);
			}
			break;
			case IT_HLE_COPY_COLORBUFFER_TO_SCANBUFFER:
			{
				cmd = LatteCP_itHLECopyColorBufferToScanBuffer(cmd, nWords);
			}
			break;
			case IT_HLE_TRIGGER_SCANBUFFER_SWAP:
			{
				cmd = LatteCP_itHLESwapScanBuffer(cmd, nWords);
			}
			break;
			case IT_HLE_WAIT_FOR_FLIP:
			{
				cmd = LatteCP_itHLEWaitForFlip(cmd, nWords);
			}
			break;
			case IT_HLE_REQUEST_SWAP_BUFFERS:
			{
				cmd = LatteCP_itHLERequestSwapBuffers(cmd, nWords);
			}
			break;
			case IT_HLE_CLEAR_COLOR_DEPTH_STENCIL:
			{
				cmd = LatteCP_itHLEClearColorDepthStencil(cmd, nWords);
			}
			break;
			case IT_HLE_COPY_SURFACE_NEW:
			{
				cmd = LatteCP_itHLECopySurfaceNew(cmd, nWords);
			}
			break;
			case IT_HLE_SAMPLE_TIMER:
			{
				cmd = LatteCP_itHLESampleTimer(cmd, nWords);
			}
			break;
			case IT_HLE_SPECIAL_STATE:
			{
				cmd = LatteCP_itHLESpecialState(cmd, nWords);
			}
			break;
			case IT_HLE_BEGIN_OCCLUSION_QUERY:
			{
				cmd = LatteCP_itHLEBeginOcclusionQuery(cmd, nWords);
			}
			break;
			case IT_HLE_END_OCCLUSION_QUERY:
			{
				cmd = LatteCP_itHLEEndOcclusionQuery(cmd, nWords);
			}
			break;
			case IT_HLE_SET_CB_RETIREMENT_TIMESTAMP:
			{
				cmd = LatteCP_itHLESetRetirementTimestamp(cmd, nWords);
			}
			break;
			case IT_HLE_BOTTOM_OF_PIPE_CB:
			{
				cmd = LatteCP_itHLEBottomOfPipeCB(cmd, nWords);
			}
			break;
			case IT_HLE_SYNC_ASYNC_OPERATIONS:
			{
				LatteSkipCMD(nWords);
				LatteTextureReadback_UpdateFinishedTransfers(true);
				LatteQuery_UpdateFinishedQueriesForceFinishAll();
			}
			break;
			default:
				debug_printf("Unhandled IT %02x\n", itCode);
				cemu_assert_debug(false);
				LatteSkipCMD(nWords);	
			}
#ifdef CEMU_DEBUG_ASSERT
			if(cmd != expectedPostCmd)
				debug_printf("cmd %016p expectedPostCmd %016p\n", cmd, expectedPostCmd);
			cemu_assert_debug(cmd == expectedPostCmd);
#endif
		}
		else if (itHeaderType == 2)
		{
			// filler packet
			// has no body
		}
		else if (itHeaderType == 0)
		{
			uint32 registerBase = (itHeader & 0xFFFF);
			uint32 registerCount = ((itHeader >> 16) & 0x3FFF) + 1;
			if (registerBase == 0x304A)
			{
				GX2::__GX2NotifyEvent(GX2::GX2CallbackEventType::TIMESTAMP_TOP);
				LatteSkipCMD(registerCount);
			}
			else if (registerBase == 0x304B)
			{
				LatteSkipCMD(registerCount);
			}
			else
			{
				LatteCP_dumpCommandBufferError(cmdStart, cmdEnd, cmd);
				cemu_assert_debug(false);
			}
		}
		else
		{
			debug_printf("invalid itHeaderType %08x\n", itHeaderType);
			LatteCP_dumpCommandBufferError(cmdStart, cmdEnd, cmd);
			cemu_assert_debug(false);
		}
	}
	cemu_assert_debug(cmd == cmdEnd);
}

void LatteCP_ProcessRingbuffer()
{
	sint32 timerRecheck = 0; // estimates how much CP processing time passed based on the executed commands, if the value exceeds CP_TIMER_RECHECK then _handleTimers() is called
	while (true)
	{
		uint32 itHeader = LatteCP_readU32Deprc();
		uint32 itHeaderType = (itHeader >> 30) & 3;
		if (itHeaderType == 3)
		{
			uint32 itCode = (itHeader >> 8) & 0xFF;
			uint32 nWords = ((itHeader >> 16) & 0x3FFF) + 1;
			LatteCP_waitForNWords(nWords);
			LatteCMDPtr cmd = (LatteCMDPtr)gxRingBufferReadPtr;
			uint8* expectedGxRingBufferReadPtr = gxRingBufferReadPtr + nWords*4;
			switch (itCode)
			{
			case IT_SURFACE_SYNC:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itSurfaceSync(cmd);
				timerRecheck += CP_TIMER_RECHECK / 512;
			}
			break;
			case IT_SET_CONTEXT_REG:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_CONTEXT>(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
			}
			break;
			case IT_SET_RESOURCE:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_RESOURCE>(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
			}
			break;
			case IT_SET_ALU_CONST:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_ALU_CONST>(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_SET_CTL_CONST:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itSetRegistersGeneric<mmSQ_VTX_BASE_VTX_LOC>(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_SET_SAMPLER:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_SAMPLER>(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_SET_CONFIG_REG:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itSetRegistersGeneric<LATTE_REG_BASE_CONFIG>(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_INDIRECT_BUFFER_PRIV:
			{
#ifdef FAST_DRAW_LOGGING
				if (GetAsyncKeyState('A'))
					forceLogRemoveMe_printf("[FAST-DRAW] BEGIN CMD BUFFER");
#endif
				LatteCP_itIndirectBufferDepr<LatteCP_readU32Deprc>(nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
#ifdef FAST_DRAW_LOGGING
				if (GetAsyncKeyState('A'))
					forceLogRemoveMe_printf("[FAST-DRAW] END CMD BUFFER");
#endif
				break;
			}
			case IT_STRMOUT_BUFFER_UPDATE:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itStreamoutBufferUpdate(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_INDEX_TYPE:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itIndexType(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 1024;
				break;
			}
			case IT_NUM_INSTANCES:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itNumInstances(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 1024;
				break;
			}
			case IT_DRAW_INDEX_2:
			{
				DrawPassContext drawPassCtx;
				drawPassCtx.beginDrawPass();
				gxRingBufferReadPtr = (uint8*)LatteCP_itDrawIndex2(cmd, nWords, drawPassCtx);
				drawPassCtx.endDrawPass();
				timerRecheck += CP_TIMER_RECHECK / 64;
				break;
			}
			case IT_DRAW_INDEX_AUTO:
			{
				DrawPassContext drawPassCtx;
				drawPassCtx.beginDrawPass();
				gxRingBufferReadPtr = (uint8*)LatteCP_itDrawIndexAuto(cmd, nWords, drawPassCtx);
				drawPassCtx.endDrawPass();
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_DRAW_INDEX_IMMD:
			{
				DrawPassContext drawPassCtx;
				drawPassCtx.beginDrawPass();
				gxRingBufferReadPtr = (uint8*)LatteCP_itDrawImmediate(cmd, nWords, drawPassCtx);
				drawPassCtx.endDrawPass();
				timerRecheck += CP_TIMER_RECHECK / 64;
				break;
			}
			case IT_WAIT_REG_MEM:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itWaitRegMem(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 16;
				break;
			}
			case IT_MEM_WRITE:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itMemWrite(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 128;
				break;
			}
			case IT_CONTEXT_CONTROL:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itContextControl(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 128;
				break;
			}
			case IT_MEM_SEMAPHORE:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itMemSemaphore(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 128;
				break;
			}
			case IT_LOAD_CONFIG_REG:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_CONFIG);
				timerRecheck += CP_TIMER_RECHECK / 64;
				break;
			}
			case IT_LOAD_CONTEXT_REG:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_CONTEXT);
				timerRecheck += CP_TIMER_RECHECK / 64;
				break;
			}
			case IT_LOAD_ALU_CONST:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_ALU_CONST);
				timerRecheck += CP_TIMER_RECHECK / 64;
				break;
			}
			case IT_LOAD_LOOP_CONST:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_LOOP_CONST);
				timerRecheck += CP_TIMER_RECHECK / 64;
				break;
			}
			case IT_LOAD_RESOURCE:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_RESOURCE);
				timerRecheck += CP_TIMER_RECHECK / 64;
				break;
			}
			case IT_LOAD_SAMPLER:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itLoadReg(cmd, nWords, LATTE_REG_BASE_SAMPLER);
				timerRecheck += CP_TIMER_RECHECK / 64;
				break;
			}
			case IT_SET_LOOP_CONST:
			{
				LatteSkipCMD(nWords);
				gxRingBufferReadPtr = (uint8*)cmd;
				// todo
				break;
			}
			case IT_SET_PREDICATION:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itSetPredication(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_HLE_COPY_COLORBUFFER_TO_SCANBUFFER:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLECopyColorBufferToScanBuffer(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 64;
				break;
			}
			case IT_HLE_TRIGGER_SCANBUFFER_SWAP:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLESwapScanBuffer(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 64;
				break;
			}
			case IT_HLE_WAIT_FOR_FLIP:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLEWaitForFlip(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 1;
				break;
			}
			case IT_HLE_REQUEST_SWAP_BUFFERS:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLERequestSwapBuffers(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 32;
				break;
			}
			case IT_HLE_CLEAR_COLOR_DEPTH_STENCIL:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLEClearColorDepthStencil(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 128;
				break;
			}
			case IT_HLE_COPY_SURFACE_NEW:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLECopySurfaceNew(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 128;
				break;
			}
			case IT_HLE_FIFO_WRAP_AROUND:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLEFifoWrapAround(cmd, nWords);
				expectedGxRingBufferReadPtr = gxRingBufferReadPtr;
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_HLE_SAMPLE_TIMER:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLESampleTimer(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_HLE_SPECIAL_STATE:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLESpecialState(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_HLE_BEGIN_OCCLUSION_QUERY:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLEBeginOcclusionQuery(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_HLE_END_OCCLUSION_QUERY:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLEEndOcclusionQuery(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_HLE_SET_CB_RETIREMENT_TIMESTAMP:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLESetRetirementTimestamp(cmd, nWords);
				timerRecheck += CP_TIMER_RECHECK / 512;
				break;
			}
			case IT_HLE_BOTTOM_OF_PIPE_CB:
			{
				gxRingBufferReadPtr = (uint8*)LatteCP_itHLEBottomOfPipeCB(cmd, nWords);
				break;
			}
			case IT_HLE_SYNC_ASYNC_OPERATIONS:
			{
				LatteCP_skipWords<LatteCP_readU32Deprc>(nWords);
				LatteTextureReadback_UpdateFinishedTransfers(true);
				LatteQuery_UpdateFinishedQueriesForceFinishAll();
				break;
			}
			default:
				cemu_assert_debug(false);
			}
			cemu_assert_debug(expectedGxRingBufferReadPtr == gxRingBufferReadPtr);
		}
		else if (itHeaderType == 2)
		{
			// filler packet, skip this
			cemu_assert_debug(itHeader == 0x80000000);
		}
		else if (itHeaderType == 0)
		{
			uint32 registerBase = (itHeader & 0xFFFF);
			uint32 registerCount = ((itHeader >> 16) & 0x3FFF) + 1;
			if (registerBase == 0x304A)
			{
				GX2::__GX2NotifyEvent(GX2::GX2CallbackEventType::TIMESTAMP_TOP);
				LatteCP_skipWords<LatteCP_readU32Deprc>(registerCount);
			}
			else if (registerBase == 0x304B)
			{
				LatteCP_skipWords<LatteCP_readU32Deprc>(registerCount);
			}
			else
			{
				cemu_assert_debug(false);
			}
		}
		else
		{
			debug_printf("invalid itHeaderType %08x\n", itHeaderType);
			cemu_assert_debug(false);
		}
		if (timerRecheck >= CP_TIMER_RECHECK)
		{
			LatteTiming_HandleTimedVsync();
			LatteAsyncCommands_checkAndExecute();
			timerRecheck = 0;
		}
	}
}
