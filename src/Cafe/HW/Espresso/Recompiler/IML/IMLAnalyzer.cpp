#include "IML.h"
//#include "PPCRecompilerIml.h"
#include "util/helpers/fixedSizeList.h"

#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"

/*
 * Analyzes a single segment and returns true if it is a finite loop
 */
bool IMLAnalyzer_IsTightFiniteLoop(IMLSegment* imlSegment)
{
	return false; // !!! DISABLED !!!

	bool isTightFiniteLoop = false;
	// base criteria, must jump to beginning of same segment
	if (imlSegment->nextSegmentBranchTaken != imlSegment)
		return false;
	// loops using BDNZ are assumed to always be finite
	for(const IMLInstruction& instIt : imlSegment->imlList)
	{
		if (instIt.type == PPCREC_IML_TYPE_R_S32 && instIt.operation == PPCREC_IML_OP_SUB)
		{
			return true;
		}
	}
	// for non-BDNZ loops, check for common patterns
	// risky approach, look for ADD/SUB operations and assume that potential overflow means finite (does not include r_r_s32 ADD/SUB)
	// this catches most loops with load-update and store-update instructions, but also those with decrementing counters
	FixedSizeList<IMLReg, 64, true> list_modifiedRegisters;
	for (const IMLInstruction& instIt : imlSegment->imlList)
	{
		if (instIt.type == PPCREC_IML_TYPE_R_S32 && (instIt.operation == PPCREC_IML_OP_ADD || instIt.operation == PPCREC_IML_OP_SUB) )
		{
			list_modifiedRegisters.addUnique(instIt.op_r_immS32.regR);
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
			registersUsed.ForEachWrittenGPR([&](IMLReg r) { list_modifiedRegisters.remove(r); });
		}
		if (list_modifiedRegisters.count > 0)
		{
			return true;
		}
	}
	return false;
}