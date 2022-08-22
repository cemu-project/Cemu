#pragma once

raLivenessRange_t* PPCRecRA_createRangeBase(ppcImlGenContext_t* ppcImlGenContext, uint32 virtualRegister, uint32 name);
raLivenessSubrange_t* PPCRecRA_createSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange_t* range, PPCRecImlSegment_t* imlSegment, sint32 startIndex, sint32 endIndex);
void PPCRecRA_deleteSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessSubrange_t* subrange);
void PPCRecRA_deleteRange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange_t* range);
void PPCRecRA_deleteAllRanges(ppcImlGenContext_t* ppcImlGenContext);

void PPCRecRA_mergeRanges(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange_t* range, raLivenessRange_t* absorbedRange);
void PPCRecRA_explodeRange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange_t* range);

void PPCRecRA_mergeSubranges(ppcImlGenContext_t* ppcImlGenContext, raLivenessSubrange_t* subrange, raLivenessSubrange_t* absorbedSubrange);

raLivenessSubrange_t* PPCRecRA_splitLocalSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessSubrange_t* subrange, sint32 splitIndex, bool trimToHole = false);

void PPCRecRA_updateOrAddSubrangeLocation(raLivenessSubrange_t* subrange, sint32 index, bool isRead, bool isWrite);
void PPCRecRA_debugValidateSubrange(raLivenessSubrange_t* subrange);

// cost estimation
sint32 PPCRecRARange_getReadWriteCost(PPCRecImlSegment_t* imlSegment);
sint32 PPCRecRARange_estimateCost(raLivenessRange_t* range);
sint32 PPCRecRARange_estimateAdditionalCostAfterRangeExplode(raLivenessRange_t* range);
sint32 PPCRecRARange_estimateAdditionalCostAfterSplit(raLivenessSubrange_t* subrange, sint32 splitIndex);

// special values to mark the index of ranges that reach across the segment border
#define RA_INTER_RANGE_START	(-1)
#define RA_INTER_RANGE_END		(0x70000000)
