
#define _signExtend16To32(__v) ((uint32)(sint32)(sint16)(__v))

// store

#define DSI_EXIT() \
	if constexpr(ppcItpCtrl::allowDSI) \
	{ \
		if (hCPU->memoryException) \
		{ \
			hCPU->memoryException = false; \
			return; \
		} \
	}

static void PPCInterpreter_STW(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rS, rA, imm);
	if (rA != 0)
	{
		ppcItpCtrl::ppcMem_writeDataU32(hCPU, hCPU->gpr[rA] + imm, hCPU->gpr[rS]);
	}
	else
	{
		PPC_ASSERT(true);
	}
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STWU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rS, rA, imm);
	ppcItpCtrl::ppcMem_writeDataU32(hCPU, hCPU->gpr[rA] + imm, hCPU->gpr[rS]);
	// check for rA != 0 ? 
	hCPU->gpr[rA] += imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STWX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);
	ppcItpCtrl::ppcMem_writeDataU32(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], hCPU->gpr[rS]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STWCX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	// http://www.ibm.com/developerworks/library/pa-atom/
	sint32 rA, rS, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);
	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];
	// check if we hold a reservation for the memory location

	// todo - this isnt accurate. STWCX can succeed even with a different EA if the reserved value remained untouched
	if (hCPU->reservedMemAddr == ea)
	{
		uint32be reservedValue = hCPU->reservedMemValue; // this is the value we expect in memory (if it does not match, STWCX fails)
		std::atomic<uint32be>* wordPtr;		
		if constexpr(ppcItpCtrl::allowSupervisorMode)
		{
			wordPtr = _rawPtrToAtomic((uint32be*)(memory_base + ppcItpCtrl::ppcMem_translateVirtualDataToPhysicalAddr(hCPU, ea)));
			DSI_EXIT();
		}
		else
		{
			wordPtr = _rawPtrToAtomic((uint32be*)memory_getPointerFromVirtualOffset(ea));
		}
		uint32be newValue = hCPU->gpr[rS];
		if (!wordPtr->compare_exchange_strong(reservedValue, newValue))
		{
			// failed
			ppc_setCRBit(hCPU, CR_BIT_LT, 0);
			ppc_setCRBit(hCPU, CR_BIT_GT, 0);
			ppc_setCRBit(hCPU, CR_BIT_EQ, 0);
		}
		else
		{
			// success, new value has been written
			ppc_setCRBit(hCPU, CR_BIT_LT, 0);
			ppc_setCRBit(hCPU, CR_BIT_GT, 0);
			ppc_setCRBit(hCPU, CR_BIT_EQ, 1);
		}
		ppc_setCRBit(hCPU, CR_BIT_SO, (hCPU->spr.XER&XER_SO) != 0 ? 1 : 0);
		// remove reservation
		hCPU->reservedMemAddr = 0;
		hCPU->reservedMemValue = 0;
	}
	else
	{
		// failed
		ppc_setCRBit(hCPU, CR_BIT_LT, 0);
		ppc_setCRBit(hCPU, CR_BIT_GT, 0);
		ppc_setCRBit(hCPU, CR_BIT_EQ, 0);
	}
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STWUX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);
	ppcItpCtrl::ppcMem_writeDataU32(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], hCPU->gpr[rS]);
	if (rA)
		hCPU->gpr[rA] += hCPU->gpr[rB];
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STWBRX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);
	ppcItpCtrl::ppcMem_writeDataU32(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], _swapEndianU32(hCPU->gpr[rS]));
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STMW(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rS, rA, imm);
	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + imm;
	while (rS <= 31)
	{
		ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea, hCPU->gpr[rS]);
		rS++;
		ea += 4;
	}
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STH(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rS, rA, imm);
	ppcItpCtrl::ppcMem_writeDataU16(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm, (uint16)hCPU->gpr[rS]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STHU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rS, rA, imm);
	ppcItpCtrl::ppcMem_writeDataU16(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm, (uint16)hCPU->gpr[rS]);
	if (rA)
		hCPU->gpr[rA] += imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STHX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);
	ppcItpCtrl::ppcMem_writeDataU16(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], (uint16)hCPU->gpr[rS]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STHUX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);
	ppcItpCtrl::ppcMem_writeDataU16(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], (uint16)hCPU->gpr[rS]);
	if (rA)
		hCPU->gpr[rA] += hCPU->gpr[rB];
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STHBRX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);
	ppcItpCtrl::ppcMem_writeDataU16(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], _swapEndianU16((uint16)hCPU->gpr[rS]));
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STB(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rS, rA, imm);
	ppcItpCtrl::ppcMem_writeDataU8(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm, (uint8)hCPU->gpr[rS]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STBU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rS, rA, imm);
	ppcItpCtrl::ppcMem_writeDataU8(hCPU, hCPU->gpr[rA] + imm, (uint8)hCPU->gpr[rS]);
	hCPU->gpr[rA] += imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STBX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);
	ppcItpCtrl::ppcMem_writeDataU8(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], (uint8)hCPU->gpr[rS]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STBUX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);
	ppcItpCtrl::ppcMem_writeDataU8(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], (uint8)hCPU->gpr[rS]);
	if (rA)
		hCPU->gpr[rA] += hCPU->gpr[rB];
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STSWI(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rS, nb;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, nb);
	if (nb == 0) nb = 32;
	uint32 ea = rA ? hCPU->gpr[rA] : 0;
	uint32 r = 0;
	int i = 0;
	while (nb > 0)
	{
		if (i == 0)
		{
			r = hCPU->gpr[rS];
			rS++;
			rS %= 32;
			i = 4;
		}
		ppcItpCtrl::ppcMem_writeDataU8(hCPU, ea, (r >> 24));
		r <<= 8;
		ea++;
		i--;
		nb--;
	}
	PPCInterpreter_nextInstruction(hCPU);
}

