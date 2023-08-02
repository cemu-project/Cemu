#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/Espresso/Recompiler/IML/IML.h"
#include "Cafe/HW/Espresso/Recompiler/IML/IMLInstruction.h"

#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"
#include "../BackendX64/BackendX64.h"

IMLReg _FPRRegFromID(IMLRegID regId)
{
	return IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, regId);
}

void PPCRecompiler_optimizeDirectFloatCopiesScanForward(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 imlIndexLoad, IMLReg fprReg)
{
	IMLRegID fprIndex = fprReg.GetRegID();

	IMLInstruction* imlInstructionLoad = imlSegment->imlList.data() + imlIndexLoad;
	if (imlInstructionLoad->op_storeLoad.flags2.notExpanded)
		return;

	IMLUsedRegisters registersUsed;
	sint32 scanRangeEnd = std::min<sint32>(imlIndexLoad + 25, imlSegment->imlList.size()); // don't scan too far (saves performance and also the chances we can merge the load+store become low at high distances)
	bool foundMatch = false;
	sint32 lastStore = -1;
	for (sint32 i = imlIndexLoad + 1; i < scanRangeEnd; i++)
	{
		IMLInstruction* imlInstruction = imlSegment->imlList.data() + i;
		if (imlInstruction->IsSuffixInstruction())
			break;
		// check if FPR is stored
		if ((imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE && imlInstruction->op_storeLoad.mode == PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0) ||
			(imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE_INDEXED && imlInstruction->op_storeLoad.mode == PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0))
		{
			if (imlInstruction->op_storeLoad.registerData.GetRegID() == fprIndex)
			{
				if (foundMatch == false)
				{
					// flag the load-single instruction as "don't expand" (leave single value as-is)
					imlInstructionLoad->op_storeLoad.flags2.notExpanded = true;
				}
				// also set the flag for the store instruction
				IMLInstruction* imlInstructionStore = imlInstruction;
				imlInstructionStore->op_storeLoad.flags2.notExpanded = true;

				foundMatch = true;
				lastStore = i + 1;

				continue;
			}
		}

		// check if FPR is overwritten (we can actually ignore read operations?)
		imlInstruction->CheckRegisterUsage(&registersUsed);
		if (registersUsed.writtenFPR1.IsValidAndSameRegID(fprIndex))
			break;
		if (registersUsed.readFPR1.IsValidAndSameRegID(fprIndex))
			break;
		if (registersUsed.readFPR2.IsValidAndSameRegID(fprIndex))
			break;
		if (registersUsed.readFPR3.IsValidAndSameRegID(fprIndex))
			break;
		if (registersUsed.readFPR4.IsValidAndSameRegID(fprIndex))
			break;
	}

	if (foundMatch)
	{
		// insert expand instruction after store
		IMLInstruction* newExpand = PPCRecompiler_insertInstruction(imlSegment, lastStore);
		PPCRecompilerImlGen_generateNewInstruction_fpr_r(ppcImlGenContext, newExpand, PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64, _FPRRegFromID(fprIndex));
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
void IMLOptimizer_OptimizeDirectFloatCopies(ppcImlGenContext_t* ppcImlGenContext)
{
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		for (sint32 i = 0; i < segIt->imlList.size(); i++)
		{
			IMLInstruction* imlInstruction = segIt->imlList.data() + i;
			if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD && imlInstruction->op_storeLoad.mode == PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1)
			{
				PPCRecompiler_optimizeDirectFloatCopiesScanForward(ppcImlGenContext, segIt, i, imlInstruction->op_storeLoad.registerData);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED && imlInstruction->op_storeLoad.mode == PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1)
			{
				PPCRecompiler_optimizeDirectFloatCopiesScanForward(ppcImlGenContext, segIt, i, imlInstruction->op_storeLoad.registerData);
			}
		}
	}
}

