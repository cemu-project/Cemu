#pragma once
#include "Cafe/HW/Espresso/EspressoISA.h"
#include "Cafe/HW/MMU/MMU.h"

bool GamePatch_IsNonReturnFunction(uint32 hleIndex);

// utility class to determine shape of a function
class PPCFunctionBoundaryTracker
{
public:
	struct PPCRange_t
	{
		PPCRange_t() = default;
		PPCRange_t(uint32 _startAddress) : startAddress(_startAddress) {};

		uint32 startAddress{};
		uint32 length{};
		//bool isProcessed{false};

		uint32 getEndAddress() const { return startAddress + length; };
	};

public:
	void trackStartPoint(MPTR startAddress)
	{
		processRange(startAddress, nullptr, nullptr);
		processBranchTargets();
	}

	bool getRangeForAddress(uint32 address, PPCRange_t& range)
	{
		for (auto itr : map_ranges)
		{
			if (address >= itr->startAddress && address < (itr->startAddress + itr->length))
			{
				range = *itr;
				return true;
			}
		}
		return false;
	}

private:
	void addBranchDestination(PPCRange_t* sourceRange, MPTR address)
	{
		map_branchTargets.emplace(address);
	}

	// process flow of instruction
	// returns false if the IP cannot increment past the current instruction
	bool processInstruction(PPCRange_t* range, MPTR address)
	{
		// parse instructions
		uint32 opcode = memory_readU32(address);
		switch (Espresso::GetPrimaryOpcode(opcode))
		{
		case Espresso::PrimaryOpcode::ZERO:
		{
			if (opcode == 0)
				return false; // invalid instruction
			break;
		}
		case Espresso::PrimaryOpcode::VIRTUAL_HLE:
		{
			// end of function
			// is there a jump to a instruction after this one?
			uint32 hleFuncId = opcode & 0xFFFF;
			if (hleFuncId >= 0x1000 && hleFuncId < 0x4000)
			{
				if (GamePatch_IsNonReturnFunction(hleFuncId - 0x1000) == false)
				{
					return true;
				}
			}
			return false;
		}
		case Espresso::PrimaryOpcode::BC:
		{
			uint32 BD, BI;
			Espresso::BOField BO;
			bool AA, LK;
			Espresso::decodeOp_BC(opcode, BD, BO, BI, AA, LK);
			uint32 branchTarget = AA ? BD : BD + address;
			if (!LK)
				addBranchDestination(range, branchTarget);
			break;
		}
		case Espresso::PrimaryOpcode::B:	
		{
			uint32 LI;
			bool AA, LK;
			Espresso::decodeOp_B(opcode, LI, AA, LK);
			uint32 branchTarget = AA ? LI : LI + address;
			if (!LK)
			{
				addBranchDestination(range, branchTarget);
				// if the next two or previous two instructions are branch instructions, we assume that they are destinations of a jump table
				// todo - can we make this more reliable by checking for BCTR or similar instructions first?
				// example: The Swapper 0x01B1FC04
				if (PPCRecompilerCalcFuncSize_isUnconditionalBranchInstruction(memory_readU32(address + 4)) && PPCRecompilerCalcFuncSize_isUnconditionalBranchInstruction(memory_readU32(address + 8)) ||
					PPCRecompilerCalcFuncSize_isUnconditionalBranchInstruction(memory_readU32(address - 8)) && PPCRecompilerCalcFuncSize_isUnconditionalBranchInstruction(memory_readU32(address - 4)))
				{
					return true;
				}
				return false; // current flow ends at unconditional branch instruction
			}
			break;
		}
		case Espresso::PrimaryOpcode::GROUP_19:
			switch (Espresso::GetGroup19Opcode(opcode))
			{
			case Espresso::Opcode19::BCLR:
			{
				Espresso::BOField BO;
				uint32 BI;
				bool LK;
				Espresso::decodeOp_BCLR(opcode, BO, BI, LK);
				if (BO.branchAlways() && !LK)
				{
					// unconditional BLR
					return false;
				}
				break;
			}
			case Espresso::Opcode19::BCCTR:
				if (opcode == 0x4E800420)
				{
					// unconditional BCTR
					// this instruction is often used for switch statements, therefore we should be wary of ending the function here
					// It's better to overestimate function size than to predict sizes that are too short

					// Currently we only end the function if the BCTR is followed by a NOP (alignment) or invalid instruction
					// todo: improve robustness, find better ways to handle false positives
					uint32 nextOpcode = memory_readU32(address + 4);

					if (nextOpcode == 0x60000000 || PPCRecompilerCalcFuncSize_isValidInstruction(nextOpcode) == false)
					{
						return false;
					}
					return true;
				}
				// conditional BCTR
				return true;
			default:
				break;
			}
			break;
		default:
			break;
		}
		return true;
	}

