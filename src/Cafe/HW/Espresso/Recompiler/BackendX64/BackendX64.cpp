#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterHelper.h"
#include "../PPCRecompiler.h"
#include "../PPCRecompilerIml.h"
#include "BackendX64.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "util/MemMapper/MemMapper.h"
#include "Common/cpu_features.h"

static x86Assembler64::GPR32 _reg32(IMLReg physReg)
{
	cemu_assert_debug(physReg.GetRegFormat() == IMLRegFormat::I32);
	IMLRegID regId = physReg.GetRegID();
	cemu_assert_debug(regId < 16);
	return (x86Assembler64::GPR32)regId;
}

static uint32 _reg64(IMLReg physReg)
{
	cemu_assert_debug(physReg.GetRegFormat() == IMLRegFormat::I64);
	IMLRegID regId = physReg.GetRegID();
	cemu_assert_debug(regId < 16);
	return regId;
}

uint32 _regF64(IMLReg physReg)
{
	cemu_assert_debug(physReg.GetRegFormat() == IMLRegFormat::F64);
	IMLRegID regId = physReg.GetRegID();
	cemu_assert_debug(regId >= IMLArchX86::PHYSREG_FPR_BASE && regId < IMLArchX86::PHYSREG_FPR_BASE+16);
	regId -= IMLArchX86::PHYSREG_FPR_BASE;
	return regId;
}

static x86Assembler64::GPR8_REX _reg8(IMLReg physReg)
{
	cemu_assert_debug(physReg.GetRegFormat() == IMLRegFormat::I32); // for now these are represented as 32bit
	return (x86Assembler64::GPR8_REX)physReg.GetRegID();
}

static x86Assembler64::GPR32 _reg32_from_reg8(x86Assembler64::GPR8_REX regId)
{
	return (x86Assembler64::GPR32)regId;
}

static x86Assembler64::GPR8_REX _reg8_from_reg32(x86Assembler64::GPR32 regId)
{
	return (x86Assembler64::GPR8_REX)regId;
}

static x86Assembler64::GPR8_REX _reg8_from_reg64(uint32 regId)
{
	return (x86Assembler64::GPR8_REX)regId;
}

static x86Assembler64::GPR64 _reg64_from_reg32(x86Assembler64::GPR32 regId)
{
	return (x86Assembler64::GPR64)regId;
}

X86Cond _x86Cond(IMLCondition imlCond)
{
	switch (imlCond)
	{
	case IMLCondition::EQ:
		return X86_CONDITION_Z;
	case IMLCondition::NEQ:
		return X86_CONDITION_NZ;
	case IMLCondition::UNSIGNED_GT:
		return X86_CONDITION_NBE;
	case IMLCondition::UNSIGNED_LT:
		return X86_CONDITION_B;
	case IMLCondition::SIGNED_GT:
		return X86_CONDITION_NLE;
	case IMLCondition::SIGNED_LT:
		return X86_CONDITION_L;
	default:
		break;
	}
	cemu_assert_suspicious();
	return X86_CONDITION_Z;
}

X86Cond _x86CondInverted(IMLCondition imlCond)
{
	switch (imlCond)
	{
	case IMLCondition::EQ:
		return X86_CONDITION_NZ;
	case IMLCondition::NEQ:
		return X86_CONDITION_Z;
	case IMLCondition::UNSIGNED_GT:
		return X86_CONDITION_BE;
	case IMLCondition::UNSIGNED_LT:
		return X86_CONDITION_NB;
	case IMLCondition::SIGNED_GT:
		return X86_CONDITION_LE;
	case IMLCondition::SIGNED_LT:
		return X86_CONDITION_NL;
	default:
		break;
	}
	cemu_assert_suspicious();
	return X86_CONDITION_Z;
}

X86Cond _x86Cond(IMLCondition imlCond, bool condIsInverted)
{
	if (condIsInverted)
		return _x86CondInverted(imlCond);
	return _x86Cond(imlCond);
}

/*
* Remember current instruction output offset for reloc
* The instruction generated after this method has been called will be adjusted
*/
void PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext_t* x64GenContext, void* extraInfo = nullptr)
{
	x64GenContext->relocateOffsetTable2.emplace_back(x64GenContext->emitter->GetWriteIndex(), extraInfo);
}

void PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext_t* x64GenContext, sint32 jumpInstructionOffset, sint32 destinationOffset)
{
	uint8* instructionData = x64GenContext->emitter->GetBufferPtr() + jumpInstructionOffset;
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

bool PPCRecompilerX64Gen_imlInstruction_macro(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	if (imlInstruction->operation == PPCREC_IML_MACRO_B_TO_REG)
	{
		uint32 branchDstReg = _reg32(imlInstruction->op_macro.paramReg);
		if(X86_REG_RDX != branchDstReg)
			x64Gen_mov_reg64_reg64(x64GenContext, X86_REG_RDX, branchDstReg);
		// potential optimization: Use branchDstReg directly if possible instead of moving to RDX/EDX
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
		x64Gen_mov_mem32Reg64_imm32(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, spr.LR), newLR);
		// remember new instruction pointer in RDX
		uint32 newIP = imlInstruction->op_macro.param2;
		x64Gen_mov_reg64Low32_imm32(x64GenContext, X86_REG_RDX, newIP);
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
		x64Gen_mov_reg64Low32_imm32(x64GenContext, X86_REG_RDX, newIP);
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
		x64Gen_mov_reg64Low32_imm32(x64GenContext, X86_REG_RDX, currentInstructionAddress);

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
		x64Gen_sub_mem32reg64_imm32(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, remainingCycles), cycleCount);
		return true;
	}
	else if( imlInstruction->operation == PPCREC_IML_MACRO_HLE )
	{
		uint32 ppcAddress = imlInstruction->op_macro.param;
		uint32 funcId = imlInstruction->op_macro.param2;
		//x64Gen_int3(x64GenContext);
		// update instruction pointer
		x64Gen_mov_mem32Reg64_imm32(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, instructionPointer), ppcAddress);
		//// save hCPU (RSP)
		//x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)&ppcRecompilerX64_hCPUTemp);
		//x64Emit_mov_mem64_reg64(x64GenContext, REG_RESV_TEMP, 0, REG_RSP);
		// set parameters
		x64Gen_mov_reg64_reg64(x64GenContext, X86_REG_RCX, X86_REG_RSP);
		x64Gen_mov_reg64_imm64(x64GenContext, X86_REG_RDX, funcId);
		// restore stackpointer from executionContext/hCPU->rspTemp
		x64Emit_mov_reg64_mem64(x64GenContext, X86_REG_RSP, REG_RESV_HCPU, offsetof(PPCInterpreter_t, rspTemp));
		//x64Emit_mov_reg64_mem64(x64GenContext, REG_RSP, REG_R14, 0);
		//x64Gen_int3(x64GenContext);
		// reserve space on stack for call parameters
		x64Gen_sub_reg64_imm32(x64GenContext, X86_REG_RSP, 8*11); // must be uneven number in order to retain stack 0x10 alignment
		x64Gen_mov_reg64_imm64(x64GenContext, X86_REG_RBP, 0);
		// call HLE function
		x64Gen_mov_reg64_imm64(x64GenContext, X86_REG_RAX, (uint64)PPCRecompiler_virtualHLE);
		x64Gen_call_reg64(x64GenContext, X86_REG_RAX);
		// restore RSP to hCPU (from RAX, result of PPCRecompiler_virtualHLE)
		//x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)&ppcRecompilerX64_hCPUTemp);
		//x64Emit_mov_reg64_mem64Reg64(x64GenContext, REG_RSP, REG_RESV_TEMP, 0);
		x64Gen_mov_reg64_reg64(x64GenContext, X86_REG_RSP, X86_REG_RAX);
		// MOV R15, ppcRecompilerInstanceData
		x64Gen_mov_reg64_imm64(x64GenContext, X86_REG_R15, (uint64)ppcRecompilerInstanceData);
		// MOV R13, memory_base
		x64Gen_mov_reg64_imm64(x64GenContext, X86_REG_R13, (uint64)memory_base);
		// check if cycles where decreased beyond zero, if yes -> leave recompiler
		x64Gen_bt_mem8(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, remainingCycles), 31); // check if negative
		sint32 jumpInstructionOffset1 = x64GenContext->emitter->GetWriteIndex();
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NOT_CARRY, 0);
		//x64Gen_int3(x64GenContext);
		//x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RDX, ppcAddress);

		x64Emit_mov_reg64_mem32(x64GenContext, X86_REG_RDX, X86_REG_RSP, offsetof(PPCInterpreter_t, instructionPointer));
		// set EAX to 0 (we assume that ppcRecompilerDirectJumpTable[0] will be a recompiler escape function)
		x64Gen_xor_reg32_reg32(x64GenContext, X86_REG_RAX, X86_REG_RAX);
		// ADD RAX, R15 (R15 -> Pointer to ppcRecompilerInstanceData
		x64Gen_add_reg64_reg64(x64GenContext, X86_REG_RAX, X86_REG_R15);
		//// JMP [recompilerCallTable+EAX/4*8]
		//x64Gen_int3(x64GenContext);
		x64Gen_jmp_memReg64(x64GenContext, X86_REG_RAX, (uint32)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1, x64GenContext->emitter->GetWriteIndex());
		// check if instruction pointer was changed
		// assign new instruction pointer to EAX
		x64Emit_mov_reg64_mem32(x64GenContext, X86_REG_RAX, X86_REG_RSP, offsetof(PPCInterpreter_t, instructionPointer));
		// remember instruction pointer in REG_EDX
		x64Gen_mov_reg64_reg64(x64GenContext, X86_REG_RDX, X86_REG_RAX);
		// EAX *= 2
		x64Gen_add_reg64_reg64(x64GenContext, X86_REG_RAX, X86_REG_RAX);
		// ADD RAX, R15 (R15 -> Pointer to ppcRecompilerInstanceData
		x64Gen_add_reg64_reg64(x64GenContext, X86_REG_RAX, X86_REG_R15);
		// JMP [ppcRecompilerDirectJumpTable+RAX/4*8]
		x64Gen_jmp_memReg64(x64GenContext, X86_REG_RAX, (uint32)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
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
bool PPCRecompilerX64Gen_imlInstruction_load(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction, bool indexed)
{
	cemu_assert_debug(imlInstruction->op_storeLoad.registerData.GetRegFormat() == IMLRegFormat::I32);
	cemu_assert_debug(imlInstruction->op_storeLoad.registerMem.GetRegFormat() == IMLRegFormat::I32);
	if (indexed)
		cemu_assert_debug(imlInstruction->op_storeLoad.registerMem2.GetRegFormat() == IMLRegFormat::I32);

	IMLRegID realRegisterData = imlInstruction->op_storeLoad.registerData.GetRegID();
	IMLRegID realRegisterMem = imlInstruction->op_storeLoad.registerMem.GetRegID();
	IMLRegID realRegisterMem2 = PPC_REC_INVALID_REGISTER;
	if( indexed )
		realRegisterMem2 = imlInstruction->op_storeLoad.registerMem2.GetRegID();
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
				x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, realRegisterData, X86_REG_R13, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32);
				//if (indexed && realRegisterMem != realRegisterData)
				//	x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			}
			else
			{
				x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, realRegisterData, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
			}
		}
		else
		{
			if (indexed)
			{
				x64Emit_mov_reg32_mem32(x64GenContext, realRegisterData, X86_REG_R13, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32);
				//if (realRegisterMem != realRegisterData)
				//	x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
				if (switchEndian)
					x64Gen_bswap_reg64Lower32bit(x64GenContext, realRegisterData);
			}
			else
			{
				x64Emit_mov_reg32_mem32(x64GenContext, realRegisterData, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
				if (switchEndian)
					x64Gen_bswap_reg64Lower32bit(x64GenContext, realRegisterData);
			}
		}
	}
	else if( imlInstruction->op_storeLoad.copyWidth == 16 )
	{
		if (indexed)
		{
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}			
		if(g_CPUFeatures.x86.movbe && switchEndian )
		{
			x64Gen_movBEZeroExtend_reg64Low16_mem16Reg64PlusReg64(x64GenContext, realRegisterData, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
			if( indexed && realRegisterMem != realRegisterData )
				x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		else
		{
			x64Gen_movZeroExtend_reg64Low16_mem16Reg64PlusReg64(x64GenContext, realRegisterData, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
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
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		if( signExtend )
			x64Gen_movSignExtend_reg64Low32_mem8Reg64PlusReg64(x64GenContext, realRegisterData, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
		else
			x64Emit_movZX_reg32_mem8(x64GenContext, realRegisterData, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32);
		if( indexed && realRegisterMem != realRegisterData )
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
	}
	else
		return false;
	return true;
}

/*
* Write to memory
*/
bool PPCRecompilerX64Gen_imlInstruction_store(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction, bool indexed)
{
	cemu_assert_debug(imlInstruction->op_storeLoad.registerData.GetRegFormat() == IMLRegFormat::I32);
	cemu_assert_debug(imlInstruction->op_storeLoad.registerMem.GetRegFormat() == IMLRegFormat::I32);
	if (indexed)
		cemu_assert_debug(imlInstruction->op_storeLoad.registerMem2.GetRegFormat() == IMLRegFormat::I32);

	IMLRegID realRegisterData = imlInstruction->op_storeLoad.registerData.GetRegID();
	IMLRegID realRegisterMem = imlInstruction->op_storeLoad.registerMem.GetRegID();
	IMLRegID realRegisterMem2 = PPC_REC_INVALID_REGISTER;
	if (indexed)
		realRegisterMem2 = imlInstruction->op_storeLoad.registerMem2.GetRegID();

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
		if (!g_CPUFeatures.x86.movbe && swapEndian)
			x64Gen_bswap_reg64Lower32bit(x64GenContext, valueRegister);
		if (indexed)
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		if (g_CPUFeatures.x86.movbe && swapEndian)
			x64Gen_movBETruncate_mem32Reg64PlusReg64_reg64(x64GenContext, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, valueRegister);
		else
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, valueRegister);
		if (indexed)
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
	}
	else if (imlInstruction->op_storeLoad.copyWidth == 16)
	{
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, realRegisterData);
		if (swapEndian)
			x64Gen_rol_reg64Low16_imm8(x64GenContext, REG_RESV_TEMP, 8);
		if (indexed)
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		x64Gen_movTruncate_mem16Reg64PlusReg64_reg64(x64GenContext, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
		if (indexed)
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		// todo: Optimize this, e.g. by using MOVBE
	}
	else if (imlInstruction->op_storeLoad.copyWidth == 8)
	{
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
	else
		return false;
	return true;
}

void PPCRecompilerX64Gen_imlInstruction_atomic_cmp_store(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	auto regBoolOut = _reg32_from_reg8(_reg8(imlInstruction->op_atomic_compare_store.regBoolOut));
	auto regEA = _reg32(imlInstruction->op_atomic_compare_store.regEA);
	auto regVal = _reg32(imlInstruction->op_atomic_compare_store.regWriteValue);
	auto regCmp = _reg32(imlInstruction->op_atomic_compare_store.regCompareValue);

	cemu_assert_debug(regBoolOut == X86_REG_EAX);
	cemu_assert_debug(regEA != X86_REG_EAX);
	cemu_assert_debug(regVal != X86_REG_EAX);
	cemu_assert_debug(regCmp != X86_REG_EAX);

	x64GenContext->emitter->MOV_dd(X86_REG_EAX, regCmp);
	x64GenContext->emitter->LockPrefix();
	x64GenContext->emitter->CMPXCHG_dd_l(REG_RESV_MEMBASE, 0, _reg64_from_reg32(regEA), 1, regVal);
	x64GenContext->emitter->SETcc_b(X86Cond::X86_CONDITION_Z, regBoolOut);
	x64GenContext->emitter->AND_di32(regBoolOut, 1); // SETcc doesn't clear the upper bits so we do it manually here
}

void PPCRecompilerX64Gen_imlInstruction_call_imm(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	// the register allocator takes care of spilling volatile registers and moving parameters to the right registers, so we don't need to do any special handling here
	x64GenContext->emitter->SUB_qi8(X86_REG_RSP, 0x28); // reserve enough space for any parameters while keeping stack alignment of 16 intact
	x64GenContext->emitter->MOV_qi64(X86_REG_RAX, imlInstruction->op_call_imm.callAddress);
	x64GenContext->emitter->CALL_q(X86_REG_RAX);
	x64GenContext->emitter->ADD_qi8(X86_REG_RSP, 0x28);
}

bool PPCRecompilerX64Gen_imlInstruction_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = _reg32(imlInstruction->op_r_r.regR);
	auto regA = _reg32(imlInstruction->op_r_r.regA);

	if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	{
		// registerResult = registerA
		if (regR != regA)
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ENDIAN_SWAP)
	{
		if (regA != regR)
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, regR, regA); // if movbe is available we can move and swap in a single instruction?
		x64Gen_bswap_reg64Lower32bit(x64GenContext, regR);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_ASSIGN_S8_TO_S32 )
	{
		x64Gen_movSignExtend_reg64Low32_reg64Low8(x64GenContext, regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN_S16_TO_S32)
	{
		x64Gen_movSignExtend_reg64Low32_reg64Low16(x64GenContext, regR, reg32ToReg16(regA));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_NOT )
	{
		// copy register content if different registers
		if( regR != regA )
			x64Gen_mov_reg64_reg64(x64GenContext, regR, regA);
		x64Gen_not_reg64Low32(x64GenContext, regR);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_NEG)
	{
		// copy register content if different registers
		if (regR != regA)
			x64Gen_mov_reg64_reg64(x64GenContext, regR, regA);
		x64Gen_neg_reg64Low32(x64GenContext, regR);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_CNTLZW )
	{
		// count leading zeros
		// LZCNT instruction (part of SSE4, CPUID.80000001H:ECX.ABM[Bit 5])
		if(g_CPUFeatures.x86.lzcnt)
		{
			x64Gen_lzcnt_reg64Low32_reg64Low32(x64GenContext, regR, regA);
		}
		else
		{
			x64Gen_test_reg64Low32_reg64Low32(x64GenContext, regA, regA);
			sint32 jumpInstructionOffset1 = x64GenContext->emitter->GetWriteIndex();
			x64Gen_jmpc_near(x64GenContext, X86_CONDITION_EQUAL, 0);
			x64Gen_bsr_reg64Low32_reg64Low32(x64GenContext, regR, regA);
			x64Gen_neg_reg64Low32(x64GenContext, regR);
			x64Gen_add_reg64Low32_imm32(x64GenContext, regR, 32-1);
			sint32 jumpInstructionOffset2 = x64GenContext->emitter->GetWriteIndex();
			x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NONE, 0);
			PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1, x64GenContext->emitter->GetWriteIndex());
			x64Gen_mov_reg64Low32_imm32(x64GenContext, regR, 32);
			PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2, x64GenContext->emitter->GetWriteIndex());
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_X86_CMP)
	{
		x64GenContext->emitter->CMP_dd(regR, regA);
	}
	else
	{
		debug_printf("PPCRecompilerX64Gen_imlInstruction_r_r(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_r_s32(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = _reg32(imlInstruction->op_r_immS32.regR);

	if( imlInstruction->operation == PPCREC_IML_OP_ASSIGN )
	{
		x64Gen_mov_reg64Low32_imm32(x64GenContext, regR, (uint32)imlInstruction->op_r_immS32.immS32);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_LEFT_ROTATE )
	{
		cemu_assert_debug((imlInstruction->op_r_immS32.immS32 & 0x80) == 0);
		x64Gen_rol_reg64Low32_imm8(x64GenContext, regR, (uint8)imlInstruction->op_r_immS32.immS32);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_X86_CMP)
	{
		sint32 imm = imlInstruction->op_r_immS32.immS32;
		x64GenContext->emitter->CMP_di32(regR, imm);
	}
	else
	{
		debug_printf("PPCRecompilerX64Gen_imlInstruction_r_s32(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_conditional_r_s32(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	cemu_assert_unimplemented();
	//if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	//{
	//	// registerResult = immS32 (conditional)
	//	if (imlInstruction->crRegister != PPC_REC_INVALID_REGISTER)
	//	{
	//		assert_dbg();
	//	}

	//	x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, (uint32)imlInstruction->op_conditional_r_s32.immS32);
	//	uint8 crBitIndex = imlInstruction->op_conditional_r_s32.crRegisterIndex * 4 + imlInstruction->op_conditional_r_s32.crBitIndex;
	//	x64Gen_bt_mem8(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, cr) + crBitIndex * sizeof(uint8), 0);
	//	if (imlInstruction->op_conditional_r_s32.bitMustBeSet)
	//		x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, X86_CONDITION_CARRY, imlInstruction->op_conditional_r_s32.registerIndex, REG_RESV_TEMP);
	//	else
	//		x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext, X86_CONDITION_NOT_CARRY, imlInstruction->op_conditional_r_s32.registerIndex, REG_RESV_TEMP);
	//	return true;
	//}
	return false;
}

bool PPCRecompilerX64Gen_imlInstruction_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	auto rRegResult = _reg32(imlInstruction->op_r_r_r.regR);
	auto rRegOperand1 = _reg32(imlInstruction->op_r_r_r.regA);
	auto rRegOperand2 = _reg32(imlInstruction->op_r_r_r.regB);

	if (imlInstruction->operation == PPCREC_IML_OP_ADD)
	{
		// registerResult = registerOperand1 + registerOperand2
		if( (rRegResult == rRegOperand1) || (rRegResult == rRegOperand2) )
		{
			// be careful not to overwrite the operand before we use it
			if( rRegResult == rRegOperand1 )
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
			else
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand1);
		}
		else
		{
			// copy operand1 to destination register before doing addition
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand1);
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_SUB )
	{
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
			x64Gen_neg_reg64Low32(x64GenContext, rRegResult);
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand1);
		}
		else
		{
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand1);
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
		}
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_OR || imlInstruction->operation == PPCREC_IML_OP_AND || imlInstruction->operation == PPCREC_IML_OP_XOR)
	{
		if (rRegResult == rRegOperand2)
			std::swap(rRegOperand1, rRegOperand2);

		if (rRegResult != rRegOperand1)
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand1);

		if (imlInstruction->operation == PPCREC_IML_OP_OR)
			x64Gen_or_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
		else if (imlInstruction->operation == PPCREC_IML_OP_AND)
			x64Gen_and_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
		else
			x64Gen_xor_reg64Low32_reg64Low32(x64GenContext, rRegResult, rRegOperand2);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_SIGNED )
	{
		// registerResult = registerOperand1 * registerOperand2
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
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_SLW || imlInstruction->operation == PPCREC_IML_OP_SRW )
	{
		// registerResult = registerOperand1(rA) >> registerOperand2(rB) (up to 63 bits)

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
				sint32 jumpInstructionOffset = x64GenContext->emitter->GetWriteIndex();
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
				PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset, x64GenContext->emitter->GetWriteIndex());
			}
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, REG_RESV_TEMP);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_LEFT_ROTATE )
	{
		// todo: Use BMI2 rotate if available
		// check if CL/ECX/RCX is available
		if( rRegResult != X86_REG_RCX && rRegOperand1 != X86_REG_RCX && rRegOperand2 != X86_REG_RCX )
		{
			// swap operand 2 with RCX
			x64Gen_xchg_reg64_reg64(x64GenContext, X86_REG_RCX, rRegOperand2);
			// move operand 1 to temp register
			x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand1);
			// rotate
			x64Gen_rol_reg64Low32_cl(x64GenContext, REG_RESV_TEMP);
			// undo swap operand 2 with RCX
			x64Gen_xchg_reg64_reg64(x64GenContext, X86_REG_RCX, rRegOperand2);
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
				sint32 jumpInstructionOffset = x64GenContext->emitter->GetWriteIndex();
				x64Gen_jmpc_near(x64GenContext, X86_CONDITION_EQUAL, 0); // jump if bit not set
				x64Gen_rol_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (1<<b));
				PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset, x64GenContext->emitter->GetWriteIndex());
			}
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, REG_RESV_TEMP);
		}
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_S ||
		imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_U ||
		imlInstruction->operation == PPCREC_IML_OP_LEFT_SHIFT)
	{
		if(g_CPUFeatures.x86.bmi2)
		{
			if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_S)
				x64Gen_sarx_reg32_reg32_reg32(x64GenContext, rRegResult, rRegOperand1, rRegOperand2);
			else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_U)
				x64Gen_shrx_reg32_reg32_reg32(x64GenContext, rRegResult, rRegOperand1, rRegOperand2);
			else if (imlInstruction->operation == PPCREC_IML_OP_LEFT_SHIFT)
				x64Gen_shlx_reg32_reg32_reg32(x64GenContext, rRegResult, rRegOperand1, rRegOperand2);
		}
		else
		{
			cemu_assert_debug(rRegResult != rRegOperand2);
			cemu_assert_debug(rRegResult != X86_REG_RCX);
			cemu_assert_debug(rRegOperand2 == X86_REG_RCX);
			if(rRegOperand1 != rRegResult)
				x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, rRegOperand1);
			if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_S)
				x64GenContext->emitter->SAR_d_CL(rRegResult);
			else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_U)
				x64GenContext->emitter->SHR_d_CL(rRegResult);
			else if (imlInstruction->operation == PPCREC_IML_OP_LEFT_SHIFT)
				x64GenContext->emitter->SHL_d_CL(rRegResult);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_DIVIDE_SIGNED || imlInstruction->operation == PPCREC_IML_OP_DIVIDE_UNSIGNED )
	{
		x64Emit_mov_mem32_reg32(x64GenContext, X86_REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[0]), X86_REG_EAX);
		x64Emit_mov_mem32_reg32(x64GenContext, X86_REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[1]), X86_REG_EDX);
		// mov operand 2 to temp register
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand2);
		// mov operand1 to EAX
		x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, X86_REG_EAX, rRegOperand1);
		// sign or zero extend EAX to EDX:EAX based on division sign mode
		if( imlInstruction->operation == PPCREC_IML_OP_DIVIDE_SIGNED )
			x64Gen_cdq(x64GenContext);
		else
			x64Gen_xor_reg64Low32_reg64Low32(x64GenContext, X86_REG_EDX, X86_REG_EDX);
		// make sure we avoid division by zero
		x64Gen_test_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_EQUAL, 3);
		// divide
		if( imlInstruction->operation == PPCREC_IML_OP_DIVIDE_SIGNED )
			x64Gen_idiv_reg64Low32(x64GenContext, REG_RESV_TEMP);
		else
			x64Gen_div_reg64Low32(x64GenContext, REG_RESV_TEMP);
		// result of division is now stored in EAX, move it to result register
		if( rRegResult != X86_REG_EAX )
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, X86_REG_EAX);
		// restore EAX / EDX
		if( rRegResult != X86_REG_RAX )
			x64Emit_mov_reg64_mem32(x64GenContext, X86_REG_EAX, X86_REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[0]));
		if( rRegResult != X86_REG_RDX )
			x64Emit_mov_reg64_mem32(x64GenContext, X86_REG_EDX, X86_REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[1]));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED || imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_HIGH_UNSIGNED )
	{
		x64Emit_mov_mem32_reg32(x64GenContext, X86_REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[0]), X86_REG_EAX);
		x64Emit_mov_mem32_reg32(x64GenContext, X86_REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[1]), X86_REG_EDX);
		// mov operand 2 to temp register
		x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, rRegOperand2);
		// mov operand1 to EAX
		x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, X86_REG_EAX, rRegOperand1);
		if( imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED )
		{
			// zero extend EAX to EDX:EAX
			x64Gen_xor_reg64Low32_reg64Low32(x64GenContext, X86_REG_EDX, X86_REG_EDX);
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
		if( rRegResult != X86_REG_EDX )
			x64Gen_mov_reg64_reg64(x64GenContext, rRegResult, X86_REG_EDX);
		// restore EAX / EDX
		if( rRegResult != X86_REG_RAX )
			x64Emit_mov_reg64_mem32(x64GenContext, X86_REG_EAX, X86_REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[0]));
		if( rRegResult != X86_REG_RDX )
			x64Emit_mov_reg64_mem32(x64GenContext, X86_REG_EDX, X86_REG_RSP, (uint32)offsetof(PPCInterpreter_t, temporaryGPR[1]));
	}
	else
	{
		debug_printf("PPCRecompilerX64Gen_imlInstruction_r_r_r(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_r_r_r_carry(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = _reg32(imlInstruction->op_r_r_r_carry.regR);
	auto regA = _reg32(imlInstruction->op_r_r_r_carry.regA);
	auto regB = _reg32(imlInstruction->op_r_r_r_carry.regB);
	auto regCarry = _reg32(imlInstruction->op_r_r_r_carry.regCarry);
	bool carryRegIsShared = regCarry == regA || regCarry == regB;
	cemu_assert_debug(regCarry != regR); // two outputs sharing the same register is undefined behavior

	switch (imlInstruction->operation)
	{
	case PPCREC_IML_OP_ADD:
		if (regB == regR)
			std::swap(regB, regA);
		if (regR != regA)
			x64GenContext->emitter->MOV_dd(regR, regA);
		if(!carryRegIsShared)
			x64GenContext->emitter->XOR_dd(regCarry, regCarry);
		x64GenContext->emitter->ADD_dd(regR, regB);
		x64GenContext->emitter->SETcc_b(X86_CONDITION_B, _reg8_from_reg32(regCarry)); // below condition checks carry flag
		if(carryRegIsShared)
			x64GenContext->emitter->AND_di8(regCarry, 1); // clear upper bits
		break;
	case PPCREC_IML_OP_ADD_WITH_CARRY:
		// assumes that carry is already correctly initialized as 0 or 1
		if (regB == regR)
			std::swap(regB, regA);
		if (regR != regA)
			x64GenContext->emitter->MOV_dd(regR, regA);
		x64GenContext->emitter->BT_du8(regCarry, 0); // copy carry register to x86 carry flag
		x64GenContext->emitter->ADC_dd(regR, regB);
		x64GenContext->emitter->SETcc_b(X86_CONDITION_B, _reg8_from_reg32(regCarry));
		break;
	default:
		cemu_assert_unimplemented();
		return false;
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_compare(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = _reg8(imlInstruction->op_compare.regR);
	auto regA = _reg32(imlInstruction->op_compare.regA);
	auto regB = _reg32(imlInstruction->op_compare.regB);
	X86Cond cond = _x86Cond(imlInstruction->op_compare.cond);
	bool keepR = regR == regA || regR == regB;
	if(!keepR)
	{
		x64GenContext->emitter->XOR_dd(_reg32_from_reg8(regR), _reg32_from_reg8(regR)); // zero bytes unaffected by SETcc
		x64GenContext->emitter->CMP_dd(regA, regB);
		x64GenContext->emitter->SETcc_b(cond, regR);
	}
	else
	{
		x64GenContext->emitter->CMP_dd(regA, regB);
		x64GenContext->emitter->MOV_di32(_reg32_from_reg8(regR), 0);
		x64GenContext->emitter->SETcc_b(cond, regR);
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_compare_s32(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = _reg8(imlInstruction->op_compare_s32.regR);
	auto regA = _reg32(imlInstruction->op_compare_s32.regA);
	sint32 imm = imlInstruction->op_compare_s32.immS32;
	X86Cond cond = _x86Cond(imlInstruction->op_compare_s32.cond);
	bool keepR = regR == regA;
	if(!keepR)
	{
		x64GenContext->emitter->XOR_dd(_reg32_from_reg8(regR), _reg32_from_reg8(regR)); // zero bytes unaffected by SETcc
		x64GenContext->emitter->CMP_di32(regA, imm);
		x64GenContext->emitter->SETcc_b(cond, regR);
	}
	else
	{
		x64GenContext->emitter->CMP_di32(regA, imm);
		x64GenContext->emitter->MOV_di32(_reg32_from_reg8(regR), 0);
		x64GenContext->emitter->SETcc_b(cond, regR);
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_cjump2(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction, IMLSegment* imlSegment)
{
	auto regBool = _reg8(imlInstruction->op_conditional_jump.registerBool);
	bool mustBeTrue = imlInstruction->op_conditional_jump.mustBeTrue;
	x64GenContext->emitter->TEST_bb(regBool, regBool);
	PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, imlSegment->nextSegmentBranchTaken);
	x64GenContext->emitter->Jcc_j32(mustBeTrue ? X86_CONDITION_NZ : X86_CONDITION_Z, 0);
	return true;
}

void PPCRecompilerX64Gen_imlInstruction_x86_eflags_jcc(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction, IMLSegment* imlSegment)
{
	X86Cond cond = _x86Cond(imlInstruction->op_x86_eflags_jcc.cond, imlInstruction->op_x86_eflags_jcc.invertedCondition);
	PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, imlSegment->nextSegmentBranchTaken);
	x64GenContext->emitter->Jcc_j32(cond, 0);
}

bool PPCRecompilerX64Gen_imlInstruction_jump2(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction, IMLSegment* imlSegment)
{
	PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, imlSegment->nextSegmentBranchTaken);
	x64GenContext->emitter->JMP_j32(0);
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_r_r_s32(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = _reg32(imlInstruction->op_r_r_s32.regR);
	auto regA = _reg32(imlInstruction->op_r_r_s32.regA);
	uint32 immS32 = imlInstruction->op_r_r_s32.immS32;

	if( imlInstruction->operation == PPCREC_IML_OP_ADD )
	{
		uint32 immU32 = (uint32)imlInstruction->op_r_r_s32.immS32;
		if(regR != regA)
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, regR, regA);
		x64Gen_add_reg64Low32_imm32(x64GenContext, regR, (uint32)immU32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_SUB)
	{
		if (regR != regA)
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, regR, regA);
		x64Gen_sub_reg64Low32_imm32(x64GenContext, regR, immS32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_AND || 
		imlInstruction->operation == PPCREC_IML_OP_OR ||
		imlInstruction->operation == PPCREC_IML_OP_XOR)
	{
		if (regR != regA)
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, regR, regA);
		if (imlInstruction->operation == PPCREC_IML_OP_AND)
			x64Gen_and_reg64Low32_imm32(x64GenContext, regR, immS32);
		else if (imlInstruction->operation == PPCREC_IML_OP_OR)
			x64Gen_or_reg64Low32_imm32(x64GenContext, regR, immS32);
		else // XOR
			x64Gen_xor_reg64Low32_imm32(x64GenContext, regR, immS32);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_SIGNED )
	{
		// registerResult = registerOperand * immS32
		sint32 immS32 = (uint32)imlInstruction->op_r_r_s32.immS32;
		x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (sint64)immS32); // todo: Optimize
		if( regR != regA )
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, regR, regA);
		x64Gen_imul_reg64Low32_reg64Low32(x64GenContext, regR, REG_RESV_TEMP);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_LEFT_SHIFT ||
		imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_U ||
		imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_S)
	{
		if( regA != regR )
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, regR, regA);
		if (imlInstruction->operation == PPCREC_IML_OP_LEFT_SHIFT)
			x64Gen_shl_reg64Low32_imm8(x64GenContext, regR, imlInstruction->op_r_r_s32.immS32);
		else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_U)
			x64Gen_shr_reg64Low32_imm8(x64GenContext, regR, imlInstruction->op_r_r_s32.immS32);
		else // RIGHT_SHIFT_S
			x64Gen_sar_reg64Low32_imm8(x64GenContext, regR, imlInstruction->op_r_r_s32.immS32);
	}
	else
	{
		debug_printf("PPCRecompilerX64Gen_imlInstruction_r_r_s32(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_r_r_s32_carry(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = _reg32(imlInstruction->op_r_r_s32_carry.regR);
	auto regA = _reg32(imlInstruction->op_r_r_s32_carry.regA);
	sint32 immS32 = imlInstruction->op_r_r_s32_carry.immS32;
	auto regCarry = _reg32(imlInstruction->op_r_r_s32_carry.regCarry);
	cemu_assert_debug(regCarry != regR); // we dont allow two different outputs sharing the same register

	bool delayCarryInit = regCarry == regA;

	switch (imlInstruction->operation)
	{
	case PPCREC_IML_OP_ADD:
		if(!delayCarryInit)
			x64GenContext->emitter->XOR_dd(regCarry, regCarry);
		if (regR != regA)
			x64GenContext->emitter->MOV_dd(regR, regA);
		x64GenContext->emitter->ADD_di32(regR, immS32);
		if(delayCarryInit)
			x64GenContext->emitter->MOV_di32(regCarry, 0);
		x64GenContext->emitter->SETcc_b(X86_CONDITION_B, _reg8_from_reg32(regCarry));
		break;
	case PPCREC_IML_OP_ADD_WITH_CARRY:
		// assumes that carry is already correctly initialized as 0 or 1
		cemu_assert_debug(regCarry != regR);
		if (regR != regA)
			x64GenContext->emitter->MOV_dd(regR, regA);
		x64GenContext->emitter->BT_du8(regCarry, 0); // copy carry register to x86 carry flag
		x64GenContext->emitter->ADC_di32(regR, immS32);
		x64GenContext->emitter->SETcc_b(X86_CONDITION_B, _reg8_from_reg32(regCarry));
		break;
	default:
		cemu_assert_unimplemented();
		return false;
	}
	return true;
}

bool PPCRecompilerX64Gen_imlInstruction_conditionalJumpCycleCheck(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	// some tests (all performed on a i7-4790K)
	// 1) DEC [mem] + JNS has significantly worse performance than BT + JNC (probably due to additional memory write and direct dependency)
	// 2) CMP [mem], 0 + JG has about equal (or slightly worse) performance than BT + JNC

	// BT
	x64Gen_bt_mem8(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, remainingCycles), 31); // check if negative
	cemu_assert_debug(x64GenContext->currentSegment->GetBranchTaken());
	PPCRecompilerX64Gen_rememberRelocatableOffset(x64GenContext, x64GenContext->currentSegment->GetBranchTaken());
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_CARRY, 0);
	return true;
}

