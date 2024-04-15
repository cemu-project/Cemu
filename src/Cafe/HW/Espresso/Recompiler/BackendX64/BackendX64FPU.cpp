#include "../PPCRecompiler.h"
#include "../IML/IML.h"
#include "BackendX64.h"
#include "Common/cpu_features.h"

#include "asm/x64util.h" // for recompiler_fres / frsqrte

uint32 _regF64(IMLReg physReg);

uint32 _regI32(IMLReg r)
{
	cemu_assert_debug(r.GetRegFormat() == IMLRegFormat::I32);
	return (uint32)r.GetRegID();
}

static x86Assembler64::GPR32 _reg32(sint8 physRegId)
{
	return (x86Assembler64::GPR32)physRegId;
}

static x86Assembler64::GPR8_REX _reg8(IMLReg r)
{
	cemu_assert_debug(r.GetRegFormat() == IMLRegFormat::I32); // currently bool regs are implemented as 32bit registers
	return (x86Assembler64::GPR8_REX)r.GetRegID();
}

static x86Assembler64::GPR32 _reg32_from_reg8(x86Assembler64::GPR8_REX regId)
{
	return (x86Assembler64::GPR32)regId;
}

static x86Assembler64::GPR8_REX _reg8_from_reg32(x86Assembler64::GPR32 regId)
{
	return (x86Assembler64::GPR8_REX)regId;
}

void PPCRecompilerX64Gen_imlInstr_gqr_generateScaleCode(ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, sint32 registerXMM, bool isLoad, bool scalePS1, IMLReg registerGQR)
{
	// load GQR
	x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, _regI32(registerGQR));
	// extract scale field and multiply by 16 to get array offset
	x64Gen_shr_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, (isLoad?16:0)+8-4);
	x64Gen_and_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, (0x3F<<4));
	// multiply xmm by scale
	x64Gen_add_reg64_reg64(x64GenContext, REG_RESV_TEMP, REG_RESV_RECDATA);
	if (isLoad)
	{
		if(scalePS1)
			x64Gen_mulpd_xmmReg_memReg128(x64GenContext, registerXMM, REG_RESV_TEMP, offsetof(PPCRecompilerInstanceData_t, _psq_ld_scale_ps0_ps1));
		else
			x64Gen_mulpd_xmmReg_memReg128(x64GenContext, registerXMM, REG_RESV_TEMP, offsetof(PPCRecompilerInstanceData_t, _psq_ld_scale_ps0_1));
	}
	else
	{
		if (scalePS1)
			x64Gen_mulpd_xmmReg_memReg128(x64GenContext, registerXMM, REG_RESV_TEMP, offsetof(PPCRecompilerInstanceData_t, _psq_st_scale_ps0_ps1));
		else
			x64Gen_mulpd_xmmReg_memReg128(x64GenContext, registerXMM, REG_RESV_TEMP, offsetof(PPCRecompilerInstanceData_t, _psq_st_scale_ps0_1));
	}
}

