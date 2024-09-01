#include "BackendAArch64.h"

#pragma push_macro("CSIZE")
#undef CSIZE
#include <xbyak_aarch64.h>
#pragma pop_macro("CSIZE")

#include <cstddef>

#include "../PPCRecompiler.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Common/precompiled.h"
#include "HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "HW/Espresso/PPCState.h"

#include <HW/Espresso/Interpreter/PPCInterpreterHelper.h>
#include <asm/x64util.h>

using namespace Xbyak_aarch64;

constexpr uint32_t TEMP_REGISTER_ID = 25;
constexpr uint32_t TEMP_REGISTER_2_ID = 26;
constexpr uint32_t PPC_RECOMPILER_INSTANCE_DATA_REG_ID = 27;
constexpr uint32_t MEMORY_BASE_REG_ID = 28;
constexpr uint32_t HCPU_REG_ID = 29;
// FPR
constexpr uint32_t TEMP_VECTOR_REGISTER_ID = 29;
constexpr uint32_t TEMP_VECTOR_REGISTER_2_ID = 30;
constexpr uint32_t ASM_ROUTINE_REGISTER_ID = 31;

constexpr uint64_t DOUBLE_1_0 = std::bit_cast<uint64_t>(1.0);
static const XReg hCPU{HCPU_REG_ID}, ppcRec{PPC_RECOMPILER_INSTANCE_DATA_REG_ID}, memBase{MEMORY_BASE_REG_ID};
static const XReg tempXReg{TEMP_REGISTER_ID}, temp2XReg{TEMP_REGISTER_2_ID};
static const WReg tempWReg{TEMP_REGISTER_ID}, temp2WReg{TEMP_REGISTER_2_ID};
static const VReg tempVReg{TEMP_VECTOR_REGISTER_ID}, temp2VReg{TEMP_VECTOR_REGISTER_2_ID}, asmRoutineVReg{ASM_ROUTINE_REGISTER_ID};
static const DReg tempDReg{TEMP_VECTOR_REGISTER_ID};
static const SReg tempSReg{TEMP_VECTOR_REGISTER_ID};
static const QReg tempQReg{TEMP_VECTOR_REGISTER_ID};
static const WReg lrWReg{TEMP_REGISTER_2_ID};
static const XReg lrXReg{TEMP_REGISTER_2_ID};

struct AArch64GenContext_t : Xbyak_aarch64::CodeGenerator, CodeContext
{
	AArch64GenContext_t();

	void enterRecompilerCode();
	void leaveRecompilerCode();

	void r_name(IMLInstruction* imlInstruction);
	void name_r(IMLInstruction* imlInstruction);
	bool r_s32(IMLInstruction* imlInstruction);
	bool r_r(IMLInstruction* imlInstruction);
	bool r_r_s32(IMLInstruction* imlInstruction);
	bool r_r_s32_carry(IMLInstruction* imlInstruction);
	bool r_r_r(IMLInstruction* imlInstruction);
	bool r_r_r_carry(IMLInstruction* imlInstruction);
	void compare(IMLInstruction* imlInstruction);
	void compare_s32(IMLInstruction* imlInstruction);
	bool load(IMLInstruction* imlInstruction, bool indexed);
	bool store(IMLInstruction* imlInstruction, bool indexed);
	void atomic_cmp_store(IMLInstruction* imlInstruction);
	bool macro(IMLInstruction* imlInstruction);
	bool fpr_load(IMLInstruction* imlInstruction, bool indexed);
	void psq_load(uint8 mode, Xbyak_aarch64::VReg& dataReg, Xbyak_aarch64::WReg& memReg, Xbyak_aarch64::WReg& indexReg, sint32 memImmS32, bool indexed, IMLReg registerGQR = IMLREG_INVALID);
	void psq_load_generic(uint8 mode, Xbyak_aarch64::VReg& dataReg, Xbyak_aarch64::WReg& memReg, Xbyak_aarch64::WReg& indexReg, sint32 memImmS32, bool indexed, IMLReg registerGQR);
	bool fpr_store(IMLInstruction* imlInstruction, bool indexed);
	void psq_store(uint8 mode, IMLRegID dataRegId, Xbyak_aarch64::WReg& memReg, Xbyak_aarch64::WReg& indexReg, sint32 memOffset, bool indexed, IMLReg registerGQR = IMLREG_INVALID);
	void psq_store_generic(uint8 mode, IMLRegID dataRegId, Xbyak_aarch64::WReg& memReg, Xbyak_aarch64::WReg& indexReg, sint32 memOffset, bool indexed, IMLReg registerGQR);
	void fpr_r_r(IMLInstruction* imlInstruction);
	void fpr_r_r_r(IMLInstruction* imlInstruction);
	void fpr_r_r_r_r(IMLInstruction* imlInstruction);
	void fpr_r(IMLInstruction* imlInstruction);
	void fpr_compare(IMLInstruction* imlInstruction);
	void cjump(const std::unordered_map<IMLSegment*, Xbyak_aarch64::Label>& labels, IMLInstruction* imlInstruction, IMLSegment* imlSegment);
	void jump(const std::unordered_map<IMLSegment*, Xbyak_aarch64::Label>& labels, IMLSegment* imlSegment);
	void conditionalJumpCycleCheck(const std::unordered_map<IMLSegment*, Xbyak_aarch64::Label>& labels, IMLSegment* imlSegment);

	void gqr_generateScaleCode(Xbyak_aarch64::VReg& dataReg, bool isLoad, bool scalePS1, IMLReg registerGQR);

	bool conditional_r_s32([[maybe_unused]] IMLInstruction* imlInstruction)
	{
		cemu_assert_unimplemented();
		return false;
	}
};

AArch64GenContext_t::AArch64GenContext_t()
	: CodeGenerator(DEFAULT_MAX_CODE_SIZE, AutoGrow)
{
}

void AArch64GenContext_t::r_name(IMLInstruction* imlInstruction)
{
	uint32 name = imlInstruction->op_r_name.name;
	auto regId = imlInstruction->op_r_name.regR.GetRegID();

	if (imlInstruction->op_r_name.regR.GetBaseFormat() == IMLRegFormat::I64)
	{
		WReg regR = WReg(regId);
		if (name >= PPCREC_NAME_R0 && name < PPCREC_NAME_R0 + 32)
		{
			ldr(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, gpr) + sizeof(uint32) * (name - PPCREC_NAME_R0)));
		}
		else if (name >= PPCREC_NAME_SPR0 && name < PPCREC_NAME_SPR0 + 999)
		{
			sint32 sprIndex = (name - PPCREC_NAME_SPR0);
			if (sprIndex == SPR_LR)
				ldr(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, spr.LR)));
			else if (sprIndex == SPR_CTR)
				ldr(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, spr.CTR)));
			else if (sprIndex == SPR_XER)
				ldr(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, spr.XER)));
			else if (sprIndex >= SPR_UGQR0 && sprIndex <= SPR_UGQR7)
				ldr(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, spr.UGQR) + sizeof(PPCInterpreter_t::spr.UGQR[0]) * (sprIndex - SPR_UGQR0)));
			else
				cemu_assert_suspicious();
		}
		else if (name >= PPCREC_NAME_TEMPORARY && name < PPCREC_NAME_TEMPORARY + 4)
		{
			ldr(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, temporaryGPR_reg) + sizeof(uint32) * (name - PPCREC_NAME_TEMPORARY)));
		}
		else if (name == PPCREC_NAME_XER_CA)
		{
			ldrb(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, xer_ca)));
		}
		else if (name == PPCREC_NAME_XER_SO)
		{
			ldrb(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, xer_so)));
		}
		else if (name >= PPCREC_NAME_CR && name <= PPCREC_NAME_CR_LAST)
		{
			ldrb(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, cr) + (name - PPCREC_NAME_CR)));
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_EA)
		{
			ldr(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, reservedMemAddr)));
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_VAL)
		{
			ldr(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, reservedMemValue)));
		}
		else
		{
			cemu_assert_suspicious();
		}
	}
	else if (imlInstruction->op_r_name.regR.GetBaseFormat() == IMLRegFormat::F64)
	{
		VReg regR = VReg(imlInstruction->op_r_name.regR.GetRegID());
		if (name >= PPCREC_NAME_FPR0 && name < (PPCREC_NAME_FPR0 + 32))
		{
			mov(tempXReg, offsetof(PPCInterpreter_t, fpr) + sizeof(FPR_t) * (name - PPCREC_NAME_FPR0));
			add(tempXReg, tempXReg, hCPU);
			ld1(regR.d2, AdrNoOfs(tempXReg));
		}
		else if (name >= PPCREC_NAME_TEMPORARY_FPR0 && name < (PPCREC_NAME_TEMPORARY_FPR0 + 8))
		{
			mov(tempXReg, offsetof(PPCInterpreter_t, temporaryFPR) + sizeof(FPR_t) * (name - PPCREC_NAME_TEMPORARY_FPR0));
			add(tempXReg, tempXReg, hCPU);
			ld1(regR.d2, AdrNoOfs(tempXReg));
		}
		else
		{
			cemu_assert_debug(false);
		}
	}
	else
	{
		cemu_assert_suspicious();
	}
}