void PPCRecompilerX64Gen_imlInstruction_r_name(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	uint32 name = imlInstruction->op_r_name.name;
	if (imlInstruction->op_r_name.regR.GetBaseFormat() == IMLRegFormat::I64)
	{
		auto regR = _reg64(imlInstruction->op_r_name.regR);
		if (name >= PPCREC_NAME_R0 && name < PPCREC_NAME_R0 + 32)
		{
			x64Emit_mov_reg64_mem32(x64GenContext, regR, X86_REG_RSP, offsetof(PPCInterpreter_t, gpr) + sizeof(uint32) * (name - PPCREC_NAME_R0));
		}
		else if (name >= PPCREC_NAME_SPR0 && name < PPCREC_NAME_SPR0 + 999)
		{
			sint32 sprIndex = (name - PPCREC_NAME_SPR0);
			if (sprIndex == SPR_LR)
				x64Emit_mov_reg64_mem32(x64GenContext, regR, X86_REG_RSP, offsetof(PPCInterpreter_t, spr.LR));
			else if (sprIndex == SPR_CTR)
				x64Emit_mov_reg64_mem32(x64GenContext, regR, X86_REG_RSP, offsetof(PPCInterpreter_t, spr.CTR));
			else if (sprIndex == SPR_XER)
				x64Emit_mov_reg64_mem32(x64GenContext, regR, X86_REG_RSP, offsetof(PPCInterpreter_t, spr.XER));
			else if (sprIndex >= SPR_UGQR0 && sprIndex <= SPR_UGQR7)
			{
				sint32 memOffset = offsetof(PPCInterpreter_t, spr.UGQR) + sizeof(PPCInterpreter_t::spr.UGQR[0]) * (sprIndex - SPR_UGQR0);
				x64Emit_mov_reg64_mem32(x64GenContext, regR, X86_REG_RSP, memOffset);
			}
			else
				assert_dbg();
		}
		else if (name >= PPCREC_NAME_TEMPORARY && name < PPCREC_NAME_TEMPORARY + 4)
		{
			x64Emit_mov_reg64_mem32(x64GenContext, regR, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryGPR_reg) + sizeof(uint32) * (name - PPCREC_NAME_TEMPORARY));
		}
		else if (name == PPCREC_NAME_XER_CA)
		{
			x64Emit_movZX_reg64_mem8(x64GenContext, regR, X86_REG_RSP, offsetof(PPCInterpreter_t, xer_ca));
		}
		else if (name == PPCREC_NAME_XER_SO)
		{
			x64Emit_movZX_reg64_mem8(x64GenContext, regR, X86_REG_RSP, offsetof(PPCInterpreter_t, xer_so));
		}
		else if (name >= PPCREC_NAME_CR && name <= PPCREC_NAME_CR_LAST)
		{
			x64Emit_movZX_reg64_mem8(x64GenContext, regR, X86_REG_RSP, offsetof(PPCInterpreter_t, cr) + (name - PPCREC_NAME_CR));
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_EA)
		{
			x64Emit_mov_reg64_mem32(x64GenContext, regR, X86_REG_RSP, offsetof(PPCInterpreter_t, reservedMemAddr));
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_VAL)
		{
			x64Emit_mov_reg64_mem32(x64GenContext, regR, X86_REG_RSP, offsetof(PPCInterpreter_t, reservedMemValue));
		}
		else
			assert_dbg();
	}
	else if (imlInstruction->op_r_name.regR.GetBaseFormat() == IMLRegFormat::F64)
	{
		auto regR = _regF64(imlInstruction->op_r_name.regR);
		if (name >= PPCREC_NAME_FPR0 && name < (PPCREC_NAME_FPR0 + 32))
		{
			x64Gen_movupd_xmmReg_memReg128(x64GenContext, regR, X86_REG_ESP, offsetof(PPCInterpreter_t, fpr) + sizeof(FPR_t) * (name - PPCREC_NAME_FPR0));
		}
		else if (name >= PPCREC_NAME_TEMPORARY_FPR0 || name < (PPCREC_NAME_TEMPORARY_FPR0 + 8))
		{
			x64Gen_movupd_xmmReg_memReg128(x64GenContext, regR, X86_REG_ESP, offsetof(PPCInterpreter_t, temporaryFPR) + sizeof(FPR_t) * (name - PPCREC_NAME_TEMPORARY_FPR0));
		}
		else
		{
			cemu_assert_debug(false);
		}
	}
	else
		DEBUG_BREAK;

}

