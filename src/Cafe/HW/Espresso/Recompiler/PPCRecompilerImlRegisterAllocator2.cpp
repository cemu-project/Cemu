#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "PPCRecompilerX64.h"
#include "PPCRecompilerImlRanges.h"
#include <queue>

bool _isRangeDefined(PPCRecImlSegment_t* imlSegment, sint32 vGPR)
{
	return (imlSegment->raDistances.reg[vGPR].usageStart != INT_MAX);
}

void PPCRecRA_calculateSegmentMinMaxRanges(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment)
{
	for (sint32 i = 0; i < PPC_REC_MAX_VIRTUAL_GPR; i++)
	{
		imlSegment->raDistances.reg[i].usageStart = INT_MAX;
		imlSegment->raDistances.reg[i].usageEnd = INT_MIN;
	}
	// scan instructions for usage range
	sint32 index = 0;
	PPCImlOptimizerUsedRegisters_t gprTracking;
	while (index < imlSegment->imlListCount)
	{
		// end loop at suffix instruction
		if (PPCRecompiler_isSuffixInstruction(imlSegment->imlList + index))
			break;
		// get accessed GPRs
		PPCRecompiler_checkRegisterUsage(NULL, imlSegment->imlList + index, &gprTracking);
		for (sint32 t = 0; t < 4; t++)
		{
			sint32 virtualRegister = gprTracking.gpr[t];
			if (virtualRegister < 0)
				continue;
			cemu_assert_debug(virtualRegister < PPC_REC_MAX_VIRTUAL_GPR);
			imlSegment->raDistances.reg[virtualRegister].usageStart = std::min(imlSegment->raDistances.reg[virtualRegister].usageStart, index); // index before/at instruction
			imlSegment->raDistances.reg[virtualRegister].usageEnd = std::max(imlSegment->raDistances.reg[virtualRegister].usageEnd, index+1); // index after instruction
		}
		// next instruction
		index++;
	}
}

void PPCRecRA_calculateLivenessRangesV2(ppcImlGenContext_t* ppcImlGenContext)
{
	// for each register calculate min/max index of usage range within each segment
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecRA_calculateSegmentMinMaxRanges(ppcImlGenContext, ppcImlGenContext->segmentList[s]);
	}
}

raLivenessSubrange_t* PPCRecRA_convertToMappedRanges(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 vGPR, raLivenessRange_t* range)
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
	// return subrange
	return subrange;
}

void PPCRecRA_createSegmentLivenessRanges(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment)
{
	for (sint32 i = 0; i < PPC_REC_MAX_VIRTUAL_GPR; i++)
	{
		if( _isRangeDefined(imlSegment, i) == false )
			continue;
		if( imlSegment->raDistances.isProcessed[i])
			continue;
		raLivenessRange_t* range = PPCRecRA_createRangeBase(ppcImlGenContext, i, ppcImlGenContext->mappedRegister[i]);
		PPCRecRA_convertToMappedRanges(ppcImlGenContext, imlSegment, i, range);
	}
	// create lookup table of ranges
	raLivenessSubrange_t* vGPR2Subrange[PPC_REC_MAX_VIRTUAL_GPR];
	for (sint32 i = 0; i < PPC_REC_MAX_VIRTUAL_GPR; i++)
	{
		vGPR2Subrange[i] = imlSegment->raInfo.linkedList_perVirtualGPR[i];
#ifdef CEMU_DEBUG_ASSERT
		if (vGPR2Subrange[i] && vGPR2Subrange[i]->link_sameVirtualRegisterGPR.next != nullptr)
			assert_dbg();
#endif
	}
	// parse instructions and convert to locations
	sint32 index = 0;
	PPCImlOptimizerUsedRegisters_t gprTracking;
	while (index < imlSegment->imlListCount)
	{
		// end loop at suffix instruction
		if (PPCRecompiler_isSuffixInstruction(imlSegment->imlList + index))
			break;
		// get accessed GPRs
		PPCRecompiler_checkRegisterUsage(NULL, imlSegment->imlList + index, &gprTracking);
		// handle accessed GPR
		for (sint32 t = 0; t < 4; t++)
		{
			sint32 virtualRegister = gprTracking.gpr[t];
			if (virtualRegister < 0)
				continue;
			bool isWrite = (t == 3);
			// add location
			PPCRecRA_updateOrAddSubrangeLocation(vGPR2Subrange[virtualRegister], index, isWrite == false, isWrite);
#ifdef CEMU_DEBUG_ASSERT
			if (index < vGPR2Subrange[virtualRegister]->start.index)
				assert_dbg();
			if (index+1 > vGPR2Subrange[virtualRegister]->end.index)
				assert_dbg();
#endif
		}
		// next instruction
		index++;
	}
}

