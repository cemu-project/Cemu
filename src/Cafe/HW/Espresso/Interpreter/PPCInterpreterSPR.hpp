#define SPR_TBL_WRITE	(284)
#define SPR_TBU_WRITE	(285)

#define SPR_DBATU_0		(536)
#define SPR_DBATU_1		(538)
#define SPR_DBATU_2		(540)
#define SPR_DBATU_3		(542)
#define SPR_DBATU_4		(568)
#define SPR_DBATU_5		(570)
#define SPR_DBATU_6		(572)
#define SPR_DBATU_7		(574)

#define SPR_DBATL_0		(537)
#define SPR_DBATL_1		(539)
#define SPR_DBATL_2		(541)
#define SPR_DBATL_3		(543)
#define SPR_DBATL_4		(569)
#define SPR_DBATL_5		(571)
#define SPR_DBATL_6		(573)
#define SPR_DBATL_7		(575)

#define SPR_IBATU_0		(528)
#define SPR_IBATU_1		(530)
#define SPR_IBATU_2		(532)
#define SPR_IBATU_3		(534)
#define SPR_IBATU_4		(560)
#define SPR_IBATU_5		(562)
#define SPR_IBATU_6		(564)
#define SPR_IBATU_7		(566)

#define SPR_IBATL_0		(529)
#define SPR_IBATL_1		(531)
#define SPR_IBATL_2		(533)
#define SPR_IBATL_3		(535)
#define SPR_IBATL_4		(561)
#define SPR_IBATL_5		(563)
#define SPR_IBATL_6		(565)
#define SPR_IBATL_7		(567)

#define SPR_DSISR		(18)
#define SPR_DAR			(19)

#define SPR_SPRG0		(272)
#define SPR_SPRG1		(273)
#define SPR_SPRG2		(274)
#define SPR_SPRG3		(275)

//#define SPR_HID0		(1008)
//#define SPR_HID2		(920)
#define SPR_HID4		(1011)
#define SPR_HID5		(944)

#define SPR_L2CR		(1017) // L2 cache control

#define SPR_CAR			(948) // global
#define SPR_BCR			(949) // global

static uint32 getPVR(PPCInterpreter_t* hCPU)
{
	return 0x70010101; // guessed
}

static uint32 getFPECR(PPCInterpreter_t* hCPU)
{
	return hCPU->sprExtended.fpecr;
}

static void setFPECR(PPCInterpreter_t* hCPU, uint32 newValue)
{
	hCPU->sprExtended.fpecr = newValue;
}

static void setDEC(PPCInterpreter_t* hCPU, uint32 newValue)
{
	debug_printf("Set DEC to 0x%08x\n", newValue);
	//hCPU->sprExtended.fpecr = newValue;
}

static uint32 getSPRG(PPCInterpreter_t* hCPU, uint32 sprgIndex)
{
	return hCPU->sprExtended.sprg[sprgIndex];
}

static void setSPRG(PPCInterpreter_t* hCPU, uint32 sprgIndex, uint32 newValue)
{
	hCPU->sprExtended.sprg[sprgIndex] = newValue;
}

static uint32 getDAR(PPCInterpreter_t* hCPU)
{
	return hCPU->sprExtended.dar;
}

static uint32 getDSISR(PPCInterpreter_t* hCPU)
{
	return hCPU->sprExtended.dsisr;
}

static uint32 getHID0(PPCInterpreter_t* hCPU)
{
	return 0; // todo
}

static void setHID0(PPCInterpreter_t* hCPU, uint32 newValue)
{
	// todo
	debug_printf("Set HID0 to 0x%08x\n", newValue);
}

static uint32 getHID1(PPCInterpreter_t* hCPU)
{
	debug_printf("Get HID1 IP 0x%08x\n", hCPU->instructionPointer);
	return 0; // todo
}

static uint32 getHID2(PPCInterpreter_t* hCPU)
{
	debug_printf("Get HID2 IP 0x%08x\n", hCPU->instructionPointer);
	return 0; // todo
}

