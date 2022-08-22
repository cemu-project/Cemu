#pragma once

#include "Cafe/HW/Espresso/PPCState.h"

// SPR constants
#define SPR_XER		1	
#define SPR_LR		8	
#define SPR_CTR		9	
#define SPR_DEC		22	
#define SPR_SRR0	26	
#define SPR_SRR1	27	
#define SPR_HID0	1008
#define SPR_HID1	1009
#define SPR_HID2	920	
#define SPR_TBL		268	
#define SPR_TBU		269	
#define SPR_DMAU	922	
#define SPR_DMAL	923	

// graphics quantization registers
#define SPR_GQR0 912
#define SPR_GQR1 913
#define SPR_GQR2 914
#define SPR_GQR3 915
#define SPR_GQR4 916
#define SPR_GQR5 917
#define SPR_GQR6 918
#define SPR_GQR7 919

// user graphics quantization registers
#define SPR_UGQR0	896
#define SPR_UGQR1	897
#define SPR_UGQR2	898
#define SPR_UGQR3	899
#define SPR_UGQR4	900
#define SPR_UGQR5	901
#define SPR_UGQR6	902
#define SPR_UGQR7	903

#define SPR_FPECR	1022	// used by the OS to store values

#define SPR_PVR		287		// processor version, for Wii U this must be 0x7001xxxx - this register is only readable
#define SPR_UPIR	1007	// core index
#define SPR_SCR		947		// core control
#define SPR_SDR1	25

// reversed CR bit indices
#define CR_BIT_LT	0
#define CR_BIT_GT	1
#define CR_BIT_EQ	2
#define CR_BIT_SO	3

#define XER_SO			(1<<31)	// summary overflow bit
#define XER_OV			(1<<30)	// overflow bit
#define XER_BIT_CA		(29)	// carry bit index. To accelerate frequent access, this bit is stored as a separate uint8

// FPSCR
#define FPSCR_VXSNAN	(1<<24)
#define FPSCR_VXVC		(1<<19)

#define MSR_SF			(1<<31)
#define MSR_UNKNOWN		(1<<30)
#define MSR_UNKNOWN2	(1<<27)
#define MSR_VEC			(1<<25)
#define MSR_POW			(1<<18)
#define MSR_TGPR		(1<<15)
#define MSR_ILE			(1<<16)
#define MSR_EE			(1<<15)
#define MSR_PR			(1<<14)
#define MSR_FP			(1<<13)
#define MSR_ME			(1<<12)
#define MSR_FE0			(1<<11)
#define MSR_SE			(1<<10)
#define MSR_BE			(1<<9)
#define MSR_FE1			(1<<8)
#define MSR_IP			(1<<6)
#define MSR_IR			(1<<5)
#define MSR_DR			(1<<4)
#define MSR_PM			(1<<2)
#define MSR_RI			(1<<1)
#define MSR_LE			(1)

// helpers

#define GET_MSR_BIT(__bit) ((hCPU->sprExtended.msr&(__bit))!=0)

#define opHasRC() ((opcode & PPC_OPC_RC) != 0)

// assume fixed values for PSE/LSQE. This optimization is possible because Wii U applications run only in user mode (todo - handle this correctly in LLE mode)
//#define PPC_LSQE	(hCPU->LSQE)
//#define PPC_PSE	(hCPU->PSE)

#define PPC_LSQE		(1)
#define PPC_PSE			(1)

#define PPC_ASSERT(v)

#define PPC_OPC_RC		1
#define PPC_OPC_OE		(1<<10)
#define PPC_OPC_LK		1
#define PPC_OPC_AA		(1<<1)

