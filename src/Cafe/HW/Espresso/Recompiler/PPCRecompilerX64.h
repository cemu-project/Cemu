
typedef struct  
{
	uint32 offset;
	uint8  type;
	void*  extraInfo;
}x64RelocEntry_t;

typedef struct  
{
	uint8* codeBuffer;
	sint32 codeBufferIndex;
	sint32 codeBufferSize;
	// cr state
	sint32 activeCRRegister; // current x86 condition flags reflect this cr* register
	sint32 activeCRState; // describes the way in which x86 flags map to the cr register (signed / unsigned)
	// relocate offsets
	x64RelocEntry_t* relocateOffsetTable;
	sint32 relocateOffsetTableSize;
	sint32 relocateOffsetTableCount;
}x64GenContext_t;

// Some of these are defined by winnt.h and gnu headers
#undef REG_EAX
#undef REG_ECX
#undef REG_EDX
#undef REG_EBX
#undef REG_ESP
#undef REG_EBP
#undef REG_ESI
#undef REG_EDI
#undef REG_NONE
#undef REG_RAX
#undef REG_RCX
#undef REG_RDX
#undef REG_RBX
#undef REG_RSP
#undef REG_RBP
#undef REG_RSI
#undef REG_RDI
#undef REG_R8
#undef REG_R9
#undef REG_R10
#undef REG_R11
#undef REG_R12
#undef REG_R13
#undef REG_R14
#undef REG_R15

#define REG_EAX		0
#define REG_ECX		1
#define REG_EDX		2
#define REG_EBX		3
#define REG_ESP		4	// reserved for low half of hCPU pointer
#define REG_EBP		5
#define REG_ESI		6
#define REG_EDI		7
#define REG_NONE	-1

#define REG_RAX		0
#define REG_RCX		1
#define REG_RDX		2
#define REG_RBX		3
#define REG_RSP		4	// reserved for hCPU pointer
#define REG_RBP		5
#define REG_RSI		6
#define REG_RDI		7
#define REG_R8		8
#define REG_R9		9
#define REG_R10		10
#define REG_R11		11
#define REG_R12		12
#define REG_R13		13 // reserved to hold pointer to memory base? (Not decided yet)
#define REG_R14		14 // reserved as temporary register
#define REG_R15		15 // reserved for pointer to ppcRecompilerInstanceData

#define REG_AL		0
#define REG_CL		1
#define REG_DL		2
#define REG_BL		3
#define REG_AH		4
#define REG_CH		5
#define REG_DH		6
#define REG_BH		7

// reserved registers
#define REG_RESV_TEMP		(REG_R14)
#define REG_RESV_HCPU		(REG_RSP)
#define REG_RESV_MEMBASE	(REG_R13)
#define REG_RESV_RECDATA	(REG_R15)

// reserved floating-point registers
#define REG_RESV_FPR_TEMP	(15)


extern sint32 x64Gen_registerMap[12];

#define tempToRealRegister(__x) (x64Gen_registerMap[__x])
#define tempToRealFPRRegister(__x) (__x)
#define reg32ToReg16(__x)	(__x)

enum
{
	X86_CONDITION_EQUAL, // or zero
	X86_CONDITION_NOT_EQUAL, // or not zero
	X86_CONDITION_SIGNED_LESS, // or not greater/equal
	X86_CONDITION_SIGNED_GREATER, // or not less/equal
	X86_CONDITION_SIGNED_LESS_EQUAL, // or not greater
	X86_CONDITION_SIGNED_GREATER_EQUAL, // or not less
	X86_CONDITION_UNSIGNED_BELOW, // or not above/equal
	X86_CONDITION_UNSIGNED_ABOVE, // or not below/equal
	X86_CONDITION_UNSIGNED_BELOW_EQUAL, // or not above
	X86_CONDITION_UNSIGNED_ABOVE_EQUAL, // or not below
	X86_CONDITION_CARRY, // carry flag must be set
	X86_CONDITION_NOT_CARRY, // carry flag must not be set
	X86_CONDITION_SIGN, // sign flag must be set
	X86_CONDITION_NOT_SIGN, // sign flag must not be set
	X86_CONDITION_PARITY, // parity flag must be set
	X86_CONDITION_NONE, // no condition, jump always
};

#define PPCREC_CR_TEMPORARY							(8)		// never stored
#define PPCREC_CR_STATE_TYPE_UNSIGNED_ARITHMETIC	(0)		// for signed arithmetic operations (ADD, CMPI)
#define PPCREC_CR_STATE_TYPE_SIGNED_ARITHMETIC		(1)		// for unsigned arithmetic operations (ADD, CMPI)
#define PPCREC_CR_STATE_TYPE_LOGICAL				(2)		// for unsigned operations (CMPLI)

