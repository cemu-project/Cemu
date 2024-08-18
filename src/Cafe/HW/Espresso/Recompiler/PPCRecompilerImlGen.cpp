#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterHelper.h"
#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "PPCRecompilerX64.h"
#include "PPCRecompilerImlRanges.h"
#include "util/helpers/StringBuf.h"

bool PPCRecompiler_decodePPCInstruction(ppcImlGenContext_t* ppcImlGenContext);
uint32 PPCRecompiler_iterateCurrentInstruction(ppcImlGenContext_t* ppcImlGenContext);
uint32 PPCRecompiler_getInstructionByOffset(ppcImlGenContext_t* ppcImlGenContext, uint32 offset);

PPCRecImlInstruction_t* PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext_t* ppcImlGenContext)
{
	if( ppcImlGenContext->imlListCount+1 > ppcImlGenContext->imlListSize )
	{
		sint32 newSize = ppcImlGenContext->imlListCount*2 + 2;
		ppcImlGenContext->imlList = (PPCRecImlInstruction_t*)realloc(ppcImlGenContext->imlList, sizeof(PPCRecImlInstruction_t)*newSize);
		ppcImlGenContext->imlListSize = newSize;
	}
	PPCRecImlInstruction_t* imlInstruction = ppcImlGenContext->imlList+ppcImlGenContext->imlListCount;
	memset(imlInstruction, 0x00, sizeof(PPCRecImlInstruction_t));
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER; // dont update any cr register by default
	imlInstruction->associatedPPCAddress = ppcImlGenContext->ppcAddressOfCurrentInstruction;
	ppcImlGenContext->imlListCount++;
	return imlInstruction;
}

void PPCRecompilerImlGen_generateNewInstruction_jumpmark(ppcImlGenContext_t* ppcImlGenContext, uint32 address)
{
	// no-op that indicates possible destination of a jump
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_JUMPMARK;
	imlInstruction->op_jumpmark.address = address;
}

void PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext_t* ppcImlGenContext, uint32 macroId, uint32 param, uint32 param2, uint16 paramU16)
{
	// no-op that indicates possible destination of a jump
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_MACRO;
	imlInstruction->operation = macroId;
	imlInstruction->op_macro.param = param;
	imlInstruction->op_macro.param2 = param2;
	imlInstruction->op_macro.paramU16 = paramU16;
}

/*
 * Generates a marker for Interpreter -> Recompiler entrypoints
 * PPC_ENTER iml instructions have no associated PPC address but the instruction itself has one
 */
void PPCRecompilerImlGen_generateNewInstruction_ppcEnter(ppcImlGenContext_t* ppcImlGenContext, uint32 ppcAddress)
{
	// no-op that indicates possible destination of a jump
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_PPC_ENTER;
	imlInstruction->operation = 0;
	imlInstruction->op_ppcEnter.ppcAddress = ppcAddress;
	imlInstruction->op_ppcEnter.x64Offset = 0;
	imlInstruction->associatedPPCAddress = 0;
}

void PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, uint32 operation, uint8 registerResult, uint8 registerA, uint8 crRegister, uint8 crMode)
{
	// operation with two register operands (e.g. "t0 = t1")
	if(imlInstruction == NULL)
		imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_R_R;
	imlInstruction->operation = operation;
	imlInstruction->crRegister = crRegister;
	imlInstruction->crMode = crMode;
	imlInstruction->op_r_r.registerResult = registerResult;
	imlInstruction->op_r_r.registerA = registerA;
}

void PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext_t* ppcImlGenContext, uint32 operation, uint8 registerResult, uint8 registerA, uint8 registerB, uint8 crRegister=PPC_REC_INVALID_REGISTER, uint8 crMode=0)
{
	// operation with three register operands (e.g. "t0 = t1 + t4")
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_R_R_R;
	imlInstruction->operation = operation;
	imlInstruction->crRegister = crRegister;
	imlInstruction->crMode = crMode;
	imlInstruction->op_r_r_r.registerResult = registerResult;
	imlInstruction->op_r_r_r.registerA = registerA;
	imlInstruction->op_r_r_r.registerB = registerB;
}

void PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext_t* ppcImlGenContext, uint32 operation, uint8 registerResult, uint8 registerA, sint32 immS32, uint8 crRegister=PPC_REC_INVALID_REGISTER, uint8 crMode=0)
{
	// operation with two register operands and one signed immediate (e.g. "t0 = t1 + 1234")
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_R_R_S32;
	imlInstruction->operation = operation;
	imlInstruction->crRegister = crRegister;
	imlInstruction->crMode = crMode;
	imlInstruction->op_r_r_s32.registerResult = registerResult;
	imlInstruction->op_r_r_s32.registerA = registerA;
	imlInstruction->op_r_r_s32.immS32 = immS32;
}

void PPCRecompilerImlGen_generateNewInstruction_name_r(ppcImlGenContext_t* ppcImlGenContext, uint32 operation, uint8 registerIndex, uint32 name, uint32 copyWidth, bool signExtend, bool bigEndian)
{
	// Store name (e.g. "'r3' = t0" which translates to MOV [ESP+offset_r3], reg32)
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_NAME_R;
	imlInstruction->operation = operation;
	imlInstruction->op_r_name.registerIndex = registerIndex;
	imlInstruction->op_r_name.name = name;
	imlInstruction->op_r_name.copyWidth = copyWidth;
	imlInstruction->op_r_name.flags = (signExtend?PPCREC_IML_OP_FLAG_SIGNEXTEND:0)|(bigEndian?PPCREC_IML_OP_FLAG_SWITCHENDIAN:0);
}

void PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext_t* ppcImlGenContext, uint32 operation, uint8 registerIndex, sint32 immS32, uint32 copyWidth, bool signExtend, bool bigEndian, uint8 crRegister, uint32 crMode)
{
	// two variations:
	// operation without store (e.g. "'r3' < 123" which has no effect other than updating a condition flags register)
	// operation with store (e.g. "'r3' = 123")
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_R_S32;
	imlInstruction->operation = operation;
	imlInstruction->crRegister = crRegister;
	imlInstruction->crMode = crMode;
	imlInstruction->op_r_immS32.registerIndex = registerIndex;
	imlInstruction->op_r_immS32.immS32 = immS32;
}

void PPCRecompilerImlGen_generateNewInstruction_conditional_r_s32(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, uint32 operation, uint8 registerIndex, sint32 immS32, uint32 crRegisterIndex, uint32 crBitIndex, bool bitMustBeSet)
{
	if(imlInstruction == NULL)
		imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	else
		memset(imlInstruction, 0, sizeof(PPCRecImlInstruction_t));
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


void PPCRecompilerImlGen_generateNewInstruction_jump(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, uint32 jumpmarkAddress)
{
	// jump
	if (imlInstruction == NULL)
		imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	else
		memset(imlInstruction, 0, sizeof(PPCRecImlInstruction_t));
	imlInstruction->type = PPCREC_IML_TYPE_CJUMP;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_conditionalJump.jumpmarkAddress = jumpmarkAddress;
	imlInstruction->op_conditionalJump.jumpAccordingToSegment = false;
	imlInstruction->op_conditionalJump.condition = PPCREC_JUMP_CONDITION_NONE;
	imlInstruction->op_conditionalJump.crRegisterIndex = 0;
	imlInstruction->op_conditionalJump.crBitIndex = 0;
	imlInstruction->op_conditionalJump.bitMustBeSet = false;
}

// jump based on segment branches
void PPCRecompilerImlGen_generateNewInstruction_jumpSegment(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction)
{
	// jump
	if (imlInstruction == NULL)
		imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->associatedPPCAddress = 0;
	imlInstruction->type = PPCREC_IML_TYPE_CJUMP;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_conditionalJump.jumpmarkAddress = 0;
	imlInstruction->op_conditionalJump.jumpAccordingToSegment = true;
	imlInstruction->op_conditionalJump.condition = PPCREC_JUMP_CONDITION_NONE;
	imlInstruction->op_conditionalJump.crRegisterIndex = 0;
	imlInstruction->op_conditionalJump.crBitIndex = 0;
	imlInstruction->op_conditionalJump.bitMustBeSet = false;
}

void PPCRecompilerImlGen_generateNewInstruction_noOp(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction)
{
	if (imlInstruction == NULL)
		imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_NO_OP;
	imlInstruction->operation = 0;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->crMode = 0;
}

void PPCRecompilerImlGen_generateNewInstruction_cr(ppcImlGenContext_t* ppcImlGenContext, uint32 operation, uint8 crD, uint8 crA, uint8 crB)
{
	// multiple variations:
	// operation involving only one cr bit (like clear crD bit)
	// operation involving three cr bits (like crD = crA or crB)
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_CR;
	imlInstruction->operation = operation;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->crMode = 0;
	imlInstruction->op_cr.crD = crD;
	imlInstruction->op_cr.crA = crA;
	imlInstruction->op_cr.crB = crB;
}

void PPCRecompilerImlGen_generateNewInstruction_conditionalJump(ppcImlGenContext_t* ppcImlGenContext, uint32 jumpmarkAddress, uint32 jumpCondition, uint32 crRegisterIndex, uint32 crBitIndex, bool bitMustBeSet)
{
	// conditional jump
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_CJUMP;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_conditionalJump.jumpmarkAddress = jumpmarkAddress;
	imlInstruction->op_conditionalJump.condition = jumpCondition;
	imlInstruction->op_conditionalJump.crRegisterIndex = crRegisterIndex;
	imlInstruction->op_conditionalJump.crBitIndex = crBitIndex;
	imlInstruction->op_conditionalJump.bitMustBeSet = bitMustBeSet;
}

void PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext_t* ppcImlGenContext, uint8 registerDestination, uint8 registerMemory, sint32 immS32, uint32 copyWidth, bool signExtend, bool switchEndian)
{
	// load from memory
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_LOAD;
	imlInstruction->operation = 0;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_storeLoad.registerData = registerDestination;
	imlInstruction->op_storeLoad.registerMem = registerMemory;
	imlInstruction->op_storeLoad.immS32 = immS32;
	imlInstruction->op_storeLoad.copyWidth = copyWidth;
	//imlInstruction->op_storeLoad.flags = (signExtend ? PPCREC_IML_OP_FLAG_SIGNEXTEND : 0) | (switchEndian ? PPCREC_IML_OP_FLAG_SWITCHENDIAN : 0);
	imlInstruction->op_storeLoad.flags2.swapEndian = switchEndian;
	imlInstruction->op_storeLoad.flags2.signExtend = signExtend;
}

void PPCRecompilerImlGen_generateNewInstruction_r_memory_indexed(ppcImlGenContext_t* ppcImlGenContext, uint8 registerDestination, uint8 registerMemory1, uint8 registerMemory2, uint32 copyWidth, bool signExtend, bool switchEndian)
{
	// load from memory
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_LOAD_INDEXED;
	imlInstruction->operation = 0;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_storeLoad.registerData = registerDestination;
	imlInstruction->op_storeLoad.registerMem = registerMemory1;
	imlInstruction->op_storeLoad.registerMem2 = registerMemory2;
	imlInstruction->op_storeLoad.copyWidth = copyWidth;
	//imlInstruction->op_storeLoad.flags = (signExtend?PPCREC_IML_OP_FLAG_SIGNEXTEND:0)|(switchEndian?PPCREC_IML_OP_FLAG_SWITCHENDIAN:0);
	imlInstruction->op_storeLoad.flags2.swapEndian = switchEndian;
	imlInstruction->op_storeLoad.flags2.signExtend = signExtend;
}

void PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext_t* ppcImlGenContext, uint8 registerSource, uint8 registerMemory, sint32 immS32, uint32 copyWidth, bool switchEndian)
{
	// load from memory
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_STORE;
	imlInstruction->operation = 0;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_storeLoad.registerData = registerSource;
	imlInstruction->op_storeLoad.registerMem = registerMemory;
	imlInstruction->op_storeLoad.immS32 = immS32;
	imlInstruction->op_storeLoad.copyWidth = copyWidth;
	//imlInstruction->op_storeLoad.flags = (switchEndian?PPCREC_IML_OP_FLAG_SWITCHENDIAN:0);
	imlInstruction->op_storeLoad.flags2.swapEndian = switchEndian;
	imlInstruction->op_storeLoad.flags2.signExtend = false;
}

void PPCRecompilerImlGen_generateNewInstruction_memory_r_indexed(ppcImlGenContext_t* ppcImlGenContext, uint8 registerDestination, uint8 registerMemory1, uint8 registerMemory2, uint32 copyWidth, bool signExtend, bool switchEndian)
{
	// load from memory
	PPCRecImlInstruction_t* imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_STORE_INDEXED;
	imlInstruction->operation = 0;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_storeLoad.registerData = registerDestination;
	imlInstruction->op_storeLoad.registerMem = registerMemory1;
	imlInstruction->op_storeLoad.registerMem2 = registerMemory2;
	imlInstruction->op_storeLoad.copyWidth = copyWidth;
	//imlInstruction->op_storeLoad.flags = (signExtend?PPCREC_IML_OP_FLAG_SIGNEXTEND:0)|(switchEndian?PPCREC_IML_OP_FLAG_SWITCHENDIAN:0);
	imlInstruction->op_storeLoad.flags2.swapEndian = switchEndian;
	imlInstruction->op_storeLoad.flags2.signExtend = signExtend;
}

void PPCRecompilerImlGen_generateNewInstruction_memory_memory(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, uint8 srcMemReg, sint32 srcImmS32, uint8 dstMemReg, sint32 dstImmS32, uint8 copyWidth)
{
	// copy from memory to memory
	if(imlInstruction == NULL)
		imlInstruction = PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext);
	imlInstruction->type = PPCREC_IML_TYPE_MEM2MEM;
	imlInstruction->operation = 0;
	imlInstruction->crRegister = PPC_REC_INVALID_REGISTER;
	imlInstruction->op_mem2mem.src.registerMem = srcMemReg;
	imlInstruction->op_mem2mem.src.immS32 = srcImmS32;
	imlInstruction->op_mem2mem.dst.registerMem = dstMemReg;
	imlInstruction->op_mem2mem.dst.immS32 = dstImmS32;
	imlInstruction->op_mem2mem.copyWidth = copyWidth;
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

void PPCRecompilerImlGen_TW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
//#ifdef CEMU_DEBUG_ASSERT
//	PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, 0, 0);
//#endif
	PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_LEAVE, ppcImlGenContext->ppcAddressOfCurrentInstruction, 0, 0);
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
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, sprReg, gprReg);
	}
	else if (spr >= SPR_UGQR0 && spr <= SPR_UGQR7)
	{
		uint32 gprReg = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0 + rD);
		if (gprReg == PPC_REC_INVALID_REGISTER)
			gprReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rD);
		uint32 sprReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + spr);
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, sprReg, gprReg);
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
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprReg, sprReg);
	}
	else if (spr >= SPR_UGQR0 && spr <= SPR_UGQR7)
	{
		uint32 sprReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + spr);
		uint32 gprReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0 + rD);
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprReg, sprReg);
	}
	else
		return false;
	return true;
}

