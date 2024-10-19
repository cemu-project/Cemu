#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"
#include "IMLRegisterAllocatorRanges.h"
#include "util/helpers/MemoryPool.h"

uint32 PPCRecRA_getNextIterationIndex();

IMLRegID raLivenessRange::GetVirtualRegister() const
{
	return virtualRegister;
}

sint32 raLivenessRange::GetPhysicalRegister() const
{
	return physicalRegister;
}

IMLName raLivenessRange::GetName() const
{
	return name;
}

void raLivenessRange::SetPhysicalRegister(sint32 physicalRegister)
{
	this->physicalRegister = physicalRegister;
}

void raLivenessRange::SetPhysicalRegisterForCluster(sint32 physicalRegister)
{
	auto clusterRanges = GetAllSubrangesInCluster();
	for(auto& range : clusterRanges)
		range->physicalRegister = physicalRegister;
}

boost::container::small_vector<raLivenessRange*, 32> raLivenessRange::GetAllSubrangesInCluster()
{
	uint32 iterationIndex = PPCRecRA_getNextIterationIndex();
	boost::container::small_vector<raLivenessRange*, 32> subranges;
	subranges.push_back(this);
	this->lastIterationIndex = iterationIndex;
	size_t i = 0;
	while(i<subranges.size())
	{
		raLivenessRange* cur = subranges[i];
		i++;
		// check successors
		if(cur->subrangeBranchTaken && cur->subrangeBranchTaken->lastIterationIndex != iterationIndex)
		{
			cur->subrangeBranchTaken->lastIterationIndex = iterationIndex;
			subranges.push_back(cur->subrangeBranchTaken);
		}
		if(cur->subrangeBranchNotTaken && cur->subrangeBranchNotTaken->lastIterationIndex != iterationIndex)
		{
			cur->subrangeBranchNotTaken->lastIterationIndex = iterationIndex;
			subranges.push_back(cur->subrangeBranchNotTaken);
		}
		// check predecessors
		for(auto& prev : cur->previousRanges)
		{
			if(prev->lastIterationIndex != iterationIndex)
			{
				prev->lastIterationIndex = iterationIndex;
				subranges.push_back(prev);
			}
		}
	}
	return subranges;
}

bool raLivenessRange::GetAllowedRegistersEx(IMLPhysRegisterSet& allowedRegisters)
{
	if(interval2.ExtendsPreviousSegment() || interval2.ExtendsIntoNextSegment())
	{
		auto clusterRanges = GetAllSubrangesInCluster();
		bool hasAnyRequirement = false;
		for(auto& subrange : clusterRanges)
		{
			if(subrange->list_fixedRegRequirements.empty())
				continue;
			allowedRegisters = subrange->list_fixedRegRequirements.front().allowedReg;
			hasAnyRequirement = true;
			break;
		}
		if(!hasAnyRequirement)
			return false;
		for(auto& subrange : clusterRanges)
		{
			for(auto& fixedRegLoc : subrange->list_fixedRegRequirements)
				allowedRegisters &= fixedRegLoc.allowedReg;
		}
	}
	else
	{
		// local check only, slightly faster
		if(list_fixedRegRequirements.empty())
			return false;
		allowedRegisters = list_fixedRegRequirements.front().allowedReg;
		for(auto& fixedRegLoc : list_fixedRegRequirements)
			allowedRegisters &= fixedRegLoc.allowedReg;
	}
	return true;
}

IMLPhysRegisterSet raLivenessRange::GetAllowedRegisters(IMLPhysRegisterSet regPool)
{
	IMLPhysRegisterSet fixedRegRequirements = regPool;
	if(interval2.ExtendsPreviousSegment() || interval2.ExtendsIntoNextSegment())
	{
		auto clusterRanges = GetAllSubrangesInCluster();
		for(auto& subrange : clusterRanges)
		{
			for(auto& fixedRegLoc : subrange->list_fixedRegRequirements)
				fixedRegRequirements &= fixedRegLoc.allowedReg;
		}
		return fixedRegRequirements;
	}
	for(auto& fixedRegLoc : list_fixedRegRequirements)
		fixedRegRequirements &= fixedRegLoc.allowedReg;
	return fixedRegRequirements;
}

