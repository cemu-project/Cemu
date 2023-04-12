#include "../PPCState.h"
#include "PPCInterpreterInternal.h"
#include "PPCInterpreterHelper.h"

#include<math.h>

// floating point utility

#include <limits>
#include <array>

const int ieee_double_e_bits = 11; // exponent bits
const int ieee_double_m_bits = 52; // mantissa bits

const int espresso_frsqrte_i_bits = 5; // index bits (the highest bit is the LSB of the exponent)

typedef struct
{
	uint32 offset;
	uint32 step;
}espresso_frsqrte_entry_t;

espresso_frsqrte_entry_t frsqrteLookupTable[32] =
{
	{0x1a7e800, 0x568},{0x17cb800, 0x4f3},{0x1552800, 0x48d},{0x130c000, 0x435},
	{0x10f2000, 0x3e7},{0xeff000, 0x3a2},{0xd2e000, 0x365},{0xb7c000, 0x32e},
	{0x9e5000, 0x2fc},{0x867000, 0x2d0},{0x6ff000, 0x2a8},{0x5ab800, 0x283},
	{0x46a000, 0x261},{0x339800, 0x243},{0x218800, 0x226},{0x105800, 0x20b},
	{0x3ffa000, 0x7a4},{0x3c29000, 0x700},{0x38aa000, 0x670},{0x3572000, 0x5f2},
	{0x3279000, 0x584},{0x2fb7000, 0x524},{0x2d26000, 0x4cc},{0x2ac0000, 0x47e},
	{0x2881000, 0x43a},{0x2665000, 0x3fa},{0x2468000, 0x3c2},{0x2287000, 0x38e},
	{0x20c1000, 0x35e},{0x1f12000, 0x332},{0x1d79000, 0x30a},{0x1bf4000, 0x2e6},
};

double frsqrte_espresso(double input)
{
	unsigned long long x = *(unsigned long long*)&input;

	// 0.0 and -0.0
	if ((x << 1) == 0)
	{
		// result is inf or -inf
		x &= ~0x7FFFFFFFFFFFFFFF;
		x |= 0x7FF0000000000000;
		return *(double*)&x;
	}
	// get exponent
	uint32 e = (x >> ieee_double_m_bits) & ((1ull << ieee_double_e_bits) - 1ull);
	// NaN or INF
	if (e == 0x7FF)
	{
		if ((x&((1ull << ieee_double_m_bits) - 1)) == 0)
		{
			// negative INF returns +NaN
			if ((sint64)x < 0)
			{
				x = 0x7FF8000000000000;
				return *(double*)&x;
			}
			// positive INF returns +0.0
			return 0.0;
		}
		// result is NaN with same sign and same mantissa (todo: verify)
		return *(double*)&x;
	}
	// negative number (other than -0.0)
	if ((sint64)x < 0)
	{
		// result is positive NaN
		x = 0x7FF8000000000000;
		return *(double*)&x;
	}
	// todo: handle denormals

	// get index (lsb of exponent, remaining bits of mantissa)
	uint32 idx = (x >> (ieee_double_m_bits - espresso_frsqrte_i_bits + 1ull))&((1 << espresso_frsqrte_i_bits) - 1);
	// get step multiplier
	uint32 stepMul = (x >> (ieee_double_m_bits - espresso_frsqrte_i_bits + 1 - 11))&((1 << 11) - 1);

	sint32 sum = frsqrteLookupTable[idx].offset - frsqrteLookupTable[idx].step * stepMul;

	e = 1023 - ((e - 1021) >> 1);
	x &= ~(((1ull << ieee_double_e_bits) - 1ull) << ieee_double_m_bits);
	x |= ((unsigned long long)e << ieee_double_m_bits);

	x &= ~((1ull << ieee_double_m_bits) - 1ull);
	x += ((unsigned long long)sum << 26ull);

	return *(double*)&x;
}

const int espresso_fres_i_bits = 5; // index bits
const int espresso_fres_s_bits = 10; // step multiplier bits