#define X86_RELOC_MAKE_RELATIVE				(0)		// make code imm relative to instruction
#define X64_RELOC_LINK_TO_PPC				(1)		// translate from ppc address to x86 offset 
#define X64_RELOC_LINK_TO_SEGMENT			(2)		// link to beginning of segment

#define PPC_X64_GPR_USABLE_REGISTERS		(16-4)
#define PPC_X64_FPR_USABLE_REGISTERS		(16-1) // Use XMM0 - XMM14, XMM15 is the temp register


bool PPCRecompiler_generateX64Code(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext);

void PPCRecompilerX64Gen_crConditionFlags_forget(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext);

void PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext_t* x64GenContext, sint32 jumpInstructionOffset, sint32 destinationOffset);

void PPCRecompilerX64Gen_generateRecompilerInterfaceFunctions();

void PPCRecompilerX64Gen_imlInstruction_fpr_r_name(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction);
void PPCRecompilerX64Gen_imlInstruction_fpr_name_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction);
bool PPCRecompilerX64Gen_imlInstruction_fpr_load(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction, bool indexed);
bool PPCRecompilerX64Gen_imlInstruction_fpr_store(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction, bool indexed);

void PPCRecompilerX64Gen_imlInstruction_fpr_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction);
void PPCRecompilerX64Gen_imlInstruction_fpr_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction);
void PPCRecompilerX64Gen_imlInstruction_fpr_r_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction);
void PPCRecompilerX64Gen_imlInstruction_fpr_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, PPCRecImlInstruction_t* imlInstruction);

// ASM gen
void x64Gen_writeU8(x64GenContext_t* x64GenContext, uint8 v);
void x64Gen_writeU16(x64GenContext_t* x64GenContext, uint32 v);
void x64Gen_writeU32(x64GenContext_t* x64GenContext, uint32 v);

void x64Emit_mov_reg32_mem32(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memOffset);
void x64Emit_mov_mem32_reg32(x64GenContext_t* x64GenContext, sint32 memBaseReg64, sint32 memOffset, sint32 srcReg);
void x64Emit_mov_mem64_reg64(x64GenContext_t* x64GenContext, sint32 memBaseReg64, sint32 memOffset, sint32 srcReg);
void x64Emit_mov_reg64_mem64(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memOffset);
void x64Emit_mov_reg64_mem32(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memOffset);
void x64Emit_mov_mem32_reg64(x64GenContext_t* x64GenContext, sint32 memBaseReg64, sint32 memOffset, sint32 srcReg);
void x64Emit_mov_reg64_mem64(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memIndexReg64, sint32 memOffset);
void x64Emit_mov_reg32_mem32(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memIndexReg64, sint32 memOffset);
void x64Emit_mov_reg64b_mem8(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memIndexReg64, sint32 memOffset);
void x64Emit_movZX_reg32_mem8(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memIndexReg64, sint32 memOffset);
void x64Emit_movZX_reg64_mem8(x64GenContext_t* x64GenContext, sint32 destReg, sint32 memBaseReg64, sint32 memOffset);

void x64Gen_movSignExtend_reg64Low32_mem8Reg64PlusReg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32);

void x64Gen_movZeroExtend_reg64Low16_mem16Reg64PlusReg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32);
void x64Gen_mov_mem64Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32);
void x64Gen_movTruncate_mem32Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32, sint32 srcRegister);
void x64Gen_movTruncate_mem16Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32, sint32 srcRegister);
void x64Gen_movTruncate_mem8Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32, sint32 srcRegister);
void x64Gen_mov_mem32Reg64_imm32(x64GenContext_t* x64GenContext, sint32 memRegister, uint32 memImmU32, uint32 dataImmU32);
void x64Gen_mov_mem64Reg64_imm32(x64GenContext_t* x64GenContext, sint32 memRegister, uint32 memImmU32, uint32 dataImmU32);
void x64Gen_mov_mem8Reg64_imm8(x64GenContext_t* x64GenContext, sint32 memRegister, uint32 memImmU32, uint8 dataImmU8);

void x64Gen_mov_reg64_imm64(x64GenContext_t* x64GenContext, sint32 destRegister, uint64 immU64);
void x64Gen_mov_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 destRegister, uint64 immU32);
void x64Gen_mov_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);

void x64Gen_lea_reg64Low32_reg64Low32PlusReg64Low32(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegisterA64, sint32 memRegisterB64);

