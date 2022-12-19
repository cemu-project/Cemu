#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterHelper.h"
#include "Cafe/HW/Espresso/EspressoISA.h"
#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "IML/IML.h"
#include "IML/IMLRegisterAllocatorRanges.h"
#include "PPCFunctionBoundaryTracker.h"

struct PPCBasicBlockInfo
{
	PPCBasicBlockInfo(uint32 startAddress, const std::set<uint32>& entryAddresses) : startAddress(startAddress), lastAddress(startAddress)
	{
		isEnterable = entryAddresses.find(startAddress) != entryAddresses.end();
	}

	uint32 startAddress;
	uint32 lastAddress; // inclusive
	bool isEnterable{ false };
	//uint32 enterableAddress{}; -> covered by startAddress
	bool hasContinuedFlow{ true }; // non-branch path goes to next segment (lastAddress+4), assumed by default
	bool hasBranchTarget{ false };
	uint32 branchTarget{};

	// associated IML segments
	IMLSegment* firstSegment{}; // first segment in chain, used as branch target for other segments
	IMLSegment* appendSegment{}; // last segment in chain, new instructions should be appended to this segment

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

bool PPCRecompiler_decodePPCInstruction(ppcImlGenContext_t* ppcImlGenContext);
uint32 PPCRecompiler_iterateCurrentInstruction(ppcImlGenContext_t* ppcImlGenContext);

IMLInstruction* PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext_t* ppcImlGenContext)
{
	IMLInstruction& inst = ppcImlGenContext->currentOutputSegment->imlList.emplace_back();
	memset(&inst, 0x00, sizeof(IMLInstruction));
	inst.crRegister = PPC_REC_INVALID_REGISTER; // dont update any cr register by default
	return &inst;
}

void PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext_t* ppcImlGenContext, IMLInstruction* imlInstruction, uint32 operation, uint8 registerResult, uint8 registerA, uint8 crRegister, uint8 crMode)
{
	if (imlInstruction)
		__debugbreak(); // not supported

	ppcImlGenContext->emitInst().make_r_r(operation, registerResult, registerA, crRegister, crMode);
}

void PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext_t* ppcImlGenContext, uint32 operation, uint8 registerIndex, sint32 immS32, uint8 crRegister, uint32 crMode)
{
	ppcImlGenContext->emitInst().make_r_s32(operation, registerIndex, immS32, crRegister, crMode);
}

void PPCRecompilerImlGen_generateNewInstruction_name_r(ppcImlGenContext_t* ppcImlGenContext, uint32 operation, uint8 registerIndex, uint32 name)
{
	// Store name (e.g. "'r3' = t0" which translates to MOV [ESP+offset_r3], reg32)
	IMLInstruction* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_NAME_R;
	imlInstruction->operation = operation;
	imlInstruction->op_r_name.registerIndex = registerIndex;
	imlInstruction->op_r_name.name = name;
}

void PPCRecompilerImlGen_generateNewInstruction_conditional_r_s32(ppcImlGenContext_t* ppcImlGenContext, IMLInstruction* imlInstruction, uint32 operation, uint8 registerIndex, sint32 immS32, uint32 crRegisterIndex, uint32 crBitIndex, bool bitMustBeSet)
{
	if(imlInstruction == NULL)
		imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	else
		memset(imlInstruction, 0, sizeof(IMLInstruction));
	imlInstruction->type = PPCREC_IML_TYPE_CONDITIONAL_R_S32;
	imlInstruction->operation = operation;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	// r_s32 operation
	imlInstruction->op_conditional_r_s32.registerIndex = registerIndex;
	imlInstruction->op_conditional_r_s32.immS32 = immS32;
	// condition
	imlInstruction->op_conditional_r_s32.crRegisterIndex = crRegisterIndex;
	imlInstruction->op_conditional_r_s32.crBitIndex = crBitIndex;
	imlInstruction->op_conditional_r_s32.bitMustBeSet = bitMustBeSet;
}


// jump based on segment branches
void PPCRecompilerImlGen_generateNewInstruction_jumpSegment(ppcImlGenContext_t* ppcImlGenContext, IMLInstruction* imlInstruction)
{
	// jump
	if (imlInstruction == NULL)
		imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_CJUMP;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_conditionalJump.condition = PPCREC_JUMP_CONDITION_NONE;
	imlInstruction->op_conditionalJump.crRegisterIndex = 0;
	imlInstruction->op_conditionalJump.crBitIndex = 0;
	imlInstruction->op_conditionalJump.bitMustBeSet = false;
}

void PPCRecompilerImlGen_generateNewInstruction_conditionalJumpSegment(ppcImlGenContext_t* ppcImlGenContext, uint32 jumpCondition, uint32 crRegisterIndex, uint32 crBitIndex, bool bitMustBeSet)
{
	// conditional jump
	IMLInstruction* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_CJUMP;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_conditionalJump.condition = jumpCondition;
	imlInstruction->op_conditionalJump.crRegisterIndex = crRegisterIndex;
	imlInstruction->op_conditionalJump.crBitIndex = crBitIndex;
	imlInstruction->op_conditionalJump.bitMustBeSet = bitMustBeSet;
}

void PPCRecompilerImlGen_generateNewInstruction_cr(ppcImlGenContext_t* ppcImlGenContext, uint32 operation, uint8 crD, uint8 crA, uint8 crB)
{
	// multiple variations:
	// operation involving only one cr bit (like clear crD bit)
	// operation involving three cr bits (like crD = crA or crB)
	IMLInstruction* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_CR;
	imlInstruction->operation = operation;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->crMode = 0;
	imlInstruction->op_cr.crD = crD;
	imlInstruction->op_cr.crA = crA;
	imlInstruction->op_cr.crB = crB;
}

void PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext_t* ppcImlGenContext, uint8 registerDestination, uint8 registerMemory, sint32 immS32, uint32 copyWidth, bool signExtend, bool switchEndian)
{
	ppcImlGenContext->emitInst().make_r_memory(registerDestination, registerMemory, immS32, copyWidth, signExtend, switchEndian);
}

void PPCRecompilerImlGen_generateNewInstruction_r_memory_indexed(ppcImlGenContext_t* ppcImlGenContext, uint8 registerDestination, uint8 registerMemory1, uint8 registerMemory2, uint32 copyWidth, bool signExtend, bool switchEndian)
{
	// load from memory
	IMLInstruction* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_LOAD_INDEXED;
	imlInstruction->operation = 0;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_storeLoad.registerData = registerDestination;
	imlInstruction->op_storeLoad.registerMem = registerMemory1;
	imlInstruction->op_storeLoad.registerMem2 = registerMemory2;
	imlInstruction->op_storeLoad.copyWidth = copyWidth;
	imlInstruction->op_storeLoad.flags2.swapEndian = switchEndian;
	imlInstruction->op_storeLoad.flags2.signExtend = signExtend;
}

void PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext_t* ppcImlGenContext, uint8 registerSource, uint8 registerMemory, sint32 immS32, uint32 copyWidth, bool switchEndian)
{
	ppcImlGenContext->emitInst().make_memory_r(registerSource, registerMemory, immS32, copyWidth, switchEndian);
}

void PPCRecompilerImlGen_generateNewInstruction_memory_r_indexed(ppcImlGenContext_t* ppcImlGenContext, uint8 registerDestination, uint8 registerMemory1, uint8 registerMemory2, uint32 copyWidth, bool signExtend, bool switchEndian)
{
	// load from memory
	IMLInstruction* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_STORE_INDEXED;
	imlInstruction->operation = 0;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_storeLoad.registerData = registerDestination;
	imlInstruction->op_storeLoad.registerMem = registerMemory1;
	imlInstruction->op_storeLoad.registerMem2 = registerMemory2;
	imlInstruction->op_storeLoad.copyWidth = copyWidth;
	imlInstruction->op_storeLoad.flags2.swapEndian = switchEndian;
	imlInstruction->op_storeLoad.flags2.signExtend = signExtend;
}

uint32 PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName)
{
	if( mappedName == PPCREC_NAME_NONE )
	{
		debug_printf("PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(): Invalid mappedName parameter\n");
		return PPC_REC_INVALID_REGISTER;
	}
	for(uint32 i=0; i<(PPC_REC_MAX_VIRTUAL_GPR-1); i++)
	{
		if( ppcImlGenContext->mappedRegister[i] == PPCREC_NAME_NONE )
		{
			ppcImlGenContext->mappedRegister[i] = mappedName;
			return i;
		}
	}
	return 0;
}

uint32 PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName)
{
	for(uint32 i=0; i< PPC_REC_MAX_VIRTUAL_GPR; i++)
	{
		if( ppcImlGenContext->mappedRegister[i] == mappedName )
		{
			return i;
		}
	}
	return PPC_REC_INVALID_REGISTER;
}

uint32 PPCRecompilerImlGen_getAndLockFreeTemporaryFPR(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName)
{
	if( mappedName == PPCREC_NAME_NONE )
	{
		debug_printf("PPCRecompilerImlGen_getAndLockFreeTemporaryFPR(): Invalid mappedName parameter\n");
		return PPC_REC_INVALID_REGISTER;
	}
	for(uint32 i=0; i<255; i++)
	{
		if( ppcImlGenContext->mappedFPRRegister[i] == PPCREC_NAME_NONE )
		{
			ppcImlGenContext->mappedFPRRegister[i] = mappedName;
			return i;
		}
	}
	return 0;
}

uint32 PPCRecompilerImlGen_findFPRRegisterByMappedName(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName)
{
	for(uint32 i=0; i<255; i++)
	{
		if( ppcImlGenContext->mappedFPRRegister[i] == mappedName )
		{
			return i;
		}
	}
	return PPC_REC_INVALID_REGISTER;
}

/*
 * Loads a PPC gpr into any of the available IML registers
 * If loadNew is false, it will reuse already loaded instances
 */
uint32 PPCRecompilerImlGen_loadRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName, bool loadNew)
{
	if( loadNew == false )
	{
		uint32 loadedRegisterIndex = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, mappedName);
		if( loadedRegisterIndex != PPC_REC_INVALID_REGISTER )
			return loadedRegisterIndex;
	}
	uint32 registerIndex = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, mappedName);
	return registerIndex;
}

/*
 * Reuse already loaded register if present
 * Otherwise create new IML register and map the name. The register contents will be undefined
 */