void PPCRecRARange_addLink_perVirtualGPR(std::unordered_map<IMLRegID, raLivenessRange*>& root, raLivenessRange* subrange)
{
	IMLRegID regId = subrange->GetVirtualRegister();
	auto it = root.find(regId);
	if (it == root.end())
	{
		// new single element
		root.try_emplace(regId, subrange);
		subrange->link_sameVirtualRegister.prev = nullptr;
		subrange->link_sameVirtualRegister.next = nullptr;
	}
	else
	{
		// insert in first position
		raLivenessRange* priorFirst = it->second;
		subrange->link_sameVirtualRegister.next = priorFirst;
		it->second = subrange;
		subrange->link_sameVirtualRegister.prev = nullptr;
		priorFirst->link_sameVirtualRegister.prev = subrange;
	}
}

void PPCRecRARange_addLink_allSegmentRanges(raLivenessRange** root, raLivenessRange* subrange)
{
	subrange->link_allSegmentRanges.next = *root;
	if (*root)
		(*root)->link_allSegmentRanges.prev = subrange;
	subrange->link_allSegmentRanges.prev = nullptr;
	*root = subrange;
}

void PPCRecRARange_removeLink_perVirtualGPR(std::unordered_map<IMLRegID, raLivenessRange*>& root, raLivenessRange* subrange)
{
	IMLRegID regId = subrange->GetVirtualRegister();
	raLivenessRange* nextRange = subrange->link_sameVirtualRegister.next;
	raLivenessRange* prevRange = subrange->link_sameVirtualRegister.prev;
	raLivenessRange* newBase = prevRange ? prevRange : nextRange;
	if (prevRange)
		prevRange->link_sameVirtualRegister.next = subrange->link_sameVirtualRegister.next;
	if (nextRange)
		nextRange->link_sameVirtualRegister.prev = subrange->link_sameVirtualRegister.prev;

	if (!prevRange)
	{
		if (nextRange)
		{
			root.find(regId)->second = nextRange;
		}
		else
		{
			root.erase(regId);
		}
	}
#ifdef CEMU_DEBUG_ASSERT
	subrange->link_sameVirtualRegister.prev = (raLivenessRange*)1;
	subrange->link_sameVirtualRegister.next = (raLivenessRange*)1;
#endif
}

void PPCRecRARange_removeLink_allSegmentRanges(raLivenessRange** root, raLivenessRange* subrange)
{
	raLivenessRange* tempPrev = subrange->link_allSegmentRanges.prev;
	if (subrange->link_allSegmentRanges.prev)
		subrange->link_allSegmentRanges.prev->link_allSegmentRanges.next = subrange->link_allSegmentRanges.next;
	else
		(*root) = subrange->link_allSegmentRanges.next;
	if (subrange->link_allSegmentRanges.next)
		subrange->link_allSegmentRanges.next->link_allSegmentRanges.prev = tempPrev;
#ifdef CEMU_DEBUG_ASSERT
	subrange->link_allSegmentRanges.prev = (raLivenessRange*)1;
	subrange->link_allSegmentRanges.next = (raLivenessRange*)1;
#endif
}

MemoryPoolPermanentObjects<raLivenessRange> memPool_livenessSubrange(4096);

