
#include "../PPCRecompiler.h" // todo - get rid of dependency

#include "x86Emitter.h"

struct x64RelocEntry_t
{
	x64RelocEntry_t(uint32 offset, void* extraInfo) : offset(offset), extraInfo(extraInfo) {};

	uint32 offset;
	void*  extraInfo;
};

struct x64GenContext_t
{
	IMLSegment* currentSegment{};
	x86Assembler64* emitter;
	sint32 m_currentInstructionEmitIndex;

	x64GenContext_t()
	{
		emitter = new x86Assembler64();
	}

	~x64GenContext_t()
	{
		delete emitter;
	}

	IMLInstruction* GetNextInstruction(sint32 relativeIndex = 1)
	{
		sint32 index = m_currentInstructionEmitIndex + relativeIndex;
		if(index < 0 || index >= (sint32)currentSegment->imlList.size())
			return nullptr;
		return currentSegment->imlList.data() + index;
	}

	// relocate offsets
	std::vector<x64RelocEntry_t> relocateOffsetTable2;
};

// reserved registers
#define REG_RESV_TEMP		(X86_REG_R14)
#define REG_RESV_HCPU		(X86_REG_RSP)
#define REG_RESV_MEMBASE	(X86_REG_R13)
#define REG_RESV_RECDATA	(X86_REG_R15)

// reserved floating-point registers
#define REG_RESV_FPR_TEMP	(15)

#define reg32ToReg16(__x)	(__x) // deprecated

// deprecated condition flags
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

bool PPCRecompiler_generateX64Code(struct PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext);

void PPCRecompilerX64Gen_redirectRelativeJump(x64GenContext_t* x64GenContext, sint32 jumpInstructionOffset, sint32 destinationOffset);

void PPCRecompilerX64Gen_generateRecompilerInterfaceFunctions();

void PPCRecompilerX64Gen_imlInstruction_fpr_r_name(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction);
void PPCRecompilerX64Gen_imlInstruction_fpr_name_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction);
bool PPCRecompilerX64Gen_imlInstruction_fpr_load(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction, bool indexed);
bool PPCRecompilerX64Gen_imlInstruction_fpr_store(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction, bool indexed);

void PPCRecompilerX64Gen_imlInstruction_fpr_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction);
void PPCRecompilerX64Gen_imlInstruction_fpr_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction);
void PPCRecompilerX64Gen_imlInstruction_fpr_r_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction);
void PPCRecompilerX64Gen_imlInstruction_fpr_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction);

void PPCRecompilerX64Gen_imlInstruction_fpr_compare(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, x64GenContext_t* x64GenContext, IMLInstruction* imlInstruction);

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

void x64Gen_add_reg64_reg64(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_add_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_add_reg64_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_add_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_sub_reg64Low32_reg64Low32(x64GenContext_t* x64GenContext, sint32 destRegister, sint32 srcRegister);
void x64Gen_sub_reg64Low32_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_sub_reg64_imm32(x64GenContext_t* x64GenContext, sint32 srcRegister, uint32 immU32);
void x64Gen_sub_mem32reg64_imm32(x64GenContext_t* x64GenContext, sint32 memRegister, sint32 memImmS32, uint64 immU32);
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

void x64Gen_bswap_reg64Lower32bit(x64GenContext_t* x64GenContext, sint32 destRegister);

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
void x64Gen_shrx_reg32_reg32_reg32(x64GenContext_t* x64GenContext, sint32 registerDst, sint32 registerA, sint32 registerB);
void x64Gen_sarx_reg64_reg64_reg64(x64GenContext_t* x64GenContext, sint32 registerDst, sint32 registerA, sint32 registerB);
void x64Gen_sarx_reg32_reg32_reg32(x64GenContext_t* x64GenContext, sint32 registerDst, sint32 registerA, sint32 registerB);
void x64Gen_shlx_reg64_reg64_reg64(x64GenContext_t* x64GenContext, sint32 registerDst, sint32 registerA, sint32 registerB);
void x64Gen_shlx_reg32_reg32_reg32(x64GenContext_t* x64GenContext, sint32 registerDst, sint32 registerA, sint32 registerB);