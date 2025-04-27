#include "Cafe/HW/Espresso/EspressoISA.h"
#include "../Interpreter/PPCInterpreterInternal.h"
#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "Cafe/GameProfile/GameProfile.h"

ATTR_MS_ABI double frsqrte_espresso(double input);
ATTR_MS_ABI double fres_espresso(double input);

IMLReg _GetRegCR(ppcImlGenContext_t* ppcImlGenContext, uint8 crReg, uint8 crBit);


// todo
IMLReg _GetRegFPRGekko(ppcImlGenContext_t* ppcImlGenContext, uint32 index)
{
	cemu_assert_debug(index < 32);
	return PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + index);
}

// deprecated way of getting a float register (always in paired mode)
IMLReg PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName)
{
	return PPCRecompilerImlGen_LookupReg(ppcImlGenContext, mappedName, IMLRegFormat::F64);
}

/*
 * Rounds the bottom double to single precision (if single precision accuracy is emulated)
 */
void PPRecompilerImmGen_optionalRoundBottomFPRToSinglePrecision(ppcImlGenContext_t* ppcImlGenContext, IMLReg fprRegister, bool flushDenormals=false)
{
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM, fprRegister);
	if( flushDenormals )
		assert_dbg();
}

/*
 * Rounds pair of doubles to single precision (if single precision accuracy is emulated)
 */
void PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext_t* ppcImlGenContext, IMLReg fprRegister, bool flushDenormals=false)
{
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_PAIR, fprRegister);
	if( flushDenormals )
		assert_dbg();
}

bool PPCRecompilerImlGen_LFS_LFSU_LFD_LFDU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withUpdate, bool isDouble)
{
	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, imm);
	// get memory gpr register index
	IMLReg gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	if (withUpdate)
	{
		// add imm to memory register
		cemu_assert_debug(rA != 0);
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
		imm = 0; // set imm to 0 so we dont add it twice
	}
	// get fpr register index
	IMLReg fprRegister = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	if (isDouble)
	{
		// LFD/LFDU
		ppcImlGenContext->emitInst().make_fpr_r_memory(fprRegister, gprRegister, imm, PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0, true);
	}
	else
	{
		// LFS/LFSU
		if( ppcImlGenContext->LSQE )
		{
			ppcImlGenContext->emitInst().make_fpr_r_memory(fprRegister, gprRegister, imm, PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1, true);
		}
		else
		{
			ppcImlGenContext->emitInst().make_fpr_r_memory(fprRegister, gprRegister, imm, PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0, true);
		}
	}
	return true;
}

bool PPCRecompilerImlGen_LFSX_LFSUX_LFDX_LFDUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withUpdate, bool isDouble)
{
	sint32 rA, frD, rB;
	PPC_OPC_TEMPL_X(opcode, frD, rA, rB);
	if( rA == 0 )
	{
		debugBreakpoint();
		return false;
	}
	// get memory gpr registers
	IMLReg gprRegister1 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	IMLReg gprRegister2 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
	if (withUpdate)
	{
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, gprRegister1, gprRegister1, gprRegister2);
	}
	// get fpr register index
	IMLReg fprRegister = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	if (isDouble)
	{
		if (withUpdate)
			ppcImlGenContext->emitInst().make_fpr_r_memory(fprRegister, gprRegister1, 0, PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0, true);
		else
			ppcImlGenContext->emitInst().make_fpr_r_memory_indexed(fprRegister, gprRegister1, gprRegister2, PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0, true);
	}
	else
	{
		if( ppcImlGenContext->LSQE )
		{
			if (withUpdate)
				ppcImlGenContext->emitInst().make_fpr_r_memory( fprRegister, gprRegister1, 0, PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1, true);
			else
				ppcImlGenContext->emitInst().make_fpr_r_memory_indexed( fprRegister, gprRegister1, gprRegister2, PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1, true);
		}
		else
		{
			if (withUpdate)
				ppcImlGenContext->emitInst().make_fpr_r_memory( fprRegister, gprRegister1, 0, PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0, true);
			else
				ppcImlGenContext->emitInst().make_fpr_r_memory_indexed( fprRegister, gprRegister1, gprRegister2, PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0, true);
		}
	}
	return true;
}

bool PPCRecompilerImlGen_STFS_STFSU_STFD_STFDU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withUpdate, bool isDouble)
{
	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, imm);
	IMLReg gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	IMLReg fprRegister = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	if (withUpdate)
	{
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
		imm = 0;
	}
	if (isDouble)
		ppcImlGenContext->emitInst().make_fpr_memory_r(fprRegister, gprRegister, imm, PPCREC_FPR_ST_MODE_DOUBLE_FROM_PS0, true);
	else
		ppcImlGenContext->emitInst().make_fpr_memory_r(fprRegister, gprRegister, imm, PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0, true);
	return true;
}

