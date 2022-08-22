#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"
#include "PPCRecompilerX64.h"

// x86/x64 extension opcodes that could be useful:
// ANDN
// mulx, rorx, sarx, shlx, shrx
// PDEP, PEXT

void x64Gen_checkBuffer(x64GenContext_t* x64GenContext)
{
	// todo
}

void x64Gen_writeU8(x64GenContext_t* x64GenContext, uint8 v)
{
	if( x64GenContext->codeBufferIndex+1 > x64GenContext->codeBufferSize )
	{
		x64GenContext->codeBufferSize *= 2;
		x64GenContext->codeBuffer = (uint8*)realloc(x64GenContext->codeBuffer, x64GenContext->codeBufferSize);
	}
	*(uint8*)(x64GenContext->codeBuffer+x64GenContext->codeBufferIndex) = v;
	x64GenContext->codeBufferIndex++;
}

void x64Gen_writeU16(x64GenContext_t* x64GenContext, uint32 v)
{
	if( x64GenContext->codeBufferIndex+2 > x64GenContext->codeBufferSize )
	{
		x64GenContext->codeBufferSize *= 2;
		x64GenContext->codeBuffer = (uint8*)realloc(x64GenContext->codeBuffer, x64GenContext->codeBufferSize);
	}
	*(uint16*)(x64GenContext->codeBuffer+x64GenContext->codeBufferIndex) = v;
	x64GenContext->codeBufferIndex += 2;
}

void x64Gen_writeU32(x64GenContext_t* x64GenContext, uint32 v)
{
	if( x64GenContext->codeBufferIndex+4 > x64GenContext->codeBufferSize )
	{
		x64GenContext->codeBufferSize *= 2;
		x64GenContext->codeBuffer = (uint8*)realloc(x64GenContext->codeBuffer, x64GenContext->codeBufferSize);
	}
	*(uint32*)(x64GenContext->codeBuffer+x64GenContext->codeBufferIndex) = v;
	x64GenContext->codeBufferIndex += 4;
}

void x64Gen_writeU64(x64GenContext_t* x64GenContext, uint64 v)
{
	if( x64GenContext->codeBufferIndex+8 > x64GenContext->codeBufferSize )
	{
		x64GenContext->codeBufferSize *= 2;
		x64GenContext->codeBuffer = (uint8*)realloc(x64GenContext->codeBuffer, x64GenContext->codeBufferSize);
	}
	*(uint64*)(x64GenContext->codeBuffer+x64GenContext->codeBufferIndex) = v;
	x64GenContext->codeBufferIndex += 8;
}

#include "x64Emit.hpp"

void _x64Gen_writeMODRMDeprecated(x64GenContext_t* x64GenContext, sint32 dataRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32)
{
	bool forceUseOffset = false;
	if ((memRegisterA64 & 7) == 5)
	{
		// RBP and R13 have no memImmS32 == 0 encoding, therefore we need to use a 1 byte offset with the value 0
		forceUseOffset = true;
	}

	if (memRegisterB64 == REG_NONE)
	{
		// memRegisterA64 + memImmS32
		uint8 modRM = (dataRegister & 7) * 8 + (memRegisterA64 & 7);
		if (forceUseOffset && memImmS32 == 0)
		{
			// 1 byte offset
			modRM |= (1 << 6);
		}
		if (memImmS32 == 0)
		{
			// no offset
			modRM |= (0 << 6);
		}
		else if (memImmS32 >= -128 && memImmS32 <= 127)
		{
			// 1 byte offset
			modRM |= (1 << 6);
		}
		else
		{
			// 4 byte offset
			modRM |= (2 << 6);
		}
		x64Gen_writeU8(x64GenContext, modRM);
		// SIB byte
		if ((memRegisterA64 & 7) == 4) // RSP and R12
		{
			x64Gen_writeU8(x64GenContext, 0x24);
		}
		// offset
		if (((modRM >> 6) & 3) == 0)
			; // no offset
		else if (((modRM >> 6) & 3) == 1)
			x64Gen_writeU8(x64GenContext, (uint8)memImmS32);
		else if (((modRM >> 6) & 3) == 2)
			x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
		else
			assert_dbg();
		return;
	}
	// note: Swapping mem register A and mem register B does not work because the instruction prefix defines the register group which might not match (e.g. regA in r0-r8 range and regB in RAX-RDI range)
	if( (memRegisterA64&7) == 4 )
	{
		assert_dbg();
		//sint32 temp = memRegisterA64;
		//memRegisterA64 = memRegisterB64;
		//memRegisterB64 = temp;
	}
	//if( (memRegisterA64&7) == 5 )
	//{
	//	sint32 temp = memRegisterA64;
	//	memRegisterA64 = memRegisterB64;
	//	memRegisterB64 = temp;
	//}
	if( (memRegisterA64&7) == 4 )
		assert_dbg();
	uint8 modRM = (0x04<<0)+((dataRegister&7)<<3);
	if( forceUseOffset && memImmS32 == 0 )
	{
		// 1 byte offset
		modRM |= (1<<6);
	}
	if( memImmS32 == 0 )
	{
		// no offset
		modRM |= (0<<6);
	}
	else if( memImmS32 >= -128 && memImmS32 <= 127 )
	{
		// 1 byte offset
		modRM |= (1<<6);
	}
	else
	{
		// 4 byte offset
		modRM |= (2<<6);
	}
	x64Gen_writeU8(x64GenContext, modRM);
	// sib byte
	x64Gen_writeU8(x64GenContext, 0x00+(memRegisterA64&7)+(memRegisterB64&7)*8);
	// offset
	if( ((modRM>>6)&3) == 0 )
		; // no offset
	else if( ((modRM>>6)&3) == 1 )
		x64Gen_writeU8(x64GenContext, (uint8)memImmS32);
	else if( ((modRM>>6)&3) == 2 )
		x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
	else
		assert_dbg();
}

void x64Emit_mov_reg32_mem32(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memOffset)
{
	x64Gen_writeMODRM_dyn<x64_opc_1byte<0x8B>>(x64GenContext, x64MODRM_opr_reg64(destReg), x64MODRM_opr_memReg64(memBaseReg64, memOffset));
}

void x64Emit_mov_mem32_reg32(x64GenContext_t* x64GenContext, sint32 memBaseReg64, sint32 memOffset, sint32 srcReg)
{
	x64Gen_writeMODRM_dyn<x64_opc_1byte_rev<0x89>>(x64GenContext, x64MODRM_opr_memReg64(memBaseReg64, memOffset), x64MODRM_opr_reg64(srcReg));
}

void x64Emit_mov_mem64_reg64(x64GenContext_t* x64GenContext, sint32 memBaseReg64, sint32 memOffset, sint32 srcReg)
{
	x64Gen_writeMODRM_dyn<x64_opc_1byte_rev<0x89, true>>(x64GenContext, x64MODRM_opr_memReg64(memBaseReg64, memOffset), x64MODRM_opr_reg64(srcReg));
}

