#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "PPCRecompilerX64.h"
#include "asm/x64util.h"
#include "Common/cpu_features.h"

void PPCRecompilerX64Gen_imlInstruction_fpr_r_name(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	uint32 name = imlInstruction->op_r_name.name;
	if( name >= PPCREC_NAME_FPR0 && name < (PPCREC_NAME_FPR0+32) )
	{
		x64Gen_movupd_xmmReg_memReg128(x64GenContext, tempToRealFPRRegister(imlInstruction->op_r_name.registerIndex), REG_ESP, offsetof(PPCInterpreter_t, fpr)+sizeof(FPR_t)*(name-PPCREC_NAME_FPR0));
	}
	else if( name >= PPCREC_NAME_TEMPORARY_FPR0 || name < (PPCREC_NAME_TEMPORARY_FPR0+8) )
	{
		x64Gen_movupd_xmmReg_memReg128(x64GenContext, tempToRealFPRRegister(imlInstruction->op_r_name.registerIndex), REG_ESP, offsetof(PPCInterpreter_t, temporaryFPR)+sizeof(FPR_t)*(name-PPCREC_NAME_TEMPORARY_FPR0));
	}
	else
	{
		cemu_assert_debug(false);
	}
}

void PPCRecompilerX64Gen_imlInstruction_fpr_name_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	uint32 name = imlInstruction->op_r_name.name;
	if( name >= PPCREC_NAME_FPR0 && name < (PPCREC_NAME_FPR0+32) )
	{
		x64Gen_movupd_memReg128_xmmReg(x64GenContext, tempToRealFPRRegister(imlInstruction->op_r_name.registerIndex), REG_ESP, offsetof(PPCInterpreter_t, fpr)+sizeof(FPR_t)*(name-PPCREC_NAME_FPR0));
	}
	else if( name >= PPCREC_NAME_TEMPORARY_FPR0 && name < (PPCREC_NAME_TEMPORARY_FPR0+8) )
	{
		x64Gen_movupd_memReg128_xmmReg(x64GenContext, tempToRealFPRRegister(imlInstruction->op_r_name.registerIndex), REG_ESP, offsetof(PPCInterpreter_t, temporaryFPR)+sizeof(FPR_t)*(name-PPCREC_NAME_TEMPORARY_FPR0));
	}
	else
	{
		cemu_assert_debug(false);
	}
}