bool PPCRecompilerImlGen_STFSX_STFSUX_STFDX_STFDUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool hasUpdate, bool isDouble)
{
	sint32 rA, frS, rB;
	PPC_OPC_TEMPL_X(opcode, frS, rA, rB);
	if( rA == 0 )
	{
		debugBreakpoint();
		return false;
	}
	// get memory gpr registers
	IMLReg gprRegister1 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	IMLReg gprRegister2 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
	if (hasUpdate)
	{
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, gprRegister1, gprRegister1, gprRegister2);
	}
	// get fpr register index
	IMLReg fprRegister = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frS);
	auto mode = isDouble ? PPCREC_FPR_ST_MODE_DOUBLE_FROM_PS0 : PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0;
	if( ppcImlGenContext->LSQE )
	{
		if (hasUpdate)
			ppcImlGenContext->emitInst().make_fpr_memory_r(fprRegister, gprRegister1, 0, mode, true);
		else
			ppcImlGenContext->emitInst().make_fpr_memory_r_indexed(fprRegister, gprRegister1, gprRegister2, 0, mode, true);
	}
	else
	{
		if (hasUpdate)
			ppcImlGenContext->emitInst().make_fpr_memory_r(fprRegister, gprRegister1, 0, mode, true);
		else
			ppcImlGenContext->emitInst().make_fpr_memory_r_indexed(fprRegister, gprRegister1, gprRegister2, 0, mode, true);
	}
	return true;
}

bool PPCRecompilerImlGen_STFIWX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, frS, rB;
	PPC_OPC_TEMPL_X(opcode, frS, rA, rB);
	// get memory gpr registers
	IMLReg gprRegister1;
	IMLReg gprRegister2;
	if( rA != 0 )
	{
		gprRegister1 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
		gprRegister2 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
	}
	else
	{
		// rA is not used
		gprRegister1 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rB);
		gprRegister2 = IMLREG_INVALID;
	}
	// get fpr register index
	IMLReg fprRegister = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frS);
	if( rA != 0 )
		ppcImlGenContext->emitInst().make_fpr_memory_r_indexed(fprRegister, gprRegister1, gprRegister2, 0, PPCREC_FPR_ST_MODE_UI32_FROM_PS0, true);
	else
		ppcImlGenContext->emitInst().make_fpr_memory_r(fprRegister, gprRegister1, 0, PPCREC_FPR_ST_MODE_UI32_FROM_PS0, true);
	return true;
}

bool PPCRecompilerImlGen_FADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	PPC_ASSERT(frC==0);

	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);

	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_ADD_BOTTOM, fprRegisterD, fprRegisterA, fprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_FSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	PPC_ASSERT(frC==0);
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// subtract bottom double of frB from bottom double of frD
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_SUB_BOTTOM, fprRegisterD, fprRegisterA, fprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_FMUL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB_unused, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB_unused, frC);
	if( frD == frC )
	{
		// swap frA and frB
		sint32 temp = frA;
		frA = frC;
		frC = temp;
	}
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// move frA to frD (if different register)
	if( fprRegisterD != fprRegisterA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_ASSIGN, fprRegisterD, fprRegisterA); // always copy ps0 and ps1
	// multiply bottom double of frD with bottom double of frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM, fprRegisterD, fprRegisterC);
	return true;
}