void AArch64GenContext_t::name_r(IMLInstruction* imlInstruction)
{
	uint32 name = imlInstruction->op_r_name.name;
	IMLRegID regId = imlInstruction->op_r_name.regR.GetRegID();

	if (imlInstruction->op_r_name.regR.GetBaseFormat() == IMLRegFormat::I64)
	{
		auto regR = WReg(regId);
		if (name >= PPCREC_NAME_R0 && name < PPCREC_NAME_R0 + 32)
		{
			str(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, gpr) + sizeof(uint32) * (name - PPCREC_NAME_R0)));
		}
		else if (name >= PPCREC_NAME_SPR0 && name < PPCREC_NAME_SPR0 + 999)
		{
			uint32 sprIndex = (name - PPCREC_NAME_SPR0);
			if (sprIndex == SPR_LR)
				str(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, spr.LR)));
			else if (sprIndex == SPR_CTR)
				str(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, spr.CTR)));
			else if (sprIndex == SPR_XER)
				str(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, spr.XER)));
			else if (sprIndex >= SPR_UGQR0 && sprIndex <= SPR_UGQR7)
				str(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, spr.UGQR) + sizeof(PPCInterpreter_t::spr.UGQR[0]) * (sprIndex - SPR_UGQR0)));
			else
				cemu_assert_suspicious();
		}
		else if (name >= PPCREC_NAME_TEMPORARY && name < PPCREC_NAME_TEMPORARY + 4)
		{
			str(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, temporaryGPR_reg) + sizeof(uint32) * (name - PPCREC_NAME_TEMPORARY)));
		}
		else if (name == PPCREC_NAME_XER_CA)
		{
			strb(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, xer_ca)));
		}
		else if (name == PPCREC_NAME_XER_SO)
		{
			strb(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, xer_so)));
		}
		else if (name >= PPCREC_NAME_CR && name <= PPCREC_NAME_CR_LAST)
		{
			strb(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, cr) + (name - PPCREC_NAME_CR)));
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_EA)
		{
			str(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, reservedMemAddr)));
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_VAL)
		{
			str(regR, AdrImm(hCPU, offsetof(PPCInterpreter_t, reservedMemValue)));
		}
		else
		{
			cemu_assert_suspicious();
		}
	}
	else if (imlInstruction->op_r_name.regR.GetBaseFormat() == IMLRegFormat::F64)
	{
		auto regR = VReg(imlInstruction->op_r_name.regR.GetRegID());
		auto tempReg = XReg(TEMP_REGISTER_ID);
		if (name >= PPCREC_NAME_FPR0 && name < (PPCREC_NAME_FPR0 + 32))
		{
			mov(tempReg, offsetof(PPCInterpreter_t, fpr) + sizeof(FPR_t) * (name - PPCREC_NAME_FPR0));
			add(tempReg, tempReg, hCPU);
			st1(regR.d2, AdrNoOfs(tempReg));
		}
		else if (name >= PPCREC_NAME_TEMPORARY_FPR0 && name < (PPCREC_NAME_TEMPORARY_FPR0 + 8))
		{
			mov(tempReg, offsetof(PPCInterpreter_t, temporaryFPR) + sizeof(FPR_t) * (name - PPCREC_NAME_TEMPORARY_FPR0));
			add(tempReg, tempReg, hCPU);
			st1(regR.d2, AdrNoOfs(tempReg));
		}
		else
		{
			cemu_assert_debug(false);
		}
	}
	else
	{
		cemu_assert_suspicious();
	}
}