void PPCRecompilerX64Gen_imlInstruction_name_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	uint32 name = imlInstruction->op_r_name.name;
	
	if (imlInstruction->op_r_name.regR.GetBaseFormat() == IMLRegFormat::I64)
	{
		auto regR = _reg64(imlInstruction->op_r_name.regR);
		if (name >= PPCREC_NAME_R0 && name < PPCREC_NAME_R0 + 32)
		{
			x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, gpr) + sizeof(uint32) * (name - PPCREC_NAME_R0), regR);
		}
		else if (name >= PPCREC_NAME_SPR0 && name < PPCREC_NAME_SPR0 + 999)
		{
			uint32 sprIndex = (name - PPCREC_NAME_SPR0);
			if (sprIndex == SPR_LR)
				x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, spr.LR), regR);
			else if (sprIndex == SPR_CTR)
				x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, spr.CTR), regR);
			else if (sprIndex == SPR_XER)
				x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, spr.XER), regR);
			else if (sprIndex >= SPR_UGQR0 && sprIndex <= SPR_UGQR7)
			{
				sint32 memOffset = offsetof(PPCInterpreter_t, spr.UGQR) + sizeof(PPCInterpreter_t::spr.UGQR[0]) * (sprIndex - SPR_UGQR0);
				x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, memOffset, regR);
			}
			else
				assert_dbg();
		}
		else if (name >= PPCREC_NAME_TEMPORARY && name < PPCREC_NAME_TEMPORARY + 4)
		{
			x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryGPR_reg) + sizeof(uint32) * (name - PPCREC_NAME_TEMPORARY), regR);
		}
		else if (name == PPCREC_NAME_XER_CA)
		{
			x64GenContext->emitter->MOV_bb_l(X86_REG_RSP, offsetof(PPCInterpreter_t, xer_ca), X86_REG_NONE, 0, _reg8_from_reg64(regR));
		}
		else if (name == PPCREC_NAME_XER_SO)
		{
			x64GenContext->emitter->MOV_bb_l(X86_REG_RSP, offsetof(PPCInterpreter_t, xer_so), X86_REG_NONE, 0, _reg8_from_reg64(regR));
		}
		else if (name >= PPCREC_NAME_CR && name <= PPCREC_NAME_CR_LAST)
		{
			x64GenContext->emitter->MOV_bb_l(X86_REG_RSP, offsetof(PPCInterpreter_t, cr) + (name - PPCREC_NAME_CR), X86_REG_NONE, 0, _reg8_from_reg64(regR));
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_EA)
		{
			x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, reservedMemAddr), regR);
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_VAL)
		{
			x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, reservedMemValue), regR);
		}
		else
			assert_dbg();
	}
	else if (imlInstruction->op_r_name.regR.GetBaseFormat() == IMLRegFormat::F64)
	{
		auto regR = _regF64(imlInstruction->op_r_name.regR);
		uint32 name = imlInstruction->op_r_name.name;
		if (name >= PPCREC_NAME_FPR0 && name < (PPCREC_NAME_FPR0 + 32))
		{
			x64Gen_movupd_memReg128_xmmReg(x64GenContext, regR, X86_REG_ESP, offsetof(PPCInterpreter_t, fpr) + sizeof(FPR_t) * (name - PPCREC_NAME_FPR0));
		}
		else if (name >= PPCREC_NAME_TEMPORARY_FPR0 && name < (PPCREC_NAME_TEMPORARY_FPR0 + 8))
		{
			x64Gen_movupd_memReg128_xmmReg(x64GenContext, regR, X86_REG_ESP, offsetof(PPCInterpreter_t, temporaryFPR) + sizeof(FPR_t) * (name - PPCREC_NAME_TEMPORARY_FPR0));
		}
		else
		{
			cemu_assert_debug(false);
		}
	}
	else
		DEBUG_BREAK;


}