uint32 PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName)
{
	uint32 loadedRegisterIndex = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, mappedName);
	if( loadedRegisterIndex != PPC_REC_INVALID_REGISTER )
		return loadedRegisterIndex;
	uint32 registerIndex = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, mappedName);
	return registerIndex;
}

// get throw-away register. Only valid for the scope of a single translated instruction
// be careful to not collide with manually loaded temporary register
uint32 PPCRecompilerImlGen_grabTemporaryS8Register(ppcImlGenContext_t* ppcImlGenContext, uint32 temporaryIndex)
{
	return PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + temporaryIndex);
}

/*
 * Loads a PPC fpr into any of the available IML FPU registers
 * If loadNew is false, it will check first if the fpr is already loaded into any IML register
 */
uint32 PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName, bool loadNew)
{
	if( loadNew == false )
	{
		uint32 loadedRegisterIndex = PPCRecompilerImlGen_findFPRRegisterByMappedName(ppcImlGenContext, mappedName);
		if( loadedRegisterIndex != PPC_REC_INVALID_REGISTER )
			return loadedRegisterIndex;
	}
	uint32 registerIndex = PPCRecompilerImlGen_getAndLockFreeTemporaryFPR(ppcImlGenContext, mappedName);
	return registerIndex;
}

/*
 * Checks if a PPC fpr register is already loaded into any IML register
 * If no, it will create a new undefined temporary IML FPU register and map the name (effectively overwriting the old ppc register)
 */
uint32 PPCRecompilerImlGen_loadOverwriteFPRRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName)
{
	uint32 loadedRegisterIndex = PPCRecompilerImlGen_findFPRRegisterByMappedName(ppcImlGenContext, mappedName);
	if( loadedRegisterIndex != PPC_REC_INVALID_REGISTER )
		return loadedRegisterIndex;
	uint32 registerIndex = PPCRecompilerImlGen_getAndLockFreeTemporaryFPR(ppcImlGenContext, mappedName);
	return registerIndex;
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
void PPCImlGen_UpdateCR0Logical(ppcImlGenContext_t* ppcImlGenContext, uint32 registerR)
{
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, registerR, registerR, 0, PPCREC_CR_MODE_LOGICAL);
}

void PPCRecompilerImlGen_TW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// split before and after to make sure the macro is in an isolated segment that we can make enterable
	PPCIMLGen_CreateSplitSegmentAtEnd(*ppcImlGenContext, *ppcImlGenContext->currentBasicBlock);
	ppcImlGenContext->currentOutputSegment->SetEnterable(ppcImlGenContext->ppcAddressOfCurrentInstruction);
	PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext)->make_macro(PPCREC_IML_MACRO_LEAVE, ppcImlGenContext->ppcAddressOfCurrentInstruction, 0, 0);
	IMLSegment* middleSeg = PPCIMLGen_CreateSplitSegmentAtEnd(*ppcImlGenContext, *ppcImlGenContext->currentBasicBlock);
	middleSeg->SetLinkBranchTaken(nullptr);
	middleSeg->SetLinkBranchNotTaken(nullptr);
}

bool PPCRecompilerImlGen_MTSPR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 rD, spr1, spr2, spr;
	PPC_OPC_TEMPL_XO(opcode, rD, spr1, spr2);
	spr = spr1 | (spr2<<5);
	if (spr == SPR_CTR || spr == SPR_LR)
	{
		uint32 gprReg = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0 + rD);
		if (gprReg == PPC_REC_INVALID_REGISTER)
			gprReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rD);
		uint32 sprReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + spr);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, sprReg, gprReg);
	}
	else if (spr >= SPR_UGQR0 && spr <= SPR_UGQR7)
	{
		uint32 gprReg = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0 + rD);
		if (gprReg == PPC_REC_INVALID_REGISTER)
			gprReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rD);
		uint32 sprReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + spr);
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
	if (spr == SPR_LR || spr == SPR_CTR)
	{
		uint32 sprReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + spr);
		uint32 gprReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0 + rD);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprReg, sprReg);
	}
	else if (spr >= SPR_UGQR0 && spr <= SPR_UGQR7)
	{
		uint32 sprReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + spr);
		uint32 gprReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0 + rD);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprReg, sprReg);
	}
	else
		return false;
	return true;
}

bool PPCRecompilerImlGen_MFTB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	printf("PPCRecompilerImlGen_MFTB(): Not supported\n");
	return false;

	uint32 rD, spr1, spr2, spr;
	PPC_OPC_TEMPL_XO(opcode, rD, spr1, spr2);
	spr = spr1 | (spr2<<5);
	
	if (spr == 268 || spr == 269)
	{
		// TBL / TBU
		uint32 param2 = spr | (rD << 16);
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_MFTB, ppcImlGenContext->ppcAddressOfCurrentInstruction, param2, 0);
		IMLSegment* middleSeg = PPCIMLGen_CreateSplitSegmentAtEnd(*ppcImlGenContext, *ppcImlGenContext->currentBasicBlock);

		return true;
	}
	return false;
}

bool PPCRecompilerImlGen_MFCR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	uint32 gprReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0 + rD);
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_MFCR, gprReg, 0, PPC_REC_INVALID_REGISTER, 0);
	return true;
}

bool PPCRecompilerImlGen_MTCRF(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 rS;
	uint32 crMask;
	PPC_OPC_TEMPL_XFX(opcode, rS, crMask);
	uint32 gprReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0 + rS);
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_MTCRF, gprReg, crMask, PPC_REC_INVALID_REGISTER, 0);
	return true;
}

void PPCRecompilerImlGen_CMP(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(opcode, cr, rA, rB);
	cr >>= 2;
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_COMPARE_SIGNED, gprRegisterA, gprRegisterB, cr, PPCREC_CR_MODE_COMPARE_SIGNED);
}

void PPCRecompilerImlGen_CMPL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(opcode, cr, rA, rB);
	cr >>= 2;
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_COMPARE_UNSIGNED, gprRegisterA, gprRegisterB, cr, PPCREC_CR_MODE_COMPARE_UNSIGNED);
}

void PPCRecompilerImlGen_CMPI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, cr, rA, imm);
	cr >>= 2;
	sint32 b = imm;
	// load gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_COMPARE_SIGNED, gprRegister, b, cr, PPCREC_CR_MODE_COMPARE_SIGNED);
}

void PPCRecompilerImlGen_CMPLI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(opcode, cr, rA, imm);
	cr >>= 2;
	uint32 b = imm;
	// load gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_COMPARE_UNSIGNED, gprRegister, (sint32)b, cr, PPCREC_CR_MODE_COMPARE_UNSIGNED);
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
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_BL, ppcImlGenContext->ppcAddressOfCurrentInstruction, jumpAddressDest, ppcImlGenContext->cyclesSinceLastBranch);
		return true;
	}
	// is jump destination within recompiled function?
	if( ppcImlGenContext->boundaryTracker->ContainsAddress(jumpAddressDest) )
		PPCRecompilerImlGen_generateNewInstruction_jumpSegment(ppcImlGenContext, nullptr);
	else
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_B_FAR, ppcImlGenContext->ppcAddressOfCurrentInstruction, jumpAddressDest, ppcImlGenContext->cyclesSinceLastBranch);
	return true;
}

