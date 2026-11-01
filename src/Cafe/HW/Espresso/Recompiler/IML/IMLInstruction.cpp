#include "IMLInstruction.h"
#include "IML.h"

#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"

// return true if an instruction has side effects on top of just reading and writing registers
bool IMLInstruction::HasSideEffects() const
{
	bool hasSideEffects = true;
	if(type == IMLType::R_R || type == IMLType::R_R_S32 || type == IMLType::COMPARE || type == IMLType::COMPARE_S32)
		hasSideEffects = false;
	// todo - add more cases
	return hasSideEffects;
}

void IMLInstruction::CheckRegisterUsage(IMLUsedRegisters* registersUsed) const
{
	registersUsed->readGPR1 = IMLREG_INVALID;
	registersUsed->readGPR2 = IMLREG_INVALID;
	registersUsed->readGPR3 = IMLREG_INVALID;
	registersUsed->readGPR4 = IMLREG_INVALID;
	registersUsed->writtenGPR1 = IMLREG_INVALID;
	registersUsed->writtenGPR2 = IMLREG_INVALID;
	if (type == IMLType::R_NAME)
	{
		registersUsed->writtenGPR1 = op_r_name.regR;
	}
	else if (type == IMLType::NAME_R)
	{
		registersUsed->readGPR1 = op_r_name.regR;
	}
	else if (type == IMLType::R_R)
	{
		if (operation == IMLOperation::OP_X86_CMP)
		{
			// both operands are read only
			registersUsed->readGPR1 = op_r_r.regR;
			registersUsed->readGPR2 = op_r_r.regA;
		}
		else if (
			operation == IMLOperation::OP_ASSIGN ||
			operation == IMLOperation::OP_ENDIAN_SWAP ||
			operation == IMLOperation::OP_CNTLZW ||
			operation == IMLOperation::OP_NOT ||
			operation == IMLOperation::OP_NEG ||
			operation == IMLOperation::OP_ASSIGN_S16_TO_S32 ||
			operation == IMLOperation::OP_ASSIGN_S8_TO_S32)
		{
			// result is written, operand is read
			registersUsed->writtenGPR1 = op_r_r.regR;
			registersUsed->readGPR1 = op_r_r.regA;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == IMLType::R_S32)
	{
		cemu_assert_debug(operation != IMLOperation::OP_ADD &&
			operation != IMLOperation::OP_SUB &&
			operation != IMLOperation::OP_AND &&
			operation != IMLOperation::OP_OR &&
			operation != IMLOperation::OP_XOR); // deprecated, use r_r_s32 for these

		if (operation == IMLOperation::OP_LEFT_ROTATE)
		{
			// register operand is read and write
			registersUsed->readGPR1 = op_r_immS32.regR;
			registersUsed->writtenGPR1 = op_r_immS32.regR;
		}
		else if (operation == IMLOperation::OP_X86_CMP)
		{
			// register operand is read only
			registersUsed->readGPR1 = op_r_immS32.regR;
		}
		else
		{
			// register operand is write only
			// todo - use explicit lists, avoid default cases
			registersUsed->writtenGPR1 = op_r_immS32.regR;
		}
	}
	else if (type == IMLType::R_R_S32)
	{
		registersUsed->writtenGPR1 = op_r_r_s32.regR;
		registersUsed->readGPR1 = op_r_r_s32.regA;
	}
	else if (type == IMLType::R_R_S32_CARRY)
	{
		registersUsed->writtenGPR1 = op_r_r_s32_carry.regR;
		registersUsed->readGPR1 = op_r_r_s32_carry.regA;
		// some operations read carry
		switch (operation)
		{
		case IMLOperation::OP_ADD_WITH_CARRY:
			registersUsed->readGPR2 = op_r_r_s32_carry.regCarry;
			break;
		case IMLOperation::OP_ADD:
			break;
		default:
			cemu_assert_unimplemented();
		}
		// carry is always written
		registersUsed->writtenGPR2 = op_r_r_s32_carry.regCarry;
	}
	else if (type == IMLType::R_R_R)
	{
		// in all cases result is written and other operands are read only
		// with the exception of XOR, where if regA == regB then all bits are zeroed out. So we don't consider it a read
		registersUsed->writtenGPR1 = op_r_r_r.regR;
		if(!(operation == IMLOperation::OP_XOR && op_r_r_r.regA == op_r_r_r.regB))
		{
			registersUsed->readGPR1 = op_r_r_r.regA;
			registersUsed->readGPR2 = op_r_r_r.regB;
		}
	}
	else if (type == IMLType::R_R_R_CARRY)
	{
		registersUsed->writtenGPR1 = op_r_r_r_carry.regR;
		registersUsed->readGPR1 = op_r_r_r_carry.regA;
		registersUsed->readGPR2 = op_r_r_r_carry.regB;
		// some operations read carry
		switch (operation)
		{
		case IMLOperation::OP_ADD_WITH_CARRY:
			registersUsed->readGPR3 = op_r_r_r_carry.regCarry;
			break;
		case IMLOperation::OP_ADD:
			break;
		default:
			cemu_assert_unimplemented();
		}
		// carry is always written
		registersUsed->writtenGPR2 = op_r_r_r_carry.regCarry;
	}
	else if (type == IMLType::CJUMP_CYCLE_CHECK)
	{
		// no effect on registers
	}
	else if (type == IMLType::NO_OP)
	{
		// no effect on registers
	}
	else if (type == IMLType::MACRO)
	{
		if (operation == IMLOperation::MACRO_BL || operation == IMLOperation::MACRO_B_FAR || operation == IMLOperation::MACRO_LEAVE || operation == IMLOperation::MACRO_DEBUGBREAK || operation == IMLOperation::MACRO_COUNT_CYCLES || operation == IMLOperation::MACRO_HLE)
		{
			// no effect on registers
		}
		else if (operation == IMLOperation::MACRO_B_TO_REG)
		{
			cemu_assert_debug(op_macro.paramReg.IsValid());
			registersUsed->readGPR1 = op_macro.paramReg;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == IMLType::COMPARE)
	{
		registersUsed->readGPR1 = op_compare.regA;
		registersUsed->readGPR2 = op_compare.regB;
		registersUsed->writtenGPR1 = op_compare.regR;
	}
	else if (type == IMLType::COMPARE_S32)
	{
		registersUsed->readGPR1 = op_compare_s32.regA;
		registersUsed->writtenGPR1 = op_compare_s32.regR;
	}
	else if (type == IMLType::CONDITIONAL_JUMP)
	{
		registersUsed->readGPR1 = op_conditional_jump.registerBool;
	}
	else if (type == IMLType::JUMP)
	{
		// no registers affected
	}
	else if (type == IMLType::LOAD)
	{
		registersUsed->writtenGPR1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem.IsValid())
			registersUsed->readGPR1 = op_storeLoad.registerMem;
	}
	else if (type == IMLType::LOAD_INDEXED)
	{
		registersUsed->writtenGPR1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem.IsValid())
			registersUsed->readGPR1 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2.IsValid())
			registersUsed->readGPR2 = op_storeLoad.registerMem2;
	}
	else if (type == IMLType::STORE)
	{
		registersUsed->readGPR1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem.IsValid())
			registersUsed->readGPR2 = op_storeLoad.registerMem;
	}
	else if (type == IMLType::STORE_INDEXED)
	{
		registersUsed->readGPR1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem.IsValid())
			registersUsed->readGPR2 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2.IsValid())
			registersUsed->readGPR3 = op_storeLoad.registerMem2;
	}
	else if (type == IMLType::ATOMIC_CMP_STORE)
	{
		registersUsed->readGPR1 = op_atomic_compare_store.regEA;
		registersUsed->readGPR2 = op_atomic_compare_store.regCompareValue;
		registersUsed->readGPR3 = op_atomic_compare_store.regWriteValue;
		registersUsed->writtenGPR1 = op_atomic_compare_store.regBoolOut;
	}
	else if (type == IMLType::CALL_IMM)
	{
		if (op_call_imm.regParam0.IsValid())
			registersUsed->readGPR1 = op_call_imm.regParam0;
		if (op_call_imm.regParam1.IsValid())
			registersUsed->readGPR2 = op_call_imm.regParam1;
		if (op_call_imm.regParam2.IsValid())
			registersUsed->readGPR3 = op_call_imm.regParam2;
		registersUsed->writtenGPR1 = op_call_imm.regReturn;
	}
	else if (type == IMLType::FPR_LOAD)
	{
		// fpr load operation
		registersUsed->writtenGPR1 = op_storeLoad.registerData;
		// address is in gpr register
		if (op_storeLoad.registerMem.IsValid())
			registersUsed->readGPR1 = op_storeLoad.registerMem;
	}
	else if (type == IMLType::FPR_LOAD_INDEXED)
	{
		// fpr load operation
		registersUsed->writtenGPR1 = op_storeLoad.registerData;
		// address is in gpr registers
		if (op_storeLoad.registerMem.IsValid())
			registersUsed->readGPR1 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2.IsValid())
			registersUsed->readGPR2 = op_storeLoad.registerMem2;
	}
	else if (type == IMLType::FPR_STORE)
	{
		// fpr store operation
		registersUsed->readGPR1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem.IsValid())
			registersUsed->readGPR2 = op_storeLoad.registerMem;
	}
	else if (type == IMLType::FPR_STORE_INDEXED)
	{
		// fpr store operation
		registersUsed->readGPR1 = op_storeLoad.registerData;
		// address is in gpr registers
		if (op_storeLoad.registerMem.IsValid())
			registersUsed->readGPR2 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2.IsValid())
			registersUsed->readGPR3 = op_storeLoad.registerMem2;
	}
	else if (type == IMLType::FPR_R_R)
	{
		// fpr operation
		if (
			operation == IMLOperation::OP_FPR_ASSIGN ||
			operation == IMLOperation::OP_FPR_EXPAND_F32_TO_F64 ||
			operation == IMLOperation::OP_FPR_FCTIWZ
			)
		{
			registersUsed->readGPR1 = op_fpr_r_r.regA;
			registersUsed->writtenGPR1 = op_fpr_r_r.regR;
		}
		else if (operation == IMLOperation::OP_FPR_MULTIPLY ||
			operation == IMLOperation::OP_FPR_DIVIDE ||
			operation == IMLOperation::OP_FPR_ADD ||
			operation == IMLOperation::OP_FPR_SUB)
		{
			registersUsed->readGPR1 = op_fpr_r_r.regA;
			registersUsed->readGPR2 = op_fpr_r_r.regR;
			registersUsed->writtenGPR1 = op_fpr_r_r.regR;

		}
		else if (operation == IMLOperation::OP_FPR_FLOAT_TO_INT ||
			operation == IMLOperation::OP_FPR_INT_TO_FLOAT ||
			operation == IMLOperation::OP_FPR_BITCAST_INT_TO_FLOAT)
		{
			registersUsed->writtenGPR1 = op_fpr_r_r.regR;
			registersUsed->readGPR1 = op_fpr_r_r.regA;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == IMLType::FPR_R_R_R)
	{
		// fpr operation
		registersUsed->readGPR1 = op_fpr_r_r_r.regA;
		registersUsed->readGPR2 = op_fpr_r_r_r.regB;
		registersUsed->writtenGPR1 = op_fpr_r_r_r.regR;
	}
	else if (type == IMLType::FPR_R_R_R_R)
	{
		// fpr operation
		registersUsed->readGPR1 = op_fpr_r_r_r_r.regA;
		registersUsed->readGPR2 = op_fpr_r_r_r_r.regB;
		registersUsed->readGPR3 = op_fpr_r_r_r_r.regC;
		registersUsed->writtenGPR1 = op_fpr_r_r_r_r.regR;
	}
	else if (type == IMLType::FPR_R)
	{
		// fpr operation
		if (operation == IMLOperation::OP_FPR_NEGATE ||
			operation == IMLOperation::OP_FPR_ABS ||
			operation == IMLOperation::OP_FPR_NEGATIVE_ABS ||
			operation == IMLOperation::OP_FPR_EXPAND_F32_TO_F64 ||
			operation == IMLOperation::OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM)
		{
			registersUsed->readGPR1 = op_fpr_r.regR;
			registersUsed->writtenGPR1 = op_fpr_r.regR;
		}
		else if (operation == IMLOperation::OP_FPR_LOAD_ONE)
		{
			registersUsed->writtenGPR1 = op_fpr_r.regR;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == IMLType::FPR_COMPARE)
	{
		registersUsed->writtenGPR1 = op_fpr_compare.regR;
		registersUsed->readGPR1 = op_fpr_compare.regA;
		registersUsed->readGPR2 = op_fpr_compare.regB;
	}
	else if (type == IMLType::X86_EFLAGS_JCC)
	{
		// no registers read or written (except for the implicit eflags)
	}
	else
	{
		cemu_assert_unimplemented();
	}
}

IMLReg replaceRegisterIdMultiple(IMLReg reg, const std::unordered_map<IMLRegID, IMLRegID>& translationTable)
{
	if (reg.IsInvalid())
		return reg;
	const auto& it = translationTable.find(reg.GetRegID());
	cemu_assert_debug(it != translationTable.cend());
	IMLReg alteredReg = reg;
	alteredReg.SetRegID(it->second);
	return alteredReg;
}

void IMLInstruction::RewriteGPR(const std::unordered_map<IMLRegID, IMLRegID>& translationTable)
{
	if (type == IMLType::R_NAME)
	{
		op_r_name.regR = replaceRegisterIdMultiple(op_r_name.regR, translationTable);
	}
	else if (type == IMLType::NAME_R)
	{
		op_r_name.regR = replaceRegisterIdMultiple(op_r_name.regR, translationTable);
	}
	else if (type == IMLType::R_R)
	{
		op_r_r.regR = replaceRegisterIdMultiple(op_r_r.regR, translationTable);
		op_r_r.regA = replaceRegisterIdMultiple(op_r_r.regA, translationTable);
	}
	else if (type == IMLType::R_S32)
	{
		op_r_immS32.regR = replaceRegisterIdMultiple(op_r_immS32.regR, translationTable);
	}
	else if (type == IMLType::R_R_S32)
	{
		op_r_r_s32.regR = replaceRegisterIdMultiple(op_r_r_s32.regR, translationTable);
		op_r_r_s32.regA = replaceRegisterIdMultiple(op_r_r_s32.regA, translationTable);
	}
	else if (type == IMLType::R_R_S32_CARRY)
	{
		op_r_r_s32_carry.regR = replaceRegisterIdMultiple(op_r_r_s32_carry.regR, translationTable);
		op_r_r_s32_carry.regA = replaceRegisterIdMultiple(op_r_r_s32_carry.regA, translationTable);
		op_r_r_s32_carry.regCarry = replaceRegisterIdMultiple(op_r_r_s32_carry.regCarry, translationTable);
	}
	else if (type == IMLType::R_R_R)
	{
		op_r_r_r.regR = replaceRegisterIdMultiple(op_r_r_r.regR, translationTable);
		op_r_r_r.regA = replaceRegisterIdMultiple(op_r_r_r.regA, translationTable);
		op_r_r_r.regB = replaceRegisterIdMultiple(op_r_r_r.regB, translationTable);
	}
	else if (type == IMLType::R_R_R_CARRY)
	{
		op_r_r_r_carry.regR = replaceRegisterIdMultiple(op_r_r_r_carry.regR, translationTable);
		op_r_r_r_carry.regA = replaceRegisterIdMultiple(op_r_r_r_carry.regA, translationTable);
		op_r_r_r_carry.regB = replaceRegisterIdMultiple(op_r_r_r_carry.regB, translationTable);
		op_r_r_r_carry.regCarry = replaceRegisterIdMultiple(op_r_r_r_carry.regCarry, translationTable);
	}
	else if (type == IMLType::COMPARE)
	{
		op_compare.regR = replaceRegisterIdMultiple(op_compare.regR, translationTable);
		op_compare.regA = replaceRegisterIdMultiple(op_compare.regA, translationTable);
		op_compare.regB = replaceRegisterIdMultiple(op_compare.regB, translationTable);
	}
	else if (type == IMLType::COMPARE_S32)
	{
		op_compare_s32.regR = replaceRegisterIdMultiple(op_compare_s32.regR, translationTable);
		op_compare_s32.regA = replaceRegisterIdMultiple(op_compare_s32.regA, translationTable);
	}
	else if (type == IMLType::CONDITIONAL_JUMP)
	{
		op_conditional_jump.registerBool = replaceRegisterIdMultiple(op_conditional_jump.registerBool, translationTable);
	}
	else if (type == IMLType::CJUMP_CYCLE_CHECK || type == IMLType::JUMP)
	{
		// no effect on registers
	}
	else if (type == IMLType::NO_OP)
	{
		// no effect on registers
	}
	else if (type == IMLType::MACRO)
	{
		if (operation == IMLOperation::MACRO_BL || operation == IMLOperation::MACRO_B_FAR || operation == IMLOperation::MACRO_LEAVE || operation == IMLOperation::MACRO_DEBUGBREAK || operation == IMLOperation::MACRO_HLE || operation == IMLOperation::MACRO_COUNT_CYCLES)
		{
			// no effect on registers
		}
		else if (operation == IMLOperation::MACRO_B_TO_REG)
		{
			op_macro.paramReg = replaceRegisterIdMultiple(op_macro.paramReg, translationTable);
		}
		else
		{
			cemu_assert_unimplemented();
		}
	}
	else if (type == IMLType::LOAD)
	{
		op_storeLoad.registerData = replaceRegisterIdMultiple(op_storeLoad.registerData, translationTable);
		if (op_storeLoad.registerMem.IsValid())
		{
			op_storeLoad.registerMem = replaceRegisterIdMultiple(op_storeLoad.registerMem, translationTable);
		}
	}
	else if (type == IMLType::LOAD_INDEXED)
	{
		op_storeLoad.registerData = replaceRegisterIdMultiple(op_storeLoad.registerData, translationTable);
		if (op_storeLoad.registerMem.IsValid())
			op_storeLoad.registerMem = replaceRegisterIdMultiple(op_storeLoad.registerMem, translationTable);
		if (op_storeLoad.registerMem2.IsValid())
			op_storeLoad.registerMem2 = replaceRegisterIdMultiple(op_storeLoad.registerMem2, translationTable);
	}
	else if (type == IMLType::STORE)
	{
		op_storeLoad.registerData = replaceRegisterIdMultiple(op_storeLoad.registerData, translationTable);
		if (op_storeLoad.registerMem.IsValid())
			op_storeLoad.registerMem = replaceRegisterIdMultiple(op_storeLoad.registerMem, translationTable);
	}
	else if (type == IMLType::STORE_INDEXED)
	{
		op_storeLoad.registerData = replaceRegisterIdMultiple(op_storeLoad.registerData, translationTable);
		if (op_storeLoad.registerMem.IsValid())
			op_storeLoad.registerMem = replaceRegisterIdMultiple(op_storeLoad.registerMem, translationTable);
		if (op_storeLoad.registerMem2.IsValid())
			op_storeLoad.registerMem2 = replaceRegisterIdMultiple(op_storeLoad.registerMem2, translationTable);
	}
	else if (type == IMLType::ATOMIC_CMP_STORE)
	{
		op_atomic_compare_store.regEA = replaceRegisterIdMultiple(op_atomic_compare_store.regEA, translationTable);
		op_atomic_compare_store.regCompareValue = replaceRegisterIdMultiple(op_atomic_compare_store.regCompareValue, translationTable);
		op_atomic_compare_store.regWriteValue = replaceRegisterIdMultiple(op_atomic_compare_store.regWriteValue, translationTable);
		op_atomic_compare_store.regBoolOut = replaceRegisterIdMultiple(op_atomic_compare_store.regBoolOut, translationTable);
	}
	else if (type == IMLType::CALL_IMM)
	{
		op_call_imm.regReturn = replaceRegisterIdMultiple(op_call_imm.regReturn, translationTable);
		if (op_call_imm.regParam0.IsValid())
			op_call_imm.regParam0 = replaceRegisterIdMultiple(op_call_imm.regParam0, translationTable);
		if (op_call_imm.regParam1.IsValid())
			op_call_imm.regParam1 = replaceRegisterIdMultiple(op_call_imm.regParam1, translationTable);
		if (op_call_imm.regParam2.IsValid())
			op_call_imm.regParam2 = replaceRegisterIdMultiple(op_call_imm.regParam2, translationTable);
	}
	else if (type == IMLType::FPR_LOAD)
	{
		op_storeLoad.registerData = replaceRegisterIdMultiple(op_storeLoad.registerData, translationTable);
		op_storeLoad.registerMem = replaceRegisterIdMultiple(op_storeLoad.registerMem, translationTable);
	}
	else if (type == IMLType::FPR_LOAD_INDEXED)
	{
		op_storeLoad.registerData = replaceRegisterIdMultiple(op_storeLoad.registerData, translationTable);
		op_storeLoad.registerMem = replaceRegisterIdMultiple(op_storeLoad.registerMem, translationTable);
		op_storeLoad.registerMem2 = replaceRegisterIdMultiple(op_storeLoad.registerMem2, translationTable);
	}
	else if (type == IMLType::FPR_STORE)
	{
		op_storeLoad.registerData = replaceRegisterIdMultiple(op_storeLoad.registerData, translationTable);
		op_storeLoad.registerMem = replaceRegisterIdMultiple(op_storeLoad.registerMem, translationTable);
	}
	else if (type == IMLType::FPR_STORE_INDEXED)
	{
		op_storeLoad.registerData = replaceRegisterIdMultiple(op_storeLoad.registerData, translationTable);
		op_storeLoad.registerMem = replaceRegisterIdMultiple(op_storeLoad.registerMem, translationTable);
		op_storeLoad.registerMem2 = replaceRegisterIdMultiple(op_storeLoad.registerMem2, translationTable);
	}
	else if (type == IMLType::FPR_R)
	{
		op_fpr_r.regR = replaceRegisterIdMultiple(op_fpr_r.regR, translationTable);
	}
	else if (type == IMLType::FPR_R_R)
	{
		op_fpr_r_r.regR = replaceRegisterIdMultiple(op_fpr_r_r.regR, translationTable);
		op_fpr_r_r.regA = replaceRegisterIdMultiple(op_fpr_r_r.regA, translationTable);
	}
	else if (type == IMLType::FPR_R_R_R)
	{
		op_fpr_r_r_r.regR = replaceRegisterIdMultiple(op_fpr_r_r_r.regR, translationTable);
		op_fpr_r_r_r.regA = replaceRegisterIdMultiple(op_fpr_r_r_r.regA, translationTable);
		op_fpr_r_r_r.regB = replaceRegisterIdMultiple(op_fpr_r_r_r.regB, translationTable);
	}
	else if (type == IMLType::FPR_R_R_R_R)
	{
		op_fpr_r_r_r_r.regR = replaceRegisterIdMultiple(op_fpr_r_r_r_r.regR, translationTable);
		op_fpr_r_r_r_r.regA = replaceRegisterIdMultiple(op_fpr_r_r_r_r.regA, translationTable);
		op_fpr_r_r_r_r.regB = replaceRegisterIdMultiple(op_fpr_r_r_r_r.regB, translationTable);
		op_fpr_r_r_r_r.regC = replaceRegisterIdMultiple(op_fpr_r_r_r_r.regC, translationTable);
	}
	else if (type == IMLType::FPR_COMPARE)
	{
		op_fpr_compare.regA = replaceRegisterIdMultiple(op_fpr_compare.regA, translationTable);
		op_fpr_compare.regB = replaceRegisterIdMultiple(op_fpr_compare.regB, translationTable);
		op_fpr_compare.regR = replaceRegisterIdMultiple(op_fpr_compare.regR, translationTable);
	}
	else if (type == IMLType::X86_EFLAGS_JCC)
	{
		// no registers read or written (except for the implicit eflags)
	}
	else
	{
		cemu_assert_unimplemented();
	}
}
