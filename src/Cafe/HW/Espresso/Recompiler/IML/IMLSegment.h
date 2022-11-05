#pragma once
#include "IMLInstruction.h"

#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h" // remove once dependency is gone

struct IMLSegment
{
	sint32 momentaryIndex{}; // index in segment list, generally not kept up to date except if needed (necessary for loop detection)
	sint32 startOffset{}; // offset to first instruction in iml instruction list
	sint32 count{}; // number of instructions in segment
	uint32 ppcAddress{}; // ppc address (0xFFFFFFFF if not associated with an address)
	uint32 x64Offset{}; // x64 code offset of segment start
	uint32 cycleCount{}; // number of PPC cycles required to execute this segment (roughly)
	// list of intermediate instructions in this segment
	std::vector<IMLInstruction> imlList;
	// segment link
	IMLSegment* nextSegmentBranchNotTaken{}; // this is also the default for segments where there is no branch
	IMLSegment* nextSegmentBranchTaken{};
	bool nextSegmentIsUncertain{};
	sint32 loopDepth{};
	std::vector<IMLSegment*> list_prevSegments{};
	// PPC range of segment
	uint32 ppcAddrMin{};
	uint32 ppcAddrMax{};
	// enterable segments
	bool isEnterable{}; // this segment can be entered from outside the recompiler (no preloaded registers necessary)
	uint32 enterPPCAddress{}; // used if isEnterable is true
	// jump destination segments
	bool isJumpDestination{}; // segment is a destination for one or more (conditional) jumps
	uint32 jumpDestinationPPCAddress{};
	// PPC FPR use mask
	bool ppcFPRUsed[32]{}; // same as ppcGPRUsed, but for FPR
	// CR use mask
	uint32 crBitsInput{}; // bits that are expected to be set from the previous segment (read in this segment but not overwritten)
	uint32 crBitsRead{}; // all bits that are read in this segment
	uint32 crBitsWritten{}; // bits that are written in this segment
	// register allocator info
	PPCSegmentRegisterAllocatorInfo_t raInfo{};
	PPCRecVGPRDistances_t raDistances{};
	bool raRangeExtendProcessed{};
	// segment points
	ppcRecompilerSegmentPoint_t* segmentPointList{};

	bool HasSuffixInstruction() const;
};