#define PPC_OPC_TEMPL_A(opc, rD, rA, rB, rC) {rD=((opc)>>21)&0x1f;rA=((opc)>>16)&0x1f;rB=((opc)>>11)&0x1f;rC=((opc)>>6)&0x1f;}
#define PPC_OPC_TEMPL_B(opc, BO, BI, BD) {BO=((opc)>>21)&0x1f;BI=((opc)>>16)&0x1f;BD=(uint32)(sint32)(sint16)((opc)&0xfffc);}
#define PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm) {rD=((opc)>>21)&0x1f;rA=((opc)>>16)&0x1f;imm=(uint32)(sint32)(sint16)((opc)&0xffff);}
#define PPC_OPC_TEMPL_D_UImm(opc, rD, rA, imm) {rD=((opc)>>21)&0x1f;rA=((opc)>>16)&0x1f;imm=(opc)&0xffff;}
#define PPC_OPC_TEMPL_D_Shift16(opc, rD, rA, imm) {rD=((opc)>>21)&0x1f;rA=((opc)>>16)&0x1f;imm=(opc)<<16;}
#define PPC_OPC_TEMPL_I(opc, LI) {LI=(opc)&0x3fffffc;if (LI&0x02000000) LI |= 0xfc000000;}
#define PPC_OPC_TEMPL_M(opc, rS, rA, SH, MB, ME) {rS=((opc)>>21)&0x1f;rA=((opc)>>16)&0x1f;SH=((opc)>>11)&0x1f;MB=((opc)>>6)&0x1f;ME=((opc)>>1)&0x1f;}
#define PPC_OPC_TEMPL_X(opc, rS, rA, rB) {rS=((opc)>>21)&0x1f;rA=((opc)>>16)&0x1f;rB=((opc)>>11)&0x1f;}
#define PPC_OPC_TEMPL_XFX(opc, rS, CRM) {rS=((opc)>>21)&0x1f;CRM=((opc)>>12)&0xff;}
#define PPC_OPC_TEMPL_XO(opc, rS, rA, rB) {rS=((opc)>>21)&0x1f;rA=((opc)>>16)&0x1f;rB=((opc)>>11)&0x1f;}
#define PPC_OPC_TEMPL_XL(opc, BO, BI, BD) {BO=((opc)>>21)&0x1f;BI=((opc)>>16)&0x1f;BD=((opc)>>11)&0x1f;}
#define PPC_OPC_TEMPL_XFL(opc, rB, FM) {rB=((opc)>>11)&0x1f;FM=((opc)>>17)&0xff;}

#define PPC_OPC_TEMPL3_XO() sint32 rD, rA, rB; rD=((opcode)>>21)&0x1f;rA=((opcode)>>16)&0x1f;rB=((opcode)>>11)&0x1f
#define PPC_OPC_TEMPL_X_CR() sint32 crD, crA, crB; crD=((opcode)>>21)&0x1f;crA=((opcode)>>16)&0x1f;crB=((opcode)>>11)&0x1f

static inline void ppc_update_cr0(PPCInterpreter_t* hCPU, uint32 r)
{
	hCPU->cr[CR_BIT_SO] = (hCPU->spr.XER&XER_SO) ? 1 : 0;
	hCPU->cr[CR_BIT_LT] = ((r != 0) ? 1 : 0) & ((r & 0x80000000) ? 1 : 0);
	hCPU->cr[CR_BIT_EQ] = (r == 0);
	hCPU->cr[CR_BIT_GT] = hCPU->cr[CR_BIT_EQ] ^ hCPU->cr[CR_BIT_LT] ^ 1;  // this works because EQ and LT can never be set at the same time. So the only case where GT becomes 1 is when LT=0 and EQ=0
}

static inline uint8 ppc_getCRBit(PPCInterpreter_t* hCPU, uint32 r)
{
	return hCPU->cr[r];
}

static inline bool ppc_MTCRFMaskHasCRFieldSet(const uint32 mtcrfMask, const uint32 crIndex)
{
	// 1000 0000 (0x80) -> cr0
	// 0000 0001 (0x01) -> cr7
	return (mtcrfMask & (1 << (7 - crIndex))) != 0;
}

