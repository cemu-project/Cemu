#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"
#include "IMLRegisterAllocatorRanges.h"
#include "util/helpers/MemoryPool.h"

uint32 IMLRA_GetNextIterationIndex();

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

void raLivenessRange::SetPhysicalRegister(IMLPhysReg physicalRegister)
{
	this->physicalRegister = physicalRegister;
}

void raLivenessRange::SetPhysicalRegisterForCluster(IMLPhysReg physicalRegister)
{
	auto clusterRanges = GetAllSubrangesInCluster();
	for(auto& range : clusterRanges)
		range->physicalRegister = physicalRegister;
}

boost::container::small_vector<raLivenessRange*, 128> raLivenessRange::GetAllSubrangesInCluster()
{
	uint32 iterationIndex = IMLRA_GetNextIterationIndex();
	boost::container::small_vector<raLivenessRange*, 128> subranges;
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

void raLivenessRange::GetAllowedRegistersExRecursive(raLivenessRange* range, uint32 iterationIndex, IMLPhysRegisterSet& allowedRegs)
{
	range->lastIterationIndex = iterationIndex;
	for (auto& it : range->list_fixedRegRequirements)
		allowedRegs &= it.allowedReg;
	// check successors
	if (range->subrangeBranchTaken && range->subrangeBranchTaken->lastIterationIndex != iterationIndex)
		GetAllowedRegistersExRecursive(range->subrangeBranchTaken, iterationIndex, allowedRegs);
	if (range->subrangeBranchNotTaken && range->subrangeBranchNotTaken->lastIterationIndex != iterationIndex)
		GetAllowedRegistersExRecursive(range->subrangeBranchNotTaken, iterationIndex, allowedRegs);
	// check predecessors
	for (auto& prev : range->previousRanges)
	{
		if (prev->lastIterationIndex != iterationIndex)
			GetAllowedRegistersExRecursive(prev, iterationIndex, allowedRegs);
	}
};

bool raLivenessRange::GetAllowedRegistersEx(IMLPhysRegisterSet& allowedRegisters)
{
	uint32 iterationIndex = IMLRA_GetNextIterationIndex();
	allowedRegisters.SetAllAvailable();
	GetAllowedRegistersExRecursive(this, iterationIndex, allowedRegisters);
	return !allowedRegisters.HasAllAvailable();
}

IMLPhysRegisterSet raLivenessRange::GetAllowedRegisters(IMLPhysRegisterSet regPool)
{
	IMLPhysRegisterSet fixedRegRequirements = regPool;
	if(interval.ExtendsPreviousSegment() || interval.ExtendsIntoNextSegment())
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
#ifdef CEMU_DEBUG_ASSERT
	raLivenessRange* cur = root.find(subrange->GetVirtualRegister())->second;
	bool hasRangeFound = false;
	while(cur)
	{
		if(cur == subrange)
		{
			hasRangeFound = true;
			break;
		}
		cur = cur->link_sameVirtualRegister.next;
	}
	cemu_assert_debug(hasRangeFound);
#endif
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
			cemu_assert_debug(root.find(regId)->second == subrange);
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
raLivenessRange* IMLRA_CreateRange(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, IMLRegID virtualRegister, IMLName name, raInstructionEdge startPosition, raInstructionEdge endPosition)
{
	raLivenessRange* range = memPool_livenessSubrange.acquireObj();
	range->previousRanges.clear();
	range->list_accessLocations.clear();
	range->list_fixedRegRequirements.clear();
	range->imlSegment = imlSegment;

	cemu_assert_debug(startPosition <= endPosition);
	range->interval.start = startPosition;
	range->interval.end = endPosition;

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

void _unlinkSubrange(raLivenessRange* range)
{
	IMLSegment* imlSegment = range->imlSegment;
	PPCRecRARange_removeLink_perVirtualGPR(imlSegment->raInfo.linkedList_perVirtualRegister, range);
	PPCRecRARange_removeLink_allSegmentRanges(&imlSegment->raInfo.linkedList_allSubranges, range);
	// unlink reverse references
	if(range->subrangeBranchTaken)
		range->subrangeBranchTaken->previousRanges.erase(std::find(range->subrangeBranchTaken->previousRanges.begin(), range->subrangeBranchTaken->previousRanges.end(), range));
	if(range->subrangeBranchNotTaken)
		range->subrangeBranchNotTaken->previousRanges.erase(std::find(range->subrangeBranchNotTaken->previousRanges.begin(), range->subrangeBranchNotTaken->previousRanges.end(), range));
	range->subrangeBranchTaken = (raLivenessRange*)(uintptr_t)-1;
	range->subrangeBranchNotTaken = (raLivenessRange*)(uintptr_t)-1;
	// remove forward references
	for(auto& prev : range->previousRanges)
	{
		if(prev->subrangeBranchTaken == range)
			prev->subrangeBranchTaken = nullptr;
		if(prev->subrangeBranchNotTaken == range)
			prev->subrangeBranchNotTaken = nullptr;
	}
	range->previousRanges.clear();
}

void IMLRA_DeleteRange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* range)
{
	_unlinkSubrange(range);
	range->list_accessLocations.clear();
	range->list_fixedRegRequirements.clear();
	memPool_livenessSubrange.releaseObj(range);
}

void IMLRA_DeleteRangeCluster(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* range)
{
	auto clusterRanges = range->GetAllSubrangesInCluster();
	for (auto& subrange : clusterRanges)
		IMLRA_DeleteRange(ppcImlGenContext, subrange);
}

void IMLRA_DeleteAllRanges(ppcImlGenContext_t* ppcImlGenContext)
{
	for(auto& seg : ppcImlGenContext->segmentList2)
	{
		raLivenessRange* cur;
		while(cur = seg->raInfo.linkedList_allSubranges)
			IMLRA_DeleteRange(ppcImlGenContext, cur);
		seg->raInfo.linkedList_allSubranges = nullptr;
		seg->raInfo.linkedList_perVirtualRegister.clear();
	}
}

void IMLRA_MergeSubranges(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange, raLivenessRange* absorbedSubrange)
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
	for (auto& accessLoc : absorbedSubrange->list_accessLocations)
		subrange->list_accessLocations.push_back(accessLoc);
	absorbedSubrange->list_accessLocations.clear();
	// merge fixed reg locations
#ifdef CEMU_DEBUG_ASSERT
	if(!subrange->list_fixedRegRequirements.empty() && !absorbedSubrange->list_fixedRegRequirements.empty())
	{
		cemu_assert_debug(subrange->list_fixedRegRequirements.back().pos < absorbedSubrange->list_fixedRegRequirements.front().pos);
	}
#endif
	for (auto& fixedReg : absorbedSubrange->list_fixedRegRequirements)
		subrange->list_fixedRegRequirements.push_back(fixedReg);
	absorbedSubrange->list_fixedRegRequirements.clear();

	subrange->interval.end = absorbedSubrange->interval.end;

	PPCRecRA_debugValidateSubrange(subrange);

	IMLRA_DeleteRange(ppcImlGenContext, absorbedSubrange);
}

// remove all inter-segment connections from the range cluster and split it into local ranges. Ranges are trimmed and if they have no access location they will be removed
void IMLRA_ExplodeRangeCluster(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* originRange)
{
	cemu_assert_debug(originRange->interval.ExtendsPreviousSegment() || originRange->interval.ExtendsIntoNextSegment()); // only call this on ranges that span multiple segments
	auto clusterRanges = originRange->GetAllSubrangesInCluster();
	for (auto& subrange : clusterRanges)
	{
		if (subrange->list_accessLocations.empty())
			continue;
		raInterval interval;
		interval.SetInterval(subrange->list_accessLocations.front().pos, subrange->list_accessLocations.back().pos);
		raLivenessRange* newSubrange = IMLRA_CreateRange(ppcImlGenContext, subrange->imlSegment, subrange->GetVirtualRegister(), subrange->GetName(), interval.start, interval.end);
		// copy locations and fixed reg indices
		newSubrange->list_accessLocations = subrange->list_accessLocations;
		newSubrange->list_fixedRegRequirements = subrange->list_fixedRegRequirements;
		if(originRange->HasPhysicalRegister())
		{
			cemu_assert_debug(subrange->list_fixedRegRequirements.empty()); // avoid unassigning a register from a range with a fixed register requirement
		}
		// validate
		if(!newSubrange->list_accessLocations.empty())
		{
			cemu_assert_debug(newSubrange->list_accessLocations.front().pos >= newSubrange->interval.start);
			cemu_assert_debug(newSubrange->list_accessLocations.back().pos <= newSubrange->interval.end);
		}
		if(!newSubrange->list_fixedRegRequirements.empty())
		{
			cemu_assert_debug(newSubrange->list_fixedRegRequirements.front().pos >= newSubrange->interval.start); // fixed register requirements outside of the actual access range probably means there is a mistake in GetInstructionFixedRegisters()
			cemu_assert_debug(newSubrange->list_fixedRegRequirements.back().pos <= newSubrange->interval.end);
		}
	}
	// delete the original range cluster
	IMLRA_DeleteRangeCluster(ppcImlGenContext, originRange);
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
	if (!range->list_accessLocations.empty())
	{
		cemu_assert_debug(range->list_accessLocations.front().pos >= range->interval2.start);
		cemu_assert_debug(range->list_accessLocations.back().pos <= range->interval2.end);
	}
	// validate fixed reg requirements
	if (!range->list_fixedRegRequirements.empty())
	{
		cemu_assert_debug(range->list_fixedRegRequirements.front().pos >= range->interval2.start);
		cemu_assert_debug(range->list_fixedRegRequirements.back().pos <= range->interval2.end);
		for(sint32 i = 0; i < (sint32)range->list_fixedRegRequirements.size()-1; i++)
			cemu_assert_debug(range->list_fixedRegRequirements[i].pos < range->list_fixedRegRequirements[i+1].pos);
	}

}
#else
void PPCRecRA_debugValidateSubrange(raLivenessRange* range) {}
#endif

// trim start and end of range to match first and last read/write locations
// does not trim start/endpoints which extend into the next/previous segment
void IMLRA_TrimRangeToUse(raLivenessRange* range)
{
	if(range->list_accessLocations.empty())
	{
		// special case where we trim ranges extending from other segments to a single instruction edge
		cemu_assert_debug(!range->interval.start.IsInstructionIndex() || !range->interval.end.IsInstructionIndex());
		if(range->interval.start.IsInstructionIndex())
			range->interval.start = range->interval.end;
		if(range->interval.end.IsInstructionIndex())
			range->interval.end = range->interval.start;
		return;
	}
	// trim start and end
	raInterval prevInterval = range->interval;
	if(range->interval.start.IsInstructionIndex())
		range->interval.start = range->list_accessLocations.front().pos;
	if(range->interval.end.IsInstructionIndex())
		range->interval.end = range->list_accessLocations.back().pos;
	// extra checks
#ifdef CEMU_DEBUG_ASSERT
	cemu_assert_debug(range->interval2.start <= range->interval2.end);
	for(auto& loc : range->list_accessLocations)
	{
		cemu_assert_debug(range->interval2.ContainsEdge(loc.pos));
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
// If trimToUsage is true, the end of the head subrange and the start of the tail subrange will be shrunk to fit the read/write locations within. If there are no locations then the range will be deleted
raLivenessRange* IMLRA_SplitRange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange*& subrange, raInstructionEdge splitPosition, bool trimToUsage)
{
	cemu_assert_debug(splitPosition.IsInstructionIndex());
	cemu_assert_debug(!subrange->interval.IsNextSegmentOnly() && !subrange->interval.IsPreviousSegmentOnly());
	cemu_assert_debug(subrange->interval.ContainsEdge(splitPosition));
	// determine new intervals
	raInterval headInterval, tailInterval;
	headInterval.SetInterval(subrange->interval.start, splitPosition-1);
	tailInterval.SetInterval(splitPosition, subrange->interval.end);
	cemu_assert_debug(headInterval.start <= headInterval.end);
	cemu_assert_debug(tailInterval.start <= tailInterval.end);
	// create tail
	raLivenessRange* tailSubrange = IMLRA_CreateRange(ppcImlGenContext, subrange->imlSegment, subrange->GetVirtualRegister(), subrange->GetName(), tailInterval.start, tailInterval.end);
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
	if(subrange->list_accessLocations.size() > 1)
	{
		for(size_t i=0; i<subrange->list_accessLocations.size()-1; i++)
		{
			cemu_assert_debug(subrange->list_accessLocations[i].pos < subrange->list_accessLocations[i+1].pos);
		}
	}
#endif
	// split locations
	auto it = std::lower_bound(
		subrange->list_accessLocations.begin(), subrange->list_accessLocations.end(), splitPosition,
		[](const raAccessLocation& accessLoc, raInstructionEdge value) { return accessLoc.pos < value; }
	);
	size_t originalCount = subrange->list_accessLocations.size();
	tailSubrange->list_accessLocations.insert(tailSubrange->list_accessLocations.end(), it, subrange->list_accessLocations.end());
	subrange->list_accessLocations.erase(it, subrange->list_accessLocations.end());
	cemu_assert_debug(subrange->list_accessLocations.empty() || subrange->list_accessLocations.back().pos < splitPosition);
	cemu_assert_debug(tailSubrange->list_accessLocations.empty() || tailSubrange->list_accessLocations.front().pos >= splitPosition);
	cemu_assert_debug(subrange->list_accessLocations.size() + tailSubrange->list_accessLocations.size() == originalCount);
	// split fixed reg requirements
	for (sint32 i = 0; i < subrange->list_fixedRegRequirements.size(); i++)
	{
		raFixedRegRequirement* fixedReg = subrange->list_fixedRegRequirements.data() + i;
		if (tailInterval.ContainsEdge(fixedReg->pos))
		{
			tailSubrange->list_fixedRegRequirements.push_back(*fixedReg);
		}
	}
	// remove tail fixed reg requirements from head
	for (sint32 i = 0; i < subrange->list_fixedRegRequirements.size(); i++)
	{
		raFixedRegRequirement* fixedReg = subrange->list_fixedRegRequirements.data() + i;
		if (!headInterval.ContainsEdge(fixedReg->pos))
		{
			subrange->list_fixedRegRequirements.resize(i);
			break;
		}
	}
	// adjust intervals
	subrange->interval = headInterval;
	tailSubrange->interval = tailInterval;
	// trim to hole
	if(trimToUsage)
	{
		if(subrange->list_accessLocations.empty() && (subrange->interval.start.IsInstructionIndex() && subrange->interval.end.IsInstructionIndex()))
		{
			IMLRA_DeleteRange(ppcImlGenContext, subrange);
			subrange = nullptr;
		}
		else
		{
			IMLRA_TrimRangeToUse(subrange);
		}
		if(tailSubrange->list_accessLocations.empty() && (tailSubrange->interval.start.IsInstructionIndex() && tailSubrange->interval.end.IsInstructionIndex()))
		{
			IMLRA_DeleteRange(ppcImlGenContext, tailSubrange);
			tailSubrange = nullptr;
		}
		else
		{
			IMLRA_TrimRangeToUse(tailSubrange);
		}
	}
	// validation
	cemu_assert_debug(!subrange || subrange->interval.start <= subrange->interval.end);
	cemu_assert_debug(!tailSubrange || tailSubrange->interval.start <= tailSubrange->interval.end);
	cemu_assert_debug(!tailSubrange || tailSubrange->interval.start >= splitPosition);
	if (!trimToUsage)
		cemu_assert_debug(!tailSubrange || tailSubrange->interval.start == splitPosition);

	if(subrange)
		PPCRecRA_debugValidateSubrange(subrange);
	if(tailSubrange)
		PPCRecRA_debugValidateSubrange(tailSubrange);
	return tailSubrange;
}

sint32 IMLRA_GetSegmentReadWriteCost(IMLSegment* imlSegment)
{
	sint32 v = imlSegment->loopDepth + 1;
	v *= 5;
	return v*v; // 25, 100, 225, 400
}

// calculate additional cost of range that it would have after calling _ExplodeRange() on it
sint32 IMLRA_CalculateAdditionalCostOfRangeExplode(raLivenessRange* subrange)
{
	auto ranges = subrange->GetAllSubrangesInCluster();
	sint32 cost = 0;//-PPCRecRARange_estimateTotalCost(ranges);
	for (auto& subrange : ranges)
	{
		if (subrange->list_accessLocations.empty())
			continue; // this range would be deleted and thus has no cost
		sint32 segmentLoadStoreCost = IMLRA_GetSegmentReadWriteCost(subrange->imlSegment);
		bool hasAdditionalLoad = subrange->interval.ExtendsPreviousSegment();
		bool hasAdditionalStore = subrange->interval.ExtendsIntoNextSegment();
		if(hasAdditionalLoad && subrange->list_accessLocations.front().IsWrite()) // if written before read then a load isn't necessary
		{
			cemu_assert_debug(!subrange->list_accessLocations.front().IsRead());
			cost += segmentLoadStoreCost;
		}
		if(hasAdditionalStore)
		{
			bool hasWrite = std::find_if(subrange->list_accessLocations.begin(), subrange->list_accessLocations.end(), [](const raAccessLocation& loc) { return loc.IsWrite(); }) != subrange->list_accessLocations.end();
			if(!hasWrite) // ranges which don't modify their value do not need to be stored
				cost += segmentLoadStoreCost;
		}
	}
	// todo - properly calculating all the data-flow dependency based costs is more complex so this currently is an approximation
	return cost;
}

sint32 IMLRA_CalculateAdditionalCostAfterSplit(raLivenessRange* subrange, raInstructionEdge splitPosition)
{
	// validation
#ifdef CEMU_DEBUG_ASSERT
	if (subrange->interval2.ExtendsIntoNextSegment())
		assert_dbg();
#endif
	cemu_assert_debug(splitPosition.IsInstructionIndex());

	sint32 cost = 0;
	// find split position in location list
	if (subrange->list_accessLocations.empty())
		return 0;
	if (splitPosition <= subrange->list_accessLocations.front().pos)
		return 0;
	if (splitPosition > subrange->list_accessLocations.back().pos)
		return 0;

	size_t firstTailLocationIndex = 0;
	for (size_t i = 0; i < subrange->list_accessLocations.size(); i++)
	{
		if (subrange->list_accessLocations[i].pos >= splitPosition)
		{
			firstTailLocationIndex = i;
			break;
		}
	}
	std::span<raAccessLocation> headLocations{subrange->list_accessLocations.data(), firstTailLocationIndex};
	std::span<raAccessLocation> tailLocations{subrange->list_accessLocations.data() + firstTailLocationIndex, subrange->list_accessLocations.size() - firstTailLocationIndex};
	cemu_assert_debug(headLocations.empty() || headLocations.back().pos < splitPosition);
	cemu_assert_debug(tailLocations.empty() || tailLocations.front().pos >= splitPosition);

	sint32 segmentLoadStoreCost = IMLRA_GetSegmentReadWriteCost(subrange->imlSegment);

	auto CalculateCostFromLocationRange = [segmentLoadStoreCost](std::span<raAccessLocation> locations, bool trackLoadCost = true, bool trackStoreCost = true) -> sint32
	{
		if(locations.empty())
			return 0;
		sint32 cost = 0;
		if(locations.front().IsRead() && trackLoadCost)
			cost += segmentLoadStoreCost; // not overwritten, so there is a load cost
		bool hasWrite = std::find_if(locations.begin(), locations.end(), [](const raAccessLocation& loc) { return loc.IsWrite(); }) != locations.end();
		if(hasWrite && trackStoreCost)
			cost += segmentLoadStoreCost; // modified, so there is a store cost
		return cost;
	};

	sint32 baseCost = CalculateCostFromLocationRange(subrange->list_accessLocations);

	bool tailOverwritesValue = !tailLocations.empty() && !tailLocations.front().IsRead() && tailLocations.front().IsWrite();

	sint32 newCost = CalculateCostFromLocationRange(headLocations) + CalculateCostFromLocationRange(tailLocations, !tailOverwritesValue, true);
	cemu_assert_debug(newCost >= baseCost);
	cost = newCost - baseCost;

	return cost;
}