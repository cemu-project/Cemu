#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/Espresso/Recompiler/IML/IML.h"
#include "Cafe/HW/Espresso/Recompiler/IML/IMLInstruction.h"

#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"
#include "../PPCRecompilerX64.h"

struct replacedRegisterTracker_t
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
};

bool PPCRecompiler_findAvailableRegisterDepr(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 imlIndexStart, replacedRegisterTracker_t* replacedRegisterTracker, sint32* registerIndex, sint32* registerName, bool* isUsed)
{
	IMLUsedRegisters registersUsed;
	imlSegment->imlList[imlIndexStart].CheckRegisterUsage(&registersUsed);
	// mask all registers used by this instruction
	uint32 instructionReservedRegisterMask = 0;
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

void PPCRecompiler_storeReplacedRegister(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, replacedRegisterTracker_t* replacedRegisterTracker, sint32 registerTrackerIndex, sint32* imlIndex)
{
	// store register
	sint32 imlIndexEdit = *imlIndex;
	PPCRecompiler_pushBackIMLInstructions(imlSegment, imlIndexEdit, 1);
	// name_unusedRegister = unusedRegister
	IMLInstruction& imlInstructionItr = imlSegment->imlList[imlIndexEdit + 0];
	memset(&imlInstructionItr, 0x00, sizeof(IMLInstruction));
	imlInstructionItr.type = PPCREC_IML_TYPE_NAME_R;
	imlInstructionItr.crRegister = PPC_REC_INVALID_REGISTER;
	imlInstructionItr.operation = PPCREC_IML_OP_ASSIGN;
	imlInstructionItr.op_r_name.registerIndex = replacedRegisterTracker->replacedRegisterEntry[registerTrackerIndex].index;
	imlInstructionItr.op_r_name.name = replacedRegisterTracker->replacedRegisterEntry[registerTrackerIndex].registerNewName;
	imlIndexEdit++;
	// load new register if required
	if( replacedRegisterTracker->replacedRegisterEntry[registerTrackerIndex].nameMustBeMaintained )
	{
		PPCRecompiler_pushBackIMLInstructions(imlSegment, imlIndexEdit, 1);
		IMLInstruction& imlInstructionItr = imlSegment->imlList[imlIndexEdit];
		memset(&imlInstructionItr, 0x00, sizeof(IMLInstruction));
		imlInstructionItr.type = PPCREC_IML_TYPE_R_NAME;
		imlInstructionItr.crRegister = PPC_REC_INVALID_REGISTER;
		imlInstructionItr.operation = PPCREC_IML_OP_ASSIGN;
		imlInstructionItr.op_r_name.registerIndex = replacedRegisterTracker->replacedRegisterEntry[registerTrackerIndex].index;
		imlInstructionItr.op_r_name.name = replacedRegisterTracker->replacedRegisterEntry[registerTrackerIndex].registerPreviousName;//ppcImlGenContext->mappedRegister[replacedRegisterTracker.replacedRegisterEntry[i].index];
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
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		size_t imlIndex = 0;
		while( imlIndex < segIt->imlList.size() )
		{
			IMLInstruction& imlInstructionItr = segIt->imlList[imlIndex];
			if( imlInstructionItr.type == PPCREC_IML_TYPE_FPR_R_NAME || imlInstructionItr.type == PPCREC_IML_TYPE_FPR_NAME_R )
			{
				if( imlInstructionItr.op_r_name.registerIndex >= PPC_X64_FPR_USABLE_REGISTERS )
				{
					// convert to NO-OP instruction
					imlInstructionItr.type = PPCREC_IML_TYPE_NO_OP;
					imlInstructionItr.associatedPPCAddress = 0;
				}
			}
			imlIndex++;
		}	
	}
	// replace registers
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		size_t imlIndex = 0;
		while( imlIndex < segIt->imlList.size() )
		{
			IMLUsedRegisters registersUsed;
			while( true )
			{
				segIt->imlList[imlIndex].CheckRegisterUsage(&registersUsed);
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
						replacedRegisterIsUsed = segIt->ppcFPRUsed[unusedRegisterName-PPCREC_NAME_FPR0];
					}
					// replace registers that are out of range
					segIt->imlList[imlIndex].ReplaceFPR(fprToReplace, unusedRegisterIndex);
					// add load/store name after instruction
					PPCRecompiler_pushBackIMLInstructions(segIt, imlIndex+1, 2);
					// add load/store before current instruction
					PPCRecompiler_pushBackIMLInstructions(segIt, imlIndex, 2);
					// name_unusedRegister = unusedRegister
					IMLInstruction* imlInstructionItr = segIt->imlList.data() + (imlIndex + 0);
					memset(imlInstructionItr, 0x00, sizeof(IMLInstruction));
					if( replacedRegisterIsUsed )
					{
						imlInstructionItr->type = PPCREC_IML_TYPE_FPR_NAME_R;
						imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
						imlInstructionItr->op_r_name.registerIndex = unusedRegisterIndex;
						imlInstructionItr->op_r_name.name = ppcImlGenContext->mappedFPRRegister[unusedRegisterIndex];
					}
					else
						imlInstructionItr->type = PPCREC_IML_TYPE_NO_OP;
					imlInstructionItr = segIt->imlList.data() + (imlIndex + 1);
					memset(imlInstructionItr, 0x00, sizeof(IMLInstruction));
					imlInstructionItr->type = PPCREC_IML_TYPE_FPR_R_NAME;
					imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
					imlInstructionItr->op_r_name.registerIndex = unusedRegisterIndex;
					imlInstructionItr->op_r_name.name = ppcImlGenContext->mappedFPRRegister[fprToReplace];
					// name_gprToReplace = unusedRegister
					imlInstructionItr = segIt->imlList.data() + (imlIndex + 3);
					memset(imlInstructionItr, 0x00, sizeof(IMLInstruction));
					imlInstructionItr->type = PPCREC_IML_TYPE_FPR_NAME_R;
					imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
					imlInstructionItr->op_r_name.registerIndex = unusedRegisterIndex;
					imlInstructionItr->op_r_name.name = ppcImlGenContext->mappedFPRRegister[fprToReplace];
					// unusedRegister = name_unusedRegister
					imlInstructionItr = segIt->imlList.data() + (imlIndex + 4);
					memset(imlInstructionItr, 0x00, sizeof(IMLInstruction));
					if( replacedRegisterIsUsed )
					{
						imlInstructionItr->type = PPCREC_IML_TYPE_FPR_R_NAME;
						imlInstructionItr->operation = PPCREC_IML_OP_ASSIGN;
						imlInstructionItr->op_r_name.registerIndex = unusedRegisterIndex;
						imlInstructionItr->op_r_name.name = ppcImlGenContext->mappedFPRRegister[unusedRegisterIndex];
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

ppcRecRegisterMapping_t* PPCRecompiler_findAvailableRegisterDepr(ppcRecManageRegisters_t* rCtx, IMLUsedRegisters* instructionUsedRegisters)
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

ppcRecRegisterMapping_t* PPCRecompiler_findUnloadableRegister(ppcRecManageRegisters_t* rCtx, IMLUsedRegisters* instructionUsedRegisters, uint32 unloadLockedMask)
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
	IMLSegment* imlSegment = ppcImlGenContext->segmentList2[segmentIndex];
	size_t idx = 0;
	sint32 currentUseIndex = 0;
	IMLUsedRegisters registersUsed;
	while (idx < imlSegment->imlList.size())
	{
		IMLInstruction& idxInst = imlSegment->imlList[idx];
		if (idxInst.IsSuffixInstruction())
			break;
		idxInst.CheckRegisterUsage(&registersUsed);
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
						IMLInstruction* imlInstructionTemp = imlSegment->imlList.data() + idx;
						memset(imlInstructionTemp, 0x00, sizeof(IMLInstruction));
						imlInstructionTemp->type = PPCREC_IML_TYPE_FPR_NAME_R;
						imlInstructionTemp->operation = PPCREC_IML_OP_ASSIGN;
						imlInstructionTemp->op_r_name.registerIndex = (uint8)(unloadRegMapping - rCtx.currentMapping);
						imlInstructionTemp->op_r_name.name = ppcImlGenContext->mappedFPRRegister[unloadRegMapping->virtualReg];
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
				IMLInstruction* imlInstructionTemp = imlSegment->imlList.data() + idx;
				memset(imlInstructionTemp, 0x00, sizeof(IMLInstruction));
				imlInstructionTemp->type = PPCREC_IML_TYPE_FPR_R_NAME;
				imlInstructionTemp->operation = PPCREC_IML_OP_ASSIGN;
				imlInstructionTemp->op_r_name.registerIndex = (uint8)(regMapping-rCtx.currentMapping);
				imlInstructionTemp->op_r_name.name = ppcImlGenContext->mappedFPRRegister[virtualFpr];
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
			imlSegment->imlList[idx].ReplaceFPRs(fprMatch, fprReplace);
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
			IMLInstruction* imlInstructionTemp = imlSegment->imlList.data() + idx;
			memset(imlInstructionTemp, 0x00, sizeof(IMLInstruction));
			imlInstructionTemp->type = PPCREC_IML_TYPE_FPR_NAME_R;
			imlInstructionTemp->operation = PPCREC_IML_OP_ASSIGN;
			imlInstructionTemp->op_r_name.registerIndex = i;
			imlInstructionTemp->op_r_name.name = ppcImlGenContext->mappedFPRRegister[rCtx.currentMapping[i].virtualReg];
			idx++;
		}
	}
	return true;
}

bool PPCRecompiler_manageFPRRegisters(ppcImlGenContext_t* ppcImlGenContext)
{
	for (sint32 s = 0; s < ppcImlGenContext->segmentList2.size(); s++)
	{
		if (PPCRecompiler_manageFPRRegistersForSegment(ppcImlGenContext, s) == false)
			return false;
	}
	return true;
}


/*
 * Returns true if the loaded value is guaranteed to be overwritten
 */
bool PPCRecompiler_trackRedundantNameLoadInstruction(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 startIndex, IMLInstruction* nameStoreInstruction, sint32 scanDepth)
{
	sint16 registerIndex = nameStoreInstruction->op_r_name.registerIndex;
	for(size_t i=startIndex; i<imlSegment->imlList.size(); i++)
	{
		IMLInstruction* imlInstruction = imlSegment->imlList.data() + i;
		IMLUsedRegisters registersUsed;
		imlInstruction->CheckRegisterUsage(&registersUsed);
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
bool PPCRecompiler_trackRedundantFPRNameLoadInstruction(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 startIndex, IMLInstruction* nameStoreInstruction, sint32 scanDepth)
{
	sint16 registerIndex = nameStoreInstruction->op_r_name.registerIndex;
	for(size_t i=startIndex; i<imlSegment->imlList.size(); i++)
	{
		IMLInstruction* imlInstruction = imlSegment->imlList.data() + i;
		IMLUsedRegisters registersUsed;
		imlInstruction->CheckRegisterUsage(&registersUsed);
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
bool PPCRecompiler_trackRedundantNameStoreInstruction(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 startIndex, IMLInstruction* nameStoreInstruction, sint32 scanDepth)
{
	sint16 registerIndex = nameStoreInstruction->op_r_name.registerIndex;
	for(sint32 i=startIndex; i>=0; i--)
	{
		IMLInstruction* imlInstruction = imlSegment->imlList.data() + i;
		IMLUsedRegisters registersUsed;
		imlInstruction->CheckRegisterUsage(&registersUsed);
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
bool PPCRecompiler_trackOverwrittenNameStoreInstruction(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 startIndex, IMLInstruction* nameStoreInstruction, sint32 scanDepth)
{
	uint32 name = nameStoreInstruction->op_r_name.name;
	for(size_t i=startIndex; i<imlSegment->imlList.size(); i++)
	{
		const IMLInstruction& imlInstruction = imlSegment->imlList[i];
		if(imlInstruction.type == PPCREC_IML_TYPE_R_NAME )
		{
			// name is loaded before being written
			if (imlInstruction.op_r_name.name == name)
				return false;
		}
		else if(imlInstruction.type == PPCREC_IML_TYPE_NAME_R )
		{
			// name is written before being loaded
			if (imlInstruction.op_r_name.name == name)
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
	if( imlSegment->nextSegmentBranchTaken == nullptr && imlSegment->nextSegmentBranchNotTaken == nullptr)
		return false;

	return true;
}

/*
 * Returns true if the loaded FPR name is never changed
 */
bool PPCRecompiler_trackRedundantFPRNameStoreInstruction(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 startIndex, IMLInstruction* nameStoreInstruction, sint32 scanDepth)
{
	sint16 registerIndex = nameStoreInstruction->op_r_name.registerIndex;
	for(sint32 i=startIndex; i>=0; i--)
	{
		IMLInstruction* imlInstruction = imlSegment->imlList.data() + i;
		IMLUsedRegisters registersUsed;
		imlInstruction->CheckRegisterUsage(&registersUsed);
		if( registersUsed.writtenFPR1 == registerIndex )
		{
			if(imlInstruction->type == PPCREC_IML_TYPE_FPR_R_NAME )
				return true;
			return false;
		}
	}
	// todo: Scan next segment(s)
	return false;
}

uint32 _PPCRecompiler_getCROverwriteMask(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, uint32 currentOverwriteMask, uint32 currentReadMask, uint32 scanDepth)
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
		if (ppcImlGenContext->segmentList2.size() >= 5)
		{
			return 7; // for more complex functions we assume that CR is not passed on (hack)
		}
	}
	return currentOverwriteMask;
}

/*
 * Returns a mask of all CR bits that are overwritten (written but not read) in the segment and all it's following segments
 * If the write state of a CR bit cannot be determined, it is returned as 0 (not overwritten)
 */
uint32 PPCRecompiler_getCROverwriteMask(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment)
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
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		for(IMLInstruction& instIt : segIt->imlList)
		{
			if (instIt.type == PPCREC_IML_TYPE_CJUMP)
			{
				if (instIt.op_conditionalJump.condition != PPCREC_JUMP_CONDITION_NONE)
				{
					uint32 crBitFlag = 1 << (instIt.op_conditionalJump.crRegisterIndex * 4 + instIt.op_conditionalJump.crBitIndex);
					segIt->crBitsInput |= (crBitFlag&~segIt->crBitsWritten); // flag bits that have not already been written
					segIt->crBitsRead |= (crBitFlag);
				}
			}
			else if (instIt.type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
			{
				uint32 crBitFlag = 1 << (instIt.op_conditional_r_s32.crRegisterIndex * 4 + instIt.op_conditional_r_s32.crBitIndex);
				segIt->crBitsInput |= (crBitFlag&~segIt->crBitsWritten); // flag bits that have not already been written
				segIt->crBitsRead |= (crBitFlag);
			}
			else if (instIt.type == PPCREC_IML_TYPE_R_S32 && instIt.operation == PPCREC_IML_OP_MFCR)
			{
				segIt->crBitsRead |= 0xFFFFFFFF;
			}
			else if (instIt.type == PPCREC_IML_TYPE_R_S32 && instIt.operation == PPCREC_IML_OP_MTCRF)
			{
				segIt->crBitsWritten |= ppc_MTCRFMaskToCRBitMask((uint32)instIt.op_r_immS32.immS32);
			}
			else if( instIt.type == PPCREC_IML_TYPE_CR )
			{
				if (instIt.operation == PPCREC_IML_OP_CR_CLEAR ||
					instIt.operation == PPCREC_IML_OP_CR_SET)
				{
					uint32 crBitFlag = 1 << (instIt.op_cr.crD);
					segIt->crBitsWritten |= (crBitFlag & ~segIt->crBitsWritten);
				}
				else if (instIt.operation == PPCREC_IML_OP_CR_OR ||
					instIt.operation == PPCREC_IML_OP_CR_ORC ||
					instIt.operation == PPCREC_IML_OP_CR_AND ||
					instIt.operation == PPCREC_IML_OP_CR_ANDC)
				{
					uint32 crBitFlag = 1 << (instIt.op_cr.crD);
					segIt->crBitsWritten |= (crBitFlag & ~segIt->crBitsWritten);
					crBitFlag = 1 << (instIt.op_cr.crA);
					segIt->crBitsRead |= (crBitFlag & ~segIt->crBitsRead);
					crBitFlag = 1 << (instIt.op_cr.crB);
					segIt->crBitsRead |= (crBitFlag & ~segIt->crBitsRead);
				}
				else
					cemu_assert_unimplemented();
			}
			else if (IMLAnalyzer_CanTypeWriteCR(&instIt) && instIt.crRegister >= 0 && instIt.crRegister <= 7)
			{
				segIt->crBitsWritten |= (0xF<<(instIt.crRegister*4));
			}
			else if( (instIt.type == PPCREC_IML_TYPE_STORE || instIt.type == PPCREC_IML_TYPE_STORE_INDEXED) && instIt.op_storeLoad.copyWidth == PPC_REC_STORE_STWCX_MARKER )
			{
				// overwrites CR0
				segIt->crBitsWritten |= (0xF<<0);
			}
		}
	}
	// flag instructions that write to CR where we can ignore individual CR bits
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		for (IMLInstruction& instIt : segIt->imlList)
		{
			if (IMLAnalyzer_CanTypeWriteCR(&instIt) && instIt.crRegister >= 0 && instIt.crRegister <= 7)
			{
				uint32 crBitFlags = 0xF<<((uint32)instIt.crRegister*4);
				uint32 crOverwriteMask = PPCRecompiler_getCROverwriteMask(ppcImlGenContext, segIt);
				uint32 crIgnoreMask = crOverwriteMask & ~segIt->crBitsRead;
				instIt.crIgnoreMask = crIgnoreMask;
			}
		}
	}
}

bool PPCRecompiler_checkIfGPRIsModifiedInRange(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 startIndex, sint32 endIndex, sint32 vreg)
{
	IMLUsedRegisters registersUsed;
	for (sint32 i = startIndex; i <= endIndex; i++)
	{
		IMLInstruction* imlInstruction = imlSegment->imlList.data() + i;
		imlInstruction->CheckRegisterUsage(&registersUsed);
		if (registersUsed.writtenNamedReg1 == vreg)
			return true;
	}
	return false;
}

sint32 PPCRecompiler_scanBackwardsForReusableRegister(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* startSegment, sint32 startIndex, sint32 name)
{
	// current segment
	sint32 currentIndex = startIndex;
	IMLSegment* currentSegment = startSegment;
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
		currentIndex = currentSegment->imlList.size() - 1;
		segmentIterateCount++;
	}
	// scan again to make sure the register is not modified inbetween
	currentIndex = startIndex;
	currentSegment = startSegment;
	segmentIterateCount = 0;
	IMLUsedRegisters registersUsed;
	while (true)
	{
		while (currentIndex >= 0)
		{
			// check if register is modified
			currentSegment->imlList[currentIndex].CheckRegisterUsage(&registersUsed);
			if (registersUsed.writtenNamedReg1 == foundRegister)
				return -1;
			// check if end of scan reached
			if (currentSegment->imlList[currentIndex].type == PPCREC_IML_TYPE_NAME_R && currentSegment->imlList[currentIndex].op_r_name.name == name)
			{
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
		currentIndex = currentSegment->imlList.size() - 1;
		segmentIterateCount++;
	}
	return -1;
}

void PPCRecompiler_optimizeDirectFloatCopiesScanForward(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 imlIndexLoad, sint32 fprIndex)
{
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
			if (imlInstruction->op_storeLoad.registerData == fprIndex)
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
		IMLInstruction* newExpand = PPCRecompiler_insertInstruction(imlSegment, lastStore);
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

void PPCRecompiler_optimizeDirectIntegerCopiesScanForward(ppcImlGenContext_t* ppcImlGenContext, IMLSegment* imlSegment, sint32 imlIndexLoad, sint32 gprIndex)
{
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
			if (imlInstruction->op_storeLoad.registerMem == gprIndex)
				break;
			if (imlInstruction->op_storeLoad.registerData == gprIndex)
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
		IMLInstruction* newExpand = PPCRecompiler_insertInstruction(imlSegment, i);
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
				cemu_assert_debug(instIt.op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
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
				}
			}
			else if (instIt.type == PPCREC_IML_TYPE_FPR_STORE || instIt.type == PPCREC_IML_TYPE_FPR_STORE_INDEXED)
			{
				if(instIt.op_storeLoad.mode != PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0 &&
					instIt.op_storeLoad.mode != PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1)
					continue;
				// get GQR value
				cemu_assert_debug(instIt.op_storeLoad.registerGQR != PPC_REC_INVALID_REGISTER);
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
				}
			}
		}
	}
}

/*
 * Returns true if registerWrite overwrites any of the registers read by registerRead
 */
bool PPCRecompilerAnalyzer_checkForGPROverwrite(IMLUsedRegisters* registerRead, IMLUsedRegisters* registerWrite)
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

void _reorderConditionModifyInstructions(IMLSegment* imlSegment)
{
	IMLInstruction* lastInstruction = PPCRecompilerIML_getLastInstruction(imlSegment);
	// last instruction a conditional branch?
	if (lastInstruction == nullptr || lastInstruction->type != PPCREC_IML_TYPE_CJUMP)
		return;
	if (lastInstruction->op_conditionalJump.crRegisterIndex >= 8)
		return;
	// get CR bitmask of bit required for conditional jump
	PPCRecCRTracking_t crTracking;
	IMLAnalyzer_GetCRTracking(lastInstruction, &crTracking);
	uint32 requiredCRBits = crTracking.readCRBits;

	// scan backwards until we find the instruction that sets the CR
	sint32 crSetterInstructionIndex = -1;
	sint32 unsafeInstructionIndex = -1;
	for (sint32 i = imlSegment->imlList.size() - 2; i >= 0; i--)
	{
		IMLInstruction* imlInstruction = imlSegment->imlList.data() + i;
		IMLAnalyzer_GetCRTracking(imlInstruction, &crTracking);
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
	IMLUsedRegisters regTrackingCRSetter;
	imlSegment->imlList[crSetterInstructionIndex].CheckRegisterUsage(&regTrackingCRSetter);
	if (regTrackingCRSetter.writtenFPR1 >= 0 || regTrackingCRSetter.readFPR1 >= 0 || regTrackingCRSetter.readFPR2 >= 0 || regTrackingCRSetter.readFPR3 >= 0 || regTrackingCRSetter.readFPR4 >= 0)
		return; // we don't handle FPR dependency yet so just ignore FPR instructions
	IMLUsedRegisters registerTracking;
	if (regTrackingCRSetter.writtenNamedReg1 >= 0)
	{
		// CR setter does write GPR
		for (sint32 i = crSetterInstructionIndex + 1; i <= unsafeInstructionIndex; i++)
		{
			imlSegment->imlList[i].CheckRegisterUsage(&registerTracking);
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
			imlSegment->imlList[i].CheckRegisterUsage(&registerTracking);
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
	IMLInstruction* newCRSetterInstruction = PPCRecompiler_insertInstruction(imlSegment, unsafeInstructionIndex+1);
	memcpy(newCRSetterInstruction, imlSegment->imlList.data() + crSetterInstructionIndex, sizeof(IMLInstruction));
	PPCRecompilerImlGen_generateNewInstruction_noOp(nullptr, imlSegment->imlList.data() + crSetterInstructionIndex);
}

/*
 * Move instructions which update the condition flags closer to the instruction that consumes them
 * On x64 this improves performance since we often can avoid storing CR in memory
 */
void PPCRecompiler_reorderConditionModifyInstructions(ppcImlGenContext_t* ppcImlGenContext)
{
	// check if this segment has a conditional branch
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		_reorderConditionModifyInstructions(segIt);
	}
}