void PPCRecompilerX64Gen_imlInstr_gqr_generateScaleCode(ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, sint32 registerXMM, bool isLoad, bool scalePS1, sint32 registerGQR)
{
	// load GQR
	x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, registerGQR);
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
void PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, uint8 mode, sint32 registerXMM, sint32 memReg, sint32 memRegEx, sint32 memImmS32, bool indexed, sint32 registerGQR = -1)
{
	if (mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1)
	{
		if (indexed)
		{
			assert_dbg();
		}
		// optimized code for ps float load
		x64Emit_mov_reg64_mem64(x64GenContext, REG_RESV_TEMP, REG_R13, memReg, memImmS32);
		x64Gen_bswap_reg64(x64GenContext, REG_RESV_TEMP);
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
			x64Emit_mov_mem32_reg64(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR), REG_RESV_TEMP);
			x64Gen_movddup_xmmReg_memReg64(x64GenContext, REG_RESV_FPR_TEMP, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
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
					x64Gen_movZeroExtend_reg64Low16_mem16Reg64PlusReg64(x64GenContext, REG_RESV_TEMP, REG_R13, memReg, memOffset);
					x64Gen_rol_reg64Low16_imm8(x64GenContext, REG_RESV_TEMP, 8); // endian swap
					if (isSigned)
						x64Gen_movSignExtend_reg64Low32_reg64Low16(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
					else
						x64Gen_movZeroExtend_reg64Low32_reg64Low16(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
				}
				else if (readSize == 8)
				{
					// byte
					x64Emit_mov_reg64b_mem8(x64GenContext, REG_RESV_TEMP, REG_R13, memReg, memOffset);
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
		if (registerGQR >= 0)
			PPCRecompilerX64Gen_imlInstr_gqr_generateScaleCode(ppcImlGenContext, x64GenContext, registerXMM, true, loadPS1, registerGQR);
	}
}

void PPCRecompilerX64Gen_imlInstr_psq_load_generic(ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, uint8 mode, sint32 registerXMM, sint32 memReg, sint32 memRegEx, sint32 memImmS32, bool indexed, sint32 registerGQR)
{
	bool loadPS1 = (mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1);
	// load GQR
	x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, registerGQR);
	// extract load type field
	x64Gen_shr_reg64Low32_imm8(x64GenContext, REG_RESV_TEMP, 16);
	x64Gen_and_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 7);
	// jump cases
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 4); // type 4 -> u8
	sint32 jumpOffset_caseU8 = x64GenContext->codeBufferIndex;
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 5); // type 5 -> u16
	sint32 jumpOffset_caseU16 = x64GenContext->codeBufferIndex;
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 6); // type 4 -> s8
	sint32 jumpOffset_caseS8 = x64GenContext->codeBufferIndex;
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 7); // type 5 -> s16
	sint32 jumpOffset_caseS16 = x64GenContext->codeBufferIndex;
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	// default case -> float

	// generate cases
	uint32 jumpOffset_endOfFloat;
	uint32 jumpOffset_endOfU8;
	uint32 jumpOffset_endOfU16;
	uint32 jumpOffset_endOfS8;

	PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext, x64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfFloat = x64GenContext->codeBufferIndex;
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseU16, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext, x64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_U16_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfU8 = x64GenContext->codeBufferIndex;
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseS16, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext, x64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_S16_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfU16 = x64GenContext->codeBufferIndex;
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseU8, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext, x64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_U8_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfS8 = x64GenContext->codeBufferIndex;
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseS8, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_imlInstr_psq_load(ppcImlGenContext, x64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_S8_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfFloat, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfU8, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfU16, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfS8, x64GenContext->codeBufferIndex);
}

// load from memory
bool PPCRecompilerX64Gen_imlInstruction_fpr_load(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction, bool indexed)
{
	PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
	sint32 realRegisterXMM = tempToRealFPRRegister(imlInstruction->op_storeLoad.registerData);
	sint32 realRegisterMem = tempToRealRegister(imlInstruction->op_storeLoad.registerMem);
	sint32 realRegisterMem2 = PPC_REC_INVALID_REGISTER;
	if( indexed )
		realRegisterMem2 = tempToRealRegister(imlInstruction->op_storeLoad.registerMem2);
	uint8 mode = imlInstruction->op_storeLoad.mode;

	if( mode == PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1 )
	{
		// load byte swapped single into temporary FPR
		if( indexed )
		{
			x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem2);
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem);
			if( g_CPUFeatures.x86.movbe )
				x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32);
			else
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32);
		}
		else
		{
			if( g_CPUFeatures.x86.movbe )
				x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32);
			else
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32);
		}
		if( g_CPUFeatures.x86.movbe == false )
			x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		if( g_CPUFeatures.x86.avx )
		{
			x64Gen_movd_xmmReg_reg64Low32(x64GenContext, realRegisterXMM, REG_RESV_TEMP);
		}
		else
		{
			x64Emit_mov_mem32_reg64(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR), REG_RESV_TEMP);
			x64Gen_movddup_xmmReg_memReg64(x64GenContext, realRegisterXMM, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
		}

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
				x64Emit_mov_reg64_mem64(x64GenContext, REG_RESV_TEMP, REG_R13, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32+0);
				x64Gen_bswap_reg64(x64GenContext, REG_RESV_TEMP);
				x64Gen_movq_xmmReg_reg64(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_TEMP);
				x64Gen_movsd_xmmReg_xmmReg(x64GenContext, realRegisterXMM, REG_RESV_FPR_TEMP);
			}
			else
			{
				x64Emit_mov_reg64_mem64(x64GenContext, REG_RESV_TEMP, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32+0);
				x64Gen_bswap_reg64(x64GenContext, REG_RESV_TEMP);
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
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_R13, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32+0);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+4, REG_RESV_TEMP);
				// calculate offset again
				x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem);
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem2);
				// load double high part to temporaryFPR
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_R13, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32+4);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+0, REG_RESV_TEMP);
				// load double from temporaryFPR
				x64Gen_movlpd_xmmReg_memReg64(x64GenContext, realRegisterXMM, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
			}
			else
			{
				// load double low part to temporaryFPR
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32+0);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+4, REG_RESV_TEMP);
				// load double high part to temporaryFPR
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32+4);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+0, REG_RESV_TEMP);
				// load double from temporaryFPR
				x64Gen_movlpd_xmmReg_memReg64(x64GenContext, realRegisterXMM, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
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
		PPCRecompilerX64Gen_imlInstr_psq_load_generic(ppcImlGenContext, x64GenContext, mode, realRegisterXMM, realRegisterMem, realRegisterMem2, imlInstruction->op_storeLoad.immS32, indexed, tempToRealRegister(imlInstruction->op_storeLoad.registerGQR));
	}
	else
	{
		return false;
	}
	return true;
}

void PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, uint8 mode, sint32 registerXMM, sint32 memReg, sint32 memRegEx, sint32 memImmS32, bool indexed, sint32 registerGQR = -1)
{
	bool storePS1 = (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1 ||
		mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1);
	bool isFloat = mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0 || mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1;
	if (registerGQR >= 0)
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
		if (g_CPUFeatures.x86.avx)
		{
			x64Gen_movd_reg64Low32_xmmReg(x64GenContext, REG_RESV_TEMP, REG_RESV_FPR_TEMP);
		}
		else
		{
			x64Gen_movsd_memReg64_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
			x64Emit_mov_reg64_mem32(x64GenContext, REG_RESV_TEMP, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
		}
		if (g_CPUFeatures.x86.movbe == false)
			x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		if (indexed)
		{
			cemu_assert_debug(memReg != memRegEx);
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, memReg, memRegEx);
		}
		if (g_CPUFeatures.x86.movbe)
			x64Gen_movBETruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_R13, memReg, memImmS32, REG_RESV_TEMP);
		else
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_R13, memReg, memImmS32, REG_RESV_TEMP);
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
		x64Gen_bswap_reg64(x64GenContext, REG_RESV_TEMP);
		x64Gen_mov_mem64Reg64PlusReg64_reg64(x64GenContext, REG_RESV_TEMP, REG_R13, memReg, memImmS32);
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
		sint32 jumpInstructionOffset1 = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_SIGNED_GREATER_EQUAL, 0);
		x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, clampMin);
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1, x64GenContext->codeBufferIndex);
		// min(i, clampMax)
		x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, clampMax);
		sint32 jumpInstructionOffset2 = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_SIGNED_LESS_EQUAL, 0);
		x64Gen_mov_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, clampMax);
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2, x64GenContext->codeBufferIndex);
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

void PPCRecompilerX64Gen_imlInstr_psq_store_generic(ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, uint8 mode, sint32 registerXMM, sint32 memReg, sint32 memRegEx, sint32 memImmS32, bool indexed, sint32 registerGQR)
{
	bool storePS1 = (mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1);
	// load GQR
	x64Gen_mov_reg64_reg64(x64GenContext, REG_RESV_TEMP, registerGQR);
	// extract store type field
	x64Gen_and_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 7);
	// jump cases
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 4); // type 4 -> u8
	sint32 jumpOffset_caseU8 = x64GenContext->codeBufferIndex;
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 5); // type 5 -> u16
	sint32 jumpOffset_caseU16 = x64GenContext->codeBufferIndex;
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 6); // type 4 -> s8
	sint32 jumpOffset_caseS8 = x64GenContext->codeBufferIndex;
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	x64Gen_cmp_reg64Low32_imm32(x64GenContext, REG_RESV_TEMP, 7); // type 5 -> s16
	sint32 jumpOffset_caseS16 = x64GenContext->codeBufferIndex;
	x64Gen_jmpc_far(x64GenContext, X86_CONDITION_EQUAL, 0);
	// default case -> float

	// generate cases
	uint32 jumpOffset_endOfFloat;
	uint32 jumpOffset_endOfU8;
	uint32 jumpOffset_endOfU16;
	uint32 jumpOffset_endOfS8;

	PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext, x64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfFloat = x64GenContext->codeBufferIndex;
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseU16, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext, x64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_U16_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfU8 = x64GenContext->codeBufferIndex;
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseS16, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext, x64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_S16_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfU16 = x64GenContext->codeBufferIndex;
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseU8, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext, x64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_U8_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);
	jumpOffset_endOfS8 = x64GenContext->codeBufferIndex;
	x64Gen_jmp_imm32(x64GenContext, 0);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_caseS8, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_imlInstr_psq_store(ppcImlGenContext, x64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_S8_PS0, registerXMM, memReg, memRegEx, memImmS32, indexed, registerGQR);

	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfFloat, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfU8, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfU16, x64GenContext->codeBufferIndex);
	PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpOffset_endOfS8, x64GenContext->codeBufferIndex);
}

