#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterHelper.h"
#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "PPCRecompilerX64.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "util/MemMapper/MemMapper.h"
#include "Common/cpu_features.h"

sint32 x64Gen_registerMap[12] = // virtual GPR to x64 register mapping
{
	REG_RAX, REG_RDX, REG_RBX, REG_RBP, REG_RSI, REG_RDI, REG_R8, REG_R9, REG_R10, REG_R11, REG_R12, REG_RCX
};

/*
* Remember current instruction output offset for reloc
* The instruction generated after this method has been called will be adjusted
*/
void PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext_t* x64GenContext, uint8 type, void* extraInfo = nullptr)
{
	if( x64GenContext->relocateOffsetTableCount >= x64GenContext->relocateOffsetTableSize )
	{
		x64GenContext->relocateOffsetTableSize = std::max(4, x64GenContext->relocateOffsetTableSize*2);
		x64GenContext->relocateOffsetTable = (x64RelocEntry_t*)realloc(x64GenContext->relocateOffsetTable, sizeof(x64RelocEntry_t)*x64GenContext->relocateOffsetTableSize);
	}
	x64GenContext->relocateOffsetTable[x64GenContext->relocateOffsetTableCount].offset = x64GenContext->codeBufferIndex;
	x64GenContext->relocateOffsetTable[x64GenContext->relocateOffsetTableCount].type = type;
	x64GenContext->relocateOffsetTable[x64GenContext->relocateOffsetTableCount].extraInfo = extraInfo;
	x64GenContext->relocateOffsetTableCount++;
}

/*
* Overwrites the currently cached (in x64 cf) cr* register
* Should be called before each x64 instruction which overwrites the current status flags (with mappedCRRegister set to PPCREC_CR_TEMPORARY unless explicitly set by PPC instruction)
*/
void PPCRecompilerX64Gen_crConditionFlags_set(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, sint32 mappedCRRegister, sint32 crState)
{
	x64GenContext->activeCRRegister = mappedCRRegister;
	x64GenContext->activeCRState = crState;
}

/*
* Reset cached cr* register without storing it first
*/
void PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext)
{
	x64GenContext->activeCRRegister = PPC_REC_INVALID_REGISTER;
}

void PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext_t* x64GenContext, sint32 jumpInstructionOffset, sint32 destinationOffset)
{
	uint8* instructionData = x64GenContext->codeBuffer + jumpInstructionOffset;
	if (instructionData[0] == 0x0F && (instructionData[1] >= 0x80 && instructionData[1] <= 0x8F))
	{
		// far conditional jump
		*(uint32*)(instructionData + 2) = (destinationOffset - (jumpInstructionOffset + 6));
	}
	else if (instructionData[0] >= 0x70 && instructionData[0] <= 0x7F)
	{
		// short conditional jump
		sint32 distance = (sint32)((destinationOffset - (jumpInstructionOffset + 2)));
		cemu_assert_debug(distance >= -128 && distance <= 127);
		*(uint8*)(instructionData + 1) = (uint8)distance;
	}
	else if (instructionData[0] == 0xE9)
	{
		*(uint32*)(instructionData + 1) = (destinationOffset - (jumpInstructionOffset + 5));
	}
	else if (instructionData[0] == 0xEB)
	{
		sint32 distance = (sint32)((destinationOffset - (jumpInstructionOffset + 2)));
		cemu_assert_debug(distance >= -128 && distance <= 127);
		*(uint8*)(instructionData + 1) = (uint8)distance;
	}
	else
	{
		assert_dbg();
	}
}

void PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	sint32 crRegister = imlInstruction->crRegister;
	if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_LT))) == 0 )
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_SIGN, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_LT)); // check for sign instead of _BELOW (CF) which is not set by TEST
	if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_GT))) == 0 )
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_SIGNED_GREATER, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_GT));
	if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_EQ))) == 0 )
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_EQUAL, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_EQ));
	// todo: Set CR SO if XER SO bit is set
	PPCRecompilerX64Gen_crConditionFlags_set(PPCRecFunction, ppcImlGenContext, x64GenContext, crRegister, PPCREC_CR_STATE_TYPE_LOGICAL);
}

void* ATTR_MS_ABI PPCRecompiler_virtualHLE(PPCInterpreter_t* hCPU, uint32 hleFuncId)
{
	void* prevRSPTemp = hCPU->rspTemp;
	if( hleFuncId == 0xFFD0 )
	{
		hCPU->remainingCycles -= 500; // let subtract about 500 cycles for each HLE call
		hCPU->gpr[3] = 0;
		PPCInterpreter_nextInstruction(hCPU);
		return hCPU;
	}
	else
	{
		auto hleCall = PPCInterpreter_getHLECall(hleFuncId);
		cemu_assert(hleCall != nullptr);
		hleCall(hCPU);
	}
	hCPU->rspTemp = prevRSPTemp;
	return PPCInterpreter_getCurrentInstance();
}

void ATTR_MS_ABI PPCRecompiler_getTBL(PPCInterpreter_t* hCPU, uint32 gprIndex)
{
	uint64 coreTime = coreinit::coreinit_getTimerTick();
	hCPU->gpr[gprIndex] = (uint32)(coreTime&0xFFFFFFFF);
}

void ATTR_MS_ABI PPCRecompiler_getTBU(PPCInterpreter_t* hCPU, uint32 gprIndex)
{
	uint64 coreTime = coreinit::coreinit_getTimerTick();
	hCPU->gpr[gprIndex] = (uint32)((coreTime>>32)&0xFFFFFFFF);
}

bool PPCRecompilerX64Gen_imlInstruction_macro(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
	if( imlInstruction->operation == PPCREC_IML_MACRO_BLR || imlInstruction->operation == PPCREC_IML_MACRO_BLRL )
	{
		uint32 currentInstructionAddress = imlInstruction->op_macro.param;
		// MOV EDX, [SPR_LR]
		x64Emit_mov_reg64_mem32(x64GenContext, REG_RDX, REG_RSP, offsetof(PPCInterpreter_t, spr.LR));
		// if BLRL, then update SPR LR
		if (imlInstruction->operation == PPCREC_IML_MACRO_BLRL)
			x64Gen_mov_mem32Reg64_imm32(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, spr.LR), currentInstructionAddress + 4);
		// JMP [offset+RDX*(8/4)+R15]
		x64Gen_writeU8(x64GenContext, 0x41);
		x64Gen_writeU8(x64GenContext, 0xFF);
		x64Gen_writeU8(x64GenContext, 0xA4);
		x64Gen_writeU8(x64GenContext, 0x57);
		x64Gen_writeU32(x64GenContext, (uint32)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		return true;
	}
	else if( imlInstruction->operation == PPCREC_IML_MACRO_BCTR || imlInstruction->operation == PPCREC_IML_MACRO_BCTRL )
	{
		uint32 currentInstructionAddress = imlInstruction->op_macro.param;
		// MOV EDX, [SPR_CTR]
		x64Emit_mov_reg64_mem32(x64GenContext, REG_RDX, REG_RSP, offsetof(PPCInterpreter_t, spr.CTR));
		// if BCTRL, then update SPR LR
		if (imlInstruction->operation == PPCREC_IML_MACRO_BCTRL)
			x64Gen_mov_mem32Reg64_imm32(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, spr.LR), currentInstructionAddress + 4);
		// JMP [offset+RDX*(8/4)+R15]
		x64Gen_writeU8(x64GenContext, 0x41);
		x64Gen_writeU8(x64GenContext, 0xFF);
		x64Gen_writeU8(x64GenContext, 0xA4);
		x64Gen_writeU8(x64GenContext, 0x57);
		x64Gen_writeU32(x64GenContext, (uint32)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		return true;
	}
	else if( imlInstruction->operation == PPCREC_IML_MACRO_BL )
	{
		// MOV DWORD [SPR_LinkRegister], newLR
		uint32 newLR = imlInstruction->op_macro.param + 4;
		x64Gen_mov_mem32Reg64_imm32(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, spr.LR), newLR);
		// remember new instruction pointer in RDX
		uint32 newIP = imlInstruction->op_macro.param2;
		x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RDX, newIP);
		// since RDX is constant we can use JMP [R15+const_offset] if jumpTableOffset+RDX*2 does not exceed the 2GB boundary
		uint64 lookupOffset = (uint64)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable) + (uint64)newIP * 2ULL;
		if (lookupOffset >= 0x80000000ULL)
		{
			// JMP [offset+RDX*(8/4)+R15]
			x64Gen_writeU8(x64GenContext, 0x41);
			x64Gen_writeU8(x64GenContext, 0xFF);
			x64Gen_writeU8(x64GenContext, 0xA4);
			x64Gen_writeU8(x64GenContext, 0x57);
			x64Gen_writeU32(x64GenContext, (uint32)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		}
		else
		{
			x64Gen_writeU8(x64GenContext, 0x41);
			x64Gen_writeU8(x64GenContext, 0xFF);
			x64Gen_writeU8(x64GenContext, 0xA7);
			x64Gen_writeU32(x64GenContext, (uint32)lookupOffset);
		}
		return true;
	}
	else if( imlInstruction->operation == PPCREC_IML_MACRO_B_FAR )
	{
		// remember new instruction pointer in RDX
		uint32 newIP = imlInstruction->op_macro.param2;
		x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RDX, newIP);
		// Since RDX is constant we can use JMP [R15+const_offset] if jumpTableOffset+RDX*2 does not exceed the 2GB boundary
		uint64 lookupOffset = (uint64)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable) + (uint64)newIP * 2ULL;
		if (lookupOffset >= 0x80000000ULL)
		{
			// JMP [offset+RDX*(8/4)+R15]
			x64Gen_writeU8(x64GenContext, 0x41);
			x64Gen_writeU8(x64GenContext, 0xFF);
			x64Gen_writeU8(x64GenContext, 0xA4);
			x64Gen_writeU8(x64GenContext, 0x57);
			x64Gen_writeU32(x64GenContext, (uint32)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		}
		else
		{
			x64Gen_writeU8(x64GenContext, 0x41);
			x64Gen_writeU8(x64GenContext, 0xFF);
			x64Gen_writeU8(x64GenContext, 0xA7);
			x64Gen_writeU32(x64GenContext, (uint32)lookupOffset);
		}
		return true;
	}
	else if( imlInstruction->operation == PPCREC_IML_MACRO_LEAVE )
	{
		uint32 currentInstructionAddress = imlInstruction->op_macro.param;
		// remember PC value in REG_EDX
		x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RDX, currentInstructionAddress);

		uint32 newIP = 0; // special value for recompiler exit
		uint64 lookupOffset = (uint64)&(((PPCRecompilerInstanceData_t*)NULL)->ppcRecompilerDirectJumpTable) + (uint64)newIP * 2ULL;
		// JMP [R15+offset]
		x64Gen_writeU8(x64GenContext, 0x41);
		x64Gen_writeU8(x64GenContext, 0xFF);
		x64Gen_writeU8(x64GenContext, 0xA7);
		x64Gen_writeU32(x64GenContext, (uint32)lookupOffset);
		return true;
	}
	else if( imlInstruction->operation == PPCREC_IML_MACRO_DEBUGBREAK )
	{
		x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, imlInstruction->op_macro.param2);
		x64Gen_int3(x64GenContext);
		return true;
	}
	else if( imlInstruction->operation == PPCREC_IML_MACRO_COUNT_CYCLES )
	{
		uint32 cycleCount = imlInstruction->op_macro.param;
		x64Gen_sub_mem32reg64_imm32(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, remainingCycles), cycleCount);
		return true;
	}
	else if( imlInstruction->operation == PPCREC_IML_MACRO_HLE )
	{
		uint32 ppcAddress = imlInstruction->op_macro.param;
		uint32 funcId = imlInstruction->op_macro.param2;
		//x64Gen_int3(x64GenContext);
		// update instruction pointer
		x64Gen_mov_mem32Reg64_imm32(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, instructionPointer), ppcAddress);
		//// save hCPU (RSP)
		//x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)&ppcRecompilerX64_hCPUTemp);
		//x64Emit_mov_mem64_reg64(x64GenContext, REG_RESV_TEMP, 0, REG_RSP);
		// set parameters
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RCX, REG_RSP);
		x64Gen_mov_reg64_imm64(x64GenContext, REG_RDX, funcId);
		// restore stackpointer from executionContext/hCPU->rspTemp
		x64Emit_mov_reg64_mem64(x64GenContext, REG_RSP, REG_RESV_HCPU, offsetof(PPCInterpreter_t, rspTemp));
		//x64Emit_mov_reg64_mem64(x64GenContext, REG_RSP, REG_R14, 0);
		//x64Gen_int3(x64GenContext);
		// reserve space on stack for call parameters
		x64Gen_sub_reg64_imm32(x64GenContext, REG_RSP, 8*11); // must be uneven number in order to retain stack 0x10 alignment
		x64Gen_mov_reg64_imm64(x64GenContext, REG_RBP, 0);
		// call HLE function
		x64Gen_mov_reg64_imm64(x64GenContext, REG_RAX, (uint64)PPCRecompiler_virtualHLE);
		x64Gen_call_reg64(x64GenContext, REG_RAX);
		// restore RSP to hCPU (from RAX, result of PPCRecompiler_virtualHLE)
		//x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)&ppcRecompilerX64_hCPUTemp);
		//x64Emit_mov_reg64_mem64Reg64(x64GenContext, REG_RSP, REG_RESV_TEMP, 0);
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RSP, REG_RAX);
		// MOV R15, ppcRecompilerInstanceData
		x64Gen_mov_reg64_imm64(x64GenContext, REG_R15, (uint64)ppcRecompilerInstanceData);
		// MOV R13, memory_base
		x64Gen_mov_reg64_imm64(x64GenContext, REG_R13, (uint64)memory_base);
		// check if cycles where decreased beyond zero, if yes -> leave recompiler
		x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, remainingCycles), 31); // check if negative
		sint32 jumpInstructionOffset1 = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NOT_CARRY, 0);
		//x64Gen_int3(x64GenContext);
		//x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RDX, ppcAddress);

		x64Emit_mov_reg64_mem32(x64GenContext, REG_RDX, REG_RSP, offsetof(PPCInterpreter_t, instructionPointer));
		// set EAX to 0 (we assume that ppcRecompilerDirectJumpTable[0] will be a recompiler escape function)
		x64Gen_xor_reg32_reg32(x64GenContext, REG_RAX, REG_RAX);
		// ADD RAX, R15 (R15 -> Pointer to ppcRecompilerInstanceData
		x64Gen_add_reg64_reg64(x64GenContext, REG_RAX, REG_R15);
		//// JMP [recompilerCallTable+EAX/4*8]
		//x64Gen_int3(x64GenContext);
		x64Gen_jmp_memReg64(x64GenContext, REG_RAX, (uint32)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1, x64GenContext->codeBufferIndex);
		// check if instruction pointer was changed
		// assign new instruction pointer to EAX
		x64Emit_mov_reg64_mem32(x64GenContext, REG_RAX, REG_RSP, offsetof(PPCInterpreter_t, instructionPointer));
		// remember instruction pointer in REG_EDX
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RDX, REG_RAX);
		// EAX *= 2
		x64Gen_add_reg64_reg64(x64GenContext, REG_RAX, REG_RAX);
		// ADD RAX, R15 (R15 -> Pointer to ppcRecompilerInstanceData
		x64Gen_add_reg64_reg64(x64GenContext, REG_RAX, REG_R15);
		// JMP [ppcRecompilerDirectJumpTable+RAX/4*8]
		x64Gen_jmp_memReg64(x64GenContext, REG_RAX, (uint32)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		return true;
	}
	else if( imlInstruction->operation == PPCREC_IML_MACRO_MFTB )
	{
		uint32 ppcAddress = imlInstruction->op_macro.param;
		uint32 sprId = imlInstruction->op_macro.param2&0xFFFF;
		uint32 gprIndex = (imlInstruction->op_macro.param2>>16)&0x1F;
		// update instruction pointer
		x64Gen_mov_mem32Reg64_imm32(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, instructionPointer), ppcAddress);
		// set parameters
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RCX, REG_RSP);
		x64Gen_mov_reg64_imm64(x64GenContext, REG_RDX, gprIndex);
		// restore stackpointer to original RSP
		x64Emit_mov_reg64_mem64(x64GenContext, REG_RSP, REG_RESV_HCPU, offsetof(PPCInterpreter_t, rspTemp));
		// push hCPU on stack
		x64Gen_push_reg64(x64GenContext, REG_RCX);
		// reserve space on stack for call parameters
		x64Gen_sub_reg64_imm32(x64GenContext, REG_RSP, 8*11 + 8);
		x64Gen_mov_reg64_imm64(x64GenContext, REG_RBP, 0);
		// call HLE function
		if( sprId == SPR_TBL )
			x64Gen_mov_reg64_imm64(x64GenContext, REG_RAX, (uint64)PPCRecompiler_getTBL);
		else if( sprId == SPR_TBU )
			x64Gen_mov_reg64_imm64(x64GenContext, REG_RAX, (uint64)PPCRecompiler_getTBU);
		else
			assert_dbg();
		x64Gen_call_reg64(x64GenContext, REG_RAX);
		// restore hCPU from stack
		x64Gen_add_reg64_imm32(x64GenContext, REG_RSP, 8 * 11 + 8);
		x64Gen_pop_reg64(x64GenContext, REG_RSP);
		// MOV R15, ppcRecompilerInstanceData
		x64Gen_mov_reg64_imm64(x64GenContext, REG_R15, (uint64)ppcRecompilerInstanceData);
		// MOV R13, memory_base
		x64Gen_mov_reg64_imm64(x64GenContext, REG_R13, (uint64)memory_base);
		return true;
	}
	else
	{
		debug_printf("Unknown recompiler macro operation %d\n", imlInstruction->operation);
		assert_dbg();
	}
	return false;
}

