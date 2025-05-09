#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/Espresso/Recompiler/IML/IML.h"
#include "Cafe/HW/Espresso/Recompiler/IML/IMLInstruction.h"

#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"
#include "../BackendX64/BackendX64.h"

#include "Common/FileStream.h"

#include <boost/container/static_vector.hpp>
#include <boost/container/small_vector.hpp>

IMLReg _FPRRegFromID(IMLRegID regId)
{
	return IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, regId);
}

void PPCRecompiler_optimizeDirectFloatCopiesScanForward(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 imlIndexLoad, IMLReg fprReg)
{
	IMLRegID fprIndex = fprReg.GetRegID();

	IMLInstruction* imlInstructionLoad = imlSegment->imlList.data() + imlIndexLoad;
	if (imlInstructionLoad->op_storeLoad.flags2.notExpanded)
		return;
	boost::container::static_vector<sint32, 4> trackedMoves; // only track up to 4 copies
	IMLUsedRegisters registersUsed;
	sint32 scanRangeEnd = std::min<sint32>(imlIndexLoad + 25, imlSegment->imlList.size()); // don't scan too far (saves performance and also the chances we can merge the load+store become low at high distances)
	bool foundMatch = false;
	sint32 lastStore = -1;
	for (sint32 i = imlIndexLoad + 1; i < scanRangeEnd; i++)
	{
		IMLInstruction* imlInstruction = imlSegment->imlList.data() + i;
		if (imlInstruction->IsSuffixInstruction())
			break;
		// check if FPR is stored
		if ((imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE && imlInstruction->op_storeLoad.mode == PPCREC_FPR_ST_MODE_SINGLE) ||
			(imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE_INDEXED && imlInstruction->op_storeLoad.mode == PPCREC_FPR_ST_MODE_SINGLE))
		{
			if (imlInstruction->op_storeLoad.registerData.GetRegID() == fprIndex)
			{
				if (foundMatch == false)
				{
					// flag the load-single instruction as "don't expand" (leave single value as-is)
					imlInstructionLoad->op_storeLoad.flags2.notExpanded = true;
				}
				// also set the flag for the store instruction
				IMLInstruction* imlInstructionStore = imlInstruction;
				imlInstructionStore->op_storeLoad.flags2.notExpanded = true;

				foundMatch = true;
				lastStore = i + 1;

				continue;
			}
		}
		// if the FPR is copied then keep track of it. We can expand the copies instead of the original
		if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R && imlInstruction->operation == PPCREC_IML_OP_FPR_ASSIGN && imlInstruction->op_fpr_r_r.regA.GetRegID() == fprIndex)
		{
			if (imlInstruction->op_fpr_r_r.regR.GetRegID() == fprIndex)
			{
				// unexpected no-op
				break;
			}
			if (trackedMoves.size() >= trackedMoves.capacity())
			{
				// we cant track any more moves, expand here
				lastStore = i;
				break;
			}
			trackedMoves.push_back(i);
			continue;
		}
		// check if FPR is overwritten
		imlInstruction->CheckRegisterUsage(&registersUsed);
		if (registersUsed.writtenGPR1.IsValidAndSameRegID(fprIndex) || registersUsed.writtenGPR2.IsValidAndSameRegID(fprIndex))
			break;
		if (registersUsed.readGPR1.IsValidAndSameRegID(fprIndex))
			break;
		if (registersUsed.readGPR2.IsValidAndSameRegID(fprIndex))
			break;
		if (registersUsed.readGPR3.IsValidAndSameRegID(fprIndex))
			break;
		if (registersUsed.readGPR4.IsValidAndSameRegID(fprIndex))
			break;
	}

	if (foundMatch)
	{
		// insert expand instructions for each target register of a move
		sint32 positionBias = 0;
		for (auto& trackedMove : trackedMoves)
		{
			sint32 realPosition = trackedMove + positionBias;
			IMLInstruction* imlMoveInstruction = imlSegment->imlList.data() + realPosition;
			if (realPosition >= lastStore)
				break; // expand is inserted before this move
			else
				lastStore++;

			cemu_assert_debug(imlMoveInstruction->type == PPCREC_IML_TYPE_FPR_R_R && imlMoveInstruction->op_fpr_r_r.regA.GetRegID() == fprIndex);
			cemu_assert_debug(imlMoveInstruction->op_fpr_r_r.regA.GetRegFormat() == IMLRegFormat::F64);
			auto dstReg = imlMoveInstruction->op_fpr_r_r.regR;
			IMLInstruction* newExpand = PPCRecompiler_insertInstruction(imlSegment, realPosition+1); // one after the move
			newExpand->make_fpr_r(PPCREC_IML_OP_FPR_EXPAND_F32_TO_F64, dstReg);
			positionBias++;
		}
		// insert expand instruction after store
		IMLInstruction* newExpand = PPCRecompiler_insertInstruction(imlSegment, lastStore);
		newExpand->make_fpr_r(PPCREC_IML_OP_FPR_EXPAND_F32_TO_F64, _FPRRegFromID(fprIndex));
	}
}

