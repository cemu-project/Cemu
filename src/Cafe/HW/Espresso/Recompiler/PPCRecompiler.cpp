#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "PPCFunctionBoundaryTracker.h"
#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "Cafe/OS/RPL/rpl.h"
#include "util/containers/RangeStore.h"
#include "Cafe/OS/libs/coreinit/coreinit_CodeGen.h"
#include "config/ActiveSettings.h"
#include "config/LaunchSettings.h"
#include "Common/ExceptionHandler/ExceptionHandler.h"
#include "Common/cpu_features.h"
#include "util/helpers/fspinlock.h"
#include "util/helpers/helpers.h"
#include "util/MemMapper/MemMapper.h"

#include "IML/IML.h"
#include "IML/IMLRegisterAllocator.h"
#include "BackendX64/BackendX64.h"
#include "util/highresolutiontimer/HighResolutionTimer.h"

#define PPCREC_FORCE_SYNCHRONOUS_COMPILATION	0 // if 1, then function recompilation will block and execute on the thread that called PPCRecompiler_visitAddressNoBlock
#define PPCREC_LOG_RECOMPILATION_RESULTS		0

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

#if PPCREC_FORCE_SYNCHRONOUS_COMPILATION
static std::mutex s_singleRecompilationMutex;
#endif

bool ppcRecompilerEnabled = false;

void PPCRecompiler_recompileAtAddress(uint32 address);

// this function does never block and can fail if the recompiler lock cannot be acquired immediately
void PPCRecompiler_visitAddressNoBlock(uint32 enterAddress)
{
#if PPCREC_FORCE_SYNCHRONOUS_COMPILATION
	if (ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4] != PPCRecompiler_leaveRecompilerCode_unvisited)
		return;
	PPCRecompilerState.recompilerSpinlock.lock();
	if (ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4] != PPCRecompiler_leaveRecompilerCode_unvisited)
	{
		PPCRecompilerState.recompilerSpinlock.unlock();
		return;
	}
	ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4] = PPCRecompiler_leaveRecompilerCode_visited;
	PPCRecompilerState.recompilerSpinlock.unlock();
	s_singleRecompilationMutex.lock();
	if (ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4] == PPCRecompiler_leaveRecompilerCode_visited)
	{
		PPCRecompiler_recompileAtAddress(enterAddress);
	}
	s_singleRecompilationMutex.unlock();
	return;
#endif
	// quick read-only check without lock
	if (ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4] != PPCRecompiler_leaveRecompilerCode_unvisited)
		return;
	// try to acquire lock
	if (!PPCRecompilerState.recompilerSpinlock.try_lock())
		return;
	auto funcPtr = ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4];
	if (funcPtr != PPCRecompiler_leaveRecompilerCode_unvisited)
	{
		// was visited since previous check
		PPCRecompilerState.recompilerSpinlock.unlock();
		return;
	}
	// add to recompilation queue and flag as visited
	PPCRecompilerState.targetQueue.emplace(enterAddress);
	ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4] = PPCRecompiler_leaveRecompilerCode_visited;

	PPCRecompilerState.recompilerSpinlock.unlock();
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
		cemuLog_log(LogType::Force, "Recompiler exit: 0x{:08x} LR: 0x{:08x} Timestamp {}.{:04}", hCPU->instructionPointer, hCPU->spr.LR, dur / 1000LL, (dur % 1000LL));
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
bool PPCRecompiler_ApplyIMLPasses(ppcImlGenContext_t& ppcImlGenContext);