/*
* Load from memory
*/
bool PPCRecompilerX64Gen_imlInstruction_load(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction, bool indexed)
{
	sint32 realRegisterData = tempToRealRegister(imlInstruction->op_storeLoad.registerData);
	sint32 realRegisterMem = tempToRealRegister(imlInstruction->op_storeLoad.registerMem);
	sint32 realRegisterMem2 = PPC_REC_INVALID_REGISTER;
	if( indexed )
		realRegisterMem2 = tempToRealRegister(imlInstruction->op_storeLoad.registerMem2);
	if( false )//imlInstruction->op_storeLoad.flags & PPCREC_IML_OP_FLAG_FASTMEMACCESS )
	{
		// load u8/u16/u32 via direct memory access + optional sign extend
		assert_dbg(); // todo
	}
	else
	{
		if( indexed && realRegisterMem == realRegisterMem2 )
		{
			return false;
		}
		if( indexed && realRegisterData == realRegisterMem2 )
		{
			// for indexed memory access realRegisterData must not be the same register as the second memory register,
			// this can easily be fixed by swapping the logic of realRegisterMem and realRegisterMem2
			sint32 temp = realRegisterMem;
			realRegisterMem = realRegisterMem2;
			realRegisterMem2 = temp;
		}

		bool signExtend = imlInstruction->op_storeLoad.flags2.signExtend;
		bool switchEndian = imlInstruction->op_storeLoad.flags2.swapEndian;
		if( imlInstruction->op_storeLoad.copyWidth == 32 )
		{
			//if( indexed )
			//	PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
			if (indexed)
			{
				x64Gen_lea_reg64Low32_reg64Low32PlusReg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem, realRegisterMem2);
			}
			if( g_CPUFeatures.x86.movbe && switchEndian )
			{
				if (indexed)
				{
					x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, realRegisterData, REG_R13, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32);
					//if (indexed && realRegisterMem != realRegisterData)
					//	x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
				}
				else
				{
					x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, realRegisterData, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
				}
			}
			else
			{
				if (indexed)
				{
					x64Emit_mov_reg32_mem32(x64GenContext, realRegisterData, REG_R13, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32);
					//if (realRegisterMem != realRegisterData)
					//	x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
					if (switchEndian)
						x64Gen_bswap_reg64Lower32bit(x64GenContext, realRegisterData);
				}
				else
				{
					x64Emit_mov_reg32_mem32(x64GenContext, realRegisterData, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
					if (switchEndian)
						x64Gen_bswap_reg64Lower32bit(x64GenContext, realRegisterData);
				}
			}
		}
		else if( imlInstruction->op_storeLoad.copyWidth == 16 )
		{
			PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext); // todo: We can avoid this if MOVBE is available
			if (indexed)
			{
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			}			
			if( g_CPUFeatures.x86.movbe && switchEndian )
			{
				x64Gen_movBEZeroExtend_reg64Low16_mem16Reg64PlusReg64(x64GenContext, realRegisterData, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
				if( indexed && realRegisterMem != realRegisterData )
					x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			}
			else
			{
				x64Gen_movZeroExtend_reg64Low16_mem16Reg64PlusReg64(x64GenContext, realRegisterData, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
				if( indexed && realRegisterMem != realRegisterData )
					x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
				if( switchEndian )
					x64Gen_rol_reg64Low16_imm8(x64GenContext, realRegisterData, 8);
			}
			if( signExtend )
				x64Gen_movSignExtend_reg64Low32_reg64Low16(x64GenContext, realRegisterData, realRegisterData);
			else
				x64Gen_movZeroExtend_reg64Low32_reg64Low16(x64GenContext, realRegisterData, realRegisterData);
		}
		else if( imlInstruction->op_storeLoad.copyWidth == 8 )
		{
			if( indexed )
				PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
			// todo: Optimize by using only MOVZX/MOVSX
			if( indexed )
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			// todo: Use sign extend move from memory instead of separate sign-extend?
			if( signExtend )
				x64Gen_movSignExtend_reg64Low32_mem8Reg64PlusReg64(x64GenContext, realRegisterData, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
			else
				x64Emit_movZX_reg32_mem8(x64GenContext, realRegisterData, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
			if( indexed && realRegisterMem != realRegisterData )
				x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		else if( imlInstruction->op_storeLoad.copyWidth == PPC_REC_LOAD_LWARX_MARKER )
		{
			PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
			if( imlInstruction->op_storeLoad.immS32 != 0 )
				assert_dbg(); // not supported
			if( indexed )
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			x64Emit_mov_mem32_reg32(x64GenContext, REG_RSP, (uint32)offsetof(PPCInterpreter_t, reservedMemAddr), realRegisterMem); // remember EA for reservation
			x64Emit_mov_reg32_mem32(x64GenContext, realRegisterData, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
			if( indexed && realRegisterMem != realRegisterData )
				x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			if( switchEndian )
				x64Gen_bswap_reg64Lower32bit(x64GenContext, realRegisterData);
			x64Emit_mov_mem32_reg32(x64GenContext, REG_RSP, (uint32)offsetof(PPCInterpreter_t, reservedMemValue), realRegisterData); // remember value for reservation
			// LWARX instruction costs extra cycles (this speeds up busy loops)
			x64Gen_sub_mem32reg64_imm32(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, remainingCycles), 20);
		}
		else if( imlInstruction->op_storeLoad.copyWidth == PPC_REC_STORE_LSWI_3 )
		{
			PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
			if( switchEndian == false )
				assert_dbg();
			if( indexed )
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2); // can be replaced with LEA temp, [memReg1+memReg2] (this way we can avoid the SUB instruction after the move)
			if( g_CPUFeatures.x86.movbe )
			{
				x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, realRegisterData, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
				if( indexed && realRegisterMem != realRegisterData )
					x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			}
			else
			{
				x64Emit_mov_reg32_mem32(x64GenContext, realRegisterData, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
				if( indexed && realRegisterMem != realRegisterData )
					x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, realRegisterData);
			}
			x64Gen_and_reg64Low32_imm32(x64GenContext, realRegisterData, 0xFFFFFF00);
		}
		else
			return false;
		return true;
	}
	return false;
}

/*
* Write to memory
*/
bool PPCRecompilerX64Gen_imlInstruction_store(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction, bool indexed)
{
	sint32 realRegisterData = tempToRealRegister(imlInstruction->op_storeLoad.registerData);
	sint32 realRegisterMem = tempToRealRegister(imlInstruction->op_storeLoad.registerMem);
	sint32 realRegisterMem2 = PPC_REC_INVALID_REGISTER;
	if (indexed)
		realRegisterMem2 = tempToRealRegister(imlInstruction->op_storeLoad.registerMem2);

	if (false)//imlInstruction->op_storeLoad.flags & PPCREC_IML_OP_FLAG_FASTMEMACCESS )
	{
		// load u8/u16/u32 via direct memory access + optional sign extend
		assert_dbg(); // todo
	}
	else
	{
		if (indexed && realRegisterMem == realRegisterMem2)
		{
			return false;
		}
		if (indexed && realRegisterData == realRegisterMem2)
		{
			// for indexed memory access realRegisterData must not be the same register as the second memory register,
			// this can easily be fixed by swapping the logic of realRegisterMem and realRegisterMem2
			sint32 temp = realRegisterMem;
			realRegisterMem = realRegisterMem2;
			realRegisterMem2 = temp;
		}

		bool signExtend = imlInstruction->op_storeLoad.flags2.signExtend;
		bool swapEndian = imlInstruction->op_storeLoad.flags2.swapEndian;
		if (imlInstruction->op_storeLoad.copyWidth == 32)
		{
			if (indexed)
				PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
			uint32 valueRegister;
			if ((swapEndian == false || g_CPUFeatures.x86.movbe) && realRegisterMem != realRegisterData)
			{
				valueRegister = realRegisterData;
			}
			else
			{
				x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, realRegisterData);
				valueRegister = REG_RESV_TEMP;
			}
			if (g_CPUFeatures.x86.movbe == false && swapEndian)
				x64Gen_bswap_reg64Lower32bit(x64GenContext, valueRegister);
			if (indexed)
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			if (g_CPUFeatures.x86.movbe && swapEndian)
				x64Gen_movBETruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, valueRegister);
			else
				x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, valueRegister);
			if (indexed)
				x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		else if (imlInstruction->op_storeLoad.copyWidth == 16)
		{
			if (indexed || swapEndian)
				PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
			x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, realRegisterData);
			if (swapEndian)
				x64Gen_rol_reg64Low16_imm8(x64GenContext, REG_RESV_TEMP, 8);
			if (indexed)
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			x64Gen_movTruncate_mem16Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
			if (indexed)
				x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			// todo: Optimize this, e.g. by using MOVBE
		}
		else if (imlInstruction->op_storeLoad.copyWidth == 8)
		{
			if (indexed)
				PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
			if (indexed && realRegisterMem == realRegisterData)
			{
				x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, realRegisterData);
				realRegisterData = REG_RESV_TEMP;
			}
			if (indexed)
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			x64Gen_movTruncate_mem8Reg64PlusReg64_reg64(x64GenContext, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32, realRegisterData);
			if (indexed)
				x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		else if (imlInstruction->op_storeLoad.copyWidth == PPC_REC_STORE_STWCX_MARKER)
		{
			PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
			if (imlInstruction->op_storeLoad.immS32 != 0)
				assert_dbg(); // todo
								// reset cr0 LT, GT and EQ
			sint32 crRegister = 0;
			x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*(crRegister * 4 + PPCREC_CR_BIT_LT), 0);
			x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*(crRegister * 4 + PPCREC_CR_BIT_GT), 0);
			x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*(crRegister * 4 + PPCREC_CR_BIT_EQ), 0);
			// calculate effective address
			x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, realRegisterData);
			if (swapEndian)
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
			if (indexed)
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			// realRegisterMem now holds EA
			x64Gen_cmp_reg64Low32_mem32reg64(x64GenContext, realRegisterMem, REG_RESV_HCPU, offsetof(PPCInterpreter_t, reservedMemAddr));
			sint32 jumpInstructionOffsetJumpToEnd = x64GenContext->codeBufferIndex;
			x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NOT_EQUAL, 0);
			// EA matches reservation
			// backup EAX (since it's an explicit operand of CMPXCHG and will be overwritten)
			x64Emit_mov_mem32_reg32(x64GenContext, REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[0]), REG_EAX);
			// backup REG_RESV_MEMBASE
			x64Emit_mov_mem64_reg64(x64GenContext, REG_RESV_HCPU, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[2]), REG_RESV_MEMBASE);
			// add mem register to REG_RESV_MEMBASE
			x64Gen_add_reg64_reg64(x64GenContext, REG_RESV_MEMBASE, realRegisterMem);
			// load reserved value in EAX
			x64Emit_mov_reg64_mem32(x64GenContext, REG_EAX, REG_RESV_HCPU, offsetof(PPCInterpreter_t, reservedMemValue));
			// bswap EAX
			x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_EAX);

			//x64Gen_lock_cmpxchg_mem32Reg64PlusReg64_reg64(x64GenContext, REG_RESV_MEMBASE, realRegisterMem, 0, REG_RESV_TEMP);
			x64Gen_lock_cmpxchg_mem32Reg64_reg64(x64GenContext, REG_RESV_MEMBASE, 0, REG_RESV_TEMP);

			x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_EQUAL, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*(crRegister * 4 + PPCREC_CR_BIT_EQ));

			// reset reservation
			x64Gen_mov_mem32Reg64_imm32(x64GenContext, REG_RESV_HCPU, (uint32)offsetof(PPCInterpreter_t, reservedMemAddr), 0);
			x64Gen_mov_mem32Reg64_imm32(x64GenContext, REG_RESV_HCPU, (uint32)offsetof(PPCInterpreter_t, reservedMemValue), 0);

			// restore EAX
			x64Emit_mov_reg64_mem32(x64GenContext, REG_EAX, REG_RESV_HCPU, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[0]));
			// restore REG_RESV_MEMBASE
			x64Emit_mov_reg64_mem64(x64GenContext, REG_RESV_MEMBASE, REG_RESV_HCPU, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[2]));

			// copy XER SO to CR0 SO
			x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, spr.XER), 31);
			x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_CARRY, REG_RESV_HCPU, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*(crRegister * 4 + PPCREC_CR_BIT_SO));
			// end
			PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffsetJumpToEnd, x64GenContext->codeBufferIndex);
		}
		else if (imlInstruction->op_storeLoad.copyWidth == PPC_REC_STORE_STSWI_2)
		{
			PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
			x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, realRegisterData);
			x64Gen_shr_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, 16); // store upper 2 bytes ..
			x64Gen_rol_reg64Low16_imm8(x64GenContext, REG_RESV_TEMP, 8); // .. as big-endian
			if (indexed)
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);

			x64Gen_movTruncate_mem16Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
			if (indexed)
				x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		else if (imlInstruction->op_storeLoad.copyWidth == PPC_REC_STORE_STSWI_3)
		{
			PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
			x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, realRegisterData);
			if (indexed)
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);

			x64Gen_shr_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, 8);
			x64Gen_movTruncate_mem8Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32 + 2, REG_RESV_TEMP);
			x64Gen_shr_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, 8);
			x64Gen_movTruncate_mem8Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32 + 1, REG_RESV_TEMP);
			x64Gen_shr_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, 8);
			x64Gen_movTruncate_mem8Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32 + 0, REG_RESV_TEMP);

			if (indexed)
				x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		else
			return false;
		return true;
	}
	return false;
}

