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
	cemu_assert_suspicious(); // not used yet
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
		subrange->link_sameVirtualRegister.next = it->second;
		it->second = subrange;
		subrange->link_sameVirtualRegister.prev = subrange;
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

raLivenessRange* PPCRecRA_createSubrange(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, IMLRegID virtualRegister, IMLName name, sint32 startIndex, sint32 endIndex)
{
	raLivenessRange* range = memPool_livenessSubrange.acquireObj();
	range->previousRanges.clear();
	range->list_locations.resize(0);
	range->imlSegment = imlSegment;
	PPCRecompilerIml_setSegmentPoint(&range->start, imlSegment, startIndex);
	PPCRecompilerIml_setSegmentPoint(&range->end, imlSegment, endIndex);
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
}

void PPCRecRA_deleteSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange)
{
	_unlinkSubrange(subrange);
	//subrange->range->list_subranges.erase(std::find(subrange->range->list_subranges.begin(), subrange->range->list_subranges.end(), subrange));
	subrange->list_locations.clear();
	// unlink reverse references
	if(subrange->subrangeBranchTaken)
		subrange->subrangeBranchTaken->previousRanges.erase(std::find(subrange->subrangeBranchTaken->previousRanges.begin(), subrange->subrangeBranchTaken->previousRanges.end(), subrange));
	if(subrange->subrangeBranchNotTaken)
		subrange->subrangeBranchTaken->previousRanges.erase(std::find(subrange->subrangeBranchNotTaken->previousRanges.begin(), subrange->subrangeBranchNotTaken->previousRanges.end(), subrange));

	PPCRecompilerIml_removeSegmentPoint(&subrange->start);
	PPCRecompilerIml_removeSegmentPoint(&subrange->end);
	memPool_livenessSubrange.releaseObj(subrange);
}

// leaves range and linked ranges in invalid state. Only use at final clean up when no range is going to be accessed anymore
void _PPCRecRA_deleteSubrangeNoUnlink(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange)
{
	_unlinkSubrange(subrange);
	PPCRecompilerIml_removeSegmentPoint(&subrange->start);
	PPCRecompilerIml_removeSegmentPoint(&subrange->end);
	memPool_livenessSubrange.releaseObj(subrange);
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
	if (subrange->end.index > absorbedSubrange->start.index)
		assert_dbg();
	if (subrange->subrangeBranchTaken || subrange->subrangeBranchNotTaken)
		assert_dbg();
	if (subrange == absorbedSubrange)
		assert_dbg();
#endif

	// update references
	if(absorbedSubrange->subrangeBranchTaken)
		*std::find(absorbedSubrange->subrangeBranchTaken->previousRanges.begin(), absorbedSubrange->subrangeBranchTaken->previousRanges.end(), absorbedSubrange) = subrange;
	if(absorbedSubrange->subrangeBranchNotTaken)
		*std::find(absorbedSubrange->subrangeBranchNotTaken->previousRanges.begin(), absorbedSubrange->subrangeBranchNotTaken->previousRanges.end(), absorbedSubrange) = subrange;
	subrange->subrangeBranchTaken = absorbedSubrange->subrangeBranchTaken;
	subrange->subrangeBranchNotTaken = absorbedSubrange->subrangeBranchNotTaken;

	// merge usage locations
	for (auto& location : absorbedSubrange->list_locations)
	{
		subrange->list_locations.push_back(location);
	}
	absorbedSubrange->list_locations.clear();

	subrange->end.index = absorbedSubrange->end.index;

	PPCRecRA_debugValidateSubrange(subrange);

	PPCRecRA_deleteSubrange(ppcImlGenContext, absorbedSubrange);
}

// remove all inter-segment connections from the range cluster and split it into local ranges (also removes empty ranges)
void PPCRecRA_explodeRange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* originRange)
{
	auto clusterRanges = originRange->GetAllSubrangesInCluster();
	for (auto& subrange : clusterRanges)
	{
		if (subrange->list_locations.empty())
			continue;
		raLivenessRange* newSubrange = PPCRecRA_createSubrange(ppcImlGenContext, subrange->imlSegment, subrange->GetVirtualRegister(), subrange->GetName(), subrange->list_locations.data()[0].index, subrange->list_locations.data()[subrange->list_locations.size() - 1].index + 1);
		// copy locations
		for (auto& location : subrange->list_locations)
		{
			newSubrange->list_locations.push_back(location);
		}
	}
	// remove subranges
	PPCRecRA_deleteSubrangeCluster(ppcImlGenContext, originRange);
}