//void PPCRecompilerX64Gen_imlInstruction_fpr_r_name(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
//{
//	uint32 name = imlInstruction->op_r_name.name;
//	uint32 fprReg = _regF64(imlInstruction->op_r_name.regR);
//	if (name >= PPCREC_NAME_FPR0 && name < (PPCREC_NAME_FPR0 + 32))
//	{
//		x64Gen_movupd_xmmReg_memReg128(x64GenContext, fprReg, X86_REG_ESP, offsetof(PPCInterpreter_t, fpr) + sizeof(FPR_t) * (name - PPCREC_NAME_FPR0));
//	}
//	else if (name >= PPCREC_NAME_TEMPORARY_FPR0 || name < (PPCREC_NAME_TEMPORARY_FPR0 + 8))
//	{
//		x64Gen_movupd_xmmReg_memReg128(x64GenContext, fprReg, X86_REG_ESP, offsetof(PPCInterpreter_t, temporaryFPR) + sizeof(FPR_t) * (name - PPCREC_NAME_TEMPORARY_FPR0));
//	}
//	else
//	{
//		cemu_assert_debug(false);
//	}
//}
//
//void PPCRecompilerX64Gen_imlInstruction_fpr_name_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
//{
//	uint32 name = imlInstruction->op_r_name.name;
//	uint32 fprReg = _regF64(imlInstruction->op_r_name.regR);
//	if (name >= PPCREC_NAME_FPR0 && name < (PPCREC_NAME_FPR0 + 32))
//	{
//		x64Gen_movupd_memReg128_xmmReg(x64GenContext, fprReg, X86_REG_ESP, offsetof(PPCInterpreter_t, fpr) + sizeof(FPR_t) * (name - PPCREC_NAME_FPR0));
//	}
//	else if (name >= PPCREC_NAME_TEMPORARY_FPR0 && name < (PPCREC_NAME_TEMPORARY_FPR0 + 8))
//	{
//		x64Gen_movupd_memReg128_xmmReg(x64GenContext, fprReg, X86_REG_ESP, offsetof(PPCInterpreter_t, temporaryFPR) + sizeof(FPR_t) * (name - PPCREC_NAME_TEMPORARY_FPR0));
//	}
//	else
//	{
//		cemu_assert_debug(false);
//	}
//}

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