// load

static void PPCInterpreter_LWZ(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rD, rA, imm);
	uint32 v = ppcItpCtrl::ppcMem_readDataU32(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm);
	DSI_EXIT();
	hCPU->gpr[rD] = v;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LWZU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rD, rA, imm);
	hCPU->gpr[rA] += imm;
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU32(hCPU, hCPU->gpr[rA]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LMW(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rD, rA, imm);
	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + imm;
	while (rD <= 31)
	{
		hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU32(hCPU, ea);
		rD++;
		ea += 4;
	}
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LWZX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU32(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LWZXU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU32(hCPU, ea);
	if (rA && rA != rD)
		hCPU->gpr[rA] = ea;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LWBRX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	hCPU->gpr[rD] = CPU_swapEndianU32(ppcItpCtrl::ppcMem_readDataU32(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB]));

	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LWARX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU32(hCPU, ea);
	// set reservation	
	hCPU->reservedMemAddr = ea;
	hCPU->reservedMemValue = hCPU->gpr[rD];
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LHZ(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rD, rA, imm);
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU16(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LHZU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rD, rA, imm);
	// FIXME: rA!=0
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU16(hCPU, hCPU->gpr[rA] + imm);
	hCPU->gpr[rA] += imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LHZX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU16(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LHZUX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU16(hCPU, ea);
	if (rA && rA != rD)
		hCPU->gpr[rA] = ea;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LHBRX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	hCPU->gpr[rD] = CPU_swapEndianU16(ppcItpCtrl::ppcMem_readDataU16(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB]));
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LHA(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rD, rA, imm);
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU16(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm);
	hCPU->gpr[rD] = _signExtend16To32(hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LHAU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rD, rA, imm);
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU16(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm);
	if (rA && rA != rD)
		hCPU->gpr[rA] += imm;
	hCPU->gpr[rD] = _signExtend16To32(hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LHAUX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU16(hCPU, ea);
	if (rA && rA != rD)
		hCPU->gpr[rA] = ea;
	hCPU->gpr[rD] = _signExtend16To32(hCPU->gpr[rD]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LHAX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rS, rB;
	PPC_OPC_TEMPL_X(Opcode, rS, rA, rB);

	hCPU->gpr[rS] = ppcItpCtrl::ppcMem_readDataU16(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB]);
	hCPU->gpr[rS] = _signExtend16To32(hCPU->gpr[rS]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LBZ(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rD, rA, imm);
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU8(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LBZX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU8(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LBZXU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD, rB;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, rB);
	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];
	hCPU->gpr[rD] = ppcItpCtrl::ppcMem_readDataU8(hCPU, ea);
	if (rA && rA != rD)
		hCPU->gpr[rA] = ea;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LBZU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, rD, rA, imm);
	PPC_ASSERT(rA == 0);
	uint8 r;
	uint32 ea = hCPU->gpr[rA] + imm;
	hCPU->gpr[rA] = ea;
	r = ppcItpCtrl::ppcMem_readDataU8(hCPU, ea);
	hCPU->gpr[rD] = r;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LSWI(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	sint32 rA, rD, nb;
	PPC_OPC_TEMPL_X(Opcode, rD, rA, nb);
	if (nb == 0)
		nb = 32;

	uint32 ea = rA ? hCPU->gpr[rA] : 0;
	uint32 r = 0;
	int i = 4;
	uint8 v;
	while (nb>0)
	{
		if (i == 0)
		{
			i = 4;
			hCPU->gpr[rD] = r;
			rD++;
			rD %= 32;
			r = 0;
		}
		v = ppcItpCtrl::ppcMem_readDataU8(hCPU, ea);
		r <<= 8;
		r |= v;
		ea++;
		i--;
		nb--;
	}
	while (i)
	{
		r <<= 8;
		i--;
	}
	hCPU->gpr[rD] = r;
	PPCInterpreter_nextInstruction(hCPU);
}

// floating point load

static void PPCInterpreter_LFS(PPCInterpreter_t* hCPU, uint32 Opcode) //Copied
{
	FPUCheckAvailable();
	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, frD, rA, imm);

	uint64 val;
	//*(uint32*)&Val = ppcItpCtrl::ppcMem_readDataU32(hCPU, (rA?hCPU->gpr[rA]:0)+imm);
	val = ppcItpCtrl::ppcMem_readDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm);

	if (PPC_LSQE)
		hCPU->fpr[frD].fp0int = hCPU->fpr[frD].fp1int = val;
	else
		hCPU->fpr[frD].fp0int = val;

	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LFSX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	sint32 rA, frD, rB;
	PPC_OPC_TEMPL_X(Opcode, frD, rA, rB);

	uint64 val;
	val = ppcItpCtrl::ppcMem_readDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB]);

	if (PPC_LSQE)
		hCPU->fpr[frD].fp0int = hCPU->fpr[frD].fp1int = val;
	else
		hCPU->fpr[frD].fp0int = val;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LFSUX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	sint32 rA, frD, rB;
	PPC_OPC_TEMPL_X(Opcode, frD, rA, rB);

	uint64 Val;
	//*(uint32*)&Val = ppcItpCtrl::ppcMem_readDataU32(hCPU, (rA?hCPU->gpr[rA]:0)+hCPU->gpr[rB]);
	Val = ppcItpCtrl::ppcMem_readDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB]);
	if (rA)
		hCPU->gpr[rA] += hCPU->gpr[rB];

	if (PPC_LSQE)
		hCPU->fpr[frD].fp0int = hCPU->fpr[frD].fp1int = Val;
	else
		hCPU->fpr[frD].fp0int = Val;//ppcItpCtrl::ppcMem_readDataFloat((rA?hCPU->gpr[rA]:0)+hCPU->gpr[rB]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LFSU(PPCInterpreter_t* hCPU, uint32 Opcode) //Copied
{
	FPUCheckAvailable();
	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, frD, rA, imm);
	uint64 Val;

	//(uint32*)&Val = ppcItpCtrl::ppcMem_readDataU32(hCPU, (rA?hCPU->gpr[rA]:0)+imm);
	Val = ppcItpCtrl::ppcMem_readDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm);


	if (PPC_LSQE)
		hCPU->fpr[frD].fp0int = hCPU->fpr[frD].fp1int = Val;
	else
		hCPU->fpr[frD].fp0int = Val;

	if (rA)
		hCPU->gpr[rA] += imm;

	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LFD(PPCInterpreter_t* hCPU, uint32 Opcode) //Copied
{
	FPUCheckAvailable();
	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, frD, rA, imm);
	hCPU->fpr[frD].fpr = ppcItpCtrl::ppcMem_readDataDouble(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm);//ppcItpCtrl::ppcMem_readDataQUAD((rA?hCPU->gpr[rA]:0)+imm);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LFDU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, frD, rA, imm);

	hCPU->fpr[frD].fpr = ppcItpCtrl::ppcMem_readDataDouble(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm);//ppcItpCtrl::ppcMem_readDataQUAD((rA?hCPU->gpr[rA]:0)+imm);
	if (rA)
		hCPU->gpr[rA] += imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LFDX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	sint32 rA, frD, rB;
	PPC_OPC_TEMPL_X(Opcode, frD, rA, rB);
	hCPU->fpr[frD].fpr = ppcItpCtrl::ppcMem_readDataDouble(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB]);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_LFDUX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	sint32 rA, frD, rB;
	PPC_OPC_TEMPL_X(Opcode, frD, rA, rB);
	hCPU->fpr[frD].fpr = ppcItpCtrl::ppcMem_readDataDouble(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB]);
	if (rA)
		hCPU->gpr[rA] += hCPU->gpr[rB];
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STFS(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, frD, rA, imm);
	if (PPC_LSQE)
		ppcItpCtrl::ppcMem_writeDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm, hCPU->fpr[frD].fp0int);
	else
		ppcItpCtrl::ppcMem_writeDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm, hCPU->fpr[frD].fp0int);
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STFSU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, frD, rA, imm);

	if (PPC_LSQE)
		ppcItpCtrl::ppcMem_writeDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm, hCPU->fpr[frD].fp0int);
	else
		ppcItpCtrl::ppcMem_writeDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm, hCPU->fpr[frD].fp0int);

	if (rA)
		hCPU->gpr[rA] += imm;
	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STFSX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	sint32 rA, frS, rB;
	PPC_OPC_TEMPL_X(Opcode, frS, rA, rB);

	if (PPC_LSQE)
		ppcItpCtrl::ppcMem_writeDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], hCPU->fpr[frS].fp0int);
	else
		ppcItpCtrl::ppcMem_writeDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], hCPU->fpr[frS].fp0int);

	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_STFSUX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);

	int rA, frS, rB;
	PPC_OPC_TEMPL_X(Opcode, frS, rA, rB);

	if (PPC_LSQE)
		ppcItpCtrl::ppcMem_writeDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], hCPU->fpr[frS].fp0int);
	else
		ppcItpCtrl::ppcMem_writeDataFloatEx(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], hCPU->fpr[frS].fp0int);

	if (rA)
		hCPU->gpr[rA] += hCPU->gpr[rB];
}