bool PPCRecompilerImlGen_FDIV(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC_unused;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC_unused);
	PPC_ASSERT(frB==0);
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frD);

	if( frB == frD && frA != frB )
	{
		IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
		// move frA to temporary register
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_ASSIGN, fprRegisterTemp, fprRegisterA);
		// divide bottom double of temporary register by bottom double of frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE_BOTTOM, fprRegisterTemp, fprRegisterB);
		// move result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterTemp);
		return true;
	}
	// move frA to frD (if different register)
	if( fprRegisterD != fprRegisterA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterA); // copy ps0
	// divide bottom double of frD by bottom double of frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE_BOTTOM, fprRegisterD, fprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_FMADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// if frB is already in frD we need a temporary register to store the product of frA*frC
	if( frB == frD )
	{
		IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
		// move frA to temporary register
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_ASSIGN, fprRegisterTemp, fprRegisterA);
		// multiply bottom double of temporary register with bottom double of frC
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM, fprRegisterTemp, fprRegisterC);
		// add result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_BOTTOM, fprRegisterD, fprRegisterTemp);
		return true;
	}
	// if frC == frD -> swap registers, we assume that frC != frD
	if( fprRegisterD == fprRegisterC )
	{
		// swap frA and frC
		IMLReg temp = fprRegisterA;
		fprRegisterA = fprRegisterC;
		fprRegisterC = temp;
	}
	// move frA to frD (if different register)
	if( fprRegisterD != fprRegisterA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_ASSIGN, fprRegisterD, fprRegisterA); // always copy ps0 and ps1
	// multiply bottom double of frD with bottom double of frC
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM, fprRegisterD, fprRegisterC);
	// add frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_BOTTOM, fprRegisterD, fprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_FMSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// if frB is already in frD we need a temporary register to store the product of frA*frC
	if( frB == frD )
	{
		// not implemented
		return false;
	}
	// if frC == frD -> swap registers, we assume that frC != frD
	if( fprRegisterD == fprRegisterC )
	{
		// swap frA and frC
		IMLReg temp = fprRegisterA;
		fprRegisterA = fprRegisterC;
		fprRegisterC = temp;
	}
	// move frA to frD (if different register)
	if( fprRegisterD != fprRegisterA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_ASSIGN, fprRegisterD, fprRegisterA); // always copy ps0 and ps1
	// multiply bottom double of frD with bottom double of frC
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM, fprRegisterD, fprRegisterC);
	// sub frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB_BOTTOM, fprRegisterD, fprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_FNMSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);

	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// if frB is already in frD we need a temporary register to store the product of frA*frC
	if( frB == frD )
	{
		// hCPU->fpr[frD].fpr = -(hCPU->fpr[frA].fpr * hCPU->fpr[frC].fpr - hCPU->fpr[frD].fpr);
		IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
		//// negate frB/frD
		//ppcImlGenContext->emitInst().make_fpr_r(NULL,PPCREC_IML_OP_FPR_NEGATE_BOTTOM, fprRegisterD, true);
		// move frA to temporary register
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterTemp, fprRegisterA);
		// multiply bottom double of temporary register with bottom double of frC
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM, fprRegisterTemp, fprRegisterC);
		// sub frB from temporary register
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB_BOTTOM, fprRegisterTemp, fprRegisterB);
		// negate result
		ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE_BOTTOM, fprRegisterTemp);
		// move result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterTemp);
		return true;
	}
	// if frC == frD -> swap registers, we assume that frC != frD
	if( fprRegisterD == fprRegisterC )
	{
		// swap frA and frC
		IMLReg temp = fprRegisterA;
		fprRegisterA = fprRegisterC;
		fprRegisterC = temp;
	}
	// move frA to frD (if different register)
	if( fprRegisterD != fprRegisterA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterA); // always copy ps0 and ps1
	// multiply bottom double of frD with bottom double of frC
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM, fprRegisterD, fprRegisterC);
	// sub frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB_BOTTOM, fprRegisterD, fprRegisterB);
	// negate result
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE_BOTTOM, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_FMULS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB_unused, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB_unused, frC);

	if( frD == frC )
	{
		// swap frA and frC
		sint32 temp = frA;
		frA = frC;
		frC = temp;
	}
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// move frA to frD (if different register)
	if( fprRegisterD != fprRegisterA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_ASSIGN, fprRegisterD, fprRegisterA); // always copy ps0 and ps1
	
	// multiply bottom double of frD with bottom double of frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM, fprRegisterD, fprRegisterC);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundBottomFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	// if paired single mode, copy frD ps0 to ps1
	if( ppcImlGenContext->PSE )
	{	
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterD);
	}
	
	return true;
}

bool PPCRecompilerImlGen_FDIVS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC_unused;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC_unused);
	PPC_ASSERT(frB==0);
	/*hCPU->fpr[frD].fpr = (float)(hCPU->fpr[frA].fpr / hCPU->fpr[frB].fpr);
	if( hCPU->PSE )
		hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;*/
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);

	if( frB == frD && frA != frB )
	{
		IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
		// move frA to temporary register
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_ASSIGN, fprRegisterTemp, fprRegisterA);
		// divide bottom double of temporary register by bottom double of frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE_BOTTOM, fprRegisterTemp, fprRegisterB);
		// move result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterTemp);
		// adjust accuracy
		PPRecompilerImmGen_optionalRoundBottomFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
		// if paired single mode, copy frD ps0 to ps1
		if( ppcImlGenContext->PSE )
		{	
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterD);
		}
		return true;
	}
	// move frA to frD (if different register)
	if( fprRegisterD != fprRegisterA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_ASSIGN, fprRegisterD, fprRegisterA); // always copy ps0 and ps1
	// subtract bottom double of frB from bottom double of frD
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE_BOTTOM, fprRegisterD, fprRegisterB);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundBottomFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	// if paired single mode, copy frD ps0 to ps1
	if( ppcImlGenContext->PSE )
	{	
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterD);
	}
	return true;
}

bool PPCRecompilerImlGen_FADDS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);

	if( frD == frB )
	{
		// swap frA and frB
		sint32 temp = frA;
		frA = frB;
		frB = temp;
	}
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// move frA to frD (if different register)
	if( fprRegisterD != fprRegisterA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_ASSIGN, fprRegisterD, fprRegisterA); // always copy ps0 and ps1
	// add bottom double of frD and bottom double of frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_BOTTOM, fprRegisterD, fprRegisterB);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundBottomFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	// if paired single mode, copy frD ps0 to ps1
	if( ppcImlGenContext->PSE )
	{	
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterD);
	}
	return true;
}

bool PPCRecompilerImlGen_FSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	PPC_ASSERT(frB==0);

	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// subtract bottom
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_SUB_BOTTOM, fprRegisterD, fprRegisterA, fprRegisterB);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundBottomFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	// if paired single mode, copy frD ps0 to ps1
	if( ppcImlGenContext->PSE )
	{	
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterD);
	}
	return true;
}