void x64Emit_mov_reg64_mem64(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memOffset)
{
	x64Gen_writeMODRM_dyn<x64_opc_1byte<0x8B, true>>(x64GenContext, x64MODRM_opr_reg64(destReg), x64MODRM_opr_memReg64(memBaseReg64, memOffset));
}

void x64Emit_mov_reg64_mem32(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memOffset)
{
	x64Gen_writeMODRM_dyn<x64_opc_1byte<0x8B>>(x64GenContext, x64MODRM_opr_reg64(destReg), x64MODRM_opr_memReg64(memBaseReg64, memOffset));
}

void x64Emit_mov_mem32_reg64(x64GenContext_t* x64GenContext, sint32 memBaseReg64, sint32 memOffset, sint32 srcReg)
{
	x64Gen_writeMODRM_dyn<x64_opc_1byte_rev<0x89>>(x64GenContext, x64MODRM_opr_memReg64(memBaseReg64, memOffset), x64MODRM_opr_reg64(srcReg));
}

void x64Emit_mov_reg64_mem64(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memIndexReg64, sint32 memOffset)
{
	x64Gen_writeMODRM_dyn<x64_opc_1byte<0x8B, true>>(x64GenContext, x64MODRM_opr_reg64(destReg), x64MODRM_opr_memRegPlusReg(memBaseReg64, memIndexReg64, memOffset));
}

void x64Emit_mov_reg32_mem32(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memIndexReg64, sint32 memOffset)
{
	x64Gen_writeMODRM_dyn<x64_opc_1byte<0x8B>>(x64GenContext, x64MODRM_opr_reg64(destReg), x64MODRM_opr_memRegPlusReg(memBaseReg64, memIndexReg64, memOffset));
}

void x64Emit_mov_reg64b_mem8(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memIndexReg64, sint32 memOffset)
{
	x64Gen_writeMODRM_dyn<x64_opc_1byte<0x8A>>(x64GenContext, x64MODRM_opr_reg64(destReg), x64MODRM_opr_memRegPlusReg(memBaseReg64, memIndexReg64, memOffset));
}

void x64Emit_movZX_reg32_mem8(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memIndexReg64, sint32 memOffset)
{
	x64Gen_writeMODRM_dyn<x64_opc_2byte<0x0F,0xB6>>(x64GenContext, x64MODRM_opr_reg64(destReg), x64MODRM_opr_memRegPlusReg(memBaseReg64, memIndexReg64, memOffset));
}

void x64Emit_movZX_reg64_mem8(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memOffset)
{
	x64Gen_writeMODRM_dyn<x64_opc_2byte<0x0F, 0xB6>>(x64GenContext, x64MODRM_opr_reg64(destReg), x64MODRM_opr_memReg64(memBaseReg64, memOffset));
}

void x64Gen_movSignExtend_reg64Low32_mem8Reg64PlusReg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32)
{
	// MOVSX <dstReg64> (low dword), BYTE [<reg64> + <reg64> + <imm64>]
	if (dstRegister >= 8 && memRegisterA64 >= 8 && memRegisterB64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x47);
	else if (memRegisterA64 >= 8 && memRegisterB64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x43);
	else if (dstRegister >= 8 && memRegisterB64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x42);
	else if (dstRegister >= 8 && memRegisterA64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x45);
	else if (dstRegister >= 8)
		x64Gen_writeU8(x64GenContext, 0x44);
	else if (memRegisterA64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x41);
	else if (memRegisterB64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x42);
	else if (dstRegister >= 4)
		x64Gen_writeU8(x64GenContext, 0x40);

	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xBE);
	_x64Gen_writeMODRMDeprecated(x64GenContext, dstRegister, memRegisterA64, memRegisterB64, memImmS32);
}

void x64Gen_mov_mem64Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 srcRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32)
{
	// MOV QWORD [<reg64> + <reg64> + <imm64>], <dstReg64>
	if( srcRegister >= 8 && memRegisterA64 >= 8 && memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x47|8);
	else if( memRegisterA64 >= 8 && memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x43|8);
	else if( srcRegister >= 8 && memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x42|8);
	else if( srcRegister >= 8 && memRegisterA64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45|8);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44|8);
	else if( memRegisterA64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41|8);
	else if( memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x42|8);
	else
		x64Gen_writeU8(x64GenContext, 0x48);
	x64Gen_writeU8(x64GenContext, 0x89);
	_x64Gen_writeMODRMDeprecated(x64GenContext, srcRegister, memRegisterA64, memRegisterB64, memImmS32);
}

void x64Gen_movZeroExtend_reg64Low16_mem16Reg64PlusReg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32)
{
	// MOV <dstReg64> (low word), WORD [<reg64> + <reg64> + <imm64>]
	x64Gen_writeU8(x64GenContext, 0x66); // 16bit prefix
	x64Emit_mov_reg32_mem32(x64GenContext, dstRegister, memRegisterA64, memRegisterB64, memImmS32);
}

void x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32, sint32 srcRegister)
{
	// MOV DWORD [<reg64> + <reg64> + <imm64>], <srcReg64> (low dword)
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

	x64Gen_writeU8(x64GenContext, 0x89);
	_x64Gen_writeMODRMDeprecated(x64GenContext, srcRegister, memRegisterA64, memRegisterB64, memImmS32);
}

void x64Gen_movTruncate_mem16Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32, sint32 srcRegister)
{
	// MOV WORD [<reg64> + <reg64> + <imm64>], <srcReg64> (low dword)
	x64Gen_writeU8(x64GenContext, 0x66); // 16bit prefix	
	x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext, memRegisterA64, memRegisterB64, memImmS32, srcRegister);
}

void x64Gen_movTruncate_mem8Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32, sint32 srcRegister)
{
	// MOV BYTE [<reg64> + <reg64> + <imm64>], <srcReg64> (low byte)

	// when no REX byte is present: Source register can range from AL to BH
	// when a REX byte is present: Source register can range from AL to DIL or R8B to R15B
	// todo: We don't need the REX byte when when the source register is AL,BL,CL or DL and neither memRegister A or B are within r8 - r15

	uint8 rexByte = 0x40;
	if( srcRegister >= 8 )
		rexByte |= (1<<2);
	if( memRegisterA64 >= 8 )
		rexByte |= (1<<0);
	if( memRegisterB64 >= 8 )
		rexByte |= (1<<1);
	x64Gen_writeU8(x64GenContext, rexByte);

	x64Gen_writeU8(x64GenContext, 0x88);
	_x64Gen_writeMODRMDeprecated(x64GenContext, srcRegister, memRegisterA64, memRegisterB64, memImmS32);
}

