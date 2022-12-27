#include "IMLInstruction.h"
#include "IML.h"

#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"

void IMLInstruction::CheckRegisterUsage(IMLUsedRegisters* registersUsed) const
{
	registersUsed->readNamedReg1 = -1;
	registersUsed->readNamedReg2 = -1;
	registersUsed->readNamedReg3 = -1;
	registersUsed->writtenNamedReg1 = -1;
	registersUsed->writtenNamedReg2 = -1;
	registersUsed->readFPR1 = -1;
	registersUsed->readFPR2 = -1;
	registersUsed->readFPR3 = -1;
	registersUsed->readFPR4 = -1;
	registersUsed->writtenFPR1 = -1;
	if (type == PPCREC_IML_TYPE_R_NAME)
	{
		registersUsed->writtenNamedReg1 = op_r_name.registerIndex;
	}
	else if (type == PPCREC_IML_TYPE_NAME_R)
	{
		registersUsed->readNamedReg1 = op_r_name.registerIndex;
	}
	else if (type == PPCREC_IML_TYPE_R_R)
	{
		if (operation == PPCREC_IML_OP_COMPARE_SIGNED || operation == PPCREC_IML_OP_COMPARE_UNSIGNED || operation == PPCREC_IML_OP_DCBZ)
		{
			// both operands are read only
			registersUsed->readNamedReg1 = op_r_r.registerResult;
			registersUsed->readNamedReg2 = op_r_r.registerA;
		}
		else if (
			operation == PPCREC_IML_OP_OR ||
			operation == PPCREC_IML_OP_AND ||
			operation == PPCREC_IML_OP_XOR)
		{
			// result is read and written, operand is read
			registersUsed->writtenNamedReg1 = op_r_r.registerResult;
			registersUsed->readNamedReg1 = op_r_r.registerResult;
			registersUsed->readNamedReg2 = op_r_r.registerA;
		}
		else if (
			operation == PPCREC_IML_OP_ASSIGN ||
			operation == PPCREC_IML_OP_ENDIAN_SWAP ||
			operation == PPCREC_IML_OP_CNTLZW ||
			operation == PPCREC_IML_OP_NOT ||
			operation == PPCREC_IML_OP_NEG ||
			operation == PPCREC_IML_OP_ASSIGN_S16_TO_S32 ||
			operation == PPCREC_IML_OP_ASSIGN_S8_TO_S32)
		{
			// result is written, operand is read
			registersUsed->writtenNamedReg1 = op_r_r.registerResult;
			registersUsed->readNamedReg1 = op_r_r.registerA;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == PPCREC_IML_TYPE_R_S32)
	{
		if (operation == PPCREC_IML_OP_COMPARE_SIGNED || operation == PPCREC_IML_OP_COMPARE_UNSIGNED || operation == PPCREC_IML_OP_MTCRF)
		{
			// operand register is read only
			registersUsed->readNamedReg1 = op_r_immS32.registerIndex;
		}
		else if (operation == PPCREC_IML_OP_ADD || // deprecated
			operation == PPCREC_IML_OP_SUB ||
			operation == PPCREC_IML_OP_AND ||
			operation == PPCREC_IML_OP_OR ||
			operation == PPCREC_IML_OP_XOR ||
			operation == PPCREC_IML_OP_LEFT_ROTATE)
		{
			// operand register is read and write
			registersUsed->readNamedReg1 = op_r_immS32.registerIndex;
			registersUsed->writtenNamedReg1 = op_r_immS32.registerIndex;
		}
		else
		{
			// operand register is write only
			// todo - use explicit lists, avoid default cases
			registersUsed->writtenNamedReg1 = op_r_immS32.registerIndex;
		}
	}
	else if (type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
	{
		if (operation == PPCREC_IML_OP_ASSIGN)
		{
			// result is written, but also considered read (in case the condition fails)
			registersUsed->readNamedReg1 = op_conditional_r_s32.registerIndex;
			registersUsed->writtenNamedReg1 = op_conditional_r_s32.registerIndex;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == PPCREC_IML_TYPE_R_R_S32)
	{
		if (operation == PPCREC_IML_OP_RLWIMI)
		{
			// result and operand register are both read, result is written
			registersUsed->writtenNamedReg1 = op_r_r_s32.registerResult;
			registersUsed->readNamedReg1 = op_r_r_s32.registerResult;
			registersUsed->readNamedReg2 = op_r_r_s32.registerA;
		}
		else
		{
			// result is write only and operand is read only
			registersUsed->writtenNamedReg1 = op_r_r_s32.registerResult;
			registersUsed->readNamedReg1 = op_r_r_s32.registerA;
		}
	}
	else if (type == PPCREC_IML_TYPE_R_R_S32_CARRY)
	{
		registersUsed->writtenNamedReg1 = op_r_r_s32_carry.regR;
		registersUsed->readNamedReg1 = op_r_r_s32_carry.regA;
		// some operations read carry
		switch (operation)
		{
		case PPCREC_IML_OP_ADD_WITH_CARRY:
			registersUsed->readNamedReg2 = op_r_r_s32_carry.regCarry;
			break;
		case PPCREC_IML_OP_ADD:
			break;
		default:
			cemu_assert_unimplemented();
		}
		// carry is always written
		registersUsed->writtenNamedReg2 = op_r_r_s32_carry.regCarry;
	}
	else if (type == PPCREC_IML_TYPE_R_R_R)
	{
		// in all cases result is written and other operands are read only
		registersUsed->writtenNamedReg1 = op_r_r_r.registerResult;
		registersUsed->readNamedReg1 = op_r_r_r.registerA;
		registersUsed->readNamedReg2 = op_r_r_r.registerB;
	}
	else if (type == PPCREC_IML_TYPE_R_R_R_CARRY)
	{
		registersUsed->writtenNamedReg1 = op_r_r_r_carry.regR;
		registersUsed->readNamedReg1 = op_r_r_r_carry.regA;
		registersUsed->readNamedReg2 = op_r_r_r_carry.regB;
		// some operations read carry
		switch (operation)
		{
		case PPCREC_IML_OP_ADD_WITH_CARRY:
			registersUsed->readNamedReg3 = op_r_r_r_carry.regCarry;
			break;
		case PPCREC_IML_OP_ADD:
			break;
		default:
			cemu_assert_unimplemented();
		}
		// carry is always written
		registersUsed->writtenNamedReg2 = op_r_r_r_carry.regCarry;
	}
	else if (type == PPCREC_IML_TYPE_CJUMP || type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
	{
		// no effect on registers
	}
	else if (type == PPCREC_IML_TYPE_NO_OP)
	{
		// no effect on registers
	}
	else if (type == PPCREC_IML_TYPE_MACRO)
	{
		if (operation == PPCREC_IML_MACRO_BL || operation == PPCREC_IML_MACRO_B_FAR || operation == PPCREC_IML_MACRO_LEAVE || operation == PPCREC_IML_MACRO_DEBUGBREAK || operation == PPCREC_IML_MACRO_COUNT_CYCLES || operation == PPCREC_IML_MACRO_HLE || operation == PPCREC_IML_MACRO_MFTB)
		{
			// no effect on registers
		}
		else if (operation == PPCREC_IML_MACRO_B_TO_REG)
		{
			registersUsed->readNamedReg1 = op_macro.param;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == PPCREC_IML_TYPE_COMPARE)
	{
		registersUsed->readNamedReg1 = op_compare.registerOperandA;
		registersUsed->readNamedReg2 = op_compare.registerOperandB;
		registersUsed->writtenNamedReg1 = op_compare.registerResult;
	}
	else if (type == PPCREC_IML_TYPE_COMPARE_S32)
	{
		registersUsed->readNamedReg1 = op_compare_s32.registerOperandA;
		registersUsed->writtenNamedReg1 = op_compare_s32.registerResult;
	}
	else if (type == PPCREC_IML_TYPE_CONDITIONAL_JUMP)
	{
		registersUsed->readNamedReg1 = op_conditionalJump2.registerBool;
	}
	else if (type == PPCREC_IML_TYPE_JUMP)
	{
		// no registers affected
	}
	else if (type == PPCREC_IML_TYPE_LOAD)
	{
		registersUsed->writtenNamedReg1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg1 = op_storeLoad.registerMem;
	}
	else if (type == PPCREC_IML_TYPE_LOAD_INDEXED)
	{
		registersUsed->writtenNamedReg1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg1 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg2 = op_storeLoad.registerMem2;
	}
	else if (type == PPCREC_IML_TYPE_STORE)
	{
		registersUsed->readNamedReg1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg2 = op_storeLoad.registerMem;
	}
	else if (type == PPCREC_IML_TYPE_STORE_INDEXED)
	{
		registersUsed->readNamedReg1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg2 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg3 = op_storeLoad.registerMem2;
	}
	else if (type == PPCREC_IML_TYPE_CR)
	{
		// only affects cr register	
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_NAME)
	{
		// fpr operation
		registersUsed->writtenFPR1 = op_r_name.registerIndex;
	}
	else if (type == PPCREC_IML_TYPE_FPR_NAME_R)
	{
		// fpr operation
		registersUsed->readFPR1 = op_r_name.registerIndex;
	}
	else if (type == PPCREC_IML_TYPE_FPR_LOAD)
	{
		// fpr load operation
		registersUsed->writtenFPR1 = op_storeLoad.registerData;
		// address is in gpr register
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg1 = op_storeLoad.registerMem;
		// determine partially written result
		switch (op_storeLoad.mode)
		{
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readNamedReg2 = op_storeLoad.registerGQR;
			break;
		case PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0:
			// PS1 remains the same
			registersUsed->readFPR4 = op_storeLoad.registerData;
			cemu_assert_debug(op_storeLoad.registerGQR == PPC_REC_INVALID_REGISTER);
			break;
		case PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_S16_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_U16_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_U8_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_S8_PS0:
			cemu_assert_debug(op_storeLoad.registerGQR == PPC_REC_INVALID_REGISTER);
			break;
		default:
			cemu_assert_unimplemented();
		}
	}
	else if (type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED)
	{
		// fpr load operation
		registersUsed->writtenFPR1 = op_storeLoad.registerData;
		// address is in gpr registers
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg1 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg2 = op_storeLoad.registerMem2;
		// determine partially written result
		switch (op_storeLoad.mode)
		{
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readNamedReg3 = op_storeLoad.registerGQR;
			break;
		case PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0:
			// PS1 remains the same
			cemu_assert_debug(op_storeLoad.registerGQR == PPC_REC_INVALID_REGISTER);
			registersUsed->readFPR4 = op_storeLoad.registerData;
			break;
		case PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_S16_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_U16_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1:
		case PPCREC_FPR_LD_MODE_PSQ_U8_PS0:
			cemu_assert_debug(op_storeLoad.registerGQR == PPC_REC_INVALID_REGISTER);
			break;
		default:
			cemu_assert_unimplemented();
		}
	}
	else if (type == PPCREC_IML_TYPE_FPR_STORE)
	{
		// fpr store operation
		registersUsed->readFPR1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg1 = op_storeLoad.registerMem;
		// PSQ generic stores also access GQR
		switch (op_storeLoad.mode)
		{
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readNamedReg2 = op_storeLoad.registerGQR;
			break;
		default:
			cemu_assert_debug(op_storeLoad.registerGQR == PPC_REC_INVALID_REGISTER);
			break;
		}
	}
	else if (type == PPCREC_IML_TYPE_FPR_STORE_INDEXED)
	{
		// fpr store operation
		registersUsed->readFPR1 = op_storeLoad.registerData;
		// address is in gpr registers
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg1 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg2 = op_storeLoad.registerMem2;
		// PSQ generic stores also access GQR
		switch (op_storeLoad.mode)
		{
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readNamedReg3 = op_storeLoad.registerGQR;
			break;
		default:
			cemu_assert_debug(op_storeLoad.registerGQR == PPC_REC_INVALID_REGISTER);
			break;
		}
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R)
	{
		// fpr operation
		if (operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP ||
			operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP ||
			operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED ||
			operation == PPCREC_IML_OP_ASSIGN ||
			operation == PPCREC_IML_OP_FPR_BOTTOM_FRES_TO_BOTTOM_AND_TOP ||
			operation == PPCREC_IML_OP_FPR_NEGATE_PAIR ||
			operation == PPCREC_IML_OP_FPR_ABS_PAIR ||
			operation == PPCREC_IML_OP_FPR_FRES_PAIR ||
			operation == PPCREC_IML_OP_FPR_FRSQRTE_PAIR)
		{
			// operand read, result written
			registersUsed->readFPR1 = op_fpr_r_r.registerOperand;
			registersUsed->writtenFPR1 = op_fpr_r_r.registerResult;
		}
		else if (
			operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM ||
			operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_TOP ||
			operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_TOP ||
			operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM ||
			operation == PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64 ||
			operation == PPCREC_IML_OP_FPR_BOTTOM_FCTIWZ ||
			operation == PPCREC_IML_OP_FPR_BOTTOM_RECIPROCAL_SQRT
			)
		{
			// operand read, result read and (partially) written
			registersUsed->readFPR1 = op_fpr_r_r.registerOperand;
			registersUsed->readFPR4 = op_fpr_r_r.registerResult;
			registersUsed->writtenFPR1 = op_fpr_r_r.registerResult;
		}
		else if (operation == PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM ||
			operation == PPCREC_IML_OP_FPR_MULTIPLY_PAIR ||
			operation == PPCREC_IML_OP_FPR_DIVIDE_BOTTOM ||
			operation == PPCREC_IML_OP_FPR_DIVIDE_PAIR ||
			operation == PPCREC_IML_OP_FPR_ADD_BOTTOM ||
			operation == PPCREC_IML_OP_FPR_ADD_PAIR ||
			operation == PPCREC_IML_OP_FPR_SUB_PAIR ||
			operation == PPCREC_IML_OP_FPR_SUB_BOTTOM)
		{
			// operand read, result read and written
			registersUsed->readFPR1 = op_fpr_r_r.registerOperand;
			registersUsed->readFPR2 = op_fpr_r_r.registerResult;
			registersUsed->writtenFPR1 = op_fpr_r_r.registerResult;

		}
		else if (operation == PPCREC_IML_OP_FPR_FCMPU_BOTTOM ||
			operation == PPCREC_IML_OP_FPR_FCMPU_TOP ||
			operation == PPCREC_IML_OP_FPR_FCMPO_BOTTOM)
		{
			// operand read, result read
			registersUsed->readFPR1 = op_fpr_r_r.registerOperand;
			registersUsed->readFPR2 = op_fpr_r_r.registerResult;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R)
	{
		// fpr operation
		registersUsed->readFPR1 = op_fpr_r_r_r.registerOperandA;
		registersUsed->readFPR2 = op_fpr_r_r_r.registerOperandB;
		registersUsed->writtenFPR1 = op_fpr_r_r_r.registerResult;
		// handle partially written result
		switch (operation)
		{
		case PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM:
		case PPCREC_IML_OP_FPR_ADD_BOTTOM:
		case PPCREC_IML_OP_FPR_SUB_BOTTOM:
			registersUsed->readFPR4 = op_fpr_r_r_r.registerResult;
			break;
		case PPCREC_IML_OP_FPR_SUB_PAIR:
			break;
		default:
			cemu_assert_unimplemented();
		}
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R_R)
	{
		// fpr operation
		registersUsed->readFPR1 = op_fpr_r_r_r_r.registerOperandA;
		registersUsed->readFPR2 = op_fpr_r_r_r_r.registerOperandB;
		registersUsed->readFPR3 = op_fpr_r_r_r_r.registerOperandC;
		registersUsed->writtenFPR1 = op_fpr_r_r_r_r.registerResult;
		// handle partially written result
		switch (operation)
		{
		case PPCREC_IML_OP_FPR_SELECT_BOTTOM:
			registersUsed->readFPR4 = op_fpr_r_r_r_r.registerResult;
			break;
		case PPCREC_IML_OP_FPR_SUM0:
		case PPCREC_IML_OP_FPR_SUM1:
		case PPCREC_IML_OP_FPR_SELECT_PAIR:
			break;
		default:
			cemu_assert_unimplemented();
		}
	}
	else if (type == PPCREC_IML_TYPE_FPR_R)
	{
		// fpr operation
		if (operation == PPCREC_IML_OP_FPR_NEGATE_BOTTOM ||
			operation == PPCREC_IML_OP_FPR_ABS_BOTTOM ||
			operation == PPCREC_IML_OP_FPR_NEGATIVE_ABS_BOTTOM ||
			operation == PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64 ||
			operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM ||
			operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_PAIR)
		{
			registersUsed->readFPR1 = op_fpr_r.registerResult;
			registersUsed->writtenFPR1 = op_fpr_r.registerResult;
		}
		else
			cemu_assert_unimplemented();
	}
	else
	{
		cemu_assert_unimplemented();
	}
}

#define replaceRegister(__x,__r,__n) (((__x)==(__r))?(__n):(__x))

sint32 replaceRegisterMultiple(sint32 reg, const std::unordered_map<IMLReg, IMLReg>& translationTable)
{
	const auto& it = translationTable.find(reg);
	cemu_assert_debug(it != translationTable.cend());
	return it->second;
}

sint32 replaceRegisterMultiple(sint32 reg, sint32 match[4], sint32 replaced[4])
{
	// deprecated but still used for FPRs
	for (sint32 i = 0; i < 4; i++)
	{
		if (match[i] < 0)
			continue;
		if (reg == match[i])
		{
			return replaced[i];
		}
	}
	return reg;
}

//void IMLInstruction::ReplaceGPR(sint32 gprRegisterSearched[4], sint32 gprRegisterReplaced[4])
void IMLInstruction::RewriteGPR(const std::unordered_map<IMLReg, IMLReg>& translationTable)
{
	if (type == PPCREC_IML_TYPE_R_NAME)
	{
		op_r_name.registerIndex = replaceRegisterMultiple(op_r_name.registerIndex, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_NAME_R)
	{
		op_r_name.registerIndex = replaceRegisterMultiple(op_r_name.registerIndex, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_R_R)
	{
		op_r_r.registerResult = replaceRegisterMultiple(op_r_r.registerResult, translationTable);
		op_r_r.registerA = replaceRegisterMultiple(op_r_r.registerA, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_R_S32)
	{
		op_r_immS32.registerIndex = replaceRegisterMultiple(op_r_immS32.registerIndex, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
	{
		op_conditional_r_s32.registerIndex = replaceRegisterMultiple(op_conditional_r_s32.registerIndex, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_R_R_S32)
	{
		op_r_r_s32.registerResult = replaceRegisterMultiple(op_r_r_s32.registerResult, translationTable);
		op_r_r_s32.registerA = replaceRegisterMultiple(op_r_r_s32.registerA, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_R_R_S32_CARRY)
	{
		op_r_r_s32_carry.regR = replaceRegisterMultiple(op_r_r_s32_carry.regR, translationTable);
		op_r_r_s32_carry.regA = replaceRegisterMultiple(op_r_r_s32_carry.regA, translationTable);
		op_r_r_s32_carry.regCarry = replaceRegisterMultiple(op_r_r_s32_carry.regCarry, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_R_R_R)
	{
		op_r_r_r.registerResult = replaceRegisterMultiple(op_r_r_r.registerResult, translationTable);
		op_r_r_r.registerA = replaceRegisterMultiple(op_r_r_r.registerA, translationTable);
		op_r_r_r.registerB = replaceRegisterMultiple(op_r_r_r.registerB, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_R_R_R_CARRY)
	{
		op_r_r_r_carry.regR = replaceRegisterMultiple(op_r_r_r_carry.regR, translationTable);
		op_r_r_r_carry.regA = replaceRegisterMultiple(op_r_r_r_carry.regA, translationTable);
		op_r_r_r_carry.regB = replaceRegisterMultiple(op_r_r_r_carry.regB, translationTable);
		op_r_r_r_carry.regCarry = replaceRegisterMultiple(op_r_r_r_carry.regCarry, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_COMPARE)
	{
		op_compare.registerResult = replaceRegisterMultiple(op_compare.registerResult, translationTable);
		op_compare.registerOperandA = replaceRegisterMultiple(op_compare.registerOperandA, translationTable);
		op_compare.registerOperandB = replaceRegisterMultiple(op_compare.registerOperandB, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_COMPARE_S32)
	{
		op_compare_s32.registerResult = replaceRegisterMultiple(op_compare_s32.registerResult, translationTable);
		op_compare_s32.registerOperandA = replaceRegisterMultiple(op_compare_s32.registerOperandA, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_CONDITIONAL_JUMP)
	{
		op_conditionalJump2.registerBool = replaceRegisterMultiple(op_conditionalJump2.registerBool, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_CJUMP || type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK || type == PPCREC_IML_TYPE_JUMP)
	{
		// no effect on registers
	}
	else if (type == PPCREC_IML_TYPE_NO_OP)
	{
		// no effect on registers
	}
	else if (type == PPCREC_IML_TYPE_MACRO)
	{
		if (operation == PPCREC_IML_MACRO_BL || operation == PPCREC_IML_MACRO_B_FAR || operation == PPCREC_IML_MACRO_LEAVE || operation == PPCREC_IML_MACRO_DEBUGBREAK || operation == PPCREC_IML_MACRO_HLE || operation == PPCREC_IML_MACRO_MFTB || operation == PPCREC_IML_MACRO_COUNT_CYCLES)
		{
			// no effect on registers
		}
		else if (operation == PPCREC_IML_MACRO_B_TO_REG)
		{
			op_macro.param = replaceRegisterMultiple(op_macro.param, translationTable);
		}
		else
		{
			cemu_assert_unimplemented();
		}
	}
	else if (type == PPCREC_IML_TYPE_LOAD)
	{
		op_storeLoad.registerData = replaceRegisterMultiple(op_storeLoad.registerData, translationTable);
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
		{
			op_storeLoad.registerMem = replaceRegisterMultiple(op_storeLoad.registerMem, translationTable);
		}
	}
	else if (type == PPCREC_IML_TYPE_LOAD_INDEXED)
	{
		op_storeLoad.registerData = replaceRegisterMultiple(op_storeLoad.registerData, translationTable);
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			op_storeLoad.registerMem = replaceRegisterMultiple(op_storeLoad.registerMem, translationTable);
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			op_storeLoad.registerMem2 = replaceRegisterMultiple(op_storeLoad.registerMem2, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_STORE)
	{
		op_storeLoad.registerData = replaceRegisterMultiple(op_storeLoad.registerData, translationTable);
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			op_storeLoad.registerMem = replaceRegisterMultiple(op_storeLoad.registerMem, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_STORE_INDEXED)
	{
		op_storeLoad.registerData = replaceRegisterMultiple(op_storeLoad.registerData, translationTable);
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			op_storeLoad.registerMem = replaceRegisterMultiple(op_storeLoad.registerMem, translationTable);
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			op_storeLoad.registerMem2 = replaceRegisterMultiple(op_storeLoad.registerMem2, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_CR)
	{
		// only affects cr register	
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_NAME)
	{

	}
	else if (type == PPCREC_IML_TYPE_FPR_NAME_R)
	{

	}
	else if (type == PPCREC_IML_TYPE_FPR_LOAD)
	{
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
		{
			op_storeLoad.registerMem = replaceRegisterMultiple(op_storeLoad.registerMem, translationTable);
		}
		if (op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER)
		{
			op_storeLoad.registerGQR = replaceRegisterMultiple(op_storeLoad.registerGQR, translationTable);
		}
	}
	else if (type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED)
	{
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
		{
			op_storeLoad.registerMem = replaceRegisterMultiple(op_storeLoad.registerMem, translationTable);
		}
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
		{
			op_storeLoad.registerMem2 = replaceRegisterMultiple(op_storeLoad.registerMem2, translationTable);
		}
		if (op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER)
		{
			op_storeLoad.registerGQR = replaceRegisterMultiple(op_storeLoad.registerGQR, translationTable);
		}
	}
	else if (type == PPCREC_IML_TYPE_FPR_STORE)
	{
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
		{
			op_storeLoad.registerMem = replaceRegisterMultiple(op_storeLoad.registerMem, translationTable);
		}
		if (op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER)
		{
			op_storeLoad.registerGQR = replaceRegisterMultiple(op_storeLoad.registerGQR, translationTable);
		}
	}
	else if (type == PPCREC_IML_TYPE_FPR_STORE_INDEXED)
	{
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
		{
			op_storeLoad.registerMem = replaceRegisterMultiple(op_storeLoad.registerMem, translationTable);
		}
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
		{
			op_storeLoad.registerMem2 = replaceRegisterMultiple(op_storeLoad.registerMem2, translationTable);
		}
		if (op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER)
		{
			op_storeLoad.registerGQR = replaceRegisterMultiple(op_storeLoad.registerGQR, translationTable);
		}
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R)
	{
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R)
	{
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R_R)
	{
	}
	else if (type == PPCREC_IML_TYPE_FPR_R)
	{
	}
	else
	{
		cemu_assert_unimplemented();
	}
}

void IMLInstruction::ReplaceFPRs(sint32 fprRegisterSearched[4], sint32 fprRegisterReplaced[4])
{
	if (type == PPCREC_IML_TYPE_R_NAME)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_NAME_R)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_R_R)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_R_S32)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_R_R_S32)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_R_R_R)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_COMPARE || type == PPCREC_IML_TYPE_COMPARE_S32 || type == PPCREC_IML_TYPE_CONDITIONAL_JUMP || type == PPCREC_IML_TYPE_JUMP)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_CJUMP || type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_NO_OP)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_MACRO)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_LOAD)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_LOAD_INDEXED)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_STORE)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_STORE_INDEXED)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_CR)
	{
		// only affects cr register	
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_NAME)
	{
		op_r_name.registerIndex = replaceRegisterMultiple(op_r_name.registerIndex, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_NAME_R)
	{
		op_r_name.registerIndex = replaceRegisterMultiple(op_r_name.registerIndex, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_LOAD)
	{
		op_storeLoad.registerData = replaceRegisterMultiple(op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED)
	{
		op_storeLoad.registerData = replaceRegisterMultiple(op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_STORE)
	{
		op_storeLoad.registerData = replaceRegisterMultiple(op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_STORE_INDEXED)
	{
		op_storeLoad.registerData = replaceRegisterMultiple(op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R)
	{
		op_fpr_r_r.registerResult = replaceRegisterMultiple(op_fpr_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r.registerOperand = replaceRegisterMultiple(op_fpr_r_r.registerOperand, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R)
	{
		op_fpr_r_r_r.registerResult = replaceRegisterMultiple(op_fpr_r_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r.registerOperandA = replaceRegisterMultiple(op_fpr_r_r_r.registerOperandA, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r.registerOperandB = replaceRegisterMultiple(op_fpr_r_r_r.registerOperandB, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R_R)
	{
		op_fpr_r_r_r_r.registerResult = replaceRegisterMultiple(op_fpr_r_r_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.registerOperandA = replaceRegisterMultiple(op_fpr_r_r_r_r.registerOperandA, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.registerOperandB = replaceRegisterMultiple(op_fpr_r_r_r_r.registerOperandB, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.registerOperandC = replaceRegisterMultiple(op_fpr_r_r_r_r.registerOperandC, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R)
	{
		op_fpr_r.registerResult = replaceRegisterMultiple(op_fpr_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
	}
	else
	{
		cemu_assert_unimplemented();
	}
}

void IMLInstruction::ReplaceFPR(sint32 fprRegisterSearched, sint32 fprRegisterReplaced)
{
	if (type == PPCREC_IML_TYPE_R_NAME)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_NAME_R)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_R_R)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_R_S32)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_R_R_S32 || type == PPCREC_IML_TYPE_R_R_S32_CARRY)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_R_R_R || type == PPCREC_IML_TYPE_R_R_R_CARRY)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_COMPARE || type == PPCREC_IML_TYPE_COMPARE_S32 || type == PPCREC_IML_TYPE_CONDITIONAL_JUMP || type == PPCREC_IML_TYPE_JUMP)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_CJUMP || type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_NO_OP)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_MACRO)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_LOAD)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_LOAD_INDEXED)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_STORE)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_STORE_INDEXED)
	{
		// not affected
	}
	else if (type == PPCREC_IML_TYPE_CR)
	{
		// only affects cr register	
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_NAME)
	{
		op_r_name.registerIndex = replaceRegister(op_r_name.registerIndex, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_NAME_R)
	{
		op_r_name.registerIndex = replaceRegister(op_r_name.registerIndex, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_LOAD)
	{
		op_storeLoad.registerData = replaceRegister(op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED)
	{
		op_storeLoad.registerData = replaceRegister(op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_STORE)
	{
		op_storeLoad.registerData = replaceRegister(op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_STORE_INDEXED)
	{
		op_storeLoad.registerData = replaceRegister(op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R)
	{
		op_fpr_r_r.registerResult = replaceRegister(op_fpr_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r.registerOperand = replaceRegister(op_fpr_r_r.registerOperand, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R)
	{
		op_fpr_r_r_r.registerResult = replaceRegister(op_fpr_r_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r.registerOperandA = replaceRegister(op_fpr_r_r_r.registerOperandA, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r.registerOperandB = replaceRegister(op_fpr_r_r_r.registerOperandB, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R_R)
	{
		op_fpr_r_r_r_r.registerResult = replaceRegister(op_fpr_r_r_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.registerOperandA = replaceRegister(op_fpr_r_r_r_r.registerOperandA, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.registerOperandB = replaceRegister(op_fpr_r_r_r_r.registerOperandB, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.registerOperandC = replaceRegister(op_fpr_r_r_r_r.registerOperandC, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R)
	{
		op_fpr_r.registerResult = replaceRegister(op_fpr_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
	}
	else
	{
		cemu_assert_unimplemented();
	}
}
