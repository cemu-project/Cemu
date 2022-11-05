#pragma once

#define PPCREC_IML_OP_FLAG_SIGNEXTEND			(1<<0)
#define PPCREC_IML_OP_FLAG_SWITCHENDIAN			(1<<1)
#define PPCREC_IML_OP_FLAG_NOT_EXPANDED			(1<<2) // set single-precision load instructions to indicate that the value should not be rounded to double-precision 
#define PPCREC_IML_OP_FLAG_UNUSED				(1<<7) // used to mark instructions that are not used


enum
{
	PPCREC_IML_OP_ASSIGN,			// '=' operator
	PPCREC_IML_OP_ENDIAN_SWAP,		// '=' operator with 32bit endian swap
	PPCREC_IML_OP_ADD,				// '+' operator
	PPCREC_IML_OP_SUB,				// '-' operator
	PPCREC_IML_OP_SUB_CARRY_UPDATE_CARRY, // complex operation, result = operand + ~operand2 + carry bit, updates carry bit
	PPCREC_IML_OP_COMPARE_SIGNED,	// arithmetic/signed comparison operator (updates cr)
	PPCREC_IML_OP_COMPARE_UNSIGNED, // logical/unsigned comparison operator (updates cr)
	PPCREC_IML_OP_MULTIPLY_SIGNED,  // '*' operator (signed multiply)
	PPCREC_IML_OP_MULTIPLY_HIGH_UNSIGNED, // unsigned 64bit multiply, store only high 32bit-word of result
	PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED, // signed 64bit multiply, store only high 32bit-word of result
	PPCREC_IML_OP_DIVIDE_SIGNED,	// '/' operator (signed divide)
	PPCREC_IML_OP_DIVIDE_UNSIGNED,	// '/' operator (unsigned divide)
	PPCREC_IML_OP_ADD_CARRY,		// complex operation, result = operand + carry bit, updates carry bit
	PPCREC_IML_OP_ADD_CARRY_ME,		// complex operation, result = operand + carry bit + (-1), updates carry bit
	PPCREC_IML_OP_ADD_UPDATE_CARRY,	// '+' operator but also updates carry flag
	PPCREC_IML_OP_ADD_CARRY_UPDATE_CARRY, // '+' operator and also adds carry, updates carry flag
	// assign operators with cast
	PPCREC_IML_OP_ASSIGN_S16_TO_S32, // copy 16bit and sign extend
	PPCREC_IML_OP_ASSIGN_S8_TO_S32,  // copy 8bit and sign extend
	// binary operation
	PPCREC_IML_OP_OR,				// '|' operator
	PPCREC_IML_OP_ORC,				// '|' operator, second operand is complemented first
	PPCREC_IML_OP_AND,				// '&' operator
	PPCREC_IML_OP_XOR,				// '^' operator
	PPCREC_IML_OP_LEFT_ROTATE,		// left rotate operator
	PPCREC_IML_OP_LEFT_SHIFT,		// shift left operator
	PPCREC_IML_OP_RIGHT_SHIFT,		// right shift operator (unsigned)
	PPCREC_IML_OP_NOT,				// complement each bit
	PPCREC_IML_OP_NEG,				// negate
	// ppc
	PPCREC_IML_OP_RLWIMI,			// RLWIMI instruction (rotate, merge based on mask)
	PPCREC_IML_OP_SRAW,				// SRAWI/SRAW instruction (algebraic shift right, sets ca flag)
	PPCREC_IML_OP_SLW,				// SLW (shift based on register by up to 63 bits)
	PPCREC_IML_OP_SRW,				// SRW (shift based on register by up to 63 bits)
	PPCREC_IML_OP_CNTLZW,
	PPCREC_IML_OP_SUBFC,			// SUBFC and SUBFIC (subtract from and set carry)
	PPCREC_IML_OP_DCBZ,				// clear 32 bytes aligned to 0x20
	PPCREC_IML_OP_MFCR,				// copy cr to gpr
	PPCREC_IML_OP_MTCRF,			// copy gpr to cr (with mask)
	// condition register
	PPCREC_IML_OP_CR_CLEAR,			// clear cr bit
	PPCREC_IML_OP_CR_SET,			// set cr bit
	PPCREC_IML_OP_CR_OR,			// OR cr bits
	PPCREC_IML_OP_CR_ORC,			// OR cr bits, complement second input operand bit first
	PPCREC_IML_OP_CR_AND,			// AND cr bits
	PPCREC_IML_OP_CR_ANDC,			// AND cr bits, complement second input operand bit first
	// FPU
	PPCREC_IML_OP_FPR_ADD_BOTTOM,
	PPCREC_IML_OP_FPR_ADD_PAIR,
	PPCREC_IML_OP_FPR_SUB_PAIR,
	PPCREC_IML_OP_FPR_SUB_BOTTOM,
	PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM,
	PPCREC_IML_OP_FPR_MULTIPLY_PAIR,
	PPCREC_IML_OP_FPR_DIVIDE_BOTTOM,
	PPCREC_IML_OP_FPR_DIVIDE_PAIR,
	PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP,
	PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP,
	PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM,
	PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_TOP, // leave bottom of destination untouched
	PPCREC_IML_OP_FPR_COPY_TOP_TO_TOP, // leave bottom of destination untouched
	PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM, // leave top of destination untouched
	PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED,
	PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64, // expand bottom f32 to f64 in bottom and top half
	PPCREC_IML_OP_FPR_BOTTOM_FRES_TO_BOTTOM_AND_TOP, // calculate reciprocal with Espresso accuracy of source bottom half and write result to destination bottom and top half
	PPCREC_IML_OP_FPR_FCMPO_BOTTOM,
	PPCREC_IML_OP_FPR_FCMPU_BOTTOM,
	PPCREC_IML_OP_FPR_FCMPU_TOP,
	PPCREC_IML_OP_FPR_NEGATE_BOTTOM,
	PPCREC_IML_OP_FPR_NEGATE_PAIR,
	PPCREC_IML_OP_FPR_ABS_BOTTOM, // abs(fp0)
	PPCREC_IML_OP_FPR_ABS_PAIR,
	PPCREC_IML_OP_FPR_FRES_PAIR, // 1.0/fp approx (Espresso accuracy)
	PPCREC_IML_OP_FPR_FRSQRTE_PAIR, // 1.0/sqrt(fp) approx (Espresso accuracy)
	PPCREC_IML_OP_FPR_NEGATIVE_ABS_BOTTOM, // -abs(fp0)
	PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM, // round 64bit double to 64bit double with 32bit float precision (in bottom half of xmm register)
	PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_PAIR, // round two 64bit doubles to 64bit double with 32bit float precision
	PPCREC_IML_OP_FPR_BOTTOM_RECIPROCAL_SQRT,
	PPCREC_IML_OP_FPR_BOTTOM_FCTIWZ,
	PPCREC_IML_OP_FPR_SELECT_BOTTOM, // selectively copy bottom value from operand B or C based on value in operand A
	PPCREC_IML_OP_FPR_SELECT_PAIR, // selectively copy top/bottom from operand B or C based on value in top/bottom of operand A
	// PS
	PPCREC_IML_OP_FPR_SUM0,
	PPCREC_IML_OP_FPR_SUM1,
};

