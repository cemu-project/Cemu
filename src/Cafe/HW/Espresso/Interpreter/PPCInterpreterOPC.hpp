
static void PPCInterpreter_MFSPR(PPCInterpreter_t* hCPU, uint32 opcode)
{
	uint32 rD, spr1, spr2, spr;
	PPC_OPC_TEMPL_XO(opcode, rD, spr1, spr2);
	spr = spr1 | (spr2 << 5);
	// copy SPR
	hCPU->gpr[rD] = PPCSpr_get(hCPU, spr);
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_MTSPR(PPCInterpreter_t* hCPU, uint32 opcode)
{
	uint32 rD, spr1, spr2, spr;
	PPC_OPC_TEMPL_XO(opcode, rD, spr1, spr2);
	spr = spr1 | (spr2 << 5);
	PPCSpr_set(hCPU, spr, hCPU->gpr[rD]);
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_MFSR(PPCInterpreter_t* hCPU, uint32 opcode)
{
	uint32 rD, SR, rB;
	PPC_OPC_TEMPL_X(opcode, rD, SR, rB);
	hCPU->gpr[rD] = getSR(hCPU, SR & 0xF);
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_MTSR(PPCInterpreter_t* hCPU, uint32 opcode)
{
	uint32 rS, SR, rB;
	PPC_OPC_TEMPL_X(opcode, rS, SR, rB);
	setSR(hCPU, SR&0xF, hCPU->gpr[rS]);
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_MFTB(PPCInterpreter_t* hCPU, uint32 opcode)
{
	uint32 rD, spr1, spr2, spr;
	// get SPR ID
	PPC_OPC_TEMPL_XO(opcode, rD, spr1, spr2);
	spr = spr1 | (spr2 << 5);
	// get core cycle counter
	uint64 coreTime = ppcItpCtrl::getTB(hCPU);

	switch (spr)
	{
	case 268: // TBL
		hCPU->gpr[rD] = (uint32)(coreTime & 0xFFFFFFFF);
		break;
	case 269: // TBU
		hCPU->gpr[rD] = (uint32)((coreTime >> 32) & 0xFFFFFFFF);
		break;
	default:
		assert_dbg();
	}
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_TW(PPCInterpreter_t* hCPU, uint32 opcode)
{
	sint32 to, rA, rB;
	PPC_OPC_TEMPL_X(opcode, to, rA, rB);

	cemu_assert_debug(to == 0);

    if (rA == DEBUGGER_BP_T_DEBUGGER)
	    debugger_enterTW(hCPU);
    else if (rA == DEBUGGER_BP_T_GDBSTUB)
        g_gdbstub->HandleTrapInstruction(hCPU);
}