/*
* Scans for patterns:
* <Load sp float into register f>
* <Random unrelated instructions>
* <Store sp float from register f>
* For these patterns the store and load is modified to work with un-extended values (float remains as float, no double conversion)
* The float->double extension is then executed later
* Advantages:
* Keeps denormals and other special float values intact
* Slightly improves performance
*/
void IMLOptimizer_OptimizeDirectFloatCopies(ppcImlGenContext_t* ppcImlGenContext)
{
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		for (sint32 i = 0; i < segIt->imlList.size(); i++)
		{
			IMLInstruction* imlInstruction = segIt->imlList.data() + i;
			if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD && imlInstruction->op_storeLoad.mode == PPCREC_FPR_LD_MODE_SINGLE)
			{
				PPCRecompiler_optimizeDirectFloatCopiesScanForward(ppcImlGenContext, segIt, i, imlInstruction->op_storeLoad.registerData);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED && imlInstruction->op_storeLoad.mode == PPCREC_FPR_LD_MODE_SINGLE)
			{
				PPCRecompiler_optimizeDirectFloatCopiesScanForward(ppcImlGenContext, segIt, i, imlInstruction->op_storeLoad.registerData);
			}
		}
	}
}

void PPCRecompiler_optimizeDirectIntegerCopiesScanForward(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 imlIndexLoad, IMLReg gprReg)
{
	cemu_assert_debug(gprReg.GetBaseFormat() == IMLRegFormat::I64); // todo - proper handling required for non-standard sizes
	cemu_assert_debug(gprReg.GetRegFormat() == IMLRegFormat::I32);

	IMLRegID gprIndex = gprReg.GetRegID();
	IMLInstruction* imlInstructionLoad = imlSegment->imlList.data() + imlIndexLoad;
	if ( imlInstructionLoad->op_storeLoad.flags2.swapEndian == false )
		return;
	bool foundMatch = false;
	IMLUsedRegisters registersUsed;
	sint32 scanRangeEnd = std::min<sint32>(imlIndexLoad + 25, imlSegment->imlList.size()); // don't scan too far (saves performance and also the chances we can merge the load+store become low at high distances)
	sint32 i = imlIndexLoad + 1;
	for (; i < scanRangeEnd; i++)
	{
		IMLInstruction* imlInstruction = imlSegment->imlList.data() + i;
		if (imlInstruction->IsSuffixInstruction())
			break;
		// check if GPR is stored
		if ((imlInstruction->type == PPCREC_IML_TYPE_STORE && imlInstruction->op_storeLoad.copyWidth == 32 ) )
		{
			if (imlInstruction->op_storeLoad.registerMem.GetRegID() == gprIndex)
				break;
			if (imlInstruction->op_storeLoad.registerData.GetRegID() == gprIndex)
			{
				IMLInstruction* imlInstructionStore = imlInstruction;
				if (foundMatch == false)
				{
					// switch the endian swap flag for the load instruction
					imlInstructionLoad->op_storeLoad.flags2.swapEndian = !imlInstructionLoad->op_storeLoad.flags2.swapEndian;
					foundMatch = true;
				}
				// switch the endian swap flag for the store instruction
				imlInstructionStore->op_storeLoad.flags2.swapEndian = !imlInstructionStore->op_storeLoad.flags2.swapEndian;
				// keep scanning
				continue;
			}
		}
		// check if GPR is accessed
		imlInstruction->CheckRegisterUsage(&registersUsed);
		if (registersUsed.readGPR1.IsValidAndSameRegID(gprIndex) ||
			registersUsed.readGPR2.IsValidAndSameRegID(gprIndex) ||
			registersUsed.readGPR3.IsValidAndSameRegID(gprIndex))
		{
			break;
		}
		if (registersUsed.IsBaseGPRWritten(gprReg))
			return; // GPR overwritten, we don't need to byte swap anymore
	}
	if (foundMatch)
	{
		PPCRecompiler_insertInstruction(imlSegment, i)->make_r_r(PPCREC_IML_OP_ENDIAN_SWAP, gprReg, gprReg);
	}
}