// startPosition and endPosition are inclusive
raLivenessRange* PPCRecRA_createSubrange2(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, IMLRegID virtualRegister, IMLName name, raInstructionEdge startPosition, raInstructionEdge endPosition)
{
	raLivenessRange* range = memPool_livenessSubrange.acquireObj();
	range->previousRanges.clear();
	range->list_locations.clear();
	range->list_fixedRegRequirements.clear();
	range->imlSegment = imlSegment;

	cemu_assert_debug(startPosition <= endPosition);
	range->interval2.start = startPosition;
	range->interval2.end = endPosition;

	// register mapping
	range->virtualRegister = virtualRegister;
	range->name = name;
	range->physicalRegister = -1;
	// default values
	range->hasStore = false;
	range->hasStoreDelayed = false;
	range->lastIterationIndex = 0;
	range->subrangeBranchNotTaken = nullptr;
	range->subrangeBranchTaken = nullptr;
	cemu_assert_debug(range->previousRanges.empty());
	range->_noLoad = false;
	// add to segment linked lists
	PPCRecRARange_addLink_perVirtualGPR(imlSegment->raInfo.linkedList_perVirtualRegister, range);
	PPCRecRARange_addLink_allSegmentRanges(&imlSegment->raInfo.linkedList_allSubranges, range);
	return range;
}

void _unlinkSubrange(raLivenessRange* subrange)
{
	IMLSegment* imlSegment = subrange->imlSegment;
	PPCRecRARange_removeLink_perVirtualGPR(imlSegment->raInfo.linkedList_perVirtualRegister, subrange);
	PPCRecRARange_removeLink_allSegmentRanges(&imlSegment->raInfo.linkedList_allSubranges, subrange);
	// unlink reverse references
	if(subrange->subrangeBranchTaken)
		subrange->subrangeBranchTaken->previousRanges.erase(std::find(subrange->subrangeBranchTaken->previousRanges.begin(), subrange->subrangeBranchTaken->previousRanges.end(), subrange));
	if(subrange->subrangeBranchNotTaken)
		subrange->subrangeBranchNotTaken->previousRanges.erase(std::find(subrange->subrangeBranchNotTaken->previousRanges.begin(), subrange->subrangeBranchNotTaken->previousRanges.end(), subrange));
	subrange->subrangeBranchTaken = (raLivenessRange*)(uintptr_t)-1;
	subrange->subrangeBranchNotTaken = (raLivenessRange*)(uintptr_t)-1;
	// remove forward references
	for(auto& prev : subrange->previousRanges)
	{
		if(prev->subrangeBranchTaken == subrange)
			prev->subrangeBranchTaken = nullptr;
		if(prev->subrangeBranchNotTaken == subrange)
			prev->subrangeBranchNotTaken = nullptr;
	}
	subrange->previousRanges.clear();
}

void PPCRecRA_deleteSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange)
{
	_unlinkSubrange(subrange);
	//subrange->range->list_subranges.erase(std::find(subrange->range->list_subranges.begin(), subrange->range->list_subranges.end(), subrange));
	subrange->list_locations.clear();

	//PPCRecompilerIml_removeSegmentPoint(&subrange->interval.start);
	//PPCRecompilerIml_removeSegmentPoint(&subrange->interval.end);
	memPool_livenessSubrange.releaseObj(subrange);
}

// leaves range and linked ranges in invalid state. Only use at final clean up when no range is going to be accessed anymore
void _PPCRecRA_deleteSubrangeNoUnlink(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange)
{
	_unlinkSubrange(subrange);
	//PPCRecompilerIml_removeSegmentPoint(&subrange->interval.start);
	//PPCRecompilerIml_removeSegmentPoint(&subrange->interval.end);
	memPool_livenessSubrange.releaseObj(subrange);

// #ifdef CEMU_DEBUG_ASSERT
// 	// DEBUG BEGIN
// 	subrange->lastIterationIndex = 0xFFFFFFFE;
// 	subrange->subrangeBranchTaken = (raLivenessRange*)(uintptr_t)-1;
// 	subrange->subrangeBranchNotTaken = (raLivenessRange*)(uintptr_t)-1;
//
// 	// DEBUG END
// #endif
}