#define PPCREC_IML_OP_FPR_COPY_PAIR (PPCREC_IML_OP_ASSIGN)

enum
{
	PPCREC_IML_MACRO_BLR,			// macro for BLR instruction code
	PPCREC_IML_MACRO_BLRL,			// macro for BLRL instruction code
	PPCREC_IML_MACRO_BCTR,			// macro for BCTR instruction code
	PPCREC_IML_MACRO_BCTRL,			// macro for BCTRL instruction code
	PPCREC_IML_MACRO_BL,			// call to different function (can be within same function)
	PPCREC_IML_MACRO_B_FAR,			// branch to different function
	PPCREC_IML_MACRO_COUNT_CYCLES,	// decrease current remaining thread cycles by a certain amount
	PPCREC_IML_MACRO_HLE,			// HLE function call
	PPCREC_IML_MACRO_MFTB,			// get TB register value (low or high)
	PPCREC_IML_MACRO_LEAVE,			// leaves recompiler and switches to interpeter
	// debugging
	PPCREC_IML_MACRO_DEBUGBREAK,	// throws a debugbreak
};

enum
{
	PPCREC_JUMP_CONDITION_NONE,
	PPCREC_JUMP_CONDITION_E, // equal / zero
	PPCREC_JUMP_CONDITION_NE, // not equal / not zero
	PPCREC_JUMP_CONDITION_LE, // less or equal
	PPCREC_JUMP_CONDITION_L, // less
	PPCREC_JUMP_CONDITION_GE, // greater or equal
	PPCREC_JUMP_CONDITION_G, // greater
	// special case:
	PPCREC_JUMP_CONDITION_SUMMARYOVERFLOW, // needs special handling
	PPCREC_JUMP_CONDITION_NSUMMARYOVERFLOW, // not summaryoverflow

};

