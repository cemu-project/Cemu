#include "IML.h"

#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"
#include "IMLRegisterAllocator.h"
#include "IMLRegisterAllocatorRanges.h"

#include "../BackendX64/BackendX64.h"

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

sint32 PPCRecRA_countInstructionsUntilNextUse(raLivenessRange* subrange, sint32 startIndex)
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
	raLivenessRange* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while(subrangeItr)
	{
		if (subrangeItr->GetPhysicalRegister() != physRegister)
		{
			subrangeItr = subrangeItr->link_allSegmentRanges.next;
			continue;
		}
		if (startIndex >= subrangeItr->start.index && startIndex < subrangeItr->end.index)
			return 0;
		if (subrangeItr->start.index >= startIndex)
		{
			minDistance = std::min(minDistance, (subrangeItr->start.index - startIndex));
		}
		subrangeItr = subrangeItr->link_allSegmentRanges.next;
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
	void AddActiveRange(raLivenessRange* subrange)
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
			raLivenessRange* liverange = activeRanges[f];
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

	std::span<raLivenessRange*> GetExpiredRanges()
	{
		return { expiredRanges.data(), expiredRanges.size() };
	}

	boost::container::small_vector<raLivenessRange*, 64> activeRanges;

private:
	boost::container::small_vector<raLivenessRange*, 16> expiredRanges;
};

bool IsRangeOverlapping(raLivenessRange* rangeA, raLivenessRange* rangeB)
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
			if(IsRangeOverlapping(subrange, subrangeItr))
			{
				if (subrangeItr->GetPhysicalRegister() >= 0)
					physRegSet.SetReserved(subrangeItr->GetPhysicalRegister());
			}
			// next
			subrangeItr = subrangeItr->link_allSegmentRanges.next;
		}
	}
}