bool AArch64GenContext_t::r_r(IMLInstruction* imlInstruction)
{
	IMLRegID regRId = imlInstruction->op_r_r.regR.GetRegID();
	IMLRegID regAId = imlInstruction->op_r_r.regA.GetRegID();
	WReg regR = WReg(regRId);
	WReg regA = WReg(regAId);

	if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	{
		if (regRId != regAId)
			mov(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ENDIAN_SWAP)
	{
		rev(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN_S8_TO_S32)
	{
		sxtb(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN_S16_TO_S32)
	{
		sxth(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_NOT)
	{
		mvn(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_NEG)
	{
		neg(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_CNTLZW)
	{
		clz(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_DCBZ)
	{
		movi(tempVReg.d2, 0);
		if (regRId != regAId)
		{
			add(tempWReg, regA, regR);
			and_(tempWReg, tempWReg, ~0x1f);
		}
		else
		{
			and_(tempWReg, regA, ~0x1f);
		}
		add(tempXReg, memBase, tempXReg);
		stp(tempQReg, tempQReg, AdrNoOfs(tempXReg));
		return true;
	}
	else
	{
		debug_printf("PPCRecompilerAArch64Gen_imlInstruction_r_r(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool AArch64GenContext_t::r_s32(IMLInstruction* imlInstruction)
{
	sint32 imm32 = imlInstruction->op_r_immS32.immS32;
	WReg reg = WReg(imlInstruction->op_r_immS32.regR.GetRegID());

	if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	{
		mov(reg, imm32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_LEFT_ROTATE)
	{
		ror(reg, reg, 32 - (imm32 & 0x1f));
	}
	else
	{
		debug_printf("PPCRecompilerAArch64Gen_imlInstruction_r_s32(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool AArch64GenContext_t::r_r_s32(IMLInstruction* imlInstruction)
{
	WReg regR = WReg(imlInstruction->op_r_r_s32.regR.GetRegID());
	WReg regA = WReg(imlInstruction->op_r_r_s32.regA.GetRegID());
	sint32 immS32 = imlInstruction->op_r_r_s32.immS32;

	if (imlInstruction->operation == PPCREC_IML_OP_ADD)
	{
		add_imm(regR, regA, immS32, tempWReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_SUB)
	{
		sub_imm(regR, regA, immS32, tempWReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_AND)
	{
		mov(tempWReg, immS32);
		and_(regR, regA, tempWReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_OR)
	{
		mov(tempWReg, immS32);
		orr(regR, regA, tempWReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_XOR)
	{
		mov(tempWReg, immS32);
		eor(regR, regA, tempWReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_RLWIMI)
	{
		uint32 vImm = (uint32)immS32;
		uint32 mb = (vImm >> 0) & 0xFF;
		uint32 me = (vImm >> 8) & 0xFF;
		uint32 sh = (vImm >> 16) & 0xFF;
		uint32 mask = ppc_mask(mb, me);
		if (sh)
		{
			ror(tempWReg, regA, 32 - (sh & 0x1F));
			and_(tempWReg, tempWReg, mask);
		}
		else
		{
			and_(tempWReg, regA, mask);
		}
		and_(regR, regR, ~mask);
		orr(regR, regR, tempWReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_SIGNED)
	{
		mov(tempWReg, immS32);
		mul(regR, regA, tempWReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_LEFT_SHIFT)
	{
		lsl(regR, regA, (uint32)immS32 & 0x1f);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_U)
	{
		lsr(regR, regA, (uint32)immS32 & 0x1f);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_S)
	{
		asr(regR, regA, (uint32)immS32 & 0x1f);
	}
	else
	{
		debug_printf("PPCRecompilerAArch64Gen_imlInstruction_r_r_s32(): Unsupported operation 0x%x\n", imlInstruction->operation);
		cemu_assert_suspicious();
		return false;
	}
	return true;
}

bool AArch64GenContext_t::r_r_s32_carry(IMLInstruction* imlInstruction)
{
	WReg regR = WReg(imlInstruction->op_r_r_s32_carry.regR.GetRegID());
	WReg regA = WReg(imlInstruction->op_r_r_s32_carry.regA.GetRegID());
	XReg regCarry = XReg(imlInstruction->op_r_r_s32_carry.regCarry.GetRegID());

	sint32 immS32 = imlInstruction->op_r_r_s32_carry.immS32;
	if (imlInstruction->operation == PPCREC_IML_OP_ADD)
	{
		adds_imm(regR, regA, immS32, tempWReg);
		cset(regCarry, Cond::CS);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ADD_WITH_CARRY)
	{
		mrs(tempXReg, 0b11, 0b011, 0b0100, 0b0010, 0b000); // NZCV
		bfi(tempXReg, regCarry, 29, 1);
		msr(0b11, 0b011, 0b0100, 0b0010, 0b000, tempXReg);
		mov(tempWReg, immS32);
		adcs(regR, regA, tempWReg);
		cset(regCarry, Cond::CS);
	}
	else
	{
		cemu_assert_suspicious();
		return false;
	}

	return true;
}

bool AArch64GenContext_t::r_r_r(IMLInstruction* imlInstruction)
{
	WReg regResult = WReg(imlInstruction->op_r_r_r.regR.GetRegID());
	XReg reg64Result = XReg(imlInstruction->op_r_r_r.regR.GetRegID());
	WReg regOperand1 = WReg(imlInstruction->op_r_r_r.regA.GetRegID());
	WReg regOperand2 = WReg(imlInstruction->op_r_r_r.regB.GetRegID());

	if (imlInstruction->operation == PPCREC_IML_OP_ADD)
	{
		add(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_SUB)
	{
		sub(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_OR)
	{
		orr(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_AND)
	{
		and_(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_XOR)
	{
		eor(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_SIGNED)
	{
		mul(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_SLW)
	{
		tst(regOperand2, 32);
		lsl(regResult, regOperand1, regOperand2);
		csel(regResult, regResult, wzr, Cond::EQ);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_SRW)
	{
		tst(regOperand2, 32);
		lsr(regResult, regOperand1, regOperand2);
		csel(regResult, regResult, wzr, Cond::EQ);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_LEFT_ROTATE)
	{
		neg(tempWReg, regOperand2);
		ror(regResult, regOperand1, tempWReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_S)
	{
		asr(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_U)
	{
		lsr(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_LEFT_SHIFT)
	{
		lsl(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_DIVIDE_SIGNED)
	{
		sdiv(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_DIVIDE_UNSIGNED)
	{
		udiv(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED)
	{
		smull(reg64Result, regOperand1, regOperand2);
		lsr(reg64Result, reg64Result, 32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_HIGH_UNSIGNED)
	{
		umull(reg64Result, regOperand1, regOperand2);
		lsr(reg64Result, reg64Result, 32);
	}
	else
	{
		debug_printf("PPCRecompilerAArch64Gen_imlInstruction_r_r_r(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool AArch64GenContext_t::r_r_r_carry(IMLInstruction* imlInstruction)
{
	WReg regR = WReg(imlInstruction->op_r_r_r_carry.regR.GetRegID());
	WReg regA = WReg(imlInstruction->op_r_r_r_carry.regA.GetRegID());
	WReg regB = WReg(imlInstruction->op_r_r_r_carry.regB.GetRegID());
	XReg regCarry = XReg(imlInstruction->op_r_r_r_carry.regCarry.GetRegID());

	if (imlInstruction->operation == PPCREC_IML_OP_ADD)
	{
		adds(regR, regA, regB);
		cset(regCarry, Cond::CS);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ADD_WITH_CARRY)
	{
		mrs(tempXReg, 0b11, 0b011, 0b0100, 0b0010, 0b000); // NZCV
		bfi(tempXReg, regCarry, 29, 1);
		msr(0b11, 0b011, 0b0100, 0b0010, 0b000, tempXReg);
		adcs(regR, regA, regB);
		cset(regCarry, Cond::CS);
	}
	else
	{
		cemu_assert_suspicious();
		return false;
	}

	return true;
}

Cond ImlCondToArm64Cond(IMLCondition condition)
{
	switch (condition)
	{
	case IMLCondition::EQ:
		return Cond::EQ;
	case IMLCondition::NEQ:
		return Cond::NE;
	case IMLCondition::UNSIGNED_GT:
		return Cond::HI;
	case IMLCondition::UNSIGNED_LT:
		return Cond::LO;
	case IMLCondition::SIGNED_GT:
		return Cond::GT;
	case IMLCondition::SIGNED_LT:
		return Cond::LT;
	default:
	{
		cemu_assert_suspicious();
		return Cond::EQ;
	}
	}
}

void AArch64GenContext_t::compare(IMLInstruction* imlInstruction)
{
	WReg regR = WReg(imlInstruction->op_compare.regR.GetRegID());
	WReg regA = WReg(imlInstruction->op_compare.regA.GetRegID());
	WReg regB = WReg(imlInstruction->op_compare.regB.GetRegID());
	Cond cond = ImlCondToArm64Cond(imlInstruction->op_compare.cond);
	cmp(regA, regB);
	cset(regR, cond);
}

void AArch64GenContext_t::compare_s32(IMLInstruction* imlInstruction)
{
	WReg regR = WReg(imlInstruction->op_compare.regR.GetRegID());
	WReg regA = WReg(imlInstruction->op_compare.regA.GetRegID());
	sint32 imm = imlInstruction->op_compare_s32.immS32;
	auto cond = ImlCondToArm64Cond(imlInstruction->op_compare.cond);
	cmp_imm(regA, imm, tempWReg);
	cset(regR, cond);
}

void AArch64GenContext_t::cjump(const std::unordered_map<IMLSegment*, Label>& labels, IMLInstruction* imlInstruction, IMLSegment* imlSegment)
{
	const Label& label = labels.at(imlSegment->nextSegmentBranchTaken);
	auto regBool = WReg(imlInstruction->op_conditional_jump.registerBool.GetRegID());
	Label skipJump;
	if (imlInstruction->op_conditional_jump.mustBeTrue)
		cbz(regBool, skipJump);
	else
		cbnz(regBool, skipJump);
	b(label);
	L(skipJump);
}

void AArch64GenContext_t::jump(const std::unordered_map<IMLSegment*, Label>& labels, IMLSegment* imlSegment)
{
	const Label& label = labels.at(imlSegment->nextSegmentBranchTaken);
	b(label);
}

void AArch64GenContext_t::conditionalJumpCycleCheck(const std::unordered_map<IMLSegment*, Label>& labels, IMLSegment* imlSegment)
{
	Label positiveRegCycles;
	const Label& label = labels.at(imlSegment->nextSegmentBranchTaken);
	Label skipJump;
	ldr(tempWReg, AdrImm(hCPU, offsetof(PPCInterpreter_t, remainingCycles)));
	tbz(tempWReg, 31, skipJump);
	b(label);
	L(skipJump);
}

void ATTR_MS_ABI PPCRecompiler_getTBL(PPCInterpreter_t* ppcInterpreter, uint32 gprIndex)
{
	uint64 coreTime = coreinit::coreinit_getTimerTick();
	ppcInterpreter->gpr[gprIndex] = (uint32)(coreTime & 0xFFFFFFFF);
}

void ATTR_MS_ABI PPCRecompiler_getTBU(PPCInterpreter_t* ppcInterpreter, uint32 gprIndex)
{
	uint64 coreTime = coreinit::coreinit_getTimerTick();
	ppcInterpreter->gpr[gprIndex] = (uint32)((coreTime >> 32) & 0xFFFFFFFF);
}

void* ATTR_MS_ABI PPCRecompiler_virtualHLE(PPCInterpreter_t* ppcInterpreter, uint32 hleFuncId)
{
	void* prevRSPTemp = ppcInterpreter->rspTemp;
	if (hleFuncId == 0xFFD0)
	{
		ppcInterpreter->remainingCycles -= 500; // let subtract about 500 cycles for each HLE call
		ppcInterpreter->gpr[3] = 0;
		PPCInterpreter_nextInstruction(ppcInterpreter);
		return PPCInterpreter_getCurrentInstance();
	}
	else
	{
		auto hleCall = PPCInterpreter_getHLECall(hleFuncId);
		cemu_assert(hleCall != nullptr);
		hleCall(ppcInterpreter);
	}
	ppcInterpreter->rspTemp = prevRSPTemp;
	return PPCInterpreter_getCurrentInstance();
}

bool AArch64GenContext_t::macro(IMLInstruction* imlInstruction)
{
	if (imlInstruction->operation == PPCREC_IML_MACRO_B_TO_REG)
	{
		XReg branchDstReg = XReg(imlInstruction->op_macro.paramReg.GetRegID());

		mov(tempXReg, offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		add(tempXReg, tempXReg, branchDstReg, ShMod::LSL, 1);
		ldr(tempXReg, AdrReg(ppcRec, tempXReg));
		mov(lrXReg, branchDstReg);
		br(tempXReg);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_BL)
	{
		uint32 newLR = imlInstruction->op_macro.param + 4;

		mov(tempWReg, newLR);
		str(tempWReg, AdrImm(XReg(HCPU_REG_ID), offsetof(PPCInterpreter_t, spr.LR)));

		uint32 newIP = imlInstruction->op_macro.param2;
		uint64 lookupOffset = (uint64)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable) + (uint64)newIP * 2ULL;
		mov(tempXReg, lookupOffset);
		ldr(tempXReg, AdrReg(ppcRec, tempXReg));
		mov(lrWReg, newIP);
		br(tempXReg);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_B_FAR)
	{
		uint32 newIP = imlInstruction->op_macro.param2;
		uint64 lookupOffset = (uint64)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable) + (uint64)newIP * 2ULL;
		mov(tempXReg, lookupOffset);
		ldr(tempXReg, AdrReg(ppcRec, tempXReg));
		mov(lrWReg, newIP);
		br(tempXReg);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_LEAVE)
	{
		uint32 currentInstructionAddress = imlInstruction->op_macro.param;
		mov(tempXReg, (uint64)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable)); // newIP = 0 special value for recompiler exit
		ldr(tempXReg, AdrReg(ppcRec, tempXReg));
		mov(lrWReg, currentInstructionAddress);
		br(tempXReg);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_DEBUGBREAK)
	{
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_COUNT_CYCLES)
	{
		uint32 cycleCount = imlInstruction->op_macro.param;
		AdrImm adrCycles = AdrImm(hCPU, offsetof(PPCInterpreter_t, remainingCycles));
		ldr(tempWReg, adrCycles);
		sub_imm(tempWReg, tempWReg, cycleCount, temp2WReg);
		str(tempWReg, adrCycles);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_HLE)
	{
		uint32 ppcAddress = imlInstruction->op_macro.param;
		uint32 funcId = imlInstruction->op_macro.param2;
		Label cyclesLeftLabel;

		// update instruction pointer
		mov(tempWReg, ppcAddress);
		str(tempWReg, AdrImm(hCPU, offsetof(PPCInterpreter_t, instructionPointer)));
		// set parameters
		str(x30, AdrPreImm(sp, -16));

		mov(x0, hCPU);
		mov(w1, funcId);
		// call HLE function

		mov(tempXReg, (uint64)PPCRecompiler_virtualHLE);
		blr(tempXReg);

		mov(hCPU, x0);

		ldr(x30, AdrPostImm(sp, 16));

		// check if cycles where decreased beyond zero, if yes -> leave recompiler
		ldr(tempWReg, AdrImm(hCPU, offsetof(PPCInterpreter_t, remainingCycles)));
		tbz(tempWReg, 31, cyclesLeftLabel); // check if negative

		mov(tempXReg, offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		ldr(tempXReg, AdrReg(ppcRec, tempXReg));
		ldr(lrWReg, AdrImm(hCPU, offsetof(PPCInterpreter_t, instructionPointer)));
		// JMP [recompilerCallTable+EAX/4*8]
		br(tempXReg);

		L(cyclesLeftLabel);
		// check if instruction pointer was changed
		// assign new instruction pointer to EAX
		ldr(lrWReg, AdrImm(hCPU, offsetof(PPCInterpreter_t, instructionPointer)));
		mov(tempXReg, offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		// remember instruction pointer in REG_EDX
		// EAX *= 2
		add(tempXReg, tempXReg, lrXReg, ShMod::LSL, 1);
		// ADD RAX, R15 (R15 -> Pointer to ppcRecompilerInstanceData
		ldr(tempXReg, AdrReg(ppcRec, tempXReg));
		// JMP [ppcRecompilerDirectJumpTable+RAX/4*8]
		br(tempXReg);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_MFTB)
	{
		uint32 ppcAddress = imlInstruction->op_macro.param;
		uint32 sprId = imlInstruction->op_macro.param2 & 0xFFFF;
		uint32 gprIndex = (imlInstruction->op_macro.param2 >> 16) & 0x1F;

		// update instruction pointer
		mov(tempWReg, ppcAddress);
		str(tempWReg, AdrImm(hCPU, offsetof(PPCInterpreter_t, instructionPointer)));
		// set parameters

		mov(x0, hCPU);
		mov(x1, gprIndex);
		// call function
		if (sprId == SPR_TBL)
			mov(tempXReg, (uint64)PPCRecompiler_getTBL);
		else if (sprId == SPR_TBU)
			mov(tempXReg, (uint64)PPCRecompiler_getTBU);
		else
			cemu_assert_suspicious();

		str(x30, AdrPreImm(sp, -16));
		blr(tempXReg);
		ldr(x30, AdrPostImm(sp, 16));
		return true;
	}
	else
	{
		debug_printf("Unknown recompiler macro operation %d\n", imlInstruction->operation);
		cemu_assert_suspicious();
	}
	return false;
}

bool AArch64GenContext_t::load(IMLInstruction* imlInstruction, bool indexed)
{
	cemu_assert_debug(imlInstruction->op_storeLoad.registerData.GetRegFormat() == IMLRegFormat::I32);
	cemu_assert_debug(imlInstruction->op_storeLoad.registerMem.GetRegFormat() == IMLRegFormat::I32);
	if (indexed)
		cemu_assert_debug(imlInstruction->op_storeLoad.registerMem2.GetRegFormat() == IMLRegFormat::I32);

	IMLRegID dataRegId = imlInstruction->op_storeLoad.registerData.GetRegID();
	IMLRegID memRegId = imlInstruction->op_storeLoad.registerMem.GetRegID();
	IMLRegID memOffsetRegId = indexed ? imlInstruction->op_storeLoad.registerMem2.GetRegID() : PPC_REC_INVALID_REGISTER;
	sint32 memOffset = imlInstruction->op_storeLoad.immS32;
	bool signExtend = imlInstruction->op_storeLoad.flags2.signExtend;
	bool switchEndian = imlInstruction->op_storeLoad.flags2.swapEndian;

	WReg memReg = WReg(memRegId);
	WReg dataReg = WReg(dataRegId);

	add_imm(tempWReg, memReg, memOffset, tempWReg);
	if (indexed)
		add(tempWReg, tempWReg, WReg(memOffsetRegId));

	auto adr = AdrExt(memBase, tempWReg, ExtMod::UXTW);
	if (imlInstruction->op_storeLoad.copyWidth == 32)
	{
		ldr(dataReg, adr);
		if (switchEndian)
			rev(dataReg, dataReg);
	}
	else if (imlInstruction->op_storeLoad.copyWidth == 16)
	{
		if (switchEndian)
		{
			ldrh(dataReg, adr);
			rev(dataReg, dataReg);
			if (signExtend)
				asr(dataReg, dataReg, 16);
			else
				lsr(dataReg, dataReg, 16);
		}
		else
		{
			if (signExtend)
				ldrsh(dataReg, adr);
			else
				ldrh(dataReg, adr);
		}
	}
	else if (imlInstruction->op_storeLoad.copyWidth == 8)
	{
		if (signExtend)
			ldrsb(dataReg, adr);
		else
			ldrb(dataReg, adr);
	}
	else
	{
		return false;
	}
	return true;
}

bool AArch64GenContext_t::store(IMLInstruction* imlInstruction, bool indexed)
{
	cemu_assert_debug(imlInstruction->op_storeLoad.registerData.GetRegFormat() == IMLRegFormat::I32);
	cemu_assert_debug(imlInstruction->op_storeLoad.registerMem.GetRegFormat() == IMLRegFormat::I32);
	if (indexed)
		cemu_assert_debug(imlInstruction->op_storeLoad.registerMem2.GetRegFormat() == IMLRegFormat::I32);

	WReg dataReg = WReg(imlInstruction->op_storeLoad.registerData.GetRegID());
	WReg memReg = WReg(imlInstruction->op_storeLoad.registerMem.GetRegID());
	sint32 memOffset = imlInstruction->op_storeLoad.immS32;
	bool swapEndian = imlInstruction->op_storeLoad.flags2.swapEndian;

	add_imm(tempWReg, memReg, memOffset, tempWReg);
	if (indexed)
		add(tempWReg, tempWReg, WReg(imlInstruction->op_storeLoad.registerMem2.GetRegID()));
	AdrExt adr = AdrExt(memBase, tempWReg, ExtMod::UXTW);
	if (imlInstruction->op_storeLoad.copyWidth == 32)
	{
		if (swapEndian)
		{
			rev(temp2WReg, dataReg);
			str(temp2WReg, adr);
		}
		else
		{
			str(dataReg, adr);
		}
	}
	else if (imlInstruction->op_storeLoad.copyWidth == 16)
	{
		if (swapEndian)
		{
			rev(temp2WReg, dataReg);
			lsr(temp2WReg, temp2WReg, 16);
			strh(temp2WReg, adr);
		}
		else
		{
			strh(dataReg, adr);
		}
	}
	else if (imlInstruction->op_storeLoad.copyWidth == 8)
	{
		strb(dataReg, adr);
	}
	else
	{
		return false;
	}
	return true;
}

void AArch64GenContext_t::atomic_cmp_store(IMLInstruction* imlInstruction)
{
	WReg outReg = WReg(imlInstruction->op_atomic_compare_store.regBoolOut.GetRegID());
	WReg eaReg = WReg(imlInstruction->op_atomic_compare_store.regEA.GetRegID());
	WReg valReg = WReg(imlInstruction->op_atomic_compare_store.regWriteValue.GetRegID());
	WReg cmpValReg = WReg(imlInstruction->op_atomic_compare_store.regCompareValue.GetRegID());

	Label endCmpStore;
	Label notEqual;
	Label storeFailed;

	add(tempXReg, memBase, eaReg, ExtMod::UXTW);
	L(storeFailed);
	ldaxr(temp2WReg, AdrNoOfs(tempXReg));
	cmp(temp2WReg, cmpValReg);
	bne(notEqual);
	stlxr(temp2WReg, valReg, AdrNoOfs(tempXReg));
	cbnz(temp2WReg, storeFailed);
	mov(outReg, 1);
	b(endCmpStore);

	L(notEqual);
	mov(outReg, 0);
	L(endCmpStore);
}

void AArch64GenContext_t::gqr_generateScaleCode(VReg& dataReg, bool isLoad, bool scalePS1, IMLReg registerGQR)
{
	auto gqrReg = XReg(registerGQR.GetRegID());
	// load GQR
	mov(tempXReg, gqrReg);
	// extract scale field and multiply by 16 to get array offset
	lsr(tempWReg, tempWReg, (isLoad ? 16 : 0) + 8 - 4);
	and_(tempWReg, tempWReg, (0x3F << 4));
	// multiply dataReg by scale
	add(tempXReg, tempXReg, ppcRec);
	if (isLoad)
	{
		if (scalePS1)
			mov(temp2XReg, offsetof(PPCRecompilerInstanceData_t, _psq_ld_scale_ps0_ps1));
		else
			mov(temp2XReg, offsetof(PPCRecompilerInstanceData_t, _psq_ld_scale_ps0_1));
	}
	else
	{
		if (scalePS1)
			mov(temp2XReg, offsetof(PPCRecompilerInstanceData_t, _psq_st_scale_ps0_ps1));
		else
			mov(temp2XReg, offsetof(PPCRecompilerInstanceData_t, _psq_st_scale_ps0_1));
	}
	add(tempXReg, tempXReg, temp2XReg);
	ld1(temp2VReg.d2, AdrNoOfs(tempXReg));
	fmul(dataReg.d2, dataReg.d2, temp2VReg.d2);
}

// generate code for PSQ load for a particular type
// if scaleGQR is -1 then a scale of 1.0 is assumed (no scale)
void AArch64GenContext_t::psq_load(uint8 mode, VReg& dataReg, WReg& memReg, WReg& indexReg, sint32 memImmS32, bool indexed, IMLReg registerGQR)
{
	if (mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1)
	{
		add_imm(tempWReg, memReg, memImmS32, tempWReg);
		if (indexed)
			cemu_assert_suspicious();
		ldr(tempXReg, AdrExt(memBase, tempWReg, ExtMod::UXTW));
		rev(tempXReg, tempXReg);
		ror(tempXReg, tempXReg, 32); // swap upper and lower DWORD
		mov(dataReg.d[0], tempXReg);
		fcvtl(dataReg.d2, dataReg.s2);
		// note: floats are not scaled
	}
	else if (mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0)
	{
		add_imm(tempWReg, memReg, memImmS32, tempWReg);
		if (indexed)
			cemu_assert_suspicious();
		ldr(tempWReg, AdrExt(memBase, tempWReg, ExtMod::UXTW));
		rev(tempWReg, tempWReg);
		mov(dataReg.s[0], tempWReg);
		fcvtl(dataReg.d2, dataReg.s2);

		// load constant 1.0 to temp register
		mov(tempXReg, DOUBLE_1_0);
		// overwrite lower half with single from memory
		mov(dataReg.d[1], tempXReg);
		// note: floats are not scaled
	}
	else
	{
		sint32 readSize;
		bool isSigned;
		if (mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0 || mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1)
		{
			readSize = 16;
			isSigned = true;
		}
		else if (mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0 || mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1)
		{
			readSize = 16;
			isSigned = false;
		}
		else if (mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0 || mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1)
		{
			readSize = 8;
			isSigned = true;
		}
		else if (mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0 || mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1)
		{
			readSize = 8;
			isSigned = false;
		}
		else
		{
			cemu_assert_suspicious();
			return;
		}

		bool loadPS1 = mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1 ||
					   mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1 ||
					   mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1 ||
					   mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1;
		if (indexed)
			cemu_assert_suspicious();
		for (sint32 wordIndex = 0; wordIndex < 2; wordIndex++)
		{
			// read from memory
			if (wordIndex == 1 && !loadPS1)
			{
				// store constant 1
				mov(tempXReg, 1);
				mov(dataReg.d[wordIndex], tempXReg);
			}
			else
			{
				sint32 memOffset = memImmS32 + wordIndex * (readSize / 8);
				add_imm(tempWReg, memReg, memOffset, tempWReg);
				auto adr = AdrExt(memBase, tempWReg, ExtMod::UXTW);
				if (readSize == 16)
				{
					// half word
					ldrh(tempWReg, adr);
					rev(tempWReg, tempWReg); // endian swap
					lsr(tempWReg, tempWReg, 16);
					if (isSigned)
						sxth(tempXReg, tempWReg);
				}
				else
				{
					// byte
					if (isSigned)
						ldrsb(tempXReg, adr);
					else
						ldrb(tempWReg, adr);
				}
				// store
				mov(dataReg.d[wordIndex], tempXReg);
			}
		}
		// convert the two integers to doubles
		scvtf(dataReg.d2, dataReg.d2);
		// scale
		if (registerGQR.IsValid())
			gqr_generateScaleCode(dataReg, true, loadPS1, registerGQR);
	}
}

void AArch64GenContext_t::psq_load_generic(uint8 mode, VReg& dataReg, WReg& memReg, WReg& indexReg, sint32 memImmS32, bool indexed, IMLReg registerGQR)
{
	bool loadPS1 = (mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1);
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	Label u8FormatLabel, u16FormatLabel, s8FormatLabel, s16FormatLabel, casesEndLabel;

	// load GQR & extract load type field
	lsr(tempReg32, WReg(registerGQR.GetRegID()), 16);
	and_(tempReg32, tempReg32, 7);

	// jump cases
	cmp(tempReg32, 4); // type 4 -> u8
	beq(u8FormatLabel);

	cmp(tempReg32, 5); // type 5 -> u16
	beq(u16FormatLabel);

	cmp(tempReg32, 6); // type 6 -> s8
	beq(s8FormatLabel);

	cmp(tempReg32, 7); // type 7 -> s16
	beq(s16FormatLabel);

	// default case -> float

	// generate cases
	psq_load(loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0, dataReg, memReg, indexReg, memImmS32, indexed, registerGQR);
	b(casesEndLabel);

	L(u16FormatLabel);
	psq_load(loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_U16_PS0, dataReg, memReg, indexReg, memImmS32, indexed, registerGQR);
	b(casesEndLabel);

	L(s16FormatLabel);
	psq_load(loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_S16_PS0, dataReg, memReg, indexReg, memImmS32, indexed, registerGQR);
	b(casesEndLabel);

	L(u8FormatLabel);
	psq_load(loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_U8_PS0, dataReg, memReg, indexReg, memImmS32, indexed, registerGQR);
	b(casesEndLabel);

	L(s8FormatLabel);
	psq_load(loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_S8_PS0, dataReg, memReg, indexReg, memImmS32, indexed, registerGQR);

	L(casesEndLabel);
}

bool AArch64GenContext_t::fpr_load(IMLInstruction* imlInstruction, bool indexed)
{
	IMLRegID dataRegId = imlInstruction->op_storeLoad.registerData.GetRegID();
	VReg dataVReg = VReg(imlInstruction->op_storeLoad.registerData.GetRegID());
	SReg dataSReg = SReg(dataRegId);
	WReg realRegisterMem = WReg(imlInstruction->op_storeLoad.registerMem.GetRegID());
	WReg realRegisterMem2 = indexed ? WReg(imlInstruction->op_storeLoad.registerMem2.GetRegID()) : wzr;
	sint32 adrOffset = imlInstruction->op_storeLoad.immS32;
	uint8 mode = imlInstruction->op_storeLoad.mode;

	if (mode == PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1)
	{
		add_imm(tempWReg, realRegisterMem, adrOffset, tempWReg);
		if (indexed)
			add(tempWReg, tempWReg, realRegisterMem2);
		ldr(tempWReg, AdrExt(memBase, tempWReg, ExtMod::UXTW));
		rev(tempWReg, tempWReg);
		fmov(dataSReg, tempWReg);

		if (imlInstruction->op_storeLoad.flags2.notExpanded)
		{
			// leave value as single
		}
		else
		{
			fcvtl(dataVReg.d2, dataVReg.s2);
			mov(dataVReg.d[1], dataVReg.d[0]);
		}
	}
	else if (mode == PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0)
	{
		add_imm(tempWReg, realRegisterMem, adrOffset, tempWReg);
		if (indexed)
			add(tempWReg, tempWReg, realRegisterMem2);
		ldr(tempXReg, AdrExt(memBase, tempWReg, ExtMod::UXTW));
		rev(tempXReg, tempXReg);
		mov(dataVReg.d[0], tempXReg);
	}
	else if (mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1)
	{
		psq_load(mode, dataVReg, realRegisterMem, realRegisterMem2, imlInstruction->op_storeLoad.immS32, indexed);
	}
	else if (mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0)
	{
		psq_load_generic(mode, dataVReg, realRegisterMem, realRegisterMem2, imlInstruction->op_storeLoad.immS32, indexed, imlInstruction->op_storeLoad.registerGQR);
	}
	else
	{
		return false;
	}
	return true;
}

void AArch64GenContext_t::psq_store(uint8 mode, IMLRegID dataRegId, WReg& memReg, WReg& indexReg, sint32 memOffset, bool indexed, IMLReg registerGQR)
{
	auto dataVReg = VReg(dataRegId);
	auto dataDReg = DReg(dataRegId);

	bool storePS1 = (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1 ||
					 mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 ||
					 mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 ||
					 mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1 ||
					 mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1);
	bool isFloat = mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0 || mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1;

	if (registerGQR.IsValid())
	{
		// move to temporary reg and update data reg
		mov(tempVReg.b16, dataVReg.b16);
		dataVReg = tempVReg;
		// apply scale
		if (!isFloat)
			gqr_generateScaleCode(dataVReg, false, storePS1, registerGQR);
	}
	if (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0)
	{
		add_imm(tempWReg, memReg, memOffset, tempWReg);
		if (indexed)
			add(tempWReg, tempWReg, indexReg);
		fcvt(tempSReg, dataDReg);
		fmov(temp2WReg, tempSReg);
		rev(temp2WReg, temp2WReg);
		str(temp2WReg, AdrExt(memBase, tempWReg, ExtMod::UXTW));
	}
	else if (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1)
	{
		add_imm(tempWReg, memReg, memOffset, tempWReg);
		if (indexed)
			add(tempWReg, tempWReg, indexReg);
		fcvtn(tempVReg.s2, dataVReg.d2);
		mov(temp2XReg, tempVReg.d[0]);
		ror(temp2XReg, temp2XReg, 32); // swap upper and lower DWORD
		rev(temp2XReg, temp2XReg);
		str(temp2XReg, AdrExt(memBase, tempWReg, ExtMod::UXTW));
	}
	else
	{
		// store as integer
		// set clamp from mode
		sint32 bitWriteSize;
		std::function<void()> clamp;
		if (mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0 || mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1)
		{
			clamp = [&]() {
				mov(temp2WReg, -128);
				cmn(tempWReg, 128);
				csel(temp2WReg, tempWReg, temp2WReg, Cond::GT);
				mov(tempWReg, 127);
				cmp(temp2WReg, 127);
				csel(temp2WReg, temp2WReg, tempWReg, Cond::LT);
			};
			bitWriteSize = 8;
		}
		else if (mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0 || mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1)
		{
			clamp = [&]() {
				mov(temp2WReg, 255);
				bic(tempWReg, tempWReg, tempWReg, ShMod::ASR, 31);
				cmp(tempWReg, 255);
				csel(temp2WReg, tempWReg, temp2WReg, Cond::LT);
			};
			bitWriteSize = 8;
		}
		else if (mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0 || mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1)
		{
			clamp = [&]() {
				mov(temp2WReg, 65535);
				bic(tempWReg, tempWReg, tempWReg, ShMod::ASR, 31);
				cmp(tempWReg, temp2WReg);
				csel(temp2WReg, tempWReg, temp2WReg, Cond::LT);
			};
			bitWriteSize = 16;
		}
		else if (mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0 || mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1)
		{
			clamp = [&]() {
				mov(temp2WReg, -32768);
				cmn(tempWReg, 8, 12);
				csel(temp2WReg, tempWReg, temp2WReg, Cond::GT);
				mov(tempWReg, 32767);
				cmp(temp2WReg, tempWReg);
				csel(temp2WReg, temp2WReg, tempWReg, Cond::LT);
			};
			bitWriteSize = 16;
		}
		else
		{
			cemu_assert_suspicious();
			return;
		}

		if (indexed)
			cemu_assert_suspicious(); // unsupported

		fcvtn(tempVReg.s2, dataVReg.d2);
		fcvtzs(tempVReg.s2, tempVReg.s2);
		for (sint32 valueIndex = 0; valueIndex < (storePS1 ? 2 : 1); valueIndex++)
		{
			// todo - multiply by GQR scale?
			mov(tempWReg, tempVReg.s[valueIndex]);
			// clamp value in tempWReg and store in temp2WReg
			clamp();

			if (bitWriteSize == 16)
			{
				// endian swap
				rev(temp2WReg, temp2WReg);
				lsr(temp2WReg, temp2WReg, 16);
			}
			sint32 address = memOffset + valueIndex * (bitWriteSize / 8);
			add_imm(tempWReg, memReg, address, tempWReg);
			auto adr = AdrExt(memBase, tempWReg, ExtMod::UXTW);
			// write to memory
			if (bitWriteSize == 8)
				strb(temp2WReg, adr);
			else if (bitWriteSize == 16)
				strh(temp2WReg, adr);
		}
	}
}

void AArch64GenContext_t::psq_store_generic(uint8 mode, IMLRegID dataRegId, WReg& memReg, WReg& indexReg, sint32 memOffset, bool indexed, IMLReg registerGQR)
{
	bool storePS1 = (mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1);
	Label u8FormatLabel, u16FormatLabel, s8FormatLabel, s16FormatLabel, casesEndLabel;
	// load GQR & extract store type field
	and_(tempWReg, WReg(registerGQR.GetRegID()), 7);

	// jump cases
	cmp(tempWReg, 4); // type 4 -> u8
	beq(u8FormatLabel);

	cmp(tempWReg, 5); // type 5 -> u16
	beq(u16FormatLabel);

	cmp(tempWReg, 6); // type 6 -> s8
	beq(s8FormatLabel);

	cmp(tempWReg, 7); // type 7 -> s16
	beq(s16FormatLabel);

	// default case -> float

	// generate cases
	psq_store(storePS1 ? PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0, dataRegId, memReg, indexReg, memOffset, indexed, registerGQR);
	b(casesEndLabel);

	L(u16FormatLabel);
	psq_store(storePS1 ? PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_U16_PS0, dataRegId, memReg, indexReg, memOffset, indexed, registerGQR);
	b(casesEndLabel);

	L(s16FormatLabel);
	psq_store(storePS1 ? PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_S16_PS0, dataRegId, memReg, indexReg, memOffset, indexed, registerGQR);
	b(casesEndLabel);

	L(u8FormatLabel);
	psq_store(storePS1 ? PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_U8_PS0, dataRegId, memReg, indexReg, memOffset, indexed, registerGQR);
	b(casesEndLabel);

	L(s8FormatLabel);
	psq_store(storePS1 ? PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_S8_PS0, dataRegId, memReg, indexReg, memOffset, indexed, registerGQR);

	L(casesEndLabel);
}

// store to memory
bool AArch64GenContext_t::fpr_store(IMLInstruction* imlInstruction, bool indexed)
{
	IMLRegID dataRegId = imlInstruction->op_storeLoad.registerData.GetRegID();
	VReg dataReg = VReg(dataRegId);
	DReg dataDReg = DReg(dataRegId);
	WReg memReg = WReg(imlInstruction->op_storeLoad.registerMem.GetRegID());
	WReg indexReg = indexed ? WReg(imlInstruction->op_storeLoad.registerMem2.GetRegID()) : wzr;
	sint32 memOffset = imlInstruction->op_storeLoad.immS32;
	uint8 mode = imlInstruction->op_storeLoad.mode;

	if (mode == PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0)
	{
		add_imm(tempWReg, memReg, memOffset, tempWReg);
		if (indexed)
			add(tempWReg, tempWReg, indexReg);
		auto adr = AdrExt(memBase, tempWReg, ExtMod::UXTW);
		if (imlInstruction->op_storeLoad.flags2.notExpanded)
		{
			// value is already in single format
			mov(temp2WReg, dataReg.s[0]);
		}
		else
		{
			fcvt(tempSReg, dataDReg);
			fmov(temp2WReg, tempSReg);
		}
		rev(temp2WReg, temp2WReg);
		str(temp2WReg, adr);
	}
	else if (mode == PPCREC_FPR_ST_MODE_DOUBLE_FROM_PS0)
	{
		add_imm(tempWReg, memReg, memOffset, tempWReg);
		if (indexed)
			add(tempWReg, tempWReg, indexReg);
		mov(temp2XReg, dataReg.d[0]);
		rev(temp2XReg, temp2XReg);
		str(temp2XReg, AdrExt(memBase, tempWReg, ExtMod::UXTW));
	}
	else if (mode == PPCREC_FPR_ST_MODE_UI32_FROM_PS0)
	{
		add_imm(tempWReg, memReg, memOffset, tempWReg);
		if (indexed)
			add(tempWReg, tempWReg, indexReg);
		mov(temp2WReg, dataReg.s[0]);
		rev(temp2WReg, temp2WReg);
		str(temp2WReg, AdrExt(memBase, tempWReg, ExtMod::UXTW));
	}
	else if (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1)
	{
		cemu_assert_debug(imlInstruction->op_storeLoad.flags2.notExpanded == false);
		psq_store(mode, dataRegId, memReg, indexReg, imlInstruction->op_storeLoad.immS32, indexed);
	}
	else if (mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0)
	{
		psq_store_generic(mode, dataRegId, memReg, indexReg, imlInstruction->op_storeLoad.immS32, indexed, imlInstruction->op_storeLoad.registerGQR);
	}
	else
	{
		cemu_assert_suspicious();
		debug_printf("PPCRecompilerAArch64Gen_imlInstruction_fpr_store(): Unsupported mode %d\n", mode);
		return false;
	}
	return true;
}

// FPR op FPR
void AArch64GenContext_t::fpr_r_r(IMLInstruction* imlInstruction)
{
	IMLRegID regAId = imlInstruction->op_fpr_r_r.regA.GetRegID();
	IMLRegID regRId = imlInstruction->op_fpr_r_r.regR.GetRegID();
	VReg regRVReg = VReg(regRId);
	VReg regAVReg = VReg(regAId);
	DReg regADReg = DReg(regAId);

	if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP)
	{
		dup(regRVReg.d2, regAVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP)
	{
		dup(regRVReg.d2, regAVReg.d[1]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM)
	{
		if (regRId != regAId)
			mov(regRVReg.d[0], regAVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_TOP)
	{
		mov(regRVReg.d[1], regAVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED)
	{
		mov(tempVReg.b16, regAVReg.b16);
		mov(regRVReg.d[0], tempVReg.d[1]);
		mov(regRVReg.d[1], tempVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_TOP)
	{
		if (regRId != regAId)
			mov(regRVReg.d[1], regAVReg.d[1]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM)
	{
		mov(regRVReg.d[0], regAVReg.d[1]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM)
	{
		mov(tempVReg.b16, regAVReg.b16);
		fmul(tempVReg.d2, regRVReg.d2, tempVReg.d2);
		mov(regRVReg.d[0], tempVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_PAIR)
	{
		fmul(regRVReg.d2, regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_DIVIDE_BOTTOM)
	{
		mov(tempVReg.b16, regAVReg.b16);
		fdiv(tempVReg.d2, regRVReg.d2, tempVReg.d2);
		mov(regRVReg.d[0], tempVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_DIVIDE_PAIR)
	{
		fdiv(regRVReg.d2, regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_BOTTOM)
	{
		mov(tempVReg.b16, regAVReg.b16);
		fadd(tempVReg.d2, regRVReg.d2, tempVReg.d2);
		mov(regRVReg.d[0], tempVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_PAIR)
	{
		fadd(regRVReg.d2, regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_PAIR)
	{
		fsub(regRVReg.d2, regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_BOTTOM)
	{
		mov(tempVReg.b16, regAVReg.b16);
		fsub(tempVReg.d2, regRVReg.d2, tempVReg.d2);
		mov(regRVReg.d[0], tempVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	{
		if (regRId != regAId)
			mov(regRVReg.b16, regAVReg.b16);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_FCTIWZ)
	{
		fcvtzs(tempWReg, regADReg);
		mov(regRVReg.d[0], tempXReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_FRES_TO_BOTTOM_AND_TOP)
	{
		mov(temp2XReg, x30);
		mov(tempXReg, (uint64)recompiler_fres);
		mov(asmRoutineVReg.d[0], regAVReg.d[0]);
		blr(tempXReg);
		dup(regRVReg.d2, asmRoutineVReg.d[0]);
		mov(x30, temp2XReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_RECIPROCAL_SQRT)
	{
		mov(temp2XReg, x30);
		mov(tempXReg, (uint64)recompiler_frsqrte);
		mov(asmRoutineVReg.d[0], regAVReg.d[0]);
		blr(tempXReg);
		mov(regRVReg.d[0], asmRoutineVReg.d[0]);
		mov(x30, temp2XReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATE_PAIR)
	{
		fneg(regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ABS_PAIR)
	{
		fabs(regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_FRES_PAIR)
	{
		mov(temp2XReg, x30);
		mov(tempXReg, (uint64)recompiler_fres);
		mov(asmRoutineVReg.d[0], regAVReg.d[0]);
		blr(tempXReg);
		mov(regRVReg.d[0], asmRoutineVReg.d[0]);
		mov(asmRoutineVReg.d[0], regAVReg.d[1]);
		blr(tempXReg);
		mov(regRVReg.d[1], asmRoutineVReg.d[0]);
		mov(x30, temp2XReg);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_FRSQRTE_PAIR)
	{
		mov(temp2XReg, x30);
		mov(tempXReg, (uint64)recompiler_frsqrte);
		mov(asmRoutineVReg.d[0], regAVReg.d[0]);
		blr(tempXReg);
		mov(regRVReg.d[0], asmRoutineVReg.d[0]);
		mov(asmRoutineVReg.d[0], regAVReg.d[1]);
		blr(tempXReg);
		mov(regRVReg.d[1], asmRoutineVReg.d[0]);
		mov(x30, temp2XReg);
	}
	else
	{
		cemu_assert_suspicious();
	}
}

void AArch64GenContext_t::fpr_r_r_r(IMLInstruction* imlInstruction)
{
	auto regR = VReg(imlInstruction->op_fpr_r_r_r.regR.GetRegID());
	auto regA = VReg(imlInstruction->op_fpr_r_r_r.regA.GetRegID());
	auto regB = VReg(imlInstruction->op_fpr_r_r_r.regB.GetRegID());

	if (imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM)
	{
		fmul(tempVReg.d2, regA.d2, regB.d2);
		mov(regR.d[0], tempVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_BOTTOM)
	{
		fadd(tempVReg.d2, regA.d2, regB.d2);
		mov(regR.d[0], tempVReg.d[0]);
		mov(regR.d[1], regA.d[1]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_PAIR)
	{
		fsub(regR.d2, regA.d2, regB.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_BOTTOM)
	{
		fsub(tempVReg.d2, regA.d2, regB.d2);
		mov(regR.d[0], tempVReg.d[0]);
	}
	else
	{
		cemu_assert_suspicious();
	}
}

/*
 * FPR = op (fprA, fprB, fprC)
 */
void AArch64GenContext_t::fpr_r_r_r_r(IMLInstruction* imlInstruction)
{
	auto regR = VReg(imlInstruction->op_fpr_r_r_r_r.regR.GetRegID());
	auto regA = VReg(imlInstruction->op_fpr_r_r_r_r.regA.GetRegID());
	auto regB = VReg(imlInstruction->op_fpr_r_r_r_r.regB.GetRegID());
	auto regC = VReg(imlInstruction->op_fpr_r_r_r_r.regC.GetRegID());

	if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUM0)
	{
		mov(tempVReg.d[0], regB.d[1]);
		fadd(tempVReg.d2, tempVReg.d2, regA.d2);
		mov(tempVReg.d[1], regC.d[1]);
		mov(regR.b16, tempVReg.b16);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUM1)
	{
		mov(tempVReg.d[1], regA.d[0]);
		fadd(tempVReg.d2, tempVReg.d2, regB.d2);
		mov(tempVReg.d[0], regC.d[0]);
		mov(regR.b16, tempVReg.b16);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SELECT_BOTTOM)
	{
		auto regADReg = DReg(imlInstruction->op_fpr_r_r_r_r.regA.GetRegID());
		auto regBDReg = DReg(imlInstruction->op_fpr_r_r_r_r.regB.GetRegID());
		auto regCDReg = DReg(imlInstruction->op_fpr_r_r_r_r.regC.GetRegID());
		fcmp(regADReg, 0.0);
		fcsel(tempDReg, regCDReg, regBDReg, Cond::GE);
		mov(regR.d[0], tempVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SELECT_PAIR)
	{
		fcmge(tempVReg.d2, regA.d2, 0.0);
		bsl(tempVReg.b16, regC.b16, regB.b16);
		mov(regR.b16, tempVReg.b16);
	}
	else
	{
		cemu_assert_suspicious();
	}
}

void AArch64GenContext_t::fpr_r(IMLInstruction* imlInstruction)
{
	auto regR = VReg(imlInstruction->op_fpr_r.regR.GetRegID());

	if (imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATE_BOTTOM)
	{
		fneg(tempVReg.d2, regR.d2);
		mov(regR.d[0], tempVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ABS_BOTTOM)
	{
		fabs(tempVReg.d2, regR.d2);
		mov(regR.d[0], tempVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATIVE_ABS_BOTTOM)
	{
		fabs(tempVReg.d2, regR.d2);
		fneg(tempVReg.d2, tempVReg.d2);
		mov(regR.d[0], tempVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM)
	{
		// convert to 32bit single
		fcvtn(tempVReg.s2, regR.d2);
		// convert back to 64bit double
		fcvtl(tempVReg.d2, tempVReg.s2);
		mov(regR.d[0], tempVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_PAIR)
	{
		// convert to 32bit singles
		fcvtn(regR.s2, regR.d2);
		// convert back to 64bit doubles
		fcvtl(regR.d2, regR.s2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64)
	{
		// convert bottom to 64bit double
		fcvtl(regR.d2, regR.s2);
		// copy to top half
		dup(regR.d2, regR.d[0]);
	}
	else
	{
		cemu_assert_unimplemented();
	}
}

Cond ImlFPCondToArm64Cond(IMLCondition cond)
{
	switch (cond)
	{
	case IMLCondition::UNORDERED_GT:
		return Cond::GT;
	case IMLCondition::UNORDERED_LT:
		return Cond::MI;
	case IMLCondition::UNORDERED_EQ:
		return Cond::EQ;
	case IMLCondition::UNORDERED_U:
		return Cond::VS;
	default:
	{
		cemu_assert_suspicious();
		return Cond::EQ;
	}
	}
}

void AArch64GenContext_t::fpr_compare(IMLInstruction* imlInstruction)
{
	auto regR = XReg(imlInstruction->op_fpr_compare.regR.GetRegID());
	auto regA = DReg(imlInstruction->op_fpr_compare.regA.GetRegID());
	auto regB = DReg(imlInstruction->op_fpr_compare.regB.GetRegID());
	auto cond = ImlFPCondToArm64Cond(imlInstruction->op_fpr_compare.cond);
	fcmp(regA, regB);
	cset(regR, cond);
}

std::unique_ptr<CodeContext> PPCRecompiler_generateAArch64Code(struct PPCRecFunction_t* PPCRecFunction, struct ppcImlGenContext_t* ppcImlGenContext)
{
	auto aarch64GenContext = std::make_unique<AArch64GenContext_t>();
	std::unordered_map<IMLSegment*, Label> labels;
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		if (segIt->nextSegmentBranchTaken != nullptr)
		{
			labels[segIt->nextSegmentBranchTaken] = Label();
		}
	}
	// generate iml instruction code
	bool codeGenerationFailed = false;
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		if (codeGenerationFailed)
			break;
		segIt->x64Offset = aarch64GenContext->getSize();

		if (auto label = labels.find(segIt); label != labels.end())
		{
			aarch64GenContext->L(label->second);
		}

		for (size_t i = 0; i < segIt->imlList.size(); i++)
		{
			IMLInstruction* imlInstruction = segIt->imlList.data() + i;
			if (imlInstruction->type == PPCREC_IML_TYPE_R_NAME)
			{
				aarch64GenContext->r_name(imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_NAME_R)
			{
				aarch64GenContext->name_r(imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R)
			{
				if (!aarch64GenContext->r_r(imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_S32)
			{
				if (!aarch64GenContext->r_s32(imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
			{
				if (!aarch64GenContext->conditional_r_s32(imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_S32)
			{
				if (!aarch64GenContext->r_r_s32(imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_S32_CARRY)
			{
				if (!aarch64GenContext->r_r_s32_carry(imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_R)
			{
				if (!aarch64GenContext->r_r_r(imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_R_CARRY)
			{
				if (!aarch64GenContext->r_r_r_carry(imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_COMPARE)
			{
				aarch64GenContext->compare(imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_COMPARE_S32)
			{
				aarch64GenContext->compare_s32(imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_JUMP)
			{
				if (segIt->nextSegmentBranchTaken == segIt)
					cemu_assert_suspicious();
				aarch64GenContext->cjump(labels, imlInstruction, segIt);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_JUMP)
			{
				aarch64GenContext->jump(labels, segIt);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
			{
				aarch64GenContext->conditionalJumpCycleCheck(labels, segIt);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_MACRO)
			{
				if (!aarch64GenContext->macro(imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_LOAD)
			{
				if (!aarch64GenContext->load(imlInstruction, false))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_LOAD_INDEXED)
			{
				if (!aarch64GenContext->load(imlInstruction, true))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_STORE)
			{
				if (!aarch64GenContext->store(imlInstruction, false))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_STORE_INDEXED)
			{
				if (!aarch64GenContext->store(imlInstruction, true))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_ATOMIC_CMP_STORE)
			{
				aarch64GenContext->atomic_cmp_store(imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_NO_OP)
			{
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD)
			{
				if (!aarch64GenContext->fpr_load(imlInstruction, false))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED)
			{
				if (!aarch64GenContext->fpr_load(imlInstruction, true))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE)
			{
				if (!aarch64GenContext->fpr_store(imlInstruction, false))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE_INDEXED)
			{
				if (!aarch64GenContext->fpr_store(imlInstruction, true))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R)
			{
				aarch64GenContext->fpr_r_r(imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R)
			{
				aarch64GenContext->fpr_r_r_r(imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R_R)
			{
				aarch64GenContext->fpr_r_r_r_r(imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R)
			{
				aarch64GenContext->fpr_r(imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_COMPARE)
			{
				aarch64GenContext->fpr_compare(imlInstruction);
			}
			else
			{
				codeGenerationFailed = true;
				// cemu_assert_suspicious();
				debug_printf("PPCRecompiler_generateX64Code(): Unsupported iml type 0x%x\n", imlInstruction->type);
			}
		}
	}

	// handle failed code generation
	if (codeGenerationFailed)
	{
		return nullptr;
	}
	aarch64GenContext->readyRE();

	// set code
	PPCRecFunction->x86Code = aarch64GenContext->getCode<void*>();
	PPCRecFunction->x86Size = aarch64GenContext->getSize();
	return aarch64GenContext;
}

void AArch64GenContext_t::enterRecompilerCode()
{
	constexpr size_t stackSize = 8 * (30 - 16) /* x16 - x30 */ + 8 * (15 - 8) /*v8.d[0] - v15.d[0]*/ + 8;
	static_assert(stackSize % 16 == 0);
	sub(sp, sp, stackSize);
	mov(x9, sp);

	stp(x16, x17, AdrPostImm(x9, 16));
	stp(x19, x20, AdrPostImm(x9, 16));
	stp(x21, x22, AdrPostImm(x9, 16));
	stp(x23, x24, AdrPostImm(x9, 16));
	stp(x25, x26, AdrPostImm(x9, 16));
	stp(x27, x28, AdrPostImm(x9, 16));
	stp(x29, x30, AdrPostImm(x9, 16));
	st4((v8.d - v11.d)[0], AdrPostImm(x9, 32));
	st4((v12.d - v15.d)[0], AdrPostImm(x9, 32));
	// MOV RSP, RDX (ppc interpreter instance)
	mov(hCPU, x1); // call argument 2
	// MOV R15, ppcRecompilerInstanceData
	mov(ppcRec, (uint64)ppcRecompilerInstanceData);
	// MOV R13, memory_base
	mov(memBase, (uint64)memory_base);

	// JMP recFunc
	blr(x0); // call argument 1

	mov(x9, sp);
	ldp(x16, x17, AdrPostImm(x9, 16));
	ldp(x19, x20, AdrPostImm(x9, 16));
	ldp(x21, x22, AdrPostImm(x9, 16));
	ldp(x23, x24, AdrPostImm(x9, 16));
	ldp(x25, x26, AdrPostImm(x9, 16));
	ldp(x27, x28, AdrPostImm(x9, 16));
	ldp(x29, x30, AdrPostImm(x9, 16));
	ld4((v8.d - v11.d)[0], AdrPostImm(x9, 32));
	ld4((v12.d - v15.d)[0], AdrPostImm(x9, 32));

	add(sp, sp, stackSize);
	ret();
}

void AArch64GenContext_t::leaveRecompilerCode()
{
	str(lrWReg, AdrImm(hCPU, offsetof(PPCInterpreter_t, instructionPointer)));
	ret();
}

bool initializedInterfaceFunctions = false;
AArch64GenContext_t enterRecompilerCode_ctx{};

AArch64GenContext_t leaveRecompilerCode_unvisited_ctx{};
AArch64GenContext_t leaveRecompilerCode_visited_ctx{};
void PPCRecompilerAArch64Gen_generateRecompilerInterfaceFunctions()
{
	if (initializedInterfaceFunctions)
		return;
	initializedInterfaceFunctions = true;

	enterRecompilerCode_ctx.enterRecompilerCode();
	enterRecompilerCode_ctx.readyRE();
	PPCRecompiler_enterRecompilerCode = enterRecompilerCode_ctx.getCode<decltype(PPCRecompiler_enterRecompilerCode)>();

	leaveRecompilerCode_unvisited_ctx.leaveRecompilerCode();
	leaveRecompilerCode_unvisited_ctx.readyRE();
	PPCRecompiler_leaveRecompilerCode_unvisited = leaveRecompilerCode_unvisited_ctx.getCode<decltype(PPCRecompiler_leaveRecompilerCode_unvisited)>();

	leaveRecompilerCode_visited_ctx.leaveRecompilerCode();
	leaveRecompilerCode_visited_ctx.readyRE();
	PPCRecompiler_leaveRecompilerCode_visited = leaveRecompilerCode_visited_ctx.getCode<decltype(PPCRecompiler_leaveRecompilerCode_visited)>();

	cemu_assert_debug(PPCRecompiler_leaveRecompilerCode_unvisited != PPCRecompiler_leaveRecompilerCode_visited);
}
