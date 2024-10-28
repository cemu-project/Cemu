#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterHelper.h"
#include "Cafe/HW/Espresso/EspressoISA.h"
#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "IML/IML.h"
#include "IML/IMLRegisterAllocatorRanges.h"
#include "PPCFunctionBoundaryTracker.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"

bool PPCRecompiler_decodePPCInstruction(ppcImlGenContext_t* ppcImlGenContext);

struct PPCBasicBlockInfo
{
	PPCBasicBlockInfo(uint32 startAddress, const std::set<uint32>& entryAddresses) : startAddress(startAddress), lastAddress(startAddress)
	{
		isEnterable = entryAddresses.find(startAddress) != entryAddresses.end();
	}

	uint32 startAddress;
	uint32 lastAddress; // inclusive
	bool isEnterable{ false };
	bool hasContinuedFlow{ true }; // non-branch path goes to next segment, assumed by default
	bool hasBranchTarget{ false };
	uint32 branchTarget{};

	// associated IML segments
	IMLSegment* firstSegment{}; // first segment in chain, used as branch target for other segments
	IMLSegment* appendSegment{}; // last segment in chain, additional instructions should be appended to this segment

	void SetInitialSegment(IMLSegment* seg)
	{
		cemu_assert_debug(!firstSegment);
		cemu_assert_debug(!appendSegment);
		firstSegment = seg;
		appendSegment = seg;
	}

	IMLSegment* GetFirstSegmentInChain()
	{
		return firstSegment;
	}

	IMLSegment* GetSegmentForInstructionAppend()
	{
		return appendSegment;
	}
};

IMLInstruction* PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext_t* ppcImlGenContext)
{
	IMLInstruction& inst = ppcImlGenContext->currentOutputSegment->imlList.emplace_back();
	memset(&inst, 0x00, sizeof(IMLInstruction));
	return &inst;
}

void PPCRecompilerImlGen_generateNewInstruction_r_memory_indexed(ppcImlGenContext_t* ppcImlGenContext, IMLReg registerDestination, IMLReg registerMemory1, IMLReg registerMemory2, uint32 copyWidth, bool signExtend, bool switchEndian)
{
	cemu_assert_debug(registerMemory1.IsValid());
	cemu_assert_debug(registerMemory2.IsValid());
	cemu_assert_debug(registerDestination.IsValid());
	IMLInstruction* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_LOAD_INDEXED;
	imlInstruction->operation = 0;
	imlInstruction->op_storeLoad.registerData = registerDestination;
	imlInstruction->op_storeLoad.registerMem = registerMemory1;
	imlInstruction->op_storeLoad.registerMem2 = registerMemory2;
	imlInstruction->op_storeLoad.copyWidth = copyWidth;
	imlInstruction->op_storeLoad.flags2.swapEndian = switchEndian;
	imlInstruction->op_storeLoad.flags2.signExtend = signExtend;
}

void PPCRecompilerImlGen_generateNewInstruction_memory_r_indexed(ppcImlGenContext_t* ppcImlGenContext, IMLReg registerDestination, IMLReg registerMemory1, IMLReg registerMemory2, uint32 copyWidth, bool signExtend, bool switchEndian)
{
	cemu_assert_debug(registerMemory1.IsValid());
	cemu_assert_debug(registerMemory2.IsValid());
	cemu_assert_debug(registerDestination.IsValid());
	IMLInstruction* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_STORE_INDEXED;
	imlInstruction->operation = 0;
	imlInstruction->op_storeLoad.registerData = registerDestination;
	imlInstruction->op_storeLoad.registerMem = registerMemory1;
	imlInstruction->op_storeLoad.registerMem2 = registerMemory2;
	imlInstruction->op_storeLoad.copyWidth = copyWidth;
	imlInstruction->op_storeLoad.flags2.swapEndian = switchEndian;
	imlInstruction->op_storeLoad.flags2.signExtend = signExtend;
}

// create and fill two segments (branch taken and branch not taken) as a follow up to the current segment and then merge flow afterwards
template<typename F1n, typename F2n>
void PPCIMLGen_CreateSegmentBranchedPath(ppcImlGenContext_t& ppcImlGenContext, PPCBasicBlockInfo& basicBlockInfo, F1n genSegmentBranchTaken, F2n genSegmentBranchNotTaken)
{
	IMLSegment* currentWriteSegment = basicBlockInfo.GetSegmentForInstructionAppend();

	std::span<IMLSegment*> segments = ppcImlGenContext.InsertSegments(ppcImlGenContext.GetSegmentIndex(currentWriteSegment) + 1, 3);
	IMLSegment* segBranchNotTaken = segments[0];
	IMLSegment* segBranchTaken = segments[1];
	IMLSegment* segMerge = segments[2];

	// link the segments
	segMerge->SetLinkBranchTaken(currentWriteSegment->GetBranchTaken());
	segMerge->SetLinkBranchNotTaken(currentWriteSegment->GetBranchNotTaken());
	currentWriteSegment->SetLinkBranchTaken(segBranchTaken);
	currentWriteSegment->SetLinkBranchNotTaken(segBranchNotTaken);
	segBranchTaken->SetLinkBranchNotTaken(segMerge);
	segBranchNotTaken->SetLinkBranchTaken(segMerge);
	// generate code for branch taken segment
	ppcImlGenContext.currentOutputSegment = segBranchTaken;
	genSegmentBranchTaken(ppcImlGenContext);
	cemu_assert_debug(ppcImlGenContext.currentOutputSegment == segBranchTaken);
	// generate code for branch not taken segment
	ppcImlGenContext.currentOutputSegment = segBranchNotTaken;
	genSegmentBranchNotTaken(ppcImlGenContext);
	cemu_assert_debug(ppcImlGenContext.currentOutputSegment == segBranchNotTaken);
	ppcImlGenContext.emitInst().make_jump();
	// make merge segment the new write segment
	ppcImlGenContext.currentOutputSegment = segMerge;
	basicBlockInfo.appendSegment = segMerge;
}

IMLReg PPCRecompilerImlGen_LookupReg(ppcImlGenContext_t* ppcImlGenContext, IMLName mappedName, IMLRegFormat regFormat)
{
	auto it = ppcImlGenContext->mappedRegs.find(mappedName);
	if (it != ppcImlGenContext->mappedRegs.end())
		return it->second;
	// create new reg entry
	IMLRegFormat baseFormat;
	if (regFormat == IMLRegFormat::F64)
		baseFormat = IMLRegFormat::F64;
	else if (regFormat == IMLRegFormat::I32)
		baseFormat = IMLRegFormat::I64;
	else
	{
		cemu_assert_suspicious();
	}
	IMLRegID newRegId = ppcImlGenContext->mappedRegs.size();
	IMLReg newReg(baseFormat, regFormat, 0, newRegId);
	ppcImlGenContext->mappedRegs.try_emplace(mappedName, newReg);
	return newReg;
}

IMLName PPCRecompilerImlGen_GetRegName(ppcImlGenContext_t* ppcImlGenContext, IMLReg reg)
{
	for (auto& it : ppcImlGenContext->mappedRegs)
	{
		if (it.second.GetRegID() == reg.GetRegID())
			return it.first;
	}
	cemu_assert(false);
	return 0;
}

uint32 PPCRecompilerImlGen_getAndLockFreeTemporaryFPR(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName)
{
	DEBUG_BREAK;
	//if( mappedName == PPCREC_NAME_NONE )
	//{
	//	debug_printf("PPCRecompilerImlGen_getAndLockFreeTemporaryFPR(): Invalid mappedName parameter\n");
	//	return PPC_REC_INVALID_REGISTER;
	//}
	//for(uint32 i=0; i<255; i++)
	//{
	//	if( ppcImlGenContext->mappedFPRRegister[i] == PPCREC_NAME_NONE )
	//	{
	//		ppcImlGenContext->mappedFPRRegister[i] = mappedName;
	//		return i;
	//	}
	//}
	return 0;
}

uint32 PPCRecompilerImlGen_findFPRRegisterByMappedName(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName)
{
	DEBUG_BREAK;
	//for(uint32 i=0; i<255; i++)
	//{
	//	if( ppcImlGenContext->mappedFPRRegister[i] == mappedName )
	//	{
	//		return i;
	//	}
	//}
	return PPC_REC_INVALID_REGISTER;
}

IMLReg PPCRecompilerImlGen_loadRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName)
{
	return PPCRecompilerImlGen_LookupReg(ppcImlGenContext, mappedName, IMLRegFormat::I32);
}

IMLReg _GetRegGPR(ppcImlGenContext_t* ppcImlGenContext, uint32 index)
{
	cemu_assert_debug(index < 32);
	return PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + index);
}

IMLReg _GetRegCR(ppcImlGenContext_t* ppcImlGenContext, uint32 index)
{
	cemu_assert_debug(index < 32);
	return PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_CR + index);
}

IMLReg _GetRegCR(ppcImlGenContext_t* ppcImlGenContext, uint8 crReg, uint8 crBit)
{
	cemu_assert_debug(crReg < 8);
	cemu_assert_debug(crBit < 4);
	return _GetRegCR(ppcImlGenContext, (crReg * 4) + crBit);
}

IMLReg _GetRegTemporary(ppcImlGenContext_t* ppcImlGenContext, uint32 index)
{
	cemu_assert_debug(index < 4);
	return PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + index);
}

// get throw-away register. Only valid for the scope of a single translated instruction
// be careful to not collide with manually loaded temporary register
IMLReg _GetRegTemporaryS8(ppcImlGenContext_t* ppcImlGenContext, uint32 index)
{
	cemu_assert_debug(index < 4);
	return PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + index);
}

/*
 * Loads a PPC fpr into any of the available IML FPU registers
 * If loadNew is false, it will check first if the fpr is already loaded into any IML register
 */
IMLReg PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName, bool loadNew)
{
	return PPCRecompilerImlGen_LookupReg(ppcImlGenContext, mappedName, IMLRegFormat::F64);
}

/*
 * Checks if a PPC fpr register is already loaded into any IML register
 * If not, it will create a new undefined temporary IML FPU register and map the name (effectively overwriting the old ppc register)
 */
IMLReg PPCRecompilerImlGen_loadOverwriteFPRRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName)
{
	return PPCRecompilerImlGen_LookupReg(ppcImlGenContext, mappedName, IMLRegFormat::F64);
}

bool PPCRecompiler_canInlineFunction(MPTR functionPtr, sint32* functionInstructionCount)
{
	for (sint32 i = 0; i < 6; i++)
	{
		uint32 opcode = memory_readU32(functionPtr + i * 4);
		switch ((opcode >> 26))
		{
		case 14: // ADDI
		case 15: // ADDIS
			continue;
		case 19: // opcode category 19
			switch (PPC_getBits(opcode, 30, 10))
			{
			case 16:
				if (opcode == 0x4E800020)
				{
					*functionInstructionCount = i;
					return true; // BLR
				}
				return false;
			}
			return false;
		case 32: // LWZ
		case 33: // LWZU
		case 34: // LBZ
		case 35: // LBZU
		case 36: // STW
		case 37: // STWU
		case 38: // STB
		case 39: // STBU
		case 40: // LHZ
		case 41: // LHZU
		case 42: // LHA
		case 43: // LHAU
		case 44: // STH
		case 45: // STHU
		case 46: // LMW
		case 47: // STMW
		case 48: // LFS
		case 49: // LFSU
		case 50: // LFD
		case 51: // LFDU
		case 52: // STFS
		case 53: // STFSU
		case 54: // STFD
		case 55: // STFDU
			continue;
		default:
			return false;
		}
	}
	return false;
}

void PPCRecompiler_generateInlinedCode(ppcImlGenContext_t* ppcImlGenContext, uint32 startAddress, sint32 instructionCount)
{
	for (sint32 i = 0; i < instructionCount; i++)
	{
		ppcImlGenContext->ppcAddressOfCurrentInstruction = startAddress + i * 4;
		ppcImlGenContext->cyclesSinceLastBranch++;
		if (PPCRecompiler_decodePPCInstruction(ppcImlGenContext))
		{
			cemu_assert_suspicious();
		}
	}
	// add range
	cemu_assert_unimplemented();
	//ppcRecRange_t recRange;
	//recRange.ppcAddress = startAddress;
	//recRange.ppcSize = instructionCount*4 + 4; // + 4 because we have to include the BLR
	//ppcImlGenContext->functionRef->list_ranges.push_back(recRange);
}

// for handling RC bit of many instructions
void PPCImlGen_UpdateCR0(ppcImlGenContext_t* ppcImlGenContext, IMLReg regR)
{
	IMLReg crBitRegLT = _GetRegCR(ppcImlGenContext, 0, Espresso::CR_BIT::CR_BIT_INDEX_LT);
	IMLReg crBitRegGT = _GetRegCR(ppcImlGenContext, 0, Espresso::CR_BIT::CR_BIT_INDEX_GT);
	IMLReg crBitRegEQ = _GetRegCR(ppcImlGenContext, 0, Espresso::CR_BIT::CR_BIT_INDEX_EQ);
	// todo - SO bit

	ppcImlGenContext->emitInst().make_compare_s32(regR, 0, crBitRegLT, IMLCondition::SIGNED_LT);
	ppcImlGenContext->emitInst().make_compare_s32(regR, 0, crBitRegGT, IMLCondition::SIGNED_GT);
	ppcImlGenContext->emitInst().make_compare_s32(regR, 0, crBitRegEQ, IMLCondition::EQ);

	//ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, crBitRegSO, 0); // todo - copy from XER

	//ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, registerR, registerR, 0, PPCREC_CR_MODE_LOGICAL);
}