bool PPCRecompilerImlGen_MFTB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 rD, spr1, spr2, spr;
	PPC_OPC_TEMPL_XO(opcode, rD, spr1, spr2);
	spr = spr1 | (spr2<<5);
	
	if (spr == 268 || spr == 269)
	{
		// TBL / TBU
		uint32 param2 = spr | (rD << 16);
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_MFTB, ppcImlGenContext->ppcAddressOfCurrentInstruction, param2, 0);
		return true;
	}
	return false;
}

bool PPCRecompilerImlGen_MFCR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rD, rA, rB);
	uint32 gprReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0 + rD);
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_MFCR, gprReg, 0, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
	return true;
}

bool PPCRecompilerImlGen_MTCRF(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 rS;
	uint32 crMask;
	PPC_OPC_TEMPL_XFX(opcode, rS, crMask);
	uint32 gprReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0 + rS);
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_MTCRF, gprReg, crMask, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
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
	PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_COMPARE_SIGNED, gprRegisterA, gprRegisterB, cr, PPCREC_CR_MODE_COMPARE_SIGNED);
}

void PPCRecompilerImlGen_CMPL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(opcode, cr, rA, rB);
	cr >>= 2;
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_COMPARE_UNSIGNED, gprRegisterA, gprRegisterB, cr, PPCREC_CR_MODE_COMPARE_UNSIGNED);
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
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_COMPARE_SIGNED, gprRegister, b, 0, false, false, cr, PPCREC_CR_MODE_COMPARE_SIGNED);
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
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_COMPARE_UNSIGNED, gprRegister, (sint32)b, 0, false, false, cr, PPCREC_CR_MODE_COMPARE_UNSIGNED);
}

bool PPCRecompiler_canInlineFunction(MPTR functionPtr, sint32* functionInstructionCount)
{
	for (sint32 i = 0; i < 6; i++)
	{
		uint32 opcode = memory_readU32(functionPtr+i*4);
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
		ppcImlGenContext->ppcAddressOfCurrentInstruction = startAddress + i*4;
		ppcImlGenContext->cyclesSinceLastBranch++;
		if (PPCRecompiler_decodePPCInstruction(ppcImlGenContext))
		{
			assert_dbg();
		}
	}
	// add range
	ppcRecRange_t recRange;
	recRange.ppcAddress = startAddress;
	recRange.ppcSize = instructionCount*4 + 4; // + 4 because we have to include the BLR
	ppcImlGenContext->functionRef->list_ranges.push_back(recRange);
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
		// check if function can be inlined
		sint32 inlineFuncInstructionCount = 0;
		if (PPCRecompiler_canInlineFunction(jumpAddressDest, &inlineFuncInstructionCount))
		{
			// generate NOP iml instead of BL macro (this assures that segment PPC range remains intact)
			PPCRecompilerImlGen_generateNewInstruction_noOp(ppcImlGenContext, NULL);
			//cemuLog_log(LogType::Force, "Inline func 0x{:08x} at {:08x}", jumpAddressDest, ppcImlGenContext->ppcAddressOfCurrentInstruction);
			uint32* prevInstructionPtr = ppcImlGenContext->currentInstruction;
			ppcImlGenContext->currentInstruction = (uint32*)memory_getPointerFromVirtualOffset(jumpAddressDest);
			PPCRecompiler_generateInlinedCode(ppcImlGenContext, jumpAddressDest, inlineFuncInstructionCount);
			ppcImlGenContext->currentInstruction = prevInstructionPtr;
			return true;
		}
		// generate funtion call instructions
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_BL, ppcImlGenContext->ppcAddressOfCurrentInstruction, jumpAddressDest, ppcImlGenContext->cyclesSinceLastBranch);
		PPCRecompilerImlGen_generateNewInstruction_ppcEnter(ppcImlGenContext, ppcImlGenContext->ppcAddressOfCurrentInstruction+4);
		return true;
	}
	// is jump destination within recompiled function?
	if( jumpAddressDest >= ppcImlGenContext->functionRef->ppcAddress && jumpAddressDest < (ppcImlGenContext->functionRef->ppcAddress + ppcImlGenContext->functionRef->ppcSize) )
	{
		// generate instruction
		PPCRecompilerImlGen_generateNewInstruction_jump(ppcImlGenContext, NULL, jumpAddressDest);
	}
	else
	{
		// todo: Inline this jump destination if possible (in many cases it's a bunch of GPR/FPR store instructions + BLR)
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_B_FAR, ppcImlGenContext->ppcAddressOfCurrentInstruction, jumpAddressDest, ppcImlGenContext->cyclesSinceLastBranch);
	}
	return true;
}

bool PPCRecompilerImlGen_BC(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
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
			// generate instruction
			//PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, 0, 0);
			PPCRecompilerImlGen_generateNewInstruction_conditionalJump(ppcImlGenContext, ppcImlGenContext->ppcAddressOfCurrentInstruction+4, jumpCondition, crRegister, crBit, !conditionMustBeTrue);
			PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_BL, ppcImlGenContext->ppcAddressOfCurrentInstruction, jumpAddressDest, ppcImlGenContext->cyclesSinceLastBranch);
			PPCRecompilerImlGen_generateNewInstruction_ppcEnter(ppcImlGenContext, ppcImlGenContext->ppcAddressOfCurrentInstruction+4);
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
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_SUB, ctrRegister, 1, 0, false, false, PPCREC_CR_REG_TEMP, PPCREC_CR_MODE_ARITHMETIC);
		if( decrementerMustBeZero )
			PPCRecompilerImlGen_generateNewInstruction_conditionalJump(ppcImlGenContext, jumpAddressDest, PPCREC_JUMP_CONDITION_E, PPCREC_CR_REG_TEMP, 0, false);
		else
			PPCRecompilerImlGen_generateNewInstruction_conditionalJump(ppcImlGenContext, jumpAddressDest, PPCREC_JUMP_CONDITION_NE, PPCREC_CR_REG_TEMP, 0, false);
		return true;
	}
	else
	{
		if( ignoreCondition )
		{
			// branch always, no condition and no decrementer
			debugBreakpoint();
			crRegister = PPC_REC_INVALID_REGISTER; // not necessary but lets optimizer know we dont care for cr register on this instruction
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

			if (jumpAddressDest >= ppcImlGenContext->functionRef->ppcAddress && jumpAddressDest < (ppcImlGenContext->functionRef->ppcAddress + ppcImlGenContext->functionRef->ppcSize))
			{
				// near jump
				PPCRecompilerImlGen_generateNewInstruction_conditionalJump(ppcImlGenContext, jumpAddressDest, jumpCondition, crRegister, crBit, conditionMustBeTrue);
			}
			else
			{
				// far jump
				PPCRecompilerImlGen_generateNewInstruction_conditionalJump(ppcImlGenContext, ppcImlGenContext->ppcAddressOfCurrentInstruction + 4, jumpCondition, crRegister, crBit, !conditionMustBeTrue);
				PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_B_FAR, ppcImlGenContext->ppcAddressOfCurrentInstruction, jumpAddressDest, ppcImlGenContext->cyclesSinceLastBranch);
				PPCRecompilerImlGen_generateNewInstruction_ppcEnter(ppcImlGenContext, ppcImlGenContext->ppcAddressOfCurrentInstruction + 4);
			}
		}
	}
	return true;
}

bool PPCRecompilerImlGen_BCLR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(opcode, BO, BI, BD);

	uint32 crRegister = BI/4;
	uint32 crBit = BI%4;

	uint32 jumpCondition = 0;

	bool conditionMustBeTrue = (BO&8)!=0;
	bool useDecrementer = (BO&4)==0; // bit not set -> decrement
	bool decrementerMustBeZero = (BO&2)!=0; // bit set -> branch if CTR = 0, bit not set -> branch if CTR != 0
	bool ignoreCondition = (BO&16)!=0;
	bool saveLR = (opcode&PPC_OPC_LK)!=0;
	// since we skip this instruction if the condition is true, we need to invert the logic
	bool invertedConditionMustBeTrue = !conditionMustBeTrue;
	if( useDecrementer )
	{
		cemu_assert_debug(false);
		return false; // unsupported
	}
	else
	{
		if( ignoreCondition )
		{
			// store LR
			if( saveLR )
			{
				PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_BLRL, ppcImlGenContext->ppcAddressOfCurrentInstruction, 0, ppcImlGenContext->cyclesSinceLastBranch);
				PPCRecompilerImlGen_generateNewInstruction_ppcEnter(ppcImlGenContext, ppcImlGenContext->ppcAddressOfCurrentInstruction+4);
			}
			else
			{
				// branch always, no condition and no decrementer
				PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_BLR, ppcImlGenContext->ppcAddressOfCurrentInstruction, 0, ppcImlGenContext->cyclesSinceLastBranch);
			}
		}
		else
		{
			// store LR
			if( saveLR )
			{
				uint32 registerLR = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_SPR0+SPR_LR);
				PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ASSIGN, registerLR, (ppcImlGenContext->ppcAddressOfCurrentInstruction+4)&0x7FFFFFFF, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
			}
			// generate jump condition
			if( invertedConditionMustBeTrue )
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
			else
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
			// jump if BCLR condition NOT met (jump to jumpmark of next instruction, essentially skipping current instruction)
			PPCRecompilerImlGen_generateNewInstruction_conditionalJump(ppcImlGenContext, ppcImlGenContext->ppcAddressOfCurrentInstruction+4, jumpCondition, crRegister, crBit, invertedConditionMustBeTrue);
			PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_BLR, ppcImlGenContext->ppcAddressOfCurrentInstruction, 0, ppcImlGenContext->cyclesSinceLastBranch);
		}
	}
	return true;
}

bool PPCRecompilerImlGen_BCCTR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(opcode, BO, BI, BD);

	uint32 crRegister = BI/4;
	uint32 crBit = BI%4;

	uint32 jumpCondition = 0;

	bool conditionMustBeTrue = (BO&8)!=0;
	bool useDecrementer = (BO&4)==0; // bit not set -> decrement
	bool decrementerMustBeZero = (BO&2)!=0; // bit set -> branch if CTR = 0, bit not set -> branch if CTR != 0
	bool ignoreCondition = (BO&16)!=0;
	bool saveLR = (opcode&PPC_OPC_LK)!=0;
	// since we skip this instruction if the condition is true, we need to invert the logic
	bool invertedConditionMustBeTrue = !conditionMustBeTrue;
	if( useDecrementer )
	{
		assert_dbg();
		// if added, dont forget inverted logic
		debug_printf("Rec: BCLR unsupported decrementer\n");
		return false; // unsupported
	}
	else
	{
		if( ignoreCondition )
		{
			// store LR
			if( saveLR )
			{
				uint32 registerLR = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_SPR0+SPR_LR);
				PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ASSIGN, registerLR, (ppcImlGenContext->ppcAddressOfCurrentInstruction+4)&0x7FFFFFFF, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
				PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_BCTRL, ppcImlGenContext->ppcAddressOfCurrentInstruction, 0, ppcImlGenContext->cyclesSinceLastBranch);
				PPCRecompilerImlGen_generateNewInstruction_ppcEnter(ppcImlGenContext, ppcImlGenContext->ppcAddressOfCurrentInstruction+4);
			}
			else
			{
				// branch always, no condition and no decrementer
				PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_BCTR, ppcImlGenContext->ppcAddressOfCurrentInstruction, 0, ppcImlGenContext->cyclesSinceLastBranch);
			}
		}
		else
		{
			// store LR
			if( saveLR )
			{
				uint32 registerLR = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_SPR0+SPR_LR);
				PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ASSIGN, registerLR, (ppcImlGenContext->ppcAddressOfCurrentInstruction+4)&0x7FFFFFFF, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
			}
			// generate jump condition
			if( invertedConditionMustBeTrue )
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
			else
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
			// jump if BCLR condition NOT met (jump to jumpmark of next instruction, essentially skipping current instruction)
			PPCRecompilerImlGen_generateNewInstruction_conditionalJump(ppcImlGenContext, ppcImlGenContext->ppcAddressOfCurrentInstruction+4, jumpCondition, crRegister, crBit, invertedConditionMustBeTrue);
			PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_BCTR, ppcImlGenContext->ppcAddressOfCurrentInstruction, 0, ppcImlGenContext->cyclesSinceLastBranch);
		}
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
	if( opcode&PPC_OPC_RC )
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_ADD, registerRD, registerRA, registerRB, 0, PPCREC_CR_MODE_LOGICAL);
	}
	else
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_ADD, registerRD, registerRA, registerRB);
	}
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
	if( opcode&PPC_OPC_RC )
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_ADD_UPDATE_CARRY, registerRD, registerRA, registerRB, 0, PPCREC_CR_MODE_LOGICAL);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_ADD_UPDATE_CARRY, registerRD, registerRA, registerRB);
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
	if( opcode&PPC_OPC_RC )
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_ADD_CARRY_UPDATE_CARRY, registerRD, registerRB, registerRA, 0, PPCREC_CR_MODE_LOGICAL);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_ADD_CARRY_UPDATE_CARRY, registerRD, registerRB, registerRA);
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
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, registerRD, registerRA);
	}
	if( opcode&PPC_OPC_RC )
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ADD_CARRY, registerRD, registerRD, 0, PPCREC_CR_MODE_LOGICAL);
	}
	else
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ADD_CARRY, registerRD, registerRD);
	}
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
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, registerRD, registerRA);
	}
	if( opcode&PPC_OPC_RC )
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ADD_CARRY_ME, registerRD, registerRD, 0, PPCREC_CR_MODE_LOGICAL);
	}
	else
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ADD_CARRY_ME, registerRD, registerRD);
	}
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
		PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, registerRD, registerRA, imm);
	}
	else
	{
		// rA not used, instruction is value assignment
		// rD = imm
		uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ASSIGN, registerRD, imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
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
		PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, registerRD, registerRA, (sint32)imm);
	}
	else
	{
		// rA not used, instruction turns into simple value assignment
		// rD = imm
		uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ASSIGN, registerRD, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
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
	// check if rD is already loaded, else use new temporary register
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD_UPDATE_CARRY, registerRD, registerRA, imm);
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
	// check if rD is already loaded, else use new temporary register
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD_UPDATE_CARRY, registerRD, registerRA, imm, 0, PPCREC_CR_MODE_LOGICAL);
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
	if( opcode&PPC_OPC_RC )
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_SUB, registerRD, registerRB, registerRA, 0, PPCREC_CR_MODE_LOGICAL);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_SUB, registerRD, registerRB, registerRA);
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
	if( opcode&PPC_OPC_RC )
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_SUB_CARRY_UPDATE_CARRY, registerRD, registerRB, registerRA, 0, PPCREC_CR_MODE_LOGICAL);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_SUB_CARRY_UPDATE_CARRY, registerRD, registerRB, registerRA);
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
	if( opcode&PPC_OPC_RC )
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_SUB_CARRY_UPDATE_CARRY, registerRD, registerRA, 0, PPCREC_CR_MODE_LOGICAL);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_SUB_CARRY_UPDATE_CARRY, registerRD, registerRA);
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
	PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_SUBFC, registerRD, registerRA, registerRB);
	if (opcode & PPC_OPC_RC)
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, registerRD, registerRD, 0, PPCREC_CR_MODE_LOGICAL);
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
	PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_SUBFC, registerRD, registerRA, imm);
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
	PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_MULTIPLY_SIGNED, registerResult, registerOperand, (sint32)imm);
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
	if( opcode&PPC_OPC_RC )
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_MULTIPLY_SIGNED, registerResult, registerOperand1, registerOperand2, 0, PPCREC_CR_MODE_LOGICAL);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_MULTIPLY_SIGNED, registerResult, registerOperand1, registerOperand2);
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
	if( opcode&PPC_OPC_RC )
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED, registerResult, registerOperand1, registerOperand2, 0, PPCREC_CR_MODE_LOGICAL);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED, registerResult, registerOperand1, registerOperand2);
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
	if( opcode&PPC_OPC_RC )
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_MULTIPLY_HIGH_UNSIGNED, registerResult, registerOperand1, registerOperand2, 0, PPCREC_CR_MODE_LOGICAL);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_MULTIPLY_HIGH_UNSIGNED, registerResult, registerOperand1, registerOperand2);
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
	if (opcode & PPC_OPC_RC)
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_DIVIDE_SIGNED, registerResult, registerOperand1, registerOperand2, 0, PPCREC_CR_MODE_ARITHMETIC);
	}
	else
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_DIVIDE_SIGNED, registerResult, registerOperand1, registerOperand2);
	}
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
	if (opcode & PPC_OPC_RC)
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_DIVIDE_UNSIGNED, registerResult, registerOperand1, registerOperand2, 0, PPCREC_CR_MODE_ARITHMETIC);
	}
	else
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_DIVIDE_UNSIGNED, registerResult, registerOperand1, registerOperand2);
	}
	return true;
}