void x64Gen_mov_mem32Reg64_imm32(x64GenContext_t* x64GenContext, sint32 memRegister, uint32 memImmU32, uint32 dataImmU32)
{
	// MOV DWORD [<memReg>+<memImmU32>], dataImmU32
	if( (memRegister&7) == 4 )
	{
		if( memRegister >= 8 )
			x64Gen_writeU8(x64GenContext, 0x41);
		sint32 memImmS32 = (sint32)memImmU32;
		if( memImmS32 >= -128 && memImmS32 <= 127 )
		{
			x64Gen_writeU8(x64GenContext, 0xC7);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint8)memImmU32);
		}
		else
		{
			x64Gen_writeU8(x64GenContext, 0xC7);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, memImmU32);
		}
		x64Gen_writeU32(x64GenContext, dataImmU32);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_mov_mem64Reg64_imm32(x64GenContext_t* x64GenContext, sint32 memRegister, uint32 memImmU32, uint32 dataImmU32)
{
	// MOV QWORD [<memReg>+<memImmU32>], dataImmU32
	if( memRegister == REG_R14 )
	{
		sint32 memImmS32 = (sint32)memImmU32;
		if( memImmS32 == 0 )
		{
			x64Gen_writeU8(x64GenContext, 0x49);
			x64Gen_writeU8(x64GenContext, 0xC7);
			x64Gen_writeU8(x64GenContext, 0x06);
			x64Gen_writeU32(x64GenContext, dataImmU32);
		}
		else if( memImmS32 >= -128 && memImmS32 <= 127 )
		{
			x64Gen_writeU8(x64GenContext, 0x49);
			x64Gen_writeU8(x64GenContext, 0xC7);
			x64Gen_writeU8(x64GenContext, 0x46);
			x64Gen_writeU8(x64GenContext, (uint8)memImmS32);
			x64Gen_writeU32(x64GenContext, dataImmU32);
		}
		else
		{
			assert_dbg();
		}
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_mov_mem8Reg64_imm8(x64GenContext_t* x64GenContext, sint32 memRegister, uint32 memImmU32, uint8 dataImmU8)
{
	// MOV BYTE [<memReg64>+<memImmU32>], dataImmU8
	if( memRegister == REG_RSP )
	{
		sint32 memImmS32 = (sint32)memImmU32;
		if( memImmS32 >= -128 && memImmS32 <= 127 )
		{
			x64Gen_writeU8(x64GenContext, 0xC6);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint8)memImmU32);
		}
		else
		{
			x64Gen_writeU8(x64GenContext, 0xC6);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, memImmU32);
		}
		x64Gen_writeU8(x64GenContext, dataImmU8);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_mov_reg64_imm64(x64GenContext_t* x64GenContext, sint32 destRegister, uint64 immU64)
{
	// MOV <destReg64>, <imm64>
	x64Gen_writeU8(x64GenContext, 0x48+(destRegister/8));
	x64Gen_writeU8(x64GenContext, 0xB8+(destRegister%8));
	x64Gen_writeU64(x64GenContext, immU64);
}

void x64Gen_mov_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 destRegister, uint64 immU32)
{
	// todo: Emit shorter opcode if immU32 is 0 or falls in sint8 range?
	// MOV <destReg64>, <imm64>
	if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0xB8+(destRegister&7));
	x64Gen_writeU32(x64GenContext, (uint32)immU32);
}

void x64Gen_mov_reg64_reg64(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// MOV <destReg64>, <srcReg64>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4D);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x49);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4C);
	else
		x64Gen_writeU8(x64GenContext, 0x48);
	x64Gen_writeU8(x64GenContext, 0x89);
	x64Gen_writeU8(x64GenContext, 0xC0+(destRegister&7)+(srcRegister&7)*8);
}

void x64Gen_xchg_reg64_reg64(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// XCHG <destReg64>, <srcReg64>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4D);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x49);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4C);
	else
		x64Gen_writeU8(x64GenContext, 0x48);
	x64Gen_writeU8(x64GenContext, 0x87);
	x64Gen_writeU8(x64GenContext, 0xC0+(destRegister&7)+(srcRegister&7)*8);
}

void x64Gen_mov_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// MOV <destReg64_low32>, <srcReg64_low32>
	if (destRegister >= 8 && srcRegister >= 8)
		x64Gen_writeU8(x64GenContext, 0x45);
	else if (destRegister >= 8)
		x64Gen_writeU8(x64GenContext, 0x41);
	else if (srcRegister >= 8)
		x64Gen_writeU8(x64GenContext, 0x44);
	x64Gen_writeU8(x64GenContext, 0x89);
	x64Gen_writeU8(x64GenContext, 0xC0 + (destRegister & 7) + (srcRegister & 7) * 8);
}

void x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, uint32 conditionType, sint32 destRegister, sint32 srcRegister)
{
	// cMOVcc <destReg64_low32>, <srcReg64_low32>
	if (destRegister >= 8 && srcRegister >= 8)
		x64Gen_writeU8(x64GenContext, 0x45);
	else if (srcRegister >= 8)
		x64Gen_writeU8(x64GenContext, 0x41);
	else if (destRegister >= 8)
		x64Gen_writeU8(x64GenContext, 0x44);
	x64Gen_writeU8(x64GenContext, 0x0F);
	if (conditionType == X86_CONDITION_CARRY || conditionType == X86_CONDITION_UNSIGNED_BELOW)
		x64Gen_writeU8(x64GenContext, 0x42);
	else if (conditionType == X86_CONDITION_NOT_CARRY || conditionType == X86_CONDITION_UNSIGNED_ABOVE_EQUAL)
		x64Gen_writeU8(x64GenContext, 0x43);
	else if (conditionType == X86_CONDITION_EQUAL)
		x64Gen_writeU8(x64GenContext, 0x44);
	else if (conditionType == X86_CONDITION_NOT_EQUAL)
		x64Gen_writeU8(x64GenContext, 0x45);
	else if (conditionType == X86_CONDITION_UNSIGNED_BELOW_EQUAL)
		x64Gen_writeU8(x64GenContext, 0x46);
	else if (conditionType == X86_CONDITION_UNSIGNED_ABOVE)
		x64Gen_writeU8(x64GenContext, 0x47);
	else if (conditionType == X86_CONDITION_SIGN)
		x64Gen_writeU8(x64GenContext, 0x48);
	else if (conditionType == X86_CONDITION_NOT_SIGN)
		x64Gen_writeU8(x64GenContext, 0x49);
	else if (conditionType == X86_CONDITION_PARITY)
		x64Gen_writeU8(x64GenContext, 0x4A);
	else if (conditionType == X86_CONDITION_SIGNED_LESS)
		x64Gen_writeU8(x64GenContext, 0x4C);
	else if (conditionType == X86_CONDITION_SIGNED_GREATER_EQUAL)
		x64Gen_writeU8(x64GenContext, 0x4D);
	else if (conditionType == X86_CONDITION_SIGNED_LESS_EQUAL)
		x64Gen_writeU8(x64GenContext, 0x4E);
	else if (conditionType == X86_CONDITION_SIGNED_GREATER)
		x64Gen_writeU8(x64GenContext, 0x4F);
	else
	{
		assert_dbg();
	}
	x64Gen_writeU8(x64GenContext, 0xC0 + (destRegister & 7) * 8 + (srcRegister & 7));
}

