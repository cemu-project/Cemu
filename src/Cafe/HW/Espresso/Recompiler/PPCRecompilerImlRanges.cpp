#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "PPCRecompilerX64.h"
#include "PPCRecompilerImlRanges.h"
#include "util/helpers/MemoryPool.h"

void PPCRecRARange_addLink_perVirtualGPR(raLivenessSubrange_t** root, raLivenessSubrange_t* subrange)
{
#ifdef CEMU_DEBUG_ASSERT
	if ((*root) && (*root)->range->virtualRegister != subrange->range->virtualRegister)
		assert_dbg();
#endif
	subrange->link_sameVirtualRegisterGPR.next = *root;
	if (*root)
		(*root)->link_sameVirtualRegisterGPR.prev = subrange;
	subrange->link_sameVirtualRegisterGPR.prev = nullptr;
	*root = subrange;
}

void PPCRecRARange_addLink_allSubrangesGPR(raLivenessSubrange_t** root, raLivenessSubrange_t* subrange)
{
	subrange->link_segmentSubrangesGPR.next = *root;
	if (*root)
		(*root)->link_segmentSubrangesGPR.prev = subrange;
	subrange->link_segmentSubrangesGPR.prev = nullptr;
	*root = subrange;
}

void PPCRecRARange_removeLink_perVirtualGPR(raLivenessSubrange_t** root, raLivenessSubrange_t* subrange)
{
	raLivenessSubrange_t* tempPrev = subrange->link_sameVirtualRegisterGPR.prev;
	if (subrange->link_sameVirtualRegisterGPR.prev)
		subrange->link_sameVirtualRegisterGPR.prev->link_sameVirtualRegisterGPR.next = subrange->link_sameVirtualRegisterGPR.next;
	else
		(*root) = subrange->link_sameVirtualRegisterGPR.next;
	if (subrange->link_sameVirtualRegisterGPR.next)
		subrange->link_sameVirtualRegisterGPR.next->link_sameVirtualRegisterGPR.prev = tempPrev;
#ifdef CEMU_DEBUG_ASSERT
	subrange->link_sameVirtualRegisterGPR.prev = (raLivenessSubrange_t*)1;
	subrange->link_sameVirtualRegisterGPR.next = (raLivenessSubrange_t*)1;
#endif
}

void PPCRecRARange_removeLink_allSubrangesGPR(raLivenessSubrange_t** root, raLivenessSubrange_t* subrange)
{
	raLivenessSubrange_t* tempPrev = subrange->link_segmentSubrangesGPR.prev;
	if (subrange->link_segmentSubrangesGPR.prev)
		subrange->link_segmentSubrangesGPR.prev->link_segmentSubrangesGPR.next = subrange->link_segmentSubrangesGPR.next;
	else
		(*root) = subrange->link_segmentSubrangesGPR.next;
	if (subrange->link_segmentSubrangesGPR.next)
		subrange->link_segmentSubrangesGPR.next->link_segmentSubrangesGPR.prev = tempPrev;
#ifdef CEMU_DEBUG_ASSERT
	subrange->link_segmentSubrangesGPR.prev = (raLivenessSubrange_t*)1;
	subrange->link_segmentSubrangesGPR.next = (raLivenessSubrange_t*)1;
#endif
}

MemoryPoolPermanentObjects<raLivenessRange_t> memPool_livenessRange(4096);
MemoryPoolPermanentObjects<raLivenessSubrange_t> memPool_livenessSubrange(4096);

raLivenessRange_t* PPCRecRA_createRangeBase(ppcImlGenContext_t* ppcImlGenContext, uint32 virtualRegister, uint32 name)
{
	raLivenessRange_t* livenessRange = memPool_livenessRange.acquireObj();
	livenessRange->list_subranges.resize(0);
	livenessRange->virtualRegister = virtualRegister;
	livenessRange->name = name;
	livenessRange->physicalRegister = -1;
	ppcImlGenContext->raInfo.list_ranges.push_back(livenessRange);
	return livenessRange;
}

