#include "../PPCState.h"
#include "PPCInterpreterInternal.h"
#include "PPCInterpreterHelper.h"

#include "Cafe/OS/libs/coreinit/coreinit_CodeGen.h"

#include "../Recompiler/PPCRecompiler.h"
#include "../Recompiler/PPCRecompilerX64.h"

#include <float.h>
#include "Cafe/HW/Latte/Core/LatteBufferCache.h"

void PPCInterpreter_MFMSR(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	cemuLog_logDebug(LogType::Force, "Rare instruction: MFMSR");
	if (hCPU->sprExtended.msr & MSR_PR)
	{
		PPC_ASSERT(true);
		return;
	}
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	hCPU->gpr[rD] = hCPU->sprExtended.msr;

	PPCInterpreter_nextInstruction(hCPU);

}

void PPCInterpreter_MTMSR(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	cemuLog_logDebug(LogType::Force, "Rare instruction: MTMSR");
	if (hCPU->sprExtended.msr & MSR_PR)
	{
		PPC_ASSERT(true);
		return;
	}
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);

	hCPU->sprExtended.msr = hCPU->gpr[rS];
	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_MTFSB1X(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	cemuLog_logDebug(LogType::Force, "Rare instruction: MTFSB1X");
	int crbD, n1, n2;
	PPC_OPC_TEMPL_X(Opcode, crbD, n1, n2);
	if (crbD != 1 && crbD != 2) 
	{
		hCPU->fpscr |= 1 << (31 - crbD);
	}
	if (Opcode & PPC_OPC_RC) 
	{
		// update cr1 flags
		PPC_ASSERT(true);
	}

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_MCRF(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	uint32 crD, crS, b;
	PPC_OPC_TEMPL_X(Opcode, crD, crS, b);
	crD >>= 2;
	crS >>= 2;
	for (sint32 i = 0; i<4; i++)
		ppc_setCRBit(hCPU, crD * 4 + i, ppc_getCRBit(hCPU, crS * 4 + i));

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_MFCR(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	// frequently used by GCC compiled code (e.g. SM64 port)
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	
	// in our array: cr0.LT is entry with index 0
	// in GPR: cr0.LT is in MSB
	uint32 cr = 0;
	for (sint32 i = 0; i < 32; i++)
	{
		cr <<= 1;
		if (ppc_getCRBit(hCPU, i) != 0)
			cr |= 1;
	}
	hCPU->gpr[rD] = cr;
	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_MTCRF(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	// frequently used by GCC compiled code (e.g. SM64 port)
	// tested

	uint32 rS;
	uint32 crfMask;
	PPC_OPC_TEMPL_XFX(Opcode, rS, crfMask);

	for (sint32 crIndex = 0; crIndex < 8; crIndex++)
	{
		if (!ppc_MTCRFMaskHasCRFieldSet(crfMask, crIndex))
			continue;

		uint32 crBitBase = crIndex * 4;
		uint8 nibble = (uint8)(hCPU->gpr[rS] >> (28 - crIndex * 4));
		ppc_setCRBit(hCPU, crBitBase + 0, (nibble >> 3) & 1);
		ppc_setCRBit(hCPU, crBitBase + 1, (nibble >> 2) & 1);
		ppc_setCRBit(hCPU, crBitBase + 2, (nibble >> 1) & 1);
		ppc_setCRBit(hCPU, crBitBase + 3, (nibble >> 0) & 1);
	}

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_MCRXR(PPCInterpreter_t* hCPU, uint32 Opcode)
{	
	// used in Dont Starve: Giant Edition
	// also used frequently by Web Browser (webkit?)
	uint32 cr;
	cr = (Opcode >> (31 - 8)) & 7;
	cr >>= 2;

	uint32 xer = PPCInterpreter_getXER(hCPU);
	uint32 xerBits = (xer >> 28) & 0xF;

	// todo - is the order correct?
	ppc_setCRBit(hCPU, cr * 4 + 0, (xerBits >> 0) & 1);
	ppc_setCRBit(hCPU, cr * 4 + 1, (xerBits >> 1) & 1);
	ppc_setCRBit(hCPU, cr * 4 + 2, (xerBits >> 2) & 1);
	ppc_setCRBit(hCPU, cr * 4 + 3, (xerBits >> 3) & 1);

	// reset copied bits
	PPCInterpreter_setXER(hCPU, xer&~0xF0000000);

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_TLBIE(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_TLBSYNC(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	cemu_assert_unimplemented();
	PPCInterpreter_nextInstruction(hCPU);
}

// branch instructions

void PPCInterpreter_BX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	uint32 li;
	PPC_OPC_TEMPL_I(Opcode, li);
	if ((Opcode & PPC_OPC_AA) == 0)
		li += (unsigned int)hCPU->instructionPointer;
	if (Opcode & PPC_OPC_LK) 
	{
		// update LR and IP
		hCPU->spr.LR = (unsigned int)hCPU->instructionPointer + 4;
		hCPU->instructionPointer = li;
		PPCInterpreter_jumpToInstruction(hCPU, li);
		PPCRecompiler_attemptEnter(hCPU, li);
		return;
	}
	PPCInterpreter_jumpToInstruction(hCPU, li);
}


void PPCInterpreter_BCX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_B(Opcode, BO, BI, BD);
	if (!(BO & 4))
		hCPU->spr.CTR--;
	bool bo2 = (BO & 2) != 0;
	bool bo8 = (BO & 8) != 0; // branch condition true
	bool cr = ppc_getCRBit(hCPU, BI) != 0;
	if (((BO & 4) || ((hCPU->spr.CTR != 0) ^ bo2)) 
		&& ((BO & 16) || (!(cr ^ bo8)))) 
	{
		if (!(Opcode & PPC_OPC_AA))
		{
			BD += (unsigned int)hCPU->instructionPointer;
		}
		else
		{
			// should never happen
			cemu_assert_unimplemented();
		}
		if (Opcode & PPC_OPC_LK)
			hCPU->spr.LR = ((unsigned int)hCPU->instructionPointer) + 4;
		PPCInterpreter_jumpToInstruction(hCPU, BD);
	}
	else
		PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_BCLRX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(Opcode, BO, BI, BD);
	PPC_ASSERT(BD == 0);
	if (!(BO & 4))
	{
		if (hCPU->spr.CTR == 0)
		{
			PPC_ASSERT(true);
			cemuLog_logDebug(LogType::Force, "Decrementer underflow!");
		}
		hCPU->spr.CTR--;
	}
	bool bo2 = (BO & 2) ? true : false;
	bool bo8 = (BO & 8) ? true : false;
	bool cr = ppc_getCRBit(hCPU, BI) != 0;
	if (((BO & 4) || ((hCPU->spr.CTR != 0) ^ bo2))
		&& ((BO & 16) || (!(cr ^ bo8))))
	{
		BD = hCPU->spr.LR & 0xfffffffc;
		if (Opcode & PPC_OPC_LK)
		{
			hCPU->spr.LR = (unsigned int)hCPU->instructionPointer + 4;
		}
		PPCInterpreter_jumpToInstruction(hCPU, BD);
		PPCRecompiler_attemptEnter(hCPU, BD);
		return;
	}
	else
	{
		BD = (unsigned int)hCPU->instructionPointer + 4;
		PPCInterpreter_nextInstruction(hCPU);
	}
}

void PPCInterpreter_BCCTR(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	uint32 x = (unsigned int)hCPU->instructionPointer;
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(Opcode, BO, BI, BD);
	PPC_ASSERT(BD == 0);
	PPC_ASSERT(!(BO & 2));
	bool bo8 = (BO & 8) ? true : false;
	bool cr = ppc_getCRBit(hCPU, BI) != 0;
	if ((BO & 16) || (!(cr ^ bo8)))
	{
		if (Opcode & PPC_OPC_LK)
		{
			hCPU->spr.LR = (unsigned int)hCPU->instructionPointer + 4;
			hCPU->instructionPointer = (unsigned int)(hCPU->spr.CTR & 0xfffffffc);
		}
		else
		{
			hCPU->instructionPointer = (unsigned int)(hCPU->spr.CTR & 0xfffffffc);
		}
		PPCRecompiler_attemptEnter(hCPU, hCPU->instructionPointer);
	}
	else
	{
		hCPU->instructionPointer += 4;
	}
}

void PPCInterpreter_DCBT(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rB;
	rA = (Opcode >> (31 - 15)) & 0x1F;
	rB = (Opcode >> (31 - 20)) & 0x1F;
	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_DCBST(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rB;
	rA = (Opcode >> (31 - 15)) & 0x1F;
	rB = (Opcode >> (31 - 20)) & 0x1F;

	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];

	LatteBufferCache_notifyDCFlush(ea, 32);
	
	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_DCBF(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rB;
	rA = (Opcode >> (31 - 15)) & 0x1F;
	rB = (Opcode >> (31 - 20)) & 0x1F;

	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];

	LatteBufferCache_notifyDCFlush(ea, 32);

	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_DCBZL(PPCInterpreter_t* hCPU, uint32 Opcode) //Undocumented
{
	// no-op
	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_DCBI(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	// no-op
	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_ICBI(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rD, rA, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];
	// invalidate range
	coreinit::codeGenHandleICBI(ea);
	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_EIEIO(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	// no effect
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_SC(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	cemuLog_logDebug(LogType::Force, "SC executed at 0x{:08x}", hCPU->instructionPointer);
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_SYNC(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	// no-op
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_ISYNC(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	// no-op
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);
}

void PPCInterpreter_RFI(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	cemuLog_logDebug(LogType::Force, "RFI");
	hCPU->sprExtended.msr &= ~(0x87C0FF73 | 0x00040000);
	hCPU->sprExtended.msr |= hCPU->sprExtended.srr1 & 0x87c0ff73;
	hCPU->sprExtended.msr |= MSR_RI;
	hCPU->instructionPointer = (unsigned int)(hCPU->sprExtended.srr0);
}