void PPCRecRA_deleteSubrangeCluster(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange)
{
	auto clusterRanges = subrange->GetAllSubrangesInCluster();
	for (auto& subrange : clusterRanges)
	{
		_PPCRecRA_deleteSubrangeNoUnlink(ppcImlGenContext, subrange);
	}
}

void PPCRecRA_deleteAllRanges(ppcImlGenContext_t* ppcImlGenContext)
{
	for(auto& seg : ppcImlGenContext->segmentList2)
	{
		raLivenessRange* cur;
		while(cur = seg->raInfo.linkedList_allSubranges)
		{
			_PPCRecRA_deleteSubrangeNoUnlink(ppcImlGenContext, cur);
		}
		seg->raInfo.linkedList_allSubranges = nullptr;
		seg->raInfo.linkedList_perVirtualRegister.clear();
	}
}

void PPCRecRA_mergeSubranges(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange, raLivenessRange* absorbedSubrange)
{
#ifdef CEMU_DEBUG_ASSERT
	PPCRecRA_debugValidateSubrange(subrange);
	PPCRecRA_debugValidateSubrange(absorbedSubrange);
	if (subrange->imlSegment != absorbedSubrange->imlSegment)
		assert_dbg();
	cemu_assert_debug(subrange->interval2.end == absorbedSubrange->interval2.start);

	if (subrange->subrangeBranchTaken || subrange->subrangeBranchNotTaken)
		assert_dbg();
	if (subrange == absorbedSubrange)
		assert_dbg();
#endif

	// update references
	subrange->subrangeBranchTaken = absorbedSubrange->subrangeBranchTaken;
	subrange->subrangeBranchNotTaken = absorbedSubrange->subrangeBranchNotTaken;
	absorbedSubrange->subrangeBranchTaken = nullptr;
	absorbedSubrange->subrangeBranchNotTaken = nullptr;
	if(subrange->subrangeBranchTaken)
		*std::find(subrange->subrangeBranchTaken->previousRanges.begin(), subrange->subrangeBranchTaken->previousRanges.end(), absorbedSubrange) = subrange;
	if(subrange->subrangeBranchNotTaken)
		*std::find(subrange->subrangeBranchNotTaken->previousRanges.begin(), subrange->subrangeBranchNotTaken->previousRanges.end(), absorbedSubrange) = subrange;

	// merge usage locations
	// at the merge point both ranges might track the same instruction, we handle this by first merging this duplicate location
	if(subrange && absorbedSubrange && !subrange->list_locations.empty() && !absorbedSubrange->list_locations.empty())
	{
		if(subrange->list_locations.back().index == absorbedSubrange->list_locations.front().index)
		{
			subrange->list_locations.back().isRead |= absorbedSubrange->list_locations.front().isRead;
			subrange->list_locations.back().isWrite |= absorbedSubrange->list_locations.front().isWrite;
			absorbedSubrange->list_locations.erase(absorbedSubrange->list_locations.begin()); // inefficient
		}
	}
	for (auto& location : absorbedSubrange->list_locations)
	{
		cemu_assert_debug(subrange->list_locations.empty() || (subrange->list_locations.back().index < location.index)); // todo - sometimes a subrange can contain the same instruction at the merge point if they are covering half of the instruction edge
		subrange->list_locations.push_back(location);
	}
	absorbedSubrange->list_locations.clear();
	// merge fixed reg locations
#ifdef CEMU_DEBUG_ASSERT
	if(!subrange->list_fixedRegRequirements.empty() && !absorbedSubrange->list_fixedRegRequirements.empty())
	{
		cemu_assert_debug(subrange->list_fixedRegRequirements.back().pos < absorbedSubrange->list_fixedRegRequirements.front().pos);
	}
#endif
	for (auto& fixedReg : absorbedSubrange->list_fixedRegRequirements)
	{
		subrange->list_fixedRegRequirements.push_back(fixedReg);
	}

	subrange->interval2.end = absorbedSubrange->interval2.end;

	PPCRecRA_debugValidateSubrange(subrange);

	PPCRecRA_deleteSubrange(ppcImlGenContext, absorbedSubrange);
}