void x64Gen_cmovcc_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, uint32 conditionType, sint32 destRegister, sint32 srcRegister);
void x64Gen_mov_reg64_reg64(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_xchg_reg64_reg64(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_movSignExtend_reg64Low32_reg64Low16(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_movZeroExtend_reg64Low32_reg64Low16(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_movSignExtend_reg64Low32_reg64Low8(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_movZeroExtend_reg64Low32_reg64Low8(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);

void x64Gen_or_reg64Low8_mem8Reg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegister64, sint32 memImmS32);
void x64Gen_and_reg64Low8_mem8Reg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegister64, sint32 memImmS32);
void x64Gen_mov_mem8Reg64_reg64Low8(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegister64, sint32 memImmS32);

void x64Gen_lock_cmpxchg_mem32Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32, sint32 srcRegister);
void x64Gen_lock_cmpxchg_mem32Reg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegister64, sint32 memImmS32, sint32 srcRegister);

void x64Gen_add_reg64_reg64(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_add_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_add_reg64_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_add_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_sub_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_sub_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_sub_reg64_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_sub_mem32reg64_imm32(x64GenContext_t* x64GenContext, sint32 memRegister, sint32 memImmS32, uint64 immU32);
void x64Gen_sbb_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_adc_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_adc_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_dec_mem32(x64GenContext_t* x64GenContext, sint32 memoryRegister, uint32 memoryImmU32);
void x64Gen_imul_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 operandRegister);
void x64Gen_idiv_reg64Low32(x64GenContext_t* x64GenContext, sint32 operandRegister);
void x64Gen_div_reg64Low32(x64GenContext_t* x64GenContext, sint32 operandRegister);
void x64Gen_imul_reg64Low32(x64GenContext_t* x64GenContext, sint32 operandRegister);
void x64Gen_mul_reg64Low32(x64GenContext_t* x64GenContext, sint32 operandRegister);
void x64Gen_and_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_and_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_test_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_test_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_cmp_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, sint32 immS32);
void x64Gen_cmp_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_cmp_reg64Low32_mem32reg64(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 memRegister, sint32 memImmS32);
void x64Gen_or_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_or_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_xor_reg32_reg32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_xor_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_xor_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);

void x64Gen_rol_reg64Low32_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8);
void x64Gen_rol_reg64Low32_cl(x64GenContext_t* x64GenContext, sint32 srcRegister);
void x64Gen_rol_reg64Low16_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8);
void x64Gen_rol_reg64_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8);
void x64Gen_shl_reg64Low32_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8);
void x64Gen_shr_reg64Low32_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8);
void x64Gen_sar_reg64Low32_imm8(x64GenContext_t* x64GenContext, sint32 srcRegister, sint8 immS8);

void x64Gen_not_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister);
void x64Gen_neg_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister);
void x64Gen_cdq(x64GenContext_t* x64GenContext);

void x64Gen_bswap_reg64(x64GenContext_t* x64GenContext, sint32 destRegister);
void x64Gen_bswap_reg64Lower32bit(x64GenContext_t* x64GenContext, sint32 destRegister);
void x64Gen_bswap_reg64Lower16bit(x64GenContext_t* x64GenContext, sint32 destRegister);

void x64Gen_lzcnt_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_bsr_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_cmp_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, sint32 immS32);
void x64Gen_setcc_mem8(x64GenContext_t* x64GenContext, sint32 conditionType, sint32 memoryRegister, uint32 memoryImmU32);
void x64Gen_setcc_reg64b(x64GenContext_t* x64GenContext, sint32 conditionType, sint32 dataRegister);
void x64Gen_bt_mem8(x64GenContext_t* x64GenContext, sint32 memoryRegister, uint32 memoryImmU32, uint8 bitIndex);
void x64Gen_cmc(x64GenContext_t* x64GenContext);

void x64Gen_jmp_imm32(x64GenContext_t* x64GenContext, uint32 destImm32);
void x64Gen_jmp_memReg64(x64GenContext_t* x64GenContext, sint32 memRegister, uint32 immU32);
void x64Gen_jmpc_far(x64GenContext_t* x64GenContext, sint32 conditionType, sint32 relativeDest);
void x64Gen_jmpc_near(x64GenContext_t* x64GenContext, sint32 conditionType, sint32 relativeDest);