	void checkForCollisions()
	{
#ifdef CEMU_DEBUG_ASSERT
		uint32 endOfPrevious = 0;
		for (auto itr : map_ranges)
		{
			if (endOfPrevious > itr->startAddress)
			{
				cemu_assert_debug(false);
			}
			endOfPrevious = itr->startAddress + itr->length;
		}
#endif
	}

	// nextRange must point to the closest range after startAddress, or NULL if there is none
	void processRange(MPTR startAddress, PPCRange_t* previousRange, PPCRange_t* nextRange)
	{
		checkForCollisions();
		cemu_assert_debug(previousRange == nullptr || (startAddress == (previousRange->startAddress + previousRange->length)));
		PPCRange_t* newRange;
		if (previousRange && (previousRange->startAddress + previousRange->length) == startAddress)
		{
			newRange = previousRange;
		}
		else
		{
			cemu_assert_debug(previousRange == nullptr);
			newRange = new PPCRange_t(startAddress);
			map_ranges.emplace(newRange);
		}
		// process instruction flow until it is interrupted by a non-conditional branch
		MPTR currentAddress = startAddress;
		MPTR endAddress = 0xFFFFFFFF;
		if (nextRange)
			endAddress = nextRange->startAddress;
		while (currentAddress < endAddress)
		{
			if (!processInstruction(newRange, currentAddress))
			{
				currentAddress += 4;
				break;
			}
			currentAddress += 4;
		}
		newRange->length = currentAddress - newRange->startAddress;

		if (nextRange && currentAddress >= nextRange->startAddress)
		{
			// merge with next range
			newRange->length = (nextRange->startAddress + nextRange->length) - newRange->startAddress;
			map_ranges.erase(nextRange);
			delete nextRange;
			checkForCollisions();
			return;
		}
		checkForCollisions();
	}

	// find first unvisited branch target and start a new range there
	// return true if method should be called again
	bool processBranchTargetsSinglePass()
	{
		cemu_assert_debug(!map_ranges.empty());
		auto rangeItr = map_ranges.begin();

		PPCRange_t* previousRange = nullptr;
		for (std::set<uint32_t>::const_iterator targetItr = map_branchTargets.begin() ; targetItr != map_branchTargets.end(); )
		{
			while (rangeItr != map_ranges.end() && ((*rangeItr)->startAddress + (*rangeItr)->length) <= (*targetItr))
			{
				previousRange = *rangeItr;
				rangeItr++;
				if (rangeItr == map_ranges.end())
				{
					// last range reached
					if ((previousRange->startAddress + previousRange->length) == *targetItr)
						processRange(*targetItr, previousRange, nullptr);
					else
						processRange(*targetItr, nullptr, nullptr);
					return true;
				}
			}

			if ((*targetItr) >= (*rangeItr)->startAddress &&
				(*targetItr) < ((*rangeItr)->startAddress + (*rangeItr)->length))
			{
				// delete visited targets
				targetItr = map_branchTargets.erase(targetItr);
				continue;
			}

			cemu_assert_debug((*rangeItr)->startAddress > (*targetItr));
			if (previousRange && (previousRange->startAddress + previousRange->length) == *targetItr)
				processRange(*targetItr, previousRange, *rangeItr); // extend previousRange
			else
				processRange(*targetItr, nullptr, *rangeItr);
			return true;
		}
		return false;
	}

	void processBranchTargets()
	{
		while (processBranchTargetsSinglePass());
	}

	private:
	bool PPCRecompilerCalcFuncSize_isUnconditionalBranchInstruction(uint32 opcode)
	{
		if (Espresso::GetPrimaryOpcode(opcode) == Espresso::PrimaryOpcode::B)
		{
			uint32 LI;
			bool AA, LK;
			Espresso::decodeOp_B(opcode, LI, AA, LK);
			if (!LK)
				return true;
		}
		return false;
	}

	bool PPCRecompilerCalcFuncSize_isValidInstruction(uint32 opcode)
	{
		if ((opcode >> 26) == 0)
			return false;
		return true;
	}

private:
	struct RangePtrCmp
	{
		bool operator()(const PPCRange_t* lhs, const PPCRange_t* rhs) const
		{
			return lhs->startAddress < rhs->startAddress;
		}
	};

	std::set<PPCRange_t*, RangePtrCmp> map_ranges;
	std::set<uint32> map_branchTargets;
};