// generate code for PSQ load for a particular type
// if scaleGQR is -1 then a scale of 1.0 is assumed (no scale)
void PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, uint8 mode, sint32 registerXMM, sint32 memReg, sint32 memRegEx, sint32 memImmS32, bool indexed, IMLReg registerGQR = IMLREG_INVALID)
{
	if (mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1)
	{
		if (indexed)
		{
			assert_dbg();
		}
		// optimized code for ps float load
		x64Emit_mov_reg64_mem64(x64GenContext, REG_RESV_TEMP, X86_REG_R13, memReg, memImmS32);
		x64GenContext->emitter->BSWAP_q(REG_RESV_TEMP);
		x64Gen_rol_reg64_imm8(x64GenContext, REG_RESV_TEMP, 32); // swap upper and lower DWORD
		x64Gen_movq_xmmReg_reg64(x64GenContext, registerXMM, REG_RESV_TEMP);
		x64Gen_cvtps2pd_xmmReg_xmmReg(x64GenContext, registerXMM, registerXMM);
		// note: floats are not scaled
	}
	else if (mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0)
	{
		if (indexed)
		{
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, memRegEx);
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, memReg);
			if (g_CPUFeatures.x86.movbe)
			{
				x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, REG_RESV_TEMP, memImmS32);
			}
			else
			{
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, REG_RESV_TEMP, memImmS32);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
			}
		}
		else
		{
			if (g_CPUFeatures.x86.movbe)
			{
				x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, memReg, memImmS32);
			}
			else
			{
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, memReg, memImmS32);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
			}
		}
		if (g_CPUFeatures.x86.avx)
		{
			x64Gen_movd_xmmReg_reg64Low32(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_TEMP);
		}
		else
		{
			x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR), REG_RESV_TEMP);
			x64Gen_movddup_xmmReg_memReg64(x64GenContext, REG_RESV_FPR_TEMP, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
		}
		x64Gen_cvtss2sd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_FPR_TEMP);
		// load constant 1.0 into lower half and upper half of temp register
		x64Gen_movddup_xmmReg_memReg64(x64GenContext, registerXMM, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_constDouble1_1));
		// overwrite lower half with single from memory
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, registerXMM, REG_RESV_FPR_TEMP);
		// note: floats are not scaled
	}
	else
	{
		sint32 readSize;
		bool isSigned = false;
		if (mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0 ||
			mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1)
		{
			readSize = 16;
			isSigned = true;
		}
		else if (mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0 ||
			mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1)
		{
			readSize = 16;
			isSigned = false;
		}
		else if (mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0 ||
			mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1)
		{
			readSize = 8;
			isSigned = true;
		}
		else if (mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0 ||
			mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1)
		{
			readSize = 8;
			isSigned = false;
		}
		else
			assert_dbg();

		bool loadPS1 = (mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1 ||
						mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1 ||
						mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1 ||
						mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1);
		for (sint32 wordIndex = 0; wordIndex < 2; wordIndex++)
		{
			if (indexed)
			{
				assert_dbg();
			}
			// read from memory
			if (wordIndex == 1 && loadPS1 == false)
			{
				// store constant 1
				x64Gen_mov_mem32Reg64_imm32(x64GenContext, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryGPR) + sizeof(uint32) * 1, 1);
			}
			else
			{
				uint32 memOffset = memImmS32 + wordIndex * (readSize / 8);
				if (readSize == 16)
				{
					// half word
					x64Gen_movZeroExtend_reg64Low16_mem16Reg64PlusReg64(x64GenContext, REG_RESV_TEMP, X86_REG_R13, memReg, memOffset);
					x64Gen_rol_reg64Low16_imm8(x64GenContext, REG_RESV_TEMP, 8); // endian swap
					if (isSigned)
						x64Gen_movSignExtend_reg64Low32_reg64Low16(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
					else
						x64Gen_movZeroExtend_reg64Low32_reg64Low16(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
				}
				else if (readSize == 8)
				{
					// byte
					x64Emit_mov_reg64b_mem8(x64GenContext, REG_RESV_TEMP, X86_REG_R13, memReg, memOffset);
					if (isSigned)
						x64Gen_movSignExtend_reg64Low32_reg64Low8(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
					else
						x64Gen_movZeroExtend_reg64Low32_reg64Low8(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
				}
				// store
				x64Emit_mov_mem32_reg32(x64GenContext, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryGPR) + sizeof(uint32) * wordIndex, REG_RESV_TEMP);
			}
		}
		// convert the two integers to doubles
		x64Gen_cvtpi2pd_xmmReg_mem64Reg64(x64GenContext, registerXMM, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryGPR));
		// scale
		if (registerGQR.IsValid())
			PPCRecompilerX64Gen_imlInstr_gqr_generateScaleCode(ppcImlGenContext, x64GenContext, registerXMM, true, loadPS1, registerGQR);
	}
}

void PPCRecompilerX64Gen_imlInstr_psq_load_generic(ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, uint8 mode, sint32 registerXMM, sint32 memReg, sint32 memRegEx, sint32 memImmS32, bool indexed, IMLReg registerGQR)
{
	bool loadPS1 = (mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1);
	// load GQR
	x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, _regI32(registerGQR));
	// extract load type field
	x64Gen_shr_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, 16);
	x64Gen_and_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 7);
	// jump cases
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 4); // type 4 -> u8
	sint32 jumpOffset_caseU8 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 5); // type 5 -> u16
	sint32 jumpOffset_caseU16 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 6); // type 4 -> s8
	sint32 jumpOffset_caseS8 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 7); // type 5 -> s16
	sint32 jumpOffset_caseS16 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	// default case -> float

	// generate cases
	uint32 jumpOffset_endOfFloat;
	uint32 jumpOffset_endOfU8;
	uint32 jumpOffset_endOfU16;
	uint32 jumpOffset_endOfS8;

	PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext, x64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfFloat = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseU16, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext, x64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_U16_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfU8 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseS16, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext, x64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_S16_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfU16 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseU8, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext, x64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_U8_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfS8 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseS8, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext, x64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_S8_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfFloat, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfU8, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfU16, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfS8, x64GenContext->emitter->GetWriteIndex());
}

