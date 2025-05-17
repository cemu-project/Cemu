#include "Cafe/HW/Espresso/EspressoISA.h"
#include "../Interpreter/PPCInterpreterInternal.h"
#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "Cafe/GameProfile/GameProfile.h"
#include "IML/IML.h"

ATTR_MS_ABI double frsqrte_espresso(double input);
ATTR_MS_ABI double fres_espresso(double input);

IMLReg _GetRegCR(ppcImlGenContext_t* ppcImlGenContext, uint8 crReg, uint8 crBit);

#define DefinePS0(name, regIndex) IMLReg name = _GetFPRRegPS0(ppcImlGenContext, regIndex);
#define DefinePS1(name, regIndex) IMLReg name = _GetFPRRegPS1(ppcImlGenContext, regIndex);
#define DefinePSX(name, regIndex, isPS1) IMLReg name = isPS1 ? _GetFPRRegPS1(ppcImlGenContext, regIndex) : _GetFPRRegPS0(ppcImlGenContext, regIndex);
#define DefineTempFPR(name, index) IMLReg name = _GetFPRTemp(ppcImlGenContext, index);

IMLReg _GetFPRRegPS0(ppcImlGenContext_t* ppcImlGenContext, uint32 regIndex)
{
	cemu_assert_debug(regIndex < 32);
	return PPCRecompilerImlGen_LookupReg(ppcImlGenContext, PPCREC_NAME_FPR_HALF + regIndex * 2 + 0, IMLRegFormat::F64);
}

IMLReg _GetFPRRegPS1(ppcImlGenContext_t* ppcImlGenContext, uint32 regIndex)
{
	cemu_assert_debug(regIndex < 32);
	return PPCRecompilerImlGen_LookupReg(ppcImlGenContext, PPCREC_NAME_FPR_HALF + regIndex * 2 + 1, IMLRegFormat::F64);
}

IMLReg _GetFPRTemp(ppcImlGenContext_t* ppcImlGenContext, uint32 index)
{
	cemu_assert_debug(index < 4);
	return PPCRecompilerImlGen_LookupReg(ppcImlGenContext, PPCREC_NAME_TEMPORARY_FPR0 + index, IMLRegFormat::F64);
}

IMLReg _GetFPRReg(ppcImlGenContext_t* ppcImlGenContext, uint32 regIndex, bool selectPS1)
{
	cemu_assert_debug(regIndex < 32);
	return PPCRecompilerImlGen_LookupReg(ppcImlGenContext, PPCREC_NAME_FPR_HALF + regIndex * 2 + (selectPS1 ? 1 : 0), IMLRegFormat::F64);
}

void PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext_t* ppcImlGenContext, IMLReg fprRegister, bool flushDenormals=false)
{
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM, fprRegister);
	if( flushDenormals )
		assert_dbg();
}

bool PPCRecompilerImlGen_LFS_LFSU_LFD_LFDU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withUpdate, bool isDouble)
{
	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, imm);
	IMLReg gprRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	if (withUpdate)
	{
		// add imm to memory register
		cemu_assert_debug(rA != 0);
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
		imm = 0; // set imm to 0 so we dont add it twice
	}
	DefinePS0(fpPs0, frD);
	if (isDouble)
	{
		// LFD/LFDU
		ppcImlGenContext->emitInst().make_fpr_r_memory(fpPs0, gprRegister, imm, PPCREC_FPR_LD_MODE_DOUBLE, true);
	}
	else
	{
		// LFS/LFSU
		ppcImlGenContext->emitInst().make_fpr_r_memory(fpPs0, gprRegister, imm, PPCREC_FPR_LD_MODE_SINGLE, true);
		if( ppcImlGenContext->LSQE )
		{
			DefinePS1(fpPs1, frD);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fpPs1, fpPs0);
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
		ppcImlGenContext->emitInst().make_r_r_r(PPCREC_IML_OP_ADD, gprRegister1, gprRegister1, gprRegister2);
	DefinePS0(fpPs0, frD);
	if (isDouble)
	{
		if (withUpdate)
			ppcImlGenContext->emitInst().make_fpr_r_memory(fpPs0, gprRegister1, 0, PPCREC_FPR_LD_MODE_DOUBLE, true);
		else
			ppcImlGenContext->emitInst().make_fpr_r_memory_indexed(fpPs0, gprRegister1, gprRegister2, PPCREC_FPR_LD_MODE_DOUBLE, true);
	}
	else
	{
		if (withUpdate)
			ppcImlGenContext->emitInst().make_fpr_r_memory( fpPs0, gprRegister1, 0, PPCREC_FPR_LD_MODE_SINGLE, true);
		else
			ppcImlGenContext->emitInst().make_fpr_r_memory_indexed( fpPs0, gprRegister1, gprRegister2, PPCREC_FPR_LD_MODE_SINGLE, true);
		if( ppcImlGenContext->LSQE )
		{
			DefinePS1(fpPs1, frD);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fpPs1, fpPs0);
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
	DefinePS0(fpPs0, frD);
	if (withUpdate)
	{
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprRegister, gprRegister, (sint32)imm);
		imm = 0;
	}
	if (isDouble)
		ppcImlGenContext->emitInst().make_fpr_memory_r(fpPs0, gprRegister, imm, PPCREC_FPR_ST_MODE_DOUBLE, true);
	else
		ppcImlGenContext->emitInst().make_fpr_memory_r(fpPs0, gprRegister, imm, PPCREC_FPR_ST_MODE_SINGLE, true);
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
	DefinePS0(fpPs0, frS);
	auto mode = isDouble ? PPCREC_FPR_ST_MODE_DOUBLE : PPCREC_FPR_ST_MODE_SINGLE;
	if( ppcImlGenContext->LSQE )
	{
		if (hasUpdate)
			ppcImlGenContext->emitInst().make_fpr_memory_r(fpPs0, gprRegister1, 0, mode, true);
		else
			ppcImlGenContext->emitInst().make_fpr_memory_r_indexed(fpPs0, gprRegister1, gprRegister2, 0, mode, true);
	}
	else
	{
		if (hasUpdate)
			ppcImlGenContext->emitInst().make_fpr_memory_r(fpPs0, gprRegister1, 0, mode, true);
		else
			ppcImlGenContext->emitInst().make_fpr_memory_r_indexed(fpPs0, gprRegister1, gprRegister2, 0, mode, true);
	}
	return true;
}

