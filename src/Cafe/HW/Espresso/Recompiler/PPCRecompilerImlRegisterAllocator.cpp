#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "PPCRecompilerX64.h"
#include "PPCRecompilerImlRanges.h"

void PPCRecompiler_replaceGPRRegisterUsageMultiple(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, sint32 gprRegisterSearched[4], sint32 gprRegisterReplaced[4]);

bool PPCRecompiler_isSuffixInstruction(PPCRecImlInstruction_t* iml);

uint32 recRACurrentIterationIndex = 0;

uint32 PPCRecRA_getNextIterationIndex()
{
	recRACurrentIterationIndex++;
	return recRACurrentIterationIndex;
}

bool _detectLoop(PPCRecImlSegment_t* currentSegment, sint32 depth, uint32 iterationIndex, PPCRecImlSegment_t* imlSegmentLoopBase)
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

void PPCRecRA_detectLoop(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegmentLoopBase)
{
	uint32 iterationIndex = PPCRecRA_getNextIterationIndex();
	imlSegmentLoopBase->raInfo.lastIterationIndex = iterationIndex;
	if (_detectLoop(imlSegmentLoopBase->nextSegmentBranchTaken, 0, iterationIndex, imlSegmentLoopBase))
	{
		imlSegmentLoopBase->loopDepth++;
	}
}

void PPCRecRA_identifyLoop(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment)
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

typedef struct
{
	sint32 name;
	sint32 virtualRegister;
	sint32 physicalRegister;
	bool isDirty;
}raRegisterState_t;

const sint32 _raInfo_physicalGPRCount = PPC_X64_GPR_USABLE_REGISTERS;

raRegisterState_t* PPCRecRA_getRegisterState(raRegisterState_t* regState, sint32 virtualRegister)
{
	for (sint32 i = 0; i < _raInfo_physicalGPRCount; i++)
	{
		if (regState[i].virtualRegister == virtualRegister)
		{
#ifdef CEMU_DEBUG_ASSERT
			if (regState[i].physicalRegister < 0)
				assert_dbg();
#endif
			return regState + i;
		}
	}
	return nullptr;
}

raRegisterState_t* PPCRecRA_getFreePhysicalRegister(raRegisterState_t* regState)
{
	for (sint32 i = 0; i < _raInfo_physicalGPRCount; i++)
	{
		if (regState[i].physicalRegister < 0)
		{
			regState[i].physicalRegister = i;
			return regState + i;
		}
	}
	return nullptr;
}

typedef struct
{
	uint16 registerIndex;
	uint16 registerName;
}raLoadStoreInfo_t;

void PPCRecRA_insertGPRLoadInstruction(PPCRecImlSegment_t* imlSegment, sint32 insertIndex, sint32 registerIndex, sint32 registerName)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, insertIndex, 1);
	PPCRecImlInstruction_t* imlInstructionItr = imlSegment->imlList + (insertIndex + 0);
	memset(imlInstructionItr, 0x00, sizeof(PPCRecImlInstruction_t));
	imlInstructionItr->type = PPCREC_IML_TYPE_R_NAME;
	imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
	imlInstructionItr->op_r_name.registerIndex = registerIndex;
	imlInstructionItr->op_r_name.name = registerName;
	imlInstructionItr->op_r_name.copyWidth = 32;
	imlInstructionItr->op_r_name.flags = 0;
}

void PPCRecRA_insertGPRLoadInstructions(PPCRecImlSegment_t* imlSegment, sint32 insertIndex, raLoadStoreInfo_t* loadList, sint32 loadCount)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, insertIndex, loadCount);
	memset(imlSegment->imlList + (insertIndex + 0), 0x00, sizeof(PPCRecImlInstruction_t)*loadCount);
	for (sint32 i = 0; i < loadCount; i++)
	{
		PPCRecImlInstruction_t* imlInstructionItr = imlSegment->imlList + (insertIndex + i);
		imlInstructionItr->type = PPCREC_IML_TYPE_R_NAME;
		imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
		imlInstructionItr->op_r_name.registerIndex = (uint8)loadList[i].registerIndex;
		imlInstructionItr->op_r_name.name = (uint32)loadList[i].registerName;
		imlInstructionItr->op_r_name.copyWidth = 32;
		imlInstructionItr->op_r_name.flags = 0;
	}
}