// load from memory
bool PPCRecompilerX64Gen_imlInstruction_fpr_load(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction, bool indexed)
{
	sint32 realRegisterXMM =  _regF64(imlInstruction->op_storeLoad.registerData);
	sint32 realRegisterMem = _regI32(imlInstruction->op_storeLoad.registerMem);
	sint32 realRegisterMem2 = PPC_REC_INVALID_REGISTER;
	if( indexed )
		realRegisterMem2 = _regI32(imlInstruction->op_storeLoad.registerMem2);
	uint8 mode = imlInstruction->op_storeLoad.mode;

	if( mode == PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1 )
	{
		// load byte swapped single into temporary FPR
		if( indexed )
		{
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem2);
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem);
			if(g_CPUFeatures.x86.movbe)
				x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32);
			else
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32);
		}
		else
		{
			if(g_CPUFeatures.x86.movbe)
				x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32);
			else
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32);
		}
		if(g_CPUFeatures.x86.movbe == false )
			x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		x64Gen_movd_xmmReg_reg64Low32(x64GenContext, realRegisterXMM, REG_RESV_TEMP);

		if (imlInstruction->op_storeLoad.flags2.notExpanded)
		{
			// leave value as single
		}
		else
		{
			x64Gen_cvtss2sd_xmmReg_xmmReg(x64GenContext, realRegisterXMM, realRegisterXMM);
			x64Gen_movddup_xmmReg_xmmReg(x64GenContext, realRegisterXMM, realRegisterXMM);
		}		
	}
	else if( mode == PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0 )
	{
		if( g_CPUFeatures.x86.avx )
		{
			if( indexed )
			{
				// calculate offset
				x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem);
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem2);
				// load value
				x64Emit_mov_reg64_mem64(x64GenContext, REG_RESV_TEMP, X86_REG_R13, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32+0);
				x64GenContext->emitter->BSWAP_q(REG_RESV_TEMP);
				x64Gen_movq_xmmReg_reg64(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_TEMP);
				x64Gen_movsd_xmmReg_xmmReg(x64GenContext, realRegisterXMM, REG_RESV_FPR_TEMP);
			}
			else
			{
				x64Emit_mov_reg64_mem64(x64GenContext, REG_RESV_TEMP, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32+0);
				x64GenContext->emitter->BSWAP_q(REG_RESV_TEMP);
				x64Gen_movq_xmmReg_reg64(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_TEMP);
				x64Gen_movsd_xmmReg_xmmReg(x64GenContext, realRegisterXMM, REG_RESV_FPR_TEMP);
			}
		}
		else
		{
			if( indexed )
			{
				// calculate offset
				x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem);
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem2);
				// load double low part to temporaryFPR
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, X86_REG_R13, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32+0);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+4, REG_RESV_TEMP);
				// calculate offset again
				x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem);
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem2);
				// load double high part to temporaryFPR
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, X86_REG_R13, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32+4);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+0, REG_RESV_TEMP);
				// load double from temporaryFPR
				x64Gen_movlpd_xmmReg_memReg64(x64GenContext, realRegisterXMM, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
			}
			else
			{
				// load double low part to temporaryFPR
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32+0);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+4, REG_RESV_TEMP);
				// load double high part to temporaryFPR
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32+4);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+0, REG_RESV_TEMP);
				// load double from temporaryFPR
				x64Gen_movlpd_xmmReg_memReg64(x64GenContext, realRegisterXMM, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
			}
		}
	}
	else if (mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0 ||
 			 mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0 ||
 			 mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1 )
	{
		PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext, x64GenContext, mode, realRegisterXMM, realRegisterMem, realRegisterMem2, imlInstruction->op_storeLoad.immS32, indexed);
	}
	else if (mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1 ||
		   	 mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0)
	{
		PPCRecompilerX64Gen_imlInstr_psq_load_generic(ppcImlGenContext, x64GenContext, mode, realRegisterXMM, realRegisterMem, realRegisterMem2, imlInstruction->op_storeLoad.immS32, indexed, imlInstruction->op_storeLoad.registerGQR);
	}
	else
	{
		return false;
	}
	return true;
}

void PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, uint8 mode, sint32 registerXMM, sint32 memReg, sint32 memRegEx, sint32 memImmS32, bool indexed, IMLReg registerGQR = IMLREG_INVALID)
{
	bool storePS1 = (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1);
	bool isFloat = mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0 || mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1;
	if (registerGQR.IsValid())
	{
		// move to temporary xmm and update registerXMM
		x64Gen_movaps_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, registerXMM);
		registerXMM = REG_RESV_FPR_TEMP;
		// apply scale
		if(isFloat == false)
			PPCRecompilerX64Gen_imlInstr_gqr_generateScaleCode(ppcImlGenContext, x64GenContext, registerXMM, false, storePS1, registerGQR);
	}
	if (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0)
	{
		x64Gen_cvtsd2ss_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, registerXMM);
		x64Gen_movd_reg64Low32_xmmReg(x64GenContext, REG_RESV_TEMP, REG_RESV_FPR_TEMP);
		if (g_CPUFeatures.x86.movbe == false)
			x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		if (indexed)
		{
			cemu_assert_debug(memReg != memRegEx);
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, memReg, memRegEx);
		}
		if (g_CPUFeatures.x86.movbe)
			x64Gen_movBETruncate_mem32Reg64PlusReg64_reg64(x64GenContext, X86_REG_R13, memReg, memImmS32, REG_RESV_TEMP);
		else
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, X86_REG_R13, memReg, memImmS32, REG_RESV_TEMP);
		if (indexed)
		{
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, memReg, memRegEx);
		}
		return;
	}
	else if (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1)
	{
		if (indexed)
			assert_dbg(); // todo
		x64Gen_cvtpd2ps_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, registerXMM);
		x64Gen_movq_reg64_xmmReg(x64GenContext, REG_RESV_TEMP, REG_RESV_FPR_TEMP);
		x64Gen_rol_reg64_imm8(x64GenContext, REG_RESV_TEMP, 32); // swap upper and lower DWORD
		x64GenContext->emitter->BSWAP_q(REG_RESV_TEMP);
		x64Gen_mov_mem64Reg64PlusReg64_reg64(x64GenContext, REG_RESV_TEMP, X86_REG_R13, memReg, memImmS32);
		return;
	}
	// store as integer
	// get limit from mode
	sint32 clampMin, clampMax;
	sint32 bitWriteSize;
	if (mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 )
	{
		clampMin = -128;
		clampMax = 127;
		bitWriteSize = 8;
	}
	else if (mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 )
	{
		clampMin = 0;
		clampMax = 255;
		bitWriteSize = 8;
	}
	else if (mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1 )
	{
		clampMin = 0;
		clampMax = 0xFFFF;
		bitWriteSize = 16;
	}
	else if (mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1 )
	{
		clampMin = -32768;
		clampMax = 32767;
		bitWriteSize = 16;
	}
	else
	{
		cemu_assert(false);
	}
	for (sint32 valueIndex = 0; valueIndex < (storePS1?2:1); valueIndex++)
	{
		// todo - multiply by GQR scale
		if (valueIndex == 0)
		{
			// convert low half (PS0) to integer
			x64Gen_cvttsd2si_reg64Low_xmmReg(x64GenContext, REG_RESV_TEMP, registerXMM);
		}
		else
		{
			// load top half (PS1) into bottom half of temporary register
			x64Gen_movhlps_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, registerXMM);
			// convert low half to integer
			x64Gen_cvttsd2si_reg64Low_xmmReg(x64GenContext, REG_RESV_TEMP, REG_RESV_FPR_TEMP);
		}
		// max(i, -clampMin)
		x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, clampMin);
		sint32 jumpInstructionOffset1 = x64GenContext->emitter->GetWriteIndex();
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_SIGNED_GREATER_EQUAL, 0);
		x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, clampMin);
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1, x64GenContext->emitter->GetWriteIndex());
		// min(i, clampMax)
		x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, clampMax);
		sint32 jumpInstructionOffset2 = x64GenContext->emitter->GetWriteIndex();
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_SIGNED_LESS_EQUAL, 0);
		x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, clampMax);
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2, x64GenContext->emitter->GetWriteIndex());
		// endian swap
		if( bitWriteSize == 16)
			x64Gen_rol_reg64Low16_imm8(x64GenContext, REG_RESV_TEMP, 8);
		// write to memory
		if (indexed)
			assert_dbg(); // unsupported
		sint32 memOffset = memImmS32 + valueIndex * (bitWriteSize/8);
		if (bitWriteSize == 8)
			x64Gen_movTruncate_mem8Reg64PlusReg64_reg64(x64GenContext, REG_RESV_MEMBASE, memReg, memOffset, REG_RESV_TEMP);
		else if (bitWriteSize == 16)
			x64Gen_movTruncate_mem16Reg64PlusReg64_reg64(x64GenContext, REG_RESV_MEMBASE, memReg, memOffset, REG_RESV_TEMP);
	}
}

void PPCRecompilerX64Gen_imlInstr_psq_store_generic(ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, uint8 mode, sint32 registerXMM, sint32 memReg, sint32 memRegEx, sint32 memImmS32, bool indexed, IMLReg registerGQR)
{
	bool storePS1 = (mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1);
	// load GQR
	x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, _regI32(registerGQR));
	// extract store type field
	x64Gen_and_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 7);
	// jump cases
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 4); // type 4 -> u8
	sint32 jumpOffset_caseU8 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 5); // type 5 -> u16
	sint32 jumpOffset_caseU16 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 6); // type 4 -> s8
	sint32 jumpOffset_caseS8 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 7); // type 5 -> s16
	sint32 jumpOffset_caseS16 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	// default case -> float

	// generate cases
	uint32 jumpOffset_endOfFloat;
	uint32 jumpOffset_endOfU8;
	uint32 jumpOffset_endOfU16;
	uint32 jumpOffset_endOfS8;

	PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext, x64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfFloat = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseU16, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext, x64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_U16_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfU8 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseS16, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext, x64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_S16_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfU16 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseU8, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext, x64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_U8_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfS8 = x64GenContext->emitter->GetWriteIndex();
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseS8, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext, x64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_S8_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfFloat, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfU8, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfU16, x64GenContext->emitter->GetWriteIndex());
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfS8, x64GenContext->emitter->GetWriteIndex());
}