bool _livenessRangeStartCompare(raLivenessRange* lhs, raLivenessRange* rhs) { return lhs->start.index < rhs->start.index; }

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
	sint32 currentStartIndex = RA_INTER_RANGE_START;
	while (subrangeItr)
	{
		count2++;
		if (subrangeItr->start.index < currentStartIndex)
			assert_dbg();
		currentStartIndex = subrangeItr->start.index;
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

raLivenessRange* _GetSubrangeByInstructionIndexAndVirtualReg(IMLSegment* imlSegment, IMLReg regToSearch, sint32 instructionIndex)
{
	uint32 regId = regToSearch.GetRegID();
	raLivenessRange* subrangeItr = IMLRA_GetSubrange(imlSegment, regId);
	while (subrangeItr)
	{
		if (subrangeItr->start.index <= instructionIndex && subrangeItr->end.index > instructionIndex)
			return subrangeItr;
		subrangeItr = subrangeItr->link_sameVirtualRegister.next;
	}
	return nullptr;
}

void IMLRA_IsolateRangeOnInstruction(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, raLivenessRange* subrange, sint32 instructionIndex)
{
	DEBUG_BREAK;
}

void IMLRA_HandleFixedRegisters(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
{
	// this works as a pre-pass to actual register allocation. Assigning registers in advance based on fixed requirements (e.g. calling conventions and operations with fixed-reg input/output like x86 DIV/MUL)
	// algorithm goes as follows:
	// 1) Iterate all instructions in the function from beginning to end and keep a list of active ranges for the currently iterated instruction
	// 2) If we encounter an instruction with a fixed register requirement we:
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
	raLivenessRange* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while(subrangeItr)
	{
		sint32 currentIndex = subrangeItr->start.index;
		PPCRecRA_debugValidateSubrange(subrangeItr);
		livenessTimeline.ExpireRanges(std::min<sint32>(currentIndex, RA_INTER_RANGE_END-1)); // expire up to currentIndex (inclusive), but exclude infinite ranges
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
		// find free register for current subrangeItr and segment
		IMLRegFormat regBaseFormat = ctx.GetBaseFormatByRegId(subrangeItr->GetVirtualRegister());
		IMLPhysRegisterSet physRegSet = ctx.raParam->GetPhysRegPool(regBaseFormat);
		cemu_assert_debug(physRegSet.HasAnyAvailable()); // register uses type with no valid pool
		for (auto& liverangeItr : livenessTimeline.activeRanges)
		{
			cemu_assert_debug(liverangeItr->GetPhysicalRegister() >= 0);
			physRegSet.SetReserved(liverangeItr->GetPhysicalRegister());
		}
		// check intersections with other ranges and determine allowed registers
		IMLPhysRegisterSet localAvailableRegsMask = physRegSet; // mask of registers that are currently not used (does not include range checks in other segments)
		if(physRegSet.HasAnyAvailable())
		{
			// check globally in all segments
			PPCRecRA_MaskOverlappingPhysRegForGlobalRange(subrangeItr, physRegSet);
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
					raLivenessRange* largestHoleSubrange;
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
					raLivenessRange* range;
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
					cost = PPCRecRARange_estimateCostAfterRangeExplode(candidate);
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
						spillStrategies.explodeRange.range = candidate;
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
					PPCRecRA_explodeRange(ppcImlGenContext, subrangeItr);
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
					if (candidate->GetPhysicalRegister() < 0 && candidate != subrangeItr)
						continue;
					
					sint32 cost;
					cost = PPCRecRARange_estimateCostAfterRangeExplode(candidate);
					// compare with current best candidate for this strategy
					if (cost < spillStrategies.explodeRange.cost)
					{
						spillStrategies.explodeRange.cost = cost;
						spillStrategies.explodeRange.distance = INT_MAX;
						spillStrategies.explodeRange.range = candidate;
					}
				}
				// add current range as a candidate too
				sint32 ownCost;
				ownCost = PPCRecRARange_estimateCostAfterRangeExplode(subrangeItr);
				if (ownCost < spillStrategies.explodeRange.cost)
				{
					spillStrategies.explodeRange.cost = ownCost;
					spillStrategies.explodeRange.distance = INT_MAX;
					spillStrategies.explodeRange.range = subrangeItr;
				}
				if (spillStrategies.explodeRange.cost == INT_MAX)
					assert_dbg(); // should not happen
				PPCRecRA_explodeRange(ppcImlGenContext, spillStrategies.explodeRange.range);
			}
			return false;
		}
		// assign register to range
		//subrangeItr->SetPhysicalRegister(physRegSet.GetFirstAvailableReg());
		subrangeItr->SetPhysicalRegisterForCluster(physRegSet.GetFirstAvailableReg());
		livenessTimeline.AddActiveRange(subrangeItr);
		// next
		subrangeItr = subrangeItr->link_allSegmentRanges.next;
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

inline IMLReg _MakeNativeReg(IMLRegFormat baseFormat, IMLRegID regId)
{
	return IMLReg(baseFormat, baseFormat, 0, regId);
}

void PPCRecRA_insertGPRLoadInstructions(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment, sint32 insertIndex, std::span<raLivenessRange*> loadList)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, insertIndex, loadList.size());
	for (sint32 i = 0; i < loadList.size(); i++)
	{
		IMLRegFormat baseFormat = ctx.regIdToBaseFormat[loadList[i]->GetVirtualRegister()];
		cemu_assert_debug(baseFormat != IMLRegFormat::INVALID_FORMAT);
		imlSegment->imlList[insertIndex + i].make_r_name(_MakeNativeReg(baseFormat, loadList[i]->GetPhysicalRegister()), loadList[i]->GetName());
	}
}

void PPCRecRA_insertGPRStoreInstructions(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment, sint32 insertIndex, std::span<raLivenessRange*> storeList)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, insertIndex, storeList.size());
	for (size_t i = 0; i < storeList.size(); i++)
	{
		IMLRegFormat baseFormat = ctx.regIdToBaseFormat[storeList[i]->GetVirtualRegister()];
		cemu_assert_debug(baseFormat != IMLRegFormat::INVALID_FORMAT);
		imlSegment->imlList[insertIndex + i].make_name_r(storeList[i]->GetName(), _MakeNativeReg(baseFormat, storeList[i]->GetPhysicalRegister()));
	}
}

