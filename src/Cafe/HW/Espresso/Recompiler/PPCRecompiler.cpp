#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "PPCFunctionBoundaryTracker.h"
#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "PPCRecompilerX64.h"
#include "Cafe/OS/RPL/rpl.h"
#include "util/containers/RangeStore.h"
#include "Cafe/OS/libs/coreinit/coreinit_CodeGen.h"
#include "config/ActiveSettings.h"
#include "config/LaunchSettings.h"

#include "util/helpers/fspinlock.h"
#include "Common/ExceptionHandler/ExceptionHandler.h"
#include "util/helpers/helpers.h"

#include "util/MemMapper/MemMapper.h"

struct PPCInvalidationRange
{
	MPTR startAddress;
	uint32 size;

	PPCInvalidationRange(MPTR _startAddress, uint32 _size) : startAddress(_startAddress), size(_size) {};
};

struct
{
	FSpinlock recompilerSpinlock;
	std::queue<MPTR> targetQueue;
	std::vector<PPCInvalidationRange> invalidationRanges;
}PPCRecompilerState;

RangeStore<PPCRecFunction_t*, uint32, 7703, 0x2000> rangeStore_ppcRanges;

void ATTR_MS_ABI (*PPCRecompiler_enterRecompilerCode)(uint64 codeMem, uint64 ppcInterpreterInstance);
void ATTR_MS_ABI (*PPCRecompiler_leaveRecompilerCode_visited)();
void ATTR_MS_ABI (*PPCRecompiler_leaveRecompilerCode_unvisited)();

PPCRecompilerInstanceData_t* ppcRecompilerInstanceData;

bool ppcRecompilerEnabled = false;

// this function does never block and can fail if the recompiler lock cannot be acquired immediately
void PPCRecompiler_visitAddressNoBlock(uint32 enterAddress)
{
	// quick read-only check without lock
	if (ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4] != PPCRecompiler_leaveRecompilerCode_unvisited)
		return;
	// try to acquire lock
	if (!PPCRecompilerState.recompilerSpinlock.tryAcquire())
		return;
	auto funcPtr = ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4];
	if (funcPtr != PPCRecompiler_leaveRecompilerCode_unvisited)
	{
		// was visited since previous check
		PPCRecompilerState.recompilerSpinlock.release();
		return;
	}
	// add to recompilation queue and flag as visited
	PPCRecompilerState.targetQueue.emplace(enterAddress);
	ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4] = PPCRecompiler_leaveRecompilerCode_visited;

	PPCRecompilerState.recompilerSpinlock.release();
}

void PPCRecompiler_recompileIfUnvisited(uint32 enterAddress)
{
	if (ppcRecompilerEnabled == false)
		return;
	PPCRecompiler_visitAddressNoBlock(enterAddress);
}

void PPCRecompiler_enter(PPCInterpreter_t* hCPU, PPCREC_JUMP_ENTRY funcPtr)
{
#if BOOST_OS_WINDOWS
	uint32 prevState = _controlfp(0, 0);
	_controlfp(_RC_NEAR, _MCW_RC);
	PPCRecompiler_enterRecompilerCode((uint64)funcPtr, (uint64)hCPU);
	_controlfp(prevState, _MCW_RC);
	// debug recompiler exit - useful to find frequently executed functions which couldn't be recompiled
	#ifdef CEMU_DEBUG_ASSERT
	if (hCPU->remainingCycles > 0 && GetAsyncKeyState(VK_F4))
	{
		auto t = std::chrono::high_resolution_clock::now();
		auto dur = std::chrono::duration_cast<std::chrono::microseconds>(t.time_since_epoch()).count();
		forceLog_printf("Recompiler exit: 0x%08x LR: 0x%08x Timestamp %lld.%04lld", hCPU->instructionPointer, hCPU->spr.LR, dur / 1000LL, (dur % 1000LL));
	}
	#endif
#else
	PPCRecompiler_enterRecompilerCode((uint64)funcPtr, (uint64)hCPU);
#endif
	// after leaving recompiler prematurely attempt to recompile the code at the new location
	if (hCPU->remainingCycles > 0)
	{
		PPCRecompiler_visitAddressNoBlock(hCPU->instructionPointer);
	}
}