static void setHID2(PPCInterpreter_t* hCPU, uint32 newValue)
{
	// todo
	debug_printf("Set HID2 to 0x%08x\n", newValue);
}

static uint32 getHID4(PPCInterpreter_t* hCPU)
{
	debug_printf("Get HID4 IP 0x%08x\n", hCPU->instructionPointer);
	return 0; // todo
}

static void setHID4(PPCInterpreter_t* hCPU, uint32 newValue)
{
	// todo
	debug_printf("Set HID4 to 0x%08x\n", newValue);
}

static uint32 getHID5(PPCInterpreter_t* hCPU)
{
	// Wii-U only
	debug_printf("Get HID5 IP 0x%08x\n", hCPU->instructionPointer);
	return 0; // todo
}

static void setHID5(PPCInterpreter_t* hCPU, uint32 newValue)
{
	// Wii-U only
	// todo
	debug_printf("Set HID5 to 0x%08x\n", newValue);
}

static uint32 getSCR(PPCInterpreter_t* hCPU)
{
	// WiiU mode only?
	return 0; // todo
}

static void setSCR(PPCInterpreter_t* hCPU, uint32 newValue)
{
	uint32 previousSCR = hCPU->global->sprGlobal.scr;
	newValue |= (previousSCR&0x80000000); // this bit always sticks?
	if ((previousSCR&0x80000000) == 0 && (newValue & 0x80000000) != 0)
	{
		// this bit is used to disable bootrom mapping, but we use it to know when to copy the decrypted ancast image into kernel memory
		debug_printf("SCR MSB set. Unmap bootrom?\n");

		//memcpy(memory_base + 0xFFE00000, memory_base + 0x08000000, 0x180000);
		// hack - clear low memory (where bootrom was mapped/loaded)
		memset(memory_base, 0, 0x4000);
		//// todo - normally IOSU sets up some stuff here (probably)
		
		// for debugging purposes make lowest page read-only
#ifdef _WIN32
		DWORD oldProtect;
		VirtualProtect(memory_base, 0x1000, PAGE_READONLY, &oldProtect);
#endif
	}
	debug_printf("Set SCR to 0x%08x\n", newValue);
	hCPU->global->sprGlobal.scr = newValue;
}

// SCR probably has bits to control following:
// disable bootrom (bit 0x80000000)
// disable PPC OTP
// bits to start the extra cores

static uint32 getCAR(PPCInterpreter_t* hCPU)
{
	// global
	// WiiU mode only
	return 0; // todo
}

static void setCAR(PPCInterpreter_t* hCPU, uint32 newValue)
{
	// global
	// WiiU mode only
	debug_printf("Set CAR to 0x%08x\n", newValue);
}

static uint32 getBCR(PPCInterpreter_t* hCPU)
{
	// global
	// WiiU mode only
	return 0; // todo
}

static void setBCR(PPCInterpreter_t* hCPU, uint32 newValue)
{
	// global
	// WiiU mode only
	debug_printf("Set BCR to 0x%08x\n", newValue);
}


static uint32 getL2CR(PPCInterpreter_t* hCPU)
{
	return 0; // todo
}

static void setL2CR(PPCInterpreter_t* hCPU, uint32 newValue)
{
	// todo
}

static void setSRR0(PPCInterpreter_t* hCPU, uint32 newValue)
{
	hCPU->sprExtended.srr0 = newValue;
}

static void setSRR1(PPCInterpreter_t* hCPU, uint32 newValue)
{
	hCPU->sprExtended.srr1 = newValue;
}

static void setDMAU(PPCInterpreter_t* hCPU, uint32 newValue)
{
	hCPU->sprExtended.dmaU = newValue;
}