bool PPCRecompilerImlGen_BC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	PPCIMLGen_AssertIfNotLastSegmentInstruction(*ppcImlGenContext);

	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_B(opcode, BO, BI, BD);

	uint32 crRegister = BI/4;
	uint32 crBit = BI%4;
	uint32 jumpCondition = 0;
	bool conditionMustBeTrue = (BO&8)!=0;
	bool useDecrementer = (BO&4)==0; // bit not set -> decrement
	bool decrementerMustBeZero = (BO&2)!=0; // bit set -> branch if CTR = 0, bit not set -> branch if CTR != 0
	bool ignoreCondition = (BO&16)!=0;

	uint32 jumpAddressDest = BD;
	if( (opcode&PPC_OPC_AA) == 0 )
	{
		jumpAddressDest = BD + (unsigned int)ppcImlGenContext->ppcAddressOfCurrentInstruction;
	}

	if( opcode&PPC_OPC_LK )
	{
		// conditional function calls are not supported
		if( ignoreCondition == false )
		{
			// generate jump condition
			if( conditionMustBeTrue )
			{
				if( crBit == 0 )
					jumpCondition = PPCREC_JUMP_CONDITION_GE;
				else if( crBit == 1 )
					jumpCondition = PPCREC_JUMP_CONDITION_LE;
				else if( crBit == 2 )
					jumpCondition = PPCREC_JUMP_CONDITION_NE;
				else if( crBit == 3 )
					jumpCondition = PPCREC_JUMP_CONDITION_NSUMMARYOVERFLOW;
			}
			else
			{
				if( crBit == 0 )
					jumpCondition = PPCREC_JUMP_CONDITION_L;
				else if( crBit == 1 )
					jumpCondition = PPCREC_JUMP_CONDITION_G;
				else if( crBit == 2 )
					jumpCondition = PPCREC_JUMP_CONDITION_E;
				else if( crBit == 3 )
					jumpCondition = PPCREC_JUMP_CONDITION_SUMMARYOVERFLOW;
			}
			PPCBasicBlockInfo* currentBasicBlock = ppcImlGenContext->currentBasicBlock;
			IMLSegment* blSeg = PPCIMLGen_CreateNewSegmentAsBranchTarget(*ppcImlGenContext, *currentBasicBlock);
			PPCRecompilerImlGen_generateNewInstruction_conditionalJumpSegment(ppcImlGenContext, jumpCondition, crRegister, crBit, conditionMustBeTrue);
			blSeg->AppendInstruction()->make_macro(PPCREC_IML_MACRO_BL, ppcImlGenContext->ppcAddressOfCurrentInstruction, jumpAddressDest, ppcImlGenContext->cyclesSinceLastBranch);
			return true;
		}
		return false;
	}
	// generate iml instructions depending on flags
	if( useDecrementer )
	{
		if( ignoreCondition == false )
			return false; // not supported for the moment
		uint32 ctrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0+SPR_CTR, false);
		uint32 tmpBoolReg = PPCRecompilerImlGen_grabTemporaryS8Register(ppcImlGenContext, 1);
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_SUB, ctrRegister, ctrRegister, 1);
		ppcImlGenContext->emitInst().make_compare_s32(ctrRegister, 0, tmpBoolReg, decrementerMustBeZero ? IMLCondition::EQ : IMLCondition::NEQ);
		ppcImlGenContext->emitInst().make_conditional_jump_new(tmpBoolReg, true);
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
			// generate jump condition
			if( conditionMustBeTrue )
			{
				if( crBit == 0 )
					jumpCondition = PPCREC_JUMP_CONDITION_GE;
				else if( crBit == 1 )
					jumpCondition = PPCREC_JUMP_CONDITION_LE;
				else if( crBit == 2 )
					jumpCondition = PPCREC_JUMP_CONDITION_NE;
				else if( crBit == 3 )
					jumpCondition = PPCREC_JUMP_CONDITION_NSUMMARYOVERFLOW;
			}
			else
			{
				if( crBit == 0 )
					jumpCondition = PPCREC_JUMP_CONDITION_L;
				else if( crBit == 1 )
					jumpCondition = PPCREC_JUMP_CONDITION_G;
				else if( crBit == 2 )
					jumpCondition = PPCREC_JUMP_CONDITION_E;
				else if( crBit == 3 )
					jumpCondition = PPCREC_JUMP_CONDITION_SUMMARYOVERFLOW;
			}

			if (ppcImlGenContext->boundaryTracker->ContainsAddress(jumpAddressDest))
			{
				// near jump
				PPCRecompilerImlGen_generateNewInstruction_conditionalJumpSegment(ppcImlGenContext, jumpCondition, crRegister, crBit, conditionMustBeTrue);
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

	uint32 branchDestReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + sprReg);
	if (LK)
	{
		if (sprReg == SPR_LR)
		{
			// if the branch target is LR, then preserve it in a temporary
			cemu_assert_suspicious(); // this case needs testing
			uint32 tmpRegister = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY);
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, tmpRegister, branchDestReg);
			branchDestReg = tmpRegister;
		}
		uint32 registerLR = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_LR);
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, registerLR, ppcImlGenContext->ppcAddressOfCurrentInstruction + 4, PPC_REC_INVALID_REGISTER, 0);
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
		// generate jump condition
		uint32 jumpCondition = 0;
		if (!BO.conditionInverted())
		{
			// CR bit must be set
			if (crBit == 0)
				jumpCondition = PPCREC_JUMP_CONDITION_L;
			else if (crBit == 1)
				jumpCondition = PPCREC_JUMP_CONDITION_G;
			else if (crBit == 2)
				jumpCondition = PPCREC_JUMP_CONDITION_E;
			else if (crBit == 3)
				jumpCondition = PPCREC_JUMP_CONDITION_SUMMARYOVERFLOW;
		}
		else
		{
			if (crBit == 0)
				jumpCondition = PPCREC_JUMP_CONDITION_GE;
			else if (crBit == 1)
				jumpCondition = PPCREC_JUMP_CONDITION_LE;
			else if (crBit == 2)
				jumpCondition = PPCREC_JUMP_CONDITION_NE;
			else if (crBit == 3)
				jumpCondition = PPCREC_JUMP_CONDITION_NSUMMARYOVERFLOW;
		}

		// write the dynamic branch instruction to a new segment that is set as a branch target for the current segment
		PPCBasicBlockInfo* currentBasicBlock = ppcImlGenContext->currentBasicBlock;
		IMLSegment* bctrSeg = PPCIMLGen_CreateNewSegmentAsBranchTarget(*ppcImlGenContext, *currentBasicBlock);

		PPCRecompilerImlGen_generateNewInstruction_conditionalJumpSegment(ppcImlGenContext, jumpCondition, crRegister, crBit, !BO.conditionInverted());


		bctrSeg->AppendInstruction()->make_macro(PPCREC_IML_MACRO_B_TO_REG, branchDestReg, 0, 0);
	}
	else
	{
		// branch always, no condition and no decrementer check
		cemu_assert_debug(!ppcImlGenContext->currentBasicBlock->hasContinuedFlow);
		cemu_assert_debug(!ppcImlGenContext->currentBasicBlock->hasBranchTarget);
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_B_TO_REG, branchDestReg, 0, 0);

	}
	return true;
}

bool PPCRecompilerImlGen_ISYNC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// does not need to be translated
	return true;
}

bool PPCRecompilerImlGen_SYNC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// does not need to be translated
	return true;
}

bool PPCRecompilerImlGen_ADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	//hCPU->gpr[rD] = (int)hCPU->gpr[rA] + (int)hCPU->gpr[rB];
	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, registerRD, registerRA, registerRB);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRD);
	return true;
}

bool PPCRecompilerImlGen_ADDC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	//hCPU->gpr[rD] = (int)hCPU->gpr[rA] + (int)hCPU->gpr[rB]; -> Update carry
	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD_UPDATE_CARRY, registerRD, registerRA, registerRB);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRD);
	return true;
}

bool PPCRecompilerImlGen_ADDE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	// hCPU->gpr[rD] = hCPU->gpr[rA] + hCPU->gpr[rB] + ca;
	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD_CARRY_UPDATE_CARRY, registerRD, registerRB, registerRA);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRD);
	return true;
}

bool PPCRecompilerImlGen_ADDZE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	PPC_ASSERT(rB == 0);
	//uint32 a = hCPU->gpr[rA];
	//uint32 ca = hCPU->xer_ca;
	//hCPU->gpr[rD] = a + ca;

	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	// move rA to rD
	if( registerRA != registerRD )
	{
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, registerRD, registerRA);
	}
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ADD_CARRY, registerRD, registerRD);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRD);
	return true;
}

bool PPCRecompilerImlGen_ADDME(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	PPC_ASSERT(rB == 0);
	//uint32 a = hCPU->gpr[rA];
	//uint32 ca = hCPU->xer_ca;
	//hCPU->gpr[rD] = a + ca + -1;

	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	// move rA to rD
	if( registerRA != registerRD )
	{
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, registerRD, registerRA);
	}
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ADD_CARRY_ME, registerRD, registerRD);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRD);
	return true;
}

bool PPCRecompilerImlGen_ADDI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	//hCPU->gpr[rD] = (rA ? (int)hCPU->gpr[rA] : 0) + (int)imm;
	if( rA != 0 )
	{
		uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
		// check if rD is already loaded, else use new temporary register
		uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, registerRD, registerRA, imm);
	}
	else
	{
		// rA not used, instruction is value assignment
		// rD = imm
		uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, registerRD, imm);
	}
	// never updates any cr
	return true;
}

bool PPCRecompilerImlGen_ADDIS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(opcode, rD, rA, imm);
	if( rA != 0 )
	{
		uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
		// check if rD is already loaded, else use new temporary register
		uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, registerRD, registerRA, (sint32)imm);
	}
	else
	{
		// rA not used, instruction turns into simple value assignment
		// rD = imm
		uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, registerRD, (sint32)imm, PPC_REC_INVALID_REGISTER, 0);
	}
	// never updates any cr
	return true;
}

bool PPCRecompilerImlGen_ADDIC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	// rD = rA + imm;
	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD_UPDATE_CARRY, registerRD, registerRA, imm);
	// never updates any cr
	return true;
}

bool PPCRecompilerImlGen_ADDIC_(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// this opcode is identical to ADDIC but additionally it updates CR0
	sint32 rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	// rD = rA + imm;
	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD_UPDATE_CARRY, registerRD, registerRA, imm);
	PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRD);
	return true;
}

bool PPCRecompilerImlGen_SUBF(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	// hCPU->gpr[rD] = ~hCPU->gpr[rA] + hCPU->gpr[rB] + 1;
	// rD = rB - rA
	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_SUB, registerRD, registerRB, registerRA);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRD);
	return true;
}

bool PPCRecompilerImlGen_SUBFE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	// hCPU->gpr[rD] = ~hCPU->gpr[rA] + hCPU->gpr[rB] + ca;
	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_SUB_CARRY_UPDATE_CARRY, registerRD, registerRB, registerRA);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRD);
	return true;
}

bool PPCRecompilerImlGen_SUBFZE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	if( rB != 0 )
		debugBreakpoint();
	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_SUB_CARRY_UPDATE_CARRY, registerRD, registerRA);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRD);
	return true;
}

bool PPCRecompilerImlGen_SUBFC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	// hCPU->gpr[rD] = ~hCPU->gpr[rA] + hCPU->gpr[rB] + 1;
	// rD = rB - rA
	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_SUBFC, registerRD, registerRA, registerRB);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRD);
	return true;
}

bool PPCRecompilerImlGen_SUBFIC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	//uint32 a = hCPU->gpr[rA];
	//hCPU->gpr[rD] = ~a + imm + 1;
	// cr0 is never affected
	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_SUBFC, registerRD, registerRA, imm);
	return true;
}

bool PPCRecompilerImlGen_MULLI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	// mulli instruction does not modify any flags
	uint32 registerResult = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false);
	uint32 registerOperand = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_MULTIPLY_SIGNED, registerResult, registerOperand, (sint32)imm);
	return true;
}

bool PPCRecompilerImlGen_MULLW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	//hCPU->gpr[rD] = hCPU->gpr[rA] * hCPU->gpr[rB];
	uint32 registerResult = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false);
	uint32 registerOperand1 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerOperand2 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	if (opcode & PPC_OPC_OE)
	{
		return false;
	}
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_MULTIPLY_SIGNED, registerResult, registerOperand1, registerOperand2);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerResult);
	return true;
}

bool PPCRecompilerImlGen_MULHW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	//hCPU->gpr[rD] = ((sint64)(sint32)hCPU->gpr[rA] * (sint64)(sint32)hCPU->gpr[rB])>>32;
	uint32 registerResult = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false);
	uint32 registerOperand1 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerOperand2 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED, registerResult, registerOperand1, registerOperand2);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerResult);
	return true;
}

bool PPCRecompilerImlGen_MULHWU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	//hCPU->gpr[rD] = (hCPU->gpr[rA] * hCPU->gpr[rB])>>32;
	uint32 registerResult = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false);
	uint32 registerOperand1 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerOperand2 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_MULTIPLY_HIGH_UNSIGNED, registerResult, registerOperand1, registerOperand2);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerResult);
	return true;
}

bool PPCRecompilerImlGen_DIVW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	// hCPU->gpr[rD] = (sint32)a / (sint32)b;
	uint32 registerResult = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false);
	uint32 registerOperand1 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerOperand2 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_DIVIDE_SIGNED, registerResult, registerOperand1, registerOperand2);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerResult);
	return true;
}