#ifdef CEMU_DEBUG_ASSERT
void PPCRecRA_debugValidateSubrange(raLivenessRange* subrange)
{
	// validate subrange
	if (subrange->subrangeBranchTaken && subrange->subrangeBranchTaken->imlSegment != subrange->imlSegment->nextSegmentBranchTaken)
		assert_dbg();
	if (subrange->subrangeBranchNotTaken && subrange->subrangeBranchNotTaken->imlSegment != subrange->imlSegment->nextSegmentBranchNotTaken)
		assert_dbg();
}
#else
void PPCRecRA_debugValidateSubrange(raLivenessSubrange_t* subrange) {}
#endif

// split subrange at the given index
// After the split there will be two ranges and subranges:
// head -> subrange is shortened to end at splitIndex (exclusive)
// tail -> a new subrange that ranges from splitIndex (inclusive) to the end of the original subrange
// if head has a physical register assigned it will not carry over to tail
// The return value is the tail subrange
// If trimToHole is true, the end of the head subrange and the start of the tail subrange will be moved to fit the locations
// Ranges that begin at RA_INTER_RANGE_START are allowed and can be split
raLivenessRange* PPCRecRA_splitLocalSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange, sint32 splitIndex, bool trimToHole)
{
	// validation
#ifdef CEMU_DEBUG_ASSERT
	//if (subrange->end.index == RA_INTER_RANGE_END || subrange->end.index == RA_INTER_RANGE_START)
	//	assert_dbg();
	if (subrange->start.index == RA_INTER_RANGE_END || subrange->end.index == RA_INTER_RANGE_START)
		assert_dbg();
	if (subrange->start.index >= splitIndex)
		assert_dbg();
	if (subrange->end.index <= splitIndex)
		assert_dbg();
#endif
	// create tail
	raLivenessRange* tailSubrange = PPCRecRA_createSubrange(ppcImlGenContext, subrange->imlSegment, subrange->GetVirtualRegister(), subrange->GetName(), splitIndex, subrange->end.index);
	// copy locations
	for (auto& location : subrange->list_locations)
	{
		if (location.index >= splitIndex)
			tailSubrange->list_locations.push_back(location);
	}
	// remove tail locations from head
	for (sint32 i = 0; i < subrange->list_locations.size(); i++)
	{
		raLivenessLocation_t* location = subrange->list_locations.data() + i;
		if (location->index >= splitIndex)
		{
			subrange->list_locations.resize(i);
			break;
		}
	}
	// adjust start/end
	if (trimToHole)
	{
		if (subrange->list_locations.empty())
		{
			subrange->end.index = subrange->start.index+1;
		}
		else
		{
			subrange->end.index = subrange->list_locations.back().index + 1;
		}
		if (tailSubrange->list_locations.empty())
		{
			assert_dbg(); // should not happen? (In this case we can just avoid generating a tail at all)
		}
		else
		{
			tailSubrange->start.index = tailSubrange->list_locations.front().index;
		}
	}
	else
	{
		// set head range to end at split index
		subrange->end.index = splitIndex;
	}
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
		if (subrange->start.index != RA_INTER_RANGE_START)
		{
			//cost += PPCRecRARange_getReadWriteCost(subrange->imlSegment);
			mostExpensiveRead = std::max(mostExpensiveRead, PPCRecRARange_getReadWriteCost(subrange->imlSegment));
			readCount++;
		}
		if (subrange->end.index != RA_INTER_RANGE_END)
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

sint32 PPCRecRARange_estimateAdditionalCostAfterSplit(raLivenessRange* subrange, sint32 splitIndex)
{
	// validation
#ifdef CEMU_DEBUG_ASSERT
	if (subrange->end.index == RA_INTER_RANGE_END)
		assert_dbg();
#endif

	sint32 cost = 0;
	// find split position in location list
	if (subrange->list_locations.empty())
	{
		assert_dbg(); // should not happen?
		return 0;
	}
	if (splitIndex <= subrange->list_locations.front().index)
		return 0;
	if (splitIndex > subrange->list_locations.back().index)
		return 0;

	// todo - determine exact cost of split subranges

	cost += PPCRecRARange_getReadWriteCost(subrange->imlSegment) * 2; // currently we assume that the additional region will require a read and a store

	//for (sint32 f = 0; f < subrange->list_locations.size(); f++)
	//{
	//	raLivenessLocation_t* location = subrange->list_locations.data() + f;
	//	if (location->index >= splitIndex)
	//	{
	//		...
	//		return cost;
	//	}
	//}

	return cost;
}

