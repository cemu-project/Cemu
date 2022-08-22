
#define PPCREC_CR_REG_TEMP			8 // there are only 8 cr registers (0-7) we use the 8th as temporary cr register that is never stored (BDNZ instruction for example)

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
	// special
	PPCREC_IML_TYPE_MEM2MEM,			// memory to memory copy (deprecated)

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

bool PPCRecompiler_generateIntermediateCode(ppcImlGenContext_t& ppcImlGenContext, PPCRecFunction_t* PPCRecFunction, std::set<uint32>& entryAddresses);
void PPCRecompiler_freeContext(ppcImlGenContext_t* ppcImlGenContext); // todo - move to destructor

PPCRecImlInstruction_t* PPCRecompilerImlGen_generateNewEmptyInstruction(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_pushBackIMLInstructions(PPCRecImlSegment_t* imlSegment, sint32 index, sint32 shiftBackCount);
PPCRecImlInstruction_t* PPCRecompiler_insertInstruction(PPCRecImlSegment_t* imlSegment, sint32 index);

void PPCRecompilerIml_insertSegments(ppcImlGenContext_t* ppcImlGenContext, sint32 index, sint32 count);

void PPCRecompilerIml_setSegmentPoint(ppcRecompilerSegmentPoint_t* segmentPoint, PPCRecImlSegment_t* imlSegment, sint32 index);
void PPCRecompilerIml_removeSegmentPoint(ppcRecompilerSegmentPoint_t* segmentPoint);

// GPR register management
uint32 PPCRecompilerImlGen_loadRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName, bool loadNew = false);
uint32 PPCRecompilerImlGen_loadOverwriteRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName);

// FPR register management
uint32 PPCRecompilerImlGen_loadFPRRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName, bool loadNew = false);
uint32 PPCRecompilerImlGen_loadOverwriteFPRRegister(ppcImlGenContext_t* ppcImlGenContext, uint32 mappedName);

// IML instruction generation
void PPCRecompilerImlGen_generateNewInstruction_jump(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, uint32 jumpmarkAddress);
void PPCRecompilerImlGen_generateNewInstruction_jumpSegment(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction);

void PPCRecompilerImlGen_generateNewInstruction_r_s32(ppcImlGenContext_t* ppcImlGenContext, uint32 operation, uint8 registerIndex, sint32 immS32, uint32 copyWidth, bool signExtend, bool bigEndian, uint8 crRegister, uint32 crMode);
void PPCRecompilerImlGen_generateNewInstruction_conditional_r_s32(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, uint32 operation, uint8 registerIndex, sint32 immS32, uint32 crRegisterIndex, uint32 crBitIndex, bool bitMustBeSet);
void PPCRecompilerImlGen_generateNewInstruction_r_r(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, uint32 operation, uint8 registerResult, uint8 registerA, uint8 crRegister = PPC_REC_INVALID_REGISTER, uint8 crMode = 0);



// IML instruction generation (new style, can generate new instructions but also overwrite existing ones)

void PPCRecompilerImlGen_generateNewInstruction_noOp(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction);
void PPCRecompilerImlGen_generateNewInstruction_memory_memory(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, uint8 srcMemReg, sint32 srcImmS32, uint8 dstMemReg, sint32 dstImmS32, uint8 copyWidth);

void PPCRecompilerImlGen_generateNewInstruction_fpr_r(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, sint32 operation, uint8 registerResult, sint32 crRegister = PPC_REC_INVALID_REGISTER);

