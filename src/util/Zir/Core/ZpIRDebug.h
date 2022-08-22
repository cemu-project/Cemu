#pragma once
#include "util/Zir/Core/IR.h"

namespace ZpIR
{

	class DebugPrinter
	{

	public:
		void debugPrint(ZpIRFunction* irFunction);

		void setShowPhysicalRegisters(bool showPhys)
		{
			m_showPhysicalRegisters = showPhys;
		}

		void setVirtualRegisterNameSource(std::string(*getRegisterNameCustom)(ZpIRBasicBlock* block, IRReg r))
		{
			m_getRegisterNameCustom = getRegisterNameCustom;
		}

		void setPhysicalRegisterNameSource(std::string(*getRegisterNameCustom)(ZpIRBasicBlock* block, ZpIRPhysicalReg r))
		{
			m_getPhysicalRegisterNameCustom = getRegisterNameCustom;
		}

	private:
		std::string getRegisterName(ZpIRBasicBlock* block, IRReg r);
		std::string getInstructionHRF(ZpIRBasicBlock* block, IR::__InsBase* cmd);
		void debugPrintBlock(ZpIRBasicBlock* block);

		std::string(*m_getRegisterNameCustom)(ZpIRBasicBlock* block, IRReg r) { nullptr };
		std::string(*m_getPhysicalRegisterNameCustom)(ZpIRBasicBlock* block, ZpIRPhysicalReg r) { nullptr };

		bool m_showPhysicalRegisters{}; // show global/physical register mapping instead of local IRReg indices
	};

}