static void PPCInterpreter_STFD(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);

	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, frD, rA, imm);

	ppcItpCtrl::ppcMem_writeDataDouble(hCPU, (rA ? hCPU->gpr[rA] : 0) + imm, hCPU->fpr[frD].fpr);

	// debug output
#ifdef __DEBUG_OUTPUT_INSTRUCTION
	debug_printf("STFD f%d, %d(r%d)\n", frD, imm, rA);
#endif
}

static void PPCInterpreter_STFDU(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);

	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(Opcode, frD, rA, imm);

	if (rA)
	{
		hCPU->gpr[rA] += imm;
	}
	else
	{
		PPC_ASSERT(true);
	}

	ppcItpCtrl::ppcMem_writeDataDouble(hCPU, hCPU->gpr[rA], hCPU->fpr[frD].fpr);

	// debug output
#ifdef __DEBUG_OUTPUT_INSTRUCTION
	debug_printf("STFD f%d, %d(r%d)\n", frD, imm, rA);
#endif
}

static void PPCInterpreter_STFDX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);

	int rA, frS, rB;
	PPC_OPC_TEMPL_X(Opcode, frS, rA, rB);

	ppcItpCtrl::ppcMem_writeDataDouble(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], hCPU->fpr[frS].fpr);

	// debug output