// store to memory
bool PPCRecompilerX64Gen_imlInstruction_fpr_store(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction, bool indexed)
{
	PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
	sint32 realRegisterXMM = tempToRealFPRRegister(imlInstruction->op_storeLoad.registerData);
	sint32 realRegisterMem = tempToRealRegister(imlInstruction->op_storeLoad.registerMem);
	sint32 realRegisterMem2 = PPC_REC_INVALID_REGISTER;
	if( indexed )
		realRegisterMem2 = tempToRealRegister(imlInstruction->op_storeLoad.registerMem2);
	uint8 mode = imlInstruction->op_storeLoad.mode;
	if( mode == PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0 )
	{
		if (imlInstruction->op_storeLoad.flags2.notExpanded)
		{
			// value is already in single format
			if (g_CPUFeatures.x86.avx)
			{
				x64Gen_movd_reg64Low32_xmmReg(x64GenContext, REG_RESV_TEMP, realRegisterXMM);
			}
			else
			{
				x64Gen_movsd_memReg64_xmmReg(x64GenContext, realRegisterXMM, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
				x64Emit_mov_reg64_mem32(x64GenContext, REG_RESV_TEMP, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
			}
		}
		else
		{
			x64Gen_cvtsd2ss_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, realRegisterXMM);
			if (g_CPUFeatures.x86.avx)
			{
				x64Gen_movd_reg64Low32_xmmReg(x64GenContext, REG_RESV_TEMP, REG_RESV_FPR_TEMP);
			}
			else
			{
				x64Gen_movsd_memReg64_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
				x64Emit_mov_reg64_mem32(x64GenContext, REG_RESV_TEMP, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
			}
		}
		if( g_CPUFeatures.x86.movbe == false )
			x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		if( indexed )
		{
			if( realRegisterMem == realRegisterMem2 )
				assert_dbg();
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		if( g_CPUFeatures.x86.movbe )
			x64Gen_movBETruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
		else
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
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
		x64Gen_movsd_memReg64_xmmReg(x64GenContext, realRegisterXMM, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
		// store double low part		
		x64Emit_mov_reg64_mem32(x64GenContext, REG_RESV_TEMP, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+0);
		x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32+4, REG_RESV_TEMP);
		// store double high part		
		x64Emit_mov_reg64_mem32(x64GenContext, REG_RESV_TEMP, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR)+4);
		x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32+0, REG_RESV_TEMP);
		if( indexed )
		{
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
	}
	else if( mode == PPCREC_FPR_ST_MODE_UI32_FROM_PS0 )
	{
		if( g_CPUFeatures.x86.avx )
		{
			x64Gen_movd_reg64Low32_xmmReg(x64GenContext, REG_RESV_TEMP, realRegisterXMM);
		}
		else
		{
			x64Gen_movsd_memReg64_xmmReg(x64GenContext, realRegisterXMM, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
			x64Emit_mov_reg64_mem32(x64GenContext, REG_RESV_TEMP, REG_RSP, offsetof(PPCInterpreter_t, temporaryFPR));
		}
		x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		if( indexed )
		{
			if( realRegisterMem == realRegisterMem2 )
				assert_dbg();
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		else
		{
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_R13, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
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
		PPCRecompilerX64Gen_imlInstr_psq_store_generic(ppcImlGenContext, x64GenContext, mode, realRegisterXMM, realRegisterMem, realRegisterMem2, imlInstruction->op_storeLoad.immS32, indexed, tempToRealRegister(imlInstruction->op_storeLoad.registerGQR));
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
void PPCRecompilerX64Gen_imlInstruction_fpr_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
	if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		x64Gen_movddup_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		// VPUNPCKHQDQ
		if (imlInstruction->op_fpr_r_r.registerResult == imlInstruction->op_fpr_r_r.registerOperand)
		{
			// unpack top to bottom and top
			x64Gen_unpckhpd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
		}
		//else if ( g_CPUFeatures.x86.avx )
		//{
		//	// unpack top to bottom and top with non-destructive destination
		//	// update: On Ivy Bridge this causes weird stalls?
		//	x64Gen_avx_VUNPCKHPD_xmm_xmm_xmm(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand, imlInstruction->op_fpr_r_r.registerOperand);
		//}
		else
		{
			// move top to bottom
			x64Gen_movhlps_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
			// duplicate bottom
			x64Gen_movddup_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerResult);
		}

	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_TOP )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		x64Gen_unpcklpd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		if( imlInstruction->op_fpr_r_r.registerResult != imlInstruction->op_fpr_r_r.registerOperand )
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
		_swapPS0PS1(x64GenContext, imlInstruction->op_fpr_r_r.registerResult);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_TOP )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand, 2);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// use unpckhpd here?
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand, 3);
		_swapPS0PS1(x64GenContext, imlInstruction->op_fpr_r_r.registerResult);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		x64Gen_mulsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_PAIR )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		x64Gen_mulpd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_DIVIDE_BOTTOM )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		x64Gen_divsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_DIVIDE_PAIR)
	{
		if (imlInstruction->crRegister != PPC_REC_INVALID_REGISTER)
		{
			assert_dbg();
		}
		x64Gen_divpd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_BOTTOM )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		x64Gen_addsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_PAIR )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		x64Gen_addpd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_PAIR )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		x64Gen_subpd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_BOTTOM )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		x64Gen_subsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_ASSIGN )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		x64Gen_movaps_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_FCTIWZ )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		x64Gen_cvttsd2si_xmmReg_xmmReg(x64GenContext, REG_RESV_TEMP, imlInstruction->op_fpr_r_r.registerOperand);
		x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
		// move to FPR register
		x64Gen_movq_xmmReg_reg64(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, REG_RESV_TEMP);
	}
	else if(imlInstruction->operation == PPCREC_IML_OP_FPR_FCMPU_BOTTOM ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_FCMPU_TOP ||
			imlInstruction->operation == PPCREC_IML_OP_FPR_FCMPO_BOTTOM )
	{
		if( imlInstruction->crRegister == PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		if (imlInstruction->operation == PPCREC_IML_OP_FPR_FCMPU_BOTTOM)
			x64Gen_ucomisd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
		else if (imlInstruction->operation == PPCREC_IML_OP_FPR_FCMPU_TOP)
		{
			// temporarily switch top/bottom of both operands and compare
			if (imlInstruction->op_fpr_r_r.registerResult == imlInstruction->op_fpr_r_r.registerOperand)
			{
				_swapPS0PS1(x64GenContext, imlInstruction->op_fpr_r_r.registerResult);
				x64Gen_ucomisd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
				_swapPS0PS1(x64GenContext, imlInstruction->op_fpr_r_r.registerResult);
			}
			else
			{
				_swapPS0PS1(x64GenContext, imlInstruction->op_fpr_r_r.registerResult);
				_swapPS0PS1(x64GenContext, imlInstruction->op_fpr_r_r.registerOperand);
				x64Gen_ucomisd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
				_swapPS0PS1(x64GenContext, imlInstruction->op_fpr_r_r.registerResult);
				_swapPS0PS1(x64GenContext, imlInstruction->op_fpr_r_r.registerOperand);
			}
		}
		else
			x64Gen_comisd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
		// todo: handle FPSCR updates
		// update cr
		sint32 crRegister = imlInstruction->crRegister;
		// if the parity bit is set (NaN) we need to manually set CR LT, GT and EQ to 0 (comisd/ucomisd sets the respective flags to 1 in case of NaN)
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_PARITY, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_SO)); // unordered
		sint32 jumpInstructionOffset1 = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_PARITY, 0);
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_UNSIGNED_BELOW, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_LT)); // same as X64_CONDITION_CARRY
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_UNSIGNED_ABOVE, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_GT));
		x64Gen_setcc_mem8(x64GenContext, X86_CONDITION_EQUAL, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_EQ));
		sint32 jumpInstructionOffset2 = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NONE, 0);
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1, x64GenContext->codeBufferIndex);
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_LT), 0);
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_GT), 0);
		x64Gen_mov_mem8Reg64_imm8(x64GenContext, REG_RSP, offsetof(PPCInterpreter_t, cr)+sizeof(uint8)*(crRegister*4+PPCREC_CR_BIT_EQ), 0);
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2, x64GenContext->codeBufferIndex);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_FRES_TO_BOTTOM_AND_TOP )
	{
		if( imlInstruction->crRegister != PPC_REC_INVALID_REGISTER )
		{
			assert_dbg();
		}
		// move register to XMM15
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r.registerOperand);
		
		// call assembly routine to calculate accurate FRES result in XMM15
		x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)recompiler_fres);
		x64Gen_call_reg64(x64GenContext, REG_RESV_TEMP);

		// copy result to bottom and top half of result register
		x64Gen_movddup_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, REG_RESV_FPR_TEMP);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_RECIPROCAL_SQRT)
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// move register to XMM15
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r.registerOperand);

		// call assembly routine to calculate accurate FRSQRTE result in XMM15
		x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)recompiler_frsqrte);
		x64Gen_call_reg64(x64GenContext, REG_RESV_TEMP);

		// copy result to bottom of result register
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, REG_RESV_FPR_TEMP);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATE_PAIR )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// copy register
		if( imlInstruction->op_fpr_r_r.registerResult != imlInstruction->op_fpr_r_r.registerOperand )
		{
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
		}
		// toggle sign bits
		x64Gen_xorps_xmmReg_mem128Reg64(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_xorNegateMaskPair));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ABS_PAIR )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// copy register
		if( imlInstruction->op_fpr_r_r.registerResult != imlInstruction->op_fpr_r_r.registerOperand )
		{
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, imlInstruction->op_fpr_r_r.registerOperand);
		}
		// set sign bit to 0
		x64Gen_andps_xmmReg_mem128Reg64(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_andAbsMaskPair));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_FRES_PAIR || imlInstruction->operation == PPCREC_IML_OP_FPR_FRSQRTE_PAIR)
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// calculate bottom half of result
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r.registerOperand);
		if(imlInstruction->operation == PPCREC_IML_OP_FPR_FRES_PAIR)
			x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)recompiler_fres);
		else
			x64Gen_mov_reg64_imm64(x64GenContext, REG_RESV_TEMP, (uint64)recompiler_frsqrte);
		x64Gen_call_reg64(x64GenContext, REG_RESV_TEMP); // calculate fres result in xmm15
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, REG_RESV_FPR_TEMP);

		// calculate top half of result
		// todo - this top to bottom copy can be optimized?
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r.registerOperand, 3);
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_FPR_TEMP, 1); // swap top and bottom

		x64Gen_call_reg64(x64GenContext, REG_RESV_TEMP); // calculate fres result in xmm15

		x64Gen_unpcklpd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r.registerResult, REG_RESV_FPR_TEMP); // copy bottom to top
	}
	else
	{
		assert_dbg();
	}
}