enum
{
	PPCREC_CR_MODE_COMPARE_SIGNED,
	PPCREC_CR_MODE_COMPARE_UNSIGNED, // alias logic compare
	// others: 	PPCREC_CR_MODE_ARITHMETIC,
	PPCREC_CR_MODE_ARITHMETIC, // arithmetic use (for use with add/sub instructions without generating extra code)
	PPCREC_CR_MODE_LOGICAL,
};

enum
{
	PPCREC_IML_TYPE_NONE,
	PPCREC_IML_TYPE_NO_OP,				// no-op instruction
	PPCREC_IML_TYPE_JUMPMARK,			// possible jump destination (generated before each ppc instruction)
	PPCREC_IML_TYPE_R_R,				// r* (op) *r
	PPCREC_IML_TYPE_R_R_R,				// r* = r* (op) r*
	PPCREC_IML_TYPE_R_R_S32,			// r* = r* (op) s32*
	PPCREC_IML_TYPE_LOAD,				// r* = [r*+s32*]
	PPCREC_IML_TYPE_LOAD_INDEXED,		// r* = [r*+r*]
	PPCREC_IML_TYPE_STORE,				// [r*+s32*] = r*
	PPCREC_IML_TYPE_STORE_INDEXED,		// [r*+r*] = r*
	PPCREC_IML_TYPE_R_NAME,				// r* = name
	PPCREC_IML_TYPE_NAME_R,				// name* = r*
	PPCREC_IML_TYPE_R_S32,				// r* (op) imm
	PPCREC_IML_TYPE_MACRO,
	PPCREC_IML_TYPE_CJUMP,				// conditional jump
	PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK,	// jumps only if remaining thread cycles >= 0
	PPCREC_IML_TYPE_PPC_ENTER,			// used to mark locations that should be written to recompilerCallTable
	PPCREC_IML_TYPE_CR,					// condition register specific operations (one or more operands)
	// conditional
	PPCREC_IML_TYPE_CONDITIONAL_R_S32,
	// FPR
	PPCREC_IML_TYPE_FPR_R_NAME,			// name = f*
	PPCREC_IML_TYPE_FPR_NAME_R,			// f* = name
	PPCREC_IML_TYPE_FPR_LOAD,			// r* = (bitdepth) [r*+s32*] (single or paired single mode)
	PPCREC_IML_TYPE_FPR_LOAD_INDEXED,	// r* = (bitdepth) [r*+r*] (single or paired single mode)
	PPCREC_IML_TYPE_FPR_STORE,			// (bitdepth) [r*+s32*] = r* (single or paired single mode)
	PPCREC_IML_TYPE_FPR_STORE_INDEXED,	// (bitdepth) [r*+r*] = r* (single or paired single mode)
	PPCREC_IML_TYPE_FPR_R_R,
	PPCREC_IML_TYPE_FPR_R_R_R,
	PPCREC_IML_TYPE_FPR_R_R_R_R,
	PPCREC_IML_TYPE_FPR_R,
};