void PPCRecompilerImlGen_TW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// split before and after to make sure the macro is in an isolated segment that we can make enterable
	PPCIMLGen_CreateSplitSegmentAtEnd(*ppcImlGenContext, *ppcImlGenContext->currentBasicBlock);
	ppcImlGenContext->currentOutputSegment->SetEnterable(ppcImlGenContext->ppcAddressOfCurrentInstruction);
	PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext)->make_macro(PPCREC_IML_MACRO_LEAVE, ppcImlGenContext->ppcAddressOfCurrentInstruction, 0, 0, IMLREG_INVALID);
	IMLSegment* middleSeg = PPCIMLGen_CreateSplitSegmentAtEnd(*ppcImlGenContext, *ppcImlGenContext->currentBasicBlock);
	middleSeg->SetLinkBranchTaken(nullptr);
	middleSeg->SetLinkBranchNotTaken(nullptr);
}

bool PPCRecompilerImlGen_MTSPR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 rD, spr1, spr2, spr;
	PPC_OPC_TEMPL_XO(opcode, rD, spr1, spr2);
	spr = spr1 | (spr2<<5);
	IMLReg gprReg = _GetRegGPR(ppcImlGenContext, rD);
	if (spr == SPR_CTR || spr == SPR_LR)
	{
		IMLReg sprReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + spr);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, sprReg, gprReg);
	}
	else if (spr >= SPR_UGQR0 && spr <= SPR_UGQR7)
	{
		IMLReg sprReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + spr);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, sprReg, gprReg);
		ppcImlGenContext->tracking.modifiesGQR[spr - SPR_UGQR0] = true;
	}
	else
		return false;
	return true;
}

bool PPCRecompilerImlGen_MFSPR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 rD, spr1, spr2, spr;
	PPC_OPC_TEMPL_XO(opcode, rD, spr1, spr2);
	spr = spr1 | (spr2<<5);
	IMLReg gprReg = _GetRegGPR(ppcImlGenContext, rD);
	if (spr == SPR_LR || spr == SPR_CTR)
	{
		IMLReg sprReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + spr);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprReg, sprReg);
	}
	else if (spr >= SPR_UGQR0 && spr <= SPR_UGQR7)
	{
		IMLReg sprReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + spr);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprReg, sprReg);
	}
	else
		return false;
	return true;
}

ATTR_MS_ABI uint32 PPCRecompiler_GetTBL()
{
	return (uint32)coreinit::coreinit_getTimerTick();
}

ATTR_MS_ABI uint32 PPCRecompiler_GetTBU()
{
	return (uint32)(coreinit::coreinit_getTimerTick() >> 32);
}

bool PPCRecompilerImlGen_MFTB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 rD, spr1, spr2, spr;
	PPC_OPC_TEMPL_XO(opcode, rD, spr1, spr2);
	spr = spr1 | (spr2<<5);

	if( spr == SPR_TBL || spr == SPR_TBU )
	{
		IMLReg resultReg = _GetRegGPR(ppcImlGenContext, rD);
		ppcImlGenContext->emitInst().make_call_imm(spr == SPR_TBL ? (uintptr_t)PPCRecompiler_GetTBL : (uintptr_t)PPCRecompiler_GetTBU, IMLREG_INVALID, IMLREG_INVALID, IMLREG_INVALID, resultReg);
		return true;
	}
	return false;
}

void PPCRecompilerImlGen_MCRF(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 crD, crS, b;
	PPC_OPC_TEMPL_X(opcode, crD, crS, b);
	cemu_assert_debug((crD&3) == 0);
	cemu_assert_debug((crS&3) == 0);
	crD >>= 2;
	crS >>= 2;
	for (sint32 i = 0; i<4; i++)
	{
		IMLReg regCrSrcBit = _GetRegCR(ppcImlGenContext, crS * 4 + i);
		IMLReg regCrDstBit = _GetRegCR(ppcImlGenContext, crD * 4 + i);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regCrDstBit, regCrSrcBit);
	}
}

bool PPCRecompilerImlGen_MFCR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regD, 0);
	for (sint32 i = 0; i < 32; i++)
	{
		IMLReg regCrBit = _GetRegCR(ppcImlGenContext, i);
		cemu_assert_debug(regCrBit.GetRegFormat() == IMLRegFormat::I32); // addition is only allowed between same-format regs
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_LEFT_SHIFT, regD, regD, 1);
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, regD, regD, regCrBit);
	}
	return true;
}

bool PPCRecompilerImlGen_MTCRF(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 rS;
	uint32 crMask;
	PPC_OPC_TEMPL_XFX(opcode, rS, crMask);
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regTmp = _GetRegTemporary(ppcImlGenContext, 0);
	uint32 crBitMask = ppc_MTCRFMaskToCRBitMask(crMask);
	for (sint32 f = 0; f < 32; f++)
	{
		if(((crBitMask >> f) & 1) == 0)
			continue;
		IMLReg regCrBit = _GetRegCR(ppcImlGenContext, f);
		cemu_assert_debug(regCrBit.GetRegFormat() == IMLRegFormat::I32);
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_RIGHT_SHIFT_U, regTmp, regS, (31-f));
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, regCrBit, regTmp, 1);
	}
	return true;
}

void PPCRecompilerImlGen_CMP(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool isUnsigned)
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(opcode, cr, rA, rB);
	cr >>= 2;

	IMLReg gprRegisterA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg gprRegisterB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regXerSO = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_SO);

	IMLReg crBitRegLT = _GetRegCR(ppcImlGenContext, cr, Espresso::CR_BIT::CR_BIT_INDEX_LT);
	IMLReg crBitRegGT = _GetRegCR(ppcImlGenContext, cr, Espresso::CR_BIT::CR_BIT_INDEX_GT);
	IMLReg crBitRegEQ = _GetRegCR(ppcImlGenContext, cr, Espresso::CR_BIT::CR_BIT_INDEX_EQ);
	IMLReg crBitRegSO = _GetRegCR(ppcImlGenContext, cr, Espresso::CR_BIT::CR_BIT_INDEX_SO);

	ppcImlGenContext->emitInst().make_compare(gprRegisterA, gprRegisterB, crBitRegLT, isUnsigned ? IMLCondition::UNSIGNED_LT : IMLCondition::SIGNED_LT);
	ppcImlGenContext->emitInst().make_compare(gprRegisterA, gprRegisterB, crBitRegGT, isUnsigned ? IMLCondition::UNSIGNED_GT : IMLCondition::SIGNED_GT);
	ppcImlGenContext->emitInst().make_compare(gprRegisterA, gprRegisterB, crBitRegEQ, IMLCondition::EQ);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, crBitRegSO, regXerSO);
}

bool PPCRecompilerImlGen_CMPI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool isUnsigned)
{
	uint32 cr;
	int rA;
	uint32 imm;
	if (isUnsigned)
	{
		PPC_OPC_TEMPL_D_UImm(opcode, cr, rA, imm);
	}
	else
	{
		PPC_OPC_TEMPL_D_SImm(opcode, cr, rA, imm);
	}
	cr >>= 2;

	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regXerSO = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_SO);

	IMLReg crBitRegLT = _GetRegCR(ppcImlGenContext, cr, Espresso::CR_BIT::CR_BIT_INDEX_LT);
	IMLReg crBitRegGT = _GetRegCR(ppcImlGenContext, cr, Espresso::CR_BIT::CR_BIT_INDEX_GT);
	IMLReg crBitRegEQ = _GetRegCR(ppcImlGenContext, cr, Espresso::CR_BIT::CR_BIT_INDEX_EQ);
	IMLReg crBitRegSO = _GetRegCR(ppcImlGenContext, cr, Espresso::CR_BIT::CR_BIT_INDEX_SO);

	ppcImlGenContext->emitInst().make_compare_s32(regA, (sint32)imm, crBitRegLT, isUnsigned ? IMLCondition::UNSIGNED_LT : IMLCondition::SIGNED_LT);
	ppcImlGenContext->emitInst().make_compare_s32(regA, (sint32)imm, crBitRegGT, isUnsigned ? IMLCondition::UNSIGNED_GT : IMLCondition::SIGNED_GT);
	ppcImlGenContext->emitInst().make_compare_s32(regA, (sint32)imm, crBitRegEQ, IMLCondition::EQ);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, crBitRegSO, regXerSO);

	return true;
}

bool PPCRecompilerImlGen_B(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 li;
	PPC_OPC_TEMPL_I(opcode, li);
	uint32 jumpAddressDest = li;
	if( (opcode&PPC_OPC_AA) == 0 )
	{
		jumpAddressDest = li + (unsigned int)ppcImlGenContext->ppcAddressOfCurrentInstruction;
	}
	if( opcode&PPC_OPC_LK )
	{
		// function call
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_BL, ppcImlGenContext->ppcAddressOfCurrentInstruction, jumpAddressDest, ppcImlGenContext->cyclesSinceLastBranch, IMLREG_INVALID);
		return true;
	}
	// is jump destination within recompiled function?
	if (ppcImlGenContext->boundaryTracker->ContainsAddress(jumpAddressDest))
		ppcImlGenContext->emitInst().make_jump();
	else
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_B_FAR, ppcImlGenContext->ppcAddressOfCurrentInstruction, jumpAddressDest, ppcImlGenContext->cyclesSinceLastBranch, IMLREG_INVALID);
	return true;
}

bool PPCRecompilerImlGen_BC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	PPCIMLGen_AssertIfNotLastSegmentInstruction(*ppcImlGenContext);

	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_B(opcode, BO, BI, BD);

	Espresso::BOField boField(BO);

	uint32 crRegister = BI/4;
	uint32 crBit = BI%4;
	uint32 jumpCondition = 0;
	bool conditionMustBeTrue = (BO&8)!=0;
	bool useDecrementer = (BO&4)==0; // bit not set -> decrement
	bool decrementerMustBeZero = (BO&2)!=0; // bit set -> branch if CTR = 0, bit not set -> branch if CTR != 0
	bool ignoreCondition = (BO&16)!=0;

	IMLReg regCRBit;
	if (!ignoreCondition)
		regCRBit = _GetRegCR(ppcImlGenContext, crRegister, crBit);

	uint32 jumpAddressDest = BD;
	if( (opcode&PPC_OPC_AA) == 0 )
	{
		jumpAddressDest = BD + (unsigned int)ppcImlGenContext->ppcAddressOfCurrentInstruction;
	}

	if( opcode&PPC_OPC_LK )
	{
		if (useDecrementer)
			return false;
		// conditional function calls are not supported
		if( ignoreCondition == false )
		{
			PPCBasicBlockInfo* currentBasicBlock = ppcImlGenContext->currentBasicBlock;
			IMLSegment* blSeg = PPCIMLGen_CreateNewSegmentAsBranchTarget(*ppcImlGenContext, *currentBasicBlock);
			ppcImlGenContext->emitInst().make_conditional_jump(regCRBit, conditionMustBeTrue);
			blSeg->AppendInstruction()->make_macro(PPCREC_IML_MACRO_BL, ppcImlGenContext->ppcAddressOfCurrentInstruction, jumpAddressDest, ppcImlGenContext->cyclesSinceLastBranch, IMLREG_INVALID);
			return true;
		}
		return false;
	}
	// generate iml instructions depending on flags
	if( useDecrementer )
	{
		if( ignoreCondition == false )
			return false; // not supported for the moment
		IMLReg ctrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0+SPR_CTR);
		IMLReg tmpBoolReg = _GetRegTemporaryS8(ppcImlGenContext, 1);
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_SUB, ctrRegister, ctrRegister, 1);
		ppcImlGenContext->emitInst().make_compare_s32(ctrRegister, 0, tmpBoolReg, decrementerMustBeZero ? IMLCondition::EQ : IMLCondition::NEQ);
		ppcImlGenContext->emitInst().make_conditional_jump(tmpBoolReg, true);
		return true;
	}
	else
	{
		if( ignoreCondition )
		{
			// branch always, no condition and no decrementer
			// not supported
			return false;
		}
		else
		{
			if (ppcImlGenContext->boundaryTracker->ContainsAddress(jumpAddressDest))
			{
				// near jump
				ppcImlGenContext->emitInst().make_conditional_jump(regCRBit, conditionMustBeTrue);
			}
			else
			{
				// far jump
				debug_printf("PPCRecompilerImlGen_BC(): Far jump not supported yet");
				return false;
			}
		}
	}
	return true;
}