bool PPCRecompilerImlGen_DIVWU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	// hCPU->gpr[rD] = (uint32)a / (uint32)b;
	uint32 registerResult = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false);
	uint32 registerOperand1 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerOperand2 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_DIVIDE_UNSIGNED, registerResult, registerOperand1, registerOperand2);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerResult);
	return true;
}

bool PPCRecompilerImlGen_RLWINM(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	uint32 mask = ppc_mask(MB, ME);

	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	if( ME == (31-SH) && MB == 0 )
	{
		// SLWI
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_LEFT_SHIFT, registerRA, registerRS, SH);
	}
	else if( SH == (32-MB) && ME == 31 )
	{
		// SRWI
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_RIGHT_SHIFT, registerRA, registerRS, MB);
	}
	else
	{
		// general handler
		if (registerRA != registerRS)
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, registerRA, registerRS);
		if (SH != 0)
			ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_LEFT_ROTATE, registerRA, SH);
		if (mask != 0xFFFFFFFF)
			ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_AND, registerRA, (sint32)mask);
	}
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRA);
	return true;
}

bool PPCRecompilerImlGen_RLWIMI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);

	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	// pack RLWIMI parameters into single integer
	uint32 vImm = MB|(ME<<8)|(SH<<16);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_RLWIMI, registerRA, registerRS, (sint32)vImm, PPC_REC_INVALID_REGISTER, 0);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRA);
	return true;
}

bool PPCRecompilerImlGen_RLWNM(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, rB, MB, ME);
	uint32 mask = ppc_mask(MB, ME);
	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_LEFT_ROTATE, registerRA, registerRS, registerRB);
	if( mask != 0xFFFFFFFF )
		ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_AND, registerRA, (sint32)mask);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRA);
	return true;
}

bool PPCRecompilerImlGen_SRAW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_SRAW, registerRA, registerRS, registerRB);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRA);
	return true;
}

bool PPCRecompilerImlGen_SRAWI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA;
	uint32 SH;
	PPC_OPC_TEMPL_X(opcode, rS, rA, SH);
	cemu_assert_debug(SH < 32);
	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_SRAW, registerRA, registerRS, (sint32)SH);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRA);
	return true;
}

bool PPCRecompilerImlGen_SLW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);

	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_SLW, registerRA, registerRS, registerRB);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRA);
	return true;
}

bool PPCRecompilerImlGen_SRW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);

	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_SRW, registerRA, registerRS, registerRB, PPC_REC_INVALID_REGISTER, 0);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRA);
	return true;
}

bool PPCRecompilerImlGen_EXTSH(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	PPC_ASSERT(rB==0);
	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN_S16_TO_S32, registerRA, registerRS);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRA);
	return true;
}

bool PPCRecompilerImlGen_EXTSB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN_S8_TO_S32, registerRA, registerRS);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRA);
	return true;
}

bool PPCRecompilerImlGen_CNTLZW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	PPC_ASSERT(rB==0);
	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_CNTLZW, registerRA, registerRS);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRA);
	return true;
}

bool PPCRecompilerImlGen_NEG(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	PPC_ASSERT(rB == 0);

	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NEG, registerRD, registerRA);
	if (opcode & PPC_OPC_RC)
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, registerRD);
	return true;
}

void PPCRecompilerImlGen_LWZ(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// load half
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, imm, 32, false, true);
}

void PPCRecompilerImlGen_LWZU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// add imm to memory register
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// load half
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, 0, 32, false, true);
}

void PPCRecompilerImlGen_LHA(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new temporary register
	// load half
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, imm, 16, true, true);
}

void PPCRecompilerImlGen_LHAU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// add imm to memory register
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new temporary register
	// load half
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, 0, 16, true, true);
}

void PPCRecompilerImlGen_LHZ(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		// note: Darksiders 2 has this instruction form but it is never executed.
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new temporary register
	// load half
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, imm, 16, false, true);
}

void PPCRecompilerImlGen_LHZU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// add imm to memory register
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new temporary register
	// load half
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, 0, 16, false, true);
}

void PPCRecompilerImlGen_LBZ(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// load byte
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, imm, 8, false, true);
}

void PPCRecompilerImlGen_LBZU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// add imm to memory register
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// load byte
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, 0, 8, false, true);
}

bool PPCRecompilerImlGen_LWZX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	if( rA == 0 )
	{
		return false;
	}
	// hCPU->gpr[rD] = memory_readU8((rA?hCPU->gpr[rA]:0)+hCPU->gpr[rB]);
	// load memory rA and rB into register
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// load word
	PPCRecompilerImlGen_generateNewInstruction_r_memory_indexed(ppcImlGenContext, destinationRegister, gprRegisterA, gprRegisterB, 32, false, true);
	return true;
}

bool PPCRecompilerImlGen_LWZUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	if( rA == 0 )
	{
		return false;
	}
	// load memory rA and rB into register
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// add rB to rA
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterA, gprRegisterB);
	// load word
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegisterA, 0, 32, false, true);
	return true;
}

bool PPCRecompilerImlGen_LWBRX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	// load memory rA and rB into register
	uint32 gprRegisterA = 0;
	if( rA )
		gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rB, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0 + rD);
	if (destinationRegister == PPC_REC_INVALID_REGISTER)
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0 + rD); // else just create new register
	// load word
	if( rA )
		PPCRecompilerImlGen_generateNewInstruction_r_memory_indexed(ppcImlGenContext, destinationRegister, gprRegisterA, gprRegisterB, 32, false, false);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegisterB, 0, 32, false, false);
	return true;
}

bool PPCRecompilerImlGen_LHAX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return true;
	}
	// load memory rA and rB into register
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// load half word
	PPCRecompilerImlGen_generateNewInstruction_r_memory_indexed(ppcImlGenContext, destinationRegister, gprRegisterA, gprRegisterB, 16, true, true);
	return true;
}

bool PPCRecompilerImlGen_LHAUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return true;
	}
	// load memory rA and rB into register
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// add rB to rA
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterA, gprRegisterB);
	// load half word
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegisterA, 0, 16, true, true);
	return true;
}

bool PPCRecompilerImlGen_LHZX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return true;
	}
	// load memory rA and rB into register
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// load half word
	PPCRecompilerImlGen_generateNewInstruction_r_memory_indexed(ppcImlGenContext, destinationRegister, gprRegisterA, gprRegisterB, 16, false, true);
	return true;
}

bool PPCRecompilerImlGen_LHZUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return true;
	}
	// load memory rA and rB into register
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// add rB to rA
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterA, gprRegisterB);
	// load hald word
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegisterA, 0, 16, false, true);
	return true;
}

void PPCRecompilerImlGen_LHBRX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	// load memory rA and rB into register
	uint32 gprRegisterA = rA != 0 ? PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA, false) : 0;
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rB, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0 + rD);
	if (destinationRegister == PPC_REC_INVALID_REGISTER)
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0 + rD); // else just create new register
	// load half word (little-endian)
	if (rA == 0)
		PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegisterB, 0, 16, false, false);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_memory_indexed(ppcImlGenContext, destinationRegister, gprRegisterA, gprRegisterB, 16, false, false);
}

bool PPCRecompilerImlGen_LBZX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	if( rA == 0 )
	{
		// special case where rA is ignored and only rB is used
		return false;
	}
	// hCPU->gpr[rD] = memory_readU8((rA?hCPU->gpr[rA]:0)+hCPU->gpr[rB]);
	// load memory rA and rB into register
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// load byte
	PPCRecompilerImlGen_generateNewInstruction_r_memory_indexed(ppcImlGenContext, destinationRegister, gprRegisterA, gprRegisterB, 8, false, true);
	return true;
}

bool PPCRecompilerImlGen_LBZUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	if (rA == 0)
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return true;
	}
	// load memory rA and rB into register
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rB, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0 + rD);
	if (destinationRegister == PPC_REC_INVALID_REGISTER)
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0 + rD); // else just create new register
	// add rB to rA
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterA, gprRegisterB);
	// load byte
	PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegisterA, 0, 8, false, true);
	return true;
}

bool PPCRecompilerImlGen_LWARX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	// load memory rA and rB into register
	uint32 gprRegisterA = rA != 0?PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false):0;
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	// check if destination register is already loaded
	uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( destinationRegister == PPC_REC_INVALID_REGISTER )
		destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
	// load word
	if( rA != 0 )
		PPCRecompilerImlGen_generateNewInstruction_r_memory_indexed(ppcImlGenContext, destinationRegister, gprRegisterA, gprRegisterB, PPC_REC_LOAD_LWARX_MARKER, false, true);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegisterB, 0, PPC_REC_LOAD_LWARX_MARKER, false, true);
	return true;
}

void PPCRecompilerImlGen_LMW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	//uint32 ea = (rA ? hCPU->gpr[rA] : 0) + imm;
	sint32 index = 0;
	while( rD <= 31 )
	{
		// load memory gpr into register
		uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
		// check if destination register is already loaded
		uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
		if( destinationRegister == PPC_REC_INVALID_REGISTER )
			destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
		// load word
		PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, imm+index*4, 32, false, true);
		// next
		rD++;
		index++;
	}
}

void PPCRecompilerImlGen_STW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		// note: Darksiders 2 has this instruction form but it is never executed.
		//ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// load source register
	uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false); // can be the same as gprRegister
	// store word
	PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, imm, 32, true);
}

void PPCRecompilerImlGen_STWU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// store&update instructions where rD==rA store the register contents without added imm, therefore we need to handle it differently
	// get memory gpr register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// get source register
	uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false); // can be the same as gprRegister
	// add imm to memory register early if possible
	if( rD != rA )
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
	// store word
	PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, (rD==rA)?imm:0, 32, true);
	// add imm to memory register late if we couldn't do it early
	if( rD == rA )
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
}

void PPCRecompilerImlGen_STH(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// load source register
	uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false); // can be the same as gprRegister
	// load half
	PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, imm, 16, true);
}

void PPCRecompilerImlGen_STHU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// get memory gpr register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// get source register
	uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false); // can be the same as gprRegister
	// add imm to memory register early if possible
	if( rD != rA )
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
	// store word
	PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, (rD==rA)?imm:0, 16, true);
	// add imm to memory register late if we couldn't do it early
	if( rD == rA )
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
}