void PPCRecRA_extendRangeToEndOfSegment(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 vGPR)
{
	if (_isRangeDefined(imlSegment, vGPR) == false)
	{
		imlSegment->raDistances.reg[vGPR].usageStart = RA_INTER_RANGE_END;
		imlSegment->raDistances.reg[vGPR].usageEnd = RA_INTER_RANGE_END;
		return;
	}
	imlSegment->raDistances.reg[vGPR].usageEnd = RA_INTER_RANGE_END;
}

void PPCRecRA_extendRangeToBeginningOfSegment(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 vGPR)
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

void _PPCRecRA_connectRanges(ppcImlGenContext_t* ppcImlGenContext, sint32 vGPR, PPCRecImlSegment_t** route, sint32 routeDepth)
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
	PPCRecRA_extendRangeToBeginningOfSegment(ppcImlGenContext, route[routeDepth-1], vGPR);
}

void _PPCRecRA_checkAndTryExtendRange(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* currentSegment, sint32 vGPR, sint32 distanceLeft, PPCRecImlSegment_t** route, sint32 routeDepth)
{
	if (routeDepth >= 64)
	{
		cemuLog_logDebug(LogType::Force, "Recompiler RA route maximum depth exceeded for function 0x{:08x}", ppcImlGenContext->functionRef->ppcAddress);
		return;
	}
	route[routeDepth] = currentSegment;
	if (currentSegment->raDistances.reg[vGPR].usageStart == INT_MAX)
	{
		// measure distance to end of segment
		distanceLeft -= currentSegment->imlListCount;
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
			if (distanceLeft < currentSegment->imlListCount)
				return; // range too far away
		}
		else if (currentSegment->raDistances.reg[vGPR].usageStart != RA_INTER_RANGE_START && currentSegment->raDistances.reg[vGPR].usageStart > distanceLeft)
			return; // out of range
		// found close range -> connect ranges
		_PPCRecRA_connectRanges(ppcImlGenContext, vGPR, route, routeDepth + 1);
	}
}

void PPCRecRA_checkAndTryExtendRange(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* currentSegment, sint32 vGPR)
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
		instructionsUntilEndOfSeg = currentSegment->imlListCount - currentSegment->raDistances.reg[vGPR].usageEnd;

#ifdef CEMU_DEBUG_ASSERT
	if (instructionsUntilEndOfSeg < 0)
		assert_dbg();
#endif
	sint32 remainingScanDist = 45 - instructionsUntilEndOfSeg;
	if (remainingScanDist <= 0)
		return; // can't reach end

	// also dont forget: Extending is easier if we allow 'non symetric' branches. E.g. register range one enters one branch
	PPCRecImlSegment_t* route[64];
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

void PPCRecRA_mergeCloseRangesForSegmentV2(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment)
{
	for (sint32 i = 0; i < PPC_REC_MAX_VIRTUAL_GPR; i++) // todo: Use dynamic maximum or list of used vGPRs so we can avoid parsing empty entries
	{
		if(imlSegment->raDistances.reg[i].usageStart == INT_MAX)
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

void PPCRecRA_followFlowAndExtendRanges(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment)
{
	std::vector<PPCRecImlSegment_t*> list_segments;
	list_segments.reserve(1000);
	sint32 index = 0;
	imlSegment->raRangeExtendProcessed = true;
	list_segments.push_back(imlSegment);
	while (index < list_segments.size())
	{
		PPCRecImlSegment_t* currentSegment = list_segments[index];
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
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
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
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
		auto localLoopDepth = imlSegment->loopDepth;
		if( localLoopDepth <= 0 )
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
		if(hasLoopExit == false)
			continue;

		// extend looping ranges into all exits (this allows the data flow analyzer to move stores out of the loop)
		for (sint32 i = 0; i < PPC_REC_MAX_VIRTUAL_GPR; i++) // todo: Use dynamic maximum or list of used vGPRs so we can avoid parsing empty entries
		{
			if (imlSegment->raDistances.reg[i].usageEnd != RA_INTER_RANGE_END)
				continue; // range not set or does not reach end of segment
			if(imlSegment->nextSegmentBranchTaken)
				PPCRecRA_extendRangeToBeginningOfSegment(ppcImlGenContext, imlSegment->nextSegmentBranchTaken, i);
			if(imlSegment->nextSegmentBranchNotTaken)
				PPCRecRA_extendRangeToBeginningOfSegment(ppcImlGenContext, imlSegment->nextSegmentBranchNotTaken, i);
		}
	}
}

void PPCRecRA_processFlowAndCalculateLivenessRangesV2(ppcImlGenContext_t* ppcImlGenContext)
{
	// merge close ranges
	PPCRecRA_mergeCloseRangesV2(ppcImlGenContext);
	// extra pass to move register stores out of loops
	PPCRecRA_extendRangesOutOfLoopsV2(ppcImlGenContext);
	// calculate liveness ranges
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
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

void _analyzeRangeDataFlow(raLivenessSubrange_t* subrange);

void PPCRecRA_analyzeRangeDataFlowV2(ppcImlGenContext_t* ppcImlGenContext)
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