static void setDMAL(PPCInterpreter_t* hCPU, uint32 newValue)
{
	hCPU->sprExtended.dmaL = newValue;
	// LC DMA
	if(newValue &0x2 )
	{
		uint32 transferLength = (((hCPU->sprExtended.dmaU>>0)&0x1F)<<2)|((newValue>>2)&3);
		uint32 memAddr = (hCPU->sprExtended.dmaU)&0xFFFFFFE0;
		uint32 cacheAddr = (newValue)&0xFFFFFFE0;
		if( transferLength == 0 )
			transferLength = 128;
		transferLength *= 32;
		bool isLoad = ((newValue>>4)&1)!=0;
		if( (cacheAddr>>28) != 0xE )
		{
			debug_printf("LCTransfer: Not a cache address\n");
			cacheAddr = 0;
		}
		else
		{
			cacheAddr -= 0xE0000000;
		}
		if( isLoad == 0 )
		{
			// locked cache -> memory
			debug_printf("L2->MEM %08x -> %08x size: 0x%x\n", memAddr, 0xE0000000 + cacheAddr, transferLength);
			memcpy(memory_getPointerFromVirtualOffset(memAddr), memory_base+0xE0000000+cacheAddr, transferLength);
		}
		else
		{
			// memory -> locked cache
			debug_printf("MEM->L2 %08x -> %08x size: 0x%x\n", 0xE0000000 + cacheAddr, memAddr, transferLength);
			memcpy(memory_base + 0xE0000000 + cacheAddr, memory_getPointerFromVirtualOffset(memAddr), transferLength);
		}
		newValue &= ~2;
		hCPU->sprExtended.dmaL = newValue;
	}
}

static void setDBATL(PPCInterpreter_t* hCPU, uint32 index, uint32 newValue)
{
	debug_printf("Set DBATL%d to 0x%08x\n", index, newValue);
	hCPU->sprExtended.dbatL[index] = newValue;
}

static void setDBATU(PPCInterpreter_t* hCPU, uint32 index, uint32 newValue)
{
	debug_printf("Set DBATU%d to 0x%08x\n", index, newValue);
	hCPU->sprExtended.dbatU[index] = newValue;
}

static void setIBATL(PPCInterpreter_t* hCPU, uint32 index, uint32 newValue)
{
	debug_printf("Set IBATL%d to 0x%08x\n", index, newValue);
	hCPU->sprExtended.ibatL[index] = newValue;
}

static void setIBATU(PPCInterpreter_t* hCPU, uint32 index, uint32 newValue)
{
	debug_printf("Set IBATU%d to 0x%08x\n", index, newValue);
	hCPU->sprExtended.ibatU[index] = newValue;
}

static uint32 getDBATL(PPCInterpreter_t* hCPU, uint32 index)
{
	return hCPU->sprExtended.dbatL[index];
}

static uint32 getDBATU(PPCInterpreter_t* hCPU, uint32 index)
{
	return hCPU->sprExtended.dbatU[index];
}

static uint32 getIBATL(PPCInterpreter_t* hCPU, uint32 index)
{
	return hCPU->sprExtended.ibatL[index];
}

static uint32 getIBATU(PPCInterpreter_t* hCPU, uint32 index)
{
	return hCPU->sprExtended.ibatU[index];
}

static void setSR(PPCInterpreter_t* hCPU, uint32 index, uint32 newValue)
{
	debug_printf("Set SR%d to 0x%08x IP %08x LR %08x\n", index, newValue, hCPU->instructionPointer, hCPU->spr.LR);
	hCPU->sprExtended.sr[index] = newValue;
}

static uint32 getSR(PPCInterpreter_t* hCPU, uint32 index)
{
	return hCPU->sprExtended.sr[index];
}

static void setSDR1(PPCInterpreter_t* hCPU, uint32 newValue)
{
	debug_printf("Set SDR1 to 0x%08x\n", newValue);
	hCPU->sprExtended.sdr1 = newValue;
}

static void setTBL(PPCInterpreter_t* hCPU, uint32 newValue)
{
	if (newValue != 0)
		assert_dbg();
	debug_printf("Reset TB\n");
	hCPU->global->tb = 0;
}

static void setTBU(PPCInterpreter_t* hCPU, uint32 newValue)
{
	if (newValue != 0)
		assert_dbg();
	debug_printf("Reset TB\n");
	hCPU->global->tb = 0;
}