// remove all inter-segment connections from the range cluster and split it into local ranges (also removes empty ranges)
void PPCRecRA_explodeRange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* originRange)
{
	cemu_assert_debug(originRange->interval2.ExtendsPreviousSegment() || originRange->interval2.ExtendsIntoNextSegment()); // only call this on ranges that span multiple segments
	auto clusterRanges = originRange->GetAllSubrangesInCluster();
	for (auto& subrange : clusterRanges)
	{
		if (subrange->list_locations.empty())
			continue;
		raInterval interval;
		interval.SetInterval(subrange->list_locations.front().index, true, subrange->list_locations.back().index, true);
		raLivenessRange* newSubrange = PPCRecRA_createSubrange2(ppcImlGenContext, subrange->imlSegment, subrange->GetVirtualRegister(), subrange->GetName(), interval.start, interval.end);
		// copy locations and fixed reg indices
		newSubrange->list_locations = subrange->list_locations;
		newSubrange->list_fixedRegRequirements = subrange->list_fixedRegRequirements;
		if(originRange->HasPhysicalRegister())
		{
			cemu_assert_debug(subrange->list_fixedRegRequirements.empty()); // avoid unassigning a register from a range with a fixed register requirement
		}
	}
	// remove subranges
	PPCRecRA_deleteSubrangeCluster(ppcImlGenContext, originRange);
}

#ifdef CEMU_DEBUG_ASSERT
void PPCRecRA_debugValidateSubrange(raLivenessRange* range)
{
	// validate subrange
	if (range->subrangeBranchTaken && range->subrangeBranchTaken->imlSegment != range->imlSegment->nextSegmentBranchTaken)
		assert_dbg();
	if (range->subrangeBranchNotTaken && range->subrangeBranchNotTaken->imlSegment != range->imlSegment->nextSegmentBranchNotTaken)
		assert_dbg();

	if(range->subrangeBranchTaken || range->subrangeBranchNotTaken)
	{
		cemu_assert_debug(range->interval2.end.ConnectsToNextSegment());
	}
	if(!range->previousRanges.empty())
	{
		cemu_assert_debug(range->interval2.start.ConnectsToPreviousSegment());
	}
	// validate locations
	if (!range->list_locations.empty())
	{
		cemu_assert_debug(range->list_locations.front().index >= range->interval2.start.GetInstructionIndexEx());
		cemu_assert_debug(range->list_locations.back().index <= range->interval2.end.GetInstructionIndexEx());
	}

}
#else
void PPCRecRA_debugValidateSubrange(raLivenessRange* range) {}
#endif

// since locations are per-instruction, but intervals are per-edge, it's possible that locations track reads/writes outside of the range
// this function will remove any outside read/write locations
void IMLRA_FixLocations(raLivenessRange* range)
{
	if(range->list_locations.empty())
		return;
	if(range->interval2.start.IsInstructionIndex() && range->interval2.start.GetInstructionIndex() == range->list_locations.front().index)
	{
		auto& location = range->list_locations.front();
		if(range->interval2.start.IsOnOutputEdge())
		{
			location.isRead = false;
			if(!location.isRead && !location.isWrite)
				range->list_locations.erase(range->list_locations.begin());
		}
	}
	if(range->list_locations.empty())
		return;
	if(range->interval2.end.IsInstructionIndex() && range->interval2.end.GetInstructionIndex() == range->list_locations.back().index)
	{
		auto& location = range->list_locations.back();
		if(range->interval2.end.IsOnInputEdge())
		{
			location.isWrite = false;
			if(!location.isRead && !location.isWrite)
				range->list_locations.pop_back();
		}
	}
}