bool PPCRecompilerImlGen_FMADDS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	//FPRD(RD) = FPRD(RA) * FPRD(RC) + FPRD(RB);
	//hCPU->fpr[frD].fpr = hCPU->fpr[frA].fpr * hCPU->fpr[frC].fpr + hCPU->fpr[frB].fpr;
	//if( hCPU->PSE )
	//	hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	IMLReg fprRegisterTemp;
	// if none of the operand registers overlap with the result register then we can avoid the usage of a temporary register
	if( fprRegisterD != fprRegisterA && fprRegisterD != fprRegisterB && fprRegisterD != fprRegisterC )
		fprRegisterTemp = fprRegisterD;
	else
		fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM, fprRegisterTemp, fprRegisterA, fprRegisterC);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_BOTTOM, fprRegisterTemp, fprRegisterB);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundBottomFPRToSinglePrecision(ppcImlGenContext, fprRegisterTemp);
	// set result
	if( ppcImlGenContext->PSE )
	{	
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterTemp);
	}
	else if( fprRegisterD != fprRegisterTemp )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterTemp);
	}
	return true;
}

bool PPCRecompilerImlGen_FMSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	//hCPU->fpr[frD].fp0 = (float)(hCPU->fpr[frA].fp0 * hCPU->fpr[frC].fp0 - hCPU->fpr[frB].fp0);
	//if( hCPU->PSE )
	//	hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	IMLReg fprRegisterTemp;
	// if none of the operand registers overlap with the result register then we can avoid the usage of a temporary register
	if( fprRegisterD != fprRegisterA && fprRegisterD != fprRegisterB && fprRegisterD != fprRegisterC )
		fprRegisterTemp = fprRegisterD;
	else
		fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM, fprRegisterTemp, fprRegisterA, fprRegisterC);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB_BOTTOM, fprRegisterTemp, fprRegisterB);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundBottomFPRToSinglePrecision(ppcImlGenContext, fprRegisterTemp);
	// set result
	if( ppcImlGenContext->PSE )
	{	
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterTemp);
	}
	else if( fprRegisterD != fprRegisterTemp )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterTemp);
	}
	return true;
}

bool PPCRecompilerImlGen_FNMSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);

	//[FP1(RD) = ]FP0(RD) = -(FP0(RA) * FP0(RC) - FP0(RB));
	//hCPU->fpr[frD].fp0 = (float)-(hCPU->fpr[frA].fp0 * hCPU->fpr[frC].fp0 - hCPU->fpr[frB].fp0);
	//if( PPC_PSE )
	//	hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;

	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	IMLReg fprRegisterTemp;
	// if none of the operand registers overlap with the result register then we can avoid the usage of a temporary register
	if( fprRegisterD != fprRegisterA && fprRegisterD != fprRegisterB && fprRegisterD != fprRegisterC )
		fprRegisterTemp = fprRegisterD;
	else
		fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM, fprRegisterTemp, fprRegisterA, fprRegisterC);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB_BOTTOM, fprRegisterTemp, fprRegisterB);
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE_BOTTOM, fprRegisterTemp);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundBottomFPRToSinglePrecision(ppcImlGenContext, fprRegisterTemp);
	// set result
	if( ppcImlGenContext->PSE )
	{	
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterTemp);
	}
	else if( fprRegisterD != fprRegisterTemp )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterTemp);
	}
	return true;
}

bool PPCRecompilerImlGen_FCMPO(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	printf("FCMPO: Not implemented\n");
	return false;

	//sint32 crfD, frA, frB;
	//PPC_OPC_TEMPL_X(opcode, crfD, frA, frB);
	//crfD >>= 2;
	//IMLReg regFprA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frA);
	//IMLReg regFprB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frB);

	//IMLReg crBitRegLT = _GetCRReg(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_LT);
	//IMLReg crBitRegGT = _GetCRReg(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_GT);
	//IMLReg crBitRegEQ = _GetCRReg(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_EQ);
	//IMLReg crBitRegSO = _GetCRReg(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_SO);

	//ppcImlGenContext->emitInst().make_fpr_compare(regFprA, regFprB, crBitRegLT, IMLCondition::UNORDERED_LT);
	//ppcImlGenContext->emitInst().make_fpr_compare(regFprA, regFprB, crBitRegGT, IMLCondition::UNORDERED_GT);
	//ppcImlGenContext->emitInst().make_fpr_compare(regFprA, regFprB, crBitRegEQ, IMLCondition::UNORDERED_EQ);
	//ppcImlGenContext->emitInst().make_fpr_compare(regFprA, regFprB, crBitRegSO, IMLCondition::UNORDERED_U);

	// todo - set fpscr

	//sint32 crfD, frA, frB;
	//PPC_OPC_TEMPL_X(opcode, crfD, frA, frB);
	//crfD >>= 2;
	//uint32 fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	//uint32 fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	//ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_FCMPO_BOTTOM, fprRegisterA, fprRegisterB, crfD);
	return true;
}

bool PPCRecompilerImlGen_FCMPU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 crfD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, crfD, frA, frB);
	crfD >>= 2;
	IMLReg regFprA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frA);
	IMLReg regFprB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frB);

	IMLReg crBitRegLT = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_LT);
	IMLReg crBitRegGT = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_GT);
	IMLReg crBitRegEQ = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_EQ);
	IMLReg crBitRegSO = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_SO);

	ppcImlGenContext->emitInst().make_fpr_compare(regFprA, regFprB, crBitRegLT, IMLCondition::UNORDERED_LT);
	ppcImlGenContext->emitInst().make_fpr_compare(regFprA, regFprB, crBitRegGT, IMLCondition::UNORDERED_GT);
	ppcImlGenContext->emitInst().make_fpr_compare(regFprA, regFprB, crBitRegEQ, IMLCondition::UNORDERED_EQ);
	ppcImlGenContext->emitInst().make_fpr_compare(regFprA, regFprB, crBitRegSO, IMLCondition::UNORDERED_U);

	// todo: set fpscr

	return true;
}