bool PPCRecompilerImlGen_STFIWX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 rA, frS, rB;
	PPC_OPC_TEMPL_X(opcode, frS, rA, rB);
	DefinePS0(fpPs0, frS);
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
	if( rA != 0 )
		ppcImlGenContext->emitInst().make_fpr_memory_r_indexed(fpPs0, gprRegister1, gprRegister2, 0, PPCREC_FPR_ST_MODE_UI32_FROM_PS0, true);
	else
		ppcImlGenContext->emitInst().make_fpr_memory_r(fpPs0, gprRegister1, 0, PPCREC_FPR_ST_MODE_UI32_FROM_PS0, true);
	return true;
}

bool PPCRecompilerImlGen_FADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	PPC_ASSERT(frC==0);
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_ADD, fprD, fprA, fprB);
	return true;
}

bool PPCRecompilerImlGen_FSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	PPC_ASSERT(frC==0);
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_SUB, fprD, fprA, fprB);
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
	DefinePS0(fprA, frA);
	DefinePS0(fprC, frC);
	DefinePS0(fprD, frD);
	// move frA to frD (if different register)
	if( frD != frA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprA);
	// multiply bottom double of frD with bottom double of frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprD, fprC);
	return true;
}

bool PPCRecompilerImlGen_FDIV(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC_unused;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC_unused);
	PPC_ASSERT(frB==0);
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	if( frB == frD && frA != frB )
	{
		DefineTempFPR(fprTemp, 0);
		// move frA to temporary register
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp, fprA);
		// divide bottom double of temporary register by bottom double of frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE, fprTemp, fprB);
		// move result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprTemp);
		return true;
	}
	// move frA to frD (if different register)
	if( frD != frA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprA); // copy ps0
	// divide bottom double of frD by bottom double of frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE, fprD, fprB);
	return true;
}

bool PPCRecompilerImlGen_FMADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprC, frC);
	DefinePS0(fprD, frD);
	// if frB is already in frD we need a temporary register to store the product of frA*frC
	if( frB == frD )
	{
		DefineTempFPR(fprTemp, 0);
		// move frA to temporary register
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp, fprA);
		// multiply bottom double of temporary register with bottom double of frC
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTemp, fprC);
		// add result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprD, fprTemp);
		return true;
	}
	// if frC == frD -> swap registers, we assume that frC != frD
	if( frD == frC )
	{
		// swap frA and frC
		IMLReg temp = fprA;
		fprA = fprC;
		fprC = temp;
	}
	// move frA to frD (if different register)
	if( frD != frA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprA); // always copy ps0 and ps1
	// multiply bottom double of frD with bottom double of frC
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprD, fprC);
	// add frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprD, fprB);
	return true;
}

bool PPCRecompilerImlGen_FMSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprC, frC);
	DefinePS0(fprD, frD);
	if( frB == frD )
	{
		// if frB is already in frD we need a temporary register to store the product of frA*frC
		DefineTempFPR(fprTemp, 0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp, fprA);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTemp, fprC);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprTemp, fprB);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprTemp);
		return false;
	}
	if( frD == frC )
	{
		// swap frA and frC
		IMLReg temp = fprA;
		fprA = fprC;
		fprC = temp;
	}
	// move frA to frD
	if( frD != frA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprA);
	// multiply bottom double of frD with bottom double of frC
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprD, fprC);
	// sub frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprD, fprB);
	return true;
}

bool PPCRecompilerImlGen_FNMSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprC, frC);
	DefinePS0(fprD, frD);
	// if frB is already in frD we need a temporary register to store the product of frA*frC
	if( frB == frD )
	{
		DefineTempFPR(fprTemp, 0);
		// move frA to temporary register
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp, fprA);
		// multiply bottom double of temporary register with bottom double of frC
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTemp, fprC);
		// sub frB from temporary register
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprTemp, fprB);
		// negate result
		ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE, fprTemp);
		// move result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprTemp);
		return true;
	}
	// if frC == frD -> swap registers, we assume that frC != frD
	if( frD == frC )
	{
		// swap frA and frC
		IMLReg temp = fprA;
		fprA = fprC;
		fprC = temp;
	}
	// move frA to frD (if different register)
	if( frD != frA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprA);
	// multiply bottom double of frD with bottom double of frC
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprD, fprC);
	// sub frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprD, fprB);
	// negate result
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE, fprD);
	return true;
}

#define PSE_CopyResultToPs1() 	if( ppcImlGenContext->PSE ) \
								{ \
									DefinePS1(fprDPS1, frD); \
									ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDPS1, fprD); \
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
	DefinePS0(fprA, frA);
	DefinePS0(fprC, frC);
	DefinePS0(fprD, frD);
	// move frA to frD (if different register)
	if( frD != frA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprA);
	// multiply bottom double of frD with bottom double of frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprD, fprC);
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprD);
	// if paired single mode, copy frD ps0 to ps1
	PSE_CopyResultToPs1();
	return true;
}

bool PPCRecompilerImlGen_FDIVS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC_unused;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC_unused);
	PPC_ASSERT(frB==0);
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	if( frB == frD && frA != frB )
	{
		DefineTempFPR(fprTemp, 0);
		// move frA to temporary register
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp, fprA);
		// divide bottom double of temporary register by bottom double of frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE, fprTemp, fprB);
		// move result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprTemp);
		// adjust accuracy
		PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprD);
		PSE_CopyResultToPs1();
		return true;
	}
	// move frA to frD (if different register)
	if( frD != frA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprA);
	// subtract bottom double of frB from bottom double of frD
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE, fprD, fprB);
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprD);
	PSE_CopyResultToPs1();
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
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	// move frA to frD (if different register)
	if( frD != frA )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprA);
	// add bottom double of frD and bottom double of frB
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprD, fprB);
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprD);
	PSE_CopyResultToPs1();
	return true;
}