// trim start and end of range to match first and last read/write locations
// does not trim start/endpoints which extend into the next/previous segment
void IMLRA_TrimRangeToUse(raLivenessRange* range)
{
	if(range->list_locations.empty())
	{
		// special case where we trim ranges extending from other segments to a single instruction edge
		cemu_assert_debug(!range->interval2.start.IsInstructionIndex() || !range->interval2.end.IsInstructionIndex());
		if(range->interval2.start.IsInstructionIndex())
			range->interval2.start = range->interval2.end;
		if(range->interval2.end.IsInstructionIndex())
			range->interval2.end = range->interval2.start;
		return;
	}
	raInterval prevInterval = range->interval2;
	// trim start
	if(range->interval2.start.IsInstructionIndex())
	{
		bool isInputEdge = range->list_locations.front().isRead;
		range->interval2.start.Set(range->list_locations.front().index, isInputEdge);
	}
	// trim end
	if(range->interval2.end.IsInstructionIndex())
	{
		bool isOutputEdge = range->list_locations.back().isWrite;
		range->interval2.end.Set(range->list_locations.back().index, !isOutputEdge);
	}
	// extra checks
#ifdef CEMU_DEBUG_ASSERT
	cemu_assert_debug(range->interval2.start <= range->interval2.end);
	for(auto& loc : range->list_locations)
	{
		cemu_assert_debug(range->interval2.ContainsInstructionIndex(loc.index));
	}
	cemu_assert_debug(prevInterval.ContainsWholeInterval(range->interval2));
#endif
}

