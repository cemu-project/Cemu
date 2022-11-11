#include "../Interpreter/PPCInterpreterInternal.h"
#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "PPCRecompilerX64.h"

void PPCRecompiler_checkRegisterUsage(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, PPCImlOptimizerUsedRegisters_t* registersUsed)
{
	registersUsed->readNamedReg1 = -1;
	registersUsed->readNamedReg2 = -1;
	registersUsed->readNamedReg3 = -1;
	registersUsed->writtenNamedReg1 = -1;
	registersUsed->readFPR1 = -1;
	registersUsed->readFPR2 = -1;
	registersUsed->readFPR3 = -1;
	registersUsed->readFPR4 = -1;
	registersUsed->writtenFPR1 = -1;
	if( imlInstruction->type == PPCREC_IML_TYPE_R_NAME )
	{
		registersUsed->writtenNamedReg1 = imlInstruction->op_r_name.registerIndex;
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_NAME_R )
	{
		registersUsed->readNamedReg1 = imlInstruction->op_r_name.registerIndex;
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_R_R )
	{
		if (imlInstruction->operation == PPCREC_IML_OP_COMPARE_SIGNED || imlInstruction->operation == PPCREC_IML_OP_COMPARE_UNSIGNED || imlInstruction->operation == PPCREC_IML_OP_DCBZ)
		{
			// both operands are read only
			registersUsed->readNamedReg1 = imlInstruction->op_r_r.registerResult;
			registersUsed->readNamedReg2 = imlInstruction->op_r_r.registerA;
		}
		else if (
			imlInstruction->operation == PPCREC_IML_OP_OR ||
			imlInstruction->operation == PPCREC_IML_OP_AND ||
			imlInstruction->operation == PPCREC_IML_OP_XOR ||
			imlInstruction->operation == PPCREC_IML_OP_ADD ||
			imlInstruction->operation == PPCREC_IML_OP_ADD_CARRY ||
			imlInstruction->operation == PPCREC_IML_OP_ADD_CARRY_ME ||
			imlInstruction->operation == PPCREC_IML_OP_SUB_CARRY_UPDATE_CARRY)
		{
			// result is read and written, operand is read
			registersUsed->writtenNamedReg1 = imlInstruction->op_r_r.registerResult;
			registersUsed->readNamedReg1 = imlInstruction->op_r_r.registerResult;
			registersUsed->readNamedReg2 = imlInstruction->op_r_r.registerA;
		}
		else if (
			imlInstruction->operation == PPCREC_IML_OP_ASSIGN ||
			imlInstruction->operation == PPCREC_IML_OP_ENDIAN_SWAP ||
			imlInstruction->operation == PPCREC_IML_OP_CNTLZW ||
			imlInstruction->operation == PPCREC_IML_OP_NOT ||
			imlInstruction->operation == PPCREC_IML_OP_NEG ||
			imlInstruction->operation == PPCREC_IML_OP_ASSIGN_S16_TO_S32 ||
			imlInstruction->operation == PPCREC_IML_OP_ASSIGN_S8_TO_S32)
		{
			// result is written, operand is read
			registersUsed->writtenNamedReg1 = imlInstruction->op_r_r.registerResult;
			registersUsed->readNamedReg1 = imlInstruction->op_r_r.registerA;
		}
		else
			cemu_assert_unimplemented();
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_R_S32)
	{
		if (imlInstruction->operation == PPCREC_IML_OP_COMPARE_SIGNED || imlInstruction->operation == PPCREC_IML_OP_COMPARE_UNSIGNED || imlInstruction->operation == PPCREC_IML_OP_MTCRF)
		{
			// operand register is read only
			registersUsed->readNamedReg1 = imlInstruction->op_r_immS32.registerIndex;
		}
		else if (imlInstruction->operation == PPCREC_IML_OP_ADD ||
			imlInstruction->operation == PPCREC_IML_OP_SUB ||
			imlInstruction->operation == PPCREC_IML_OP_AND ||
			imlInstruction->operation == PPCREC_IML_OP_OR ||
			imlInstruction->operation == PPCREC_IML_OP_XOR ||
			imlInstruction->operation == PPCREC_IML_OP_LEFT_ROTATE)
		{
			// operand register is read and write
			registersUsed->readNamedReg1 = imlInstruction->op_r_immS32.registerIndex;
			registersUsed->writtenNamedReg1 = imlInstruction->op_r_immS32.registerIndex;
		}
		else
		{
			// operand register is write only
			// todo - use explicit lists, avoid default cases
			registersUsed->writtenNamedReg1 = imlInstruction->op_r_immS32.registerIndex;
		}
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
	{
		if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
		{
			// result is written, but also considered read (in case the condition fails)
			registersUsed->readNamedReg1 = imlInstruction->op_conditional_r_s32.registerIndex;
			registersUsed->writtenNamedReg1 = imlInstruction->op_conditional_r_s32.registerIndex;
		}
		else
			cemu_assert_unimplemented();
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_R_R_S32 )
	{
		if( imlInstruction->operation == PPCREC_IML_OP_RLWIMI )
		{
			// result and operand register are both read, result is written
			registersUsed->writtenNamedReg1 = imlInstruction->op_r_r_s32.registerResult;
			registersUsed->readNamedReg1 = imlInstruction->op_r_r_s32.registerResult;
			registersUsed->readNamedReg2 = imlInstruction->op_r_r_s32.registerA;
		}
		else
		{
			// result is write only and operand is read only
			registersUsed->writtenNamedReg1 = imlInstruction->op_r_r_s32.registerResult;
			registersUsed->readNamedReg1 = imlInstruction->op_r_r_s32.registerA;
		}
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_R_R_R )
	{
		// in all cases result is written and other operands are read only
		registersUsed->writtenNamedReg1 = imlInstruction->op_r_r_r.registerResult;
		registersUsed->readNamedReg1 = imlInstruction->op_r_r_r.registerA;
		registersUsed->readNamedReg2 = imlInstruction->op_r_r_r.registerB;
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_CJUMP || imlInstruction->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK )
	{
		// no effect on registers
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_NO_OP )
	{
		// no effect on registers
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_MACRO )
	{
		if( imlInstruction->operation == PPCREC_IML_MACRO_BL || imlInstruction->operation == PPCREC_IML_MACRO_B_FAR || imlInstruction->operation == PPCREC_IML_MACRO_BLR || imlInstruction->operation == PPCREC_IML_MACRO_BLRL || imlInstruction->operation == PPCREC_IML_MACRO_BCTR || imlInstruction->operation == PPCREC_IML_MACRO_BCTRL || imlInstruction->operation == PPCREC_IML_MACRO_LEAVE || imlInstruction->operation == PPCREC_IML_MACRO_DEBUGBREAK || imlInstruction->operation == PPCREC_IML_MACRO_COUNT_CYCLES || imlInstruction->operation == PPCREC_IML_MACRO_HLE || imlInstruction->operation == PPCREC_IML_MACRO_MFTB )
		{
			// no effect on registers
		}
		else
			cemu_assert_unimplemented();
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_LOAD)
	{
		registersUsed->writtenNamedReg1 = imlInstruction->op_storeLoad.registerData;
		if (imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg1 = imlInstruction->op_storeLoad.registerMem;
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_MEM2MEM)
	{
		registersUsed->readNamedReg1 = imlInstruction->op_mem2mem.src.registerMem;
		registersUsed->readNamedReg2 = imlInstruction->op_mem2mem.dst.registerMem;
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_LOAD_INDEXED )
	{
		registersUsed->writtenNamedReg1 = imlInstruction->op_storeLoad.registerData;
		if( imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER )
			registersUsed->readNamedReg1 = imlInstruction->op_storeLoad.registerMem;
		if( imlInstruction->op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER )
			registersUsed->readNamedReg2 = imlInstruction->op_storeLoad.registerMem2;
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_STORE )
	{
		registersUsed->readNamedReg1 = imlInstruction->op_storeLoad.registerData;
		if( imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER )
			registersUsed->readNamedReg2 = imlInstruction->op_storeLoad.registerMem;
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_STORE_INDEXED )
	{
		registersUsed->readNamedReg1 = imlInstruction->op_storeLoad.registerData;
		if( imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER )
			registersUsed->readNamedReg2 = imlInstruction->op_storeLoad.registerMem;
		if( imlInstruction->op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER )
			registersUsed->readNamedReg3 = imlInstruction->op_storeLoad.registerMem2;
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_CR )
	{
		// only affects cr register	
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_JUMPMARK )
	{
		// no effect on registers
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_PPC_ENTER )
	{
		// no op
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_NAME )
	{
		// fpr operation
		registersUsed->writtenFPR1 = imlInstruction->op_r_name.registerIndex;
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_NAME_R )
	{
		// fpr operation
		registersUsed->readFPR1 = imlInstruction->op_r_name.registerIndex;
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD )
	{
		// fpr load operation
		registersUsed->writtenFPR1 = imlInstruction->op_storeLoad.registerData;
		// address is in gpr register
		if (imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg1 = imlInstruction->op_storeLoad.registerMem;
		// determine partially written result
		switch (imlInstruction->op_storeLoad.mode)
		{
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(imlInstruction->op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readNamedReg2 = imlInstruction->op_storeLoad.registerGQR;
			break;
		case PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0:
			// PS1 remains the same
			registersUsed->readFPR4 = imlInstruction->op_storeLoad.registerData;
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
			break;
		default:
			cemu_assert_unimplemented();
		}
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED )
	{
		// fpr load operation
		registersUsed->writtenFPR1 = imlInstruction->op_storeLoad.registerData;
		// address is in gpr registers
		if (imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg1 = imlInstruction->op_storeLoad.registerMem;
		if (imlInstruction->op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			registersUsed->readNamedReg2 = imlInstruction->op_storeLoad.registerMem2;
		// determine partially written result
		switch (imlInstruction->op_storeLoad.mode)
		{
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(imlInstruction->op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readNamedReg3 = imlInstruction->op_storeLoad.registerGQR;
			break;
		case PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0:
			// PS1 remains the same
			registersUsed->readFPR4 = imlInstruction->op_storeLoad.registerData;
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
			break;
		default:
			cemu_assert_unimplemented();
		}
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE )
	{
		// fpr store operation
		registersUsed->readFPR1 = imlInstruction->op_storeLoad.registerData;
		if( imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER )
			registersUsed->readNamedReg1 = imlInstruction->op_storeLoad.registerMem;
		// PSQ generic stores also access GQR
		switch (imlInstruction->op_storeLoad.mode)
		{
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(imlInstruction->op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readNamedReg2 = imlInstruction->op_storeLoad.registerGQR;
			break;
		default:
			break;
		}
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE_INDEXED )
	{
		// fpr store operation
		registersUsed->readFPR1 = imlInstruction->op_storeLoad.registerData;
		// address is in gpr registers
		if( imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER )
			registersUsed->readNamedReg1 = imlInstruction->op_storeLoad.registerMem;
		if( imlInstruction->op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER )
			registersUsed->readNamedReg2 = imlInstruction->op_storeLoad.registerMem2;
		// PSQ generic stores also access GQR
		switch (imlInstruction->op_storeLoad.mode)
		{
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0:
		case PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1:
			cemu_assert_debug(imlInstruction->op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
			registersUsed->readNamedReg3 = imlInstruction->op_storeLoad.registerGQR;
			break;
		default:
			break;
		}
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R )
	{
		// fpr operation
		if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED ||
			imlInstruction->operation == PPCREC_IML_OP_ASSIGN ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_FRES_TO_BOTTOM_AND_TOP ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATE_PAIR ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_ABS_PAIR ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_FRES_PAIR ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_FRSQRTE_PAIR )
		{
			// operand read, result written
			registersUsed->readFPR1 = imlInstruction->op_fpr_r_r.registerOperand;
			registersUsed->writtenFPR1 = imlInstruction->op_fpr_r_r.registerResult;
		}
		else if( 
			imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_TOP ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_TOP ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64 ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_FCTIWZ ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_RECIPROCAL_SQRT
			 )
		{
			// operand read, result read and (partially) written
			registersUsed->readFPR1 = imlInstruction->op_fpr_r_r.registerOperand;
			registersUsed->readFPR4 = imlInstruction->op_fpr_r_r.registerResult;
			registersUsed->writtenFPR1 = imlInstruction->op_fpr_r_r.registerResult;
		}
		else if( imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_PAIR ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_DIVIDE_BOTTOM ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_DIVIDE_PAIR ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_BOTTOM ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_PAIR ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_PAIR ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_BOTTOM )
		{
			// operand read, result read and written
			registersUsed->readFPR1 = imlInstruction->op_fpr_r_r.registerOperand;
			registersUsed->readFPR2 = imlInstruction->op_fpr_r_r.registerResult;
			registersUsed->writtenFPR1 = imlInstruction->op_fpr_r_r.registerResult;

		}
		else if(imlInstruction->operation == PPCREC_IML_OP_FPR_FCMPU_BOTTOM ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_FCMPU_TOP ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_FCMPO_BOTTOM)
		{
			// operand read, result read
			registersUsed->readFPR1 = imlInstruction->op_fpr_r_r.registerOperand;
			registersUsed->readFPR2 = imlInstruction->op_fpr_r_r.registerResult;
		}
		else
			cemu_assert_unimplemented();
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R )
	{
		// fpr operation
		registersUsed->readFPR1 = imlInstruction->op_fpr_r_r_r.registerOperandA;
		registersUsed->readFPR2 = imlInstruction->op_fpr_r_r_r.registerOperandB;
		registersUsed->writtenFPR1 = imlInstruction->op_fpr_r_r_r.registerResult;
		// handle partially written result
		switch (imlInstruction->operation)
		{
		case PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM:
		case PPCREC_IML_OP_FPR_ADD_BOTTOM:
		case PPCREC_IML_OP_FPR_SUB_BOTTOM:
			registersUsed->readFPR4 = imlInstruction->op_fpr_r_r_r.registerResult;
			break;
		case PPCREC_IML_OP_FPR_SUB_PAIR:
			break;
		default:
			cemu_assert_unimplemented();
		}
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R_R )
	{
		// fpr operation
		registersUsed->readFPR1 = imlInstruction->op_fpr_r_r_r_r.registerOperandA;
		registersUsed->readFPR2 = imlInstruction->op_fpr_r_r_r_r.registerOperandB;
		registersUsed->readFPR3 = imlInstruction->op_fpr_r_r_r_r.registerOperandC;
		registersUsed->writtenFPR1 = imlInstruction->op_fpr_r_r_r_r.registerResult;
		// handle partially written result
		switch (imlInstruction->operation)
		{
		case PPCREC_IML_OP_FPR_SELECT_BOTTOM:
			registersUsed->readFPR4 = imlInstruction->op_fpr_r_r_r_r.registerResult;
			break;
		case PPCREC_IML_OP_FPR_SUM0:
		case PPCREC_IML_OP_FPR_SUM1:
		case PPCREC_IML_OP_FPR_SELECT_PAIR:
			break;
		default:
			cemu_assert_unimplemented();
		}
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R )
	{
		// fpr operation
		if( imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATE_BOTTOM ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_ABS_BOTTOM ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATIVE_ABS_BOTTOM ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64 ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_PAIR )
		{
			registersUsed->readFPR1 = imlInstruction->op_fpr_r.registerResult;
			registersUsed->writtenFPR1 = imlInstruction->op_fpr_r.registerResult;		
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

sint32 replaceRegisterMultiple(sint32 reg, sint32 match[4], sint32 replaced[4])
{
	for (sint32 i = 0; i < 4; i++)
	{
		if(match[i] < 0)
			continue;
		if (reg == match[i])
		{
			return replaced[i];
		}
	}
	return reg;
}

void PPCRecompiler_replaceGPRRegisterUsageMultiple(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, sint32 gprRegisterSearched[4], sint32 gprRegisterReplaced[4])
{
	if (imlInstruction->type == PPCREC_IML_TYPE_R_NAME)
	{
		imlInstruction->op_r_name.registerIndex = replaceRegisterMultiple(imlInstruction->op_r_name.registerIndex, gprRegisterSearched, gprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_NAME_R)
	{
		imlInstruction->op_r_name.registerIndex = replaceRegisterMultiple(imlInstruction->op_r_name.registerIndex, gprRegisterSearched, gprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_R_R)
	{
		imlInstruction->op_r_r.registerResult = replaceRegisterMultiple(imlInstruction->op_r_r.registerResult, gprRegisterSearched, gprRegisterReplaced);
		imlInstruction->op_r_r.registerA = replaceRegisterMultiple(imlInstruction->op_r_r.registerA, gprRegisterSearched, gprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_R_S32)
	{
		imlInstruction->op_r_immS32.registerIndex = replaceRegisterMultiple(imlInstruction->op_r_immS32.registerIndex, gprRegisterSearched, gprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
	{
		imlInstruction->op_conditional_r_s32.registerIndex = replaceRegisterMultiple(imlInstruction->op_conditional_r_s32.registerIndex, gprRegisterSearched, gprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_S32)
	{
		// in all cases result is written and other operand is read only
		imlInstruction->op_r_r_s32.registerResult = replaceRegisterMultiple(imlInstruction->op_r_r_s32.registerResult, gprRegisterSearched, gprRegisterReplaced);
		imlInstruction->op_r_r_s32.registerA = replaceRegisterMultiple(imlInstruction->op_r_r_s32.registerA, gprRegisterSearched, gprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_R)
	{
		// in all cases result is written and other operands are read only
		imlInstruction->op_r_r_r.registerResult = replaceRegisterMultiple(imlInstruction->op_r_r_r.registerResult, gprRegisterSearched, gprRegisterReplaced);
		imlInstruction->op_r_r_r.registerA = replaceRegisterMultiple(imlInstruction->op_r_r_r.registerA, gprRegisterSearched, gprRegisterReplaced);
		imlInstruction->op_r_r_r.registerB = replaceRegisterMultiple(imlInstruction->op_r_r_r.registerB, gprRegisterSearched, gprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_CJUMP || imlInstruction->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
	{
		// no effect on registers
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_NO_OP)
	{
		// no effect on registers
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_MACRO)
	{
		if (imlInstruction->operation == PPCREC_IML_MACRO_BL || imlInstruction->operation == PPCREC_IML_MACRO_B_FAR || imlInstruction->operation == PPCREC_IML_MACRO_BLR || imlInstruction->operation == PPCREC_IML_MACRO_BLRL || imlInstruction->operation == PPCREC_IML_MACRO_BCTR || imlInstruction->operation == PPCREC_IML_MACRO_BCTRL || imlInstruction->operation == PPCREC_IML_MACRO_LEAVE || imlInstruction->operation == PPCREC_IML_MACRO_DEBUGBREAK || imlInstruction->operation == PPCREC_IML_MACRO_HLE || imlInstruction->operation == PPCREC_IML_MACRO_MFTB || imlInstruction->operation == PPCREC_IML_MACRO_COUNT_CYCLES )
		{
			// no effect on registers
		}
		else
		{
			cemu_assert_unimplemented();
		}
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_LOAD)
	{
		imlInstruction->op_storeLoad.registerData = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerData, gprRegisterSearched, gprRegisterReplaced);
		if (imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
		{
			imlInstruction->op_storeLoad.registerMem = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem, gprRegisterSearched, gprRegisterReplaced);
		}
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_LOAD_INDEXED)
	{
		imlInstruction->op_storeLoad.registerData = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerData, gprRegisterSearched, gprRegisterReplaced);
		if (imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			imlInstruction->op_storeLoad.registerMem = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem, gprRegisterSearched, gprRegisterReplaced);
		if (imlInstruction->op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			imlInstruction->op_storeLoad.registerMem2 = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem2, gprRegisterSearched, gprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_STORE)
	{
		imlInstruction->op_storeLoad.registerData = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerData, gprRegisterSearched, gprRegisterReplaced);
		if (imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			imlInstruction->op_storeLoad.registerMem = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem, gprRegisterSearched, gprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_STORE_INDEXED)
	{
		imlInstruction->op_storeLoad.registerData = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerData, gprRegisterSearched, gprRegisterReplaced);
		if (imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
			imlInstruction->op_storeLoad.registerMem = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem, gprRegisterSearched, gprRegisterReplaced);
		if (imlInstruction->op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
			imlInstruction->op_storeLoad.registerMem2 = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem2, gprRegisterSearched, gprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_CR)
	{
		// only affects cr register	
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_JUMPMARK)
	{
		// no effect on registers
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_PPC_ENTER)
	{
		// no op
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_NAME)
	{

	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_NAME_R)
	{

	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD)
	{
		if (imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
		{
			imlInstruction->op_storeLoad.registerMem = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem, gprRegisterSearched, gprRegisterReplaced);
		}
		if (imlInstruction->op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER)
		{
			imlInstruction->op_storeLoad.registerGQR = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerGQR, gprRegisterSearched, gprRegisterReplaced);
		}
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED)
	{
		if (imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
		{
			imlInstruction->op_storeLoad.registerMem = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem, gprRegisterSearched, gprRegisterReplaced);
		}
		if (imlInstruction->op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
		{
			imlInstruction->op_storeLoad.registerMem2 = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem2, gprRegisterSearched, gprRegisterReplaced);
		}
		if (imlInstruction->op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER)
		{
			imlInstruction->op_storeLoad.registerGQR = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerGQR, gprRegisterSearched, gprRegisterReplaced);
		}
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE)
	{
		if (imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
		{
			imlInstruction->op_storeLoad.registerMem = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem, gprRegisterSearched, gprRegisterReplaced);
		}
		if (imlInstruction->op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER)
		{
			imlInstruction->op_storeLoad.registerGQR = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerGQR, gprRegisterSearched, gprRegisterReplaced);
		}
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE_INDEXED)
	{
		if (imlInstruction->op_storeLoad.registerMem != PPC_REC_INVALID_REGISTER)
		{
			imlInstruction->op_storeLoad.registerMem = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem, gprRegisterSearched, gprRegisterReplaced);
		}
		if (imlInstruction->op_storeLoad.registerMem2 != PPC_REC_INVALID_REGISTER)
		{
			imlInstruction->op_storeLoad.registerMem2 = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerMem2, gprRegisterSearched, gprRegisterReplaced);
		}
		if (imlInstruction->op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER)
		{
			imlInstruction->op_storeLoad.registerGQR = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerGQR, gprRegisterSearched, gprRegisterReplaced);
		}
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R)
	{
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R)
	{
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R_R)
	{
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R)
	{
	}
	else
	{
		cemu_assert_unimplemented();
	}
}

void PPCRecompiler_replaceFPRRegisterUsageMultiple(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, sint32 fprRegisterSearched[4], sint32 fprRegisterReplaced[4])
{
	if (imlInstruction->type == PPCREC_IML_TYPE_R_NAME)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_NAME_R)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_R_R)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_R_S32)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_S32)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_R)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_CJUMP || imlInstruction->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
	{
		// no effect on registers
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_NO_OP)
	{
		// no effect on registers
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_MACRO)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_LOAD)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_MEM2MEM)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_LOAD_INDEXED)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_STORE)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_STORE_INDEXED)
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_CR)
	{
		// only affects cr register	
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_JUMPMARK)
	{
		// no effect on registers
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_PPC_ENTER)
	{
		// no op
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_NAME)
	{
		imlInstruction->op_r_name.registerIndex = replaceRegisterMultiple(imlInstruction->op_r_name.registerIndex, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_NAME_R)
	{
		imlInstruction->op_r_name.registerIndex = replaceRegisterMultiple(imlInstruction->op_r_name.registerIndex, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD)
	{
		imlInstruction->op_storeLoad.registerData = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED)
	{
		imlInstruction->op_storeLoad.registerData = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE)
	{
		imlInstruction->op_storeLoad.registerData = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE_INDEXED)
	{
		imlInstruction->op_storeLoad.registerData = replaceRegisterMultiple(imlInstruction->op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R)
	{
		imlInstruction->op_fpr_r_r.registerResult = replaceRegisterMultiple(imlInstruction->op_fpr_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r.registerOperand = replaceRegisterMultiple(imlInstruction->op_fpr_r_r.registerOperand, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R)
	{
		imlInstruction->op_fpr_r_r_r.registerResult = replaceRegisterMultiple(imlInstruction->op_fpr_r_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r_r.registerOperandA = replaceRegisterMultiple(imlInstruction->op_fpr_r_r_r.registerOperandA, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r_r.registerOperandB = replaceRegisterMultiple(imlInstruction->op_fpr_r_r_r.registerOperandB, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R_R)
	{
		imlInstruction->op_fpr_r_r_r_r.registerResult = replaceRegisterMultiple(imlInstruction->op_fpr_r_r_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r_r_r.registerOperandA = replaceRegisterMultiple(imlInstruction->op_fpr_r_r_r_r.registerOperandA, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r_r_r.registerOperandB = replaceRegisterMultiple(imlInstruction->op_fpr_r_r_r_r.registerOperandB, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r_r_r.registerOperandC = replaceRegisterMultiple(imlInstruction->op_fpr_r_r_r_r.registerOperandC, fprRegisterSearched, fprRegisterReplaced);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R)
	{
		imlInstruction->op_fpr_r.registerResult = replaceRegisterMultiple(imlInstruction->op_fpr_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
	}
	else
	{
		cemu_assert_unimplemented();
	}
}

void PPCRecompiler_replaceFPRRegisterUsage(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, sint32 fprRegisterSearched, sint32 fprRegisterReplaced)
{
	if( imlInstruction->type == PPCREC_IML_TYPE_R_NAME )
	{
		// not affected
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_NAME_R )
	{
		// not affected
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_R_R )
	{
		// not affected
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_R_S32 )
	{
		// not affected
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_R_R_S32 )
	{
		// not affected
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_R_R_R )
	{
		// not affected
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_CJUMP || imlInstruction->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK )
	{
		// no effect on registers
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_NO_OP )
	{
		// no effect on registers
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_MACRO )
	{
		// not affected
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_LOAD )
	{
		// not affected
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_MEM2MEM)
	{
		// not affected
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_LOAD_INDEXED )
	{
		// not affected
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_STORE )
	{
		// not affected
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_STORE_INDEXED )
	{
		// not affected
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_CR )
	{
		// only affects cr register	
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_JUMPMARK )
	{
		// no effect on registers
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_PPC_ENTER )
	{
		// no op
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_NAME )
	{
		imlInstruction->op_r_name.registerIndex = replaceRegister(imlInstruction->op_r_name.registerIndex, fprRegisterSearched, fprRegisterReplaced);
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_NAME_R )
	{
		imlInstruction->op_r_name.registerIndex = replaceRegister(imlInstruction->op_r_name.registerIndex, fprRegisterSearched, fprRegisterReplaced);
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD )
	{
		imlInstruction->op_storeLoad.registerData = replaceRegister(imlInstruction->op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED )
	{
		imlInstruction->op_storeLoad.registerData = replaceRegister(imlInstruction->op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE )
	{
		imlInstruction->op_storeLoad.registerData = replaceRegister(imlInstruction->op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE_INDEXED )
	{
		imlInstruction->op_storeLoad.registerData = replaceRegister(imlInstruction->op_storeLoad.registerData, fprRegisterSearched, fprRegisterReplaced);
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R )
	{
		imlInstruction->op_fpr_r_r.registerResult = replaceRegister(imlInstruction->op_fpr_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r.registerOperand = replaceRegister(imlInstruction->op_fpr_r_r.registerOperand, fprRegisterSearched, fprRegisterReplaced);
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R )
	{
		imlInstruction->op_fpr_r_r_r.registerResult = replaceRegister(imlInstruction->op_fpr_r_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r_r.registerOperandA = replaceRegister(imlInstruction->op_fpr_r_r_r.registerOperandA, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r_r.registerOperandB = replaceRegister(imlInstruction->op_fpr_r_r_r.registerOperandB, fprRegisterSearched, fprRegisterReplaced);
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R_R )
	{
		imlInstruction->op_fpr_r_r_r_r.registerResult = replaceRegister(imlInstruction->op_fpr_r_r_r_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r_r_r.registerOperandA = replaceRegister(imlInstruction->op_fpr_r_r_r_r.registerOperandA, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r_r_r.registerOperandB = replaceRegister(imlInstruction->op_fpr_r_r_r_r.registerOperandB, fprRegisterSearched, fprRegisterReplaced);
		imlInstruction->op_fpr_r_r_r_r.registerOperandC = replaceRegister(imlInstruction->op_fpr_r_r_r_r.registerOperandC, fprRegisterSearched, fprRegisterReplaced);
	}
	else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R )
	{
		imlInstruction->op_fpr_r.registerResult = replaceRegister(imlInstruction->op_fpr_r.registerResult, fprRegisterSearched, fprRegisterReplaced);
	}
	else
	{
		cemu_assert_unimplemented();
	}
}

typedef struct  
{
	struct  
	{
		sint32 instructionIndex;
		sint32 registerPreviousName;
		sint32 registerNewName;
		sint32 index; // new index
		sint32 previousIndex; // previous index (always out of range)
		bool nameMustBeMaintained; // must be stored before replacement and loaded after replacement ends
	}replacedRegisterEntry[PPC_X64_GPR_USABLE_REGISTERS];
	sint32 count;
}replacedRegisterTracker_t;

bool PPCRecompiler_checkIfGPRRegisterIsAccessed(PPCImlOptimizerUsedRegisters_t* registersUsed, sint32 gprRegister)
{
	if( registersUsed->readNamedReg1 == gprRegister )
		return true;
	if( registersUsed->readNamedReg2 == gprRegister )
		return true;
	if( registersUsed->readNamedReg3 == gprRegister )
		return true;
	if( registersUsed->writtenNamedReg1 == gprRegister )
		return true;
	return false;
}

/*
 * Returns index of register to replace
 * If no register needs to be replaced, -1 is returned
 */
sint32 PPCRecompiler_getNextRegisterToReplace(PPCImlOptimizerUsedRegisters_t* registersUsed)
{
	// get index of register to replace
	sint32 gprToReplace = -1;
	if( registersUsed->readNamedReg1 >= PPC_X64_GPR_USABLE_REGISTERS )
		gprToReplace = registersUsed->readNamedReg1;
	else if( registersUsed->readNamedReg2 >= PPC_X64_GPR_USABLE_REGISTERS )
		gprToReplace = registersUsed->readNamedReg2;
	else if( registersUsed->readNamedReg3 >= PPC_X64_GPR_USABLE_REGISTERS )
		gprToReplace = registersUsed->readNamedReg3;
	else if( registersUsed->writtenNamedReg1 >= PPC_X64_GPR_USABLE_REGISTERS )
		gprToReplace = registersUsed->writtenNamedReg1;
	// return 
	return gprToReplace;
}

bool PPCRecompiler_findAvailableRegisterDepr(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 imlIndexStart, replacedRegisterTracker_t* replacedRegisterTracker, sint32* registerIndex, sint32* registerName, bool* isUsed)
{
	PPCImlOptimizerUsedRegisters_t registersUsed;
	PPCRecompiler_checkRegisterUsage(ppcImlGenContext, imlSegment->imlList+imlIndexStart, &registersUsed);
	// mask all registers used by this instruction
	uint32 instructionReservedRegisterMask = 0;//(1<<(PPC_X64_GPR_USABLE_REGISTERS+1))-1;
	if( registersUsed.readNamedReg1 != -1 )
		instructionReservedRegisterMask |= (1<<(registersUsed.readNamedReg1));
	if( registersUsed.readNamedReg2 != -1 )
		instructionReservedRegisterMask |= (1<<(registersUsed.readNamedReg2));
	if( registersUsed.readNamedReg3 != -1 )
		instructionReservedRegisterMask |= (1<<(registersUsed.readNamedReg3));
	if( registersUsed.writtenNamedReg1 != -1 )
		instructionReservedRegisterMask |= (1<<(registersUsed.writtenNamedReg1));
	// mask all registers that are reserved for other replacements
	uint32 replacementReservedRegisterMask = 0;
	for(sint32 i=0; i<replacedRegisterTracker->count; i++)
	{
		replacementReservedRegisterMask |= (1<<replacedRegisterTracker->replacedRegisterEntry[i].index);
	}

	// potential improvement: Scan ahead a few instructions and look for registers that are the least used (or ideally never used)

	// pick available register
	const uint32 allRegisterMask = (1<<(PPC_X64_GPR_USABLE_REGISTERS+1))-1; // mask with set bit for every register
	uint32 reservedRegisterMask = instructionReservedRegisterMask | replacementReservedRegisterMask;
	cemu_assert(instructionReservedRegisterMask != allRegisterMask); // no usable register! (Need to store a register from the replacedRegisterTracker)
	sint32 usedRegisterIndex = -1;
	for(sint32 i=0; i<PPC_X64_GPR_USABLE_REGISTERS; i++)
	{
		if( (reservedRegisterMask&(1<<i)) == 0 )
		{
			if( (instructionReservedRegisterMask&(1<<i)) == 0 && ppcImlGenContext->mappedRegister[i] != -1 )
			{
				// register is reserved by segment -> In use
				*isUsed = true;
				*registerName = ppcImlGenContext->mappedRegister[i];
			}
			else
			{
				*isUsed = false;
				*registerName = -1;
			}
			*registerIndex = i;
			return true;
		}
	}
	return false;

}

bool PPCRecompiler_hasSuffixInstruction(PPCRecImlSegment_t* imlSegment)
{
	if( imlSegment->imlListCount == 0 )
		return false;
	PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList+imlSegment->imlListCount-1;
	if( imlInstruction->type == PPCREC_IML_TYPE_MACRO && (imlInstruction->operation == PPCREC_IML_MACRO_BLR || imlInstruction->operation == PPCREC_IML_MACRO_BCTR) ||
		imlInstruction->type == PPCREC_IML_TYPE_MACRO && imlInstruction->operation == PPCREC_IML_MACRO_BL ||
		imlInstruction->type == PPCREC_IML_TYPE_MACRO && imlInstruction->operation == PPCREC_IML_MACRO_B_FAR ||
		imlInstruction->type == PPCREC_IML_TYPE_MACRO && imlInstruction->operation == PPCREC_IML_MACRO_BLRL ||
		imlInstruction->type == PPCREC_IML_TYPE_MACRO && imlInstruction->operation == PPCREC_IML_MACRO_BCTRL ||
		imlInstruction->type == PPCREC_IML_TYPE_MACRO && imlInstruction->operation == PPCREC_IML_MACRO_LEAVE ||
		imlInstruction->type == PPCREC_IML_TYPE_MACRO && imlInstruction->operation == PPCREC_IML_MACRO_HLE ||
		imlInstruction->type == PPCREC_IML_TYPE_MACRO && imlInstruction->operation == PPCREC_IML_MACRO_MFTB ||
		imlInstruction->type == PPCREC_IML_TYPE_PPC_ENTER ||
		imlInstruction->type == PPCREC_IML_TYPE_CJUMP ||
		imlInstruction->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK )
		return true;
	return false;
}

void PPCRecompiler_storeReplacedRegister(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, replacedRegisterTracker_t* replacedRegisterTracker, sint32 registerTrackerIndex, sint32* imlIndex)
{
	// store register
	sint32 imlIndexEdit = *imlIndex;
	PPCRecompiler_pushBackIMLInstructions(imlSegment, imlIndexEdit, 1);
	// name_unusedRegister = unusedRegister
	PPCRecImlInstruction_t* imlInstructionItr = imlSegment->imlList+(imlIndexEdit+0);
	memset(imlInstructionItr, 0x00, sizeof(PPCRecImlInstruction_t));
	imlInstructionItr->type = PPCREC_IML_TYPE_NAME_R;
	imlInstructionItr->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
	imlInstructionItr->op_r_name.registerIndex = replacedRegisterTracker->replacedRegisterEntry[registerTrackerIndex].index;
	imlInstructionItr->op_r_name.name = replacedRegisterTracker->replacedRegisterEntry[registerTrackerIndex].registerNewName;
	imlInstructionItr->op_r_name.copyWidth = 32;
	imlInstructionItr->op_r_name.flags = 0;
	imlIndexEdit++;
	// load new register if required
	if( replacedRegisterTracker->replacedRegisterEntry[registerTrackerIndex].nameMustBeMaintained )
	{
		PPCRecompiler_pushBackIMLInstructions(imlSegment, imlIndexEdit, 1);
		PPCRecImlInstruction_t* imlInstructionItr = imlSegment->imlList+(imlIndexEdit+0);
		memset(imlInstructionItr, 0x00, sizeof(PPCRecImlInstruction_t));
		imlInstructionItr->type = PPCREC_IML_TYPE_R_NAME;
		imlInstructionItr->crRegister = PPC_REC_INVALID_REGISTER;
		imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
		imlInstructionItr->op_r_name.registerIndex = replacedRegisterTracker->replacedRegisterEntry[registerTrackerIndex].index;
		imlInstructionItr->op_r_name.name = replacedRegisterTracker->replacedRegisterEntry[registerTrackerIndex].registerPreviousName;//ppcImlGenContext->mappedRegister[replacedRegisterTracker.replacedRegisterEntry[i].index];
		imlInstructionItr->op_r_name.copyWidth = 32;
		imlInstructionItr->op_r_name.flags = 0;
		imlIndexEdit += 1;
	}
	// move last entry to current one
	memcpy(replacedRegisterTracker->replacedRegisterEntry+registerTrackerIndex, replacedRegisterTracker->replacedRegisterEntry+replacedRegisterTracker->count-1, sizeof(replacedRegisterTracker->replacedRegisterEntry[0]));
	replacedRegisterTracker->count--;
	*imlIndex = imlIndexEdit;
}

bool PPCRecompiler_reduceNumberOfFPRRegisters(ppcImlGenContext_t* ppcImlGenContext)
{
	// only xmm0 to xmm14 may be used, xmm15 is reserved
	// this method will reduce the number of fpr registers used
	// inefficient algorithm for optimizing away excess registers
	// we simply load, use and store excess registers into other unused registers when we need to
	// first we remove all name load and store instructions that involve out-of-bounds registers
	for(sint32 s=0; s<ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
		sint32 imlIndex = 0;
		while( imlIndex < imlSegment->imlListCount )
		{
			PPCRecImlInstruction_t* imlInstructionItr = imlSegment->imlList+imlIndex;
			if( imlInstructionItr->type == PPCREC_IML_TYPE_FPR_R_NAME || imlInstructionItr->type == PPCREC_IML_TYPE_FPR_NAME_R )
			{
				if( imlInstructionItr->op_r_name.registerIndex >= PPC_X64_FPR_USABLE_REGISTERS )
				{
					// convert to NO-OP instruction
					imlInstructionItr->type = PPCREC_IML_TYPE_NO_OP;
					imlInstructionItr->associatedPPCAddress = 0;
				}
			}
			imlIndex++;
		}	
	}
	// replace registers
	for(sint32 s=0; s<ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
		sint32 imlIndex = 0;
		while( imlIndex < imlSegment->imlListCount )
		{
			PPCImlOptimizerUsedRegisters_t registersUsed;
			while( true )
			{
				PPCRecompiler_checkRegisterUsage(ppcImlGenContext, imlSegment->imlList+imlIndex, &registersUsed);
				if( registersUsed.readFPR1 >= PPC_X64_FPR_USABLE_REGISTERS || registersUsed.readFPR2 >= PPC_X64_FPR_USABLE_REGISTERS || registersUsed.readFPR3 >= PPC_X64_FPR_USABLE_REGISTERS || registersUsed.readFPR4 >= PPC_X64_FPR_USABLE_REGISTERS || registersUsed.writtenFPR1 >= PPC_X64_FPR_USABLE_REGISTERS )
				{
					// get index of register to replace
					sint32 fprToReplace = -1;
					if( registersUsed.readFPR1 >= PPC_X64_FPR_USABLE_REGISTERS )
						fprToReplace = registersUsed.readFPR1;
					else if( registersUsed.readFPR2 >= PPC_X64_FPR_USABLE_REGISTERS )
						fprToReplace = registersUsed.readFPR2;
					else if (registersUsed.readFPR3 >= PPC_X64_FPR_USABLE_REGISTERS)
						fprToReplace = registersUsed.readFPR3;
					else if (registersUsed.readFPR4 >= PPC_X64_FPR_USABLE_REGISTERS)
						fprToReplace = registersUsed.readFPR4;
					else if( registersUsed.writtenFPR1 >= PPC_X64_FPR_USABLE_REGISTERS )
						fprToReplace = registersUsed.writtenFPR1;
					// generate mask of useable registers
					uint8 useableRegisterMask = 0x7F; // lowest bit is fpr register 0
					if( registersUsed.readFPR1 != -1 )
						useableRegisterMask &= ~(1<<(registersUsed.readFPR1));
					if( registersUsed.readFPR2 != -1 )
						useableRegisterMask &= ~(1<<(registersUsed.readFPR2));
					if (registersUsed.readFPR3 != -1)
						useableRegisterMask &= ~(1 << (registersUsed.readFPR3));
					if (registersUsed.readFPR4 != -1)
						useableRegisterMask &= ~(1 << (registersUsed.readFPR4));
					if( registersUsed.writtenFPR1 != -1 )
						useableRegisterMask &= ~(1<<(registersUsed.writtenFPR1));
					// get highest unused register index (0-6 range)
					sint32 unusedRegisterIndex = -1;
					for(sint32 f=0; f<PPC_X64_FPR_USABLE_REGISTERS; f++)
					{
						if( useableRegisterMask&(1<<f) )
						{
							unusedRegisterIndex = f;
						}
					}
					if( unusedRegisterIndex == -1 )
						assert_dbg();
					// determine if the placeholder register is actually used (if not we must not load/store it)
					uint32 unusedRegisterName = ppcImlGenContext->mappedFPRRegister[unusedRegisterIndex];
					bool replacedRegisterIsUsed = true;
					if( unusedRegisterName >= PPCREC_NAME_FPR0 && unusedRegisterName < (PPCREC_NAME_FPR0+32) )
					{
						replacedRegisterIsUsed = imlSegment->ppcFPRUsed[unusedRegisterName-PPCREC_NAME_FPR0];
					}
					// replace registers that are out of range
					PPCRecompiler_replaceFPRRegisterUsage(ppcImlGenContext, imlSegment->imlList+imlIndex, fprToReplace, unusedRegisterIndex);
					// add load/store name after instruction
					PPCRecompiler_pushBackIMLInstructions(imlSegment, imlIndex+1, 2);
					// add load/store before current instruction
					PPCRecompiler_pushBackIMLInstructions(imlSegment, imlIndex, 2);
					// name_unusedRegister = unusedRegister
					PPCRecImlInstruction_t* imlInstructionItr = imlSegment->imlList+(imlIndex+0);
					memset(imlInstructionItr, 0x00, sizeof(PPCRecImlInstruction_t));
					if( replacedRegisterIsUsed )
					{
						imlInstructionItr->type = PPCREC_IML_TYPE_FPR_NAME_R;
						imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
						imlInstructionItr->op_r_name.registerIndex = unusedRegisterIndex;
						imlInstructionItr->op_r_name.name = ppcImlGenContext->mappedFPRRegister[unusedRegisterIndex];
						imlInstructionItr->op_r_name.copyWidth = 32;
						imlInstructionItr->op_r_name.flags = 0;
					}
					else
						imlInstructionItr->type = PPCREC_IML_TYPE_NO_OP;
					imlInstructionItr = imlSegment->imlList+(imlIndex+1);
					memset(imlInstructionItr, 0x00, sizeof(PPCRecImlInstruction_t));
					imlInstructionItr->type = PPCREC_IML_TYPE_FPR_R_NAME;
					imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
					imlInstructionItr->op_r_name.registerIndex = unusedRegisterIndex;
					imlInstructionItr->op_r_name.name = ppcImlGenContext->mappedFPRRegister[fprToReplace];
					imlInstructionItr->op_r_name.copyWidth = 32;
					imlInstructionItr->op_r_name.flags = 0;
					// name_gprToReplace = unusedRegister
					imlInstructionItr = imlSegment->imlList+(imlIndex+3);
					memset(imlInstructionItr, 0x00, sizeof(PPCRecImlInstruction_t));
					imlInstructionItr->type = PPCREC_IML_TYPE_FPR_NAME_R;
					imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
					imlInstructionItr->op_r_name.registerIndex = unusedRegisterIndex;
					imlInstructionItr->op_r_name.name = ppcImlGenContext->mappedFPRRegister[fprToReplace];
					imlInstructionItr->op_r_name.copyWidth = 32;
					imlInstructionItr->op_r_name.flags = 0;
					// unusedRegister = name_unusedRegister
					imlInstructionItr = imlSegment->imlList+(imlIndex+4);
					memset(imlInstructionItr, 0x00, sizeof(PPCRecImlInstruction_t));
					if( replacedRegisterIsUsed )
					{
						imlInstructionItr->type = PPCREC_IML_TYPE_FPR_R_NAME;
						imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
						imlInstructionItr->op_r_name.registerIndex = unusedRegisterIndex;
						imlInstructionItr->op_r_name.name = ppcImlGenContext->mappedFPRRegister[unusedRegisterIndex];
						imlInstructionItr->op_r_name.copyWidth = 32;
						imlInstructionItr->op_r_name.flags = 0;
					}
					else
						imlInstructionItr->type = PPCREC_IML_TYPE_NO_OP;
				}
				else
					break;
			}
			imlIndex++;
		}
	}
	return true;
}

typedef struct  
{
	bool isActive;
	uint32 virtualReg;
	sint32 lastUseIndex;
}ppcRecRegisterMapping_t;

typedef struct  
{
	ppcRecRegisterMapping_t currentMapping[PPC_X64_FPR_USABLE_REGISTERS];
	sint32 ppcRegToMapping[64];
	sint32 currentUseIndex;
}ppcRecManageRegisters_t;

ppcRecRegisterMapping_t* PPCRecompiler_findAvailableRegisterDepr(ppcRecManageRegisters_t* rCtx, PPCImlOptimizerUsedRegisters_t* instructionUsedRegisters)
{
	// find free register
	for (sint32 i = 0; i < PPC_X64_FPR_USABLE_REGISTERS; i++)
	{
		if (rCtx->currentMapping[i].isActive == false)
		{
			rCtx->currentMapping[i].isActive = true;
			rCtx->currentMapping[i].virtualReg = -1;
			rCtx->currentMapping[i].lastUseIndex = rCtx->currentUseIndex;
			return rCtx->currentMapping + i;
		}
	}
	// all registers are used
	return nullptr;
}

ppcRecRegisterMapping_t* PPCRecompiler_findUnloadableRegister(ppcRecManageRegisters_t* rCtx, PPCImlOptimizerUsedRegisters_t* instructionUsedRegisters, uint32 unloadLockedMask)
{
	// find unloadable register (with lowest lastUseIndex)
	sint32 unloadIndex = -1;
	sint32 unloadIndexLastUse = 0x7FFFFFFF;
	for (sint32 i = 0; i < PPC_X64_FPR_USABLE_REGISTERS; i++)
	{
		if (rCtx->currentMapping[i].isActive == false)
			continue;
		if( (unloadLockedMask&(1<<i)) != 0 )
			continue;
		uint32 virtualReg = rCtx->currentMapping[i].virtualReg;
		bool isReserved = false;
		for (sint32 f = 0; f < 4; f++)
		{
			if (virtualReg == (sint32)instructionUsedRegisters->fpr[f])
			{
				isReserved = true;
				break;
			}
		}
		if (isReserved)
			continue;
		if (rCtx->currentMapping[i].lastUseIndex < unloadIndexLastUse)
		{
			unloadIndexLastUse = rCtx->currentMapping[i].lastUseIndex;
			unloadIndex = i;
		}
	}
	cemu_assert(unloadIndex != -1);
	return rCtx->currentMapping + unloadIndex;
}

bool PPCRecompiler_manageFPRRegistersForSegment(ppcImlGenContext_t* ppcImlGenContext, sint32 segmentIndex)
{
	ppcRecManageRegisters_t rCtx = { 0 };
	for (sint32 i = 0; i < 64; i++)
		rCtx.ppcRegToMapping[i] = -1;
	PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[segmentIndex];
	sint32 idx = 0;
	sint32 currentUseIndex = 0;
	PPCImlOptimizerUsedRegisters_t registersUsed;
	while (idx < imlSegment->imlListCount)
	{
		if ( PPCRecompiler_isSuffixInstruction(imlSegment->imlList + idx) )
			break;
		PPCRecompiler_checkRegisterUsage(ppcImlGenContext, imlSegment->imlList + idx, &registersUsed);
		sint32 fprMatch[4];
		sint32 fprReplace[4];
		fprMatch[0] = -1;
		fprMatch[1] = -1;
		fprMatch[2] = -1;
		fprMatch[3] = -1;
		fprReplace[0] = -1;
		fprReplace[1] = -1;
		fprReplace[2] = -1;
		fprReplace[3] = -1;
		// generate a mask of registers that we may not free
		sint32 numReplacedOperands = 0;
		uint32 unloadLockedMask = 0;
		for (sint32 f = 0; f < 5; f++)
		{
			sint32 virtualFpr;
			if (f == 0)
				virtualFpr = registersUsed.readFPR1;
			else if (f == 1)
				virtualFpr = registersUsed.readFPR2;
			else if (f == 2)
				virtualFpr = registersUsed.readFPR3;
			else if (f == 3)
				virtualFpr = registersUsed.readFPR4;
			else if (f == 4)
				virtualFpr = registersUsed.writtenFPR1;
			if( virtualFpr < 0 )
				continue;
			cemu_assert_debug(virtualFpr < 64);
			// check if this virtual FPR is already loaded in any real register
			ppcRecRegisterMapping_t* regMapping;
			if (rCtx.ppcRegToMapping[virtualFpr] == -1)
			{
				// not loaded
				// find available register
				while (true)
				{
					regMapping = PPCRecompiler_findAvailableRegisterDepr(&rCtx, &registersUsed);
					if (regMapping == NULL)
					{
						// unload least recently used register and try again
						ppcRecRegisterMapping_t* unloadRegMapping = PPCRecompiler_findUnloadableRegister(&rCtx, &registersUsed, unloadLockedMask);
						// mark as locked
						unloadLockedMask |= (1<<(unloadRegMapping- rCtx.currentMapping));
						// create unload instruction
						PPCRecompiler_pushBackIMLInstructions(imlSegment, idx, 1);
						PPCRecImlInstruction_t* imlInstructionTemp = imlSegment->imlList + idx;
						memset(imlInstructionTemp, 0x00, sizeof(PPCRecImlInstruction_t));
						imlInstructionTemp->type = PPCREC_IML_TYPE_FPR_NAME_R;
						imlInstructionTemp->operation = PPCREC_IML_OP_ASSIGN;
						imlInstructionTemp->op_r_name.registerIndex = (uint8)(unloadRegMapping - rCtx.currentMapping);
						imlInstructionTemp->op_r_name.name = ppcImlGenContext->mappedFPRRegister[unloadRegMapping->virtualReg];
						imlInstructionTemp->op_r_name.copyWidth = 32;
						imlInstructionTemp->op_r_name.flags = 0;
						idx++;
						// update mapping
						unloadRegMapping->isActive = false;
						rCtx.ppcRegToMapping[unloadRegMapping->virtualReg] = -1;
					}
					else
						break;
				}
				// create load instruction
				PPCRecompiler_pushBackIMLInstructions(imlSegment, idx, 1);
				PPCRecImlInstruction_t* imlInstructionTemp = imlSegment->imlList + idx;
				memset(imlInstructionTemp, 0x00, sizeof(PPCRecImlInstruction_t));
				imlInstructionTemp->type = PPCREC_IML_TYPE_FPR_R_NAME;
				imlInstructionTemp->operation = PPCREC_IML_OP_ASSIGN;
				imlInstructionTemp->op_r_name.registerIndex = (uint8)(regMapping-rCtx.currentMapping);
				imlInstructionTemp->op_r_name.name = ppcImlGenContext->mappedFPRRegister[virtualFpr];
				imlInstructionTemp->op_r_name.copyWidth = 32;
				imlInstructionTemp->op_r_name.flags = 0;
				idx++;
				// update mapping
				regMapping->virtualReg = virtualFpr;
				rCtx.ppcRegToMapping[virtualFpr] = (sint32)(regMapping - rCtx.currentMapping);
				regMapping->lastUseIndex = rCtx.currentUseIndex;
				rCtx.currentUseIndex++;
			}
			else
			{
				regMapping = rCtx.currentMapping + rCtx.ppcRegToMapping[virtualFpr];
				regMapping->lastUseIndex = rCtx.currentUseIndex;
				rCtx.currentUseIndex++;
			}
			// replace FPR
			bool entryFound = false;
			for (sint32 t = 0; t < numReplacedOperands; t++)
			{
				if (fprMatch[t] == virtualFpr)
				{
					cemu_assert_debug(fprReplace[t] == (regMapping - rCtx.currentMapping));
					entryFound = true;
					break;
				}
			}
			if (entryFound == false)
			{
				cemu_assert_debug(numReplacedOperands != 4);
				fprMatch[numReplacedOperands] = virtualFpr;
				fprReplace[numReplacedOperands] = (sint32)(regMapping - rCtx.currentMapping);
				numReplacedOperands++;
			}
		}
		if (numReplacedOperands > 0)
		{
			PPCRecompiler_replaceFPRRegisterUsageMultiple(ppcImlGenContext, imlSegment->imlList + idx, fprMatch, fprReplace);
		}
		// next
		idx++;
	}
	// count loaded registers
	sint32 numLoadedRegisters = 0;
	for (sint32 i = 0; i < PPC_X64_FPR_USABLE_REGISTERS; i++)
	{
		if (rCtx.currentMapping[i].isActive)
			numLoadedRegisters++;
	}
	// store all loaded registers
	if (numLoadedRegisters > 0)
	{
		PPCRecompiler_pushBackIMLInstructions(imlSegment, idx, numLoadedRegisters);
		for (sint32 i = 0; i < PPC_X64_FPR_USABLE_REGISTERS; i++)
		{
			if (rCtx.currentMapping[i].isActive == false)
				continue;
			PPCRecImlInstruction_t* imlInstructionTemp = imlSegment->imlList + idx;
			memset(imlInstructionTemp, 0x00, sizeof(PPCRecImlInstruction_t));
			imlInstructionTemp->type = PPCREC_IML_TYPE_FPR_NAME_R;
			imlInstructionTemp->operation = PPCREC_IML_OP_ASSIGN;
			imlInstructionTemp->op_r_name.registerIndex = i;
			imlInstructionTemp->op_r_name.name = ppcImlGenContext->mappedFPRRegister[rCtx.currentMapping[i].virtualReg];
			imlInstructionTemp->op_r_name.copyWidth = 32;
			imlInstructionTemp->op_r_name.flags = 0;
			idx++;
		}
	}
	return true;
}

bool PPCRecompiler_manageFPRRegisters(ppcImlGenContext_t* ppcImlGenContext)
{
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		if (PPCRecompiler_manageFPRRegistersForSegment(ppcImlGenContext, s) == false)
			return false;
	}
	return true;
}


/*
 * Returns true if the loaded value is guaranteed to be overwritten
 */
bool PPCRecompiler_trackRedundantNameLoadInstruction(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 startIndex, PPCRecImlInstruction_t* nameStoreInstruction, sint32 scanDepth)
{
	sint16 registerIndex = nameStoreInstruction->op_r_name.registerIndex;
	for(sint32 i=startIndex; i<imlSegment->imlListCount; i++)
	{
		PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList+i;
		//nameStoreInstruction->op_r_name.registerIndex
		PPCImlOptimizerUsedRegisters_t registersUsed;
		PPCRecompiler_checkRegisterUsage(ppcImlGenContext, imlSegment->imlList+i, &registersUsed);
		if( registersUsed.readNamedReg1 == registerIndex || registersUsed.readNamedReg2 == registerIndex || registersUsed.readNamedReg3 == registerIndex )
			return false;
		if( registersUsed.writtenNamedReg1 == registerIndex )
			return true;
	}
	// todo: Scan next segment(s)
	return false;
}

/*
 * Returns true if the loaded value is guaranteed to be overwritten
 */
bool PPCRecompiler_trackRedundantFPRNameLoadInstruction(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 startIndex, PPCRecImlInstruction_t* nameStoreInstruction, sint32 scanDepth)
{
	sint16 registerIndex = nameStoreInstruction->op_r_name.registerIndex;
	for(sint32 i=startIndex; i<imlSegment->imlListCount; i++)
	{
		PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList+i;
		PPCImlOptimizerUsedRegisters_t registersUsed;
		PPCRecompiler_checkRegisterUsage(ppcImlGenContext, imlSegment->imlList+i, &registersUsed);
		if( registersUsed.readFPR1 == registerIndex || registersUsed.readFPR2 == registerIndex || registersUsed.readFPR3 == registerIndex || registersUsed.readFPR4 == registerIndex)
			return false;
		if( registersUsed.writtenFPR1 == registerIndex )
			return true;
	}
	// todo: Scan next segment(s)
	return false;
}

/*
 * Returns true if the loaded name is never changed
 */
bool PPCRecompiler_trackRedundantNameStoreInstruction(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 startIndex, PPCRecImlInstruction_t* nameStoreInstruction, sint32 scanDepth)
{
	sint16 registerIndex = nameStoreInstruction->op_r_name.registerIndex;
	for(sint32 i=startIndex; i>=0; i--)
	{
		PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList+i;
		PPCImlOptimizerUsedRegisters_t registersUsed;
		PPCRecompiler_checkRegisterUsage(ppcImlGenContext, imlSegment->imlList+i, &registersUsed);
		if( registersUsed.writtenNamedReg1 == registerIndex )
		{
			if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_R_NAME )
				return true;
			return false;
		}
	}
	return false;
}

sint32 debugCallCounter1 = 0;

/*
 * Returns true if the name is overwritten in the current or any following segments
 */
bool PPCRecompiler_trackOverwrittenNameStoreInstruction(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 startIndex, PPCRecImlInstruction_t* nameStoreInstruction, sint32 scanDepth)
{
	//sint16 registerIndex = nameStoreInstruction->op_r_name.registerIndex;
	uint32 name = nameStoreInstruction->op_r_name.name;
	for(sint32 i=startIndex; i<imlSegment->imlListCount; i++)
	{
		PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList+i;
		if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_R_NAME )
		{
			// name is loaded before being written
			if( imlSegment->imlList[i].op_r_name.name == name )
				return false;
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_NAME_R )
		{
			// name is written before being loaded
			if( imlSegment->imlList[i].op_r_name.name == name )
				return true;
		}
	}
	if( scanDepth >= 2 )
		return false;
	if( imlSegment->nextSegmentIsUncertain )
		return false;
	if( imlSegment->nextSegmentBranchTaken && PPCRecompiler_trackOverwrittenNameStoreInstruction(ppcImlGenContext, imlSegment->nextSegmentBranchTaken, 0, nameStoreInstruction, scanDepth+1) == false )
		return false;
	if( imlSegment->nextSegmentBranchNotTaken && PPCRecompiler_trackOverwrittenNameStoreInstruction(ppcImlGenContext, imlSegment->nextSegmentBranchNotTaken, 0, nameStoreInstruction, scanDepth+1) == false )
		return false;
	if( imlSegment->nextSegmentBranchTaken == NULL && imlSegment->nextSegmentBranchNotTaken == NULL )
		return false;

	return true;
}

/*
 * Returns true if the loaded FPR name is never changed
 */
bool PPCRecompiler_trackRedundantFPRNameStoreInstruction(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 startIndex, PPCRecImlInstruction_t* nameStoreInstruction, sint32 scanDepth)
{
	sint16 registerIndex = nameStoreInstruction->op_r_name.registerIndex;
	for(sint32 i=startIndex; i>=0; i--)
	{
		PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList+i;
		PPCImlOptimizerUsedRegisters_t registersUsed;
		PPCRecompiler_checkRegisterUsage(ppcImlGenContext, imlSegment->imlList+i, &registersUsed);
		if( registersUsed.writtenFPR1 == registerIndex )
		{
			if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_FPR_R_NAME )
				return true;
			return false;
		}
	}
	// todo: Scan next segment(s)
	return false;
}

uint32 _PPCRecompiler_getCROverwriteMask(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, uint32 currentOverwriteMask, uint32 currentReadMask, uint32 scanDepth)
{
	// is any bit overwritten but not read?
	uint32 overwriteMask = imlSegment->crBitsWritten&~imlSegment->crBitsInput;
	currentOverwriteMask |= overwriteMask;
	// next segment
	if( imlSegment->nextSegmentIsUncertain == false && scanDepth < 3 )
	{
		uint32 nextSegmentOverwriteMask = 0;
		if( imlSegment->nextSegmentBranchTaken && imlSegment->nextSegmentBranchNotTaken )
		{
			uint32 mask0 = _PPCRecompiler_getCROverwriteMask(ppcImlGenContext, imlSegment->nextSegmentBranchTaken, 0, 0, scanDepth+1);
			uint32 mask1 = _PPCRecompiler_getCROverwriteMask(ppcImlGenContext, imlSegment->nextSegmentBranchNotTaken, 0, 0, scanDepth+1);
			nextSegmentOverwriteMask = mask0&mask1;
		}
		else if( imlSegment->nextSegmentBranchNotTaken)
		{
			nextSegmentOverwriteMask = _PPCRecompiler_getCROverwriteMask(ppcImlGenContext, imlSegment->nextSegmentBranchNotTaken, 0, 0, scanDepth+1);
		}
		nextSegmentOverwriteMask &= ~imlSegment->crBitsRead;
		currentOverwriteMask |= nextSegmentOverwriteMask;
	}
	else if (imlSegment->nextSegmentIsUncertain)
	{
		if (ppcImlGenContext->segmentListCount >= 5)
		{
			return 7; // for more complex functions we assume that CR is not passed on
		}
	}
	return currentOverwriteMask;
}

/*
 * Returns a mask of all CR bits that are overwritten (written but not read) in the segment and all it's following segments
 * If the write state of a CR bit cannot be determined, it is returned as 0 (not overwritten)
 */
uint32 PPCRecompiler_getCROverwriteMask(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment)
{
	if (imlSegment->nextSegmentIsUncertain)
	{
		return 0;
	}
	if( imlSegment->nextSegmentBranchTaken && imlSegment->nextSegmentBranchNotTaken )
	{
		uint32 mask0 = _PPCRecompiler_getCROverwriteMask(ppcImlGenContext, imlSegment->nextSegmentBranchTaken, 0, 0, 0);
		uint32 mask1 = _PPCRecompiler_getCROverwriteMask(ppcImlGenContext, imlSegment->nextSegmentBranchNotTaken, 0, 0, 0);
		return mask0&mask1; // only return bits that are overwritten in both branches
	}
	else if( imlSegment->nextSegmentBranchNotTaken )
	{
		uint32 mask = _PPCRecompiler_getCROverwriteMask(ppcImlGenContext, imlSegment->nextSegmentBranchNotTaken, 0, 0, 0);
		return mask;
	}
	else
	{
		// not implemented
	}
	return 0;
}

void PPCRecompiler_removeRedundantCRUpdates(ppcImlGenContext_t* ppcImlGenContext)
{
	for(sint32 s=0; s<ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];

		for(sint32 i=0; i<imlSegment->imlListCount; i++)
		{
			PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList+i;
			if (imlInstruction->type == PPCREC_IML_TYPE_CJUMP)
			{
				if (imlInstruction->op_conditionalJump.condition != PPCREC_JUMP_CONDITION_NONE)
				{
					uint32 crBitFlag = 1 << (imlInstruction->op_conditionalJump.crRegisterIndex * 4 + imlInstruction->op_conditionalJump.crBitIndex);
					imlSegment->crBitsInput |= (crBitFlag&~imlSegment->crBitsWritten); // flag bits that have not already been written
					imlSegment->crBitsRead |= (crBitFlag);
				}
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
			{
				uint32 crBitFlag = 1 << (imlInstruction->op_conditional_r_s32.crRegisterIndex * 4 + imlInstruction->op_conditional_r_s32.crBitIndex);
				imlSegment->crBitsInput |= (crBitFlag&~imlSegment->crBitsWritten); // flag bits that have not already been written
				imlSegment->crBitsRead |= (crBitFlag);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_S32 && imlInstruction->operation == PPCREC_IML_OP_MFCR)
			{
				imlSegment->crBitsRead |= 0xFFFFFFFF;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_S32 && imlInstruction->operation == PPCREC_IML_OP_MTCRF)
			{
				imlSegment->crBitsWritten |= ppc_MTCRFMaskToCRBitMask((uint32)imlInstruction->op_r_immS32.immS32);
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_CR )
			{
				if (imlInstruction->operation == PPCREC_IML_OP_CR_CLEAR ||
					imlInstruction->operation == PPCREC_IML_OP_CR_SET)
				{
					uint32 crBitFlag = 1 << (imlInstruction->op_cr.crD);
					imlSegment->crBitsWritten |= (crBitFlag & ~imlSegment->crBitsWritten);
				}
				else if (imlInstruction->operation == PPCREC_IML_OP_CR_OR ||
					imlInstruction->operation == PPCREC_IML_OP_CR_ORC ||
					imlInstruction->operation == PPCREC_IML_OP_CR_AND ||
					imlInstruction->operation == PPCREC_IML_OP_CR_ANDC)
				{
					uint32 crBitFlag = 1 << (imlInstruction->op_cr.crD);
					imlSegment->crBitsWritten |= (crBitFlag & ~imlSegment->crBitsWritten);
					crBitFlag = 1 << (imlInstruction->op_cr.crA);
					imlSegment->crBitsRead |= (crBitFlag & ~imlSegment->crBitsRead);
					crBitFlag = 1 << (imlInstruction->op_cr.crB);
					imlSegment->crBitsRead |= (crBitFlag & ~imlSegment->crBitsRead);
				}
				else
					cemu_assert_unimplemented();
			}
			else if( PPCRecompilerImlAnalyzer_canTypeWriteCR(imlInstruction) && imlInstruction->crRegister >= 0 && imlInstruction->crRegister <= 7 )
			{
				imlSegment->crBitsWritten |= (0xF<<(imlInstruction->crRegister*4));
			}
			else if( (imlInstruction->type == PPCREC_IML_TYPE_STORE || imlInstruction->type == PPCREC_IML_TYPE_STORE_INDEXED) && imlInstruction->op_storeLoad.copyWidth == PPC_REC_STORE_STWCX_MARKER )
			{
				// overwrites CR0
				imlSegment->crBitsWritten |= (0xF<<0);
			}
		}
	}
	// flag instructions that write to CR where we can ignore individual CR bits
	for(sint32 s=0; s<ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
		for(sint32 i=0; i<imlSegment->imlListCount; i++)
		{
			PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList+i;
			if( PPCRecompilerImlAnalyzer_canTypeWriteCR(imlInstruction) && imlInstruction->crRegister >= 0 && imlInstruction->crRegister <= 7 )
			{
				uint32 crBitFlags = 0xF<<((uint32)imlInstruction->crRegister*4);
				uint32 crOverwriteMask = PPCRecompiler_getCROverwriteMask(ppcImlGenContext, imlSegment);
				uint32 crIgnoreMask = crOverwriteMask & ~imlSegment->crBitsRead;
				imlInstruction->crIgnoreMask = crIgnoreMask;
			}
		}
	}
}

bool PPCRecompiler_checkIfGPRIsModifiedInRange(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 startIndex, sint32 endIndex, sint32 vreg)
{
	PPCImlOptimizerUsedRegisters_t registersUsed;
	for (sint32 i = startIndex; i <= endIndex; i++)
	{
		PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList + i;
		PPCRecompiler_checkRegisterUsage(ppcImlGenContext, imlInstruction, &registersUsed);
		if (registersUsed.writtenNamedReg1 == vreg)
			return true;
	}
	return false;
}

sint32 PPCRecompiler_scanBackwardsForReusableRegister(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* startSegment, sint32 startIndex, sint32 name)
{
	// current segment
	sint32 currentIndex = startIndex;
	PPCRecImlSegment_t* currentSegment = startSegment;
	sint32 segmentIterateCount = 0;
	sint32 foundRegister = -1;
	while (true)
	{
		// stop scanning if segment is enterable
		if (currentSegment->isEnterable)
			return -1;
		while (currentIndex >= 0)
		{
			if (currentSegment->imlList[currentIndex].type == PPCREC_IML_TYPE_NAME_R && currentSegment->imlList[currentIndex].op_r_name.name == name)
			{
				foundRegister = currentSegment->imlList[currentIndex].op_r_name.registerIndex;
				break;
			}
			// previous instruction
			currentIndex--;
		}
		if (foundRegister >= 0)
			break;
		// continue at previous segment (if there is only one)
		if (segmentIterateCount >= 1)
			return -1;
		if (currentSegment->list_prevSegments.size() != 1)
			return -1;
		currentSegment = currentSegment->list_prevSegments[0];
		currentIndex = currentSegment->imlListCount - 1;
		segmentIterateCount++;
	}
	// scan again to make sure the register is not modified inbetween
	currentIndex = startIndex;
	currentSegment = startSegment;
	segmentIterateCount = 0;
	PPCImlOptimizerUsedRegisters_t registersUsed;
	while (true)
	{
		while (currentIndex >= 0)
		{
			// check if register is modified
			PPCRecompiler_checkRegisterUsage(ppcImlGenContext, currentSegment->imlList+currentIndex, &registersUsed);
			if (registersUsed.writtenNamedReg1 == foundRegister)
				return -1;
			// check if end of scan reached
			if (currentSegment->imlList[currentIndex].type == PPCREC_IML_TYPE_NAME_R && currentSegment->imlList[currentIndex].op_r_name.name == name)
			{
				//foundRegister = currentSegment->imlList[currentIndex].op_r_name.registerIndex;
				return foundRegister;
			}
			// previous instruction
			currentIndex--;
		}
		// continue at previous segment (if there is only one)
		if (segmentIterateCount >= 1)
			return -1;
		if (currentSegment->list_prevSegments.size() != 1)
			return -1;
		currentSegment = currentSegment->list_prevSegments[0];
		currentIndex = currentSegment->imlListCount - 1;
		segmentIterateCount++;
	}
	return -1;
}

void PPCRecompiler_optimizeDirectFloatCopiesScanForward(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 imlIndexLoad, sint32 fprIndex)
{
	PPCRecImlInstruction_t* imlInstructionLoad = imlSegment->imlList + imlIndexLoad;
	if (imlInstructionLoad->op_storeLoad.flags2.notExpanded)
		return;

	PPCImlOptimizerUsedRegisters_t registersUsed;
	sint32 scanRangeEnd = std::min(imlIndexLoad + 25, imlSegment->imlListCount); // don't scan too far (saves performance and also the chances we can merge the load+store become low at high distances)
	bool foundMatch = false;
	sint32 lastStore = -1;
	for (sint32 i = imlIndexLoad + 1; i < scanRangeEnd; i++)
	{
		PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList + i;
		if (PPCRecompiler_isSuffixInstruction(imlInstruction))
		{
			break;
		}

		// check if FPR is stored
		if ((imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE && imlInstruction->op_storeLoad.mode == PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0) ||
			(imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE_INDEXED && imlInstruction->op_storeLoad.mode == PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0))
		{
			if (imlInstruction->op_storeLoad.registerData == fprIndex)
			{
				if (foundMatch == false)
				{
					// flag the load-single instruction as "don't expand" (leave single value as-is)
					imlInstructionLoad->op_storeLoad.flags2.notExpanded = true;
				}
				// also set the flag for the store instruction
				PPCRecImlInstruction_t* imlInstructionStore = imlInstruction;
				imlInstructionStore->op_storeLoad.flags2.notExpanded = true;

				foundMatch = true;
				lastStore = i + 1;

				continue;
			}
		}

		// check if FPR is overwritten (we can actually ignore read operations?)
		PPCRecompiler_checkRegisterUsage(ppcImlGenContext, imlInstruction, &registersUsed);
		if (registersUsed.writtenFPR1 == fprIndex)
			break;
		if (registersUsed.readFPR1 == fprIndex)
			break;
		if (registersUsed.readFPR2 == fprIndex)
			break;
		if (registersUsed.readFPR3 == fprIndex)
			break;
		if (registersUsed.readFPR4 == fprIndex)
			break;
	}

	if (foundMatch)
	{
		// insert expand instruction after store
		PPCRecImlInstruction_t* newExpand = PPCRecompiler_insertInstruction(imlSegment, lastStore);
		PPCRecompilerImlGen_generateNewInstruction_fpr_r(ppcImlGenContext, newExpand, PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64, fprIndex);
	}
}

/*
* Scans for patterns:
* <Load sp float into register f>
* <Random unrelated instructions>
* <Store sp float from register f>
* For these patterns the store and load is modified to work with un-extended values (float remains as float, no double conversion)
* The float->double extension is then executed later
* Advantages:
* Keeps denormals and other special float values intact
* Slightly improves performance
*/
void PPCRecompiler_optimizeDirectFloatCopies(ppcImlGenContext_t* ppcImlGenContext)
{
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];

		for (sint32 i = 0; i < imlSegment->imlListCount; i++)
		{
			PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList + i;
			if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD && imlInstruction->op_storeLoad.mode == PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1)
			{
				PPCRecompiler_optimizeDirectFloatCopiesScanForward(ppcImlGenContext, imlSegment, i, imlInstruction->op_storeLoad.registerData);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED && imlInstruction->op_storeLoad.mode == PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1)
			{
				PPCRecompiler_optimizeDirectFloatCopiesScanForward(ppcImlGenContext, imlSegment, i, imlInstruction->op_storeLoad.registerData);
			}
		}
	}
}

void PPCRecompiler_optimizeDirectIntegerCopiesScanForward(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlSegment_t* imlSegment, sint32 imlIndexLoad, sint32 gprIndex)
{
	PPCRecImlInstruction_t* imlInstructionLoad = imlSegment->imlList + imlIndexLoad;
	if ( imlInstructionLoad->op_storeLoad.flags2.swapEndian == false )
		return;
	bool foundMatch = false;
	PPCImlOptimizerUsedRegisters_t registersUsed;
	sint32 scanRangeEnd = std::min(imlIndexLoad + 25, imlSegment->imlListCount); // don't scan too far (saves performance and also the chances we can merge the load+store become low at high distances)
	sint32 i = imlIndexLoad + 1;
	for (; i < scanRangeEnd; i++)
	{
		PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList + i;
		if (PPCRecompiler_isSuffixInstruction(imlInstruction))
		{
			break;
		}
		// check if GPR is stored
		if ((imlInstruction->type == PPCREC_IML_TYPE_STORE && imlInstruction->op_storeLoad.copyWidth == 32 ) )
		{
			if (imlInstruction->op_storeLoad.registerMem == gprIndex)
				break;
			if (imlInstruction->op_storeLoad.registerData == gprIndex)
			{
				PPCRecImlInstruction_t* imlInstructionStore = imlInstruction;
				if (foundMatch == false)
				{
					// switch the endian swap flag for the load instruction
					imlInstructionLoad->op_storeLoad.flags2.swapEndian = !imlInstructionLoad->op_storeLoad.flags2.swapEndian;
					foundMatch = true;
				}
				// switch the endian swap flag for the store instruction
				imlInstructionStore->op_storeLoad.flags2.swapEndian = !imlInstructionStore->op_storeLoad.flags2.swapEndian;
				// keep scanning
				continue;
			}
		}
		// check if GPR is accessed
		PPCRecompiler_checkRegisterUsage(ppcImlGenContext, imlInstruction, &registersUsed);
		if (registersUsed.readNamedReg1 == gprIndex ||
			registersUsed.readNamedReg2 == gprIndex ||
			registersUsed.readNamedReg3 == gprIndex)
		{
			break;
		}
		if (registersUsed.writtenNamedReg1 == gprIndex)
			return; // GPR overwritten, we don't need to byte swap anymore
	}
	if (foundMatch)
	{
		// insert expand instruction
		PPCRecImlInstruction_t* newExpand = PPCRecompiler_insertInstruction(imlSegment, i);
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, newExpand, PPCREC_IML_OP_ENDIAN_SWAP, gprIndex, gprIndex);
	}
}

/*
* Scans for patterns:
* <Load sp integer into register r>
* <Random unrelated instructions>
* <Store sp integer from register r>
* For these patterns the store and load is modified to work with non-swapped values
* The big_endian->little_endian conversion is then executed later
* Advantages:
* Slightly improves performance
*/
void PPCRecompiler_optimizeDirectIntegerCopies(ppcImlGenContext_t* ppcImlGenContext)
{
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];

		for (sint32 i = 0; i < imlSegment->imlListCount; i++)
		{
			PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList + i;
			if (imlInstruction->type == PPCREC_IML_TYPE_LOAD && imlInstruction->op_storeLoad.copyWidth == 32 && imlInstruction->op_storeLoad.flags2.swapEndian )
			{
				PPCRecompiler_optimizeDirectIntegerCopiesScanForward(ppcImlGenContext, imlSegment, i, imlInstruction->op_storeLoad.registerData);
			}
		}
	}
}

sint32 _getGQRIndexFromRegister(ppcImlGenContext_t* ppcImlGenContext, sint32 registerIndex)
{
	if (registerIndex == PPC_REC_INVALID_REGISTER)
		return -1;
	sint32 namedReg = ppcImlGenContext->mappedRegister[registerIndex];
	if (namedReg >= (PPCREC_NAME_SPR0 + SPR_UGQR0) && namedReg <= (PPCREC_NAME_SPR0 + SPR_UGQR7))
	{
		return namedReg - (PPCREC_NAME_SPR0 + SPR_UGQR0);
	}
	return -1;
}

bool PPCRecompiler_isUGQRValueKnown(ppcImlGenContext_t* ppcImlGenContext, sint32 gqrIndex, uint32& gqrValue)
{
	// UGQR 2 to 7 are initialized by the OS and we assume that games won't ever permanently touch those
	// todo - hack - replace with more accurate solution
	if (gqrIndex == 2)
		gqrValue = 0x00040004;
	else if (gqrIndex == 3)
		gqrValue = 0x00050005;
	else if (gqrIndex == 4)
		gqrValue = 0x00060006;
	else if (gqrIndex == 5)
		gqrValue = 0x00070007;
	else
		return false;
	return true;
}

/*
 * If value of GQR can be predicted for a given PSQ load or store instruction then replace it with an optimized version
 */
void PPCRecompiler_optimizePSQLoadAndStore(ppcImlGenContext_t* ppcImlGenContext)
{
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
		for (sint32 i = 0; i < imlSegment->imlListCount; i++)
		{
			PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList + i;
			if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD || imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED)
			{
				if(imlInstruction->op_storeLoad.mode != PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0 &&
					imlInstruction->op_storeLoad.mode != PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1 )
					continue;
				// get GQR value
				cemu_assert_debug(imlInstruction->op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
				sint32 gqrIndex = _getGQRIndexFromRegister(ppcImlGenContext, imlInstruction->op_storeLoad.registerGQR);
				cemu_assert(gqrIndex >= 0);
				if (ppcImlGenContext->tracking.modifiesGQR[gqrIndex])
					continue;
				//uint32 gqrValue = ppcInterpreterCurrentInstance->sprNew.UGQR[gqrIndex];
				uint32 gqrValue;
				if (!PPCRecompiler_isUGQRValueKnown(ppcImlGenContext, gqrIndex, gqrValue))
					continue;

				uint32 formatType = (gqrValue >> 16) & 7;
				uint32 scale = (gqrValue >> 24) & 0x3F;
				if (scale != 0)
					continue; // only generic handler supports scale
				if (imlInstruction->op_storeLoad.mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0)
				{
					if (formatType == 0)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0;
					else if (formatType == 4)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_U8_PS0;
					else if (formatType == 5)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_U16_PS0;
					else if (formatType == 6)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_S8_PS0;
					else if (formatType == 7)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_S16_PS0;
				}
				else if (imlInstruction->op_storeLoad.mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1)
				{
					if (formatType == 0)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1;
					else if (formatType == 4)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1;
					else if (formatType == 5)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1;
					else if (formatType == 6)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1;
					else if (formatType == 7)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1;
				}
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE || imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE_INDEXED)
			{
				if(imlInstruction->op_storeLoad.mode != PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0 &&
					imlInstruction->op_storeLoad.mode != PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1)
					continue;
				// get GQR value
				cemu_assert_debug(imlInstruction->op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
				sint32 gqrIndex = _getGQRIndexFromRegister(ppcImlGenContext, imlInstruction->op_storeLoad.registerGQR);
				cemu_assert(gqrIndex >= 0);
				if (ppcImlGenContext->tracking.modifiesGQR[gqrIndex])
					continue;
				uint32 gqrValue;
				if(!PPCRecompiler_isUGQRValueKnown(ppcImlGenContext, gqrIndex, gqrValue))
					continue;
				uint32 formatType = (gqrValue >> 16) & 7;
				uint32 scale = (gqrValue >> 24) & 0x3F;
				if (scale != 0)
					continue; // only generic handler supports scale
				if (imlInstruction->op_storeLoad.mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0)
				{
					if (formatType == 0)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0;
					else if (formatType == 4)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_U8_PS0;
					else if (formatType == 5)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_U16_PS0;
					else if (formatType == 6)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_S8_PS0;
					else if (formatType == 7)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_S16_PS0;
				}
				else if (imlInstruction->op_storeLoad.mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1)
				{
					if (formatType == 0)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1;
					else if (formatType == 4)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1;
					else if (formatType == 5)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1;
					else if (formatType == 6)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1;
					else if (formatType == 7)
						imlInstruction->op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1;
				}
			}
		}
	}
}

/*
 * Returns true if registerWrite overwrites any of the registers read by registerRead
 */
bool PPCRecompilerAnalyzer_checkForGPROverwrite(PPCImlOptimizerUsedRegisters_t* registerRead, PPCImlOptimizerUsedRegisters_t* registerWrite)
{
	if (registerWrite->writtenNamedReg1 < 0)
		return false;

	if (registerWrite->writtenNamedReg1 == registerRead->readNamedReg1)
		return true;
	if (registerWrite->writtenNamedReg1 == registerRead->readNamedReg2)
		return true;
	if (registerWrite->writtenNamedReg1 == registerRead->readNamedReg3)
		return true;
	return false;
}

void _reorderConditionModifyInstructions(PPCRecImlSegment_t* imlSegment)
{
	PPCRecImlInstruction_t* lastInstruction = PPCRecompilerIML_getLastInstruction(imlSegment);
	// last instruction a conditional branch?
	if (lastInstruction == nullptr || lastInstruction->type != PPCREC_IML_TYPE_CJUMP)
		return;
	if (lastInstruction->op_conditionalJump.crRegisterIndex >= 8)
		return;
	// get CR bitmask of bit required for conditional jump
	PPCRecCRTracking_t crTracking;
	PPCRecompilerImlAnalyzer_getCRTracking(lastInstruction, &crTracking);
	uint32 requiredCRBits = crTracking.readCRBits;

	// scan backwards until we find the instruction that sets the CR
	sint32 crSetterInstructionIndex = -1;
	sint32 unsafeInstructionIndex = -1;
	for (sint32 i = imlSegment->imlListCount-2; i >= 0; i--)
	{
		PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList + i;
		PPCRecompilerImlAnalyzer_getCRTracking(imlInstruction, &crTracking);
		if (crTracking.readCRBits != 0)
			return; // dont handle complex cases for now
		if (crTracking.writtenCRBits != 0)
		{
			if ((crTracking.writtenCRBits&requiredCRBits) != 0)
			{
				crSetterInstructionIndex = i;
				break;
			}
			else
			{
				return; // other CR bits overwritten (dont handle complex cases)
			}
		}
		// is safe? (no risk of overwriting x64 eflags)
		if ((imlInstruction->type == PPCREC_IML_TYPE_NAME_R || imlInstruction->type == PPCREC_IML_TYPE_R_NAME || imlInstruction->type == PPCREC_IML_TYPE_NO_OP) ||
			(imlInstruction->type == PPCREC_IML_TYPE_FPR_NAME_R || imlInstruction->type == PPCREC_IML_TYPE_FPR_R_NAME) ||
			(imlInstruction->type == PPCREC_IML_TYPE_R_S32 && (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)) ||
			(imlInstruction->type == PPCREC_IML_TYPE_R_R && (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)) )
			continue;
		// not safe
		//hasUnsafeInstructions = true;
		if (unsafeInstructionIndex == -1)
			unsafeInstructionIndex = i;
	}
	if (crSetterInstructionIndex < 0)
		return;
	if (unsafeInstructionIndex < 0)
		return; // no danger of overwriting eflags, don't reorder
	// check if we can move the CR setter instruction to after unsafeInstructionIndex
	PPCRecCRTracking_t crTrackingSetter = crTracking;
	PPCImlOptimizerUsedRegisters_t regTrackingCRSetter;
	PPCRecompiler_checkRegisterUsage(NULL, imlSegment->imlList+crSetterInstructionIndex, &regTrackingCRSetter);
	if (regTrackingCRSetter.writtenFPR1 >= 0 || regTrackingCRSetter.readFPR1 >= 0 || regTrackingCRSetter.readFPR2 >= 0 || regTrackingCRSetter.readFPR3 >= 0 || regTrackingCRSetter.readFPR4 >= 0)
		return; // we don't handle FPR dependency yet so just ignore FPR instructions
	PPCImlOptimizerUsedRegisters_t registerTracking;
	if (regTrackingCRSetter.writtenNamedReg1 >= 0)
	{
		// CR setter does write GPR
		for (sint32 i = crSetterInstructionIndex + 1; i <= unsafeInstructionIndex; i++)
		{
			PPCRecompiler_checkRegisterUsage(NULL, imlSegment->imlList + i, &registerTracking);
			// reads register written by CR setter?
			if (PPCRecompilerAnalyzer_checkForGPROverwrite(&registerTracking, &regTrackingCRSetter))
			{
				return; // cant move CR setter because of dependency
			}
			// writes register read by CR setter?
			if (PPCRecompilerAnalyzer_checkForGPROverwrite(&regTrackingCRSetter, &registerTracking))
			{
				return; // cant move CR setter because of dependency
			}
			// overwrites register written by CR setter?
			if (regTrackingCRSetter.writtenNamedReg1 == registerTracking.writtenNamedReg1)
				return;
		}
	}
	else
	{
		// CR setter does not write GPR
		for (sint32 i = crSetterInstructionIndex + 1; i <= unsafeInstructionIndex; i++)
		{
			PPCRecompiler_checkRegisterUsage(NULL, imlSegment->imlList + i, &registerTracking);
			// writes register read by CR setter?
			if (PPCRecompilerAnalyzer_checkForGPROverwrite(&regTrackingCRSetter, &registerTracking))
			{
				return; // cant move CR setter because of dependency
			}
		}
	}

	// move CR setter instruction
#ifdef CEMU_DEBUG_ASSERT
	if ((unsafeInstructionIndex + 1) <= crSetterInstructionIndex)
		assert_dbg();
#endif
	PPCRecImlInstruction_t* newCRSetterInstruction = PPCRecompiler_insertInstruction(imlSegment, unsafeInstructionIndex+1);
	memcpy(newCRSetterInstruction, imlSegment->imlList + crSetterInstructionIndex, sizeof(PPCRecImlInstruction_t));
	PPCRecompilerImlGen_generateNewInstruction_noOp(NULL, imlSegment->imlList + crSetterInstructionIndex);
}

/*
 * Move instructions which update the condition flags closer to the instruction that consumes them
 * On x64 this improves performance since we often can avoid storing CR in memory
 */
void PPCRecompiler_reorderConditionModifyInstructions(ppcImlGenContext_t* ppcImlGenContext)
{
	// check if this segment has a conditional branch
	for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
		_reorderConditionModifyInstructions(imlSegment);
	}
}