// store to memory
bool PPCRecompilerX64Gen_imlInstruction_fpr_store(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction, bool indexed)
{
	sint32 realRegisterXMM = _regF64(imlInstruction->op_storeLoad.registerData);
	sint32 realRegisterMem = _regI32(imlInstruction->op_storeLoad.registerMem);
	sint32 realRegisterMem2 = PPC_REC_INVALID_REGISTER;
	if( indexed )
		realRegisterMem2 = _regI32(imlInstruction->op_storeLoad.registerMem2);
	uint8 mode = imlInstruction->op_storeLoad.mode;
	if( mode == PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0 )
	{
		if (imlInstruction->op_storeLoad.flags2.notExpanded)
		{
			// value is already in single format
			x64Gen_movd_reg64Low32_xmmReg(x64GenContext, REG_RESV_TEMP, realRegisterXMM);
		}
		else
		{
			x64Gen_cvtsd2ss_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, realRegisterXMM);
			x64Gen_movd_reg64Low32_xmmReg(x64GenContext, REG_RESV_TEMP, REG_RESV_FPR_TEMP);
		}
		if(g_CPUFeatures.x86.movbe == false )
			x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		if( indexed )
		{
			if( realRegisterMem == realRegisterMem2 )
				assert_dbg();
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		if(g_CPUFeatures.x86.movbe)
			x64Gen_movBETruncate_mem32Reg64PlusReg64_reg64(x64GenContext, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
		else
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
		if( indexed )
		{
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
	}
	else if( mode == PPCREC_FPR_ST_MODE_DOUBLE_FROM_PS0 )
	{
		if( indexed )
		{
			if( realRegisterMem == realRegisterMem2 )
				assert_dbg();
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		x64Gen_movsd_memReg64_xmmReg(x64GenContext, realRegisterXMM, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
		// store double low part	
		x64Emit_mov_reg64_mem32(x64GenContext, REG_RESV_TEMP, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+0);
		x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32+4, REG_RESV_TEMP);
		// store double high part	
		x64Emit_mov_reg64_mem32(x64GenContext, REG_RESV_TEMP, X86_REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+4);
		x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32+0, REG_RESV_TEMP);
		if( indexed )
		{
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
	}
	else if( mode == PPCREC_FPR_ST_MODE_UI32_FROM_PS0 )
	{
		x64Gen_movd_reg64Low32_xmmReg(x64GenContext, REG_RESV_TEMP, realRegisterXMM);
		x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		if( indexed )
		{
			if( realRegisterMem == realRegisterMem2 )
				assert_dbg();
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		else
		{
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, X86_REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
		}
	}
	else if(mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1 ||
			mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0 ||
			mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0 ||
			mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 ||
			mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0 ||
			mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 ||
			mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0 ||
			mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1 ||
			mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0 ||
			mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1 )
	{
		cemu_assert_debug(imlInstruction->op_storeLoad.flags2.notExpanded == false);
		PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext, x64GenContext, mode, realRegisterXMM, realRegisterMem, realRegisterMem2, imlInstruction->op_storeLoad.immS32, indexed);
	}
	else if (mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0)
	{
		PPCRecompilerX64Gen_imlInstr_psq_store_generic(ppcImlGenContext, x64GenContext, mode, realRegisterXMM, realRegisterMem, realRegisterMem2, imlInstruction->op_storeLoad.immS32, indexed, imlInstruction->op_storeLoad.registerGQR);
	}
	else
	{
		if( indexed )
			assert_dbg(); // todo
		debug_printf("PPCRecompilerX64Gen_imlInstruction_fpr_store(): Unsupported mode %d\n", mode);
		return false;
	}
	return true;
}

void _swapPS0PS1(x64GenContext_t* x64GenContext, sint32 xmmReg)
{
	x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, xmmReg, xmmReg, 1);
}

// FPR op FPR
void PPCRecompilerX64Gen_imlInstruction_fpr_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	uint32 regR = _regF64(imlInstruction->op_fpr_r_r.regR);
	uint32 regA = _regF64(imlInstruction->op_fpr_r_r.regA);

	if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP )
	{
		x64Gen_movddup_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP )
	{
		// VPUNPCKHQDQ
		if (regR == regA)
		{
			// unpack top to bottom and top
			x64Gen_unpckhpd_xmmReg_xmmReg(x64GenContext, regR, regA);
		}
		//else if ( hasAVXSupport )
		//{
		//	// unpack top to bottom and top with non-destructive destination
		//	// update: On Ivy Bridge this causes weird stalls?
		//	x64Gen_avx_VUNPCKHPD_xmm_xmm_xmm(x64GenContext, registerResult, registerOperand, registerOperand);
		//}
		else
		{
			// move top to bottom
			x64Gen_movhlps_xmmReg_xmmReg(x64GenContext, regR, regA);
			// duplicate bottom
			x64Gen_movddup_xmmReg_xmmReg(x64GenContext, regR, regR);
		}

	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM )
	{
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_TOP )
	{
		x64Gen_unpcklpd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED )
	{
		if( regR != regA )
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, regR, regA);
		_swapPS0PS1(x64GenContext, regR);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_TOP )
	{
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, regR, regA, 2);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM )
	{
		// use unpckhpd here?
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, regR, regA, 3);
		_swapPS0PS1(x64GenContext, regR);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM )
	{
		x64Gen_mulsd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_PAIR )
	{
		x64Gen_mulpd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_DIVIDE_BOTTOM )
	{
		x64Gen_divsd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_DIVIDE_PAIR)
	{
		x64Gen_divpd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_BOTTOM )
	{
		x64Gen_addsd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_PAIR )
	{
		x64Gen_addpd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_PAIR )
	{
		x64Gen_subpd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_BOTTOM )
	{
		x64Gen_subsd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_ASSIGN )
	{
		x64Gen_movaps_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_FCTIWZ )
	{
		x64Gen_cvttsd2si_xmmReg_xmmReg(x64GenContext, REG_RESV_TEMP, regA);
		x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
		// move to FPR register
		x64Gen_movq_xmmReg_reg64(x64GenContext, regR, REG_RESV_TEMP);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_FRES_TO_BOTTOM_AND_TOP )
	{
		// move register to XMM15
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regA);
		
		// call assembly routine to calculate accurate FRES result in XMM15
		x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)recompiler_fres);
		x64Gen_call_reg64(x64GenContext, REG_RESV_TEMP);

		// copy result to bottom and top half of result register
		x64Gen_movddup_xmmReg_xmmReg(x64GenContext, regR, REG_RESV_FPR_TEMP);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_RECIPROCAL_SQRT)
	{
		// move register to XMM15
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regA);

		// call assembly routine to calculate accurate FRSQRTE result in XMM15
		x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)recompiler_frsqrte);
		x64Gen_call_reg64(x64GenContext, REG_RESV_TEMP);

		// copy result to bottom of result register
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, regR, REG_RESV_FPR_TEMP);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATE_PAIR )
	{
		// copy register
		if( regR != regA )
		{
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, regR, regA);
		}
		// toggle sign bits
		x64Gen_xorps_xmmReg_mem128Reg64(x64GenContext, regR, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_xorNegateMaskPair));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ABS_PAIR )
	{
		// copy register
		if( regR != regA )
		{
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, regR, regA);
		}
		// set sign bit to 0
		x64Gen_andps_xmmReg_mem128Reg64(x64GenContext, regR, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_andAbsMaskPair));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_FRES_PAIR || imlInstruction->operation == PPCREC_IML_OP_FPR_FRSQRTE_PAIR)
	{
		// calculate bottom half of result
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regA);
		if(imlInstruction->operation == PPCREC_IML_OP_FPR_FRES_PAIR)
			x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)recompiler_fres);
		else
			x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)recompiler_frsqrte);
		x64Gen_call_reg64(x64GenContext, REG_RESV_TEMP); // calculate fres result in xmm15
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, regR, REG_RESV_FPR_TEMP);

		// calculate top half of result
		// todo - this top to bottom copy can be optimized?
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, REG_RESV_FPR_TEMP, regA, 3);
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_FPR_TEMP, 1); // swap top and bottom

		x64Gen_call_reg64(x64GenContext, REG_RESV_TEMP); // calculate fres result in xmm15

		x64Gen_unpcklpd_xmmReg_xmmReg(x64GenContext, regR, REG_RESV_FPR_TEMP); // copy bottom to top
	}
	else
	{
		assert_dbg();
	}
}

