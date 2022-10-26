#include "util/Zir/Core/IR.h"
#include "util/Zir/Core/ZpIRDebug.h"

#include <cinttypes>

namespace ZpIR
{

	const char* getOpcodeName(IR::OpCode opcode)
	{
		switch (opcode)
		{
		case IR::OpCode::ADD:
			return "ADD";
		case IR::OpCode::MOV:
			return "MOV";
		case IR::OpCode::MUL:
			return "MUL";
		case IR::OpCode::DIV:
			return "DIV";

		case IR::OpCode::BITCAST:
			return "BITCAST";
		case IR::OpCode::SWAP_ENDIAN:
			return "SWAP_ENDIAN";
		case IR::OpCode::CONVERT_INT_TO_FLOAT:
			return "CONV_I2F";
		case IR::OpCode::CONVERT_FLOAT_TO_INT:
			return "CONV_F2I";

		case IR::OpCode::IMPORT_SINGLE:
			return "IMPORT_S";
		case IR::OpCode::EXPORT:
			return "EXPORT";
		case IR::OpCode::IMPORT:
			return "IMPORT";
		default:
			cemu_assert_debug(false);
			return "UKN";
		}
		return "";
	}

	const char* getTypeName(DataType t)
	{
		switch (t)
		{
		case DataType::S64:
			return "s64";
		case DataType::U64:
			return "u64";
		case DataType::S32:
			return "s32";
		case DataType::U32:
			return "u32";
		case DataType::S16:
			return "s16";
		case DataType::U16:
			return "u16";
		case DataType::S8:
			return "s8";
		case DataType::U8:
			return "u8";
		case DataType::BOOL:
			return "bool";
		case DataType::POINTER:
			return "ptr";
		}
		return "";
	}

	std::string DebugPrinter::getRegisterName(ZpIRBasicBlock* block, IRReg r)
	{
		std::string s;

		if ((uint16)r < 0x8000 && m_showPhysicalRegisters)
		{
			auto& reg = block->m_regs[(uint16)r];
			if (!reg.hasAssignedPhysicalRegister())
				return "UNASSIGNED";
			s = m_getPhysicalRegisterNameCustom(block, reg.physicalRegister);
			return s;
		}

		if ((uint16)r < 0x8000 && m_getRegisterNameCustom)
		{
			return m_getRegisterNameCustom(block, r);
		}

		if ((uint16)r >= 0x8000)
		{
			auto& reg = block->m_consts[(uint16)r & 0x7FFF];
			switch (reg.type)
			{
			case DataType::POINTER:
				return fmt::format("ptr:{}", reg.value_ptr);
			case DataType::U64:
			{
				if(reg.value_u64 >= 0x1000)
					return fmt::format("u64:0x{0:x}", reg.value_u64);
				return fmt::format("u64:{}", reg.value_u64);
			}
			case DataType::U32:
				return fmt::format("u32:{}", reg.value_u32);
			case DataType::S32:
				return fmt::format("s32:{}", reg.value_u32);
			case DataType::F32:
				return fmt::format("f32:{}", reg.value_f32);
			default:
				break;
			}
			return "ukn_const_type";
		}
		else
		{
			auto& reg = block->m_regs[(uint16)r];
			
			const char* regLetter = "r";
			switch (reg.type)
			{
			case DataType::U64:
				regLetter = "uq"; // quad-word
				break;
			case DataType::U32:
				regLetter = "ud"; // double-word
				break;
			case DataType::U16:
				regLetter = "uw"; // word
				break;
			case DataType::U8:
				regLetter = "uc"; // char
				break;
			case DataType::S64:
				regLetter = "sq"; // signed quad-word
				break;
			case DataType::S32:
				regLetter = "sd"; // signed double-word
				break;
			case DataType::S16:
				regLetter = "sw"; // signed word
				break;
			case DataType::S8:
				regLetter = "sc"; // signed char
				break;
			case DataType::F32:
				regLetter = "fv"; // 32bit float
				break;
			case DataType::POINTER:
				regLetter = "ptr";
				break;
			default:
				assert_dbg();
			}
			if (reg.elementCount != 1)
				assert_dbg();
			s = fmt::format("{}{}", regLetter, (uint16)r);
		}
		return s;
	}