bool PPCRecompilerImlGen_RLWINM(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	uint32 mask = ppc_mask(MB, ME);
	//uint32 v = ppc_word_rotl(hCPU->gpr[rS], SH);
	//hCPU->gpr[rA] = v & mask;

	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	// handle special forms of RLWINM
	if( SH == 0 && SH == (ME-SH) && MB == 0 )
	{
		// CLRRWI
		// todo
	}
	else if( ME == (31-SH) && MB == 0 )
	{
		// SLWI
		if(opcode&PPC_OPC_RC)
			PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_LEFT_SHIFT, registerRA, registerRS, SH, 0, PPCREC_CR_MODE_LOGICAL);
		else
			PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_LEFT_SHIFT, registerRA, registerRS, SH);
		return true;
	}
	else if( SH == (32-MB) && ME == 31 )
	{
		// SRWI
		if(opcode&PPC_OPC_RC)
			PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_RIGHT_SHIFT, registerRA, registerRS, MB, 0, PPCREC_CR_MODE_LOGICAL);
		else
			PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_RIGHT_SHIFT, registerRA, registerRS, MB);
		return true;
	}
	// general handler
	if( registerRA != registerRS )
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, registerRA, registerRS);
	if( SH != 0 )
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_LEFT_ROTATE, registerRA, SH, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
	if(opcode&PPC_OPC_RC)
	{
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_AND, registerRA, (sint32)mask, 0, false, false, 0, PPCREC_CR_MODE_LOGICAL);
	}
	else
	{
		if( mask != 0xFFFFFFFF )
			PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_AND, registerRA, (sint32)mask, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
	}
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
	PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_RLWIMI, registerRA, registerRS, (sint32)vImm, PPC_REC_INVALID_REGISTER, 0);
	if (opcode & PPC_OPC_RC)
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, registerRA, registerRA, 0, PPCREC_CR_MODE_LOGICAL);
	return true;
}

bool PPCRecompilerImlGen_RLWNM(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, rB, MB, ME);
	//	uint32 v = ppc_word_rotl(hCPU->gpr[rS], hCPU->gpr[rB]);
	uint32 mask = ppc_mask(MB, ME);
	//	uint32 v = ppc_word_rotl(hCPU->gpr[rS], hCPU->gpr[rB]);
	//	hCPU->gpr[rA] = v & mask;
	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);

	PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_LEFT_ROTATE, registerRA, registerRS, registerRB);
	if (opcode & PPC_OPC_RC)
	{
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_AND, registerRA, (sint32)mask, 32, false, false, 0, PPCREC_CR_MODE_LOGICAL);
	}
	else
	{
		if( mask != 0xFFFFFFFF )
			PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_AND, registerRA, (sint32)mask, 32, false, false, PPC_REC_INVALID_REGISTER, 0);
	}
	return true;
}

bool PPCRecompilerImlGen_SRAW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	//uint32 SH = hCPU->gpr[rB] & 0x3f;
	//hCPU->gpr[rA] = hCPU->gpr[rS];
	//hCPU->xer_ca = 0;
	//if (hCPU->gpr[rA] & 0x80000000) {
	//	uint32 ca = 0;
	//	for (uint32 i=0; i < SH; i++) {
	//		if (hCPU->gpr[rA] & 1) ca = 1;
	//		hCPU->gpr[rA] >>= 1;
	//		hCPU->gpr[rA] |= 0x80000000;
	//	}
	//	if (ca) hCPU->xer_ca = 1;
	//} else {
	//	if (SH > 31) {
	//		hCPU->gpr[rA] = 0;
	//	} else {
	//		hCPU->gpr[rA] >>= SH;
	//	}
	//}     
	//if (Opcode & PPC_OPC_RC) {
	//	// update cr0 flags
	//	ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	//}

	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	if( (opcode&PPC_OPC_RC) != 0 )
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_SRAW, registerRA, registerRS, registerRB, 0, PPCREC_CR_MODE_LOGICAL);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_SRAW, registerRA, registerRS, registerRB);
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
	if( opcode&PPC_OPC_RC )
		PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_SRAW, registerRA, registerRS, (sint32)SH, 0, PPCREC_CR_MODE_LOGICAL);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_SRAW, registerRA, registerRS, (sint32)SH);
	return true;
}

bool PPCRecompilerImlGen_SLW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);

	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	if (opcode & PPC_OPC_RC)
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_SLW, registerRA, registerRS, registerRB, 0, PPCREC_CR_MODE_LOGICAL);
	}
	else
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_SLW, registerRA, registerRS, registerRB, PPC_REC_INVALID_REGISTER, 0);
	}
	return true;
}

bool PPCRecompilerImlGen_SRW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);

	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	if (opcode & PPC_OPC_RC)
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_SRW, registerRA, registerRS, registerRB, 0, PPCREC_CR_MODE_LOGICAL);
	}
	else
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_SRW, registerRA, registerRS, registerRB, PPC_REC_INVALID_REGISTER, 0);
	}
	return true;
}


bool PPCRecompilerImlGen_EXTSH(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	PPC_ASSERT(rB==0);
	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	if ( opcode&PPC_OPC_RC )
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN_S16_TO_S32, registerRA, registerRS, 0, PPCREC_CR_MODE_ARITHMETIC);
	}
	else
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN_S16_TO_S32, registerRA, registerRS);
	}
	return true;
}

bool PPCRecompilerImlGen_EXTSB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	if ( opcode&PPC_OPC_RC )
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN_S8_TO_S32, registerRA, registerRS, 0, PPCREC_CR_MODE_ARITHMETIC);
	}
	else
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN_S8_TO_S32, registerRA, registerRS);
	}
	return true;
}

bool PPCRecompilerImlGen_CNTLZW(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	PPC_ASSERT(rB==0);
	if( opcode&PPC_OPC_RC )
	{
		return false;
	}
	uint32 registerRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false);
	uint32 registerRA = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_CNTLZW, registerRA, registerRS);

	//uint32 n=0;
	//uint32 x=0x80000000;
	//uint32 v=hCPU->gpr[rS];
	//while (!(v & x)) {
	//	n++;
	//	if (n==32) break;
	//	x>>=1;
	//}
	//hCPU->gpr[rA] = n;
	//if (Opcode & PPC_OPC_RC) {
	//	// update cr0 flags
	//	ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	//}


	return true;
}

bool PPCRecompilerImlGen_NEG(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_XO(opcode, rD, rA, rB);
	PPC_ASSERT(rB == 0);
	//hCPU->gpr[rD] = -((signed int)hCPU->gpr[rA]);
	//if (Opcode & PPC_OPC_RC) {
	//	// update cr0 flags
	//	ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	//}
	uint32 registerRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 registerRD = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rD);
	if( opcode&PPC_OPC_RC )
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NEG, registerRD, registerRA, 0, PPCREC_CR_MODE_ARITHMETIC);
	}
	else
	{
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NEG, registerRD, registerRA);
	}
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// add imm to memory register
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, gprRegister, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// add imm to memory register
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, gprRegister, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// add imm to memory register
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, gprRegister, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// load memory gpr into register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// add imm to memory register
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, gprRegister, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
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
	PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterB);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
	PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterB);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
	PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterB);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
	PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterB);
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
		//PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// store&update instructions where rD==rA store the register contents without added imm, therefore we need to handle it differently
	// get memory gpr register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// get source register
	uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false); // can be the same as gprRegister
	// add imm to memory register early if possible
	if( rD != rA )
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, gprRegister, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
	// store word
	PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, (rD==rA)?imm:0, 32, true);
	// add imm to memory register late if we couldn't do it early
	if( rD == rA )
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, gprRegister, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
}

void PPCRecompilerImlGen_STH(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// get memory gpr register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// get source register
	uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false); // can be the same as gprRegister
	// add imm to memory register early if possible
	if( rD != rA )
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, gprRegister, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
	// store word
	PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, (rD==rA)?imm:0, 16, true);
	// add imm to memory register late if we couldn't do it early
	if( rD == rA )
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, gprRegister, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
}

void PPCRecompilerImlGen_STB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rS, rA, imm);
	if( rA == 0 )
	{
		// special form where gpr is ignored and only imm is used
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
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
		PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_DEBUGBREAK, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->ppcAddressOfCurrentInstruction, ppcImlGenContext->cyclesSinceLastBranch);
		return;
	}
	// get memory gpr register
	uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	// get source register
	uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rD, false); // can be the same as gprRegister
	// add imm to memory register early if possible
	if( rD != rA )
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, gprRegister, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
	// store byte
	PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, (rD==rA)?imm:0, 8, true);
	// add imm to memory register late if we couldn't do it early
	if( rD == rA )
		PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_ADD, gprRegister, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
}

// generic indexed store (STWX, STHX, STBX, STWUX. If bitReversed == true -> STHBRX)
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
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterB);
		return true;
	}
	// prepare registers
	uint32 gprRegisterA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
	uint32 gprRegisterB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB, false);
	uint32 sourceRegister = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	// update EA
	PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ADD, gprRegisterA, gprRegisterB);	
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
	if( nb == 4 )
	{
		// if nb == 4 this instruction immitates LWZ
		if( rA == 0 )
		{
#ifdef CEMU_DEBUG_ASSERT
			assert_dbg(); // special form where gpr is ignored and only imm is used
#endif
			return false;
		}
		// load memory gpr into register
		uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
		// check if destination register is already loaded
		uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
		if( destinationRegister == PPC_REC_INVALID_REGISTER )
			destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
		// load half
		PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, 0, 32, false, true);
		return true;
	}
	else if( nb == 2 )
	{
		// if nb == 2 this instruction immitates a LHZ but the result is shifted left by 16 bits
		if( rA == 0 )
		{
#ifdef CEMU_DEBUG_ASSERT
			assert_dbg(); // special form where gpr is ignored and only imm is used
#endif
			return false;
		}
		// load memory gpr into register
		uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
		// check if destination register is already loaded
		uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
		if( destinationRegister == PPC_REC_INVALID_REGISTER )
			destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
		// load half
		PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, 0, 16, false, true);
		// shift
		PPCRecompilerImlGen_generateNewInstruction_r_r_s32(ppcImlGenContext, PPCREC_IML_OP_LEFT_SHIFT, destinationRegister, destinationRegister, 16);		
		return true;
	}
	else if( nb == 3 )
	{
		// if nb == 3 this instruction loads a 3-byte big-endian and the result is shifted left by 8 bits
		if( rA == 0 )
		{
#ifdef CEMU_DEBUG_ASSERT
			assert_dbg(); // special form where gpr is ignored and only imm is used
#endif
			return false;
		}
		// load memory gpr into register
		uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
		// check if destination register is already loaded
		uint32 destinationRegister = PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, PPCREC_NAME_R0+rD);
		if( destinationRegister == PPC_REC_INVALID_REGISTER )
			destinationRegister = PPCRecompilerImlGen_getAndLockFreeTemporaryGPR(ppcImlGenContext, PPCREC_NAME_R0+rD); // else just create new register
		// load half
		PPCRecompilerImlGen_generateNewInstruction_r_memory(ppcImlGenContext, destinationRegister, gprRegister, 0, PPC_REC_STORE_LSWI_3, false, true);
		return true;
	}
	debug_printf("PPCRecompilerImlGen_LSWI(): Unsupported nb value %d\n", nb);
	return false;
}

