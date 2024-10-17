#pragma once
#include "IMLRegisterAllocator.h"

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

struct raInstructionEdge
{
	friend struct raInterval;
public:
	raInstructionEdge()
	{
		index = 0;
	}

	raInstructionEdge(sint32 instructionIndex, bool isInputEdge)
	{
		Set(instructionIndex, isInputEdge);
	}

	void Set(sint32 instructionIndex, bool isInputEdge)
	{
		if(instructionIndex == RA_INTER_RANGE_START || instructionIndex == RA_INTER_RANGE_END)
		{
			index = instructionIndex;
			return;
		}
		index = instructionIndex * 2 + (isInputEdge ? 0 : 1);
		cemu_assert_debug(index >= 0 && index < 0x100000*2); // make sure index value is sane
	}

	void SetRaw(sint32 index)
	{
		this->index = index;
		cemu_assert_debug(index == RA_INTER_RANGE_START || index == RA_INTER_RANGE_END || (index >= 0 && index < 0x100000*2)); // make sure index value is sane
	}

	// sint32 GetRaw()
	// {
	// 	this->index = index;
	// }

	std::string GetDebugString()
	{
		if(index == RA_INTER_RANGE_START)
			return "RA_START";
		else if(index == RA_INTER_RANGE_END)
			return "RA_END";
		std::string str = fmt::format("{}", GetInstructionIndex());
		if(IsOnInputEdge())
			str += "i";
		else if(IsOnOutputEdge())
			str += "o";
		return str;
	}

	sint32 GetInstructionIndex() const
	{
		cemu_assert_debug(index != RA_INTER_RANGE_START && index != RA_INTER_RANGE_END);
		return index >> 1;
	}

	// returns instruction index or RA_INTER_RANGE_START/RA_INTER_RANGE_END
	sint32 GetInstructionIndexEx() const
	{
		if(index == RA_INTER_RANGE_START || index == RA_INTER_RANGE_END)
			return index;
		return index >> 1;
	}

	sint32 GetRaw() const
	{
		return index;
	}

	bool IsOnInputEdge() const
	{
		cemu_assert_debug(index != RA_INTER_RANGE_START && index != RA_INTER_RANGE_END);
		return (index&1) == 0;
	}

	bool IsOnOutputEdge() const
	{
		cemu_assert_debug(index != RA_INTER_RANGE_START && index != RA_INTER_RANGE_END);
		return (index&1) != 0;
	}

	bool ConnectsToPreviousSegment() const
	{
		return index == RA_INTER_RANGE_START;
	}

	bool ConnectsToNextSegment() const
	{
		return index == RA_INTER_RANGE_END;
	}

	bool IsInstructionIndex() const
	{
		return index != RA_INTER_RANGE_START && index != RA_INTER_RANGE_END;
	}

	// comparison operators
	bool operator>(const raInstructionEdge& other) const
	{
		return index > other.index;
	}
	bool operator<(const raInstructionEdge& other) const
	{
		return index < other.index;
	}
	bool operator<=(const raInstructionEdge& other) const
	{
		return index <= other.index;
	}
	bool operator>=(const raInstructionEdge& other) const
	{
		return index >= other.index;
	}
	bool operator==(const raInstructionEdge& other) const
	{
		return index == other.index;
	}

	raInstructionEdge operator+(sint32 offset) const
	{
		cemu_assert_debug(IsInstructionIndex());
		cemu_assert_debug(offset >= 0 && offset < RA_INTER_RANGE_END);
		raInstructionEdge edge;
		edge.index = index + offset;
		return edge;
	}

	raInstructionEdge operator-(sint32 offset) const
	{
		cemu_assert_debug(IsInstructionIndex());
		cemu_assert_debug(offset >= 0 && offset < RA_INTER_RANGE_END);
		raInstructionEdge edge;
		edge.index = index - offset;
		return edge;
	}

	raInstructionEdge& operator++()
	{
		cemu_assert_debug(IsInstructionIndex());
		index++;
		return *this;
	}

private:
	sint32 index; // can also be RA_INTER_RANGE_START or RA_INTER_RANGE_END, otherwise contains instruction index * 2

};

struct raInterval
{
	raInterval()
	{

	}

	raInterval(raInstructionEdge start, raInstructionEdge end)
	{
		SetInterval(start, end);
	}

	// isStartOnInput = Input+Output edge on first instruction. If false then only output
	// isEndOnOutput = Input+Output edge on last instruction. If false then only input
	void SetInterval(sint32 start, bool isStartOnInput, sint32 end, bool isEndOnOutput)
	{
		this->start.Set(start, isStartOnInput);
		this->end.Set(end, !isEndOnOutput);
	}

	void SetInterval(raInstructionEdge start, raInstructionEdge end)
	{
		cemu_assert_debug(start <= end);
		this->start = start;
		this->end = end;
	}

	void SetStart(const raInstructionEdge& edge)
	{
		start = edge;
	}

	void SetEnd(const raInstructionEdge& edge)
	{
		end = edge;
	}

	sint32 GetStartIndex() const
	{
		return start.GetInstructionIndex();
	}

	sint32 GetEndIndex() const
	{
		return end.GetInstructionIndex();
	}

	bool ExtendsPreviousSegment() const
	{
		return start.ConnectsToPreviousSegment();
	}

	bool ExtendsIntoNextSegment() const
	{
		return end.ConnectsToNextSegment();
	}