/*
 * FPR = op (fprA, fprB)
 */
void PPCRecompilerX64Gen_imlInstruction_fpr_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	uint32 regR = _regF64(imlInstruction->op_fpr_r_r_r.regR);
	uint32 regA = _regF64(imlInstruction->op_fpr_r_r_r.regA);
	uint32 regB = _regF64(imlInstruction->op_fpr_r_r_r.regB);

	if (imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM)
	{
		if (regR == regA)
		{
			x64Gen_mulsd_xmmReg_xmmReg(x64GenContext, regR, regB);
		}
		else if (regR == regB)
		{
			x64Gen_mulsd_xmmReg_xmmReg(x64GenContext, regR, regA);
		}
		else
		{
			x64Gen_movsd_xmmReg_xmmReg(x64GenContext, regR, regA);
			x64Gen_mulsd_xmmReg_xmmReg(x64GenContext, regR, regB);
		}
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_BOTTOM)
	{
		// todo: Use AVX 3-operand VADDSD if available
		if (regR == regA)
		{
			x64Gen_addsd_xmmReg_xmmReg(x64GenContext, regR, regB);
		}
		else if (regR == regB)
		{
			x64Gen_addsd_xmmReg_xmmReg(x64GenContext, regR, regA);
		}
		else
		{
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, regR, regA);
			x64Gen_addsd_xmmReg_xmmReg(x64GenContext, regR, regB);
		}
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_PAIR)
	{
		// registerResult = registerOperandA - registerOperandB
		if( regR == regA )
		{
			x64Gen_subpd_xmmReg_xmmReg(x64GenContext, regR, regB);
		}
		else if (g_CPUFeatures.x86.avx)
		{
			x64Gen_avx_VSUBPD_xmm_xmm_xmm(x64GenContext, regR, regA, regB);
		}
		else if( regR == regB )
		{
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regA);
			x64Gen_subpd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regB);
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, regR, REG_RESV_FPR_TEMP);
		}
		else
		{
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, regR, regA);
			x64Gen_subpd_xmmReg_xmmReg(x64GenContext, regR, regB);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_BOTTOM )
	{
		if( regR == regA )
		{
			x64Gen_subsd_xmmReg_xmmReg(x64GenContext, regR, regB);
		}
		else if( regR == regB )
		{
			x64Gen_movsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regA);
			x64Gen_subsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regB);
			x64Gen_movsd_xmmReg_xmmReg(x64GenContext, regR, REG_RESV_FPR_TEMP);
		}
		else
		{
			x64Gen_movsd_xmmReg_xmmReg(x64GenContext, regR, regA);
			x64Gen_subsd_xmmReg_xmmReg(x64GenContext, regR, regB);
		}
	}
	else
		assert_dbg();
}