void IMLRA_GenerateSegmentMoveInstructions(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment)
{
	std::unordered_map<IMLRegID, IMLRegID> virtId2PhysRegIdMap; // key = virtual register, value = physical register
	IMLRALivenessTimeline livenessTimeline;
	sint32 index = 0;
	sint32 suffixInstructionCount = imlSegment->HasSuffixInstruction() ? 1 : 0;
	// load register ranges that are supplied from previous segments
	raLivenessRange* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
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
			cemu_assert_debug(!virtId2PhysRegIdMap.contains(subrangeItr->GetVirtualRegister()));
#endif
			virtId2PhysRegIdMap.try_emplace(subrangeItr->GetVirtualRegister(), subrangeItr->GetPhysicalRegister());
		}
		// next
		subrangeItr = subrangeItr->link_allSegmentRanges.next;
	}
	// process instructions
	while(index < imlSegment->imlList.size() + 1)
	{
		// expire ranges
		livenessTimeline.ExpireRanges(index);
		for (auto& expiredRange : livenessTimeline.GetExpiredRanges())
		{
			// update translation table
			virtId2PhysRegIdMap.erase(expiredRange->GetVirtualRegister());
			// store GPR if required
			// special care has to be taken to execute any stores before the suffix instruction since trailing instructions may not get executed
			if (expiredRange->hasStore)
			{
				PPCRecRA_insertGPRStoreInstructions(ctx, imlSegment, std::min<sint32>(index, imlSegment->imlList.size() - suffixInstructionCount), {&expiredRange, 1});
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
					PPCRecRA_insertGPRLoadInstructions(ctx, imlSegment, std::min<sint32>(index, imlSegment->imlList.size() - suffixInstructionCount), {&subrangeItr , 1});
					index++;
					subrangeItr->start.index--;
				}
				// update translation table
				virtId2PhysRegIdMap.insert_or_assign(subrangeItr->GetVirtualRegister(), subrangeItr->GetPhysicalRegister());
			}
			subrangeItr = subrangeItr->link_allSegmentRanges.next;
		}
		// rewrite registers
		if (index < imlSegment->imlList.size())
			imlSegment->imlList[index].RewriteGPR(virtId2PhysRegIdMap);
		// next iml instruction
		index++;
	}
	// expire infinite subranges (subranges which cross the segment border)
	std::vector<raLivenessRange*> loadStoreList;
	livenessTimeline.ExpireRanges(RA_INTER_RANGE_END);
	for (auto liverange : livenessTimeline.GetExpiredRanges())
	{
		// update translation table
		virtId2PhysRegIdMap.erase(liverange->GetVirtualRegister());
		// store GPR
		if (liverange->hasStore)
			loadStoreList.emplace_back(liverange);
	}
	cemu_assert_debug(livenessTimeline.activeRanges.empty());
	if (!loadStoreList.empty())
		PPCRecRA_insertGPRStoreInstructions(ctx, imlSegment, imlSegment->imlList.size() - suffixInstructionCount, loadStoreList);
	// load subranges for next segments
	subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	loadStoreList.clear();
	while(subrangeItr)
	{
		if (subrangeItr->start.index == RA_INTER_RANGE_END)
		{
			livenessTimeline.AddActiveRange(subrangeItr);
			// load GPR
			if (subrangeItr->_noLoad == false)
				loadStoreList.emplace_back(subrangeItr);
			// update translation table
			virtId2PhysRegIdMap.try_emplace(subrangeItr->GetVirtualRegister(), subrangeItr->GetPhysicalRegister());
		}
		// next
		subrangeItr = subrangeItr->link_allSegmentRanges.next;
	}
	if (!loadStoreList.empty())
		PPCRecRA_insertGPRLoadInstructions(ctx, imlSegment, imlSegment->imlList.size() - suffixInstructionCount, loadStoreList);
}

