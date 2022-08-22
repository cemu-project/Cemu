#include "util/Zir/Core/IR.h"
#include "util/Zir/Core/ZirUtility.h"
#include "util/Zir/Core/ZpIRPasses.h"
#include "util/Zir/Core/ZpIRDebug.h"

namespace ZirPass
{
	using namespace ZpIR;

	/*
	   Algorithm description:

	   Prepare phase:
	    Assign every basic block an index
		Create internal arrays to match index count

	   First phase:
		Create liveness ranges for each basic block
		Link liveness ranges by import/export
		Constrained instructions split affected ranges into their own single instruction liveness range

	   Second phase:
 		Assign registers. Start with constrained ranges first, then process from beginning to end
		Whenever we assign a register to a range, we also try to propagate it to all the connected/coalesced ranges

	
		A liveness range is described by:
		- Source (Can be any of: List of previous basic blocks, liveness range in same basic block)
		- Destination (list of liveness ranges)
		- Index of basic block
		- First instruction (where register is assigned, -1 if passed from previous block)
		- Last instruction (where register is last accessed)
		- IR-Register (within the same basic block)
		During algorithm:
		- Spillcost (probably can calculate this dynamically)
		- Physical location (-1 if not assigned. Otherwise register index or spill memory offset)

	*/

	RALivenessRange_t::RALivenessRange_t(RABlock_t* block, IRReg irReg, sint32 start, sint32 end, DataType irDataType) : m_block(block), m_irReg(irReg), m_irDataType(irDataType)
	{
		block->livenessRanges.emplace(irReg, this);
		m_startIndex = start;
		m_endIndex = end;
		// register
		for (auto& itr : block->livenessRanges)
		{
			RALivenessRange_t* itrRange = itr.second;
			if (start < itrRange->m_endIndex && end >= itrRange->m_startIndex)
			{
				m_overlappingRanges.emplace_back(itrRange);
				itrRange->m_overlappingRanges.emplace_back(this);
				// todo - also immediately flag physical registers as unavailable
			}
		}
		block->unassignedRanges.emplace(this);
	}

	RALivenessRange_t::~RALivenessRange_t()
	{
		for (auto& itr : m_overlappingRanges)
		{
			RALivenessRange_t* overlappedRange = itr;
			// todo - unflag physical register (if this has one set)
			overlappedRange->m_overlappingRanges.erase(std::remove(overlappedRange->m_overlappingRanges.begin(), overlappedRange->m_overlappingRanges.end(), overlappedRange), overlappedRange->m_overlappingRanges.end());
		}
		m_overlappingRanges.clear();
		assert_dbg();
	}


	void RALivenessRange_t::setStart(sint32 startIndex)
	{
		m_startIndex = startIndex;
		assert_dbg(); // re-register in sorted range list (if no reg assigned)
	}

	void RALivenessRange_t::setEnd(sint32 endIndex)
	{
		if (endIndex > m_endIndex)
		{
			// add ranges that are now overlapping
			for (auto& itr : m_block->livenessRanges)
			{
				RALivenessRange_t* itrRange = itr.second;
				if(itrRange->isOverlapping(this))
					continue; // was overlapping before
				if(itrRange == this)
					continue;
				if (itrRange->isOverlapping(m_startIndex, endIndex))
				{
					m_overlappingRanges.emplace_back(itrRange);
					itrRange->m_overlappingRanges.emplace_back(this);
					// todo - also immediately flag physical registers as unavailable
				}
			}
		}
		else if (endIndex < m_endIndex)
		{
			// remove ranges that are no longer overlapping
			cemu_assert_suspicious();
		}
		m_endIndex = endIndex;
	}

	void RALivenessRange_t::assignPhysicalRegister(ZpIRPhysicalReg physReg)
	{
		if (m_location != LOCATION::UNASSIGNED)
			cemu_assert_suspicious();
		m_location = LOCATION::PHYSICAL_REGISTER;
		m_physicalRegister = physReg;
		// remove this from unassignedRanges
		auto itr = m_block->unassignedRanges.find(this);
		if (itr == m_block->unassignedRanges.end())
			cemu_assert_suspicious();
		if (*itr != this)
			cemu_assert_suspicious();
		m_block->unassignedRanges.erase(itr);
	}