// split range at the given position
// After the split there will be two ranges:
// head -> subrange is shortened to end at splitIndex (exclusive)
// tail -> a new subrange that ranges from splitIndex (inclusive) to the end of the original subrange
// if head has a physical register assigned it will not carry over to tail
// The return value is the tail range
// If trimToHole is true, the end of the head subrange and the start of the tail subrange will be shrunk to fit the read/write locations within them
// the range after the split point does not inherit the physical register
// if trimToHole is true and any of the halfes is empty, it will be deleted
raLivenessRange* PPCRecRA_splitLocalSubrange2(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange*& subrange, raInstructionEdge splitPosition, bool trimToHole)
{
	cemu_assert_debug(splitPosition.IsInstructionIndex());
	cemu_assert_debug(!subrange->interval2.IsNextSegmentOnly() && !subrange->interval2.IsPreviousSegmentOnly());
	cemu_assert_debug(subrange->interval2.ContainsEdge(splitPosition));
	// determine new intervals
	raInterval headInterval, tailInterval;
	headInterval.SetInterval(subrange->interval2.start, splitPosition-1);
	tailInterval.SetInterval(splitPosition, subrange->interval2.end);
	cemu_assert_debug(headInterval.start <= headInterval.end);
	cemu_assert_debug(tailInterval.start <= tailInterval.end);
	// create tail
	raLivenessRange* tailSubrange = PPCRecRA_createSubrange2(ppcImlGenContext, subrange->imlSegment, subrange->GetVirtualRegister(), subrange->GetName(), tailInterval.start, tailInterval.end);
	tailSubrange->SetPhysicalRegister(subrange->GetPhysicalRegister());
	// carry over branch targets and update reverse references
	tailSubrange->subrangeBranchTaken = subrange->subrangeBranchTaken;
	tailSubrange->subrangeBranchNotTaken = subrange->subrangeBranchNotTaken;
	subrange->subrangeBranchTaken = nullptr;
	subrange->subrangeBranchNotTaken = nullptr;
	if(tailSubrange->subrangeBranchTaken)
		*std::find(tailSubrange->subrangeBranchTaken->previousRanges.begin(), tailSubrange->subrangeBranchTaken->previousRanges.end(), subrange) = tailSubrange;
	if(tailSubrange->subrangeBranchNotTaken)
		*std::find(tailSubrange->subrangeBranchNotTaken->previousRanges.begin(), tailSubrange->subrangeBranchNotTaken->previousRanges.end(), subrange) = tailSubrange;
	// we assume that list_locations is ordered by instruction index and contains no duplicate indices, so lets check that here just in case
#ifdef CEMU_DEBUG_ASSERT
	if(!subrange->list_locations.empty())
	{
		sint32 curIdx = -1;
		for(auto& location : subrange->list_locations)
		{
			cemu_assert_debug(curIdx < location.index);
			curIdx = location.index;
		}
	}
#endif
	// split locations
	// since there are 2 edges per instruction and locations track both via a single index, locations on the split point might need to be copied into both ranges
	for (auto& location : subrange->list_locations)
	{
		if(tailInterval.ContainsInstructionIndex(location.index))
			tailSubrange->list_locations.push_back(location);
	}
	// remove tail locations from head
	for (sint32 i = 0; i < subrange->list_locations.size(); i++)
	{
		raLivenessLocation_t* location = subrange->list_locations.data() + i;
		if (!headInterval.ContainsInstructionIndex(location->index))
		{
			subrange->list_locations.resize(i);
			break;
		}
	}
	// split fixed reg requirements
	for (sint32 i = 0; i < subrange->list_fixedRegRequirements.size(); i++)
	{
		raFixedRegRequirement* fixedReg = subrange->list_fixedRegRequirements.data() + i;
		if (tailInterval.ContainsInstructionIndex(fixedReg->pos.GetInstructionIndex()))
		{
			tailSubrange->list_fixedRegRequirements.push_back(*fixedReg);
		}
	}
	// remove tail fixed reg requirements from head
	for (sint32 i = 0; i < subrange->list_fixedRegRequirements.size(); i++)
	{
		raFixedRegRequirement* fixedReg = subrange->list_fixedRegRequirements.data() + i;
		if (!headInterval.ContainsInstructionIndex(fixedReg->pos.GetInstructionIndex()))
		{
			subrange->list_fixedRegRequirements.resize(i);
			break;
		}
	}
	// adjust intervals
	subrange->interval2 = headInterval;
	tailSubrange->interval2 = tailInterval;
	// fix locations to only include read/write edges within the range
	if(subrange)
		IMLRA_FixLocations(subrange);
	if(tailSubrange)
		IMLRA_FixLocations(tailSubrange);
	// trim to hole
	if(trimToHole)
	{
		if(subrange->list_locations.empty() && (subrange->interval2.start.IsInstructionIndex() && subrange->interval2.end.IsInstructionIndex()))
		{
			PPCRecRA_deleteSubrange(ppcImlGenContext, subrange);
			subrange = nullptr;
		}
		else
		{
			IMLRA_TrimRangeToUse(subrange);
		}
		if(tailSubrange->list_locations.empty() && (tailSubrange->interval2.start.IsInstructionIndex() && tailSubrange->interval2.end.IsInstructionIndex()))
		{
			PPCRecRA_deleteSubrange(ppcImlGenContext, tailSubrange);
			tailSubrange = nullptr;
		}
		else
		{
			IMLRA_TrimRangeToUse(tailSubrange);
		}
	}
	// validation
	cemu_assert_debug(!subrange || subrange->interval2.start <= subrange->interval2.end);
	cemu_assert_debug(!tailSubrange || tailSubrange->interval2.start <= tailSubrange->interval2.end);
	cemu_assert_debug(!tailSubrange || tailSubrange->interval2.start >= splitPosition);
	if (!trimToHole)
		cemu_assert_debug(!tailSubrange || tailSubrange->interval2.start == splitPosition);

	if(subrange)
		PPCRecRA_debugValidateSubrange(subrange);
	if(tailSubrange)
		PPCRecRA_debugValidateSubrange(tailSubrange);
	return tailSubrange;
}