/*
 * FPR = op (fprA, fprB)
 */
void PPCRecompilerX64Gen_imlInstruction_fpr_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);

	if (imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM)
	{
		if (imlInstruction->crRegister != PPC_REC_INVALID_REGISTER)
		{
			assert_dbg();
		}
		if (imlInstruction->op_fpr_r_r_r.registerResult == imlInstruction->op_fpr_r_r_r.registerOperandA)
		{
			x64Gen_mulsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandB);
		}
		else if (imlInstruction->op_fpr_r_r_r.registerResult == imlInstruction->op_fpr_r_r_r.registerOperandB)
		{
			x64Gen_mulsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandA);
		}
		else
		{
			x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandA);
			x64Gen_mulsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandB);
		}
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_BOTTOM)
	{
		// registerResult(fp0) = registerOperandA(fp0) + registerOperandB(fp0)
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// todo: Use AVX 3-operand VADDSD if available
		if (imlInstruction->op_fpr_r_r_r.registerResult == imlInstruction->op_fpr_r_r_r.registerOperandA)
		{
			x64Gen_addsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandB);
		}
		else if (imlInstruction->op_fpr_r_r_r.registerResult == imlInstruction->op_fpr_r_r_r.registerOperandB)
		{
			x64Gen_addsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandA);
		}
		else
		{
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandA);
			x64Gen_addsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandB);
		}
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_PAIR)
	{
		// registerResult = registerOperandA - registerOperandB
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		if( imlInstruction->op_fpr_r_r_r.registerResult == imlInstruction->op_fpr_r_r_r.registerOperandA )
		{
			x64Gen_subpd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandB);
		}
		else if (g_CPUFeatures.x86.avx)
		{
			x64Gen_avx_VSUBPD_xmm_xmm_xmm(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandA, imlInstruction->op_fpr_r_r_r.registerOperandB);
		}
		else if( imlInstruction->op_fpr_r_r_r.registerResult == imlInstruction->op_fpr_r_r_r.registerOperandB )
		{
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r_r.registerOperandA);
			x64Gen_subpd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r_r.registerOperandB);
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, REG_RESV_FPR_TEMP);
		}
		else
		{
			x64Gen_movaps_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandA);
			x64Gen_subpd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandB);
		}
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_BOTTOM )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		if( imlInstruction->op_fpr_r_r_r.registerResult == imlInstruction->op_fpr_r_r_r.registerOperandA )
		{
			x64Gen_subsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandB);
		}
		else if( imlInstruction->op_fpr_r_r_r.registerResult == imlInstruction->op_fpr_r_r_r.registerOperandB )
		{
			x64Gen_movsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r_r.registerOperandA);
			x64Gen_subsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r_r.registerOperandB);
			x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, REG_RESV_FPR_TEMP);
		}
		else
		{
			x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandA);
			x64Gen_subsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r.registerOperandB);
		}
	}
	else
		assert_dbg();
}