void x64Gen_push_reg64(x64GenContext_t* x64GenContext, sint32 srcRegister);
void x64Gen_pop_reg64(x64GenContext_t* x64GenContext, sint32 destRegister);
void x64Gen_jmp_reg64(x64GenContext_t* x64GenContext, sint32 srcRegister);
void x64Gen_call_reg64(x64GenContext_t* x64GenContext, sint32 srcRegister);
void x64Gen_ret(x64GenContext_t* x64GenContext);
void x64Gen_int3(x64GenContext_t* x64GenContext);

// floating-point (SIMD/SSE) gen
void x64Gen_movaps_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSource);
void x64Gen_movupd_xmmReg_memReg128(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32);
void x64Gen_movupd_memReg128_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32);
void x64Gen_movddup_xmmReg_memReg64(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32);
void x64Gen_movddup_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_movhlps_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_movsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_movsd_memReg64_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32);
void x64Gen_movlpd_xmmReg_memReg64(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32);
void x64Gen_unpcklpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_unpckhpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_shufpd_xmmReg_xmmReg_imm8(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc, uint8 imm8);
void x64Gen_addsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_addpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_subsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_subpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_mulsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_mulpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_mulpd_xmmReg_memReg128(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32);
void x64Gen_divsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_divpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_comisd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_comisd_xmmReg_mem64Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 memoryReg, sint32 memImmS32);
void x64Gen_ucomisd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_comiss_xmmReg_mem64Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 memoryReg, sint32 memImmS32);
void x64Gen_orps_xmmReg_mem128Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, uint32 memReg, uint32 memImmS32);
void x64Gen_xorps_xmmReg_mem128Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, uint32 memReg, uint32 memImmS32);
void x64Gen_andps_xmmReg_mem128Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, uint32 memReg, uint32 memImmS32);
void x64Gen_andpd_xmmReg_memReg128(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32);
void x64Gen_andps_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_pcmpeqd_xmmReg_mem128Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, uint32 memReg, uint32 memImmS32);
void x64Gen_cvttpd2dq_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_cvttsd2si_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 registerDest, sint32 xmmRegisterSrc);
void x64Gen_cvtsd2ss_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_cvtpd2ps_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_cvtss2sd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_cvtps2pd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_cvtpi2pd_xmmReg_mem64Reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 memReg, sint32 memImmS32);
void x64Gen_cvtsd2si_reg64Low_xmmReg(x64GenContext_t* x64GenContext, sint32 registerDest, sint32 xmmRegisterSrc);
void x64Gen_cvttsd2si_reg64Low_xmmReg(x64GenContext_t* x64GenContext, sint32 registerDest, sint32 xmmRegisterSrc);
void x64Gen_sqrtsd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_sqrtpd_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_rcpss_xmmReg_xmmReg(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 xmmRegisterSrc);
void x64Gen_mulss_xmmReg_memReg64(x64GenContext_t* x64GenContext, sint32 xmmRegister, sint32 memRegister, uint32 memImmU32);

void x64Gen_movd_xmmReg_reg64Low32(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 registerSrc);
void x64Gen_movd_reg64Low32_xmmReg(x64GenContext_t* x64GenContext, sint32 registerDest, sint32 xmmRegisterSrc);
void x64Gen_movq_xmmReg_reg64(x64GenContext_t* x64GenContext, sint32 xmmRegisterDest, sint32 registerSrc);
void x64Gen_movq_reg64_xmmReg(x64GenContext_t* x64GenContext, sint32 registerDst, sint32 xmmRegisterSrc);

// AVX

void x64Gen_avx_VPUNPCKHQDQ_xmm_xmm_xmm(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 srcRegisterA, sint32 srcRegisterB);
void x64Gen_avx_VUNPCKHPD_xmm_xmm_xmm(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 srcRegisterA, sint32 srcRegisterB);
void x64Gen_avx_VSUBPD_xmm_xmm_xmm(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 srcRegisterA, sint32 srcRegisterB);

// BMI
void x64Gen_movBEZeroExtend_reg64_mem32Reg64PlusReg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32);
void x64Gen_movBEZeroExtend_reg64Low16_mem16Reg64PlusReg64(x64GenContext_t* x64GenContext, sint32 dstRegister, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32);

void x64Gen_movBETruncate_mem32Reg64PlusReg64_reg64(x64GenContext_t* x64GenContext, sint32 memRegisterA64, sint32 memRegisterB64, sint32 memImmS32, sint32 srcRegister);

void x64Gen_shrx_reg64_reg64_reg64(x64GenContext_t* x64GenContext, sint32 registerDst, sint32 registerA, sint32 registerB);
void x64Gen_shlx_reg64_reg64_reg64(x64GenContext_t* x64GenContext, sint32 registerDst, sint32 registerA, sint32 registerB);