#ifdef __DEBUG_OUTPUT_INSTRUCTION
	debug_printf("STFD f%d, r%d+r%d\n", frS, rA, rB);
#endif
}

static void PPCInterpreter_STFDUX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);

	int rA, frS, rB;
	PPC_OPC_TEMPL_X(Opcode, frS, rA, rB);

	if (rA == 0)
	{
		ppcItpCtrl::ppcMem_writeDataDouble(hCPU, hCPU->gpr[rB], hCPU->fpr[frS].fpr);
	}
	else
	{
		hCPU->gpr[rA] += hCPU->gpr[rB];
		ppcItpCtrl::ppcMem_writeDataDouble(hCPU, hCPU->gpr[rA], hCPU->fpr[frS].fpr);
	}

}

static void PPCInterpreter_STFIWX(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	FPUCheckAvailable();
	sint32 rA, frS, rB;
	PPC_OPC_TEMPL_X(Opcode, frS, rA, rB);

	uint32 val = (uint32)hCPU->fpr[frS].fp0int;
	ppcItpCtrl::ppcMem_writeDataU32(hCPU, (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB], val);
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);
}

// paired single

// ST_TYPE:
// 4 - uint8
// 5 - uint16
// 6 - sint8
// 7 - sint16
// 0 - float32

#define LD_SCALE(n) ((hCPU->spr.UGQR[0+n] >> 24) & 0x3f)
#define LD_TYPE(n)  ((hCPU->spr.UGQR[0+n] >> 16) & 7)
#define ST_SCALE(n) ((hCPU->spr.UGQR[0+n] >>  8) & 0x3f)
#define ST_TYPE(n)  ((hCPU->spr.UGQR[0+n]      ) & 7)
#define PSW         (opcode & 0x8000)
#define PSI         ((opcode >> 12) & 7)