/*
 * FPR = op (fprA, fprB, fprC)
 */
void PPCRecompilerX64Gen_imlInstruction_fpr_r_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	uint32 regR = _regF64(imlInstruction->op_fpr_r_r_r_r.regR);
	uint32 regA = _regF64(imlInstruction->op_fpr_r_r_r_r.regA);
	uint32 regB = _regF64(imlInstruction->op_fpr_r_r_r_r.regB);
	uint32 regC = _regF64(imlInstruction->op_fpr_r_r_r_r.regC);

	if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUM0 )
	{
		// todo: Investigate if there are other optimizations possible if the operand registers overlap
		// generic case
		// 1) move frA bottom to frTemp bottom and top
		x64Gen_movddup_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regA);
		// 2) add frB (both halfs, lower half is overwritten in the next step)
		x64Gen_addpd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regB);
		// 3) Interleave top of frTemp and frC
		x64Gen_unpckhpd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regC);
		// todo: We can optimize the REG_RESV_FPR_TEMP -> resultReg copy operation away when the result register does not overlap with any of the operand registers
		x64Gen_movaps_xmmReg_xmmReg(x64GenContext, regR, REG_RESV_FPR_TEMP);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUM1 )
	{
		// todo: Investigate if there are other optimizations possible if the operand registers overlap
		// 1) move frA bottom to frTemp bottom and top
		x64Gen_movddup_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regA);
		// 2) add frB (both halfs, lower half is overwritten in the next step)
		x64Gen_addpd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regB);
		// 3) Copy bottom from frC
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regC);
		//// 4) Swap bottom and top half
		//x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_FPR_TEMP, 1);
		// todo: We can optimize the REG_RESV_FPR_TEMP -> resultReg copy operation away when the result register does not overlap with any of the operand registers
		x64Gen_movaps_xmmReg_xmmReg(x64GenContext, regR, REG_RESV_FPR_TEMP);

		//float s0 = (float)hCPU->fpr[frC].fp0;
		//float s1 = (float)(hCPU->fpr[frA].fp0 + hCPU->fpr[frB].fp1);
		//hCPU->fpr[frD].fp0 = s0;
		//hCPU->fpr[frD].fp1 = s1;
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SELECT_BOTTOM )
	{
		x64Gen_comisd_xmmReg_mem64Reg64(x64GenContext, regA, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_constDouble0_0));
		sint32 jumpInstructionOffset1 = x64GenContext->emitter->GetWriteIndex();
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_UNSIGNED_BELOW, 0);
		// select C
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, regR, regC);
		sint32 jumpInstructionOffset2 = x64GenContext->emitter->GetWriteIndex();
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NONE, 0);
		// select B
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1, x64GenContext->emitter->GetWriteIndex());
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, regR, regB);
		// end
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2, x64GenContext->emitter->GetWriteIndex());
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SELECT_PAIR )
	{
		// select bottom
		x64Gen_comisd_xmmReg_mem64Reg64(x64GenContext, regA, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_constDouble0_0));
		sint32 jumpInstructionOffset1_bottom = x64GenContext->emitter->GetWriteIndex();
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_UNSIGNED_BELOW, 0);
		// select C bottom
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, regR, regC);
		sint32 jumpInstructionOffset2_bottom = x64GenContext->emitter->GetWriteIndex();
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NONE, 0);
		// select B bottom
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1_bottom, x64GenContext->emitter->GetWriteIndex());
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, regR, regB);
		// end
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2_bottom, x64GenContext->emitter->GetWriteIndex());
		// select top
		x64Gen_movhlps_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, regA); // copy top to bottom (todo: May cause stall?)
		x64Gen_comisd_xmmReg_mem64Reg64(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_constDouble0_0));
		sint32 jumpInstructionOffset1_top = x64GenContext->emitter->GetWriteIndex();
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_UNSIGNED_BELOW, 0);
		// select C top
		//x64Gen_movsd_xmmReg_xmmReg(x64GenContext, registerResult, registerOperandC);
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, regR, regC, 2);
		sint32 jumpInstructionOffset2_top = x64GenContext->emitter->GetWriteIndex();
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NONE, 0);
		// select B top
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1_top, x64GenContext->emitter->GetWriteIndex());
		//x64Gen_movsd_xmmReg_xmmReg(x64GenContext, registerResult, registerOperandB);
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, regR, regB, 2);
		// end
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2_top, x64GenContext->emitter->GetWriteIndex());
	}
	else
		assert_dbg();
}