/*
* Scans for patterns:
* <Load sp integer into register r>
* <Random unrelated instructions>
* <Store sp integer from register r>
* For these patterns the store and load is modified to work with non-swapped values
* The big_endian->little_endian conversion is then executed later
* Advantages:
* Slightly improves performance
*/
void IMLOptimizer_OptimizeDirectIntegerCopies(ppcImlGenContext_t* ppcImlGenContext)
{
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		for (sint32 i = 0; i < segIt->imlList.size(); i++)
		{
			IMLInstruction* imlInstruction = segIt->imlList.data() + i;
			if (imlInstruction->type == PPCREC_IML_TYPE_LOAD && imlInstruction->op_storeLoad.copyWidth == 32 && imlInstruction->op_storeLoad.flags2.swapEndian )
			{
				PPCRecompiler_optimizeDirectIntegerCopiesScanForward(ppcImlGenContext, segIt, i, imlInstruction->op_storeLoad.registerData);
			}
		}
	}
}

IMLName PPCRecompilerImlGen_GetRegName(ppcImlGenContext_t* ppcImlGenContext, IMLReg reg);

sint32 _getGQRIndexFromRegister(ppcImlGenContext_t* ppcImlGenContext, IMLReg gqrReg)
{
	if (gqrReg.IsInvalid())
		return -1;
	sint32 namedReg = PPCRecompilerImlGen_GetRegName(ppcImlGenContext, gqrReg);
	if (namedReg >= (PPCREC_NAME_SPR0 + SPR_UGQR0) && namedReg <= (PPCREC_NAME_SPR0 + SPR_UGQR7))
	{
		return namedReg - (PPCREC_NAME_SPR0 + SPR_UGQR0);
	}
	else
	{
		cemu_assert_suspicious();
	}
	return -1;
}

bool PPCRecompiler_isUGQRValueKnown(ppcImlGenContext_t* ppcImlGenContext, sint32 gqrIndex, uint32& gqrValue)
{
	// the default configuration is:
	// UGQR0 = 0x00000000
	// UGQR2 = 0x00040004
	// UGQR3 = 0x00050005
	// UGQR4 = 0x00060006
	// UGQR5 = 0x00070007
	// but games are free to modify UGQR2 to UGQR7 it seems.
	// no game modifies UGQR0 so it's safe enough to optimize for the default value
	// Ideally we would do some kind of runtime tracking and second recompilation to create fast paths for PSQ_L/PSQ_ST but thats todo
	if (gqrIndex == 0)
		gqrValue = 0x00000000;
	else
		return false;
	return true;
}

// analyses register dependencies across the entire function
// per segment this will generate information about which registers need to be preserved and which ones don't (e.g. are overwritten)
class IMLOptimizerRegIOAnalysis
{
  public:
	// constructor with segment pointer list as span
	IMLOptimizerRegIOAnalysis(std::span<IMLSegment*> segmentList, uint32 maxRegId) : m_segmentList(segmentList), m_maxRegId(maxRegId)
	{
		m_segRegisterInOutList.resize(segmentList.size());
	}