raLivenessSubrange_t* PPCRecRA_createSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange_t* range, PPCRecImlSegment_t* imlSegment, sint32 startIndex, sint32 endIndex)
{
	raLivenessSubrange_t* livenessSubrange = memPool_livenessSubrange.acquireObj();
	livenessSubrange->list_locations.resize(0);
	livenessSubrange->range = range;
	livenessSubrange->imlSegment = imlSegment;
	PPCRecompilerIml_setSegmentPoint(&livenessSubrange->start, imlSegment, startIndex);
	PPCRecompilerIml_setSegmentPoint(&livenessSubrange->end, imlSegment, endIndex);
	// default values
	livenessSubrange->hasStore = false;
	livenessSubrange->hasStoreDelayed = false;
	livenessSubrange->lastIterationIndex = 0;
	livenessSubrange->subrangeBranchNotTaken = nullptr;
	livenessSubrange->subrangeBranchTaken = nullptr;
	livenessSubrange->_noLoad = false;
	// add to range
	range->list_subranges.push_back(livenessSubrange);
	// add to segment
	PPCRecRARange_addLink_perVirtualGPR(&(imlSegment->raInfo.linkedList_perVirtualGPR[range->virtualRegister]), livenessSubrange);
	PPCRecRARange_addLink_allSubrangesGPR(&imlSegment->raInfo.linkedList_allSubranges, livenessSubrange);
	return livenessSubrange;
}

void _unlinkSubrange(raLivenessSubrange_t* subrange)
{
	PPCRecImlSegment_t* imlSegment = subrange->imlSegment;
	PPCRecRARange_removeLink_perVirtualGPR(&imlSegment->raInfo.linkedList_perVirtualGPR[subrange->range->virtualRegister], subrange);
	PPCRecRARange_removeLink_allSubrangesGPR(&imlSegment->raInfo.linkedList_allSubranges, subrange);
}

void PPCRecRA_deleteSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessSubrange_t* subrange)
{
	_unlinkSubrange(subrange);
	subrange->range->list_subranges.erase(std::find(subrange->range->list_subranges.begin(), subrange->range->list_subranges.end(), subrange));
	subrange->list_locations.clear();
	PPCRecompilerIml_removeSegmentPoint(&subrange->start);
	PPCRecompilerIml_removeSegmentPoint(&subrange->end);
	memPool_livenessSubrange.releaseObj(subrange);
}

void _PPCRecRA_deleteSubrangeNoUnlinkFromRange(ppcImlGenContext_t* ppcImlGenContext, raLivenessSubrange_t* subrange)
{
	_unlinkSubrange(subrange);
	PPCRecompilerIml_removeSegmentPoint(&subrange->start);
	PPCRecompilerIml_removeSegmentPoint(&subrange->end);
	memPool_livenessSubrange.releaseObj(subrange);
}

void PPCRecRA_deleteRange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange_t* range)
{
	for (auto& subrange : range->list_subranges)
	{
		_PPCRecRA_deleteSubrangeNoUnlinkFromRange(ppcImlGenContext, subrange);
	}
	ppcImlGenContext->raInfo.list_ranges.erase(std::find(ppcImlGenContext->raInfo.list_ranges.begin(), ppcImlGenContext->raInfo.list_ranges.end(), range));
	memPool_livenessRange.releaseObj(range);
}

void PPCRecRA_deleteRangeNoUnlink(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange_t* range)
{
	for (auto& subrange : range->list_subranges)
	{
		_PPCRecRA_deleteSubrangeNoUnlinkFromRange(ppcImlGenContext, subrange);
	}
	memPool_livenessRange.releaseObj(range);
}

void PPCRecRA_deleteAllRanges(ppcImlGenContext_t* ppcImlGenContext)
{
	for(auto& range : ppcImlGenContext->raInfo.list_ranges)
	{
		PPCRecRA_deleteRangeNoUnlink(ppcImlGenContext, range);
	}
	ppcImlGenContext->raInfo.list_ranges.clear();
}

void PPCRecRA_mergeRanges(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange_t* range, raLivenessRange_t* absorbedRange)
{
	cemu_assert_debug(range != absorbedRange);
	cemu_assert_debug(range->virtualRegister == absorbedRange->virtualRegister);
	// move all subranges from absorbedRange to range
	for (auto& subrange : absorbedRange->list_subranges)
	{
		range->list_subranges.push_back(subrange);
		subrange->range = range;
	}
	absorbedRange->list_subranges.clear();
	PPCRecRA_deleteRange(ppcImlGenContext, absorbedRange);
}