bool PPCRecompilerImlGen_FMR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, rA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, rA, frB);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_FABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	// load registers
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// move frB to frD (if different register)
	if( fprRegisterD != fprRegisterB )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterB);
	// abs frD
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_ABS_BOTTOM, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_FNABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	// load registers
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// move frB to frD (if different register)
	if( fprRegisterD != fprRegisterB )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterB);
	// abs frD
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATIVE_ABS_BOTTOM, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_FRES(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	// load registers
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_call_imm((uintptr_t)fres_espresso, fprRegisterB, IMLREG_INVALID, IMLREG_INVALID, fprRegisterD);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundBottomFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	// copy result to top
	if( ppcImlGenContext->PSE )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_FRSP(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	if( fprRegisterD != fprRegisterB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterB);
	}
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM, fprRegisterD);
	if( ppcImlGenContext->PSE )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_FNEG(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	if( opcode&PPC_OPC_RC )
	{
		return false;
	}
	// load registers
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// move frB to frD (if different register)
	if( fprRegisterD != fprRegisterB )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterB);
	// negate frD
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE_BOTTOM, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_FSEL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	if( opcode&PPC_OPC_RC )
	{
		return false;
	}
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_fpr_r_r_r_r(PPCREC_IML_OP_FPR_SELECT_BOTTOM, fprRegisterD, fprRegisterA, fprRegisterB, fprRegisterC);
	return true;
}

bool PPCRecompilerImlGen_FRSQRTE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	// hCPU->fpr[frD].fpr = 1.0 / sqrt(hCPU->fpr[frB].fpr);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_call_imm((uintptr_t)frsqrte_espresso, fprRegisterB, IMLREG_INVALID, IMLREG_INVALID, fprRegisterD);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundBottomFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_FCTIWZ(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_BOTTOM_FCTIWZ, fprRegisterD, fprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_PSQ_L(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, frD;
	uint32 immUnused;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, immUnused);

	sint32 gqrIndex = ((opcode >> 12) & 7);
	uint32 imm = opcode & 0xFFF;
	if (imm & 0x800)
		imm |= ~0xFFF;

	bool readPS1 = (opcode & 0x8000) == false;

	IMLReg gqrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_UGQR0 + gqrIndex);
	IMLReg gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA);
	IMLReg fprRegister = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frD);
	// psq load
	ppcImlGenContext->emitInst().make_fpr_r_memory(fprRegister, gprRegister, imm, readPS1 ? PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0, true, gqrRegister);
	return true;
}

bool PPCRecompilerImlGen_PSQ_LU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, frD;
	uint32 immUnused;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, immUnused);
	if (rA == 0)
		return false;

	sint32 gqrIndex = ((opcode >> 12) & 7);
	uint32 imm = opcode & 0xFFF;
	if (imm & 0x800)
		imm |= ~0xFFF;

	bool readPS1 = (opcode & 0x8000) == false;
	
	IMLReg gqrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_UGQR0 + gqrIndex);
	IMLReg gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA);

	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);

	IMLReg fprRegister = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frD);
	// paired load
	ppcImlGenContext->emitInst().make_fpr_r_memory(fprRegister, gprRegister, 0, readPS1 ? PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0, true, gqrRegister);
	return true;
}

bool PPCRecompilerImlGen_PSQ_ST(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, frD;
	uint32 immUnused;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, immUnused);
	uint32 imm = opcode & 0xFFF;
	if (imm & 0x800)
		imm |= ~0xFFF;
	sint32 gqrIndex = ((opcode >> 12) & 7);

	bool storePS1 = (opcode & 0x8000) == false;

	IMLReg gqrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_UGQR0 + gqrIndex);
	IMLReg gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA);
	IMLReg fprRegister = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frD);
	// paired store
	ppcImlGenContext->emitInst().make_fpr_memory_r(fprRegister, gprRegister, imm, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0, true, gqrRegister);
	return true;
}

bool PPCRecompilerImlGen_PSQ_STU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int rA, frD;
	uint32 immUnused;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, immUnused);
	if (rA == 0)
		return false;

	uint32 imm = opcode & 0xFFF;
	if (imm & 0x800)
		imm |= ~0xFFF;
	sint32 gqrIndex = ((opcode >> 12) & 7);

	bool storePS1 = (opcode & 0x8000) == false;

	IMLReg gqrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_UGQR0 + gqrIndex);
	IMLReg gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0 + rA);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);

	IMLReg fprRegister = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frD);
	// paired store
	ppcImlGenContext->emitInst().make_fpr_memory_r(fprRegister, gprRegister, 0, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0, true, gqrRegister);
	return true;
}