	void RARegular::prepareRABlocks()
	{
		auto& irBasicBlocks = m_irFunction->m_basicBlocks;

		m_raBlockArray.resize(m_irFunction->m_basicBlocks.size());
	}

	void RARegular::generateLivenessRanges()
	{
		auto& irBasicBlocks = m_irFunction->m_basicBlocks;
		//for (auto& itr : irBasicBlocks)
		for (uint32 basicBlockIndex = 0; basicBlockIndex < (uint32)irBasicBlocks.size(); basicBlockIndex++)
		{
			auto& blockItr = irBasicBlocks[basicBlockIndex];
			RABlock_t* raBlock = m_raBlockArray.data() + basicBlockIndex;
			std::unordered_map<IRReg, RALivenessRange_t*>& blockRanges = raBlock->livenessRanges;
			// init ranges for imports first
			for (auto& regImport : blockItr->m_imports)
			{
				new RALivenessRange_t(raBlock, regImport.reg, -1, -1, blockItr->m_regs[(uint16)regImport.reg].type);
				// imports start before the current basic block
			}
			// parse instructions and create/update ranges
			IR::__InsBase* ins = blockItr->m_instructionFirst;
			size_t i = 0;
			while(ins)
			{
				ZpIRCmdUtil::forEachAccessedReg(*blockItr, ins,
					[&blockRanges, i, raBlock](IRReg readReg)
					{
						if (readReg >= 0x8000)
							cemu_assert_suspicious();
						// read access
						auto livenessRange = blockRanges.find(readReg);
						if (livenessRange == blockRanges.end())
							cemu_assert_suspicious();
						livenessRange->second->setEnd((sint32)i);
					},
					[&blockRanges, i, raBlock, blockItr](IRReg writtenReg)
					{
						if (writtenReg >= 0x8000)
							cemu_assert_suspicious();
						// write access
						auto livenessRange = blockRanges.find(writtenReg);
						if (livenessRange != blockRanges.end())
							cemu_assert_suspicious();
						new RALivenessRange_t(raBlock, writtenReg, (sint32)i, (sint32)i, blockItr->m_regs[(uint16)writtenReg].type);
					});
				i++;
				ins = ins->next;
			}
			// exports extend ranges to one instruction past the end of the block
			for (auto& regExport : blockItr->m_exports)
			{
				auto livenessRange = blockRanges.find(regExport.reg);
				if (livenessRange == blockRanges.end())
					cemu_assert_suspicious();
				cemu_assert_unimplemented();
				//livenessRange->second->setEnd((sint32)blockItr->m_cmdsDepr.size());
			}
		}
		// connect liveness ranges across basic blocks based on their import/export names
		std::unordered_map<LocationSymbolName, RALivenessRange_t*> listExportedRanges;
		for (uint32 basicBlockIndex = 0; basicBlockIndex < (uint32)irBasicBlocks.size(); basicBlockIndex++)
		{
			// for each block take all exported ranges and connect them to the imports of the successor blocks
			auto& blockItr = irBasicBlocks[basicBlockIndex];
			// collect all exported liveness ranges
			std::unordered_map<IRReg, RALivenessRange_t*>& localRanges = m_raBlockArray[basicBlockIndex].livenessRanges;
			listExportedRanges.clear();
			for (auto& regExport : blockItr->m_exports)
			{
				auto livenessRange = localRanges.find(regExport.reg);
				if (livenessRange == localRanges.end())
					assert_dbg();
				listExportedRanges.emplace(regExport.name, livenessRange->second);
			}
			// handle imports in the connected blocks
			if (blockItr->m_branchTaken)
			{
				ZpIRBasicBlock* successorBlock = blockItr->m_branchTaken;
				std::unordered_map<IRReg, RALivenessRange_t*>& successorRanges = localRanges = m_raBlockArray[basicBlockIndex].livenessRanges;
				for (auto& regImport : successorBlock->m_exports)
				{
					auto livenessRange = successorRanges.find(regImport.reg);
					if (livenessRange == successorRanges.end())
						assert_dbg();
					auto connectedSourceRange = listExportedRanges.find(regImport.name);
					if (connectedSourceRange == listExportedRanges.end())
						assert_dbg();
					livenessRange->second->addSourceFromPreviousBlock(connectedSourceRange->second);
				}
			}
			// handle imports for entry blocks
			// todo
			// handle export for exit blocks
			// todo
		}
	}