// BCCTR or BCLR
bool PPCRecompilerImlGen_BCSPR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, uint32 sprReg)
{
	PPCIMLGen_AssertIfNotLastSegmentInstruction(*ppcImlGenContext);

	Espresso::BOField BO;
	uint32 BI;
	bool LK;
	Espresso::decodeOp_BCSPR(opcode, BO, BI, LK);
	uint32 crRegister = BI/4;
	uint32 crBit = BI%4;

	IMLReg regCRBit;
	if (!BO.conditionIgnore())
		regCRBit = _GetRegCR(ppcImlGenContext, crRegister, crBit);

	IMLReg branchDestReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + sprReg);
	if (LK)
	{
		if (sprReg == SPR_LR)
		{
			// if the branch target is LR, then preserve it in a temporary
			cemu_assert_suspicious(); // this case needs testing
			IMLReg tmpRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY);
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, tmpRegister, branchDestReg);
			branchDestReg = tmpRegister;
		}
		IMLReg registerLR = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_LR);
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, registerLR, ppcImlGenContext->ppcAddressOfCurrentInstruction + 4);
	}

	if (!BO.decrementerIgnore())
	{
		cemu_assert_unimplemented();
		return false;
	}
	else if (!BO.conditionIgnore())
	{
		// no decrementer but CR check
		cemu_assert_debug(ppcImlGenContext->currentBasicBlock->hasContinuedFlow);
		cemu_assert_debug(!ppcImlGenContext->currentBasicBlock->hasBranchTarget);
		PPCBasicBlockInfo* currentBasicBlock = ppcImlGenContext->currentBasicBlock;
		IMLSegment* bctrSeg = PPCIMLGen_CreateNewSegmentAsBranchTarget(*ppcImlGenContext, *currentBasicBlock);
		ppcImlGenContext->emitInst().make_conditional_jump(regCRBit, !BO.conditionInverted());
		bctrSeg->AppendInstruction()->make_macro(PPCREC_IML_MACRO_B_TO_REG, 0, 0, 0, branchDestReg);
	}
	else
	{
		// branch always, no condition and no decrementer check
		cemu_assert_debug(!ppcImlGenContext->currentBasicBlock->hasContinuedFlow);
		cemu_assert_debug(!ppcImlGenContext->currentBasicBlock->hasBranchTarget);
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_B_TO_REG, 0, 0, 0, branchDestReg);
	}
	return true;
}

bool PPCRecompilerImlGen_ISYNC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	return true;
}

bool PPCRecompilerImlGen_SYNC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	return true;
}

bool PPCRecompilerImlGen_ADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, regD, regA, regB);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regD);
	return true;
}

bool PPCRecompilerImlGen_ADDI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	if (rA != 0)
	{
		IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, regD, regA, imm);
	}
	else
	{
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regD, imm);
	}
	return true;
}

bool PPCRecompilerImlGen_ADDIS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(opcode, rD, rA, imm);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	if (rA != 0)
	{
		IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, regD, regA, (sint32)imm);
	}
	else
	{
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regD, (sint32)imm);
	}
	return true;
}

bool PPCRecompilerImlGen_ADDC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// r = a + b -> update carry
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regRA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regRB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regRD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regCa = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_CA);
	ppcImlGenContext->emitInst().make_r_r_r_carry(PPCREC_IML_OP_ADD, regRD, regRA, regRB, regCa);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regRD);
	return true;
}

bool PPCRecompilerImlGen_ADDIC_(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool updateCR0)
{
	sint32 rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regCa = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_CA);
	ppcImlGenContext->emitInst().make_r_r_s32_carry(PPCREC_IML_OP_ADD, regD, regA, (sint32)imm, regCa);
	if(updateCR0)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regD);
	return true;
}

bool PPCRecompilerImlGen_ADDE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// r = a + b + carry -> update carry
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regRA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regRB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regRD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regCa = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_CA);
	ppcImlGenContext->emitInst().make_r_r_r_carry(PPCREC_IML_OP_ADD_WITH_CARRY, regRD, regRA, regRB, regCa);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regRD);
	return true;
}

bool PPCRecompilerImlGen_ADDZE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// r = a + carry -> update carry
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regRA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regRD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regCa = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_CA);
	ppcImlGenContext->emitInst().make_r_r_s32_carry(PPCREC_IML_OP_ADD_WITH_CARRY, regRD, regRA, 0, regCa);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regRD);
	return true;
}

bool PPCRecompilerImlGen_ADDME(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// r = a + 0xFFFFFFFF + carry -> update carry
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regRA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regRD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regCa = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_CA);
	ppcImlGenContext->emitInst().make_r_r_s32_carry(PPCREC_IML_OP_ADD_WITH_CARRY, regRD, regRA, -1, regCa);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regRD);
	return true;
}

bool PPCRecompilerImlGen_SUBF(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	// rD = ~rA + rB + 1
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_SUB, regD, regB, regA);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0(ppcImlGenContext, regD);
	return true;
}

bool PPCRecompilerImlGen_SUBFE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// d = ~a + b + ca;
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
	IMLReg regCa = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_CA);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, regTmp, regA);
	ppcImlGenContext->emitInst().make_r_r_r_carry(PPCREC_IML_OP_ADD_WITH_CARRY, regD, regTmp, regB, regCa);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regD);
	return true;
}

bool PPCRecompilerImlGen_SUBFZE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// d = ~a + ca;
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
	IMLReg regCa = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_CA);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, regTmp, regA);
	ppcImlGenContext->emitInst().make_r_r_s32_carry(PPCREC_IML_OP_ADD_WITH_CARRY, regD, regTmp, 0, regCa);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regD);
	return true;
}

bool PPCRecompilerImlGen_SUBFC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// d = ~a + b + 1;
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
	IMLReg regCa = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_CA);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, regTmp, regA);
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regCa, 1); // set input carry to simulate offset of 1
	ppcImlGenContext->emitInst().make_r_r_r_carry(PPCREC_IML_OP_ADD_WITH_CARRY, regD, regTmp, regB, regCa);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0(ppcImlGenContext, regD);
	return true;
}

bool PPCRecompilerImlGen_SUBFIC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// d = ~a + imm + 1
	sint32 rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regCa = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_CA);
	IMLReg regTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, regTmp, regA);
	ppcImlGenContext->emitInst().make_r_r_s32_carry(PPCREC_IML_OP_ADD, regD, regTmp, (sint32)imm + 1, regCa);
	return true;
}

bool PPCRecompilerImlGen_MULLI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_MULTIPLY_SIGNED, regD, regA, (sint32)imm);
	return true;
}

bool PPCRecompilerImlGen_MULLW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	if (opcode & PPC_OPC_OE)
	{
		return false;
	}
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_MULTIPLY_SIGNED, regD, regA, regB);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regD);
	return true;
}

bool PPCRecompilerImlGen_MULHW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED, regD, regA, regB);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regD);
	return true;
}

bool PPCRecompilerImlGen_MULHWU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_MULTIPLY_HIGH_UNSIGNED, regD, regA, regB);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regD);
	return true;
}

bool PPCRecompilerImlGen_DIVW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regR = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_DIVIDE_SIGNED, regR, regA, regB);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regR);
	return true;
}

bool PPCRecompilerImlGen_DIVWU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_DIVIDE_UNSIGNED, regD, regA, regB);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regD);
	return true;
}

bool PPCRecompilerImlGen_RLWINM(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	uint32 mask = ppc_mask(MB, ME);

	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	if( ME == (31-SH) && MB == 0 )
	{
		// SLWI
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_LEFT_SHIFT, regA, regS, SH);
	}
	else if( SH == (32-MB) && ME == 31 )
	{
		// SRWI
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_RIGHT_SHIFT_U, regA, regS, MB);
	}
	else
	{
		// general handler
		if (rA != rS)
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regA, regS);
		if (SH != 0)
			ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_LEFT_ROTATE, regA, SH);
		if (mask != 0xFFFFFFFF)
			ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, regA, regA, (sint32)mask);
	}
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_RLWIMI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regR = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regTmp = _GetRegTemporary(ppcImlGenContext, 0);
	uint32 mask = ppc_mask(MB, ME);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regTmp, regS);
	if (SH)
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_LEFT_ROTATE, regTmp, SH);
	if (mask != 0)
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, regR, regR, (sint32)~mask);
	if (mask != 0xFFFFFFFF)
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, regTmp, regTmp, (sint32)mask);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_OR, regR, regR, regTmp);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regR);
	return true;
}

bool PPCRecompilerImlGen_RLWNM(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, rB, MB, ME);
	uint32 mask = ppc_mask(MB, ME);
	IMLReg regS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	IMLReg regB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
	IMLReg regA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_LEFT_ROTATE, regA, regS, regB);
	if( mask != 0xFFFFFFFF )
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, regA, regA, (sint32)mask);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_SRAW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// unlike SRAWI, for SRAW the shift range is 0-63 (masked to 6 bits)
	// but only shifts up to register bitwidth minus one are well defined in IML so this requires special handling for shifts >= 32
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	IMLReg regS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	IMLReg regB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
	IMLReg regA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	IMLReg regCarry = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_CA);

	IMLReg regTmpShiftAmount = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
	IMLReg regTmpCondBool = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 1);
	IMLReg regTmp1 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 2);
	IMLReg regTmp2 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 3);

	// load masked shift factor into temporary register
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, regTmpShiftAmount, regB, 0x3F);
	ppcImlGenContext->emitInst().make_compare_s32(regTmpShiftAmount, 32, regTmpCondBool, IMLCondition::UNSIGNED_GT);
	ppcImlGenContext->emitInst().make_conditional_jump(regTmpCondBool, true);

	PPCIMLGen_CreateSegmentBranchedPath(*ppcImlGenContext, *ppcImlGenContext->currentBasicBlock,
		[&](ppcImlGenContext_t& genCtx)
		{
			/* branch taken */
			genCtx.emitInst().make_r_r_r(PPCREC_IML_OP_RIGHT_SHIFT_S, regA, regS, regTmpShiftAmount);
			genCtx.emitInst().make_compare_s32(regA, 0, regCarry, IMLCondition::NEQ); // if the sign bit is still set it also means it was shifted out and we can set carry
		},
		[&](ppcImlGenContext_t& genCtx) 
		{
			/* branch not taken, shift size below 32 */
			genCtx.emitInst().make_r_r_s32(PPCREC_IML_OP_RIGHT_SHIFT_S, regTmp1, regS, 31); // signMask = input >> 31 (arithmetic shift)
			genCtx.emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regTmp2, 1); // shiftMask = ((1<<SH)-1)
			genCtx.emitInst().make_r_r_r(PPCREC_IML_OP_LEFT_SHIFT, regTmp2, regTmp2, regTmpShiftAmount);
			genCtx.emitInst().make_r_r_s32(PPCREC_IML_OP_SUB, regTmp2, regTmp2, 1);
			genCtx.emitInst().make_r_r_r(PPCREC_IML_OP_AND, regTmp1, regTmp1, regTmp2); // signMask & shiftMask & input
			genCtx.emitInst().make_r_r_r(PPCREC_IML_OP_AND, regTmp1, regTmp1, regS);
			genCtx.emitInst().make_compare_s32(regTmp1, 0, regCarry, IMLCondition::NEQ);
			genCtx.emitInst().make_r_r_r(PPCREC_IML_OP_RIGHT_SHIFT_S, regA, regS, regTmpShiftAmount);
		}
	);
	return true;
}

bool PPCRecompilerImlGen_SRAWI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA;
	uint32 SH;
	PPC_OPC_TEMPL_X(opcode, rS, rA, SH);
	cemu_assert_debug(SH < 32);
	if (SH == 0)
		return false; // becomes a no-op (unless RC bit is set) but also sets ca bit to 0?
	IMLReg regS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rS);
	IMLReg regA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA);
	IMLReg regCarry = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_XER_CA);
	IMLReg regTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
	// calculate CA first
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_RIGHT_SHIFT_S, regTmp, regS, 31); // signMask = input >> 31 (arithmetic shift)
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_AND, regTmp, regTmp, regS); // testValue = input & signMask & ((1<<SH)-1)
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, regTmp, regTmp, ((1 << SH) - 1));
	ppcImlGenContext->emitInst().make_compare_s32(regTmp, 0, regCarry, IMLCondition::NEQ); // ca = (testValue != 0)
	// do the actual shift
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_RIGHT_SHIFT_S, regA, regS, (sint32)SH);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_SLW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);

	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_SLW, regA, regS, regB);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_SRW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_SRW, regA, regS, regB);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_EXTSH(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN_S16_TO_S32, regA, regS);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_EXTSB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN_S8_TO_S32, regA, regS);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_CNTLZW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_CNTLZW, regA, regS);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_NEG(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NEG, regD, regA);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regD);
	return true;
}

bool PPCRecompilerImlGen_LOAD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, uint32 bitWidth, bool signExtend, bool isBigEndian, bool updateAddrReg)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	IMLReg regMemAddr;
	if (rA == 0)
	{
		if (updateAddrReg)
			return false; // invalid instruction form
		regMemAddr = _GetRegTemporary(ppcImlGenContext, 0);
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regMemAddr, 0);
	}
	else
	{
		if (updateAddrReg && rA == rD)
			return false; // invalid instruction form
		regMemAddr = _GetRegGPR(ppcImlGenContext, rA);
	}
	if (updateAddrReg)
	{
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, regMemAddr, regMemAddr, (sint32)imm);
		imm = 0;
	}
	IMLReg regDst = _GetRegGPR(ppcImlGenContext, rD);
	ppcImlGenContext->emitInst().make_r_memory(regDst, regMemAddr, (sint32)imm, bitWidth, signExtend, isBigEndian);
	return true;
}