PPCRecFunction_t* PPCRecompiler_recompileFunction(PPCFunctionBoundaryTracker::PPCRange_t range, std::set<uint32>& entryAddresses, std::vector<std::pair<MPTR, uint32>>& entryPointsOut, PPCFunctionBoundaryTracker& boundaryTracker)
{
	if (range.startAddress >= PPC_REC_CODE_AREA_END)
	{
		cemuLog_log(LogType::Force, "Attempting to recompile function outside of allowed code area");
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

#if PPCREC_LOG_RECOMPILATION_RESULTS
	BenchmarkTimer bt;
	bt.Start();
#endif

	// generate intermediate code
	ppcImlGenContext_t ppcImlGenContext = { 0 };
	ppcImlGenContext.debug_entryPPCAddress = range.startAddress;
	bool compiledSuccessfully = PPCRecompiler_generateIntermediateCode(ppcImlGenContext, ppcRecFunc, entryAddresses, boundaryTracker);
	if (compiledSuccessfully == false)
	{
		delete ppcRecFunc;
		return nullptr;
	}

	uint32 ppcRecLowerAddr = LaunchSettings::GetPPCRecLowerAddr();
	uint32 ppcRecUpperAddr = LaunchSettings::GetPPCRecUpperAddr();

	if (ppcRecLowerAddr != 0 && ppcRecUpperAddr != 0)
	{
		if (ppcRecFunc->ppcAddress < ppcRecLowerAddr || ppcRecFunc->ppcAddress > ppcRecUpperAddr)
		{
			delete ppcRecFunc;
			return nullptr;
		}
	}

	// apply passes
	if (!PPCRecompiler_ApplyIMLPasses(ppcImlGenContext))
	{
		delete ppcRecFunc;
		return nullptr;
	}

	// emit x64 code
	bool x64GenerationSuccess = PPCRecompiler_generateX64Code(ppcRecFunc, &ppcImlGenContext);
	if (x64GenerationSuccess == false)
	{
		return nullptr;
	}
	if (ActiveSettings::DumpRecompilerFunctionsEnabled())
	{
		FileStream* fs = FileStream::createFile2(ActiveSettings::GetUserDataPath(fmt::format("dump/recompiler/ppc_{:08x}.bin", ppcRecFunc->ppcAddress)));
		if (fs)
		{
			fs->writeData(ppcRecFunc->x86Code, ppcRecFunc->x86Size);
			delete fs;
		}
	}

	// collect list of PPC-->x64 entry points
	entryPointsOut.clear();
	for(IMLSegment* imlSegment : ppcImlGenContext.segmentList2)
	{
		if (imlSegment->isEnterable == false)
			continue;

		uint32 ppcEnterOffset = imlSegment->enterPPCAddress;
		uint32 x64Offset = imlSegment->x64Offset;

		entryPointsOut.emplace_back(ppcEnterOffset, x64Offset);
	}

#if PPCREC_LOG_RECOMPILATION_RESULTS
	bt.Stop();
	uint32 codeHash = 0;
	for (uint32 i = 0; i < ppcRecFunc->x86Size; i++)
	{
		codeHash = _rotr(codeHash, 3);
		codeHash += ((uint8*)ppcRecFunc->x86Code)[i];
	}
	cemuLog_log(LogType::Force, "[Recompiler] PPC 0x{:08x} -> x64: 0x{:x} Took {:.4}ms | Size {:04x} CodeHash {:08x}", (uint32)ppcRecFunc->ppcAddress, (uint64)(uintptr_t)ppcRecFunc->x86Code, bt.GetElapsedMilliseconds(), ppcRecFunc->x86Size, codeHash);
#endif

	return ppcRecFunc;
}

void PPCRecompiler_NativeRegisterAllocatorPass(ppcImlGenContext_t& ppcImlGenContext)
{
	IMLRegisterAllocatorParameters raParam;

	for (auto& it : ppcImlGenContext.mappedRegs)
		raParam.regIdToName.try_emplace(it.second.GetRegID(), it.first);

	auto& gprPhysPool = raParam.GetPhysRegPool(IMLRegFormat::I64);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_RAX);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_RDX);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_RBX);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_RBP);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_RSI);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_RDI);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_R8);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_R9);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_R10);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_R11);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_R12);
	gprPhysPool.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE + X86_REG_RCX);

	// add XMM registers, except XMM15 which is the temporary register
	auto& fprPhysPool = raParam.GetPhysRegPool(IMLRegFormat::F64);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 0);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 1);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 2);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 3);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 4);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 5);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 6);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 7);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 8);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 9);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 10);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 11);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 12);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 13);
	fprPhysPool.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE + 14);

	IMLRegisterAllocator_AllocateRegisters(&ppcImlGenContext, raParam);
}