static void PPCSprSupervisor_set(PPCInterpreter_t* hCPU, uint32 spr, uint32 newValue)
{
	switch (spr)
	{
	case SPR_LR:
		hCPU->spr.LR = newValue;
		break;
	case SPR_CTR:
		hCPU->spr.CTR = newValue;
		break;
	case SPR_DEC:
		setDEC(hCPU, newValue);
		break;
	case SPR_XER:
		PPCInterpreter_setXER(hCPU, newValue);
		break;
	case SPR_UGQR0:
	case SPR_UGQR1:
	case SPR_UGQR2:
	case SPR_UGQR3:
	case SPR_UGQR4:
	case SPR_UGQR5:
	case SPR_UGQR6:
	case SPR_UGQR7:
		hCPU->spr.UGQR[spr - SPR_UGQR0] = newValue;
		break;
	// values above are user mode accessible
	case SPR_TBL_WRITE: // TBL
		setTBL(hCPU, newValue);
		break;
	case SPR_TBU_WRITE: // TBU
		setTBU(hCPU, newValue);
		break;
	case SPR_FPECR:
		setFPECR(hCPU, newValue);
		break;
	case SPR_HID0:
		setHID0(hCPU, newValue);
		break;
	case SPR_HID2:
		setHID2(hCPU, newValue);
		break;
	case SPR_HID4:
		setHID4(hCPU, newValue);
		break;
	case SPR_HID5:
		setHID5(hCPU, newValue);
		break;
	case SPR_L2CR:
		setL2CR(hCPU, newValue);
		break;
	case SPR_SRR0:
		setSRR0(hCPU, newValue);
		break;
	case SPR_SRR1:
		setSRR1(hCPU, newValue);
		break;
	case SPR_SPRG0:
		setSPRG(hCPU, 0, newValue);
		break;
	case SPR_SPRG1:
		setSPRG(hCPU, 1, newValue);
		break;
	case SPR_SPRG2:
		setSPRG(hCPU, 2, newValue);
		break;
	case SPR_SPRG3:
		setSPRG(hCPU, 3, newValue);
		break;
	case SPR_SCR:
		setSCR(hCPU, newValue);
		break;
	case SPR_CAR:
		setCAR(hCPU, newValue);
		break;
	case SPR_BCR:
		setBCR(hCPU, newValue);
		break;
	case SPR_DMAU:
		setDMAU(hCPU, newValue);
		break;
	case SPR_DMAL:
		setDMAL(hCPU, newValue);
		break;
	case SPR_DBATU_0:
		setDBATU(hCPU, 0, newValue);
		break;
	case SPR_DBATU_1:
		setDBATU(hCPU, 1, newValue);
		break;
	case SPR_DBATU_2:
		setDBATU(hCPU, 2, newValue);
		break;
	case SPR_DBATU_3:
		setDBATU(hCPU, 3, newValue);
		break;
	case SPR_DBATU_4:
		setDBATU(hCPU, 4, newValue);
		break;
	case SPR_DBATU_5:
		setDBATU(hCPU, 5, newValue);
		break;
	case SPR_DBATU_6:
		setDBATU(hCPU, 6, newValue);
		break;
	case SPR_DBATU_7:
		setDBATU(hCPU, 7, newValue);
		break;
	case SPR_DBATL_0:
		setDBATL(hCPU, 0, newValue);
		break;
	case SPR_DBATL_1:
		setDBATL(hCPU, 1, newValue);
		break;
	case SPR_DBATL_2:
		setDBATL(hCPU, 2, newValue);
		break;
	case SPR_DBATL_3:
		setDBATL(hCPU, 3, newValue);
		break;
	case SPR_DBATL_4:
		setDBATL(hCPU, 4, newValue);
		break;
	case SPR_DBATL_5:
		setDBATL(hCPU, 5, newValue);
		break;
	case SPR_DBATL_6:
		setDBATL(hCPU, 6, newValue);
		break;
	case SPR_DBATL_7:
		setDBATL(hCPU, 7, newValue);
		break;
	case SPR_IBATU_0:
		setIBATU(hCPU, 0, newValue);
		break;
	case SPR_IBATU_1:
		setIBATU(hCPU, 1, newValue);
		break;
	case SPR_IBATU_2:
		setIBATU(hCPU, 2, newValue);
		break;
	case SPR_IBATU_3:
		setIBATU(hCPU, 3, newValue);
		break;
	case SPR_IBATU_4:
		setIBATU(hCPU, 4, newValue);
		break;
	case SPR_IBATU_5:
		setIBATU(hCPU, 5, newValue);
		break;
	case SPR_IBATU_6:
		setIBATU(hCPU, 6, newValue);
		break;
	case SPR_IBATU_7:
		setIBATU(hCPU, 7, newValue);
		break;
	case SPR_IBATL_0:
		setIBATL(hCPU, 0, newValue);
		break;
	case SPR_IBATL_1:
		setIBATL(hCPU, 1, newValue);
		break;
	case SPR_IBATL_2:
		setIBATL(hCPU, 2, newValue);
		break;
	case SPR_IBATL_3:
		setIBATL(hCPU, 3, newValue);
		break;
	case SPR_IBATL_4:
		setIBATL(hCPU, 4, newValue);
		break;
	case SPR_IBATL_5:
		setIBATL(hCPU, 5, newValue);
		break;
	case SPR_IBATL_6:
		setIBATL(hCPU, 6, newValue);
		break;
	case SPR_IBATL_7:
		setIBATL(hCPU, 7, newValue);
		break;
	case SPR_SDR1:
		setSDR1(hCPU, newValue);
		break;
	case 0x3B8: // mmcr0
		debug_printf("Write performance monitor SPR mmcr0 0x%08x", newValue);
		break;
	case 0x3B9: // PMC1
		debug_printf("Write performance monitor SPR PMC1 0x%08x", newValue);
		break;
	case 0x3BA: // PMC2
		debug_printf("Write performance monitor SPR PMC2 0x%08x", newValue);
		break;
	case 0x3BC: // mmcr1
		debug_printf("Write performance monitor SPR mmcr1 0x%08x", newValue);
		break;
	case 0x3BD: // PMC3
		debug_printf("Write performance monitor SPR PMC3 0x%08x", newValue);
		break;
	case 0x3BE: // PMC4
		debug_printf("Write performance monitor SPR PMC4 0x%08x", newValue);
		break;
	default:
		debug_printf("[C%d] Set unhandled SPR 0x%x to %08x (supervisor mode)\n", hCPU->spr.UPIR, spr, newValue);
#ifdef CEMU_DEBUG_ASSERT
		assert_dbg();
#endif
		break;
	}
}

