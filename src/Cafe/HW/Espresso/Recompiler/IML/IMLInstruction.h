#pragma once

enum
{
	PPCREC_IML_OP_ASSIGN,			// '=' operator
	PPCREC_IML_OP_ENDIAN_SWAP,		// '=' operator with 32bit endian swap
	PPCREC_IML_OP_MULTIPLY_SIGNED,  // '*' operator (signed multiply)
	PPCREC_IML_OP_MULTIPLY_HIGH_UNSIGNED, // unsigned 64bit multiply, store only high 32bit-word of result
	PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED, // signed 64bit multiply, store only high 32bit-word of result
	PPCREC_IML_OP_DIVIDE_SIGNED,	// '/' operator (signed divide)
	PPCREC_IML_OP_DIVIDE_UNSIGNED,	// '/' operator (unsigned divide)

	// binary operation
	PPCREC_IML_OP_OR,				// '|' operator
	PPCREC_IML_OP_AND,				// '&' operator
	PPCREC_IML_OP_XOR,				// '^' operator
	PPCREC_IML_OP_LEFT_ROTATE,		// left rotate operator
	PPCREC_IML_OP_LEFT_SHIFT,		// shift left operator
	PPCREC_IML_OP_RIGHT_SHIFT_U,	// right shift operator (unsigned)
	PPCREC_IML_OP_RIGHT_SHIFT_S,	// right shift operator (signed)
	// ppc
	PPCREC_IML_OP_RLWIMI,			// RLWIMI instruction (rotate, merge based on mask)
	PPCREC_IML_OP_SLW,				// SLW (shift based on register by up to 63 bits)
	PPCREC_IML_OP_SRW,				// SRW (shift based on register by up to 63 bits)
	PPCREC_IML_OP_CNTLZW,
	PPCREC_IML_OP_DCBZ,				// clear 32 bytes aligned to 0x20
	PPCREC_IML_OP_MFCR,				// copy cr to gpr
	PPCREC_IML_OP_MTCRF,			// copy gpr to cr (with mask)
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
	PPCREC_IML_OP_FPR_FCMPO_BOTTOM, // deprecated
	PPCREC_IML_OP_FPR_FCMPU_BOTTOM, // deprecated
	PPCREC_IML_OP_FPR_FCMPU_TOP, // deprecated
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



	// working towards defining ops per-form
	// R_R_R only

	// R_R_S32 only

	// R_R_R + R_R_S32
	PPCREC_IML_OP_ADD, // also R_R_R_CARRY
	PPCREC_IML_OP_SUB,

	// R_R only
	PPCREC_IML_OP_NOT,
	PPCREC_IML_OP_NEG,
	PPCREC_IML_OP_ASSIGN_S16_TO_S32,
	PPCREC_IML_OP_ASSIGN_S8_TO_S32,

	// R_R_R_carry
	PPCREC_IML_OP_ADD_WITH_CARRY, // similar to ADD but also adds carry bit (0 or 1)
};

#define PPCREC_IML_OP_FPR_COPY_PAIR (PPCREC_IML_OP_ASSIGN)

enum
{
	PPCREC_IML_MACRO_B_TO_REG,		// branch to PPC address in register (used for BCCTR, BCLR)

	PPCREC_IML_MACRO_BL,			// call to different function (can be within same function)
	PPCREC_IML_MACRO_B_FAR,			// branch to different function
	PPCREC_IML_MACRO_COUNT_CYCLES,	// decrease current remaining thread cycles by a certain amount
	PPCREC_IML_MACRO_HLE,			// HLE function call
	PPCREC_IML_MACRO_MFTB,			// get TB register value (low or high)
	PPCREC_IML_MACRO_LEAVE,			// leaves recompiler and switches to interpeter
	// debugging
	PPCREC_IML_MACRO_DEBUGBREAK,	// throws a debugbreak
};