void x64Gen_movSignExtend_reg64Low32_reg64Low16(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// MOVSX <destReg64_lowDWORD>, <srcReg64_lowWORD>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4D);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4C);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xBF);
	x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7)+(destRegister&7)*8);
}

void x64Gen_movZeroExtend_reg64Low32_reg64Low16(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// MOVZX <destReg64_lowDWORD>, <srcReg64_lowWORD>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4D);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4C);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xB7);
	x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7)+(destRegister&7)*8);
}

void x64Gen_movSignExtend_reg64Low32_reg64Low8(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// MOVSX <destReg64_lowDWORD>, <srcReg64_lowBYTE>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4D);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4C);
	else if( srcRegister >= 4 )
		x64Gen_writeU8(x64GenContext, 0x40);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xBE);
	x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7)+(destRegister&7)*8);
}

void x64Gen_movZeroExtend_reg64Low32_reg64Low8(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// MOVZX <destReg64_lowDWORD>, <srcReg64_lowBYTE>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4D);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x4C);
	else if( srcRegister >= 4 )
		x64Gen_writeU8(x64GenContext, 0x40);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xB6);
	x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7)+(destRegister&7)*8);
}

void x64Gen_lea_reg64Low32_reg64Low32PlusReg64Low32(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegisterA64, sint32 memRegisterB64)
{
	// MOV <reg32>, DWORD [<reg32> + <reg32>]
	if ((memRegisterA64 & 0x7) == 5)
	{
		// RBP
		// swap mem registers to get the shorter instruction encoding
		sint32 temp = memRegisterA64;
		memRegisterA64 = memRegisterB64;
		memRegisterB64 = temp;
	}
	if ((memRegisterA64 & 0x7) == 4)
	{
		// RSP
		// swap mem registers
		sint32 temp = memRegisterA64;
		memRegisterA64 = memRegisterB64;
		memRegisterB64 = temp;
		if ((memRegisterA64 & 0x7) == 4)
			assert_dbg(); // double RSP not supported
	}

	x64Gen_writeU8(x64GenContext, 0x67);
	if (dstRegister >= 8 && memRegisterA64 >= 8 && memRegisterB64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x47);
	else if (dstRegister >= 8 && memRegisterA64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x45);
	else if (dstRegister >= 8 && memRegisterB64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x46);
	else if (dstRegister >= 8)
		x64Gen_writeU8(x64GenContext, 0x44);
	else if (memRegisterA64 >= 8 && memRegisterB64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x43);
	else if (memRegisterB64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x42);
	else if (memRegisterA64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x41);

	x64Gen_writeU8(x64GenContext, 0x8D);
	_x64Gen_writeMODRMDeprecated(x64GenContext, dstRegister&0x7, memRegisterA64 & 0x7, memRegisterB64 & 0x7, 0);
}

void _x64_op_reg64Low_mem8Reg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegister64, sint32 memImmS32, uint8 opByte)
{
	// OR <dstReg64> (low byte), BYTE [<reg64> + <imm64>]
	if (dstRegister >= 8 && memRegister64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x45);
	if (dstRegister >= 8)
		x64Gen_writeU8(x64GenContext, 0x44);
	if (memRegister64 >= 8)
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, opByte);
	_x64Gen_writeMODRMDeprecated(x64GenContext, dstRegister, memRegister64, REG_NONE, memImmS32);
}

void x64Gen_or_reg64Low8_mem8Reg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegister64, sint32 memImmS32)
{
	_x64_op_reg64Low_mem8Reg64(x64GenContext, dstRegister, memRegister64, memImmS32, 0x0A);
}

void x64Gen_and_reg64Low8_mem8Reg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegister64, sint32 memImmS32)
{
	_x64_op_reg64Low_mem8Reg64(x64GenContext, dstRegister, memRegister64, memImmS32, 0x22);
}

void x64Gen_mov_mem8Reg64_reg64Low8(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegister64, sint32 memImmS32)
{
	_x64_op_reg64Low_mem8Reg64(x64GenContext, dstRegister, memRegister64, memImmS32, 0x88);
}

void x64Gen_lock_cmpxchg_mem32Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32, sint32 srcRegister)
{
	// LOCK CMPXCHG DWORD [<reg64> + <reg64> + <imm64>], <srcReg64> (low dword)
	x64Gen_writeU8(x64GenContext, 0xF0); // LOCK prefix

	if( srcRegister >= 8 || memRegisterA64 >= 8|| memRegisterB64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x40+((srcRegister>=8)?4:0)+((memRegisterA64>=8)?1:0)+((memRegisterB64>=8)?2:0));

	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xB1);

	_x64Gen_writeMODRMDeprecated(x64GenContext, srcRegister, memRegisterA64, memRegisterB64, memImmS32);
}

void x64Gen_lock_cmpxchg_mem32Reg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegister64, sint32 memImmS32, sint32 srcRegister)
{
	// LOCK CMPXCHG DWORD [<reg64> + <imm64>], <srcReg64> (low dword)
	x64Gen_writeU8(x64GenContext, 0xF0); // LOCK prefix

	if( srcRegister >= 8 || memRegister64 >= 8 )
		x64Gen_writeU8(x64GenContext, 0x40+((srcRegister>=8)?4:0)+((memRegister64>=8)?1:0));

	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xB1);

	if( memImmS32 == 0 )
	{
		x64Gen_writeU8(x64GenContext, 0x45+(srcRegister&7)*8);
		x64Gen_writeU8(x64GenContext, 0x00);
	}
	else
		assert_dbg();
}

void x64Gen_add_reg64_reg64(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// ADD <destReg>, <srcReg>
	x64Gen_writeU8(x64GenContext, 0x48+(destRegister/8)+(srcRegister/8)*4);
	x64Gen_writeU8(x64GenContext, 0x01);
	x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7)*8+(destRegister&7));
}

void x64Gen_add_reg64_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32)
{
	sint32 immS32 = (sint32)immU32;
	if (srcRegister >= 8)
		x64Gen_writeU8(x64GenContext, 0x49);
	else
		x64Gen_writeU8(x64GenContext, 0x48);
	if (immS32 >= -128 && immS32 <= 127)
	{
		x64Gen_writeU8(x64GenContext, 0x83);
		x64Gen_writeU8(x64GenContext, 0xC0 + (srcRegister & 7));
		x64Gen_writeU8(x64GenContext, (uint8)immS32);
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0x81);
		x64Gen_writeU8(x64GenContext, 0xC0 + (srcRegister & 7));
		x64Gen_writeU32(x64GenContext, immU32);
	}
}

void x64Gen_add_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// ADD <destReg64_low32>, <srcReg64_low32>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x01);
	x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7)*8+(destRegister&7));
}

