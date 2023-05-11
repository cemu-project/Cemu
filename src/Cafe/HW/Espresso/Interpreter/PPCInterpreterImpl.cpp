#include "PPCInterpreterInternal.h"
#include "PPCInterpreterHelper.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"
#include "Cafe/HW/Espresso/Debugger/GDBStub.h"

class PPCItpCafeOSUsermode
{
public:
	static const bool allowSupervisorMode = false;
	static const bool allowDSI = false;

	inline static uint32 memory_readCodeU32(PPCInterpreter_t* hCPU, uint32 address)
	{
		return _swapEndianU32(*(uint32*)(memory_base + address));
	}

	inline static void ppcMem_writeDataDouble(PPCInterpreter_t* hCPU, uint32 address, double vf)
	{
		uint64 v = *(uint64*)&vf;
		uint32 v1 = v & 0xFFFFFFFF;
		uint32 v2 = v >> 32;
		uint8* ptr = memory_getPointerFromVirtualOffset(address);
		*(uint32*)(ptr + 4) = CPU_swapEndianU32(v1);
		*(uint32*)(ptr + 0) = CPU_swapEndianU32(v2);
	}

	inline static void ppcMem_writeDataU64(PPCInterpreter_t* hCPU, uint32 address, uint64 v)
	{
		*(uint64*)(memory_getPointerFromVirtualOffset(address)) = CPU_swapEndianU64(v);
	}

	inline static void ppcMem_writeDataU32(PPCInterpreter_t* hCPU, uint32 address, uint32 v)
	{
		*(uint32*)(memory_getPointerFromVirtualOffset(address)) = CPU_swapEndianU32(v);
	}

	inline static void ppcMem_writeDataU16(PPCInterpreter_t* hCPU, uint32 address, uint16 v)
	{
		*(uint16*)(memory_getPointerFromVirtualOffset(address)) = CPU_swapEndianU16(v);
	}

	inline static void ppcMem_writeDataU8(PPCInterpreter_t* hCPU, uint32 address, uint8 v)
	{
		*(uint8*)(memory_getPointerFromVirtualOffset(address)) = v;
	}
	
	inline static double ppcMem_readDataDouble(PPCInterpreter_t* hCPU, uint32 address)
	{
		uint32 v[2];
		v[1] = *(uint32*)(memory_getPointerFromVirtualOffset(address));
		v[0] = *(uint32*)(memory_getPointerFromVirtualOffset(address) + 4);
		v[0] = CPU_swapEndianU32(v[0]);
		v[1] = CPU_swapEndianU32(v[1]);
		return *(double*)v;
	}

	inline static float ppcMem_readDataFloat(PPCInterpreter_t* hCPU, uint32 address)
	{
		uint32 v = *(uint32*)(memory_getPointerFromVirtualOffset(address));
		v = CPU_swapEndianU32(v);
		return *(float*)&v;
	}

	inline static uint64 ppcMem_readDataU64(PPCInterpreter_t* hCPU, uint32 address)
	{
		uint64 v = *(uint64*)(memory_getPointerFromVirtualOffset(address));
		return CPU_swapEndianU64(v);
	}

	inline static uint32 ppcMem_readDataU32(PPCInterpreter_t* hCPU, uint32 address)
	{
		uint32 v = *(uint32*)(memory_getPointerFromVirtualOffset(address));
		return CPU_swapEndianU32(v);
	}

	inline static uint16 ppcMem_readDataU16(PPCInterpreter_t* hCPU, uint32 address)
	{
		uint16 v = *(uint16*)(memory_getPointerFromVirtualOffset(address));
		return CPU_swapEndianU16(v);
	}

	inline static uint8 ppcMem_readDataU8(PPCInterpreter_t* hCPU, uint32 address)
	{
		return *(uint8*)(memory_getPointerFromVirtualOffset(address));
	}

	inline static uint64 ppcMem_readDataFloatEx(PPCInterpreter_t* hCPU, uint32 addr)
	{
		return ConvertToDoubleNoFTZ(_swapEndianU32(*(uint32*)(memory_base + addr)));
	}

	inline static void ppcMem_writeDataFloatEx(PPCInterpreter_t* hCPU, uint32 addr, uint64 value)
	{
		*(uint32*)(memory_base + addr) = _swapEndianU32(ConvertToSingleNoFTZ(value));
	}

	inline static uint64 getTB(PPCInterpreter_t* hCPU)
	{
		return PPCInterpreter_getMainCoreCycleCounter();
	}
};

uint32 debug_lastTranslatedHit;

void generateDSIException(PPCInterpreter_t* hCPU, uint32 dataAddress)
{
	// todo - check if we are already inside an interrupt handler (in which case the DSI exception is queued and not executed immediately?)

	// set flag to cancel current instruction
	hCPU->memoryException = true;

	hCPU->sprExtended.srr0 = hCPU->instructionPointer;
	hCPU->sprExtended.srr1 = hCPU->sprExtended.msr & 0x87C0FFFF;

	hCPU->sprExtended.dar = dataAddress;

	hCPU->sprExtended.msr &= ~0x04EF36;

	hCPU->instructionPointer = 0xFFF00300;


	uint32 dsisr = 0;
	dsisr |= (1<<(31-1)); // set if no TLB/BAT match found

	hCPU->sprExtended.dsisr = dsisr;

}

class PPCItpSupervisorWithMMU
{
public:
	static const bool allowSupervisorMode = true;
	static const bool allowDSI = true;