void PPCRecompilerImlGen_LOAD_INDEXED(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, uint32 bitWidth, bool signExtend, bool isBigEndian, bool updateAddrReg)
{
	// if rA == rD, then the EA wont be stored to rA. We could set updateAddrReg to false in such cases but the end result is the same since the loaded value would overwrite rA
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	updateAddrReg = updateAddrReg && (rA != 0);
	IMLReg regA = rA != 0 ? _GetRegGPR(ppcImlGenContext, rA) : IMLREG_INVALID;
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regDst = _GetRegGPR(ppcImlGenContext, rD);
	if (updateAddrReg)
	{
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, regA, regA, regB);
		// use single register addressing
		regB = regA;
		regA = IMLREG_INVALID;
	}
	if(regA.IsValid())
		PPCRecompilerImlGen_generateNewInstruction_r_memory_indexed(ppcImlGenContext, regDst, regA, regB, bitWidth, signExtend, isBigEndian);
	else
		ppcImlGenContext->emitInst().make_r_memory(regDst, regB, 0, bitWidth, signExtend, isBigEndian);
}

bool PPCRecompilerImlGen_STORE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, uint32 bitWidth, bool isBigEndian, bool updateAddrReg)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	IMLReg regA;
	if (rA != 0)
	{
		regA = _GetRegGPR(ppcImlGenContext, rA);
	}
	else
	{
		if (updateAddrReg)
			return false; // invalid instruction form
		regA = _GetRegTemporary(ppcImlGenContext, 0);
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regA, 0);
	}
	IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
	if (updateAddrReg)
	{
		if (rD == rA)
		{
			// make sure to keep source data intact
			regD = _GetRegTemporary(ppcImlGenContext, 0);
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regD, regA);
		}
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, regA, regA, (sint32)imm);
		imm = 0;
	}
	ppcImlGenContext->emitInst().make_memory_r(regD, regA, (sint32)imm, bitWidth, isBigEndian);
	return true;
}

bool PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, uint32 bitWidth, bool isBigEndian, bool updateAddrReg)
{
	sint32 rA, rS, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	IMLReg regA = rA != 0 ? _GetRegGPR(ppcImlGenContext, rA) : IMLREG_INVALID;
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regSrc = _GetRegGPR(ppcImlGenContext, rS);
	if (updateAddrReg)
	{
		if(rA == 0)
			return false; // invalid instruction form
		if (regSrc == regA)
		{
			// make sure to keep source data intact
			regSrc = _GetRegTemporary(ppcImlGenContext, 0);
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regSrc, regA);
		}
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, regA, regA, regB);
		// use single register addressing
		regB = regA;
		regA = IMLREG_INVALID;
	}
	if (regA.IsInvalid())
		ppcImlGenContext->emitInst().make_memory_r(regSrc, regB, 0, bitWidth, isBigEndian);
	else
		PPCRecompilerImlGen_generateNewInstruction_memory_r_indexed(ppcImlGenContext, regSrc, regA, regB, bitWidth, false, isBigEndian);
	return true;
}

void PPCRecompilerImlGen_LMW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	cemu_assert_debug(rA != 0);
	sint32 index = 0;
	while (rD <= 31)
	{
		IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
		IMLReg regD = _GetRegGPR(ppcImlGenContext, rD);
		// load word
		ppcImlGenContext->emitInst().make_r_memory(regD, regA, (sint32)imm + index * 4, 32, false, true);
		// next
		rD++;
		index++;
	}
}

void PPCRecompilerImlGen_STMW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rS, rA, imm);
	cemu_assert_debug(rA != 0);
	sint32 index = 0;
	while( rS <= 31 )
	{
		IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
		IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
		// store word
		ppcImlGenContext->emitInst().make_memory_r(regS, regA, (sint32)imm + index * 4, 32, true);
		// next
		rS++;
		index++;
	}
}

bool PPCRecompilerImlGen_LSWI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD, nb;
	PPC_OPC_TEMPL_X(opcode, rD, rA, nb);
	if( nb == 0 )
		nb = 32;

	if (rA == 0)
	{
		cemu_assert_unimplemented(); // special form where gpr is ignored and EA is 0
		return false;
	}

	// potential optimization: On x86 unaligned access is allowed and we could handle the case nb==4 with a single memory read, and nb==2 with a memory read and shift

	IMLReg memReg = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regTmp = _GetRegTemporary(ppcImlGenContext, 0);
	uint32 memOffset = 0;
	while (nb > 0)
	{
		if (rD == rA)
			return false;
		cemu_assert(rD < 32);
		IMLReg regDst = _GetRegGPR(ppcImlGenContext, rD);
		// load bytes one-by-one
		for (sint32 b = 0; b < 4; b++)
		{
			ppcImlGenContext->emitInst().make_r_memory(regTmp, memReg, memOffset + b, 8, false, false);
			sint32 shiftAmount = (3 - b) * 8;
			if(shiftAmount)
				ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_LEFT_SHIFT, regTmp, regTmp, shiftAmount);
			if(b == 0)
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regDst, regTmp);
			else
				ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_OR, regDst, regDst, regTmp);
			nb--;
			if (nb == 0)
				break;
		}
		memOffset += 4;
		rD++;
	}
	return true;
}

bool PPCRecompilerImlGen_STSWI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rS, nb;
	PPC_OPC_TEMPL_X(opcode, rS, rA, nb);
	if( nb == 0 )
		nb = 32;

	IMLReg regMem = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regTmp = _GetRegTemporary(ppcImlGenContext, 0);
	uint32 memOffset = 0;
	while (nb > 0)
	{
		if (rS == rA)
			return false;
		cemu_assert(rS < 32);
		IMLReg regSrc = _GetRegGPR(ppcImlGenContext, rS);
		// store bytes one-by-one
		for (sint32 b = 0; b < 4; b++)
		{
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regTmp, regSrc);
			sint32 shiftAmount = (3 - b) * 8;
			if (shiftAmount)
				ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_RIGHT_SHIFT_U, regTmp, regTmp, shiftAmount);
			ppcImlGenContext->emitInst().make_memory_r(regTmp, regMem, memOffset + b, 8, false);
			nb--;
			if (nb == 0)
				break;
		}
		memOffset += 4;
		rS++;
	}
	return true;
}

bool PPCRecompilerImlGen_LWARX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);

	IMLReg regA = rA != 0 ? PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA) : IMLREG_INVALID;
	IMLReg regB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rB);
	IMLReg regD = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rD);
	IMLReg regMemResEA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_CPU_MEMRES_EA);
	IMLReg regMemResVal = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_CPU_MEMRES_VAL);
	// calculate EA
	if (regA.IsValid())
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, regMemResEA, regA, regB);
	else
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regMemResEA, regB);
	// load word
	ppcImlGenContext->emitInst().make_r_memory(regD, regMemResEA, 0, 32, false, true);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regMemResVal, regD);
	return true;
}

bool PPCRecompilerImlGen_STWCX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rS, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	IMLReg regA = rA != 0 ? PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA) : IMLREG_INVALID;
	IMLReg regB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rB);
	IMLReg regData = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rS);
	IMLReg regTmpDataBE = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 2);
	IMLReg regTmpCompareBE = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 3);
	// calculate EA
	IMLReg regCalcEA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY);
	if (regA.IsValid())
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, regCalcEA, regA, regB);
	else
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regCalcEA, regB);
	// get  CR bit regs and set LT, GT and SO immediately
	IMLReg regCrLT = _GetRegCR(ppcImlGenContext, 0, Espresso::CR_BIT_INDEX_LT);
	IMLReg regCrGT = _GetRegCR(ppcImlGenContext, 0, Espresso::CR_BIT_INDEX_GT);
	IMLReg regCrEQ = _GetRegCR(ppcImlGenContext, 0, Espresso::CR_BIT_INDEX_EQ);
	IMLReg regCrSO = _GetRegCR(ppcImlGenContext, 0, Espresso::CR_BIT_INDEX_SO);
	IMLReg regXerSO = _GetRegCR(ppcImlGenContext, 0, Espresso::CR_BIT_INDEX_SO);
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regCrLT, 0);
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regCrGT, 0);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regCrSO, regXerSO);
	// get regs for reservation address and value
	IMLReg regMemResEA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_CPU_MEMRES_EA);
	IMLReg regMemResVal = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_CPU_MEMRES_VAL);
	// compare calculated EA with reservation
	IMLReg regTmpBool = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 1);
	ppcImlGenContext->emitInst().make_compare(regCalcEA, regMemResEA, regTmpBool, IMLCondition::EQ);
	ppcImlGenContext->emitInst().make_conditional_jump(regTmpBool, true);

	PPCIMLGen_CreateSegmentBranchedPath(*ppcImlGenContext, *ppcImlGenContext->currentBasicBlock,
		[&](ppcImlGenContext_t& genCtx)
		{
			/* branch taken, EA matching */
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ENDIAN_SWAP, regTmpDataBE, regData);
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ENDIAN_SWAP, regTmpCompareBE, regMemResVal);
			ppcImlGenContext->emitInst().make_atomic_cmp_store(regMemResEA, regTmpCompareBE, regTmpDataBE, regCrEQ);
		},
		[&](ppcImlGenContext_t& genCtx)
		{
			/* branch not taken, EA mismatching */
			ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regCrEQ, 0);
		}
	);

	// reset reservation
	// I found contradictory information of whether the reservation is cleared in all cases, so unit testing would be required
	// Most sources state that it is cleared on successful store. They don't explicitly mention what happens on failure
	// "The PowerPC 600 series, part 7: Atomic memory access and cache coherency" states that it is always cleared
	// There may also be different behavior between individual PPC architectures
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regMemResEA, 0);
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regMemResVal, 0);

	return true;
}

bool PPCRecompilerImlGen_DCBZ(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rB;
	rA = (opcode>>16)&0x1F;
	rB = (opcode>>11)&0x1F;
	// prepare registers
	IMLReg regA = rA!=0?PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA):IMLREG_INVALID;
	IMLReg regB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
	// load zero into a temporary register
	IMLReg regZero = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regZero, 0);
	// prepare EA and align it to cacheline
	IMLReg regMemResEA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 1);
	if(rA != 0)
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, regMemResEA, regA, regB);
	else
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regMemResEA, regB);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, regMemResEA, regMemResEA, ~31);
	// zero out the cacheline
	for(sint32 i = 0; i < 32; i += 4)
		ppcImlGenContext->emitInst().make_memory_r(regZero, regMemResEA, i, 32, false);
	return true;
}

bool PPCRecompilerImlGen_OR_NOR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool complementResult)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	if(rS == rB) // check for MR mnemonic
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regA, regS);
	else
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_OR, regA, regS, regB);
	if(complementResult)
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, regA, regA);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_ORC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	// rA = rS | ~rB;
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regTmp = _GetRegTemporary(ppcImlGenContext, 0);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, regTmp, regB);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_OR, regA, regS, regTmp);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_AND_NAND(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool complementResult)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	if (regS == regB)
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, regA, regS);
	else
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_AND, regA, regS, regB);
	if (complementResult)
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, regA, regA);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_ANDC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	// rA = rS & ~rB;
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
	IMLReg regTmp = _GetRegTemporary(ppcImlGenContext, 0);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, regTmp, regB);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_AND, regA, regS, regTmp);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

bool PPCRecompilerImlGen_XOR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool complementResult)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	if( rS == rB )
	{
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regA, 0);
	}
	else
	{
		IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
		IMLReg regB = _GetRegGPR(ppcImlGenContext, rB);
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_XOR, regA, regS, regB);
	}
	if (complementResult)
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, regA, regA);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
	return true;
}

void PPCRecompilerImlGen_ANDI_ANDIS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool isShifted)
{
	sint32 rS, rA;
	uint32 imm;
	if (isShifted)
	{
		PPC_OPC_TEMPL_D_Shift16(opcode, rS, rA, imm);
	}
	else
	{
		PPC_OPC_TEMPL_D_UImm(opcode, rS, rA, imm);
	}
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, regA, regS, (sint32)imm);
	// ANDI/ANDIS always updates cr0
	PPCImlGen_UpdateCR0(ppcImlGenContext, regA);
}

void PPCRecompilerImlGen_ORI_ORIS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool isShifted)
{
	sint32 rS, rA;
	uint32 imm;
	if (isShifted)
	{
		PPC_OPC_TEMPL_D_Shift16(opcode, rS, rA, imm);
	}
	else
	{
		PPC_OPC_TEMPL_D_UImm(opcode, rS, rA, imm);
	}
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_OR, regA, regS, (sint32)imm);
}

void PPCRecompilerImlGen_XORI_XORIS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool isShifted)
{
	sint32 rS, rA;
	uint32 imm;
	if (isShifted)
	{
		PPC_OPC_TEMPL_D_Shift16(opcode, rS, rA, imm);
	}
	else
	{
		PPC_OPC_TEMPL_D_UImm(opcode, rS, rA, imm);
	}
	IMLReg regS = _GetRegGPR(ppcImlGenContext, rS);
	IMLReg regA = _GetRegGPR(ppcImlGenContext, rA);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_XOR, regA, regS, (sint32)imm);
}

bool PPCRecompilerImlGen_CROR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	IMLReg regCrA = _GetRegCR(ppcImlGenContext, crA);
	IMLReg regCrB = _GetRegCR(ppcImlGenContext, crB);
	IMLReg regCrR = _GetRegCR(ppcImlGenContext, crD);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_OR, regCrR, regCrA, regCrB);
	return true;
}

bool PPCRecompilerImlGen_CRORC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	IMLReg regCrA = _GetRegCR(ppcImlGenContext, crA);
	IMLReg regCrB = _GetRegCR(ppcImlGenContext, crB);
	IMLReg regCrR = _GetRegCR(ppcImlGenContext, crD);
	IMLReg regTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_XOR, regTmp, regCrB, 1); // invert crB
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_OR, regCrR, regCrA, regTmp);
	return true;
}