typedef struct
{
	uint32 offset;
	uint32 step;
}espresso_fres_entry_t;

espresso_fres_entry_t fresLookupTable[32] =
{
	// table calculated by fres_gen_table()
	{0x7ff800, 0x3e1},	{0x783800, 0x3a7},	{0x70ea00, 0x371},	{0x6a0800, 0x340},
	{0x638800, 0x313},	{0x5d6200, 0x2ea},	{0x579000, 0x2c4},	{0x520800, 0x2a0},
	{0x4cc800, 0x27f},	{0x47ca00, 0x261},	{0x430800, 0x245},	{0x3e8000, 0x22a},
	{0x3a2c00, 0x212},	{0x360800, 0x1fb},	{0x321400, 0x1e5},	{0x2e4a00, 0x1d1},
	{0x2aa800, 0x1be},	{0x272c00, 0x1ac},	{0x23d600, 0x19b},	{0x209e00, 0x18b},
	{0x1d8800, 0x17c},	{0x1a9000, 0x16e},	{0x17ae00, 0x15b},	{0x14f800, 0x15b},
	{0x124400, 0x143},	{0xfbe00, 0x143},	{0xd3800, 0x12d},	{0xade00, 0x12d},
	{0x88400, 0x11a},	{0x65000, 0x11a},	{0x41c00, 0x108},	{0x20c00, 0x106}
};

double fres_espresso(double input)
{
	// based on testing we know that fres uses only the first 15 bits of the mantissa
	// seee eeee eeee mmmm mmmm mmmm mmmx xxxx ....		(s = sign, e = exponent, m = mantissa, x = not used)
	// the mantissa bits are interpreted as following:
	// 0000 0000 0000 iiii ifff ffff fff0 ...			(i = table look up index , f = step multiplier)
	unsigned long long x = *(unsigned long long*)&input;

	// get index
	uint32 idx = (x >> (ieee_double_m_bits - espresso_fres_i_bits))&((1 << espresso_fres_i_bits) - 1);
	// get step multiplier
	uint32 stepMul = (x >> (ieee_double_m_bits - espresso_fres_i_bits - 10))&((1 << 10) - 1);


	uint32 sum = fresLookupTable[idx].offset - (fresLookupTable[idx].step * stepMul + 1) / 2;

	// get exponent
	uint32 e = (x >> ieee_double_m_bits) & ((1ull << ieee_double_e_bits) - 1ull);
	if (e == 0)
	{
		// todo?
		//x &= 0x7FFFFFFFFFFFFFFFull;
		x |= 0x7FF0000000000000ull;
		return *(double*)&x;
	}
	else if (e == 0x7ff) // NaN or INF
	{
		if ((x&((1ull << ieee_double_m_bits) - 1)) == 0)
		{
			// negative INF returns -0.0
			if ((sint64)x < 0)
			{
				x = 0x8000000000000000;
				return *(double*)&x;
			}
			// positive INF returns +0.0
			return 0.0;
		}
		// result is NaN with same sign and same mantissa (todo: verify)
		return *(double*)&x;
	}
	// todo - needs more testing (especially NaN and INF values)

	e = 2045 - e;
	x &= ~(((1ull << ieee_double_e_bits) - 1ull) << ieee_double_m_bits);
	x |= ((unsigned long long)e << ieee_double_m_bits);

	x &= ~((1ull << ieee_double_m_bits) - 1ull);
	x += ((unsigned long long)sum << 29ull);

	return *(double*)&x;
}