enum class IMLCondition : uint8
{
	EQ,
	NEQ,
	SIGNED_GT,
	SIGNED_LT,
	UNSIGNED_GT,
	UNSIGNED_LT,

	SIGNED_OVERFLOW,
	SIGNED_NOVERFLOW,

	// floating point conditions
	UNORDERED_GT, // a > b, false if either is NaN
	UNORDERED_LT, // a < b, false if either is NaN
	UNORDERED_EQ, // a == b, false if either is NaN
	UNORDERED_U, // unordered (true if either operand is NaN)

	ORDERED_GT,
	ORDERED_LT,
	ORDERED_EQ,
	ORDERED_U
};

enum
{
	PPCREC_IML_TYPE_NONE,
	PPCREC_IML_TYPE_NO_OP,				// no-op instruction
	PPCREC_IML_TYPE_R_R,				// r* = (op) *r			(can also be r* (op) *r) 
	PPCREC_IML_TYPE_R_R_R,				// r* = r* (op) r*
	PPCREC_IML_TYPE_R_R_R_CARRY,		// r* = r* (op) r*		(reads and/or updates carry)
	PPCREC_IML_TYPE_R_R_S32,			// r* = r* (op) s32*
	PPCREC_IML_TYPE_R_R_S32_CARRY,		// r* = r* (op) s32*	(reads and/or updates carry)
	PPCREC_IML_TYPE_LOAD,				// r* = [r*+s32*]
	PPCREC_IML_TYPE_LOAD_INDEXED,		// r* = [r*+r*]
	PPCREC_IML_TYPE_STORE,				// [r*+s32*] = r*
	PPCREC_IML_TYPE_STORE_INDEXED,		// [r*+r*] = r*
	PPCREC_IML_TYPE_R_NAME,				// r* = name
	PPCREC_IML_TYPE_NAME_R,				// name* = r*
	PPCREC_IML_TYPE_R_S32,				// r* (op) imm
	PPCREC_IML_TYPE_MACRO,
	PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK,	// jumps only if remaining thread cycles < 0

	// conditions and branches
	PPCREC_IML_TYPE_COMPARE,			// r* = r* CMP[cond] r*
	PPCREC_IML_TYPE_COMPARE_S32,		// r* = r* CMP[cond] imm
	PPCREC_IML_TYPE_JUMP,				// jump always
	PPCREC_IML_TYPE_CONDITIONAL_JUMP,	// jump conditionally based on boolean value in register

	// atomic
	PPCREC_IML_TYPE_ATOMIC_CMP_STORE,

	// conditional (legacy)
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

	PPCREC_IML_TYPE_FPR_COMPARE,		// r* = r* CMP[cond] r*
};

enum
{
	PPCREC_NAME_NONE,
	PPCREC_NAME_TEMPORARY = 1000,
	PPCREC_NAME_R0 = 2000,
	PPCREC_NAME_SPR0 = 3000,
	PPCREC_NAME_FPR0 = 4000,
	PPCREC_NAME_TEMPORARY_FPR0 = 5000, // 0 to 7
	PPCREC_NAME_XER_CA = 6000, // carry bit from XER
	PPCREC_NAME_XER_OV = 6001, // overflow bit from XER
	PPCREC_NAME_XER_SO = 6002, // summary overflow bit from XER
	PPCREC_NAME_CR = 7000, // CR register bits (31 to 0)
	PPCREC_NAME_CR_LAST = PPCREC_NAME_CR+31,
	PPCREC_NAME_CPU_MEMRES_EA = 8000,
	PPCREC_NAME_CPU_MEMRES_VAL = 8001
};

#define PPC_REC_INVALID_REGISTER	0xFF	// deprecated. Use IMLREG_INVALID instead

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