bool PPCRecompilerImlGen_PS_MULS0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frC;
	frC = (opcode>>6)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// we need a temporary register to store frC.fp0 in low and high half
	IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterTemp, fprRegisterC);
	// if frD == frA we can multiply frD immediately and safe a copy instruction
	if( frD == frA )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	else
	{
		// we multiply temporary by frA
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterTemp, fprRegisterA);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_MULS1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frC;
	frC = (opcode>>6)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// we need a temporary register to store frC.fp0 in low and high half
	IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP, fprRegisterTemp, fprRegisterC);
	// if frD == frA we can multiply frD immediately and safe a copy instruction
	if( frD == frA )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	else
	{
		// we multiply temporary by frA
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterTemp, fprRegisterA);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_MADDS0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	//float s0 = (float)(hCPU->fpr[frA].fp0 * hCPU->fpr[frC].fp0 + hCPU->fpr[frB].fp0);
	//float s1 = (float)(hCPU->fpr[frA].fp1 * hCPU->fpr[frC].fp0 + hCPU->fpr[frB].fp1);
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// we need a temporary register to store frC.fp0 in low and high half
	IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterTemp, fprRegisterC);
	// if frD == frA and frD != frB we can multiply frD immediately and safe a copy instruction
	if( frD == frA && frD != frB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterD, fprRegisterTemp);
		// add frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_PAIR, fprRegisterD, fprRegisterB);
	}
	else
	{
		// we multiply temporary by frA
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterTemp, fprRegisterA);
		// add frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_PAIR, fprRegisterTemp, fprRegisterB);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_MADDS1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	//float s0 = (float)(hCPU->fpr[frA].fp0 * hCPU->fpr[frC].fp1 + hCPU->fpr[frB].fp0);
	//float s1 = (float)(hCPU->fpr[frA].fp1 * hCPU->fpr[frC].fp1 + hCPU->fpr[frB].fp1);
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// we need a temporary register to store frC.fp1 in bottom and top half
	IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP, fprRegisterTemp, fprRegisterC);
	// if frD == frA and frD != frB we can multiply frD immediately and safe a copy instruction
	if( frD == frA && frD != frB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterD, fprRegisterTemp);
		// add frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_PAIR, fprRegisterD, fprRegisterB);
	}
	else
	{
		// we multiply temporary by frA
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterTemp, fprRegisterA);
		// add frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_PAIR, fprRegisterTemp, fprRegisterB);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_ADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	//hCPU->fpr[frD].fp0 = hCPU->fpr[frA].fp0 + hCPU->fpr[frB].fp0;
	//hCPU->fpr[frD].fp1 = hCPU->fpr[frA].fp1 + hCPU->fpr[frB].fp1;
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	if( frD == frA )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_PAIR, fprRegisterD, fprRegisterB);
	}
	else if( frD == frB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_PAIR, fprRegisterD, fprRegisterA);
	}
	else
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterA);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_PAIR, fprRegisterD, fprRegisterB);
	}
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_SUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	//hCPU->fpr[frD].fp0 = hCPU->fpr[frA].fp0 - hCPU->fpr[frB].fp0;
	//hCPU->fpr[frD].fp1 = hCPU->fpr[frA].fp1 - hCPU->fpr[frB].fp1;
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_SUB_PAIR, fprRegisterD, fprRegisterA, fprRegisterB);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_MUL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frC;
	frC = (opcode >> 6) & 0x1F;
	frA = (opcode >> 16) & 0x1F;
	frD = (opcode >> 21) & 0x1F;
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frA);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frD);
	// we need a temporary register
	IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0 + 0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterTemp, fprRegisterC);
	// todo-optimize: This instruction can be optimized so that it doesn't always use a temporary register
	// if frD == frA we can multiply frD immediately and safe a copy instruction
	if (frD == frA)
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	else
	{
		// we multiply temporary by frA
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterTemp, fprRegisterA);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_DIV(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	frB = (opcode >> 11) & 0x1F;
	frA = (opcode >> 16) & 0x1F;
	frD = (opcode >> 21) & 0x1F;
	//hCPU->fpr[frD].fp0 = hCPU->fpr[frA].fp0 / hCPU->fpr[frB].fp0;
	//hCPU->fpr[frD].fp1 = hCPU->fpr[frA].fp1 / hCPU->fpr[frB].fp1;
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frD);
	// todo-optimize: This instruction can be optimized so that it doesn't always use a temporary register
	// if frD == frA we can divide frD immediately and safe a copy instruction
	if (frD == frA)
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE_PAIR, fprRegisterD, fprRegisterB);
	}
	else
	{
		// we need a temporary register
		IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0 + 0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterTemp, fprRegisterA);
		// we divide temporary by frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE_PAIR, fprRegisterTemp, fprRegisterB);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_MADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	//float s0 = (float)(hCPU->fpr[frA].fp0 * hCPU->fpr[frC].fp0 + hCPU->fpr[frB].fp0);
	//float s1 = (float)(hCPU->fpr[frA].fp1 * hCPU->fpr[frC].fp1 + hCPU->fpr[frB].fp1);

	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// we need a temporary register to store frC.fp0 in low and high half
	IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterTemp, fprRegisterC);
	// todo-optimize: This instruction can be optimized so that it doesn't always use a temporary register
	// if frD == frA and frD != frB we can multiply frD immediately and save a copy instruction
	if( frD == frA && frD != frB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterD, fprRegisterTemp);
		// add frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_PAIR, fprRegisterD, fprRegisterB);
	}
	else
	{
		// we multiply temporary by frA
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterTemp, fprRegisterA);
		// add frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_PAIR, fprRegisterTemp, fprRegisterB);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_NMADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// we need a temporary register to store frC.fp0 in low and high half
	IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterTemp, fprRegisterC);
	// todo-optimize: This instruction can be optimized so that it doesn't always use a temporary register
	// if frD == frA and frD != frB we can multiply frD immediately and safe a copy instruction
	if( frD == frA && frD != frB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterD, fprRegisterTemp);
		// add frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_PAIR, fprRegisterD, fprRegisterB);
	}
	else
	{
		// we multiply temporary by frA
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterTemp, fprRegisterA);
		// add frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD_PAIR, fprRegisterTemp, fprRegisterB);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	// negate
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_NEGATE_PAIR, fprRegisterD, fprRegisterD);
	// adjust accuracy
	//PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	// Splatoon requires that we emulate flush-to-denormals for this instruction
	//ppcImlGenContext->emitInst().make_fpr_r(NULL,PPCREC_IML_OP_FPR_ROUND_FLDN_TO_SINGLE_PRECISION_PAIR, fprRegisterD, false);
	return true;
}

