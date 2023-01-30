#include "IML.h"

#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"
#include "IMLRegisterAllocator.h"
#include "IMLRegisterAllocatorRanges.h"

#include "../BackendX64/BackendX64.h"

#include <boost/container/small_vector.hpp>

struct IMLRegisterAllocatorContext
{
	IMLRegisterAllocatorParameters* raParam;
};

uint32 recRACurrentIterationIndex = 0;

uint32 PPCRecRA_getNextIterationIndex()
{
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
			currentSegment->raInfo.isPartOfProcessedLoop = _detectLoop(currentSegment->nextSegmentBranchNotTaken, depth + 1, iterationIndex, imlSegmentLoopBase);
		}
	}
	if (currentSegment->nextSegmentBranchTaken)
	{
		if (currentSegment->nextSegmentBranchTaken->momentaryIndex > currentSegment->momentaryIndex)
		{
			currentSegment->raInfo.isPartOfProcessedLoop = _detectLoop(currentSegment->nextSegmentBranchTaken, depth + 1, iterationIndex, imlSegmentLoopBase);
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

//typedef struct
//{
//	sint32 name;
//	sint32 virtualRegister;
//	sint32 physicalRegister;
//	bool isDirty;
//}raRegisterState_t;

//const sint32 _raInfo_physicalGPRCount = PPC_X64_GPR_USABLE_REGISTERS;
//
//raRegisterState_t* PPCRecRA_getRegisterState(raRegisterState_t* regState, sint32 virtualRegister)
//{
//	for (sint32 i = 0; i < _raInfo_physicalGPRCount; i++)
//	{
//		if (regState[i].virtualRegister == virtualRegister)
//		{
//#ifdef CEMU_DEBUG_ASSERT
//			if (regState[i].physicalRegister < 0)
//				assert_dbg();
//#endif
//			return regState + i;
//		}
//	}
//	return nullptr;
//}
//
//raRegisterState_t* PPCRecRA_getFreePhysicalRegister(raRegisterState_t* regState)
//{
//	for (sint32 i = 0; i < _raInfo_physicalGPRCount; i++)
//	{
//		if (regState[i].physicalRegister < 0)
//		{
//			regState[i].physicalRegister = i;
//			return regState + i;
//		}
//	}
//	return nullptr;
//}

typedef struct
{
	uint16 registerIndex;
	uint16 registerName;
}raLoadStoreInfo_t;

void PPCRecRA_insertGPRLoadInstruction(IMLSegment* imlSegment, sint32 insertIndex, sint32 registerIndex, sint32 registerName)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, insertIndex, 1);
	IMLInstruction* imlInstructionItr = imlSegment->imlList.data() + (insertIndex + 0);
	memset(imlInstructionItr, 0x00, sizeof(IMLInstruction));
	imlInstructionItr->type = PPCREC_IML_TYPE_R_NAME;
	imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
	imlInstructionItr->op_r_name.regR = registerIndex;
	imlInstructionItr->op_r_name.name = registerName;
}

void PPCRecRA_insertGPRLoadInstructions(IMLSegment* imlSegment, sint32 insertIndex, raLoadStoreInfo_t* loadList, sint32 loadCount)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, insertIndex, loadCount);
	memset(imlSegment->imlList.data() + (insertIndex + 0), 0x00, sizeof(IMLInstruction)*loadCount);
	for (sint32 i = 0; i < loadCount; i++)
	{
		IMLInstruction* imlInstructionItr = imlSegment->imlList.data() + (insertIndex + i);
		imlInstructionItr->type = PPCREC_IML_TYPE_R_NAME;
		imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
		imlInstructionItr->op_r_name.regR = (uint8)loadList[i].registerIndex;
		imlInstructionItr->op_r_name.name = (uint32)loadList[i].registerName;
	}
}

void PPCRecRA_insertGPRStoreInstruction(IMLSegment* imlSegment, sint32 insertIndex, sint32 registerIndex, sint32 registerName)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, insertIndex, 1);
	IMLInstruction* imlInstructionItr = imlSegment->imlList.data() + (insertIndex + 0);
	memset(imlInstructionItr, 0x00, sizeof(IMLInstruction));
	imlInstructionItr->type = PPCREC_IML_TYPE_NAME_R;
	imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
	imlInstructionItr->op_r_name.regR = registerIndex;
	imlInstructionItr->op_r_name.name = registerName;
}

void PPCRecRA_insertGPRStoreInstructions(IMLSegment* imlSegment, sint32 insertIndex, raLoadStoreInfo_t* storeList, sint32 storeCount)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, insertIndex, storeCount);
	memset(imlSegment->imlList.data() + (insertIndex + 0), 0x00, sizeof(IMLInstruction)*storeCount);
	for (sint32 i = 0; i < storeCount; i++)
	{
		IMLInstruction* imlInstructionItr = imlSegment->imlList.data() + (insertIndex + i);
		memset(imlInstructionItr, 0x00, sizeof(IMLInstruction));
		imlInstructionItr->type = PPCREC_IML_TYPE_NAME_R;
		imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
		imlInstructionItr->op_r_name.regR = (uint8)storeList[i].registerIndex;
		imlInstructionItr->op_r_name.name = (uint32)storeList[i].registerName;
	}
}

#define SUBRANGE_LIST_SIZE	(128)

sint32 PPCRecRA_countInstructionsUntilNextUse(raLivenessSubrange_t* subrange, sint32 startIndex)
{
	for (sint32 i = 0; i < subrange->list_locations.size(); i++)
	{
		if (subrange->list_locations.data()[i].index >= startIndex)
			return subrange->list_locations.data()[i].index - startIndex;
	}
	return INT_MAX;
}

// count how many instructions there are until physRegister is used by any subrange (returns 0 if register is in use at startIndex, and INT_MAX if not used for the remainder of the segment)
sint32 PPCRecRA_countInstructionsUntilNextLocalPhysRegisterUse(IMLSegment* imlSegment, sint32 startIndex, sint32 physRegister)
{
	sint32 minDistance = INT_MAX;
	// next
	raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while(subrangeItr)
	{
		if (subrangeItr->range->physicalRegister != physRegister)
		{
			subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
			continue;
		}
		if (startIndex >= subrangeItr->start.index && startIndex < subrangeItr->end.index)
			return 0;
		if (subrangeItr->start.index >= startIndex)
		{
			minDistance = std::min(minDistance, (subrangeItr->start.index - startIndex));
		}
		subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
	}
	return minDistance;
}