#define PSWX         (opcode & (1<<(7+3)))
#define PSIX         ((opcode >> 7) & 7)

static void PPCInterpreter_PSQ_ST(PPCInterpreter_t* hCPU, unsigned int opcode)
{
	FPUCheckAvailable();
	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, imm);

	uint32 ea = _uint32_fastSignExtend(imm, 11);

	ea += (rA ? hCPU->gpr[rA] : 0);

	sint32 type = ST_TYPE(PSI);
	uint8 scale = (uint8)ST_SCALE(PSI);

	if (opcode & 0x8000) // PSW?
	{
		if ((type == 4) || (type == 6)) ppcItpCtrl::ppcMem_writeDataU8(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else if ((type == 5) || (type == 7)) ppcItpCtrl::ppcMem_writeDataU16(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));
	}
	else
	{
		if ((type == 4) || (type == 6)) ppcItpCtrl::ppcMem_writeDataU8(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else if ((type == 5) || (type == 7)) ppcItpCtrl::ppcMem_writeDataU16(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));

		if ((type == 4) || (type == 6)) ppcItpCtrl::ppcMem_writeDataU8(hCPU, ea + 1, quantize((float)hCPU->fpr[frD].fp1, type, scale));
		else if ((type == 5) || (type == 7)) ppcItpCtrl::ppcMem_writeDataU16(hCPU, ea + 2, quantize((float)hCPU->fpr[frD].fp1, type, scale));
		else ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea + 4, quantize((float)hCPU->fpr[frD].fp1, type, scale));
	}

	PPCInterpreter_nextInstruction(hCPU);
}

