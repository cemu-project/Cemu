// GPU7 instruction set is a hybrid between R600 and R700

// CF instructions
#define CF_INST_NOP					(0x00)
#define CF_INST_TEX					(0x01) 
#define CF_INST_LOOP_END			(0x05)
#define CF_INST_LOOP_START_DX10		(0x06)
#define CF_INST_JUMP				(0x0A)
#define CF_INST_ELSE				(0x0D)
#define CF_INST_POP					(0x0E)
#define CF_INST_ELSE_AFTER			(0x0F)
#define CF_INST_CALL_FS				(0x13)
#define CF_INST_EXPORT				(0x27)
#define CF_INST_EXPORT_DONE			(0x28)

#define CF_INST_ALU					(0x08)
#define CF_INST_ALU_PUSH_BEFORE		(0x09)
#define CF_INST_ALU_POP_AFTER		(0x0A)
#define CF_INST_ALU_POP2_AFTER		(0x0B)
#define CF_INST_ALU_BREAK			(0x0E) // leave loop if pred is modified

#define ALU_OMOD_NONE				(0)
#define ALU_OMOD_MUL2				(1)
#define ALU_OMOD_MUL4				(2)
#define ALU_OMOD_DIV2				(3)

// ALU op2 instructions
#define ALU_OP2_INST_ADD			(0x000)
#define ALU_OP2_INST_MUL			(0x001)
#define ALU_OP2_INST_MUL_IEEE		(0x002)
#define ALU_OP2_INST_MAX			(0x003)
#define ALU_OP2_INST_MIN			(0x004)
#define ALU_OP2_INST_MAX_DX10		(0x005)
#define ALU_OP2_INST_MIN_DX10		(0x006)
#define ALU_OP2_INST_SETE			(0x008)
#define ALU_OP2_INST_SETGT			(0x009)
#define ALU_OP2_INST_SETGE			(0x00A)
#define ALU_OP2_INST_SETNE			(0x00B)
#define ALU_OP2_INST_SETE_DX10		(0x00C)
#define ALU_OP2_INST_SETGT_DX10		(0x00D)
#define ALU_OP2_INST_SETGE_DX10		(0x00E)
#define ALU_OP2_INST_SETNE_DX10		(0x00F)
#define ALU_OP2_INST_FLOOR			(0x014)
#define ALU_OP2_INST_FRACT			(0x010)
#define ALU_OP2_INST_TRUNC			(0x011)
#define ALU_OP2_INST_RNDNE			(0x013)
#define ALU_OP2_INST_MOVA_FLOOR		(0x016) // changes address register
#define ALU_OP2_INST_MOVA_INT		(0x018) // changes address register
#define ALU_OP2_INST_MOV			(0x019)
#define ALU_OP2_INST_NOP			(0x01A)
#define ALU_OP2_INST_PRED_SETE		(0x020)
#define ALU_OP2_INST_PRED_SETGT		(0x021)
#define ALU_OP2_INST_PRED_SETGE		(0x022)
#define ALU_OP2_INST_PRED_SETNE		(0x023)
#define ALU_OP2_INST_AND_INT		(0x030) // integer instruction
#define ALU_OP2_INST_OR_INT			(0x031) // integer instruction
#define ALU_OP2_INST_XOR_INT		(0x032) // integer instruction
#define ALU_OP2_INST_NOT_INT		(0x033) // integer instruction
#define ALU_OP2_INST_ADD_INT		(0x034) // integer instruction
#define ALU_OP2_INST_SUB_INT		(0x035) // integer instruction
#define ALU_OP2_INST_MAX_INT		(0x036) // integer instruction
#define ALU_OP2_INST_MIN_INT		(0x037) // integer instruction
#define ALU_OP2_INST_MAX_UINT		(0x038) // integer instruction
#define ALU_OP2_INST_MIN_UINT		(0x039) // integer instruction
#define ALU_OP2_INST_SETE_INT		(0x03A) // integer instruction
#define ALU_OP2_INST_SETGT_INT		(0x03B) // integer instruction
#define ALU_OP2_INST_SETGE_INT		(0x03C) // integer instruction
#define ALU_OP2_INST_SETNE_INT		(0x03D) // integer instruction
#define ALU_OP2_INST_SETGT_UINT		(0x03E) // integer instruction
#define ALU_OP2_INST_SETGE_UINT		(0x03F) // integer instruction
#define ALU_OP2_INST_PRED_SETE_INT	(0x042) // integer instruction 
#define ALU_OP2_INST_PRED_SETGT_INT	(0x043) // integer instruction
#define ALU_OP2_INST_PRED_SETGE_INT	(0x044) // integer instruction
#define ALU_OP2_INST_PRED_SETNE_INT	(0x045) // integer instruction 
#define ALU_OP2_INST_KILLE			(0x02C)
#define ALU_OP2_INST_KILLGT			(0x02D)
#define ALU_OP2_INST_KILLGE			(0x02E)
#define ALU_OP2_INST_KILLE_INT		(0x046)
#define ALU_OP2_INST_KILLGT_INT		(0x047)
#define ALU_OP2_INST_KILLNE_INT		(0x049)
#define ALU_OP2_INST_DOT4			(0x050)
#define ALU_OP2_INST_DOT4_IEEE		(0x051)
#define ALU_OP2_INST_CUBE			(0x052)
#define ALU_OP2_INST_EXP_IEEE		(0x061)
#define ALU_OP2_INST_LOG_CLAMPED	(0x062)
#define ALU_OP2_INST_LOG_IEEE		(0x063)
#define ALU_OP2_INST_SQRT_IEEE		(0x06A)
#define ALU_OP2_INST_SIN			(0x06E)
#define ALU_OP2_INST_COS			(0x06F)
#define ALU_OP2_INST_RECIP_FF		(0x065)
#define ALU_OP2_INST_RECIP_IEEE		(0x066)
#define ALU_OP2_INST_RECIPSQRT_CLAMPED (0x067)
#define ALU_OP2_INST_RECIPSQRT_FF	(0x068)
#define ALU_OP2_INST_RECIPSQRT_IEEE	(0x069)
#define ALU_OP2_INST_FLT_TO_INT		(0x06B) // conversion instruction
#define ALU_OP2_INST_INT_TO_FLOAT	(0x06C) // conversion instruction
#define ALU_OP2_INST_UINT_TO_FLOAT	(0x06D) // conversion instruction
#define ALU_OP2_INST_ASHR_INT		(0x070) // integer instruction
#define ALU_OP2_INST_LSHR_INT		(0x071) // integer instruction
#define ALU_OP2_INST_LSHL_INT		(0x072) // integer instruction
#define ALU_OP2_INST_MULLO_INT		(0x073) // integer instruction
#define ALU_OP2_INST_MULLO_UINT		(0x075) // integer instruction
#define ALU_OP2_INST_FLT_TO_UINT	(0x079) // conversion instruction

// ALU op3 instructions
#define ALU_OP3_INST_MULADD			(0x10)
#define ALU_OP3_INST_MULADD_M2		(0x11)
#define ALU_OP3_INST_MULADD_M4		(0x12)
#define ALU_OP3_INST_MULADD_D2		(0x13)
#define ALU_OP3_INST_MULADD_IEEE	(0x14)
#define ALU_OP3_INST_CMOVE			(0x18)
#define ALU_OP3_INST_CMOVGT			(0x19)
#define ALU_OP3_INST_CMOVGE			(0x1A)
#define ALU_OP3_INST_CNDE_INT		(0x1C) // integer instruction
#define ALU_OP3_INST_CNDGT_INT		(0x1D) // integer instruction
#define ALU_OP3_INST_CMOVGE_INT		(0x1E) // integer instruction

// fetch shader
#define		VTX_INST_SEMANTIC	(0x01)
#define		VTX_INST_MEM		(0x02)