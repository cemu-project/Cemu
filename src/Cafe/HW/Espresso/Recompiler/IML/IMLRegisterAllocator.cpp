#include "IML.h"

#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"
#include "IMLRegisterAllocator.h"
#include "IMLRegisterAllocatorRanges.h"

#include "../BackendX64/BackendX64.h"

#include <boost/container/static_vector.hpp>
#include <boost/container/small_vector.hpp>

struct IMLRARegAbstractLiveness // preliminary liveness info. One entry per register and segment
{
	IMLRARegAbstractLiveness(IMLRegFormat regBaseFormat, sint32 usageStart, sint32 usageEnd) : regBaseFormat(regBaseFormat), usageStart(usageStart), usageEnd(usageEnd) {};

	void TrackInstruction(sint32 index)
	{
		usageStart = std::min<sint32>(usageStart, index);
		usageEnd = std::max<sint32>(usageEnd, index + 1); // exclusive index
	}

	sint32 usageStart;
	sint32 usageEnd;
	bool isProcessed{false};
	IMLRegFormat regBaseFormat;
};

struct IMLRegisterAllocatorContext
{
	IMLRegisterAllocatorParameters* raParam;
	ppcImlGenContext_t* deprGenContext; // deprecated. Try to decouple IMLRA from other parts of IML/PPCRec

	std::unordered_map<IMLRegID, IMLRegFormat> regIdToBaseFormat; // a vector would be more efficient but it also means that reg ids have to be continuous and not completely arbitrary
	// first pass
	std::vector<std::unordered_map<IMLRegID, IMLRARegAbstractLiveness>> perSegmentAbstractRanges;
	// second pass

	// helper methods
	inline std::unordered_map<IMLRegID, IMLRARegAbstractLiveness>& GetSegmentAbstractRangeMap(IMLSegment* imlSegment)
	{
		return perSegmentAbstractRanges[imlSegment->momentaryIndex];
	}

	inline IMLRegFormat GetBaseFormatByRegId(IMLRegID regId) const
	{
		auto it = regIdToBaseFormat.find(regId);
		cemu_assert_debug(it != regIdToBaseFormat.cend());
		return it->second;
	}

};

struct IMLFixedRegisters
{
	struct Entry
	{
		Entry(IMLReg reg, IMLPhysRegisterSet physRegSet) : reg(reg), physRegSet(physRegSet) {}

		IMLReg reg;
		IMLPhysRegisterSet physRegSet;
	};
	boost::container::small_vector<Entry, 4> listInput; // fixed registers for instruction input edge
	boost::container::small_vector<Entry, 4> listOutput; // fixed registers for instruction output edge
};

static void GetInstructionFixedRegisters(IMLInstruction* instruction, IMLFixedRegisters& fixedRegs)
{
	fixedRegs.listInput.clear();
	fixedRegs.listOutput.clear();

	// x86 specific logic is hardcoded for now
	if(instruction->type == PPCREC_IML_TYPE_R_R_R)
	{
		if(instruction->operation == PPCREC_IML_OP_LEFT_SHIFT || instruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_S || instruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_U)
		{
			// todo: We can skip this if g_CPUFeatures.x86.bmi2 is set, but for now we just assume it's not so we can properly test increased register pressure
			IMLPhysRegisterSet ps;
			ps.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE+X86_REG_ECX);
			fixedRegs.listInput.emplace_back(instruction->op_r_r_r.regB, ps);
		}
	}
	else if(instruction->type == PPCREC_IML_TYPE_ATOMIC_CMP_STORE)
	{
		IMLPhysRegisterSet ps;
		ps.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE+X86_REG_EAX);
		fixedRegs.listInput.emplace_back(instruction->op_atomic_compare_store.regBoolOut, ps);
	}
	else if(instruction->type == PPCREC_IML_TYPE_CALL_IMM)
	{
		// parameters (todo)
		cemu_assert_debug(!instruction->op_call_imm.regParam0.IsValid());
		cemu_assert_debug(!instruction->op_call_imm.regParam1.IsValid());
		cemu_assert_debug(!instruction->op_call_imm.regParam2.IsValid());
		// return value
		if(instruction->op_call_imm.regReturn.IsValid())
		{
			IMLRegFormat returnFormat = instruction->op_call_imm.regReturn.GetBaseFormat();
			bool isIntegerFormat = returnFormat == IMLRegFormat::I64 || returnFormat == IMLRegFormat::I32 || returnFormat == IMLRegFormat::I16 || returnFormat == IMLRegFormat::I8;
			cemu_assert_debug(isIntegerFormat); // float return values are still todo
			IMLPhysRegisterSet ps;
			ps.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE+X86_REG_EAX);
			fixedRegs.listOutput.emplace_back(instruction->op_call_imm.regReturn, ps);
		}
		// block volatile registers from being used on the output edge, this makes the RegAlloc store them during the call
		IMLPhysRegisterSet ps;
		if(!instruction->op_call_imm.regReturn.IsValid())
			ps.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE+X86_REG_RAX);
		ps.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE+X86_REG_RCX);
		ps.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE+X86_REG_RDX);
		ps.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE+X86_REG_R8);
		ps.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE+X86_REG_R9);
		ps.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE+X86_REG_R10);
		ps.SetAvailable(IMLArchX86::PHYSREG_GPR_BASE+X86_REG_R11);
		for(int i=0; i<=5; i++)
			ps.SetAvailable(IMLArchX86::PHYSREG_FPR_BASE+i); // YMM0-YMM5 are volatile
		// for YMM6-YMM15 only the upper 128 bits are volatile which we dont use
		fixedRegs.listOutput.emplace_back(IMLREG_INVALID, ps);
	}

}


uint32 PPCRecRA_getNextIterationIndex()
{
	static uint32 recRACurrentIterationIndex = 0;
	recRACurrentIterationIndex++;
	return recRACurrentIterationIndex;
}

bool _detectLoop(IMLSegment* currentSegment, sint32 depth, uint32 iterationIndex, IMLSegment* imlSegmentLoopBase)
{
	if (currentSegment == imlSegmentLoopBase)
		return true;
	if (currentSegment->raInfo.lastIterationIndex == iterationIndex)
		return currentSegment->raInfo.isPartOfProcessedLoop;
	if (depth >= 9)
		return false;
	currentSegment->raInfo.lastIterationIndex = iterationIndex;
	currentSegment->raInfo.isPartOfProcessedLoop = false;
	
	if (currentSegment->nextSegmentIsUncertain)
		return false;
	if (currentSegment->nextSegmentBranchNotTaken)
	{
		if (currentSegment->nextSegmentBranchNotTaken->momentaryIndex > currentSegment->momentaryIndex)
		{
			currentSegment->raInfo.isPartOfProcessedLoop |= _detectLoop(currentSegment->nextSegmentBranchNotTaken, depth + 1, iterationIndex, imlSegmentLoopBase);
		}
	}
	if (currentSegment->nextSegmentBranchTaken)
	{
		if (currentSegment->nextSegmentBranchTaken->momentaryIndex > currentSegment->momentaryIndex)
		{
			currentSegment->raInfo.isPartOfProcessedLoop |= _detectLoop(currentSegment->nextSegmentBranchTaken, depth + 1, iterationIndex, imlSegmentLoopBase);
		}
	}
	if (currentSegment->raInfo.isPartOfProcessedLoop)
		currentSegment->loopDepth++;
	return currentSegment->raInfo.isPartOfProcessedLoop;
}

void PPCRecRA_detectLoop(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegmentLoopBase)
{
	uint32 iterationIndex = PPCRecRA_getNextIterationIndex();
	imlSegmentLoopBase->raInfo.lastIterationIndex = iterationIndex;
	if (_detectLoop(imlSegmentLoopBase->nextSegmentBranchTaken, 0, iterationIndex, imlSegmentLoopBase))
	{
		imlSegmentLoopBase->loopDepth++;
	}
}

