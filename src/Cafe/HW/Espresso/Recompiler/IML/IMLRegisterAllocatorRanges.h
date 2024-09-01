#pragma once

struct raLivenessLocation_t
{
	sint32 index;
	bool isRead;
	bool isWrite;

	raLivenessLocation_t() = default;

	raLivenessLocation_t(sint32 index, bool isRead, bool isWrite)
		: index(index), isRead(isRead), isWrite(isWrite) {};
};

struct raLivenessSubrangeLink
{
	struct raLivenessRange* prev;
	struct raLivenessRange* next;
};

struct raLivenessRange
{
	IMLSegment* imlSegment;
	IMLSegmentPoint start;
	IMLSegmentPoint end;
	// dirty state tracking
	bool _noLoad;
	bool hasStore;
	bool hasStoreDelayed;
	// next
	raLivenessRange* subrangeBranchTaken;
	raLivenessRange* subrangeBranchNotTaken;
	// reverse counterpart of BranchTaken/BranchNotTaken
	boost::container::small_vector<raLivenessRange*, 4> previousRanges;
	// processing
	uint32 lastIterationIndex;
	// instruction locations
	std::vector<raLivenessLocation_t> list_locations;
	// linked list (subranges with same GPR virtual register)
	raLivenessSubrangeLink link_sameVirtualRegister;
	// linked list (all subranges for this segment)
	raLivenessSubrangeLink link_allSegmentRanges;
	// register mapping (constant)
	IMLRegID virtualRegister;
	IMLName name;
	// register allocator result
	sint32 physicalRegister;

	boost::container::small_vector<raLivenessRange*, 32> GetAllSubrangesInCluster();

	IMLRegID GetVirtualRegister() const;
	sint32 GetPhysicalRegister() const;
	IMLName GetName() const;
	void SetPhysicalRegister(sint32 physicalRegister);
	void SetPhysicalRegisterForCluster(sint32 physicalRegister);
};

raLivenessRange* PPCRecRA_createSubrange(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, IMLRegID virtualRegister, IMLName name, sint32 startIndex, sint32 endIndex);
void PPCRecRA_deleteSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange);
void PPCRecRA_deleteAllRanges(ppcImlGenContext_t* ppcImlGenContext);

void PPCRecRA_explodeRange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* originRange);

void PPCRecRA_mergeSubranges(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange, raLivenessRange* absorbedSubrange);

raLivenessRange* PPCRecRA_splitLocalSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange, sint32 splitIndex, bool trimToHole = false);

void PPCRecRA_updateOrAddSubrangeLocation(raLivenessRange* subrange, sint32 index, bool isRead, bool isWrite);
void PPCRecRA_debugValidateSubrange(raLivenessRange* subrange);

// cost estimation
sint32 PPCRecRARange_getReadWriteCost(IMLSegment* imlSegment);
sint32 PPCRecRARange_estimateCostAfterRangeExplode(raLivenessRange* subrange);
sint32 PPCRecRARange_estimateAdditionalCostAfterSplit(raLivenessRange* subrange, sint32 splitIndex);

// special values to mark the index of ranges that reach across the segment border
#define RA_INTER_RANGE_START	(-1)
#define RA_INTER_RANGE_END		(0x70000000)