void x64Gen_add_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32)
{
	sint32 immS32 = (sint32)immU32;
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS32 >= -128 && immS32 <= 127 )
	{
		x64Gen_writeU8(x64GenContext, 0x83);
		x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS32);
	}
	else
	{
		if( srcRegister == REG_RAX )
		{
			// special EAX short form
			x64Gen_writeU8(x64GenContext, 0x05);
		}
		else
		{
			x64Gen_writeU8(x64GenContext, 0x81);
			x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7));
		}
		x64Gen_writeU32(x64GenContext, immU32);
	}
}

void x64Gen_sub_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// SUB <destReg64_low32>, <srcReg64_low32>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x29);
	x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7)*8+(destRegister&7));
}

void x64Gen_sub_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32)
{
	sint32 immS32 = (sint32)immU32;
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS32 >= -128 && immS32 <= 127 )
	{
		x64Gen_writeU8(x64GenContext, 0x83);
		x64Gen_writeU8(x64GenContext, 0xE8+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS32);
	}
	else
	{
		if( srcRegister == REG_RAX )
		{
			// special EAX short form
			x64Gen_writeU8(x64GenContext, 0x2D);
		}
		else
		{
			x64Gen_writeU8(x64GenContext, 0x81);
			x64Gen_writeU8(x64GenContext, 0xE8+(srcRegister&7));
		}
		x64Gen_writeU32(x64GenContext, immU32);
	}
}

void x64Gen_sub_reg64_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32)
{
	sint32 immS32 = (sint32)immU32;
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x49);
	else
		x64Gen_writeU8(x64GenContext, 0x48);
	if( immS32 >= -128 && immS32 <= 127 )
	{
		x64Gen_writeU8(x64GenContext, 0x83);
		x64Gen_writeU8(x64GenContext, 0xE8+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS32);
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0x81);
		x64Gen_writeU8(x64GenContext, 0xE8+(srcRegister&7));
		x64Gen_writeU32(x64GenContext, immU32);
	}
}

void x64Gen_sub_mem32reg64_imm32(x64GenContext_t* x64GenContext, sint32 memRegister, sint32 memImmS32, uint64 immU32)
{
	// SUB <mem32_memReg64>, <imm32>
	sint32 immS32 = (sint32)immU32;
	if( memRegister == REG_RSP )
	{
		if( memImmS32 >= 128 )
		{
			if( immS32 >= -128 && immS32 <= 127 )
			{
				// 4 byte mem imm + 1 byte imm
				x64Gen_writeU8(x64GenContext, 0x83);
				x64Gen_writeU8(x64GenContext, 0xAC);
				x64Gen_writeU8(x64GenContext, 0x24);
				x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
				x64Gen_writeU8(x64GenContext, (uint8)immU32);
			}
			else
			{
				// 4 byte mem imm + 4 byte imm
				x64Gen_writeU8(x64GenContext, 0x81);
				x64Gen_writeU8(x64GenContext, 0xAC);
				x64Gen_writeU8(x64GenContext, 0x24);
				x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
				x64Gen_writeU32(x64GenContext, (uint32)immU32);
			}
		}
		else
			assert_dbg();
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_sbb_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// SBB <destReg64_low32>, <srcReg64_low32>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x19);
	x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7)*8+(destRegister&7));
}

void x64Gen_adc_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// ADC <destReg64_low32>, <srcReg64_low32>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x11);
	x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7)*8+(destRegister&7));
}

void x64Gen_adc_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32)
{
	sint32 immS32 = (sint32)immU32;
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS32 >= -128 && immS32 <= 127 )
	{
		x64Gen_writeU8(x64GenContext, 0x83);
		x64Gen_writeU8(x64GenContext, 0xD0+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS32);
	}
	else
	{
		if( srcRegister == REG_RAX )
		{
			// special EAX short form
			x64Gen_writeU8(x64GenContext, 0x15);
		}
		else
		{
			x64Gen_writeU8(x64GenContext, 0x81);
			x64Gen_writeU8(x64GenContext, 0xD0+(srcRegister&7));
		}
		x64Gen_writeU32(x64GenContext, immU32);
	}
}

void x64Gen_dec_mem32(x64GenContext_t* x64GenContext, sint32 memoryRegister, uint32 memoryImmU32)
{
	// DEC dword [<reg64>+imm]
	sint32 memoryImmS32 = (sint32)memoryImmU32;
	if (memoryRegister != REG_RSP)
		assert_dbg(); // not supported yet
	if (memoryImmS32 >= -128 && memoryImmS32 <= 127)
	{
		x64Gen_writeU8(x64GenContext, 0xFF);
		x64Gen_writeU8(x64GenContext, 0x4C);
		x64Gen_writeU8(x64GenContext, 0x24);
		x64Gen_writeU8(x64GenContext, (uint8)memoryImmU32);
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0xFF);
		x64Gen_writeU8(x64GenContext, 0x8C);
		x64Gen_writeU8(x64GenContext, 0x24);
		x64Gen_writeU32(x64GenContext, memoryImmU32);
	}
}

void x64Gen_imul_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 operandRegister)
{
	// IMUL <destReg64_low32>, <operandReg64_low32>
	if( destRegister >= 8 && operandRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( operandRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xAF);
	x64Gen_writeU8(x64GenContext, 0xC0+(operandRegister&7)+(destRegister&7)*8);
}

void x64Gen_idiv_reg64Low32(x64GenContext_t* x64GenContext, sint32 operandRegister)
{
	// IDIV <destReg64_low32>
	if( operandRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0xF7);
	x64Gen_writeU8(x64GenContext, 0xF8+(operandRegister&7));
}

void x64Gen_div_reg64Low32(x64GenContext_t* x64GenContext, sint32 operandRegister)
{
	// DIV <destReg64_low32>
	if( operandRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0xF7);
	x64Gen_writeU8(x64GenContext, 0xF0+(operandRegister&7));
}

void x64Gen_imul_reg64Low32(x64GenContext_t* x64GenContext, sint32 operandRegister)
{
	// IMUL <destReg64_low32>
	if( operandRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0xF7);
	x64Gen_writeU8(x64GenContext, 0xE8+(operandRegister&7));
}

void x64Gen_mul_reg64Low32(x64GenContext_t* x64GenContext, sint32 operandRegister)
{
	// MUL <destReg64_low32>
	if( operandRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0xF7);
	x64Gen_writeU8(x64GenContext, 0xE0+(operandRegister&7));
}

void x64Gen_and_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32)
{
	sint32 immS32 = (sint32)immU32;
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS32 >= -128 && immS32 <= 127 )
	{
		x64Gen_writeU8(x64GenContext, 0x83);
		x64Gen_writeU8(x64GenContext, 0xE0+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS32);
	}
	else
	{
		if( srcRegister == REG_RAX )
		{
			// special EAX short form
			x64Gen_writeU8(x64GenContext, 0x25);
		}
		else
		{
			x64Gen_writeU8(x64GenContext, 0x81);
			x64Gen_writeU8(x64GenContext, 0xE0+(srcRegister&7));
		}
		x64Gen_writeU32(x64GenContext, immU32);
	}
}