	struct IMLSegmentRegisterInOut
	{
		// todo - since our register ID range is usually pretty small (<64) we could use integer bitmasks to accelerate this? There is a helper class used in RA code already
		std::unordered_set<IMLRegID> regWritten; // registers which are modified in this segment
		std::unordered_set<IMLRegID> regImported; // registers which are read in this segment before they are written (importing value from previous segments)
		std::unordered_set<IMLRegID> regForward; // registers which are not read or written in this segment, but are imported into a later segment (propagated info)
	};

	// calculate which registers are imported (read-before-written) and forwarded (read-before-written by a later segment) per segment
	// then in a second step propagate the dependencies across linked segments
	void ComputeDepedencies()
	{
		std::vector<IMLSegmentRegisterInOut>& segRegisterInOutList = m_segRegisterInOutList;
		IMLSegmentRegisterInOut* segIO = segRegisterInOutList.data();
		uint32 index = 0;
		for(auto& seg : m_segmentList)
		{
			seg->momentaryIndex = index;
			index++;
			for(auto& instr : seg->imlList)
			{
				IMLUsedRegisters registerUsage;
				instr.CheckRegisterUsage(&registerUsage);
				// registers are considered imported if they are read before being written in this seg
				registerUsage.ForEachReadGPR([&](IMLReg gprReg) {
					IMLRegID gprId = gprReg.GetRegID();
					if (!segIO->regWritten.contains(gprId))
					{
						segIO->regImported.insert(gprId);
					}
				});
				registerUsage.ForEachWrittenGPR([&](IMLReg gprReg) {
					IMLRegID gprId = gprReg.GetRegID();
					segIO->regWritten.insert(gprId);
				});
			}
			segIO++;
		}
		// for every exit segment, import all registers
		for(auto& seg : m_segmentList)
		{
			if (!seg->nextSegmentIsUncertain)
				continue;
			if(seg->deadCodeEliminationHintSeg)
				continue;
			IMLSegmentRegisterInOut& segIO = segRegisterInOutList[seg->momentaryIndex];
			for(uint32 i=0; i<=m_maxRegId; i++)
			{
				segIO.regImported.insert((IMLRegID)i);
			}
		}
		// broadcast dependencies across segment chains
		std::unordered_set<uint32> segIdsWhichNeedUpdate;
		for (uint32 i = 0; i < m_segmentList.size(); i++)
		{
			segIdsWhichNeedUpdate.insert(i);
		}
		while(!segIdsWhichNeedUpdate.empty())
		{
			auto firstIt = segIdsWhichNeedUpdate.begin();
			uint32 segId = *firstIt;
			segIdsWhichNeedUpdate.erase(firstIt);
			// forward regImported and regForward to earlier segments into their regForward, unless the register is written
			auto& curSeg = m_segmentList[segId];
			IMLSegmentRegisterInOut& curSegIO = segRegisterInOutList[segId];
			for(auto& prevSeg : curSeg->list_prevSegments)
			{
				IMLSegmentRegisterInOut& prevSegIO = segRegisterInOutList[prevSeg->momentaryIndex];
				bool prevSegChanged = false;
				for(auto& regId : curSegIO.regImported)
				{
					if (!prevSegIO.regWritten.contains(regId))
						prevSegChanged |= prevSegIO.regForward.insert(regId).second;
				}
				for(auto& regId : curSegIO.regForward)
				{
					if (!prevSegIO.regWritten.contains(regId))
						prevSegChanged |= prevSegIO.regForward.insert(regId).second;
				}
				if(prevSegChanged)
					segIdsWhichNeedUpdate.insert(prevSeg->momentaryIndex);
			}
			// same for hint links
			for(auto& prevSeg : curSeg->list_deadCodeHintBy)
			{
				IMLSegmentRegisterInOut& prevSegIO = segRegisterInOutList[prevSeg->momentaryIndex];
				bool prevSegChanged = false;
				for(auto& regId : curSegIO.regImported)
				{
					if (!prevSegIO.regWritten.contains(regId))
						prevSegChanged |= prevSegIO.regForward.insert(regId).second;
				}
				for(auto& regId : curSegIO.regForward)
				{
					if (!prevSegIO.regWritten.contains(regId))
						prevSegChanged |= prevSegIO.regForward.insert(regId).second;
				}
				if(prevSegChanged)
					segIdsWhichNeedUpdate.insert(prevSeg->momentaryIndex);
			}
		}
	}