bool PPCRecompiler_generateX64Code(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext)
{
	x64GenContext_t x64GenContext{};

	// generate iml instruction code
	bool codeGenerationFailed = false;
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		x64GenContext.currentSegment = segIt;
		segIt->x64Offset = x64GenContext.emitter->GetWriteIndex();
		for(size_t i=0; i<segIt->imlList.size(); i++)
		{
			IMLInstruction* imlInstruction = segIt->imlList.data() + i;

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
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_S32)
			{
				if (PPCRecompilerX64Gen_imlInstruction_r_s32(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false)
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
			{
				if (PPCRecompilerX64Gen_imlInstruction_conditional_r_s32(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false)
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_S32)
			{
				if (PPCRecompilerX64Gen_imlInstruction_r_r_s32(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false)
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_S32_CARRY)
			{
				if (PPCRecompilerX64Gen_imlInstruction_r_r_s32_carry(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false)
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_R)
			{
				if (PPCRecompilerX64Gen_imlInstruction_r_r_r(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false)
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_R_CARRY)
			{
				if (PPCRecompilerX64Gen_imlInstruction_r_r_r_carry(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction) == false)
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_COMPARE)
			{
				PPCRecompilerX64Gen_imlInstruction_compare(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_COMPARE_S32)
			{
				PPCRecompilerX64Gen_imlInstruction_compare_s32(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_JUMP)
			{
				if (PPCRecompilerX64Gen_imlInstruction_cjump2(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction, segIt) == false)
					codeGenerationFailed = true;
			}
			else if(imlInstruction->type == PPCREC_IML_TYPE_X86_EFLAGS_JCC)
			{
				PPCRecompilerX64Gen_imlInstruction_x86_eflags_jcc(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction, segIt);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_JUMP)
			{
				if (PPCRecompilerX64Gen_imlInstruction_jump2(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction, segIt) == false)
					codeGenerationFailed = true;
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
			else if (imlInstruction->type == PPCREC_IML_TYPE_ATOMIC_CMP_STORE)
			{
				PPCRecompilerX64Gen_imlInstruction_atomic_cmp_store(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_CALL_IMM)
			{
				PPCRecompilerX64Gen_imlInstruction_call_imm(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
			}
			else if( imlInstruction->type == PPCREC_IML_TYPE_NO_OP )
			{
				// no op
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
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_COMPARE)
			{
				PPCRecompilerX64Gen_imlInstruction_fpr_compare(PPCRecFunction, ppcImlGenContext, &x64GenContext, imlInstruction);
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
		return false;
	}
	// allocate executable memory
	uint8* executableMemory = PPCRecompilerX86_allocateExecutableMemory(x64GenContext.emitter->GetBuffer().size_bytes());
	size_t baseAddress = (size_t)executableMemory;
	// fix relocs
	for(auto& relocIt : x64GenContext.relocateOffsetTable2)
	{
		// search for segment that starts with this offset
		uint32 ppcOffset = (uint32)(size_t)relocIt.extraInfo;
		uint32 x64Offset = 0xFFFFFFFF;

		IMLSegment* destSegment = (IMLSegment*)relocIt.extraInfo;
		x64Offset = destSegment->x64Offset;

		uint32 relocBase = relocIt.offset;
		uint8* relocInstruction = x64GenContext.emitter->GetBufferPtr()+relocBase;
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

	// copy code to executable memory
	std::span<uint8> codeBuffer = x64GenContext.emitter->GetBuffer();
	memcpy(executableMemory, codeBuffer.data(), codeBuffer.size_bytes());
	// set code
	PPCRecFunction->x86Code = executableMemory;
	PPCRecFunction->x86Size = codeBuffer.size_bytes();
	return true;
}

void PPCRecompilerX64Gen_generateEnterRecompilerCode()
{
	x64GenContext_t x64GenContext{};

	// start of recompiler entry function
	x64Gen_push_reg64(&x64GenContext, X86_REG_RAX);
	x64Gen_push_reg64(&x64GenContext, X86_REG_RCX);
	x64Gen_push_reg64(&x64GenContext, X86_REG_RDX);
	x64Gen_push_reg64(&x64GenContext, X86_REG_RBX);
	x64Gen_push_reg64(&x64GenContext, X86_REG_RBP);
	x64Gen_push_reg64(&x64GenContext, X86_REG_RDI);
	x64Gen_push_reg64(&x64GenContext, X86_REG_RSI);
	x64Gen_push_reg64(&x64GenContext, X86_REG_R8);
	x64Gen_push_reg64(&x64GenContext, X86_REG_R9);
	x64Gen_push_reg64(&x64GenContext, X86_REG_R10);
	x64Gen_push_reg64(&x64GenContext, X86_REG_R11);
	x64Gen_push_reg64(&x64GenContext, X86_REG_R12);
	x64Gen_push_reg64(&x64GenContext, X86_REG_R13);
	x64Gen_push_reg64(&x64GenContext, X86_REG_R14);
	x64Gen_push_reg64(&x64GenContext, X86_REG_R15);

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
	uint32 jmpPatchOffset = x64GenContext.emitter->GetWriteIndex();
	x64Gen_writeU8(&x64GenContext, 0); // skip the distance until after the JMP
	x64Emit_mov_mem64_reg64(&x64GenContext, X86_REG_RDX, offsetof(PPCInterpreter_t, rspTemp), X86_REG_RSP);


	// MOV RSP, RDX (ppc interpreter instance)
	x64Gen_mov_reg64_reg64(&x64GenContext, X86_REG_RSP, X86_REG_RDX);
	// MOV R15, ppcRecompilerInstanceData
	x64Gen_mov_reg64_imm64(&x64GenContext, X86_REG_R15, (uint64)ppcRecompilerInstanceData);
	// MOV R13, memory_base
	x64Gen_mov_reg64_imm64(&x64GenContext, X86_REG_R13, (uint64)memory_base);

	//JMP recFunc
	x64Gen_jmp_reg64(&x64GenContext, X86_REG_RCX); // call argument 1

	x64GenContext.emitter->GetBuffer()[jmpPatchOffset] = (x64GenContext.emitter->GetWriteIndex() -(jmpPatchOffset-4));

	//recompilerExit1:
	x64Gen_pop_reg64(&x64GenContext, X86_REG_R15);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_R14);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_R13);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_R12);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_R11);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_R10);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_R9);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_R8);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_RSI);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_RDI);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_RBP);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_RBX);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_RDX);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_RCX);
	x64Gen_pop_reg64(&x64GenContext, X86_REG_RAX);
	// RET
	x64Gen_ret(&x64GenContext);

	uint8* executableMemory = PPCRecompilerX86_allocateExecutableMemory(x64GenContext.emitter->GetBuffer().size_bytes());
	// copy code to executable memory
	memcpy(executableMemory, x64GenContext.emitter->GetBuffer().data(), x64GenContext.emitter->GetBuffer().size_bytes());
	PPCRecompiler_enterRecompilerCode = (void ATTR_MS_ABI (*)(uint64,uint64))executableMemory;
}