	bool IsNextSegmentOnly() const
	{
		return start.ConnectsToNextSegment() && end.ConnectsToNextSegment();
	}

	bool IsPreviousSegmentOnly() const
	{
		return start.ConnectsToPreviousSegment() && end.ConnectsToPreviousSegment();
	}

	// returns true if range is contained within a single segment
	bool IsLocal() const
	{
		return start.GetRaw() > RA_INTER_RANGE_START && end.GetRaw() < RA_INTER_RANGE_END;
	}

	bool ContainsInstructionIndex(sint32 instructionIndex) const
	{
		cemu_assert_debug(instructionIndex != RA_INTER_RANGE_START && instructionIndex != RA_INTER_RANGE_END);
		return instructionIndex >= start.GetInstructionIndexEx() && instructionIndex <= end.GetInstructionIndexEx();
	}

	// similar to ContainsInstructionIndex, but allows RA_INTER_RANGE_START/END as input
	bool ContainsInstructionIndexEx(sint32 instructionIndex) const
	{
		if(instructionIndex == RA_INTER_RANGE_START)
			return start.ConnectsToPreviousSegment();
		if(instructionIndex == RA_INTER_RANGE_END)
			return end.ConnectsToNextSegment();
		return instructionIndex >= start.GetInstructionIndexEx() && instructionIndex <= end.GetInstructionIndexEx();
	}

	bool ContainsEdge(const raInstructionEdge& edge) const
	{
		return edge >= start && edge <= end;
	}

	bool ContainsWholeInterval(const raInterval& other) const
	{
		return other.start >= start && other.end <= end;
	}

	bool IsOverlapping(const raInterval& other) const
	{
		return start <= other.end && end >= other.start;
	}

	sint32 GetPreciseDistance()
	{
		cemu_assert_debug(!start.ConnectsToNextSegment()); // how to handle this?
		if(start == end)
			return 1;
		cemu_assert_debug(!end.ConnectsToPreviousSegment() && !end.ConnectsToNextSegment());
		if(start.ConnectsToPreviousSegment())
			return end.GetRaw() + 1;

		return end.GetRaw() - start.GetRaw() + 1; // +1 because end is inclusive
	}

//private: not making these directly accessible only forces us to create loads of verbose getters and setters
	raInstructionEdge start;
	raInstructionEdge end;
};

struct raFixedRegRequirement
{
	raInstructionEdge pos;
	IMLPhysRegisterSet allowedReg;
};

struct raLivenessRange
{
	IMLSegment* imlSegment;
	raInterval interval2;

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
	// instruction read/write locations
	std::vector<raLivenessLocation_t> list_locations;
	// ordered list of all raInstructionEdge indices which require a fixed register
	std::vector<raFixedRegRequirement> list_fixedRegRequirements;
	// linked list (subranges with same GPR virtual register)
	raLivenessSubrangeLink link_sameVirtualRegister;
	// linked list (all subranges for this segment)
	raLivenessSubrangeLink link_allSegmentRanges;
	// register info
	IMLRegID virtualRegister;
	IMLName name;
	// register allocator result
	sint32 physicalRegister;

	boost::container::small_vector<raLivenessRange*, 32> GetAllSubrangesInCluster();
	bool GetAllowedRegistersEx(IMLPhysRegisterSet& allowedRegisters); // if the cluster has fixed register requirements in any instruction this returns the combined register mask. Otherwise returns false in which case allowedRegisters is left undefined
	IMLPhysRegisterSet GetAllowedRegisters(IMLPhysRegisterSet regPool); // return regPool with fixed register requirements filtered out

	IMLRegID GetVirtualRegister() const;
	sint32 GetPhysicalRegister() const;
	bool HasPhysicalRegister() const { return physicalRegister >= 0; }
	IMLName GetName() const;
	void SetPhysicalRegister(sint32 physicalRegister);
	void SetPhysicalRegisterForCluster(sint32 physicalRegister);
	void UnsetPhysicalRegister() { physicalRegister = -1; }
};

raLivenessRange* PPCRecRA_createSubrange2(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, IMLRegID virtualRegister, IMLName name, raInstructionEdge startPosition, raInstructionEdge endPosition);
void PPCRecRA_deleteSubrange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange);
void PPCRecRA_deleteAllRanges(ppcImlGenContext_t* ppcImlGenContext);

void PPCRecRA_explodeRange(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* originRange);

void PPCRecRA_mergeSubranges(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange* subrange, raLivenessRange* absorbedSubrange);

raLivenessRange* PPCRecRA_splitLocalSubrange2(ppcImlGenContext_t* ppcImlGenContext, raLivenessRange*& subrange, raInstructionEdge splitPosition, bool trimToHole = false);

void PPCRecRA_updateOrAddSubrangeLocation(raLivenessRange* subrange, sint32 index, bool isRead, bool isWrite);
void PPCRecRA_debugValidateSubrange(raLivenessRange* subrange);

// cost estimation
sint32 PPCRecRARange_getReadWriteCost(IMLSegment* imlSegment);
sint32 PPCRecRARange_estimateCostAfterRangeExplode(raLivenessRange* subrange);
//sint32 PPCRecRARange_estimateAdditionalCostAfterSplit(raLivenessRange* subrange, sint32 splitIndex);
sint32 PPCRecRARange_estimateAdditionalCostAfterSplit(raLivenessRange* subrange, raInstructionEdge splitPosition);