	std::unordered_set<IMLRegID> GetRegistersNeededAtEndOfSegment(IMLSegment& seg)
	{
		std::unordered_set<IMLRegID> regsNeeded;
		if(seg.nextSegmentIsUncertain)
		{
			if(seg.deadCodeEliminationHintSeg)
			{
				auto& nextSegIO = m_segRegisterInOutList[seg.deadCodeEliminationHintSeg->momentaryIndex];
				regsNeeded.insert(nextSegIO.regImported.begin(), nextSegIO.regImported.end());
				regsNeeded.insert(nextSegIO.regForward.begin(), nextSegIO.regForward.end());
			}
			else
			{
				// add all regs
				for(uint32 i = 0; i <= m_maxRegId; i++)
					regsNeeded.insert(i);
			}
			return regsNeeded;
		}
		if(seg.nextSegmentBranchTaken)
		{
			auto& nextSegIO = m_segRegisterInOutList[seg.nextSegmentBranchTaken->momentaryIndex];
			regsNeeded.insert(nextSegIO.regImported.begin(), nextSegIO.regImported.end());
			regsNeeded.insert(nextSegIO.regForward.begin(), nextSegIO.regForward.end());
		}
		if(seg.nextSegmentBranchNotTaken)
		{
			auto& nextSegIO = m_segRegisterInOutList[seg.nextSegmentBranchNotTaken->momentaryIndex];
			regsNeeded.insert(nextSegIO.regImported.begin(), nextSegIO.regImported.end());
			regsNeeded.insert(nextSegIO.regForward.begin(), nextSegIO.regForward.end());
		}
		return regsNeeded;
	}

	bool IsRegisterNeededAtEndOfSegment(IMLSegment& seg, IMLRegID regId)
	{
		if(seg.nextSegmentIsUncertain)
		{
			if(!seg.deadCodeEliminationHintSeg)
				return true;
			auto& nextSegIO = m_segRegisterInOutList[seg.deadCodeEliminationHintSeg->momentaryIndex];
			if(nextSegIO.regImported.contains(regId))
				return true;
			if(nextSegIO.regForward.contains(regId))
				return true;
			return false;
		}
		if(seg.nextSegmentBranchTaken)
		{
			auto& nextSegIO = m_segRegisterInOutList[seg.nextSegmentBranchTaken->momentaryIndex];
			if(nextSegIO.regImported.contains(regId))
				return true;
			if(nextSegIO.regForward.contains(regId))
				return true;
		}
		if(seg.nextSegmentBranchNotTaken)
		{
			auto& nextSegIO = m_segRegisterInOutList[seg.nextSegmentBranchNotTaken->momentaryIndex];
			if(nextSegIO.regImported.contains(regId))
				return true;
			if(nextSegIO.regForward.contains(regId))
				return true;
		}
		return false;
	}

  private:
	std::span<IMLSegment*> m_segmentList;
	uint32 m_maxRegId;

	std::vector<IMLSegmentRegisterInOut> m_segRegisterInOutList;

};

// scan backwards starting from index and return the index of the first found instruction which writes to the given register (by id)
sint32 IMLUtil_FindInstructionWhichWritesRegister(IMLSegment& seg, sint32 startIndex, IMLReg reg, sint32 maxScanDistance = -1)
{
	sint32 endIndex = std::max<sint32>(startIndex - maxScanDistance, 0);
	for (sint32 i = startIndex; i >= endIndex; i--)
	{
		IMLInstruction& imlInstruction = seg.imlList[i];
		IMLUsedRegisters registersUsed;
		imlInstruction.CheckRegisterUsage(&registersUsed);
		if (registersUsed.IsBaseGPRWritten(reg))
			return i;
	}
	return -1;
}