bool PPCRecompilerImlGen_FSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	PPC_ASSERT(frB==0);
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_SUB, fprD, fprA, fprB);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprD);
	PSE_CopyResultToPs1();
	return true;
}

bool PPCRecompilerImlGen_FMADDS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprC, frC);
	DefinePS0(fprD, frD);
	// if none of the operand registers overlap with the result register then we can avoid the usage of a temporary register
	IMLReg fprRegisterTemp;
	if( frD != frA && frD != frB && frD != frC )
		fprRegisterTemp = fprD;
	else
		fprRegisterTemp = _GetFPRTemp(ppcImlGenContext, 0);
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprRegisterTemp, fprA, fprC);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprRegisterTemp, fprB);
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprRegisterTemp);
	// set result
	if( fprD != fprRegisterTemp )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprRegisterTemp);
	}
	PSE_CopyResultToPs1();
	return true;
}

bool PPCRecompilerImlGen_FMSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprC, frC);
	DefinePS0(fprD, frD);

	IMLReg fprRegisterTemp;
	// if none of the operand registers overlap with the result register then we can avoid the usage of a temporary register
	if( frD != frA && frD != frB && frD != frC )
		fprRegisterTemp = fprD;
	else
		fprRegisterTemp = _GetFPRTemp(ppcImlGenContext, 0);
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprRegisterTemp, fprA, fprC);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprRegisterTemp, fprB);
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprRegisterTemp);
	// set result
	if( fprD != fprRegisterTemp )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprRegisterTemp);
	}
	PSE_CopyResultToPs1();
	return true;
}

bool PPCRecompilerImlGen_FNMSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprC, frC);
	DefinePS0(fprD, frD);
	IMLReg fprRegisterTemp;
	// if none of the operand registers overlap with the result register then we can avoid the usage of a temporary register
	if( frD != frA && frD != frB && frD != frC )
		fprRegisterTemp = fprD;
	else
		fprRegisterTemp = _GetFPRTemp(ppcImlGenContext, 0);
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprRegisterTemp, fprA, fprC);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprRegisterTemp, fprB);
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE, fprRegisterTemp);
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprRegisterTemp);
	// set result
	if( fprD != fprRegisterTemp )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprRegisterTemp);
	PSE_CopyResultToPs1();
	return true;
}

bool PPCRecompilerImlGen_FCMPO(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// Not implemented
	return false;
}

bool PPCRecompilerImlGen_FCMPU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 crfD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, crfD, frA, frB);
	crfD >>= 2;
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);

	IMLReg crBitRegLT = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_LT);
	IMLReg crBitRegGT = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_GT);
	IMLReg crBitRegEQ = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_EQ);
	IMLReg crBitRegSO = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_SO);

	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegLT, IMLCondition::UNORDERED_LT);
	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegGT, IMLCondition::UNORDERED_GT);
	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegEQ, IMLCondition::UNORDERED_EQ);
	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegSO, IMLCondition::UNORDERED_U);

	// todo: set fpscr

	return true;
}

bool PPCRecompilerImlGen_FMR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, rA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, rA, frB);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprB);
	return true;
}

bool PPCRecompilerImlGen_FABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	if( frD != frB )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprB);
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_ABS, fprD);
	return true;
}

bool PPCRecompilerImlGen_FNABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	if( frD != frB )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprB);
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATIVE_ABS, fprD);
	return true;
}

bool PPCRecompilerImlGen_FRES(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	ppcImlGenContext->emitInst().make_call_imm((uintptr_t)fres_espresso, fprB, IMLREG_INVALID, IMLREG_INVALID, fprD);
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprD);
	PSE_CopyResultToPs1();
	return true;
}

bool PPCRecompilerImlGen_FRSP(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	if( fprD != fprB )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprB);
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM, fprD);
	PSE_CopyResultToPs1();
	return true;
}

bool PPCRecompilerImlGen_FNEG(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	if( opcode&PPC_OPC_RC )
		return false;
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	if( frD != frB )
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprD, fprB);
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE, fprD);
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
	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);
	DefinePS0(fprC, frC);
	DefinePS0(fprD, frD);
	ppcImlGenContext->emitInst().make_fpr_r_r_r_r(PPCREC_IML_OP_FPR_SELECT, fprD, fprA, fprB, fprC);
	return true;
}

bool PPCRecompilerImlGen_FRSQRTE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(opcode, frD, frA, frB, frC);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	ppcImlGenContext->emitInst().make_call_imm((uintptr_t)frsqrte_espresso, fprB, IMLREG_INVALID, IMLREG_INVALID, fprD);
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprD);
	return true;
}

bool PPCRecompilerImlGen_FCTIWZ(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	PPC_OPC_TEMPL_X(opcode, frD, frA, frB);
	DefinePS0(fprB, frB);
	DefinePS0(fprD, frD);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_FCTIWZ, fprD, fprB);
	return true;
}

bool PPCRecompiler_isUGQRValueKnown(ppcImlGenContext_t* ppcImlGenContext, sint32 gqrIndex, uint32& gqrValue);

void PPCRecompilerImlGen_ClampInteger(ppcImlGenContext_t* ppcImlGenContext, IMLReg reg, sint32 clampMin, sint32 clampMax)
{
	IMLReg regTmpCondBool = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 1);
	// min(reg, clampMax)
	ppcImlGenContext->emitInst().make_compare_s32(reg, clampMax, regTmpCondBool, IMLCondition::SIGNED_GT);
	ppcImlGenContext->emitInst().make_conditional_jump(regTmpCondBool, false); // condition needs to be inverted because we skip if the condition is true
	PPCIMLGen_CreateSegmentBranchedPath(*ppcImlGenContext, *ppcImlGenContext->currentBasicBlock,
		[&](ppcImlGenContext_t& genCtx)
		{
			/* branch not taken */
			genCtx.emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, reg, clampMax);
		}
	);
	// max(reg, clampMin)
	ppcImlGenContext->emitInst().make_compare_s32(reg, clampMin, regTmpCondBool, IMLCondition::SIGNED_LT);
	ppcImlGenContext->emitInst().make_conditional_jump(regTmpCondBool, false);
	PPCIMLGen_CreateSegmentBranchedPath(*ppcImlGenContext, *ppcImlGenContext->currentBasicBlock,
		[&](ppcImlGenContext_t& genCtx)
		{
			/* branch not taken */
			genCtx.emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, reg, clampMin);
		}
	);
}