	std::string DebugPrinter::getInstructionHRF(ZpIRBasicBlock* block, IR::__InsBase* cmd)
	{
		if (auto ins = IR::InsRR::getIfForm(cmd))
		{
			return fmt::format("{:<10} {}, {}", getOpcodeName(cmd->opcode), getRegisterName(block, ins->rA), getRegisterName(block, ins->rB));
		}
		else if (auto ins = IR::InsRRR::getIfForm(cmd))
		{
			return fmt::format("{:<10} {}, {}, {}", getOpcodeName(cmd->opcode), getRegisterName(block, ins->rA), getRegisterName(block, ins->rB), getRegisterName(block, ins->rC));
		}
		else if (auto ins = IR::InsEXPORT::getIfForm(cmd))
		{
			if (ins->count == 4)
				return fmt::format("{:<10} {}, {}, {}, {}, loc: {}", getOpcodeName(cmd->opcode), getRegisterName(block, ins->regArray[0]), getRegisterName(block, ins->regArray[1]), getRegisterName(block, ins->regArray[2]), getRegisterName(block, ins->regArray[3]), ins->exportSymbol);
			else if (ins->count == 3)
				return fmt::format("{:<10} {}, {}, {}, loc: {}", getOpcodeName(cmd->opcode), getRegisterName(block, ins->regArray[0]), getRegisterName(block, ins->regArray[1]), getRegisterName(block, ins->regArray[2]), ins->exportSymbol);
			else if (ins->count == 2)
				return fmt::format("{:<10} {}, {}, loc: {}", getOpcodeName(cmd->opcode), getRegisterName(block, ins->regArray[0]), getRegisterName(block, ins->regArray[1]), ins->exportSymbol);
			else if (ins->count == 1)
				return fmt::format("{:<10} {}, loc: {}", getOpcodeName(cmd->opcode), getRegisterName(block, ins->regArray[0]), ins->exportSymbol);
			assert_dbg();
		}
		else if (auto ins = IR::InsIMPORT::getIfForm(cmd))
		{
			ShaderSubset::ShaderImportLocation importLocation = ins->importSymbol;
			std::string locDebugName = importLocation.GetDebugName();

			if (ins->count == 4)
				return fmt::format("{:<10} {}, {}, {}, {}, loc: {}", getOpcodeName(cmd->opcode), getRegisterName(block, ins->regArray[0]), getRegisterName(block, ins->regArray[1]), getRegisterName(block, ins->regArray[2]), getRegisterName(block, ins->regArray[3]), locDebugName);
			else if (ins->count == 3)
				return fmt::format("{:<10} {}, {}, {}, loc: {}", getOpcodeName(cmd->opcode), getRegisterName(block, ins->regArray[0]), getRegisterName(block, ins->regArray[1]), getRegisterName(block, ins->regArray[2]), locDebugName);
			else if (ins->count == 2)
				return fmt::format("{:<10} {}, {}, loc: {}", getOpcodeName(cmd->opcode), getRegisterName(block, ins->regArray[0]), getRegisterName(block, ins->regArray[1]), locDebugName);
			else if (ins->count == 1)
				return fmt::format("{:<10} {}, loc: {}", getOpcodeName(cmd->opcode), getRegisterName(block, ins->regArray[0]), locDebugName);
			assert_dbg();
		}
		else
			assert_dbg();
		return "";
	}

	void DebugPrinter::debugPrintBlock(ZpIRBasicBlock* block)
	{
		// print name
		printf("IRBasicBlock %" PRIxPTR "\n", (uintptr_t)block);
		// print imports
		printf("Imports:\n");
		for(auto itr : block->m_imports)
			printf("   reg: %s sym:0x%llx\n", getRegisterName(block, itr.reg).c_str(), itr.name);
		// print exports
		printf("Exports:\n");
		for (auto itr : block->m_exports)
			printf("   reg: %s sym:0x%llx\n", getRegisterName(block, itr.reg).c_str(), itr.name);
		// print instructions
		printf("Assembly:\n");
		IR::__InsBase* instruction = block->m_instructionFirst;
		size_t i = 0;
		while(instruction)
		{
			std::string s = getInstructionHRF(block, instruction);
			printf("%04x %s\n", (unsigned int)i, s.c_str());
			i++;
			instruction = instruction->next;
		}
	}

	void DebugPrinter::debugPrint(ZpIRFunction* irFunction)
	{
		printf("--- Print IR function assembly ---\n");
		for (auto& itr : irFunction->m_basicBlocks)
		{
			debugPrintBlock(itr);
			printf("\n");
		}
	}

}