void fcmpu_espresso(PPCInterpreter_t* hCPU, int crfD, double a, double b)
{
	uint32 c;

	ppc_setCRBit(hCPU, crfD + 0, 0);
	ppc_setCRBit(hCPU, crfD + 1, 0);
	ppc_setCRBit(hCPU, crfD + 2, 0);
	ppc_setCRBit(hCPU, crfD + 3, 0);

	if (IS_NAN(*(uint64*)&a) || IS_NAN(*(uint64*)&b))
	{
		c = 1;
		ppc_setCRBit(hCPU, crfD + CR_BIT_SO, 1);
	}
	else if (a < b)
	{
		c = 8;
		ppc_setCRBit(hCPU, crfD + CR_BIT_LT, 1);
	}
	else if (a > b)
	{
		c = 4;
		ppc_setCRBit(hCPU, crfD + CR_BIT_GT, 1);
	}
	else
	{
		c = 2;
		ppc_setCRBit(hCPU, crfD + CR_BIT_EQ, 1);
	}

	if (IS_SNAN(*(uint64*)&a) || IS_SNAN(*(uint64*)&b))
		hCPU->fpscr |= FPSCR_VXSNAN;

	hCPU->fpscr = (hCPU->fpscr & 0xffff0fff) | (c << 12);
}