void PPCRecRA_identifyLoop(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
{
	if (imlSegment->nextSegmentIsUncertain)
		return;
	// check if this segment has a branch that links to itself (tight loop)
	if (imlSegment->nextSegmentBranchTaken == imlSegment)
	{
		// segment loops over itself
		imlSegment->loopDepth++;
		return;
	}
	// check if this segment has a branch that goes backwards (potential complex loop)
	if (imlSegment->nextSegmentBranchTaken && imlSegment->nextSegmentBranchTaken->momentaryIndex < imlSegment->momentaryIndex)
	{
		PPCRecRA_detectLoop(ppcImlGenContext, imlSegment);
	}
}

#define SUBRANGE_LIST_SIZE	(128)

sint32 PPCRecRA_countDistanceUntilNextUse2(raLivenessRange* subrange, raInstructionEdge startPosition)
{
	sint32 startInstructionIndex;
	if(startPosition.ConnectsToPreviousSegment())
		startInstructionIndex = 0;
	else
		startInstructionIndex = startPosition.GetInstructionIndex();
	for (sint32 i = 0; i < subrange->list_locations.size(); i++)
	{
		if (subrange->list_locations[i].index >= startInstructionIndex)
		{
			sint32 preciseIndex = subrange->list_locations[i].index * 2;
			cemu_assert_debug(subrange->list_locations[i].isRead || subrange->list_locations[i].isWrite); // locations must have any access
			// check read edge
			if(subrange->list_locations[i].isRead)
			{
				if(preciseIndex >= startPosition.GetRaw())
					return preciseIndex - startPosition.GetRaw();
			}
			// check write edge
			if(subrange->list_locations[i].isWrite)
			{
				preciseIndex++;
				if(preciseIndex >= startPosition.GetRaw())
					return preciseIndex - startPosition.GetRaw();
			}
		}
	}
	cemu_assert_debug(subrange->imlSegment->imlList.size() < 10000);
	return 10001*2;
}

// returns -1 if there is no fixed register requirement on or after startPosition
sint32 IMLRA_CountDistanceUntilFixedRegUsageInRange(IMLSegment* imlSegment, raLivenessRange* range, raInstructionEdge startPosition, sint32 physRegister, bool& hasFixedAccess)
{
	hasFixedAccess = false;
	cemu_assert_debug(startPosition.IsInstructionIndex());
	for(auto& fixedReqEntry : range->list_fixedRegRequirements)
	{
		if(fixedReqEntry.pos < startPosition)
			continue;
		if(fixedReqEntry.allowedReg.IsAvailable(physRegister))
		{
			hasFixedAccess = true;
			return fixedReqEntry.pos.GetRaw() - startPosition.GetRaw();
		}
	}
	cemu_assert_debug(range->interval2.end.IsInstructionIndex());
	return range->interval2.end.GetRaw() - startPosition.GetRaw();
}

sint32 IMLRA_CountDistanceUntilFixedRegUsage(IMLSegment* imlSegment, raInstructionEdge startPosition, sint32 maxDistance, IMLRegID ourRegId, sint32 physRegister)
{
	cemu_assert_debug(startPosition.IsInstructionIndex());
	raInstructionEdge lastPos2;
	lastPos2.Set(imlSegment->imlList.size(), false);

	raInstructionEdge endPos;
	endPos = startPosition + maxDistance;
	if(endPos > lastPos2)
		endPos = lastPos2;
	IMLFixedRegisters fixedRegs;
	if(startPosition.IsOnOutputEdge())
		GetInstructionFixedRegisters(imlSegment->imlList.data()+startPosition.GetInstructionIndex(), fixedRegs);
	for(raInstructionEdge currentPos = startPosition; currentPos <= endPos; ++currentPos)
	{
		if(currentPos.IsOnInputEdge())
		{
			GetInstructionFixedRegisters(imlSegment->imlList.data()+currentPos.GetInstructionIndex(), fixedRegs);
		}
		auto& fixedRegAccess = currentPos.IsOnInputEdge() ? fixedRegs.listInput : fixedRegs.listOutput;
		for(auto& fixedRegLoc : fixedRegAccess)
		{
			if(fixedRegLoc.reg.IsInvalid() || fixedRegLoc.reg.GetRegID() != ourRegId)
			{
				cemu_assert_debug(fixedRegLoc.reg.IsInvalid() || fixedRegLoc.physRegSet.HasExactlyOneAvailable()); // this whole function only makes sense when there is only one fixed register, otherwise there are extra permutations to consider. Except for IMLREG_INVALID which is used to indicate reserved registers
				if(fixedRegLoc.physRegSet.IsAvailable(physRegister))
					return currentPos.GetRaw() - startPosition.GetRaw();
			}
		}
	}
	return endPos.GetRaw() - startPosition.GetRaw();
}

// count how many instructions there are until physRegister is used by any subrange or reserved for any fixed register requirement (returns 0 if register is in use at startIndex)
sint32 PPCRecRA_countDistanceUntilNextLocalPhysRegisterUse(IMLSegment* imlSegment, raInstructionEdge startPosition, sint32 physRegister)
{
	cemu_assert_debug(startPosition.IsInstructionIndex());
	sint32 minDistance = (sint32)imlSegment->imlList.size()*2 - startPosition.GetRaw();
	// next
	raLivenessRange* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while(subrangeItr)
	{
		if (subrangeItr->GetPhysicalRegister() != physRegister)
		{
			subrangeItr = subrangeItr->link_allSegmentRanges.next;
			continue;
		}
		if(subrangeItr->interval2.ContainsEdge(startPosition))
			return 0;
		if (subrangeItr->interval2.end < startPosition)
		{
			subrangeItr = subrangeItr->link_allSegmentRanges.next;
			continue;
		}
		cemu_assert_debug(startPosition <= subrangeItr->interval2.start);
		sint32 currentDist = subrangeItr->interval2.start.GetRaw() - startPosition.GetRaw();
		minDistance = std::min(minDistance, currentDist);
		subrangeItr = subrangeItr->link_allSegmentRanges.next;
	}
	return minDistance;
}

struct IMLRALivenessTimeline
{
	IMLRALivenessTimeline()
	{
	}

	// manually add an active range
	void AddActiveRange(raLivenessRange* subrange)
	{
		activeRanges.emplace_back(subrange);
	}

	// remove all ranges from activeRanges with end <= instructionIndex
	void ExpireRanges(sint32 instructionIndex)
	{
		__debugbreak(); // maybe replace calls with raInstructionEdge variant?
		expiredRanges.clear();
		size_t count = activeRanges.size();
		for (size_t f = 0; f < count; f++)
		{
			raLivenessRange* liverange = activeRanges[f];
			if (liverange->interval2.end.GetInstructionIndex() < instructionIndex) // <= to < since end is now inclusive
			{
#ifdef CEMU_DEBUG_ASSERT
				if (instructionIndex != RA_INTER_RANGE_END && (liverange->subrangeBranchTaken || liverange->subrangeBranchNotTaken))
					assert_dbg(); // infinite subranges should not expire
#endif
				expiredRanges.emplace_back(liverange);
				// remove entry
				activeRanges[f] = activeRanges[count-1];
				f--;
				count--;
			}
		}
		if(count != activeRanges.size())
			activeRanges.resize(count);
	}

	void ExpireRanges(raInstructionEdge expireUpTo)
	{
		expiredRanges.clear();
		size_t count = activeRanges.size();
		for (size_t f = 0; f < count; f++)
		{
			raLivenessRange* liverange = activeRanges[f];
			if (liverange->interval2.end < expireUpTo) // this was <= but since end is not inclusive we need to use <
			{
#ifdef CEMU_DEBUG_ASSERT
				if (!expireUpTo.ConnectsToNextSegment() && (liverange->subrangeBranchTaken || liverange->subrangeBranchNotTaken))
					assert_dbg(); // infinite subranges should not expire
#endif
				expiredRanges.emplace_back(liverange);
				// remove entry
				activeRanges[f] = activeRanges[count-1];
				f--;
				count--;
			}
		}
		if(count != activeRanges.size())
			activeRanges.resize(count);
	}

	std::span<raLivenessRange*> GetExpiredRanges()
	{
		return { expiredRanges.data(), expiredRanges.size() };
	}

	std::span<raLivenessRange*> GetActiveRanges()
	{
		return { activeRanges.data(), activeRanges.size() };
	}

	raLivenessRange* GetActiveRangeByVirtualRegId(IMLRegID regId)
	{
		for(auto& it : activeRanges)
			if(it->virtualRegister == regId)
				return it;
		return nullptr;
	}

	raLivenessRange* GetActiveRangeByPhysicalReg(sint32 physReg)
	{
		cemu_assert_debug(physReg >= 0);
		for(auto& it : activeRanges)
			if(it->physicalRegister == physReg)
				return it;
		return nullptr;
	}

	boost::container::small_vector<raLivenessRange*, 64> activeRanges;

private:
	boost::container::small_vector<raLivenessRange*, 16> expiredRanges;
};

// mark occupied registers by any overlapping range as unavailable in physRegSet
void PPCRecRA_MaskOverlappingPhysRegForGlobalRange(raLivenessRange* range2, IMLPhysRegisterSet& physRegSet)
{
	auto clusterRanges = range2->GetAllSubrangesInCluster();
	for (auto& subrange : clusterRanges)
	{
		IMLSegment* imlSegment = subrange->imlSegment;
		raLivenessRange* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while(subrangeItr)
		{
			if (subrange == subrangeItr)
			{
				// next
				subrangeItr = subrangeItr->link_allSegmentRanges.next;
				continue;
			}
			if(subrange->interval2.IsOverlapping(subrangeItr->interval2))
			{
				if (subrangeItr->GetPhysicalRegister() >= 0)
					physRegSet.SetReserved(subrangeItr->GetPhysicalRegister());
			}
			// next
			subrangeItr = subrangeItr->link_allSegmentRanges.next;
		}
	}
}

bool _livenessRangeStartCompare(raLivenessRange* lhs, raLivenessRange* rhs) { return lhs->interval2.start < rhs->interval2.start; }

void _sortSegmentAllSubrangesLinkedList(IMLSegment* imlSegment)
{
	raLivenessRange* subrangeList[4096+1];
	sint32 count = 0;
	// disassemble linked list
	raLivenessRange* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while (subrangeItr)
	{
		if (count >= 4096)
			assert_dbg();
		subrangeList[count] = subrangeItr;
		count++;
		// next
		subrangeItr = subrangeItr->link_allSegmentRanges.next;
	}
	if (count == 0)
	{
		imlSegment->raInfo.linkedList_allSubranges = nullptr;
		return;
	}
	// sort
	std::sort(subrangeList, subrangeList + count, _livenessRangeStartCompare);
	// reassemble linked list
	subrangeList[count] = nullptr;
	imlSegment->raInfo.linkedList_allSubranges = subrangeList[0];
	subrangeList[0]->link_allSegmentRanges.prev = nullptr;
	subrangeList[0]->link_allSegmentRanges.next = subrangeList[1];
	for (sint32 i = 1; i < count; i++)
	{
		subrangeList[i]->link_allSegmentRanges.prev = subrangeList[i - 1];
		subrangeList[i]->link_allSegmentRanges.next = subrangeList[i + 1];
	}
	// validate list
#ifdef CEMU_DEBUG_ASSERT
	sint32 count2 = 0;
	subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	raInstructionEdge currentStartPosition;
	currentStartPosition.SetRaw(RA_INTER_RANGE_START);
	while (subrangeItr)
	{
		count2++;
		if (subrangeItr->interval2.start < currentStartPosition)
			assert_dbg();
		currentStartPosition = subrangeItr->interval2.start;
		// next
		subrangeItr = subrangeItr->link_allSegmentRanges.next;
	}
	if (count != count2)
		assert_dbg();
#endif
}

std::unordered_map<IMLRegID, raLivenessRange*>& IMLRA_GetSubrangeMap(IMLSegment* imlSegment)
{
	return imlSegment->raInfo.linkedList_perVirtualRegister;
}

raLivenessRange* IMLRA_GetSubrange(IMLSegment* imlSegment, IMLRegID regId)
{
	auto it = imlSegment->raInfo.linkedList_perVirtualRegister.find(regId);
	if (it == imlSegment->raInfo.linkedList_perVirtualRegister.end())
		return nullptr;
	return it->second;
}

struct raFixedRegRequirementWithVGPR
{
	raInstructionEdge pos;
	IMLPhysRegisterSet allowedReg;
	IMLRegID regId;
};

std::vector<raFixedRegRequirementWithVGPR> IMLRA_BuildSegmentInstructionFixedRegList(IMLSegment* imlSegment)
{
	std::vector<raFixedRegRequirementWithVGPR> frrList;

	size_t index = 0;
	IMLUsedRegisters gprTracking;
	while (index < imlSegment->imlList.size())
	{
		IMLFixedRegisters fixedRegs;
		GetInstructionFixedRegisters(&imlSegment->imlList[index], fixedRegs);
		raInstructionEdge pos;
		pos.Set(index, true);
		for(auto& fixedRegAccess : fixedRegs.listInput)
		{
			frrList.emplace_back(pos, fixedRegAccess.physRegSet, fixedRegAccess.reg.GetRegID());
		}
		pos = pos + 1;
		for(auto& fixedRegAccess : fixedRegs.listOutput)
		{
			frrList.emplace_back(pos, fixedRegAccess.physRegSet, fixedRegAccess.reg.IsValid()?fixedRegAccess.reg.GetRegID():IMLRegID_INVALID);
		}
		index++;
	}
	return frrList;
}

boost::container::small_vector<raLivenessRange*, 8> IMLRA_GetRangeWithFixedRegReservationOverlappingPos(IMLSegment* imlSegment, raInstructionEdge pos, IMLPhysReg physReg)
{
	boost::container::small_vector<raLivenessRange*, 8> rangeList;
	for(raLivenessRange* currentRange = imlSegment->raInfo.linkedList_allSubranges; currentRange; currentRange = currentRange->link_allSegmentRanges.next)
	{
		if(!currentRange->interval2.ContainsEdge(pos))
			continue;
		IMLPhysRegisterSet allowedRegs;
		if(!currentRange->GetAllowedRegistersEx(allowedRegs))
			continue;
		if(allowedRegs.IsAvailable(physReg))
			rangeList.emplace_back(currentRange);
	}
	return rangeList;
}

void IMLRA_HandleFixedRegisters(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
{
	// first pass - iterate over all ranges with fixed register requirements and split them if they cross the segment border
	// todo - this can be optimized. Ranges only need to be split if there are conflicts with other segments. Note that below passes rely on the fact that this pass currently splits all ranges with fixed register requirements
	for(raLivenessRange* currentRange = imlSegment->raInfo.linkedList_allSubranges; currentRange;)
	{
		IMLPhysRegisterSet allowedRegs;
		if(!currentRange->GetAllowedRegistersEx(allowedRegs))
		{
			currentRange = currentRange->link_allSegmentRanges.next;
			continue;
		}
		if(currentRange->interval2.ExtendsPreviousSegment() || currentRange->interval2.ExtendsIntoNextSegment())
		{
			raLivenessRange* nextRange = currentRange->link_allSegmentRanges.next;
			PPCRecRA_explodeRange(ppcImlGenContext, currentRange);
			currentRange = nextRange;
			continue;
		}
		currentRange = currentRange->link_allSegmentRanges.next;
	}
	// second pass - look for ranges with conflicting fixed register requirements and split these too (locally)
	for(raLivenessRange* currentRange = imlSegment->raInfo.linkedList_allSubranges; currentRange; currentRange = currentRange->link_allSegmentRanges.next)
	{
		IMLPhysRegisterSet allowedRegs;
		if(currentRange->list_fixedRegRequirements.empty())
			continue; // we dont need to check whole clusters because the pass above guarantees that there are no ranges with fixed register requirements that extend outside of this segment
		if(!currentRange->GetAllowedRegistersEx(allowedRegs))
			continue;
		if(allowedRegs.HasAnyAvailable())
			continue;
		cemu_assert_unimplemented();
	}
	// third pass - assign fixed registers, split ranges if needed
	std::vector<raFixedRegRequirementWithVGPR> frr = IMLRA_BuildSegmentInstructionFixedRegList(imlSegment);
	std::unordered_map<IMLPhysReg, IMLRegID> lastVGPR;
	for(size_t i=0; i<frr.size(); i++)
	{
		raFixedRegRequirementWithVGPR& entry = frr[i];
		// we currently only handle fixed register requirements with a single register
		// with one exception: When regId is IMLRegID_INVALID then the entry acts as a list of reserved registers
		cemu_assert_debug(entry.regId == IMLRegID_INVALID || entry.allowedReg.HasExactlyOneAvailable());
		for(IMLPhysReg physReg = entry.allowedReg.GetFirstAvailableReg(); physReg >= 0; physReg = entry.allowedReg.GetNextAvailableReg(physReg+1))
		{
			// check if the assigned vGPR has changed
			bool vgprHasChanged = false;
			auto it = lastVGPR.find(physReg);
			if(it != lastVGPR.end())
				vgprHasChanged = it->second != entry.regId;
			else
				vgprHasChanged = true;
			lastVGPR[physReg] = entry.regId;

			if(!vgprHasChanged)
				continue;

			boost::container::small_vector<raLivenessRange*, 8> overlappingRanges = IMLRA_GetRangeWithFixedRegReservationOverlappingPos(imlSegment, entry.pos, physReg);
			if(entry.regId != IMLRegID_INVALID)
				cemu_assert_debug(!overlappingRanges.empty()); // there should always be at least one range that overlaps corresponding to the fixed register requirement, except for IMLRegID_INVALID which is used to indicate reserved registers

			for(auto& range : overlappingRanges)
			{
				if(range->interval2.start < entry.pos)
				{
					PPCRecRA_splitLocalSubrange2(ppcImlGenContext, range, entry.pos, true);
				}
			}

		}
	}
	// finally iterate ranges and assign fixed registers
	for(raLivenessRange* currentRange = imlSegment->raInfo.linkedList_allSubranges; currentRange; currentRange = currentRange->link_allSegmentRanges.next)
	{
		IMLPhysRegisterSet allowedRegs;
		if(currentRange->list_fixedRegRequirements.empty())
			continue; // we dont need to check whole clusters because the pass above guarantees that there are no ranges with fixed register requirements that extend outside of this segment
		if(!currentRange->GetAllowedRegistersEx(allowedRegs))
		{
			cemu_assert_debug(currentRange->list_fixedRegRequirements.empty());
			continue;
		}
		cemu_assert_debug(allowedRegs.HasExactlyOneAvailable());
		currentRange->SetPhysicalRegister(allowedRegs.GetFirstAvailableReg());
	}
	// DEBUG - check for collisions and make sure all ranges with fixed register requirements got their physical register assigned
#ifdef CEMU_DEBUG_ASSERT
	for(raLivenessRange* currentRange = imlSegment->raInfo.linkedList_allSubranges; currentRange; currentRange = currentRange->link_allSegmentRanges.next)
	{
		IMLPhysRegisterSet allowedRegs;
		if(!currentRange->HasPhysicalRegister())
			continue;
		for(raLivenessRange* currentRange2 = imlSegment->raInfo.linkedList_allSubranges; currentRange2; currentRange2 = currentRange2->link_allSegmentRanges.next)
		{
			if(currentRange == currentRange2)
				continue;
			if(currentRange->interval2.IsOverlapping(currentRange2->interval2))
			{
				cemu_assert_debug(currentRange->GetPhysicalRegister() != currentRange2->GetPhysicalRegister());
			}
		}
	}
	for(raLivenessRange* currentRange = imlSegment->raInfo.linkedList_allSubranges; currentRange; currentRange = currentRange->link_allSegmentRanges.next)
	{
		IMLPhysRegisterSet allowedRegs;
		if(!currentRange->GetAllowedRegistersEx(allowedRegs))
		{
			cemu_assert_debug(currentRange->list_fixedRegRequirements.empty());
			continue;
		}
		cemu_assert_debug(currentRange->HasPhysicalRegister() && allowedRegs.IsAvailable(currentRange->GetPhysicalRegister()));
	}
#endif
}

// we should not split ranges on instructions with tied registers (i.e. where a register encoded as a single parameter is both input and output)
// otherwise the RA algorithm has to assign both ranges the same physical register (not supported yet) and the point of splitting to fit another range is nullified
void IMLRA_MakeSafeSplitPosition(IMLSegment* imlSegment, raInstructionEdge& pos)
{
	// we ignore the instruction for now and just always make it a safe split position
	cemu_assert_debug(pos.IsInstructionIndex());
	if(pos.IsOnOutputEdge())
		pos = pos - 1;
}

// convenience wrapper for IMLRA_MakeSafeSplitPosition
void IMLRA_MakeSafeSplitDistance(IMLSegment* imlSegment, raInstructionEdge startPos, sint32& distance)
{
	cemu_assert_debug(startPos.IsInstructionIndex());
	cemu_assert_debug(distance >= 0);
	raInstructionEdge endPos = startPos + distance;
	IMLRA_MakeSafeSplitPosition(imlSegment, endPos);
	if(endPos < startPos)
	{
		distance = 0;
		return;
	}
	distance = endPos.GetRaw() - startPos.GetRaw();
}

void DbgVerifyAllRanges(IMLRegisterAllocatorContext& ctx);

class RASpillStrategy
{
public:
	virtual void Apply(ppcImlGenContext_t* ctx, IMLSegment* imlSegment, raLivenessRange* currentRange) = 0;

	sint32 GetCost()
	{
		return strategyCost;
	}

protected:
	void ResetCost()
	{
		strategyCost = INT_MAX;
	}

	sint32 strategyCost;
};

class RASpillStrategy_LocalRangeHoleCutting : public RASpillStrategy
{
public:
	void Reset()
	{
		localRangeHoleCutting.distance = -1;
		localRangeHoleCutting.largestHoleSubrange = nullptr;
		ResetCost();
	}

	void Evaluate(IMLSegment* imlSegment, raLivenessRange* currentRange, const IMLRALivenessTimeline& timeline, const IMLPhysRegisterSet& allowedRegs)
	{
		raInstructionEdge currentRangeStart = currentRange->interval2.start;
		sint32 requiredSize2 = currentRange->interval2.GetPreciseDistance();
		cemu_assert_debug(localRangeHoleCutting.distance == -1);
		cemu_assert_debug(strategyCost == INT_MAX);
		if(!currentRangeStart.ConnectsToPreviousSegment())
		{
			cemu_assert_debug(currentRangeStart.GetRaw() >= 0);
			for (auto candidate : timeline.activeRanges)
			{
				if (candidate->interval2.ExtendsIntoNextSegment())
					continue;
				// new checks (Oct 2024):
				if(candidate == currentRange)
					continue;
				if(candidate->GetPhysicalRegister() < 0)
					continue;
				if(!allowedRegs.IsAvailable(candidate->GetPhysicalRegister()))
					continue;

				sint32 distance2 = PPCRecRA_countDistanceUntilNextUse2(candidate, currentRangeStart);
				IMLRA_MakeSafeSplitDistance(imlSegment, currentRangeStart, distance2);
				if (distance2 < 2)
					continue;
				cemu_assert_debug(currentRangeStart.IsInstructionIndex());
				distance2 = std::min<sint32>(distance2, imlSegment->imlList.size()*2 - currentRangeStart.GetRaw()); // limit distance to end of segment
				// calculate split cost of candidate
				sint32 cost = PPCRecRARange_estimateAdditionalCostAfterSplit(candidate, currentRangeStart + distance2);
				// calculate additional split cost of currentRange if hole is not large enough
				if (distance2 < requiredSize2)
				{
					cost += PPCRecRARange_estimateAdditionalCostAfterSplit(currentRange, currentRangeStart + distance2);
					// we also slightly increase cost in relation to the remaining length (in order to make the algorithm prefer larger holes)
					cost += (requiredSize2 - distance2) / 10;
				}
				// compare cost with previous candidates
				if (cost < strategyCost)
				{
					strategyCost = cost;
					localRangeHoleCutting.distance = distance2;
					localRangeHoleCutting.largestHoleSubrange = candidate;
				}
			}
		}
	}

	void Apply(ppcImlGenContext_t* ctx, IMLSegment* imlSegment, raLivenessRange* currentRange) override
	{
		cemu_assert_debug(strategyCost != INT_MAX);
		sint32 requiredSize2 = currentRange->interval2.GetPreciseDistance();
		raInstructionEdge currentRangeStart = currentRange->interval2.start;

		raInstructionEdge holeStartPosition = currentRangeStart;
		raInstructionEdge holeEndPosition = currentRangeStart + localRangeHoleCutting.distance;
		raLivenessRange* collisionRange = localRangeHoleCutting.largestHoleSubrange;

		if(collisionRange->interval2.start < holeStartPosition)
		{
			collisionRange = PPCRecRA_splitLocalSubrange2(nullptr, collisionRange, holeStartPosition, true);
			cemu_assert_debug(!collisionRange || collisionRange->interval2.start >= holeStartPosition); // verify if splitting worked at all, tail must be on or after the split point
			cemu_assert_debug(!collisionRange || collisionRange->interval2.start >= holeEndPosition); // also verify that the trimmed hole is actually big enough
		}
		else
		{
			cemu_assert_unimplemented(); // we still need to trim?
		}
		// we may also have to cut the current range to fit partially into the hole
		if (requiredSize2 > localRangeHoleCutting.distance)
		{
			raLivenessRange* tailRange = PPCRecRA_splitLocalSubrange2(nullptr, currentRange, currentRangeStart + localRangeHoleCutting.distance, true);
			if(tailRange)
			{
				cemu_assert_debug(tailRange->list_fixedRegRequirements.empty()); // we are not allowed to unassign fixed registers
				tailRange->UnsetPhysicalRegister();
			}
		}
		// verify that the hole is large enough
		if(collisionRange)
		{
			cemu_assert_debug(!collisionRange->interval2.IsOverlapping(currentRange->interval2));
		}
	}

private:
	struct
	{
		sint32 distance;
		raLivenessRange* largestHoleSubrange;
	}localRangeHoleCutting;
};

class RASpillStrategy_AvailableRegisterHole : public RASpillStrategy
{
	// split current range (this is generally only a good choice when the current range is long but has few usages)
	public:
	void Reset()
	{
		ResetCost();
		availableRegisterHole.distance = -1;
		availableRegisterHole.physRegister = -1;
	}

	void Evaluate(IMLSegment* imlSegment, raLivenessRange* currentRange, const IMLRALivenessTimeline& timeline, const IMLPhysRegisterSet& localAvailableRegsMask, const IMLPhysRegisterSet& allowedRegs)
	{
		sint32 requiredSize2 = currentRange->interval2.GetPreciseDistance();

		raInstructionEdge currentRangeStart = currentRange->interval2.start;
		cemu_assert_debug(strategyCost == INT_MAX);
		availableRegisterHole.distance = -1;
		availableRegisterHole.physRegister = -1;
		if (currentRangeStart.GetRaw() >= 0)
		{
			if (localAvailableRegsMask.HasAnyAvailable())
			{
				sint32 physRegItr = -1;
				while (true)
				{
					physRegItr = localAvailableRegsMask.GetNextAvailableReg(physRegItr + 1);
					if (physRegItr < 0)
						break;
					if(!allowedRegs.IsAvailable(physRegItr))
						continue;
					// get size of potential hole for this register
					sint32 distance = PPCRecRA_countDistanceUntilNextLocalPhysRegisterUse(imlSegment, currentRangeStart, physRegItr);

					// some instructions may require the same register for another range, check the distance here
					sint32 distUntilFixedReg = IMLRA_CountDistanceUntilFixedRegUsage(imlSegment, currentRangeStart, distance, currentRange->GetVirtualRegister(), physRegItr);
					if(distUntilFixedReg < distance)
						distance = distUntilFixedReg;

					IMLRA_MakeSafeSplitDistance(imlSegment, currentRangeStart, distance);
					if (distance < 2)
						continue;
					// calculate additional cost due to split
					cemu_assert_debug(distance < requiredSize2); // should always be true otherwise previous step would have selected this register?
					sint32 cost = PPCRecRARange_estimateAdditionalCostAfterSplit(currentRange, currentRangeStart + distance);
					// add small additional cost for the remaining range (prefer larger holes)
					cost += ((requiredSize2 - distance) / 2) / 10;
					if (cost < strategyCost)
					{
						strategyCost = cost;
						availableRegisterHole.distance = distance;
						availableRegisterHole.physRegister = physRegItr;
					}
				}
			}
		}
	}

	void Apply(ppcImlGenContext_t* ctx, IMLSegment* imlSegment, raLivenessRange* currentRange) override
	{
		cemu_assert_debug(strategyCost != INT_MAX);
		raInstructionEdge currentRangeStart = currentRange->interval2.start;
		// use available register
		raLivenessRange* tailRange = PPCRecRA_splitLocalSubrange2(nullptr, currentRange, currentRangeStart + availableRegisterHole.distance, true);
		if(tailRange)
		{
			cemu_assert_debug(tailRange->list_fixedRegRequirements.empty()); // we are not allowed to unassign fixed registers
			tailRange->UnsetPhysicalRegister();
		}
	}

	private:
	struct
	{
		sint32 physRegister;
		sint32 distance; // size of hole
	}availableRegisterHole;
};

class RASpillStrategy_ExplodeRange : public RASpillStrategy
{
public:
	void Reset()
	{
		ResetCost();
		explodeRange.range = nullptr;
		explodeRange.distance = -1;
	}

	void Evaluate(IMLSegment* imlSegment, raLivenessRange* currentRange, const IMLRALivenessTimeline& timeline, const IMLPhysRegisterSet& allowedRegs)
	{
		raInstructionEdge currentRangeStart = currentRange->interval2.start;
		if(currentRangeStart.ConnectsToPreviousSegment())
			currentRangeStart.Set(0, true);
		sint32 requiredSize2 = currentRange->interval2.GetPreciseDistance();
		cemu_assert_debug(strategyCost == INT_MAX);
		explodeRange.range = nullptr;
		explodeRange.distance = -1;
		for (auto candidate : timeline.activeRanges)
		{
			if (!candidate->interval2.ExtendsIntoNextSegment())
				continue;
			// new checks (Oct 2024):
			if(candidate == currentRange)
				continue;
			if(candidate->GetPhysicalRegister() < 0)
				continue;
			if(!allowedRegs.IsAvailable(candidate->GetPhysicalRegister()))
				continue;

			sint32 distance = PPCRecRA_countDistanceUntilNextUse2(candidate, currentRangeStart);
			IMLRA_MakeSafeSplitDistance(imlSegment, currentRangeStart, distance);
			if( distance < 2)
				continue;
			sint32 cost = PPCRecRARange_estimateCostAfterRangeExplode(candidate);
			// if the hole is not large enough, add cost of splitting current subrange
			if (distance < requiredSize2)
			{
				cost += PPCRecRARange_estimateAdditionalCostAfterSplit(currentRange, currentRangeStart + distance);
				// add small additional cost for the remaining range (prefer larger holes)
				cost += ((requiredSize2 - distance) / 2) / 10;
			}
			// compare with current best candidate for this strategy
			if (cost < strategyCost)
			{
				strategyCost = cost;
				explodeRange.distance = distance;
				explodeRange.range = candidate;
			}
		}
	}

	void Apply(ppcImlGenContext_t* ctx, IMLSegment* imlSegment, raLivenessRange* currentRange) override
	{
		raInstructionEdge currentRangeStart = currentRange->interval2.start;
		if(currentRangeStart.ConnectsToPreviousSegment())
			currentRangeStart.Set(0, true);
		sint32 requiredSize2 = currentRange->interval2.GetPreciseDistance();
		// explode range
		PPCRecRA_explodeRange(nullptr, explodeRange.range);
		// split current subrange if necessary
		if( requiredSize2 > explodeRange.distance)
		{
			raLivenessRange* tailRange = PPCRecRA_splitLocalSubrange2(nullptr, currentRange, currentRangeStart+explodeRange.distance, true);
			if(tailRange)
			{
				cemu_assert_debug(tailRange->list_fixedRegRequirements.empty()); // we are not allowed to unassign fixed registers
				tailRange->UnsetPhysicalRegister();
			}
		}
	}

private:
	struct
	{
		raLivenessRange* range;
		sint32 distance; // size of hole
		// note: If we explode a range, we still have to check the size of the hole that becomes available, if too small then we need to add cost of splitting local subrange
	}explodeRange;
};


class RASpillStrategy_ExplodeRangeInter : public RASpillStrategy
{
public:
	void Reset()
	{
		ResetCost();
		explodeRange.range = nullptr;
		explodeRange.distance = -1;
	}

	void Evaluate(IMLSegment* imlSegment, raLivenessRange* currentRange, const IMLRALivenessTimeline& timeline, const IMLPhysRegisterSet& allowedRegs)
	{
		// explode the range with the least cost
		cemu_assert_debug(strategyCost == INT_MAX);
		cemu_assert_debug(explodeRange.range == nullptr && explodeRange.distance == -1);
		for(auto candidate : timeline.activeRanges)
		{
			if (!candidate->interval2.ExtendsIntoNextSegment())
				continue;
			// only select candidates that clash with current subrange
			if (candidate->GetPhysicalRegister() < 0 && candidate != currentRange)
				continue;
			// and also filter any that dont meet fixed register requirements
			if(!allowedRegs.IsAvailable(candidate->GetPhysicalRegister()))
				continue;
			sint32 cost;
			cost = PPCRecRARange_estimateCostAfterRangeExplode(candidate);
			// compare with current best candidate for this strategy
			if (cost < strategyCost)
			{
				strategyCost = cost;
				explodeRange.distance = INT_MAX;
				explodeRange.range = candidate;
			}
		}
		// add current range as a candidate too
		sint32 ownCost;
		ownCost = PPCRecRARange_estimateCostAfterRangeExplode(currentRange);
		if (ownCost < strategyCost)
		{
			strategyCost = ownCost;
			explodeRange.distance = INT_MAX;
			explodeRange.range = currentRange;
		}
	}

	void Apply(ppcImlGenContext_t* ctx, IMLSegment* imlSegment, raLivenessRange* currentRange) override
	{
		cemu_assert_debug(strategyCost != INT_MAX);
		PPCRecRA_explodeRange(ctx, explodeRange.range);
	}

private:
	struct
	{
		raLivenessRange* range;
		sint32 distance; // size of hole
		// note: If we explode a range, we still have to check the size of the hole that becomes available, if too small then we need to add cost of splitting local subrange
	}explodeRange;
};

// filter any registers from candidatePhysRegSet which cannot be used by currentRange due to fixed register requirements within the range that it occupies
void IMLRA_FilterReservedFixedRegisterRequirementsForSegment(IMLRegisterAllocatorContext& ctx, raLivenessRange* currentRange, IMLPhysRegisterSet& candidatePhysRegSet)
{
	IMLSegment* seg = currentRange->imlSegment;
	if(seg->imlList.empty())
		return; // there can be no fixed register requirements if there are no instructions

	raInstructionEdge firstPos = currentRange->interval2.start;
	if(currentRange->interval2.start.ConnectsToPreviousSegment())
		firstPos.SetRaw(0);
	else if(currentRange->interval2.start.ConnectsToNextSegment())
		firstPos.Set(seg->imlList.size()-1, false);

	raInstructionEdge lastPos = currentRange->interval2.end;
	if(currentRange->interval2.end.ConnectsToPreviousSegment())
		lastPos.SetRaw(0);
	else if(currentRange->interval2.end.ConnectsToNextSegment())
		lastPos.Set(seg->imlList.size()-1, false);
	cemu_assert_debug(firstPos <= lastPos);

	IMLRegID ourRegId = currentRange->GetVirtualRegister();

	IMLFixedRegisters fixedRegs;
	if(firstPos.IsOnOutputEdge())
		GetInstructionFixedRegisters(seg->imlList.data()+firstPos.GetInstructionIndex(), fixedRegs);
	for(raInstructionEdge currentPos = firstPos; currentPos <= lastPos; ++currentPos)
	{
		if(currentPos.IsOnInputEdge())
		{
			GetInstructionFixedRegisters(seg->imlList.data()+currentPos.GetInstructionIndex(), fixedRegs);
		}
		auto& fixedRegAccess = currentPos.IsOnInputEdge() ? fixedRegs.listInput : fixedRegs.listOutput;
		for(auto& fixedRegLoc : fixedRegAccess)
		{
			if(fixedRegLoc.reg.IsInvalid() || fixedRegLoc.reg.GetRegID() != ourRegId)
				candidatePhysRegSet.RemoveRegisters(fixedRegLoc.physRegSet);
		}
	}
}

// filter out any registers along the range cluster
void IMLRA_FilterReservedFixedRegisterRequirementsForCluster(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment, raLivenessRange* currentRange, IMLPhysRegisterSet& candidatePhysRegSet)
{
	cemu_assert_debug(currentRange->imlSegment == imlSegment);
	if(currentRange->interval2.ExtendsPreviousSegment() || currentRange->interval2.ExtendsIntoNextSegment())
	{
		auto clusterRanges = currentRange->GetAllSubrangesInCluster();
		for(auto& rangeIt : clusterRanges)
		{
			IMLRA_FilterReservedFixedRegisterRequirementsForSegment(ctx, rangeIt, candidatePhysRegSet);
			if(!candidatePhysRegSet.HasAnyAvailable())
				break;
		}
		return;
	}
	IMLRA_FilterReservedFixedRegisterRequirementsForSegment(ctx, currentRange, candidatePhysRegSet);
}

bool IMLRA_AssignSegmentRegisters(IMLRegisterAllocatorContext& ctx, ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
{
	// sort subranges ascending by start index
	_sortSegmentAllSubrangesLinkedList(imlSegment);

	IMLRALivenessTimeline livenessTimeline;
	raLivenessRange* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	raInstructionEdge lastInstructionEdge;
	lastInstructionEdge.SetRaw(RA_INTER_RANGE_END);

	struct
	{
		RASpillStrategy_LocalRangeHoleCutting localRangeHoleCutting;
		RASpillStrategy_AvailableRegisterHole availableRegisterHole;
		RASpillStrategy_ExplodeRange explodeRange;
		// for ranges that connect to follow up segments:
		RASpillStrategy_ExplodeRangeInter explodeRangeInter;
	}strategy;

	while(subrangeItr)
	{
		raInstructionEdge currentRangeStart = subrangeItr->interval2.start; // used to be currentIndex before refactor
		PPCRecRA_debugValidateSubrange(subrangeItr);

		// below used to be: std::min<sint32>(currentIndex, RA_INTER_RANGE_END-1)
		livenessTimeline.ExpireRanges((currentRangeStart > lastInstructionEdge) ? lastInstructionEdge : currentRangeStart); // expire up to currentIndex (inclusive), but exclude infinite ranges
		// note: The logic here is complicated in regards to whether the instruction index should be inclusive or exclusive. Find a way to simplify?

		// if subrange already has register assigned then add it to the active list and continue
		if (subrangeItr->GetPhysicalRegister() >= 0)
		{
			// verify if register is actually available
#ifdef CEMU_DEBUG_ASSERT
			for (auto& liverangeItr : livenessTimeline.activeRanges)
			{
				// check for register mismatch
				cemu_assert_debug(liverangeItr->GetPhysicalRegister() != subrangeItr->GetPhysicalRegister());
			}
#endif
			livenessTimeline.AddActiveRange(subrangeItr);
			subrangeItr = subrangeItr->link_allSegmentRanges.next;
			continue;
		}
		// ranges with fixed register requirements should already have a phys register assigned
		if(!subrangeItr->list_fixedRegRequirements.empty())
		{
			cemu_assert_debug(subrangeItr->HasPhysicalRegister());
		}
		// find free register for current subrangeItr and segment
		IMLRegFormat regBaseFormat = ctx.GetBaseFormatByRegId(subrangeItr->GetVirtualRegister());
		IMLPhysRegisterSet candidatePhysRegSet = ctx.raParam->GetPhysRegPool(regBaseFormat);
		cemu_assert_debug(candidatePhysRegSet.HasAnyAvailable()); // no valid pool provided for this register type

		IMLPhysRegisterSet allowedRegs = subrangeItr->GetAllowedRegisters(candidatePhysRegSet);
		cemu_assert_debug(allowedRegs.HasAnyAvailable()); // if zero regs are available, then this range needs to be split to avoid mismatching register requirements (do this in the initial pass to keep the code here simpler)
		candidatePhysRegSet &= allowedRegs;

		for (auto& liverangeItr : livenessTimeline.activeRanges)
		{
			cemu_assert_debug(liverangeItr->GetPhysicalRegister() >= 0);
			candidatePhysRegSet.SetReserved(liverangeItr->GetPhysicalRegister());
		}
		// check intersections with other ranges and determine allowed registers
		IMLPhysRegisterSet localAvailableRegsMask = candidatePhysRegSet; // mask of registers that are currently not used (does not include range checks in other segments)
		if(candidatePhysRegSet.HasAnyAvailable())
		{
			// check for overlaps on a global scale (subrangeItr can be part of a larger range cluster across multiple segments)
			PPCRecRA_MaskOverlappingPhysRegForGlobalRange(subrangeItr, candidatePhysRegSet);
		}
		// some target instructions may enforce specific registers (e.g. common on X86 where something like SHL <reg>, CL forces CL as the count register)
		// we determine the list of allowed registers here
		// this really only works if we assume single-register requirements (otherwise its better not to filter out early and instead allow register corrections later but we don't support this yet)
		if (candidatePhysRegSet.HasAnyAvailable())
		{
			IMLRA_FilterReservedFixedRegisterRequirementsForCluster(ctx, imlSegment, subrangeItr, candidatePhysRegSet);
		}
		if(candidatePhysRegSet.HasAnyAvailable())
		{
			// use free register
			subrangeItr->SetPhysicalRegisterForCluster(candidatePhysRegSet.GetFirstAvailableReg());
			livenessTimeline.AddActiveRange(subrangeItr);
			subrangeItr = subrangeItr->link_allSegmentRanges.next; // next
			continue;
		}
		// there is no free register for the entire range
		// evaluate different strategies of splitting ranges to free up another register or shorten the current range
		strategy.localRangeHoleCutting.Reset();
		strategy.availableRegisterHole.Reset();
		strategy.explodeRange.Reset();
		// cant assign register
		// there might be registers available, we just can't use them due to range conflicts
		RASpillStrategy* selectedStrategy = nullptr;
		auto SelectStrategyIfBetter = [&selectedStrategy](RASpillStrategy& newStrategy)
		{
			if(newStrategy.GetCost() == INT_MAX)
				return;
			if(selectedStrategy == nullptr || newStrategy.GetCost() < selectedStrategy->GetCost())
				selectedStrategy = &newStrategy;
		};

		if (!subrangeItr->interval2.ExtendsIntoNextSegment())
		{
			// range ends in current segment, use local strategies
			// evaluate strategy: Cut hole into local subrange
			strategy.localRangeHoleCutting.Evaluate(imlSegment, subrangeItr, livenessTimeline, allowedRegs);
			SelectStrategyIfBetter(strategy.localRangeHoleCutting);
			// evaluate strategy: Split current range to fit in available holes
			// todo - are checks required to avoid splitting on the suffix instruction?
			strategy.availableRegisterHole.Evaluate(imlSegment, subrangeItr, livenessTimeline, localAvailableRegsMask, allowedRegs);
			SelectStrategyIfBetter(strategy.availableRegisterHole);
			// evaluate strategy: Explode inter-segment ranges
			strategy.explodeRange.Evaluate(imlSegment, subrangeItr, livenessTimeline, allowedRegs);
			SelectStrategyIfBetter(strategy.explodeRange);
		}
		else // if subrangeItr->interval2.ExtendsIntoNextSegment()
		{
			strategy.explodeRangeInter.Reset();
			strategy.explodeRangeInter.Evaluate(imlSegment, subrangeItr, livenessTimeline, allowedRegs);
			SelectStrategyIfBetter(strategy.explodeRangeInter);
		}
		// choose strategy
		if(selectedStrategy)
		{
			selectedStrategy->Apply(ppcImlGenContext, imlSegment, subrangeItr);
		}
		else
		{
			// none of the evulated strategies can be applied, this should only happen if the segment extends into the next segment(s) for which we have no good strategy
			cemu_assert_debug(subrangeItr->interval2.ExtendsPreviousSegment());
			// alternative strategy if we have no other choice: explode current range
			PPCRecRA_explodeRange(ppcImlGenContext, subrangeItr);
		}
		return false;
	}
	return true;
}

void IMLRA_AssignRegisters(IMLRegisterAllocatorContext& ctx, ppcImlGenContext_t* ppcImlGenContext)
{
	// start with frequently executed segments first
	sint32 maxLoopDepth = 0;
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		maxLoopDepth = std::max(maxLoopDepth, segIt->loopDepth);
	}
	// assign fixed registers first
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
		IMLRA_HandleFixedRegisters(ppcImlGenContext, segIt);

	while (true)
	{
		bool done = false;
		for (sint32 d = maxLoopDepth; d >= 0; d--)
		{
			for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
			{
				if (segIt->loopDepth != d)
					continue;
				done = IMLRA_AssignSegmentRegisters(ctx, ppcImlGenContext, segIt);
				if (done == false)
					break;
			}
			if (done == false)
				break;
		}
		if (done)
			break;
	}
}

void IMLRA_ReshapeForRegisterAllocation(ppcImlGenContext_t* ppcImlGenContext)
{
	// insert empty segments after every non-taken branch if the linked segment has more than one input
	// this gives the register allocator more room to create efficient spill code
	size_t segmentIndex = 0;
	while (segmentIndex < ppcImlGenContext->segmentList2.size())
	{
		IMLSegment* imlSegment = ppcImlGenContext->segmentList2[segmentIndex];
		if (imlSegment->nextSegmentIsUncertain)
		{
			segmentIndex++;
			continue;
		}
		if (imlSegment->nextSegmentBranchTaken == nullptr || imlSegment->nextSegmentBranchNotTaken == nullptr)
		{
			segmentIndex++;
			continue;
		}
		if (imlSegment->nextSegmentBranchNotTaken->list_prevSegments.size() <= 1)
		{
			segmentIndex++;
			continue;
		}
		if (imlSegment->nextSegmentBranchNotTaken->isEnterable)
		{
			segmentIndex++;
			continue;
		}
		PPCRecompilerIml_insertSegments(ppcImlGenContext, segmentIndex + 1, 1);
		IMLSegment* imlSegmentP0 = ppcImlGenContext->segmentList2[segmentIndex + 0];
		IMLSegment* imlSegmentP1 = ppcImlGenContext->segmentList2[segmentIndex + 1];
		IMLSegment* nextSegment = imlSegment->nextSegmentBranchNotTaken;
		IMLSegment_RemoveLink(imlSegmentP0, nextSegment);
		IMLSegment_SetLinkBranchNotTaken(imlSegmentP1, nextSegment);
		IMLSegment_SetLinkBranchNotTaken(imlSegmentP0, imlSegmentP1);
		segmentIndex++;
	}
	// detect loops
	for (size_t s = 0; s < ppcImlGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ppcImlGenContext->segmentList2[s];
		imlSegment->momentaryIndex = s;
	}
	for (size_t s = 0; s < ppcImlGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ppcImlGenContext->segmentList2[s];
		PPCRecRA_identifyLoop(ppcImlGenContext, imlSegment);
	}
}

IMLRARegAbstractLiveness* _GetAbstractRange(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment, IMLRegID regId)
{
	auto& segMap = ctx.GetSegmentAbstractRangeMap(imlSegment);
	auto it = segMap.find(regId);
	return it != segMap.end() ? &it->second : nullptr;
}

// scan instructions and establish register usage range for segment
void IMLRA_CalculateSegmentMinMaxAbstractRanges(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment)
{
	size_t instructionIndex = 0;
	IMLUsedRegisters gprTracking;
	auto& segDistMap = ctx.GetSegmentAbstractRangeMap(imlSegment);
	while (instructionIndex < imlSegment->imlList.size())
	{
		imlSegment->imlList[instructionIndex].CheckRegisterUsage(&gprTracking);
		gprTracking.ForEachAccessedGPR([&](IMLReg gprReg, bool isWritten) {
			IMLRegID gprId = gprReg.GetRegID();
			auto it = segDistMap.find(gprId);
			if (it == segDistMap.end())
			{
				segDistMap.try_emplace(gprId, gprReg.GetBaseFormat(), (sint32)instructionIndex, (sint32)instructionIndex + 1);
				ctx.regIdToBaseFormat.try_emplace(gprId, gprReg.GetBaseFormat());
			}
			else
			{
				it->second.TrackInstruction(instructionIndex);
#ifdef CEMU_DEBUG_ASSERT
				cemu_assert_debug(ctx.regIdToBaseFormat[gprId] == gprReg.GetBaseFormat()); // the base type per register always has to be the same
#endif
			}
			});
		instructionIndex++;
	}
}

void IMLRA_CalculateLivenessRanges(IMLRegisterAllocatorContext& ctx)
{
	// for each register calculate min/max index of usage range within each segment
	size_t dbgIndex = 0;
	for (IMLSegment* segIt : ctx.deprGenContext->segmentList2)
	{
		cemu_assert_debug(segIt->momentaryIndex == dbgIndex);
		IMLRA_CalculateSegmentMinMaxAbstractRanges(ctx, segIt);
		dbgIndex++;
	}
}

raLivenessRange* PPCRecRA_convertToMappedRanges(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment, IMLRegID vGPR, IMLName name)
{
	IMLRARegAbstractLiveness* abstractRange = _GetAbstractRange(ctx, imlSegment, vGPR);
	if (!abstractRange)
		return nullptr;
	if (abstractRange->isProcessed)
	{
		// return already existing segment
		raLivenessRange* existingRange = IMLRA_GetSubrange(imlSegment, vGPR);
		cemu_assert_debug(existingRange);
		return existingRange;
	}
	abstractRange->isProcessed = true;
	// create subrange
#ifdef CEMU_DEBUG_ASSERT
	cemu_assert_debug(IMLRA_GetSubrange(imlSegment, vGPR) == nullptr);
#endif
	cemu_assert_debug(
	(abstractRange->usageStart == abstractRange->usageEnd && (abstractRange->usageStart == RA_INTER_RANGE_START || abstractRange->usageStart == RA_INTER_RANGE_END)) ||
	abstractRange->usageStart < abstractRange->usageEnd); // usageEnd is exclusive so it should always be larger
	sint32 inclusiveEnd = abstractRange->usageEnd;
	if(inclusiveEnd != RA_INTER_RANGE_START && inclusiveEnd != RA_INTER_RANGE_END)
		inclusiveEnd--; // subtract one, because usageEnd is exclusive, but the end value of the interval passed to createSubrange is inclusive
	raInterval interval;
	interval.SetInterval(abstractRange->usageStart, true, inclusiveEnd, true);
	raLivenessRange* subrange = PPCRecRA_createSubrange2(ctx.deprGenContext, imlSegment, vGPR, name, interval.start, interval.end);
	// traverse forward
	if (abstractRange->usageEnd == RA_INTER_RANGE_END)
	{
		if (imlSegment->nextSegmentBranchTaken)
		{
			IMLRARegAbstractLiveness* branchTakenRange = _GetAbstractRange(ctx, imlSegment->nextSegmentBranchTaken, vGPR);
			if (branchTakenRange && branchTakenRange->usageStart == RA_INTER_RANGE_START)
			{
				subrange->subrangeBranchTaken = PPCRecRA_convertToMappedRanges(ctx, imlSegment->nextSegmentBranchTaken, vGPR, name);
				subrange->subrangeBranchTaken->previousRanges.push_back(subrange);
				cemu_assert_debug(subrange->subrangeBranchTaken->interval2.ExtendsPreviousSegment());
			}
		}
		if (imlSegment->nextSegmentBranchNotTaken)
		{
			IMLRARegAbstractLiveness* branchNotTakenRange = _GetAbstractRange(ctx, imlSegment->nextSegmentBranchNotTaken, vGPR);
			if (branchNotTakenRange && branchNotTakenRange->usageStart == RA_INTER_RANGE_START)
			{
				subrange->subrangeBranchNotTaken = PPCRecRA_convertToMappedRanges(ctx, imlSegment->nextSegmentBranchNotTaken, vGPR, name);
				subrange->subrangeBranchNotTaken->previousRanges.push_back(subrange);
				cemu_assert_debug(subrange->subrangeBranchNotTaken->interval2.ExtendsPreviousSegment());
			}
		}
	}
	// traverse backward
	if (abstractRange->usageStart == RA_INTER_RANGE_START)
	{
		for (auto& it : imlSegment->list_prevSegments)
		{
			IMLRARegAbstractLiveness* prevRange = _GetAbstractRange(ctx, it, vGPR);
			if(!prevRange)
				continue;
			if (prevRange->usageEnd == RA_INTER_RANGE_END)
				PPCRecRA_convertToMappedRanges(ctx, it, vGPR, name);
		}
	}
	// for subranges which exit the segment at the end there is a hard requirement that they cover the suffix instruction
	// this is due to range load instructions being inserted before the suffix instruction
	// todo - currently later steps might break this assumption, look into this
	// if (subrange->interval2.ExtendsIntoNextSegment())
	// {
	// 	if (imlSegment->HasSuffixInstruction())
	// 	{
	// 		cemu_assert_debug(subrange->interval2.start.GetInstructionIndex() <= imlSegment->GetSuffixInstructionIndex());
	// 	}
	// }
	return subrange;
}

// take abstract range data and create LivenessRanges
void IMLRA_ConvertAbstractToLivenessRanges(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment)
{
	const std::unordered_map<IMLRegID, raLivenessRange*>& regToSubrange = IMLRA_GetSubrangeMap(imlSegment);

	auto AddOrUpdateFixedRegRequirement = [&](IMLRegID regId, sint32 instructionIndex, bool isInput, const IMLPhysRegisterSet& physRegSet)
	{
		raLivenessRange* subrange = regToSubrange.find(regId)->second;
		cemu_assert_debug(subrange);
		raFixedRegRequirement tmp;
		tmp.pos.Set(instructionIndex, isInput);
		tmp.allowedReg = physRegSet;
		if(subrange->list_fixedRegRequirements.empty() || subrange->list_fixedRegRequirements.back().pos != tmp.pos)
			subrange->list_fixedRegRequirements.push_back(tmp);
	};

	// convert abstract min-max ranges to liveness range objects
	auto& segMap = ctx.GetSegmentAbstractRangeMap(imlSegment);
	for (auto& it : segMap)
	{
		if(it.second.isProcessed)
			continue;
		IMLRegID regId = it.first;
		PPCRecRA_convertToMappedRanges(ctx, imlSegment, regId, ctx.raParam->regIdToName.find(regId)->second);
	}
	// fill created ranges with read/write location indices
	// note that at this point there is only one range per register per segment
	// and the algorithm below relies on this
	size_t index = 0;
	IMLUsedRegisters gprTracking;
	while (index < imlSegment->imlList.size())
	{
		imlSegment->imlList[index].CheckRegisterUsage(&gprTracking);
		gprTracking.ForEachAccessedGPR([&](IMLReg gprReg, bool isWritten) {
			IMLRegID gprId = gprReg.GetRegID();
			raLivenessRange* subrange = regToSubrange.find(gprId)->second;
			PPCRecRA_updateOrAddSubrangeLocation(subrange, index, !isWritten, isWritten);
			cemu_assert_debug(!subrange->interval2.start.IsInstructionIndex() || subrange->interval2.start.GetInstructionIndex() <= index);
			cemu_assert_debug(!subrange->interval2.end.IsInstructionIndex() || subrange->interval2.end.GetInstructionIndex() >= index);
			});
		// check fixed register requirements
		IMLFixedRegisters fixedRegs;
		GetInstructionFixedRegisters(&imlSegment->imlList[index], fixedRegs);
		for(auto& fixedRegAccess : fixedRegs.listInput)
		{
			if(fixedRegAccess.reg != IMLREG_INVALID)
				AddOrUpdateFixedRegRequirement(fixedRegAccess.reg.GetRegID(), index, true, fixedRegAccess.physRegSet);
		}
		for(auto& fixedRegAccess : fixedRegs.listOutput)
		{
			if(fixedRegAccess.reg != IMLREG_INVALID)
				AddOrUpdateFixedRegRequirement(fixedRegAccess.reg.GetRegID(), index, false, fixedRegAccess.physRegSet);
		}
		index++;
	}
}

void IMLRA_extendAbstractRangeToEndOfSegment(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment, IMLRegID regId)
{
	auto& segDistMap = ctx.GetSegmentAbstractRangeMap(imlSegment);
	auto it = segDistMap.find(regId);
	if (it == segDistMap.end())
	{
		sint32 startIndex;
		if(imlSegment->HasSuffixInstruction())
			startIndex = imlSegment->GetSuffixInstructionIndex();
		else
			startIndex = RA_INTER_RANGE_END;
		segDistMap.try_emplace((IMLRegID)regId, IMLRegFormat::INVALID_FORMAT, startIndex, RA_INTER_RANGE_END);
	}
	else
	{
		it->second.usageEnd = RA_INTER_RANGE_END;
	}
}

void IMLRA_extendAbstractRangeToBeginningOfSegment(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment, IMLRegID regId)
{
	auto& segDistMap = ctx.GetSegmentAbstractRangeMap(imlSegment);
	auto it = segDistMap.find(regId);
	if (it == segDistMap.end())
	{
		segDistMap.try_emplace((IMLRegID)regId, IMLRegFormat::INVALID_FORMAT, RA_INTER_RANGE_START, RA_INTER_RANGE_START);
	}
	else
	{
		it->second.usageStart = RA_INTER_RANGE_START;
	}
	// propagate backwards
	for (auto& it : imlSegment->list_prevSegments)
	{
		IMLRA_extendAbstractRangeToEndOfSegment(ctx, it, regId);
	}
}

void IMLRA_connectAbstractRanges(IMLRegisterAllocatorContext& ctx, IMLRegID regId, IMLSegment** route, sint32 routeDepth)
{
#ifdef CEMU_DEBUG_ASSERT
	if (routeDepth < 2)
		assert_dbg();
#endif
	// extend starting range to end of segment
	IMLRA_extendAbstractRangeToEndOfSegment(ctx, route[0], regId);
	// extend all the connecting segments in both directions
	for (sint32 i = 1; i < (routeDepth - 1); i++)
	{
		IMLRA_extendAbstractRangeToEndOfSegment(ctx, route[i], regId);
		IMLRA_extendAbstractRangeToBeginningOfSegment(ctx, route[i], regId);
	}
	// extend the final segment towards the beginning
	IMLRA_extendAbstractRangeToBeginningOfSegment(ctx, route[routeDepth - 1], regId);
}

void _IMLRA_checkAndTryExtendRange(IMLRegisterAllocatorContext& ctx, IMLSegment* currentSegment, IMLRegID regID, sint32 distanceLeft, IMLSegment** route, sint32 routeDepth)
{
	if (routeDepth >= 64)
	{
		cemuLog_logDebug(LogType::Force, "Recompiler RA route maximum depth exceeded\n");
		return;
	}
	route[routeDepth] = currentSegment;

	IMLRARegAbstractLiveness* range = _GetAbstractRange(ctx, currentSegment, regID);

	if (!range)
	{
		// measure distance over entire segment
		distanceLeft -= (sint32)currentSegment->imlList.size();
		if (distanceLeft > 0)
		{
			if (currentSegment->nextSegmentBranchNotTaken)
				_IMLRA_checkAndTryExtendRange(ctx, currentSegment->nextSegmentBranchNotTaken, regID, distanceLeft, route, routeDepth + 1);
			if (currentSegment->nextSegmentBranchTaken)
				_IMLRA_checkAndTryExtendRange(ctx, currentSegment->nextSegmentBranchTaken, regID, distanceLeft, route, routeDepth + 1);
		}
		return;
	}
	else
	{
		// measure distance to range
		if (range->usageStart == RA_INTER_RANGE_END)
		{
			if (distanceLeft < (sint32)currentSegment->imlList.size())
				return; // range too far away
		}
		else if (range->usageStart != RA_INTER_RANGE_START && range->usageStart > distanceLeft)
			return; // out of range
		// found close range -> connect ranges
		IMLRA_connectAbstractRanges(ctx, regID, route, routeDepth + 1);
	}
}

void PPCRecRA_checkAndTryExtendRange(IMLRegisterAllocatorContext& ctx, IMLSegment* currentSegment, IMLRARegAbstractLiveness* range, IMLRegID regID)
{
	cemu_assert_debug(range->usageEnd >= 0);
	// count instructions to end of initial segment
	sint32 instructionsUntilEndOfSeg;
	if (range->usageEnd == RA_INTER_RANGE_END)
		instructionsUntilEndOfSeg = 0;
	else
		instructionsUntilEndOfSeg = (sint32)currentSegment->imlList.size() - range->usageEnd;
	cemu_assert_debug(instructionsUntilEndOfSeg >= 0);
	sint32 remainingScanDist = 45 - instructionsUntilEndOfSeg;
	if (remainingScanDist <= 0)
		return; // can't reach end

	IMLSegment* route[64];
	route[0] = currentSegment;
	if (currentSegment->nextSegmentBranchNotTaken)
		_IMLRA_checkAndTryExtendRange(ctx, currentSegment->nextSegmentBranchNotTaken, regID, remainingScanDist, route, 1);
	if (currentSegment->nextSegmentBranchTaken)
		_IMLRA_checkAndTryExtendRange(ctx, currentSegment->nextSegmentBranchTaken, regID, remainingScanDist, route, 1);
}

void PPCRecRA_mergeCloseRangesForSegmentV2(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment)
{
	auto& segMap = ctx.GetSegmentAbstractRangeMap(imlSegment);
	for (auto& it : segMap)
	{
		PPCRecRA_checkAndTryExtendRange(ctx, imlSegment, &(it.second), it.first);
	}
#ifdef CEMU_DEBUG_ASSERT
	if (imlSegment->list_prevSegments.empty() == false && imlSegment->isEnterable)
		assert_dbg();
	if ((imlSegment->nextSegmentBranchNotTaken != nullptr || imlSegment->nextSegmentBranchTaken != nullptr) && imlSegment->nextSegmentIsUncertain)
		assert_dbg();
#endif
}

void PPCRecRA_followFlowAndExtendRanges(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment)
{
	std::vector<IMLSegment*> list_segments;
	std::vector<bool> list_processedSegment;
	size_t segmentCount = ctx.deprGenContext->segmentList2.size();
	list_segments.reserve(segmentCount+1);
	list_processedSegment.resize(segmentCount);

	auto markSegProcessed = [&list_processedSegment](IMLSegment* seg) {list_processedSegment[seg->momentaryIndex] = true; };
	auto isSegProcessed = [&list_processedSegment](IMLSegment* seg) -> bool { return list_processedSegment[seg->momentaryIndex]; };
	markSegProcessed(imlSegment);

	sint32 index = 0;
	list_segments.push_back(imlSegment);
	while (index < list_segments.size())
	{
		IMLSegment* currentSegment = list_segments[index];
		PPCRecRA_mergeCloseRangesForSegmentV2(ctx, currentSegment);
		// follow flow
		if (currentSegment->nextSegmentBranchNotTaken && !isSegProcessed(currentSegment->nextSegmentBranchNotTaken))
		{
			markSegProcessed(currentSegment->nextSegmentBranchNotTaken);
			list_segments.push_back(currentSegment->nextSegmentBranchNotTaken);
		}
		if (currentSegment->nextSegmentBranchTaken && !isSegProcessed(currentSegment->nextSegmentBranchTaken))
		{
			markSegProcessed(currentSegment->nextSegmentBranchTaken);
			list_segments.push_back(currentSegment->nextSegmentBranchTaken);
		}
		index++;
	}
}

void IMLRA_MergeCloseAbstractRanges(IMLRegisterAllocatorContext& ctx)
{
	for (size_t s = 0; s < ctx.deprGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ctx.deprGenContext->segmentList2[s];
		if (!imlSegment->list_prevSegments.empty())
			continue; // not an entry/standalone segment
		PPCRecRA_followFlowAndExtendRanges(ctx, imlSegment);
	}
}

void IMLRA_ExtendAbstractRangesOutOfLoops(IMLRegisterAllocatorContext& ctx)
{
	for (size_t s = 0; s < ctx.deprGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ctx.deprGenContext->segmentList2[s];
		auto localLoopDepth = imlSegment->loopDepth;
		if (localLoopDepth <= 0)
			continue; // not inside a loop
		// look for loop exit
		bool hasLoopExit = false;
		if (imlSegment->nextSegmentBranchTaken && imlSegment->nextSegmentBranchTaken->loopDepth < localLoopDepth)
		{
			hasLoopExit = true;
		}
		if (imlSegment->nextSegmentBranchNotTaken && imlSegment->nextSegmentBranchNotTaken->loopDepth < localLoopDepth)
		{
			hasLoopExit = true;
		}
		if (hasLoopExit == false)
			continue;

		// extend looping ranges into all exits (this allows the data flow analyzer to move stores out of the loop)		
		auto& segMap = ctx.GetSegmentAbstractRangeMap(imlSegment);
		for (auto& it : segMap)
		{
			if(it.second.usageEnd != RA_INTER_RANGE_END)
				continue;
			if (imlSegment->nextSegmentBranchTaken)
				IMLRA_extendAbstractRangeToBeginningOfSegment(ctx, imlSegment->nextSegmentBranchTaken, it.first);
			if (imlSegment->nextSegmentBranchNotTaken)
				IMLRA_extendAbstractRangeToBeginningOfSegment(ctx, imlSegment->nextSegmentBranchNotTaken, it.first);
		}
	}
}

void IMLRA_ProcessFlowAndCalculateLivenessRanges(IMLRegisterAllocatorContext& ctx)
{
	IMLRA_MergeCloseAbstractRanges(ctx);
	// extra pass to move register loads and stores out of loops
	IMLRA_ExtendAbstractRangesOutOfLoops(ctx);
	// calculate liveness ranges
	for (auto& segIt : ctx.deprGenContext->segmentList2)
		IMLRA_ConvertAbstractToLivenessRanges(ctx, segIt);
}

void IMLRA_AnalyzeSubrangeDataDependency(raLivenessRange* subrange)
{
	bool isRead = false;
	bool isWritten = false;
	bool isOverwritten = false;
	for (auto& location : subrange->list_locations)
	{
		if (location.isRead)
		{
			isRead = true;
		}
		if (location.isWrite)
		{
			if (isRead == false)
				isOverwritten = true;
			isWritten = true;
		}
	}
	subrange->_noLoad = isOverwritten;
	subrange->hasStore = isWritten;

	if (subrange->interval2.ExtendsPreviousSegment())
		subrange->_noLoad = true;
}


struct subrangeEndingInfo_t
{
	//boost::container::small_vector<raLivenessSubrange_t*, 32> subrangeList2;
	raLivenessRange* subrangeList[SUBRANGE_LIST_SIZE];
	sint32 subrangeCount;

	bool hasUndefinedEndings;
};

void _findSubrangeWriteEndings(raLivenessRange* subrange, uint32 iterationIndex, sint32 depth, subrangeEndingInfo_t* info)
{
	if (depth >= 30)
	{
		info->hasUndefinedEndings = true;
		return;
	}
	if (subrange->lastIterationIndex == iterationIndex)
		return; // already processed
	subrange->lastIterationIndex = iterationIndex;
	if (subrange->hasStoreDelayed)
		return; // no need to traverse this subrange
	IMLSegment* imlSegment = subrange->imlSegment;
	if (!subrange->interval2.ExtendsIntoNextSegment())
	{
		// ending segment
		if (info->subrangeCount >= SUBRANGE_LIST_SIZE)
		{
			info->hasUndefinedEndings = true;
			return;
		}
		else
		{
			info->subrangeList[info->subrangeCount] = subrange;
			info->subrangeCount++;
		}
		return;
	}

	// traverse next subranges in flow
	if (imlSegment->nextSegmentBranchNotTaken)
	{
		if (subrange->subrangeBranchNotTaken == nullptr)
		{
			info->hasUndefinedEndings = true;
		}
		else
		{
			_findSubrangeWriteEndings(subrange->subrangeBranchNotTaken, iterationIndex, depth + 1, info);
		}
	}
	if (imlSegment->nextSegmentBranchTaken)
	{
		if (subrange->subrangeBranchTaken == nullptr)
		{
			info->hasUndefinedEndings = true;
		}
		else
		{
			_findSubrangeWriteEndings(subrange->subrangeBranchTaken, iterationIndex, depth + 1, info);
		}
	}
}

static void IMLRA_AnalyzeRangeDataFlow(raLivenessRange* subrange)
{
	if (!subrange->interval2.ExtendsIntoNextSegment())
		return;
	// analyze data flow across segments (if this segment has writes)
	if (subrange->hasStore)
	{
		subrangeEndingInfo_t writeEndingInfo;
		writeEndingInfo.subrangeCount = 0;
		writeEndingInfo.hasUndefinedEndings = false;
		_findSubrangeWriteEndings(subrange, PPCRecRA_getNextIterationIndex(), 0, &writeEndingInfo);
		if (writeEndingInfo.hasUndefinedEndings == false)
		{
			// get cost of delaying store into endings
			sint32 delayStoreCost = 0;
			bool alreadyStoredInAllEndings = true;
			for (sint32 i = 0; i < writeEndingInfo.subrangeCount; i++)
			{
				raLivenessRange* subrangeItr = writeEndingInfo.subrangeList[i];
				if( subrangeItr->hasStore )
					continue; // this ending already stores, no extra cost
				alreadyStoredInAllEndings = false;
				sint32 storeCost = PPCRecRARange_getReadWriteCost(subrangeItr->imlSegment);
				delayStoreCost = std::max(storeCost, delayStoreCost);
			}
			if (alreadyStoredInAllEndings)
			{
				subrange->hasStore = false;
				subrange->hasStoreDelayed = true;
			}
			else if (delayStoreCost <= PPCRecRARange_getReadWriteCost(subrange->imlSegment))
			{
				subrange->hasStore = false;
				subrange->hasStoreDelayed = true;
				for (sint32 i = 0; i < writeEndingInfo.subrangeCount; i++)
				{
					raLivenessRange* subrangeItr = writeEndingInfo.subrangeList[i];
					subrangeItr->hasStore = true;
				}
			}
		}
	}
}

void IMLRA_AnalyzeRangeDataFlow(ppcImlGenContext_t* ppcImlGenContext)
{
	// this function is called after _AssignRegisters(), which means that all liveness ranges are already final and must not be modified anymore
	// track read/write dependencies per segment
	for(auto& seg : ppcImlGenContext->segmentList2)
	{
		raLivenessRange* subrange = seg->raInfo.linkedList_allSubranges;
		while(subrange)
		{
			IMLRA_AnalyzeSubrangeDataDependency(subrange);
			subrange = subrange->link_allSegmentRanges.next;
		}
	}
	// propagate information across segment boundaries
	for(auto& seg : ppcImlGenContext->segmentList2)
	{
		raLivenessRange* subrange = seg->raInfo.linkedList_allSubranges;
		while(subrange)
		{
			IMLRA_AnalyzeRangeDataFlow(subrange);
			subrange = subrange->link_allSegmentRanges.next;
		}
	}
}

/* Generate move instructions */

inline IMLReg _MakeNativeReg(IMLRegFormat baseFormat, IMLRegID regId)
{
	return IMLReg(baseFormat, baseFormat, 0, regId);
}

#define DEBUG_RA_INSTRUCTION_GEN 0

// prepass for IMLRA_GenerateSegmentMoveInstructions which updates all virtual registers to their physical counterparts
void IMLRA_RewriteRegisters(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment)
{
	std::unordered_map<IMLRegID, IMLRegID> virtId2PhysReg;
	boost::container::small_vector<raLivenessRange*, 64> activeRanges;
	raLivenessRange* currentRange = imlSegment->raInfo.linkedList_allSubranges;
	raInstructionEdge currentEdge;
	for(size_t i=0; i<imlSegment->imlList.size(); i++)
	{
		currentEdge.Set(i, false); // set to instruction index on output edge
		// activate ranges which begin before or during this instruction
		while(currentRange && currentRange->interval2.start <= currentEdge)
		{
			cemu_assert_debug(virtId2PhysReg.find(currentRange->GetVirtualRegister()) == virtId2PhysReg.end() || virtId2PhysReg[currentRange->GetVirtualRegister()] == currentRange->GetPhysicalRegister()); // check for register conflict

			virtId2PhysReg[currentRange->GetVirtualRegister()] = currentRange->GetPhysicalRegister();
			activeRanges.push_back(currentRange);
			currentRange = currentRange->link_allSegmentRanges.next;
		}
		// rewrite registers
		imlSegment->imlList[i].RewriteGPR(virtId2PhysReg);
		// deactivate ranges which end during this instruction
		auto it = activeRanges.begin();
		while(it != activeRanges.end())
		{
			if((*it)->interval2.end <= currentEdge)
			{
				virtId2PhysReg.erase((*it)->GetVirtualRegister());
				it = activeRanges.erase(it);
			}
			else
				++it;
		}
	}
}

void IMLRA_GenerateSegmentMoveInstructions2(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment)
{
	IMLRA_RewriteRegisters(ctx, imlSegment);

#if DEBUG_RA_INSTRUCTION_GEN
	cemuLog_log(LogType::Force, "");
	cemuLog_log(LogType::Force, "[Seg before RA]");
	IMLDebug_DumpSegment(nullptr, imlSegment, true);
#endif

	bool hadSuffixInstruction = imlSegment->HasSuffixInstruction();

	std::vector<IMLInstruction> rebuiltInstructions;
	sint32 numInstructionsWithoutSuffix = (sint32)imlSegment->imlList.size() - (imlSegment->HasSuffixInstruction() ? 1 : 0);

	if(imlSegment->imlList.empty())
	{
		// empty segments need special handling (todo - look into merging this with the core logic below eventually)
		// store all ranges
		raLivenessRange* currentRange = imlSegment->raInfo.linkedList_allSubranges;
		while(currentRange)
		{
			if(currentRange->hasStore)
				rebuiltInstructions.emplace_back().make_name_r(currentRange->GetName(), _MakeNativeReg(ctx.regIdToBaseFormat[currentRange->GetVirtualRegister()], currentRange->GetPhysicalRegister()));
			currentRange = currentRange->link_allSegmentRanges.next;
		}
		// load ranges
		currentRange = imlSegment->raInfo.linkedList_allSubranges;
		while(currentRange)
		{
			if(!currentRange->_noLoad)
			{
				cemu_assert_debug(currentRange->interval2.ExtendsIntoNextSegment());
				rebuiltInstructions.emplace_back().make_r_name(_MakeNativeReg(ctx.regIdToBaseFormat[currentRange->GetVirtualRegister()], currentRange->GetPhysicalRegister()), currentRange->GetName());
			}
			currentRange = currentRange->link_allSegmentRanges.next;
		}
		imlSegment->imlList = std::move(rebuiltInstructions);
		return;
	}

	// make sure that no range exceeds the suffix instruction input edge except if they need to be loaded for the next segment (todo - for those, set the start point accordingly?)
	{
		raLivenessRange* currentRange = imlSegment->raInfo.linkedList_allSubranges;
		raInstructionEdge edge;
		if(imlSegment->HasSuffixInstruction())
			edge.Set(numInstructionsWithoutSuffix, true);
		else
			edge.Set(numInstructionsWithoutSuffix-1, false);

		while(currentRange)
		{
			if(!currentRange->interval2.IsNextSegmentOnly() && currentRange->interval2.end > edge)
			{
				currentRange->interval2.SetEnd(edge);
			}
			currentRange = currentRange->link_allSegmentRanges.next;
		}
	}

#if DEBUG_RA_INSTRUCTION_GEN
	cemuLog_log(LogType::Force, "");
	cemuLog_log(LogType::Force, "--- Intermediate liveness info ---");
	{
		raLivenessRange* dbgRange = imlSegment->raInfo.linkedList_allSubranges;
		while(dbgRange)
		{
			cemuLog_log(LogType::Force, "Range i{}: {}-{}", dbgRange->GetVirtualRegister(), dbgRange->interval2.start.GetDebugString(), dbgRange->interval2.end.GetDebugString());
			dbgRange = dbgRange->link_allSegmentRanges.next;
		}
	}
#endif

	boost::container::small_vector<raLivenessRange*, 64> activeRanges;
	// first we add all the ranges that extend from the previous segment, some of these will end immediately at the first instruction so we might need to store them early
	raLivenessRange* currentRange = imlSegment->raInfo.linkedList_allSubranges;

	// make all ranges active that start on RA_INTER_RANGE_START
	while(currentRange && currentRange->interval2.start.ConnectsToPreviousSegment())
	{
		activeRanges.push_back(currentRange);
		currentRange = currentRange->link_allSegmentRanges.next;
	}
	// store all ranges that end before the first output edge (includes RA_INTER_RANGE_START)
	auto it = activeRanges.begin();
	raInstructionEdge firstOutputEdge;
	firstOutputEdge.Set(0, false);
	while(it != activeRanges.end())
	{
		if( (*it)->interval2.end < firstOutputEdge)
		{
			raLivenessRange* storedRange = *it;
			if(storedRange->hasStore)
				rebuiltInstructions.emplace_back().make_name_r(storedRange->GetName(), _MakeNativeReg(ctx.regIdToBaseFormat[storedRange->GetVirtualRegister()], storedRange->GetPhysicalRegister()));
			it = activeRanges.erase(it);
			continue;
		}
		++it;
	}

	sint32 numInstructions = (sint32)imlSegment->imlList.size();
	for(sint32 i=0; i<numInstructions; i++)
	{
		raInstructionEdge curEdge;
		// input edge
		curEdge.SetRaw(i*2+1); // +1 to include ranges that start at the output of the instruction
		while(currentRange && currentRange->interval2.start <= curEdge)
		{
			if(!currentRange->_noLoad)
			{
				rebuiltInstructions.emplace_back().make_r_name(_MakeNativeReg(ctx.regIdToBaseFormat[currentRange->GetVirtualRegister()], currentRange->GetPhysicalRegister()), currentRange->GetName());
			}
			activeRanges.push_back(currentRange);
			currentRange = currentRange->link_allSegmentRanges.next;
		}
		// copy instruction
		rebuiltInstructions.push_back(imlSegment->imlList[i]);
		// output edge
		curEdge.SetRaw(i*2+1+1);
		// also store ranges that end on the next input edge, we handle this by adding an extra 1 above
		auto it = activeRanges.begin();
		while(it != activeRanges.end())
		{
			if( (*it)->interval2.end <= curEdge)
			{
				// range expires
				// we cant erase it from virtId2PhysReg right away because a store might happen before the last use (the +1 thing above)


				// todo - check hasStore
				raLivenessRange* storedRange = *it;
				if(storedRange->hasStore)
				{
					cemu_assert_debug(i != numInstructionsWithoutSuffix); // not allowed to emit after suffix
					rebuiltInstructions.emplace_back().make_name_r(storedRange->GetName(), _MakeNativeReg(ctx.regIdToBaseFormat[storedRange->GetVirtualRegister()], storedRange->GetPhysicalRegister()));
				}

				it = activeRanges.erase(it);
				continue;
			}
			++it;
		}
	}
	// if there is no suffix instruction we currently need to handle the final loads here
	cemu_assert_debug(hadSuffixInstruction == imlSegment->HasSuffixInstruction());
	if(imlSegment->HasSuffixInstruction())
	{
		cemu_assert_debug(!currentRange); // currentRange should be NULL?
		for(auto& remainingRange : activeRanges)
		{
			cemu_assert_debug(!remainingRange->hasStore);
		}
	}
	else
	{
		for(auto& remainingRange : activeRanges)
		{
			cemu_assert_debug(!remainingRange->hasStore); // this range still needs to be stored
		}
		while(currentRange)
		{
			cemu_assert_debug(currentRange->interval2.IsNextSegmentOnly());
			cemu_assert_debug(!currentRange->_noLoad);
			rebuiltInstructions.emplace_back().make_r_name(_MakeNativeReg(ctx.regIdToBaseFormat[currentRange->GetVirtualRegister()], currentRange->GetPhysicalRegister()), currentRange->GetName());
			currentRange = currentRange->link_allSegmentRanges.next;
		}
	}

	imlSegment->imlList = std::move(rebuiltInstructions);
	cemu_assert_debug(hadSuffixInstruction == imlSegment->HasSuffixInstruction());

#if DEBUG_RA_INSTRUCTION_GEN
	cemuLog_log(LogType::Force, "");
	cemuLog_log(LogType::Force, "[Seg after RA]");
	IMLDebug_DumpSegment(nullptr, imlSegment, false);
#endif
}

void IMLRA_GenerateMoveInstructions(IMLRegisterAllocatorContext& ctx)
{
	for (size_t s = 0; s < ctx.deprGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ctx.deprGenContext->segmentList2[s];
		IMLRA_GenerateSegmentMoveInstructions2(ctx, imlSegment);
	}
}

void DbgVerifyAllRanges(IMLRegisterAllocatorContext& ctx)
{
	for (size_t s = 0; s < ctx.deprGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ctx.deprGenContext->segmentList2[s];
		raLivenessRange* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while(subrangeItr)
		{
			PPCRecRA_debugValidateSubrange(subrangeItr);
			subrangeItr = subrangeItr->link_allSegmentRanges.next;
		}
	}
}

void IMLRegisterAllocator_AllocateRegisters(ppcImlGenContext_t* ppcImlGenContext, IMLRegisterAllocatorParameters& raParam)
{
	IMLRegisterAllocatorContext ctx;
	ctx.raParam = &raParam;
	ctx.deprGenContext = ppcImlGenContext;

	IMLRA_ReshapeForRegisterAllocation(ppcImlGenContext);
	ppcImlGenContext->UpdateSegmentIndices(); // update momentaryIndex of each segment
	ctx.perSegmentAbstractRanges.resize(ppcImlGenContext->segmentList2.size());
	IMLRA_CalculateLivenessRanges(ctx);
	IMLRA_ProcessFlowAndCalculateLivenessRanges(ctx);
	IMLRA_AssignRegisters(ctx, ppcImlGenContext);
	DbgVerifyAllRanges(ctx); // DEBUG
	IMLRA_AnalyzeRangeDataFlow(ppcImlGenContext);
	IMLRA_GenerateMoveInstructions(ctx);

	IMLRA_DeleteAllRanges(ppcImlGenContext);
}
