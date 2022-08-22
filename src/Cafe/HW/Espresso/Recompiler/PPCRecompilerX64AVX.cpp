#include "PPCRecompiler.h"
#include "PPCRecompilerX64.h"

void _x64Gen_writeMODRMDeprecated(x64GenContext_t* x64GenContext, sint32 dataRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32);

void _x64Gen_vex128_nds(x64GenContext_t* x64GenContext, uint8 opcodeMap, uint8 additionalOperand, uint8 pp, uint8 vex_ext, uint8 vex_r, uint8 vex_b, uint8 opcode)
{
	if(vex_b != 0)
		x64Gen_writeU8(x64GenContext, 0xC4); // three byte VEX
	else
		x64Gen_writeU8(x64GenContext, 0xC5); // two byte VEX

	if (vex_b != 0)
	{
		uint8 vex_x = 0;
		x64Gen_writeU8(x64GenContext, (vex_r ? 0x00 : 0x80) | (vex_x ? 0x00 : 0x40) | (vex_b ? 0x00 : 0x20) | 1);
	}

	x64Gen_writeU8(x64GenContext, (vex_ext<<7) | (((~additionalOperand)&0xF)<<3) | pp);

	x64Gen_writeU8(x64GenContext, opcode);
}

#define VEX_PP_0F		0 // guessed
#define VEX_PP_66_0F	1
#define VEX_PP_F3_0F	2 // guessed
#define VEX_PP_F2_0F	3 // guessed


void x64Gen_avx_VPUNPCKHQDQ_xmm_xmm_xmm(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 srcRegisterA, sint32 srcRegisterB)
{
	_x64Gen_vex128_nds(x64GenContext, 0, srcRegisterA, VEX_PP_66_0F, dstRegister < 8 ? 1 : 0, (dstRegister >= 8 && srcRegisterB >= 8) ? 1 : 0, srcRegisterB < 8 ? 0 : 1, 0x6D);

	x64Gen_writeU8(x64GenContext, 0xC0 + (srcRegisterB & 7) + (dstRegister & 7) * 8);
}

void x64Gen_avx_VUNPCKHPD_xmm_xmm_xmm(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 srcRegisterA, sint32 srcRegisterB)
{
	_x64Gen_vex128_nds(x64GenContext, 0, srcRegisterA, VEX_PP_66_0F, dstRegister < 8 ? 1 : 0, (dstRegister >= 8 && srcRegisterB >= 8) ? 1 : 0, srcRegisterB < 8 ? 0 : 1, 0x15);

	x64Gen_writeU8(x64GenContext, 0xC0 + (srcRegisterB & 7) + (dstRegister & 7) * 8);
}

void x64Gen_avx_VSUBPD_xmm_xmm_xmm(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 srcRegisterA, sint32 srcRegisterB)
{
	_x64Gen_vex128_nds(x64GenContext, 0, srcRegisterA, VEX_PP_66_0F, dstRegister < 8 ? 1 : 0, (dstRegister >= 8 && srcRegisterB >= 8) ? 1 : 0, srcRegisterB < 8 ? 0 : 1, 0x5C);

	x64Gen_writeU8(x64GenContext, 0xC0 + (srcRegisterB & 7) + (dstRegister & 7) * 8);
}