void PPCRecompilerIMLGen_GetPSQScale(ppcImlGenContext_t* ppcImlGenContext, IMLReg gqrRegister, IMLReg fprRegScaleOut, bool isLoad)
{
	IMLReg gprTmp2 = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 2);
	// extract scale factor and sign extend it
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_LEFT_SHIFT, gprTmp2, gqrRegister, 32 - ((isLoad ? 24 : 8)+7));
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_RIGHT_SHIFT_S, gprTmp2, gprTmp2, (32-23)-7);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, gprTmp2, gprTmp2, 0x1FF<<23);
	if (isLoad)
		ppcImlGenContext->emitInst().make_r_r(PPCREC_IML_OP_NEG, gprTmp2, gprTmp2);
	ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprTmp2, gprTmp2, 0x7F<<23);
	// gprTmp2 now holds the scale float bits, bitcast to float
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_BITCAST_INT_TO_FLOAT, fprRegScaleOut, gprTmp2);
}

void PPCRecompilerImlGen_EmitPSQLoadCase(ppcImlGenContext_t* ppcImlGenContext, sint32 gqrIndex, Espresso::PSQ_LOAD_TYPE loadType, bool readPS1, IMLReg gprA, sint32 imm, IMLReg fprDPS0, IMLReg fprDPS1)
{
	if (loadType == Espresso::PSQ_LOAD_TYPE::TYPE_F32)
	{
		ppcImlGenContext->emitInst().make_fpr_r_memory(fprDPS0, gprA, imm, PPCREC_FPR_LD_MODE_SINGLE, true);
		if(readPS1)
		{
			ppcImlGenContext->emitInst().make_fpr_r_memory(fprDPS1, gprA, imm + 4, PPCREC_FPR_LD_MODE_SINGLE, true);
		}
	}
	if (loadType == Espresso::PSQ_LOAD_TYPE::TYPE_U16 || loadType == Espresso::PSQ_LOAD_TYPE::TYPE_S16)
	{
		// get scale factor
		IMLReg gqrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_UGQR0 + gqrIndex);
		IMLReg fprScaleReg = _GetFPRTemp(ppcImlGenContext, 2);
		PPCRecompilerIMLGen_GetPSQScale(ppcImlGenContext, gqrRegister, fprScaleReg, true);

		bool isSigned = (loadType == Espresso::PSQ_LOAD_TYPE::TYPE_S16);
		IMLReg gprTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
		ppcImlGenContext->emitInst().make_r_memory(gprTmp, gprA, imm, 16, isSigned, true);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_INT_TO_FLOAT, fprDPS0, gprTmp);

		ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDPS0, fprDPS0, fprScaleReg);

		if(readPS1)
		{
			ppcImlGenContext->emitInst().make_r_memory(gprTmp, gprA, imm + 2, 16, isSigned, true);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_INT_TO_FLOAT, fprDPS1, gprTmp);
			ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDPS1, fprDPS1, fprScaleReg);
		}
	}
	else if (loadType == Espresso::PSQ_LOAD_TYPE::TYPE_U8 || loadType == Espresso::PSQ_LOAD_TYPE::TYPE_S8)
	{
		// get scale factor
		IMLReg gqrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_UGQR0 + gqrIndex);
		IMLReg fprScaleReg = _GetFPRTemp(ppcImlGenContext, 2);
		PPCRecompilerIMLGen_GetPSQScale(ppcImlGenContext, gqrRegister, fprScaleReg, true);

		bool isSigned = (loadType == Espresso::PSQ_LOAD_TYPE::TYPE_S8);
		IMLReg gprTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
		ppcImlGenContext->emitInst().make_r_memory(gprTmp, gprA, imm, 8, isSigned, true);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_INT_TO_FLOAT, fprDPS0, gprTmp);
		ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDPS0, fprDPS0, fprScaleReg);
		if(readPS1)
		{
			ppcImlGenContext->emitInst().make_r_memory(gprTmp, gprA, imm + 1, 8, isSigned, true);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_INT_TO_FLOAT, fprDPS1, gprTmp);
			ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDPS1, fprDPS1, fprScaleReg);
		}
	}
}

// PSQ_L and PSQ_LU
bool PPCRecompilerImlGen_PSQ_L(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withUpdate)
{
	int rA, frD;
	uint32 immUnused;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, immUnused);
	sint32 gqrIndex = ((opcode >> 12) & 7);
	uint32 imm = opcode & 0xFFF;
	if (imm & 0x800)
		imm |= ~0xFFF;
	bool readPS1 = (opcode & 0x8000) == false;

	IMLReg gprA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	DefinePS0(fprDPS0, frD);
	DefinePS1(fprDPS1, frD);
	if (!readPS1)
	{
		// if PS1 is not explicitly read then set it to 1.0
		ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_LOAD_ONE, fprDPS1);
	}
	if (withUpdate)
	{
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprA, gprA, (sint32)imm);
		imm = 0;
	}
	uint32 knownGQRValue = 0;
	if ( !PPCRecompiler_isUGQRValueKnown(ppcImlGenContext, gqrIndex, knownGQRValue) )
	{
		// generate complex dynamic handler when we dont know the GQR value ahead of time
		IMLReg gqrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_UGQR0 + gqrIndex);
		IMLReg loadTypeReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
		// extract the load type from the GQR register
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_RIGHT_SHIFT_U, loadTypeReg, gqrRegister, 16);
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, loadTypeReg, loadTypeReg, 0x7);
		IMLSegment* caseSegment[6];
		sint32 compareValues[6] = {0, 4, 5, 6, 7};
		PPCIMLGen_CreateSegmentBranchedPathMultiple(*ppcImlGenContext, *ppcImlGenContext->currentBasicBlock, caseSegment, loadTypeReg, compareValues, 5, 0);
		for (sint32 i=0; i<5; i++)
		{
			IMLRedirectInstOutput outputToCase(ppcImlGenContext, caseSegment[i]); // while this is in scope, instructions go to caseSegment[i]
			PPCRecompilerImlGen_EmitPSQLoadCase(ppcImlGenContext, gqrIndex, static_cast<Espresso::PSQ_LOAD_TYPE>(compareValues[i]), readPS1, gprA, imm, fprDPS0, fprDPS1);
			// create the case jump instructions here because we need to add it last
			caseSegment[i]->AppendInstruction()->make_jump();
		}
		return true;
	}

	Espresso::PSQ_LOAD_TYPE type = static_cast<Espresso::PSQ_LOAD_TYPE>((knownGQRValue >> 0) & 0x7);
	sint32 scale = (knownGQRValue >> 8) & 0x3F;
	cemu_assert_debug(scale == 0); // known GQR values always use a scale of 0 (1.0f)
	if (scale != 0)
		return false;

	if (type == Espresso::PSQ_LOAD_TYPE::TYPE_UNUSED1 ||
		type == Espresso::PSQ_LOAD_TYPE::TYPE_UNUSED2 ||
		type == Espresso::PSQ_LOAD_TYPE::TYPE_UNUSED3)
	{
		return false;
	}

	PPCRecompilerImlGen_EmitPSQLoadCase(ppcImlGenContext, gqrIndex, type, readPS1, gprA, imm, fprDPS0, fprDPS1);
	return true;
}