/*
 * Copy byte/word/dword from memory to memory
 */
void PPCRecompilerX64Gen_imlInstruction_mem2mem(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	sint32 realSrcMemReg = tempToRealRegister(imlInstruction->op_mem2mem.src.registerMem);
	sint32 realSrcMemImm = imlInstruction->op_mem2mem.src.immS32;
	sint32 realDstMemReg = tempToRealRegister(imlInstruction->op_mem2mem.dst.registerMem);
	sint32 realDstMemImm = imlInstruction->op_mem2mem.dst.immS32;
	// PPCRecompilerX64Gen_crConditionFlags_forget() is not needed here, since MOVs don't affect eflags
	if (imlInstruction->op_mem2mem.copyWidth == 32)
	{
		x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_R13, realSrcMemReg, realSrcMemImm);
		x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_R13, realDstMemReg, realDstMemImm, REG_RESV_TEMP);
	}
	else
	{
		assert_dbg();
	}
}

bool PPCRecompilerX64Gen_imlInstruction_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	{
		// registerResult = registerA
		if (imlInstruction->crRegister != PPC_REC_INVALID_REGISTER)
		{
			x64Gen_mov_reg64_reg64(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
			if (imlInstruction->crMode == PPCREC_CR_MODE_LOGICAL)
			{
				// since MOV doesn't set eflags we need another test instruction
				x64Gen_test_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerResult));
				// set cr bits
				PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
			}
			else
			{
				assert_dbg();
			}
		}
		else
		{
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
		}
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ENDIAN_SWAP)
	{
		// registerResult = endianSwap32(registerA)
		if (imlInstruction->op_r_r.registerA != imlInstruction->op_r_r.registerResult)
			assert_dbg();
		x64Gen_bswap_reg64Lower32bit(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_ADD )
	{
		// registerResult += registerA
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		x64Gen_add_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_ASSIGN_S8_TO_S32 )
	{
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		x64Gen_movSignExtend_reg64Low32_reg64Low8(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode == PPCREC_CR_MODE_ARITHMETIC )
			{
				x64Gen_test_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerResult));
				// set cr bits
				PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
			}
			else
			{
				debug_printf("PPCRecompilerX64Gen_imlInstruction_r_r(): Unsupported operation\n");
				assert_dbg();
			}
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_OR || imlInstruction->operation == PPCREC_IML_OP_AND || imlInstruction->operation == PPCREC_IML_OP_XOR )
	{
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		if( imlInstruction->operation == PPCREC_IML_OP_OR )
		{
			// registerResult |= registerA
			x64Gen_or_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
		}
		else if( imlInstruction->operation == PPCREC_IML_OP_AND )
		{
			// registerResult &= registerA
			x64Gen_and_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
		}
		else
		{
			// registerResult ^= registerA
			x64Gen_xor_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
		}
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			// set cr bits
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_NOT )
	{
		// copy register content if different registers
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		if( imlInstruction->op_r_r.registerResult != imlInstruction->op_r_r.registerA )
		{
			x64Gen_mov_reg64_reg64(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
		}
		// NOT destination register
		x64Gen_not_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult));
		// update cr bits
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			// NOT instruction does not update flags, so we have to generate an additional TEST instruction
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerResult));
			// set cr bits
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_CNTLZW )
	{
		// count leading zeros
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		if( g_CPUFeatures.x86.lzcnt )
		{
			x64Gen_lzcnt_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
		}
		else
		{
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerA), tempToRealRegister(imlInstruction->op_r_r.registerA));
			sint32 jumpInstructionOffset1 = x64GenContext->codeBufferIndex;
			x64Gen_jmpc_near(x64GenContext, X86_CONDITION_EQUAL, 0);
			x64Gen_bsr_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
			x64Gen_neg_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult));
			x64Gen_add_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), 32-1);
			sint32 jumpInstructionOffset2 = x64GenContext->codeBufferIndex;
			x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NONE, 0);
			PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1, x64GenContext->codeBufferIndex);
			x64Gen_mov_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), 32);
			PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2, x64GenContext->codeBufferIndex);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_COMPARE_SIGNED || imlInstruction->operation == PPCREC_IML_OP_COMPARE_UNSIGNED )
	{
		// registerA CMP registerB (arithmetic compare)
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		if( imlInstruction->crRegister == PPC_REC_INVALID_REGISTER )
		{
			return false; // a NO-OP instruction
		}
		if( imlInstruction->crRegister >= 8 )
		{
			return false;
		}
		// update state of cr register
		if( imlInstruction->operation == PPCREC_IML_OP_COMPARE_SIGNED )
			PPCRecompilerX64Gen_crConditionFlags_set(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction->crRegister, PPCREC_CR_STATE_TYPE_SIGNED_ARITHMETIC);
		else
			PPCRecompilerX64Gen_crConditionFlags_set(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction->crRegister, PPCREC_CR_STATE_TYPE_UNSIGNED_ARITHMETIC);
		// create compare instruction
		x64Gen_cmp_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
		// set cr bits
		sint32 crRegister = imlInstruction->crRegister;
		if( imlInstruction->operation == PPCREC_IML_OP_COMPARE_SIGNED )
		{
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_LT))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_SIGNED_LESS, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_LT));
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_GT))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_SIGNED_GREATER, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_GT));
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_EQ))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_EQUAL, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_EQ));
			// todo: Also set summary overflow if xer bit is set
		}
		else if( imlInstruction->operation == PPCREC_IML_OP_COMPARE_UNSIGNED )
		{
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_LT))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_UNSIGNED_BELOW, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_LT));
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_GT))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_UNSIGNED_ABOVE, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_GT));
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_EQ))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_EQUAL, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_EQ));
			// todo: Also set summary overflow if xer bit is set
		}
		else
			assert_dbg();
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_NEG )
	{
		// copy register content if different registers
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		if( imlInstruction->op_r_r.registerResult != imlInstruction->op_r_r.registerA )
		{
			x64Gen_mov_reg64_reg64(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
		}
		// NEG destination register
		x64Gen_neg_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult));
		// update cr bits
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			// set cr bits
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_ADD_CARRY )
	{
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		// copy operand to result if different registers
		if( imlInstruction->op_r_r.registerResult != imlInstruction->op_r_r.registerA )
		{
			x64Gen_mov_reg64_reg64(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
		}
		// copy xer_ca to eflags carry
		x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
		// add carry bit
		x64Gen_adc_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), 0);
		// update xer carry
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_CARRY, REG_RSP, offsetof(PPCInterpreter_t, xer_ca));
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			// set cr bits
			sint32 crRegister = imlInstruction->crRegister;
			x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_SIGN, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_LT)); // check for sign instead of _BELOW (CF) which is not set by AND/OR
			x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_UNSIGNED_ABOVE, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_GT));
			x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_EQUAL, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_EQ));
			// todo: Use different version of PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction)
			// todo: Also set summary overflow if xer bit is set
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_ADD_CARRY_ME )
	{
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		// copy operand to result if different registers
		if( imlInstruction->op_r_r.registerResult != imlInstruction->op_r_r.registerA )
		{
			x64Gen_mov_reg64_reg64(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerA));
		}
		// copy xer_ca to eflags carry
		x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
		// add carry bit
		x64Gen_adc_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), (uint32)-1);
		// update xer carry
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_CARRY, REG_RSP, offsetof(PPCInterpreter_t, xer_ca));
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			// set cr bits
			sint32 crRegister = imlInstruction->crRegister;
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerResult));
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_SUB_CARRY_UPDATE_CARRY )
	{
		// registerResult = ~registerOperand1 + carry
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r.registerResult);
		sint32 rRegOperand1 = tempToRealRegister(imlInstruction->op_r_r.registerA);
		// copy operand to result register
		x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand1);		
		// execute NOT on result
		x64Gen_not_reg64Low32(x64GenContext, rRegResult);
		// copy xer_ca to eflags carry
		x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
		// add carry
		x64Gen_adc_reg64Low32_imm32(x64GenContext, rRegResult, 0);
		// update carry
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_CARRY, REG_RSP, offsetof(PPCInterpreter_t, xer_ca));
		// update cr if requested
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode == PPCREC_CR_MODE_LOGICAL )
			{
				x64Gen_test_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegResult);
				// set cr bits
				PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
			}
			else
			{
				assert_dbg();
			}
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_ASSIGN_S16_TO_S32 )
	{
		// registerResult = (uint32)(sint32)(sint16)registerA
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		x64Gen_movSignExtend_reg64Low32_reg64Low16(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), reg32ToReg16(tempToRealRegister(imlInstruction->op_r_r.registerA)));
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode == PPCREC_CR_MODE_ARITHMETIC )
			{
				x64Gen_test_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r.registerResult), tempToRealRegister(imlInstruction->op_r_r.registerResult));
				// set cr bits
				PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
			}
			else
			{
				debug_printf("PPCRecompilerX64Gen_imlInstruction_r_r(): Unsupported operation\n");
				assert_dbg();
			}
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_DCBZ )
	{
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		if( imlInstruction->op_r_r.registerResult != imlInstruction->op_r_r.registerA )
		{
			x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, tempToRealRegister(imlInstruction->op_r_r.registerA));
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, tempToRealRegister(imlInstruction->op_r_r.registerResult));
			x64Gen_and_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, ~0x1F);
			x64Gen_add_reg64_reg64(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE);
			for(sint32 f=0; f<0x20; f+=8)
				x64Gen_mov_mem64Reg64_imm32(x64GenContext, REG_RESV_TEMP, f, 0);
		}
		else
		{
			// calculate effective address
			x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, tempToRealRegister(imlInstruction->op_r_r.registerA));
			x64Gen_and_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, ~0x1F);
			x64Gen_add_reg64_reg64(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE);
			for(sint32 f=0; f<0x20; f+=8)
				x64Gen_mov_mem64Reg64_imm32(x64GenContext, REG_RESV_TEMP, f, 0);
		}
	}
	else
	{
		debug_printf("PPCRecompilerX64Gen_imlInstruction_r_r(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_r_s32(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	if( imlInstruction->operation == PPCREC_IML_OP_ASSIGN )
	{
		// registerResult = immS32
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		x64Gen_mov_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_immS32.registerIndex), (uint32)imlInstruction->op_r_immS32.immS32);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_ADD )
	{
		// registerResult += immS32
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		x64Gen_add_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_immS32.registerIndex), (uint32)imlInstruction->op_r_immS32.immS32);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_SUB )
	{
		// registerResult -= immS32
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		if (imlInstruction->crRegister == PPCREC_CR_REG_TEMP)
		{
			// do nothing -> SUB is for BDNZ instruction
		}
		else if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			// update cr register
			assert_dbg();
		}
		x64Gen_sub_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_immS32.registerIndex), (uint32)imlInstruction->op_r_immS32.immS32);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_AND )
	{
		// registerResult &= immS32
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		x64Gen_and_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_immS32.registerIndex), (uint32)imlInstruction->op_r_immS32.immS32);
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode != PPCREC_CR_MODE_LOGICAL )
			{
				assert_dbg();
			}
			// set cr bits
			sint32 crRegister = imlInstruction->crRegister;
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
			// todo: Set CR SO if XER SO bit is set
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_OR )
	{
		// registerResult |= immS32
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		x64Gen_or_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_immS32.registerIndex), (uint32)imlInstruction->op_r_immS32.immS32);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_XOR )
	{
		// registerResult ^= immS32
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		x64Gen_xor_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_immS32.registerIndex), (uint32)imlInstruction->op_r_immS32.immS32);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_LEFT_ROTATE )
	{
		// registerResult <<<= immS32
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		if( (imlInstruction->op_r_immS32.immS32&0x80) )
			assert_dbg(); // should not happen
		x64Gen_rol_reg64Low32_imm8(x64GenContext, tempToRealRegister(imlInstruction->op_r_immS32.registerIndex), (uint8)imlInstruction->op_r_immS32.immS32);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_COMPARE_SIGNED || imlInstruction->operation == PPCREC_IML_OP_COMPARE_UNSIGNED )
	{
		// registerResult CMP immS32 (arithmetic compare)
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		if( imlInstruction->crRegister == PPC_REC_INVALID_REGISTER )
		{
			debug_printf("PPCRecompilerX64Gen_imlInstruction_r_s32(): No-Op CMP found\n");
			return true; // a NO-OP instruction
		}
		if( imlInstruction->crRegister >= 8 )
		{
			debug_printf("PPCRecompilerX64Gen_imlInstruction_r_s32(): Unsupported CMP with crRegister = 8\n");
			return false;
		}
		// update state of cr register
		if( imlInstruction->operation == PPCREC_IML_OP_COMPARE_SIGNED )
			PPCRecompilerX64Gen_crConditionFlags_set(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction->crRegister, PPCREC_CR_STATE_TYPE_SIGNED_ARITHMETIC);
		else
			PPCRecompilerX64Gen_crConditionFlags_set(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction->crRegister, PPCREC_CR_STATE_TYPE_UNSIGNED_ARITHMETIC);
		// create compare instruction
		x64Gen_cmp_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_immS32.registerIndex), imlInstruction->op_r_immS32.immS32);
		// set cr bits
		uint32 crRegister = imlInstruction->crRegister;
		if( imlInstruction->operation == PPCREC_IML_OP_COMPARE_SIGNED )
		{
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_LT))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_SIGNED_LESS, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_LT));
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_GT))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_SIGNED_GREATER, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_GT));
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_EQ))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_EQUAL, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_EQ));
		}
		else if( imlInstruction->operation == PPCREC_IML_OP_COMPARE_UNSIGNED )
		{
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_LT))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_UNSIGNED_BELOW, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_LT));
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_GT))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_UNSIGNED_ABOVE, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_GT));
			if( (imlInstruction->crIgnoreMask&(1<<(crRegister*4+PPCREC_CR_BIT_EQ))) == 0 )
				x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_EQUAL, REG_ESP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_EQ));
		}
		else
			assert_dbg();
		// todo: Also set summary overflow if xer bit is set?
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_MFCR )
	{
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		uint32 destRegister = tempToRealRegister(imlInstruction->op_r_immS32.registerIndex);
		x64Gen_xor_reg64Low32_reg64Low32(x64GenContext, destRegister, destRegister);
		for(sint32 f=0; f<32; f++)
		{
			x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, cr)+f, 0);
			x64Gen_adc_reg64Low32_reg64Low32(x64GenContext, destRegister, destRegister);
		}
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_MTCRF)
	{
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		uint32 srcRegister = tempToRealRegister(imlInstruction->op_r_immS32.registerIndex);
		uint32 crBitMask = ppc_MTCRFMaskToCRBitMask((uint32)imlInstruction->op_r_immS32.immS32);
		for (sint32 f = 0; f < 32; f++)
		{
			if(((crBitMask >> f) & 1) == 0)
				continue;
			x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_ESP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8) * (f), 0);
			x64Gen_test_reg64Low32_imm32(x64GenContext, srcRegister, 0x80000000>>f);
			x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_NOT_EQUAL, REG_ESP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8) * (f));
		}
	}
	else
	{
		debug_printf("PPCRecompilerX64Gen_imlInstruction_r_s32(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_conditional_r_s32(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	{
		// registerResult = immS32 (conditional)
		if (imlInstruction->crRegister != PPC_REC_INVALID_REGISTER)
		{
			assert_dbg();
		}

		x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, (uint32)imlInstruction->op_conditional_r_s32.immS32);

		uint8 crBitIndex = imlInstruction->op_conditional_r_s32.crRegisterIndex * 4 + imlInstruction->op_conditional_r_s32.crBitIndex;
		if (imlInstruction->op_conditional_r_s32.crRegisterIndex == x64GenContext->activeCRRegister)
		{
			if (x64GenContext->activeCRState == PPCREC_CR_STATE_TYPE_UNSIGNED_ARITHMETIC)
			{
				if (imlInstruction->op_conditional_r_s32.crBitIndex == CR_BIT_LT)
				{
					x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, imlInstruction->op_conditional_r_s32.bitMustBeSet ? X86_CONDITION_CARRY : X86_CONDITION_NOT_CARRY, tempToRealRegister(imlInstruction->op_conditional_r_s32.registerIndex), REG_RESV_TEMP);
					return true;
				}
				else if (imlInstruction->op_conditional_r_s32.crBitIndex == CR_BIT_EQ)
				{
					x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, imlInstruction->op_conditional_r_s32.bitMustBeSet ? X86_CONDITION_EQUAL : X86_CONDITION_NOT_EQUAL, tempToRealRegister(imlInstruction->op_conditional_r_s32.registerIndex), REG_RESV_TEMP);
					return true;
				}
				else if (imlInstruction->op_conditional_r_s32.crBitIndex == CR_BIT_GT)
				{
					x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, imlInstruction->op_conditional_r_s32.bitMustBeSet ? X86_CONDITION_UNSIGNED_ABOVE : X86_CONDITION_UNSIGNED_BELOW_EQUAL, tempToRealRegister(imlInstruction->op_conditional_r_s32.registerIndex), REG_RESV_TEMP);
					return true;
				}
			}
			else if (x64GenContext->activeCRState == PPCREC_CR_STATE_TYPE_SIGNED_ARITHMETIC)
			{
				if (imlInstruction->op_conditional_r_s32.crBitIndex == CR_BIT_LT)
				{
					x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, imlInstruction->op_conditional_r_s32.bitMustBeSet ? X86_CONDITION_SIGNED_LESS : X86_CONDITION_SIGNED_GREATER_EQUAL, tempToRealRegister(imlInstruction->op_conditional_r_s32.registerIndex), REG_RESV_TEMP);
					return true;
				}
				else if (imlInstruction->op_conditional_r_s32.crBitIndex == CR_BIT_EQ)
				{
					x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, imlInstruction->op_conditional_r_s32.bitMustBeSet ? X86_CONDITION_EQUAL : X86_CONDITION_NOT_EQUAL, tempToRealRegister(imlInstruction->op_conditional_r_s32.registerIndex), REG_RESV_TEMP);
					return true;
				}
				else if (imlInstruction->op_conditional_r_s32.crBitIndex == CR_BIT_GT)
				{
					x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, imlInstruction->op_conditional_r_s32.bitMustBeSet ? X86_CONDITION_SIGNED_GREATER : X86_CONDITION_SIGNED_LESS_EQUAL, tempToRealRegister(imlInstruction->op_conditional_r_s32.registerIndex), REG_RESV_TEMP);
					return true;
				}
			}
			else if (x64GenContext->activeCRState == PPCREC_CR_STATE_TYPE_LOGICAL)
			{
				if (imlInstruction->op_conditional_r_s32.crBitIndex == CR_BIT_LT)
				{
					x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, imlInstruction->op_conditional_r_s32.bitMustBeSet ? X86_CONDITION_SIGN : X86_CONDITION_NOT_SIGN, tempToRealRegister(imlInstruction->op_conditional_r_s32.registerIndex), REG_RESV_TEMP);
					return true;
				}
				else if (imlInstruction->op_conditional_r_s32.crBitIndex == CR_BIT_EQ)
				{
					x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, imlInstruction->op_conditional_r_s32.bitMustBeSet ? X86_CONDITION_EQUAL : X86_CONDITION_NOT_EQUAL, tempToRealRegister(imlInstruction->op_conditional_r_s32.registerIndex), REG_RESV_TEMP);
					return true;
				}
				else if (imlInstruction->op_conditional_r_s32.crBitIndex == CR_BIT_GT)
				{
					x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, imlInstruction->op_conditional_r_s32.bitMustBeSet ? X86_CONDITION_SIGNED_GREATER : X86_CONDITION_SIGNED_LESS_EQUAL, tempToRealRegister(imlInstruction->op_conditional_r_s32.registerIndex), REG_RESV_TEMP);
					return true;
				}
			}
		}
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, cr) + crBitIndex * sizeof(uint8), 0);
		if (imlInstruction->op_conditional_r_s32.bitMustBeSet)
			x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, X86_CONDITION_CARRY, tempToRealRegister(imlInstruction->op_conditional_r_s32.registerIndex), REG_RESV_TEMP);
		else
			x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, X86_CONDITION_NOT_CARRY, tempToRealRegister(imlInstruction->op_conditional_r_s32.registerIndex), REG_RESV_TEMP);
		return true;
	}
	return false;
}