static void PPCInterpreter_PSQ_STU(PPCInterpreter_t* hCPU, unsigned int opcode)
{
	FPUCheckAvailable();
	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, imm);
	uint32 ea = _uint32_fastSignExtend(imm, 11);

	if (ea & 0x800)
		ea |= 0xfffff000;

	ea += (rA ? hCPU->gpr[rA] : 0);
	if (rA)
		hCPU->gpr[rA] = ea;

	sint32 type = ST_TYPE((opcode >> 12) & 0x7);
	uint8 scale = (uint8)ST_SCALE(PSI);

	if (opcode & 0x8000) //PSW?
	{
		if ((type == 4) || (type == 6)) ppcItpCtrl::ppcMem_writeDataU8(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else if ((type == 5) || (type == 7)) ppcItpCtrl::ppcMem_writeDataU16(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));
	}
	else
	{
		if ((type == 4) || (type == 6)) ppcItpCtrl::ppcMem_writeDataU8(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else if ((type == 5) || (type == 7)) ppcItpCtrl::ppcMem_writeDataU16(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea, quantize((float)hCPU->fpr[frD].fp0, type, scale));

		if ((type == 4) || (type == 6)) ppcItpCtrl::ppcMem_writeDataU8(hCPU, ea + 1, quantize((float)hCPU->fpr[frD].fp1, type, scale));
		else if ((type == 5) || (type == 7)) ppcItpCtrl::ppcMem_writeDataU16(hCPU, ea + 2, quantize((float)hCPU->fpr[frD].fp1, type, scale));
		else ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea + 4, quantize((float)hCPU->fpr[frD].fp1, type, scale));
	}

	PPCInterpreter_nextInstruction(hCPU);
}


static void PPCInterpreter_PSQ_STX(PPCInterpreter_t* hCPU, unsigned int opcode)
{
	FPUCheckAvailable();

	// next instruction
	PPCInterpreter_nextInstruction(hCPU);

	sint32 frD;
	uint32 rA, rB;
	frD = (opcode >> (31 - 10)) & 0x1F;
	rA = (opcode >> (31 - 15)) & 0x1F;
	rB = (opcode >> (31 - 20)) & 0x1F;
	uint32 EA = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];

	sint32 type = ST_TYPE(PSIX);
	uint8 scale = (uint8)ST_SCALE(PSIX);

	if (PSWX)
	{
		if ((type == 4) || (type == 6)) ppcItpCtrl::ppcMem_writeDataU8(hCPU, EA, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else if ((type == 5) || (type == 7)) ppcItpCtrl::ppcMem_writeDataU16(hCPU, EA, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else ppcItpCtrl::ppcMem_writeDataU32(hCPU, EA, quantize((float)hCPU->fpr[frD].fp0, type, scale));
	}
	else
	{
		if ((type == 4) || (type == 6)) ppcItpCtrl::ppcMem_writeDataU8(hCPU, EA, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else if ((type == 5) || (type == 7)) ppcItpCtrl::ppcMem_writeDataU16(hCPU, EA, quantize((float)hCPU->fpr[frD].fp0, type, scale));
		else ppcItpCtrl::ppcMem_writeDataU32(hCPU, EA, quantize((float)hCPU->fpr[frD].fp0, type, scale));

		if ((type == 4) || (type == 6)) ppcItpCtrl::ppcMem_writeDataU8(hCPU, EA + 1, quantize((float)hCPU->fpr[frD].fp1, type, scale));
		else if ((type == 5) || (type == 7)) ppcItpCtrl::ppcMem_writeDataU16(hCPU, EA + 2, quantize((float)hCPU->fpr[frD].fp1, type, scale));
		else ppcItpCtrl::ppcMem_writeDataU32(hCPU, EA + 4, quantize((float)hCPU->fpr[frD].fp1, type, scale));
	}
}

static void PPCInterpreter_PSQ_L(PPCInterpreter_t* hCPU, unsigned int opcode)
{
	FPUCheckAvailable();
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);

	sint32 rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, imm);

	uint32 EA, data0 = 0, data1 = 0;
	sint32 type = LD_TYPE(PSI);
	uint8 scale = (uint8)LD_SCALE(PSI);

	EA = _uint32_fastSignExtend(opcode, 11);

	if (rA) EA += hCPU->gpr[rA];

	if (opcode & 0x8000)
	{
		if ((type == 4) || (type == 6)) *(uint8*)&data0 = ppcItpCtrl::ppcMem_readDataU8(hCPU, EA);
		else if ((type == 5) || (type == 7)) *(uint16*)&data0 = ppcItpCtrl::ppcMem_readDataU16(hCPU, EA);
		else *(uint32*)&data0 = ppcItpCtrl::ppcMem_readDataU32(hCPU, EA);
		if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
		if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

		hCPU->fpr[frD].fp0 = (double)dequantize(data0, type, scale);
		hCPU->fpr[frD].fp1 = 1.0f;
	}
	else
	{
		if ((type == 4) || (type == 6))
		{
			*(uint8*)&data0 = ppcItpCtrl::ppcMem_readDataU8(hCPU, EA);
			*(uint8*)&data1 = ppcItpCtrl::ppcMem_readDataU8(hCPU, EA + 1);
		}
		else if ((type == 5) || (type == 7))
		{
			*(uint16*)&data0 = ppcItpCtrl::ppcMem_readDataU16(hCPU, EA);
			*(uint16*)&data1 = ppcItpCtrl::ppcMem_readDataU16(hCPU, EA + 2);
		}
		else
		{
			*(uint32*)&data0 = ppcItpCtrl::ppcMem_readDataU32(hCPU, EA);
			*(uint32*)&data1 = ppcItpCtrl::ppcMem_readDataU32(hCPU, EA + 4);
		}
		if (type == 6)
		{
			if (data0 & 0x80) data0 |= 0xffffff00;
			if (data1 & 0x80) data1 |= 0xffffff00;
		}
		if (type == 7)
		{
			if (data0 & 0x8000) data0 |= 0xffff0000;
			if (data1 & 0x8000) data1 |= 0xffff0000;
		}

		hCPU->fpr[frD].fp0 = (double)dequantize(data0, type, scale);
		hCPU->fpr[frD].fp1 = (double)dequantize(data1, type, scale);
	}
}