// returns true if the instruction can safely be moved while keeping ordering constraints and data dependencies intact
// initialIndex is inclusive, targetIndex is exclusive
bool IMLUtil_CanMoveInstructionTo(IMLSegment& seg, sint32 initialIndex, sint32 targetIndex)
{
	boost::container::static_vector<IMLRegID, 8> regsWritten;
	boost::container::static_vector<IMLRegID, 8> regsRead;
	// get list of read and written registers
	IMLUsedRegisters registersUsed;
	seg.imlList[initialIndex].CheckRegisterUsage(&registersUsed);
	registersUsed.ForEachAccessedGPR([&](IMLReg reg, bool isWritten) {
		if (isWritten)
			regsWritten.push_back(reg.GetRegID());
		else
			regsRead.push_back(reg.GetRegID());
	});
	// check all the instructions inbetween
	if(initialIndex < targetIndex)
	{
		sint32 scanStartIndex = initialIndex+1; // +1 to skip the moving instruction itself
		sint32 scanEndIndex = targetIndex;
		for (sint32 i = scanStartIndex; i < scanEndIndex; i++)
		{
			IMLUsedRegisters registersUsed;
			seg.imlList[i].CheckRegisterUsage(&registersUsed);
			// in order to be able to move an instruction past another instruction, any of the read registers must not be modified (written)
			// and any of it's written registers must not be read
			bool canMove = true;
			registersUsed.ForEachAccessedGPR([&](IMLReg reg, bool isWritten) {
				IMLRegID regId = reg.GetRegID();
				if (!isWritten)
					canMove = canMove && std::find(regsWritten.begin(), regsWritten.end(), regId) == regsWritten.end();
				else
					canMove = canMove && std::find(regsRead.begin(), regsRead.end(), regId) == regsRead.end();
			});
			if(!canMove)
				return false;
		}
	}
	else
	{
		cemu_assert_unimplemented(); // backwards scan is todo
		return false;
	}
	return true;
}

sint32 IMLUtil_CountRegisterReadsInRange(IMLSegment& seg, sint32 scanStartIndex, sint32 scanEndIndex, IMLRegID regId)
{
	cemu_assert_debug(scanStartIndex <= scanEndIndex);
	cemu_assert_debug(scanEndIndex < seg.imlList.size());
	sint32 count = 0;
	for (sint32 i = scanStartIndex; i <= scanEndIndex; i++)
	{
		IMLUsedRegisters registersUsed;
		seg.imlList[i].CheckRegisterUsage(&registersUsed);
		registersUsed.ForEachReadGPR([&](IMLReg reg) {
			if (reg.GetRegID() == regId)
				count++;
		});
	}
	return count;
}

// move instruction from one index to another
// instruction will be inserted before the instruction at targetIndex
// returns the new instruction index of the moved instruction
sint32 IMLUtil_MoveInstructionTo(IMLSegment& seg, sint32 initialIndex, sint32 targetIndex)
{
	cemu_assert_debug(initialIndex != targetIndex);
	IMLInstruction temp = seg.imlList[initialIndex];
	if (initialIndex < targetIndex)
	{
		cemu_assert_debug(targetIndex > 0);
		targetIndex--;
		for(size_t i=initialIndex; i<targetIndex; i++)
			seg.imlList[i] = seg.imlList[i+1];
		seg.imlList[targetIndex] = temp;
		return targetIndex;
	}
	else
	{
		cemu_assert_unimplemented(); // testing needed
		std::copy(seg.imlList.begin() + targetIndex, seg.imlList.begin() + initialIndex, seg.imlList.begin() + targetIndex + 1);
		seg.imlList[targetIndex] = temp;
		return targetIndex;
	}
}

// x86 specific
bool IMLOptimizerX86_ModifiesEFlags(IMLInstruction& inst)
{
	// this is a very conservative implementation. There are more cases but this is good enough for now
	if(inst.type == PPCREC_IML_TYPE_NAME_R || inst.type == PPCREC_IML_TYPE_R_NAME)
		return false;
	if((inst.type == PPCREC_IML_TYPE_R_R || inst.type == PPCREC_IML_TYPE_R_S32) && inst.operation == PPCREC_IML_OP_ASSIGN)
		return false;
	return true; // if we dont know for sure, assume it does
}