enum
{
	PPCREC_NAME_NONE,
	PPCREC_NAME_TEMPORARY,
	PPCREC_NAME_R0 = 1000,
	PPCREC_NAME_SPR0 = 2000,
	PPCREC_NAME_FPR0 = 3000,
	PPCREC_NAME_TEMPORARY_FPR0 = 4000, // 0 to 7
	//PPCREC_NAME_CR0 = 3000, // value mapped condition register (usually it isn't needed and can be optimized away)
};

// special cases for LOAD/STORE
#define PPC_REC_LOAD_LWARX_MARKER	(100)	// lwarx instruction (similar to LWZX but sets reserved address/value)
#define PPC_REC_STORE_STWCX_MARKER	(100)	// stwcx instruction (similar to STWX but writes only if reservation from LWARX is valid)
#define PPC_REC_STORE_STSWI_1		(200)   // stswi nb = 1
#define PPC_REC_STORE_STSWI_2		(201)   // stswi nb = 2
#define PPC_REC_STORE_STSWI_3		(202)   // stswi nb = 3
#define PPC_REC_STORE_LSWI_1		(200)   // lswi nb = 1
#define PPC_REC_STORE_LSWI_2		(201)   // lswi nb = 2
#define PPC_REC_STORE_LSWI_3		(202)   // lswi nb = 3

#define PPC_REC_INVALID_REGISTER		0xFF

#define PPCREC_CR_BIT_LT	0
#define PPCREC_CR_BIT_GT	1
#define PPCREC_CR_BIT_EQ	2
#define PPCREC_CR_BIT_SO	3

enum
{
	// fpr load
	PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0,
	PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1,
	PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0,
	PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0,
	PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1,
	PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0,
	PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1,
	PPCREC_FPR_LD_MODE_PSQ_S16_PS0,
	PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1,
	PPCREC_FPR_LD_MODE_PSQ_U16_PS0,
	PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1,
	PPCREC_FPR_LD_MODE_PSQ_S8_PS0,
	PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1,
	PPCREC_FPR_LD_MODE_PSQ_U8_PS0,
	PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1,
	// fpr store
	PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0, // store 1 single precision float from ps0
	PPCREC_FPR_ST_MODE_DOUBLE_FROM_PS0, // store 1 double precision float from ps0

	PPCREC_FPR_ST_MODE_UI32_FROM_PS0, // store raw low-32bit of PS0

	PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1,
	PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0,
	PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1,
	PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0,
	PPCREC_FPR_ST_MODE_PSQ_S8_PS0,
	PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1,
	PPCREC_FPR_ST_MODE_PSQ_U8_PS0,
	PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1,
	PPCREC_FPR_ST_MODE_PSQ_U16_PS0,
	PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1,
	PPCREC_FPR_ST_MODE_PSQ_S16_PS0,
	PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1,
};