void PPCRecRA_insertGPRStoreInstruction(PPCRecImlSegment_t* imlSegment, sint32 insertIndex, sint32 registerIndex, sint32 registerName)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, insertIndex, 1);
	PPCRecImlInstruction_t* imlInstructionItr = imlSegment->imlList + (insertIndex + 0);
	memset(imlInstructionItr, 0x00, sizeof(PPCRecImlInstruction_t));
	imlInstructionItr->type = PPCREC_IML_TYPE_NAME_R;
	imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
	imlInstructionItr->op_r_name.registerIndex = registerIndex;
	imlInstructionItr->op_r_name.name = registerName;
	imlInstructionItr->op_r_name.copyWidth = 32;
	imlInstructionItr->op_r_name.flags = 0;
}

void PPCRecRA_insertGPRStoreInstructions(PPCRecImlSegment_t* imlSegment, sint32 insertIndex, raLoadStoreInfo_t* storeList, sint32 storeCount)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, insertIndex, storeCount);
	memset(imlSegment->imlList + (insertIndex + 0), 0x00, sizeof(PPCRecImlInstruction_t)*storeCount);
	for (sint32 i = 0; i < storeCount; i++)
	{
		PPCRecImlInstruction_t* imlInstructionItr = imlSegment->imlList + (insertIndex + i);
		memset(imlInstructionItr, 0x00, sizeof(PPCRecImlInstruction_t));
		imlInstructionItr->type = PPCREC_IML_TYPE_NAME_R;
		imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
		imlInstructionItr->op_r_name.registerIndex = (uint8)storeList[i].registerIndex;
		imlInstructionItr->op_r_name.name = (uint32)storeList[i].registerName;
		imlInstructionItr->op_r_name.copyWidth = 32;
		imlInstructionItr->op_r_name.flags = 0;
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
sint32 PPCRecRA_countInstructionsUntilNextLocalPhysRegisterUse(PPCRecImlSegment_t* imlSegment, sint32 startIndex, sint32 physRegister)
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

typedef struct  
{
	raLivenessSubrange_t* liveRangeList[64];
	sint32 liveRangesCount;
}raLiveRangeInfo_t;

// return a bitmask that contains only registers that are not used by any colliding range
uint32 PPCRecRA_getAllowedRegisterMaskForFullRange(raLivenessRange_t* range)
{
	uint32 physRegisterMask = (1 << PPC_X64_GPR_USABLE_REGISTERS) - 1;
	for (auto& subrange : range->list_subranges)
	{
		PPCRecImlSegment_t* imlSegment = subrange->imlSegment;
		raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while(subrangeItr)
		{
			if (subrange == subrangeItr)
			{
				// next
				subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
				continue;
			}

			if (subrange->start.index < subrangeItr->end.index && subrange->end.index > subrangeItr->start.index ||
				(subrange->start.index == RA_INTER_RANGE_START && subrange->start.index == subrangeItr->start.index) ||
				(subrange->end.index == RA_INTER_RANGE_END && subrange->end.index == subrangeItr->end.index) )
			{
				if(subrangeItr->range->physicalRegister >= 0)
					physRegisterMask &= ~(1<<(subrangeItr->range->physicalRegister));
			}
			// next
			subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
		}
	}
	return physRegisterMask;
}

bool _livenessRangeStartCompare(raLivenessSubrange_t* lhs, raLivenessSubrange_t* rhs) { return lhs->start.index < rhs->start.index; }

void _sortSegmentAllSubrangesLinkedList(PPCRecImlSegment_t* imlSegment)
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
	//for (sint32 i1 = 0; i1 < count; i1++)
	//{
	//	for (sint32 i2 = i1+1; i2 < count; i2++)
	//	{
	//		if (subrangeList[i1]->start.index > subrangeList[i2]->start.index)
	//		{
	//			// swap
	//			raLivenessSubrange_t* temp = subrangeList[i1];
	//			subrangeList[i1] = subrangeList[i2];
	//			subrangeList[i2] = temp;
	//		}
	//	}
	//}
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

bool PPCRecRA_assignSegmentRegisters(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment)
{

	// sort subranges ascending by start index

	//std::sort(imlSegment->raInfo.list_subranges.begin(), imlSegment->raInfo.list_subranges.end(), _sortSubrangesByStartIndexDepr);
	_sortSegmentAllSubrangesLinkedList(imlSegment);
	
	raLiveRangeInfo_t liveInfo;
	liveInfo.liveRangesCount = 0;
	//sint32 subrangeIndex = 0;
	//for (auto& subrange : imlSegment->raInfo.list_subranges)
	raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while(subrangeItr)
	{
		sint32 currentIndex = subrangeItr->start.index;
		// validate subrange
		PPCRecRA_debugValidateSubrange(subrangeItr);
		// expire ranges
		for (sint32 f = 0; f < liveInfo.liveRangesCount; f++)
		{
			raLivenessSubrange_t* liverange = liveInfo.liveRangeList[f];
			if (liverange->end.index <= currentIndex && liverange->end.index != RA_INTER_RANGE_END)
			{
#ifdef CEMU_DEBUG_ASSERT
				if (liverange->subrangeBranchTaken || liverange->subrangeBranchNotTaken)
					assert_dbg(); // infinite subranges should not expire
#endif
				// remove entry
				liveInfo.liveRangesCount--;
				liveInfo.liveRangeList[f] = liveInfo.liveRangeList[liveInfo.liveRangesCount];
				f--;
			}
		}
		// check if subrange already has register assigned
		if (subrangeItr->range->physicalRegister >= 0)
		{
			// verify if register is actually available
#ifdef CEMU_DEBUG_ASSERT
			for (sint32 f = 0; f < liveInfo.liveRangesCount; f++)
			{
				raLivenessSubrange_t* liverangeItr = liveInfo.liveRangeList[f];
				if (liverangeItr->range->physicalRegister == subrangeItr->range->physicalRegister)
				{
					// this should never happen because we try to preventively avoid register conflicts
					assert_dbg();				
				}
			}
#endif
			// add to live ranges
			liveInfo.liveRangeList[liveInfo.liveRangesCount] = subrangeItr;
			liveInfo.liveRangesCount++;
			// next
			subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
			continue;
		}
		// find free register
		uint32 physRegisterMask = (1<<PPC_X64_GPR_USABLE_REGISTERS)-1;
		for (sint32 f = 0; f < liveInfo.liveRangesCount; f++)
		{
			raLivenessSubrange_t* liverange = liveInfo.liveRangeList[f];
			if (liverange->range->physicalRegister < 0)
				assert_dbg();
			physRegisterMask &= ~(1<<liverange->range->physicalRegister);
		}
		// check intersections with other ranges and determine allowed registers
		uint32 allowedPhysRegisterMask = 0;
		uint32 unusedRegisterMask = physRegisterMask; // mask of registers that are currently not used (does not include range checks)
		if (physRegisterMask != 0)
		{
			allowedPhysRegisterMask = PPCRecRA_getAllowedRegisterMaskForFullRange(subrangeItr->range);
			physRegisterMask &= allowedPhysRegisterMask;
		}
		if (physRegisterMask == 0)
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
					for (sint32 f = 0; f < liveInfo.liveRangesCount; f++)
					{
						raLivenessSubrange_t* candidate = liveInfo.liveRangeList[f];
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
				spillStrategies.availableRegisterHole.cost = INT_MAX;
				spillStrategies.availableRegisterHole.distance = -1;
				spillStrategies.availableRegisterHole.physRegister = -1;
				if (currentIndex >= 0)
				{
					if (unusedRegisterMask != 0)
					{
						for (sint32 t = 0; t < PPC_X64_GPR_USABLE_REGISTERS; t++)
						{
							if ((unusedRegisterMask&(1 << t)) == 0)
								continue;
							// get size of potential hole for this register
							sint32 distance = PPCRecRA_countInstructionsUntilNextLocalPhysRegisterUse(imlSegment, currentIndex, t);
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
								spillStrategies.availableRegisterHole.physRegister = t;
							}
						}
					}
				}
				// evaluate strategy: Explode inter-segment ranges
				spillStrategies.explodeRange.cost = INT_MAX;
				spillStrategies.explodeRange.range = nullptr;
				spillStrategies.explodeRange.distance = -1;
				for (sint32 f = 0; f < liveInfo.liveRangesCount; f++)
				{
					raLivenessSubrange_t* candidate = liveInfo.liveRangeList[f];
					if (candidate->end.index != RA_INTER_RANGE_END)
						continue;
					sint32 distance = PPCRecRA_countInstructionsUntilNextUse(liveInfo.liveRangeList[f], currentIndex);
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
				for (sint32 f = 0; f < liveInfo.liveRangesCount; f++)
				{
					raLivenessSubrange_t* candidate = liveInfo.liveRangeList[f];
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
		sint32 registerIndex = -1;
		for (sint32 f = 0; f < PPC_X64_GPR_USABLE_REGISTERS; f++)
		{
			if ((physRegisterMask&(1 << f)) != 0)
			{
				registerIndex = f;
				break;
			}
		}
		subrangeItr->range->physicalRegister = registerIndex;
		// add to live ranges
		liveInfo.liveRangeList[liveInfo.liveRangesCount] = subrangeItr;
		liveInfo.liveRangesCount++;
		// next
		subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;	
	}
	return true;
}

void PPCRecRA_assignRegisters(ppcImlGenContext_t* ppcImlGenContext)
{
	// start with frequently executed segments first
	sint32 maxLoopDepth = 0;
	for (sint32 i = 0; i < ppcImlGenContext->segmentListCount; i++)
	{
		maxLoopDepth = std::max(maxLoopDepth, ppcImlGenContext->segmentList[i]->loopDepth);
	}
	while (true)
	{
		bool done = false;
		for (sint32 d = maxLoopDepth; d >= 0; d--)
		{
			for (sint32 i = 0; i < ppcImlGenContext->segmentListCount; i++)
			{
				PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[i];
				if (imlSegment->loopDepth != d)
					continue;
				done = PPCRecRA_assignSegmentRegisters(ppcImlGenContext, imlSegment);
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

typedef struct  
{
	raLivenessSubrange_t* subrangeList[SUBRANGE_LIST_SIZE];
	sint32 subrangeCount;
	bool hasUndefinedEndings;
}subrangeEndingInfo_t;

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
	PPCRecImlSegment_t* imlSegment = subrange->imlSegment;
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

void PPCRecRA_generateSegmentInstructions(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment)
{
	sint16 virtualReg2PhysReg[PPC_REC_MAX_VIRTUAL_GPR];
	for (sint32 i = 0; i < PPC_REC_MAX_VIRTUAL_GPR; i++)
		virtualReg2PhysReg[i] = -1;

	raLiveRangeInfo_t liveInfo;
	liveInfo.liveRangesCount = 0;
	sint32 index = 0;
	sint32 suffixInstructionCount = (imlSegment->imlListCount > 0 && PPCRecompiler_isSuffixInstruction(imlSegment->imlList + imlSegment->imlListCount - 1)) ? 1 : 0;
	// load register ranges that are supplied from previous segments
	raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	//for (auto& subrange : imlSegment->raInfo.list_subranges)
	while(subrangeItr)
	{
		if (subrangeItr->start.index == RA_INTER_RANGE_START)
		{
			liveInfo.liveRangeList[liveInfo.liveRangesCount] = subrangeItr;
			liveInfo.liveRangesCount++;
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
		}
		// next
		subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
	}
	// process instructions
	while(index < imlSegment->imlListCount+1)
	{
		// expire ranges
		for (sint32 f = 0; f < liveInfo.liveRangesCount; f++)
		{
			raLivenessSubrange_t* liverange = liveInfo.liveRangeList[f];
			if (liverange->end.index <= index)
			{
				// update translation table
				if (virtualReg2PhysReg[liverange->range->virtualRegister] == -1)
					assert_dbg();
				virtualReg2PhysReg[liverange->range->virtualRegister] = -1;
				// store GPR
				if (liverange->hasStore)
				{
					PPCRecRA_insertGPRStoreInstruction(imlSegment, std::min(index, imlSegment->imlListCount - suffixInstructionCount), liverange->range->physicalRegister, liverange->range->name);
					index++;
				}
				// remove entry
				liveInfo.liveRangesCount--;
				liveInfo.liveRangeList[f] = liveInfo.liveRangeList[liveInfo.liveRangesCount];
				f--;
			}
		}
		// load new ranges
		subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while(subrangeItr)
		{
			if (subrangeItr->start.index == index)
			{
				liveInfo.liveRangeList[liveInfo.liveRangesCount] = subrangeItr;
				liveInfo.liveRangesCount++;
				// load GPR
				if (subrangeItr->_noLoad == false)
				{
					PPCRecRA_insertGPRLoadInstruction(imlSegment, std::min(index, imlSegment->imlListCount - suffixInstructionCount), subrangeItr->range->physicalRegister, subrangeItr->range->name);
					index++;
					subrangeItr->start.index--;
				}
				// update translation table
				cemu_assert_debug(virtualReg2PhysReg[subrangeItr->range->virtualRegister] == -1);
				virtualReg2PhysReg[subrangeItr->range->virtualRegister] = subrangeItr->range->physicalRegister;
			}
			subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
		}
		// replace registers
		if (index < imlSegment->imlListCount)
		{
			PPCImlOptimizerUsedRegisters_t gprTracking;
			PPCRecompiler_checkRegisterUsage(NULL, imlSegment->imlList + index, &gprTracking);

			sint32 inputGpr[4];
			inputGpr[0] = gprTracking.gpr[0];
			inputGpr[1] = gprTracking.gpr[1];
			inputGpr[2] = gprTracking.gpr[2];
			inputGpr[3] = gprTracking.gpr[3];
			sint32 replaceGpr[4];
			for (sint32 f = 0; f < 4; f++)
			{
				sint32 virtualRegister = gprTracking.gpr[f];
				if (virtualRegister < 0)
				{
					replaceGpr[f] = -1;
					continue;
				}
				if (virtualRegister >= PPC_REC_MAX_VIRTUAL_GPR)
					assert_dbg();
				replaceGpr[f] = virtualReg2PhysReg[virtualRegister];
				cemu_assert_debug(replaceGpr[f] >= 0);
			}
			PPCRecompiler_replaceGPRRegisterUsageMultiple(ppcImlGenContext, imlSegment->imlList + index, inputGpr, replaceGpr);
		}
		// next iml instruction
		index++;
	}
	// expire infinite subranges (subranges that cross the segment border)
	sint32 storeLoadListLength = 0;
	raLoadStoreInfo_t loadStoreList[PPC_REC_MAX_VIRTUAL_GPR];
	for (sint32 f = 0; f < liveInfo.liveRangesCount; f++)
	{
		raLivenessSubrange_t* liverange = liveInfo.liveRangeList[f];
		if (liverange->end.index == RA_INTER_RANGE_END)
		{
			// update translation table
			cemu_assert_debug(virtualReg2PhysReg[liverange->range->virtualRegister] != -1);
			virtualReg2PhysReg[liverange->range->virtualRegister] = -1;
			// store GPR
			if (liverange->hasStore)
			{
				loadStoreList[storeLoadListLength].registerIndex = liverange->range->physicalRegister;
				loadStoreList[storeLoadListLength].registerName = liverange->range->name;
				storeLoadListLength++;
			}
			// remove entry
			liveInfo.liveRangesCount--;
			liveInfo.liveRangeList[f] = liveInfo.liveRangeList[liveInfo.liveRangesCount];
			f--;
		}
		else
		{
			cemu_assert_suspicious();
		}
	}
	if (storeLoadListLength > 0)
	{
		PPCRecRA_insertGPRStoreInstructions(imlSegment, imlSegment->imlListCount - suffixInstructionCount, loadStoreList, storeLoadListLength);
	}
	// load subranges for next segments
	subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	storeLoadListLength = 0;
	while(subrangeItr)
	{
		if (subrangeItr->start.index == RA_INTER_RANGE_END)
		{
			liveInfo.liveRangeList[liveInfo.liveRangesCount] = subrangeItr;
			liveInfo.liveRangesCount++;
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
		}
		// next
		subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
	}
	if (storeLoadListLength > 0)
	{
		PPCRecRA_insertGPRLoadInstructions(imlSegment, imlSegment->imlListCount - suffixInstructionCount, loadStoreList, storeLoadListLength);
	}
}

void PPCRecRA_generateMoveInstructions(ppcImlGenContext_t* ppcImlGenContext)
{
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
		PPCRecRA_generateSegmentInstructions(ppcImlGenContext, imlSegment);
	}
}

void PPCRecRA_calculateLivenessRangesV2(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecRA_processFlowAndCalculateLivenessRangesV2(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecRA_analyzeRangeDataFlowV2(ppcImlGenContext_t* ppcImlGenContext);

void PPCRecompilerImm_prepareForRegisterAllocation(ppcImlGenContext_t* ppcImlGenContext)
{
	// insert empty segments after every non-taken branch if the linked segment has more than one input
	// this gives the register allocator more room to create efficient spill code
	sint32 segmentIndex = 0;
	while (segmentIndex < ppcImlGenContext->segmentListCount)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[segmentIndex];
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
		PPCRecImlSegment_t* imlSegmentP0 = ppcImlGenContext->segmentList[segmentIndex + 0];
		PPCRecImlSegment_t* imlSegmentP1 = ppcImlGenContext->segmentList[segmentIndex + 1];
		PPCRecImlSegment_t* nextSegment = imlSegment->nextSegmentBranchNotTaken;
		PPCRecompilerIML_removeLink(imlSegmentP0, nextSegment);
		PPCRecompilerIml_setLinkBranchNotTaken(imlSegmentP1, nextSegment);
		PPCRecompilerIml_setLinkBranchNotTaken(imlSegmentP0, imlSegmentP1);
		segmentIndex++;
	}
	// detect loops
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
		imlSegment->momentaryIndex = s;
	}
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
		PPCRecRA_identifyLoop(ppcImlGenContext, imlSegment);
	}
}

void PPCRecompilerImm_allocateRegisters(ppcImlGenContext_t* ppcImlGenContext)
{
	PPCRecompilerImm_prepareForRegisterAllocation(ppcImlGenContext);

	ppcImlGenContext->raInfo.list_ranges = std::vector<raLivenessRange_t*>();
	
	// calculate liveness
	PPCRecRA_calculateLivenessRangesV2(ppcImlGenContext);
	PPCRecRA_processFlowAndCalculateLivenessRangesV2(ppcImlGenContext);

	PPCRecRA_assignRegisters(ppcImlGenContext);

	PPCRecRA_analyzeRangeDataFlowV2(ppcImlGenContext);
	PPCRecRA_generateMoveInstructions(ppcImlGenContext);

	PPCRecRA_deleteAllRanges(ppcImlGenContext);
}