void PPCRecompilerImlGen_STB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rS, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// load source register
	uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false); // can be the same as gprRegister
	// store byte
	PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, imm, 8, true);
}

void PPCRecompilerImlGen_STBU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// get memory gpr register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// get source register
	uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false); // can be the same as gprRegister
	// add imm to memory register early if possible
	if( rD != rA )
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
	// store byte
	PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, (rD==rA)?imm:0, 8, true);
	// add imm to memory register late if we couldn't do it early
	if( rD == rA )
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
}

// generic indexed store (STWX, STHX, STBX, STWUX. If byteReversed == true -> STHBRX)
bool PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, uint32 storeBitWidth, bool byteReversed = false)
{
	sint32 rA, rS, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	// prepare registers
	uint32 gprRegisterA;
	if(rA != 0)
		gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 destinationRegister = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	// store word
	if (rA == 0)
	{
		PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, destinationRegister, gprRegisterB, 0, storeBitWidth, !byteReversed);
	}
	else
		PPCRecompilerImlGen_generateNewInstruction_memory_r_indexed(ppcImlGenContext, destinationRegister, gprRegisterA, gprRegisterB, storeBitWidth, false, !byteReversed);
	return true;
}

bool PPCRecompilerImlGen_STORE_INDEXED_UPDATE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, uint32 storeBitWidth)
{
	sint32 rA, rS, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	if( rA == 0 )
	{
		// not supported
		return false;
	}
	if( rS == rA || rS == rB )
	{
		// prepare registers
		uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
		uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
		uint32 destinationRegister = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
		// store word
		PPCRecompilerImlGen_generateNewInstruction_memory_r_indexed(ppcImlGenContext, destinationRegister, gprRegisterA, gprRegisterB, storeBitWidth, false, true);
		// update EA after store
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterA, gprRegisterB);
		return true;
	}
	// prepare registers
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 sourceRegister = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	// update EA
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterA, gprRegisterB);
	// store word
	PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegisterA, 0, storeBitWidth, true);
	return true;
}

bool PPCRecompilerImlGen_STWCX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rS, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	// prepare registers
	uint32 gprRegisterA = rA!=0?PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false):0;
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 destinationRegister = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	// store word
	if( rA != 0 )
		PPCRecompilerImlGen_generateNewInstruction_memory_r_indexed(ppcImlGenContext, destinationRegister, gprRegisterA, gprRegisterB, PPC_REC_STORE_STWCX_MARKER, false, true);
	else
		PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, destinationRegister, gprRegisterB, 0, PPC_REC_STORE_STWCX_MARKER, true);
	return true;
}

bool PPCRecompilerImlGen_STWBRX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rS, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	// prepare registers
	uint32 gprRegisterA = rA!=0?PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false):0;
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 destinationRegister = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	// store word
	if( rA != 0 )
		PPCRecompilerImlGen_generateNewInstruction_memory_r_indexed(ppcImlGenContext, destinationRegister, gprRegisterA, gprRegisterB, 32, false, false);
	else
		PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, destinationRegister, gprRegisterB, 0, 32, false);
	return true;
}

void PPCRecompilerImlGen_STMW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rS, rA, imm);
	sint32 index = 0;
	while( rS <= 31 )
	{
		// load memory gpr into register
		uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
		// load source register
		uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false); // can be the same as gprRegister
		// store word
		PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, imm+index*4, 32, true);
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

	uint32 memReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA);
	uint32 tmpReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
	uint32 memOffset = 0;
	while (nb > 0)
	{
		if (rD == rA)
			return false;
		cemu_assert(rD < 32);
		uint32 destinationRegister = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0 + rD);
		// load bytes one-by-one
		for (sint32 b = 0; b < 4; b++)
		{
			ppcImlGenContext->emitInst().make_r_memory(tmpReg, memReg, memOffset + b, 8, false, false);
			sint32 shiftAmount = (3 - b) * 8;
			if(shiftAmount)
				ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_LEFT_SHIFT, tmpReg, tmpReg, shiftAmount);
			ppcImlGenContext->emitInst().make_r_r(b == 0 ? PPCREC_IML_OP_ASSIGN : PPCREC_IML_OP_OR, destinationRegister, tmpReg);
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

	uint32 memReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA);
	uint32 tmpReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
	uint32 memOffset = 0;
	while (nb > 0)
	{
		if (rS == rA)
			return false;
		cemu_assert(rS < 32);
		uint32 dataRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rS);
		// store bytes one-by-one
		for (sint32 b = 0; b < 4; b++)
		{
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, tmpReg, dataRegister);
			sint32 shiftAmount = (3 - b) * 8;
			if (shiftAmount)
				ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_RIGHT_SHIFT, tmpReg, tmpReg, shiftAmount);
			ppcImlGenContext->emitInst().make_memory_r(tmpReg, memReg, memOffset + b, 8, false);
			nb--;
			if (nb == 0)
				break;
		}
		memOffset += 4;
		rS++;
	}
	return true;
}

bool PPCRecompilerImlGen_DCBZ(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, rB;
	rA = (opcode>>16)&0x1F;
	rB = (opcode>>11)&0x1F;
	// prepare registers
	uint32 gprRegisterA = rA!=0?PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false):0;
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	// store
	if( rA != 0 )
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_DCBZ, gprRegisterA, gprRegisterB);
	else
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_DCBZ, gprRegisterB, gprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_OR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	// check for MR mnemonic
	if( rS == rB )
	{
		// simple register copy
		if( rA != rS ) // check if no-op
		{
			sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
			sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
		}
		if ((opcode & PPC_OPC_RC))
		{
			sint32 gprDestReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA);
			PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
		}
	}
	else
	{
		// rA = rS | rA
		sint32 gprSource1Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
		sint32 gprSource2Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
		sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		if( gprSource1Reg == gprDestReg || gprSource2Reg == gprDestReg )
		{
			// make sure we don't overwrite rS or rA
			if( gprSource1Reg == gprDestReg )
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_OR, gprDestReg, gprSource2Reg);
			else
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_OR, gprDestReg, gprSource1Reg);
		}
		else
		{
			// rA = rS
			if( gprDestReg != gprSource1Reg )
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSource1Reg);
			// rA |= rB
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_OR, gprDestReg, gprSource2Reg);
		}
		if ((opcode & PPC_OPC_RC))
			PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
	}
	return true;
}

bool PPCRecompilerImlGen_NOR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	//hCPU->gpr[rA] = ~(hCPU->gpr[rS] | hCPU->gpr[rB]);
	// check for NOT mnemonic
	if (rS == rB)
	{
		// simple register copy with NOT
		sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rS);
		sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA);
		if (gprDestReg != gprSourceReg)
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
		if ((opcode & PPC_OPC_RC))
			PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
	}
	else
	{
		// rA = rS | rA
		sint32 gprSource1Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rS);
		sint32 gprSource2Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rB);
		sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA);
		if (gprSource1Reg == gprDestReg || gprSource2Reg == gprDestReg)
		{
			// make sure we don't overwrite rS or rA
			if (gprSource1Reg == gprDestReg)
			{
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_OR, gprDestReg, gprSource2Reg);
			}
			else
			{
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_OR, gprDestReg, gprSource1Reg);
			}
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
			if ((opcode & PPC_OPC_RC))
				PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
		}
		else
		{
			// rA = rS
			if (gprDestReg != gprSource1Reg)
			{
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSource1Reg);
			}
			// rA |= rB
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_OR, gprDestReg, gprSource2Reg);
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
			if ((opcode & PPC_OPC_RC))
				PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
		}
	}
	return true;
}

bool PPCRecompilerImlGen_ORC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	// hCPU->gpr[rA] = hCPU->gpr[rS] | ~hCPU->gpr[rB];
	sint32 gprSource1Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	sint32 gprSource2Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
	sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ORC, gprDestReg, gprSource1Reg, gprSource2Reg);
	if ((opcode & PPC_OPC_RC))
		PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
	return true;
}

bool PPCRecompilerImlGen_AND(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	// check for MR mnemonic
	if( rS == rB )
	{
		// simple register copy
		if( rA != rS ) // check if no-op
		{
			sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
			sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
			if ((opcode & PPC_OPC_RC))
				PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
		}
		else
		{
			cemu_assert_unimplemented(); // no-op -> verify this case
		}
	}
	else
	{
		// rA = rS & rA
		sint32 gprSource1Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
		sint32 gprSource2Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
		sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		if( gprSource1Reg == gprDestReg || gprSource2Reg == gprDestReg )
		{
			// make sure we don't overwrite rS or rA
			if( gprSource1Reg == gprDestReg )
			{
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_AND, gprDestReg, gprSource2Reg);
			}
			else
			{
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_AND, gprDestReg, gprSource1Reg);
			}
			if ((opcode & PPC_OPC_RC))
				PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
		}
		else
		{
			// rA = rS
			if( gprDestReg != gprSource1Reg )
			{
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSource1Reg);
			}
			// rA &= rB
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_AND, gprDestReg, gprSource2Reg);
			if ((opcode & PPC_OPC_RC))
				PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
		}
	}
	return true;
}

bool PPCRecompilerImlGen_ANDC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	//hCPU->gpr[rA] = hCPU->gpr[rS] & ~hCPU->gpr[rB];
	//if (Opcode & PPC_OPC_RC) {
	if( rS == rB )
	{
		// result is always 0 -> replace with XOR rA,rA
		sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_XOR, gprDestReg, gprDestReg);
		if ((opcode & PPC_OPC_RC))
			PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
	}
	else if( rA == rB )
	{
		// rB already in rA, therefore we complement rA first and then AND it with rS
		sint32 gprRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
		sint32 gprDestReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		// rA = ~rA
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
		// rA &= rS
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_AND, gprDestReg, gprRS);
		if ((opcode & PPC_OPC_RC))
			PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
	}
	else
	{
		// a & (~b) is the same as ~((~a) | b)
		sint32 gprDestReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		sint32 gprRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
		sint32 gprRS = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
		// move rS to rA (if required)
		if( gprDestReg != gprRS )
		{
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprRS);
		}
		// rS already in rA, therefore we complement rS first and then OR it with rB
		// rA = ~rA
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
		// rA |= rB
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_OR, gprDestReg, gprRB);
		// rA = ~rA
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
		if ((opcode & PPC_OPC_RC))
			PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
	}
	return true;
}