bool PPCRecompilerX64Gen_imlInstruction_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	if( imlInstruction->operation == PPCREC_IML_OP_ADD || imlInstruction->operation == PPCREC_IML_OP_ADD_UPDATE_CARRY || imlInstruction->operation == PPCREC_IML_OP_ADD_CARRY_UPDATE_CARRY )
	{
		// registerResult = registerOperand1 + registerOperand2
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_r.registerResult);
		sint32 rRegOperand1 = tempToRealRegister(imlInstruction->op_r_r_r.registerA);
		sint32 rRegOperand2 = tempToRealRegister(imlInstruction->op_r_r_r.registerB);

		bool addCarry = imlInstruction->operation == PPCREC_IML_OP_ADD_CARRY_UPDATE_CARRY;
		if( (rRegResult == rRegOperand1) || (rRegResult == rRegOperand2) )
		{
			// be careful not to overwrite the operand before we use it
			if( rRegResult == rRegOperand1 )
			{
				if( addCarry )
				{
					x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
					x64Gen_adc_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
				}
				else
					x64Gen_add_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
			}
			else
			{
				if( addCarry )
				{
					x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
					x64Gen_adc_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand1);
				}
				else
					x64Gen_add_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand1);
			}
		}
		else
		{
			// copy operand1 to destination register before doing addition
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand1);
			// add operand2
			if( addCarry )
			{
				x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
				x64Gen_adc_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
			}
			else
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
		}
		// update carry
		if( imlInstruction->operation == PPCREC_IML_OP_ADD_UPDATE_CARRY || imlInstruction->operation == PPCREC_IML_OP_ADD_CARRY_UPDATE_CARRY )
		{
			x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_CARRY, REG_RSP, offsetof(PPCInterpreter_t, xer_ca));
		}
		// set cr bits if enabled
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode != PPCREC_CR_MODE_LOGICAL )
			{
				assert_dbg();
			}
			sint32 crRegister = imlInstruction->crRegister;
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
			return true;
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_SUB )
	{
		// registerResult = registerOperand1 - registerOperand2
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_r.registerResult);
		sint32 rRegOperand1 = tempToRealRegister(imlInstruction->op_r_r_r.registerA);
		sint32 rRegOperand2 = tempToRealRegister(imlInstruction->op_r_r_r.registerB);
		if( rRegOperand1 == rRegOperand2 )
		{
			// result = operand1 - operand1 -> 0
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegResult);
		}
		else if( rRegResult == rRegOperand1 )
		{
			// result = result - operand2
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
		}
		else if ( rRegResult == rRegOperand2 )
		{
			// result = operand1 - result
			// NEG result
			x64Gen_neg_reg64Low32(x64GenContext, rRegResult);
			// ADD result, operand1
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand1);
		}
		else
		{
			// copy operand1 to destination register before doing addition
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand1);
			// sub operand2
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
		}
		// set cr bits if enabled
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode != PPCREC_CR_MODE_LOGICAL )
			{
				assert_dbg();
			}
			sint32 crRegister = imlInstruction->crRegister;
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
			return true;
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_SUB_CARRY_UPDATE_CARRY )
	{
		// registerResult = registerOperand1 - registerOperand2 + carry
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_r.registerResult);
		sint32 rRegOperand1 = tempToRealRegister(imlInstruction->op_r_r_r.registerA);
		sint32 rRegOperand2 = tempToRealRegister(imlInstruction->op_r_r_r.registerB);
		if( rRegOperand1 == rRegOperand2 )
		{
			// copy xer_ca to eflags carry
			x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
			x64Gen_cmc(x64GenContext);
			// result = operand1 - operand1 -> 0
			x64Gen_sbb_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegResult);
		}
		else if( rRegResult == rRegOperand1 )
		{
			// copy inverted xer_ca to eflags carry
			x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
			x64Gen_cmc(x64GenContext);
			// result = result - operand2
			x64Gen_sbb_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
		}
		else if ( rRegResult == rRegOperand2 )
		{
			// result = operand1 - result
			// NOT result
			x64Gen_not_reg64Low32(x64GenContext, rRegResult);
			// copy xer_ca to eflags carry
			x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
			// ADC result, operand1
			x64Gen_adc_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand1);
		}
		else
		{
			// copy operand1 to destination register before doing addition
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand1);
			// copy xer_ca to eflags carry
			x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
			x64Gen_cmc(x64GenContext);
			// sub operand2
			x64Gen_sbb_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
		}
		// update carry flag (todo: is this actually correct in all cases?)
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_CARRY, REG_RSP, offsetof(PPCInterpreter_t, xer_ca));
		// update cr0 if requested
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode != PPCREC_CR_MODE_LOGICAL )
				assert_dbg();
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegResult);
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_SIGNED )
	{
		// registerResult = registerOperand1 * registerOperand2
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_r.registerResult);
		sint32 rRegOperand1 = tempToRealRegister(imlInstruction->op_r_r_r.registerA);
		sint32 rRegOperand2 = tempToRealRegister(imlInstruction->op_r_r_r.registerB);
		if( (rRegResult == rRegOperand1) || (rRegResult == rRegOperand2) )
		{
			// be careful not to overwrite the operand before we use it
			if( rRegResult == rRegOperand1 )
				x64Gen_imul_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
			else
				x64Gen_imul_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand1);
		}
		else
		{
			// copy operand1 to destination register before doing multiplication
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand1);
			// add operand2
			x64Gen_imul_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
		}
		// set cr bits if enabled
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode != PPCREC_CR_MODE_LOGICAL )
			{
				assert_dbg();
			}
			// since IMUL instruction leaves relevant flags undefined, we have to use another TEST instruction to get the correct results
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegResult);
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_SUBFC )
	{
		// registerResult = registerOperand2(rB) - registerOperand1(rA)
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		// updates carry flag
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			return false;
		}
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_r.registerResult);
		sint32 rRegOperandA = tempToRealRegister(imlInstruction->op_r_r_r.registerA);
		sint32 rRegOperandB = tempToRealRegister(imlInstruction->op_r_r_r.registerB);
		// update carry flag
		// carry flag is detected this way:
		//if ((~a+b) < a) {
		//	return true;
		//}
		//if ((~a+b+1) < 1) {
		//	return true;
		//}
		// set carry to zero
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
		// ((~a+b)<~a) == true -> ca = 1
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperandA);
		x64Gen_not_reg64Low32(x64GenContext, REG_RESV_TEMP);
		x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, rRegOperandB);
		x64Gen_not_reg64Low32(x64GenContext, rRegOperandA);
		x64Gen_cmp_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, rRegOperandA);
		x64Gen_not_reg64Low32(x64GenContext, rRegOperandA);
		sint32 jumpInstructionOffset1 = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_UNSIGNED_ABOVE_EQUAL, 0);
		// reset carry flag + jump destination afterwards
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 1);
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1, x64GenContext->codeBufferIndex);
		// OR ((~a+b+1)<1) == true -> ca = 1
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperandA);
		// todo: Optimize by reusing result in REG_RESV_TEMP from above and only add 1
		x64Gen_not_reg64Low32(x64GenContext, REG_RESV_TEMP);
		x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, rRegOperandB);
		x64Gen_add_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 1);
		x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 1);
		sint32 jumpInstructionOffset2 = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_UNSIGNED_ABOVE_EQUAL, 0);
		// reset carry flag + jump destination afterwards
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 1);
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2, x64GenContext->codeBufferIndex);
		// do subtraction
		if( rRegOperandB == rRegOperandA )
		{
			// result = operandA - operandA -> 0
			x64Gen_xor_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegResult);
		}
		else if( rRegResult == rRegOperandB )
		{
			// result = result - operandA
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperandA);
		}
		else if ( rRegResult == rRegOperandA )
		{
			// result = operandB - result
			// NEG result
			x64Gen_neg_reg64Low32(x64GenContext, rRegResult);
			// ADD result, operandB
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperandB);
		}
		else
		{
			// copy operand1 to destination register before doing addition
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperandB);
			// sub operand2
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperandA);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_SLW || imlInstruction->operation == PPCREC_IML_OP_SRW )
	{
		// registerResult = registerOperand1(rA) >> registerOperand2(rB) (up to 63 bits)
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_r.registerResult);
		sint32 rRegOperand1 = tempToRealRegister(imlInstruction->op_r_r_r.registerA);
		sint32 rRegOperand2 = tempToRealRegister(imlInstruction->op_r_r_r.registerB);

		if (g_CPUFeatures.x86.bmi2 && imlInstruction->operation == PPCREC_IML_OP_SRW)
		{
			// use BMI2 SHRX if available
			x64Gen_shrx_reg64_reg64_reg64(x64GenContext, rRegResult, rRegOperand1, rRegOperand2);
		}
		else if (g_CPUFeatures.x86.bmi2 && imlInstruction->operation == PPCREC_IML_OP_SLW)
		{
			// use BMI2 SHLX if available
			x64Gen_shlx_reg64_reg64_reg64(x64GenContext, rRegResult, rRegOperand1, rRegOperand2);
			x64Gen_and_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegResult); // trim result to 32bit
		}
		else
		{
			// lazy and slow way to do shift by register without relying on ECX/CL or BMI2
			x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand1);
			for (sint32 b = 0; b < 6; b++)
			{
				x64Gen_test_reg64Low32_imm32(x64GenContext, rRegOperand2, (1 << b));
				sint32 jumpInstructionOffset = x64GenContext->codeBufferIndex;
				x64Gen_jmpc_near(x64GenContext, X86_CONDITION_EQUAL, 0); // jump if bit not set
				if (b == 5)
				{
					x64Gen_xor_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
				}
				else
				{
					if (imlInstruction->operation == PPCREC_IML_OP_SLW)
						x64Gen_shl_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (1 << b));
					else
						x64Gen_shr_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (1 << b));
				}
				PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset, x64GenContext->codeBufferIndex);
			}
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, REG_RESV_TEMP);
		}
		// set cr bits if enabled
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode != PPCREC_CR_MODE_LOGICAL )
			{
				assert_dbg();
			}
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegResult);
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_LEFT_ROTATE )
	{
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_r.registerResult);
		sint32 rRegOperand1 = tempToRealRegister(imlInstruction->op_r_r_r.registerA);
		sint32 rRegOperand2 = tempToRealRegister(imlInstruction->op_r_r_r.registerB);
		// todo: Use BMI2 rotate if available
		// check if CL/ECX/RCX is available
		if( rRegResult != REG_RCX && rRegOperand1 != REG_RCX && rRegOperand2 != REG_RCX )
		{
			// swap operand 2 with RCX
			x64Gen_xchg_reg64_reg64(x64GenContext, REG_RCX, rRegOperand2);
			// move operand 1 to temp register
			x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand1);
			// rotate
			x64Gen_rol_reg64Low32_cl(x64GenContext, REG_RESV_TEMP);
			// undo swap operand 2 with RCX
			x64Gen_xchg_reg64_reg64(x64GenContext, REG_RCX, rRegOperand2);
			// copy to result register
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, REG_RESV_TEMP);
		}
		else
		{
			x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand1);
			// lazy and slow way to do shift by register without relying on ECX/CL
			for(sint32 b=0; b<5; b++)
			{
				x64Gen_test_reg64Low32_imm32(x64GenContext, rRegOperand2, (1<<b));
				sint32 jumpInstructionOffset = x64GenContext->codeBufferIndex;
				x64Gen_jmpc_near(x64GenContext, X86_CONDITION_EQUAL, 0); // jump if bit not set
				x64Gen_rol_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (1<<b));
				PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset, x64GenContext->codeBufferIndex);
			}
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, REG_RESV_TEMP);
		}
		// set cr bits if enabled
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode != PPCREC_CR_MODE_LOGICAL )
			{
				assert_dbg();
			}
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegResult);
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_SRAW )
	{
		// registerResult = (sint32)registerOperand1(rA) >> (sint32)registerOperand2(rB) (up to 63 bits)
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_r.registerResult);
		sint32 rRegOperand1 = tempToRealRegister(imlInstruction->op_r_r_r.registerA);
		sint32 rRegOperand2 = tempToRealRegister(imlInstruction->op_r_r_r.registerB);
		// save cr
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			return false;
		}
		// todo: Use BMI instructions if available?
		// MOV registerResult, registerOperand (if different)
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand1);
		 // reset carry
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
		// we use the same shift by register approach as in SLW/SRW, but we have to differentiate by signed/unsigned shift since it influences how the carry flag is set
		x64Gen_test_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 0x80000000);
		sint32 jumpInstructionJumpToSignedShift = x64GenContext->codeBufferIndex;		
		x64Gen_jmpc_far(x64GenContext, X86_CONDITION_NOT_EQUAL, 0);
		//sint32 jumpInstructionJumpToEnd = x64GenContext->codeBufferIndex;
		//x64Gen_jmpc(x64GenContext, X86_CONDITION_EQUAL, 0);
		// unsigned shift (MSB of input register is not set)
		for(sint32 b=0; b<6; b++)
		{
			x64Gen_test_reg64Low32_imm32(x64GenContext, rRegOperand2, (1<<b));
			sint32 jumpInstructionOffset = x64GenContext->codeBufferIndex;
			x64Gen_jmpc_near(x64GenContext, X86_CONDITION_EQUAL, 0); // jump if bit not set
			if( b == 5 )
			{
				x64Gen_sar_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (1<<b)/2);
				x64Gen_sar_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (1<<b)/2);
			}
			else
			{
				x64Gen_sar_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (1<<b));
			}
			PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset, x64GenContext->codeBufferIndex);
		}
		sint32 jumpInstructionJumpToEnd = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_far(x64GenContext, X86_CONDITION_NONE, 0);
		// signed shift
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionJumpToSignedShift, x64GenContext->codeBufferIndex);
		for(sint32 b=0; b<6; b++)
		{
			// check if we need to shift by (1<<bit)
			x64Gen_test_reg64Low32_imm32(x64GenContext, rRegOperand2, (1<<b));
			sint32 jumpInstructionOffset = x64GenContext->codeBufferIndex;
			x64Gen_jmpc_near(x64GenContext, X86_CONDITION_EQUAL, 0); // jump if bit not set
			// set ca if any non-zero bit is shifted out
			x64Gen_test_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, (1<<(1<<b))-1);
			sint32 jumpInstructionJumpToAfterCa = x64GenContext->codeBufferIndex;
			x64Gen_jmpc_near(x64GenContext, X86_CONDITION_EQUAL, 0); // jump if no bit is set
			x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 1);
			PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionJumpToAfterCa, x64GenContext->codeBufferIndex);
			// arithmetic shift
			if( b == 5 )
			{
				// copy sign bit into all bits
				x64Gen_sar_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (1<<b)/2);
				x64Gen_sar_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (1<<b)/2);
			}
			else
			{
				x64Gen_sar_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (1<<b));
			}
			PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset, x64GenContext->codeBufferIndex);
		}
		// end
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionJumpToEnd, x64GenContext->codeBufferIndex);
		x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, REG_RESV_TEMP);
		// update CR if requested
		// todo
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_DIVIDE_SIGNED || imlInstruction->operation == PPCREC_IML_OP_DIVIDE_UNSIGNED )
	{
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_r.registerResult);
		sint32 rRegOperand1 = tempToRealRegister(imlInstruction->op_r_r_r.registerA);
		sint32 rRegOperand2 = tempToRealRegister(imlInstruction->op_r_r_r.registerB);

		x64Emit_mov_mem32_reg32(x64GenContext, REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[0]), REG_EAX);
		x64Emit_mov_mem32_reg32(x64GenContext, REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[1]), REG_EDX);
		// mov operand 2 to temp register
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand2);
		// mov operand1 to EAX
		x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_EAX, rRegOperand1);
		// sign or zero extend EAX to EDX:EAX based on division sign mode
		if( imlInstruction->operation == PPCREC_IML_OP_DIVIDE_SIGNED )
			x64Gen_cdq(x64GenContext);
		else
			x64Gen_xor_reg64Low32_reg64Low32(x64GenContext, REG_EDX, REG_EDX);
		// make sure we avoid division by zero
		x64Gen_test_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_EQUAL, 3);
		// divide
		if( imlInstruction->operation == PPCREC_IML_OP_DIVIDE_SIGNED )
			x64Gen_idiv_reg64Low32(x64GenContext, REG_RESV_TEMP);
		else
			x64Gen_div_reg64Low32(x64GenContext, REG_RESV_TEMP);
		// result of division is now stored in EAX, move it to result register
		if( rRegResult != REG_EAX )
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, REG_EAX);
		// restore EAX / EDX
		if( rRegResult != REG_RAX )
			x64Emit_mov_reg64_mem32(x64GenContext, REG_EAX, REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[0]));
		if( rRegResult != REG_RDX )
			x64Emit_mov_reg64_mem32(x64GenContext, REG_EDX, REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[1]));
		// set cr bits if requested
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode != PPCREC_CR_MODE_ARITHMETIC )
			{
				assert_dbg();
			}
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegResult);
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED || imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_HIGH_UNSIGNED )
	{
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_r.registerResult);
		sint32 rRegOperand1 = tempToRealRegister(imlInstruction->op_r_r_r.registerA);
		sint32 rRegOperand2 = tempToRealRegister(imlInstruction->op_r_r_r.registerB);

		x64Emit_mov_mem32_reg32(x64GenContext, REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[0]), REG_EAX);
		x64Emit_mov_mem32_reg32(x64GenContext, REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[1]), REG_EDX);
		// mov operand 2 to temp register
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand2);
		// mov operand1 to EAX
		x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_EAX, rRegOperand1);
		if( imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED )
		{
			// zero extend EAX to EDX:EAX
			x64Gen_xor_reg64Low32_reg64Low32(x64GenContext, REG_EDX, REG_EDX);
		}
		else
		{
			// sign extend EAX to EDX:EAX
			x64Gen_cdq(x64GenContext);
		}
		// multiply
		if( imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED )
			x64Gen_imul_reg64Low32(x64GenContext, REG_RESV_TEMP);
		else
			x64Gen_mul_reg64Low32(x64GenContext, REG_RESV_TEMP);
		// result of multiplication is now stored in EDX:EAX, move it to result register
		if( rRegResult != REG_EDX )
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, REG_EDX);
		// restore EAX / EDX
		if( rRegResult != REG_RAX )
			x64Emit_mov_reg64_mem32(x64GenContext, REG_EAX, REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[0]));
		if( rRegResult != REG_RDX )
			x64Emit_mov_reg64_mem32(x64GenContext, REG_EDX, REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[1]));
		// set cr bits if requested
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode != PPCREC_CR_MODE_LOGICAL )
			{
				assert_dbg();
			}
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegResult);
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_ORC )
	{
		// registerResult = registerOperand1 | ~registerOperand2
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_r.registerResult);
		sint32 rRegOperand1 = tempToRealRegister(imlInstruction->op_r_r_r.registerA);
		sint32 rRegOperand2 = tempToRealRegister(imlInstruction->op_r_r_r.registerB);

		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand2);
		x64Gen_not_reg64Low32(x64GenContext, REG_RESV_TEMP);
		if( rRegResult != rRegOperand1 )
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand1);
		x64Gen_or_reg64Low32_reg64Low32(x64GenContext, rRegResult, REG_RESV_TEMP);

		// set cr bits if enabled
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode != PPCREC_CR_MODE_LOGICAL )
			{
				assert_dbg();
			}
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
			return true;
		}
	}
	else
	{
		debug_printf("PPCRecompilerX64Gen_imlInstruction_r_r_r(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_r_r_s32(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	if( imlInstruction->operation == PPCREC_IML_OP_ADD )
	{
		// registerResult = registerOperand + immS32
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_s32.registerResult);
		sint32 rRegOperand = tempToRealRegister(imlInstruction->op_r_r_s32.registerA);
		uint32 immU32 = (uint32)imlInstruction->op_r_r_s32.immS32;
		if( rRegResult != rRegOperand )
		{
			// copy value to destination register before doing addition
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand);
		}
		x64Gen_add_reg64Low32_imm32(x64GenContext, rRegResult, (uint32)immU32);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_ADD_UPDATE_CARRY )
	{
		// registerResult = registerOperand + immS32
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_s32.registerResult);
		sint32 rRegOperand = tempToRealRegister(imlInstruction->op_r_r_s32.registerA);
		uint32 immU32 = (uint32)imlInstruction->op_r_r_s32.immS32;
		if( rRegResult != rRegOperand )
		{
			// copy value to destination register before doing addition
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand);
		}
		x64Gen_add_reg64Low32_imm32(x64GenContext, rRegResult, (uint32)immU32);
		// update carry flag
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_CARRY, REG_RSP, offsetof(PPCInterpreter_t, xer_ca));
		// set cr bits if enabled
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			if( imlInstruction->crMode != PPCREC_CR_MODE_LOGICAL )
			{
				assert_dbg();
			}
			sint32 crRegister = imlInstruction->crRegister;
			//x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_SIGN, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_LT));
			//x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_SIGNED_GREATER, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_GT));
			//x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_EQUAL, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_EQ));
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_SUBFC )
	{
		// registerResult = immS32 - registerOperand
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_s32.registerResult);
		sint32 rRegOperand = tempToRealRegister(imlInstruction->op_r_r_s32.registerA);
		sint32 immS32 = (sint32)imlInstruction->op_r_r_s32.immS32;
		if( rRegResult != rRegOperand )
		{
			// copy value to destination register before doing addition
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand);
		}
		// set carry to zero
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
		// ((~a+b)<~a) == true -> ca = 1
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand);
		x64Gen_not_reg64Low32(x64GenContext, REG_RESV_TEMP);
		x64Gen_add_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, (uint32)immS32);
		x64Gen_not_reg64Low32(x64GenContext, rRegOperand);
		x64Gen_cmp_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, rRegOperand);
		x64Gen_not_reg64Low32(x64GenContext, rRegOperand);
		sint32 jumpInstructionOffset1 = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_far(x64GenContext, X86_CONDITION_UNSIGNED_ABOVE_EQUAL, 0);
		// reset carry flag + jump destination afterwards
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 1);
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1, x64GenContext->codeBufferIndex);
		// OR ((~a+b+1)<1) == true -> ca = 1
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand);
		// todo: Optimize by reusing result in REG_RESV_TEMP from above and only add 1
		x64Gen_not_reg64Low32(x64GenContext, REG_RESV_TEMP);
		x64Gen_add_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, (uint32)immS32);
		x64Gen_add_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 1);
		x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 1);
		sint32 jumpInstructionOffset2 = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_far(x64GenContext, X86_CONDITION_UNSIGNED_ABOVE_EQUAL, 0);
		// reset carry flag + jump destination afterwards
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 1);
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2, x64GenContext->codeBufferIndex);
		// do actual computation of value, note: a - b is equivalent to a + ~b + 1
		x64Gen_not_reg64Low32(x64GenContext, rRegResult);
		x64Gen_add_reg64Low32_imm32(x64GenContext, rRegResult, (uint32)immS32 + 1);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_RLWIMI )
	{
		// registerResult = ((registerResult<<<SH)&mask) | (registerOperand&~mask)
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		uint32 vImm = (uint32)imlInstruction->op_r_r_s32.immS32;
		uint32 mb = (vImm>>0)&0xFF;
		uint32 me = (vImm>>8)&0xFF;
		uint32 sh = (vImm>>16)&0xFF;
		uint32 mask = ppc_mask(mb, me);
		// save cr
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// copy rS to temporary register
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, tempToRealRegister(imlInstruction->op_r_r_s32.registerA));
		// rotate destination register
		if( sh )
			x64Gen_rol_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (uint8)sh&0x1F);
		// AND destination register with inverted mask
		x64Gen_and_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r_s32.registerResult), ~mask);
		// AND temporary rS register with mask
		x64Gen_and_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, mask);
		// OR result with temporary
		x64Gen_or_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r_s32.registerResult), REG_RESV_TEMP);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_SIGNED )
	{
		// registerResult = registerOperand * immS32
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		sint32 rRegResult = tempToRealRegister(imlInstruction->op_r_r_s32.registerResult);
		sint32 rRegOperand = tempToRealRegister(imlInstruction->op_r_r_s32.registerA);
		sint32 immS32 = (uint32)imlInstruction->op_r_r_s32.immS32;
		x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (sint64)immS32); // todo: Optimize
		if( rRegResult != rRegOperand )
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand);
		x64Gen_imul_reg64Low32_reg64Low32(x64GenContext, rRegResult, REG_RESV_TEMP);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_SRAW )
	{
		// registerResult = registerOperand>>SH   and set xer ca flag
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		uint32 sh = (uint32)imlInstruction->op_r_r_s32.immS32;
		// MOV registerResult, registerOperand (if different)
		if( imlInstruction->op_r_r_s32.registerA != imlInstruction->op_r_r_s32.registerResult )
			x64Gen_mov_reg64_reg64(x64GenContext, tempToRealRegister(imlInstruction->op_r_r_s32.registerResult), tempToRealRegister(imlInstruction->op_r_r_s32.registerA));
		// todo: Detect if we don't need to update carry
		// generic case
		// TEST registerResult, (1<<(SH+1))-1
		uint32 caTestMask = 0;
		if (sh >= 31)
			caTestMask = 0x7FFFFFFF;
		else
			caTestMask = (1 << (sh)) - 1;
		x64Gen_test_reg64Low32_imm32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r_s32.registerResult), caTestMask);
		// SETNE/NZ [ESP+XER_CA]
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_NOT_EQUAL, REG_RSP, offsetof(PPCInterpreter_t, xer_ca));
		// SAR registerResult, SH
		x64Gen_sar_reg64Low32_imm8(x64GenContext, tempToRealRegister(imlInstruction->op_r_r_s32.registerResult), sh);
		// JNS <skipInstruction> (if sign not set)
		sint32 jumpInstructionOffset = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_SIGN, 0); // todo: Can use 2-byte form of jump instruction here
		// MOV BYTE [ESP+xer_ca], 0
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, xer_ca), 0);
		// jump destination
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset, x64GenContext->codeBufferIndex);
		// CR update
		if (imlInstruction->crRegister != PPC_REC_INVALID_REGISTER)
		{
			sint32 crRegister = imlInstruction->crRegister;
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r_s32.registerResult), tempToRealRegister(imlInstruction->op_r_r_s32.registerResult));
			x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_SIGN, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*(crRegister * 4 + PPCREC_CR_BIT_LT));
			x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_SIGNED_GREATER, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*(crRegister * 4 + PPCREC_CR_BIT_GT));
			x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_EQUAL, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*(crRegister * 4 + PPCREC_CR_BIT_EQ));
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_LEFT_SHIFT ||
		imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT )
	{
		PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
		// MOV registerResult, registerOperand (if different)
		if( imlInstruction->op_r_r_s32.registerA != imlInstruction->op_r_r_s32.registerResult )
			x64Gen_mov_reg64_reg64(x64GenContext, tempToRealRegister(imlInstruction->op_r_r_s32.registerResult), tempToRealRegister(imlInstruction->op_r_r_s32.registerA));
		// Shift
		if( imlInstruction->operation == PPCREC_IML_OP_LEFT_SHIFT )
			x64Gen_shl_reg64Low32_imm8(x64GenContext, tempToRealRegister(imlInstruction->op_r_r_s32.registerResult), imlInstruction->op_r_r_s32.immS32);
		else
			x64Gen_shr_reg64Low32_imm8(x64GenContext, tempToRealRegister(imlInstruction->op_r_r_s32.registerResult), imlInstruction->op_r_r_s32.immS32);
		// CR update
		if (imlInstruction->crRegister != PPC_REC_INVALID_REGISTER)
		{
			// since SHL/SHR only modifies the OF flag we need another TEST reg,reg here
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, tempToRealRegister(imlInstruction->op_r_r_s32.registerResult), tempToRealRegister(imlInstruction->op_r_r_s32.registerResult));
			PPCRecompilerX64Gen_updateCRLogical(PPCRecFunction, ppcImlGenContext, x64GenContext, imlInstruction);
		}
	}
	else
	{
		debug_printf("PPCRecompilerX64Gen_imlInstruction_r_r_s32(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_conditionalJump(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlSegment_t* imlSegment, PPCRecImlInstruction_t* imlInstruction)
{
	if( imlInstruction->op_conditionalJump.condition == PPCREC_JUMP_CONDITION_NONE )
	{
		// jump always
		if (imlInstruction->op_conditionalJump.jumpAccordingToSegment)
		{
			// jump to segment
			if (imlSegment->nextSegmentBranchTaken == nullptr)
				assert_dbg();
			PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_SEGMENT, imlSegment->nextSegmentBranchTaken);
			x64Gen_jmp_imm32(x64GenContext, 0);
		}
		else
		{
			// deprecated (jump to jumpmark)
			PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
			x64Gen_jmp_imm32(x64GenContext, 0);
		}
	}
	else
	{
		if (imlInstruction->op_conditionalJump.jumpAccordingToSegment)
			assert_dbg();
		// generate jump update marker
		if( imlInstruction->op_conditionalJump.crRegisterIndex == PPCREC_CR_TEMPORARY || imlInstruction->op_conditionalJump.crRegisterIndex >= 8 )
		{
			// temporary cr is used, which means we use the currently active eflags
			PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
			sint32 condition = imlInstruction->op_conditionalJump.condition;
			if( condition == PPCREC_JUMP_CONDITION_E )
				x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
			else if( condition == PPCREC_JUMP_CONDITION_NE )
				x64Gen_jmpc_far(x64GenContext, X86_CONDITION_NOT_EQUAL, 0);
			else
				assert_dbg();
		}
		else
		{
			uint8 crBitIndex = imlInstruction->op_conditionalJump.crRegisterIndex*4 + imlInstruction->op_conditionalJump.crBitIndex;
			if (imlInstruction->op_conditionalJump.crRegisterIndex == x64GenContext->activeCRRegister )
			{
				if (x64GenContext->activeCRState == PPCREC_CR_STATE_TYPE_UNSIGNED_ARITHMETIC)
				{
					if (imlInstruction->op_conditionalJump.crBitIndex == CR_BIT_LT)
					{
						PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
						x64Gen_jmpc_far(x64GenContext, imlInstruction->op_conditionalJump.bitMustBeSet ? X86_CONDITION_CARRY : X86_CONDITION_NOT_CARRY, 0);
						return true;
					}
					else if (imlInstruction->op_conditionalJump.crBitIndex == CR_BIT_EQ)
					{
						PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
						x64Gen_jmpc_far(x64GenContext, imlInstruction->op_conditionalJump.bitMustBeSet ? X86_CONDITION_EQUAL : X86_CONDITION_NOT_EQUAL, 0);
						return true;
					}
					else if (imlInstruction->op_conditionalJump.crBitIndex == CR_BIT_GT)
					{
						PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
						x64Gen_jmpc_far(x64GenContext, imlInstruction->op_conditionalJump.bitMustBeSet ? X86_CONDITION_UNSIGNED_ABOVE : X86_CONDITION_UNSIGNED_BELOW_EQUAL, 0);
						return true;
					}
				}
				else if (x64GenContext->activeCRState == PPCREC_CR_STATE_TYPE_SIGNED_ARITHMETIC)
				{
					if (imlInstruction->op_conditionalJump.crBitIndex == CR_BIT_LT)
					{
						PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
						x64Gen_jmpc_far(x64GenContext, imlInstruction->op_conditionalJump.bitMustBeSet ? X86_CONDITION_SIGNED_LESS : X86_CONDITION_SIGNED_GREATER_EQUAL, 0);
						return true;
					}
					else if (imlInstruction->op_conditionalJump.crBitIndex == CR_BIT_EQ)
					{
						PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
						x64Gen_jmpc_far(x64GenContext, imlInstruction->op_conditionalJump.bitMustBeSet ? X86_CONDITION_EQUAL : X86_CONDITION_NOT_EQUAL, 0);
						return true;
					}
					else if (imlInstruction->op_conditionalJump.crBitIndex == CR_BIT_GT)
					{
						PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
						x64Gen_jmpc_far(x64GenContext, imlInstruction->op_conditionalJump.bitMustBeSet ? X86_CONDITION_SIGNED_GREATER : X86_CONDITION_SIGNED_LESS_EQUAL, 0);
						return true;
					}
				}
				else if (x64GenContext->activeCRState == PPCREC_CR_STATE_TYPE_LOGICAL)
				{
					if (imlInstruction->op_conditionalJump.crBitIndex == CR_BIT_LT)
					{
						PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
						x64Gen_jmpc_far(x64GenContext, imlInstruction->op_conditionalJump.bitMustBeSet ? X86_CONDITION_SIGN : X86_CONDITION_NOT_SIGN, 0);
						return true;
					}
					else if (imlInstruction->op_conditionalJump.crBitIndex == CR_BIT_EQ)
					{
						PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
						x64Gen_jmpc_far(x64GenContext, imlInstruction->op_conditionalJump.bitMustBeSet ? X86_CONDITION_EQUAL : X86_CONDITION_NOT_EQUAL, 0);
						return true;
					}
					else if (imlInstruction->op_conditionalJump.crBitIndex == CR_BIT_GT)
					{
						PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
						x64Gen_jmpc_far(x64GenContext, imlInstruction->op_conditionalJump.bitMustBeSet ? X86_CONDITION_SIGNED_GREATER : X86_CONDITION_SIGNED_LESS_EQUAL, 0);
						return true;
					}
				}
			}
			x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, cr) + crBitIndex * sizeof(uint8), 0);
			PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
			if( imlInstruction->op_conditionalJump.bitMustBeSet )
			{
				x64Gen_jmpc_far(x64GenContext, X86_CONDITION_CARRY, 0);
			}
			else
			{
				x64Gen_jmpc_far(x64GenContext, X86_CONDITION_NOT_CARRY, 0);
			}
		}
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_conditionalJumpCycleCheck(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
	// some tests (all performed on a i7-4790K)
	// 1) DEC [mem] + JNS has significantly worse performance than BT + JNC (probably due to additional memory write)
	// 2) CMP [mem], 0 + JG has about equal (or slightly worse) performance than BT + JNC

	// BT
	x64Gen_bt_mem8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, remainingCycles), 31); // check if negative
	PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X64_RELOC_LINK_TO_PPC, (void*)(size_t)imlInstruction->op_conditionalJump.jumpmarkAddress);
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_NOT_CARRY, 0);
	return true;
}