static void PPCInterpreter_PSQ_LU(PPCInterpreter_t* hCPU, unsigned int opcode)
{
	FPUCheckAvailable();
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);

	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(opcode, frD, rA, imm);

	uint32 EA = opcode & 0xfff, data0 = 0, data1 = 0;
	sint32 type = LD_TYPE(PSI);
	uint8 scale = (uint8)LD_SCALE(PSI);

	if (EA & 0x800) EA |= 0xfffff000;

	if (rA)
	{
		EA += hCPU->gpr[rA];
		hCPU->gpr[rA] = EA;
	}

	if (opcode & 0x8000)
	{
		if ((type == 4) || (type == 6)) *(uint8*)&data0 = ppcItpCtrl::ppcMem_readDataU8(hCPU, EA);
		else if ((type == 5) || (type == 7)) *(uint16*)&data0 = ppcItpCtrl::ppcMem_readDataU16(hCPU, EA);
		else *(uint32*)&data0 = ppcItpCtrl::ppcMem_readDataU32(hCPU, EA);
		if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
		if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

		hCPU->fpr[frD].fp0 = (double)dequantize(data0, type, scale);
		hCPU->fpr[frD].fp1 = 1.0f;
	}
	else
	{
		if ((type == 4) || (type == 6)) *(uint8*)&data0 = ppcItpCtrl::ppcMem_readDataU8(hCPU, EA);
		else if ((type == 5) || (type == 7)) *(uint16*)&data0 = ppcItpCtrl::ppcMem_readDataU16(hCPU, EA);
		else *(uint32*)&data0 = ppcItpCtrl::ppcMem_readDataU32(hCPU, EA);
		if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
		if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

		if ((type == 4) || (type == 6)) *(uint8*)&data1 = ppcItpCtrl::ppcMem_readDataU8(hCPU, EA + 1);
		else if ((type == 5) || (type == 7)) *(uint16*)&data1 = ppcItpCtrl::ppcMem_readDataU16(hCPU, EA + 2);
		else *(uint32*)&data1 = ppcItpCtrl::ppcMem_readDataU32(hCPU, EA + 4);
		if (type == 6) if (data1 & 0x80) data1 |= 0xffffff00;
		if (type == 7) if (data1 & 0x8000) data1 |= 0xffff0000;

		hCPU->fpr[frD].fp0 = (double)dequantize(data0, type, scale);
		hCPU->fpr[frD].fp1 = (double)dequantize(data1, type, scale);
	}
}