void PPCRecompiler_attemptEnterWithoutRecompile(PPCInterpreter_t* hCPU, uint32 enterAddress)
{
	cemu_assert_debug(hCPU->instructionPointer == enterAddress);
	if (ppcRecompilerEnabled == false)
		return;
	auto funcPtr = ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4];
	if (funcPtr != PPCRecompiler_leaveRecompilerCode_unvisited && funcPtr != PPCRecompiler_leaveRecompilerCode_visited)
	{
		cemu_assert_debug(ppcRecompilerInstanceData != nullptr);
		PPCRecompiler_enter(hCPU, funcPtr);
	}
}

void PPCRecompiler_attemptEnter(PPCInterpreter_t* hCPU, uint32 enterAddress)
{
	cemu_assert_debug(hCPU->instructionPointer == enterAddress);
	if (ppcRecompilerEnabled == false)
		return;
	if (hCPU->remainingCycles <= 0)
		return;
	auto funcPtr = ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4];
	if (funcPtr == PPCRecompiler_leaveRecompilerCode_unvisited)
	{
		PPCRecompiler_visitAddressNoBlock(enterAddress);
	}
	else if (funcPtr != PPCRecompiler_leaveRecompilerCode_visited)
	{
		// enter
		cemu_assert_debug(ppcRecompilerInstanceData != nullptr);
		PPCRecompiler_enter(hCPU, funcPtr);
	}
}

PPCRecFunction_t* PPCRecompiler_recompileFunction(PPCFunctionBoundaryTracker::PPCRange_t range, std::set<uint32>& entryAddresses, std::vector<std::pair<MPTR, uint32>>& entryPointsOut)
{
	if (range.startAddress >= PPC_REC_CODE_AREA_END)
	{
		cemuLog_force("Attempting to recompile function outside of allowed code area");
		return nullptr;
	}

	uint32 codeGenRangeStart;
	uint32 codeGenRangeSize = 0;
	coreinit::OSGetCodegenVirtAddrRangeInternal(codeGenRangeStart, codeGenRangeSize);
	if (codeGenRangeSize != 0)
	{
		if (range.startAddress >= codeGenRangeStart && range.startAddress < (codeGenRangeStart + codeGenRangeSize))
		{
			if (coreinit::codeGenShouldAvoid())
			{
				return nullptr;
			}
		}
	}

	PPCRecFunction_t* ppcRecFunc = new PPCRecFunction_t();
	ppcRecFunc->ppcAddress = range.startAddress;
	ppcRecFunc->ppcSize = range.length;
	// generate intermediate code
	ppcImlGenContext_t ppcImlGenContext = { 0 };
	bool compiledSuccessfully = PPCRecompiler_generateIntermediateCode(ppcImlGenContext, ppcRecFunc, entryAddresses);
	if (compiledSuccessfully == false)
	{
		// todo: Free everything
		PPCRecompiler_freeContext(&ppcImlGenContext);
		delete ppcRecFunc;
		return NULL;
	}
	// emit x64 code
	bool x64GenerationSuccess = PPCRecompiler_generateX64Code(ppcRecFunc, &ppcImlGenContext);
	if (x64GenerationSuccess == false)
	{
		PPCRecompiler_freeContext(&ppcImlGenContext);
		return nullptr;
	}

	// collect list of PPC-->x64 entry points
	entryPointsOut.clear();
	for (sint32 s = 0; s < ppcImlGenContext.segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext.segmentList[s];
		if (imlSegment->isEnterable == false)
			continue;

		uint32 ppcEnterOffset = imlSegment->enterPPCAddress;
		uint32 x64Offset = imlSegment->x64Offset;

		entryPointsOut.emplace_back(ppcEnterOffset, x64Offset);
	}

	PPCRecompiler_freeContext(&ppcImlGenContext);
	return ppcRecFunc;
}