void x64Gen_and_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// AND <destReg64_lowDWORD>, <srcReg64_lowDWORD>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x21);
	x64Gen_writeU8(x64GenContext, 0xC0+(destRegister&7)+(srcRegister&7)*8);
}

void x64Gen_test_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// TEST <destReg64_lowDWORD>, <srcReg64_lowDWORD>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	x64Gen_writeU8(x64GenContext, 0x85);
	x64Gen_writeU8(x64GenContext, 0xC0+(destRegister&7)*8+(srcRegister&7));
}

void x64Gen_test_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32)
{
	sint32 immS32 = (sint32)immU32;
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( srcRegister == REG_RAX )
	{
		// special EAX short form
		x64Gen_writeU8(x64GenContext, 0xA9);
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0xF7);
		x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7));
	}
	x64Gen_writeU32(x64GenContext, immU32);
}

void x64Gen_cmp_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, sint32 immS32)
{
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS32 >= -128 && immS32 <= 127 )
	{
		// 83 F8 00          CMP EAX,0
		x64Gen_writeU8(x64GenContext, 0x83);
		x64Gen_writeU8(x64GenContext, 0xF8+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS32);
	}
	else
	{
		if( srcRegister == REG_RAX )
		{
			// special RAX short form
			x64Gen_writeU8(x64GenContext, 0x3D);
		}
		else
		{
			x64Gen_writeU8(x64GenContext, 0x81);
			x64Gen_writeU8(x64GenContext, 0xF8+(srcRegister&7));
		}
		x64Gen_writeU32(x64GenContext, (uint32)immS32);
	}
}

void x64Gen_cmp_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// CMP <destReg64_lowDWORD>, <srcReg64_lowDWORD>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x39);
	x64Gen_writeU8(x64GenContext, 0xC0+(destRegister&7)+(srcRegister&7)*8);
}

void x64Gen_cmp_reg64Low32_mem32reg64(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 memRegister, sint32 memImmS32)
{
	// CMP <destReg64_lowDWORD>, DWORD [<memRegister>+<immS32>]
	if( memRegister == REG_RSP )
	{
		if( memImmS32 >= -128 && memImmS32 <= 127 )
			assert_dbg(); // todo -> Shorter instruction form
		if( destRegister >= 8 )
			x64Gen_writeU8(x64GenContext, 0x44);
		x64Gen_writeU8(x64GenContext, 0x3B);
		x64Gen_writeU8(x64GenContext, 0x84+(destRegister&7)*8);
		x64Gen_writeU8(x64GenContext, 0x24);
		x64Gen_writeU32(x64GenContext, (uint32)memImmS32);
	}
	else
	{
		assert_dbg();
	}
}

void x64Gen_or_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32)
{
	sint32 immS32 = (sint32)immU32;
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS32 >= -128 && immS32 <= 127 )
	{
		x64Gen_writeU8(x64GenContext, 0x83);
		x64Gen_writeU8(x64GenContext, 0xC8+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS32);
	}
	else
	{
		if( srcRegister == REG_RAX )
		{
			// special EAX short form
			x64Gen_writeU8(x64GenContext, 0x0D);
		}
		else
		{
			x64Gen_writeU8(x64GenContext, 0x81);
			x64Gen_writeU8(x64GenContext, 0xC8+(srcRegister&7));
		}
		x64Gen_writeU32(x64GenContext, immU32);
	}
}

void x64Gen_or_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// OR <destReg64_lowDWORD>, <srcReg64_lowDWORD>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x09);
	x64Gen_writeU8(x64GenContext, 0xC0+(destRegister&7)+(srcRegister&7)*8);
}

void x64Gen_xor_reg32_reg32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// XOR <destReg>, <srcReg>
	x64Gen_writeU8(x64GenContext, 0x33);
	x64Gen_writeU8(x64GenContext, 0xC0+srcRegister+destRegister*8);
}

void x64Gen_xor_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// XOR <destReg64_lowDWORD>, <srcReg64_lowDWORD>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x31);
	x64Gen_writeU8(x64GenContext, 0xC0+(destRegister&7)+(srcRegister&7)*8);
}

void x64Gen_xor_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32)
{
	sint32 immS32 = (sint32)immU32;
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS32 >= -128 && immS32 <= 127 )
	{
		x64Gen_writeU8(x64GenContext, 0x83);
		x64Gen_writeU8(x64GenContext, 0xF0+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS32);
	}
	else
	{
		if( srcRegister == REG_RAX )
		{
			// special EAX short form
			x64Gen_writeU8(x64GenContext, 0x35);
		}
		else
		{
			x64Gen_writeU8(x64GenContext, 0x81);
			x64Gen_writeU8(x64GenContext, 0xF0+(srcRegister&7));
		}
		x64Gen_writeU32(x64GenContext, immU32);
	}
}

void x64Gen_rol_reg64Low32_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8)
{
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS8 == 1 )
	{
		// short form for 1 bit ROL
		x64Gen_writeU8(x64GenContext, 0xD1);
		x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7));
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0xC1);
		x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS8);
	}
}

void x64Gen_rol_reg64Low32_cl(x64GenContext_t* x64GenContext, sint32 srcRegister)
{
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0xD3);
	x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7));
}

void x64Gen_rol_reg64Low16_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8)
{
	x64Gen_writeU8(x64GenContext, 0x66); // 16bit prefix
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS8 == 1 )
	{
		// short form for 1 bit ROL
		x64Gen_writeU8(x64GenContext, 0xD1);
		x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7));
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0xC1);
		x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS8);
	}
}

void x64Gen_rol_reg64_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8)
{
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x49);
	else
		x64Gen_writeU8(x64GenContext, 0x48);
	if( immS8 == 1 )
	{
		// short form for 1 bit ROL
		x64Gen_writeU8(x64GenContext, 0xD1);
		x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7));
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0xC1);
		x64Gen_writeU8(x64GenContext, 0xC0+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS8);
	}
}

void x64Gen_shl_reg64Low32_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8)
{
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS8 == 1 )
	{
		// short form for 1 bit SHL
		x64Gen_writeU8(x64GenContext, 0xD1);
		x64Gen_writeU8(x64GenContext, 0xF0+(srcRegister&7));
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0xC1);
		x64Gen_writeU8(x64GenContext, 0xF0+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS8);
	}
}

void x64Gen_shr_reg64Low32_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8)
{
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS8 == 1 )
	{
		// short form for 1 bit SHR
		x64Gen_writeU8(x64GenContext, 0xD1);
		x64Gen_writeU8(x64GenContext, 0xE8+(srcRegister&7));
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0xC1);
		x64Gen_writeU8(x64GenContext, 0xE8+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS8);
	}
}

void x64Gen_sar_reg64Low32_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8)
{
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	if( immS8 == 1 )
	{
		// short form for 1 bit ROL
		x64Gen_writeU8(x64GenContext, 0xD1);
		x64Gen_writeU8(x64GenContext, 0xF8+(srcRegister&7));
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0xC1);
		x64Gen_writeU8(x64GenContext, 0xF8+(srcRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immS8);
	}
}