void PPCRecompilerImlGen_EmitPSQStoreCase(ppcImlGenContext_t* ppcImlGenContext, sint32 gqrIndex, Espresso::PSQ_LOAD_TYPE storeType, bool storePS1, IMLReg gprA, sint32 imm, IMLReg fprDPS0, IMLReg fprDPS1)
{
	cemu_assert_debug(!storePS1 || fprDPS1.IsValid());
	if (storeType == Espresso::PSQ_LOAD_TYPE::TYPE_F32)
	{
		ppcImlGenContext->emitInst().make_fpr_memory_r(fprDPS0, gprA, imm, PPCREC_FPR_ST_MODE_SINGLE, true);
		if(storePS1)
		{
			ppcImlGenContext->emitInst().make_fpr_memory_r(fprDPS1, gprA, imm + 4, PPCREC_FPR_ST_MODE_SINGLE, true);
		}
	}
	else if (storeType == Espresso::PSQ_LOAD_TYPE::TYPE_U16 || storeType == Espresso::PSQ_LOAD_TYPE::TYPE_S16)
	{
		// get scale factor
		IMLReg gqrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_UGQR0 + gqrIndex);
		IMLReg fprScaleReg = _GetFPRTemp(ppcImlGenContext, 2);
		PPCRecompilerIMLGen_GetPSQScale(ppcImlGenContext, gqrRegister, fprScaleReg, false);

		bool isSigned = (storeType == Espresso::PSQ_LOAD_TYPE::TYPE_S16);
		IMLReg fprTmp = _GetFPRTemp(ppcImlGenContext, 0);

		IMLReg gprTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
		ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTmp, fprDPS0, fprScaleReg);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_FLOAT_TO_INT, gprTmp, fprTmp);

		if (isSigned)
			PPCRecompilerImlGen_ClampInteger(ppcImlGenContext, gprTmp, -32768, 32767);
		else
			PPCRecompilerImlGen_ClampInteger(ppcImlGenContext, gprTmp, 0, 65535);
		ppcImlGenContext->emitInst().make_memory_r(gprTmp, gprA, imm, 16, true);
		if(storePS1)
		{
			ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTmp, fprDPS1, fprScaleReg);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_FLOAT_TO_INT, gprTmp, fprTmp);
			if (isSigned)
				PPCRecompilerImlGen_ClampInteger(ppcImlGenContext, gprTmp, -32768, 32767);
			else
				PPCRecompilerImlGen_ClampInteger(ppcImlGenContext, gprTmp, 0, 65535);
			ppcImlGenContext->emitInst().make_memory_r(gprTmp, gprA, imm + 2, 16, true);
		}
	}
	else if (storeType == Espresso::PSQ_LOAD_TYPE::TYPE_U8 || storeType == Espresso::PSQ_LOAD_TYPE::TYPE_S8)
	{
		// get scale factor
		IMLReg gqrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_UGQR0 + gqrIndex);
		IMLReg fprScaleReg = _GetFPRTemp(ppcImlGenContext, 2);
		PPCRecompilerIMLGen_GetPSQScale(ppcImlGenContext, gqrRegister, fprScaleReg, false);

		bool isSigned = (storeType == Espresso::PSQ_LOAD_TYPE::TYPE_S8);
		IMLReg fprTmp = _GetFPRTemp(ppcImlGenContext, 0);
		IMLReg gprTmp = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
		ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTmp, fprDPS0, fprScaleReg);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_FLOAT_TO_INT, gprTmp, fprTmp);
		if (isSigned)
			PPCRecompilerImlGen_ClampInteger(ppcImlGenContext, gprTmp, -128, 127);
		else
			PPCRecompilerImlGen_ClampInteger(ppcImlGenContext, gprTmp, 0, 255);
		ppcImlGenContext->emitInst().make_memory_r(gprTmp, gprA, imm, 8, true);
		if(storePS1)
		{
			ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTmp, fprDPS1, fprScaleReg);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_FLOAT_TO_INT, gprTmp, fprTmp);
			if (isSigned)
				PPCRecompilerImlGen_ClampInteger(ppcImlGenContext, gprTmp, -128, 127);
			else
				PPCRecompilerImlGen_ClampInteger(ppcImlGenContext, gprTmp, 0, 255);
			ppcImlGenContext->emitInst().make_memory_r(gprTmp, gprA, imm + 1, 8, true);
		}
	}
}

