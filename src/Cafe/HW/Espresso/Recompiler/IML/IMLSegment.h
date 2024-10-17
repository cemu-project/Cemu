#pragma once
#include "IMLInstruction.h"

#include <boost/container/small_vector.hpp>

// special values to mark the index of ranges that reach across the segment border
#define RA_INTER_RANGE_START	(-1)
#define RA_INTER_RANGE_END		(0x70000000)

struct IMLSegmentPoint
{
	friend struct IMLSegmentInterval;

	sint32 index;
	struct IMLSegment* imlSegment; // do we really need to track this? SegmentPoints are always accessed via the segment that they are part of
	IMLSegmentPoint* next;
	IMLSegmentPoint* prev;

	// the index is the instruction index times two.
	// this gives us the ability to cover half an instruction with RA ranges
	// covering only the first half of an instruction (0-0) means that the register is read, but not preserved
	// covering first and the second half means the register is read and preserved
	// covering only the second half means the register is written but not read

	sint32 GetInstructionIndex() const
	{
		return index;
	}

	void SetInstructionIndex(sint32 index)
	{
		this->index = index;
	}

	void ShiftIfAfter(sint32 instructionIndex, sint32 shiftCount)
	{
		if (!IsPreviousSegment() && !IsNextSegment())
		{
			if (GetInstructionIndex() >= instructionIndex)
				index += shiftCount;
		}
	}

	void DecrementByOneInstruction()
	{
		index--;
	}

	// the segment point can point beyond the first and last instruction which indicates that it is an infinite range reaching up to the previous or next segment
	bool IsPreviousSegment() const { return index == RA_INTER_RANGE_START; }
	bool IsNextSegment() const { return index == RA_INTER_RANGE_END; }

	// overload operand > and <
	bool operator>(const IMLSegmentPoint& other) const { return index > other.index; }
	bool operator<(const IMLSegmentPoint& other) const { return index < other.index; }
	bool operator==(const IMLSegmentPoint& other) const { return index == other.index; }
	bool operator!=(const IMLSegmentPoint& other) const { return index != other.index; }

	// overload comparison operands for sint32
	bool operator>(const sint32 other) const { return index > other; }
	bool operator<(const sint32 other) const { return index < other; }
	bool operator<=(const sint32 other) const { return index <= other; }
	bool operator>=(const sint32 other) const { return index >= other; }
};

struct IMLSegmentInterval
{
	IMLSegmentPoint start;
	IMLSegmentPoint end;

	bool ContainsInstructionIndex(sint32 offset) const { return start <= offset && end > offset; }

	bool IsRangeOverlapping(const IMLSegmentInterval& other)
	{
		// todo - compare the raw index
		sint32 r1start = this->start.GetInstructionIndex();
		sint32 r1end = this->end.GetInstructionIndex();
		sint32 r2start = other.start.GetInstructionIndex();
		sint32 r2end = other.end.GetInstructionIndex();
		if (r1start < r2end && r1end > r2start)
			return true;
		if (this->start.IsPreviousSegment() && r1start == r2start)
			return true;
		if (this->end.IsNextSegment() && r1end == r2end)
			return true;
		return false;
	}

	bool ExtendsIntoPreviousSegment() const
	{
		return start.IsPreviousSegment();
	}

	bool ExtendsIntoNextSegment() const
	{
		return end.IsNextSegment();
	}

	bool IsNextSegmentOnly() const
	{
		if(!start.IsNextSegment())
			return false;
		cemu_assert_debug(end.IsNextSegment());
		return true;
	}

	bool IsPreviousSegmentOnly() const
	{
		if (!end.IsPreviousSegment())
			return false;
		cemu_assert_debug(start.IsPreviousSegment());
		return true;
	}

	sint32 GetDistance() const
	{
		// todo - assert if either start or end is outside the segment
		// we may also want to switch this to raw indices?
		return end.GetInstructionIndex() - start.GetInstructionIndex();
	}
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