struct IMLUsedRegisters
{
	union
	{
		struct
		{
			sint16 readNamedReg1;
			sint16 readNamedReg2;
			sint16 readNamedReg3;
			sint16 writtenNamedReg1;
			sint16 writtenNamedReg2;
		};
	};
	// FPR
	union
	{
		struct
		{
			// note: If destination operand is not fully written (PS0 and PS1) it will be added to the read registers
			sint16 readFPR1;
			sint16 readFPR2;
			sint16 readFPR3;
			sint16 readFPR4;
			sint16 writtenFPR1;
		};
		//sint16 fpr[4];
	};

	bool IsRegWritten(sint16 imlReg) const // GPRs
	{
		cemu_assert_debug(imlReg >= 0);
		return writtenNamedReg1 == imlReg || writtenNamedReg2 == imlReg;
	}

	template<typename Fn>
	void ForEachWrittenGPR(Fn F)
	{
		if (writtenNamedReg1 >= 0)
			F(writtenNamedReg1);
		if (writtenNamedReg2 >= 0)
			F(writtenNamedReg2);
	}

	template<typename Fn>
	void ForEachReadGPR(Fn F)
	{
		if (readNamedReg1 >= 0)
			F(readNamedReg1);
		if (readNamedReg2 >= 0)
			F(readNamedReg2);
		if (readNamedReg3 >= 0)
			F(readNamedReg3);
	}

	template<typename Fn>
	void ForEachAccessedGPR(Fn F)
	{
		if (readNamedReg1 >= 0)
			F(readNamedReg1, false);
		if (readNamedReg2 >= 0)
			F(readNamedReg2, false);
		if (readNamedReg3 >= 0)
			F(readNamedReg3, false);
		if (writtenNamedReg1 >= 0)
			F(writtenNamedReg1, true);
		if (writtenNamedReg2 >= 0)
			F(writtenNamedReg2, true);
	}

	bool HasFPRReg(sint16 imlReg) const
	{
		cemu_assert_debug(imlReg >= 0);
		if (readFPR1 == imlReg)
			return true;
		if (readFPR2 == imlReg)
			return true;
		if (readFPR3 == imlReg)
			return true;
		if (readFPR4 == imlReg)
			return true;
		if (writtenFPR1 == imlReg)
			return true;
		return false;
	}
};

using IMLReg = uint8;

inline constexpr IMLReg IMLREG_INVALID = (IMLReg)-1;

