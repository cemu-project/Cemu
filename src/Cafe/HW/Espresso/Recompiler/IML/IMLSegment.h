#pragma once
#include "IMLInstruction.h"

#include <boost/container/small_vector.hpp>

struct IMLSegmentPoint
{
	sint32 index;
	struct IMLSegment* imlSegment;
	IMLSegmentPoint* next;
	IMLSegmentPoint* prev;
};

struct PPCSegmentRegisterAllocatorInfo_t
{
	// used during loop detection
	bool isPartOfProcessedLoop{}; 
	sint32 lastIterationIndex{};
	// linked lists
	struct raLivenessRange* linkedList_allSubranges{};
	std::unordered_map<IMLRegID, struct raLivenessRange*> linkedList_perVirtualRegister;
};

struct IMLSegment
{
	sint32 momentaryIndex{}; // index in segment list, generally not kept up to date except if needed (necessary for loop detection)
	sint32 loopDepth{};
	uint32 ppcAddress{}; // ppc address (0xFFFFFFFF if not associated with an address)
	uint32 x64Offset{}; // x64 code offset of segment start
	// list of intermediate instructions in this segment
	std::vector<IMLInstruction> imlList;
	// segment link
	IMLSegment* nextSegmentBranchNotTaken{}; // this is also the default for segments where there is no branch
	IMLSegment* nextSegmentBranchTaken{};
	bool nextSegmentIsUncertain{};
	std::vector<IMLSegment*> list_prevSegments{};
	// source for overwrite analysis (if nextSegmentIsUncertain is true)
	// sometimes a segment is marked as an exit point, but for the purposes of dead code elimination we know the next segment
	IMLSegment* deadCodeEliminationHintSeg{};
	std::vector<IMLSegment*> list_deadCodeHintBy{};
	// enterable segments
	bool isEnterable{}; // this segment can be entered from outside the recompiler (no preloaded registers necessary)
	uint32 enterPPCAddress{}; // used if isEnterable is true
	// register allocator info
	PPCSegmentRegisterAllocatorInfo_t raInfo{};
	// segment state API
	void SetEnterable(uint32 enterAddress);
	void SetLinkBranchNotTaken(IMLSegment* imlSegmentDst);
	void SetLinkBranchTaken(IMLSegment* imlSegmentDst);

	IMLSegment* GetBranchTaken()
	{
		return nextSegmentBranchTaken;
	}

	IMLSegment* GetBranchNotTaken()
	{
		return nextSegmentBranchNotTaken;
	}

	void SetNextSegmentForOverwriteHints(IMLSegment* seg)
	{
		cemu_assert_debug(!deadCodeEliminationHintSeg);
		deadCodeEliminationHintSeg = seg;
		if (seg)
			seg->list_deadCodeHintBy.push_back(this);
	}

	// instruction API
	IMLInstruction* AppendInstruction();

	bool HasSuffixInstruction() const;
	sint32 GetSuffixInstructionIndex() const;
	IMLInstruction* GetLastInstruction();

	// segment points
	IMLSegmentPoint* segmentPointList{};
};


void IMLSegment_SetLinkBranchNotTaken(IMLSegment* imlSegmentSrc, IMLSegment* imlSegmentDst);
void IMLSegment_SetLinkBranchTaken(IMLSegment* imlSegmentSrc, IMLSegment* imlSegmentDst);
void IMLSegment_RelinkInputSegment(IMLSegment* imlSegmentOrig, IMLSegment* imlSegmentNew);
void IMLSegment_RemoveLink(IMLSegment* imlSegmentSrc, IMLSegment* imlSegmentDst);