// returns CR mask with CR0.LT in LSB
static inline uint32 ppc_MTCRFMaskToCRBitMask(const uint32 mtcrfMask)
{
	uint32 crMask = 0; 
	for (uint32 crF = 0; crF < 8; crF++)
	{
		if (ppc_MTCRFMaskHasCRFieldSet(mtcrfMask, crF))
			crMask |= (0xF << (crF * 4));
	}
	return crMask;
}

static inline void ppc_setCRBit(PPCInterpreter_t* hCPU, uint32 r, uint8 v)
{
	hCPU->cr[r] = v;
}

static inline void ppc_setCR(PPCInterpreter_t* hCPU, uint32 cr)
{
	uint32 tempCr = cr;
	for (sint32 i = 31; i >= 0; i--)
	{
		ppc_setCRBit(hCPU, i, tempCr & 1);
		tempCr >>= 1;
	}
}

static inline uint32 ppc_getCR(PPCInterpreter_t* hCPU)
{
	uint32 cr = 0;
	for (sint32 i = 0; i < 32; i++)
	{
		cr <<= 1;
		if (ppc_getCRBit(hCPU, i))
			cr |= 1;
	}
	return cr;
}

// FPU helper

#define IS_NAN(X)				((((X) & 0x000fffffffffffffULL) != 0) && (((X) & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL))
#define IS_QNAN(X)				((((X) & 0x000fffffffffffffULL) != 0) && (((X) & 0x7ff8000000000000ULL) == 0x7ff8000000000000ULL))
#define IS_SNAN(X)				((((X) & 0x000fffffffffffffULL) != 0) && (((X) & 0x7ff8000000000000ULL) == 0x7ff0000000000000ULL))

#define FPSCR_VE				(1 <<  7)

inline double roundTo25BitAccuracy(double d)
{
	uint64 v = *(uint64*)&d;
	v = (v & 0xFFFFFFFFF8000000ULL) + (v & 0x8000000ULL);
	return *(double*)&v;
}

double fres_espresso(double input);
double frsqrte_espresso(double input);

void fcmpu_espresso(PPCInterpreter_t* hCPU, int crfD, double a, double b);

// OPC
void PPCInterpreter_virtualHLE(PPCInterpreter_t* hCPU, unsigned int opcode);

void PPCInterpreter_MFMSR(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_MTMSR(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_MFTB(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_MTFSB1X(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_MFCR(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_MCRF(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_MTCRF(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_MCRXR(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_TLBIE(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_TLBSYNC(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_DCBT(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_DCBST(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_DCBZL(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_DCBF(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_DCBI(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_DCBZ(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_ICBI(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_EIEIO(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_SC(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_SYNC(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_ISYNC(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_RFI(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_BX(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_BCX(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_BCLRX(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_BCCTR(PPCInterpreter_t* hCPU, uint32 Opcode);

// FPU

void PPCInterpreter_FCMPO(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FCMPU(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_FMR(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FSEL(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FCTIWZ(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FCTIW(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FNEG(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FRSP(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FRSQRTE(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FRES(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_FABS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FNABS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FADD(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FMUL(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FDIV(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FSUB(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FMADD(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FMSUB(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FMSUBS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FNMADD(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FNMSUB(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_MFFS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_MTFSF(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_FDIVS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FMULS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FADDS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FSUBS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FMADDS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FNMADDS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_FNMSUBS(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_PS_MERGE00(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_MERGE01(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_MERGE10(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_MERGE11(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_MR(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_NEG(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_ABS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_NABS(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_RES(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_RSQRTE(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_PS_ADD(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_SUB(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_MUL(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_DIV(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_PS_MADD(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_NMADD(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_MADDS0(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_MADDS1(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_MSUB(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_NMSUB(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_PS_SEL(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_SUM0(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_SUM1(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_MULS0(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_MULS1(PPCInterpreter_t* hCPU, uint32 Opcode);

void PPCInterpreter_PS_CMPO0(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_CMPU0(PPCInterpreter_t* hCPU, uint32 Opcode);
void PPCInterpreter_PS_CMPU1(PPCInterpreter_t* hCPU, uint32 Opcode);