	inline static uint32 ppcMem_translateVirtualDataToPhysicalAddr(PPCInterpreter_t* hCPU, uint32 vAddr)
	{
		// check if address translation is disabled for data accesses
		if (GET_MSR_BIT(MSR_DR) == 0)
		{
			return vAddr;
		}

#ifdef CEMU_DEBUG_ASSERT
		if (hCPU->memoryException)
			assert_dbg(); // should not be set anymore
#endif

		// how to determine if BAT is valid:
		// BAT_entry_valid = (Vs & ~MSR[PR]) | (Vp & MSR[PR]) (The entry has separate enable flags for usermode and supervisor mode)
		for (sint32 i = 0; i < 8; i++)
		{
			// upper
			uint32 batU = hCPU->sprExtended.dbatU[i];
			uint32 BEPI = ((batU >> 17) & 0x7FFF) << 17;
			uint32 Vp = (batU >> 0) & 1;
			uint32 Vs = (batU >> 1) & 1;
			uint32 BL = (((batU >> 2) & 0x7FF) ^ 0x7FF) << 17;
			BL |= 0xF0000000;
			if (Vs == 0)
				continue; // todo - check if in supervisor/usermode
			// lower
			uint32 batL = hCPU->sprExtended.dbatL[i];
			uint32 PP = (batL >> 0) & 3;
			uint32 WIMG = (batL >> 3) & 0xF;
			uint32 BRPN = ((batL >> 17) & 0x7FFF) << 17;

			// check for match
			if ((vAddr&BL) == BEPI)
			{
				// match
				vAddr = (vAddr&~BL) | (BRPN&BL);
				debug_lastTranslatedHit = vAddr;
				return vAddr;
			}
		}

		// no match
		debug_lastTranslatedHit = 0xFFFFFFFF;

		// find segment
		uint32 segmentIndex = (vAddr>>28);
		//uint32 pageIndex = (vAddr >> 12) & 0xFFFF; // for 4KB pages
		// uint32 byteOffset = vAddr & 0xFFF; // for 4KB pages
		uint32 pageIndex = (vAddr >> 17) & 0x7FF; // for 128KB pages 
		uint32 byteOffset = vAddr & 0x1FFFF;
		uint32 srValue = hCPU->sprExtended.sr[segmentIndex];
		
		uint8 sr_ks = (srValue >> 30) & 1; // supervisor
		uint8 sr_kp = (srValue >> 29) & 1; // user mode
		uint8 sr_n = (srValue >> 28) & 1; // no-execute
		uint32 sr_vsid = (srValue & 0xFFFFFF);
		//uint32 vpn = pageIndex | (sr_vsid << 16); // 40bit virtual page number


		// look up in page table
		//uint32 lookupHash = (sr_vsid ^ pageIndex) & 0x7FFFF; // not correct for 4KB pages? sr_vsid must be shifted?
		//uint32 lookupHash = (sr_vsid ^ pageIndex) & 0x7FFFF;
		//uint32 lookupHash = ((sr_vsid>>8) ^ pageIndex) & 0x7FFFF;
		uint32 lookupHash = ((sr_vsid >> 0) ^ pageIndex) & 0x7FFFF;

		//lookupHash ^= 0x7FFFF;

		uint32 pageTableAddr = hCPU->sprExtended.sdr1&0xFFFF0000;
		uint32 pageTableMask = hCPU->sprExtended.sdr1&0x1FF;

		for (uint32 ch = 0; ch < 2; ch++)
		{
			uint32 ptegSelectLow = (lookupHash & 0x3FF);
			uint32 maskOR = (lookupHash >> 10) & pageTableMask;

			uint32* pteg = (uint32*)(memory_base + (pageTableAddr | (maskOR << 16)) + ptegSelectLow * 64);
			for (sint32 t = 0; t < 8; t++)
			{
				uint32 w0 = _swapEndianU32(pteg[0]);
				uint32 w1 = _swapEndianU32(pteg[1]);
				pteg += 2;
				if ((w0 & 0x80000000) == 0)
					continue; // entry not valid

				uint32 abPageIndex = (w0 >> 0) & 0x3F;
				uint8 h = (w0 >> 6) & 1;
				uint32 ptegVSID = (w0 >> 7) & 0xFFFFFF;

				if (abPageIndex == (pageIndex >> 5) && ptegVSID == sr_vsid && h == ch)
				{
					if (ch == 1)
						assert_dbg();
					// match
					uint32 ptegPhysicalPage = (w1 >> 12) & 0xFFFFF;
					// replace page (128KB)
					vAddr = (vAddr & ~0xFFFE0000) | (ptegPhysicalPage << 12);
					return vAddr;

				}
			}
			// calculate hash 2
			lookupHash = ~lookupHash;
		}

		cemuLog_logDebug(LogType::Force, "DSI exception at 0x{:08x} DataAddress {:08x}", hCPU->instructionPointer, vAddr);

		generateDSIException(hCPU, vAddr);

		// todo: Check hash func 1
		// todo: Check protection bits
		// todo: Check supervisor/usermode bits


		// also use this function in all the mem stuff below

		// note: bat has higher priority than TLB

		// since iterating the bats and page table is too slow, we need to pre-process the data somehow.

		return vAddr;
	}

