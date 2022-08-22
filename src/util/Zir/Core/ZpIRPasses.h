#pragma once
#include "util/Zir/Core/IR.h"

namespace ZirPass
{
	class ZpIRPass
	{
	public:
		ZpIRPass(ZpIR::ZpIRFunction* irFunction) : m_irFunction(irFunction) { };

		virtual void applyPass() = 0;

	protected:
		ZpIR::ZpIRFunction* m_irFunction;
	};

	struct RALivenessRange_t
	{
		RALivenessRange_t(struct RABlock_t* block, ZpIR::IRReg irReg, sint32 start, sint32 end, ZpIR::DataType irDataType);
		~RALivenessRange_t();

		enum class SOURCE
		{
			NONE,
			INSTRUCTION, // instruction initializes value
			PREVIOUS_BLOCK, // imported from previous block(s)
			PREVIOUS_RANGE, // from previous range within same block
		};

		enum class LOCATION
		{
			UNASSIGNED,
			PHYSICAL_REGISTER,
			SPILLED,
		};

		void setStart(sint32 startIndex);
		void setEnd(sint32 endIndex);

		void addSourceFromPreviousBlock(RALivenessRange_t* source)
		{
			if (m_source != SOURCE::NONE && m_source != SOURCE::PREVIOUS_BLOCK)
				assert_dbg();
			m_source = SOURCE::PREVIOUS_BLOCK;
			m_sourceRanges.emplace_back(source);
			source->m_destinationRanges.emplace_back(this);
		}

		void addSourceFromSameBlock(RALivenessRange_t* source)
		{
			if (m_source != SOURCE::NONE)
				assert_dbg();
			m_source = SOURCE::PREVIOUS_RANGE;
			m_sourceRanges.emplace_back(source);
			source->m_destinationRanges.emplace_back(this);
		}

		bool isOverlapping(sint32 start, sint32 end) const
		{
			return m_startIndex < end && m_endIndex >= start;
		}

		bool isOverlapping(RALivenessRange_t* range) const
		{
			return isOverlapping(range->m_startIndex, range->m_endIndex);
		}

		void assignPhysicalRegister(ZpIR::ZpIRPhysicalReg physReg);

		RABlock_t* m_block;
		ZpIR::IRReg m_irReg;
		ZpIR::DataType m_irDataType;
		sint32 m_startIndex{ -1 };
		sint32 m_endIndex{ -1 }; // inclusive

		//std::vector<bool> m_reservedPhysRegisters; // unavailable physical registers
		std::vector<RALivenessRange_t*> m_overlappingRanges;

		// state / assigned location
		LOCATION m_location{ LOCATION::UNASSIGNED };
		sint32 m_physicalRegister;

		// source
		SOURCE m_source{ SOURCE::NONE };
		std::vector<RALivenessRange_t*> m_sourceRanges;

		// destination
		//RALivenessRange_t* m_destinationRange{ nullptr };
		std::vector<RALivenessRange_t*> m_destinationRanges;

	};

	struct RABlock_t
	{
		std::unordered_map<ZpIR::IRReg, RALivenessRange_t*> livenessRanges;

		struct Compare
		{
			bool operator()(const RALivenessRange_t* lhs, const RALivenessRange_t* rhs) const /* noexcept */
			{
				// order for unassignedRanges
				// aka order in which ranges should be assigned physical registers
				return lhs->m_startIndex < rhs->m_startIndex;
			}
		};

	public:
		std::multiset<RALivenessRange_t*, Compare> unassignedRanges;
	};

	class RARegular : public ZpIRPass
	{
	public:
		RARegular(ZpIR::ZpIRFunction* irFunction) : ZpIRPass(irFunction) {};

		void applyPass()
		{
			prepareRABlocks();
			generateLivenessRanges();
			assignPhysicalRegisters();
			assert_dbg(); // todo -> rewrite doesnt need to be separate any longer since we store a separate physical register index now (in IRReg)
			rewrite();
			m_irFunction->state.registersAllocated = true;
		}


	private:
		void prepareRABlocks();
		void generateLivenessRanges();
		void assignPhysicalRegisters();
		void rewrite();

		void assignPhysicalRegistersForBlock(RABlock_t& raBlock);
		void rewriteBlock(ZpIR::ZpIRBasicBlock& basicBlock, RABlock_t& raBlock);

		std::span<sint32> extGetSuitablePhysicalRegisters(ZpIR::DataType dataType)
		{
			const sint32 OFFSET_U64 = 0;
			const sint32 OFFSET_U32 = 16;

			static sint32 _regCandidatesU64[] = { OFFSET_U64 + 0, OFFSET_U64 + 1, OFFSET_U64 + 2, OFFSET_U64 + 3, OFFSET_U64 + 4, OFFSET_U64 + 5, OFFSET_U64 + 6, OFFSET_U64 + 7, OFFSET_U64 + 8, OFFSET_U64 + 9, OFFSET_U64 + 10, OFFSET_U64 + 11, OFFSET_U64 + 12, OFFSET_U64 + 13, OFFSET_U64 + 14, OFFSET_U64 + 15 };
			static sint32 _regCandidatesU32[] = { OFFSET_U32 + 0, OFFSET_U32 + 1, OFFSET_U32 + 2, OFFSET_U32 + 3, OFFSET_U32 + 4, OFFSET_U32 + 5, OFFSET_U32 + 6, OFFSET_U32 + 7, OFFSET_U32 + 8, OFFSET_U32 + 9, OFFSET_U32 + 10, OFFSET_U32 + 11, OFFSET_U32 + 12, OFFSET_U32 + 13, OFFSET_U32 + 14, OFFSET_U32 + 15 };



			if (dataType == ZpIR::DataType::POINTER || dataType == ZpIR::DataType::U64)
				return _regCandidatesU64;
			if (dataType == ZpIR::DataType::U32)
				return _regCandidatesU32;

			//if (dataType != ZpIRDataType::POINTER)
			//{

			//}
			assert_dbg();

			return _regCandidatesU32;
		}