bool PPCRecompilerImlGen_STSWI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, rS, nb;
	PPC_OPC_TEMPL_X(opcode, rS, rA, nb);
	if( nb == 0 )
		nb = 32;
	if( nb == 4 )
	{
		// load memory gpr into register
		uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
		// load source register
		uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false); // can be the same as gprRegister
		// store word
		PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, 0, 32, true);
		return true;
	}
	else if( nb == 2 )
	{
		// load memory gpr into register
		uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
		// load source register
		uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false); // can be the same as gprRegister
		// store half-word (shifted << 16)
		PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, 0, PPC_REC_STORE_STSWI_2, false);
		return true;
	}
	else if( nb == 3 )
	{
		// load memory gpr into register
		uint32 gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA, false);
		// load source register
		uint32 sourceRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS, false); // can be the same as gprRegister
		// store 3-byte-word (shifted << 8)
		PPCRecompilerImlGen_generateNewInstruction_memory_r(ppcImlGenContext, sourceRegister, gprRegister, 0, PPC_REC_STORE_STSWI_3, false);
		return true;
	}
	debug_printf("PPCRecompilerImlGen_STSWI(): Unsupported nb value %d\n", nb);
	return false;
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
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_DCBZ, gprRegisterA, gprRegisterB);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_DCBZ, gprRegisterB, gprRegisterB);
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
			if( opcode&PPC_OPC_RC )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg, 0, PPCREC_CR_MODE_LOGICAL);
			}
			else
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
			}
		}
		else
		{
			if( opcode&PPC_OPC_RC )
			{
				// no effect but CR is updated
				sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprSourceReg, gprSourceReg, 0, PPCREC_CR_MODE_LOGICAL);
			}
			else
			{
				// no-op
			}
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
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_OR, gprDestReg, gprSource2Reg);
			}
			else
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_OR, gprDestReg, gprSource1Reg);
			}
			if( opcode&PPC_OPC_RC )
			{
				// fixme: merge CR update into OR instruction above
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, gprDestReg, gprDestReg, 0, PPCREC_CR_MODE_LOGICAL);
			}
		}
		else
		{
			// rA = rS
			if( gprDestReg != gprSource1Reg )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSource1Reg);
			}
			// rA |= rB
			if( opcode&PPC_OPC_RC )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_OR, gprDestReg, gprSource2Reg, 0, PPCREC_CR_MODE_LOGICAL);
			}
			else
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_OR, gprDestReg, gprSource2Reg);
			}
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
	if( opcode&PPC_OPC_RC )
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_ORC, gprDestReg, gprSource1Reg, gprSource2Reg, 0, PPCREC_CR_MODE_LOGICAL);
	else
		PPCRecompilerImlGen_generateNewInstruction_r_r_r(ppcImlGenContext, PPCREC_IML_OP_ORC, gprDestReg, gprSource1Reg, gprSource2Reg);
	return true;
}

bool PPCRecompilerImlGen_NOR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	//hCPU->gpr[rA] = ~(hCPU->gpr[rS] | hCPU->gpr[rB]);
	// check for NOT mnemonic
	if( rS == rB )
	{
		// simple register copy with NOT
		sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
		sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		if( gprDestReg != gprSourceReg )
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
		if( opcode&PPC_OPC_RC )
		{
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprDestReg, gprDestReg, 0, PPCREC_CR_MODE_ARITHMETIC);
		}
		else
		{
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
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
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_OR, gprDestReg, gprSource2Reg);
			}
			else
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_OR, gprDestReg, gprSource1Reg);
			}
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
			if( opcode&PPC_OPC_RC )
			{
				// fixme: merge CR update into OR instruction above
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, gprDestReg, gprDestReg, 0, PPCREC_CR_MODE_LOGICAL);
			}
		}
		else
		{
			// rA = rS
			if( gprDestReg != gprSource1Reg )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSource1Reg);
			}
			// rA |= rB
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_OR, gprDestReg, gprSource2Reg);
			if( opcode&PPC_OPC_RC )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprDestReg, gprDestReg, 0, PPCREC_CR_MODE_ARITHMETIC);
			}
			else
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
			}
		}
	}
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
			if( opcode&PPC_OPC_RC )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg, 0, PPCREC_CR_MODE_LOGICAL);
			}
			else
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
			}
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
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, gprDestReg, gprSource2Reg);
			}
			else
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, gprDestReg, gprSource1Reg);
			}
			if( opcode&PPC_OPC_RC )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, gprDestReg, gprDestReg, 0, PPCREC_CR_MODE_LOGICAL);
			}
		}
		else
		{
			// rA = rS
			if( gprDestReg != gprSource1Reg )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSource1Reg);
			}
			// rA &= rB
			if( opcode&PPC_OPC_RC )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, gprDestReg, gprSource2Reg, 0, PPCREC_CR_MODE_LOGICAL);
			}
			else
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, gprDestReg, gprSource2Reg);
			}
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
		if( opcode&PPC_OPC_RC )
		{
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprDestReg, 0, PPCREC_CR_MODE_LOGICAL);
		}
		else
		{
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprDestReg);
		}
	}
	else if( rA == rB )
	{
		// rB already in rA, therefore we complement rA first and then AND it with rS
		sint32 gprRS = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
		sint32 gprRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		// rA = ~rA
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprRA, gprRA);
		// rA &= rS
		if( opcode&PPC_OPC_RC )
		{
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, gprRA, gprRS, 0, PPCREC_CR_MODE_LOGICAL);
		}
		else
		{
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, gprRA, gprRS);
		}
	}
	else
	{
		// a & (~b) is the same as ~((~a) | b)
		sint32 gprRA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		sint32 gprRB = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
		sint32 gprRS = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
		// move rS to rA (if required)
		if( gprRA != gprRS )
		{
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprRA, gprRS);
		}
		// rS already in rA, therefore we complement rS first and then OR it with rB
		// rA = ~rA
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprRA, gprRA);
		// rA |= rB
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_OR, gprRA, gprRB);
		// rA = ~rA
		if( opcode&PPC_OPC_RC )
		{
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprRA, gprRA, 0, PPCREC_CR_MODE_LOGICAL);
		}
		else
		{
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprRA, gprRA);
		}
	}
	return true;
}

void PPCRecompilerImlGen_ANDI(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(opcode, rS, rA, imm);
	// ANDI. always sets cr0 flags
	sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	// rA = rS
	if( gprDestReg != gprSourceReg )
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA &= imm32
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_AND, gprDestReg, (sint32)imm, 0, false, false, 0, PPCREC_CR_MODE_LOGICAL);
}

void PPCRecompilerImlGen_ANDIS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(opcode, rS, rA, imm);
	// ANDI. always sets cr0 flags
	sint32 gprSourceReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rS);
	sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	// rA = rS
	if( gprDestReg != gprSourceReg )
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA &= imm32
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_AND, gprDestReg, (sint32)imm, 0, false, false, 0, PPCREC_CR_MODE_LOGICAL);
}

bool PPCRecompilerImlGen_XOR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rS, rA, rB;
	PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
	if( rS == rB )
	{
		// xor register with itself
		sint32 gprDestReg = PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		if( opcode&PPC_OPC_RC )
		{
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprDestReg, 0, PPCREC_CR_MODE_LOGICAL);
		}
		else
		{
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprDestReg);
		}
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
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprSource2Reg);
			}
			else
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprSource1Reg);
			}
			if( opcode&PPC_OPC_RC )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_AND, gprDestReg, gprDestReg, 0, PPCREC_CR_MODE_LOGICAL);
			}
		}
		else
		{
			// rA = rS
			if( gprDestReg != gprSource1Reg )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSource1Reg);
			}
			// rA ^= rB
			if( opcode&PPC_OPC_RC )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprSource2Reg, 0, PPCREC_CR_MODE_LOGICAL);
			}
			else
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprSource2Reg);
			}
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
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprDestReg);
		if( opcode&PPC_OPC_RC )
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprDestReg, gprDestReg, 0, PPCREC_CR_MODE_LOGICAL);
		else
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
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
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprSource2Reg);
			}
			else
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprSource1Reg);
			}
		}
		else
		{
			// rA = rS
			if( gprDestReg != gprSource1Reg )
			{
				PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSource1Reg);
			}
			// rA ^= rB
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_XOR, gprDestReg, gprSource2Reg);
		}
		if( opcode&PPC_OPC_RC )
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprDestReg, gprDestReg, 0, PPCREC_CR_MODE_LOGICAL);
		else
			PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_NOT, gprDestReg, gprDestReg);
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
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA |= imm32
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_OR, gprDestReg, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
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
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA |= imm32
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_OR, gprDestReg, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
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
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA |= imm32
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_XOR, gprDestReg, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
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
		PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext, NULL, PPCREC_IML_OP_ASSIGN, gprDestReg, gprSourceReg);
	// rA |= imm32
	PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext, PPCREC_IML_OP_XOR, gprDestReg, (sint32)imm, 0, false, false, PPC_REC_INVALID_REGISTER, 0);
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
	PPCRecompilerImlGen_generateNewInstruction_macro(ppcImlGenContext, PPCREC_IML_MACRO_HLE, ppcImlGenContext->ppcAddressOfCurrentInstruction, hleFuncId, 0);
	return true;
}

uint32 PPCRecompiler_iterateCurrentInstruction(ppcImlGenContext_t* ppcImlGenContext)
{
	uint32 v = CPU_swapEndianU32(*(ppcImlGenContext->currentInstruction));
	ppcImlGenContext->currentInstruction += 1;
	return v;
}