bool PPCRecompilerImlGen_PS_MSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	//hCPU->fpr[frD].fp0 = (hCPU->fpr[frA].fp0 * hCPU->fpr[frC].fp0 - hCPU->fpr[frB].fp0);
	//hCPU->fpr[frD].fp1 = (hCPU->fpr[frA].fp1 * hCPU->fpr[frC].fp1 - hCPU->fpr[frB].fp1);

	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// we need a temporary register to store frC.fp0 in low and high half
	IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterTemp, fprRegisterC);
	// todo-optimize: This instruction can be optimized so that it doesn't always use a temporary register
	// if frD == frA and frD != frB we can multiply frD immediately and safe a copy instruction
	if( frD == frA && frD != frB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterD, fprRegisterTemp);
		// sub frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB_PAIR, fprRegisterD, fprRegisterB);
	}
	else
	{
		// we multiply temporary by frA
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterTemp, fprRegisterA);
		// sub frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB_PAIR, fprRegisterTemp, fprRegisterB);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_NMSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	// we need a temporary register to store frC.fp0 in low and high half
	IMLReg fprRegisterTemp = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0+0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterTemp, fprRegisterC);
	// todo-optimize: This instruction can be optimized so that it doesn't always use a temporary register
	// if frD == frA and frD != frB we can multiply frD immediately and safe a copy instruction
	if( frD == frA && frD != frB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterD, fprRegisterTemp);
		// sub frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB_PAIR, fprRegisterD, fprRegisterB);
	}
	else
	{
		// we multiply temporary by frA
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY_PAIR, fprRegisterTemp, fprRegisterA);
		// sub frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB_PAIR, fprRegisterTemp, fprRegisterB);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterTemp);
	}
	// negate result
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_NEGATE_PAIR, fprRegisterD, fprRegisterD);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_SUM0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	//float s0 = (float)(hCPU->fpr[frA].fp0 + hCPU->fpr[frB].fp1);
	//float s1 = (float)hCPU->fpr[frC].fp1;
	//hCPU->fpr[frD].fp0 = s0;
	//hCPU->fpr[frD].fp1 = s1;
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_fpr_r_r_r_r(PPCREC_IML_OP_FPR_SUM0, fprRegisterD, fprRegisterA, fprRegisterB, fprRegisterC);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_SUM1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	//float s0 = (float)hCPU->fpr[frC].fp0;
	//float s1 = (float)(hCPU->fpr[frA].fp0 + hCPU->fpr[frB].fp1);
	//hCPU->fpr[frD].fp0 = s0;
	//hCPU->fpr[frD].fp1 = s1;
	// load registers
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_fpr_r_r_r_r(PPCREC_IML_OP_FPR_SUM1, fprRegisterD, fprRegisterA, fprRegisterB, fprRegisterC);
	// adjust accuracy
	PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	return true;
}

bool PPCRecompilerImlGen_PS_NEG(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frB;
	frB = (opcode>>11)&0x1F;
	frD = (opcode>>21)&0x1F;
	//hCPU->fpr[frD].fp0 = -hCPU->fpr[frB].fp0;
	//hCPU->fpr[frD].fp1 = -hCPU->fpr[frB].fp1;
	// load registers
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_NEGATE_PAIR, fprRegisterD, fprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_PS_ABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frB;
	frB = (opcode>>11)&0x1F;
	frD = (opcode>>21)&0x1F;
	// load registers
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ABS_PAIR, fprRegisterD, fprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_PS_RES(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frB;
	frB = (opcode>>11)&0x1F;
	frD = (opcode>>21)&0x1F;
	//hCPU->fpr[frD].fp0 = (float)(1.0f / (float)hCPU->fpr[frB].fp0);
	//hCPU->fpr[frD].fp1 = (float)(1.0f / (float)hCPU->fpr[frB].fp1);

	// load registers
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);

	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_FRES_PAIR, fprRegisterD, fprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_PS_RSQRTE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frB;
	frB = (opcode>>11)&0x1F;
	frD = (opcode>>21)&0x1F;
	//hCPU->fpr[frD].fp0 = (float)(1.0f / (float)sqrt(hCPU->fpr[frB].fp0));
	//hCPU->fpr[frD].fp1 = (float)(1.0f / (float)sqrt(hCPU->fpr[frB].fp1));

	// load registers
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_FRSQRTE_PAIR, fprRegisterD, fprRegisterB);
	return true;
}