void PPCRecompilerX64Gen_imlInstruction_fpr_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	uint32 regR = _regF64(imlInstruction->op_fpr_r.regR);

	if( imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATE_BOTTOM )
	{
		x64Gen_xorps_xmmReg_mem128Reg64(x64GenContext, regR, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_xorNegateMaskBottom));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ABS_BOTTOM )
	{
		x64Gen_andps_xmmReg_mem128Reg64(x64GenContext, regR, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_andAbsMaskBottom));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATIVE_ABS_BOTTOM )
	{
		x64Gen_orps_xmmReg_mem128Reg64(x64GenContext, regR, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_xorNegateMaskBottom));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM )
	{
		// convert to 32bit single
		x64Gen_cvtsd2ss_xmmReg_xmmReg(x64GenContext, regR, regR);
		// convert back to 64bit double
		x64Gen_cvtss2sd_xmmReg_xmmReg(x64GenContext, regR, regR);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_PAIR )
	{
		// convert to 32bit singles
		x64Gen_cvtpd2ps_xmmReg_xmmReg(x64GenContext, regR, regR);
		// convert back to 64bit doubles
		x64Gen_cvtps2pd_xmmReg_xmmReg(x64GenContext, regR, regR);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64)
	{
		// convert bottom to 64bit double
		x64Gen_cvtss2sd_xmmReg_xmmReg(x64GenContext, regR, regR);
		// copy to top half
		x64Gen_movddup_xmmReg_xmmReg(x64GenContext, regR, regR);
	}
	else
	{
		cemu_assert_unimplemented();
	}
}

void PPCRecompilerX64Gen_imlInstruction_fpr_compare(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = _reg8(imlInstruction->op_fpr_compare.regR);
	auto regA = _regF64(imlInstruction->op_fpr_compare.regA);
	auto regB = _regF64(imlInstruction->op_fpr_compare.regB);

	x64GenContext->emitter->XOR_dd(_reg32_from_reg8(regR), _reg32_from_reg8(regR));
	x64Gen_ucomisd_xmmReg_xmmReg(x64GenContext, regA, regB);

	if (imlInstruction->op_fpr_compare.cond == IMLCondition::UNORDERED_GT)
	{
		// GT case can be covered with a single SETnbe which checks CF==0 && ZF==0 (unordered sets both)
		x64GenContext->emitter->SETcc_b(X86Cond::X86_CONDITION_NBE, regR);
		return;
	}
	else if (imlInstruction->op_fpr_compare.cond == IMLCondition::UNORDERED_U)
	{
		// unordered case can be checked via PF
		x64GenContext->emitter->SETcc_b(X86Cond::X86_CONDITION_PE, regR);
		return;
	}

	// remember unordered state
	auto regTmp = _reg32_from_reg8(_reg32(REG_RESV_TEMP));
	x64GenContext->emitter->SETcc_b(X86Cond::X86_CONDITION_PO, regTmp); // by reversing the parity we can avoid having to XOR the value for masking the LT/EQ conditions

	X86Cond x86Cond;
	switch (imlInstruction->op_fpr_compare.cond)
	{
	case IMLCondition::UNORDERED_LT:
		x64GenContext->emitter->SETcc_b(X86Cond::X86_CONDITION_B, regR);
		break;
	case IMLCondition::UNORDERED_EQ:
		x64GenContext->emitter->SETcc_b(X86Cond::X86_CONDITION_Z, regR);
		break;
	default:
		cemu_assert_unimplemented();
	}
	x64GenContext->emitter->AND_bb(_reg8_from_reg32(regR), _reg8_from_reg32(regTmp)); // if unordered (PF=1) then force LT/GT/EQ to zero 
}