static void PPCInterpreter_PSQ_LX(PPCInterpreter_t* hCPU, unsigned int opcode)
{
	FPUCheckAvailable();
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);

	sint32 frD;
	uint32 rA, rB;

	frD = (opcode >> (32 - 11)) & 0x1F;
	rA = (opcode >> (32 - 16)) & 0x1F;
	rB = (opcode >> (32 - 21)) & 0x1F;

	uint32 EA = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];

	uint32 data0 = 0, data1 = 0;
	sint32 type = LD_TYPE(PSIX);
	uint8 scale = (uint8)LD_SCALE(PSIX);

	if (PSWX)
	{
		if ((type == 4) || (type == 6)) *(uint8*)&data0 = ppcItpCtrl::ppcMem_readDataU8(hCPU, EA);
		else if ((type == 5) || (type == 7)) *(uint16*)&data0 = ppcItpCtrl::ppcMem_readDataU16(hCPU, EA);
		else *(uint32*)&data0 = ppcItpCtrl::ppcMem_readDataU32(hCPU, EA);
		if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
		if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

		hCPU->fpr[frD].fp0 = (double)dequantize(data0, type, scale);
		hCPU->fpr[frD].fp1 = 1.0f;
	}
	else
	{
		if ((type == 4) || (type == 6)) *(uint8*)&data0 = ppcItpCtrl::ppcMem_readDataU8(hCPU, EA);
		else if ((type == 5) || (type == 7)) *(uint16*)&data0 = ppcItpCtrl::ppcMem_readDataU16(hCPU, EA);
		else *(uint32*)&data0 = ppcItpCtrl::ppcMem_readDataU32(hCPU, EA);
		if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
		if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

		if ((type == 4) || (type == 6)) *(uint8*)&data1 = ppcItpCtrl::ppcMem_readDataU8(hCPU, EA + 1);
		else if ((type == 5) || (type == 7)) *(uint16*)&data1 = ppcItpCtrl::ppcMem_readDataU16(hCPU, EA + 2);
		else *(uint32*)&data1 = ppcItpCtrl::ppcMem_readDataU32(hCPU, EA + 4);
		if (type == 6) if (data1 & 0x80) data1 |= 0xffffff00;
		if (type == 7) if (data1 & 0x8000) data1 |= 0xffff0000;

		hCPU->fpr[frD].fp0 = (double)dequantize(data0, type, scale);
		hCPU->fpr[frD].fp1 = (double)dequantize(data1, type, scale);
	}
}

// misc

static void PPCInterpreter_DCBZ(PPCInterpreter_t* hCPU, uint32 Opcode)
{
	int rA, rB;
	rA = (Opcode >> (31 - 15)) & 0x1F;
	rB = (Opcode >> (31 - 20)) & 0x1F;

	uint32 ea = (rA ? hCPU->gpr[rA] : 0) + hCPU->gpr[rB];
	ea &= ~31;
	if constexpr(ppcItpCtrl::allowSupervisorMode)
	{
		// todo - optimize
		ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea + 0, 0);
		DSI_EXIT();
		ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea + 4, 0);
		ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea + 8, 0);
		ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea + 12, 0);
		ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea + 16, 0);
		ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea + 20, 0);
		ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea + 24, 0);
		ppcItpCtrl::ppcMem_writeDataU32(hCPU, ea + 28, 0);
	}
	else
	{
		memset((void*)memory_getPointerFromVirtualOffset(ea), 0x00, 0x20);
	}

	// debug output
#ifdef __DEBUG_OUTPUT_INSTRUCTION
	debug_printf("DCBZ\n");
#endif
	// next instruction
	PPCInterpreter_nextInstruction(hCPU);
}