	inline static uint32 ppcMem_translateVirtualCodeToPhysicalAddr(PPCInterpreter_t* hCPU, uint32 vAddr)
	{
		// check if address translation is disabled for instruction accesses
		if (GET_MSR_BIT(MSR_IR) == 0)
		{
			return vAddr;
		}

		// how to determine if BAT is valid:
		// BAT_entry_valid = (Vs & ~MSR[PR]) | (Vp & MSR[PR]) (The entry has separate enable flags for usermode and supervisor mode)
		for (sint32 i = 0; i < 8; i++)
		{
			// upper
			uint32 batU = hCPU->sprExtended.ibatU[i];
			uint32 BEPI = ((batU >> 17) & 0x7FFF) << 17;
			uint32 Vp = (batU >> 0) & 1;
			uint32 Vs = (batU >> 1) & 1;
			uint32 BL = (((batU >> 2) & 0x7FF) ^ 0x7FF) << 17;
			BL |= 0xF0000000;
			if (Vs == 0)
				continue; // todo - check if in supervisor/usermode
			// lower
			uint32 batL = hCPU->sprExtended.ibatL[i];
			uint32 PP = (batL >> 0) & 3;
			uint32 WIMG = (batL >> 3) & 0xF;
			uint32 BRPN = ((batL >> 17) & 0x7FFF) << 17;

			// check for match
			if ((vAddr&BL) == BEPI)
			{
				// match
				vAddr = (vAddr&~BL) | (BRPN&BL);
				debug_lastTranslatedHit = vAddr;
				return vAddr;
			}
		}
		assert_dbg();

		// no match
		// todo - throw exception if translation is enabled?
		return vAddr;
	}

	static uint32 memory_readCodeU32(PPCInterpreter_t* hCPU, uint32 address)
	{
		return _swapEndianU32(*(uint32*)(memory_base + ppcMem_translateVirtualCodeToPhysicalAddr(hCPU, address)));
	}

	inline static uint8* ppcMem_getDataPtr(PPCInterpreter_t* hCPU, uint32 vAddr)
	{
		return memory_base + ppcMem_translateVirtualDataToPhysicalAddr(hCPU, vAddr);
	}

	inline static void ppcMem_writeDataDouble(PPCInterpreter_t* hCPU, uint32 address, double vf)
	{
		uint64 v = *(uint64*)&vf;
		uint32 v1 = v & 0xFFFFFFFF;
		uint32 v2 = v >> 32;
		uint8* ptr = ppcMem_getDataPtr(hCPU, address);
		*(uint32*)(ptr + 4) = CPU_swapEndianU32(v1);
		*(uint32*)(ptr + 0) = CPU_swapEndianU32(v2);
	}

	inline static void ppcMem_writeDataU64(PPCInterpreter_t* hCPU, uint32 address, uint64 v)
	{
		*(uint64*)(ppcMem_getDataPtr(hCPU, address)) = CPU_swapEndianU64(v);
	}

	inline static void ppcMem_writeDataU32(PPCInterpreter_t* hCPU, uint32 address, uint32 v)
	{
		uint32 pAddr = ppcMem_translateVirtualDataToPhysicalAddr(hCPU, address); 
		if (hCPU->memoryException)
			return;

		if (pAddr >= 0x0c000000 && pAddr < 0x0d100000)
		{
			cemu_assert_unimplemented();
			return;
		}
		*(uint32*)(memory_base + pAddr) = CPU_swapEndianU32(v);
	}

	inline static void ppcMem_writeDataU16(PPCInterpreter_t* hCPU, uint32 address, uint16 v)
	{
		*(uint16*)(ppcMem_getDataPtr(hCPU, address)) = CPU_swapEndianU16(v);
	}

	inline static void ppcMem_writeDataU8(PPCInterpreter_t* hCPU, uint32 address, uint8 v)
	{
		*(uint8*)(ppcMem_getDataPtr(hCPU, address)) = v;
	}

	inline static double ppcMem_readDataDouble(PPCInterpreter_t* hCPU, uint32 address)
	{
		uint32 v[2];
		v[1] = *(uint32*)(ppcMem_getDataPtr(hCPU, address));
		v[0] = *(uint32*)(ppcMem_getDataPtr(hCPU, address) + 4);
		v[0] = CPU_swapEndianU32(v[0]);
		v[1] = CPU_swapEndianU32(v[1]);
		return *(double*)v;
	}

	inline static float ppcMem_readDataFloat(PPCInterpreter_t* hCPU, uint32 address)
	{
		uint32 v = *(uint32*)(ppcMem_getDataPtr(hCPU, address));
		v = CPU_swapEndianU32(v);
		return *(float*)&v;
	}

	inline static uint64 ppcMem_readDataU64(PPCInterpreter_t* hCPU, uint32 address)
	{
		uint64 v = *(uint64*)(ppcMem_getDataPtr(hCPU, address));
		return CPU_swapEndianU64(v);
	}

