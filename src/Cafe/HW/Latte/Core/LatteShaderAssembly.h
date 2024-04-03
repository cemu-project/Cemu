#pragma once

// CF export

#define GPU7_DECOMPILER_CF_EXPORT_BASE_POSITION	(60)
#define GPU7_DECOMPILER_CF_EXPORT_POINT_SIZE	(61)

// CF instruction set

#define GPU7_CF_INST_NOP				(0x00)
#define GPU7_CF_INST_TEX				(0x01) 
#define GPU7_CF_INST_VTX				(0x02) // used only in GS copy program?
#define GPU7_CF_INST_LOOP_END			(0x05)
#define GPU7_CF_INST_LOOP_START_DX10	(0x06)
#define GPU7_CF_INST_LOOP_START_NO_AL	(0x07) // (Seen in Project Zero, Injustice: Gods Among Us)

#define GPU7_CF_INST_LOOP_BREAK			(0x09)
#define GPU7_CF_INST_JUMP				(0x0A)
#define GPU7_CF_INST_ELSE				(0x0D)
#define GPU7_CF_INST_POP				(0x0E)
#define GPU7_CF_INST_ELSE_AFTER			(0x0F)
#define GPU7_CF_INST_CALL				(0x12)
#define GPU7_CF_INST_CALL_FS			(0x13)
#define GPU7_CF_INST_RETURN				(0x14)
#define GPU7_CF_INST_EMIT_VERTEX		(0x15) // only available in geometry shader
#define GPU7_CF_INST_MEM_STREAM0_WRITE	(0x20) // for stream out
#define GPU7_CF_INST_MEM_STREAM1_WRITE	(0x21) // for stream out
#define GPU7_CF_INST_MEM_RING_WRITE		(0x26) // used to pass data to/from geometry shader
#define GPU7_CF_INST_EXPORT				(0x27)
#define GPU7_CF_INST_EXPORT_DONE		(0x28)

#define GPU7_CF_INST_ALU_MASK			(0x10000) // this mask is used to make sure that there is no collision between any GPU7_CF_INST_* values (e.g. GPU7_CF_INST_ALU_BREAK and GPU7_CF_INST_POP would collide due to different encoding but identical value)
#define GPU7_CF_INST_ALU				(0x08 | GPU7_CF_INST_ALU_MASK)
#define GPU7_CF_INST_ALU_PUSH_BEFORE	(0x09 | GPU7_CF_INST_ALU_MASK)
#define GPU7_CF_INST_ALU_POP_AFTER		(0x0A | GPU7_CF_INST_ALU_MASK)
#define GPU7_CF_INST_ALU_POP2_AFTER		(0x0B | GPU7_CF_INST_ALU_MASK)
#define GPU7_CF_INST_ALU_BREAK			(0x0E | GPU7_CF_INST_ALU_MASK)
#define GPU7_CF_INST_ALU_ELSE_AFTER		(0x0F | GPU7_CF_INST_ALU_MASK)


//#define INDEX_NONE						(-1) // used to indicate absolute access
#define GPU7_INDEX_AR_X						(0)
#define GPU7_INDEX_AR_Y						(1)
#define GPU7_INDEX_AR_Z						(2)
#define GPU7_INDEX_AR_W						(3)
#define GPU7_INDEX_LOOP						(4)
#define GPU7_INDEX_GLOBAL					(5)
#define GPU7_INDEX_GLOBAL_AR_X				(6)

#define GPU7_TEX_INST_VFETCH				(0x00)
#define GPU7_TEX_INST_MEM					(0x02)
#define GPU7_TEX_INST_LD					(0x03)
#define GPU7_TEX_INST_GET_TEXTURE_RESINFO	(0x04)
#define GPU7_TEX_INST_GET_COMP_TEX_LOD		(0x06)
#define GPU7_TEX_INST_GET_GRADIENTS_H		(0x07)
#define GPU7_TEX_INST_GET_GRADIENTS_V		(0x08)
#define GPU7_TEX_INST_SET_GRADIENTS_H		(0x0B)
#define GPU7_TEX_INST_SET_GRADIENTS_V		(0x0C)
#define GPU7_TEX_INST_SET_CUBEMAP_INDEX		(0x0E)
#define GPU7_TEX_INST_FETCH4				(0x0F)
#define GPU7_TEX_INST_SAMPLE				(0x10) // LOD from hw
#define GPU7_TEX_INST_SAMPLE_L				(0x11) // LOD from srcGpr.w
#define GPU7_TEX_INST_SAMPLE_LB				(0x12) // todo: Accesses LOD_BIAS field of instruction
#define GPU7_TEX_INST_SAMPLE_LZ				(0x13) // LOD is 0.0
#define GPU7_TEX_INST_SAMPLE_G				(0x14)
#define GPU7_TEX_INST_SAMPLE_C				(0x18) // sample & compare, LOD from hw
#define GPU7_TEX_INST_SAMPLE_C_L			(0x19) // sample & compare, LOD from srcGpr.w
#define GPU7_TEX_INST_SAMPLE_C_LZ			(0x1B) // sample & compare, LOD is 0.0

#define GPU7_TEX_UNNORMALIZED				(0)
#define GPU7_TEX_NORMALIZED					(1)


#define GPU7_ALU_SRC_UNUSED					(-1) // special value which indicates that the ALU operand is not used
#define GPU7_ALU_SRC_IS_GPR(__v)			((__v)>=0 && (__v)<128)	// 0-127
#define GPU7_ALU_SRC_IS_CBANK0(__v)			((__v)>=128 && (__v)<160)	// 128-159
#define GPU7_ALU_SRC_IS_CBANK1(__v)			((__v)>=160 && (__v)<192)	// 160-191
#define GPU7_ALU_SRC_IS_CONST_0F(__v)		((__v)==248)	// 248
#define GPU7_ALU_SRC_IS_CONST_1F(__v)		((__v)==249)	// 249
#define GPU7_ALU_SRC_IS_CONST_1I(__v)		((__v)==250)	// 250
#define GPU7_ALU_SRC_IS_CONST_M1I(__v)		((__v)==251)	// 251 (0xFB)
#define GPU7_ALU_SRC_IS_CONST_0_5F(__v)		((__v)==252)	// 252 (0xFC)
#define GPU7_ALU_SRC_IS_LITERAL(__v)		((__v)==253)	// 253 (0xFD)
#define GPU7_ALU_SRC_IS_PV(__v)				((__v)==254)	// 254 (0xFE)
#define GPU7_ALU_SRC_IS_PS(__v)				((__v)==255)	// 255 (0xFF)
#define GPU7_ALU_SRC_IS_CFILE(__v)			((__v)>=256 && (__v)<512) // 256-511 (uniform register)

#define GPU7_ALU_SRC_IS_ANY_CONST(__v)		((__v)>=248 && (__v)<253) // special macro to handle all GPU7_ALU_SRC_IS_CONST_*

#define GPU7_ALU_SRC_GET_GPR_INDEX(__v)		((__v))
#define GPU7_ALU_SRC_GET_CFILE_INDEX(__v)	((__v)-256)
#define GPU7_ALU_SRC_GET_CBANK0_INDEX(__v)	((__v)-128)
#define GPU7_ALU_SRC_GET_CBANK1_INDEX(__v)	((__v)-160)

#define GPU7_ALU_SRC_LITERAL					(0xFD)						
