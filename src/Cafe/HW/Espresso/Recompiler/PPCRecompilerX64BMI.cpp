#include "PPCRecompiler.h"
#include "PPCRecompilerX64.h"

void _x64Gen_writeMODRMDeprecated(x64GenContext_t* x64GenContext, sint32 dataRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32);

void x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32)
{
	// MOVBE <dstReg64> (low dword), DWORD [<reg64> + <reg64> + <imm64>]
	if( dstRegister >= 8 && memRegisterA64 >= 8 && memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x47);
	else if( memRegisterA64 >= 8 && memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x43);
	else if( dstRegister >= 8 && memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x42);
	else if( dstRegister >= 8 && memRegisterA64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( dstRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( memRegisterA64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	else if( memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x42);

	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x38);
	x64Gen_writeU8(x64GenContext, 0xF0);
	_x64Gen_writeMODRMDeprecated(x64GenContext, dstRegister, memRegisterA64, memRegisterB64, memImmS32);
}

void x64Gen_movBEZeroExtend_reg64Low16_mem16Reg64PlusReg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32)
{
	// MOVBE <dstReg64> (low word), WORD [<reg64> + <reg64> + <imm64>]
	// note: Unlike the 32bit version this instruction does not set the upper 32bits of the 64bit register to 0
	x64Gen_writeU8(x64GenContext, 0x66); // 16bit prefix
	x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext, dstRegister, memRegisterA64, memRegisterB64, memImmS32);
}

void x64Gen_movBETruncate_mem32Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32, sint32 srcRegister)
{
	// MOVBE DWORD [<reg64> + <reg64> + <imm64>], <srcReg64> (low dword)
	if( srcRegister >= 8 && memRegisterA64 >= 8 && memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x47);
	else if( memRegisterA64 >= 8 && memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x43);
	else if( srcRegister >= 8 && memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x42);
	else if( srcRegister >= 8 && memRegisterA64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( memRegisterA64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	else if( memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x42);

	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0x38);
	x64Gen_writeU8(x64GenContext, 0xF1);
	_x64Gen_writeMODRMDeprecated(x64GenContext, srcRegister, memRegisterA64, memRegisterB64, memImmS32);
}

void x64Gen_shrx_reg64_reg64_reg64(x64GenContext_t* x64GenContext, sint32 registerDst, sint32 registerA, sint32 registerB)
{
	// SHRX reg64, reg64, reg64
	x64Gen_writeU8(x64GenContext, 0xC4);
	x64Gen_writeU8(x64GenContext, 0xE2 - ((registerDst >= 8) ? 0x80 : 0) - ((registerA >= 8) ? 0x20 : 0));
	x64Gen_writeU8(x64GenContext, 0xFB - registerB * 8);
	x64Gen_writeU8(x64GenContext, 0xF7);
	x64Gen_writeU8(x64GenContext, 0xC0 + (registerDst & 7) * 8 + (registerA & 7));
}

void x64Gen_shlx_reg64_reg64_reg64(x64GenContext_t* x64GenContext, sint32 registerDst, sint32 registerA, sint32 registerB)
{
	// SHLX reg64, reg64, reg64
	x64Gen_writeU8(x64GenContext, 0xC4);
	x64Gen_writeU8(x64GenContext, 0xE2 - ((registerDst >= 8) ? 0x80 : 0) - ((registerA >= 8) ? 0x20 : 0));
	x64Gen_writeU8(x64GenContext, 0xF9 - registerB * 8);
	x64Gen_writeU8(x64GenContext, 0xF7);
	x64Gen_writeU8(x64GenContext, 0xC0 + (registerDst & 7) * 8 + (registerA & 7));
}