	void RARegular::assignPhysicalRegistersForBlock(RABlock_t& raBlock)
	{
		debugPrint(raBlock);
		std::vector<sint32> physRegCandidates;
		physRegCandidates.reserve(32);
		// process livenessRanges ascending by start address
		while (!raBlock.unassignedRanges.empty())
		{
			RALivenessRange_t* range = *raBlock.unassignedRanges.begin();
			// get a list of potential physical registers
			std::span<sint32> physReg = extGetSuitablePhysicalRegisters(range->m_irDataType);

			physRegCandidates.clear();
			for (auto& r : physReg)
				physRegCandidates.emplace_back(r);

			// try to find a physical register that we can assign to the entire liveness span (current range and all connected ranges)
			// todo
			// handle special cases like copy coalescing
			// todo
			// try to find a register for only the current range

			filterCandidates(physRegCandidates, range);
			if (!physRegCandidates.empty())
			{
				// pick preferred register
				ZpIRPhysicalReg physRegister = extPickPreferredRegister(physRegCandidates);
				range->assignPhysicalRegister(physRegister);
				continue;
			}

			// spill is necessary
			assert_dbg();


			assert_dbg();

		}

		printf("Assigned:\n");
		debugPrint(raBlock);
	}

	void RARegular::assignPhysicalRegisters()
	{
		// todo - first we should assign all the fixed registers. E.g. imports/exports, constrained instructions

		for (auto& raBlockInfo : m_raBlockArray)
			assignPhysicalRegistersForBlock(raBlockInfo);
	}

	void RARegular::rewrite()
	{
		for (size_t i = 0; i < m_raBlockArray.size(); i++)
			rewriteBlock(*m_irFunction->m_basicBlocks[i], m_raBlockArray[i]);
	}

	void RARegular::rewriteBlock(ZpIRBasicBlock& basicBlock, RABlock_t& raBlock)
	{
		assert_dbg();
		//std::vector<ZpIRCmd> cmdOut;

		//std::unordered_map<ZpIRReg, ZpIRReg> translationTable;
		//for (auto& itr : raBlock.livenessRanges)
		//	translationTable.emplace(itr.second->m_irReg, itr.second->m_physicalRegister);
		//// todo - since ir var registers are created in incremental order we could instead use a std::vector for fast look-up instead of a map?

		//for (uint32 i = 0; i < (uint32)basicBlock.m_cmdsDepr.size(); i++)
		//{
		//	// todo - insert spill and load instructions
		//	// todo - insert register moves for range-to-range copies
		//	
		//	ZpIRCmd* currentCmd = basicBlock.m_cmdsDepr.data() + i;
		//	// replace registers and then insert into output command list
		//	ZpIRCmdUtil::replaceRegisters(*currentCmd, translationTable);
		//	cmdOut.emplace_back(*currentCmd);
		//}

		//basicBlock.m_cmdsDepr = std::move(cmdOut);

		// todo - should we keep imports/exports but update them to use physical register indices?
		//        the code emitter needs to know which physical registers are exported in order to determine which optimizations are allowed

		basicBlock.m_imports.clear();
		basicBlock.m_imports.shrink_to_fit();
		basicBlock.m_exports.clear();
		basicBlock.m_exports.shrink_to_fit();
		basicBlock.m_regs.clear();
		basicBlock.m_regs.shrink_to_fit();
	}

}