void PPCInterpreter_FMR(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, rA, frB;
	PPC_OPC_TEMPL_X(Opcode, frD, rA, frB);
	PPC_ASSERT(rA==0);
	hCPU->fpr[frD].fpr = hCPU->fpr[frB].fpr;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FSEL(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);
	if ( hCPU->fpr[frA].fp0 >= -0.0f )
		hCPU->fpr[frD] = hCPU->fpr[frC];
	else
		hCPU->fpr[frD] = hCPU->fpr[frB];
	PPC_ASSERT((Opcode & PPC_OPC_RC) != 0); // update CR1 flags

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FCTIWZ(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(Opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);

	double b = hCPU->fpr[frB].fpr;
	uint64 v;
	if (b > (double)0x7FFFFFFF)
	{
		v = (uint64)0x7FFFFFFF;
	}
	else if (b < -(double)0x80000000)
	{
		v = (uint64)0x80000000;
	}
	else
	{
		v = (uint64)(uint32)(sint32)b;
	}

	hCPU->fpr[frD].guint = 0xFFF8000000000000ULL | v;
	if (v == 0 && ((*(uint64*)&b) >> 63))
		hCPU->fpr[frD].guint |= 0x100000000ull;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FCTIW(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB;
	PPC_OPC_TEMPL_X(Opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);

	double b = hCPU->fpr[frB].fpr;
	uint64 v;
	if (b > (double)0x7FFFFFFF)
	{
		v = (uint64)0x7FFFFFFF;
	}
	else if (b < -(double)0x80000000)
	{
		v = (uint64)0x80000000;
	}
	else
	{
		// todo: Support for other rounding modes than NEAR
		double t = b + 0.5;
		sint32 i = (sint32)t;
		if (t - i < 0 || (t - i == 0 && b > 0))
		{
			i--;
		}
		v = (uint64)i;
	}
	hCPU->fpr[frD].guint = 0xFFF8000000000000ULL | v;
	if (v == 0 && ((*(uint64*)&b) >> 63))
		hCPU->fpr[frD].guint |= 0x100000000ull;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FNEG(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB;
	PPC_OPC_TEMPL_X(Opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	
	hCPU->fpr[frD].guint = hCPU->fpr[frB].guint ^ (1ULL << 63);

	PPC_ASSERT((Opcode & PPC_OPC_RC) != 0); // update CR1 flags

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FRSP(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(Opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);

	if( PPC_PSE )
	{
		hCPU->fpr[frD].fp0 = (float)hCPU->fpr[frB].fpr;
		hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;
	}
	else
	{
		hCPU->fpr[frD].fpr = (float)hCPU->fpr[frB].fpr;
	}

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FRSQRTE(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);
	PPC_ASSERT(frA==0 && frC==0);
	
	hCPU->fpr[frD].fpr = frsqrte_espresso(hCPU->fpr[frB].fpr);

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FRES(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);
	PPC_ASSERT(frA==0 && frC==0);

	hCPU->fpr[frD].fpr = fres_espresso(hCPU->fpr[frB].fpr);
	
	if(PPC_PSE) 
		hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;

	PPCInterpreter_nextInstruction(hCPU);
}

// Floating point ALU

void PPCInterpreter_FABS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB;
	PPC_OPC_TEMPL_X(Opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);

	hCPU->fpr[frD].guint = hCPU->fpr[frB].guint & ~0x8000000000000000;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FNABS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB;
	PPC_OPC_TEMPL_X(Opcode, frD, frA, frB);
	PPC_ASSERT(frA==0);
	
	hCPU->fpr[frD].guint = hCPU->fpr[frB].guint | 0x8000000000000000;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FADD(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);
	PPC_ASSERT(frC==0);

	hCPU->fpr[frD].fpr = hCPU->fpr[frA].fpr + hCPU->fpr[frB].fpr;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FDIV(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);
	PPC_ASSERT(frC==0);

	hCPU->fpr[frD].fpr = hCPU->fpr[frA].fpr / hCPU->fpr[frB].fpr;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FSUB(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);
	PPC_ASSERT(frC==0);

	hCPU->fpr[frD].fpr = hCPU->fpr[frA].fpr - hCPU->fpr[frB].fpr;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FMUL(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);
	PPC_ASSERT(frC == 0);

	hCPU->fpr[frD].fpr = hCPU->fpr[frA].fpr * hCPU->fpr[frC].fpr;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FMADD(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);

	hCPU->fpr[frD].fpr = hCPU->fpr[frA].fpr * hCPU->fpr[frC].fpr + hCPU->fpr[frB].fpr;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FNMADD(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);

	hCPU->fpr[frD].fpr = -(hCPU->fpr[frA].fpr * hCPU->fpr[frC].fpr + hCPU->fpr[frB].fpr);

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FMSUB(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);

	hCPU->fpr[frD].fpr = (hCPU->fpr[frA].fpr * hCPU->fpr[frC].fpr - hCPU->fpr[frB].fpr);

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FNMSUB(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);

	hCPU->fpr[frD].fpr = -(hCPU->fpr[frA].fpr * hCPU->fpr[frC].fpr - hCPU->fpr[frB].fpr);

	PPCInterpreter_nextInstruction(hCPU);
}

// Move

void PPCInterpreter_MFFS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, rA, rB;
	PPC_OPC_TEMPL_X(Opcode, frD, rA, rB);
	PPC_ASSERT(rA==0 && rB==0);
	hCPU->fpr[frD].guint = (uint64)hCPU->fpscr;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_MTFSF(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frB;
	uint32 fm, FM;
	PPC_OPC_TEMPL_XFL(Opcode, frB, fm);
	FM = ((fm&0x80)?0xf0000000:0)|((fm&0x40)?0x0f000000:0)|((fm&0x20)?0x00f00000:0)|((fm&0x10)?0x000f0000:0)|
	     ((fm&0x08)?0x0000f000:0)|((fm&0x04)?0x00000f00:0)|((fm&0x02)?0x000000f0:0)|((fm&0x01)?0x0000000f:0);
	hCPU->fpscr = (hCPU->fpr[frB].guint & FM) | (hCPU->fpscr & ~FM);

	PPC_ASSERT((Opcode & PPC_OPC_RC) != 0); // update CR1 flags

	static bool logFPSCRWriteOnce = false;
	if( logFPSCRWriteOnce == false )
	{
		cemuLog_log(LogType::Force, "Unsupported write to FPSCR");
		logFPSCRWriteOnce = true;
	}
	PPCInterpreter_nextInstruction(hCPU);
}

// single precision

void PPCInterpreter_FADDS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);
	PPC_ASSERT(frB == 0);
	
	// todo: check for RC

	hCPU->fpr[frD].fpr = (float)(hCPU->fpr[frA].fpr + hCPU->fpr[frB].fpr);
	if (PPC_PSE)
		hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FSUBS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);
	PPC_ASSERT(frB == 0);

	hCPU->fpr[frD].fpr = (float)(hCPU->fpr[frA].fpr - hCPU->fpr[frB].fpr);
	if (PPC_PSE)
		hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FDIVS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);
	PPC_ASSERT(frB==0);

	hCPU->fpr[frD].fpr = (float)(hCPU->fpr[frA].fpr / hCPU->fpr[frB].fpr);
	if( PPC_PSE )
		hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FMULS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);
	PPC_ASSERT(frB == 0);

	hCPU->fpr[frD].fpr = (float)(hCPU->fpr[frA].fpr * roundTo25BitAccuracy(hCPU->fpr[frC].fpr));
	if (PPC_PSE)
		hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FMADDS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);

	hCPU->fpr[frD].fpr = (float)(hCPU->fpr[frA].fpr * roundTo25BitAccuracy(hCPU->fpr[frC].fpr) + hCPU->fpr[frB].fpr);
	if (PPC_PSE)
		hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FNMADDS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);

	hCPU->fpr[frD].fpr = (float)-(hCPU->fpr[frA].fpr * roundTo25BitAccuracy(hCPU->fpr[frC].fpr) + hCPU->fpr[frB].fpr);
	if (PPC_PSE)
		hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FMSUBS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);

	hCPU->fpr[frD].fp0 = (float)(hCPU->fpr[frA].fp0 * roundTo25BitAccuracy(hCPU->fpr[frC].fp0) - hCPU->fpr[frB].fp0);
	if (PPC_PSE)
		hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FNMSUBS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();

	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(Opcode, frD, frA, frB, frC);

	hCPU->fpr[frD].fp0 = (float)-(hCPU->fpr[frA].fp0 * roundTo25BitAccuracy(hCPU->fpr[frC].fp0) - hCPU->fpr[frB].fp0);
	if (PPC_PSE)
		hCPU->fpr[frD].fp1 = hCPU->fpr[frD].fp0;

	PPCInterpreter_nextInstruction(hCPU);
}