static void PPCSpr_set(PPCInterpreter_t* hCPU, uint32 spr, uint32 newValue)
{
	if constexpr(ppcItpCtrl::allowSupervisorMode)
	{
		// todo - check if in supervisor mode or user mode
		PPCSprSupervisor_set(hCPU, spr, newValue);
		return;
	}

	switch (spr)
	{
	case SPR_LR:
		hCPU->spr.LR = newValue;
		break;
	case SPR_CTR:
		hCPU->spr.CTR = newValue;
		break;
	case SPR_XER:
		PPCInterpreter_setXER(hCPU, newValue);
		break;
	case SPR_UGQR0:
	case SPR_UGQR1:
	case SPR_UGQR2:
	case SPR_UGQR3:
	case SPR_UGQR4:
	case SPR_UGQR5:
	case SPR_UGQR6:
	case SPR_UGQR7:
		hCPU->spr.UGQR[spr - SPR_UGQR0] = newValue;
		break;
	default:
		debug_printf("[C%d] Set unhandled SPR %d to %08x\n", hCPU->spr.UPIR, spr, newValue);
#ifdef CEMU_DEBUG_ASSERT
		assert_dbg();
#endif
		break;
	}
}

static uint32 PPCSprSupervisor_get(PPCInterpreter_t* hCPU, uint32 spr)
{
	uint32 v = 0;
	switch (spr)
	{
	case SPR_LR:
		v = hCPU->spr.LR;
		break;
	case SPR_CTR:
		v = hCPU->spr.CTR;
		break;
	case SPR_XER:
		v = PPCInterpreter_getXER(hCPU);
		break;
	case SPR_UPIR:
		v = hCPU->spr.UPIR;
		break;
	case SPR_UGQR0:
	case SPR_UGQR1:
	case SPR_UGQR2:
	case SPR_UGQR3:
	case SPR_UGQR4:
	case SPR_UGQR5:
	case SPR_UGQR6:
	case SPR_UGQR7:
		v = hCPU->spr.UGQR[spr - SPR_UGQR0];
		break;
	// above are registers accessible in user mode
	case SPR_PVR:
		v = getPVR(hCPU);
		break;
	case SPR_HID0:
		v = getHID0(hCPU);
		break;
	case SPR_HID1:
		v = getHID1(hCPU);
		break;
	case SPR_HID2:
		v = getHID2(hCPU);
		break;
	case SPR_HID4:
		v = getHID4(hCPU);
		break;
	case SPR_HID5:
		v = getHID5(hCPU);
		break;
	case SPR_SCR:
		v = getSCR(hCPU);
		break;
	case SPR_CAR:
		v = getCAR(hCPU);
		break;
	case SPR_BCR:
		v = getBCR(hCPU);
		break;
	case SPR_DAR:
		v = getDAR(hCPU);
		break;
	case SPR_DSISR:
		v = getDSISR(hCPU);
		break;
	case SPR_L2CR:
		v = getL2CR(hCPU);
		break;
	case SPR_FPECR:
		v = getFPECR(hCPU);
		break;
	case SPR_SPRG0:
		v = getSPRG(hCPU, 0);
		break;
	case SPR_SPRG1:
		v = getSPRG(hCPU, 1);
		break;
	case SPR_SPRG2:
		v = getSPRG(hCPU, 2);
		break;
	case SPR_SPRG3:
		v = getSPRG(hCPU, 3);
		break;
	case SPR_DBATU_0:
		v = getDBATU(hCPU, 0);
		break;
	case SPR_DBATU_1:
		v = getDBATU(hCPU, 1);
		break;
	case SPR_DBATU_2:
		v = getDBATU(hCPU, 2);
		break;
	case SPR_DBATU_3:
		v = getDBATU(hCPU, 3);
		break;
	case SPR_DBATU_4:
		v = getDBATU(hCPU, 4);
		break;
	case SPR_DBATU_5:
		v = getDBATU(hCPU, 5);
		break;
	case SPR_DBATU_6:
		v = getDBATU(hCPU, 6);
		break;
	case SPR_DBATU_7:
		v = getDBATU(hCPU, 7);
		break;
	case SPR_DBATL_0:
		v = getDBATL(hCPU, 0);
		break;
	case SPR_DBATL_1:
		v = getDBATL(hCPU, 1);
		break;
	case SPR_DBATL_2:
		v = getDBATL(hCPU, 2);
		break;
	case SPR_DBATL_3:
		v = getDBATL(hCPU, 3);
		break;
	case SPR_DBATL_4:
		v = getDBATL(hCPU, 4);
		break;
	case SPR_DBATL_5:
		v = getDBATL(hCPU, 5);
		break;
	case SPR_DBATL_6:
		v = getDBATL(hCPU, 6);
		break;
	case SPR_DBATL_7:
		v = getDBATL(hCPU, 7);
		break;
	case SPR_IBATU_0:
		v = getIBATU(hCPU, 0);
		break;
	case SPR_IBATU_1:
		v = getIBATU(hCPU, 1);
		break;
	case SPR_IBATU_2:
		v = getIBATU(hCPU, 2);
		break;
	case SPR_IBATU_3:
		v = getIBATU(hCPU, 3);
		break;
	case SPR_IBATU_4:
		v = getIBATU(hCPU, 4);
		break;
	case SPR_IBATU_5:
		v = getIBATU(hCPU, 5);
		break;
	case SPR_IBATU_6:
		v = getIBATU(hCPU, 6);
		break;
	case SPR_IBATU_7:
		v = getIBATU(hCPU, 7);
		break;
	case SPR_IBATL_0:
		v = getIBATL(hCPU, 0);
		break;
	case SPR_IBATL_1:
		v = getIBATL(hCPU, 1);
		break;
	case SPR_IBATL_2:
		v = getIBATL(hCPU, 2);
		break;
	case SPR_IBATL_3:
		v = getIBATL(hCPU, 3);
		break;
	case SPR_IBATL_4:
		v = getIBATL(hCPU, 4);
		break;
	case SPR_IBATL_5:
		v = getIBATL(hCPU, 5);
		break;
	case SPR_IBATL_6:
		v = getIBATL(hCPU, 6);
		break;
	case SPR_IBATL_7:
		v = getIBATL(hCPU, 7);
		break;
	default:
		debug_printf("[C%d] Get unhandled SPR %d\n", hCPU->spr.UPIR, spr);
#ifdef CEMU_DEBUG_ASSERT
		assert_dbg();
#endif
		break;
	}
	return v;
}

