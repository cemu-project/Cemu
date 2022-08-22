#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "PPCRecompilerX64.h"

void x64Gen_genSSEVEXPrefix2(x64GenContext_t* x64GenContext, sint32 xmmRegister1, sint32 xmmRegister2, bool use64BitMode)
{
	if( xmmRegister1 < 8 && xmmRegister2 < 8 && use64BitMode == false )
		return;
	uint8 v = 0x40;
	if( xmmRegister1 >= 8 )
		v |= 0x01;
	if( xmmRegister2 >= 8 )
		v |= 0x04;
	if( use64BitMode )
		v |= 0x08;
	x64Gen_writeU8(x64GenContext, v);
}

void x64Gen_genSSEVEXPrefix1(x64GenContext_t* x64GenContext, sint32 xmmRegister, bool use64BitMode)
{
	if( xmmRegister < 8 && use64BitMode == false )
		return;
	uint8 v = 0x40;
	if( use64BitMode )
		v |= 0x01;
	if( xmmRegister >= 8 )
		v |= 0x04;
	x64Gen_writeU8(x64GenContext, v);
}

void x64Gen_movaps_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSource)
{
	// SSE
	// copy xmm register
	// MOVAPS <xmm>, <xmm>
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSource, xmmRegisterDest, false); // tested
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x28); // alternative encoding: 0x29, source and destination register are exchanged
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSource&7));
}