void PPCRecompilerImlGen_ANDI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(opcode, rS, rA, imm);
	sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	// rA = rS
	if( gprDestReg != gprSourceReg )
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA &= imm32
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_AND, gprDestReg, (sint32)imm);
	// ANDI. always sets cr0
	PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
}

void PPCRecompilerImlGen_ANDIS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(opcode, rS, rA, imm);
	sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	// rA = rS
	if( gprDestReg != gprSourceReg )
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA &= imm32
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_AND, gprDestReg, (sint32)imm);
	// ANDIS. always sets cr0
	PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
}

bool PPCRecompilerImlGen_XOR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	if( rS == rB )
	{
		// xor register with itself
		sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_XOR, gprDestReg, gprDestReg);
		if ((opcode & PPC_OPC_RC))
			PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
	}
	else
	{
		// rA = rS ^ rA
		sint32 gprSource1Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
		sint32 gprSource2Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
		sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		if( gprSource1Reg == gprDestReg || gprSource2Reg == gprDestReg )
		{
			// make sure we don't overwrite rS or rA
			if( gprSource1Reg == gprDestReg )
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_XOR, gprDestReg, gprSource2Reg);
			else
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_XOR, gprDestReg, gprSource1Reg);
			if ((opcode & PPC_OPC_RC))
				PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
		}
		else
		{
			// rA = rS
			if( gprDestReg != gprSource1Reg )
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSource1Reg);
			// rA ^= rB
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_XOR, gprDestReg, gprSource2Reg);
			if ((opcode & PPC_OPC_RC))
				PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
		}
	}
	return true;
}


bool PPCRecompilerImlGen_EQV(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	if( rS == rB )
	{
		// xor register with itself, then invert
		sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_XOR, gprDestReg, gprDestReg);
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
		if ((opcode & PPC_OPC_RC))
			PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
	}
	else
	{
		// rA = ~(rS ^ rA)
		sint32 gprSource1Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
		sint32 gprSource2Reg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
		sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		if( gprSource1Reg == gprDestReg || gprSource2Reg == gprDestReg )
		{
			// make sure we don't overwrite rS or rA
			if( gprSource1Reg == gprDestReg )
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_XOR, gprDestReg, gprSource2Reg);
			else
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_XOR, gprDestReg, gprSource1Reg);
		}
		else
		{
			// rA = rS
			if( gprDestReg != gprSource1Reg )
			{
				ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSource1Reg);
			}
			// rA ^= rB
			ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_XOR, gprDestReg, gprSource2Reg);
		}
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
		if ((opcode & PPC_OPC_RC))
			PPCImlGen_UpdateCR0Logical(ppcImlGenContext, gprDestReg);
	}
	return true;
}

void PPCRecompilerImlGen_ORI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(opcode, rS, rA, imm);
	// ORI does not set cr0 flags
	//hCPU->gpr[rA] = hCPU->gpr[rS] | imm;
	sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	// rA = rS
	if( gprDestReg != gprSourceReg )
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA |= imm32
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_OR, gprDestReg, (sint32)imm);
}

void PPCRecompilerImlGen_ORIS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(opcode, rS, rA, imm);
	// ORI does not set cr0 flags
	//hCPU->gpr[rA] = hCPU->gpr[rS] | imm;
	sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	// rA = rS
	if( gprDestReg != gprSourceReg )
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA |= imm32
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_OR, gprDestReg, (sint32)imm);
}

void PPCRecompilerImlGen_XORI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(opcode, rS, rA, imm);
	//hCPU->gpr[rA] = hCPU->gpr[rS] ^ imm;
	// XORI does not set cr0 flags
	sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	// rA = rS
	if( gprDestReg != gprSourceReg )
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA |= imm32
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_XOR, gprDestReg, (sint32)imm);
}

void PPCRecompilerImlGen_XORIS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(opcode, rS, rA, imm);
	//hCPU->gpr[rA] = hCPU->gpr[rS] ^ imm;
	// XORIS does not set cr0 flags
	sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	// rA = rS
	if( gprDestReg != gprSourceReg )
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA |= imm32
	ppcImlGenContext->emitInst().make_r_s32(PPCREC_IML_OP_XOR, gprDestReg, (sint32)imm);
}

bool PPCRecompilerImlGen_CROR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	PPCRecompilerImlGen_generateNewInstruction_cr(ppcImlGenContext, PPCREC_IML_OP_CR_OR, crD, crA, crB);
	return true;
}

bool PPCRecompilerImlGen_CRORC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	PPCRecompilerImlGen_generateNewInstruction_cr(ppcImlGenContext, PPCREC_IML_OP_CR_ORC, crD, crA, crB);
	return true;
}

bool PPCRecompilerImlGen_CRAND(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	PPCRecompilerImlGen_generateNewInstruction_cr(ppcImlGenContext, PPCREC_IML_OP_CR_AND, crD, crA, crB);
	return true;
}

bool PPCRecompilerImlGen_CRANDC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	PPCRecompilerImlGen_generateNewInstruction_cr(ppcImlGenContext, PPCREC_IML_OP_CR_ANDC, crD, crA, crB);
	return true;
}

bool PPCRecompilerImlGen_CRXOR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	if (crA == crB)
	{
		// both operands equal, clear bit in crD
		// PPC's assert() uses this to pass a parameter to OSPanic
		PPCRecompilerImlGen_generateNewInstruction_cr(ppcImlGenContext, PPCREC_IML_OP_CR_CLEAR, crD, 0, 0);
		return true;
	}
	else
	{
		return false;
	}
	return true;
}

bool PPCRecompilerImlGen_CREQV(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(opcode, crD, crA, crB);
	if (crA == crB)
	{
		// both operands equal, set bit in crD
		PPCRecompilerImlGen_generateNewInstruction_cr(ppcImlGenContext, PPCREC_IML_OP_CR_SET, crD, 0, 0);
		return true;
	}
	else
	{
		return false;
	}
	return true;
}