bool PPCRecompiler_makeRecompiledFunctionActive(uint32 initialEntryPoint, PPCFunctionBoundaryTracker::PPCRange_t& range, PPCRecFunction_t* ppcRecFunc, std::vector<std::pair<MPTR, uint32>>& entryPoints)
{
	// update jump table
	PPCRecompilerState.recompilerSpinlock.acquire();

	// check if the initial entrypoint is still flagged for recompilation
	// its possible that the range has been invalidated during the time it took to translate the function
	if (ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[initialEntryPoint / 4] != PPCRecompiler_leaveRecompilerCode_visited)
	{
		PPCRecompilerState.recompilerSpinlock.release();
		return false;
	}

	// check if the current range got invalidated in the time it took to recompile it
	bool isInvalidated = false;
	for (auto& invRange : PPCRecompilerState.invalidationRanges)
	{
		MPTR rStartAddr = invRange.startAddress;
		MPTR rEndAddr = rStartAddr + invRange.size;
		for (auto& recFuncRange : ppcRecFunc->list_ranges)
		{
			if (recFuncRange.ppcAddress < (rEndAddr) && (recFuncRange.ppcAddress + recFuncRange.ppcSize) >= rStartAddr)
			{
				isInvalidated = true;
				break;
			}
		}
	}
	PPCRecompilerState.invalidationRanges.clear();
	if (isInvalidated)
	{
		PPCRecompilerState.recompilerSpinlock.release();
		return false;
	}


	// update jump table
	for (auto& itr : entryPoints)
	{
		ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[itr.first / 4] = (PPCREC_JUMP_ENTRY)((uint8*)ppcRecFunc->x86Code + itr.second);
	}


	// due to inlining, some entrypoints can get optimized away
	// therefore we reset all addresses that are still marked as visited (but not recompiled)
	// we dont remove the points from the queue but any address thats not marked as visited won't get recompiled
	// if they are reachable, the interpreter will queue them again
	for (uint32 v = range.startAddress; v <= (range.startAddress + range.length); v += 4)
	{
		auto funcPtr = ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[v / 4];
		if (funcPtr == PPCRecompiler_leaveRecompilerCode_visited)
			ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[v / 4] = PPCRecompiler_leaveRecompilerCode_unvisited;
	}

	// register ranges
	for (auto& r : ppcRecFunc->list_ranges)
	{
		r.storedRange = rangeStore_ppcRanges.storeRange(ppcRecFunc, r.ppcAddress, r.ppcAddress + r.ppcSize);
	}
	PPCRecompilerState.recompilerSpinlock.release();


	return true;
}

void PPCRecompiler_recompileAtAddress(uint32 address)
{
	cemu_assert_debug(ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[address / 4] == PPCRecompiler_leaveRecompilerCode_visited);

	// get size
	PPCFunctionBoundaryTracker funcBoundaries;
	funcBoundaries.trackStartPoint(address);
	// get range that encompasses address
	PPCFunctionBoundaryTracker::PPCRange_t range;
	if (funcBoundaries.getRangeForAddress(address, range) == false)
	{
		cemu_assert_debug(false);
	}

	// todo - use info from previously compiled ranges to determine full size of this function (and merge all the entryAddresses)

	// collect all currently known entry points for this range
	PPCRecompilerState.recompilerSpinlock.acquire();

	std::set<uint32> entryAddresses;

	entryAddresses.emplace(address);

	PPCRecompilerState.recompilerSpinlock.release();

	std::vector<std::pair<MPTR, uint32>> functionEntryPoints;
	auto func = PPCRecompiler_recompileFunction(range, entryAddresses, functionEntryPoints);

	if (!func)
	{
		return; // recompilation failed
	}
	bool r = PPCRecompiler_makeRecompiledFunctionActive(address, range, func, functionEntryPoints);
}