// IML generation - FPU
bool PPCRecompilerImlGen_LFS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFSU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFSX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFSUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFDU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFDX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_LFDUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFSU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFSX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFSUX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFIWX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFDU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_STFDX(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMUL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FDIV(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FNMSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMULS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FDIVS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FADDS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMADDS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FNMSUBS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FCMPO(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FCMPU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FMR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FNABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FRES(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FRSP(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FNEG(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FSEL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FRSQRTE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_FCTIWZ(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PSQ_L(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PSQ_LU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PSQ_ST(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PSQ_STU(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MULS0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MULS1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MADDS0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MADDS1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_ADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_SUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MUL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_DIV(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_NMADD(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_NMSUB(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_SUM0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_SUM1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_NEG(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_ABS(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_RES(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_RSQRTE(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MR(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_SEL(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MERGE00(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MERGE01(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MERGE10(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_MERGE11(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_CMPO0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_CMPU0(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);
bool PPCRecompilerImlGen_PS_CMPU1(ppcImlGenContext_t* ppcImlGenContext, uint32 opcode);

// IML general

bool PPCRecompiler_isSuffixInstruction(PPCRecImlInstruction_t* iml);
void PPCRecompilerIML_linkSegments(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompilerIml_setLinkBranchNotTaken(PPCRecImlSegment_t* imlSegmentSrc, PPCRecImlSegment_t* imlSegmentDst);
void PPCRecompilerIml_setLinkBranchTaken(PPCRecImlSegment_t* imlSegmentSrc, PPCRecImlSegment_t* imlSegmentDst);
void PPCRecompilerIML_relinkInputSegment(PPCRecImlSegment_t* imlSegmentOrig, PPCRecImlSegment_t* imlSegmentNew);
void PPCRecompilerIML_removeLink(PPCRecImlSegment_t* imlSegmentSrc, PPCRecImlSegment_t* imlSegmentDst);
void PPCRecompilerIML_isolateEnterableSegments(ppcImlGenContext_t* ppcImlGenContext);

PPCRecImlInstruction_t* PPCRecompilerIML_getLastInstruction(PPCRecImlSegment_t* imlSegment);

// IML analyzer
typedef struct
{
	uint32 readCRBits;
	uint32 writtenCRBits;
}PPCRecCRTracking_t;

bool PPCRecompilerImlAnalyzer_isTightFiniteLoop(PPCRecImlSegment_t* imlSegment);
bool PPCRecompilerImlAnalyzer_canTypeWriteCR(PPCRecImlInstruction_t* imlInstruction);
void PPCRecompilerImlAnalyzer_getCRTracking(PPCRecImlInstruction_t* imlInstruction, PPCRecCRTracking_t* crTracking);

// IML optimizer
bool PPCRecompiler_reduceNumberOfFPRRegisters(ppcImlGenContext_t* ppcImlGenContext);

bool PPCRecompiler_manageFPRRegisters(ppcImlGenContext_t* ppcImlGenContext);

void PPCRecompiler_removeRedundantCRUpdates(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_optimizeDirectFloatCopies(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_optimizeDirectIntegerCopies(ppcImlGenContext_t* ppcImlGenContext);

void PPCRecompiler_optimizePSQLoadAndStore(ppcImlGenContext_t* ppcImlGenContext);

// IML register allocator
void PPCRecompilerImm_allocateRegisters(ppcImlGenContext_t* ppcImlGenContext);

// late optimizations
void PPCRecompiler_reorderConditionModifyInstructions(ppcImlGenContext_t* ppcImlGenContext);

// debug

void PPCRecompiler_dumpIMLSegment(PPCRecImlSegment_t* imlSegment, sint32 segmentIndex, bool printLivenessRangeInfo = false);


typedef struct
{
	union
	{
		struct
		{
			sint16 readNamedReg1;
			sint16 readNamedReg2;
			sint16 readNamedReg3;
			sint16 writtenNamedReg1;
		};
		sint16 gpr[4]; // 3 read + 1 write
	};
	// FPR
	union
	{
		struct
		{
			// note: If destination operand is not fully written, it will be added as a read FPR as well
			sint16 readFPR1;
			sint16 readFPR2;
			sint16 readFPR3;
			sint16 readFPR4; // usually this is set to the result FPR if only partially overwritten
			sint16 writtenFPR1;
		};
		sint16 fpr[4];
	};
}PPCImlOptimizerUsedRegisters_t;

void PPCRecompiler_checkRegisterUsage(ppcImlGenContext_t* ppcImlGenContext, PPCRecImlInstruction_t* imlInstruction, PPCImlOptimizerUsedRegisters_t* registersUsed);
