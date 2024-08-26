#include "BackendAArch64.h"

#include <cstddef>

#include "../PPCRecompiler.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Common/precompiled.h"
#include "HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "HW/Espresso/PPCState.h"
#include "HW/Espresso/Recompiler/IML/IMLInstruction.h"

#pragma push_macro("CSIZE")
#undef CSIZE

#include <util/MemMapper/MemMapper.h>
#include <xbyak_aarch64.h>
#include <HW/Espresso/Interpreter/PPCInterpreterHelper.h>
#include <asm/x64util.h>

using namespace Xbyak_aarch64;

#pragma pop_macro("CSIZE")

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

struct AArch64GenContext_t : CodeGenerator
{
	AArch64GenContext_t()
		: CodeGenerator(4096, AutoGrow)
	{
	}

	IMLSegment* currentSegment{};
	std::unordered_map<IMLSegment*, Label> labels;
};

void PPCRecompilerAArch64Gen_imlInstruction_r_name(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	uint32 name = imlInstruction->op_r_name.name;
	auto regId = imlInstruction->op_r_name.regR.GetRegID();
	auto hcpuReg = XReg(HCPU_REG_ID);

	if (imlInstruction->op_r_name.regR.GetBaseFormat() == IMLRegFormat::I64)
	{
		auto regR = WReg(regId);
		if (name >= PPCREC_NAME_R0 && name < PPCREC_NAME_R0 + 32)
		{
			aarch64GenContext->ldr(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, gpr) + sizeof(uint32) * (name - PPCREC_NAME_R0)));
		}
		else if (name >= PPCREC_NAME_SPR0 && name < PPCREC_NAME_SPR0 + 999)
		{
			sint32 sprIndex = (name - PPCREC_NAME_SPR0);
			if (sprIndex == SPR_LR)
				aarch64GenContext->ldr(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, spr.LR)));
			else if (sprIndex == SPR_CTR)
				aarch64GenContext->ldr(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, spr.CTR)));
			else if (sprIndex == SPR_XER)
				aarch64GenContext->ldr(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, spr.XER)));
			else if (sprIndex >= SPR_UGQR0 && sprIndex <= SPR_UGQR7)
				aarch64GenContext->ldr(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, spr.UGQR) + sizeof(PPCInterpreter_t::spr.UGQR[0]) * (sprIndex - SPR_UGQR0)));
			else
				cemu_assert_suspicious();
		}
		else if (name >= PPCREC_NAME_TEMPORARY && name < PPCREC_NAME_TEMPORARY + 4)
		{
			aarch64GenContext->ldr(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, temporaryGPR_reg) + sizeof(uint32) * (name - PPCREC_NAME_TEMPORARY)));
		}
		else if (name == PPCREC_NAME_XER_CA)
		{
			aarch64GenContext->ldrb(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, xer_ca)));
		}
		else if (name == PPCREC_NAME_XER_SO)
		{
			aarch64GenContext->ldrb(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, xer_so)));
		}
		else if (name >= PPCREC_NAME_CR && name <= PPCREC_NAME_CR_LAST)
		{
			aarch64GenContext->ldrb(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, cr) + (name - PPCREC_NAME_CR)));
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_EA)
		{
			aarch64GenContext->ldr(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, reservedMemAddr)));
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_VAL)
		{
			aarch64GenContext->ldr(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, reservedMemValue)));
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
			aarch64GenContext->mov(tempReg, offsetof(PPCInterpreter_t, fpr) + sizeof(FPR_t) * (name - PPCREC_NAME_FPR0));
			aarch64GenContext->add(tempReg, tempReg, hcpuReg);
			aarch64GenContext->ld1(regR.d2, AdrNoOfs(tempReg));
		}
		else if (name >= PPCREC_NAME_TEMPORARY_FPR0 && name < (PPCREC_NAME_TEMPORARY_FPR0 + 8))
		{
			aarch64GenContext->mov(tempReg, offsetof(PPCInterpreter_t, temporaryFPR) + sizeof(FPR_t) * (name - PPCREC_NAME_TEMPORARY_FPR0));
			aarch64GenContext->add(tempReg, tempReg, hcpuReg);
			aarch64GenContext->ld1(regR.d2, AdrNoOfs(tempReg));
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

void PPCRecompilerAArch64Gen_imlInstruction_name_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	uint32 name = imlInstruction->op_r_name.name;
	IMLRegID regId = imlInstruction->op_r_name.regR.GetRegID();
	auto hcpuReg = XReg(HCPU_REG_ID);

	if (imlInstruction->op_r_name.regR.GetBaseFormat() == IMLRegFormat::I64)
	{
		auto regR = WReg(regId);
		if (name >= PPCREC_NAME_R0 && name < PPCREC_NAME_R0 + 32)
		{
			aarch64GenContext->str(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, gpr) + sizeof(uint32) * (name - PPCREC_NAME_R0)));
		}
		else if (name >= PPCREC_NAME_SPR0 && name < PPCREC_NAME_SPR0 + 999)
		{
			uint32 sprIndex = (name - PPCREC_NAME_SPR0);
			if (sprIndex == SPR_LR)
				aarch64GenContext->str(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, spr.LR)));
			else if (sprIndex == SPR_CTR)
				aarch64GenContext->str(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, spr.CTR)));
			else if (sprIndex == SPR_XER)
				aarch64GenContext->str(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, spr.XER)));
			else if (sprIndex >= SPR_UGQR0 && sprIndex <= SPR_UGQR7)
				aarch64GenContext->str(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, spr.UGQR) + sizeof(PPCInterpreter_t::spr.UGQR[0]) * (sprIndex - SPR_UGQR0)));
			else
				cemu_assert_suspicious();
		}
		else if (name >= PPCREC_NAME_TEMPORARY && name < PPCREC_NAME_TEMPORARY + 4)
		{
			aarch64GenContext->str(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, temporaryGPR_reg) + sizeof(uint32) * (name - PPCREC_NAME_TEMPORARY)));
		}
		else if (name == PPCREC_NAME_XER_CA)
		{
			aarch64GenContext->strb(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, xer_ca)));
		}
		else if (name == PPCREC_NAME_XER_SO)
		{
			aarch64GenContext->strb(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, xer_so)));
		}
		else if (name >= PPCREC_NAME_CR && name <= PPCREC_NAME_CR_LAST)
		{
			aarch64GenContext->strb(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, cr) + (name - PPCREC_NAME_CR)));
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_EA)
		{
			aarch64GenContext->str(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, reservedMemAddr)));
		}
		else if (name == PPCREC_NAME_CPU_MEMRES_VAL)
		{
			aarch64GenContext->str(regR, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, reservedMemValue)));
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
			aarch64GenContext->mov(tempReg, offsetof(PPCInterpreter_t, fpr) + sizeof(FPR_t) * (name - PPCREC_NAME_FPR0));
			aarch64GenContext->add(tempReg, tempReg, hcpuReg);
			aarch64GenContext->st1(regR.d2, AdrNoOfs(tempReg));
		}
		else if (name >= PPCREC_NAME_TEMPORARY_FPR0 && name < (PPCREC_NAME_TEMPORARY_FPR0 + 8))
		{
			aarch64GenContext->mov(tempReg, offsetof(PPCInterpreter_t, temporaryFPR) + sizeof(FPR_t) * (name - PPCREC_NAME_TEMPORARY_FPR0));
			aarch64GenContext->add(tempReg, tempReg, hcpuReg);
			aarch64GenContext->st1(regR.d2, AdrNoOfs(tempReg));
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

bool PPCRecompilerAArch64Gen_imlInstruction_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = WReg(imlInstruction->op_r_r.regR.GetRegID());
	auto regA = WReg(imlInstruction->op_r_r.regA.GetRegID());

	if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	{
		if (imlInstruction->op_r_r.regR.GetRegID() != imlInstruction->op_r_r.regA.GetRegID())
			aarch64GenContext->mov(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ENDIAN_SWAP)
	{
		aarch64GenContext->rev(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN_S8_TO_S32)
	{
		aarch64GenContext->sxtb(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN_S16_TO_S32)
	{
		aarch64GenContext->sxth(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_NOT)
	{
		aarch64GenContext->mvn(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_NEG)
	{
		aarch64GenContext->neg(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_CNTLZW)
	{
		aarch64GenContext->clz(regR, regA);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_DCBZ)
	{
		cemu_assert_suspicious();
		// TODO: Implement this
		return false;
	}
	else
	{
		debug_printf("PPCRecompilerAArch64Gen_imlInstruction_r_r(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerAArch64Gen_imlInstruction_r_s32(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	sint32 imm32 = imlInstruction->op_r_immS32.immS32;
	auto reg = WReg(imlInstruction->op_r_immS32.regR.GetRegID());

	if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	{
		aarch64GenContext->mov(reg, imm32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_LEFT_ROTATE)
	{
		aarch64GenContext->ror(reg, reg, 32 - (imm32 & 0x1f));
	}
	else
	{
		debug_printf("PPCRecompilerAArch64Gen_imlInstruction_r_s32(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerAArch64Gen_imlInstruction_conditional_r_s32(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	cemu_assert_unimplemented();
	return false;
}

bool PPCRecompilerAArch64Gen_imlInstruction_r_r_s32(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = WReg(imlInstruction->op_r_r_s32.regR.GetRegID());
	auto regA = WReg(imlInstruction->op_r_r_s32.regA.GetRegID());
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	sint32 immS32 = imlInstruction->op_r_r_s32.immS32;

	if (imlInstruction->operation == PPCREC_IML_OP_ADD)
	{
		aarch64GenContext->mov(tempReg32, immS32);
		aarch64GenContext->add(regR, regA, tempReg32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_SUB)
	{
		aarch64GenContext->mov(tempReg32, immS32);
		aarch64GenContext->sub(regR, regA, tempReg32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_AND)
	{
		aarch64GenContext->mov(tempReg32, immS32);
		aarch64GenContext->and_(regR, regA, tempReg32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_OR)
	{
		aarch64GenContext->mov(tempReg32, immS32);
		aarch64GenContext->orr(regR, regA, tempReg32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_XOR)
	{
		aarch64GenContext->mov(tempReg32, immS32);
		aarch64GenContext->eor(regR, regA, tempReg32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_RLWIMI)
	{
		uint32 vImm = (uint32)immS32;
		uint32 mb = (vImm >> 0) & 0xFF;
		uint32 me = (vImm >> 8) & 0xFF;
		uint32 sh = (vImm >> 16) & 0xFF;
		uint32 mask = ppc_mask(mb, me);
		if (sh)
			aarch64GenContext->ror(tempReg32, regA, 32 - (sh & 0x1F));
		else
			aarch64GenContext->mov(tempReg32, regA);
		aarch64GenContext->and_(regR, regR, ~mask);
		aarch64GenContext->and_(tempReg32, tempReg32, mask);
		aarch64GenContext->orr(regR, regR, tempReg32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_SIGNED)
	{
		aarch64GenContext->mov(tempReg32, immS32);
		aarch64GenContext->mul(regR, regA, tempReg32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_LEFT_SHIFT)
	{
		aarch64GenContext->mov(tempReg32, immS32);
		aarch64GenContext->lsl(regR, regA, tempReg32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_U)
	{
		aarch64GenContext->mov(tempReg32, immS32);
		aarch64GenContext->lsr(regR, regA, tempReg32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_S)
	{
		aarch64GenContext->mov(tempReg32, immS32);
		aarch64GenContext->asr(regR, regA, tempReg32);
	}
	else
	{
		debug_printf("PPCRecompilerAArch64Gen_imlInstruction_r_r_s32(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerAArch64Gen_imlInstruction_r_r_s32_carry(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = WReg(imlInstruction->op_r_r_s32_carry.regR.GetRegID());
	auto regA = WReg(imlInstruction->op_r_r_s32_carry.regA.GetRegID());
	auto regCarry = XReg(imlInstruction->op_r_r_s32_carry.regCarry.GetRegID());
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	auto tempReg64 = XReg(TEMP_REGISTER_ID);
	sint32 immS32 = imlInstruction->op_r_r_s32_carry.immS32;
	if (imlInstruction->operation == PPCREC_IML_OP_ADD)
	{
		aarch64GenContext->mov(tempReg32, immS32);
		aarch64GenContext->adds(regR, regA, tempReg32);
		aarch64GenContext->cset(regCarry, Cond::CS);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ADD_WITH_CARRY)
	{
		aarch64GenContext->mrs(tempReg64, 0b11, 0b011, 0b0100, 0b0010, 0b000); // NZCV
		aarch64GenContext->bfi(tempReg64, regCarry, 29, 1);
		aarch64GenContext->msr(0b11, 0b011, 0b0100, 0b0010, 0b000, tempReg64);
		aarch64GenContext->mov(tempReg32, immS32);
		aarch64GenContext->adcs(regR, regA, tempReg32);
		aarch64GenContext->cset(regCarry, Cond::CS);
	}
	else
	{
		cemu_assert_suspicious();
		return false;
	}

	return true;
}

bool PPCRecompilerAArch64Gen_imlInstruction_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regResult = WReg(imlInstruction->op_r_r_r.regR.GetRegID());
	auto regOperand1 = WReg(imlInstruction->op_r_r_r.regA.GetRegID());
	auto regOperand2 = WReg(imlInstruction->op_r_r_r.regB.GetRegID());
	auto regOperand2_64 = WReg(imlInstruction->op_r_r_r.regB.GetRegID());
	auto tempReg32 = WReg(TEMP_REGISTER_ID);

	if (imlInstruction->operation == PPCREC_IML_OP_ADD)
	{
		aarch64GenContext->add(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_SUB)
	{
		aarch64GenContext->sub(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_OR)
	{
		aarch64GenContext->orr(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_AND)
	{
		aarch64GenContext->and_(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_XOR)
	{
		aarch64GenContext->eor(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_SIGNED)
	{
		aarch64GenContext->mul(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_SLW)
	{
		aarch64GenContext->tst(regOperand2_64, 32);
		aarch64GenContext->lsl(regResult, regOperand1, regOperand2);
		aarch64GenContext->csel(regResult, regResult, aarch64GenContext->wzr, Cond::EQ);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_SRW)
	{
		aarch64GenContext->tst(regOperand2_64, 32);
		aarch64GenContext->lsr(regResult, regOperand1, regOperand2);
		aarch64GenContext->csel(regResult, regResult, aarch64GenContext->wzr, Cond::EQ);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_LEFT_ROTATE)
	{
		aarch64GenContext->neg(tempReg32, regOperand2);
		aarch64GenContext->ror(regResult, regOperand1, tempReg32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_S)
	{
		aarch64GenContext->asr(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_RIGHT_SHIFT_U)
	{
		aarch64GenContext->lsr(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_LEFT_SHIFT)
	{
		aarch64GenContext->lsl(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_DIVIDE_SIGNED)
	{
		aarch64GenContext->sdiv(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_DIVIDE_UNSIGNED)
	{
		aarch64GenContext->udiv(regResult, regOperand1, regOperand2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED)
	{
		auto reg64Result = XReg(imlInstruction->op_r_r_r.regR.GetRegID());
		auto reg32Operand1 = WReg(imlInstruction->op_r_r_r.regA.GetRegID());
		auto reg32Operand2 = WReg(imlInstruction->op_r_r_r.regB.GetRegID());
		aarch64GenContext->smull(reg64Result, reg32Operand1, reg32Operand2);
		aarch64GenContext->lsr(reg64Result, reg64Result, 32);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_MULTIPLY_HIGH_UNSIGNED)
	{
		auto reg64Result = XReg(imlInstruction->op_r_r_r.regR.GetRegID());
		auto reg32Operand1 = WReg(imlInstruction->op_r_r_r.regA.GetRegID());
		auto reg32Operand2 = WReg(imlInstruction->op_r_r_r.regB.GetRegID());
		aarch64GenContext->umull(reg64Result, reg32Operand1, reg32Operand2);
		aarch64GenContext->lsr(reg64Result, reg64Result, 32);
	}
	else
	{
		debug_printf("PPCRecompilerAArch64Gen_imlInstruction_r_r_r(): Unsupported operation 0x%x\n", imlInstruction->operation);
		return false;
	}
	return true;
}

bool PPCRecompilerAArch64Gen_imlInstruction_r_r_r_carry(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = WReg(imlInstruction->op_r_r_r_carry.regR.GetRegID());
	auto regA = WReg(imlInstruction->op_r_r_r_carry.regA.GetRegID());
	auto regB = WReg(imlInstruction->op_r_r_r_carry.regB.GetRegID());
	auto regCarry = XReg(imlInstruction->op_r_r_r_carry.regCarry.GetRegID());
	auto tempReg64 = XReg(TEMP_REGISTER_ID);
	if (imlInstruction->operation == PPCREC_IML_OP_ADD)
	{
		aarch64GenContext->adds(regR, regA, regB);
		aarch64GenContext->cset(regCarry, Cond::CS);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ADD_WITH_CARRY)
	{
		aarch64GenContext->mrs(tempReg64, 0b11, 0b011, 0b0100, 0b0010, 0b000); // NZCV
		aarch64GenContext->bfi(tempReg64, regCarry, 29, 1);
		aarch64GenContext->msr(0b11, 0b011, 0b0100, 0b0010, 0b000, tempReg64);
		aarch64GenContext->adcs(regR, regA, regB);
		aarch64GenContext->cset(regCarry, Cond::CS);
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

bool PPCRecompilerAArch64Gen_imlInstruction_compare(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = WReg(imlInstruction->op_compare.regR.GetRegID());
	auto regA = WReg(imlInstruction->op_compare.regA.GetRegID());
	auto regB = WReg(imlInstruction->op_compare.regB.GetRegID());
	auto cond = ImlCondToArm64Cond(imlInstruction->op_compare.cond);
	aarch64GenContext->cmp(regA, regB);
	aarch64GenContext->cset(regR, cond);
	return true;
}

bool PPCRecompilerAArch64Gen_imlInstruction_compare_s32(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = WReg(imlInstruction->op_compare.regR.GetRegID());
	auto regA = WReg(imlInstruction->op_compare.regA.GetRegID());
	auto tempReg = WReg(TEMP_REGISTER_ID);
	sint32 imm = imlInstruction->op_compare_s32.immS32;
	auto cond = ImlCondToArm64Cond(imlInstruction->op_compare.cond);
	aarch64GenContext->mov(tempReg, imm);
	aarch64GenContext->cmp(regA, tempReg);
	aarch64GenContext->cset(regR, cond);
	return true;
}

void PPCRecompilerAArch64Gen_imlInstruction_cjump2(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction, IMLSegment* imlSegment)
{
	if (imlSegment->nextSegmentBranchTaken == aarch64GenContext->currentSegment)
		cemu_assert_suspicious();
	Label& label = aarch64GenContext->labels.at(imlSegment->nextSegmentBranchTaken);
	auto regBool = WReg(imlInstruction->op_conditional_jump.registerBool.GetRegID());
	Label skipJump;
	if (imlInstruction->op_conditional_jump.mustBeTrue)
		aarch64GenContext->cbz(regBool, skipJump);
	else
		aarch64GenContext->cbnz(regBool, skipJump);
	aarch64GenContext->b(label);
	aarch64GenContext->L(skipJump);
}

void PPCRecompilerAArch64Gen_imlInstruction_jump2(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction, IMLSegment* imlSegment)
{
	Label& label = aarch64GenContext->labels.at(imlSegment->nextSegmentBranchTaken);
	aarch64GenContext->b(label);
}

bool PPCRecompilerAArch64Gen_imlInstruction_conditionalJumpCycleCheck(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	Label positiveRegCycles;
	Label& label = aarch64GenContext->labels.at(aarch64GenContext->currentSegment->nextSegmentBranchTaken);
	WReg regCycles = WReg(TEMP_REGISTER_ID);
	Label skipJump;
	aarch64GenContext->ldr(regCycles, AdrImm(XReg(HCPU_REG_ID), offsetof(PPCInterpreter_t, remainingCycles)));
	aarch64GenContext->tbz(regCycles, 31, skipJump);
	aarch64GenContext->b(label);
	aarch64GenContext->L(skipJump);
	return true;
}

void ATTR_MS_ABI PPCRecompiler_getTBL(PPCInterpreter_t* hCPU, uint32 gprIndex)
{
	uint64 coreTime = coreinit::coreinit_getTimerTick();
	hCPU->gpr[gprIndex] = (uint32)(coreTime & 0xFFFFFFFF);
}

void ATTR_MS_ABI PPCRecompiler_getTBU(PPCInterpreter_t* hCPU, uint32 gprIndex)
{
	uint64 coreTime = coreinit::coreinit_getTimerTick();
	hCPU->gpr[gprIndex] = (uint32)((coreTime >> 32) & 0xFFFFFFFF);
}

void* ATTR_MS_ABI PPCRecompiler_virtualHLE(PPCInterpreter_t* hCPU, uint32 hleFuncId)
{
	void* prevRSPTemp = hCPU->rspTemp;
	if (hleFuncId == 0xFFD0)
	{
		hCPU->remainingCycles -= 500; // let subtract about 500 cycles for each HLE call
		hCPU->gpr[3] = 0;
		PPCInterpreter_nextInstruction(hCPU);
		return PPCInterpreter_getCurrentInstance();
	}
	else
	{
		auto hleCall = PPCInterpreter_getHLECall(hleFuncId);
		cemu_assert(hleCall != nullptr);
		hleCall(hCPU);
	}
	hCPU->rspTemp = prevRSPTemp;
	return PPCInterpreter_getCurrentInstance();
}

bool PPCRecompilerAArch64Gen_imlInstruction_macro(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	if (imlInstruction->operation == PPCREC_IML_MACRO_B_TO_REG)
	{
		auto branchDstRegId = imlInstruction->op_macro.paramReg.GetRegID();
		auto branchDstReg = XReg(branchDstRegId);
		aarch64GenContext->mov(aarch64GenContext->x1, branchDstReg);
		auto tempReg = XReg(TEMP_REGISTER_ID);

		aarch64GenContext->mov(tempReg, offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		aarch64GenContext->add(tempReg, tempReg, branchDstReg);
		aarch64GenContext->add(tempReg, tempReg, branchDstReg);
		aarch64GenContext->add(tempReg, tempReg, XReg(PPC_RECOMPILER_INSTANCE_DATA_REG_ID));
		aarch64GenContext->ldr(tempReg, AdrNoOfs(tempReg));
		aarch64GenContext->br(tempReg);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_BL)
	{
		uint32 newLR = imlInstruction->op_macro.param + 4;
		auto tempReg32 = WReg(TEMP_REGISTER_ID);
		aarch64GenContext->mov(tempReg32, newLR);
		aarch64GenContext->str(tempReg32, AdrImm(XReg(HCPU_REG_ID), offsetof(PPCInterpreter_t, spr.LR)));

		uint32 newIP = imlInstruction->op_macro.param2;
		auto tempReg64 = XReg(TEMP_REGISTER_ID);
		uint64 lookupOffset = (uint64)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable) + (uint64)newIP * 2ULL;
		aarch64GenContext->mov(aarch64GenContext->w1, newIP);
		aarch64GenContext->mov(tempReg64, lookupOffset);
		aarch64GenContext->add(tempReg64, tempReg64, XReg(PPC_RECOMPILER_INSTANCE_DATA_REG_ID));
		aarch64GenContext->ldr(tempReg64, AdrNoOfs(tempReg64));
		aarch64GenContext->br(tempReg64);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_B_FAR)
	{
		uint32 newIP = imlInstruction->op_macro.param2;
		auto tempReg = XReg(TEMP_REGISTER_ID);
		uint64 lookupOffset = (uint64)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable) + (uint64)newIP * 2ULL;
		aarch64GenContext->mov(aarch64GenContext->w1, newIP);
		aarch64GenContext->mov(tempReg, lookupOffset);
		aarch64GenContext->add(tempReg, tempReg, XReg(PPC_RECOMPILER_INSTANCE_DATA_REG_ID));
		aarch64GenContext->ldr(tempReg, AdrNoOfs(tempReg));
		aarch64GenContext->br(tempReg);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_LEAVE)
	{
		uint32 currentInstructionAddress = imlInstruction->op_macro.param;
		auto tempReg = XReg(TEMP_REGISTER_ID);
		uint32 newIP = 0; // special value for recompiler exit
		uint64 lookupOffset = (uint64)offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable) + (uint64)newIP * 2ULL;
		aarch64GenContext->mov(aarch64GenContext->w1, currentInstructionAddress);
		aarch64GenContext->mov(tempReg, lookupOffset);
		aarch64GenContext->add(tempReg, tempReg, XReg(PPC_RECOMPILER_INSTANCE_DATA_REG_ID));
		aarch64GenContext->ldr(tempReg, AdrNoOfs(tempReg));
		aarch64GenContext->br(tempReg);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_DEBUGBREAK)
	{
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_COUNT_CYCLES)
	{
		uint32 cycleCount = imlInstruction->op_macro.param;
		auto tempReg = WReg(TEMP_REGISTER_ID);
		auto adrCycles = AdrImm(XReg(HCPU_REG_ID), offsetof(PPCInterpreter_t, remainingCycles));
		aarch64GenContext->ldr(tempReg, adrCycles);
		aarch64GenContext->sub(tempReg, tempReg, cycleCount);
		aarch64GenContext->str(tempReg, adrCycles);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_HLE)
	{
		uint32 ppcAddress = imlInstruction->op_macro.param;
		uint32 funcId = imlInstruction->op_macro.param2;
		Label cyclesLeftLabel;
		auto tempReg32 = WReg(TEMP_REGISTER_ID);
		auto tempReg64 = XReg(TEMP_REGISTER_ID);
		auto temp2Reg64 = XReg(TEMP_REGISTER_2_ID);
		auto temp2Reg32 = WReg(TEMP_REGISTER_2_ID);
		auto hcpuReg = XReg(HCPU_REG_ID);
		auto spReg = XReg(SP_IDX);
		auto ppcRecReg = XReg(PPC_RECOMPILER_INSTANCE_DATA_REG_ID);
		// update instruction pointer
		aarch64GenContext->mov(tempReg32, ppcAddress);
		aarch64GenContext->str(tempReg32, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, instructionPointer)));
		// set parameters
		aarch64GenContext->sub(spReg, spReg, 32);
		aarch64GenContext->str(aarch64GenContext->x30, AdrNoOfs(spReg));
		aarch64GenContext->str(aarch64GenContext->x0, AdrImm(spReg, 8));
		aarch64GenContext->str(aarch64GenContext->x1, AdrImm(spReg, 16));

		aarch64GenContext->mov(aarch64GenContext->x0, hcpuReg);
		aarch64GenContext->mov(aarch64GenContext->w1, funcId);
		// call HLE function

		aarch64GenContext->mov(tempReg64, (uint64)PPCRecompiler_virtualHLE);
		aarch64GenContext->blr(tempReg64);

		aarch64GenContext->mov(hcpuReg, aarch64GenContext->x0);

		aarch64GenContext->ldr(aarch64GenContext->x30, AdrNoOfs(spReg));
		aarch64GenContext->ldr(aarch64GenContext->x0, AdrImm(spReg, 8));
		aarch64GenContext->ldr(aarch64GenContext->x1, AdrImm(spReg, 16));
		aarch64GenContext->add(spReg, spReg, 32);

		// MOV R15, ppcRecompilerInstanceData
		aarch64GenContext->mov(ppcRecReg, (uint64)ppcRecompilerInstanceData);
		// MOV R13, memory_base
		aarch64GenContext->mov(XReg(MEMORY_BASE_REG_ID), (uint64)memory_base);
		// check if cycles where decreased beyond zero, if yes -> leave recompiler
		aarch64GenContext->ldr(tempReg32, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, remainingCycles)));
		aarch64GenContext->cmp(tempReg32, 0); // check if negative
		aarch64GenContext->bgt(cyclesLeftLabel);

		aarch64GenContext->ldr(aarch64GenContext->w1, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, instructionPointer)));
		aarch64GenContext->mov(tempReg64, offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		aarch64GenContext->add(tempReg64, tempReg64, ppcRecReg);
		aarch64GenContext->ldr(tempReg64, AdrNoOfs(tempReg64));
		// JMP [recompilerCallTable+EAX/4*8]
		aarch64GenContext->br(tempReg64);

		aarch64GenContext->L(cyclesLeftLabel);
		// check if instruction pointer was changed
		// assign new instruction pointer to EAX
		aarch64GenContext->ldr(temp2Reg32, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, instructionPointer)));
		aarch64GenContext->mov(tempReg64, offsetof(PPCRecompilerInstanceData_t, ppcRecompilerDirectJumpTable));
		// remember instruction pointer in REG_EDX
		aarch64GenContext->mov(aarch64GenContext->w1, temp2Reg32);
		// EAX *= 2
		aarch64GenContext->add(tempReg64, tempReg64, temp2Reg64);
		aarch64GenContext->add(tempReg64, tempReg64, temp2Reg64);
		// ADD RAX, R15 (R15 -> Pointer to ppcRecompilerInstanceData
		aarch64GenContext->add(tempReg64, tempReg64, ppcRecReg);
		aarch64GenContext->ldr(tempReg64, AdrNoOfs(tempReg64));
		// JMP [ppcRecompilerDirectJumpTable+RAX/4*8]
		aarch64GenContext->br(tempReg64);
		return true;
	}
	else if (imlInstruction->operation == PPCREC_IML_MACRO_MFTB)
	{
		// according to MS ABI the caller needs to save:
		// RAX, RCX, RDX, R8, R9, R10, R11

		uint32 ppcAddress = imlInstruction->op_macro.param;
		uint32 sprId = imlInstruction->op_macro.param2 & 0xFFFF;
		uint32 gprIndex = (imlInstruction->op_macro.param2 >> 16) & 0x1F;
		auto tempReg64 = XReg(TEMP_REGISTER_ID);
		auto tempReg32 = WReg(TEMP_REGISTER_ID);
		auto hcpuReg = XReg(HCPU_REG_ID);
		auto spReg = XReg(SP_IDX);

		// update instruction pointer
		aarch64GenContext->mov(tempReg32, ppcAddress);
		aarch64GenContext->str(tempReg32, AdrImm(hcpuReg, offsetof(PPCInterpreter_t, instructionPointer)));
		// set parameters
		aarch64GenContext->mov(aarch64GenContext->x0, hcpuReg);
		aarch64GenContext->mov(aarch64GenContext->x1, gprIndex);
		// call function
		if (sprId == SPR_TBL)
			aarch64GenContext->mov(tempReg64, (uint64)PPCRecompiler_getTBL);
		else if (sprId == SPR_TBU)
			aarch64GenContext->mov(tempReg64, (uint64)PPCRecompiler_getTBU);
		else
			cemu_assert_suspicious();

		aarch64GenContext->sub(spReg, spReg, 16);
		aarch64GenContext->str(aarch64GenContext->x30, AdrNoOfs(spReg));
		aarch64GenContext->blr(tempReg64);
		aarch64GenContext->ldr(aarch64GenContext->x30, AdrNoOfs(spReg));
		aarch64GenContext->add(spReg, spReg, 16);

		aarch64GenContext->mov(hcpuReg, aarch64GenContext->x0);
		aarch64GenContext->mov(XReg(PPC_RECOMPILER_INSTANCE_DATA_REG_ID), (uint64)ppcRecompilerInstanceData);
		aarch64GenContext->mov(XReg(MEMORY_BASE_REG_ID), (uint64)memory_base);

		return true;
	}
	else
	{
		debug_printf("Unknown recompiler macro operation %d\n", imlInstruction->operation);
		assert_dbg();
	}
	return false;
}

bool PPCRecompilerAArch64Gen_imlInstruction_load(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction, bool indexed)
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

	auto memoryBaseReg = XReg(MEMORY_BASE_REG_ID);
	auto memReg = WReg(memRegId);
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	auto dataReg32 = WReg(dataRegId);

	aarch64GenContext->mov(tempReg32, memOffset);
	aarch64GenContext->add(tempReg32, tempReg32, memReg);
	if (indexed)
		aarch64GenContext->add(tempReg32, tempReg32, WReg(memOffsetRegId));

	auto adr = AdrExt(memoryBaseReg, tempReg32, ExtMod::UXTW);
	if (imlInstruction->op_storeLoad.copyWidth == 32)
	{
		aarch64GenContext->ldr(dataReg32, adr);
		if (switchEndian)
			aarch64GenContext->rev(dataReg32, dataReg32);
	}
	else if (imlInstruction->op_storeLoad.copyWidth == 16)
	{
		if (switchEndian)
		{
			aarch64GenContext->ldrh(dataReg32, adr);
			aarch64GenContext->rev(dataReg32, dataReg32);
			if (signExtend)
				aarch64GenContext->asr(dataReg32, dataReg32, 16);
			else
				aarch64GenContext->lsr(dataReg32, dataReg32, 16);
		}
		else
		{
			if (signExtend)
				aarch64GenContext->ldrsh(dataReg32, adr);
			else
				aarch64GenContext->ldrh(dataReg32, adr);
		}
	}
	else if (imlInstruction->op_storeLoad.copyWidth == 8)
	{
		if (signExtend)
			aarch64GenContext->ldrsb(dataReg32, adr);
		else
			aarch64GenContext->ldrb(dataReg32, adr);
	}
	else
	{
		return false;
	}
	return true;
}

bool PPCRecompilerAArch64Gen_imlInstruction_store(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction, bool indexed)
{
	cemu_assert_debug(imlInstruction->op_storeLoad.registerData.GetRegFormat() == IMLRegFormat::I32);
	cemu_assert_debug(imlInstruction->op_storeLoad.registerMem.GetRegFormat() == IMLRegFormat::I32);
	if (indexed)
		cemu_assert_debug(imlInstruction->op_storeLoad.registerMem2.GetRegFormat() == IMLRegFormat::I32);

	IMLRegID dataRegId = imlInstruction->op_storeLoad.registerData.GetRegID();
	IMLRegID memRegId = imlInstruction->op_storeLoad.registerMem.GetRegID();
	IMLRegID memOffsetRegId = indexed ? imlInstruction->op_storeLoad.registerMem2.GetRegID() : 0;

	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	auto temp2Reg32 = WReg(TEMP_REGISTER_2_ID);
	auto dataReg = WReg(dataRegId);
	auto memReg = WReg(memRegId);
	sint32 memOffset = imlInstruction->op_storeLoad.immS32;
	bool swapEndian = imlInstruction->op_storeLoad.flags2.swapEndian;

	aarch64GenContext->mov(tempReg32, memOffset);
	aarch64GenContext->add(tempReg32, tempReg32, memReg);
	if (indexed)
		aarch64GenContext->add(tempReg32, tempReg32, WReg(memOffsetRegId));
	auto adr = AdrExt(XReg(MEMORY_BASE_REG_ID), tempReg32, ExtMod::UXTW);
	if (imlInstruction->op_storeLoad.copyWidth == 32)
	{
		if (swapEndian)
		{
			aarch64GenContext->rev(temp2Reg32, dataReg);
			aarch64GenContext->str(temp2Reg32, adr);
		}
		else
		{
			aarch64GenContext->str(dataReg, adr);
		}
	}
	else if (imlInstruction->op_storeLoad.copyWidth == 16)
	{
		if (swapEndian)
		{
			aarch64GenContext->rev(temp2Reg32, dataReg);
			aarch64GenContext->lsr(temp2Reg32, temp2Reg32, 16);
			aarch64GenContext->strh(temp2Reg32, adr);
		}
		else
		{
			aarch64GenContext->strh(dataReg, adr);
		}
	}
	else if (imlInstruction->op_storeLoad.copyWidth == 8)
	{
		aarch64GenContext->strb(dataReg, adr);
	}
	else
	{
		return false;
	}
	return true;
}

void PPCRecompilerAArch64Gen_imlInstruction_atomic_cmp_store(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto outReg = WReg(imlInstruction->op_atomic_compare_store.regBoolOut.GetRegID());
	auto eaReg = XReg(imlInstruction->op_atomic_compare_store.regEA.GetRegID());
	auto valReg = WReg(imlInstruction->op_atomic_compare_store.regWriteValue.GetRegID());
	auto cmpValReg = WReg(imlInstruction->op_atomic_compare_store.regCompareValue.GetRegID());
	auto tempReg64 = XReg(TEMP_REGISTER_ID);
	auto temp2Reg32 = WReg(TEMP_REGISTER_2_ID);

	Label endCmpStore;
	Label notEqual;
	Label storeFailed;

	aarch64GenContext->add(tempReg64, XReg(MEMORY_BASE_REG_ID), eaReg);
	aarch64GenContext->ldxr(temp2Reg32, AdrNoOfs(tempReg64));
	aarch64GenContext->cmp(temp2Reg32, cmpValReg);
	aarch64GenContext->bne(notEqual);
	aarch64GenContext->L(storeFailed);
	aarch64GenContext->stlxr(temp2Reg32, valReg, AdrNoOfs(tempReg64));
	aarch64GenContext->cbnz(temp2Reg32, storeFailed);
	aarch64GenContext->mov(outReg, 1);
	aarch64GenContext->b(endCmpStore);

	aarch64GenContext->L(notEqual);
	aarch64GenContext->mov(outReg, 0);
	aarch64GenContext->L(endCmpStore);
}

void PPCRecompilerAArch64Gen_imlInstr_gqr_generateScaleCode(ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, VReg& dataReg, bool isLoad, bool scalePS1, IMLReg registerGQR)
{
	auto tempReg64 = XReg(TEMP_REGISTER_ID);
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	auto tempReg64_2 = XReg(TEMP_REGISTER_2_ID);
	auto gqrReg = XReg(registerGQR.GetRegID());
	auto recDataReg = XReg(PPC_RECOMPILER_INSTANCE_DATA_REG_ID);
	auto tempVectorReg = VReg(TEMP_VECTOR_REGISTER_2_ID);
	// load GQR
	aarch64GenContext->mov(tempReg64, gqrReg);
	// extract scale field and multiply by 16 to get array offset
	aarch64GenContext->lsr(tempReg32, tempReg32, (isLoad ? 16 : 0) + 8 - 4);
	aarch64GenContext->and_(tempReg32, tempReg32, (0x3F << 4));
	// multiply dataReg by scale
	aarch64GenContext->add(tempReg64, tempReg64, recDataReg);
	if (isLoad)
	{
		if (scalePS1)
			aarch64GenContext->mov(tempReg64_2, offsetof(PPCRecompilerInstanceData_t, _psq_ld_scale_ps0_ps1));
		else
			aarch64GenContext->mov(tempReg64_2, offsetof(PPCRecompilerInstanceData_t, _psq_ld_scale_ps0_1));
	}
	else
	{
		if (scalePS1)
			aarch64GenContext->mov(tempReg64_2, offsetof(PPCRecompilerInstanceData_t, _psq_st_scale_ps0_ps1));
		else
			aarch64GenContext->mov(tempReg64_2, offsetof(PPCRecompilerInstanceData_t, _psq_st_scale_ps0_1));
	}
	aarch64GenContext->add(tempReg64, tempReg64, tempReg64_2);
	aarch64GenContext->ld1(tempVectorReg.d2, AdrNoOfs(tempReg64));
	aarch64GenContext->fmul(dataReg.d2, dataReg.d2, tempVectorReg.d2);
}

// generate code for PSQ load for a particular type
// if scaleGQR is -1 then a scale of 1.0 is assumed (no scale)
void PPCRecompilerAArch64Gen_imlInstr_psq_load(ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, uint8 mode, VReg& dataReg, WReg& memReg, WReg& indexReg, sint32 memImmS32, bool indexed, IMLReg registerGQR = IMLREG_INVALID)
{
	auto memBaseReg = XReg(MEMORY_BASE_REG_ID);
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	auto tempReg64 = XReg(TEMP_REGISTER_ID);
	if (mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1)
	{
		aarch64GenContext->mov(tempReg32, memImmS32);
		aarch64GenContext->add(tempReg32, tempReg32, memReg);
		if (indexed)
			cemu_assert_suspicious();
		aarch64GenContext->ldr(tempReg64, AdrExt(memBaseReg, tempReg32, ExtMod::UXTW));
		aarch64GenContext->rev(tempReg64, tempReg64);
		aarch64GenContext->ror(tempReg64, tempReg64, 32); // swap upper and lower DWORD
		aarch64GenContext->mov(dataReg.d[0], tempReg64);
		aarch64GenContext->fcvtl(dataReg.d2, dataReg.s2);
		// note: floats are not scaled
	}
	else if (mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0)
	{
		aarch64GenContext->mov(tempReg32, memImmS32);
		aarch64GenContext->add(tempReg32, tempReg32, memReg);
		if (indexed)
			cemu_assert_suspicious();
		aarch64GenContext->ldr(tempReg32, AdrExt(memBaseReg, tempReg32, ExtMod::UXTW));
		aarch64GenContext->rev(tempReg32, tempReg32);
		aarch64GenContext->mov(dataReg.s[0], tempReg32);
		aarch64GenContext->fcvtl(dataReg.d2, dataReg.s2);

		// load constant 1.0 to temp register
		aarch64GenContext->mov(tempReg64, DOUBLE_1_0);
		// overwrite lower half with single from memory
		aarch64GenContext->mov(dataReg.d[1], tempReg64);
		// note: floats are not scaled
	}
	else
	{
		sint32 readSize;
		bool isSigned = false;
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
				aarch64GenContext->mov(tempReg64, 1);
				aarch64GenContext->mov(dataReg.d[wordIndex], tempReg64);
			}
			else
			{
				sint32 memOffset = memImmS32 + wordIndex * (readSize / 8);
				aarch64GenContext->mov(tempReg32, memOffset);
				aarch64GenContext->add(tempReg32, tempReg32, memReg);
				auto adr = AdrExt(memBaseReg, tempReg32, ExtMod::UXTW);
				if (readSize == 16)
				{
					// half word
					aarch64GenContext->ldrh(tempReg32, adr);
					aarch64GenContext->rev(tempReg32, tempReg32); // endian swap
					aarch64GenContext->lsr(tempReg32, tempReg32, 16);
					if (isSigned)
						aarch64GenContext->sxth(tempReg64, tempReg32);
				}
				else
				{
					// byte
					if (isSigned)
						aarch64GenContext->ldrsb(tempReg64, adr);
					else
						aarch64GenContext->ldrb(tempReg32, adr);
				}
				// store
				aarch64GenContext->mov(dataReg.d[wordIndex], tempReg64);
			}
		}
		// convert the two integers to doubles
		aarch64GenContext->scvtf(dataReg.d2, dataReg.d2);
		// scale
		if (registerGQR.IsValid())
			PPCRecompilerAArch64Gen_imlInstr_gqr_generateScaleCode(ppcImlGenContext, aarch64GenContext, dataReg, true, loadPS1, registerGQR);
	}
}

void PPCRecompilerAArch64Gen_imlInstr_psq_load_generic(ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, uint8 mode, VReg& dataReg, WReg& memReg, WReg& indexReg, sint32 memImmS32, bool indexed, IMLReg registerGQR)
{
	bool loadPS1 = (mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1);
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	Label u8FormatLabel, u16FormatLabel, s8FormatLabel, s16FormatLabel, casesEndLabel;

	// load GQR & extract load type field
	aarch64GenContext->lsr(tempReg32, WReg(registerGQR.GetRegID()), 16);
	aarch64GenContext->and_(tempReg32, tempReg32, 7);

	// jump cases
	aarch64GenContext->cmp(tempReg32, 4); // type 4 -> u8
	aarch64GenContext->beq(u8FormatLabel);

	aarch64GenContext->cmp(tempReg32, 5); // type 5 -> u16
	aarch64GenContext->beq(u16FormatLabel);

	aarch64GenContext->cmp(tempReg32, 6); // type 6 -> s8
	aarch64GenContext->beq(s8FormatLabel);

	aarch64GenContext->cmp(tempReg32, 7); // type 7 -> s16
	aarch64GenContext->beq(s16FormatLabel);

	// default case -> float

	// generate cases
	PPCRecompilerAArch64Gen_imlInstr_psq_load(ppcImlGenContext, aarch64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0, dataReg, memReg, indexReg, memImmS32, indexed, registerGQR);
	aarch64GenContext->b(casesEndLabel);

	aarch64GenContext->L(u16FormatLabel);
	PPCRecompilerAArch64Gen_imlInstr_psq_load(ppcImlGenContext, aarch64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_U16_PS0, dataReg, memReg, indexReg, memImmS32, indexed, registerGQR);
	aarch64GenContext->b(casesEndLabel);

	aarch64GenContext->L(s16FormatLabel);
	PPCRecompilerAArch64Gen_imlInstr_psq_load(ppcImlGenContext, aarch64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_S16_PS0, dataReg, memReg, indexReg, memImmS32, indexed, registerGQR);
	aarch64GenContext->b(casesEndLabel);

	aarch64GenContext->L(u8FormatLabel);
	PPCRecompilerAArch64Gen_imlInstr_psq_load(ppcImlGenContext, aarch64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_U8_PS0, dataReg, memReg, indexReg, memImmS32, indexed, registerGQR);
	aarch64GenContext->b(casesEndLabel);

	aarch64GenContext->L(s8FormatLabel);
	PPCRecompilerAArch64Gen_imlInstr_psq_load(ppcImlGenContext, aarch64GenContext, loadPS1 ? PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_S8_PS0, dataReg, memReg, indexReg, memImmS32, indexed, registerGQR);

	aarch64GenContext->L(casesEndLabel);
}

bool PPCRecompilerAArch64Gen_imlInstruction_fpr_load(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction, bool indexed)
{
	auto dataReg = VReg(imlInstruction->op_storeLoad.registerData.GetRegID());
	auto realRegisterMem = WReg(imlInstruction->op_storeLoad.registerMem.GetRegID());
	auto realRegisterMem2 = indexed ? WReg(imlInstruction->op_storeLoad.registerMem2.GetRegID()) : aarch64GenContext->wzr;
	auto memBaseReg = XReg(MEMORY_BASE_REG_ID);
	auto tempReg64 = XReg(TEMP_REGISTER_ID);
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	sint32 adrOffset = imlInstruction->op_storeLoad.immS32;
	uint8 mode = imlInstruction->op_storeLoad.mode;

	if (mode == PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1)
	{
		aarch64GenContext->mov(tempReg32, adrOffset);
		aarch64GenContext->add(tempReg32, tempReg32, realRegisterMem);
		if (indexed)
			aarch64GenContext->add(tempReg32, tempReg32, realRegisterMem2);
		aarch64GenContext->ldr(tempReg32, AdrExt(memBaseReg, tempReg32, ExtMod::UXTW));
		aarch64GenContext->rev(tempReg32, tempReg32);
		aarch64GenContext->mov(dataReg.s[0], tempReg32);

		if (imlInstruction->op_storeLoad.flags2.notExpanded)
		{
			// leave value as single
		}
		else
		{
			aarch64GenContext->fcvtl(dataReg.d2, dataReg.s2);
			aarch64GenContext->mov(dataReg.d[1], dataReg.d[0]);
		}
	}
	else if (mode == PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0)
	{
		aarch64GenContext->mov(tempReg32, adrOffset);
		aarch64GenContext->add(tempReg32, tempReg32, realRegisterMem);
		if (indexed)
			aarch64GenContext->add(tempReg32, tempReg32, realRegisterMem2);
		aarch64GenContext->ldr(tempReg64, AdrExt(memBaseReg, tempReg32, ExtMod::UXTW));
		aarch64GenContext->rev(tempReg64, tempReg64);
		aarch64GenContext->mov(dataReg.d[0], tempReg64);
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
		PPCRecompilerAArch64Gen_imlInstr_psq_load(ppcImlGenContext, aarch64GenContext, mode, dataReg, realRegisterMem, realRegisterMem2, imlInstruction->op_storeLoad.immS32, indexed);
	}
	else if (mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1 ||
			 mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0)
	{
		PPCRecompilerAArch64Gen_imlInstr_psq_load_generic(ppcImlGenContext, aarch64GenContext, mode, dataReg, realRegisterMem, realRegisterMem2, imlInstruction->op_storeLoad.immS32, indexed, imlInstruction->op_storeLoad.registerGQR);
	}
	else
	{
		return false;
	}
	return true;
}

void PPCRecompilerAArch64Gen_imlInstr_psq_store(ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, uint8 mode, IMLRegID dataRegId, WReg& memReg, WReg& indexReg, sint32 memOffset, bool indexed, IMLReg registerGQR = IMLREG_INVALID)
{
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	auto temp2Reg64 = XReg(TEMP_REGISTER_2_ID);
	auto temp2Reg32 = WReg(TEMP_REGISTER_2_ID);
	auto dataVReg = VReg(dataRegId);
	auto dataDReg = DReg(dataRegId);
	auto tempFPVReg = VReg(TEMP_VECTOR_REGISTER_ID);
	auto tempFPSReg = SReg(TEMP_VECTOR_REGISTER_ID);
	auto memBaseReg = XReg(MEMORY_BASE_REG_ID);

	bool storePS1 = (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1 ||
					 mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 ||
					 mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 ||
					 mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1 ||
					 mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1);
	bool isFloat = mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0 || mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1;

	if (registerGQR.IsValid())
	{
		// move to temporary reg and update data reg
		aarch64GenContext->mov(tempFPVReg.b16, dataVReg.b16);
		dataVReg = tempFPVReg;
		// apply scale
		if (!isFloat)
			PPCRecompilerAArch64Gen_imlInstr_gqr_generateScaleCode(ppcImlGenContext, aarch64GenContext, dataVReg, false, storePS1, registerGQR);
	}
	if (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0)
	{
		aarch64GenContext->mov(tempReg32, memOffset);
		aarch64GenContext->add(tempReg32, tempReg32, memReg);
		if (indexed)
			aarch64GenContext->add(tempReg32, tempReg32, indexReg);
		aarch64GenContext->fcvt(tempFPSReg, dataDReg);
		aarch64GenContext->fmov(temp2Reg32, tempFPSReg);
		aarch64GenContext->rev(temp2Reg32, temp2Reg32);
		aarch64GenContext->str(temp2Reg32, AdrExt(memBaseReg, tempReg32, ExtMod::UXTW));
	}
	else if (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1)
	{
		aarch64GenContext->mov(tempReg32, memOffset);
		aarch64GenContext->add(tempReg32, tempReg32, memReg);
		if (indexed)
			aarch64GenContext->add(tempReg32, tempReg32, indexReg);
		aarch64GenContext->fcvtn(tempFPVReg.s2, dataVReg.d2);
		aarch64GenContext->mov(temp2Reg64, tempFPVReg.d[0]);
		aarch64GenContext->ror(temp2Reg64, temp2Reg64, 32); // swap upper and lower DWORD
		aarch64GenContext->rev(temp2Reg64, temp2Reg64);
		aarch64GenContext->str(temp2Reg64, AdrExt(memBaseReg, tempReg32, ExtMod::UXTW));
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
				aarch64GenContext->mov(temp2Reg32, -128);
				aarch64GenContext->cmn(tempReg32, 128);
				aarch64GenContext->csel(temp2Reg32, tempReg32, temp2Reg32, Cond::GT);
				aarch64GenContext->mov(tempReg32, 127);
				aarch64GenContext->cmp(temp2Reg32, 127);
				aarch64GenContext->csel(temp2Reg32, temp2Reg32, tempReg32, Cond::LT);
			};
			bitWriteSize = 8;
		}
		else if (mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0 || mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1)
		{
			clamp = [&]() {
				aarch64GenContext->mov(temp2Reg32, 255);
				aarch64GenContext->bic(tempReg32, tempReg32, tempReg32, ShMod::ASR, 31);
				aarch64GenContext->cmp(tempReg32, 255);
				aarch64GenContext->csel(temp2Reg32, tempReg32, temp2Reg32, Cond::LT);
			};
			bitWriteSize = 8;
		}
		else if (mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0 || mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1)
		{
			clamp = [&]() {
				aarch64GenContext->mov(temp2Reg32, 65535);
				aarch64GenContext->bic(tempReg32, tempReg32, tempReg32, ShMod::ASR, 31);
				aarch64GenContext->cmp(tempReg32, temp2Reg32);
				aarch64GenContext->csel(temp2Reg32, tempReg32, temp2Reg32, Cond::LT);
			};
			bitWriteSize = 16;
		}
		else if (mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0 || mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1)
		{
			clamp = [&]() {
				aarch64GenContext->mov(temp2Reg32, -32768);
				aarch64GenContext->cmn(tempReg32, 8, 12);
				aarch64GenContext->csel(temp2Reg32, tempReg32, temp2Reg32, Cond::GT);
				aarch64GenContext->mov(tempReg32, 32767);
				aarch64GenContext->cmp(temp2Reg32, tempReg32);
				aarch64GenContext->csel(temp2Reg32, temp2Reg32, tempReg32, Cond::LT);
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

		aarch64GenContext->fcvtn(tempFPVReg.s2, dataVReg.d2);
		aarch64GenContext->fcvtzs(tempFPVReg.s2, tempFPVReg.s2);
		for (sint32 valueIndex = 0; valueIndex < (storePS1 ? 2 : 1); valueIndex++)
		{
			// todo - multiply by GQR scale?
			aarch64GenContext->mov(tempReg32, tempFPVReg.s[valueIndex]);
			// clamp value in tempReg32 and store in temp2Reg32
			clamp();

			if (bitWriteSize == 16)
			{
				// endian swap
				aarch64GenContext->rev(temp2Reg32, temp2Reg32);
				aarch64GenContext->lsr(temp2Reg32, temp2Reg32, 16);
			}
			sint32 address = memOffset + valueIndex * (bitWriteSize / 8);
			aarch64GenContext->mov(tempReg32, address);
			aarch64GenContext->add(tempReg32, tempReg32, memReg);
			auto adr = AdrExt(memBaseReg, tempReg32, ExtMod::UXTW);
			// write to memory
			if (bitWriteSize == 8)
				aarch64GenContext->strb(temp2Reg32, adr);
			else if (bitWriteSize == 16)
				aarch64GenContext->strh(temp2Reg32, adr);
		}
	}
}

void PPCRecompilerAArch64Gen_imlInstr_psq_store_generic(ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, uint8 mode, IMLRegID dataRegId, WReg& memReg, WReg& indexReg, sint32 memOffset, bool indexed, IMLReg registerGQR)
{
	bool storePS1 = (mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1);
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	Label u8FormatLabel, u16FormatLabel, s8FormatLabel, s16FormatLabel, casesEndLabel;
	// load GQR & extract store type field
	aarch64GenContext->and_(tempReg32, WReg(registerGQR.GetRegID()), 7);

	// jump cases
	aarch64GenContext->cmp(tempReg32, 4); // type 4 -> u8
	aarch64GenContext->beq(u8FormatLabel);

	aarch64GenContext->cmp(tempReg32, 5); // type 5 -> u16
	aarch64GenContext->beq(u16FormatLabel);

	aarch64GenContext->cmp(tempReg32, 6); // type 6 -> s8
	aarch64GenContext->beq(s8FormatLabel);

	aarch64GenContext->cmp(tempReg32, 7); // type 7 -> s16
	aarch64GenContext->beq(s16FormatLabel);

	// default case -> float

	// generate cases
	PPCRecompilerAArch64Gen_imlInstr_psq_store(ppcImlGenContext, aarch64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0, dataRegId, memReg, indexReg, memOffset, indexed, registerGQR);
	aarch64GenContext->b(casesEndLabel);

	aarch64GenContext->L(u16FormatLabel);
	PPCRecompilerAArch64Gen_imlInstr_psq_store(ppcImlGenContext, aarch64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_U16_PS0, dataRegId, memReg, indexReg, memOffset, indexed, registerGQR);
	aarch64GenContext->b(casesEndLabel);

	aarch64GenContext->L(s16FormatLabel);
	PPCRecompilerAArch64Gen_imlInstr_psq_store(ppcImlGenContext, aarch64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_S16_PS0, dataRegId, memReg, indexReg, memOffset, indexed, registerGQR);
	aarch64GenContext->b(casesEndLabel);

	aarch64GenContext->L(u8FormatLabel);
	PPCRecompilerAArch64Gen_imlInstr_psq_store(ppcImlGenContext, aarch64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_U8_PS0, dataRegId, memReg, indexReg, memOffset, indexed, registerGQR);
	aarch64GenContext->b(casesEndLabel);

	aarch64GenContext->L(s8FormatLabel);
	PPCRecompilerAArch64Gen_imlInstr_psq_store(ppcImlGenContext, aarch64GenContext, storePS1 ? PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_S8_PS0, dataRegId, memReg, indexReg, memOffset, indexed, registerGQR);

	aarch64GenContext->L(casesEndLabel);
}

// store to memory
bool PPCRecompilerAArch64Gen_imlInstruction_fpr_store(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction, bool indexed)
{
	auto dataRegId = imlInstruction->op_storeLoad.registerData.GetRegID();
	auto dataReg = VReg(imlInstruction->op_storeLoad.registerData.GetRegID());
	auto dataDReg = DReg(dataRegId);
	auto memReg = WReg(imlInstruction->op_storeLoad.registerMem.GetRegID());
	auto indexReg = indexed ? WReg(imlInstruction->op_storeLoad.registerMem2.GetRegID()) : aarch64GenContext->wzr;
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	auto temp2Reg32 = WReg(TEMP_REGISTER_2_ID);
	auto temp2Reg64 = XReg(TEMP_REGISTER_2_ID);
	auto memBaseReg = XReg(MEMORY_BASE_REG_ID);
	auto tempFPSReg = SReg(TEMP_VECTOR_REGISTER_ID);
	sint32 memOffset = imlInstruction->op_storeLoad.immS32;
	uint8 mode = imlInstruction->op_storeLoad.mode;

	if (mode == PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0)
	{
		aarch64GenContext->mov(tempReg32, memOffset);
		aarch64GenContext->add(tempReg32, tempReg32, memReg);
		if (indexed)
			aarch64GenContext->add(tempReg32, tempReg32, indexReg);
		auto adr = AdrExt(memBaseReg, tempReg32, ExtMod::UXTW);
		if (imlInstruction->op_storeLoad.flags2.notExpanded)
		{
			// value is already in single format
			aarch64GenContext->mov(temp2Reg32, dataReg.s[0]);
		}
		else
		{
			aarch64GenContext->fcvt(tempFPSReg, dataDReg);
			aarch64GenContext->fmov(temp2Reg32, tempFPSReg);
		}
		aarch64GenContext->rev(temp2Reg32, temp2Reg32);
		aarch64GenContext->str(temp2Reg32, adr);
	}
	else if (mode == PPCREC_FPR_ST_MODE_DOUBLE_FROM_PS0)
	{
		aarch64GenContext->mov(tempReg32, memOffset);
		aarch64GenContext->add(tempReg32, tempReg32, memReg);
		if (indexed)
			aarch64GenContext->add(tempReg32, tempReg32, indexReg);
		aarch64GenContext->mov(temp2Reg64, dataReg.d[0]);
		aarch64GenContext->rev(temp2Reg64, temp2Reg64);
		aarch64GenContext->str(temp2Reg64, AdrExt(memBaseReg, tempReg32, ExtMod::UXTW));
	}
	else if (mode == PPCREC_FPR_ST_MODE_UI32_FROM_PS0)
	{
		aarch64GenContext->mov(tempReg32, memOffset);
		aarch64GenContext->add(tempReg32, tempReg32, memReg);
		if (indexed)
			aarch64GenContext->add(tempReg32, tempReg32, indexReg);
		aarch64GenContext->mov(temp2Reg32, dataReg.s[0]);
		aarch64GenContext->rev(temp2Reg32, temp2Reg32);
		aarch64GenContext->str(temp2Reg32, AdrExt(memBaseReg, tempReg32, ExtMod::UXTW));
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
		PPCRecompilerAArch64Gen_imlInstr_psq_store(ppcImlGenContext, aarch64GenContext, mode, dataRegId, memReg, indexReg, imlInstruction->op_storeLoad.immS32, indexed);
	}
	else if (mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1 ||
			 mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0)
	{
		PPCRecompilerAArch64Gen_imlInstr_psq_store_generic(ppcImlGenContext, aarch64GenContext, mode, dataRegId, memReg, indexReg, imlInstruction->op_storeLoad.immS32, indexed, imlInstruction->op_storeLoad.registerGQR);
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
void PPCRecompilerAArch64Gen_imlInstruction_fpr_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regRVReg = VReg(imlInstruction->op_fpr_r_r.regR.GetRegID());
	auto regAVReg = VReg(imlInstruction->op_fpr_r_r.regA.GetRegID());
	auto regADReg = DReg(imlInstruction->op_fpr_r_r.regA.GetRegID());
	auto tempFPReg = VReg(TEMP_VECTOR_REGISTER_ID);
	auto tempReg64 = XReg(TEMP_REGISTER_ID);
	auto tempReg32 = WReg(TEMP_REGISTER_ID);
	auto temp2Reg64 = XReg(TEMP_REGISTER_2_ID);
	auto asmRoutineVReg = VReg(ASM_ROUTINE_REGISTER_ID);
	if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP)
	{
		aarch64GenContext->dup(regRVReg.d2, regAVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP)
	{
		aarch64GenContext->dup(regRVReg.d2, regAVReg.d[1]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM)
	{
		aarch64GenContext->mov(regRVReg.d[0], regAVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_TOP)
	{
		aarch64GenContext->mov(regRVReg.d[1], regAVReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED)
	{
		aarch64GenContext->mov(tempFPReg.b16, regAVReg.b16);
		aarch64GenContext->mov(regRVReg.d[0], tempFPReg.d[1]);
		aarch64GenContext->mov(regRVReg.d[1], tempFPReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_TOP)
	{
		aarch64GenContext->mov(regRVReg.d[1], regAVReg.d[1]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM)
	{
		aarch64GenContext->mov(regRVReg.d[0], regAVReg.d[1]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM)
	{
		aarch64GenContext->mov(tempFPReg.b16, regAVReg.b16);
		aarch64GenContext->fmul(tempFPReg.d2, regRVReg.d2, tempFPReg.d2);
		aarch64GenContext->mov(regRVReg.d[0], tempFPReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_PAIR)
	{
		aarch64GenContext->fmul(regRVReg.d2, regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_DIVIDE_BOTTOM)
	{
		aarch64GenContext->mov(tempFPReg.b16, regAVReg.b16);
		aarch64GenContext->fdiv(tempFPReg.d2, regRVReg.d2, tempFPReg.d2);
		aarch64GenContext->mov(regRVReg.d[0], tempFPReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_DIVIDE_PAIR)
	{
		aarch64GenContext->fdiv(regRVReg.d2, regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_BOTTOM)
	{
		aarch64GenContext->mov(tempFPReg.b16, regAVReg.b16);
		aarch64GenContext->fadd(tempFPReg.d2, regRVReg.d2, tempFPReg.d2);
		aarch64GenContext->mov(regRVReg.d[0], tempFPReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_PAIR)
	{
		aarch64GenContext->fadd(regRVReg.d2, regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_PAIR)
	{
		aarch64GenContext->fsub(regRVReg.d2, regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_BOTTOM)
	{
		aarch64GenContext->mov(tempFPReg.b16, regAVReg.b16);
		aarch64GenContext->fsub(tempFPReg.d2, regRVReg.d2, tempFPReg.d2);
		aarch64GenContext->mov(regRVReg.d[0], tempFPReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_ASSIGN)
	{
		if (imlInstruction->op_fpr_r_r.regR.GetRegID() != imlInstruction->op_fpr_r_r.regA.GetRegID())
			aarch64GenContext->mov(regRVReg.b16, regAVReg.b16);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_FCTIWZ)
	{
		aarch64GenContext->fcvtzs(tempReg32, regADReg);
		aarch64GenContext->mov(regRVReg.d[0], tempReg64);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_FRES_TO_BOTTOM_AND_TOP)
	{
		aarch64GenContext->mov(temp2Reg64, aarch64GenContext->x30);
		aarch64GenContext->mov(tempReg64, (uint64)recompiler_fres);
		aarch64GenContext->mov(asmRoutineVReg.d[0], regAVReg.d[0]);
		aarch64GenContext->blr(tempReg64);
		aarch64GenContext->dup(regRVReg.d2, asmRoutineVReg.d[0]);
		aarch64GenContext->mov(aarch64GenContext->x30, temp2Reg64);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_BOTTOM_RECIPROCAL_SQRT)
	{
		aarch64GenContext->mov(temp2Reg64, aarch64GenContext->x30);
		aarch64GenContext->mov(tempReg64, (uint64)recompiler_frsqrte);
		aarch64GenContext->mov(asmRoutineVReg.d[0], regAVReg.d[0]);
		aarch64GenContext->blr(tempReg64);
		aarch64GenContext->mov(regRVReg.d[0], asmRoutineVReg.d[0]);
		aarch64GenContext->mov(aarch64GenContext->x30, temp2Reg64);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATE_PAIR)
	{
		aarch64GenContext->fneg(regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ABS_PAIR)
	{
		aarch64GenContext->fabs(regRVReg.d2, regAVReg.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_FRES_PAIR)
	{
		aarch64GenContext->mov(temp2Reg64, aarch64GenContext->x30);
		aarch64GenContext->mov(tempReg64, (uint64)recompiler_fres);
		aarch64GenContext->mov(asmRoutineVReg.d[0], regAVReg.d[0]);
		aarch64GenContext->blr(tempReg64);
		aarch64GenContext->mov(regRVReg.d[0], asmRoutineVReg.d[0]);
		aarch64GenContext->mov(asmRoutineVReg.d[0], regAVReg.d[1]);
		aarch64GenContext->blr(tempReg64);
		aarch64GenContext->mov(regRVReg.d[1], asmRoutineVReg.d[0]);
		aarch64GenContext->mov(aarch64GenContext->x30, temp2Reg64);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_FRSQRTE_PAIR)
	{
		aarch64GenContext->mov(temp2Reg64, aarch64GenContext->x30);
		aarch64GenContext->mov(tempReg64, (uint64)recompiler_frsqrte);
		aarch64GenContext->mov(asmRoutineVReg.d[0], regAVReg.d[0]);
		aarch64GenContext->blr(tempReg64);
		aarch64GenContext->mov(regRVReg.d[0], asmRoutineVReg.d[0]);
		aarch64GenContext->mov(asmRoutineVReg.d[0], regAVReg.d[1]);
		aarch64GenContext->blr(tempReg64);
		aarch64GenContext->mov(regRVReg.d[1], asmRoutineVReg.d[0]);
		aarch64GenContext->mov(aarch64GenContext->x30, temp2Reg64);
	}
	else
	{
		cemu_assert_suspicious();
	}
}

void PPCRecompilerAArch64Gen_imlInstruction_fpr_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = VReg(imlInstruction->op_fpr_r_r_r.regR.GetRegID());
	auto regA = VReg(imlInstruction->op_fpr_r_r_r.regA.GetRegID());
	auto regB = VReg(imlInstruction->op_fpr_r_r_r.regB.GetRegID());
	auto tempFPReg = VReg(TEMP_VECTOR_REGISTER_ID);

	if (imlInstruction->operation == PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM)
	{
		aarch64GenContext->fmul(tempFPReg.d2, regA.d2, regB.d2);
		aarch64GenContext->mov(regR.d[0], tempFPReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ADD_BOTTOM)
	{
		aarch64GenContext->fadd(tempFPReg.d2, regA.d2, regB.d2);
		aarch64GenContext->mov(regR.d[0], tempFPReg.d[0]);
		aarch64GenContext->mov(regR.d[1], regA.d[1]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_PAIR)
	{
		aarch64GenContext->fsub(regR.d2, regA.d2, regB.d2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUB_BOTTOM)
	{
		aarch64GenContext->fsub(tempFPReg.d2, regA.d2, regB.d2);
		aarch64GenContext->mov(regR.d[0], tempFPReg.d[0]);
	}
	else
		assert_dbg();
}

/*
 * FPR = op (fprA, fprB, fprC)
 */
void PPCRecompilerAArch64Gen_imlInstruction_fpr_r_r_r_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = VReg(imlInstruction->op_fpr_r_r_r_r.regR.GetRegID());
	auto regA = VReg(imlInstruction->op_fpr_r_r_r_r.regA.GetRegID());
	auto regB = VReg(imlInstruction->op_fpr_r_r_r_r.regB.GetRegID());
	auto regC = VReg(imlInstruction->op_fpr_r_r_r_r.regC.GetRegID());
	auto tempFpReg = VReg(TEMP_VECTOR_REGISTER_ID);

	if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUM0)
	{
		aarch64GenContext->mov(tempFpReg.d[0], regB.d[1]);
		aarch64GenContext->fadd(tempFpReg.d2, tempFpReg.d2, regA.d2);
		aarch64GenContext->mov(tempFpReg.d[1], regC.d[1]);
		aarch64GenContext->mov(regR.b16, tempFpReg.b16);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SUM1)
	{
		aarch64GenContext->mov(tempFpReg.d[1], regA.d[0]);
		aarch64GenContext->fadd(tempFpReg.d2, tempFpReg.d2, regB.d2);
		aarch64GenContext->mov(tempFpReg.d[0], regC.d[0]);
		aarch64GenContext->mov(regR.b16, tempFpReg.b16);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SELECT_BOTTOM)
	{
		auto regADReg = DReg(imlInstruction->op_fpr_r_r_r_r.regA.GetRegID());
		auto regBDReg = DReg(imlInstruction->op_fpr_r_r_r_r.regB.GetRegID());
		auto regCDReg = DReg(imlInstruction->op_fpr_r_r_r_r.regC.GetRegID());
		auto tempFpDReg = DReg(TEMP_VECTOR_REGISTER_ID);
		aarch64GenContext->fcmp(regADReg, 0.0);
		aarch64GenContext->fcsel(tempFpDReg, regCDReg, regBDReg, Cond::GE);
		aarch64GenContext->mov(regR.d[0], tempFpReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_SELECT_PAIR)
	{
		aarch64GenContext->fcmge(tempFpReg.d2, regA.d2, 0.0);
		aarch64GenContext->bsl(tempFpReg.b16, regC.b16, regB.b16);
		aarch64GenContext->mov(regR.b16, tempFpReg.b16);
	}
	else
		assert_dbg();
}

void PPCRecompilerAArch64Gen_imlInstruction_fpr_r(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = VReg(imlInstruction->op_fpr_r.regR.GetRegID());
	auto tempFPReg = VReg(TEMP_VECTOR_REGISTER_ID);

	if (imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATE_BOTTOM)
	{
		aarch64GenContext->fneg(tempFPReg.d2, regR.d2);
		aarch64GenContext->mov(regR.d[0], tempFPReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ABS_BOTTOM)
	{
		aarch64GenContext->fabs(tempFPReg.d2, regR.d2);
		aarch64GenContext->mov(regR.d[0], tempFPReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_NEGATIVE_ABS_BOTTOM)
	{
		aarch64GenContext->fabs(tempFPReg.d2, regR.d2);
		aarch64GenContext->fneg(tempFPReg.d2, tempFPReg.d2);
		aarch64GenContext->mov(regR.d[0], tempFPReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM)
	{
		// convert to 32bit single
		aarch64GenContext->fcvtn(tempFPReg.s2, regR.d2);
		// convert back to 64bit double
		aarch64GenContext->fcvtl(tempFPReg.d2, tempFPReg.s2);
		aarch64GenContext->mov(regR.d[0], tempFPReg.d[0]);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_PAIR)
	{
		// convert to 32bit singles
		aarch64GenContext->fcvtn(regR.s2, regR.d2);
		// convert back to 64bit doubles
		aarch64GenContext->fcvtl(regR.d2, regR.s2);
	}
	else if (imlInstruction->operation == PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64)
	{
		// convert bottom to 64bit double
		aarch64GenContext->fcvtl(regR.d2, regR.s2);
		// copy to top half
		aarch64GenContext->dup(regR.d2, regR.d[0]);
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

void PPCRecompilerAArch64Gen_imlInstruction_fpr_compare(PPCRecFunction_t* PPCRecFunction, ppcImlGenContext_t* ppcImlGenContext, AArch64GenContext_t* aarch64GenContext, IMLInstruction* imlInstruction)
{
	auto regR = XReg(imlInstruction->op_fpr_compare.regR.GetRegID());
	auto regA = DReg(imlInstruction->op_fpr_compare.regA.GetRegID());
	auto regB = DReg(imlInstruction->op_fpr_compare.regB.GetRegID());
	auto cond = ImlFPCondToArm64Cond(imlInstruction->op_fpr_compare.cond);
	aarch64GenContext->fcmp(regA, regB);
	aarch64GenContext->cset(regR, cond);
}

uint8* codeMemoryBlock = nullptr;
sint32 codeMemoryBlockIndex = 0;
sint32 codeMemoryBlockSize = 0;

std::mutex mtx_allocExecutableMemory;

uint8* PPCRecompilerX86_allocateExecutableMemory(sint32 size)
{
	std::lock_guard<std::mutex> lck(mtx_allocExecutableMemory);
	if (codeMemoryBlockIndex + size > codeMemoryBlockSize)
	{
		// allocate new block
		codeMemoryBlockSize = std::max(1024 * 1024 * 4, size + 1024); // 4MB (or more if the function is larger than 4MB)
		codeMemoryBlockIndex = 0;
		codeMemoryBlock = (uint8*)MemMapper::AllocateMemory(nullptr, codeMemoryBlockSize, MemMapper::PAGE_PERMISSION::P_RWX);
	}
	uint8* codeMem = codeMemoryBlock + codeMemoryBlockIndex;
	codeMemoryBlockIndex += size;
	// pad to 4 byte alignment
	while (codeMemoryBlockIndex & 3)
	{
		codeMemoryBlock[codeMemoryBlockIndex] = 0x90;
		codeMemoryBlockIndex++;
	}
	return codeMem;
}

bool PPCRecompiler_generateAArch64Code(struct PPCRecFunction_t* PPCRecFunction, struct ppcImlGenContext_t* ppcImlGenContext)
{
	AArch64GenContext_t aarch64GenContext{};
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		if (segIt->nextSegmentBranchTaken != nullptr)
		{
			aarch64GenContext.labels[segIt->nextSegmentBranchTaken] = Label();
		}
	}
	// generate iml instruction code
	bool codeGenerationFailed = false;
	for (IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		if (codeGenerationFailed)
			break;
		aarch64GenContext.currentSegment = segIt;
		segIt->x64Offset = aarch64GenContext.getSize();

		if (auto label = aarch64GenContext.labels.find(segIt); label != aarch64GenContext.labels.end())
		{
			aarch64GenContext.L(label->second);
		}

		for (size_t i = 0; i < segIt->imlList.size(); i++)
		{
			IMLInstruction* imlInstruction = segIt->imlList.data() + i;
			if (imlInstruction->type == PPCREC_IML_TYPE_R_NAME)
			{
				PPCRecompilerAArch64Gen_imlInstruction_r_name(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_NAME_R)
			{
				PPCRecompilerAArch64Gen_imlInstruction_name_r(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_r_r(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_S32)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_r_s32(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_conditional_r_s32(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_S32)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_r_r_s32(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_S32_CARRY)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_r_r_s32_carry(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_R)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_r_r_r(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_R_R_R_CARRY)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_r_r_r_carry(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_COMPARE)
			{
				PPCRecompilerAArch64Gen_imlInstruction_compare(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_COMPARE_S32)
			{
				PPCRecompilerAArch64Gen_imlInstruction_compare_s32(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_CONDITIONAL_JUMP)
			{
				PPCRecompilerAArch64Gen_imlInstruction_cjump2(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction, segIt);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_JUMP)
			{
				PPCRecompilerAArch64Gen_imlInstruction_jump2(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction, segIt);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
			{
				PPCRecompilerAArch64Gen_imlInstruction_conditionalJumpCycleCheck(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_MACRO)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_macro(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_LOAD)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_load(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction, false))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_LOAD_INDEXED)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_load(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction, true))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_STORE)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_store(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction, false))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_STORE_INDEXED)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_store(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction, true))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_ATOMIC_CMP_STORE)
			{
				PPCRecompilerAArch64Gen_imlInstruction_atomic_cmp_store(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_NO_OP)
			{
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_fpr_load(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction, false))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_LOAD_INDEXED)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_fpr_load(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction, true))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_fpr_store(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction, false))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_STORE_INDEXED)
			{
				if (!PPCRecompilerAArch64Gen_imlInstruction_fpr_store(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction, true))
					codeGenerationFailed = true;
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R)
			{
				PPCRecompilerAArch64Gen_imlInstruction_fpr_r_r(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R)
			{
				PPCRecompilerAArch64Gen_imlInstruction_fpr_r_r_r(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R_R_R_R)
			{
				PPCRecompilerAArch64Gen_imlInstruction_fpr_r_r_r_r(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_R)
			{
				PPCRecompilerAArch64Gen_imlInstruction_fpr_r(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction);
			}
			else if (imlInstruction->type == PPCREC_IML_TYPE_FPR_COMPARE)
			{
				PPCRecompilerAArch64Gen_imlInstruction_fpr_compare(PPCRecFunction, ppcImlGenContext, &aarch64GenContext, imlInstruction);
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
		return false;
	}
	aarch64GenContext.ready(AArch64GenContext_t::PROTECT_RWE);
	// TODO: implement without PPCRecompilerX86_allocateExecutableMemory
	// allocate executable memory
	uint8* executableMemory = PPCRecompilerX86_allocateExecutableMemory(aarch64GenContext.getSize());
	memcpy(executableMemory, aarch64GenContext.getCode(), aarch64GenContext.getSize());

	// set code
	PPCRecFunction->x86Code = executableMemory;
	PPCRecFunction->x86Size = aarch64GenContext.getSize();
	return true;
}

void PPCRecompilerAArch64Gen_generateEnterRecompilerCode()
{
	AArch64GenContext_t aarch64GenContext;

	auto spReg = XReg(SP_IDX);
	Label recompilerStartLabel;
	std::vector<uint32> regIds;

	for (uint32 index = 0; index <= 30; index++)
		regIds.push_back(index);
	std::vector<uint32> fpRegIds;
	for (uint32 index = 0; index <= 31; index++)
		fpRegIds.push_back(index);
	uint64 stackSize = std::ceil((fpRegIds.size() + regIds.size()) / 2.) * 2 * 8;
	//  start of recompiler entry function
	aarch64GenContext.sub(spReg, spReg, stackSize);
	size_t spIndex = 0;
	for (auto index = 0; index < regIds.size(); index++, spIndex++)
		aarch64GenContext.str(XReg(regIds.at(index)), AdrImm(spReg, spIndex * 8));
	for (auto index = 0; index < fpRegIds.size(); index++, spIndex++)
		aarch64GenContext.str(DReg(fpRegIds.at(index)), AdrImm(spReg, spIndex * 8));

	// MOV RSP, RDX (ppc interpreter instance)
	aarch64GenContext.mov(XReg(HCPU_REG_ID), aarch64GenContext.x1); // call argument 2
	// MOV R15, ppcRecompilerInstanceData
	aarch64GenContext.mov(XReg(PPC_RECOMPILER_INSTANCE_DATA_REG_ID), (uint64)ppcRecompilerInstanceData);
	// MOV R13, memory_base
	aarch64GenContext.mov(XReg(MEMORY_BASE_REG_ID), (uint64)memory_base);

	aarch64GenContext.mov(WReg(TEMP_REGISTER_ID), 0);
	// JMP recFunc
	aarch64GenContext.blr(aarch64GenContext.x0); // call argument 1
	// recompilerExit1:
	spIndex = 0;
	for (auto index = 0; index < regIds.size(); index++, spIndex++)
		aarch64GenContext.ldr(XReg(regIds.at(index)), AdrImm(spReg, spIndex * 8));
	for (auto index = 0; index < fpRegIds.size(); index++, spIndex++)
		aarch64GenContext.ldr(DReg(fpRegIds.at(index)), AdrImm(spReg, spIndex * 8));
	aarch64GenContext.add(spReg, spReg, stackSize);
	// RET
	aarch64GenContext.ret();

	aarch64GenContext.ready(AArch64GenContext_t::PROTECT_RWE);

	uint8* executableMemory = PPCRecompilerX86_allocateExecutableMemory(aarch64GenContext.getSize());
	// copy code to executable memory
	memcpy(executableMemory, aarch64GenContext.getCode(), aarch64GenContext.getSize());

	PPCRecompiler_enterRecompilerCode = (void ATTR_MS_ABI (*)(uint64, uint64))executableMemory;
}

void* PPCRecompilerAArch64Gen_generateLeaveRecompilerCode()
{
	AArch64GenContext_t aarch64GenContext;

	// update instruction pointer
	// LR is in EDX
	aarch64GenContext.str(aarch64GenContext.w1, AdrImm(XReg(HCPU_REG_ID), offsetof(PPCInterpreter_t, instructionPointer)));
	// RET
	aarch64GenContext.ret();

	aarch64GenContext.ready(AArch64GenContext_t::PROTECT_RWE);
	uint8* executableMemory = PPCRecompilerX86_allocateExecutableMemory(aarch64GenContext.getSize());
	// copy code to executable memory
	memcpy(executableMemory, aarch64GenContext.getCode(), aarch64GenContext.getSize());
	return executableMemory;
}

void PPCRecompilerAArch64Gen_generateRecompilerInterfaceFunctions()
{
	PPCRecompilerAArch64Gen_generateEnterRecompilerCode();
	PPCRecompiler_leaveRecompilerCode_unvisited = (void ATTR_MS_ABI (*)())PPCRecompilerAArch64Gen_generateLeaveRecompilerCode();
	PPCRecompiler_leaveRecompilerCode_visited = (void ATTR_MS_ABI (*)())PPCRecompilerAArch64Gen_generateLeaveRecompilerCode();
	cemu_assert_debug(PPCRecompiler_leaveRecompilerCode_unvisited != PPCRecompiler_leaveRecompilerCode_visited);
}