/*
* PPC condition register operation
*/
bool PPCRecompilerX64Gen_imlInstruction_cr(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext); // while these instruction do not directly affect eflags, they change the CR bit
	if (imlInstruction->operation == PPCREC_IML_OP_CR_CLEAR)
	{
		// clear cr bit
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*imlInstruction->op_cr.crD, 0);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_CR_SET)
	{
		// set cr bit
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*imlInstruction->op_cr.crD, 1);
		return true;
	}
	else if(imlInstruction->operation == PPCREC_IML_OP_CR_OR || imlInstruction->operation == PPCREC_IML_OP_CR_ORC ||
		imlInstruction->operation == PPCREC_IML_OP_CR_AND || imlInstruction->operation == PPCREC_IML_OP_CR_ANDC )
	{
		x64Emit_movZX_reg64_mem8(x64GenContext, REG_RESV_TEMP, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*imlInstruction->op_cr.crB);
		if (imlInstruction->operation == PPCREC_IML_OP_CR_ORC || imlInstruction->operation == PPCREC_IML_OP_CR_ANDC)
		{
			return false; // untested
			x64Gen_int3(x64GenContext);
			x64Gen_xor_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 1); // complement
		}
		if(imlInstruction->operation == PPCREC_IML_OP_CR_OR || imlInstruction->operation == PPCREC_IML_OP_CR_ORC)
			x64Gen_or_reg64Low8_mem8Reg64(x64GenContext, REG_RESV_TEMP, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*imlInstruction->op_cr.crA);
		else
			x64Gen_and_reg64Low8_mem8Reg64(x64GenContext, REG_RESV_TEMP, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*imlInstruction->op_cr.crA);

		x64Gen_mov_mem8Reg64_reg64Low8(x64GenContext, REG_RESV_TEMP, REG_RSP, offsetof(PPCInterpreter_t, cr) + sizeof(uint8)*imlInstruction->op_cr.crD);

		return true;
	}
	else
	{
		assert_dbg();
	}
	return false;
}