void x64Gen_movupd_xmmReg_memReg128(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32)
{
	// SSE2
	// move two doubles from memory into xmm register
	// MOVUPD <xmm>, [<reg>+<imm>]
	if( memRegister == REG_ESP )
	{
		// todo: Short form of instruction if memImmU32 is 0 or in -128 to 127 range
		// 66 0F 10 84 E4 23 01 00 00
		x64Gen_writeU8(x64GenContext, 0x66);
		x64Gen_genSSEVEXPrefix1(x64GenContext, xmmRegister, false);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x10);
		x64Gen_writeU8(x64GenContext, 0x84+(xmmRegister&7)*8);
		x64Gen_writeU8(x64GenContext, 0xE4);
		x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else if( memRegister == REG_NONE )
	{
		assert_dbg();
		//x64Gen_writeU8(x64GenContext, 0x66);
		//x64Gen_writeU8(x64GenContext, 0x0F);
		//x64Gen_writeU8(x64GenContext, 0x10);
		//x64Gen_writeU8(x64GenContext, 0x05+(xmmRegister&7)*8);
		//x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_movupd_memReg128_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32)
{
	// SSE2
	// move two doubles from memory into xmm register
	// MOVUPD [<reg>+<imm>], <xmm>
	if( memRegister == REG_ESP )
	{
		// todo: Short form of instruction if memImmU32 is 0 or in -128 to 127 range
		x64Gen_writeU8(x64GenContext, 0x66);
		x64Gen_genSSEVEXPrefix1(x64GenContext, xmmRegister, false);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x11);
		x64Gen_writeU8(x64GenContext, 0x84+(xmmRegister&7)*8);
		x64Gen_writeU8(x64GenContext, 0xE4);
		x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else if( memRegister == REG_NONE )
	{
		assert_dbg();
		//x64Gen_writeU8(x64GenContext, 0x66);
		//x64Gen_writeU8(x64GenContext, 0x0F);
		//x64Gen_writeU8(x64GenContext, 0x11);
		//x64Gen_writeU8(x64GenContext, 0x05+(xmmRegister&7)*8);
		//x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_movddup_xmmReg_memReg64(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32)
{
	// SSE3
	// move one double from memory into lower and upper half of a xmm register
	if( memRegister == REG_RSP )
	{
		// MOVDDUP <xmm>, [<reg>+<imm>]
		// todo: Short form of instruction if memImmU32 is 0 or in -128 to 127 range
		x64Gen_writeU8(x64GenContext, 0xF2);
		if( xmmRegister >= 8 )
			x64Gen_writeU8(x64GenContext, 0x44);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x12);
		x64Gen_writeU8(x64GenContext, 0x84+(xmmRegister&7)*8);
		x64Gen_writeU8(x64GenContext, 0xE4);
		x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else if( memRegister == REG_R15 )
	{
		// MOVDDUP <xmm>, [<reg>+<imm>]
		// todo: Short form of instruction if memImmU32 is 0 or in -128 to 127 range
		// F2 41 0F 12 87 - 44 33 22 11 
		x64Gen_writeU8(x64GenContext, 0xF2);
		x64Gen_genSSEVEXPrefix1(x64GenContext, xmmRegister, true);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x12);
		x64Gen_writeU8(x64GenContext, 0x87+(xmmRegister&7)*8);
		x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else if( memRegister == REG_NONE )
	{
		// MOVDDUP <xmm>, [<imm>]
		// 36 F2 0F 12 05 - 00 00 00 00
		assert_dbg();
		//x64Gen_writeU8(x64GenContext, 0x36);
		//x64Gen_writeU8(x64GenContext, 0xF2);
		//x64Gen_writeU8(x64GenContext, 0x0F);
		//x64Gen_writeU8(x64GenContext, 0x12);
		//x64Gen_writeU8(x64GenContext, 0x05+(xmmRegister&7)*8);
		//x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_movddup_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE3
	// move low double from xmm register into lower and upper half of a different xmm register
	x64Gen_writeU8(x64GenContext, 0xF2);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x12);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_movhlps_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE1
	// move high double from xmm register into lower and upper half of a different xmm register
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x12);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_movsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// move lower double from xmm register into lower half of a different xmm register, leave other half untouched
	x64Gen_writeU8(x64GenContext, 0xF2);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x10); // alternative encoding: 0x11, src and dest exchanged
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_movsd_memReg64_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32)
{
	// SSE2
	// move lower 64bits (double) of xmm register to memory location
	if( memRegister == REG_NONE )
	{
		// MOVSD [<imm>], <xmm>
		// F2 0F 11 05 - 45 23 01 00
		assert_dbg();
		//x64Gen_writeU8(x64GenContext, 0xF2);
		//x64Gen_genSSEVEXPrefix(x64GenContext, xmmRegister, 0, false);
		//x64Gen_writeU8(x64GenContext, 0x0F);
		//x64Gen_writeU8(x64GenContext, 0x11);
		//x64Gen_writeU8(x64GenContext, 0x05+xmmRegister*8);
		//x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else if( memRegister == REG_RSP )
	{
		// MOVSD [RSP+<imm>], <xmm>
		// F2 0F 11 84 24 - 33 22 11 00
		x64Gen_writeU8(x64GenContext, 0xF2);
		x64Gen_genSSEVEXPrefix2(x64GenContext, 0, xmmRegister, false);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x11);
		x64Gen_writeU8(x64GenContext, 0x84+(xmmRegister&7)*8);
		x64Gen_writeU8(x64GenContext, 0x24);
		x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_movlpd_xmmReg_memReg64(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32)
{
	// SSE3
	// move one double from memory into lower half of a xmm register, leave upper half unchanged(?)
	if( memRegister == REG_NONE )
	{
		// MOVLPD <xmm>, [<imm>]
		//x64Gen_writeU8(x64GenContext, 0x66);
		//x64Gen_writeU8(x64GenContext, 0x0F);
		//x64Gen_writeU8(x64GenContext, 0x12);
		//x64Gen_writeU8(x64GenContext, 0x05+(xmmRegister&7)*8);
		//x64Gen_writeU32(x64GenContext, memImmU32);
		assert_dbg();
	}
	else if( memRegister == REG_RSP )
	{
		// MOVLPD <xmm>, [<reg64>+<imm>]
		// 66 0F 12 84 24 - 33 22 11 00
		x64Gen_writeU8(x64GenContext, 0x66);
		x64Gen_genSSEVEXPrefix2(x64GenContext, 0, xmmRegister, false);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x12);
		x64Gen_writeU8(x64GenContext, 0x84+(xmmRegister&7)*8);
		x64Gen_writeU8(x64GenContext, 0x24);
		x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_unpcklpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x14);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_unpckhpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x15);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc, uint8 imm8)
{
	// SSE2
	// shuffled copy source to destination
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xC6);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
	x64Gen_writeU8(x64GenContext, imm8);
}

void x64Gen_addsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// add bottom double of two xmm registers, leave upper quadword unchanged
	x64Gen_writeU8(x64GenContext, 0xF2);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false); // untested
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x58);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_addpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// add both doubles of two xmm registers
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x58);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_subsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// subtract bottom double of two xmm registers, leave upper quadword unchanged
	x64Gen_writeU8(x64GenContext, 0xF2);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x5C);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_subpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// subtract both doubles of two xmm registers
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false); // untested
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x5C);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_mulsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// multiply bottom double of two xmm registers, leave upper quadword unchanged
	x64Gen_writeU8(x64GenContext, 0xF2);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x59);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_mulpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// multiply both doubles of two xmm registers
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false); // untested
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x59);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_mulpd_xmmReg_memReg128(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32)
{
	// SSE2
	if (memRegister == REG_NONE)
	{
		assert_dbg();
	}
	else if (memRegister == REG_R14)
	{
		x64Gen_writeU8(x64GenContext, 0x66);
		x64Gen_writeU8(x64GenContext, (xmmRegister < 8) ? 0x41 : 0x45);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x59);
		x64Gen_writeU8(x64GenContext, 0x86 + (xmmRegister & 7) * 8);
		x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_divsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// divide bottom double of two xmm registers, leave upper quadword unchanged
	x64Gen_writeU8(x64GenContext, 0xF2);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x5E);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_divpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// divide bottom and top double of two xmm registers
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x5E);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_comisd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// compare bottom doubles
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false); // untested
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x2F);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_comisd_xmmReg_mem64Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 memoryReg, sint32 memImmS32)
{
	// SSE2
	// compare bottom double with double from memory location
	if( memoryReg == REG_R15 )
	{
		x64Gen_writeU8(x64GenContext, 0x66);
		x64Gen_genSSEVEXPrefix1(x64GenContext, xmmRegisterDest, true);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x2F);
		x64Gen_writeU8(x64GenContext, 0x87+(xmmRegisterDest&7)*8);
		x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
	}
	else
		assert_dbg();
}