bool PPCRecompilerImlGen_CRAND(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	IMLReg regCrA = _GetRegCR(ppcImlGenContext, crA);
	IMLReg regCrB = _GetRegCR(ppcImlGenContext, crB);
	IMLReg regCrR = _GetRegCR(ppcImlGenContext, crD);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_AND, regCrR, regCrA, regCrB);
	return true;
}

bool PPCRecompilerImlGen_CRANDC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	IMLReg regCrA = _GetRegCR(ppcImlGenContext, crA);
	IMLReg regCrB = _GetRegCR(ppcImlGenContext, crB);
	IMLReg regCrR = _GetRegCR(ppcImlGenContext, crD);
	IMLReg regTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_XOR, regTmp, regCrB, 1); // invert crB
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_AND, regCrR, regCrA, regTmp);
	return true;
}

bool PPCRecompilerImlGen_CRXOR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	IMLReg regCrA = _GetRegCR(ppcImlGenContext, crA);
	IMLReg regCrB = _GetRegCR(ppcImlGenContext, crB);
	IMLReg regCrR = _GetRegCR(ppcImlGenContext, crD);
	if (regCrA == regCrB)
	{
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regCrR, 0);
		return true;
	}
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_XOR, regCrR, regCrA, regCrB);
	return true;
}

bool PPCRecompilerImlGen_CREQV(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	IMLReg regCrA = _GetRegCR(ppcImlGenContext, crA);
	IMLReg regCrB = _GetRegCR(ppcImlGenContext, crB);
	IMLReg regCrR = _GetRegCR(ppcImlGenContext, crD);
	if (regCrA == regCrB)
	{
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regCrR, 1);
		return true;
	}
	IMLReg regTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_XOR, regTmp, regCrB, 1); // invert crB
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_XOR, regCrR, regCrA, regTmp);
	return true;
}

bool PPCRecompilerImlGen_HLE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 hleFuncId = opcode&0xFFFF;
	ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_HLE, ppcImlGenContext->ppcAddressOfCurrentInstruction, hleFuncId, 0, IMLREG_INVALID);
	return true;
}

uint32 PPCRecompiler_iterateCurrentInstruction(ppcImlGenContext_t* ppcImlGenContext)
{
	uint32 v = CPU_swapEndianU32(*(ppcImlGenContext->currentInstruction));
	ppcImlGenContext->currentInstruction += 1;
	return v;
}

uint32 PPCRecompiler_getCurrentInstruction(ppcImlGenContext_t* ppcImlGenContext)
{
	uint32 v = CPU_swapEndianU32(*(ppcImlGenContext->currentInstruction));
	return v;
}

uint32 PPCRecompiler_getPreviousInstruction(ppcImlGenContext_t* ppcImlGenContext)
{
	uint32 v = CPU_swapEndianU32(*(ppcImlGenContext->currentInstruction-1));
	return v;
}

void PPCRecompilerIml_setSegmentPoint(IMLSegmentPoint* segmentPoint, IMLSegment* imlSegment, sint32 index)
{
	segmentPoint->imlSegment = imlSegment;
	segmentPoint->SetInstructionIndex(index);
	if (imlSegment->segmentPointList)
		imlSegment->segmentPointList->prev = segmentPoint;
	segmentPoint->prev = nullptr;
	segmentPoint->next = imlSegment->segmentPointList;
	imlSegment->segmentPointList = segmentPoint;
}

void PPCRecompilerIml_removeSegmentPoint(IMLSegmentPoint* segmentPoint)
{
	if (segmentPoint->prev)
		segmentPoint->prev->next = segmentPoint->next;
	else
		segmentPoint->imlSegment->segmentPointList = segmentPoint->next;
	if (segmentPoint->next)
		segmentPoint->next->prev = segmentPoint->prev;
}

/*
* Insert multiple no-op instructions
* Warning: Can invalidate any previous instruction pointers from the same segment
*/
void PPCRecompiler_pushBackIMLInstructions(IMLSegment* imlSegment, sint32 index, sint32 shiftBackCount)
{
	cemu_assert_debug(index >= 0 && index <= imlSegment->imlList.size());

	imlSegment->imlList.insert(imlSegment->imlList.begin() + index, shiftBackCount, {});

	memset(imlSegment->imlList.data() + index, 0, sizeof(IMLInstruction) * shiftBackCount);

	// fill empty space with NOP instructions
	for (sint32 i = 0; i < shiftBackCount; i++)
	{
		imlSegment->imlList[index + i].type = PPCREC_IML_TYPE_NONE;
	}

	// update position of segment points
	if (imlSegment->segmentPointList)
	{
		IMLSegmentPoint* segmentPoint = imlSegment->segmentPointList;
		while (segmentPoint)
		{
			segmentPoint->ShiftIfAfter(index, shiftBackCount);
			segmentPoint = segmentPoint->next;
		}
	}
}

IMLInstruction* PPCRecompiler_insertInstruction(IMLSegment* imlSegment, sint32 index)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, index, 1);
	return imlSegment->imlList.data() + index;
}

IMLInstruction* PPCRecompiler_appendInstruction(IMLSegment* imlSegment)
{
	size_t index = imlSegment->imlList.size();
	imlSegment->imlList.emplace_back();
	memset(imlSegment->imlList.data() + index, 0, sizeof(IMLInstruction));
	return imlSegment->imlList.data() + index;
}

IMLSegment* PPCRecompilerIml_appendSegment(ppcImlGenContext_t* ppcImlGenContext)
{
	IMLSegment* segment = new IMLSegment();
	ppcImlGenContext->segmentList2.emplace_back(segment);
	return segment;
}

void PPCRecompilerIml_insertSegments(ppcImlGenContext_t* ppcImlGenContext, sint32 index, sint32 count)
{
	ppcImlGenContext->segmentList2.insert(ppcImlGenContext->segmentList2.begin() + index, count, nullptr);
	for (sint32 i = 0; i < count; i++)
		ppcImlGenContext->segmentList2[index + i] = new IMLSegment();
}