static uint32 PPCSpr_get(PPCInterpreter_t* hCPU, uint32 spr)
{
	if constexpr(ppcItpCtrl::allowSupervisorMode)
	{
		// todo - check if in supervisor mode or user mode
		return PPCSprSupervisor_get(hCPU, spr);
	}

	uint32 v = 0;
	switch (spr)
	{
	case SPR_LR:
		v = hCPU->spr.LR;
		break;
	case SPR_CTR:
		v = hCPU->spr.CTR;
		break;
	case SPR_XER:
		v = PPCInterpreter_getXER(hCPU);
		break;
	case SPR_DEC:
		// special handling for DEC register
	{
		assert_dbg();
		uint64 passedCycled = PPCInterpreter_getMainCoreCycleCounter() - ppcMainThreadDECCycleStart;
		if (passedCycled >= (uint64)ppcMainThreadDECCycleValue)
			v = 0;
		else
			v = (uint32)(ppcMainThreadDECCycleValue - passedCycled);
	}
	break;
	case SPR_UPIR:
		v = hCPU->spr.UPIR;
		break;
	case SPR_PVR:
		assert_dbg();
		//v = hCPU->sprNew.PVR;
		break;
	case SPR_UGQR0:
	case SPR_UGQR1:
	case SPR_UGQR2:
	case SPR_UGQR3:
	case SPR_UGQR4:
	case SPR_UGQR5:
	case SPR_UGQR6:
	case SPR_UGQR7:
		v = hCPU->spr.UGQR[spr - SPR_UGQR0];
		break;
	default:
		debug_printf("[C%d] Get unhandled SPR %d\n", hCPU->spr.UPIR, spr);
#ifdef CEMU_DEBUG_ASSERT
		assert_dbg();
#endif
		break;
	}



	//if( spr == SPR_LR || spr == SPR_PVR || spr == SPR_UPIR || spr == SPR_SCR || (spr >= SPR_UGQR0 && spr <= SPR_UGQR7) )
	//{
	//	// readable registers
	//	v = hCPU->spr[spr];
	//}
	//else if( spr == SPR_DEC )
	//{
	//	// special handling for DEC register
	//	uint64 passedCycled = PPCInterpreter_getMainCoreCycleCounter() - ppcMainThreadDECCycleStart;
	//	if( passedCycled >= (uint64)ppcMainThreadDECCycleValue )
	//		v = 0;
	//	else
	//		v = ppcMainThreadDECCycleValue - passedCycled;
	//}
	//else if( spr == SPR_XER )
	//{
	//	v = PPCInterpreter_getXER(hCPU);
	//}
	//else
	//{
	//	debug_printf("[C%d] Get unhandled SPR %d value: %08x\n", hCPU->spr[SPR_UPIR], spr, hCPU->spr[spr]);
	//	v = hCPU->spr[spr];
	//}
	return v;
}