void PPCRecRA_mergeSubranges(ppcImlGenContext_t* ppcImlGenContext, raLivenessSubrange_t* subrange, raLivenessSubrange_t* absorbedSubrange)
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

// remove all inter-segment connections from the range and split it into local ranges (also removes empty ranges)
void PPCRecRA_explodeRange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange_t* range)
{
	if (range->list_subranges.size() == 1)
		assert_dbg();
	for (auto& subrange : range->list_subranges)
	{
		if (subrange->list_locations.empty())
			continue;
		raLivenessRange_t* newRange = PPCRecRA_createRangeBase(ppcImlGenContext, range->virtualRegister, range->name);
		raLivenessSubrange_t* newSubrange = PPCRecRA_createSubrange(ppcImlGenContext, newRange, subrange->imlSegment, subrange->list_locations.data()[0].index, subrange->list_locations.data()[subrange->list_locations.size() - 1].index + 1);
		// copy locations
		for (auto& location : subrange->list_locations)
		{
			newSubrange->list_locations.push_back(location);
		}
	}
	// remove original range
	PPCRecRA_deleteRange(ppcImlGenContext, range);
}

#ifdef CEMU_DEBUG_ASSERT
void PPCRecRA_debugValidateSubrange(raLivenessSubrange_t* subrange)
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
// After the split there will be two ranges/subranges:
// head -> subrange is shortned to end at splitIndex
// tail -> a new subrange that reaches from splitIndex to the end of the original subrange
// if head has a physical register assigned it will not carry over to tail
// The return value is the tail subrange
// If trimToHole is true, the end of the head subrange and the start of the tail subrange will be moved to fit the locations
// Ranges that begin at RA_INTER_RANGE_START are allowed and can be split
raLivenessSubrange_t* PPCRecRA_splitLocalSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessSubrange_t* subrange, sint32 splitIndex, bool trimToHole)
{
	// validation
#ifdef CEMU_DEBUG_ASSERT
	if (subrange->end.index == RA_INTER_RANGE_END || subrange->end.index == RA_INTER_RANGE_START)
		assert_dbg();
	if (subrange->start.index >= splitIndex)
		assert_dbg();
	if (subrange->end.index <= splitIndex)
		assert_dbg();
#endif
	// create tail
	raLivenessRange_t* tailRange = PPCRecRA_createRangeBase(ppcImlGenContext, subrange->range->virtualRegister, subrange->range->name);
	raLivenessSubrange_t* tailSubrange = PPCRecRA_createSubrange(ppcImlGenContext, tailRange, subrange->imlSegment, splitIndex, subrange->end.index);
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
	return tailSubrange;
}

void PPCRecRA_updateOrAddSubrangeLocation(raLivenessSubrange_t* subrange, sint32 index, bool isRead, bool isWrite)
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

sint32 PPCRecRARange_getReadWriteCost(PPCRecImlSegment_t* imlSegment)
{
	sint32 v = imlSegment->loopDepth + 1;
	v *= 5;
	return v*v; // 25, 100, 225, 400
}

// calculate cost of entire range
// ignores data flow and does not detect avoidable reads/stores
sint32 PPCRecRARange_estimateCost(raLivenessRange_t* range)
{
	sint32 cost = 0;

	// todo - this algorithm isn't accurate. If we have 10 parallel branches with a load each then the actual cost is still only that of one branch (plus minimal extra cost for generating more code). 

	// currently we calculate the cost based on the most expensive entry/exit point

	sint32 mostExpensiveRead = 0;
	sint32 mostExpensiveWrite = 0;
	sint32 readCount = 0;
	sint32 writeCount = 0;

	for (auto& subrange : range->list_subranges)
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
sint32 PPCRecRARange_estimateAdditionalCostAfterRangeExplode(raLivenessRange_t* range)
{
	sint32 cost = -PPCRecRARange_estimateCost(range);
	for (auto& subrange : range->list_subranges)
	{
		if (subrange->list_locations.empty())
			continue;
		cost += PPCRecRARange_getReadWriteCost(subrange->imlSegment) * 2; // we assume a read and a store
	}
	return cost;
}

sint32 PPCRecRARange_estimateAdditionalCostAfterSplit(raLivenessSubrange_t* subrange, sint32 splitIndex)
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