void IMLRA_GenerateMoveInstructions(IMLRegisterAllocatorContext& ctx)
{
	for (size_t s = 0; s < ctx.deprGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ctx.deprGenContext->segmentList2[s];
		IMLRA_GenerateSegmentMoveInstructions(ctx, imlSegment);
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
	raLivenessRange* subrange = PPCRecRA_createSubrange(ctx.deprGenContext, imlSegment, vGPR, name, abstractRange->usageStart, abstractRange->usageEnd);
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
				cemu_assert_debug(subrange->subrangeBranchTaken->start.index == RA_INTER_RANGE_START);
			}
		}
		if (imlSegment->nextSegmentBranchNotTaken)
		{
			IMLRARegAbstractLiveness* branchNotTakenRange = _GetAbstractRange(ctx, imlSegment->nextSegmentBranchNotTaken, vGPR);
			if (branchNotTakenRange && branchNotTakenRange->usageStart == RA_INTER_RANGE_START)
			{
				subrange->subrangeBranchNotTaken = PPCRecRA_convertToMappedRanges(ctx, imlSegment->nextSegmentBranchNotTaken, vGPR, name);
				subrange->subrangeBranchNotTaken->previousRanges.push_back(subrange);
				cemu_assert_debug(subrange->subrangeBranchNotTaken->start.index == RA_INTER_RANGE_START);
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
	if (subrange->end.index == RA_INTER_RANGE_END)
	{
		if (imlSegment->HasSuffixInstruction())
		{
			cemu_assert_debug(subrange->start.index <= imlSegment->GetSuffixInstructionIndex());
		}
	}
	return subrange;
}

// take abstract range data and create LivenessRanges
void IMLRA_ConvertAbstractToLivenessRanges(IMLRegisterAllocatorContext& ctx, IMLSegment* imlSegment)
{
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
	const std::unordered_map<IMLRegID, raLivenessRange*>& regToSubrange = IMLRA_GetSubrangeMap(imlSegment);
	size_t index = 0;
	IMLUsedRegisters gprTracking;
	while (index < imlSegment->imlList.size())
	{
		imlSegment->imlList[index].CheckRegisterUsage(&gprTracking);
		gprTracking.ForEachAccessedGPR([&](IMLReg gprReg, bool isWritten) {
			IMLRegID gprId = gprReg.GetRegID();
			raLivenessRange* subrange = regToSubrange.find(gprId)->second;
			PPCRecRA_updateOrAddSubrangeLocation(subrange, index, !isWritten, isWritten);
#ifdef CEMU_DEBUG_ASSERT
		if ((sint32)index < subrange->start.index)
		{
			IMLRARegAbstractLiveness* dbgAbstractRange = _GetAbstractRange(ctx, imlSegment, gprId);
			assert_dbg();
		}
		if ((sint32)index + 1 > subrange->end.index)
			assert_dbg();
#endif
			});
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

void IMLRA_mergeCloseAbstractRanges(IMLRegisterAllocatorContext& ctx)
{
	for (size_t s = 0; s < ctx.deprGenContext->segmentList2.size(); s++)
	{
		IMLSegment* imlSegment = ctx.deprGenContext->segmentList2[s];
		if (!imlSegment->list_prevSegments.empty())
			continue; // not an entry/standalone segment
		PPCRecRA_followFlowAndExtendRanges(ctx, imlSegment);
	}
}

void IMLRA_extendAbstracRangesOutOfLoops(IMLRegisterAllocatorContext& ctx)
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
	IMLRA_mergeCloseAbstractRanges(ctx);
	// extra pass to move register stores out of loops
	IMLRA_extendAbstracRangesOutOfLoops(ctx);
	// calculate liveness ranges
	for (auto& segIt : ctx.deprGenContext->segmentList2)
		IMLRA_ConvertAbstractToLivenessRanges(ctx, segIt);
}

void PPCRecRA_analyzeSubrangeDataDependencyV2(raLivenessRange* subrange)
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

static void _analyzeRangeDataFlow(raLivenessRange* subrange)
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
	// this function is called after _assignRegisters(), which means that all liveness ranges are already final and must not be changed anymore
	// in the first pass we track read/write dependencies
	for(auto& seg : ppcImlGenContext->segmentList2)
	{
		raLivenessRange* subrange = seg->raInfo.linkedList_allSubranges;
		while(subrange)
		{
			PPCRecRA_analyzeSubrangeDataDependencyV2(subrange);
			subrange = subrange->link_allSegmentRanges.next;
		}
	}
	// then we do a second pass where we scan along subrange flow
	for(auto& seg : ppcImlGenContext->segmentList2)
	{
		raLivenessRange* subrange = seg->raInfo.linkedList_allSubranges;
		while(subrange)
		{
			_analyzeRangeDataFlow(subrange);
			subrange = subrange->link_allSegmentRanges.next;
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

	IMLRA_AnalyzeRangeDataFlow(ppcImlGenContext);
	IMLRA_GenerateMoveInstructions(ctx);

	PPCRecRA_deleteAllRanges(ppcImlGenContext);
}