void PPCRecompiler_thread()
{
	SetThreadName("PPCRecompiler_thread");
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		// asynchronous recompilation:
		// 1) take address from queue
		// 2) check if address is still marked as visited
		// 3) if yes -> calculate size, gather all entry points, recompile and update jump table
		while (true)
		{
			PPCRecompilerState.recompilerSpinlock.acquire();
			if (PPCRecompilerState.targetQueue.empty())
			{
				PPCRecompilerState.recompilerSpinlock.release();
				break;
			}
			auto enterAddress = PPCRecompilerState.targetQueue.front();
			PPCRecompilerState.targetQueue.pop();

			auto funcPtr = ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4];
			if (funcPtr != PPCRecompiler_leaveRecompilerCode_visited)
			{
				// only recompile functions if marked as visited
				PPCRecompilerState.recompilerSpinlock.release();
				continue;
			}
			PPCRecompilerState.recompilerSpinlock.release();

			PPCRecompiler_recompileAtAddress(enterAddress);
		}
	}
}

#define PPC_REC_ALLOC_BLOCK_SIZE	(4*1024*1024) // 4MB

std::bitset<(MEMORY_CODEAREA_ADDR + MEMORY_CODEAREA_SIZE) / PPC_REC_ALLOC_BLOCK_SIZE> ppcRecompiler_reservedBlockMask;

void PPCRecompiler_reserveLookupTableBlock(uint32 offset)
{
	uint32 blockIndex = offset / PPC_REC_ALLOC_BLOCK_SIZE;
	offset = blockIndex * PPC_REC_ALLOC_BLOCK_SIZE;

	if (ppcRecompiler_reservedBlockMask[blockIndex])
		return;
	ppcRecompiler_reservedBlockMask[blockIndex] = true;

	void* p1 = MemMapper::AllocateMemory(&(ppcRecompilerInstanceData->ppcRecompilerFuncTable[offset/4]), (PPC_REC_ALLOC_BLOCK_SIZE/4)*sizeof(void*), MemMapper::PAGE_PERMISSION::P_RW, true);
	void* p3 = MemMapper::AllocateMemory(&(ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[offset/4]), (PPC_REC_ALLOC_BLOCK_SIZE/4)*sizeof(void*), MemMapper::PAGE_PERMISSION::P_RW, true);
	if( !p1 || !p3 )
	{
		forceLog_printf("Failed to allocate memory for recompiler (0x%08x)", offset);
		cemu_assert(false);
		return;
	}
	for(uint32 i=0; i<PPC_REC_ALLOC_BLOCK_SIZE/4; i++)
	{
		ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[offset/4+i] = PPCRecompiler_leaveRecompilerCode_unvisited;
	}
}

void PPCRecompiler_allocateRange(uint32 startAddress, uint32 size)
{
	if (ppcRecompilerInstanceData == nullptr)
		return;
	uint32 endAddress = (startAddress + size + PPC_REC_ALLOC_BLOCK_SIZE - 1) & ~(PPC_REC_ALLOC_BLOCK_SIZE-1);
	startAddress = (startAddress) & ~(PPC_REC_ALLOC_BLOCK_SIZE-1);
	startAddress = std::min(startAddress, (uint32)MEMORY_CODEAREA_ADDR + MEMORY_CODEAREA_SIZE);
	endAddress = std::min(endAddress, (uint32)MEMORY_CODEAREA_ADDR + MEMORY_CODEAREA_SIZE);
	for (uint32 i = startAddress; i < endAddress; i += PPC_REC_ALLOC_BLOCK_SIZE)
	{
		PPCRecompiler_reserveLookupTableBlock(i);
	}
}

struct ppcRecompilerFuncRange_t
{
	MPTR	ppcStart;
	uint32  ppcSize;
	void*   x86Start;
	size_t  x86Size;
};

bool PPCRecompiler_findFuncRanges(uint32 addr, ppcRecompilerFuncRange_t* rangesOut, size_t* countInOut)
{
	PPCRecompilerState.recompilerSpinlock.acquire();
	size_t countIn = *countInOut;
	size_t countOut = 0;

	rangeStore_ppcRanges.findRanges(addr, addr + 4, [rangesOut, countIn, &countOut](uint32 start, uint32 end, PPCRecFunction_t* func)
	{
		if (countOut < countIn)
		{
			rangesOut[countOut].ppcStart = start;
			rangesOut[countOut].ppcSize = (end-start);
			rangesOut[countOut].x86Start = func->x86Code;
			rangesOut[countOut].x86Size = func->x86Size;
		}
		countOut++;
	}
	);
	PPCRecompilerState.recompilerSpinlock.release();
	*countInOut = countOut;
	if (countOut > countIn)
		return false;
	return true;
}