// PSQ_ST and PSQ_STU
bool PPCRecompilerImlGen_PSQ_ST(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withUpdate)
{
	int rA, frD;
	uint32 immUnused;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, immUnused);
	uint32 imm = opcode & 0xFFF;
	if (imm & 0x800)
		imm |= ~0xFFF;
	sint32 gqrIndex = ((opcode >> 12) & 7);
	bool storePS1 = (opcode & 0x8000) == false;

	IMLReg gprA = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_R0+rA);
	DefinePS0(fprDPS0, frD);
	IMLReg fprDPS1 = storePS1 ? _GetFPRRegPS1(ppcImlGenContext, frD) : IMLREG_INVALID;

	if (withUpdate)
	{
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_ADD, gprA, gprA, (sint32)imm);
		imm = 0;
	}

	uint32 gqrValue = 0;
	if ( !PPCRecompiler_isUGQRValueKnown(ppcImlGenContext, gqrIndex, gqrValue) )
	{
		// generate complex dynamic handler when we dont know the GQR value ahead of time
		IMLReg gqrRegister = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_SPR0 + SPR_UGQR0 + gqrIndex);
		IMLReg loadTypeReg = PPCRecompilerImlGen_loadRegister(ppcImlGenContext, PPCREC_NAME_TEMPORARY + 0);
		// extract the load type from the GQR register
		ppcImlGenContext->emitInst().make_r_r_s32(PPCREC_IML_OP_AND, loadTypeReg, gqrRegister, 0x7);

		IMLSegment* caseSegment[5];
		sint32 compareValues[5] = {0, 4, 5, 6, 7};
		PPCIMLGen_CreateSegmentBranchedPathMultiple(*ppcImlGenContext, *ppcImlGenContext->currentBasicBlock, caseSegment, loadTypeReg, compareValues, 5, 0);
		for (sint32 i=0; i<5; i++)
		{
			IMLRedirectInstOutput outputToCase(ppcImlGenContext, caseSegment[i]); // while this is in scope, instructions go to caseSegment[i]
			PPCRecompilerImlGen_EmitPSQStoreCase(ppcImlGenContext, gqrIndex, static_cast<Espresso::PSQ_LOAD_TYPE>(compareValues[i]), storePS1, gprA, imm, fprDPS0, fprDPS1);
			ppcImlGenContext->emitInst().make_jump(); // finalize case
		}
		return true;
	}

	Espresso::PSQ_LOAD_TYPE type = static_cast<Espresso::PSQ_LOAD_TYPE>((gqrValue >> 16) & 0x7);
	sint32 scale = (gqrValue >> 24) & 0x3F;
	cemu_assert_debug(scale == 0); // known GQR values always use a scale of 0 (1.0f)

	if (type == Espresso::PSQ_LOAD_TYPE::TYPE_UNUSED1 ||
		type == Espresso::PSQ_LOAD_TYPE::TYPE_UNUSED2 ||
		type == Espresso::PSQ_LOAD_TYPE::TYPE_UNUSED3)
	{
		return false;
	}

	PPCRecompilerImlGen_EmitPSQStoreCase(ppcImlGenContext, gqrIndex, type, storePS1, gprA, imm, fprDPS0, fprDPS1);
	return true;
}

// PS_MULS0 and PS_MULS1
bool PPCRecompilerImlGen_PS_MULSX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool isVariant1)
{
	sint32 frD, frA, frC;
	frC = (opcode>>6)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	DefinePS0(fprAps0, frA);
	DefinePS1(fprAps1, frA);
	DefinePSX(fprC, frC, isVariant1);
	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);

	DefineTempFPR(fprTmp0, 0);
	DefineTempFPR(fprTmp1, 1);

	// todo - optimize cases where a temporary is not necessary
	// todo - round fprC to 25bit accuracy

	// copy ps0 and ps1 to temporary
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTmp0, fprAps0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTmp1, fprAps1);

	// multiply
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTmp0, fprC);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTmp1, fprC);

	// copy back to result
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprTmp0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprTmp1);

	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);

	return true;
}

// PS_MADDS0 and PS_MADDS1
bool PPCRecompilerImlGen_PS_MADDSX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool isVariant1)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	DefinePS0(fprAps0, frA);
	DefinePS1(fprAps1, frA);
	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);
	DefinePSX(fprC, frC, isVariant1);
	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);

	DefineTempFPR(fprTmp0, 0);
	DefineTempFPR(fprTmp1, 1);

	// todo - round C to 25bit
	// todo - optimize cases where a temporary is not necessary

	// copy ps0 and ps1 to temporary
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTmp0, fprAps0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTmp1, fprAps1);

	// multiply
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTmp0, fprC);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTmp1, fprC);

	// add
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprTmp0, fprBps0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprTmp1, fprBps1);

	// copy back to result
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprTmp0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprTmp1);

	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);
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

	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);
	DefinePS0(fprAps0, frA);
	DefinePS1(fprAps1, frA);
	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);

	if( frD == frA )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps0, fprBps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps1, fprBps1);
	}
	else if( frD == frB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps0, fprAps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps1, fprAps1);
	}
	else
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprAps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprAps1);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps0, fprBps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps1, fprBps1);
	}
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);
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

	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);
	DefinePS0(fprAps0, frA);
	DefinePS1(fprAps1, frA);
	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);

	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_SUB, fprDps0, fprAps0, fprBps0);
	ppcImlGenContext->emitInst().make_fpr_r_r_r(PPCREC_IML_OP_FPR_SUB, fprDps1, fprAps1, fprBps1);

	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);
	return true;
}