void PPCRecompilerX64Gen_imlInstruction_ppcEnter(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	imlInstruction->op_ppcEnter.x64Offset = x64GenContext->codeBufferIndex;
	// generate code
	if( ppcImlGenContext->hasFPUInstruction )
	{
		// old FPU unavailable code
		//PPCRecompilerX86_crConditionFlags_saveBeforeOverwrite(PPCRecFunction, ppcImlGenContext, x64GenContext);
		//// skip if FP bit in MSR is set
		//// #define MSR_FP		(1<<13)
		//x64Gen_bt_mem8(x64GenContext, REG_ESP, offsetof(PPCInterpreter_t, msr), 13);
		//uint32 jmpCodeOffset = x64GenContext->codeBufferIndex;
		//x64Gen_jmpc(x64GenContext, X86_CONDITION_CARRY, 0);
		//x64Gen_mov_reg32_imm32(x64GenContext, REG_EAX, imlInstruction->op_ppcEnter.ppcAddress&0x7FFFFFFF);
		//PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, X86_RELOC_MAKE_RELATIVE);
		//x64Gen_jmp_imm32(x64GenContext, (uint32)PPCRecompiler_recompilerCallEscapeAndCallFPUUnavailable);
		//// patch jump
		//*(uint32*)(x64GenContext->codeBuffer+jmpCodeOffset+2) = x64GenContext->codeBufferIndex-jmpCodeOffset-6;
	}
}