void x64Gen_not_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister)
{
	if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0xF7);
	x64Gen_writeU8(x64GenContext, 0xD0+(destRegister&7));
}

void x64Gen_neg_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister)
{
	if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0xF7);
	x64Gen_writeU8(x64GenContext, 0xD8+(destRegister&7));
}

void x64Gen_cdq(x64GenContext_t* x64GenContext)
{
	x64Gen_writeU8(x64GenContext, 0x99);
}

void x64Gen_bswap_reg64(x64GenContext_t* x64GenContext, sint32 destRegister)
{
	if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41|8);
	else
		x64Gen_writeU8(x64GenContext, 0x40|8);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xC8+(destRegister&7));
}

void x64Gen_bswap_reg64Lower32bit(x64GenContext_t* x64GenContext, sint32 destRegister)
{
	if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xC8+(destRegister&7));
}

void x64Gen_bswap_reg64Lower16bit(x64GenContext_t* x64GenContext, sint32 destRegister)
{
	assert_dbg(); // do not use this instruction, it's result is always undefined. Instead use ROL <reg16>, 8
	//x64Gen_writeU8(x64GenContext, 0x66);
	//if( destRegister >= 8 )
	//	x64Gen_writeU8(x64GenContext, 0x41);
	//x64Gen_writeU8(x64GenContext, 0x0F);
	//x64Gen_writeU8(x64GenContext, 0xC8+(destRegister&7));
}

void x64Gen_lzcnt_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// SSE4
	// LZCNT <destReg>, <srcReg>
	x64Gen_writeU8(x64GenContext, 0xF3);
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xBD);
	x64Gen_writeU8(x64GenContext, 0xC0+(destRegister&7)*8+(srcRegister&7));
}

void x64Gen_bsr_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister)
{
	// BSR <destReg>, <srcReg>
	if( destRegister >= 8 && srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x45);
	else if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x44);
	else if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x0F);
	x64Gen_writeU8(x64GenContext, 0xBD);
	x64Gen_writeU8(x64GenContext, 0xC0+(destRegister&7)*8+(srcRegister&7));
}

void x64Gen_setcc_mem8(x64GenContext_t* x64GenContext, sint32 conditionType, sint32 memoryRegister, uint32 memoryImmU32)
{
	// SETcc [<reg64>+imm]
	sint32 memoryImmS32 = (sint32)memoryImmU32;
	if( memoryRegister != REG_RSP )
		assert_dbg(); // not supported
	if( memoryRegister >= 8 )
		assert_dbg(); // not supported
	if( memoryImmS32 >= -128 && memoryImmS32 <= 127 )
	{
		if( conditionType == X86_CONDITION_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x94);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_NOT_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x95);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_UNSIGNED_ABOVE )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x97);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_UNSIGNED_ABOVE_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x93);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_UNSIGNED_BELOW )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x92);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_UNSIGNED_BELOW_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x96);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_SIGNED_GREATER )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x9F);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_SIGNED_GREATER_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x9D);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_SIGNED_LESS )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x9C);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_SIGNED_LESS_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x9E);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint32)memoryImmU32);
		}		
		else if( conditionType == X86_CONDITION_PARITY )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x9A);
			x64Gen_writeU8(x64GenContext, 0x44);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU8(x64GenContext, (uint32)memoryImmU32);
		}
		else
			assert_dbg();
	}
	else
	{
		if( conditionType == X86_CONDITION_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x94);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_NOT_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x95);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_UNSIGNED_ABOVE )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x97);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_UNSIGNED_ABOVE_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x93);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_UNSIGNED_BELOW || conditionType == X86_CONDITION_CARRY )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x92);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_NOT_CARRY )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x93);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_UNSIGNED_BELOW_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x96);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_SIGNED_GREATER )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x9F);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_SIGNED_GREATER_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x9D);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_SIGNED_LESS )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x9C);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_SIGNED_LESS_EQUAL )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x9E);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_SIGN )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x98);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else if( conditionType == X86_CONDITION_PARITY )
		{
			x64Gen_writeU8(x64GenContext, 0x0F);
			x64Gen_writeU8(x64GenContext, 0x9A);
			x64Gen_writeU8(x64GenContext, 0x84);
			x64Gen_writeU8(x64GenContext, 0x24);
			x64Gen_writeU32(x64GenContext, (uint32)memoryImmU32);
		}
		else
			assert_dbg();
	}
}

void x64Gen_setcc_reg64b(x64GenContext_t* x64GenContext, sint32 conditionType, sint32 dataRegister)
{
	// SETcc <reg64_low8>
	if (conditionType == X86_CONDITION_NOT_EQUAL)
	{
		if (dataRegister >= 8)
			x64Gen_writeU8(x64GenContext, 0x41);
		else if (dataRegister >= 4)
			x64Gen_writeU8(x64GenContext, 0x40);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x95);
		x64Gen_writeU8(x64GenContext, 0xC0 + (dataRegister & 7));
	}
	else if (conditionType == X86_CONDITION_EQUAL)
	{
		if (dataRegister >= 8)
			x64Gen_writeU8(x64GenContext, 0x41);
		else if (dataRegister >= 4)
			x64Gen_writeU8(x64GenContext, 0x40);
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x94);
		x64Gen_writeU8(x64GenContext, 0xC0 + (dataRegister & 7));
	}
	else
		assert_dbg();
}

void x64Gen_bt_mem8(x64GenContext_t* x64GenContext, sint32 memoryRegister, uint32 memoryImmU32, uint8 bitIndex)
{
	// BT [<reg64>+imm], bitIndex	(bit test)
	sint32 memoryImmS32 = (sint32)memoryImmU32;
	if( memoryRegister != REG_RSP )
		assert_dbg(); // not supported yet
	if( memoryImmS32 >= -128 && memoryImmS32 <= 127 )
	{
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0xBA);
		x64Gen_writeU8(x64GenContext, 0x64);
		x64Gen_writeU8(x64GenContext, 0x24);
		x64Gen_writeU8(x64GenContext, (uint8)memoryImmU32);
		x64Gen_writeU8(x64GenContext, bitIndex);
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0xBA);
		x64Gen_writeU8(x64GenContext, 0xA4);
		x64Gen_writeU8(x64GenContext, 0x24);
		x64Gen_writeU32(x64GenContext, memoryImmU32);
		x64Gen_writeU8(x64GenContext, bitIndex);
	}
}

void x64Gen_cmc(x64GenContext_t* x64GenContext)
{
	x64Gen_writeU8(x64GenContext, 0xF5);
}

void x64Gen_jmp_imm32(x64GenContext_t* x64GenContext, uint32 destImm32)
{
	x64Gen_writeU8(x64GenContext, 0xE9);
	x64Gen_writeU32(x64GenContext, destImm32);
}