// Compare

void PPCInterpreter_FCMPO(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	
	int crfD, frA, frB;
	PPC_OPC_TEMPL_X(Opcode, crfD, frA, frB);
	crfD >>= 2;
	hCPU->cr[crfD*4+0] = 0;
	hCPU->cr[crfD*4+1] = 0;
	hCPU->cr[crfD*4+2] = 0;
	hCPU->cr[crfD*4+3] = 0;

	uint32 c;
	if(IS_NAN(hCPU->fpr[frA].guint) || IS_NAN(hCPU->fpr[frB].guint))
	{
		c = 1;
		hCPU->cr[crfD*4+CR_BIT_SO] = 1;
	}
    else if(hCPU->fpr[frA].fpr < hCPU->fpr[frB].fpr)
	{
		c = 8;
		hCPU->cr[crfD*4+CR_BIT_LT] = 1;
	}
	else if(hCPU->fpr[frA].fpr > hCPU->fpr[frB].fpr)
	{
		c = 4;
		hCPU->cr[crfD*4+CR_BIT_GT] = 1;
	}
	else
	{
		c = 2;
		hCPU->cr[crfD*4+CR_BIT_EQ] = 1;
	}

    hCPU->fpscr = (hCPU->fpscr & 0xffff0fff) | (c << 12);

	if (IS_SNAN (hCPU->fpr[frA].guint) || IS_SNAN (hCPU->fpr[frB].guint))
		hCPU->fpscr |= FPSCR_VXSNAN;
	else if (!(hCPU->fpscr & FPSCR_VE) || IS_QNAN (hCPU->fpr[frA].guint) || IS_QNAN (hCPU->fpr[frB].guint))
		hCPU->fpscr |= FPSCR_VXVC;

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_FCMPU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	
	int crfD, frA, frB;
	PPC_OPC_TEMPL_X(Opcode, crfD, frA, frB);
	cemu_assert_debug((crfD % 4) == 0);
	fcmpu_espresso(hCPU, crfD, hCPU->fpr[frA].fp0, hCPU->fpr[frB].fp0);

	PPCInterpreter_nextInstruction(hCPU);
}