bool PPCRecompiler_decodePPCInstruction(ppcImlGenContext_t* ppcImlGenContext)
{
	bool unsupportedInstructionFound = false;

	uint32 opcode = PPCRecompiler_iterateCurrentInstruction(ppcImlGenContext);
	switch ((opcode >> 26))
	{
	case 1:
		if (PPCRecompilerImlGen_HLE(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 4: // opcode category - paired single
		switch (PPC_getBits(opcode, 30, 5))
		{
		case 0: // subcategory compare
			switch (PPC_getBits(opcode, 25, 5))
			{
			case 0:
				if( !PPCRecompilerImlGen_PS_CMPU0(ppcImlGenContext, opcode) )
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 1:
				if( !PPCRecompilerImlGen_PS_CMPO0(ppcImlGenContext, opcode) )
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 2:
				if( !PPCRecompilerImlGen_PS_CMPU1(ppcImlGenContext, opcode) )
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			default:
				unsupportedInstructionFound = true;
				break;
			}
			break;
		case 8: //Sub category - move/negate
			switch (PPC_getBits(opcode, 25, 5))
			{
			case 1: // PS negate
				if (PPCRecompilerImlGen_PS_NEG(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 2: // PS move register
				if (PPCRecompilerImlGen_PS_MR(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 8: // PS abs
				if (PPCRecompilerImlGen_PS_ABS(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			default:
				unsupportedInstructionFound = true;
				break;
			}
			break;
		case 10:
			if (PPCRecompilerImlGen_PS_SUM0(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 11:
			if (PPCRecompilerImlGen_PS_SUM1(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 12: // multiply scalar
			if (PPCRecompilerImlGen_PS_MULS0(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 13: // multiply scalar
			if (PPCRecompilerImlGen_PS_MULS1(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 14: // multiply add scalar
			if (PPCRecompilerImlGen_PS_MADDS0(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 15: // multiply add scalar
			if (PPCRecompilerImlGen_PS_MADDS1(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 16: // sub category - merge
			switch (PPC_getBits(opcode, 25, 5))
			{
			case 16:
				if (PPCRecompilerImlGen_PS_MERGE00(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 17:
				if (PPCRecompilerImlGen_PS_MERGE01(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 18:
				if (PPCRecompilerImlGen_PS_MERGE10(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 19:
				if (PPCRecompilerImlGen_PS_MERGE11(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			default:
				unsupportedInstructionFound = true;
				break;
			}
			break;
		case 18: // divide paired
			if (PPCRecompilerImlGen_PS_DIV(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 20: // sub paired
			if (PPCRecompilerImlGen_PS_SUB(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 21: // add paired
			if (PPCRecompilerImlGen_PS_ADD(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 23: // select paired
			if (PPCRecompilerImlGen_PS_SEL(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 25: // multiply paired
			if (PPCRecompilerImlGen_PS_MUL(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 24: // reciprocal paired
			if (PPCRecompilerImlGen_PS_RES(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 26: // reciprocal squareroot paired
			if (PPCRecompilerImlGen_PS_RSQRTE(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 28: // multiply sub paired
			if (PPCRecompilerImlGen_PS_MSUB(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 29: // multiply add paired
			if (PPCRecompilerImlGen_PS_MADD(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 30: // negative multiply sub paired
			if (PPCRecompilerImlGen_PS_NMSUB(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 31: // negative multiply add paired
			if (PPCRecompilerImlGen_PS_NMADD(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		default:
			unsupportedInstructionFound = true;
			break;
		}
		break;
	case 7: // MULLI
		PPCRecompilerImlGen_MULLI(ppcImlGenContext, opcode);
		break;
	case 8: // SUBFIC
		if (!PPCRecompilerImlGen_SUBFIC(ppcImlGenContext, opcode))
			unsupportedInstructionFound = true;
		break;
	case 10: // CMPLI
		if (!PPCRecompilerImlGen_CMPI(ppcImlGenContext, opcode, true))
			unsupportedInstructionFound = true;
		break;
	case 11: // CMPI
		if (!PPCRecompilerImlGen_CMPI(ppcImlGenContext, opcode, false))
			unsupportedInstructionFound = true;
		break;
	case 12: // ADDIC
		if (PPCRecompilerImlGen_ADDIC_(ppcImlGenContext, opcode, false) == false)
			unsupportedInstructionFound = true;
		break;
	case 13: // ADDIC.
		if (PPCRecompilerImlGen_ADDIC_(ppcImlGenContext, opcode, true) == false)
			unsupportedInstructionFound = true;
		break;
	case 14: // ADDI
		if (PPCRecompilerImlGen_ADDI(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		break;
	case 15: // ADDIS
		if (PPCRecompilerImlGen_ADDIS(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		break;
	case 16: // BC
		if (PPCRecompilerImlGen_BC(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		break;
	case 17:
		if (PPC_getBits(opcode, 30, 1) == 1)
		{
			// SC -> no-op
		}
		else
		{
			unsupportedInstructionFound = true;
		}
		break;
	case 18: // B
		if (PPCRecompilerImlGen_B(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		break;
	case 19: // opcode category 19
		switch (PPC_getBits(opcode, 30, 10))
		{
		case 0:
			PPCRecompilerImlGen_MCRF(ppcImlGenContext, opcode);
			break;
		case 16: // BCLR
			if (PPCRecompilerImlGen_BCSPR(ppcImlGenContext, opcode, SPR_LR) == false)
				unsupportedInstructionFound = true;
			break;
		case 129:
			if (PPCRecompilerImlGen_CRANDC(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 150:
			if (PPCRecompilerImlGen_ISYNC(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 193:
			if (PPCRecompilerImlGen_CRXOR(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 257:
			if (PPCRecompilerImlGen_CRAND(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 289:
			if (PPCRecompilerImlGen_CREQV(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 417:
			if (PPCRecompilerImlGen_CRORC(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 449:
			if (PPCRecompilerImlGen_CROR(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 528: // BCCTR
			if (PPCRecompilerImlGen_BCSPR(ppcImlGenContext, opcode, SPR_CTR) == false)
				unsupportedInstructionFound = true;
			break;
		default:
			unsupportedInstructionFound = true;
			break;
		}
		break;
	case 20:
		if (PPCRecompilerImlGen_RLWIMI(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		break;
	case 21:
		if (PPCRecompilerImlGen_RLWINM(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		break;
	case 23:
		if (PPCRecompilerImlGen_RLWNM(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		break;
	case 24: // ORI
		PPCRecompilerImlGen_ORI_ORIS(ppcImlGenContext, opcode, false);
		break;
	case 25: // ORIS
		PPCRecompilerImlGen_ORI_ORIS(ppcImlGenContext, opcode, true);
		break;
	case 26: // XORI
		PPCRecompilerImlGen_XORI_XORIS(ppcImlGenContext, opcode, false);
		break;
	case 27: // XORIS
		PPCRecompilerImlGen_XORI_XORIS(ppcImlGenContext, opcode, true);
		break;
	case 28: // ANDI
		PPCRecompilerImlGen_ANDI_ANDIS(ppcImlGenContext, opcode, false);
		break;
	case 29: // ANDIS
		PPCRecompilerImlGen_ANDI_ANDIS(ppcImlGenContext, opcode, true);
		break;
	case 31: // opcode category
		switch (PPC_getBits(opcode, 30, 10))
		{
		case 0:
			PPCRecompilerImlGen_CMP(ppcImlGenContext, opcode, false);
			break;
		case 4:
			PPCRecompilerImlGen_TW(ppcImlGenContext, opcode);
			break;
		case 8:
		if (PPCRecompilerImlGen_SUBFC(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		break;
		case 10:
			if (PPCRecompilerImlGen_ADDC(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 11:
			if (PPCRecompilerImlGen_MULHWU(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 19:
			if (PPCRecompilerImlGen_MFCR(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 20:
			if (PPCRecompilerImlGen_LWARX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 23: // LWZX
			PPCRecompilerImlGen_LOAD_INDEXED(ppcImlGenContext, opcode, 32, false, true, false);
			break;
		case 24:
			if (PPCRecompilerImlGen_SLW(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 26:
			if (PPCRecompilerImlGen_CNTLZW(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 28: // AND
			if (!PPCRecompilerImlGen_AND_NAND(ppcImlGenContext, opcode, false))
				unsupportedInstructionFound = true;
			break;
		case 32:
			PPCRecompilerImlGen_CMP(ppcImlGenContext, opcode, true); // CMPL
			break;
		case 40:
			if (PPCRecompilerImlGen_SUBF(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 54:
			// DBCST - Generates no code
			break;
		case 55: // LWZUX
			PPCRecompilerImlGen_LOAD_INDEXED(ppcImlGenContext, opcode, 32, false, true, true);
			break;
		case 60: // ANDC
			if (!PPCRecompilerImlGen_ANDC(ppcImlGenContext, opcode))
				unsupportedInstructionFound = true;
			break;
		case 75:
			if (PPCRecompilerImlGen_MULHW(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 86:
			// DCBF -> No-Op
			break;
		case 87: // LBZX
			PPCRecompilerImlGen_LOAD_INDEXED(ppcImlGenContext, opcode, 8, false, true, false);
			break;
		case 104:
			if (PPCRecompilerImlGen_NEG(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 119: // LBZUX
			PPCRecompilerImlGen_LOAD_INDEXED(ppcImlGenContext, opcode, 8, false, true, true);
			break;
		case 124: // NOR
			if (!PPCRecompilerImlGen_OR_NOR(ppcImlGenContext, opcode, true))
				unsupportedInstructionFound = true;
			break;
		case 136:
			if (PPCRecompilerImlGen_SUBFE(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 138:
			if (PPCRecompilerImlGen_ADDE(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 144:
			if( !PPCRecompilerImlGen_MTCRF(ppcImlGenContext, opcode))
				unsupportedInstructionFound = true;
			break;
		case 150:
			if (!PPCRecompilerImlGen_STWCX(ppcImlGenContext, opcode))
				unsupportedInstructionFound = true;
			break;
		case 151: // STWX
			if (!PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 32, true, false))
				unsupportedInstructionFound = true;
			break;
		case 183: // STWUX
			if (!PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 32, true, true))
				unsupportedInstructionFound = true;			
			break;
		case 200:
			if (PPCRecompilerImlGen_SUBFZE(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 202:
			if (PPCRecompilerImlGen_ADDZE(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 215: // STBX
			if (!PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 8, true, false))
				unsupportedInstructionFound = true;
			break;
		case 234:
			if (PPCRecompilerImlGen_ADDME(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 235:
			if (PPCRecompilerImlGen_MULLW(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 247: // STBUX
			if (!PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 8, true, true))
				unsupportedInstructionFound = true;
			break;
		case 266:
			if (PPCRecompilerImlGen_ADD(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 279: // LHZX
			PPCRecompilerImlGen_LOAD_INDEXED(ppcImlGenContext, opcode, 16, false, true, false);
			break;
		case 284: // EQV (alias to NXOR)
			if (!PPCRecompilerImlGen_XOR(ppcImlGenContext, opcode, true))
				unsupportedInstructionFound = true;
			break;
		case 311: // LHZUX
			PPCRecompilerImlGen_LOAD_INDEXED(ppcImlGenContext, opcode, 16, false, true, true);
			break;
		case 316: // XOR
			if (!PPCRecompilerImlGen_XOR(ppcImlGenContext, opcode, false))
				unsupportedInstructionFound = true;
			break;
		case 339:
			if (PPCRecompilerImlGen_MFSPR(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 343: // LHAX
			PPCRecompilerImlGen_LOAD_INDEXED(ppcImlGenContext, opcode, 16, true, true, false);
			break;
		case 371:
			if (PPCRecompilerImlGen_MFTB(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 375: // LHAUX
			PPCRecompilerImlGen_LOAD_INDEXED(ppcImlGenContext, opcode, 16, true, true, true);
			break;
		case 407: // STHX
			if (!PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 16, true, false))
				unsupportedInstructionFound = true;
			break;
		case 412:
			if (PPCRecompilerImlGen_ORC(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 439: // STHUX
			if (!PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 16, true, true))
				unsupportedInstructionFound = true;
			break;
		case 444: // OR
			if (!PPCRecompilerImlGen_OR_NOR(ppcImlGenContext, opcode, false))
				unsupportedInstructionFound = true;
			break;
		case 459:
			PPCRecompilerImlGen_DIVWU(ppcImlGenContext, opcode);
			break;
		case 467:
			if (PPCRecompilerImlGen_MTSPR(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 476: // NAND
			if (!PPCRecompilerImlGen_AND_NAND(ppcImlGenContext, opcode, true))
				unsupportedInstructionFound = true;
			break;
		case 491:
			if (PPCRecompilerImlGen_DIVW(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 534: // LWBRX
			PPCRecompilerImlGen_LOAD_INDEXED(ppcImlGenContext, opcode, 32, false, false, false);
			break;
		case 535:
			if (PPCRecompilerImlGen_LFSX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 536:
			if (PPCRecompilerImlGen_SRW(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 567:
			if (PPCRecompilerImlGen_LFSUX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 597:
			if (PPCRecompilerImlGen_LSWI(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 598:
			PPCRecompilerImlGen_SYNC(ppcImlGenContext, opcode);
			break;
		case 599:
			if (PPCRecompilerImlGen_LFDX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 631:
			if (PPCRecompilerImlGen_LFDUX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 662: // STWBRX
			if (!PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 32, false, false))
				unsupportedInstructionFound = true;
			break;
		case 663:
			if (PPCRecompilerImlGen_STFSX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 695:
			if (PPCRecompilerImlGen_STFSUX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 725:
			if (PPCRecompilerImlGen_STSWI(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 727:
			if (PPCRecompilerImlGen_STFDX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 790: // LHBRX
			PPCRecompilerImlGen_LOAD_INDEXED(ppcImlGenContext, opcode, 16, false, false, false);
			break;
		case 792:
			if (PPCRecompilerImlGen_SRAW(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 824:
			if (PPCRecompilerImlGen_SRAWI(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 918: // STHBRX
			if (!PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 16, false, true))
				unsupportedInstructionFound = true;
			break;
		case 922:
			if (PPCRecompilerImlGen_EXTSH(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 954:
			if (PPCRecompilerImlGen_EXTSB(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 983:
			if (PPCRecompilerImlGen_STFIWX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 1014:
			if (PPCRecompilerImlGen_DCBZ(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		default:
			unsupportedInstructionFound = true;
			break;
		}
		break;
	case 32: // LWZ
		if(!PPCRecompilerImlGen_LOAD(ppcImlGenContext, opcode, 32, false, true, false))
			unsupportedInstructionFound = true;
		break;
	case 33: // LWZU
		if (!PPCRecompilerImlGen_LOAD(ppcImlGenContext, opcode, 32, false, true, true))
			unsupportedInstructionFound = true;
		break;
	case 34: // LBZ
		if (!PPCRecompilerImlGen_LOAD(ppcImlGenContext, opcode, 8, false, true, false))
			unsupportedInstructionFound = true;
		break;
	case 35: // LBZU
		if (!PPCRecompilerImlGen_LOAD(ppcImlGenContext, opcode, 8, false, true, true))
			unsupportedInstructionFound = true;
		break;
	case 36: // STW
		if(!PPCRecompilerImlGen_STORE(ppcImlGenContext, opcode, 32, true, false))
			unsupportedInstructionFound = true;
		break;
	case 37: // STWU
		if (!PPCRecompilerImlGen_STORE(ppcImlGenContext, opcode, 32, true, true))
			unsupportedInstructionFound = true;
		break;
	case 38: // STB
		if (!PPCRecompilerImlGen_STORE(ppcImlGenContext, opcode, 8, true, false))
			unsupportedInstructionFound = true;
		break;
	case 39: // STBU
		if (!PPCRecompilerImlGen_STORE(ppcImlGenContext, opcode, 8, true, true))
			unsupportedInstructionFound = true;
		break;
	case 40: // LHZ
		if (!PPCRecompilerImlGen_LOAD(ppcImlGenContext, opcode, 16, false, true, false))
			unsupportedInstructionFound = true;
		break;
	case 41: // LHZU
		if (!PPCRecompilerImlGen_LOAD(ppcImlGenContext, opcode, 16, false, true, true))
			unsupportedInstructionFound = true;
		break;
	case 42: // LHA
		if (!PPCRecompilerImlGen_LOAD(ppcImlGenContext, opcode, 16, true, true, false))
			unsupportedInstructionFound = true;
		break;
	case 43: // LHAU
		if (!PPCRecompilerImlGen_LOAD(ppcImlGenContext, opcode, 16, true, true, true))
			unsupportedInstructionFound = true;
		break;
	case 44: // STH
		if (!PPCRecompilerImlGen_STORE(ppcImlGenContext, opcode, 16, true, false))
			unsupportedInstructionFound = true;
		break;
	case 45: // STHU
		if (!PPCRecompilerImlGen_STORE(ppcImlGenContext, opcode, 16, true, true))
			unsupportedInstructionFound = true;
		break;
	case 46:
		PPCRecompilerImlGen_LMW(ppcImlGenContext, opcode);
		break;
	case 47:
		PPCRecompilerImlGen_STMW(ppcImlGenContext, opcode);
		break;
	case 48:
		if (PPCRecompilerImlGen_LFS(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 49:
		if (PPCRecompilerImlGen_LFSU(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 50:
		if (PPCRecompilerImlGen_LFD(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 51:
		if (PPCRecompilerImlGen_LFDU(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 52:
		if (PPCRecompilerImlGen_STFS(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 53:
		if (PPCRecompilerImlGen_STFSU(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 54:
		if (PPCRecompilerImlGen_STFD(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 55:
		if (PPCRecompilerImlGen_STFDU(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 56:
		if (PPCRecompilerImlGen_PSQ_L(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 57:
		if (PPCRecompilerImlGen_PSQ_LU(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 59: // opcode category
		switch (PPC_getBits(opcode, 30, 5))
		{
		case 18:
			if (PPCRecompilerImlGen_FDIVS(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 20:
			if (PPCRecompilerImlGen_FSUBS(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 21:
			if (PPCRecompilerImlGen_FADDS(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 24:
			if (PPCRecompilerImlGen_FRES(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 25:
			if (PPCRecompilerImlGen_FMULS(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 28:
			if (PPCRecompilerImlGen_FMSUBS(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 29:
			if (PPCRecompilerImlGen_FMADDS(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 30:
			if (PPCRecompilerImlGen_FNMSUBS(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		default:
			unsupportedInstructionFound = true;
			break;
		}
		break;
	case 60:
		if (PPCRecompilerImlGen_PSQ_ST(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 61:
		if (PPCRecompilerImlGen_PSQ_STU(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		ppcImlGenContext->hasFPUInstruction = true;
		break;
	case 63: // opcode category
		switch (PPC_getBits(opcode, 30, 5))
		{
		case 0:
			if (PPCRecompilerImlGen_FCMPU(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 12:
			if (PPCRecompilerImlGen_FRSP(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 15:
			if (PPCRecompilerImlGen_FCTIWZ(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 18:
			if (PPCRecompilerImlGen_FDIV(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 20:
			if (PPCRecompilerImlGen_FSUB(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 21:
			if (PPCRecompilerImlGen_FADD(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 23:
			if (PPCRecompilerImlGen_FSEL(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 25:
			if (PPCRecompilerImlGen_FMUL(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 26:
			if (PPCRecompilerImlGen_FRSQRTE(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 28:
			if (PPCRecompilerImlGen_FMSUB(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 29:
			if (PPCRecompilerImlGen_FMADD(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		case 30:
			if (PPCRecompilerImlGen_FNMSUB(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
			break;
		default:
			switch (PPC_getBits(opcode, 30, 10))
			{
			case 32:
				if (PPCRecompilerImlGen_FCMPO(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 40:
				if (PPCRecompilerImlGen_FNEG(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 72:
				if (PPCRecompilerImlGen_FMR(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 136:
				if (PPCRecompilerImlGen_FNABS(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 264:
				if (PPCRecompilerImlGen_FABS(ppcImlGenContext, opcode) == false)
					unsupportedInstructionFound = true;
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			default:
				unsupportedInstructionFound = true;
				break;
			}
			break;
		}
		break;
	default:
		unsupportedInstructionFound = true;
		break;
	}
	return unsupportedInstructionFound;
}

// returns false if code flow is not interrupted
// continueDefaultPath: Controls if 
bool PPCRecompiler_CheckIfInstructionEndsSegment(PPCFunctionBoundaryTracker& boundaryTracker, uint32 instructionAddress, uint32 opcode, bool& makeNextInstEnterable, bool& continueDefaultPath, bool& hasBranchTarget, uint32& branchTarget)
{
	hasBranchTarget = false;
	branchTarget = 0xFFFFFFFF;
	makeNextInstEnterable = false;
	continueDefaultPath = false;
	switch (Espresso::GetPrimaryOpcode(opcode))
	{
	case Espresso::PrimaryOpcode::VIRTUAL_HLE:
	{
		makeNextInstEnterable = true;
		hasBranchTarget = false;
		continueDefaultPath = false;
		return true;
	}
	case Espresso::PrimaryOpcode::BC:
	{
		uint32 BD, BI;
		Espresso::BOField BO;
		bool AA, LK;
		Espresso::decodeOp_BC(opcode, BD, BO, BI, AA, LK);
		if (!LK)
		{
			hasBranchTarget = true;
			branchTarget = (AA ? BD : BD) + instructionAddress;
			if (!boundaryTracker.ContainsAddress(branchTarget))
				hasBranchTarget = false; // far jump
		}
		makeNextInstEnterable = LK;
		continueDefaultPath = true;
		return true;
	}
	case Espresso::PrimaryOpcode::B:
	{
		uint32 LI;
		bool AA, LK;
		Espresso::decodeOp_B(opcode, LI, AA, LK);
		if (!LK)
		{
			hasBranchTarget = true;
			branchTarget = AA ? LI : LI + instructionAddress;
			if (!boundaryTracker.ContainsAddress(branchTarget))
				hasBranchTarget = false; // far jump
		}
		makeNextInstEnterable = LK;
		continueDefaultPath = false;
		return true;
	}
	case Espresso::PrimaryOpcode::GROUP_19:
		switch (Espresso::GetGroup19Opcode(opcode))
		{
		case Espresso::Opcode19::BCLR:
		case Espresso::Opcode19::BCCTR:
		{
			Espresso::BOField BO;
			uint32 BI;
			bool LK;
			Espresso::decodeOp_BCSPR(opcode, BO, BI, LK);
			continueDefaultPath = !BO.conditionIgnore() || !BO.decrementerIgnore(); // if branch is always taken then there is no continued path
			makeNextInstEnterable = Espresso::DecodeLK(opcode);
			return true;
		}
		default:
			break;
		}
		break;
	case Espresso::PrimaryOpcode::GROUP_31:
		switch (Espresso::GetGroup31Opcode(opcode))
		{
		default:
			break;
		}
		break;
	default:
		break;
	}
	return false;
}

void PPCRecompiler_DetermineBasicBlockRange(std::vector<PPCBasicBlockInfo>& basicBlockList, PPCFunctionBoundaryTracker& boundaryTracker, uint32 ppcStart, uint32 ppcEnd, const std::set<uint32>& combinedBranchTargets, const std::set<uint32>& entryAddresses)
{
	cemu_assert_debug(ppcStart <= ppcEnd);

	uint32 currentAddr = ppcStart;

	PPCBasicBlockInfo* curBlockInfo = &basicBlockList.emplace_back(currentAddr, entryAddresses);

	uint32 basicBlockStart = currentAddr;	
	while (currentAddr <= ppcEnd)
	{
		curBlockInfo->lastAddress = currentAddr;
		uint32 opcode = memory_readU32(currentAddr);
		bool nextInstIsEnterable = false;
		bool hasBranchTarget = false;
		bool hasContinuedFlow = false;
		uint32 branchTarget = 0;
		if (PPCRecompiler_CheckIfInstructionEndsSegment(boundaryTracker, currentAddr, opcode, nextInstIsEnterable, hasContinuedFlow, hasBranchTarget, branchTarget))
		{
			curBlockInfo->hasBranchTarget = hasBranchTarget;
			curBlockInfo->branchTarget = branchTarget;
			curBlockInfo->hasContinuedFlow = hasContinuedFlow;
			// start new basic block, except if this is the last instruction
			if (currentAddr >= ppcEnd)
				break;
			curBlockInfo = &basicBlockList.emplace_back(currentAddr + 4, entryAddresses);
			curBlockInfo->isEnterable = curBlockInfo->isEnterable || nextInstIsEnterable;
			currentAddr += 4;
			continue;
		}
		currentAddr += 4;
		if (currentAddr <= ppcEnd)
		{
			if (combinedBranchTargets.find(currentAddr) != combinedBranchTargets.end())
			{
				// instruction is branch target, start new basic block
				curBlockInfo = &basicBlockList.emplace_back(currentAddr, entryAddresses);
			}
		}

	}
}

std::vector<PPCBasicBlockInfo> PPCRecompiler_DetermineBasicBlockRange(PPCFunctionBoundaryTracker& boundaryTracker, const std::set<uint32>& entryAddresses)
{
	cemu_assert(!entryAddresses.empty());
	std::vector<PPCBasicBlockInfo> basicBlockList;

	const std::set<uint32> branchTargets = boundaryTracker.GetBranchTargets();
	auto funcRanges = boundaryTracker.GetRanges();

	std::set<uint32> combinedBranchTargets = branchTargets;
	combinedBranchTargets.insert(entryAddresses.begin(), entryAddresses.end());

	for (auto& funcRangeIt : funcRanges)
		PPCRecompiler_DetermineBasicBlockRange(basicBlockList, boundaryTracker, funcRangeIt.startAddress, funcRangeIt.startAddress + funcRangeIt.length - 4, combinedBranchTargets, entryAddresses);

	// mark all segments that start at entryAddresses as enterable (debug code for verification, can be removed)
	size_t numMarkedEnterable = 0;
	for (auto& basicBlockIt : basicBlockList)
	{
		if (entryAddresses.find(basicBlockIt.startAddress) != entryAddresses.end())
		{
			cemu_assert_debug(basicBlockIt.isEnterable);
			numMarkedEnterable++;
		}
	}
	cemu_assert_debug(numMarkedEnterable == entryAddresses.size());

	// todo - inline BL, currently this is done in the instruction handler of BL but this will mean that instruction cycle increasing is ignored

	return basicBlockList;
}

bool PPCIMLGen_FillBasicBlock(ppcImlGenContext_t& ppcImlGenContext, PPCBasicBlockInfo& basicBlockInfo)
{
	ppcImlGenContext.currentOutputSegment = basicBlockInfo.GetSegmentForInstructionAppend();
	ppcImlGenContext.currentInstruction = (uint32*)(memory_base + basicBlockInfo.startAddress);

	uint32* firstCurrentInstruction = ppcImlGenContext.currentInstruction;
	uint32* endCurrentInstruction = (uint32*)(memory_base + basicBlockInfo.lastAddress);

	while (ppcImlGenContext.currentInstruction <= endCurrentInstruction)
	{
		uint32 addressOfCurrentInstruction = (uint32)((uint8*)ppcImlGenContext.currentInstruction - memory_base);
		ppcImlGenContext.ppcAddressOfCurrentInstruction = addressOfCurrentInstruction;

		if (PPCRecompiler_decodePPCInstruction(&ppcImlGenContext))
		{
			cemuLog_logDebug(LogType::Force, "PPCRecompiler: Unsupported instruction at 0x{:08x}", addressOfCurrentInstruction);
			ppcImlGenContext.currentOutputSegment = nullptr;
			return false;
		}
	}
	ppcImlGenContext.currentOutputSegment = nullptr;
	return true;
}

// returns split segment from which the continued segment is available via seg->GetBranchNotTaken()
IMLSegment* PPCIMLGen_CreateSplitSegmentAtEnd(ppcImlGenContext_t& ppcImlGenContext, PPCBasicBlockInfo& basicBlockInfo)
{
	IMLSegment* writeSegment = basicBlockInfo.GetSegmentForInstructionAppend();

	IMLSegment* continuedSegment = ppcImlGenContext.InsertSegment(ppcImlGenContext.GetSegmentIndex(writeSegment) + 1);

	continuedSegment->SetLinkBranchTaken(writeSegment->GetBranchTaken());
	continuedSegment->SetLinkBranchNotTaken(writeSegment->GetBranchNotTaken());

	writeSegment->SetLinkBranchNotTaken(continuedSegment);
	writeSegment->SetLinkBranchTaken(nullptr);

	if (ppcImlGenContext.currentOutputSegment == writeSegment)
		ppcImlGenContext.currentOutputSegment = continuedSegment;

	cemu_assert_debug(basicBlockInfo.appendSegment == writeSegment);
	basicBlockInfo.appendSegment = continuedSegment;

	return writeSegment;
}

// generates a new segment and sets it as branch target for the current write segment. Returns the created segment
IMLSegment* PPCIMLGen_CreateNewSegmentAsBranchTarget(ppcImlGenContext_t& ppcImlGenContext, PPCBasicBlockInfo& basicBlockInfo)
{
	IMLSegment* writeSegment = basicBlockInfo.GetSegmentForInstructionAppend();
	IMLSegment* branchTargetSegment = ppcImlGenContext.NewSegment();
	cemu_assert_debug(!writeSegment->GetBranchTaken()); // must not have a target already
	writeSegment->SetLinkBranchTaken(branchTargetSegment);
	return branchTargetSegment;
}

// verify that current instruction is the last instruction of the active basic block
void PPCIMLGen_AssertIfNotLastSegmentInstruction(ppcImlGenContext_t& ppcImlGenContext)
{
	cemu_assert_debug(ppcImlGenContext.currentBasicBlock->lastAddress == ppcImlGenContext.ppcAddressOfCurrentInstruction);
}

bool PPCRecompiler_IsBasicBlockATightFiniteLoop(IMLSegment* imlSegment, PPCBasicBlockInfo& basicBlockInfo)
{
	// if we detect a finite loop we can skip generating the cycle check
	// currently we only check for BDNZ loops since thats reasonably safe to rely on
	// however there are other forms of loops that can be classified as finite,
	// but detecting those involves analyzing PPC code and we dont have the infrastructure for that (e.g. IML has CheckRegisterUsage but we dont have an equivalent for PPC code)

	// base criteria, must jump to beginning of same segment
	if (imlSegment->nextSegmentBranchTaken != imlSegment)
		return false;

	uint32 opcode = *(uint32be*)(memory_base + basicBlockInfo.lastAddress);
	if (Espresso::GetPrimaryOpcode(opcode) != Espresso::PrimaryOpcode::BC)
		return false;
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_B(opcode, BO, BI, BD);
	Espresso::BOField boField(BO);
	if(!boField.conditionIgnore() || boField.branchAlways())
		return false;
	if(boField.decrementerIgnore())
		return false;
	return true;
}

void PPCRecompiler_HandleCycleCheckCount(ppcImlGenContext_t& ppcImlGenContext, PPCBasicBlockInfo& basicBlockInfo)
{
	IMLSegment* imlSegment = basicBlockInfo.GetFirstSegmentInChain();
	if (!basicBlockInfo.hasBranchTarget)
		return;
	if (basicBlockInfo.branchTarget > basicBlockInfo.startAddress)
		return;

	if (PPCRecompiler_IsBasicBlockATightFiniteLoop(imlSegment, basicBlockInfo))
		return;

	// make the segment enterable so execution can return after passing a check
	basicBlockInfo.GetFirstSegmentInChain()->SetEnterable(basicBlockInfo.startAddress);

	IMLSegment* splitSeg = PPCIMLGen_CreateSplitSegmentAtEnd(ppcImlGenContext, basicBlockInfo);
	splitSeg->AppendInstruction()->make_cjump_cycle_check();

	IMLSegment* exitSegment = ppcImlGenContext.NewSegment();
	splitSeg->SetLinkBranchTaken(exitSegment);

	exitSegment->AppendInstruction()->make_macro(PPCREC_IML_MACRO_LEAVE, basicBlockInfo.startAddress, 0, 0, IMLREG_INVALID);

	cemu_assert_debug(splitSeg->nextSegmentBranchNotTaken);
	// let the IML optimizer and RA know that the original segment should be used during analysis for dead code elimination
	exitSegment->SetNextSegmentForOverwriteHints(splitSeg->nextSegmentBranchNotTaken);
}

void PPCRecompiler_SetSegmentsUncertainFlow(ppcImlGenContext_t& ppcImlGenContext)
{
	for (IMLSegment* segIt : ppcImlGenContext.segmentList2)
	{
		bool isLastSegment = segIt == ppcImlGenContext.segmentList2.back();
		// handle empty segment
		if (segIt->imlList.empty())
		{
			cemu_assert_debug(segIt->GetBranchNotTaken());
			continue;
		}
		// check last instruction of segment
		IMLInstruction* imlInstruction = segIt->GetLastInstruction();
		if (imlInstruction->type == PPCREC_IML_TYPE_MACRO)
		{
			auto macroType = imlInstruction->operation;
			switch (macroType)
			{
				case PPCREC_IML_MACRO_B_TO_REG:
				case PPCREC_IML_MACRO_BL:
				case PPCREC_IML_MACRO_B_FAR:
				case PPCREC_IML_MACRO_HLE:
				case PPCREC_IML_MACRO_LEAVE:
					segIt->nextSegmentIsUncertain = true;
					break;
				case PPCREC_IML_MACRO_DEBUGBREAK:
				case PPCREC_IML_MACRO_COUNT_CYCLES:
					break;
				default:
				cemu_assert_unimplemented();
			}
		}
	}
}

bool PPCRecompiler_GenerateIML(ppcImlGenContext_t& ppcImlGenContext, PPCFunctionBoundaryTracker& boundaryTracker, std::set<uint32>& entryAddresses)
{
	std::vector<PPCBasicBlockInfo> basicBlockList = PPCRecompiler_DetermineBasicBlockRange(boundaryTracker, entryAddresses);

	// create segments
	std::unordered_map<uint32, PPCBasicBlockInfo*> addrToBB;
	ppcImlGenContext.segmentList2.resize(basicBlockList.size());
	for (size_t i = 0; i < basicBlockList.size(); i++)
	{
		PPCBasicBlockInfo& basicBlockInfo = basicBlockList[i];
		IMLSegment* seg = new IMLSegment();
		seg->ppcAddress = basicBlockInfo.startAddress;
		if(basicBlockInfo.isEnterable)
			seg->SetEnterable(basicBlockInfo.startAddress);
		ppcImlGenContext.segmentList2[i] = seg;
		cemu_assert_debug(addrToBB.find(basicBlockInfo.startAddress) == addrToBB.end());
		basicBlockInfo.SetInitialSegment(seg);
		addrToBB.emplace(basicBlockInfo.startAddress, &basicBlockInfo);
	}
	// link segments
	for (size_t i = 0; i < basicBlockList.size(); i++)
	{
		PPCBasicBlockInfo& bbInfo = basicBlockList[i];
		cemu_assert_debug(bbInfo.GetFirstSegmentInChain() == bbInfo.GetSegmentForInstructionAppend());
		IMLSegment* seg = ppcImlGenContext.segmentList2[i];
		if (bbInfo.hasBranchTarget)
		{
			PPCBasicBlockInfo* targetBB = addrToBB[bbInfo.branchTarget];
			cemu_assert_debug(targetBB);
			IMLSegment_SetLinkBranchTaken(seg, targetBB->GetFirstSegmentInChain());
		}
		if (bbInfo.hasContinuedFlow)
		{
			PPCBasicBlockInfo* targetBB = addrToBB[bbInfo.lastAddress + 4];
			if (!targetBB)
			{
				cemuLog_log(LogType::Recompiler, "Recompiler was unable to link segment [0x{:08x}-0x{:08x}] to 0x{:08x}", bbInfo.startAddress, bbInfo.lastAddress, bbInfo.lastAddress + 4);
				return false;
			}
			cemu_assert_debug(targetBB);
			IMLSegment_SetLinkBranchNotTaken(seg, targetBB->GetFirstSegmentInChain());
		}
	}
	// we assume that all unreachable segments are potentially enterable
	// todo - mark them as such


	// generate cycle counters
	// in theory we could generate these as part of FillBasicBlock() but in the future we might use more complex logic to emit fewer operations
	for (size_t i = 0; i < basicBlockList.size(); i++)
	{
		PPCBasicBlockInfo& basicBlockInfo = basicBlockList[i];
		IMLSegment* seg = basicBlockInfo.GetSegmentForInstructionAppend();

		uint32 ppcInstructionCount = (basicBlockInfo.lastAddress - basicBlockInfo.startAddress + 4) / 4;
		cemu_assert_debug(ppcInstructionCount > 0);

		PPCRecompiler_pushBackIMLInstructions(seg, 0, 1);
		seg->imlList[0].type = PPCREC_IML_TYPE_MACRO;
		seg->imlList[0].operation = PPCREC_IML_MACRO_COUNT_CYCLES;
		seg->imlList[0].op_macro.param = ppcInstructionCount;
	}

	// generate cycle check instructions
	// note: Introduces new segments
	for (size_t i = 0; i < basicBlockList.size(); i++)
	{
		PPCBasicBlockInfo& basicBlockInfo = basicBlockList[i];
		PPCRecompiler_HandleCycleCheckCount(ppcImlGenContext, basicBlockInfo);
	}

	// fill in all the basic blocks
	// note: This step introduces new segments as is necessary for some instructions
	for (size_t i = 0; i < basicBlockList.size(); i++)
	{
		PPCBasicBlockInfo& basicBlockInfo = basicBlockList[i];
		ppcImlGenContext.currentBasicBlock = &basicBlockInfo;
		if (!PPCIMLGen_FillBasicBlock(ppcImlGenContext, basicBlockInfo))
			return false;
		ppcImlGenContext.currentBasicBlock = nullptr;
	}

	// mark segments with unknown jump destination (e.g. BLR and most macros)
	PPCRecompiler_SetSegmentsUncertainFlow(ppcImlGenContext);

	// debug - check segment graph
#ifdef CEMU_DEBUG_ASSERT
	//for (size_t i = 0; i < basicBlockList.size(); i++)
	//{
	//	IMLSegment* seg = ppcImlGenContext.segmentList2[i];
	//	if (seg->list_prevSegments.empty())
	//	{
	//		cemu_assert_debug(seg->isEnterable);
	//	}
	//}
	// debug - check if suffix instructions are at the end of segments and if they are present for branching segments
	for (size_t segIndex = 0; segIndex < ppcImlGenContext.segmentList2.size(); segIndex++)
	{
		IMLSegment* seg = ppcImlGenContext.segmentList2[segIndex];
		IMLSegment* nextSeg = (segIndex+1) < ppcImlGenContext.segmentList2.size() ? ppcImlGenContext.segmentList2[segIndex + 1] : nullptr;

		if (seg->imlList.size() > 0)
		{
			for (size_t f = 0; f < seg->imlList.size() - 1; f++)
			{
				if (seg->imlList[f].IsSuffixInstruction())
				{
					debug_printf("---------------- SegmentDump (Suffix instruction at wrong pos in segment 0x%x):\n", (int)segIndex);
					IMLDebug_Dump(&ppcImlGenContext);
					DEBUG_BREAK;
				}
			}
		}
		if (seg->nextSegmentBranchTaken)
		{
			if (!seg->HasSuffixInstruction())
			{
				debug_printf("---------------- SegmentDump (NoSuffixInstruction in segment 0x%x):\n", (int)segIndex);
				IMLDebug_Dump(&ppcImlGenContext);
				DEBUG_BREAK;
			}
		}
		if (seg->nextSegmentBranchNotTaken)
		{
			// if branch not taken, flow must continue to next segment in sequence
			cemu_assert_debug(seg->nextSegmentBranchNotTaken == nextSeg);
		}
		// more detailed checks based on actual suffix instruction
		if (seg->imlList.size() > 0)
		{
			IMLInstruction* inst = seg->GetLastInstruction();
			if (inst->type == PPCREC_IML_TYPE_MACRO && inst->op_macro.param == PPCREC_IML_MACRO_B_FAR)
			{
				cemu_assert_debug(!seg->GetBranchTaken());
				cemu_assert_debug(!seg->GetBranchNotTaken());
			}
			if (inst->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
			{
				cemu_assert_debug(seg->GetBranchTaken());
				cemu_assert_debug(seg->GetBranchNotTaken());
			}
			if (inst->type == PPCREC_IML_TYPE_CONDITIONAL_JUMP)
			{
				if (!seg->GetBranchTaken() || !seg->GetBranchNotTaken())
				{
					debug_printf("---------------- SegmentDump (Missing branch for conditional jump in segment 0x%x):\n", (int)segIndex);
					IMLDebug_Dump(&ppcImlGenContext);
					cemu_assert_error();
				}
			}
		}
		segIndex++;
	}
#endif


	// todos:
	// - basic block determination should look for the B(L) B(L) pattern. Or maybe just mark every bb without any input segments as an entry segment

	return true;
}

void IMLOptimizer_replaceWithConditionalMov(ppcImlGenContext_t& ppcImlGenContext)
{
	// optimization pass - replace segments with conditional MOVs if possible
	//for (IMLSegment* segIt : ppcImlGenContext.segmentList2)
	//{
	//	if (segIt->nextSegmentBranchNotTaken == nullptr || segIt->nextSegmentBranchTaken == nullptr)
	//		continue; // not a branching segment
	//	IMLInstruction* lastInstruction = segIt->GetLastInstruction();
	//	if (lastInstruction->type != PPCREC_IML_TYPE_CJUMP || lastInstruction->op_conditionalJump.crRegisterIndex != 0)
	//		continue;
	//	IMLSegment* conditionalSegment = segIt->nextSegmentBranchNotTaken;
	//	IMLSegment* finalSegment = segIt->nextSegmentBranchTaken;
	//	if (segIt->nextSegmentBranchTaken != segIt->nextSegmentBranchNotTaken->nextSegmentBranchNotTaken)
	//		continue;
	//	if (segIt->nextSegmentBranchNotTaken->imlList.size() > 4)
	//		continue;
	//	if (conditionalSegment->list_prevSegments.size() != 1)
	//		continue; // the reduced segment must not be the target of any other branch
	//	if (conditionalSegment->isEnterable)
	//		continue;
	//	// check if the segment contains only iml instructions that can be turned into conditional moves (Value assignment, register assignment)
	//	bool canReduceSegment = true;
	//	for (sint32 f = 0; f < conditionalSegment->imlList.size(); f++)
	//	{
	//		IMLInstruction* imlInstruction = conditionalSegment->imlList.data() + f;
	//		if (imlInstruction->type == PPCREC_IML_TYPE_R_S32 && imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	//			continue;
	//		// todo: Register to register copy
	//		canReduceSegment = false;
	//		break;
	//	}

	//	if (canReduceSegment == false)
	//		continue;

	//	// remove the branch instruction
	//	uint8 branchCond_crRegisterIndex = lastInstruction->op_conditionalJump.crRegisterIndex;
	//	uint8 branchCond_crBitIndex = lastInstruction->op_conditionalJump.crBitIndex;
	//	bool  branchCond_bitMustBeSet = lastInstruction->op_conditionalJump.bitMustBeSet;
	//	lastInstruction->make_no_op();

	//	// append conditional moves based on branch condition
	//	for (sint32 f = 0; f < conditionalSegment->imlList.size(); f++)
	//	{
	//		IMLInstruction* imlInstruction = conditionalSegment->imlList.data() + f;
	//		if (imlInstruction->type == PPCREC_IML_TYPE_R_S32 && imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	//			PPCRecompilerImlGen_generateNewInstruction_conditional_r_s32(&ppcImlGenContext, PPCRecompiler_appendInstruction(segIt), PPCREC_IML_OP_ASSIGN, imlInstruction->op_r_immS32.registerIndex, imlInstruction->op_r_immS32.immS32, branchCond_crRegisterIndex, branchCond_crBitIndex, !branchCond_bitMustBeSet);
	//		else
	//			assert_dbg();
	//	}
	//	// update segment links
	//	// source segment: imlSegment, conditional/removed segment: conditionalSegment, final segment: finalSegment
	//	IMLSegment_RemoveLink(segIt, conditionalSegment);
	//	IMLSegment_RemoveLink(segIt, finalSegment);
	//	IMLSegment_RemoveLink(conditionalSegment, finalSegment);
	//	IMLSegment_SetLinkBranchNotTaken(segIt, finalSegment);
	//	// remove all instructions from conditional segment
	//	conditionalSegment->imlList.clear();

	//	// if possible, merge imlSegment with finalSegment
	//	if (finalSegment->isEnterable == false && finalSegment->list_prevSegments.size() == 1)
	//	{
	//		// todo: Clean this up and move into separate function PPCRecompilerIML_mergeSegments()
	//		IMLSegment_RemoveLink(segIt, finalSegment);
	//		if (finalSegment->nextSegmentBranchNotTaken)
	//		{
	//			IMLSegment* tempSegment = finalSegment->nextSegmentBranchNotTaken;
	//			IMLSegment_RemoveLink(finalSegment, tempSegment);
	//			IMLSegment_SetLinkBranchNotTaken(segIt, tempSegment);
	//		}
	//		if (finalSegment->nextSegmentBranchTaken)
	//		{
	//			IMLSegment* tempSegment = finalSegment->nextSegmentBranchTaken;
	//			IMLSegment_RemoveLink(finalSegment, tempSegment);
	//			IMLSegment_SetLinkBranchTaken(segIt, tempSegment);
	//		}
	//		// copy IML instructions
	//		cemu_assert_debug(segIt != finalSegment);
	//		for (sint32 f = 0; f < finalSegment->imlList.size(); f++)
	//		{
	//			memcpy(PPCRecompiler_appendInstruction(segIt), finalSegment->imlList.data() + f, sizeof(IMLInstruction));
	//		}
	//		finalSegment->imlList.clear();
	//	}

	//	// todo: If possible, merge with the segment following conditionalSegment (merging is only possible if the segment is not an entry point or has no other jump sources)
	//}
}

bool PPCRecompiler_generateIntermediateCode(ppcImlGenContext_t& ppcImlGenContext, PPCRecFunction_t* ppcRecFunc, std::set<uint32>& entryAddresses, PPCFunctionBoundaryTracker& boundaryTracker)
{
	ppcImlGenContext.boundaryTracker = &boundaryTracker;
	if (!PPCRecompiler_GenerateIML(ppcImlGenContext, boundaryTracker, entryAddresses))
		return false;

	// IMLOptimizer_replaceWithConditionalMov(ppcImlGenContext);

	// set range
	// todo - support non-continuous functions for the range tracking?
	ppcRecRange_t recRange;
	recRange.ppcAddress = ppcRecFunc->ppcAddress;
	recRange.ppcSize = ppcRecFunc->ppcSize;
	ppcRecFunc->list_ranges.push_back(recRange);

	
	return true;
}