void x64Gen_jmp_memReg64(x64GenContext_t* x64GenContext, sint32 memRegister, uint32 immU32)
{
	if( memRegister == REG_NONE )
	{
		assert_dbg();
	}
	if( memRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	sint32 immS32 = (sint32)immU32;
	if( immS32 == 0 )
	{
		x64Gen_writeU8(x64GenContext, 0xFF);
		x64Gen_writeU8(x64GenContext, 0x20+(memRegister&7));
	}
	else if( immS32 >= -128 && immS32 <= 127 )
	{
		x64Gen_writeU8(x64GenContext, 0xFF);
		x64Gen_writeU8(x64GenContext, 0x60+(memRegister&7));
		x64Gen_writeU8(x64GenContext, (uint8)immU32);
	}
	else
	{
		x64Gen_writeU8(x64GenContext, 0xFF);
		x64Gen_writeU8(x64GenContext, 0xA0+(memRegister&7));
		x64Gen_writeU32(x64GenContext, immU32);
	}
}

void x64Gen_jmpc_far(x64GenContext_t* x64GenContext, sint32 conditionType, sint32 relativeDest)
{
	// far JMPc #+relativeDest
	if( conditionType == X86_CONDITION_NONE )
	{
		// E9 FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0xE9);
	}
	else if( conditionType == X86_CONDITION_UNSIGNED_BELOW || conditionType == X86_CONDITION_CARRY )
	{
		// 0F 82 FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x82);
	}
	else if( conditionType == X86_CONDITION_NOT_CARRY || conditionType == X86_CONDITION_UNSIGNED_ABOVE_EQUAL )
	{
		// 0F 83 FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x83);
	}
	else if( conditionType == X86_CONDITION_EQUAL )
	{
		// 0F 84 FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x84);
	}
	else if( conditionType == X86_CONDITION_NOT_EQUAL )
	{
		// 0F 85 FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x85);
	}
	else if( conditionType == X86_CONDITION_UNSIGNED_BELOW_EQUAL )
	{
		// 0F 86 FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x86);
	}
	else if( conditionType == X86_CONDITION_UNSIGNED_ABOVE )
	{
		// 0F 87 FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x87);
	}
	else if( conditionType == X86_CONDITION_SIGN )
	{
		// 0F 88 FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x88);
	}
	else if( conditionType == X86_CONDITION_NOT_SIGN )
	{
		// 0F 89 FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x89);
	}
	else if( conditionType == X86_CONDITION_PARITY )
	{
		// 0F 8A FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x8A);
	}
	else if( conditionType == X86_CONDITION_SIGNED_LESS )
	{
		// 0F 8C FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x8C);
	}
	else if( conditionType == X86_CONDITION_SIGNED_GREATER_EQUAL )
	{
		// 0F 8D FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x8D);
	}
	else if( conditionType == X86_CONDITION_SIGNED_LESS_EQUAL )
	{
		// 0F 8E FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x8E);
	}
	else if( conditionType == X86_CONDITION_SIGNED_GREATER )
	{
		// 0F 8F FFFFFFFF
		x64Gen_writeU8(x64GenContext, 0x0F);
		x64Gen_writeU8(x64GenContext, 0x8F);
	}
	else
		assert_dbg();
	x64Gen_writeU32(x64GenContext, (uint32)relativeDest);
}


void x64Gen_jmpc_near(x64GenContext_t* x64GenContext, sint32 conditionType, sint32 relativeDest)
{
	// near JMPc #+relativeDest
	if (conditionType == X86_CONDITION_NONE)
	{
		x64Gen_writeU8(x64GenContext, 0xEB);
	}
	else if (conditionType == X86_CONDITION_UNSIGNED_BELOW || conditionType == X86_CONDITION_CARRY)
	{
		x64Gen_writeU8(x64GenContext, 0x72);
	}
	else if (conditionType == X86_CONDITION_NOT_CARRY || conditionType == X86_CONDITION_UNSIGNED_ABOVE_EQUAL)
	{
		x64Gen_writeU8(x64GenContext, 0x73);
	}
	else if (conditionType == X86_CONDITION_EQUAL)
	{
		x64Gen_writeU8(x64GenContext, 0x74);
	}
	else if (conditionType == X86_CONDITION_NOT_EQUAL)
	{
		x64Gen_writeU8(x64GenContext, 0x75);
	}
	else if (conditionType == X86_CONDITION_UNSIGNED_BELOW_EQUAL)
	{
		x64Gen_writeU8(x64GenContext, 0x76);
	}
	else if (conditionType == X86_CONDITION_UNSIGNED_ABOVE)
	{
		x64Gen_writeU8(x64GenContext, 0x77);
	}
	else if (conditionType == X86_CONDITION_SIGN)
	{
		x64Gen_writeU8(x64GenContext, 0x78);
	}
	else if (conditionType == X86_CONDITION_NOT_SIGN)
	{
		x64Gen_writeU8(x64GenContext, 0x79);
	}
	else if (conditionType == X86_CONDITION_PARITY)
	{
		x64Gen_writeU8(x64GenContext, 0x7A);
	}
	else if (conditionType == X86_CONDITION_SIGNED_LESS)
	{
		x64Gen_writeU8(x64GenContext, 0x7C);
	}
	else if (conditionType == X86_CONDITION_SIGNED_GREATER_EQUAL)
	{
		x64Gen_writeU8(x64GenContext, 0x7D);
	}
	else if (conditionType == X86_CONDITION_SIGNED_LESS_EQUAL)
	{
		x64Gen_writeU8(x64GenContext, 0x7E);
	}
	else if (conditionType == X86_CONDITION_SIGNED_GREATER)
	{
		x64Gen_writeU8(x64GenContext, 0x7F);
	}
	else
		assert_dbg();
	x64Gen_writeU8(x64GenContext, (uint8)relativeDest);
}

void x64Gen_push_reg64(x64GenContext_t* x64GenContext, sint32 srcRegister)
{
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x50+(srcRegister&7));
}

void x64Gen_pop_reg64(x64GenContext_t* x64GenContext, sint32 destRegister)
{
	if( destRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0x58+(destRegister&7));
}

void x64Gen_jmp_reg64(x64GenContext_t* x64GenContext, sint32 srcRegister)
{
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0xFF);
	x64Gen_writeU8(x64GenContext, 0xE0+(srcRegister&7));
}

void x64Gen_call_reg64(x64GenContext_t* x64GenContext, sint32 srcRegister)
{
	if( srcRegister >= 8 )
		x64Gen_writeU8(x64GenContext, 0x41);
	x64Gen_writeU8(x64GenContext, 0xFF);
	x64Gen_writeU8(x64GenContext, 0xD0+(srcRegister&7));
}

void x64Gen_ret(x64GenContext_t* x64GenContext)
{
	x64Gen_writeU8(x64GenContext, 0xC3);
}

void x64Gen_int3(x64GenContext_t* x64GenContext)
{
	x64Gen_writeU8(x64GenContext, 0xCC);
}