		void extFilterPhysicalRegisters(std::vector<sint32>& physRegCandidates, ZpIR::ZpIRPhysicalReg registerToFilter)
		{
			// todo - this is quite complex on x86 where registers overlap (e.g. RAX and EAX/AL/AH/AX)
			// so registerToFilter can translate to multiple filtered values

			// but for now we use a simplified placeholder implementation


			if (registerToFilter >= 0 && registerToFilter < 16)
			{
				physRegCandidates.erase(std::remove(physRegCandidates.begin(), physRegCandidates.end(), (sint32)registerToFilter), physRegCandidates.end());
				physRegCandidates.erase(std::remove(physRegCandidates.begin(), physRegCandidates.end(), (sint32)registerToFilter + 16), physRegCandidates.end());
			}
			else if (registerToFilter >= 16 && registerToFilter < 32)
			{
				physRegCandidates.erase(std::remove(physRegCandidates.begin(), physRegCandidates.end(), (sint32)registerToFilter), physRegCandidates.end());
				physRegCandidates.erase(std::remove(physRegCandidates.begin(), physRegCandidates.end(), (sint32)registerToFilter - 16), physRegCandidates.end());
			}
			else
				assert_dbg();
		}

		ZpIR::ZpIRPhysicalReg extPickPreferredRegister(std::vector<sint32>& physRegCandidates)
		{
			if (physRegCandidates.empty())
				assert_dbg();
			return physRegCandidates[0];
		}

		void debugPrint(RABlock_t& raBlock)
		{
			std::multiset<RALivenessRange_t*, RABlock_t::Compare> sortedRanges;

			for (auto& itr : raBlock.livenessRanges)
				sortedRanges.emplace(itr.second);

			for (auto& itr : sortedRanges)
			{
				printf("%04x - %04x reg %04d: ", (uint32)(uint16)itr->m_startIndex, (uint32)(uint16)itr->m_endIndex, (uint32)itr->m_irReg);

				if (itr->m_location == RALivenessRange_t::LOCATION::PHYSICAL_REGISTER)
					printf("PHYS_REG %d", (int)itr->m_physicalRegister);
				else if (itr->m_location == RALivenessRange_t::LOCATION::UNASSIGNED)
					printf("UNASSIGNED");
				else
					assert_dbg();

				printf("\n");
			}
		}

		// remove all physical registers from physRegCandidates which are already reserved by any of the overlapping ranges
		void filterCandidates(std::vector<sint32>& physRegCandidates, RALivenessRange_t* range)
		{
			for (auto& itr : range->m_overlappingRanges)
			{
				if (itr->m_location != RALivenessRange_t::LOCATION::PHYSICAL_REGISTER)
					continue;
				extFilterPhysicalRegisters(physRegCandidates, itr->m_physicalRegister);
			}
		}


		std::vector<RABlock_t> m_raBlockArray;
	};


	class RegisterAllocatorForGLSL : public ZpIRPass
	{
		enum class PHYS_REG_TYPE : uint8
		{
			U32 = 0,
			S32 = 1,
			F32 = 2
		};

	public:
		RegisterAllocatorForGLSL(ZpIR::ZpIRFunction* irFunction) : ZpIRPass(irFunction) {};

		void applyPass()
		{
			assignPhysicalRegisters();
			m_irFunction->state.registersAllocated = true;
		}

		static bool IsPhysRegTypeU32(ZpIR::ZpIRPhysicalReg physReg)
		{
			return ((uint32)physReg >> 30) == (uint32)PHYS_REG_TYPE::U32;
		}

		static bool IsPhysRegTypeS32(ZpIR::ZpIRPhysicalReg physReg)
		{
			return ((uint32)physReg >> 30) == (uint32)PHYS_REG_TYPE::S32;
		}

		static bool IsPhysRegTypeF32(ZpIR::ZpIRPhysicalReg physReg)
		{
			return ((uint32)physReg >> 30) == (uint32)PHYS_REG_TYPE::F32;
		}

		static uint32 GetPhysRegIndex(ZpIR::ZpIRPhysicalReg physReg)
		{
			return (uint32)physReg & 0x3FFFFFFF;
		}

		static std::string DebugPrintHelper_getPhysRegisterName(ZpIR::ZpIRBasicBlock* block, ZpIR::ZpIRPhysicalReg r);

	private:
		void assignPhysicalRegisters();

		void assignPhysicalRegistersForBlock(ZpIR::ZpIRBasicBlock* basicBlock);

		uint32 m_physicalRegisterCounterU32{};
		uint32 m_physicalRegisterCounterS32{};
		uint32 m_physicalRegisterCounterF32{};

		ZpIR::ZpIRPhysicalReg MakePhysReg(PHYS_REG_TYPE regType, uint32 index)
		{
			uint32 v = (uint32)regType << 30;
			v |= index;
			return (ZpIR::ZpIRPhysicalReg)v;
		}
	};

};