uint32 PPCRecompiler_getInstructionByOffset(ppcImlGenContext_t* ppcImlGenContext, uint32 offset)
{
	uint32 v = CPU_swapEndianU32(*(ppcImlGenContext->currentInstruction + offset/4));
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

char _tempOpcodename[32];

const char* PPCRecompiler_getOpcodeDebugName(PPCRecImlInstruction_t* iml)
{
	uint32 op = iml->operation;
	if (op == PPCREC_IML_OP_ASSIGN)
		return "MOV";
	else if (op == PPCREC_IML_OP_ADD)
		return "ADD";
	else if (op == PPCREC_IML_OP_SUB)
		return "SUB";
	else if (op == PPCREC_IML_OP_ADD_CARRY_UPDATE_CARRY)
		return "ADDCSC";
	else if (op == PPCREC_IML_OP_OR)
		return "OR";
	else if (op == PPCREC_IML_OP_AND)
		return "AND";
	else if (op == PPCREC_IML_OP_XOR)
		return "XOR";
	else if (op == PPCREC_IML_OP_LEFT_SHIFT)
		return "LSH";
	else if (op == PPCREC_IML_OP_RIGHT_SHIFT)
		return "RSH";
	else if (op == PPCREC_IML_OP_MULTIPLY_SIGNED)
		return "MULS";
	else if (op == PPCREC_IML_OP_DIVIDE_SIGNED)
		return "DIVS";

	sprintf(_tempOpcodename, "OP0%02x_T%d", iml->operation, iml->type);
	return _tempOpcodename;
}

void PPCRecDebug_addRegisterParam(StringBuf& strOutput, sint32 virtualRegister, bool isLast = false)
{
	if (isLast)
	{
		if (virtualRegister < 10)
			strOutput.addFmt("t{} ", virtualRegister);
		else
			strOutput.addFmt("t{}", virtualRegister);
		return;
	}
	if (virtualRegister < 10)
		strOutput.addFmt("t{} , ", virtualRegister);
	else
		strOutput.addFmt("t{}, ", virtualRegister);
}

void PPCRecDebug_addS32Param(StringBuf& strOutput, sint32 val, bool isLast = false)
{
	if (isLast)
	{
		strOutput.addFmt("0x{:08x}", val);
		return;
	}
	strOutput.addFmt("0x{:08x}, ", val);
}

void PPCRecompilerDebug_printLivenessRangeInfo(StringBuf& currentLineText, PPCRecImlSegment_t* imlSegment, sint32 offset)
{
	// pad to 70 characters
	sint32 index = currentLineText.getLen();
	while (index < 70)
	{
		debug_printf(" ");
		index++;
	}
	raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while (subrangeItr)
	{
		if (offset == subrangeItr->start.index)
		{
			if (false)//subrange->isDirtied && i == subrange->becomesDirtyAtIndex.index)
			{
				debug_printf("*%-2d", subrangeItr->range->virtualRegister);
			}
			else
			{
				debug_printf("|%-2d", subrangeItr->range->virtualRegister);
			}
		}
		else if (false)//subrange->isDirtied && i == subrange->becomesDirtyAtIndex.index )
		{
			debug_printf("*  ");
		}
		else if (offset >= subrangeItr->start.index && offset < subrangeItr->end.index)
		{
			debug_printf("|  ");
		}
		else
		{
			debug_printf("   ");
		}
		index += 3;
		// next
		subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
	}
}

void PPCRecompiler_dumpIMLSegment(PPCRecImlSegment_t* imlSegment, sint32 segmentIndex, bool printLivenessRangeInfo)
{
	StringBuf strOutput(1024);

	strOutput.addFmt("SEGMENT 0x{:04x} 0x{:08x} PPC 0x{:08x} - 0x{:08x} Loop-depth {}", segmentIndex, imlSegment->ppcAddress, imlSegment->ppcAddrMin, imlSegment->ppcAddrMax, imlSegment->loopDepth);
	if (imlSegment->isEnterable)
	{
		strOutput.addFmt(" ENTERABLE (0x{:08x})", imlSegment->enterPPCAddress);
	}
	else if( imlSegment->isJumpDestination )
	{
		strOutput.addFmt(" JUMP-DEST (0x{:08x})", imlSegment->jumpDestinationPPCAddress);
	}

	debug_printf("%s\n", strOutput.c_str());

	strOutput.reset();
	strOutput.addFmt("SEGMENT NAME 0x{:016x}", (uintptr_t)imlSegment);
	debug_printf("%s", strOutput.c_str());

	if (printLivenessRangeInfo)
	{
		PPCRecompilerDebug_printLivenessRangeInfo(strOutput, imlSegment, RA_INTER_RANGE_START);
	}
	debug_printf("\n");

	sint32 lineOffsetParameters = 18;

	for(sint32 i=0; i<imlSegment->imlListCount; i++)
	{
		// don't log NOP instructions unless they have an associated PPC address
		if(imlSegment->imlList[i].type == PPCREC_IML_TYPE_NO_OP && imlSegment->imlList[i].associatedPPCAddress == MPTR_NULL)
			continue;
		strOutput.reset();
		strOutput.addFmt("{:08x} ", imlSegment->imlList[i].associatedPPCAddress);
		if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_R_NAME || imlSegment->imlList[i].type == PPCREC_IML_TYPE_NAME_R)
		{
			if(imlSegment->imlList[i].type == PPCREC_IML_TYPE_R_NAME)
				strOutput.add("LD_NAME");
			else
				strOutput.add("ST_NAME");
			while ((sint32)strOutput.getLen() < lineOffsetParameters)
				strOutput.add(" ");

			PPCRecDebug_addRegisterParam(strOutput, imlSegment->imlList[i].op_r_name.registerIndex);

			strOutput.addFmt("name_{} (", imlSegment->imlList[i].op_r_name.registerIndex, imlSegment->imlList[i].op_r_name.name);
			if( imlSegment->imlList[i].op_r_name.name >= PPCREC_NAME_R0 && imlSegment->imlList[i].op_r_name.name < (PPCREC_NAME_R0+999) )
			{
				strOutput.addFmt("r{}", imlSegment->imlList[i].op_r_name.name-PPCREC_NAME_R0);
			}
			else if( imlSegment->imlList[i].op_r_name.name >= PPCREC_NAME_SPR0 && imlSegment->imlList[i].op_r_name.name < (PPCREC_NAME_SPR0+999) )
			{
				strOutput.addFmt("spr{}", imlSegment->imlList[i].op_r_name.name-PPCREC_NAME_SPR0);
			}
			else
				strOutput.add("ukn");
			strOutput.add(")");
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_R_R )
		{
			strOutput.addFmt("{}", PPCRecompiler_getOpcodeDebugName(imlSegment->imlList+i));
			while ((sint32)strOutput.getLen() < lineOffsetParameters)
				strOutput.add(" ");
			PPCRecDebug_addRegisterParam(strOutput, imlSegment->imlList[i].op_r_r.registerResult);
			PPCRecDebug_addRegisterParam(strOutput, imlSegment->imlList[i].op_r_r.registerA, true);

			if( imlSegment->imlList[i].crRegister != PPC_REC_INVALID_REGISTER )
			{
				strOutput.addFmt(" -> CR{}", imlSegment->imlList[i].crRegister);
			}
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_R_R_R )
		{
			strOutput.addFmt("{}", PPCRecompiler_getOpcodeDebugName(imlSegment->imlList + i));
			while ((sint32)strOutput.getLen() < lineOffsetParameters)
				strOutput.add(" ");
			PPCRecDebug_addRegisterParam(strOutput, imlSegment->imlList[i].op_r_r_r.registerResult);
			PPCRecDebug_addRegisterParam(strOutput, imlSegment->imlList[i].op_r_r_r.registerA);
			PPCRecDebug_addRegisterParam(strOutput, imlSegment->imlList[i].op_r_r_r.registerB, true);
			if( imlSegment->imlList[i].crRegister != PPC_REC_INVALID_REGISTER )
			{
				strOutput.addFmt(" -> CR{}", imlSegment->imlList[i].crRegister);
			}
		}
		else if (imlSegment->imlList[i].type == PPCREC_IML_TYPE_R_R_S32)
		{
			strOutput.addFmt("{}", PPCRecompiler_getOpcodeDebugName(imlSegment->imlList + i));
			while ((sint32)strOutput.getLen() < lineOffsetParameters)
				strOutput.add(" ");

			PPCRecDebug_addRegisterParam(strOutput, imlSegment->imlList[i].op_r_r_s32.registerResult);
			PPCRecDebug_addRegisterParam(strOutput, imlSegment->imlList[i].op_r_r_s32.registerA);
			PPCRecDebug_addS32Param(strOutput, imlSegment->imlList[i].op_r_r_s32.immS32, true);

			if (imlSegment->imlList[i].crRegister != PPC_REC_INVALID_REGISTER)
			{
				strOutput.addFmt(" -> CR{}", imlSegment->imlList[i].crRegister);
			}
		}
		else if (imlSegment->imlList[i].type == PPCREC_IML_TYPE_R_S32)
		{
			strOutput.addFmt("{}", PPCRecompiler_getOpcodeDebugName(imlSegment->imlList + i));
			while ((sint32)strOutput.getLen() < lineOffsetParameters)
				strOutput.add(" ");

			PPCRecDebug_addRegisterParam(strOutput, imlSegment->imlList[i].op_r_immS32.registerIndex);
			PPCRecDebug_addS32Param(strOutput, imlSegment->imlList[i].op_r_immS32.immS32, true);

			if (imlSegment->imlList[i].crRegister != PPC_REC_INVALID_REGISTER)
			{
				strOutput.addFmt(" -> CR{}", imlSegment->imlList[i].crRegister);
			}
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_JUMPMARK )
		{
			strOutput.addFmt("jm_{:08x}:", imlSegment->imlList[i].op_jumpmark.address);
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_PPC_ENTER )
		{
			strOutput.addFmt("ppcEnter_{:08x}:", imlSegment->imlList[i].op_ppcEnter.ppcAddress);
		}
		else if(imlSegment->imlList[i].type == PPCREC_IML_TYPE_LOAD || imlSegment->imlList[i].type == PPCREC_IML_TYPE_STORE ||
			imlSegment->imlList[i].type == PPCREC_IML_TYPE_LOAD_INDEXED || imlSegment->imlList[i].type == PPCREC_IML_TYPE_STORE_INDEXED )
		{
			if(imlSegment->imlList[i].type == PPCREC_IML_TYPE_LOAD || imlSegment->imlList[i].type == PPCREC_IML_TYPE_LOAD_INDEXED)
				strOutput.add("LD_");
			else
				strOutput.add("ST_");

			if (imlSegment->imlList[i].op_storeLoad.flags2.signExtend)
				strOutput.add("S");
			else
				strOutput.add("U");
			strOutput.addFmt("{}", imlSegment->imlList[i].op_storeLoad.copyWidth);
			
			while ((sint32)strOutput.getLen() < lineOffsetParameters)
				strOutput.add(" ");

			PPCRecDebug_addRegisterParam(strOutput, imlSegment->imlList[i].op_storeLoad.registerData);
			
			if(imlSegment->imlList[i].type == PPCREC_IML_TYPE_LOAD_INDEXED || imlSegment->imlList[i].type == PPCREC_IML_TYPE_STORE_INDEXED)
				strOutput.addFmt("[t{}+t{}]", imlSegment->imlList[i].op_storeLoad.registerMem, imlSegment->imlList[i].op_storeLoad.registerMem2);
			else
				strOutput.addFmt("[t{}+{}]", imlSegment->imlList[i].op_storeLoad.registerMem, imlSegment->imlList[i].op_storeLoad.immS32);
		}
		else if (imlSegment->imlList[i].type == PPCREC_IML_TYPE_MEM2MEM)
		{
			strOutput.addFmt("{} [t{}+{}] = [t{}+{}]", imlSegment->imlList[i].op_mem2mem.copyWidth, imlSegment->imlList[i].op_mem2mem.dst.registerMem, imlSegment->imlList[i].op_mem2mem.dst.immS32, imlSegment->imlList[i].op_mem2mem.src.registerMem, imlSegment->imlList[i].op_mem2mem.src.immS32);
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_CJUMP )
		{
			if (imlSegment->imlList[i].op_conditionalJump.condition == PPCREC_JUMP_CONDITION_E)
				strOutput.add("JE");
			else if (imlSegment->imlList[i].op_conditionalJump.condition == PPCREC_JUMP_CONDITION_NE)
				strOutput.add("JNE");
			else if (imlSegment->imlList[i].op_conditionalJump.condition == PPCREC_JUMP_CONDITION_G)
				strOutput.add("JG");
			else if (imlSegment->imlList[i].op_conditionalJump.condition == PPCREC_JUMP_CONDITION_GE)
				strOutput.add("JGE");
			else if (imlSegment->imlList[i].op_conditionalJump.condition == PPCREC_JUMP_CONDITION_L)
				strOutput.add("JL");
			else if (imlSegment->imlList[i].op_conditionalJump.condition == PPCREC_JUMP_CONDITION_LE)
				strOutput.add("JLE");
			else if (imlSegment->imlList[i].op_conditionalJump.condition == PPCREC_JUMP_CONDITION_NONE)
				strOutput.add("JALW"); // jump always
			else
				cemu_assert_unimplemented();
			strOutput.addFmt(" jm_{:08x} (cr{})", imlSegment->imlList[i].op_conditionalJump.jumpmarkAddress, imlSegment->imlList[i].crRegister);
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_NO_OP )
		{
		strOutput.add("NOP");
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_MACRO )
		{
			if( imlSegment->imlList[i].operation == PPCREC_IML_MACRO_BLR )
			{
				strOutput.addFmt("MACRO BLR 0x{:08x} cycles (depr): {}", imlSegment->imlList[i].op_macro.param, (sint32)imlSegment->imlList[i].op_macro.paramU16);
			}
			else if( imlSegment->imlList[i].operation == PPCREC_IML_MACRO_BLRL )
			{
				strOutput.addFmt("MACRO BLRL 0x{:08x} cycles (depr): {}", imlSegment->imlList[i].op_macro.param, (sint32)imlSegment->imlList[i].op_macro.paramU16);
			}
			else if( imlSegment->imlList[i].operation == PPCREC_IML_MACRO_BCTR )
			{
				strOutput.addFmt("MACRO BCTR 0x{:08x} cycles (depr): {}", imlSegment->imlList[i].op_macro.param, (sint32)imlSegment->imlList[i].op_macro.paramU16);
			}
			else if( imlSegment->imlList[i].operation == PPCREC_IML_MACRO_BCTRL )
			{
				strOutput.addFmt("MACRO BCTRL 0x{:08x} cycles (depr): {}", imlSegment->imlList[i].op_macro.param, (sint32)imlSegment->imlList[i].op_macro.paramU16);
			}
			else if( imlSegment->imlList[i].operation == PPCREC_IML_MACRO_BL )
			{
				strOutput.addFmt("MACRO BL 0x{:08x} -> 0x{:08x} cycles (depr): {}", imlSegment->imlList[i].op_macro.param, imlSegment->imlList[i].op_macro.param2, (sint32)imlSegment->imlList[i].op_macro.paramU16);
			}
			else if( imlSegment->imlList[i].operation == PPCREC_IML_MACRO_B_FAR )
			{
				strOutput.addFmt("MACRO B_FAR 0x{:08x} -> 0x{:08x} cycles (depr): {}", imlSegment->imlList[i].op_macro.param, imlSegment->imlList[i].op_macro.param2, (sint32)imlSegment->imlList[i].op_macro.paramU16);
			}
			else if( imlSegment->imlList[i].operation == PPCREC_IML_MACRO_LEAVE )
			{
				strOutput.addFmt("MACRO LEAVE ppc: 0x{:08x}", imlSegment->imlList[i].op_macro.param);
			}
			else if( imlSegment->imlList[i].operation == PPCREC_IML_MACRO_HLE )
			{
				strOutput.addFmt("MACRO HLE ppcAddr: 0x{:08x} funcId: 0x{:08x}", imlSegment->imlList[i].op_macro.param, imlSegment->imlList[i].op_macro.param2);
			}
			else if( imlSegment->imlList[i].operation == PPCREC_IML_MACRO_MFTB )
			{
				strOutput.addFmt("MACRO MFTB ppcAddr: 0x{:08x} sprId: 0x{:08x}", imlSegment->imlList[i].op_macro.param, imlSegment->imlList[i].op_macro.param2);
			}
			else if( imlSegment->imlList[i].operation == PPCREC_IML_MACRO_COUNT_CYCLES )
			{
				strOutput.addFmt("MACRO COUNT_CYCLES cycles: {}", imlSegment->imlList[i].op_macro.param);
			}
			else
			{
				strOutput.addFmt("MACRO ukn operation {}", imlSegment->imlList[i].operation);
			}
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_FPR_R_NAME )
		{
			strOutput.addFmt("fpr_t{} = name_{} (", imlSegment->imlList[i].op_r_name.registerIndex, imlSegment->imlList[i].op_r_name.name);
			if( imlSegment->imlList[i].op_r_name.name >= PPCREC_NAME_FPR0 && imlSegment->imlList[i].op_r_name.name < (PPCREC_NAME_FPR0+999) )
			{
				strOutput.addFmt("fpr{}", imlSegment->imlList[i].op_r_name.name-PPCREC_NAME_FPR0);
			}
			else if( imlSegment->imlList[i].op_r_name.name >= PPCREC_NAME_TEMPORARY_FPR0 && imlSegment->imlList[i].op_r_name.name < (PPCREC_NAME_TEMPORARY_FPR0+999) )
			{
				strOutput.addFmt("tempFpr{}", imlSegment->imlList[i].op_r_name.name-PPCREC_NAME_TEMPORARY_FPR0);
			}
			else
				strOutput.add("ukn");
			strOutput.add(")");
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_FPR_NAME_R )
		{
			strOutput.addFmt("name_{} (", imlSegment->imlList[i].op_r_name.name);
			if( imlSegment->imlList[i].op_r_name.name >= PPCREC_NAME_FPR0 && imlSegment->imlList[i].op_r_name.name < (PPCREC_NAME_FPR0+999) )
			{
				strOutput.addFmt("fpr{}", imlSegment->imlList[i].op_r_name.name-PPCREC_NAME_FPR0);
			}
			else if( imlSegment->imlList[i].op_r_name.name >= PPCREC_NAME_TEMPORARY_FPR0 && imlSegment->imlList[i].op_r_name.name < (PPCREC_NAME_TEMPORARY_FPR0+999) )
			{
				strOutput.addFmt("tempFpr{}", imlSegment->imlList[i].op_r_name.name-PPCREC_NAME_TEMPORARY_FPR0);
			}
			else
				strOutput.add("ukn");
			strOutput.addFmt(") = fpr_t{}", imlSegment->imlList[i].op_r_name.registerIndex);
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_FPR_LOAD )
		{
			strOutput.addFmt("fpr_t{} = ", imlSegment->imlList[i].op_storeLoad.registerData);
			if( imlSegment->imlList[i].op_storeLoad.flags2.signExtend )
				strOutput.add("S");
			else
				strOutput.add("U");
			strOutput.addFmt("{} [t{}+{}] mode {}", imlSegment->imlList[i].op_storeLoad.copyWidth / 8, imlSegment->imlList[i].op_storeLoad.registerMem, imlSegment->imlList[i].op_storeLoad.immS32, imlSegment->imlList[i].op_storeLoad.mode);
			if (imlSegment->imlList[i].op_storeLoad.flags2.notExpanded)
			{
				strOutput.addFmt(" <No expand>");
			}
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_FPR_STORE )
		{
			if( imlSegment->imlList[i].op_storeLoad.flags2.signExtend )
				strOutput.add("S");
			else
				strOutput.add("U");
			strOutput.addFmt("{} [t{}+{}]", imlSegment->imlList[i].op_storeLoad.copyWidth/8, imlSegment->imlList[i].op_storeLoad.registerMem, imlSegment->imlList[i].op_storeLoad.immS32);
			strOutput.addFmt("= fpr_t{} mode {}\n", imlSegment->imlList[i].op_storeLoad.registerData, imlSegment->imlList[i].op_storeLoad.mode);
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_FPR_R_R )
		{
			strOutput.addFmt("{:-6} ", PPCRecompiler_getOpcodeDebugName(&imlSegment->imlList[i]));
			strOutput.addFmt("fpr{:02d}, fpr{:02d}", imlSegment->imlList[i].op_fpr_r_r.registerResult, imlSegment->imlList[i].op_fpr_r_r.registerOperand);
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_FPR_R_R_R_R )
		{
			strOutput.addFmt("{:-6} ", PPCRecompiler_getOpcodeDebugName(&imlSegment->imlList[i]));
			strOutput.addFmt("fpr{:02d}, fpr{:02d}, fpr{:02d}, fpr{:02d}", imlSegment->imlList[i].op_fpr_r_r_r_r.registerResult, imlSegment->imlList[i].op_fpr_r_r_r_r.registerOperandA, imlSegment->imlList[i].op_fpr_r_r_r_r.registerOperandB, imlSegment->imlList[i].op_fpr_r_r_r_r.registerOperandC);
		}
		else if( imlSegment->imlList[i].type == PPCREC_IML_TYPE_FPR_R_R_R )
		{
			strOutput.addFmt("{:-6} ", PPCRecompiler_getOpcodeDebugName(&imlSegment->imlList[i]));
			strOutput.addFmt("fpr{:02d}, fpr{:02d}, fpr{:02d}", imlSegment->imlList[i].op_fpr_r_r_r.registerResult, imlSegment->imlList[i].op_fpr_r_r_r.registerOperandA, imlSegment->imlList[i].op_fpr_r_r_r.registerOperandB);
		}
		else if (imlSegment->imlList[i].type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
		{
			strOutput.addFmt("CYCLE_CHECK jm_{:08x}\n", imlSegment->imlList[i].op_conditionalJump.jumpmarkAddress);
		}
		else if (imlSegment->imlList[i].type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
		{
			strOutput.addFmt("t{} ", imlSegment->imlList[i].op_conditional_r_s32.registerIndex);
			bool displayAsHex = false;
			if (imlSegment->imlList[i].operation == PPCREC_IML_OP_ASSIGN)
			{
				displayAsHex = true;
				strOutput.add("=");
			}
			else
				strOutput.addFmt("(unknown operation CONDITIONAL_R_S32 {})", imlSegment->imlList[i].operation);
			if (displayAsHex)
				strOutput.addFmt(" 0x{:x}", imlSegment->imlList[i].op_conditional_r_s32.immS32);
			else
				strOutput.addFmt(" {}", imlSegment->imlList[i].op_conditional_r_s32.immS32);
			strOutput.add(" (conditional)");
			if (imlSegment->imlList[i].crRegister != PPC_REC_INVALID_REGISTER)
			{
				strOutput.addFmt(" -> and update CR{}", imlSegment->imlList[i].crRegister);
			}
		}
		else
		{
		strOutput.addFmt("Unknown iml type {}", imlSegment->imlList[i].type);
		}
		debug_printf("%s", strOutput.c_str());
		if (printLivenessRangeInfo)
		{
			PPCRecompilerDebug_printLivenessRangeInfo(strOutput, imlSegment, i);
		}
		debug_printf("\n");
	}
	// all ranges
	if (printLivenessRangeInfo)
	{
		debug_printf("Ranges-VirtReg                                                        ");
		raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while(subrangeItr)
		{
			debug_printf("v%-2d", subrangeItr->range->virtualRegister);
			subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
		}
		debug_printf("\n");
		debug_printf("Ranges-PhysReg                                                        ");
		subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while (subrangeItr)
		{
			debug_printf("p%-2d", subrangeItr->range->physicalRegister);
			subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
		}
		debug_printf("\n");
	}
	// branch info
	debug_printf("Links from: ");
	for (sint32 i = 0; i < imlSegment->list_prevSegments.size(); i++)
	{
		if (i)
			debug_printf(", ");
		debug_printf("%p", (void*)imlSegment->list_prevSegments[i]);
	}
	debug_printf("\n");
	debug_printf("Links to: ");
	if (imlSegment->nextSegmentBranchNotTaken)
		debug_printf("%p (no branch), ", (void*)imlSegment->nextSegmentBranchNotTaken);
	if (imlSegment->nextSegmentBranchTaken)
		debug_printf("%p (branch)", (void*)imlSegment->nextSegmentBranchTaken);
	debug_printf("\n");
}

void PPCRecompiler_dumpIML(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext)
{
	for(sint32 f=0; f<ppcImlGenContext->segmentListCount; f++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[f];
		PPCRecompiler_dumpIMLSegment(imlSegment, f);
		debug_printf("\n");
	}
}

void PPCRecompilerIml_setSegmentPoint(ppcRecompilerSegmentPoint_t* segmentPoint, PPCRecImlSegment_t* imlSegment, sint32 index)
{
	segmentPoint->imlSegment = imlSegment;
	segmentPoint->index = index;
	if (imlSegment->segmentPointList)
		imlSegment->segmentPointList->prev = segmentPoint;
	segmentPoint->prev = nullptr;
	segmentPoint->next = imlSegment->segmentPointList;
	imlSegment->segmentPointList = segmentPoint;
}

void PPCRecompilerIml_removeSegmentPoint(ppcRecompilerSegmentPoint_t* segmentPoint)
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
void PPCRecompiler_pushBackIMLInstructions(PPCRecImlSegment_t* imlSegment, sint32 index, sint32 shiftBackCount)
{
	cemu_assert(index >= 0 && index <= imlSegment->imlListCount);

	if (imlSegment->imlListCount + shiftBackCount > imlSegment->imlListSize)
	{
		sint32 newSize = imlSegment->imlListCount + shiftBackCount + std::max(2, imlSegment->imlListSize/2);
		imlSegment->imlList = (PPCRecImlInstruction_t*)realloc(imlSegment->imlList, sizeof(PPCRecImlInstruction_t)*newSize);
		imlSegment->imlListSize = newSize;
	}
	for (sint32 i = (sint32)imlSegment->imlListCount - 1; i >= index; i--)
	{
		memcpy(imlSegment->imlList + (i + shiftBackCount), imlSegment->imlList + i, sizeof(PPCRecImlInstruction_t));
	}
	// fill empty space with NOP instructions
	for (sint32 i = 0; i < shiftBackCount; i++)
	{
		imlSegment->imlList[index + i].type = PPCREC_IML_TYPE_NONE;
	}
	imlSegment->imlListCount += shiftBackCount;

	if (imlSegment->segmentPointList)
	{
		ppcRecompilerSegmentPoint_t* segmentPoint = imlSegment->segmentPointList;
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

/*
* Insert and return new instruction at index
* Warning: Can invalidate any previous instruction structs from the same segment
*/
PPCRecImlInstruction_t* PPCRecompiler_insertInstruction(PPCRecImlSegment_t* imlSegment, sint32 index)
{
	PPCRecompiler_pushBackIMLInstructions(imlSegment, index, 1);
	return imlSegment->imlList + index;
}

/*
* Append and return new instruction at the end of the segment
* Warning: Can invalidate any previous instruction structs from the same segment
*/
PPCRecImlInstruction_t* PPCRecompiler_appendInstruction(PPCRecImlSegment_t* imlSegment)
{
	sint32 index = imlSegment->imlListCount;
	if (index >= imlSegment->imlListSize)
	{
		sint32 newSize = index+1;
		imlSegment->imlList = (PPCRecImlInstruction_t*)realloc(imlSegment->imlList, sizeof(PPCRecImlInstruction_t)*newSize);
		imlSegment->imlListSize = newSize;
	}
	imlSegment->imlListCount++;
	memset(imlSegment->imlList + index, 0, sizeof(PPCRecImlInstruction_t));
	return imlSegment->imlList + index;
}

void PPCRecompilerIml_insertSegments(ppcImlGenContext_t* ppcImlGenContext, sint32 index, sint32 count)
{
	if( (ppcImlGenContext->segmentListCount+count) > ppcImlGenContext->segmentListSize )
	{
		// allocate space for more segments
		ppcImlGenContext->segmentListSize += count;
		ppcImlGenContext->segmentList = (PPCRecImlSegment_t**)realloc(ppcImlGenContext->segmentList, ppcImlGenContext->segmentListSize*sizeof(PPCRecImlSegment_t*));
	}
	for(sint32 i=(sint32)ppcImlGenContext->segmentListCount-1; i>=index; i--)
	{
		memcpy(ppcImlGenContext->segmentList+(i+count), ppcImlGenContext->segmentList+i, sizeof(PPCRecImlSegment_t*));
	}
	ppcImlGenContext->segmentListCount += count;
	for(sint32 i=0; i<count; i++)
	{
		//memset(ppcImlGenContext->segmentList+index+i, 0x00, sizeof(PPCRecImlSegment_t*));
		ppcImlGenContext->segmentList[index+i] = (PPCRecImlSegment_t*)malloc(sizeof(PPCRecImlSegment_t));
		memset(ppcImlGenContext->segmentList[index+i], 0x00, sizeof(PPCRecImlSegment_t));
		ppcImlGenContext->segmentList[index + i]->list_prevSegments = std::vector<PPCRecImlSegment_t*>();
	}
}

/*
 * Allocate and init a new iml instruction segment
 */
PPCRecImlSegment_t* PPCRecompiler_generateImlSegment(ppcImlGenContext_t* ppcImlGenContext)
{
	if( ppcImlGenContext->segmentListCount >= ppcImlGenContext->segmentListSize )
	{
		// allocate space for more segments
		ppcImlGenContext->segmentListSize *= 2;
		ppcImlGenContext->segmentList = (PPCRecImlSegment_t**)realloc(ppcImlGenContext->segmentList, ppcImlGenContext->segmentListSize*sizeof(PPCRecImlSegment_t*));
	}
	PPCRecImlSegment_t* ppcRecSegment = new PPCRecImlSegment_t();
	ppcImlGenContext->segmentList[ppcImlGenContext->segmentListCount] = ppcRecSegment;
	ppcImlGenContext->segmentListCount++;
	return ppcRecSegment;
}

void PPCRecompiler_freeContext(ppcImlGenContext_t* ppcImlGenContext)
{
	if (ppcImlGenContext->imlList)
	{
		free(ppcImlGenContext->imlList);
		ppcImlGenContext->imlList = nullptr;
	}
	for(sint32 i=0; i<ppcImlGenContext->segmentListCount; i++)
	{
		free(ppcImlGenContext->segmentList[i]->imlList);
		delete ppcImlGenContext->segmentList[i];
	}
	ppcImlGenContext->segmentListCount = 0;
	if (ppcImlGenContext->segmentList)
	{
		free(ppcImlGenContext->segmentList);
		ppcImlGenContext->segmentList = nullptr;
	}
}

bool PPCRecompiler_isSuffixInstruction(PPCRecImlInstruction_t* iml)
{
	if (iml->type == PPCREC_IML_TYPE_MACRO && (iml->operation == PPCREC_IML_MACRO_BLR || iml->operation == PPCREC_IML_MACRO_BCTR) ||
		iml->type == PPCREC_IML_TYPE_MACRO && iml->operation == PPCREC_IML_MACRO_BL ||
		iml->type == PPCREC_IML_TYPE_MACRO && iml->operation == PPCREC_IML_MACRO_B_FAR ||
		iml->type == PPCREC_IML_TYPE_MACRO && iml->operation == PPCREC_IML_MACRO_BLRL ||
		iml->type == PPCREC_IML_TYPE_MACRO && iml->operation == PPCREC_IML_MACRO_BCTRL ||
		iml->type == PPCREC_IML_TYPE_MACRO && iml->operation == PPCREC_IML_MACRO_LEAVE ||
		iml->type == PPCREC_IML_TYPE_MACRO && iml->operation == PPCREC_IML_MACRO_HLE ||
		iml->type == PPCREC_IML_TYPE_MACRO && iml->operation == PPCREC_IML_MACRO_MFTB ||
		iml->type == PPCREC_IML_TYPE_PPC_ENTER ||
		iml->type == PPCREC_IML_TYPE_CJUMP ||
		iml->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
		return true;
	return false;
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
		case 16:
			if (PPCRecompilerImlGen_BCLR(ppcImlGenContext, opcode) == false)
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
		case 528:
			if (PPCRecompilerImlGen_BCCTR(ppcImlGenContext, opcode) == false)
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

bool PPCRecompiler_generateIntermediateCode(ppcImlGenContext_t& ppcImlGenContext, PPCRecFunction_t* ppcRecFunc, std::set<uint32>& entryAddresses)
{
	//ppcImlGenContext_t ppcImlGenContext = { 0 };
	ppcImlGenContext.functionRef = ppcRecFunc;
	// add entire range
	ppcRecRange_t recRange;
	recRange.ppcAddress = ppcRecFunc->ppcAddress;
	recRange.ppcSize = ppcRecFunc->ppcSize;
	ppcRecFunc->list_ranges.push_back(recRange);
	// process ppc instructions
	ppcImlGenContext.currentInstruction = (uint32*)memory_getPointerFromVirtualOffset(ppcRecFunc->ppcAddress);
	bool unsupportedInstructionFound = false;
	sint32 numPPCInstructions = ppcRecFunc->ppcSize/4;
	sint32 unsupportedInstructionCount = 0;
	uint32 unsupportedInstructionLastOffset = 0;
	uint32* firstCurrentInstruction = ppcImlGenContext.currentInstruction;
	uint32* endCurrentInstruction = ppcImlGenContext.currentInstruction + numPPCInstructions;
	
	while(ppcImlGenContext.currentInstruction < endCurrentInstruction)
	{
		uint32 addressOfCurrentInstruction = (uint32)((uint8*)ppcImlGenContext.currentInstruction - memory_base);
		ppcImlGenContext.ppcAddressOfCurrentInstruction = addressOfCurrentInstruction;
		ppcImlGenContext.cyclesSinceLastBranch++;
		PPCRecompilerImlGen_generateNewInstruction_jumpmark(&ppcImlGenContext, addressOfCurrentInstruction);
		
		if (entryAddresses.find(addressOfCurrentInstruction) != entryAddresses.end())
		{
			// add PPCEnter for addresses that are in entryAddresses
			PPCRecompilerImlGen_generateNewInstruction_ppcEnter(&ppcImlGenContext, addressOfCurrentInstruction);
		}
		else if(ppcImlGenContext.currentInstruction != firstCurrentInstruction)
		{
			// add PPCEnter mark if code is seemingly unreachable (for example if between two unconditional jump instructions without jump goal)
			uint32 opcodeCurrent = PPCRecompiler_getCurrentInstruction(&ppcImlGenContext);
			uint32 opcodePrevious = PPCRecompiler_getPreviousInstruction(&ppcImlGenContext);
			if( ((opcodePrevious>>26) == 18) && ((opcodeCurrent>>26) == 18) )
			{
				// between two B(L) instructions
				// todo: for BL only if they are not inlineable

				bool canInlineFunction = false;
				if ((opcodePrevious & PPC_OPC_LK) && (opcodePrevious & PPC_OPC_AA) == 0)
				{
					uint32 li;
					PPC_OPC_TEMPL_I(opcodePrevious, li);
					sint32 inlineSize = 0;
					if (PPCRecompiler_canInlineFunction(li + addressOfCurrentInstruction - 4, &inlineSize))
						canInlineFunction = true;
				}
				if( canInlineFunction == false && (opcodePrevious & PPC_OPC_LK) == false)
					PPCRecompilerImlGen_generateNewInstruction_ppcEnter(&ppcImlGenContext, addressOfCurrentInstruction);
			}
			if( ((opcodePrevious>>26) == 19) && PPC_getBits(opcodePrevious, 30, 10) == 528 )
			{
				uint32 BO, BI, BD;
				PPC_OPC_TEMPL_XL(opcodePrevious, BO, BI, BD);
				if( (BO & 16) && (opcodePrevious&PPC_OPC_LK) == 0 )
				{
					// after unconditional BCTR instruction
					PPCRecompilerImlGen_generateNewInstruction_ppcEnter(&ppcImlGenContext, addressOfCurrentInstruction);
				}
			}
		}

		unsupportedInstructionFound = PPCRecompiler_decodePPCInstruction(&ppcImlGenContext);
		if( unsupportedInstructionFound )
		{
			unsupportedInstructionCount++;
			unsupportedInstructionLastOffset = ppcImlGenContext.ppcAddressOfCurrentInstruction;
			unsupportedInstructionFound = false;
			//break;
		}
	}
	ppcImlGenContext.ppcAddressOfCurrentInstruction = 0; // reset current instruction offset (any future generated IML instruction will be assigned to ppc address 0)
	if( unsupportedInstructionCount > 0 || unsupportedInstructionFound )
	{
		// could not compile function
		debug_printf("Failed recompile due to unknown instruction at 0x%08x\n", unsupportedInstructionLastOffset);
		PPCRecompiler_freeContext(&ppcImlGenContext);
		return false;
	}
	// optimize unused jumpmarks away
	// first, flag all jumpmarks as unused
	std::map<uint32, PPCRecImlInstruction_t*> map_jumpMarks;
	for(sint32 i=0; i<ppcImlGenContext.imlListCount; i++)
	{
		if( ppcImlGenContext.imlList[i].type == PPCREC_IML_TYPE_JUMPMARK )
		{
			ppcImlGenContext.imlList[i].op_jumpmark.flags |= PPCREC_IML_OP_FLAG_UNUSED;
#ifdef CEMU_DEBUG_ASSERT
			if (map_jumpMarks.find(ppcImlGenContext.imlList[i].op_jumpmark.address) != map_jumpMarks.end())
				assert_dbg();
#endif
			map_jumpMarks.emplace(ppcImlGenContext.imlList[i].op_jumpmark.address, ppcImlGenContext.imlList+i);
		}
	}
	// second, unflag jumpmarks that have at least one reference
	for(sint32 i=0; i<ppcImlGenContext.imlListCount; i++)
	{
		if( ppcImlGenContext.imlList[i].type == PPCREC_IML_TYPE_CJUMP )
		{
			uint32 jumpDest = ppcImlGenContext.imlList[i].op_conditionalJump.jumpmarkAddress;
			auto jumpMarkIml = map_jumpMarks.find(jumpDest);
			if (jumpMarkIml != map_jumpMarks.end())
				jumpMarkIml->second->op_jumpmark.flags &= ~PPCREC_IML_OP_FLAG_UNUSED;
		}
	}
	// lastly, remove jumpmarks that still have the unused flag set
	sint32 currentImlIndex = 0;
	for(sint32 i=0; i<ppcImlGenContext.imlListCount; i++)
	{
		if( ppcImlGenContext.imlList[i].type == PPCREC_IML_TYPE_JUMPMARK && (ppcImlGenContext.imlList[i].op_jumpmark.flags&PPCREC_IML_OP_FLAG_UNUSED) )
		{
			continue; // skip this instruction
		}
		// move back instruction
		if( currentImlIndex < i )
		{
			memcpy(ppcImlGenContext.imlList+currentImlIndex, ppcImlGenContext.imlList+i, sizeof(PPCRecImlInstruction_t));
		}
		currentImlIndex++;
	}
	// fix intermediate instruction count
	ppcImlGenContext.imlListCount = currentImlIndex;
	// divide iml instructions into segments
	// each segment is defined by one or more instructions with no branches or jump destinations in between
	// a branch instruction may only be the very last instruction of a segment
	ppcImlGenContext.segmentListCount = 0;
	ppcImlGenContext.segmentListSize = 2;
	ppcImlGenContext.segmentList = (PPCRecImlSegment_t**)malloc(ppcImlGenContext.segmentListSize*sizeof(PPCRecImlSegment_t*));
	sint32 segmentStart = 0;
	sint32 segmentImlIndex = 0;
	while( segmentImlIndex < ppcImlGenContext.imlListCount )
	{
		bool genNewSegment = false;
		// segment definition: 
		//    If we encounter a branch instruction -> end of segment after current instruction
		//    If we encounter a jumpmark		   -> end of segment before current instruction
		//    If we encounter ppc_enter			   -> end of segment before current instruction
		if( ppcImlGenContext.imlList[segmentImlIndex].type == PPCREC_IML_TYPE_CJUMP ||
			(ppcImlGenContext.imlList[segmentImlIndex].type == PPCREC_IML_TYPE_MACRO && (ppcImlGenContext.imlList[segmentImlIndex].operation == PPCREC_IML_MACRO_BLR || ppcImlGenContext.imlList[segmentImlIndex].operation == PPCREC_IML_MACRO_BLRL || ppcImlGenContext.imlList[segmentImlIndex].operation == PPCREC_IML_MACRO_BCTR || ppcImlGenContext.imlList[segmentImlIndex].operation == PPCREC_IML_MACRO_BCTRL)) ||
			(ppcImlGenContext.imlList[segmentImlIndex].type == PPCREC_IML_TYPE_MACRO && (ppcImlGenContext.imlList[segmentImlIndex].operation == PPCREC_IML_MACRO_BL)) ||
			(ppcImlGenContext.imlList[segmentImlIndex].type == PPCREC_IML_TYPE_MACRO && (ppcImlGenContext.imlList[segmentImlIndex].operation == PPCREC_IML_MACRO_B_FAR)) ||
			(ppcImlGenContext.imlList[segmentImlIndex].type == PPCREC_IML_TYPE_MACRO && (ppcImlGenContext.imlList[segmentImlIndex].operation == PPCREC_IML_MACRO_LEAVE)) ||
			(ppcImlGenContext.imlList[segmentImlIndex].type == PPCREC_IML_TYPE_MACRO && (ppcImlGenContext.imlList[segmentImlIndex].operation == PPCREC_IML_MACRO_HLE)) ||
			(ppcImlGenContext.imlList[segmentImlIndex].type == PPCREC_IML_TYPE_MACRO && (ppcImlGenContext.imlList[segmentImlIndex].operation == PPCREC_IML_MACRO_MFTB)) )
		{
			// segment ends after current instruction
			PPCRecImlSegment_t* ppcRecSegment = PPCRecompiler_generateImlSegment(&ppcImlGenContext);
			ppcRecSegment->startOffset = segmentStart;
			ppcRecSegment->count = segmentImlIndex-segmentStart+1;
			ppcRecSegment->ppcAddress = 0xFFFFFFFF;
			segmentStart = segmentImlIndex+1;
		}
		else if( ppcImlGenContext.imlList[segmentImlIndex].type == PPCREC_IML_TYPE_JUMPMARK ||
			ppcImlGenContext.imlList[segmentImlIndex].type == PPCREC_IML_TYPE_PPC_ENTER )
		{
			// segment ends before current instruction
			if( segmentImlIndex > segmentStart )
			{
				PPCRecImlSegment_t* ppcRecSegment = PPCRecompiler_generateImlSegment(&ppcImlGenContext);
				ppcRecSegment->startOffset = segmentStart;
				ppcRecSegment->count = segmentImlIndex-segmentStart;
				ppcRecSegment->ppcAddress = 0xFFFFFFFF;
				segmentStart = segmentImlIndex;
			}
		}
		segmentImlIndex++;
	}
	if( segmentImlIndex != segmentStart )
	{
		// final segment
		PPCRecImlSegment_t* ppcRecSegment = PPCRecompiler_generateImlSegment(&ppcImlGenContext);
		ppcRecSegment->startOffset = segmentStart;
		ppcRecSegment->count = segmentImlIndex-segmentStart;
		ppcRecSegment->ppcAddress = 0xFFFFFFFF;
		segmentStart = segmentImlIndex;
	}
	// move iml instructions into the segments
	for(sint32 s=0; s<ppcImlGenContext.segmentListCount; s++)
	{
		uint32 imlStartIndex = ppcImlGenContext.segmentList[s]->startOffset;
		uint32 imlCount = ppcImlGenContext.segmentList[s]->count;
		if( imlCount > 0 )
		{
			ppcImlGenContext.segmentList[s]->imlListSize = imlCount + 4;
			ppcImlGenContext.segmentList[s]->imlList = (PPCRecImlInstruction_t*)malloc(sizeof(PPCRecImlInstruction_t)*ppcImlGenContext.segmentList[s]->imlListSize);
			ppcImlGenContext.segmentList[s]->imlListCount = imlCount;
			memcpy(ppcImlGenContext.segmentList[s]->imlList, ppcImlGenContext.imlList+imlStartIndex, sizeof(PPCRecImlInstruction_t)*imlCount);
		}
		else
		{
			// empty segments are allowed so we can handle multiple PPC entry addresses pointing to the same code
			ppcImlGenContext.segmentList[s]->imlList = NULL;
			ppcImlGenContext.segmentList[s]->imlListSize = 0;
			ppcImlGenContext.segmentList[s]->imlListCount = 0;
		}
		ppcImlGenContext.segmentList[s]->startOffset = 9999999;
		ppcImlGenContext.segmentList[s]->count = 9999999;
	}
	// clear segment-independent iml list
	free(ppcImlGenContext.imlList);
	ppcImlGenContext.imlList = NULL;
	ppcImlGenContext.imlListCount = 999999; // set to high number to force crash in case old code still uses ppcImlGenContext.imlList
	// calculate PPC address of each segment based on iml instructions inside that segment (we need this info to calculate how many cpu cycles each segment takes)
	for(sint32 s=0; s<ppcImlGenContext.segmentListCount; s++)
	{
		uint32 segmentPPCAddrMin = 0xFFFFFFFF;
		uint32 segmentPPCAddrMax = 0x00000000;
		for(sint32 i=0; i<ppcImlGenContext.segmentList[s]->imlListCount; i++)
		{
			if( ppcImlGenContext.segmentList[s]->imlList[i].associatedPPCAddress == 0 )
				continue;
			//if( ppcImlGenContext.segmentList[s]->imlList[i].type == PPCREC_IML_TYPE_JUMPMARK || ppcImlGenContext.segmentList[s]->imlList[i].type == PPCREC_IML_TYPE_NO_OP )
			//	continue; // jumpmarks and no-op instructions must not affect segment ppc address range
			segmentPPCAddrMin = std::min(ppcImlGenContext.segmentList[s]->imlList[i].associatedPPCAddress, segmentPPCAddrMin);
			segmentPPCAddrMax = std::max(ppcImlGenContext.segmentList[s]->imlList[i].associatedPPCAddress, segmentPPCAddrMax);
		}
		if( segmentPPCAddrMin != 0xFFFFFFFF )
		{
			ppcImlGenContext.segmentList[s]->ppcAddrMin = segmentPPCAddrMin;
			ppcImlGenContext.segmentList[s]->ppcAddrMax = segmentPPCAddrMax;
		}
		else
		{
			ppcImlGenContext.segmentList[s]->ppcAddrMin = 0;
			ppcImlGenContext.segmentList[s]->ppcAddrMax = 0;
		}
	}
	// certain instructions can change the segment state
	// ppcEnter instruction marks a segment as enterable (BL, BCTR, etc. instructions can enter at this location from outside)
	// jumpmarks mark the segment as a jump destination (within the same function)
	for(sint32 s=0; s<ppcImlGenContext.segmentListCount; s++)
	{
		while( ppcImlGenContext.segmentList[s]->imlListCount > 0 )
		{
			if( ppcImlGenContext.segmentList[s]->imlList[0].type == PPCREC_IML_TYPE_PPC_ENTER )
			{
				// mark segment as enterable
				if( ppcImlGenContext.segmentList[s]->isEnterable )
					assert_dbg(); // should not happen?
				ppcImlGenContext.segmentList[s]->isEnterable = true;
				ppcImlGenContext.segmentList[s]->enterPPCAddress = ppcImlGenContext.segmentList[s]->imlList[0].op_ppcEnter.ppcAddress;
				// remove ppc_enter instruction
				ppcImlGenContext.segmentList[s]->imlList[0].type = PPCREC_IML_TYPE_NO_OP;
				ppcImlGenContext.segmentList[s]->imlList[0].crRegister = PPC_REC_INVALID_REGISTER;
				ppcImlGenContext.segmentList[s]->imlList[0].associatedPPCAddress = 0;
			}
			else if( ppcImlGenContext.segmentList[s]->imlList[0].type == PPCREC_IML_TYPE_JUMPMARK )
			{
				// mark segment as jump destination
				if( ppcImlGenContext.segmentList[s]->isJumpDestination )
					assert_dbg(); // should not happen?
				ppcImlGenContext.segmentList[s]->isJumpDestination = true;
				ppcImlGenContext.segmentList[s]->jumpDestinationPPCAddress = ppcImlGenContext.segmentList[s]->imlList[0].op_jumpmark.address;
				// remove jumpmark instruction
				ppcImlGenContext.segmentList[s]->imlList[0].type = PPCREC_IML_TYPE_NO_OP;
				ppcImlGenContext.segmentList[s]->imlList[0].crRegister = PPC_REC_INVALID_REGISTER;
				ppcImlGenContext.segmentList[s]->imlList[0].associatedPPCAddress = 0;
			}
			else
				break;
		}
	}
	// the first segment is always enterable as the recompiled functions entrypoint
	ppcImlGenContext.segmentList[0]->isEnterable = true;
	ppcImlGenContext.segmentList[0]->enterPPCAddress = ppcImlGenContext.functionRef->ppcAddress;

	// link segments for further inter-segment optimization
	PPCRecompilerIML_linkSegments(&ppcImlGenContext);

	// optimization pass - replace segments with conditional MOVs if possible
	for (sint32 s = 0; s < ppcImlGenContext.segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext.segmentList[s];
		if (imlSegment->nextSegmentBranchNotTaken == NULL || imlSegment->nextSegmentBranchTaken == NULL)
			continue; // not a branching segment
		PPCRecImlInstruction_t* lastInstruction = PPCRecompilerIML_getLastInstruction(imlSegment);
		if (lastInstruction->type != PPCREC_IML_TYPE_CJUMP || lastInstruction->op_conditionalJump.crRegisterIndex != 0)
			continue;
		PPCRecImlSegment_t* conditionalSegment = imlSegment->nextSegmentBranchNotTaken;
		PPCRecImlSegment_t* finalSegment = imlSegment->nextSegmentBranchTaken;
		if(imlSegment->nextSegmentBranchTaken != imlSegment->nextSegmentBranchNotTaken->nextSegmentBranchNotTaken)
			continue;
		if (imlSegment->nextSegmentBranchNotTaken->imlListCount > 4)
			continue;
		if(conditionalSegment->list_prevSegments.size() != 1)
			continue; // the reduced segment must not be the target of any other branch
		if(conditionalSegment->isEnterable)
			continue;
		// check if the segment contains only iml instructions that can be turned into conditional moves (Value assignment, register assignment)
		bool canReduceSegment = true;
		for (sint32 f = 0; f < conditionalSegment->imlListCount; f++)
		{
			PPCRecImlInstruction_t* imlInstruction = conditionalSegment->imlList+f;
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
		
		PPCRecompilerImlGen_generateNewInstruction_noOp(&ppcImlGenContext, lastInstruction);

		// append conditional moves based on branch condition
		for (sint32 f = 0; f < conditionalSegment->imlListCount; f++)
		{
			PPCRecImlInstruction_t* imlInstruction = conditionalSegment->imlList + f;
			if (imlInstruction->type == PPCREC_IML_TYPE_R_S32 && imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
				PPCRecompilerImlGen_generateNewInstruction_conditional_r_s32(&ppcImlGenContext, PPCRecompiler_appendInstruction(imlSegment), PPCREC_IML_OP_ASSIGN, imlInstruction->op_r_immS32.registerIndex, imlInstruction->op_r_immS32.immS32, branchCond_crRegisterIndex, branchCond_crBitIndex, !branchCond_bitMustBeSet);
			else
				assert_dbg();
		}
		// update segment links
		// source segment: imlSegment, conditional/removed segment: conditionalSegment, final segment: finalSegment
		PPCRecompilerIML_removeLink(imlSegment, conditionalSegment);
		PPCRecompilerIML_removeLink(imlSegment, finalSegment);
		PPCRecompilerIML_removeLink(conditionalSegment, finalSegment);
		PPCRecompilerIml_setLinkBranchNotTaken(imlSegment, finalSegment);
		// remove all instructions from conditional segment
		conditionalSegment->imlListCount = 0;

		// if possible, merge imlSegment with finalSegment
		if (finalSegment->isEnterable == false && finalSegment->list_prevSegments.size() == 1)
		{
			// todo: Clean this up and move into separate function PPCRecompilerIML_mergeSegments()
			PPCRecompilerIML_removeLink(imlSegment, finalSegment);
			if (finalSegment->nextSegmentBranchNotTaken)
			{
				PPCRecImlSegment_t* tempSegment = finalSegment->nextSegmentBranchNotTaken;
				PPCRecompilerIML_removeLink(finalSegment, tempSegment);
				PPCRecompilerIml_setLinkBranchNotTaken(imlSegment, tempSegment);
			}
			if (finalSegment->nextSegmentBranchTaken)
			{
				PPCRecImlSegment_t* tempSegment = finalSegment->nextSegmentBranchTaken;
				PPCRecompilerIML_removeLink(finalSegment, tempSegment);
				PPCRecompilerIml_setLinkBranchTaken(imlSegment, tempSegment);
			}
			// copy IML instructions
			for (sint32 f = 0; f < finalSegment->imlListCount; f++)
			{
				memcpy(PPCRecompiler_appendInstruction(imlSegment), finalSegment->imlList + f, sizeof(PPCRecImlInstruction_t));
			}
			finalSegment->imlListCount = 0;

			//PPCRecompiler_dumpIML(ppcRecFunc, &ppcImlGenContext);
		}

		// todo: If possible, merge with the segment following conditionalSegment (merging is only possible if the segment is not an entry point or has no other jump sources)
	}

	// insert cycle counter instruction in every segment that has a cycle count greater zero
	for(sint32 s=0; s<ppcImlGenContext.segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext.segmentList[s];
		if( imlSegment->ppcAddrMin == 0 )
			continue;
		// count number of PPC instructions in segment
		// note: This algorithm correctly counts inlined functions but it doesn't count NO-OP instructions like ISYNC
		uint32 lastPPCInstAddr = 0;
		uint32 ppcCount2 = 0;
		for (sint32 i = 0; i < imlSegment->imlListCount; i++)
		{
			if (imlSegment->imlList[i].associatedPPCAddress == 0)
				continue;
			if (imlSegment->imlList[i].associatedPPCAddress == lastPPCInstAddr)
				continue;
			lastPPCInstAddr = imlSegment->imlList[i].associatedPPCAddress;
			ppcCount2++;
		}
		//uint32 ppcCount = imlSegment->ppcAddrMax-imlSegment->ppcAddrMin+4; -> No longer works with inlined functions
		uint32 cycleCount = ppcCount2;// ppcCount / 4;
		if( cycleCount > 0 )
		{
			PPCRecompiler_pushBackIMLInstructions(imlSegment, 0, 1);
			imlSegment->imlList[0].type = PPCREC_IML_TYPE_MACRO;
			imlSegment->imlList[0].crRegister = PPC_REC_INVALID_REGISTER;
			imlSegment->imlList[0].operation = PPCREC_IML_MACRO_COUNT_CYCLES;
			imlSegment->imlList[0].op_macro.param = cycleCount;
		}
	}

	// find segments that have a (conditional) jump instruction that points in reverse direction of code flow
	// for these segments there is a risk that the recompiler could get trapped in an infinite busy loop. 
	// todo: We should do a loop-detection prepass where we flag segments that are actually in a loop. We can then use this information below to avoid generating the scheduler-exit code for segments that aren't actually in a loop despite them referencing an earlier segment (which could be an exit segment for example)	
	uint32 currentLoopEscapeJumpMarker = 0xFF000000; // start in an area where no valid code can be located
	for(sint32 s=0; s<ppcImlGenContext.segmentListCount; s++)
	{
		// todo: This currently uses segment->ppcAddrMin which isn't really reliable. (We already had a problem where function inlining would generate falsified segment ranges by omitting the branch instruction). Find a better solution (use jumpmark/enterable offsets?)
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext.segmentList[s];
		if( imlSegment->imlListCount == 0 )
			continue;
		if (imlSegment->imlList[imlSegment->imlListCount - 1].type != PPCREC_IML_TYPE_CJUMP || imlSegment->imlList[imlSegment->imlListCount - 1].op_conditionalJump.jumpmarkAddress > imlSegment->ppcAddrMin)
			continue;
		if (imlSegment->imlList[imlSegment->imlListCount - 1].type != PPCREC_IML_TYPE_CJUMP || imlSegment->imlList[imlSegment->imlListCount - 1].op_conditionalJump.jumpAccordingToSegment)
			continue;
		// exclude non-infinite tight loops
		if (PPCRecompilerImlAnalyzer_isTightFiniteLoop(imlSegment))
			continue;
		// potential loop segment found, split this segment into four:
		// P0: This segment checks if the remaining cycles counter is still above zero. If yes, it jumps to segment P2 (it's also the jump destination for other segments)
		// P1: This segment consists only of a single ppc_leave instruction and is usually skipped. Register unload instructions are later inserted here.
		// P2: This segment contains the iml instructions of the original segment
		// PEntry: This segment is used to enter the function, it jumps to P0
		// All segments are considered to be part of the same PPC instruction range
		// The first segment also retains the jump destination and enterable properties from the original segment.
		//debug_printf("--- Insert cycle counter check ---\n");
		//PPCRecompiler_dumpIML(ppcRecFunc, &ppcImlGenContext);

		PPCRecompilerIml_insertSegments(&ppcImlGenContext, s, 2);
		imlSegment = NULL;
		PPCRecImlSegment_t* imlSegmentP0 = ppcImlGenContext.segmentList[s+0];
		PPCRecImlSegment_t* imlSegmentP1 = ppcImlGenContext.segmentList[s+1];
		PPCRecImlSegment_t* imlSegmentP2 = ppcImlGenContext.segmentList[s+2];
		// create entry point segment
		PPCRecompilerIml_insertSegments(&ppcImlGenContext, ppcImlGenContext.segmentListCount, 1);
		PPCRecImlSegment_t* imlSegmentPEntry = ppcImlGenContext.segmentList[ppcImlGenContext.segmentListCount-1];
		// relink segments	
		PPCRecompilerIML_relinkInputSegment(imlSegmentP2, imlSegmentP0);
		PPCRecompilerIml_setLinkBranchNotTaken(imlSegmentP0, imlSegmentP1);
		PPCRecompilerIml_setLinkBranchTaken(imlSegmentP0, imlSegmentP2);
		PPCRecompilerIml_setLinkBranchTaken(imlSegmentPEntry, imlSegmentP0);
		// update segments
		uint32 enterPPCAddress = imlSegmentP2->ppcAddrMin;
		if (imlSegmentP2->isEnterable)
			enterPPCAddress = imlSegmentP2->enterPPCAddress;
		imlSegmentP0->ppcAddress = 0xFFFFFFFF;
		imlSegmentP1->ppcAddress = 0xFFFFFFFF;
		imlSegmentP2->ppcAddress = 0xFFFFFFFF;
		cemu_assert_debug(imlSegmentP2->ppcAddrMin != 0);
		// move segment properties from segment P2 to segment P0
		imlSegmentP0->isJumpDestination = imlSegmentP2->isJumpDestination;
		imlSegmentP0->jumpDestinationPPCAddress = imlSegmentP2->jumpDestinationPPCAddress;
		imlSegmentP0->isEnterable = false;
		//imlSegmentP0->enterPPCAddress = imlSegmentP2->enterPPCAddress;
		imlSegmentP0->ppcAddrMin = imlSegmentP2->ppcAddrMin;
		imlSegmentP0->ppcAddrMax = imlSegmentP2->ppcAddrMax;
		imlSegmentP2->isJumpDestination = false;
		imlSegmentP2->jumpDestinationPPCAddress = 0;
		imlSegmentP2->isEnterable = false;
		imlSegmentP2->enterPPCAddress = 0;
		imlSegmentP2->ppcAddrMin = 0;
		imlSegmentP2->ppcAddrMax = 0;
		// setup enterable segment
		if( enterPPCAddress != 0 && enterPPCAddress != 0xFFFFFFFF )
		{
			imlSegmentPEntry->isEnterable = true;
			imlSegmentPEntry->ppcAddress = enterPPCAddress;
			imlSegmentPEntry->enterPPCAddress = enterPPCAddress;
		}
		// assign new jumpmark to segment P2
		imlSegmentP2->isJumpDestination = true;
		imlSegmentP2->jumpDestinationPPCAddress = currentLoopEscapeJumpMarker;
		currentLoopEscapeJumpMarker++;
		// create ppc_leave instruction in segment P1
		PPCRecompiler_pushBackIMLInstructions(imlSegmentP1, 0, 1);
		imlSegmentP1->imlList[0].type = PPCREC_IML_TYPE_MACRO;
		imlSegmentP1->imlList[0].operation = PPCREC_IML_MACRO_LEAVE;
		imlSegmentP1->imlList[0].crRegister = PPC_REC_INVALID_REGISTER;
		imlSegmentP1->imlList[0].op_macro.param = imlSegmentP0->ppcAddrMin;
		imlSegmentP1->imlList[0].associatedPPCAddress = imlSegmentP0->ppcAddrMin;
		// create cycle-based conditional instruction in segment P0
		PPCRecompiler_pushBackIMLInstructions(imlSegmentP0, 0, 1);
		imlSegmentP0->imlList[0].type = PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK;
		imlSegmentP0->imlList[0].operation = 0;
		imlSegmentP0->imlList[0].crRegister = PPC_REC_INVALID_REGISTER;
		imlSegmentP0->imlList[0].op_conditionalJump.jumpmarkAddress = imlSegmentP2->jumpDestinationPPCAddress;
		imlSegmentP0->imlList[0].associatedPPCAddress = imlSegmentP0->ppcAddrMin;
		// jump instruction for PEntry
		PPCRecompiler_pushBackIMLInstructions(imlSegmentPEntry, 0, 1);
		PPCRecompilerImlGen_generateNewInstruction_jumpSegment(&ppcImlGenContext, imlSegmentPEntry->imlList + 0);

		// skip the newly created segments
		s += 2;
	}

	// isolate entry points from function flow (enterable segments must not be the target of any other segment)
	// this simplifies logic during register allocation
	PPCRecompilerIML_isolateEnterableSegments(&ppcImlGenContext);

	// if GQRs can be predicted, optimize PSQ load/stores
	PPCRecompiler_optimizePSQLoadAndStore(&ppcImlGenContext);

	// count number of used registers
	uint32 numLoadedFPRRegisters = 0;
	for(uint32 i=0; i<255; i++)
	{
		if( ppcImlGenContext.mappedFPRRegister[i] )
			numLoadedFPRRegisters++;
	}

	// insert name store instructions at the end of each segment but before branch instructions
	for(sint32 s=0; s<ppcImlGenContext.segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext.segmentList[s];
		if( ppcImlGenContext.segmentList[s]->imlListCount == 0 )
			continue; // ignore empty segments
		// analyze segment for register usage
		PPCImlOptimizerUsedRegisters_t registersUsed;
		for(sint32 i=0; i<imlSegment->imlListCount; i++)
		{
			PPCRecompiler_checkRegisterUsage(&ppcImlGenContext, imlSegment->imlList+i, &registersUsed);
			//PPCRecompilerImlGen_findRegisterByMappedName(ppcImlGenContext, registersUsed.readGPR1);
			sint32 accessedTempReg[5];
			// intermediate FPRs
			accessedTempReg[0] = registersUsed.readFPR1;
			accessedTempReg[1] = registersUsed.readFPR2;
			accessedTempReg[2] = registersUsed.readFPR3;
			accessedTempReg[3] = registersUsed.readFPR4;
			accessedTempReg[4] = registersUsed.writtenFPR1;
			for(sint32 f=0; f<5; f++)
			{
				if( accessedTempReg[f] == -1 )
					continue;
				uint32 regName = ppcImlGenContext.mappedFPRRegister[accessedTempReg[f]];
				if( regName >= PPCREC_NAME_FPR0 && regName < PPCREC_NAME_FPR0+32 )
				{
					imlSegment->ppcFPRUsed[regName - PPCREC_NAME_FPR0] = true;
				}
			}
		}
	}

	// merge certain float load+store patterns (must happen before FPR register remapping)
	PPCRecompiler_optimizeDirectFloatCopies(&ppcImlGenContext);
	// delay byte swapping for certain load+store patterns
	PPCRecompiler_optimizeDirectIntegerCopies(&ppcImlGenContext);

	if (numLoadedFPRRegisters > 0)
	{
		if (PPCRecompiler_manageFPRRegisters(&ppcImlGenContext) == false)
		{
			PPCRecompiler_freeContext(&ppcImlGenContext);
			return false;
		}
	}

	PPCRecompilerImm_allocateRegisters(&ppcImlGenContext);

	// remove redundant name load and store instructions
	PPCRecompiler_reorderConditionModifyInstructions(&ppcImlGenContext);
	PPCRecompiler_removeRedundantCRUpdates(&ppcImlGenContext);
	return true;
}