/*
 * FPR = op (fprA, fprB, fprC)
 */
void PPCRecompilerX64Gen_imlInstruction_fpr_r_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
	if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUM0 )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);

		// todo: Investigate if there are other optimizations possible if the operand registers overlap
		// generic case
		// 1) move frA bottom to frTemp bottom and top
		x64Gen_movddup_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r_r_r.registerOperandA);
		// 2) add frB (both halfs, lower half is overwritten in the next step)
		x64Gen_addpd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r_r_r.registerOperandB);
		// 3) Interleave top of frTemp and frC
		x64Gen_unpckhpd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r_r_r.registerOperandC);
		// todo: We can optimize the REG_RESV_FPR_TEMP -> resultReg copy operation away when the result register does not overlap with any of the operand registers
		x64Gen_movaps_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerResult, REG_RESV_FPR_TEMP);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUM1 )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// todo: Investigate if there are other optimizations possible if the operand registers overlap
		// 1) move frA bottom to frTemp bottom and top
		x64Gen_movddup_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r_r_r.registerOperandA);
		// 2) add frB (both halfs, lower half is overwritten in the next step)
		x64Gen_addpd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r_r_r.registerOperandB);
		// 3) Copy bottom from frC
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r_r_r.registerOperandC);
		//// 4) Swap bottom and top half
		//x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_FPR_TEMP, 1);
		// todo: We can optimize the REG_RESV_FPR_TEMP -> resultReg copy operation away when the result register does not overlap with any of the operand registers
		x64Gen_movaps_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerResult, REG_RESV_FPR_TEMP);

		//float s0 = (float)hCPU->fpr[frC].fp0;
		//float s1 = (float)(hCPU->fpr[frA].fp0 + hCPU->fpr[frB].fp1);
		//hCPU->fpr[frD].fp0 = s0;
		//hCPU->fpr[frD].fp1 = s1;
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SELECT_BOTTOM )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		x64Gen_comisd_xmmReg_mem64Reg64(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerOperandA, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_constDouble0_0));
		sint32 jumpInstructionOffset1 = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_UNSIGNED_BELOW, 0);
		// select C
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r_r.registerOperandC);
		sint32 jumpInstructionOffset2 = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NONE, 0);
		// select B
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1, x64GenContext->codeBufferIndex);
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r_r.registerOperandB);
		// end
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2, x64GenContext->codeBufferIndex);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SELECT_PAIR )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// select bottom
		x64Gen_comisd_xmmReg_mem64Reg64(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerOperandA, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_constDouble0_0));
		sint32 jumpInstructionOffset1_bottom = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_UNSIGNED_BELOW, 0);
		// select C bottom
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r_r.registerOperandC);
		sint32 jumpInstructionOffset2_bottom = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NONE, 0);
		// select B bottom
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1_bottom, x64GenContext->codeBufferIndex);
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r_r.registerOperandB);
		// end
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2_bottom, x64GenContext->codeBufferIndex);
		// select top
		x64Gen_movhlps_xmmReg_xmmReg(x64GenContext, REG_RESV_FPR_TEMP, imlInstruction->op_fpr_r_r_r_r.registerOperandA); // copy top to bottom (todo: May cause stall?)
		x64Gen_comisd_xmmReg_mem64Reg64(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_constDouble0_0));
		sint32 jumpInstructionOffset1_top = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_UNSIGNED_BELOW, 0);
		// select C top
		//x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r_r.registerOperandC);
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r_r.registerOperandC, 2);
		sint32 jumpInstructionOffset2_top = x64GenContext->codeBufferIndex;
		x64Gen_jmpc_near(x64GenContext, X86_CONDITION_NONE, 0);
		// select B top
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset1_top, x64GenContext->codeBufferIndex);
		//x64Gen_movsd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r_r.registerOperandB);
		x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext, imlInstruction->op_fpr_r_r_r_r.registerResult, imlInstruction->op_fpr_r_r_r_r.registerOperandB, 2);
		// end
		PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext, jumpInstructionOffset2_top, x64GenContext->codeBufferIndex);
	}
	else
		assert_dbg();
}