bool PPCRecompilerImlGen_PS_MUL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frC;
	frC = (opcode >> 6) & 0x1F;
	frA = (opcode >> 16) & 0x1F;
	frD = (opcode >> 21) & 0x1F;

	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);
	DefinePS0(fprAps0, frA);
	DefinePS1(fprAps1, frA);
	DefinePS0(fprCps0, frC);
	DefinePS1(fprCps1, frC);

	DefineTempFPR(fprTemp0, 0);
	DefineTempFPR(fprTemp1, 1);

	// todo: Optimize for when a temporary isnt necessary
	// todo: Round to 25bit?

	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp0, fprCps0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp1, fprCps1);
	if (frD == frA)
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps0, fprTemp0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps1, fprTemp1);
	}
	else
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTemp0, fprAps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTemp1, fprAps1);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprTemp0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprTemp1);
	}
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);
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

	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);
	DefinePS0(fprAps0, frA);
	DefinePS1(fprAps1, frA);
	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);

	if (frD == frA)
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE, fprDps0, fprBps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE, fprDps1, fprBps1);
	}
	else
	{
		DefineTempFPR(fprTemp0, 0);
		DefineTempFPR(fprTemp1, 1);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp0, fprAps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp1, fprAps1);
		// we divide temporary by frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE, fprTemp0, fprBps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_DIVIDE, fprTemp1, fprBps1);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprTemp0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprTemp1);
	}
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);
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

	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);
	DefinePS0(fprAps0, frA);
	DefinePS1(fprAps1, frA);
	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);
	DefinePS0(fprCps0, frC);
	DefinePS1(fprCps1, frC);

	if (frD != frA && frD != frB)
	{
		if (frD == frC)
		{
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprCps0, fprAps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprCps1, fprAps1);
		}
		else
		{
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprAps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprAps1);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps0, fprCps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps1, fprCps1);
		}
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps0, fprBps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps1, fprBps1);
	}
	else
	{
		DefineTempFPR(fprTemp0, 0);
		DefineTempFPR(fprTemp1, 1);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp0, fprCps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp1, fprCps1);
		if( frD == frA && frD != frB )
		{
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps0, fprTemp0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps1, fprTemp1);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps0, fprBps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps1, fprBps1);
		}
		else
		{
			// we multiply temporary by frA
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTemp0, fprAps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTemp1, fprAps1);
			// add frB
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprTemp0, fprBps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprTemp1, fprBps1);
			// copy result to frD
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprTemp0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprTemp1);
		}
	}
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);
	return true;
}

bool PPCRecompilerImlGen_PS_NMADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);
	DefinePS0(fprAps0, frA);
	DefinePS1(fprAps1, frA);
	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);
	DefinePS0(fprCps0, frC);
	DefinePS1(fprCps1, frC);

	DefineTempFPR(fprTemp0, 0);
	DefineTempFPR(fprTemp1, 1);

	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp0, fprCps0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp1, fprCps1);
	// todo-optimize: This instruction can be optimized so that it doesn't always use a temporary register
	// if frD == frA and frD != frB we can multiply frD immediately and save a copy instruction
	if( frD == frA && frD != frB )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps0, fprTemp0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps1, fprTemp1);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps0, fprBps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps1, fprBps1);
	}
	else
	{
		// we multiply temporary by frA
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTemp0, fprAps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTemp1, fprAps1);
		// add frB
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprTemp0, fprBps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprTemp1, fprBps1);
		// copy result to frD
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprTemp0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprTemp1);
	}

	// negate
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE, fprDps0);
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE, fprDps1);
	// adjust accuracy
	//PPRecompilerImmGen_optionalRoundPairFPRToSinglePrecision(ppcImlGenContext, fprRegisterD);
	// Splatoon requires that we emulate flush-to-denormals for this instruction
	//ppcImlGenContext->emitInst().make_fpr_r(NULL,PPCREC_IML_OP_FPR_ROUND_FLDN_TO_SINGLE_PRECISION_PAIR, fprRegisterD, false);
	return true;
}

// PS_MSUB and PS_NMSUB
bool PPCRecompilerImlGen_PS_MSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode, bool withNegative)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);
	DefinePS0(fprAps0, frA);
	DefinePS1(fprAps1, frA);
	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);
	DefinePS0(fprCps0, frC);
	DefinePS1(fprCps1, frC);

	if (frD != frA && frD != frB)
	{
		if (frD == frC)
		{
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprCps0, fprAps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprCps1, fprAps1);
		}
		else
		{
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprAps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprAps1);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps0, fprCps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps1, fprCps1);
		}
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprDps0, fprBps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprDps1, fprBps1);
	}
	else
	{
		DefineTempFPR(fprTemp0, 0);
		DefineTempFPR(fprTemp1, 1);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp0, fprCps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprTemp1, fprCps1);
		if( frD == frA && frD != frB )
		{
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps0, fprTemp0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprDps1, fprTemp1);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprDps0, fprBps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprDps1, fprBps1);
		}
		else
		{
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTemp0, fprAps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_MULTIPLY, fprTemp1, fprAps1);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprTemp0, fprBps0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_SUB, fprTemp1, fprBps1);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprTemp0);
			ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprTemp1);
		}
	}
	// negate result
	if (withNegative)
	{
		ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE, fprDps0);
		ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE, fprDps1);
	}
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);
	return true;
}

bool PPCRecompilerImlGen_PS_SUM0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);
	DefinePS0(fprAps0, frA);
	DefinePS1(fprBps1, frB);
	DefinePS1(fprCps1, frC);

	if( frD == frA )
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps0, fprBps1);
	}
	else
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprAps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps0, fprBps1);
	}
	if (fprDps1 != fprCps1)
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprCps1);
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);
	return true;
}

bool PPCRecompilerImlGen_PS_SUM1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB, frC;
	frC = (opcode>>6)&0x1F;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);
	DefinePS0(fprAps0, frA);
	DefinePS1(fprBps1, frB);
	DefinePS0(fprCps0, frC);

	if (frB != frD)
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprAps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps1, fprBps1);
	}
	else
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ADD, fprDps1, fprAps0);

	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprCps0);

	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);
	return true;
}

bool PPCRecompilerImlGen_PS_NEG(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frB;
	frB = (opcode>>11)&0x1F;
	frD = (opcode>>21)&0x1F;

	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);
	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);

	if (frB != frD)
	{
		// copy
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprBps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprBps1);
	}
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE, fprDps0);
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_NEGATE, fprDps1);
	return true;
}

bool PPCRecompilerImlGen_PS_ABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frB;
	frB = (opcode>>11)&0x1F;
	frD = (opcode>>21)&0x1F;

	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);
	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);

	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprBps0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprBps1);

	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_ABS, fprDps0);
	ppcImlGenContext->emitInst().make_fpr_r(PPCREC_IML_OP_FPR_ABS, fprDps1);
	return true;
}

bool PPCRecompilerImlGen_PS_RES(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frB;
	frB = (opcode>>11)&0x1F;
	frD = (opcode>>21)&0x1F;
	//hCPU->fpr[frD].fp0 = (float)(1.0f / (float)hCPU->fpr[frB].fp0);
	//hCPU->fpr[frD].fp1 = (float)(1.0f / (float)hCPU->fpr[frB].fp1);

	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);
	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);

	ppcImlGenContext->emitInst().make_call_imm((uintptr_t)fres_espresso, fprBps0, IMLREG_INVALID, IMLREG_INVALID, fprDps0);
	ppcImlGenContext->emitInst().make_call_imm((uintptr_t)fres_espresso, fprBps1, IMLREG_INVALID, IMLREG_INVALID, fprDps1);
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);
	return true;
}

