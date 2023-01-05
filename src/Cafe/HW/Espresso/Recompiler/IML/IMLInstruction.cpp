#include "IMLInstruction.h"
#include "IML.h"

#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"

void IMLInstruction::CheckRegisterUsage(IMLUsedRegisters* registersUsed) const
{
	registersUsed->readGPR1 = IMLREG_INVALID;
	registersUsed->readGPR2 = IMLREG_INVALID;
	registersUsed->readGPR3 = IMLREG_INVALID;
	registersUsed->writtenGPR1 = IMLREG_INVALID;
	registersUsed->writtenGPR2 = IMLREG_INVALID;
	registersUsed->readFPR1 = IMLREG_INVALID;
	registersUsed->readFPR2 = IMLREG_INVALID;
	registersUsed->readFPR3 = IMLREG_INVALID;
	registersUsed->readFPR4 = IMLREG_INVALID;
	registersUsed->writtenFPR1 = IMLREG_INVALID;
	if (type == PPCREC_IML_TYPE_R_NAME)
	{
		registersUsed->writtenGPR1 = op_r_name.regR;
	}
	else if (type == PPCREC_IML_TYPE_NAME_R)
	{
		registersUsed->readGPR1 = op_r_name.regR;
	}
	else if (type == PPCREC_IML_TYPE_R_R)
	{
		if (operation == PPCREC_IML_OP_DCBZ)
		{
			// both operands are read only
			registersUsed->readGPR1 = op_r_r.regR;
			registersUsed->readGPR2 = op_r_r.regA;
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
			registersUsed->writtenGPR1 = op_r_r.regR;
			registersUsed->readGPR1 = op_r_r.regA;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == PPCREC_IML_TYPE_R_S32)
	{
		cemu_assert_debug(operation != PPCREC_IML_OP_ADD &&
			operation != PPCREC_IML_OP_SUB &&
			operation != PPCREC_IML_OP_AND &&
			operation != PPCREC_IML_OP_OR &&
			operation != PPCREC_IML_OP_XOR); // deprecated, use r_r_s32 for these

		if (operation == PPCREC_IML_OP_MTCRF)
		{
			// operand register is read only
			registersUsed->readGPR1 = op_r_immS32.regR;
		}
		else if (operation == PPCREC_IML_OP_LEFT_ROTATE)
		{
			// operand register is read and write
			registersUsed->readGPR1 = op_r_immS32.regR;
			registersUsed->writtenGPR1 = op_r_immS32.regR;
		}
		else
		{
			// operand register is write only
			// todo - use explicit lists, avoid default cases
			registersUsed->writtenGPR1 = op_r_immS32.regR;
		}
	}
	else if (type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
	{
		if (operation == PPCREC_IML_OP_ASSIGN)
		{
			// result is written, but also considered read (in case the condition is false the input is preserved)
			registersUsed->readGPR1 = op_conditional_r_s32.regR;
			registersUsed->writtenGPR1 = op_conditional_r_s32.regR;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == PPCREC_IML_TYPE_R_R_S32)
	{
		if (operation == PPCREC_IML_OP_RLWIMI)
		{
			// result and operand register are both read, result is written
			registersUsed->writtenGPR1 = op_r_r_s32.regR;
			registersUsed->readGPR1 = op_r_r_s32.regR;
			registersUsed->readGPR2 = op_r_r_s32.regA;
		}
		else
		{
			// result is write only and operand is read only
			registersUsed->writtenGPR1 = op_r_r_s32.regR;
			registersUsed->readGPR1 = op_r_r_s32.regA;
		}
	}
	else if (type == PPCREC_IML_TYPE_R_R_S32_CARRY)
	{
		registersUsed->writtenGPR1 = op_r_r_s32_carry.regR;
		registersUsed->readGPR1 = op_r_r_s32_carry.regA;
		// some operations read carry
		switch (operation)
		{
		case PPCREC_IML_OP_ADD_WITH_CARRY:
			registersUsed->readGPR2 = op_r_r_s32_carry.regCarry;
			break;
		case PPCREC_IML_OP_ADD:
			break;
		default:
			cemu_assert_unimplemented();
		}
		// carry is always written
		registersUsed->writtenGPR2 = op_r_r_s32_carry.regCarry;
	}
	else if (type == PPCREC_IML_TYPE_R_R_R)
	{
		// in all cases result is written and other operands are read only
		registersUsed->writtenGPR1 = op_r_r_r.regR;
		registersUsed->readGPR1 = op_r_r_r.regA;
		registersUsed->readGPR2 = op_r_r_r.regB;
	}
	else if (type == PPCREC_IML_TYPE_R_R_R_CARRY)
	{
		registersUsed->writtenGPR1 = op_r_r_r_carry.regR;
		registersUsed->readGPR1 = op_r_r_r_carry.regA;
		registersUsed->readGPR2 = op_r_r_r_carry.regB;
		// some operations read carry
		switch (operation)
		{
		case PPCREC_IML_OP_ADD_WITH_CARRY:
			registersUsed->readGPR3 = op_r_r_r_carry.regCarry;
			break;
		case PPCREC_IML_OP_ADD:
			break;
		default:
			cemu_assert_unimplemented();
		}
		// carry is always written
		registersUsed->writtenGPR2 = op_r_r_r_carry.regCarry;
	}
	else if (type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
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
			registersUsed->readGPR1 = op_macro.param;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == PPCREC_IML_TYPE_COMPARE)
	{
		registersUsed->readGPR1 = op_compare.regA;
		registersUsed->readGPR2 = op_compare.regB;
		registersUsed->writtenGPR1 = op_compare.regR;
	}
	else if (type == PPCREC_IML_TYPE_COMPARE_S32)
	{
		registersUsed->readGPR1 = op_compare_s32.regA;
		registersUsed->writtenGPR1 = op_compare_s32.regR;
	}
	else if (type == PPCREC_IML_TYPE_CONDITIONAL_JUMP)
	{
		registersUsed->readGPR1 = op_conditional_jump.registerBool;
	}
	else if (type == PPCREC_IML_TYPE_JUMP)
	{
		// no registers affected
	}
	else if (type == PPCREC_IML_TYPE_LOAD)
	{
		registersUsed->writtenGPR1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readGPR1 = op_storeLoad.registerMem;
	}
	else if (type == PPCREC_IML_TYPE_LOAD_INDEXED)
	{
		registersUsed->writtenGPR1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readGPR1 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			registersUsed->readGPR2 = op_storeLoad.registerMem2;
	}
	else if (type == PPCREC_IML_TYPE_STORE)
	{
		registersUsed->readGPR1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readGPR2 = op_storeLoad.registerMem;
	}
	else if (type == PPCREC_IML_TYPE_STORE_INDEXED)
	{
		registersUsed->readGPR1 = op_storeLoad.registerData;
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readGPR2 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			registersUsed->readGPR3 = op_storeLoad.registerMem2;
	}
	else if (type == PPCREC_IML_TYPE_ATOMIC_CMP_STORE)
	{
		registersUsed->readGPR1 = op_atomic_compare_store.regEA;
		registersUsed->readGPR2 = op_atomic_compare_store.regCompareValue;
		registersUsed->readGPR3 = op_atomic_compare_store.regWriteValue;
		registersUsed->writtenGPR1 = op_atomic_compare_store.regBoolOut;
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_NAME)
	{
		// fpr operation
		registersUsed->writtenFPR1 = op_r_name.regR;
	}
	else if (type == PPCREC_IML_TYPE_FPR_NAME_R)
	{
		// fpr operation
		registersUsed->readFPR1 = op_r_name.regR;
	}
	else if (type == PPCREC_IML_TYPE_FPR_LOAD)
	{
		// fpr load operation
		registersUsed->writtenFPR1 = op_storeLoad.registerData;
		// address is in gpr register
		if (op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readGPR1 = op_storeLoad.registerMem;
		// determine partially written result
		switch (op_storeLoad.mode)
		{
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readGPR2 = op_storeLoad.registerGQR;
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
			registersUsed->readGPR1 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			registersUsed->readGPR2 = op_storeLoad.registerMem2;
		// determine partially written result
		switch (op_storeLoad.mode)
		{
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readGPR3 = op_storeLoad.registerGQR;
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
			registersUsed->readGPR1 = op_storeLoad.registerMem;
		// PSQ generic stores also access GQR
		switch (op_storeLoad.mode)
		{
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readGPR2 = op_storeLoad.registerGQR;
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
			registersUsed->readGPR1 = op_storeLoad.registerMem;
		if (op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			registersUsed->readGPR2 = op_storeLoad.registerMem2;
		// PSQ generic stores also access GQR
		switch (op_storeLoad.mode)
		{
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readGPR3 = op_storeLoad.registerGQR;
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
			registersUsed->readFPR1 = op_fpr_r_r.regA;
			registersUsed->writtenFPR1 = op_fpr_r_r.regR;
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
			registersUsed->readFPR1 = op_fpr_r_r.regA;
			registersUsed->readFPR4 = op_fpr_r_r.regR;
			registersUsed->writtenFPR1 = op_fpr_r_r.regR;
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
			registersUsed->readFPR1 = op_fpr_r_r.regA;
			registersUsed->readFPR2 = op_fpr_r_r.regR;
			registersUsed->writtenFPR1 = op_fpr_r_r.regR;

		}
		else if (operation == PPCREC_IML_OP_FPR_FCMPU_BOTTOM ||
			operation == PPCREC_IML_OP_FPR_FCMPU_TOP ||
			operation == PPCREC_IML_OP_FPR_FCMPO_BOTTOM)
		{
			// operand read, result read
			registersUsed->readFPR1 = op_fpr_r_r.regA;
			registersUsed->readFPR2 = op_fpr_r_r.regR;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R)
	{
		// fpr operation
		registersUsed->readFPR1 = op_fpr_r_r_r.regA;
		registersUsed->readFPR2 = op_fpr_r_r_r.regB;
		registersUsed->writtenFPR1 = op_fpr_r_r_r.regR;
		// handle partially written result
		switch (operation)
		{
		case PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM:
		case PPCREC_IML_OP_FPR_ADD_BOTTOM:
		case PPCREC_IML_OP_FPR_SUB_BOTTOM:
			registersUsed->readFPR4 = op_fpr_r_r_r.regR;
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
		registersUsed->readFPR1 = op_fpr_r_r_r_r.regA;
		registersUsed->readFPR2 = op_fpr_r_r_r_r.regB;
		registersUsed->readFPR3 = op_fpr_r_r_r_r.regC;
		registersUsed->writtenFPR1 = op_fpr_r_r_r_r.regR;
		// handle partially written result
		switch (operation)
		{
		case PPCREC_IML_OP_FPR_SELECT_BOTTOM:
			registersUsed->readFPR4 = op_fpr_r_r_r_r.regR;
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
			registersUsed->readFPR1 = op_fpr_r.regR;
			registersUsed->writtenFPR1 = op_fpr_r.regR;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (type == PPCREC_IML_TYPE_FPR_COMPARE)
	{
		registersUsed->writtenGPR1 = op_fpr_compare.regR;
		registersUsed->readFPR1 = op_fpr_compare.regA;
		registersUsed->readFPR2 = op_fpr_compare.regB;
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
		op_r_name.regR = replaceRegisterMultiple(op_r_name.regR, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_NAME_R)
	{
		op_r_name.regR = replaceRegisterMultiple(op_r_name.regR, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_R_R)
	{
		op_r_r.regR = replaceRegisterMultiple(op_r_r.regR, translationTable);
		op_r_r.regA = replaceRegisterMultiple(op_r_r.regA, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_R_S32)
	{
		op_r_immS32.regR = replaceRegisterMultiple(op_r_immS32.regR, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
	{
		op_conditional_r_s32.regR = replaceRegisterMultiple(op_conditional_r_s32.regR, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_R_R_S32)
	{
		op_r_r_s32.regR = replaceRegisterMultiple(op_r_r_s32.regR, translationTable);
		op_r_r_s32.regA = replaceRegisterMultiple(op_r_r_s32.regA, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_R_R_S32_CARRY)
	{
		op_r_r_s32_carry.regR = replaceRegisterMultiple(op_r_r_s32_carry.regR, translationTable);
		op_r_r_s32_carry.regA = replaceRegisterMultiple(op_r_r_s32_carry.regA, translationTable);
		op_r_r_s32_carry.regCarry = replaceRegisterMultiple(op_r_r_s32_carry.regCarry, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_R_R_R)
	{
		op_r_r_r.regR = replaceRegisterMultiple(op_r_r_r.regR, translationTable);
		op_r_r_r.regA = replaceRegisterMultiple(op_r_r_r.regA, translationTable);
		op_r_r_r.regB = replaceRegisterMultiple(op_r_r_r.regB, translationTable);
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
		op_compare.regR = replaceRegisterMultiple(op_compare.regR, translationTable);
		op_compare.regA = replaceRegisterMultiple(op_compare.regA, translationTable);
		op_compare.regB = replaceRegisterMultiple(op_compare.regB, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_COMPARE_S32)
	{
		op_compare_s32.regR = replaceRegisterMultiple(op_compare_s32.regR, translationTable);
		op_compare_s32.regA = replaceRegisterMultiple(op_compare_s32.regA, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_CONDITIONAL_JUMP)
	{
		op_conditional_jump.registerBool = replaceRegisterMultiple(op_conditional_jump.registerBool, translationTable);
	}
	else if (type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK || type == PPCREC_IML_TYPE_JUMP)
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
	else if (type == PPCREC_IML_TYPE_ATOMIC_CMP_STORE)
	{
		op_atomic_compare_store.regEA = replaceRegisterMultiple(op_atomic_compare_store.regEA, translationTable);
		op_atomic_compare_store.regCompareValue = replaceRegisterMultiple(op_atomic_compare_store.regCompareValue, translationTable);
		op_atomic_compare_store.regWriteValue = replaceRegisterMultiple(op_atomic_compare_store.regWriteValue, translationTable);
		op_atomic_compare_store.regBoolOut = replaceRegisterMultiple(op_atomic_compare_store.regBoolOut, translationTable);
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
	else if (type == PPCREC_IML_TYPE_FPR_COMPARE)
	{
		op_fpr_compare.regR = replaceRegisterMultiple(op_fpr_compare.regR, translationTable);
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
	else if (type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
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
	else if (type == PPCREC_IML_TYPE_ATOMIC_CMP_STORE)
	{
		;
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_NAME)
	{
		op_r_name.regR = replaceRegisterMultiple(op_r_name.regR, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_NAME_R)
	{
		op_r_name.regR = replaceRegisterMultiple(op_r_name.regR, fprRegisterSearched, fprRegisterReplaced);
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
		op_fpr_r_r.regR = replaceRegisterMultiple(op_fpr_r_r.regR, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r.regA = replaceRegisterMultiple(op_fpr_r_r.regA, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R)
	{
		op_fpr_r_r_r.regR = replaceRegisterMultiple(op_fpr_r_r_r.regR, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r.regA = replaceRegisterMultiple(op_fpr_r_r_r.regA, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r.regB = replaceRegisterMultiple(op_fpr_r_r_r.regB, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R_R)
	{
		op_fpr_r_r_r_r.regR = replaceRegisterMultiple(op_fpr_r_r_r_r.regR, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.regA = replaceRegisterMultiple(op_fpr_r_r_r_r.regA, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.regB = replaceRegisterMultiple(op_fpr_r_r_r_r.regB, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.regC = replaceRegisterMultiple(op_fpr_r_r_r_r.regC, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R)
	{
		op_fpr_r.regR = replaceRegisterMultiple(op_fpr_r.regR, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_COMPARE)
	{
		op_fpr_compare.regA = replaceRegisterMultiple(op_fpr_compare.regA, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_compare.regB = replaceRegisterMultiple(op_fpr_compare.regB, fprRegisterSearched, fprRegisterReplaced);
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
	else if (type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
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
	else if (type == PPCREC_IML_TYPE_ATOMIC_CMP_STORE)
	{
		;
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_NAME)
	{
		op_r_name.regR = replaceRegister(op_r_name.regR, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_NAME_R)
	{
		op_r_name.regR = replaceRegister(op_r_name.regR, fprRegisterSearched, fprRegisterReplaced);
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
		op_fpr_r_r.regR = replaceRegister(op_fpr_r_r.regR, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r.regA = replaceRegister(op_fpr_r_r.regA, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R)
	{
		op_fpr_r_r_r.regR = replaceRegister(op_fpr_r_r_r.regR, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r.regA = replaceRegister(op_fpr_r_r_r.regA, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r.regB = replaceRegister(op_fpr_r_r_r.regB, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R_R_R_R)
	{
		op_fpr_r_r_r_r.regR = replaceRegister(op_fpr_r_r_r_r.regR, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.regA = replaceRegister(op_fpr_r_r_r_r.regA, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.regB = replaceRegister(op_fpr_r_r_r_r.regB, fprRegisterSearched, fprRegisterReplaced);
		op_fpr_r_r_r_r.regC = replaceRegister(op_fpr_r_r_r_r.regC, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (type == PPCREC_IML_TYPE_FPR_R)
	{
		op_fpr_r.regR = replaceRegister(op_fpr_r.regR, fprRegisterSearched, fprRegisterReplaced);
	}
	else
	{
		cemu_assert_unimplemented();
	}
}