/*
 * Single FPR operation
 */
void PPCRecompilerX64Gen_imlInstruction_fpr_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction)
{
	PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction, ppcImlGenContext, x64GenContext);
	if( imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATE_BOTTOM )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// toggle sign bit
		x64Gen_xorps_xmmReg_mem128Reg64(x64GenContext, imlInstruction->op_fpr_r.registerResult, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_xorNegateMaskBottom));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ABS_BOTTOM )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// mask out sign bit
		x64Gen_andps_xmmReg_mem128Reg64(x64GenContext, imlInstruction->op_fpr_r.registerResult, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_andAbsMaskBottom));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATIVE_ABS_BOTTOM )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// set sign bit
		x64Gen_orps_xmmReg_mem128Reg64(x64GenContext, imlInstruction->op_fpr_r.registerResult, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_xorNegateMaskBottom));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// convert to 32bit single
		x64Gen_cvtsd2ss_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r.registerResult, imlInstruction->op_fpr_r.registerResult);
		// convert back to 64bit double
		x64Gen_cvtss2sd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r.registerResult, imlInstruction->op_fpr_r.registerResult);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_PAIR )
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// convert to 32bit singles
		x64Gen_cvtpd2ps_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r.registerResult, imlInstruction->op_fpr_r.registerResult);
		// convert back to 64bit doubles
		x64Gen_cvtps2pd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r.registerResult, imlInstruction->op_fpr_r.registerResult);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64)
	{
		cemu_assert_debug(imlInstruction->crRegister == PPC_REC_INVALID_REGISTER);
		// convert bottom to 64bit double
		x64Gen_cvtss2sd_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r.registerResult, imlInstruction->op_fpr_r.registerResult);
		// copy to top half
		x64Gen_movddup_xmmReg_xmmReg(x64GenContext, imlInstruction->op_fpr_r.registerResult, imlInstruction->op_fpr_r.registerResult);
	}
	else
	{
		cemu_assert_unimplemented();
	}
}