bool PPCRecompilerImlGen_PS_RSQRTE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frB;
	frB = (opcode>>11)&0x1F;
	frD = (opcode>>21)&0x1F;

	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);
	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);

	ppcImlGenContext->emitInst().make_call_imm((uintptr_t)frsqrte_espresso, fprBps0, IMLREG_INVALID, IMLREG_INVALID, fprDps0);
	ppcImlGenContext->emitInst().make_call_imm((uintptr_t)frsqrte_espresso, fprBps1, IMLREG_INVALID, IMLREG_INVALID, fprDps1);
	// adjust accuracy
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps0);
	PPRecompilerImmGen_roundToSinglePrecision(ppcImlGenContext, fprDps1);
	return true;
}

bool PPCRecompilerImlGen_PS_MR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frB;
	frB = (opcode>>11)&0x1F;
	frD = (opcode>>21)&0x1F;
	if( frB != frD )
	{
		DefinePS0(fprBps0, frB);
		DefinePS1(fprBps1, frB);
		DefinePS0(fprDps0, frD);
		DefinePS1(fprDps1, frD);

		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps0, fprBps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, fprDps1, fprBps1);
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

	DefinePS0(fprAps0, frA);
	DefinePS1(fprAps1, frA);
	DefinePS0(fprBps0, frB);
	DefinePS1(fprBps1, frB);
	DefinePS0(fprCps0, frC);
	DefinePS1(fprCps1, frC);
	DefinePS0(fprDps0, frD);
	DefinePS1(fprDps1, frD);

	ppcImlGenContext->emitInst().make_fpr_r_r_r_r(PPCREC_IML_OP_FPR_SELECT, fprDps0, fprAps0, fprBps0, fprCps0);
	ppcImlGenContext->emitInst().make_fpr_r_r_r_r(PPCREC_IML_OP_FPR_SELECT, fprDps1, fprAps1, fprBps1, fprCps1);
	return true;
}

bool PPCRecompilerImlGen_PS_MERGE00(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	DefinePS0(frpAps0, frA);
	DefinePS0(frpBps0, frB);
	DefinePS0(frpDps0, frD);
	DefinePS1(frpDps1, frD);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, frpDps1, frpBps0);
	if (frpDps0 != frpAps0)
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, frpDps0, frpAps0);
	return true;
}

bool PPCRecompilerImlGen_PS_MERGE01(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;
	DefinePS0(frpAps0, frA);
	DefinePS1(frpBps1, frB);
	DefinePS0(frpDps0, frD);
	DefinePS1(frpDps1, frD);

	if (frpDps0 != frpAps0)
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, frpDps0, frpAps0);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, frpDps1, frpBps1);
	return true;
}

bool PPCRecompilerImlGen_PS_MERGE10(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	DefinePS1(frpAps1, frA);
	DefinePS0(frpBps0, frB);
	DefinePS0(frpDps0, frD);
	DefinePS1(frpDps1, frD);

	if (frD != frB)
	{
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, frpDps0, frpAps1);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, frpDps1, frpBps0);
	}
	else
	{
		DefineTempFPR(frpTemp, 0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, frpTemp, frpBps0);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, frpDps0, frpAps1);
		ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, frpDps1, frpTemp);
	}
	return true;
}

bool PPCRecompilerImlGen_PS_MERGE11(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 frD, frA, frB;
	frB = (opcode>>11)&0x1F;
	frA = (opcode>>16)&0x1F;
	frD = (opcode>>21)&0x1F;

	DefinePS1(frpAps1, frA);
	DefinePS1(frpBps1, frB);
	DefinePS0(frpDps0, frD);
	DefinePS1(frpDps1, frD);

	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, frpDps0, frpAps1);
	ppcImlGenContext->emitInst().make_fpr_r_r(PPCREC_IML_OP_FPR_ASSIGN, frpDps1, frpBps1);
	return true;
}

bool PPCRecompilerImlGen_PS_CMPO0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	// Not implemented
	return false;
}

bool PPCRecompilerImlGen_PS_CMPU0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 crfD, frA, frB;
	frB = (opcode >> 11) & 0x1F;
	frA = (opcode >> 16) & 0x1F;
	crfD = (opcode >> 23) & 0x7;

	DefinePS0(fprA, frA);
	DefinePS0(fprB, frB);

	IMLReg crBitRegLT = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_LT);
	IMLReg crBitRegGT = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_GT);
	IMLReg crBitRegEQ = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_EQ);
	IMLReg crBitRegSO = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_SO);

	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegLT, IMLCondition::UNORDERED_LT);
	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegGT, IMLCondition::UNORDERED_GT);
	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegEQ, IMLCondition::UNORDERED_EQ);
	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegSO, IMLCondition::UNORDERED_U);

	// todo: set fpscr
	return true;
}

bool PPCRecompilerImlGen_PS_CMPU1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode)
{
	sint32 crfD, frA, frB;
	frB = (opcode >> 11) & 0x1F;
	frA = (opcode >> 16) & 0x1F;
	crfD = (opcode >> 23) & 0x7;

	DefinePS1(fprA, frA);
	DefinePS1(fprB, frB);

	IMLReg crBitRegLT = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_LT);
	IMLReg crBitRegGT = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_GT);
	IMLReg crBitRegEQ = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_EQ);
	IMLReg crBitRegSO = _GetRegCR(ppcImlGenContext, crfD, Espresso::CR_BIT::CR_BIT_INDEX_SO);

	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegLT, IMLCondition::UNORDERED_LT);
	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegGT, IMLCondition::UNORDERED_GT);
	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegEQ, IMLCondition::UNORDERED_EQ);
	ppcImlGenContext->emitInst().make_fpr_compare(fprA, fprB, crBitRegSO, IMLCondition::UNORDERED_U);
	return true;
}