void* PPCRecompilerX64Gen_generateLeaveRecompilerCode()
{
	x64GenContext_t x64GenContext{};

	// update instruction pointer
	// LR is in EDX
	x64Emit_mov_mem32_reg32(&x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, instructionPointer), X86_REG_EDX);

	// MOV RSP, [ppcRecompilerX64_rspTemp]
	x64Emit_mov_reg64_mem64(&x64GenContext, X86_REG_RSP, REG_RESV_HCPU, offsetof(PPCInterpreter_t, rspTemp));

	// RET
	x64Gen_ret(&x64GenContext);

	uint8* executableMemory = PPCRecompilerX86_allocateExecutableMemory(x64GenContext.emitter->GetBuffer().size_bytes());
	// copy code to executable memory
	memcpy(executableMemory, x64GenContext.emitter->GetBuffer().data(), x64GenContext.emitter->GetBuffer().size_bytes());
	return executableMemory;
}

void PPCRecompilerX64Gen_generateRecompilerInterfaceFunctions()
{
	PPCRecompilerX64Gen_generateEnterRecompilerCode();
	PPCRecompiler_leaveRecompilerCode_unvisited = (void ATTR_MS_ABI (*)())PPCRecompilerX64Gen_generateLeaveRecompilerCode();
	PPCRecompiler_leaveRecompilerCode_visited = (void ATTR_MS_ABI (*)())PPCRecompilerX64Gen_generateLeaveRecompilerCode();
	cemu_assert_debug(PPCRecompiler_leaveRecompilerCode_unvisited != PPCRecompiler_leaveRecompilerCode_visited);
}