	inline static uint32 ppcMem_readDataU32(PPCInterpreter_t* hCPU, uint32 address)
	{
		uint32 pAddr = ppcMem_translateVirtualDataToPhysicalAddr(hCPU, address);
		if (hCPU->memoryException)
			return 0;
		if (pAddr >= 0x01FFF000 && pAddr < 0x02000000)
		{
			debug_printf("Access u32 boot param block 0x%08x IP %08x LR %08x\n", pAddr, hCPU->instructionPointer, hCPU->spr.LR);
			cemuLog_logDebug(LogType::Force, "Access u32 boot param block 0x{:08x} (org {:08x}) IP {:08x}", pAddr, address, hCPU->instructionPointer);
		}
		if (pAddr >= 0xFFEB73B0 && pAddr < (0xFFEB73B0+0x40C))
		{
			debug_printf("Access cached u32 boot param block 0x%08x IP %08x LR %08x\n", pAddr, hCPU->instructionPointer, hCPU->spr.LR);
			cemuLog_logDebug(LogType::Force, "Access cached u32 boot param block 0x{:08x} (org {:08x}) IP {:08x}", pAddr, address, hCPU->instructionPointer);
		}

		if (pAddr >= 0x0c000000 && pAddr < 0x0d100000)
		{
			cemu_assert_unimplemented();
			return 0;
		}
		uint32 v = *(uint32*)(memory_base + pAddr);
		return CPU_swapEndianU32(v);
	}

	inline static uint16 ppcMem_readDataU16(PPCInterpreter_t* hCPU, uint32 address)
	{
		uint16 v = *(uint16*)(ppcMem_getDataPtr(hCPU, address));
		return CPU_swapEndianU16(v);
	}

	inline static uint8 ppcMem_readDataU8(PPCInterpreter_t* hCPU, uint32 address)
	{
		uint32 pAddr = ppcMem_translateVirtualDataToPhysicalAddr(hCPU, address);
		if (pAddr >= 0x0c000000 && pAddr < 0x0d100000)
		{
			cemu_assert_unimplemented();
			return 0;
		}
		return *(uint8*)(memory_base + pAddr);
	}

	inline static uint64 ppcMem_readDataFloatEx(PPCInterpreter_t* hCPU, uint32 addr)
	{
		return ConvertToDoubleNoFTZ(_swapEndianU32(*(uint32*)(memory_base + addr)));
	}

	inline static void ppcMem_writeDataFloatEx(PPCInterpreter_t* hCPU, uint32 addr, uint64 value)
	{
		*(uint32*)(memory_base + addr) = _swapEndianU32(ConvertToSingleNoFTZ(value));
	}

	inline static uint64 getTB(PPCInterpreter_t* hCPU)
	{
		return hCPU->global->tb / 20ULL;
	}
};

uint32 testIP[100];
uint32 testIPC = 0;

template <typename ppcItpCtrl>
class PPCInterpreterContainer
{
public:
#include "PPCInterpreterSPR.hpp"
#include "PPCInterpreterOPC.hpp"
#include "PPCInterpreterLoadStore.hpp"
#include "PPCInterpreterALU.hpp"

