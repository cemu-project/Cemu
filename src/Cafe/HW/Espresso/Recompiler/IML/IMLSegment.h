#pragma once
#include "IMLInstruction.h"

#define IML_RA_VIRT_REG_COUNT_MAX	40 // should match PPC_REC_MAX_VIRTUAL_GPR -> todo: Make this dynamic

struct IMLSegmentPoint
{
	sint32 index;
	struct IMLSegment* imlSegment;
	IMLSegmentPoint* next;
	IMLSegmentPoint* prev;
};

struct raLivenessLocation_t
{
	sint32 index;
	bool isRead;
	bool isWrite;

	raLivenessLocation_t() = default;

	raLivenessLocation_t(sint32 index, bool isRead, bool isWrite)
		: index(index), isRead(isRead), isWrite(isWrite) {};
};

struct raLivenessSubrangeLink_t
{
	struct raLivenessSubrange_t* prev;
	struct raLivenessSubrange_t* next;
};

struct raLivenessSubrange_t
{
	struct raLivenessRange_t* range;
	IMLSegment* imlSegment;
	IMLSegmentPoint start;
	IMLSegmentPoint end;
	// dirty state tracking
	bool _noLoad;
	bool hasStore;
	bool hasStoreDelayed;
	// next
	raLivenessSubrange_t* subrangeBranchTaken;
	raLivenessSubrange_t* subrangeBranchNotTaken;
	// processing
	uint32 lastIterationIndex;
	// instruction locations
	std::vector<raLivenessLocation_t> list_locations;
	// linked list (subranges with same GPR virtual register)
	raLivenessSubrangeLink_t link_sameVirtualRegisterGPR;
	// linked list (all subranges for this segment)
	raLivenessSubrangeLink_t link_segmentSubrangesGPR;
};

struct raLivenessRange_t
{
	sint32 virtualRegister;
	sint32 physicalRegister;
	sint32 name;
	std::vector<raLivenessSubrange_t*> list_subranges;
};

struct PPCSegmentRegisterAllocatorInfo_t
{
	// analyzer stage
	bool isPartOfProcessedLoop{}; // used during loop detection
	sint32 lastIterationIndex{};
	// linked lists
	raLivenessSubrange_t* linkedList_allSubranges{};
	raLivenessSubrange_t* linkedList_perVirtualGPR[IML_RA_VIRT_REG_COUNT_MAX]{};
};

struct PPCRecVGPRDistances_t
{
	struct _RegArrayEntry
	{
		sint32 usageStart{};
		sint32 usageEnd{};
	}reg[IML_RA_VIRT_REG_COUNT_MAX];
	bool isProcessed[IML_RA_VIRT_REG_COUNT_MAX]{};
};

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
	//bool isJumpDestination{}; // segment is a destination for one or more (conditional) jumps
	//uint32 jumpDestinationPPCAddress{};
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