struct IMLInstruction
{
	uint8 type;
	uint8 operation;
	union
	{
		struct
		{
			uint8 _padding[7];
		}padding;
		struct
		{
			uint8 registerResult;
			uint8 registerA;
		}op_r_r;
		struct
		{
			uint8 registerResult;
			uint8 registerA;
			uint8 registerB;
		}op_r_r_r;
		struct
		{
			IMLReg regR;
			IMLReg regA;
			IMLReg regB;
			IMLReg regCarry;
		}op_r_r_r_carry;
		struct
		{
			uint8 registerResult;
			uint8 registerA;
			sint32 immS32;
		}op_r_r_s32;
		struct
		{
			IMLReg regR;
			IMLReg regA;
			sint32 immS32;
			IMLReg regCarry;
		}op_r_r_s32_carry;
		struct
		{
			uint8 registerIndex;
			uint32 name;
		}op_r_name; // alias op_name_r
		struct
		{
			uint8 registerIndex;
			sint32 immS32;
		}op_r_immS32;
		struct
		{
			uint32 param;
			uint32 param2;
			uint16 paramU16;
		}op_macro;
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
			IMLReg regR; // stores the boolean result of the comparison
			IMLReg regA;
			IMLReg regB;
			IMLCondition cond;
		}op_fpr_compare;
		struct
		{
			uint8 crD; // crBitIndex (result)
			uint8 crA; // crBitIndex
			uint8 crB; // crBitIndex
		}op_cr;
		struct
		{
			uint8 registerResult; // stores the boolean result of the comparison
			uint8 registerOperandA;
			uint8 registerOperandB;
			IMLCondition cond;
		}op_compare;
		struct
		{
			uint8 registerResult; // stores the boolean result of the comparison
			uint8 registerOperandA;
			sint32 immS32;
			IMLCondition cond;
		}op_compare_s32;
		struct
		{
			uint8 registerBool;
			bool mustBeTrue;
		}op_conditionalJump2;
		struct  
		{
			IMLReg regEA;
			IMLReg regCompareValue;
			IMLReg regWriteValue;
			IMLReg regBoolOut; // boolean 0/1
		}op_atomic_compare_store;
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
		if (type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_BL ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_B_FAR ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_B_TO_REG ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_LEAVE ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_HLE ||
			type == PPCREC_IML_TYPE_MACRO && operation == PPCREC_IML_MACRO_MFTB ||
			type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK ||
			type == PPCREC_IML_TYPE_JUMP ||
			type == PPCREC_IML_TYPE_CONDITIONAL_JUMP)
			return true;
		return false;
	}

	// instruction setters
	void make_no_op()
	{
		type = PPCREC_IML_TYPE_NO_OP;
		operation = 0;
	}

	void make_debugbreak(uint32 currentPPCAddress = 0)
	{
		make_macro(PPCREC_IML_MACRO_DEBUGBREAK, 0, currentPPCAddress, 0);
	}

	void make_macro(uint32 macroId, uint32 param, uint32 param2, uint16 paramU16)
	{
		this->type = PPCREC_IML_TYPE_MACRO;
		this->operation = macroId;
		this->op_macro.param = param;
		this->op_macro.param2 = param2;
		this->op_macro.paramU16 = paramU16;
	}

	void make_cjump_cycle_check()
	{
		this->type = PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK;
		this->operation = 0;
	}

	void make_r_r(uint32 operation, uint8 registerResult, uint8 registerA)
	{
		this->type = PPCREC_IML_TYPE_R_R;
		this->operation = operation;
		this->op_r_r.registerResult = registerResult;
		this->op_r_r.registerA = registerA;
	}


	void make_r_s32(uint32 operation, uint8 registerIndex, sint32 immS32)
	{
		this->type = PPCREC_IML_TYPE_R_S32;
		this->operation = operation;
		this->op_r_immS32.registerIndex = registerIndex;
		this->op_r_immS32.immS32 = immS32;
	}

	void make_r_r_r(uint32 operation, uint8 registerResult, uint8 registerA, uint8 registerB)
	{
		this->type = PPCREC_IML_TYPE_R_R_R;
		this->operation = operation;
		this->op_r_r_r.registerResult = registerResult;
		this->op_r_r_r.registerA = registerA;
		this->op_r_r_r.registerB = registerB;
	}

	void make_r_r_r_carry(uint32 operation, uint8 registerResult, uint8 registerA, uint8 registerB, uint8 registerCarry)
	{
		this->type = PPCREC_IML_TYPE_R_R_R_CARRY;
		this->operation = operation;
		this->op_r_r_r_carry.regR = registerResult;
		this->op_r_r_r_carry.regA = registerA;
		this->op_r_r_r_carry.regB = registerB;
		this->op_r_r_r_carry.regCarry = registerCarry;
	}

	void make_r_r_s32(uint32 operation, uint8 registerResult, uint8 registerA, sint32 immS32)
	{
		this->type = PPCREC_IML_TYPE_R_R_S32;
		this->operation = operation;
		this->op_r_r_s32.registerResult = registerResult;
		this->op_r_r_s32.registerA = registerA;
		this->op_r_r_s32.immS32 = immS32;
	}

	void make_r_r_s32_carry(uint32 operation, uint8 registerResult, uint8 registerA, sint32 immS32, uint8 registerCarry)
	{
		this->type = PPCREC_IML_TYPE_R_R_S32_CARRY;
		this->operation = operation;
		this->op_r_r_s32_carry.regR = registerResult;
		this->op_r_r_s32_carry.regA = registerA;
		this->op_r_r_s32_carry.immS32 = immS32;
		this->op_r_r_s32_carry.regCarry = registerCarry;
	}

	void make_compare(uint8 registerA, uint8 registerB, uint8 registerResult, IMLCondition cond)
	{
		this->type = PPCREC_IML_TYPE_COMPARE;
		this->operation = -999;
		this->op_compare.registerResult = registerResult;
		this->op_compare.registerOperandA = registerA;
		this->op_compare.registerOperandB = registerB;
		this->op_compare.cond = cond;
	}

	void make_compare_s32(uint8 registerA, sint32 immS32, uint8 registerResult, IMLCondition cond)
	{
		this->type = PPCREC_IML_TYPE_COMPARE_S32;
		this->operation = -999;
		this->op_compare_s32.registerResult = registerResult;
		this->op_compare_s32.registerOperandA = registerA;
		this->op_compare_s32.immS32 = immS32;
		this->op_compare_s32.cond = cond;
	}

	void make_conditional_jump(uint8 registerBool, bool mustBeTrue)
	{
		this->type = PPCREC_IML_TYPE_CONDITIONAL_JUMP;
		this->operation = -999;
		this->op_conditionalJump2.registerBool = registerBool;
		this->op_conditionalJump2.mustBeTrue = mustBeTrue;
	}

	void make_jump()
	{
		this->type = PPCREC_IML_TYPE_JUMP;
		this->operation = -999;
	}

	// load from memory
	void make_r_memory(uint8 registerDestination, uint8 registerMemory, sint32 immS32, uint32 copyWidth, bool signExtend, bool switchEndian)
	{
		this->type = PPCREC_IML_TYPE_LOAD;
		this->operation = 0;
		this->op_storeLoad.registerData = registerDestination;
		this->op_storeLoad.registerMem = registerMemory;
		this->op_storeLoad.immS32 = immS32;
		this->op_storeLoad.copyWidth = copyWidth;
		this->op_storeLoad.flags2.swapEndian = switchEndian;
		this->op_storeLoad.flags2.signExtend = signExtend;
	}

	// store to memory
	void make_memory_r(uint8 registerSource, uint8 registerMemory, sint32 immS32, uint32 copyWidth, bool switchEndian)
	{
		this->type = PPCREC_IML_TYPE_STORE;
		this->operation = 0;
		this->op_storeLoad.registerData = registerSource;
		this->op_storeLoad.registerMem = registerMemory;
		this->op_storeLoad.immS32 = immS32;
		this->op_storeLoad.copyWidth = copyWidth;
		this->op_storeLoad.flags2.swapEndian = switchEndian;
		this->op_storeLoad.flags2.signExtend = false;
	}

	void make_atomic_cmp_store(IMLReg regEA, IMLReg regCompareValue, IMLReg regWriteValue, IMLReg regSuccessOutput)
	{
		this->type = PPCREC_IML_TYPE_ATOMIC_CMP_STORE;
		this->operation = 0;
		this->op_atomic_compare_store.regEA = regEA;
		this->op_atomic_compare_store.regCompareValue = regCompareValue;
		this->op_atomic_compare_store.regWriteValue = regWriteValue;
		this->op_atomic_compare_store.regBoolOut = regSuccessOutput;
	}

	void make_fpr_compare(IMLReg regA, IMLReg regB, IMLReg regR, IMLCondition cond)
	{
		this->type = PPCREC_IML_TYPE_FPR_COMPARE;
		this->operation = -999;
		this->op_fpr_compare.regR = regR;
		this->op_fpr_compare.regA = regA;
		this->op_fpr_compare.regB = regB;
		this->op_fpr_compare.cond = cond;
	}

	void CheckRegisterUsage(IMLUsedRegisters* registersUsed) const;

	void RewriteGPR(const std::unordered_map<IMLReg, IMLReg>& translationTable);
	void ReplaceFPRs(sint32 fprRegisterSearched[4], sint32 fprRegisterReplaced[4]);
	void ReplaceFPR(sint32 fprRegisterSearched, sint32 fprRegisterReplaced);
};