bool PPCRecompilerImlGen_PS_MR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frB;
	frB = (opcode>>11)&0x1F;
	frD = (opcode>>21)&0x1F;
	//hCPU->fpr[frD].fp0 = hCPU->fpr[frB].fp0;
	//hCPU->fpr[frD].fp1 = hCPU->fpr[frB].fp1;
	// load registers
	if( frB != frD )
	{
		IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
		IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_PAIR, fprRegisterD, fprRegisterB);
	}
	return true;
}

bool PPCRecompilerImlGen_PS_SEL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterC = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frC);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	ppcImlGenContext->emitInst().make_fpr_r_r_r_r(PPCREC_IML_OP_FPR_SELECT_PAIR, fprRegisterD, fprRegisterA, fprRegisterB, fprRegisterC);
	return true;
}

bool PPCRecompilerImlGen_PS_MERGE00(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	//float s0 = (float)hCPU->fpr[frA].fp0;
	//float s1 = (float)hCPU->fpr[frB].fp0;
	//hCPU->fpr[frD].fp0 = s0;
	//hCPU->fpr[frD].fp1 = s1;
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);

	if( frA == frB )
	{
		// simply duplicate bottom into bottom and top of destination register
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterA);
	}
	else
	{
		// copy bottom of frB to top first
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_TOP, fprRegisterD, fprRegisterB);
		// copy bottom of frA
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterA);
	}
	return true;
}

bool PPCRecompilerImlGen_PS_MERGE01(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	// hCPU->fpr[frD].fp0 = hCPU->fpr[frA].fp0;
	// hCPU->fpr[frD].fp1 = hCPU->fpr[frB].fp1;
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);

	if( fprRegisterD != fprRegisterB )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_TOP_TO_TOP, fprRegisterD, fprRegisterB);
	if( fprRegisterD != fprRegisterA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterA);

	return true;
}

bool PPCRecompilerImlGen_PS_MERGE10(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	if( frA == frB )
	{
		// swap bottom and top
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED, fprRegisterD, fprRegisterA);
	}
	else if( frA == frD )
	{
		// copy frB bottom to frD bottom
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterB);
		// swap lower and upper half of frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED, fprRegisterD, fprRegisterD);
	}
	else if( frB == frD )
	{
		// copy upper half of frA to upper half of frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_TOP_TO_TOP, fprRegisterD, fprRegisterA);
		// swap lower and upper half of frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED, fprRegisterD, fprRegisterD);
	}
	else
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterA);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM, fprRegisterD, fprRegisterB);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED, fprRegisterD, fprRegisterD);
	}
	return true;
}

bool PPCRecompilerImlGen_PS_MERGE11(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	IMLReg fprRegisterD = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frD);
	if( fprRegisterA == fprRegisterB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterA);
	}
	else if( fprRegisterD != fprRegisterB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP, fprRegisterD, fprRegisterA);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_TOP_TO_TOP, fprRegisterD, fprRegisterB);
	}
	else if( fprRegisterD == fprRegisterB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM, fprRegisterD, fprRegisterA);
	}
	else
	{
		debugBreakpoint();
		return false;
	}
	return true;
}

bool PPCRecompilerImlGen_PS_CMPO0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	printf("PS_CMPO0: Not implemented\n");
	return false;

	sint32 crfD, frA, frB;
	uint32 c=0;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	crfD = (opcode>>23)&0x7;

	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0+frB);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_FCMPO_BOTTOM, fprRegisterA, fprRegisterB, crfD);
	return true;
}

bool PPCRecompilerImlGen_PS_CMPU0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	printf("PS_CMPU0: Not implemented\n");
	return false;

	sint32 crfD, frA, frB;
	frB = (opcode >> 11) & 0x1F;
	frA = (opcode >> 16) & 0x1F;
	crfD = (opcode >> 23) & 0x7;
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frB);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_FCMPU_BOTTOM, fprRegisterA, fprRegisterB, crfD);
	return true;
}

bool PPCRecompilerImlGen_PS_CMPU1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	printf("PS_CMPU1: Not implemented\n");
	return false;

	sint32 crfD, frA, frB;
	frB = (opcode >> 11) & 0x1F;
	frA = (opcode >> 16) & 0x1F;
	crfD = (opcode >> 23) & 0x7;
	IMLReg fprRegisterA = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frA);
	IMLReg fprRegisterB = PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext, PPCREC_NAME_FPR0 + frB);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_FCMPU_TOP, fprRegisterA, fprRegisterB, crfD);
	return true;
}