void x64Gen_ucomisd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// compare bottom doubles
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x2E);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_comiss_xmmReg_mem64Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 memoryReg, sint32 memImmS32)
{
	// SSE2
	// compare bottom float with float from memory location
	if (memoryReg == REG_R15)
	{
		x64Gen_genSSEVEXPrefix1(x64GenContext, xmmRegisterDest, true);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x2F);
		x64Gen_writeU8(x64GenContext, 0x87 + (xmmRegisterDest & 7) * 8);
		x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
	}
	else
		assert_dbg();
}

void x64Gen_orps_xmmReg_mem128Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, uint32 memReg, uint32 memImmS32)
{
	// SSE2
	// and xmm register with 128 bit value from memory
	if( memReg == REG_R15 )
	{
		x64Gen_genSSEVEXPrefix2(x64GenContext, memReg, xmmRegisterDest, false);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x56);
		x64Gen_writeU8(x64GenContext, 0x87+(xmmRegisterDest&7)*8);
		x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
	}
	else
		assert_dbg();
}

void x64Gen_xorps_xmmReg_mem128Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, uint32 memReg, uint32 memImmS32)
{
	// SSE2
	// xor xmm register with 128 bit value from memory
	if( memReg == REG_R15 )
	{
		x64Gen_genSSEVEXPrefix1(x64GenContext, xmmRegisterDest, true); // todo: should be x64Gen_genSSEVEXPrefix2() with memReg?
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x57);
		x64Gen_writeU8(x64GenContext, 0x87+(xmmRegisterDest&7)*8);
		x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
	}
	else
		assert_dbg();
}

void x64Gen_andpd_xmmReg_memReg128(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32)
{
	// SSE2
	if (memRegister == REG_NONE)
	{
		assert_dbg();
	}
	else if (memRegister == REG_R14)
	{
		x64Gen_writeU8(x64GenContext, 0x66);
		x64Gen_writeU8(x64GenContext, (xmmRegister < 8) ? 0x41 : 0x45);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x54);
		x64Gen_writeU8(x64GenContext, 0x86 + (xmmRegister & 7) * 8);
		x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_andps_xmmReg_mem128Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, uint32 memReg, uint32 memImmS32)
{
	// SSE2
	// and xmm register with 128 bit value from memory
	if( memReg == REG_R15 )
	{
		x64Gen_genSSEVEXPrefix1(x64GenContext, xmmRegisterDest, true); // todo: should be x64Gen_genSSEVEXPrefix2() with memReg?
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x54);
		x64Gen_writeU8(x64GenContext, 0x87+(xmmRegisterDest&7)*8);
		x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
	}
	else
		assert_dbg();
}

void x64Gen_andps_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// and xmm register with xmm register
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x54);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_pcmpeqd_xmmReg_mem128Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, uint32 memReg, uint32 memImmS32)
{
	// SSE2
	// doubleword integer compare
	if( memReg == REG_R15 )
	{
		x64Gen_writeU8(x64GenContext, 0x66);
		x64Gen_genSSEVEXPrefix1(x64GenContext, xmmRegisterDest, true);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x76);
		x64Gen_writeU8(x64GenContext, 0x87+(xmmRegisterDest&7)*8);
		x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
	}
	else
		assert_dbg();
}