void PPCRecompilerX64Gen_imlInstruction_r_name(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	uint32 name = imlInstruction->op_r_name.name;
	if( name >= PPCREC_NAME_R0 && name < PPCREC_NAME_R0+32 )
	{
		x64Emit_mov_reg64_mem32(x64GenContext, tempToRealRegister(imlInstruction->op_r_name.registerIndex), REG_RSP, offsetof(PPCInterpreter_t, gpr)+sizeof(uint32)*(name-PPCREC_NAME_R0));
	}
	else if( name >= PPCREC_NAME_SPR0 && name < PPCREC_NAME_SPR0+999 )
	{
		sint32 sprIndex = (name - PPCREC_NAME_SPR0);
		if (sprIndex == SPR_LR)
			x64Emit_mov_reg64_mem32(x64GenContext, tempToRealRegister(imlInstruction->op_r_name.registerIndex), REG_RSP, offsetof(PPCInterpreter_t, spr.LR));
		else if (sprIndex == SPR_CTR)
			x64Emit_mov_reg64_mem32(x64GenContext, tempToRealRegister(imlInstruction->op_r_name.registerIndex), REG_RSP, offsetof(PPCInterpreter_t, spr.CTR));
		else if (sprIndex == SPR_XER)
			x64Emit_mov_reg64_mem32(x64GenContext, tempToRealRegister(imlInstruction->op_r_name.registerIndex), REG_RSP, offsetof(PPCInterpreter_t, spr.XER));
		else if (sprIndex >= SPR_UGQR0 && sprIndex <= SPR_UGQR7)
		{
			sint32 memOffset = offsetof(PPCInterpreter_t, spr.UGQR) + sizeof(PPCInterpreter_t::spr.UGQR[0]) * (sprIndex - SPR_UGQR0);
			x64Emit_mov_reg64_mem32(x64GenContext, tempToRealRegister(imlInstruction->op_r_name.registerIndex), REG_RSP, memOffset);
		}
		else
			assert_dbg();
		//x64Emit_mov_reg64_mem32(x64GenContext, tempToRealRegister(imlInstruction->op_r_name.registerIndex), REG_RSP, offsetof(PPCInterpreter_t, spr)+sizeof(uint32)*(name-PPCREC_NAME_SPR0));
	}
	else
		assert_dbg();
}

void PPCRecompilerX64Gen_imlInstruction_name_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	uint32 name = imlInstruction->op_r_name.name;
	if( name >= PPCREC_NAME_R0 && name < PPCREC_NAME_R0+32 )
	{
		x64Emit_mov_mem32_reg64(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, gpr)+sizeof(uint32)*(name-PPCREC_NAME_R0), tempToRealRegister(imlInstruction->op_r_name.registerIndex));
	}
	else if( name >= PPCREC_NAME_SPR0 && name < PPCREC_NAME_SPR0+999 )
	{
		uint32 sprIndex = (name - PPCREC_NAME_SPR0);
		if (sprIndex == SPR_LR)
			x64Emit_mov_mem32_reg64(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, spr.LR), tempToRealRegister(imlInstruction->op_r_name.registerIndex));
		else if (sprIndex == SPR_CTR)
			x64Emit_mov_mem32_reg64(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, spr.CTR), tempToRealRegister(imlInstruction->op_r_name.registerIndex));
		else if (sprIndex == SPR_XER)
			x64Emit_mov_mem32_reg64(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, spr.XER), tempToRealRegister(imlInstruction->op_r_name.registerIndex));
		else if (sprIndex >= SPR_UGQR0 && sprIndex <= SPR_UGQR7)
		{
			sint32 memOffset = offsetof(PPCInterpreter_t, spr.UGQR) + sizeof(PPCInterpreter_t::spr.UGQR[0]) * (sprIndex - SPR_UGQR0);
			x64Emit_mov_mem32_reg64(x64GenContext, REG_RSP, memOffset, tempToRealRegister(imlInstruction->op_r_name.registerIndex));
		}
		else
			assert_dbg();	
	}
	else
		assert_dbg();
}

uint8* codeMemoryBlock = nullptr;
sint32 codeMemoryBlockIndex = 0;
sint32 codeMemoryBlockSize = 0;

std::mutex mtx_allocExecutableMemory;

uint8* PPCRecompilerX86_allocateExecutableMemory(sint32 size)
{
	std::lock_guard<std::mutex> lck(mtx_allocExecutableMemory);
	if( codeMemoryBlockIndex+size > codeMemoryBlockSize )
	{
		// allocate new block
		codeMemoryBlockSize = std::max(1024*1024*4, size+1024); // 4MB (or more if the function is larger than 4MB)
		codeMemoryBlockIndex = 0;
		codeMemoryBlock = (uint8*)MemMapper::AllocateMemory(nullptr, codeMemoryBlockSize, MemMapper::PAGE_PERMISSION::P_RWX);
	}
	uint8* codeMem = codeMemoryBlock + codeMemoryBlockIndex;
	codeMemoryBlockIndex += size;
	// pad to 4 byte alignment
	while (codeMemoryBlockIndex & 3)
	{
		codeMemoryBlock[codeMemoryBlockIndex] = 0x90;
		codeMemoryBlockIndex++;
	}
	return codeMem;
}

void PPCRecompiler_dumpIML(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext);