void IMLOptimizer_DebugPrintSeg(ppcImlGenContext_t& ppcImlGenContext, IMLSegment& seg)
{
	printf("----------------\n");
	IMLDebug_DumpSegment(&ppcImlGenContext, &seg);
	fflush(stdout);
}

void IMLOptimizer_RemoveDeadCodeFromSegment(IMLOptimizerRegIOAnalysis& regIoAnalysis, IMLSegment& seg)
{
	// algorithm works like this:
	// Calculate which registers need to be preserved at the end of each segment
	// Then for each segment:
	// - Iterate instructions backwards
	// - Maintain a list of registers which are read at a later point (initially this is the list from the first step)
	// - If an instruction only modifies registers which are not in the read list and has no side effects, then it is dead code and can be replaced with a no-op

	std::unordered_set<IMLRegID> regsNeeded = regIoAnalysis.GetRegistersNeededAtEndOfSegment(seg);

	// start with suffix instruction
	if(seg.HasSuffixInstruction())
	{
		IMLInstruction& imlInstruction = seg.imlList[seg.GetSuffixInstructionIndex()];
		IMLUsedRegisters registersUsed;
		imlInstruction.CheckRegisterUsage(&registersUsed);
		registersUsed.ForEachWrittenGPR([&](IMLReg reg) {
			regsNeeded.erase(reg.GetRegID());
		});
		registersUsed.ForEachReadGPR([&](IMLReg reg) {
			regsNeeded.insert(reg.GetRegID());
		});
	}
	// iterate instructions backwards
	for (sint32 i = seg.imlList.size() - (seg.HasSuffixInstruction() ? 2:1); i >= 0; i--)
	{
		IMLInstruction& imlInstruction = seg.imlList[i];
		IMLUsedRegisters registersUsed;
		imlInstruction.CheckRegisterUsage(&registersUsed);
		// register read -> remove from overwritten list
		// register written -> add to overwritten list

		// check if this instruction only writes registers which will never be read
		bool onlyWritesRedundantRegisters = true;
		registersUsed.ForEachWrittenGPR([&](IMLReg reg) {
			if (regsNeeded.contains(reg.GetRegID()))
				onlyWritesRedundantRegisters = false;
		});
		// check if any of the written registers are read after this point
		registersUsed.ForEachWrittenGPR([&](IMLReg reg) {
			regsNeeded.erase(reg.GetRegID());
		});
		registersUsed.ForEachReadGPR([&](IMLReg reg) {
			regsNeeded.insert(reg.GetRegID());
		});
		if(!imlInstruction.HasSideEffects() && onlyWritesRedundantRegisters)
		{
			imlInstruction.make_no_op();
		}
	}
}