extern "C" DLLEXPORT uintptr_t * PPCRecompiler_getJumpTableBase()
{
	if (ppcRecompilerInstanceData == nullptr)
		return nullptr;
	return (uintptr_t*)ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable;
}

void PPCRecompiler_invalidateTableRange(uint32 offset, uint32 size)
{
	if (ppcRecompilerInstanceData == nullptr)
		return;
	for (uint32 i = 0; i < size / 4; i++)
	{
		ppcRecompilerInstanceData->ppcRecompilerFuncTable[offset / 4 + i] = nullptr;
		ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[offset / 4 + i] = PPCRecompiler_leaveRecompilerCode_unvisited;
	}
}

void PPCRecompiler_deleteFunction(PPCRecFunction_t* func)
{
	// assumes PPCRecompilerState.recompilerSpinlock is already held
	cemu_assert_debug(PPCRecompilerState.recompilerSpinlock.isHolding());
	for (auto& r : func->list_ranges)
	{
		PPCRecompiler_invalidateTableRange(r.ppcAddress, r.ppcSize);
		if(r.storedRange)
			rangeStore_ppcRanges.deleteRange(r.storedRange);
		r.storedRange = nullptr;
	}
	// todo - free x86 code
}

void PPCRecompiler_invalidateRange(uint32 startAddr, uint32 endAddr)
{
	if (ppcRecompilerEnabled == false)
		return;
	if (startAddr >= PPC_REC_CODE_AREA_SIZE)
		return;
	cemu_assert_debug(endAddr >= startAddr);

	PPCRecompilerState.recompilerSpinlock.acquire();

	uint32 rStart;
	uint32 rEnd;
	PPCRecFunction_t* rFunc;

	// mark range as unvisited
	for (uint64 currentAddr = (uint64)startAddr&~3; currentAddr < (uint64)(endAddr&~3); currentAddr += 4)
		ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[currentAddr / 4] = PPCRecompiler_leaveRecompilerCode_unvisited;

	// add entry to invalidation queue
	PPCRecompilerState.invalidationRanges.emplace_back(startAddr, endAddr-startAddr);


	while (rangeStore_ppcRanges.findFirstRange(startAddr, endAddr, rStart, rEnd, rFunc) )
	{
		PPCRecompiler_deleteFunction(rFunc);
	}

	PPCRecompilerState.recompilerSpinlock.release();
}

