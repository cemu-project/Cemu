#pragma once
#include "util/Zir/Core/IR.h"

namespace ZpIR
{

	struct ZpIRCmdUtil
	{
		template<typename TFuncRegRead, typename TFuncRegWrite>
		static void forEachAccessedReg(ZpIRBasicBlock& block, IR::__InsBase* instruction, TFuncRegRead funcRegRead, TFuncRegWrite funcRegWrite)
		{
			if (auto ins = IR::InsRR::getIfForm(instruction))
			{
				switch (ins->opcode)
				{
				case ZpIR::IR::OpCode::MOV:
				case ZpIR::IR::OpCode::BITCAST:
				case ZpIR::IR::OpCode::SWAP_ENDIAN:
				case ZpIR::IR::OpCode::CONVERT_FLOAT_TO_INT:
				case ZpIR::IR::OpCode::CONVERT_INT_TO_FLOAT:
					if (isRegVar(ins->rB))
						funcRegRead(ins->rB);
					cemu_assert_debug(isRegVar(ins->rA));
					funcRegWrite(ins->rA);
					break;
				default:
					cemu_assert_unimplemented();
				}
			}
			else if (auto ins = IR::InsRRR::getIfForm(instruction))
			{
				switch (ins->opcode)
				{
				case ZpIR::IR::OpCode::ADD:
				case ZpIR::IR::OpCode::SUB:
				case ZpIR::IR::OpCode::MUL:
				case ZpIR::IR::OpCode::DIV:
					if (isRegVar(ins->rB))
						funcRegRead(ins->rB);
					if (isRegVar(ins->rC))
						funcRegRead(ins->rC);
					cemu_assert_debug(isRegVar(ins->rA));
					funcRegWrite(ins->rA);
					break;
				default:
					cemu_assert_unimplemented();
				}
			}
			else if (auto ins = IR::InsIMPORT::getIfForm(instruction))
			{
				for (uint16 i = 0; i < ins->count; i++)
				{
					cemu_assert_debug(isRegVar(ins->regArray[i]));
					funcRegWrite(ins->regArray[i]);
				}
			}
			else if (auto ins = IR::InsEXPORT::getIfForm(instruction))
			{
				for (uint16 i = 0; i < ins->count; i++)
				{
					if (isRegVar(ins->regArray[i]))
						funcRegRead(ins->regArray[i]);
				}
			}
			else
			{
				cemu_assert_unimplemented();
			}
		}

		static void replaceRegisters(IR::__InsBase& ins, std::unordered_map<IRReg, IRReg>& translationTable)
		{
			cemu_assert_unimplemented();
		}
	};
}