bool PPCRecompiler_generateX64Code(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext)
{
	x64GenContext_t x64GenContext = {0};
	x64GenContext.codeBufferSize = 1024;
	x64GenContext.codeBuffer = (uint8*)malloc(x64GenContext.codeBufferSize);
	x64GenContext.codeBufferIndex = 0;
	x64GenContext.activeCRRegister = PPC_REC_INVALID_REGISTER;

	// generate iml instruction code
	bool codeGenerationFailed = false;
	for(sint32 s=0; s<ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];
		ppcImlGenContext->segmentList[s]->x64Offset = x64GenContext.codeBufferIndex;
		for(sint32 i=0; i<imlSegment->imlListCount; i++)
		{
			PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList+i;

			if( imlInstruction->type == PPCREC_IML_TYPE_R_NAME )
			{
				PPCRecompilerX64Gen_imlInstruction_r_name(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_NAME_R )
			{
				PPCRecompilerX64Gen_imlInstruction_name_r(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_R_R )
			{
				if( PPCRecompilerX64Gen_imlInstruction_r_r(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_S32)
			{
				if (PPCRecompilerX64Gen_imlInstruction_r_s32(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false)
				{
					codeGenerationFailed = true;
				}
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
			{
				if (PPCRecompilerX64Gen_imlInstruction_conditional_r_s32(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false)
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_R_R_S32 )
			{
				if( PPCRecompilerX64Gen_imlInstruction_r_r_s32(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_R_R_R )
			{
				if( PPCRecompilerX64Gen_imlInstruction_r_r_r(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_CJUMP )
			{
				if( PPCRecompilerX64Gen_imlInstruction_conditionalJump(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlSegment, imlInstruction) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK )
			{
				PPCRecompilerX64Gen_imlInstruction_conditionalJumpCycleCheck(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_MACRO )
			{
				if( PPCRecompilerX64Gen_imlInstruction_macro(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_LOAD )
			{
				if( PPCRecompilerX64Gen_imlInstruction_load(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction, false) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_LOAD_INDEXED )
			{
				if( PPCRecompilerX64Gen_imlInstruction_load(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction, true) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_STORE )
			{
				if( PPCRecompilerX64Gen_imlInstruction_store(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction, false) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_STORE_INDEXED )
			{
				if( PPCRecompilerX64Gen_imlInstruction_store(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction, true) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_MEM2MEM)
			{
				PPCRecompilerX64Gen_imlInstruction_mem2mem(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_CR )
			{
				if( PPCRecompilerX64Gen_imlInstruction_cr(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_JUMPMARK )
			{
				// no op
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_NO_OP )
			{
				// no op
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_PPC_ENTER )
			{
				PPCRecompilerX64Gen_imlInstruction_ppcEnter(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_NAME )
			{
				PPCRecompilerX64Gen_imlInstruction_fpr_r_name(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_NAME_R )
			{
				PPCRecompilerX64Gen_imlInstruction_fpr_name_r(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD )
			{
				if( PPCRecompilerX64Gen_imlInstruction_fpr_load(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction, false) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED )
			{
				if( PPCRecompilerX64Gen_imlInstruction_fpr_load(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction, true) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE )
			{
				if( PPCRecompilerX64Gen_imlInstruction_fpr_store(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction, false) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE_INDEXED )
			{
				if( PPCRecompilerX64Gen_imlInstruction_fpr_store(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction, true) == false )
				{
					codeGenerationFailed = true;
				}
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R )
			{
				PPCRecompilerX64Gen_imlInstruction_fpr_r_r(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);		
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R )
			{
				PPCRecompilerX64Gen_imlInstruction_fpr_r_r_r(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);		
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R_R )
			{
				PPCRecompilerX64Gen_imlInstruction_fpr_r_r_r_r(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);		
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_FPR_R )
			{
				PPCRecompilerX64Gen_imlInstruction_fpr_r(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);		
			}
			else
			{
				debug_printf("PPCRecompiler_generateX64Code(): Unsupported iml type 0x%x\n", imlInstruction->type);
				assert_dbg();
			}
		}
	}
	// handle failed code generation
	if( codeGenerationFailed )
	{
		free(x64GenContext.codeBuffer);
		if (x64GenContext.relocateOffsetTable)
			free(x64GenContext.relocateOffsetTable);
		return false;
	}
	// allocate executable memory
	uint8* executableMemory = PPCRecompilerX86_allocateExecutableMemory(x64GenContext.codeBufferIndex);
	size_t baseAddress = (size_t)executableMemory;
	// fix relocs
	for(sint32 i=0; i<x64GenContext.relocateOffsetTableCount; i++)
	{
		if( x64GenContext.relocateOffsetTable[i].type == X86_RELOC_MAKE_RELATIVE )
		{
			assert_dbg(); // deprecated
		}
		else if(x64GenContext.relocateOffsetTable[i].type == X64_RELOC_LINK_TO_PPC || x64GenContext.relocateOffsetTable[i].type == X64_RELOC_LINK_TO_SEGMENT)
		{
			// if link to PPC, search for segment that starts with this offset
			uint32 ppcOffset = (uint32)(size_t)x64GenContext.relocateOffsetTable[i].extraInfo;
			uint32 x64Offset = 0xFFFFFFFF;
			if (x64GenContext.relocateOffsetTable[i].type == X64_RELOC_LINK_TO_PPC)
			{
				for (sint32 s = 0; s < ppcImlGenContext->segmentListCount; s++)
				{
					if (ppcImlGenContext->segmentList[s]->isJumpDestination && ppcImlGenContext->segmentList[s]->jumpDestinationPPCAddress == ppcOffset)
					{
						x64Offset = ppcImlGenContext->segmentList[s]->x64Offset;
						break;
					}
				}
				if (x64Offset == 0xFFFFFFFF)
				{
					debug_printf("Recompiler could not resolve jump (function at 0x%08x)\n", PPCRecFunction->ppcAddress);
					// todo: Cleanup
					return false;
				}
			}
			else
			{
				PPCRecImlSegment_t* destSegment = (PPCRecImlSegment_t*)x64GenContext.relocateOffsetTable[i].extraInfo;
				x64Offset = destSegment->x64Offset;
			}
			uint32 relocBase = x64GenContext.relocateOffsetTable[i].offset;
			uint8* relocInstruction = x64GenContext.codeBuffer+relocBase;
			if( relocInstruction[0] == 0x0F && (relocInstruction[1] >= 0x80 && relocInstruction[1] <= 0x8F) )
			{
				// Jcc relativeImm32
				sint32 distanceNearJump = (sint32)((baseAddress + x64Offset) - (baseAddress + relocBase + 2));
				if (distanceNearJump >= -128 && distanceNearJump < 127) // disabled
				{
					// convert to near Jcc
					*(uint8*)(relocInstruction + 0) = (uint8)(relocInstruction[1]-0x80 + 0x70);
					// patch offset
					*(uint8*)(relocInstruction + 1) = (uint8)distanceNearJump;
					// replace unused 4 bytes with NOP instruction
					relocInstruction[2] = 0x0F;
					relocInstruction[3] = 0x1F;
					relocInstruction[4] = 0x40;
					relocInstruction[5] = 0x00;
				}
				else
				{
					// patch offset
					*(uint32*)(relocInstruction + 2) = (uint32)((baseAddress + x64Offset) - (baseAddress + relocBase + 6));
				}
			}
			else if( relocInstruction[0] == 0xE9 )
			{
				// JMP relativeImm32
				*(uint32*)(relocInstruction+1) = (uint32)((baseAddress+x64Offset)-(baseAddress+relocBase+5));
			}
			else
				assert_dbg();
		}
		else
		{
			assert_dbg();
		}
	}

	// copy code to executable memory
	memcpy(executableMemory, x64GenContext.codeBuffer, x64GenContext.codeBufferIndex);
	free(x64GenContext.codeBuffer);
	x64GenContext.codeBuffer = nullptr;
	if (x64GenContext.relocateOffsetTable)
		free(x64GenContext.relocateOffsetTable);
	// set code
	PPCRecFunction->x86Code = executableMemory;
	PPCRecFunction->x86Size = x64GenContext.codeBufferIndex;
	return true;
}

void PPCRecompilerX64Gen_generateEnterRecompilerCode()
{
	x64GenContext_t x64GenContext = {0};
	x64GenContext.codeBufferSize = 1024;
	x64GenContext.codeBuffer = (uint8*)malloc(x64GenContext.codeBufferSize);
	x64GenContext.codeBufferIndex = 0;
	x64GenContext.activeCRRegister = PPC_REC_INVALID_REGISTER;

	// start of recompiler entry function
	x64Gen_push_reg64(&x64GenContext, REG_RAX);
	x64Gen_push_reg64(&x64GenContext, REG_RCX);
	x64Gen_push_reg64(&x64GenContext, REG_RDX);
	x64Gen_push_reg64(&x64GenContext, REG_RBX);
	x64Gen_push_reg64(&x64GenContext, REG_RBP);
	x64Gen_push_reg64(&x64GenContext, REG_RDI);
	x64Gen_push_reg64(&x64GenContext, REG_RSI);
	x64Gen_push_reg64(&x64GenContext, REG_R8);
	x64Gen_push_reg64(&x64GenContext, REG_R9);
	x64Gen_push_reg64(&x64GenContext, REG_R10);
	x64Gen_push_reg64(&x64GenContext, REG_R11);
	x64Gen_push_reg64(&x64GenContext, REG_R12);
	x64Gen_push_reg64(&x64GenContext, REG_R13);
	x64Gen_push_reg64(&x64GenContext, REG_R14);
	x64Gen_push_reg64(&x64GenContext, REG_R15);

	// 000000007775EF04 | E8 00 00 00 00                      call +0x00
	x64Gen_writeU8(&x64GenContext, 0xE8);
	x64Gen_writeU8(&x64GenContext, 0x00);
	x64Gen_writeU8(&x64GenContext, 0x00);
	x64Gen_writeU8(&x64GenContext, 0x00);
	x64Gen_writeU8(&x64GenContext, 0x00);
	//000000007775EF09 | 48 83 04 24 05                       add qword ptr ss:[rsp],5
	x64Gen_writeU8(&x64GenContext, 0x48);
	x64Gen_writeU8(&x64GenContext, 0x83);
	x64Gen_writeU8(&x64GenContext, 0x04);
	x64Gen_writeU8(&x64GenContext, 0x24);
	uint32 jmpPatchOffset = x64GenContext.codeBufferIndex;
	x64Gen_writeU8(&x64GenContext, 0); // skip the distance until after the JMP
	x64Emit_mov_mem64_reg64(&x64GenContext, REG_RDX, offsetof(PPCInterpreter_t, rspTemp), REG_RSP);


	// MOV RSP, RDX (ppc interpreter instance)
	x64Gen_mov_reg64_reg64(&x64GenContext, REG_RSP, REG_RDX);
	// MOV R15, ppcRecompilerInstanceData
	x64Gen_mov_reg64_imm64(&x64GenContext, REG_R15, (uint64)ppcRecompilerInstanceData);
	// MOV R13, memory_base
	x64Gen_mov_reg64_imm64(&x64GenContext, REG_R13, (uint64)memory_base);

	//JMP recFunc
	x64Gen_jmp_reg64(&x64GenContext, REG_RCX); // call argument 1

	x64GenContext.codeBuffer[jmpPatchOffset] = (x64GenContext.codeBufferIndex-(jmpPatchOffset-4));

	//recompilerExit1:
	x64Gen_pop_reg64(&x64GenContext, REG_R15);
	x64Gen_pop_reg64(&x64GenContext, REG_R14);
	x64Gen_pop_reg64(&x64GenContext, REG_R13);
	x64Gen_pop_reg64(&x64GenContext, REG_R12);
	x64Gen_pop_reg64(&x64GenContext, REG_R11);
	x64Gen_pop_reg64(&x64GenContext, REG_R10);
	x64Gen_pop_reg64(&x64GenContext, REG_R9);
	x64Gen_pop_reg64(&x64GenContext, REG_R8);
	x64Gen_pop_reg64(&x64GenContext, REG_RSI);
	x64Gen_pop_reg64(&x64GenContext, REG_RDI);
	x64Gen_pop_reg64(&x64GenContext, REG_RBP);
	x64Gen_pop_reg64(&x64GenContext, REG_RBX);
	x64Gen_pop_reg64(&x64GenContext, REG_RDX);
	x64Gen_pop_reg64(&x64GenContext, REG_RCX);
	x64Gen_pop_reg64(&x64GenContext, REG_RAX);
	// RET
	x64Gen_ret(&x64GenContext);

	uint8* executableMemory = PPCRecompilerX86_allocateExecutableMemory(x64GenContext.codeBufferIndex);
	// copy code to executable memory
	memcpy(executableMemory, x64GenContext.codeBuffer, x64GenContext.codeBufferIndex);
	free(x64GenContext.codeBuffer);
	PPCRecompiler_enterRecompilerCode = (void ATTR_MS_ABI (*)(uint64,uint64))executableMemory;
}


void* PPCRecompilerX64Gen_generateLeaveRecompilerCode()
{
	x64GenContext_t x64GenContext = {0};
	x64GenContext.codeBufferSize = 128;
	x64GenContext.codeBuffer = (uint8*)malloc(x64GenContext.codeBufferSize);
	x64GenContext.codeBufferIndex = 0;
	x64GenContext.activeCRRegister = PPC_REC_INVALID_REGISTER;

	// update instruction pointer
	// LR is in EDX
	x64Emit_mov_mem32_reg32(&x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, instructionPointer), REG_EDX);

	// MOV RSP, [ppcRecompilerX64_rspTemp]
	x64Emit_mov_reg64_mem64(&x64GenContext, REG_RSP, REG_RESV_HCPU, offsetof(PPCInterpreter_t, rspTemp));

	// RET
	x64Gen_ret(&x64GenContext);

	uint8* executableMemory = PPCRecompilerX86_allocateExecutableMemory(x64GenContext.codeBufferIndex);
	// copy code to executable memory
	memcpy(executableMemory, x64GenContext.codeBuffer, x64GenContext.codeBufferIndex);
	free(x64GenContext.codeBuffer);
	return executableMemory;
}

void PPCRecompilerX64Gen_generateRecompilerInterfaceFunctions()
{
	PPCRecompilerX64Gen_generateEnterRecompilerCode();
	PPCRecompiler_leaveRecompilerCode_unvisited = (void ATTR_MS_ABI (*)())PPCRecompilerX64Gen_generateLeaveRecompilerCode();
	PPCRecompiler_leaveRecompilerCode_visited = (void ATTR_MS_ABI (*)())PPCRecompilerX64Gen_generateLeaveRecompilerCode();
	cemu_assert_debug(PPCRecompiler_leaveRecompilerCode_unvisited != PPCRecompiler_leaveRecompilerCode_visited);
}