bool PPCRecompiler_ApplyIMLPasses(ppcImlGenContext_t& ppcImlGenContext)
{
	// isolate entry points from function flow (enterable segments must not be the target of any other segment)
	// this simplifies logic during register allocation
	PPCRecompilerIML_isolateEnterableSegments(&ppcImlGenContext);

	// merge certain float load+store patterns
	IMLOptimizer_OptimizeDirectFloatCopies(&ppcImlGenContext);
	// delay byte swapping for certain load+store patterns
	IMLOptimizer_OptimizeDirectIntegerCopies(&ppcImlGenContext);

	IMLOptimizer_StandardOptimizationPass(ppcImlGenContext);

	PPCRecompiler_NativeRegisterAllocatorPass(ppcImlGenContext);

	return true;
}

bool PPCRecompiler_makeRecompiledFunctionActive(uint32 initialEntryPoint, PPCFunctionBoundaryTracker::PPCRange_t& range, PPCRecFunction_t* ppcRecFunc, std::vector<std::pair<MPTR, uint32>>& entryPoints)
{
	// update jump table
	PPCRecompilerState.recompilerSpinlock.lock();

	// check if the initial entrypoint is still flagged for recompilation
	// its possible that the range has been invalidated during the time it took to translate the function
	if (ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[initialEntryPoint / 4] != PPCRecompiler_leaveRecompilerCode_visited)
	{
		PPCRecompilerState.recompilerSpinlock.unlock();
		return false;
	}

	// check if the current range got invalidated during the time it took to recompile it
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
		PPCRecompilerState.recompilerSpinlock.unlock();
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
	PPCRecompilerState.recompilerSpinlock.unlock();


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
	PPCRecompilerState.recompilerSpinlock.lock();

	std::set<uint32> entryAddresses;

	entryAddresses.emplace(address);

	PPCRecompilerState.recompilerSpinlock.unlock();

	std::vector<std::pair<MPTR, uint32>> functionEntryPoints;
	auto func = PPCRecompiler_recompileFunction(range, entryAddresses, functionEntryPoints, funcBoundaries);

	if (!func)
	{
		return; // recompilation failed
	}
	bool r = PPCRecompiler_makeRecompiledFunctionActive(address, range, func, functionEntryPoints);
}

std::thread s_threadRecompiler;
std::atomic_bool s_recompilerThreadStopSignal{false};

void PPCRecompiler_thread()
{
	SetThreadName("PPCRecompiler");
#if PPCREC_FORCE_SYNCHRONOUS_COMPILATION
	return;
#endif

	while (true)
	{
        if(s_recompilerThreadStopSignal)
            return;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		// asynchronous recompilation:
		// 1) take address from queue
		// 2) check if address is still marked as visited
		// 3) if yes -> calculate size, gather all entry points, recompile and update jump table
		while (true)
		{
			PPCRecompilerState.recompilerSpinlock.lock();
			if (PPCRecompilerState.targetQueue.empty())
			{
				PPCRecompilerState.recompilerSpinlock.unlock();
				break;
			}
			auto enterAddress = PPCRecompilerState.targetQueue.front();
			PPCRecompilerState.targetQueue.pop();

			auto funcPtr = ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[enterAddress / 4];
			if (funcPtr != PPCRecompiler_leaveRecompilerCode_visited)
			{
				// only recompile functions if marked as visited
				PPCRecompilerState.recompilerSpinlock.unlock();
				continue;
			}
			PPCRecompilerState.recompilerSpinlock.unlock();

			PPCRecompiler_recompileAtAddress(enterAddress);
			if(s_recompilerThreadStopSignal)
				return;
		}
	}
}

#define PPC_REC_ALLOC_BLOCK_SIZE	(4*1024*1024) // 4MB

