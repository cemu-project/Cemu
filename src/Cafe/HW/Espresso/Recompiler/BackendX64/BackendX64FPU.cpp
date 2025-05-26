#include "../PPCRecompiler.h"
#include "../IML/IML.h"
#include "BackendX64.h"
#include "Common/cpu_features.h"

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

// load from memory
bool PPCRecompilerX64Gen_imlInstruction_fpr_load(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction, bool indexed)
{
	sint32 realRegisterXMM =  _regF64(imlInstruction->op_storeLoad.registerData);
	sint32 realRegisterMem = _regI32(imlInstruction->op_storeLoad.registerMem);
	sint32 realRegisterMem2 = PPC_REC_INVALID_REGISTER;
	if( indexed )
		realRegisterMem2 = _regI32(imlInstruction->op_storeLoad.registerMem2);
	uint8 mode = imlInstruction->op_storeLoad.mode;

	if( mode == PPCREC_FPR_LD_MODE_SINGLE )
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
		}
	}
	else if( mode == PPCREC_FPR_LD_MODE_DOUBLE )
	{
		if( g_CPUFeatures.x86.avx )
		{
			if( indexed )
			{
				// calculate offset
				x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem);
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem2);
				// load value
				x64Emit_mov_reg64_mem64(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32+0);
				x64GenContext->emitter->BSWAP_q(REG_RESV_TEMP);
				x64Gen_movq_xmmReg_reg64(x64GenContext, REG_RESV_FPR_TEMP, REG_RESV_TEMP);
				x64Gen_movsd_xmmReg_xmmReg(x64GenContext, realRegisterXMM, REG_RESV_FPR_TEMP);
			}
			else
			{
				x64Emit_mov_reg64_mem64(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32+0);
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
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32+0);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryFPR)+4, REG_RESV_TEMP);
				// calculate offset again
				x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem);
				x64Gen_add_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, realRegisterMem2);
				// load double high part to temporaryFPR
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, REG_RESV_TEMP, imlInstruction->op_storeLoad.immS32+4);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryFPR)+0, REG_RESV_TEMP);
				// load double from temporaryFPR
				x64Gen_movlpd_xmmReg_memReg64(x64GenContext, realRegisterXMM, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryFPR));
			}
			else
			{
				// load double low part to temporaryFPR
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32+0);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryFPR)+4, REG_RESV_TEMP);
				// load double high part to temporaryFPR
				x64Emit_mov_reg32_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32+4);
				x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
				x64Emit_mov_mem32_reg64(x64GenContext, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryFPR)+0, REG_RESV_TEMP);
				// load double from temporaryFPR
				x64Gen_movlpd_xmmReg_memReg64(x64GenContext, realRegisterXMM, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryFPR));
			}
		}
	}
	else
	{
		return false;
	}
	return true;
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
	if( mode == PPCREC_FPR_ST_MODE_SINGLE )
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
			x64Gen_movBETruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
		else
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
		if( indexed )
		{
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
	}
	else if( mode == PPCREC_FPR_ST_MODE_DOUBLE )
	{
		if( indexed )
		{
			if( realRegisterMem == realRegisterMem2 )
				assert_dbg();
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		x64Gen_movsd_memReg64_xmmReg(x64GenContext, realRegisterXMM, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryFPR));
		// store double low part	
		x64Emit_mov_reg64_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryFPR)+0);
		x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32+4, REG_RESV_TEMP);
		// store double high part	
		x64Emit_mov_reg64_mem32(x64GenContext, REG_RESV_TEMP, REG_RESV_HCPU, offsetof(PPCInterpreter_t, temporaryFPR)+4);
		x64Gen_bswap_reg64Lower32bit(x64GenContext, REG_RESV_TEMP);
		x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32+0, REG_RESV_TEMP);
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
			cemu_assert_debug(realRegisterMem == realRegisterMem2);
			x64Gen_add_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
			x64Gen_sub_reg64Low32_reg64Low32(x64GenContext, realRegisterMem, realRegisterMem2);
		}
		else
		{
			x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, REG_RESV_MEMBASE, realRegisterMem, imlInstruction->op_storeLoad.immS32, REG_RESV_TEMP);
		}
	}
	else
	{
		debug_printf("PPCRecompilerX64Gen_imlInstruction_fpr_store(): Unsupported mode %d\n", mode);
		return false;
	}
	return true;
}