	static void executeInstruction(PPCInterpreter_t* hCPU)
	{
		if constexpr(ppcItpCtrl::allowSupervisorMode)
		{
			hCPU->global->tb++;
		}

#ifdef __DEBUG_OUTPUT_INSTRUCTION
		debug_printf("%08x: ", hCPU->instructionPointer);
#endif

		uint32 opcode = ppcItpCtrl::memory_readCodeU32(hCPU, hCPU->instructionPointer);

		switch ((opcode >> 26))
		{
		case 0:
			debug_printf("ZERO[NOP] | 0x%08X\n", (unsigned int)hCPU->instructionPointer);
	#ifdef CEMU_DEBUG_ASSERT		
			assert_dbg();
			while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
	#endif
			hCPU->instructionPointer += 4;
			break;
		case 1: // virtual HLE
			PPCInterpreter_virtualHLE(hCPU, opcode);
			break;
		case 4:
			switch (PPC_getBits(opcode, 30, 5))
			{
			case 0: // subcategory compare
				switch (PPC_getBits(opcode, 25, 5))
				{
				case 0: // Sonic All Stars Racing
					PPCInterpreter_PS_CMPU0(hCPU, opcode);
					break;
				case 1:
					PPCInterpreter_PS_CMPO0(hCPU, opcode);
					break;
				case 2: // Assassin's Creed 3, Sonic All Stars Racing
					PPCInterpreter_PS_CMPU1(hCPU, opcode);
					break;
				default:
					debug_printf("Unknown execute %04X as [4->0] at %08X\n", PPC_getBits(opcode, 25, 5), hCPU->instructionPointer);
					cemu_assert_unimplemented();
					break;
				}
				break;
			case 6:
				PPCInterpreter_PSQ_LX(hCPU, opcode);
				break;
			case 7:
				PPCInterpreter_PSQ_STX(hCPU, opcode);
				break;
			case 8:
				switch (PPC_getBits(opcode, 25, 5))
				{
				case 1:
					PPCInterpreter_PS_NEG(hCPU, opcode);
					break;
				case 2:
					PPCInterpreter_PS_MR(hCPU, opcode);
					break;
				case 4:
					PPCInterpreter_PS_NABS(hCPU, opcode);
					break;
				case 8:
					PPCInterpreter_PS_ABS(hCPU, opcode);
					break;
				default:
					debug_printf("Unknown execute %04X as [4->8] at %08X\n", PPC_getBits(opcode, 25, 5), hCPU->instructionPointer);
					cemu_assert_unimplemented();
					break;
				}
				break;
			case 10:
				PPCInterpreter_PS_SUM0(hCPU, opcode);
				break;
			case 11:
				PPCInterpreter_PS_SUM1(hCPU, opcode);
				break;
			case 12:
				PPCInterpreter_PS_MULS0(hCPU, opcode);
				break;
			case 13:
				PPCInterpreter_PS_MULS1(hCPU, opcode);
				break;
			case 14:
				PPCInterpreter_PS_MADDS0(hCPU, opcode);
				break;
			case 15:
				PPCInterpreter_PS_MADDS1(hCPU, opcode);
				break;
			case 16: // sub category - merge
				switch (PPC_getBits(opcode, 25, 5))
				{
				case 16:
					PPCInterpreter_PS_MERGE00(hCPU, opcode);
					break;
				case 17:
					PPCInterpreter_PS_MERGE01(hCPU, opcode);
					break;
				case 18:
					PPCInterpreter_PS_MERGE10(hCPU, opcode);
					break;
				case 19:
					PPCInterpreter_PS_MERGE11(hCPU, opcode);
					break;
				default:
					debug_printf("Unknown execute %04X as [4->16] at %08X\n", PPC_getBits(opcode, 25, 5), hCPU->instructionPointer);
					debugBreakpoint();
					break;
				}
				break;
			case 18:
				PPCInterpreter_PS_DIV(hCPU, opcode);
				break;
			case 20:
				PPCInterpreter_PS_SUB(hCPU, opcode);
				break;
			case 21:
				PPCInterpreter_PS_ADD(hCPU, opcode);
				break;
			case 22:
				PPCInterpreter_DCBZL(hCPU, opcode);
				break;
			case 23:
				PPCInterpreter_PS_SEL(hCPU, opcode);
				break;
			case 24:
				PPCInterpreter_PS_RES(hCPU, opcode);
				break;
			case 25:
				PPCInterpreter_PS_MUL(hCPU, opcode);
				break;
			case 26: // sub category with only one entry - RSQRTE
				PPCInterpreter_PS_RSQRTE(hCPU, opcode);
				break;
			case 28:
				PPCInterpreter_PS_MSUB(hCPU, opcode);
				break;
			case 29:
				PPCInterpreter_PS_MADD(hCPU, opcode);
				break;
			case 30:
				PPCInterpreter_PS_NMSUB(hCPU, opcode);
				break;
			case 31:
				PPCInterpreter_PS_NMADD(hCPU, opcode);
				break;
			default:
				debug_printf("Unknown execute %04X as [4] at %08X\n", PPC_getBits(opcode, 30, 5), hCPU->instructionPointer);
				cemu_assert_unimplemented();
				break;
			}
			break;
		case 7:
			PPCInterpreter_MULLI(hCPU, opcode);
			break;
		case 8:
			PPCInterpreter_SUBFIC(hCPU, opcode);
			break;
		case 10:
			PPCInterpreter_CMPLI(hCPU, opcode);
			break;
		case 11:
			PPCInterpreter_CMPI(hCPU, opcode);
			break;
		case 12:
			PPCInterpreter_ADDIC(hCPU, opcode);
			break;
		case 13:
			PPCInterpreter_ADDIC_(hCPU, opcode);
			break;
		case 14:
			PPCInterpreter_ADDI(hCPU, opcode);
			break;
		case 15:
			PPCInterpreter_ADDIS(hCPU, opcode);
			break;
		case 16:
			PPCInterpreter_BCX(hCPU, opcode);
			break;
		case 17:
			if (PPC_getBits(opcode, 30, 1) == 1) {
				PPCInterpreter_SC(hCPU, opcode);
			}
			else {
				debug_printf("Unsupported Opcode [0x17 --> 0x0]\n");
				cemu_assert_unimplemented();
			}
			break;
		case 18:
			PPCInterpreter_BX(hCPU, opcode);
			break;
		case 19: // opcode category
			switch (PPC_getBits(opcode, 30, 10))
			{
			case 0:
				PPCInterpreter_MCRF(hCPU, opcode);
				break;
			case 16:
				PPCInterpreter_BCLRX(hCPU, opcode);
				break;
			case 33:
				PPCInterpreter_CRNOR(hCPU, opcode);
				break;
			case 50:
				PPCInterpreter_RFI(hCPU, opcode);
				break;
			case 129:
				PPCInterpreter_CRANDC(hCPU, opcode);
				break;
			case 150:
				PPCInterpreter_ISYNC(hCPU, opcode);
				break;
			case 193:
				PPCInterpreter_CRXOR(hCPU, opcode);
				break;
			case 257:
				PPCInterpreter_CRAND(hCPU, opcode);
				break;
			case 289:
				PPCInterpreter_CREQV(hCPU, opcode);
				break;
			case 417:
				PPCInterpreter_CRORC(hCPU, opcode);
				break;
			case 449:
				PPCInterpreter_CROR(hCPU, opcode);
				break;
			case 528:
				PPCInterpreter_BCCTR(hCPU, opcode);
				break;
			default:
				debug_printf("Unknown execute %04X as [19] at %08X\n", PPC_getBits(opcode, 30, 10), hCPU->instructionPointer);
				cemu_assert_unimplemented();
				break;
			}
			break;
		case 20:
			PPCInterpreter_RLWIMI(hCPU, opcode);
			break;
		case 21:
			PPCInterpreter_RLWINM(hCPU, opcode);
			break;
		case 23:
			PPCInterpreter_RLWNM(hCPU, opcode);
			break;
		case 24:
			PPCInterpreter_ORI(hCPU, opcode);
			break;
		case 25:
			PPCInterpreter_ORIS(hCPU, opcode);
			break;
		case 26:
			PPCInterpreter_XORI(hCPU, opcode);
			break;
		case 27:
			PPCInterpreter_XORIS(hCPU, opcode);
			break;
		case 28:
			PPCInterpreter_ANDI_(hCPU, opcode);
			break;
		case 29:
			PPCInterpreter_ANDIS_(hCPU, opcode);
			break;
		case 31: // opcode category
			switch (PPC_getBits(opcode, 30, 10))
			{
			case 0:
				PPCInterpreter_CMP(hCPU, opcode);
				break;
			case 4:
	#ifdef CEMU_DEBUG_ASSERT
				debug_printf("TW instruction executed at %08x\n", hCPU->instructionPointer);
	#endif
				PPCInterpreter_TW(hCPU, opcode);
				break;
			case 8:
				PPCInterpreter_SUBFC(hCPU, opcode);
				break;
			case 10:
				PPCInterpreter_ADDC(hCPU, opcode);
				break;
			case 11:
				PPCInterpreter_MULHWU_(hCPU, opcode);
				break;
			case 19:
				PPCInterpreter_MFCR(hCPU, opcode);
				break;
			case 20:
				PPCInterpreter_LWARX(hCPU, opcode);
				break;
			case 23:
				PPCInterpreter_LWZX(hCPU, opcode);
				break;
			case 24:
				PPCInterpreter_SLWX(hCPU, opcode);
				break;
			case 26:
				PPCInterpreter_CNTLZW(hCPU, opcode);
				break;
			case 28:
				PPCInterpreter_ANDX(hCPU, opcode);
				break;
			case 32:
				PPCInterpreter_CMPL(hCPU, opcode);
				break;
			case 40:
				PPCInterpreter_SUBF(hCPU, opcode);
				break;
			case 54:
				PPCInterpreter_DCBST(hCPU, opcode);
				break;
			case 55:
				PPCInterpreter_LWZXU(hCPU, opcode);
				break;
			case 60:
				PPCInterpreter_ANDCX(hCPU, opcode);
				break;
			case 75:
				PPCInterpreter_MULHW_(hCPU, opcode);
				break;
			case 83:
				PPCInterpreter_MFMSR(hCPU, opcode);
				break;
			case 86:
				PPCInterpreter_DCBF(hCPU, opcode);
				break;
			case 87:
				PPCInterpreter_LBZX(hCPU, opcode);
				break;
			case 104:
				PPCInterpreter_NEG(hCPU, opcode);
				break;
			case 119: // Sonic Lost World
				PPCInterpreter_LBZXU(hCPU, opcode);
				break;
			case 124:
				PPCInterpreter_NORX(hCPU, opcode);
				break;
			case 136:
				PPCInterpreter_SUBFE(hCPU, opcode);
				break;
			case 138:
				PPCInterpreter_ADDE(hCPU, opcode);
				break;
			case 144:
				PPCInterpreter_MTCRF(hCPU, opcode);
				break;
			case 146:
				PPCInterpreter_MTMSR(hCPU, opcode);
				break;
			case 150:
				PPCInterpreter_STWCX(hCPU, opcode);
				break;
			case 151:
				PPCInterpreter_STWX(hCPU, opcode);
				break;
			case 183:
				PPCInterpreter_STWUX(hCPU, opcode);
				break;
			case 200:
				PPCInterpreter_SUBFZE(hCPU, opcode);
				break;
			case 202:
				PPCInterpreter_ADDZE(hCPU, opcode);
				break;
			case 210:
				PPCInterpreter_MTSR(hCPU, opcode);
				break;
			case 215:
				PPCInterpreter_STBX(hCPU, opcode);
				break;
			case 232: // Trine 2
				PPCInterpreter_SUBFME(hCPU, opcode);
				break;
			case 234:
				PPCInterpreter_ADDME(hCPU, opcode);
				break;
			case 235:
				PPCInterpreter_MULLW(hCPU, opcode);
				break;
			case 247:
				PPCInterpreter_STBUX(hCPU, opcode);
				break;
			case 266:
				PPCInterpreter_ADD(hCPU, opcode);
				break;
			case 278:
				PPCInterpreter_DCBT(hCPU, opcode);
				break;
			case 279:
				PPCInterpreter_LHZX(hCPU, opcode);
				break;
			case 284:
				PPCInterpreter_EQV(hCPU, opcode);
				break;
			case 306:
				PPCInterpreter_TLBIE(hCPU, opcode);
				break;
			case 311: // Wii U Menu v177 (US)
				PPCInterpreter_LHZUX(hCPU, opcode);
				break;
			case 316:
				PPCInterpreter_XOR(hCPU, opcode);
				break;
			case 339:
				PPCInterpreter_MFSPR(hCPU, opcode);
				break;
			case 343:
				PPCInterpreter_LHAX(hCPU, opcode);
				break;
			case 371:
				PPCInterpreter_MFTB(hCPU, opcode);
				break;
			case 375: // Wii U Menu v177 (US)
				PPCInterpreter_LHAUX(hCPU, opcode);
				break;
			case 407:
				PPCInterpreter_STHX(hCPU, opcode);
				break;
			case 412:
				PPCInterpreter_ORC(hCPU, opcode);
				break;
			case 439:
				PPCInterpreter_STHUX(hCPU, opcode);
				break;
			case 444:
				PPCInterpreter_OR(hCPU, opcode);
				break;
			case 459:
				PPCInterpreter_DIVWU(hCPU, opcode);
				break;
			case 467:
				PPCInterpreter_MTSPR(hCPU, opcode);
				break;
			case 470:
				PPCInterpreter_DCBI(hCPU, opcode);
				break;
			case 476:
				PPCInterpreter_NANDX(hCPU, opcode);
				break;
			case 491:
				PPCInterpreter_DIVW(hCPU, opcode);
				break;
			case 512:
				PPCInterpreter_MCRXR(hCPU, opcode);
				break;
			case 520: // Affordable Space Adventures + other Unity games
				PPCInterpreter_SUBFCO(hCPU, opcode);
				break;
			case 522:
				PPCInterpreter_ADDCO(hCPU, opcode);
				break;
			case 534:
				PPCInterpreter_LWBRX(hCPU, opcode);
				break;
			case 535:
				PPCInterpreter_LFSX(hCPU, opcode);
				break;
			case 536:
				PPCInterpreter_SRWX(hCPU, opcode);
				break;
			case 552:
				PPCInterpreter_SUBFO(hCPU, opcode);
				break;
			case 566:
				PPCInterpreter_TLBSYNC(hCPU, opcode);
				break;
			case 567:
				PPCInterpreter_LFSUX(hCPU, opcode);
				break;
			case 595:
				PPCInterpreter_MFSR(hCPU, opcode);
				break;
			case 597:
				PPCInterpreter_LSWI(hCPU, opcode);
				break;
			case 598:
				PPCInterpreter_SYNC(hCPU, opcode);
				break;
			case 599:
				PPCInterpreter_LFDX(hCPU, opcode);
				break;
			case 616:
				PPCInterpreter_NEGO(hCPU, opcode);
				break;
			case 631:
				PPCInterpreter_LFDUX(hCPU, opcode);
				break;
			case 648: // 136 | OE
				PPCInterpreter_SUBFEO(hCPU, opcode);
				break;
			case 650: // 138 | OE
				PPCInterpreter_ADDEO(hCPU, opcode);
				break;
			case 662:
				PPCInterpreter_STWBRX(hCPU, opcode);
				break;
			case 663:
				PPCInterpreter_STFSX(hCPU, opcode);
				break;
			case 695:
				PPCInterpreter_STFSUX(hCPU, opcode);
				break;
			case 725:
				PPCInterpreter_STSWI(hCPU, opcode);
				break;
			case 727:
				PPCInterpreter_STFDX(hCPU, opcode);
				break;
			case 747:
				PPCInterpreter_MULLWO(hCPU, opcode);
				break;
			case 759:
				PPCInterpreter_STFDUX(hCPU, opcode);
				break;
			case 778:
				PPCInterpreter_ADDO(hCPU, opcode);
				break;
			case 790:
				PPCInterpreter_LHBRX(hCPU, opcode);
				break;
			case 792:
				PPCInterpreter_SRAW(hCPU, opcode);
				break;
			case 824:
				PPCInterpreter_SRAWI(hCPU, opcode);
				break;
			case 854:
				PPCInterpreter_EIEIO(hCPU, opcode);
				break;
			case 918:
				PPCInterpreter_STHBRX(hCPU, opcode);
				break;
			case 922:
				PPCInterpreter_EXTSH(hCPU, opcode);
				break;
			case 954:
				PPCInterpreter_EXTSB(hCPU, opcode);
				break;
			case 971:
				PPCInterpreter_DIVWUO(hCPU, opcode);
				break;
			case 982:
				PPCInterpreter_ICBI(hCPU, opcode);
				break;
			case 983:
				PPCInterpreter_STFIWX(hCPU, opcode);
				break;
			case 1003:
				PPCInterpreter_DIVWO(hCPU, opcode);
				break;
			case 1014:
				PPCInterpreter_DCBZ(hCPU, opcode);
				break;
			default:
				debug_printf("Unknown execute %04X as [31] at %08X\n", PPC_getBits(opcode, 30, 10), hCPU->instructionPointer);
	#ifdef CEMU_DEBUG_ASSERT
				assert_dbg();
	#endif
				hCPU->instructionPointer += 4;
				break;
			}
			break;
		case 32:
			PPCInterpreter_LWZ(hCPU, opcode);
			break;
		case 33:
			PPCInterpreter_LWZU(hCPU, opcode);
			break;
		case 34:
			PPCInterpreter_LBZ(hCPU, opcode);
			break;
		case 35:
			PPCInterpreter_LBZU(hCPU, opcode);
			break;
		case 36:
			PPCInterpreter_STW(hCPU, opcode);
			break;
		case 37:
			PPCInterpreter_STWU(hCPU, opcode);
			break;
		case 38:
			PPCInterpreter_STB(hCPU, opcode);
			break;
		case 39:
			PPCInterpreter_STBU(hCPU, opcode);
			break;
		case 40:
			PPCInterpreter_LHZ(hCPU, opcode);
			break;
		case 41:
			PPCInterpreter_LHZU(hCPU, opcode);
			break;
		case 42:
			PPCInterpreter_LHA(hCPU, opcode);
			break;
		case 43:
			PPCInterpreter_LHAU(hCPU, opcode);
			break;
		case 44:
			PPCInterpreter_STH(hCPU, opcode);
			break;
		case 45:
			PPCInterpreter_STHU(hCPU, opcode);
			break;
		case 46:
			PPCInterpreter_LMW(hCPU, opcode);
			break;
		case 47:
			PPCInterpreter_STMW(hCPU, opcode);
			break;
		case 48:
			PPCInterpreter_LFS(hCPU, opcode);
			break;
		case 49:
			PPCInterpreter_LFSU(hCPU, opcode);
			break;
		case 50:
			PPCInterpreter_LFD(hCPU, opcode);
			break;
		case 51:
			PPCInterpreter_LFDU(hCPU, opcode);
			break;
		case 52:
			PPCInterpreter_STFS(hCPU, opcode);
			break;
		case 53:
			PPCInterpreter_STFSU(hCPU, opcode);
			break;
		case 54:
			PPCInterpreter_STFD(hCPU, opcode);
			break;
		case 55:
			PPCInterpreter_STFDU(hCPU, opcode);
			break;
		case 56:
			PPCInterpreter_PSQ_L(hCPU, opcode);
			break;
		case 57:
			PPCInterpreter_PSQ_LU(hCPU, opcode);
			break;
		case 59: //Opcode category
			switch (PPC_getBits(opcode, 30, 5))
			{
			case 18:
				PPCInterpreter_FDIVS(hCPU, opcode);
				break;
			case 20:
				PPCInterpreter_FSUBS(hCPU, opcode);
				break;
			case 21:
				PPCInterpreter_FADDS(hCPU, opcode);
				break;
			case 24:
				PPCInterpreter_FRES(hCPU, opcode);
				break;
			case 25:
				PPCInterpreter_FMULS(hCPU, opcode);
				break;
			case 28:
				PPCInterpreter_FMSUBS(hCPU, opcode);
				break;
			case 29:
				PPCInterpreter_FMADDS(hCPU, opcode);
				break;
			case 30:
				PPCInterpreter_FNMSUBS(hCPU, opcode);
				break;
			case 31:
				PPCInterpreter_FNMADDS(hCPU, opcode);
				break;
			default:
				debug_printf("Unknown execute %04X as [59] at %08X\n", PPC_getBits(opcode, 30, 10), hCPU->instructionPointer);
				cemu_assert_unimplemented();
				break;
			}
			break;
		case 60:
			PPCInterpreter_PSQ_ST(hCPU, opcode);
			break;
		case 61:
			PPCInterpreter_PSQ_STU(hCPU, opcode);
			break;
		case 63: // opcode category
			switch (PPC_getBits(opcode, 30, 5))
			{
			case 0:
				PPCInterpreter_FCMPU(hCPU, opcode);
				break;
			case 12:
				PPCInterpreter_FRSP(hCPU, opcode);
				break;
			case 15:
				PPCInterpreter_FCTIWZ(hCPU, opcode);
				break;
			case 18:
				PPCInterpreter_FDIV(hCPU, opcode);
				break;
			case 20:
				PPCInterpreter_FSUB(hCPU, opcode);
				break;
			case 21:
				PPCInterpreter_FADD(hCPU, opcode);
				break;
			case 23:
				PPCInterpreter_FSEL(hCPU, opcode);
				break;
			case 25:
				PPCInterpreter_FMUL(hCPU, opcode);
				break;
			case 26:
				PPCInterpreter_FRSQRTE(hCPU, opcode);
				break;
			case 28:
				PPCInterpreter_FMSUB(hCPU, opcode);
				break;
			case 29:
				PPCInterpreter_FMADD(hCPU, opcode);
				break;
			case 30:
				PPCInterpreter_FNMSUB(hCPU, opcode);
				break;
			case 31:
				PPCInterpreter_FNMADD(hCPU, opcode);
				break;
			default:
				switch (PPC_getBits(opcode, 30, 10))
				{
				case 14:
					PPCInterpreter_FCTIW(hCPU, opcode);
					break;
				case 32:
					PPCInterpreter_FCMPO(hCPU, opcode);
					break;
				case 38:
					PPCInterpreter_MTFSB1X(hCPU, opcode);
					break;
				case 40:
					PPCInterpreter_FNEG(hCPU, opcode);
					break;
				case 72:
					PPCInterpreter_FMR(hCPU, opcode);
					break;
				case 136: // Darksiders 2
					PPCInterpreter_FNABS(hCPU, opcode);
					break;
				case 264:
					PPCInterpreter_FABS(hCPU, opcode);
					break;
				case 583:
					PPCInterpreter_MFFS(hCPU, opcode);
					break;
				case 711: // IBM documentation has this wrong as 771?
					PPCInterpreter_MTFSF(hCPU, opcode);
					break;
				default:
					debug_printf("Unknown execute %04X as [63] at %08X\n", PPC_getBits(opcode, 30, 10), hCPU->instructionPointer);
					cemu_assert_unimplemented();
					break;
				}
			}
			break;
		default:
			debug_printf("Unknown execute %04X at %08X\n", PPC_getBits(opcode, 5, 6), (unsigned int)hCPU->instructionPointer);
			cemu_assert_unimplemented();
		}
	}
};

// Slim interpreter, trades some features for extra performance
// Used when emulator runs in CafeOS HLE mode
// Assumes the following:
// - No MMU (linear memory with 1:1 mapping of physical to virtual)
// - No interrupts
// - Always runs in user mode
// - Paired single mode is always enabled
void PPCInterpreterSlim_executeInstruction(PPCInterpreter_t* hCPU)
{
	PPCInterpreterContainer<PPCItpCafeOSUsermode>::executeInstruction(hCPU);
}

// Full interpreter, supports most PowerPC features
// Used when emulator runs in LLE mode
void PPCInterpreterFull_executeInstruction(PPCInterpreter_t* hCPU)
{
	PPCInterpreterContainer<PPCItpSupervisorWithMMU>::executeInstruction(hCPU);
}