constexpr uint32 PPCRecompiler_GetNumAddressSpaceBlocks()
{
    return (MEMORY_CODEAREA_ADDR + MEMORY_CODEAREA_SIZE + PPC_REC_ALLOC_BLOCK_SIZE - 1) / PPC_REC_ALLOC_BLOCK_SIZE;
}

std::bitset<PPCRecompiler_GetNumAddressSpaceBlocks()> ppcRecompiler_reservedBlockMask;

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
		cemuLog_log(LogType::Force, "Failed to allocate memory for recompiler (0x{:08x})", offset);
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
	PPCRecompilerState.recompilerSpinlock.lock();
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
	PPCRecompilerState.recompilerSpinlock.unlock();
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
	cemu_assert_debug(PPCRecompilerState.recompilerSpinlock.is_locked());
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

	PPCRecompilerState.recompilerSpinlock.lock();

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

	PPCRecompilerState.recompilerSpinlock.unlock();
}

#if defined(ARCH_X86_64)
void PPCRecompiler_initPlatform()
{
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

	// mxcsr
	ppcRecompilerInstanceData->_x64XMM_mxCsr_ftzOn = 0x1F80 | 0x8000;
	ppcRecompilerInstanceData->_x64XMM_mxCsr_ftzOff = 0x1F80;
}
#else
void PPCRecompiler_initPlatform()
{
    
}
#endif

void PPCRecompiler_init()
{
	if (ActiveSettings::GetCPUMode() == CPUMode::SinglecoreInterpreter)
	{
		ppcRecompilerEnabled = false;
		return;
	}
	if (LaunchSettings::ForceInterpreter() || LaunchSettings::ForceMultiCoreInterpreter())
	{
		cemuLog_log(LogType::Force, "Recompiler disabled. Command line --force-interpreter or force-multicore-interpreter was passed");
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

    PPCRecompiler_allocateRange(0, 0x1000); // the first entry is used for fallback to interpreter
    PPCRecompiler_allocateRange(mmuRange_TRAMPOLINE_AREA.getBase(), mmuRange_TRAMPOLINE_AREA.getSize());
    PPCRecompiler_allocateRange(mmuRange_CODECAVE.getBase(), mmuRange_CODECAVE.getSize());

    PPCRecompiler_initPlatform();
    
	cemuLog_log(LogType::Force, "Recompiler initialized");

	ppcRecompilerEnabled = true;

	// launch recompilation thread
    s_recompilerThreadStopSignal = false;
    s_threadRecompiler = std::thread(PPCRecompiler_thread);
}

void PPCRecompiler_Shutdown()
{
    // shut down recompiler thread
    s_recompilerThreadStopSignal = true;
    if(s_threadRecompiler.joinable())
        s_threadRecompiler.join();
    // clean up queues
    while(!PPCRecompilerState.targetQueue.empty())
        PPCRecompilerState.targetQueue.pop();
    PPCRecompilerState.invalidationRanges.clear();
    // clean range store
    rangeStore_ppcRanges.clear();
    // clean up memory
    uint32 numBlocks = PPCRecompiler_GetNumAddressSpaceBlocks();
    for(uint32 i=0; i<numBlocks; i++)
    {
        if(!ppcRecompiler_reservedBlockMask[i])
            continue;
        // deallocate
        uint64 offset = i * PPC_REC_ALLOC_BLOCK_SIZE;
        MemMapper::FreeMemory(&(ppcRecompilerInstanceData->ppcRecompilerFuncTable[offset/4]), (PPC_REC_ALLOC_BLOCK_SIZE/4)*sizeof(void*), true);
        MemMapper::FreeMemory(&(ppcRecompilerInstanceData->ppcRecompilerDirectJumpTable[offset/4]), (PPC_REC_ALLOC_BLOCK_SIZE/4)*sizeof(void*), true);
        // mark as unmapped
        ppcRecompiler_reservedBlockMask[i] = false;
    }
}