struct IMLInstruction
{
	uint8 type;
	uint8 operation;
	uint8 crRegister; // set to 0xFF if not set, not all IML instruction types support cr.
	uint8 crMode; // only used when crRegister is valid, used to differentiate between various forms of condition flag set/clear behavior
	uint32 crIgnoreMask; // bit set for every respective CR bit that doesn't need to be updated
	uint32 associatedPPCAddress; // ppc address that is associated with this instruction
	union
	{
		struct
		{
			uint8 _padding[7];
		}padding;
		struct
		{
			// R (op) A [update cr* in mode *]
			uint8 registerResult;
			uint8 registerA;
		}op_r_r;
		struct
		{
			// R = A (op) B [update cr* in mode *]
			uint8 registerResult;
			uint8 registerA;
			uint8 registerB;
		}op_r_r_r;
		struct
		{
			// R = A (op) immS32 [update cr* in mode *]
			uint8 registerResult;
			uint8 registerA;
			sint32 immS32;
		}op_r_r_s32;
		struct
		{
			// R/F = NAME or NAME = R/F
			uint8 registerIndex;
			uint32 name;
		}op_r_name;
		struct
		{
			// R (op) s32 [update cr* in mode *]
			uint8 registerIndex;
			sint32 immS32;
		}op_r_immS32;
		struct
		{
			uint32 address;
			uint8 flags;
		}op_jumpmark;
		struct
		{
			uint32 param;
			uint32 param2;
			uint16 paramU16;
		}op_macro;
		struct
		{
			uint32 jumpmarkAddress;
			bool jumpAccordingToSegment; //IMLSegment* destinationSegment; // if set, this replaces jumpmarkAddress
			uint8 condition; // only used when crRegisterIndex is 8 or above (update: Apparently only used to mark jumps without a condition? -> Cleanup)
			uint8 crRegisterIndex;
			uint8 crBitIndex;
			bool  bitMustBeSet;
		}op_conditionalJump;
		struct
		{
			uint8 registerData;
			uint8 registerMem;
			uint8 registerMem2;
			uint8 registerGQR;
			uint8 copyWidth;
			struct
			{
				bool swapEndian : 1;
				bool signExtend : 1;
				bool notExpanded : 1; // for floats
			}flags2;
			uint8 mode; // transfer mode (copy width, ps0/ps1 behavior)
			sint32 immS32;
		}op_storeLoad;
		struct
		{
			uint8 registerResult;
			uint8 registerOperand;
			uint8 flags;
		}op_fpr_r_r;
		struct
		{
			uint8 registerResult;
			uint8 registerOperandA;
			uint8 registerOperandB;
			uint8 flags;
		}op_fpr_r_r_r;
		struct
		{
			uint8 registerResult;
			uint8 registerOperandA;
			uint8 registerOperandB;
			uint8 registerOperandC;
			uint8 flags;
		}op_fpr_r_r_r_r;
		struct
		{
			uint8 registerResult;
		}op_fpr_r;
		struct
		{
			uint32 ppcAddress;
			uint32 x64Offset;
		}op_ppcEnter;
		struct
		{
			uint8 crD; // crBitIndex (result)
			uint8 crA; // crBitIndex
			uint8 crB; // crBitIndex
		}op_cr;
		// conditional operations (emitted if supported by target platform)
		struct
		{
			// r_s32
			uint8 registerIndex;
			sint32 immS32;
			// condition
			uint8 crRegisterIndex;
			uint8 crBitIndex;
			bool  bitMustBeSet;
		}op_conditional_r_s32;
	};

	bool IsSuffixInstruction() const
	{
		if (type == PPCREC_IML_TYPE_MACRO && (operation == PPCREC_IML_MACRO_BLR || operation == PPCREC_IML_MACRO_BCTR) ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_BL ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_B_FAR ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_BLRL ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_BCTRL ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_LEAVE ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_HLE ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_MFTB ||
			type == PPCREC_IML_TYPE_PPC_ENTER ||
			type == PPCREC_IML_TYPE_CJUMP ||
			type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
			return true;
		return false;
	}

	// instruction setters
	void make_jumpmark(uint32 address)
	{
		type = PPCREC_IML_TYPE_JUMPMARK;
		op_jumpmark.address = address;
	}

	void make_macro(uint32 macroId, uint32 param, uint32 param2, uint16 paramU16)
	{
		type = PPCREC_IML_TYPE_MACRO;
		operation = macroId;
		op_macro.param = param;
		op_macro.param2 = param2;
		op_macro.paramU16 = paramU16;
	}

	void make_ppcEnter(uint32 ppcAddress)
	{
		type = PPCREC_IML_TYPE_PPC_ENTER;
		operation = 0;
		op_ppcEnter.ppcAddress = ppcAddress;
		op_ppcEnter.x64Offset = 0;
		associatedPPCAddress = 0;
	}

};