bool PPCRecompilerImlGen_HLE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 hleFuncId = opcode&0xFFFF;
	ppcImlGenContext->emitInst().make_macro(PPCREC_IML_MACRO_HLE, ppcImlGenContext->ppcAddressOfCurrentInstruction, hleFuncId, 0);
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
	segmentPoint->index = index;
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
* Warning: Can invalidate any previous instruction structs from the same segment
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
			if (segmentPoint->index != RA_INTER_RANGE_START && segmentPoint->index != RA_INTER_RANGE_END)
			{
				if (segmentPoint->index >= index)
					segmentPoint->index += shiftBackCount;
			}
			// next
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
				PPCRecompilerImlGen_PS_CMPU0(ppcImlGenContext, opcode);
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 1:
				PPCRecompilerImlGen_PS_CMPO0(ppcImlGenContext, opcode);
				ppcImlGenContext->hasFPUInstruction = true;
				break;
			case 2:
				PPCRecompilerImlGen_PS_CMPU1(ppcImlGenContext, opcode);
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
		PPCRecompilerImlGen_SUBFIC(ppcImlGenContext, opcode);
		break;
	case 10: // CMPLI
		PPCRecompilerImlGen_CMPLI(ppcImlGenContext, opcode);
		break;
	case 11: // CMPI
		PPCRecompilerImlGen_CMPI(ppcImlGenContext, opcode);
		break;
	case 12: // ADDIC
		if (PPCRecompilerImlGen_ADDIC(ppcImlGenContext, opcode) == false)
			unsupportedInstructionFound = true;
		break;
	case 13: // ADDIC.
		if (PPCRecompilerImlGen_ADDIC_(ppcImlGenContext, opcode) == false)
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
	case 24:
		PPCRecompilerImlGen_ORI(ppcImlGenContext, opcode);
		break;
	case 25:
		PPCRecompilerImlGen_ORIS(ppcImlGenContext, opcode);
		break;
	case 26:
		PPCRecompilerImlGen_XORI(ppcImlGenContext, opcode);
		break;
	case 27:
		PPCRecompilerImlGen_XORIS(ppcImlGenContext, opcode);
		break;
	case 28:
		PPCRecompilerImlGen_ANDI(ppcImlGenContext, opcode);
		break;
	case 29:
		PPCRecompilerImlGen_ANDIS(ppcImlGenContext, opcode);
		break;
	case 31: // opcode category
		switch (PPC_getBits(opcode, 30, 10))
		{
		case 0:
			PPCRecompilerImlGen_CMP(ppcImlGenContext, opcode);
			break;
		case 4:
			PPCRecompilerImlGen_TW(ppcImlGenContext, opcode);
			break;
		case 8:
		// todo: Check if we can optimize this pattern:
		// SUBFC + SUBFE 			
		// SUBFC
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
		case 23:
			if (PPCRecompilerImlGen_LWZX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 24:
			if (PPCRecompilerImlGen_SLW(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 26:
			if (PPCRecompilerImlGen_CNTLZW(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 28:
			if (PPCRecompilerImlGen_AND(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 32:
			PPCRecompilerImlGen_CMPL(ppcImlGenContext, opcode);
			break;
		case 40:
			if (PPCRecompilerImlGen_SUBF(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 54:
			// DBCST - Generates no code
			break;
		case 55:
			if (PPCRecompilerImlGen_LWZUX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 60:
			if (PPCRecompilerImlGen_ANDC(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 75:
			if (PPCRecompilerImlGen_MULHW(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 86:
			// DCBF -> No-Op
			break;
		case 87:
			if (PPCRecompilerImlGen_LBZX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 104:
			if (PPCRecompilerImlGen_NEG(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 119:
			if (PPCRecompilerImlGen_LBZUX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 124:
			if (PPCRecompilerImlGen_NOR(ppcImlGenContext, opcode) == false)
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
			PPCRecompilerImlGen_MTCRF(ppcImlGenContext, opcode);
			break;
		case 150:
			if (PPCRecompilerImlGen_STWCX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 151:
			if (PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 32) == false)
				unsupportedInstructionFound = true;
			break;
		case 183:
			if (PPCRecompilerImlGen_STORE_INDEXED_UPDATE(ppcImlGenContext, opcode, 32) == false)
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
		case 215:
			if (PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 8) == false)
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
		case 247:
			if (PPCRecompilerImlGen_STORE_INDEXED_UPDATE(ppcImlGenContext, opcode, 8) == false)
				unsupportedInstructionFound = true;
			break;
		case 266:
			if (PPCRecompilerImlGen_ADD(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 279:
			if (PPCRecompilerImlGen_LHZX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 284:
			PPCRecompilerImlGen_EQV(ppcImlGenContext, opcode);
			break;
		case 311:
			if (PPCRecompilerImlGen_LHZUX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 316:
			if (PPCRecompilerImlGen_XOR(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 339:
			if (PPCRecompilerImlGen_MFSPR(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 343:
			if (PPCRecompilerImlGen_LHAX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 371:
			if (PPCRecompilerImlGen_MFTB(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 375:
			if (PPCRecompilerImlGen_LHAUX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 407:
			if (PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 16) == false)
				unsupportedInstructionFound = true;
			break;
		case 412:
			if (PPCRecompilerImlGen_ORC(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 439:
			if (PPCRecompilerImlGen_STORE_INDEXED_UPDATE(ppcImlGenContext, opcode, 16) == false)
				unsupportedInstructionFound = true;
			break;
		case 444:
			if (PPCRecompilerImlGen_OR(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 459:
			PPCRecompilerImlGen_DIVWU(ppcImlGenContext, opcode);
			break;
		case 467:
			if (PPCRecompilerImlGen_MTSPR(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 491:
			if (PPCRecompilerImlGen_DIVW(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			break;
		case 534:
			if (PPCRecompilerImlGen_LWBRX(ppcImlGenContext, opcode) == false)
				unsupportedInstructionFound = true;
			ppcImlGenContext->hasFPUInstruction = true;
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
		case 662:
			if (PPCRecompilerImlGen_STWBRX(ppcImlGenContext, opcode) == false)
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
		case 790:
			PPCRecompilerImlGen_LHBRX(ppcImlGenContext, opcode);
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
			if (PPCRecompilerImlGen_STORE_INDEXED(ppcImlGenContext, opcode, 16, true) == false)
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
	case 32:
		PPCRecompilerImlGen_LWZ(ppcImlGenContext, opcode);
		break;
	case 33:
		PPCRecompilerImlGen_LWZU(ppcImlGenContext, opcode);
		break;
	case 34:
		PPCRecompilerImlGen_LBZ(ppcImlGenContext, opcode);
		break;
	case 35:
		PPCRecompilerImlGen_LBZU(ppcImlGenContext, opcode);
		break;
	case 36:
		PPCRecompilerImlGen_STW(ppcImlGenContext, opcode);
		break;
	case 37:
		PPCRecompilerImlGen_STWU(ppcImlGenContext, opcode);
		break;
	case 38:
		PPCRecompilerImlGen_STB(ppcImlGenContext, opcode);
		break;
	case 39:
		PPCRecompilerImlGen_STBU(ppcImlGenContext, opcode);
		break;
	case 40:
		PPCRecompilerImlGen_LHZ(ppcImlGenContext, opcode);
		break;
	case 41:
		PPCRecompilerImlGen_LHZU(ppcImlGenContext, opcode);
		break;
	case 42:
		PPCRecompilerImlGen_LHA(ppcImlGenContext, opcode);
		break;
	case 43:
		PPCRecompilerImlGen_LHAU(ppcImlGenContext, opcode);
		break;
	case 44:
		PPCRecompilerImlGen_STH(ppcImlGenContext, opcode);
		break;
	case 45:
		PPCRecompilerImlGen_STHU(ppcImlGenContext, opcode);
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
			debug_printf("Recompiler encountered unsupported instruction at 0x%08x\n", addressOfCurrentInstruction);
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

void PPCRecompiler_HandleCycleCheckCount(ppcImlGenContext_t& ppcImlGenContext, PPCBasicBlockInfo& basicBlockInfo)
{
	IMLSegment* imlSegment = basicBlockInfo.GetFirstSegmentInChain();
	if (!basicBlockInfo.hasBranchTarget)
		return;
	if (basicBlockInfo.branchTarget > basicBlockInfo.startAddress)
		return;

	// exclude non-infinite tight loops
	if (IMLAnalyzer_IsTightFiniteLoop(imlSegment))
		return;

	// make the segment enterable so execution can return after passing a check
	basicBlockInfo.GetFirstSegmentInChain()->SetEnterable(basicBlockInfo.startAddress);

	IMLSegment* splitSeg = PPCIMLGen_CreateSplitSegmentAtEnd(ppcImlGenContext, basicBlockInfo);
	splitSeg->AppendInstruction()->make_cjump_cycle_check();

	IMLSegment* exitSegment = ppcImlGenContext.NewSegment();
	splitSeg->SetLinkBranchTaken(exitSegment);

	exitSegment->AppendInstruction()->make_macro(PPCREC_IML_MACRO_LEAVE, basicBlockInfo.startAddress, 0, 0);
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
		if (imlInstruction->type == PPCREC_IML_TYPE_CJUMP || imlInstruction->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
		{
			cemu_assert_debug(segIt->GetBranchTaken());
			if (imlInstruction->op_conditionalJump.condition != PPCREC_JUMP_CONDITION_NONE)
			{
				cemu_assert_debug(segIt->GetBranchNotTaken());
			}
		}
		else if (imlInstruction->type == PPCREC_IML_TYPE_MACRO)
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
				case PPCREC_IML_MACRO_MFTB:
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
		seg->imlList[0].crRegister = PPC_REC_INVALID_REGISTER;
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
					__debugbreak();
				}
			}
		}
		if (seg->nextSegmentBranchTaken)
		{
			if (!seg->HasSuffixInstruction())
			{
				debug_printf("---------------- SegmentDump (NoSuffixInstruction in segment 0x%x):\n", (int)segIndex);
				IMLDebug_Dump(&ppcImlGenContext);
				__debugbreak();
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
			if (inst->type == PPCREC_IML_TYPE_CJUMP)
			{
				if (inst->op_conditionalJump.condition != PPCREC_JUMP_CONDITION_NONE)
				{
					if (!seg->GetBranchTaken() || !seg->GetBranchNotTaken())
					{
						debug_printf("---------------- SegmentDump (Missing branch for CJUMP in segment 0x%x):\n", (int)segIndex);
						IMLDebug_Dump(&ppcImlGenContext);
						cemu_assert_error();
					}
				}
				else
				{
					// proper error checking for branch-always (or branch-never if invert bit is set)
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

bool PPCRecompiler_generateIntermediateCode(ppcImlGenContext_t& ppcImlGenContext, PPCRecFunction_t* ppcRecFunc, std::set<uint32>& entryAddresses, PPCFunctionBoundaryTracker& boundaryTracker)
{
	ppcImlGenContext.boundaryTracker = &boundaryTracker;
	if (!PPCRecompiler_GenerateIML(ppcImlGenContext, boundaryTracker, entryAddresses))
		return false;

	// set range
	// todo - support non-continuous functions for the range tracking?
	ppcRecRange_t recRange;
	recRange.ppcAddress = ppcRecFunc->ppcAddress;
	recRange.ppcSize = ppcRecFunc->ppcSize;
	ppcRecFunc->list_ranges.push_back(recRange);

	// optimization pass - replace segments with conditional MOVs if possible
	for (IMLSegment* segIt : ppcImlGenContext.segmentList2)
	{
		if (segIt->nextSegmentBranchNotTaken == nullptr || segIt->nextSegmentBranchTaken == nullptr)
			continue; // not a branching segment
		IMLInstruction* lastInstruction = segIt->GetLastInstruction();
		if (lastInstruction->type != PPCREC_IML_TYPE_CJUMP || lastInstruction->op_conditionalJump.crRegisterIndex != 0)
			continue;
		IMLSegment* conditionalSegment = segIt->nextSegmentBranchNotTaken;
		IMLSegment* finalSegment = segIt->nextSegmentBranchTaken;
		if (segIt->nextSegmentBranchTaken != segIt->nextSegmentBranchNotTaken->nextSegmentBranchNotTaken)
			continue;
		if (segIt->nextSegmentBranchNotTaken->imlList.size() > 4)
			continue;
		if (conditionalSegment->list_prevSegments.size() != 1)
			continue; // the reduced segment must not be the target of any other branch
		if (conditionalSegment->isEnterable)
			continue;
		// check if the segment contains only iml instructions that can be turned into conditional moves (Value assignment, register assignment)
		bool canReduceSegment = true;
		for (sint32 f = 0; f < conditionalSegment->imlList.size(); f++)
		{
			IMLInstruction* imlInstruction = conditionalSegment->imlList.data() + f;
			if( imlInstruction->type == PPCREC_IML_TYPE_R_S32 && imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
				continue;
			// todo: Register to register copy
			canReduceSegment = false;
			break;
		}

		if( canReduceSegment == false )
			continue;
		
		// remove the branch instruction
		uint8 branchCond_crRegisterIndex = lastInstruction->op_conditionalJump.crRegisterIndex;
		uint8 branchCond_crBitIndex = lastInstruction->op_conditionalJump.crBitIndex;
		bool  branchCond_bitMustBeSet = lastInstruction->op_conditionalJump.bitMustBeSet;
		lastInstruction->make_no_op();

		// append conditional moves based on branch condition
		for (sint32 f = 0; f < conditionalSegment->imlList.size(); f++)
		{
			IMLInstruction* imlInstruction = conditionalSegment->imlList.data() + f;
			if (imlInstruction->type == PPCREC_IML_TYPE_R_S32 && imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
				PPCRecompilerImlGen_generateNewInstruction_conditional_r_s32(&ppcImlGenContext, PPCRecompiler_appendInstruction(segIt), PPCREC_IML_OP_ASSIGN, imlInstruction->op_r_immS32.registerIndex, imlInstruction->op_r_immS32.immS32, branchCond_crRegisterIndex, branchCond_crBitIndex, !branchCond_bitMustBeSet);
			else
				assert_dbg();
		}
		// update segment links
		// source segment: imlSegment, conditional/removed segment: conditionalSegment, final segment: finalSegment
		IMLSegment_RemoveLink(segIt, conditionalSegment);
		IMLSegment_RemoveLink(segIt, finalSegment);
		IMLSegment_RemoveLink(conditionalSegment, finalSegment);
		IMLSegment_SetLinkBranchNotTaken(segIt, finalSegment);
		// remove all instructions from conditional segment
		conditionalSegment->imlList.clear();

		// if possible, merge imlSegment with finalSegment
		if (finalSegment->isEnterable == false && finalSegment->list_prevSegments.size() == 1)
		{
			// todo: Clean this up and move into separate function PPCRecompilerIML_mergeSegments()
			IMLSegment_RemoveLink(segIt, finalSegment);
			if (finalSegment->nextSegmentBranchNotTaken)
			{
				IMLSegment* tempSegment = finalSegment->nextSegmentBranchNotTaken;
				IMLSegment_RemoveLink(finalSegment, tempSegment);
				IMLSegment_SetLinkBranchNotTaken(segIt, tempSegment);
			}
			if (finalSegment->nextSegmentBranchTaken)
			{
				IMLSegment* tempSegment = finalSegment->nextSegmentBranchTaken;
				IMLSegment_RemoveLink(finalSegment, tempSegment);
				IMLSegment_SetLinkBranchTaken(segIt, tempSegment);
			}
			// copy IML instructions
			cemu_assert_debug(segIt != finalSegment);
			for (sint32 f = 0; f < finalSegment->imlList.size(); f++)
			{
				memcpy(PPCRecompiler_appendInstruction(segIt), finalSegment->imlList.data() + f, sizeof(IMLInstruction));
			}
			finalSegment->imlList.clear();
		}

		// todo: If possible, merge with the segment following conditionalSegment (merging is only possible if the segment is not an entry point or has no other jump sources)
	}

	// insert cycle counter instruction in every segment that has a cycle count greater zero
	//for (IMLSegment* segIt : ppcImlGenContext.segmentList2)
	//{
	//	if( segIt->ppcAddrMin == 0 )
	//		continue;
	//	// count number of PPC instructions in segment
	//	// note: This algorithm correctly counts inlined functions but it doesn't count NO-OP instructions like ISYNC since they generate no IML instructions
	//	uint32 lastPPCInstAddr = 0;
	//	uint32 ppcCount2 = 0;
	//	for (sint32 i = 0; i < segIt->imlList.size(); i++)
	//	{
	//		if (segIt->imlList[i].associatedPPCAddress == 0)
	//			continue;
	//		if (segIt->imlList[i].associatedPPCAddress == lastPPCInstAddr)
	//			continue;
	//		lastPPCInstAddr = segIt->imlList[i].associatedPPCAddress;
	//		ppcCount2++;
	//	}
	//	//uint32 ppcCount = imlSegment->ppcAddrMax-imlSegment->ppcAddrMin+4; -> No longer works with inlined functions
	//	uint32 cycleCount = ppcCount2;// ppcCount / 4;
	//	if( cycleCount > 0 )
	//	{
	//		PPCRecompiler_pushBackIMLInstructions(segIt, 0, 1);
	//		segIt->imlList[0].type = PPCREC_IML_TYPE_MACRO;
	//		segIt->imlList[0].crRegister = PPC_REC_INVALID_REGISTER;
	//		segIt->imlList[0].operation = PPCREC_IML_MACRO_COUNT_CYCLES;
	//		segIt->imlList[0].op_macro.param = cycleCount;
	//	}
	//}
	return true;
}

void PPCRecompiler_FixLoops(ppcImlGenContext_t& ppcImlGenContext)
{
	return; // deprecated

	//// find segments that have a (conditional) jump instruction that points in reverse direction of code flow
	//// for these segments there is a risk that the recompiler could get trapped in an infinite busy loop. 
	//// todo: We should do a loop-detection prepass where we flag segments that are actually in a loop. We can then use this information below to avoid generating the scheduler-exit code for segments that aren't actually in a loop despite them referencing an earlier segment (which could be an exit segment for example)	
	//uint32 currentLoopEscapeJumpMarker = 0xFF000000; // start in an area where no valid code can be located
	//for (size_t s = 0; s < ppcImlGenContext.segmentList2.size(); s++)
	//{
	//	// todo: This currently uses segment->ppcAddrMin which isn't really reliable. (We already had a problem where function inlining would generate falsified segment ranges by omitting the branch instruction). Find a better solution (use jumpmark/enterable offsets?)
	//	IMLSegment* imlSegment = ppcImlGenContext.segmentList2[s];
	//	if (imlSegment->imlList.empty())
	//		continue;
	//	if (imlSegment->imlList[imlSegment->imlList.size() - 1].type != PPCREC_IML_TYPE_CJUMP || imlSegment->imlList[imlSegment->imlList.size() - 1].op_conditionalJump.jumpmarkAddress > imlSegment->ppcAddrMin)
	//		continue;
	//	if (imlSegment->imlList[imlSegment->imlList.size() - 1].type != PPCREC_IML_TYPE_CJUMP || imlSegment->imlList[imlSegment->imlList.size() - 1].op_conditionalJump.jumpAccordingToSegment)
	//		continue;

	//	// exclude non-infinite tight loops
	//	if (IMLAnalyzer_IsTightFiniteLoop(imlSegment))
	//		continue;
	//	// potential loop segment found, split this segment into four:
	//	// P0: This segment checks if the remaining cycles counter is still above zero. If yes, it jumps to segment P2 (it's also the jump destination for other segments)
	//	// P1: This segment consists only of a single ppc_leave instruction and is usually skipped. Register unload instructions are later inserted here.
	//	// P2: This segment contains the iml instructions of the original segment
	//	// PEntry: This segment is used to enter the function, it jumps to P0
	//	// All segments are considered to be part of the same PPC instruction range
	//	// The first segment also retains the jump destination and enterable properties from the original segment.
	//	//debug_printf("--- Insert cycle counter check ---\n");

	//	PPCRecompilerIml_insertSegments(&ppcImlGenContext, s, 2);
	//	imlSegment = NULL;
	//	IMLSegment* imlSegmentP0 = ppcImlGenContext.segmentList2[s + 0];
	//	IMLSegment* imlSegmentP1 = ppcImlGenContext.segmentList2[s + 1];
	//	IMLSegment* imlSegmentP2 = ppcImlGenContext.segmentList2[s + 2];
	//	// create entry point segment
	//	PPCRecompilerIml_insertSegments(&ppcImlGenContext, ppcImlGenContext.segmentList2.size(), 1);
	//	IMLSegment* imlSegmentPEntry = ppcImlGenContext.segmentList2[ppcImlGenContext.segmentList2.size() - 1];
	//	// relink segments	
	//	IMLSegment_RelinkInputSegment(imlSegmentP2, imlSegmentP0);
	//	IMLSegment_SetLinkBranchNotTaken(imlSegmentP0, imlSegmentP1);
	//	IMLSegment_SetLinkBranchTaken(imlSegmentP0, imlSegmentP2);
	//	IMLSegment_SetLinkBranchTaken(imlSegmentPEntry, imlSegmentP0);
	//	// update segments
	//	uint32 enterPPCAddress = imlSegmentP2->ppcAddrMin;
	//	if (imlSegmentP2->isEnterable)
	//		enterPPCAddress = imlSegmentP2->enterPPCAddress;
	//	imlSegmentP0->ppcAddress = 0xFFFFFFFF;
	//	imlSegmentP1->ppcAddress = 0xFFFFFFFF;
	//	imlSegmentP2->ppcAddress = 0xFFFFFFFF;
	//	cemu_assert_debug(imlSegmentP2->ppcAddrMin != 0);
	//	// move segment properties from segment P2 to segment P0
	//	imlSegmentP0->isJumpDestination = imlSegmentP2->isJumpDestination;
	//	imlSegmentP0->jumpDestinationPPCAddress = imlSegmentP2->jumpDestinationPPCAddress;
	//	imlSegmentP0->isEnterable = false;
	//	//imlSegmentP0->enterPPCAddress = imlSegmentP2->enterPPCAddress;
	//	imlSegmentP0->ppcAddrMin = imlSegmentP2->ppcAddrMin;
	//	imlSegmentP0->ppcAddrMax = imlSegmentP2->ppcAddrMax;
	//	imlSegmentP2->isJumpDestination = false;
	//	imlSegmentP2->jumpDestinationPPCAddress = 0;
	//	imlSegmentP2->isEnterable = false;
	//	imlSegmentP2->enterPPCAddress = 0;
	//	imlSegmentP2->ppcAddrMin = 0;
	//	imlSegmentP2->ppcAddrMax = 0;
	//	// setup enterable segment
	//	if (enterPPCAddress != 0 && enterPPCAddress != 0xFFFFFFFF)
	//	{
	//		imlSegmentPEntry->isEnterable = true;
	//		imlSegmentPEntry->ppcAddress = enterPPCAddress;
	//		imlSegmentPEntry->enterPPCAddress = enterPPCAddress;
	//	}
	//	// assign new jumpmark to segment P2
	//	imlSegmentP2->isJumpDestination = true;
	//	imlSegmentP2->jumpDestinationPPCAddress = currentLoopEscapeJumpMarker;
	//	currentLoopEscapeJumpMarker++;
	//	// create ppc_leave instruction in segment P1
	//	PPCRecompiler_pushBackIMLInstructions(imlSegmentP1, 0, 1);
	//	imlSegmentP1->imlList[0].type = PPCREC_IML_TYPE_MACRO;
	//	imlSegmentP1->imlList[0].operation = PPCREC_IML_MACRO_LEAVE;
	//	imlSegmentP1->imlList[0].crRegister = PPC_REC_INVALID_REGISTER;
	//	imlSegmentP1->imlList[0].op_macro.param = imlSegmentP0->ppcAddrMin;
	//	imlSegmentP1->imlList[0].associatedPPCAddress = imlSegmentP0->ppcAddrMin;
	//	// create cycle-based conditional instruction in segment P0
	//	PPCRecompiler_pushBackIMLInstructions(imlSegmentP0, 0, 1);
	//	imlSegmentP0->imlList[0].type = PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK;
	//	imlSegmentP0->imlList[0].operation = 0;
	//	imlSegmentP0->imlList[0].crRegister = PPC_REC_INVALID_REGISTER;
	//	imlSegmentP0->imlList[0].op_conditionalJump.jumpmarkAddress = imlSegmentP2->jumpDestinationPPCAddress;
	//	imlSegmentP0->imlList[0].associatedPPCAddress = imlSegmentP0->ppcAddrMin;
	//	// jump instruction for PEntry
	//	PPCRecompiler_pushBackIMLInstructions(imlSegmentPEntry, 0, 1);
	//	PPCRecompilerImlGen_generateNewInstruction_jumpSegment(&ppcImlGenContext, imlSegmentPEntry->imlList.data() + 0);

	//	// skip the newly created segments
	//	s += 2;
	//}
}