void IMLOptimizerX86_SubstituteCJumpForEflagsJump(IMLOptimizerRegIOAnalysis& regIoAnalysis, IMLSegment& seg)
{
	// convert and optimize bool condition jumps to eflags condition jumps
	// - Moves eflag setter (e.g. cmp) closer to eflags consumer (conditional jump) if necessary. If not possible but required then exit early
	// - Since we only rely on eflags, the boolean register can be optimized out if DCE considers it unused
	// - Further detect and optimize patterns like DEC + CMP + JCC into fused ops (todo)

	// check if this segment ends with a conditional jump
	if(!seg.HasSuffixInstruction())
		return;
	sint32 cjmpInstIndex = seg.GetSuffixInstructionIndex();
	if(cjmpInstIndex < 0)
		return;
	IMLInstruction& cjumpInstr = seg.imlList[cjmpInstIndex];
	if( cjumpInstr.type != PPCREC_IML_TYPE_CONDITIONAL_JUMP )
		return;
	IMLReg regCondBool = cjumpInstr.op_conditional_jump.registerBool;
	bool invertedCondition = !cjumpInstr.op_conditional_jump.mustBeTrue;
	// find the instruction which sets the bool
	sint32 cmpInstrIndex = IMLUtil_FindInstructionWhichWritesRegister(seg, cjmpInstIndex-1, regCondBool, 20);
	if(cmpInstrIndex < 0)
		return;
	// check if its an instruction combo which can be optimized (currently only cmp + cjump) and get the condition
	IMLInstruction& condSetterInstr = seg.imlList[cmpInstrIndex];
	IMLCondition cond;
	if(condSetterInstr.type == PPCREC_IML_TYPE_COMPARE)
		cond = condSetterInstr.op_compare.cond;
	else if(condSetterInstr.type == PPCREC_IML_TYPE_COMPARE_S32)
		cond = condSetterInstr.op_compare_s32.cond;
	else
		return;
	// check if instructions inbetween modify eflags
	sint32 indexEflagsSafeStart = -1; // index of the first instruction which does not modify eflags up to cjump
	for(sint32 i = cjmpInstIndex-1; i > cmpInstrIndex; i--)
	{
		if(IMLOptimizerX86_ModifiesEFlags(seg.imlList[i]))
		{
			indexEflagsSafeStart = i+1;
			break;
		}
	}
	if(indexEflagsSafeStart >= 0)
	{
		cemu_assert(indexEflagsSafeStart > 0);
		// there are eflags-modifying instructions inbetween the bool setter and cjump
		// try to move the eflags setter close enough to the cjump (to indexEflagsSafeStart)
		bool canMove = IMLUtil_CanMoveInstructionTo(seg, cmpInstrIndex, indexEflagsSafeStart);
		if(!canMove)
		{
			return;
		}
		else
		{
			cmpInstrIndex = IMLUtil_MoveInstructionTo(seg, cmpInstrIndex, indexEflagsSafeStart);
		}
	}
	// we can turn the jump into an eflags jump
	cjumpInstr.make_x86_eflags_jcc(cond, invertedCondition);

	if (IMLUtil_CountRegisterReadsInRange(seg, cmpInstrIndex, cjmpInstIndex, regCondBool.GetRegID()) > 1 || regIoAnalysis.IsRegisterNeededAtEndOfSegment(seg, regCondBool.GetRegID()))
		return; // bool register is used beyond the CMP, we can't drop it

	auto& cmpInstr = seg.imlList[cmpInstrIndex];
	cemu_assert_debug(cmpInstr.type == PPCREC_IML_TYPE_COMPARE || cmpInstr.type == PPCREC_IML_TYPE_COMPARE_S32);
	if(cmpInstr.type == PPCREC_IML_TYPE_COMPARE)
	{
		IMLReg regA = cmpInstr.op_compare.regA;
		IMLReg regB = cmpInstr.op_compare.regB;
		seg.imlList[cmpInstrIndex].make_r_r(PPCREC_IML_OP_X86_CMP, regA, regB);
	}
	else
	{
		IMLReg regA = cmpInstr.op_compare_s32.regA;
		sint32 val = cmpInstr.op_compare_s32.immS32;
		seg.imlList[cmpInstrIndex].make_r_s32(PPCREC_IML_OP_X86_CMP, regA, val);
	}

}

void IMLOptimizer_StandardOptimizationPassForSegment(IMLOptimizerRegIOAnalysis& regIoAnalysis, IMLSegment& seg)
{
	IMLOptimizer_RemoveDeadCodeFromSegment(regIoAnalysis, seg);

	// x86 specific optimizations
	IMLOptimizerX86_SubstituteCJumpForEflagsJump(regIoAnalysis, seg); // this pass should be applied late since it creates invisible eflags dependencies (which would break further register dependency analysis)
}

void IMLOptimizer_StandardOptimizationPass(ppcImlGenContext_t& ppcImlGenContext)
{
	IMLOptimizerRegIOAnalysis regIoAnalysis(ppcImlGenContext.segmentList2, ppcImlGenContext.GetMaxRegId());
	regIoAnalysis.ComputeDepedencies();
	for (IMLSegment* segIt : ppcImlGenContext.segmentList2)
	{
		IMLOptimizer_StandardOptimizationPassForSegment(regIoAnalysis, *segIt);
	}
}