void PPCRecRA_updateOrAddSubrangeLocation(raLivenessRange* subrange, sint32 index, bool isRead, bool isWrite)
{
	if (subrange->list_locations.empty())
	{
		subrange->list_locations.emplace_back(index, isRead, isWrite);
		return;
	}
	raLivenessLocation_t* lastLocation = subrange->list_locations.data() + (subrange->list_locations.size() - 1);
	cemu_assert_debug(lastLocation->index <= index);
	if (lastLocation->index == index)
	{
		// update
		lastLocation->isRead = lastLocation->isRead || isRead;
		lastLocation->isWrite = lastLocation->isWrite || isWrite;
		return;
	}
	// add new
	subrange->list_locations.emplace_back(index, isRead, isWrite);
}

sint32 PPCRecRARange_getReadWriteCost(IMLSegment* imlSegment)
{
	sint32 v = imlSegment->loopDepth + 1;
	v *= 5;
	return v*v; // 25, 100, 225, 400
}

// calculate cost of entire range cluster
sint32 PPCRecRARange_estimateTotalCost(std::span<raLivenessRange*> ranges)
{
	sint32 cost = 0;

	// todo - this algorithm isn't accurate. If we have 10 parallel branches with a load each then the actual cost is still only that of one branch (plus minimal extra cost for generating more code).

	// currently we calculate the cost based on the most expensive entry/exit point

	sint32 mostExpensiveRead = 0;
	sint32 mostExpensiveWrite = 0;
	sint32 readCount = 0;
	sint32 writeCount = 0;

	for (auto& subrange : ranges)
	{
		if (!subrange->interval2.ExtendsPreviousSegment())
		{
			//cost += PPCRecRARange_getReadWriteCost(subrange->imlSegment);
			mostExpensiveRead = std::max(mostExpensiveRead, PPCRecRARange_getReadWriteCost(subrange->imlSegment));
			readCount++;
		}
		if (!subrange->interval2.ExtendsIntoNextSegment())
		{
			//cost += PPCRecRARange_getReadWriteCost(subrange->imlSegment);
			mostExpensiveWrite = std::max(mostExpensiveWrite, PPCRecRARange_getReadWriteCost(subrange->imlSegment));
			writeCount++;
		}
	}
	cost = mostExpensiveRead + mostExpensiveWrite;
	cost = cost + (readCount + writeCount) / 10;
	return cost;
}

// calculate cost of range that it would have after calling PPCRecRA_explodeRange() on it
sint32 PPCRecRARange_estimateCostAfterRangeExplode(raLivenessRange* subrange)
{
	auto ranges = subrange->GetAllSubrangesInCluster();
	sint32 cost = -PPCRecRARange_estimateTotalCost(ranges);
	for (auto& subrange : ranges)
	{
		if (subrange->list_locations.empty())
			continue;
		cost += PPCRecRARange_getReadWriteCost(subrange->imlSegment) * 2; // we assume a read and a store
	}
	return cost;
}

sint32 PPCRecRARange_estimateAdditionalCostAfterSplit(raLivenessRange* subrange, raInstructionEdge splitPosition)
{
	// validation
#ifdef CEMU_DEBUG_ASSERT
	if (subrange->interval2.ExtendsIntoNextSegment())
		assert_dbg();
#endif
	cemu_assert_debug(splitPosition.IsInstructionIndex());

	sint32 cost = 0;
	// find split position in location list
	if (subrange->list_locations.empty())
	{
		assert_dbg(); // should not happen?
		return 0;
	}
	sint32 splitInstructionIndex = splitPosition.GetInstructionIndex();
	if (splitInstructionIndex <= subrange->list_locations.front().index)
		return 0;
	if (splitInstructionIndex > subrange->list_locations.back().index)
		return 0;

	// todo - determine exact cost of split subranges

	cost += PPCRecRARange_getReadWriteCost(subrange->imlSegment) * 2; // currently we assume that the additional region will require a read and a store

	return cost;
}