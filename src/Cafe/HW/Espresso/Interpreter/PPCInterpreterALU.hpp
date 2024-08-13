
static void PPCInterpreter_setXerOV(PPCInterpreter_t* hCPU, bool hasOverflow)
{
	if (hasOverflow)
	{
		hCPU->spr.XER |= XER_SO;
		hCPU->spr.XER |= XER_OV;
	}
	else
	{
		hCPU->spr.XER &= ~XER_OV;
	}
}

static bool checkAdditionOverflow(uint32 x, uint32 y, uint32 r)
{

	/*
		x	y	r	result	(has overflow)
		0	0	0	0
		1	0	0	0
		0	1	0	0
		1	1	0	1
		0	0	1	1
		1	0	1	0
		0	1	1	0
		1	1	1	0

	*/
	return (((x ^ r) & (y ^ r)) >> 31) != 0;
}

static void PPCInterpreter_ADD(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	hCPU->gpr[rD] = (int)hCPU->gpr[rA] + (int)hCPU->gpr[rB];
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ADDO(PPCInterpreter_t* hCPU, uint32 opcode)
{
	// untested (Don't Starve Giant Edition uses this instruction + BSO)
	PPC_OPC_TEMPL3_XO();
	uint32 result = hCPU->gpr[rA] + hCPU->gpr[rB];
	PPCInterpreter_setXerOV(hCPU, checkAdditionOverflow(hCPU->gpr[rA], hCPU->gpr[rB], result));
	hCPU->gpr[rD] = (uint32)result;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ADDC(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	uint32 a = hCPU->gpr[rA];
	hCPU->gpr[rD] = a + hCPU->gpr[rB];
	if (hCPU->gpr[rD] < a)
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ADDCO(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	uint32 a = hCPU->gpr[rA];
	uint32 b = hCPU->gpr[rB];
	hCPU->gpr[rD] = a + b;
	if (hCPU->gpr[rD] < a)
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	// set SO/OV
	PPCInterpreter_setXerOV(hCPU, checkAdditionOverflow(a, b, hCPU->gpr[rD]));
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ADDE(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	uint32 a = hCPU->gpr[rA];
	uint32 b = hCPU->gpr[rB];
	uint32 ca = hCPU->xer_ca;
	hCPU->gpr[rD] = a + b + ca;
	// update xer
	if (ppc_carry_3(a, b, ca))
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ADDEO(PPCInterpreter_t* hCPU, uint32 opcode)
{
	// used by DS Virtual Console (Super Mario 64 DS)
	PPC_OPC_TEMPL3_XO();
	uint32 a = hCPU->gpr[rA];
	uint32 b = hCPU->gpr[rB];
	uint32 ca = hCPU->xer_ca;
	hCPU->gpr[rD] = a + b + ca;
	// update xer carry
	if (ppc_carry_3(a, b, ca))
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	PPCInterpreter_setXerOV(hCPU, checkAdditionOverflow(a, b, hCPU->gpr[rD]));
	// update CR
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ADDI(PPCInterpreter_t* hCPU, uint32 opcode)
{
	sint32 rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	hCPU->gpr[rD] = (rA ? (int)hCPU->gpr[rA] : 0) + (int)imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ADDIC(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	uint32 a = hCPU->gpr[rA];
	hCPU->gpr[rD] = a + imm;
	// update XER
	if (hCPU->gpr[rD] < a)
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ADDIC_(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	uint32 a = hCPU->gpr[rA];
	hCPU->gpr[rD] = a + imm;
	// update XER
	if (hCPU->gpr[rD] < a)
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	// update cr0 flags
	ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ADDIS(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(opcode, rD, rA, imm);
	hCPU->gpr[rD] = (rA ? hCPU->gpr[rA] : 0) + imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ADDZE(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	PPC_ASSERT(rB == 0);
	uint32 a = hCPU->gpr[rA];
	uint32 ca = hCPU->xer_ca;
	hCPU->gpr[rD] = a + ca;
	if ((a == 0xffffffff) && ca)
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ADDME(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	PPC_ASSERT(rB == 0);
	uint32 a = hCPU->gpr[rA];
	uint32 ca = hCPU->xer_ca;
	hCPU->gpr[rD] = a + ca + 0xffffffff;
	if (a || ca)
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SUBF(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	hCPU->gpr[rD] = ~hCPU->gpr[rA] + hCPU->gpr[rB] + 1;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SUBFO(PPCInterpreter_t* hCPU, uint32 opcode)
{
	// Seen in Don't Starve Giant Edition and Teslagrad
	// also used by DS Virtual Console (Super Mario 64 DS)
	PPC_OPC_TEMPL3_XO();
	uint32 result = ~hCPU->gpr[rA] + hCPU->gpr[rB] + 1;
	PPCInterpreter_setXerOV(hCPU, checkAdditionOverflow(~hCPU->gpr[rA], hCPU->gpr[rB], result));
	hCPU->gpr[rD] = result;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SUBFC(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	uint32 a = hCPU->gpr[rA];
	uint32 b = hCPU->gpr[rB];
	hCPU->gpr[rD] = ~a + b + 1;
	// update xer
	if (ppc_carry_3(~a, b, 1))
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SUBFCO(PPCInterpreter_t* hCPU, uint32 opcode)
{
	// used by DS Virtual Console (Super Mario 64 DS)
	PPC_OPC_TEMPL3_XO();
	uint32 a = hCPU->gpr[rA];
	uint32 b = hCPU->gpr[rB];
	hCPU->gpr[rD] = ~a + b + 1;
	// update xer
	if (ppc_carry_3(~a, b, 1))
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	// update xer SO/OV
	PPCInterpreter_setXerOV(hCPU, checkAdditionOverflow(~a, b, hCPU->gpr[rD]));
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SUBFIC(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	uint32 a = hCPU->gpr[rA];
	hCPU->gpr[rD] = ~a + imm + 1;
	if (ppc_carry_3(~a, imm, 1))
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SUBFE(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	uint32 a = hCPU->gpr[rA];
	uint32 b = hCPU->gpr[rB];
	uint32 ca = hCPU->xer_ca;
	hCPU->gpr[rD] = ~a + b + ca;
	// update xer carry
	if (ppc_carry_3(~a, b, ca))
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	// update cr0
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SUBFEO(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	uint32 a = hCPU->gpr[rA];
	uint32 b = hCPU->gpr[rB];
	uint32 ca = hCPU->xer_ca;
	uint32 result = ~a + b + ca;
	hCPU->gpr[rD] = result;
	// update xer carry
	if (ppc_carry_3(~a, b, ca))
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	PPCInterpreter_setXerOV(hCPU, checkAdditionOverflow(~a, b, result));
	// update cr0
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SUBFZE(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	PPC_ASSERT(rB == 0);
	uint32 a = hCPU->gpr[rA];
	uint32 ca = hCPU->xer_ca;
	hCPU->gpr[rD] = ~a + ca;
	if (a == 0 && ca)
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SUBFME(PPCInterpreter_t* hCPU, uint32 opcode)
{
	// untested
	PPC_OPC_TEMPL3_XO();
	PPC_ASSERT(rB == 0);
	uint32 a = hCPU->gpr[rA];
	uint32 ca = hCPU->xer_ca;
	hCPU->gpr[rD] = ~a + 0xFFFFFFFF + ca;
	// update xer carry
	if (ppc_carry_3(~a, 0xFFFFFFFF, ca))
		hCPU->xer_ca = 1;
	else
		hCPU->xer_ca = 0;
	// update cr0
	if (opcode & PPC_OPC_RC)
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_MULHW_(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	sint64 a = (sint32)hCPU->gpr[rA];
	sint64 b = (sint32)hCPU->gpr[rB];
	sint64 c = a * b;
	hCPU->gpr[rD] = ((uint64)c) >> 32;
	if (opcode & PPC_OPC_RC) {
		// update cr0 flags
#ifdef CEMU_DEBUG_ASSERT
		assert_dbg();
#endif
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	}
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_MULHWU_(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	uint64 a = hCPU->gpr[rA];
	uint64 b = hCPU->gpr[rB];
	uint64 c = a * b;
	hCPU->gpr[rD] = c >> 32;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_MULLW(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	sint64 result = (sint64)hCPU->gpr[rA] * (sint64)hCPU->gpr[rB];
	hCPU->gpr[rD] = (uint32)result;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_MULLWO(PPCInterpreter_t* hCPU, uint32 opcode)
{
	// Don't Starve Giant Edition uses this instruction + BSO
	// also used by FullBlast when a save file exists + it uses mfxer to access overflow result
	PPC_OPC_TEMPL3_XO();
	sint64 result = (sint64)(sint32)hCPU->gpr[rA] * (sint64)(sint32)hCPU->gpr[rB];
	hCPU->gpr[rD] = (uint32)result;
	PPCInterpreter_setXerOV(hCPU, result < -0x80000000ll || result > 0x7FFFFFFFLL);
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_MULLI(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
	hCPU->gpr[rD] = hCPU->gpr[rA] * imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_DIVW(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	sint32 a = hCPU->gpr[rA];
	sint32 b = hCPU->gpr[rB];
	if (b == 0)
	{
		cemuLog_logDebug(LogType::Force, "Error: Division by zero! [{:08x}]", (uint32)hCPU->instructionPointer);
		b++;
	}
	hCPU->gpr[rD] = a / b;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_DIVWO(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	sint32 a = hCPU->gpr[rA];
	sint32 b = hCPU->gpr[rB];
	if (b == 0)
	{
		PPCInterpreter_setXerOV(hCPU, true);
		PPCInterpreter_nextInstruction(hCPU);
		return;
	}
	hCPU->gpr[rD] = a / b;
	PPCInterpreter_setXerOV(hCPU, false);
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_DIVWU(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	if (hCPU->gpr[rB] == 0)
	{
		PPCInterpreter_nextInstruction(hCPU);
		return;
	}
	hCPU->gpr[rD] = hCPU->gpr[rA] / hCPU->gpr[rB];
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_DIVWUO(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	if (hCPU->gpr[rB] == 0)
	{
		PPCInterpreter_setXerOV(hCPU, true);
		PPCInterpreter_nextInstruction(hCPU);
		return;
	}
	hCPU->gpr[rD] = hCPU->gpr[rA] / hCPU->gpr[rB];
	PPCInterpreter_setXerOV(hCPU, false);
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_CREQV(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL_X_CR();
	ppc_setCRBit(hCPU, crD, ppc_getCRBit(hCPU, crA) ^ ppc_getCRBit(hCPU, crB) ^ 1);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_CRAND(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL_X_CR();
	ppc_setCRBit(hCPU, crD, ppc_getCRBit(hCPU, crA)&ppc_getCRBit(hCPU, crB));
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_CRANDC(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL_X_CR();
	ppc_setCRBit(hCPU, crD, ppc_getCRBit(hCPU, crA)&(ppc_getCRBit(hCPU, crB) ^ 1));
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_CROR(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL_X_CR();
	ppc_setCRBit(hCPU, crD, ppc_getCRBit(hCPU, crA) | ppc_getCRBit(hCPU, crB));
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_CRORC(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL_X_CR();
	ppc_setCRBit(hCPU, crD, ppc_getCRBit(hCPU, crA) | (ppc_getCRBit(hCPU, crB) ^ 1));
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_CRNOR(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL_X_CR();
	ppc_setCRBit(hCPU, crD, (ppc_getCRBit(hCPU, crA) | ppc_getCRBit(hCPU, crB)) ^ 1);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_CRXOR(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL_X_CR();
	ppc_setCRBit(hCPU, crD, ppc_getCRBit(hCPU, crA) ^ ppc_getCRBit(hCPU, crB));
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_NEG(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	PPC_ASSERT(rB == 0);
	hCPU->gpr[rD] = (uint32)-((sint32)hCPU->gpr[rA]);
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_NEGO(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	PPC_ASSERT(rB == 0);
	PPCInterpreter_setXerOV(hCPU, hCPU->gpr[rA] == 0x80000000);
	hCPU->gpr[rD] = (uint32)-((sint32)hCPU->gpr[rA]);
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ANDX(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	hCPU->gpr[rA] = hCPU->gpr[rD] & hCPU->gpr[rB];
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ANDCX(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	hCPU->gpr[rA] = hCPU->gpr[rD] & ~hCPU->gpr[rB];
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ANDI_(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(opcode, rS, rA, imm);
	hCPU->gpr[rA] = hCPU->gpr[rS] & imm;
	ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ANDIS_(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(opcode, rS, rA, imm);
	hCPU->gpr[rA] = hCPU->gpr[rS] & imm;
	ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_NANDX(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	hCPU->gpr[rA] = ~(hCPU->gpr[rD] & hCPU->gpr[rB]);
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_OR(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	hCPU->gpr[rA] = hCPU->gpr[rD] | hCPU->gpr[rB];
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ORC(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	hCPU->gpr[rA] = hCPU->gpr[rD] | ~hCPU->gpr[rB];
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ORI(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(opcode, rS, rA, imm);
	hCPU->gpr[rA] = hCPU->gpr[rS] | imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_ORIS(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(opcode, rS, rA, imm);
	hCPU->gpr[rA] = hCPU->gpr[rS] | imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_NORX(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	hCPU->gpr[rA] = ~(hCPU->gpr[rD] | hCPU->gpr[rB]);
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_XOR(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	hCPU->gpr[rA] = hCPU->gpr[rD] ^ hCPU->gpr[rB];
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_XORI(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(opcode, rS, rA, imm);
	hCPU->gpr[rA] = hCPU->gpr[rS] ^ imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_XORIS(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(opcode, rS, rA, imm);
	hCPU->gpr[rA] = hCPU->gpr[rS] ^ imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_EQV(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	hCPU->gpr[rA] = ~(hCPU->gpr[rD] ^ hCPU->gpr[rB]);
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_RLWIMI(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	uint32 v = ppc_word_rotl(hCPU->gpr[rS], SH);
	uint32 mask = ppc_mask(MB, ME);
	hCPU->gpr[rA] = (v & mask) | (hCPU->gpr[rA] & ~mask);
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_RLWINM(PPCInterpreter_t* hCPU, uint32 opcode)
{
	sint32 rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	uint32 v = ppc_word_rotl(hCPU->gpr[rS], SH);
	uint32 mask = ppc_mask(MB, ME);
	hCPU->gpr[rA] = v & mask;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_RLWNM(PPCInterpreter_t* hCPU, uint32 opcode)
{
	int rS, rA, rB, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, rB, MB, ME);
	uint32 v = ppc_word_rotl(hCPU->gpr[rS], hCPU->gpr[rB]);
	uint32 mask = ppc_mask(MB, ME);
	hCPU->gpr[rA] = v & mask;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SLWX(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	uint32 s = hCPU->gpr[rB] & 0x3f;
	if (s > 31)
		hCPU->gpr[rA] = 0;
	else
		hCPU->gpr[rA] = hCPU->gpr[rD] << s;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SRAW(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	uint32 sh = hCPU->gpr[rB] & 0x3f;
	hCPU->gpr[rA] = hCPU->gpr[rD];
	if (sh > 31)
	{
		hCPU->xer_ca = (hCPU->gpr[rA] >> 31) & 1; // copy sign bit to ca
		hCPU->gpr[rA] = (uint32)((sint32)hCPU->gpr[rA] >> 31); // fill all bits with sign bit
	}
	else
	{
		// ca is set when input is negative and non-zero bits are dropped by shift operation
		uint8 caBit = (hCPU->gpr[rA] >> 31) & 1;
		uint32 shiftedBits = hCPU->gpr[rA] & ~(0xFFFFFFFF << sh);
		caBit &= (shiftedBits != 0 ? 1 : 0);
		hCPU->xer_ca = caBit;
		hCPU->gpr[rA] = (uint32)((sint32)hCPU->gpr[rA] >> sh);
	}
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SRWX(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	uint32 v = hCPU->gpr[rB] & 0x3f;
	if (v > 31)
		hCPU->gpr[rA] = 0;
	else
		hCPU->gpr[rA] = hCPU->gpr[rD] >> v;
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_SRAWI(PPCInterpreter_t* hCPU, uint32 opcode)
{
	sint32 rS, rA;
	uint32 SH;
	PPC_OPC_TEMPL_X(opcode, rS, rA, SH);
	hCPU->gpr[rA] = hCPU->gpr[rS];
	hCPU->xer_ca = 0;
	if (hCPU->gpr[rA] & 0x80000000)
	{
		uint32 ca = 0;
		for (uint32 i = 0; i < SH; i++)
		{
			if (hCPU->gpr[rA] & 1)
				ca = 1;
			hCPU->gpr[rA] >>= 1;
			hCPU->gpr[rA] |= 0x80000000;
		}
		if (ca)
			hCPU->xer_ca = 1;
	}
	else
	{
		if (SH > 31)
			hCPU->gpr[rA] = 0;
		else
			hCPU->gpr[rA] >>= SH;
	}
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static uint32 _CNTLZW(uint32 v)
{
	uint32 result = 0;
	if (v == 0)
		return 32;
	if ((v & 0xFFFF0000) != 0) { result |= 16; v >>= 16; }
	if ((v & 0xFF00FF00) != 0) { result |= 8; v >>= 8; }
	if ((v & 0xF0F0F0F0) != 0) { result |= 4; v >>= 4; }
	if ((v & 0xCCCCCCCC) != 0) { result |= 2; v >>= 2; }
	if ((v & 0xAAAAAAAA) != 0) { result |= 1; }
	result = 31 - result;
	return result;
}

static void PPCInterpreter_CNTLZW(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	PPC_ASSERT(rB == 0);
	hCPU->gpr[rA] = _CNTLZW(hCPU->gpr[rD]);
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_EXTSB(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	PPC_ASSERT(rB == 0);
	hCPU->gpr[rA] = (uint32)(sint32)(sint8)hCPU->gpr[rD];
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_EXTSH(PPCInterpreter_t* hCPU, uint32 opcode)
{
	PPC_OPC_TEMPL3_XO();
	PPC_ASSERT(rB == 0);
	hCPU->gpr[rA] = (uint32)(sint32)(sint16)hCPU->gpr[rD];
	if (opHasRC())
		ppc_update_cr0(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_CMP(PPCInterpreter_t* hCPU, uint32 opcode)
{
	uint32 cr;
	sint32 rA, rB;
	PPC_OPC_TEMPL_X(opcode, cr, rA, rB);
	cr >>= 2;
	sint32 a = hCPU->gpr[rA];
	sint32 b = hCPU->gpr[rB];
	hCPU->cr[cr * 4 + 0] = 0;
	hCPU->cr[cr * 4 + 1] = 0;
	hCPU->cr[cr * 4 + 2] = 0;
	hCPU->cr[cr * 4 + 3] = 0;
	if (a < b)
		hCPU->cr[cr * 4 + CR_BIT_LT] = 1;
	else if (a > b)
		hCPU->cr[cr * 4 + CR_BIT_GT] = 1;
	else 
		hCPU->cr[cr * 4 + CR_BIT_EQ] = 1;
	if ((hCPU->spr.XER & XER_SO) != 0)
		hCPU->cr[cr * 4 + CR_BIT_SO] = 1;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_CMPL(PPCInterpreter_t* hCPU, uint32 opcode)
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(opcode, cr, rA, rB);
	cr >>= 2;
	uint32 a = hCPU->gpr[rA];
	uint32 b = hCPU->gpr[rB];
	hCPU->cr[cr * 4 + 0] = 0;
	hCPU->cr[cr * 4 + 1] = 0;
	hCPU->cr[cr * 4 + 2] = 0;
	hCPU->cr[cr * 4 + 3] = 0;
	if (a < b)
		hCPU->cr[cr * 4 + CR_BIT_LT] = 1;
	else if (a > b)
		hCPU->cr[cr * 4 + CR_BIT_GT] = 1;
	else
		hCPU->cr[cr * 4 + CR_BIT_EQ] = 1;
	if ((hCPU->spr.XER & XER_SO) != 0)
		hCPU->cr[cr * 4 + CR_BIT_SO] = 1;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_CMPI(PPCInterpreter_t* hCPU, uint32 opcode)
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, cr, rA, imm);
	cr >>= 2;
	sint32 a = hCPU->gpr[rA];
	sint32 b = imm;
	hCPU->cr[cr * 4 + 0] = 0;
	hCPU->cr[cr * 4 + 1] = 0;
	hCPU->cr[cr * 4 + 2] = 0;
	hCPU->cr[cr * 4 + 3] = 0;
	if (a < b)
		hCPU->cr[cr * 4 + CR_BIT_LT] = 1;
	else if (a > b)
		hCPU->cr[cr * 4 + CR_BIT_GT] = 1;
	else 
		hCPU->cr[cr * 4 + CR_BIT_EQ] = 1;
	if (hCPU->spr.XER & XER_SO)
		hCPU->cr[cr * 4 + CR_BIT_SO] = 1;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_CMPLI(PPCInterpreter_t* hCPU, uint32 opcode)
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(opcode, cr, rA, imm);
	cr >>= 2;
	uint32 a = hCPU->gpr[rA];
	uint32 b = imm;
	hCPU->cr[cr * 4 + 0] = 0;
	hCPU->cr[cr * 4 + 1] = 0;
	hCPU->cr[cr * 4 + 2] = 0;
	hCPU->cr[cr * 4 + 3] = 0;
	if (a < b)
		hCPU->cr[cr * 4 + CR_BIT_LT] = 1;
	else if (a > b)
		hCPU->cr[cr * 4 + CR_BIT_GT] = 1;
	else
		hCPU->cr[cr * 4 + CR_BIT_EQ] = 1;
	if (hCPU->spr.XER & XER_SO)
		hCPU->cr[cr * 4 + CR_BIT_SO] = 1;
	PPCInterpreter_nextInstruction(hCPU);
}