// FPR op FPR
void PPCRecompilerX64Gen_imlInstruction_fpr_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	if( imlInstruction->operation == PPCREC_IML_OP_FPR_FLOAT_TO_INT )
	{
		uint32 regGpr = _regI32(imlInstruction->op_fpr_r_r.regR);
		uint32 regFpr = _regF64(imlInstruction->op_fpr_r_r.regA);
		x64Gen_cvttsd2si_reg64Low_xmmReg(x64GenContext, regGpr, regFpr);
		return;
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_INT_TO_FLOAT )
	{
		uint32 regFpr = _regF64(imlInstruction->op_fpr_r_r.regR);
		uint32 regGpr = _regI32(imlInstruction->op_fpr_r_r.regA);
		x64Gen_cvtsi2sd_xmmReg_xmmReg(x64GenContext, regFpr, regGpr);
		return;
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_BITCAST_INT_TO_FLOAT)
	{
		cemu_assert_debug(imlInstruction->op_fpr_r_r.regR.GetRegFormat() == IMLRegFormat::F64); // assuming target is always F64 for now
		cemu_assert_debug(imlInstruction->op_fpr_r_r.regA.GetRegFormat() == IMLRegFormat::I32); // supporting only 32bit floats as input for now
		// exact operation depends on size of types. Floats are automatically promoted to double if the target is F64
		uint32 regFpr = _regF64(imlInstruction->op_fpr_r_r.regR);
		if (imlInstruction->op_fpr_r_r.regA.GetRegFormat() == IMLRegFormat::I32)
		{
			uint32 regGpr = _regI32(imlInstruction->op_fpr_r_r.regA);
			x64Gen_movq_xmmReg_reg64(x64GenContext, regFpr, regGpr); // using reg32 as reg64 param here is ok. We'll refactor later
			// float to double
			x64Gen_cvtss2sd_xmmReg_xmmReg(x64GenContext, regFpr, regFpr);
		}
		else
		{
			cemu_assert_unimplemented();
		}
		return;
	}

	uint32 regR = _regF64(imlInstruction->op_fpr_r_r.regR);
	uint32 regA = _regF64(imlInstruction->op_fpr_r_r.regA);
	if( imlInstruction->operation == PPCREC_IML_OP_FPR_ASSIGN )
	{
		x64Gen_movsd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY )
	{
		x64Gen_mulsd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_DIVIDE )
	{
		x64Gen_divsd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ADD )
	{
		x64Gen_addsd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUB )
	{
		x64Gen_subsd_xmmReg_xmmReg(x64GenContext, regR, regA);
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_FCTIWZ )
	{
		x64Gen_cvttsd2si_xmmReg_xmmReg(x64GenContext, REG_RESV_TEMP, regA);
		x64Gen_mov_reg64Low32_reg64Low32(x64GenContext, REG_RESV_TEMP, REG_RESV_TEMP);
		// move to FPR register
		x64Gen_movq_xmmReg_reg64(x64GenContext, regR, REG_RESV_TEMP);
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

	if (imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY)
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
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ADD)
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
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_SUB )
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

	if( imlInstruction->operation == PPCREC_IML_OP_FPR_SELECT )
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
	else
		assert_dbg();
}

void PPCRecompilerX64Gen_imlInstruction_fpr_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction)
{
	uint32 regR = _regF64(imlInstruction->op_fpr_r.regR);

	if( imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATE )
	{
		x64Gen_xorps_xmmReg_mem128Reg64(x64GenContext, regR, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_xorNegateMaskBottom));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_LOAD_ONE )
	{
		x64Gen_movsd_xmmReg_memReg64(x64GenContext, regR, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_constDouble1_1));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_ABS )
	{
		x64Gen_andps_xmmReg_mem128Reg64(x64GenContext, regR, REG_RESV_RECDATA, offsetof(PPCRecompilerInstanceData_t, _x64XMM_andAbsMaskBottom));
	}
	else if( imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATIVE_ABS )
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
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_EXPAND_F32_TO_F64)
	{
		// convert bottom to 64bit double
		x64Gen_cvtss2sd_xmmReg_xmmReg(x64GenContext, regR, regR);
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