void PPCRecompiler_init()
{
	if (ActiveSettings::GetCPUMode() == CPUMode::SinglecoreInterpreter)
	{
		ppcRecompilerEnabled = false;
		return;
	}
	if (LaunchSettings::ForceInterpreter())
	{
		cemuLog_log(LogType::Force, "Recompiler disabled. Command line --force-interpreter was passed");
		return;
	}
	if (ppcRecompilerInstanceData)
	{
		MemMapper::FreeReservation(ppcRecompilerInstanceData, sizeof(PPCRecompilerInstanceData_t));
		ppcRecompilerInstanceData = nullptr;
	}
	debug_printf("Allocating %dMB for recompiler instance data...\n", (sint32)(sizeof(PPCRecompilerInstanceData_t) / 1024 / 1024));
	ppcRecompilerInstanceData = (PPCRecompilerInstanceData_t*)MemMapper::ReserveMemory(nullptr, sizeof(PPCRecompilerInstanceData_t), MemMapper::PAGE_PERMISSION::P_RW);
	MemMapper::AllocateMemory(&(ppcRecompilerInstanceData->_x64XMM_xorNegateMaskBottom), sizeof(PPCRecompilerInstanceData_t) - offsetof(PPCRecompilerInstanceData_t, _x64XMM_xorNegateMaskBottom), MemMapper::PAGE_PERMISSION::P_RW, true);
	PPCRecompilerX64Gen_generateRecompilerInterfaceFunctions();

	uint32 codeRegionEnd = RPLLoader_GetMaxCodeOffset();
	codeRegionEnd = (codeRegionEnd + PPC_REC_ALLOC_BLOCK_SIZE - 1) & ~(PPC_REC_ALLOC_BLOCK_SIZE - 1);

	uint32 codeRegionSize = codeRegionEnd - PPC_REC_CODE_AREA_START;
	forceLogDebug_printf("Allocating recompiler tables for range 0x%08x-0x%08x", PPC_REC_CODE_AREA_START, codeRegionEnd);

	for (uint32 i = 0; i < codeRegionSize; i += PPC_REC_ALLOC_BLOCK_SIZE)
	{
		PPCRecompiler_reserveLookupTableBlock(i);
	}

	// init x64 recompiler instance data
	ppcRecompilerInstanceData->_x64XMM_xorNegateMaskBottom[0] = 1ULL << 63ULL;
	ppcRecompilerInstanceData->_x64XMM_xorNegateMaskBottom[1] = 0ULL;
	ppcRecompilerInstanceData->_x64XMM_xorNegateMaskPair[0] = 1ULL << 63ULL;
	ppcRecompilerInstanceData->_x64XMM_xorNegateMaskPair[1] = 1ULL << 63ULL;
	ppcRecompilerInstanceData->_x64XMM_xorNOTMask[0] = 0xFFFFFFFFFFFFFFFFULL;
	ppcRecompilerInstanceData->_x64XMM_xorNOTMask[1] = 0xFFFFFFFFFFFFFFFFULL;
	ppcRecompilerInstanceData->_x64XMM_andAbsMaskBottom[0] = ~(1ULL << 63ULL);
	ppcRecompilerInstanceData->_x64XMM_andAbsMaskBottom[1] = ~0ULL;
	ppcRecompilerInstanceData->_x64XMM_andAbsMaskPair[0] = ~(1ULL << 63ULL);
	ppcRecompilerInstanceData->_x64XMM_andAbsMaskPair[1] = ~(1ULL << 63ULL);
	ppcRecompilerInstanceData->_x64XMM_andFloatAbsMaskBottom[0] = ~(1 << 31);
	ppcRecompilerInstanceData->_x64XMM_andFloatAbsMaskBottom[1] = 0xFFFFFFFF;
	ppcRecompilerInstanceData->_x64XMM_andFloatAbsMaskBottom[2] = 0xFFFFFFFF;
	ppcRecompilerInstanceData->_x64XMM_andFloatAbsMaskBottom[3] = 0xFFFFFFFF;
	ppcRecompilerInstanceData->_x64XMM_singleWordMask[0] = 0xFFFFFFFFULL;
	ppcRecompilerInstanceData->_x64XMM_singleWordMask[1] = 0ULL;
	ppcRecompilerInstanceData->_x64XMM_constDouble1_1[0] = 1.0;
	ppcRecompilerInstanceData->_x64XMM_constDouble1_1[1] = 1.0;
	ppcRecompilerInstanceData->_x64XMM_constDouble0_0[0] = 0.0;
	ppcRecompilerInstanceData->_x64XMM_constDouble0_0[1] = 0.0;
	ppcRecompilerInstanceData->_x64XMM_constFloat0_0[0] = 0.0f;
	ppcRecompilerInstanceData->_x64XMM_constFloat0_0[1] = 0.0f;
	ppcRecompilerInstanceData->_x64XMM_constFloat1_1[0] = 1.0f;
	ppcRecompilerInstanceData->_x64XMM_constFloat1_1[1] = 1.0f;
	*(uint32*)&ppcRecompilerInstanceData->_x64XMM_constFloatMin[0] = 0x00800000;
	*(uint32*)&ppcRecompilerInstanceData->_x64XMM_constFloatMin[1] = 0x00800000;
	ppcRecompilerInstanceData->_x64XMM_flushDenormalMask1[0] = 0x7F800000;
	ppcRecompilerInstanceData->_x64XMM_flushDenormalMask1[1] = 0x7F800000;
	ppcRecompilerInstanceData->_x64XMM_flushDenormalMask1[2] = 0x7F800000;
	ppcRecompilerInstanceData->_x64XMM_flushDenormalMask1[3] = 0x7F800000;
	ppcRecompilerInstanceData->_x64XMM_flushDenormalMaskResetSignBits[0] = ~0x80000000;
	ppcRecompilerInstanceData->_x64XMM_flushDenormalMaskResetSignBits[1] = ~0x80000000;
	ppcRecompilerInstanceData->_x64XMM_flushDenormalMaskResetSignBits[2] = ~0x80000000;
	ppcRecompilerInstanceData->_x64XMM_flushDenormalMaskResetSignBits[3] = ~0x80000000;

	// setup GQR scale tables

	for (uint32 i = 0; i < 32; i++)
	{
		float a = 1.0f / (float)(1u << i);
		float b = 0;
		if (i == 0)
			b = 4294967296.0f;
		else
			b = (float)(1u << (32u - i));

		float ar = (float)(1u << i);
		float br = 0;
		if (i == 0)
			br = 1.0f / 4294967296.0f;
		else
			br = 1.0f / (float)(1u << (32u - i));

		ppcRecompilerInstanceData->_psq_ld_scale_ps0_1[i * 2 + 0] = a;
		ppcRecompilerInstanceData->_psq_ld_scale_ps0_1[i * 2 + 1] = 1.0f;
		ppcRecompilerInstanceData->_psq_ld_scale_ps0_1[(i + 32) * 2 + 0] = b;
		ppcRecompilerInstanceData->_psq_ld_scale_ps0_1[(i + 32) * 2 + 1] = 1.0f;

		ppcRecompilerInstanceData->_psq_ld_scale_ps0_ps1[i * 2 + 0] = a;
		ppcRecompilerInstanceData->_psq_ld_scale_ps0_ps1[i * 2 + 1] = a;
		ppcRecompilerInstanceData->_psq_ld_scale_ps0_ps1[(i + 32) * 2 + 0] = b;
		ppcRecompilerInstanceData->_psq_ld_scale_ps0_ps1[(i + 32) * 2 + 1] = b;

		ppcRecompilerInstanceData->_psq_st_scale_ps0_1[i * 2 + 0] = ar;
		ppcRecompilerInstanceData->_psq_st_scale_ps0_1[i * 2 + 1] = 1.0f;
		ppcRecompilerInstanceData->_psq_st_scale_ps0_1[(i + 32) * 2 + 0] = br;
		ppcRecompilerInstanceData->_psq_st_scale_ps0_1[(i + 32) * 2 + 1] = 1.0f;

		ppcRecompilerInstanceData->_psq_st_scale_ps0_ps1[i * 2 + 0] = ar;
		ppcRecompilerInstanceData->_psq_st_scale_ps0_ps1[i * 2 + 1] = ar;
		ppcRecompilerInstanceData->_psq_st_scale_ps0_ps1[(i + 32) * 2 + 0] = br;
		ppcRecompilerInstanceData->_psq_st_scale_ps0_ps1[(i + 32) * 2 + 1] = br;
	}

	// mxcsr
	ppcRecompilerInstanceData->_x64XMM_mxCsr_ftzOn = 0x1F80 | 0x8000;
	ppcRecompilerInstanceData->_x64XMM_mxCsr_ftzOff = 0x1F80;

	// query processor extensions
	int cpuInfo[4];
	cpuid(cpuInfo, 0x80000001);
	hasLZCNTSupport = ((cpuInfo[2] >> 5) & 1) != 0;
	cpuid(cpuInfo, 0x1);
	hasMOVBESupport = ((cpuInfo[2] >> 22) & 1) != 0;
	hasAVXSupport = ((cpuInfo[2] >> 28) & 1) != 0;
	cpuidex(cpuInfo, 0x7, 0);
	hasBMI2Support = ((cpuInfo[1] >> 8) & 1) != 0;

	forceLog_printf("Recompiler initialized. CPU extensions: %s%s%s", hasLZCNTSupport ? "LZCNT " : "", hasMOVBESupport ? "MOVBE " : "", hasAVXSupport ? "AVX " : "");

	ppcRecompilerEnabled = true;

	// launch recompilation thread
	std::thread t_recompiler(PPCRecompiler_thread);
	t_recompiler.detach();
}
