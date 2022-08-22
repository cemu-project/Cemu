#include "util/Zir/Core/IR.h"
#include "util/Zir/Core/ZirUtility.h"
#include "util/Zir/Core/ZpIRPasses.h"
#include "util/Zir/Core/ZpIRDebug.h"

namespace ZirPass
{

	void RegisterAllocatorForGLSL::assignPhysicalRegisters()
	{
		if (m_irFunction->m_basicBlocks.size() != 1)
			cemu_assert_unimplemented();

		for (auto& itr : m_irFunction->m_basicBlocks)
			assignPhysicalRegistersForBlock(itr);
	}

	void RegisterAllocatorForGLSL::assignPhysicalRegistersForBlock(ZpIR::ZpIRBasicBlock* basicBlock)
	{
		// resolve imports
		for (auto& itr : basicBlock->m_imports)
		{
			assert_dbg(); // todo - If imported reg not assigned physical register yet -> create a shared physical register (MSB set in reg index?) And assign it to this basic block but also all the shared IRRegs in the other linked basic blocks
			// how to handle import:
			// - match physical register of every input/output
			// - every import must have a matching export in all the previous basic blocks. If not all match this is an error.
			//   In our shader emitter this could happen if the original R600 code references an uninitialized register

			// note - we also have to make sure the register type matches. If a linked block has a shared register with a different type then we need to create a new register and insert a bitcast instruction in that block
		}
		// assign a register index to every virtual register
		for (auto& itr : basicBlock->m_regs)
		{
			if (itr.type != ZpIR::DataType::NONE && !itr.hasAssignedPhysicalRegister())
			{
				if (itr.type == ZpIR::DataType::F32)
					itr.assignPhysicalRegister(MakePhysReg(PHYS_REG_TYPE::F32, m_physicalRegisterCounterF32++));
				else if (itr.type == ZpIR::DataType::S32)
					itr.assignPhysicalRegister(MakePhysReg(PHYS_REG_TYPE::S32, m_physicalRegisterCounterS32++));
				else if (itr.type == ZpIR::DataType::U32)
					itr.assignPhysicalRegister(MakePhysReg(PHYS_REG_TYPE::U32, m_physicalRegisterCounterU32++));
				else
				{
					cemu_assert_debug(false);
				}
			}
		}

	}

	std::string RegisterAllocatorForGLSL::DebugPrintHelper_getPhysRegisterName(ZpIR::ZpIRBasicBlock* block, ZpIR::ZpIRPhysicalReg r)
	{
		std::string s;
		uint32 regIndex = GetPhysRegIndex(r);

		if (IsPhysRegTypeF32(r))
			s = fmt::format("r{}f", regIndex);
		else if (IsPhysRegTypeU32(r))
			s = fmt::format("r{}u", regIndex);
		else if (IsPhysRegTypeS32(r))
			s = fmt::format("r{}i", regIndex);
		return s;
	}

}