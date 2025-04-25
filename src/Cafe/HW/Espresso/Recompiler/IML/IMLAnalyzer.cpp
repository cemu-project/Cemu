#include "IML.h"
//#include "PPCRecompilerIml.h"
#include "util/helpers/fixedSizeList.h"

#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"

/*
 * Initializes a single segment and returns true if it is a finite loop
 */
bool IMLAnalyzer_IsTightFiniteLoop(IMLSegment* imlSegment)
{
	bool isTightFiniteLoop = false;
	// base criteria, must jump to beginning of same segment
	if (imlSegment->nextSegmentBranchTaken != imlSegment)
		return false;
	// loops using BDNZ are assumed to always be finite
	for(const IMLInstruction& instIt : imlSegment->imlList)
	{
		if (instIt.type == PPCREC_IML_TYPE_R_S32 && instIt.operation == PPCREC_IML_OP_SUB && instIt.crRegister == 8)
		{
			return true;
		}
	}
	// for non-BDNZ loops, check for common patterns
	// risky approach, look for ADD/SUB operations and assume that potential overflow means finite (does not include r_r_s32 ADD/SUB)
	// this catches most loops with load-update and store-update instructions, but also those with decrementing counters
	FixedSizeList<sint32, 64, true> list_modifiedRegisters;
	for (const IMLInstruction& instIt : imlSegment->imlList)
	{
		if (instIt.type == PPCREC_IML_TYPE_R_S32 && (instIt.operation == PPCREC_IML_OP_ADD || instIt.operation == PPCREC_IML_OP_SUB) )
		{
			list_modifiedRegisters.addUnique(instIt.op_r_immS32.registerIndex);
		}
	}
	if (list_modifiedRegisters.count > 0)
	{
		// remove all registers from the list that are modified by non-ADD/SUB instructions
		// todo: We should also cover the case where ADD+SUB on the same register cancel the effect out
		IMLUsedRegisters registersUsed;
		for (const IMLInstruction& instIt : imlSegment->imlList)
		{
			if (instIt.type == PPCREC_IML_TYPE_R_S32 && (instIt.operation == PPCREC_IML_OP_ADD || instIt.operation == PPCREC_IML_OP_SUB))
				continue;
			instIt.CheckRegisterUsage(&registersUsed);
			if(registersUsed.writtenNamedReg1 < 0)
				continue;
			list_modifiedRegisters.remove(registersUsed.writtenNamedReg1);
		}
		if (list_modifiedRegisters.count > 0)
		{
			return true;
		}
	}
	return false;
}

/*
* Returns true if the instruction can overwrite CR (depending on value of ->crRegister)
*/
bool IMLAnalyzer_CanTypeWriteCR(IMLInstruction* imlInstruction)
{
	if (imlInstruction->type == PPCREC_IML_TYPE_R_R)
		return true;
	if (imlInstruction->type == PPCREC_IML_TYPE_R_R_R)
		return true;
	if (imlInstruction->type == PPCREC_IML_TYPE_COMPARE || imlInstruction->type == PPCREC_IML_TYPE_COMPARE_S32)
		return true; // ??
	if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_JUMP)
		return true; // ??
	if (imlInstruction->type == PPCREC_IML_TYPE_R_R_S32)
		return true;
	if (imlInstruction->type == PPCREC_IML_TYPE_R_S32)
		return true;
	if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R)
		return true;
	if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R)
		return true;
	if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R_R)
		return true;
	if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R)
		return true;
	return false;
}

void IMLAnalyzer_GetCRTracking(IMLInstruction* imlInstruction, PPCRecCRTracking_t* crTracking)
{
	crTracking->readCRBits = 0;
	crTracking->writtenCRBits = 0;
	if (imlInstruction->type == PPCREC_IML_TYPE_CJUMP)
	{
		if (imlInstruction->op_conditionalJump.condition != PPCREC_JUMP_CONDITION_NONE)
		{
			uint32 crBitFlag = 1 << (imlInstruction->op_conditionalJump.crRegisterIndex * 4 + imlInstruction->op_conditionalJump.crBitIndex);
			crTracking->readCRBits = (crBitFlag);
		}
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
	{
		uint32 crBitFlag = 1 << (imlInstruction->op_conditional_r_s32.crRegisterIndex * 4 + imlInstruction->op_conditional_r_s32.crBitIndex);
		crTracking->readCRBits = crBitFlag;
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_R_S32 && imlInstruction->operation == PPCREC_IML_OP_MFCR)
	{
		crTracking->readCRBits = 0xFFFFFFFF;
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_R_S32 && imlInstruction->operation == PPCREC_IML_OP_MTCRF)
	{
		crTracking->writtenCRBits |= ppc_MTCRFMaskToCRBitMask((uint32)imlInstruction->op_r_immS32.immS32);
	}
	else if (imlInstruction->type == PPCREC_IML_TYPE_CR)
	{
		if (imlInstruction->operation == PPCREC_IML_OP_CR_CLEAR ||
			imlInstruction->operation == PPCREC_IML_OP_CR_SET)
		{
			uint32 crBitFlag = 1 << (imlInstruction->op_cr.crD);
			crTracking->writtenCRBits = crBitFlag;
		}
		else if (imlInstruction->operation == PPCREC_IML_OP_CR_OR ||
			imlInstruction->operation == PPCREC_IML_OP_CR_ORC ||
			imlInstruction->operation == PPCREC_IML_OP_CR_AND || 
			imlInstruction->operation == PPCREC_IML_OP_CR_ANDC)
		{
			uint32 crBitFlag = 1 << (imlInstruction->op_cr.crD);
			crTracking->writtenCRBits = crBitFlag;
			crBitFlag = 1 << (imlInstruction->op_cr.crA);
			crTracking->readCRBits = crBitFlag;
			crBitFlag = 1 << (imlInstruction->op_cr.crB);
			crTracking->readCRBits |= crBitFlag;
		}
		else
			assert_dbg();
	}
	else if (IMLAnalyzer_CanTypeWriteCR(imlInstruction) && imlInstruction->crRegister >= 0 && imlInstruction->crRegister <= 7)
	{
		crTracking->writtenCRBits |= (0xF << (imlInstruction->crRegister * 4));
	}
	else if ((imlInstruction->type == PPCREC_IML_TYPE_STORE || imlInstruction->type == PPCREC_IML_TYPE_STORE_INDEXED) && imlInstruction->op_storeLoad.copyWidth == PPC_REC_STORE_STWCX_MARKER)
	{
		// overwrites CR0
		crTracking->writtenCRBits |= (0xF << 0);
	}
}