void x64Gen_cvttpd2dq_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// convert two doubles into two 32-bit integers in bottom part of xmm register, reset upper 64 bits of destination register
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xE6);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_cvttsd2si_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 registerDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// convert double to truncated integer in general purpose register
	x64Gen_writeU8(x64GenContext, 0xF2);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, registerDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x2C);
	x64Gen_writeU8(x64GenContext, 0xC0+(registerDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_cvtsd2ss_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// converts bottom 64bit double to bottom 32bit single
	x64Gen_writeU8(x64GenContext, 0xF2);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x5A);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_cvtpd2ps_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// converts two 64bit doubles to two 32bit singles in bottom half of register
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x5A);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_cvtps2pd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// converts two 32bit singles to two 64bit doubles
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x5A);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_cvtss2sd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// converts bottom 32bit single to bottom 64bit double
	x64Gen_writeU8(x64GenContext, 0xF3);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x5A);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_cvtpi2pd_xmmReg_mem64Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 memReg, sint32 memImmS32)
{
	// SSE2
	// converts two signed 32bit integers to two doubles
	if( memReg == REG_RSP )
	{
		x64Gen_writeU8(x64GenContext, 0x66);
		x64Gen_genSSEVEXPrefix1(x64GenContext, xmmRegisterDest, false);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x2A);
		x64Gen_writeU8(x64GenContext, 0x84+(xmmRegisterDest&7)*8);
		x64Gen_writeU8(x64GenContext, 0x24);
		x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_cvtsd2si_reg64Low_xmmReg(x64GenContext_t* x64GenContext, sint32 registerDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// converts bottom 64bit double to 32bit signed integer in general purpose register, round based on float-point control
	x64Gen_writeU8(x64GenContext, 0xF2);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, registerDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x2D);
	x64Gen_writeU8(x64GenContext, 0xC0+(registerDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_cvttsd2si_reg64Low_xmmReg(x64GenContext_t* x64GenContext, sint32 registerDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// converts bottom 64bit double to 32bit signed integer in general purpose register, always truncate
	x64Gen_writeU8(x64GenContext, 0xF2);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, registerDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x2C);
	x64Gen_writeU8(x64GenContext, 0xC0+(registerDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_sqrtsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// calculates square root of bottom double
	x64Gen_writeU8(x64GenContext, 0xF2);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x51);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_sqrtpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// calculates square root of bottom and top double
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x51);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_rcpss_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// approximates reciprocal of bottom 32bit single
	x64Gen_writeU8(x64GenContext, 0xF3);
	x64Gen_genSSEVEXPrefix2(x64GenContext, xmmRegisterSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x53);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(xmmRegisterSrc&7));
}

void x64Gen_mulss_xmmReg_memReg64(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32)
{
	// SSE2
	if( memRegister == REG_NONE )
	{
		assert_dbg();
	}
	else if( memRegister == 15 )
	{
		x64Gen_writeU8(x64GenContext, 0xF3);
		x64Gen_writeU8(x64GenContext, (xmmRegister<8)?0x41:0x45);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x59);
		x64Gen_writeU8(x64GenContext, 0x87+(xmmRegister&7)*8);
		x64Gen_writeU32(x64GenContext, memImmU32);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_movd_xmmReg_reg64Low32(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 registerSrc)
{
	// SSE2
	// copy low 32bit of general purpose register into xmm register
	// MOVD <xmm>, <reg32>
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, registerSrc, xmmRegisterDest, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x6E); // alternative encoding: 0x29, source and destination register are exchanged
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(registerSrc&7));
}

void x64Gen_movd_reg64Low32_xmmReg(x64GenContext_t* x64GenContext, sint32 registerDest, sint32 xmmRegisterSrc)
{
	// SSE2
	// copy low 32bit of general purpose register into xmm register
	// MOVD <reg32>, <xmm>
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, registerDest, xmmRegisterSrc, false);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x7E); // alternative encoding: 0x29, source and destination register are exchanged
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterSrc&7)*8+(registerDest&7));
}

void x64Gen_movq_xmmReg_reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 registerSrc)
{
	// SSE2
	// copy general purpose register into xmm register
	// MOVD <xmm>, <reg64>
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, registerSrc, xmmRegisterDest, true);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x6E); // alternative encoding: 0x29, source and destination register are exchanged
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterDest&7)*8+(registerSrc&7));
}

void x64Gen_movq_reg64_xmmReg(x64GenContext_t* x64GenContext, sint32 registerDst, sint32 xmmRegisterSrc)
{
	// SSE2
	// copy general purpose register into xmm register
	// MOVD <xmm>, <reg64>
	x64Gen_writeU8(x64GenContext, 0x66);
	x64Gen_genSSEVEXPrefix2(x64GenContext, registerDst, xmmRegisterSrc, true);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x7E);
	x64Gen_writeU8(x64GenContext, 0xC0+(xmmRegisterSrc&7)*8+(registerDst&7));
}