void PPCRecompiler_optimizeDirectIntegerCopiesScanForward(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 imlIndexLoad, IMLReg gprReg)
{
	cemu_assert_debug(gprReg.GetBaseFormat() == IMLRegFormat::I64); // todo - proper handling required for non-standard sizes
	cemu_assert_debug(gprReg.GetRegFormat() == IMLRegFormat::I32);

	IMLRegID gprIndex = gprReg.GetRegID();
	IMLInstruction* imlInstructionLoad = imlSegment->imlList.data() + imlIndexLoad;
	if ( imlInstructionLoad->op_storeLoad.flags2.swapEndian == false )
		return;
	bool foundMatch = false;
	IMLUsedRegisters registersUsed;
	sint32 scanRangeEnd = std::min<sint32>(imlIndexLoad + 25, imlSegment->imlList.size()); // don't scan too far (saves performance and also the chances we can merge the load+store become low at high distances)
	sint32 i = imlIndexLoad + 1;
	for (; i < scanRangeEnd; i++)
	{
		IMLInstruction* imlInstruction = imlSegment->imlList.data() + i;
		if (imlInstruction->IsSuffixInstruction())
			break;
		// check if GPR is stored
		if ((imlInstruction->type == PPCREC_IML_TYPE_STORE && imlInstruction->op_storeLoad.copyWidth == 32 ) )
		{
			if (imlInstruction->op_storeLoad.registerMem.GetRegID() == gprIndex)
				break;
			if (imlInstruction->op_storeLoad.registerData.GetRegID() == gprIndex)
			{
				IMLInstruction* imlInstructionStore = imlInstruction;
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
		imlInstruction->CheckRegisterUsage(&registersUsed);
		if (registersUsed.readGPR1.IsValidAndSameRegID(gprIndex) ||
			registersUsed.readGPR2.IsValidAndSameRegID(gprIndex) ||
			registersUsed.readGPR3.IsValidAndSameRegID(gprIndex))
		{
			break;
		}
		if (registersUsed.IsBaseGPRWritten(gprReg))
			return; // GPR overwritten, we don't need to byte swap anymore
	}
	if (foundMatch)
	{
		PPCRecompiler_insertInstruction(imlSegment, i)->make_r_r(PPCREC_IML_OP_ENDIAN_SWAP, gprReg, gprReg);
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
void IMLOptimizer_OptimizeDirectIntegerCopies(ppcImlGenContext_t* ppcImlGenContext)
{
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		for (sint32 i = 0; i < segIt->imlList.size(); i++)
		{
			IMLInstruction* imlInstruction = segIt->imlList.data() + i;
			if (imlInstruction->type == PPCREC_IML_TYPE_LOAD && imlInstruction->op_storeLoad.copyWidth == 32 && imlInstruction->op_storeLoad.flags2.swapEndian )
			{
				PPCRecompiler_optimizeDirectIntegerCopiesScanForward(ppcImlGenContext, segIt, i, imlInstruction->op_storeLoad.registerData);
			}
		}
	}
}

IMLName PPCRecompilerImlGen_GetRegName(ppcImlGenContext_t* ppcImlGenContext, IMLReg reg);

sint32 _getGQRIndexFromRegister(ppcImlGenContext_t* ppcImlGenContext, IMLReg gqrReg)
{
	if (gqrReg.IsInvalid())
		return -1;
	sint32 namedReg = PPCRecompilerImlGen_GetRegName(ppcImlGenContext, gqrReg);
	if (namedReg >= (PPCREC_NAME_SPR0 + SPR_UGQR0) && namedReg <= (PPCREC_NAME_SPR0 + SPR_UGQR7))
	{
		return namedReg - (PPCREC_NAME_SPR0 + SPR_UGQR0);
	}
	else
	{
		cemu_assert_suspicious();
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
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2) 
	{
		for(IMLInstruction& instIt : segIt->imlList)
		{
			if (instIt.type == PPCREC_IML_TYPE_FPR_LOAD || instIt.type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED)
			{
				if(instIt.op_storeLoad.mode != PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0 &&
					instIt.op_storeLoad.mode != PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1 )
					continue;
				// get GQR value
				cemu_assert_debug(instIt.op_storeLoad.registerGQR.IsValid());
				sint32 gqrIndex = _getGQRIndexFromRegister(ppcImlGenContext, instIt.op_storeLoad.registerGQR);
				cemu_assert(gqrIndex >= 0);
				if (ppcImlGenContext->tracking.modifiesGQR[gqrIndex])
					continue;
				uint32 gqrValue;
				if (!PPCRecompiler_isUGQRValueKnown(ppcImlGenContext, gqrIndex, gqrValue))
					continue;

				uint32 formatType = (gqrValue >> 16) & 7;
				uint32 scale = (gqrValue >> 24) & 0x3F;
				if (scale != 0)
					continue; // only generic handler supports scale
				if (instIt.op_storeLoad.mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0)
				{
					if (formatType == 0)
						instIt.op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0;
					else if (formatType == 4)
						instIt.op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_U8_PS0;
					else if (formatType == 5)
						instIt.op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_U16_PS0;
					else if (formatType == 6)
						instIt.op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_S8_PS0;
					else if (formatType == 7)
						instIt.op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_S16_PS0;
					if (instIt.op_storeLoad.mode != PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0)
						instIt.op_storeLoad.registerGQR = IMLREG_INVALID;
				}
				else if (instIt.op_storeLoad.mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1)
				{
					if (formatType == 0)
						instIt.op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1;
					else if (formatType == 4)
						instIt.op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1;
					else if (formatType == 5)
						instIt.op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1;
					else if (formatType == 6)
						instIt.op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1;
					else if (formatType == 7)
						instIt.op_storeLoad.mode = PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1;
					if (instIt.op_storeLoad.mode != PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1)
						instIt.op_storeLoad.registerGQR = IMLREG_INVALID;
				}
			}
			else if (instIt.type == PPCREC_IML_TYPE_FPR_STORE || instIt.type == PPCREC_IML_TYPE_FPR_STORE_INDEXED)
			{
				if(instIt.op_storeLoad.mode != PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0 &&
					instIt.op_storeLoad.mode != PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1)
					continue;
				// get GQR value
				cemu_assert_debug(instIt.op_storeLoad.registerGQR.IsValid());
				sint32 gqrIndex = _getGQRIndexFromRegister(ppcImlGenContext, instIt.op_storeLoad.registerGQR);
				cemu_assert(gqrIndex >= 0 && gqrIndex < 8);
				if (ppcImlGenContext->tracking.modifiesGQR[gqrIndex])
					continue;
				uint32 gqrValue;
				if(!PPCRecompiler_isUGQRValueKnown(ppcImlGenContext, gqrIndex, gqrValue))
					continue;
				uint32 formatType = (gqrValue >> 16) & 7;
				uint32 scale = (gqrValue >> 24) & 0x3F;
				if (scale != 0)
					continue; // only generic handler supports scale
				if (instIt.op_storeLoad.mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0)
				{
					if (formatType == 0)
						instIt.op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0;
					else if (formatType == 4)
						instIt.op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_U8_PS0;
					else if (formatType == 5)
						instIt.op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_U16_PS0;
					else if (formatType == 6)
						instIt.op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_S8_PS0;
					else if (formatType == 7)
						instIt.op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_S16_PS0;
					if (instIt.op_storeLoad.mode != PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0)
						instIt.op_storeLoad.registerGQR = IMLREG_INVALID;
				}
				else if (instIt.op_storeLoad.mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1)
				{
					if (formatType == 0)
						instIt.op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1;
					else if (formatType == 4)
						instIt.op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1;
					else if (formatType == 5)
						instIt.op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1;
					else if (formatType == 6)
						instIt.op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1;
					else if (formatType == 7)
						instIt.op_storeLoad.mode = PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1;
					if (instIt.op_storeLoad.mode != PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1)
						instIt.op_storeLoad.registerGQR = IMLREG_INVALID;
				}
			}
		}
	}
}