struct IMLRALivenessTimeline
{
//	IMLRALivenessTimeline(raLivenessSubrange_t* subrangeChain)
//	{
//#ifdef CEMU_DEBUG_ASSERT
//		raLivenessSubrange_t* it = subrangeChain;
//		raLivenessSubrange_t* prevIt = it;
//		while (it)
//		{
//			cemu_assert_debug(prevIt->start.index <= it->start.index);
//			prevIt = it;
//			it = it->link_segmentSubrangesGPR.next;
//		}
//#endif
//	}

	IMLRALivenessTimeline()
	{
	}

	// manually add an active range
	void AddActiveRange(raLivenessSubrange_t* subrange)
	{
		activeRanges.emplace_back(subrange);
	}

	// remove all ranges from activeRanges with end <= instructionIndex
	void ExpireRanges(sint32 instructionIndex)
	{
		expiredRanges.clear();
		size_t count = activeRanges.size();
		for (size_t f = 0; f < count; f++)
		{
			raLivenessSubrange_t* liverange = activeRanges[f];
			if (liverange->end.index <= instructionIndex)
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

	std::span<raLivenessSubrange_t*> GetExpiredRanges()
	{
		return { expiredRanges.data(), expiredRanges.size() };
	}

	boost::container::small_vector<raLivenessSubrange_t*, 64> activeRanges;

private:
	boost::container::small_vector<raLivenessSubrange_t*, 16> expiredRanges;
};

bool IsRangeOverlapping(raLivenessSubrange_t* rangeA, raLivenessSubrange_t* rangeB)
{
	if (rangeA->start.index < rangeB->end.index && rangeA->end.index > rangeB->start.index)
		return true;
	if ((rangeA->start.index == RA_INTER_RANGE_START && rangeA->start.index == rangeB->start.index))
		return true;
	if (rangeA->end.index == RA_INTER_RANGE_END && rangeA->end.index == rangeB->end.index)
		return true;
	return false;
}

// mark occupied registers by any overlapping range as unavailable in physRegSet
void PPCRecRA_MaskOverlappingPhysRegForGlobalRange(raLivenessRange_t* range, IMLPhysRegisterSet& physRegSet)
{
	for (auto& subrange : range->list_subranges)
	{
		IMLSegment* imlSegment = subrange->imlSegment;
		raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while(subrangeItr)
		{
			if (subrange == subrangeItr)
			{
				// next
				subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
				continue;
			}
			if(IsRangeOverlapping(subrange, subrangeItr))
			{
				if (subrangeItr->range->physicalRegister >= 0)
					physRegSet.SetReserved(subrangeItr->range->physicalRegister);
			}
			// next
			subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
		}
	}
}

bool _livenessRangeStartCompare(raLivenessSubrange_t* lhs, raLivenessSubrange_t* rhs) { return lhs->start.index < rhs->start.index; }

void _sortSegmentAllSubrangesLinkedList(IMLSegment* imlSegment)
{
	raLivenessSubrange_t* subrangeList[4096+1];
	sint32 count = 0;
	// disassemble linked list
	raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while (subrangeItr)
	{
		if (count >= 4096)
			assert_dbg();
		subrangeList[count] = subrangeItr;
		count++;
		// next
		subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
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
	subrangeList[0]->link_segmentSubrangesGPR.prev = nullptr;
	subrangeList[0]->link_segmentSubrangesGPR.next = subrangeList[1];
	for (sint32 i = 1; i < count; i++)
	{
		subrangeList[i]->link_segmentSubrangesGPR.prev = subrangeList[i - 1];
		subrangeList[i]->link_segmentSubrangesGPR.next = subrangeList[i + 1];
	}
	// validate list
#ifdef CEMU_DEBUG_ASSERT
	sint32 count2 = 0;
	subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	sint32 currentStartIndex = RA_INTER_RANGE_START;
	while (subrangeItr)
	{
		count2++;
		if (subrangeItr->start.index < currentStartIndex)
			assert_dbg();
		currentStartIndex = subrangeItr->start.index;
		// next
		subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
	}
	if (count != count2)
		assert_dbg();
#endif
}

raLivenessSubrange_t* _GetSubrangeByInstructionIndexAndVirtualReg(IMLSegment* imlSegment, IMLReg regToSearch, sint32 instructionIndex)
{
	uint32 regId = regToSearch & 0xFF;
	raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_perVirtualGPR[regId];
	while (subrangeItr)
	{
		if (subrangeItr->start.index <= instructionIndex && subrangeItr->end.index > instructionIndex)
			return subrangeItr;
		subrangeItr = subrangeItr->link_sameVirtualRegisterGPR.next;
	}
	return nullptr;
}

void IMLRA_IsolateRangeOnInstruction(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, raLivenessSubrange_t* subrange, sint32 instructionIndex)
{
	DEBUG_BREAK;
}

void IMLRA_HandleFixedRegisters(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
{
	// this works as a pre-pass to actual register allocation. Assigning registers in advance based on fixed requirements (e.g. calling conventions and operations with fixed-reg input/output like x86 DIV/MUL)
	// algorithm goes as follows:
	// 1) Iterate all instructions from beginning to end and keep a list of covering ranges
	// 2) If we encounter an instruction with a fixed register we:
	//   2.0) Check if there are any other ranges already using the same fixed-register and if yes, we split them and unassign the register for any follow-up instructions just prior to the current instruction
	//   2.1) For inputs: Split the range that needs to be assigned a phys reg on the current instruction. Basically creating a 1-instruction long subrange that we can assign the physical register. RA will then schedule register allocation around that and avoid moves
	//	 2.2) For outputs: Split the range that needs to be assigned a phys reg on the current instruction
	//		  Q: What if a specific fixed-register is used both for input and output and thus is destructive? A: Create temporary range
	//		  Q: What if we have 3 different inputs that are all the same virtual register? A: Create temporary range
	//		  Q: Assuming the above is implemented, do we even support overlapping two ranges of separate virtual regs on the same phys register? In theory the RA shouldn't care

	// experimental code
	//for (size_t i = 0; i < imlSegment->imlList.size(); i++)
	//{
	//	IMLInstruction& inst = imlSegment->imlList[i];
	//	if (inst.type == PPCREC_IML_TYPE_R_R_R)
	//	{
	//		if (inst.operation == PPCREC_IML_OP_LEFT_SHIFT)
	//		{
	//			// get the virtual reg which needs to be assigned a fixed register
	//			//IMLUsedRegisters usedReg;
	//			//inst.CheckRegisterUsage(&usedReg);
	//			IMLReg rB = inst.op_r_r_r.regB;
	//			// rB needs to use RCX/ECX
	//			raLivenessSubrange_t* subrange = _GetSubrangeByInstructionIndexAndVirtualReg(imlSegment, rB, i);
	//			cemu_assert_debug(subrange->range->physicalRegister < 0); // already has a phys reg assigned
	//			// make sure RCX/ECX is free
	//			// split before (if needed) and after instruction so that we get a new 1-instruction long range for which we can assign the physical register
	//			raLivenessSubrange_t* instructionRange = subrange->start.index < i ? PPCRecRA_splitLocalSubrange(ppcImlGenContext, subrange, i, false) : subrange;
	//			raLivenessSubrange_t* tailRange = PPCRecRA_splitLocalSubrange(ppcImlGenContext, instructionRange, i+1, false);

	//		}
	//	}
	//}
}

bool IMLRA_AssignSegmentRegisters(IMLRegisterAllocatorContext& ctx, ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
{
	// sort subranges ascending by start index
	_sortSegmentAllSubrangesLinkedList(imlSegment);

	IMLRALivenessTimeline livenessTimeline;
	raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while(subrangeItr)
	{
		sint32 currentIndex = subrangeItr->start.index;
		PPCRecRA_debugValidateSubrange(subrangeItr);
		livenessTimeline.ExpireRanges(std::min<sint32>(currentIndex, RA_INTER_RANGE_END-1)); // expire up to currentIndex (inclusive), but exclude infinite ranges
		// if subrange already has register assigned then add it to the active list and continue
		if (subrangeItr->range->physicalRegister >= 0)
		{
			// verify if register is actually available
#ifdef CEMU_DEBUG_ASSERT
			for (auto& liverangeItr : livenessTimeline.activeRanges)
			{
				// check for register mismatch
				cemu_assert_debug(liverangeItr->range->physicalRegister != subrangeItr->range->physicalRegister);
			}
#endif
			livenessTimeline.AddActiveRange(subrangeItr);
			subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
			continue;
		}
		// find free register for current subrangeItr and segment
		IMLPhysRegisterSet physRegSet = ctx.raParam->physicalRegisterPool;
		for (auto& liverangeItr : livenessTimeline.activeRanges)
		{
			cemu_assert_debug(liverangeItr->range->physicalRegister >= 0);
			physRegSet.SetReserved(liverangeItr->range->physicalRegister);
		}
		// check intersections with other ranges and determine allowed registers
		IMLPhysRegisterSet localAvailableRegsMask = physRegSet; // mask of registers that are currently not used (does not include range checks in other segments)
		if(physRegSet.HasAnyAvailable())
		{
			// check globally in all segments
			PPCRecRA_MaskOverlappingPhysRegForGlobalRange(subrangeItr->range, physRegSet);
		}
		if (!physRegSet.HasAnyAvailable())
		{
			struct
			{
				// estimated costs and chosen candidates for the different spill strategies
				// hole cutting into a local range
				struct
				{
					sint32 distance;
					raLivenessSubrange_t* largestHoleSubrange;
					sint32 cost; // additional cost of choosing this candidate
				}localRangeHoleCutting;
				// split current range (this is generally only a good choice when the current range is long but rarely used)
				struct
				{
					sint32 cost;
					sint32 physRegister;
					sint32 distance; // size of hole
				}availableRegisterHole;
				// explode a inter-segment range (prefer ranges that are not read/written in this segment)
				struct
				{
					raLivenessRange_t* range;
					sint32 cost;
					sint32 distance; // size of hole
					// note: If we explode a range, we still have to check the size of the hole that becomes available, if too small then we need to add cost of splitting local subrange
				}explodeRange;
				// todo - add more strategies, make cost estimation smarter (for example, in some cases splitting can have reduced or no cost if read/store can be avoided due to data flow)
			}spillStrategies;
			// cant assign register
			// there might be registers available, we just can't use them due to range conflicts
			if (subrangeItr->end.index != RA_INTER_RANGE_END)
			{
				// range ends in current segment

				// Current algo looks like this:
				// 1) Get the size of the largest possible hole that we can cut into any of the live local subranges
				// 1.1) Check if the hole is large enough to hold the current subrange
				// 2) If yes, cut hole and return false (full retry)
				// 3) If no, try to reuse free register (need to determine how large the region is we can use)
				// 4) If there is no free register or the range is extremely short go back to step 1+2 but additionally split the current subrange at where the hole ends

				cemu_assert_debug(currentIndex == subrangeItr->start.index);

				sint32 requiredSize = subrangeItr->end.index - subrangeItr->start.index;
				// evaluate strategy: Cut hole into local subrange
				spillStrategies.localRangeHoleCutting.distance = -1;
				spillStrategies.localRangeHoleCutting.largestHoleSubrange = nullptr;
				spillStrategies.localRangeHoleCutting.cost = INT_MAX;
				if (currentIndex >= 0)
				{
					for (auto candidate : livenessTimeline.activeRanges)
					{
						if (candidate->end.index == RA_INTER_RANGE_END)
							continue;
						sint32 distance = PPCRecRA_countInstructionsUntilNextUse(candidate, currentIndex);
						if (distance < 2)
							continue; // not even worth the consideration
									  // calculate split cost of candidate
						sint32 cost = PPCRecRARange_estimateAdditionalCostAfterSplit(candidate, currentIndex + distance);
						// calculate additional split cost of currentRange if hole is not large enough
						if (distance < requiredSize)
						{
							cost += PPCRecRARange_estimateAdditionalCostAfterSplit(subrangeItr, currentIndex + distance);
							// we also slightly increase cost in relation to the remaining length (in order to make the algorithm prefer larger holes)
							cost += (requiredSize - distance) / 10;
						}
						// compare cost with previous candidates
						if (cost < spillStrategies.localRangeHoleCutting.cost)
						{
							spillStrategies.localRangeHoleCutting.cost = cost;
							spillStrategies.localRangeHoleCutting.distance = distance;
							spillStrategies.localRangeHoleCutting.largestHoleSubrange = candidate;
						}
					}
				}
				// evaluate strategy: Split current range to fit in available holes
				// todo - are checks required to avoid splitting on the suffix instruction?
				spillStrategies.availableRegisterHole.cost = INT_MAX;
				spillStrategies.availableRegisterHole.distance = -1;
				spillStrategies.availableRegisterHole.physRegister = -1;
				if (currentIndex >= 0)
				{
					if (localAvailableRegsMask.HasAnyAvailable())
					{
						sint32 physRegItr = -1;
						while (true)
						{
							physRegItr = localAvailableRegsMask.GetNextAvailableReg(physRegItr + 1);
							if (physRegItr < 0)
								break;
							// get size of potential hole for this register
							sint32 distance = PPCRecRA_countInstructionsUntilNextLocalPhysRegisterUse(imlSegment, currentIndex, physRegItr);
							if (distance < 2)
								continue; // not worth consideration
							// calculate additional cost due to split
							if (distance >= requiredSize)
								assert_dbg(); // should not happen or else we would have selected this register
							sint32 cost = PPCRecRARange_estimateAdditionalCostAfterSplit(subrangeItr, currentIndex + distance);
							// add small additional cost for the remaining range (prefer larger holes)
							cost += (requiredSize - distance) / 10;
							if (cost < spillStrategies.availableRegisterHole.cost)
							{
								spillStrategies.availableRegisterHole.cost = cost;
								spillStrategies.availableRegisterHole.distance = distance;
								spillStrategies.availableRegisterHole.physRegister = physRegItr;
							}
						}
					}
				}
				// evaluate strategy: Explode inter-segment ranges
				spillStrategies.explodeRange.cost = INT_MAX;
				spillStrategies.explodeRange.range = nullptr;
				spillStrategies.explodeRange.distance = -1;
				for (auto candidate : livenessTimeline.activeRanges)
				{
					if (candidate->end.index != RA_INTER_RANGE_END)
						continue;
					sint32 distance = PPCRecRA_countInstructionsUntilNextUse(candidate, currentIndex);
					if( distance < 2)
						continue;
					sint32 cost;
					cost = PPCRecRARange_estimateAdditionalCostAfterRangeExplode(candidate->range);
					// if the hole is not large enough, add cost of splitting current subrange
					if (distance < requiredSize)
					{
						cost += PPCRecRARange_estimateAdditionalCostAfterSplit(subrangeItr, currentIndex + distance);
						// add small additional cost for the remaining range (prefer larger holes)
						cost += (requiredSize - distance) / 10;
					}
					// compare with current best candidate for this strategy
					if (cost < spillStrategies.explodeRange.cost)
					{
						spillStrategies.explodeRange.cost = cost;
						spillStrategies.explodeRange.distance = distance;
						spillStrategies.explodeRange.range = candidate->range;
					}
				}
				// choose strategy
				if (spillStrategies.explodeRange.cost != INT_MAX && spillStrategies.explodeRange.cost <= spillStrategies.localRangeHoleCutting.cost && spillStrategies.explodeRange.cost <= spillStrategies.availableRegisterHole.cost)
				{
					// explode range
					PPCRecRA_explodeRange(ppcImlGenContext, spillStrategies.explodeRange.range);
					// split current subrange if necessary
					if( requiredSize > spillStrategies.explodeRange.distance)
						PPCRecRA_splitLocalSubrange(ppcImlGenContext, subrangeItr, currentIndex+spillStrategies.explodeRange.distance, true);
				}
				else if (spillStrategies.availableRegisterHole.cost != INT_MAX && spillStrategies.availableRegisterHole.cost <= spillStrategies.explodeRange.cost && spillStrategies.availableRegisterHole.cost <= spillStrategies.localRangeHoleCutting.cost)
				{
					// use available register
					PPCRecRA_splitLocalSubrange(ppcImlGenContext, subrangeItr, currentIndex + spillStrategies.availableRegisterHole.distance, true);
				}
				else if (spillStrategies.localRangeHoleCutting.cost != INT_MAX && spillStrategies.localRangeHoleCutting.cost <= spillStrategies.explodeRange.cost && spillStrategies.localRangeHoleCutting.cost <= spillStrategies.availableRegisterHole.cost)
				{
					// cut hole
					PPCRecRA_splitLocalSubrange(ppcImlGenContext, spillStrategies.localRangeHoleCutting.largestHoleSubrange, currentIndex + spillStrategies.localRangeHoleCutting.distance, true);
					// split current subrange if necessary
					if (requiredSize > spillStrategies.localRangeHoleCutting.distance)
						PPCRecRA_splitLocalSubrange(ppcImlGenContext, subrangeItr, currentIndex + spillStrategies.localRangeHoleCutting.distance, true);
				}
				else if (subrangeItr->start.index == RA_INTER_RANGE_START)
				{
					// alternative strategy if we have no other choice: explode current range
					PPCRecRA_explodeRange(ppcImlGenContext, subrangeItr->range);
				}
				else
					assert_dbg();

				return false;
			}
			else
			{
				// range exceeds segment border
				// simple but bad solution -> explode the entire range (no longer allow it to cross segment boundaries)
				// better solutions: 1) Depending on the situation, we can explode other ranges to resolve the conflict. Thus we should explode the range with the lowest extra cost
				//					 2) Or we explode the range only partially
				// explode the range with the least cost
				spillStrategies.explodeRange.cost = INT_MAX;
				spillStrategies.explodeRange.range = nullptr;
				spillStrategies.explodeRange.distance = -1;
				for(auto candidate : livenessTimeline.activeRanges)
				{
					if (candidate->end.index != RA_INTER_RANGE_END)
						continue;
					// only select candidates that clash with current subrange
					if (candidate->range->physicalRegister < 0 && candidate != subrangeItr)
						continue;
					
					sint32 cost;
					cost = PPCRecRARange_estimateAdditionalCostAfterRangeExplode(candidate->range);
					// compare with current best candidate for this strategy
					if (cost < spillStrategies.explodeRange.cost)
					{
						spillStrategies.explodeRange.cost = cost;
						spillStrategies.explodeRange.distance = INT_MAX;
						spillStrategies.explodeRange.range = candidate->range;
					}
				}
				// add current range as a candidate too
				sint32 ownCost;
				ownCost = PPCRecRARange_estimateAdditionalCostAfterRangeExplode(subrangeItr->range);
				if (ownCost < spillStrategies.explodeRange.cost)
				{
					spillStrategies.explodeRange.cost = ownCost;
					spillStrategies.explodeRange.distance = INT_MAX;
					spillStrategies.explodeRange.range = subrangeItr->range;
				}
				if (spillStrategies.explodeRange.cost == INT_MAX)
					assert_dbg(); // should not happen
				PPCRecRA_explodeRange(ppcImlGenContext, spillStrategies.explodeRange.range);
			}
			return false;
		}
		// assign register to range
		subrangeItr->range->physicalRegister = physRegSet.GetFirstAvailableReg();
		livenessTimeline.AddActiveRange(subrangeItr);
		// next
		subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;	
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

struct subrangeEndingInfo_t
{
	raLivenessSubrange_t* subrangeList[SUBRANGE_LIST_SIZE];
	sint32 subrangeCount;
	bool hasUndefinedEndings;
};

void _findSubrangeWriteEndings(raLivenessSubrange_t* subrange, uint32 iterationIndex, sint32 depth, subrangeEndingInfo_t* info)
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
	if (subrange->end.index != RA_INTER_RANGE_END)
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

void _analyzeRangeDataFlow(raLivenessSubrange_t* subrange)
{
	if (subrange->end.index != RA_INTER_RANGE_END)
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
				raLivenessSubrange_t* subrangeItr = writeEndingInfo.subrangeList[i];
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
					raLivenessSubrange_t* subrangeItr = writeEndingInfo.subrangeList[i];
					subrangeItr->hasStore = true;
				}
			}
		}
	}
}

void IMLRA_GenerateSegmentInstructions(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
{
	sint16 virtualReg2PhysReg[IML_RA_VIRT_REG_COUNT_MAX];
	for (sint32 i = 0; i < IML_RA_VIRT_REG_COUNT_MAX; i++)
		virtualReg2PhysReg[i] = -1;
	std::unordered_map<IMLReg, IMLReg> virt2PhysRegMap; // key = virtual register, value = physical register
	IMLRALivenessTimeline livenessTimeline;
	sint32 index = 0;
	sint32 suffixInstructionCount = imlSegment->HasSuffixInstruction() ? 1 : 0;
	// load register ranges that are supplied from previous segments
	raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while(subrangeItr)
	{
		if (subrangeItr->start.index == RA_INTER_RANGE_START)
		{
			livenessTimeline.AddActiveRange(subrangeItr);
#ifdef CEMU_DEBUG_ASSERT
			// load GPR
			if (subrangeItr->_noLoad == false)
			{
				assert_dbg();
			}
			// update translation table
			if (virtualReg2PhysReg[subrangeItr->range->virtualRegister] != -1)
				assert_dbg();
#endif
			virtualReg2PhysReg[subrangeItr->range->virtualRegister] = subrangeItr->range->physicalRegister;
			virt2PhysRegMap.insert_or_assign(subrangeItr->range->virtualRegister, subrangeItr->range->physicalRegister);
		}
		// next
		subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
	}
	// process instructions
	while(index < imlSegment->imlList.size() + 1)
	{
		// expire ranges
		livenessTimeline.ExpireRanges(index);
		for (auto& expiredRange : livenessTimeline.GetExpiredRanges())
		{
			// update translation table
			if (virtualReg2PhysReg[expiredRange->range->virtualRegister] == -1)
				assert_dbg();
			virtualReg2PhysReg[expiredRange->range->virtualRegister] = -1;
			virt2PhysRegMap.erase(expiredRange->range->virtualRegister);
			// store GPR if required
			// special care has to be taken to execute any stores before the suffix instruction since trailing instructions may not get executed
			if (expiredRange->hasStore)
			{
				PPCRecRA_insertGPRStoreInstruction(imlSegment, std::min<sint32>(index, imlSegment->imlList.size() - suffixInstructionCount), expiredRange->range->physicalRegister, expiredRange->range->name);
				index++;
			}
		}

		// load new ranges
		subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while(subrangeItr)
		{
			if (subrangeItr->start.index == index)
			{
				livenessTimeline.AddActiveRange(subrangeItr);
				// load GPR
				// similar to stores, any loads for the next segment need to happen before the suffix instruction
				// however, ranges that exit the segment at the end but do not cover the suffix instruction are illegal (e.g. RA_INTER_RANGE_END to RA_INTER_RANGE_END subrange)
				// this is to prevent the RA from inserting store/load instructions after the suffix instruction
				if (imlSegment->HasSuffixInstruction())
				{
					cemu_assert_debug(subrangeItr->start.index <= imlSegment->GetSuffixInstructionIndex());
				}
				if (subrangeItr->_noLoad == false)
				{
					PPCRecRA_insertGPRLoadInstruction(imlSegment, std::min<sint32>(index, imlSegment->imlList.size() - suffixInstructionCount), subrangeItr->range->physicalRegister, subrangeItr->range->name);
					index++;
					subrangeItr->start.index--;
				}
				// update translation table
				cemu_assert_debug(virtualReg2PhysReg[subrangeItr->range->virtualRegister] == -1);
				virtualReg2PhysReg[subrangeItr->range->virtualRegister] = subrangeItr->range->physicalRegister;
				virt2PhysRegMap.insert_or_assign(subrangeItr->range->virtualRegister, subrangeItr->range->physicalRegister);
			}
			subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
		}
		// rewrite registers
		if (index < imlSegment->imlList.size())
			imlSegment->imlList[index].RewriteGPR(virt2PhysRegMap);
		// next iml instruction
		index++;
	}
	// expire infinite subranges (subranges which cross the segment border)
	sint32 storeLoadListLength = 0;
	raLoadStoreInfo_t loadStoreList[IML_RA_VIRT_REG_COUNT_MAX];
	livenessTimeline.ExpireRanges(RA_INTER_RANGE_END);
	for (auto liverange : livenessTimeline.GetExpiredRanges())
	{
		// update translation table
		cemu_assert_debug(virtualReg2PhysReg[liverange->range->virtualRegister] != -1);
		virtualReg2PhysReg[liverange->range->virtualRegister] = -1;
		virt2PhysRegMap.erase(liverange->range->virtualRegister);
		// store GPR
		if (liverange->hasStore)
		{
			loadStoreList[storeLoadListLength].registerIndex = liverange->range->physicalRegister;
			loadStoreList[storeLoadListLength].registerName = liverange->range->name;
			storeLoadListLength++;
		}
	}
	cemu_assert_debug(livenessTimeline.activeRanges.empty());
	if (storeLoadListLength > 0)
	{
		PPCRecRA_insertGPRStoreInstructions(imlSegment, imlSegment->imlList.size() - suffixInstructionCount, loadStoreList, storeLoadListLength);
	}
	// load subranges for next segments
	subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	storeLoadListLength = 0;
	while(subrangeItr)
	{
		if (subrangeItr->start.index == RA_INTER_RANGE_END)
		{
			livenessTimeline.AddActiveRange(subrangeItr);
			// load GPR
			if (subrangeItr->_noLoad == false)
			{
				loadStoreList[storeLoadListLength].registerIndex = subrangeItr->range->physicalRegister;
				loadStoreList[storeLoadListLength].registerName = subrangeItr->range->name;
				storeLoadListLength++;
			}
			// update translation table
			cemu_assert_debug(virtualReg2PhysReg[subrangeItr->range->virtualRegister] == -1);
			virtualReg2PhysReg[subrangeItr->range->virtualRegister] = subrangeItr->range->physicalRegister;
			virt2PhysRegMap.insert_or_assign(subrangeItr->range->virtualRegister, subrangeItr->range->physicalRegister);
		}
		// next
		subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
	}
	if (storeLoadListLength > 0)
	{
		PPCRecRA_insertGPRLoadInstructions(imlSegment, imlSegment->imlList.size() - suffixInstructionCount, loadStoreList, storeLoadListLength);
	}
}

void IMLRA_GenerateMoveInstructions(ppcImlGenContext_t* ppcImlGenContext)
{
	for (size_t s = 0; s < ppcImlGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ppcImlGenContext->segmentList2[s];
		IMLRA_GenerateSegmentInstructions(ppcImlGenContext, imlSegment);
	}
}

void IMLRA_CalculateLivenessRanges(ppcImlGenContext_t* ppcImlGenContext);
void IMLRA_ProcessFlowAndCalculateLivenessRanges(ppcImlGenContext_t* ppcImlGenContext);
void IMLRA_AnalyzeRangeDataFlow(ppcImlGenContext_t* ppcImlGenContext);

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

void IMLRegisterAllocator_AllocateRegisters(ppcImlGenContext_t* ppcImlGenContext, IMLRegisterAllocatorParameters& raParam)
{
	IMLRegisterAllocatorContext ctx;
	ctx.raParam = &raParam;

	IMLRA_ReshapeForRegisterAllocation(ppcImlGenContext);

	ppcImlGenContext->raInfo.list_ranges = std::vector<raLivenessRange_t*>();
	
	IMLRA_CalculateLivenessRanges(ppcImlGenContext);
	IMLRA_ProcessFlowAndCalculateLivenessRanges(ppcImlGenContext);
	IMLRA_AssignRegisters(ctx, ppcImlGenContext);

	IMLRA_AnalyzeRangeDataFlow(ppcImlGenContext);
	IMLRA_GenerateMoveInstructions(ppcImlGenContext);

	PPCRecRA_deleteAllRanges(ppcImlGenContext);
}


bool _isRangeDefined(IMLSegment* imlSegment, sint32 vGPR)
{
	return (imlSegment->raDistances.reg[vGPR].usageStart != INT_MAX);
}

void PPCRecRA_calculateSegmentMinMaxRanges(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
{
	for (sint32 i = 0; i < IML_RA_VIRT_REG_COUNT_MAX; i++)
	{
		imlSegment->raDistances.reg[i].usageStart = INT_MAX;
		imlSegment->raDistances.reg[i].usageEnd = INT_MIN;
	}
	// scan instructions for usage range
	size_t index = 0;
	IMLUsedRegisters gprTracking;
	while (index < imlSegment->imlList.size())
	{
		imlSegment->imlList[index].CheckRegisterUsage(&gprTracking);
		gprTracking.ForEachAccessedGPR([&](IMLReg gprId, bool isWritten) {
			cemu_assert_debug(gprId < IML_RA_VIRT_REG_COUNT_MAX);
			imlSegment->raDistances.reg[gprId].usageStart = std::min<sint32>(imlSegment->raDistances.reg[gprId].usageStart, index); // index before/at instruction
			imlSegment->raDistances.reg[gprId].usageEnd = std::max<sint32>(imlSegment->raDistances.reg[gprId].usageEnd, index + 1); // index after instruction
			});
		index++;
	}
}

void IMLRA_CalculateLivenessRanges(ppcImlGenContext_t* ppcImlGenContext)
{
	// for each register calculate min/max index of usage range within each segment
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		PPCRecRA_calculateSegmentMinMaxRanges(ppcImlGenContext, segIt);
	}
}

raLivenessSubrange_t* PPCRecRA_convertToMappedRanges(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 vGPR, raLivenessRange_t* range)
{
	if (imlSegment->raDistances.isProcessed[vGPR])
	{
		// return already existing segment
		return imlSegment->raInfo.linkedList_perVirtualGPR[vGPR];
	}
	imlSegment->raDistances.isProcessed[vGPR] = true;
	if (_isRangeDefined(imlSegment, vGPR) == false)
		return nullptr;
	// create subrange
	cemu_assert_debug(imlSegment->raInfo.linkedList_perVirtualGPR[vGPR] == nullptr);
	raLivenessSubrange_t* subrange = PPCRecRA_createSubrange(ppcImlGenContext, range, imlSegment, imlSegment->raDistances.reg[vGPR].usageStart, imlSegment->raDistances.reg[vGPR].usageEnd);
	// traverse forward
	if (imlSegment->raDistances.reg[vGPR].usageEnd == RA_INTER_RANGE_END)
	{
		if (imlSegment->nextSegmentBranchTaken && imlSegment->nextSegmentBranchTaken->raDistances.reg[vGPR].usageStart == RA_INTER_RANGE_START)
		{
			subrange->subrangeBranchTaken = PPCRecRA_convertToMappedRanges(ppcImlGenContext, imlSegment->nextSegmentBranchTaken, vGPR, range);
			cemu_assert_debug(subrange->subrangeBranchTaken->start.index == RA_INTER_RANGE_START);
		}
		if (imlSegment->nextSegmentBranchNotTaken && imlSegment->nextSegmentBranchNotTaken->raDistances.reg[vGPR].usageStart == RA_INTER_RANGE_START)
		{
			subrange->subrangeBranchNotTaken = PPCRecRA_convertToMappedRanges(ppcImlGenContext, imlSegment->nextSegmentBranchNotTaken, vGPR, range);
			cemu_assert_debug(subrange->subrangeBranchNotTaken->start.index == RA_INTER_RANGE_START);
		}
	}
	// traverse backward
	if (imlSegment->raDistances.reg[vGPR].usageStart == RA_INTER_RANGE_START)
	{
		for (auto& it : imlSegment->list_prevSegments)
		{
			if (it->raDistances.reg[vGPR].usageEnd == RA_INTER_RANGE_END)
				PPCRecRA_convertToMappedRanges(ppcImlGenContext, it, vGPR, range);
		}
	}
	// for subranges which exit the segment at the end there is a hard requirement that they cover the suffix instruction
	// this is due to range load instructions being inserted before the suffix instruction
	if (subrange->end.index == RA_INTER_RANGE_END)
	{
		if (imlSegment->HasSuffixInstruction())
		{
			cemu_assert_debug(subrange->start.index <= imlSegment->GetSuffixInstructionIndex());
		}
	}
	return subrange;
}

void PPCRecRA_createSegmentLivenessRanges(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
{
	for (sint32 i = 0; i < IML_RA_VIRT_REG_COUNT_MAX; i++)
	{
		if (_isRangeDefined(imlSegment, i) == false)
			continue;
		if (imlSegment->raDistances.isProcessed[i])
			continue;
		raLivenessRange_t* range = PPCRecRA_createRangeBase(ppcImlGenContext, i, ppcImlGenContext->mappedRegister[i]);
		PPCRecRA_convertToMappedRanges(ppcImlGenContext, imlSegment, i, range);
	}
	// create lookup table of ranges
	raLivenessSubrange_t* vGPR2Subrange[IML_RA_VIRT_REG_COUNT_MAX];
	for (sint32 i = 0; i < IML_RA_VIRT_REG_COUNT_MAX; i++)
	{
		vGPR2Subrange[i] = imlSegment->raInfo.linkedList_perVirtualGPR[i];
#ifdef CEMU_DEBUG_ASSERT
		if (vGPR2Subrange[i] && vGPR2Subrange[i]->link_sameVirtualRegisterGPR.next != nullptr)
			assert_dbg();
#endif
	}
	// parse instructions and convert to locations
	size_t index = 0;
	IMLUsedRegisters gprTracking;
	while (index < imlSegment->imlList.size())
	{
		imlSegment->imlList[index].CheckRegisterUsage(&gprTracking);
		gprTracking.ForEachAccessedGPR([&](IMLReg gprId, bool isWritten) {
			// add location
			PPCRecRA_updateOrAddSubrangeLocation(vGPR2Subrange[gprId], index, !isWritten, isWritten);
#ifdef CEMU_DEBUG_ASSERT
		if ((sint32)index < vGPR2Subrange[gprId]->start.index)
			assert_dbg();
		if ((sint32)index + 1 > vGPR2Subrange[gprId]->end.index)
			assert_dbg();
#endif
			});
		index++;
	}
}

void PPCRecRA_extendRangeToEndOfSegment(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 vGPR)
{
	if (_isRangeDefined(imlSegment, vGPR) == false)
	{
		if(imlSegment->HasSuffixInstruction())
			imlSegment->raDistances.reg[vGPR].usageStart = imlSegment->GetSuffixInstructionIndex();
		else
			imlSegment->raDistances.reg[vGPR].usageStart = RA_INTER_RANGE_END;
		imlSegment->raDistances.reg[vGPR].usageEnd = RA_INTER_RANGE_END;
		return;
	}
	imlSegment->raDistances.reg[vGPR].usageEnd = RA_INTER_RANGE_END;
}

void PPCRecRA_extendRangeToBeginningOfSegment(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 vGPR)
{
	if (_isRangeDefined(imlSegment, vGPR) == false)
	{
		imlSegment->raDistances.reg[vGPR].usageStart = RA_INTER_RANGE_START;
		imlSegment->raDistances.reg[vGPR].usageEnd = RA_INTER_RANGE_START;
	}
	else
	{
		imlSegment->raDistances.reg[vGPR].usageStart = RA_INTER_RANGE_START;
	}
	// propagate backwards
	for (auto& it : imlSegment->list_prevSegments)
	{
		PPCRecRA_extendRangeToEndOfSegment(ppcImlGenContext, it, vGPR);
	}
}

void _PPCRecRA_connectRanges(ppcImlGenContext_t* ppcImlGenContext, sint32 vGPR, IMLSegment** route, sint32 routeDepth)
{
#ifdef CEMU_DEBUG_ASSERT
	if (routeDepth < 2)
		assert_dbg();
#endif
	// extend starting range to end of segment
	PPCRecRA_extendRangeToEndOfSegment(ppcImlGenContext, route[0], vGPR);
	// extend all the connecting segments in both directions
	for (sint32 i = 1; i < (routeDepth - 1); i++)
	{
		PPCRecRA_extendRangeToEndOfSegment(ppcImlGenContext, route[i], vGPR);
		PPCRecRA_extendRangeToBeginningOfSegment(ppcImlGenContext, route[i], vGPR);
	}
	// extend the final segment towards the beginning
	PPCRecRA_extendRangeToBeginningOfSegment(ppcImlGenContext, route[routeDepth - 1], vGPR);
}

void _PPCRecRA_checkAndTryExtendRange(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* currentSegment, sint32 vGPR, sint32 distanceLeft, IMLSegment** route, sint32 routeDepth)
{
	if (routeDepth >= 64)
	{
		forceLogDebug_printf("Recompiler RA route maximum depth exceeded\n");
		return;
	}
	route[routeDepth] = currentSegment;
	if (currentSegment->raDistances.reg[vGPR].usageStart == INT_MAX)
	{
		// measure distance to end of segment
		distanceLeft -= (sint32)currentSegment->imlList.size();
		if (distanceLeft > 0)
		{
			if (currentSegment->nextSegmentBranchNotTaken)
				_PPCRecRA_checkAndTryExtendRange(ppcImlGenContext, currentSegment->nextSegmentBranchNotTaken, vGPR, distanceLeft, route, routeDepth + 1);
			if (currentSegment->nextSegmentBranchTaken)
				_PPCRecRA_checkAndTryExtendRange(ppcImlGenContext, currentSegment->nextSegmentBranchTaken, vGPR, distanceLeft, route, routeDepth + 1);
		}
		return;
	}
	else
	{
		// measure distance to range
		if (currentSegment->raDistances.reg[vGPR].usageStart == RA_INTER_RANGE_END)
		{
			if (distanceLeft < (sint32)currentSegment->imlList.size())
				return; // range too far away
		}
		else if (currentSegment->raDistances.reg[vGPR].usageStart != RA_INTER_RANGE_START && currentSegment->raDistances.reg[vGPR].usageStart > distanceLeft)
			return; // out of range
		// found close range -> connect ranges
		_PPCRecRA_connectRanges(ppcImlGenContext, vGPR, route, routeDepth + 1);
	}
}

void PPCRecRA_checkAndTryExtendRange(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* currentSegment, sint32 vGPR)
{
#ifdef CEMU_DEBUG_ASSERT
	if (currentSegment->raDistances.reg[vGPR].usageEnd < 0)
		assert_dbg();
#endif
	// count instructions to end of initial segment
	if (currentSegment->raDistances.reg[vGPR].usageEnd == RA_INTER_RANGE_START)
		assert_dbg();
	sint32 instructionsUntilEndOfSeg;
	if (currentSegment->raDistances.reg[vGPR].usageEnd == RA_INTER_RANGE_END)
		instructionsUntilEndOfSeg = 0;
	else
		instructionsUntilEndOfSeg = (sint32)currentSegment->imlList.size() - currentSegment->raDistances.reg[vGPR].usageEnd;

#ifdef CEMU_DEBUG_ASSERT
	if (instructionsUntilEndOfSeg < 0)
		assert_dbg();
#endif
	sint32 remainingScanDist = 45 - instructionsUntilEndOfSeg;
	if (remainingScanDist <= 0)
		return; // can't reach end

	IMLSegment* route[64];
	route[0] = currentSegment;
	if (currentSegment->nextSegmentBranchNotTaken)
	{
		_PPCRecRA_checkAndTryExtendRange(ppcImlGenContext, currentSegment->nextSegmentBranchNotTaken, vGPR, remainingScanDist, route, 1);
	}
	if (currentSegment->nextSegmentBranchTaken)
	{
		_PPCRecRA_checkAndTryExtendRange(ppcImlGenContext, currentSegment->nextSegmentBranchTaken, vGPR, remainingScanDist, route, 1);
	}
}

void PPCRecRA_mergeCloseRangesForSegmentV2(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
{
	for (sint32 i = 0; i < IML_RA_VIRT_REG_COUNT_MAX; i++) // todo: Use dynamic maximum or list of used vGPRs so we can avoid parsing empty entries
	{
		if (imlSegment->raDistances.reg[i].usageStart == INT_MAX)
			continue; // not used
		// check and extend if possible
		PPCRecRA_checkAndTryExtendRange(ppcImlGenContext, imlSegment, i);
	}
#ifdef CEMU_DEBUG_ASSERT
	if (imlSegment->list_prevSegments.empty() == false && imlSegment->isEnterable)
		assert_dbg();
	if ((imlSegment->nextSegmentBranchNotTaken != nullptr || imlSegment->nextSegmentBranchTaken != nullptr) && imlSegment->nextSegmentIsUncertain)
		assert_dbg();
#endif
}

void PPCRecRA_followFlowAndExtendRanges(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
{
	std::vector<IMLSegment*> list_segments;
	list_segments.reserve(1000);
	sint32 index = 0;
	imlSegment->raRangeExtendProcessed = true;
	list_segments.push_back(imlSegment);
	while (index < list_segments.size())
	{
		IMLSegment* currentSegment = list_segments[index];
		PPCRecRA_mergeCloseRangesForSegmentV2(ppcImlGenContext, currentSegment);
		// follow flow
		if (currentSegment->nextSegmentBranchNotTaken && currentSegment->nextSegmentBranchNotTaken->raRangeExtendProcessed == false)
		{
			currentSegment->nextSegmentBranchNotTaken->raRangeExtendProcessed = true;
			list_segments.push_back(currentSegment->nextSegmentBranchNotTaken);
		}
		if (currentSegment->nextSegmentBranchTaken && currentSegment->nextSegmentBranchTaken->raRangeExtendProcessed == false)
		{
			currentSegment->nextSegmentBranchTaken->raRangeExtendProcessed = true;
			list_segments.push_back(currentSegment->nextSegmentBranchTaken);
		}
		index++;
	}
}

void PPCRecRA_mergeCloseRangesV2(ppcImlGenContext_t* ppcImlGenContext)
{
	for (size_t s = 0; s < ppcImlGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ppcImlGenContext->segmentList2[s];
		if (imlSegment->list_prevSegments.empty())
		{
			if (imlSegment->raRangeExtendProcessed)
				assert_dbg(); // should not happen
			PPCRecRA_followFlowAndExtendRanges(ppcImlGenContext, imlSegment);
		}
	}
}

void PPCRecRA_extendRangesOutOfLoopsV2(ppcImlGenContext_t* ppcImlGenContext)
{
	for (size_t s = 0; s < ppcImlGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ppcImlGenContext->segmentList2[s];
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
		for (sint32 i = 0; i < IML_RA_VIRT_REG_COUNT_MAX; i++) // todo: Use dynamic maximum or list of used vGPRs so we can avoid parsing empty entries
		{
			if (imlSegment->raDistances.reg[i].usageEnd != RA_INTER_RANGE_END)
				continue; // range not set or does not reach end of segment
			if (imlSegment->nextSegmentBranchTaken)
				PPCRecRA_extendRangeToBeginningOfSegment(ppcImlGenContext, imlSegment->nextSegmentBranchTaken, i);
			if (imlSegment->nextSegmentBranchNotTaken)
				PPCRecRA_extendRangeToBeginningOfSegment(ppcImlGenContext, imlSegment->nextSegmentBranchNotTaken, i);
		}
	}
}

void IMLRA_ProcessFlowAndCalculateLivenessRanges(ppcImlGenContext_t* ppcImlGenContext)
{
	// merge close ranges
	PPCRecRA_mergeCloseRangesV2(ppcImlGenContext);
	// extra pass to move register stores out of loops
	PPCRecRA_extendRangesOutOfLoopsV2(ppcImlGenContext);
	// calculate liveness ranges
	for (size_t s = 0; s < ppcImlGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ppcImlGenContext->segmentList2[s];
		PPCRecRA_createSegmentLivenessRanges(ppcImlGenContext, imlSegment);
	}
}

void PPCRecRA_analyzeSubrangeDataDependencyV2(raLivenessSubrange_t* subrange)
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

	if (subrange->start.index == RA_INTER_RANGE_START)
		subrange->_noLoad = true;
}

void IMLRA_AnalyzeRangeDataFlow(ppcImlGenContext_t* ppcImlGenContext)
{
	// this function is called after _assignRegisters(), which means that all ranges are already final and wont change anymore
	// first do a per-subrange pass
	for (auto& range : ppcImlGenContext->raInfo.list_ranges)
	{
		for (auto& subrange : range->list_subranges)
		{
			PPCRecRA_analyzeSubrangeDataDependencyV2(subrange);
		}
	}
	// then do a second pass where we scan along subrange flow
	for (auto& range : ppcImlGenContext->raInfo.list_ranges)
	{
		for (auto& subrange : range->list_subranges) // todo - traversing this backwards should be faster and yield better results due to the nature of the algorithm
		{
			_analyzeRangeDataFlow(subrange);
		}
	}
}