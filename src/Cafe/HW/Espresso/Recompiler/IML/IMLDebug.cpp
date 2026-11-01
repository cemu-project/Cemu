#include "IML.h"
#include "IMLInstruction.h"
#include "IMLSegment.h"
#include "IMLRegisterAllocatorRanges.h"
#include "util/helpers/StringBuf.h"

#include "../PPCRecompiler.h"

const char* IMLDebug_GetOpcodeName(const IMLInstruction* iml)
{
	static char _tempOpcodename[32];
	IMLOperation op = iml->operation;
	if (op == IMLOperation::OP_ASSIGN)
		return "MOV";
	else if (op == IMLOperation::OP_ADD)
		return "ADD";
	else if (op == IMLOperation::OP_ADD_WITH_CARRY)
		return "ADC";
	else if (op == IMLOperation::OP_SUB)
		return "SUB";
	else if (op == IMLOperation::OP_OR)
		return "OR";
	else if (op == IMLOperation::OP_AND)
		return "AND";
	else if (op == IMLOperation::OP_XOR)
		return "XOR";
	else if (op == IMLOperation::OP_LEFT_SHIFT)
		return "LSH";
	else if (op == IMLOperation::OP_RIGHT_SHIFT_U)
		return "RSH";
	else if (op == IMLOperation::OP_RIGHT_SHIFT_S)
		return "ARSH";
	else if (op == IMLOperation::OP_LEFT_ROTATE)
		return "LROT";
	else if (op == IMLOperation::OP_MULTIPLY_SIGNED)
		return "MULS";
	else if (op == IMLOperation::OP_DIVIDE_SIGNED)
		return "DIVS";
	else if (op == IMLOperation::OP_FPR_ASSIGN)
		return "FMOV";
	else if (op == IMLOperation::OP_FPR_ADD)
		return "FADD";
	else if (op == IMLOperation::OP_FPR_SUB)
		return "FSUB";
	else if (op == IMLOperation::OP_FPR_MULTIPLY)
		return "FMUL";
	else if (op == IMLOperation::OP_FPR_DIVIDE)
		return "FDIV";
	else if (op == IMLOperation::OP_FPR_EXPAND_F32_TO_F64)
		return "F32TOF64";
	else if (op == IMLOperation::OP_FPR_ABS)
		return "FABS";
	else if (op == IMLOperation::OP_FPR_NEGATE)
		return "FNEG";
	else if (op == IMLOperation::OP_FPR_NEGATIVE_ABS)
		return "FNABS";
	else if (op == IMLOperation::OP_FPR_FLOAT_TO_INT)
		return "F2I";
	else if (op == IMLOperation::OP_FPR_INT_TO_FLOAT)
		return "I2F";
	else if (op == IMLOperation::OP_FPR_BITCAST_INT_TO_FLOAT)
		return "BITMOVE";

	sprintf(_tempOpcodename, "OP0%02x_T%d", static_cast<uint32>(iml->operation), static_cast<sint32>(iml->type));
	return _tempOpcodename;
}

std::string IMLDebug_GetRegName(IMLReg r)
{
	std::string regName;
	uint32 regId = r.GetRegID();
	switch (r.GetRegFormat())
	{
	case IMLRegFormat::F32:
		regName.append("f");
		break;
	case IMLRegFormat::F64:
		regName.append("fd");
		break;
	case IMLRegFormat::I32:
		regName.append("i");
		break;
	case IMLRegFormat::I64:
		regName.append("r");
		break;
	default:
		DEBUG_BREAK;
	}
	regName.append(fmt::format("{}", regId));
	return regName;
}

void IMLDebug_AppendRegisterParam(StringBuf& strOutput, IMLReg virtualRegister, bool isLast = false)
{
	strOutput.add(IMLDebug_GetRegName(virtualRegister));
	if (!isLast)
		strOutput.add(", ");
}

void IMLDebug_AppendS32Param(StringBuf& strOutput, sint32 val, bool isLast = false)
{
	if (val < 0)
	{
		strOutput.add("-");
		val = -val;
	}
	strOutput.addFmt("0x{:08x}", val);
	if (!isLast)
		strOutput.add(", ");
}

void IMLDebug_PrintLivenessRangeInfo(StringBuf& currentLineText, IMLSegment* imlSegment, sint32 offset)
{
	// pad to 70 characters
	sint32 index = currentLineText.getLen();
	while (index < 70)
	{
		currentLineText.add(" ");
		index++;
	}
	raLivenessRange* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while (subrangeItr)
	{
		if (subrangeItr->interval.start.GetInstructionIndexEx() == offset)
		{
			if(subrangeItr->interval.start.IsInstructionIndex() && !subrangeItr->interval.start.IsOnInputEdge())
				currentLineText.add(".");
			else
				currentLineText.add("|");

			currentLineText.addFmt("{:<4}", subrangeItr->GetVirtualRegister());
		}
		else if (subrangeItr->interval.end.GetInstructionIndexEx() == offset)
		{
			if(subrangeItr->interval.end.IsInstructionIndex() && !subrangeItr->interval.end.IsOnOutputEdge())
				currentLineText.add("*    ");
			else
				currentLineText.add("|    ");
		}
		else if (subrangeItr->interval.ContainsInstructionIndexEx(offset))
		{
			currentLineText.add("|    ");
		}
		else
		{
			currentLineText.add("     ");
		}
		index += 5;
		// next
		subrangeItr = subrangeItr->link_allSegmentRanges.next;
	}
}

std::string IMLDebug_GetSegmentName(ppcImlGenContext_t* ctx, IMLSegment* seg)
{
	if (!ctx)
	{
		return "<NoNameWithoutCtx>";
	}
	// find segment index
	for (size_t i = 0; i < ctx->segmentList2.size(); i++)
	{
		if (ctx->segmentList2[i] == seg)
		{
			return fmt::format("Seg{:04x}", i);
		}
	}
	return "<SegmentNotInCtx>";
}

std::string IMLDebug_GetConditionName(IMLCondition cond)
{
	switch (cond)
	{
	case IMLCondition::EQ:
		return "EQ";
	case IMLCondition::NEQ:
		return "NEQ";
	case IMLCondition::UNSIGNED_GT:
		return "UGT";
	case IMLCondition::UNSIGNED_LT:
		return "ULT";
	case IMLCondition::SIGNED_GT:
		return "SGT";
	case IMLCondition::SIGNED_LT:
		return "SLT";
	default:
		cemu_assert_unimplemented();
	}
	return "ukn";
}

void IMLDebug_DisassembleInstruction(const IMLInstruction& inst, std::string& disassemblyLineOut)
{
	const sint32 lineOffsetParameters = 10;//18;

	StringBuf strOutput(1024);
	strOutput.reset();
	if (inst.type == IMLType::R_NAME || inst.type == IMLType::NAME_R)
	{
		if (inst.type == IMLType::R_NAME)
			strOutput.add("R_NAME");
		else
			strOutput.add("NAME_R");
		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");

		if(inst.type == IMLType::R_NAME)
			IMLDebug_AppendRegisterParam(strOutput, inst.op_r_name.regR);

		strOutput.add("name_");
		if (inst.op_r_name.name >= PPCREC_NAME_R0 && inst.op_r_name.name < (PPCREC_NAME_R0 + 999))
		{
			strOutput.addFmt("r{}", inst.op_r_name.name - PPCREC_NAME_R0);
		}
		if (inst.op_r_name.name >= PPCREC_NAME_FPR_HALF && inst.op_r_name.name < (PPCREC_NAME_FPR_HALF + 32*2))
		{
			strOutput.addFmt("f{}", inst.op_r_name.name - ((PPCREC_NAME_FPR_HALF - inst.op_r_name.name)/2));
			if ((inst.op_r_name.name-PPCREC_NAME_FPR_HALF)&1)
				strOutput.add(".ps1");
			else
				strOutput.add(".ps0");
		}
		else if (inst.op_r_name.name >= PPCREC_NAME_SPR0 && inst.op_r_name.name < (PPCREC_NAME_SPR0 + 999))
		{
			strOutput.addFmt("spr{}", inst.op_r_name.name - PPCREC_NAME_SPR0);
		}
		else if (inst.op_r_name.name >= PPCREC_NAME_CR && inst.op_r_name.name <= PPCREC_NAME_CR_LAST)
			strOutput.addFmt("cr{}", inst.op_r_name.name - PPCREC_NAME_CR);
		else if (inst.op_r_name.name == PPCREC_NAME_XER_CA)
			strOutput.add("xer.ca");
		else if (inst.op_r_name.name == PPCREC_NAME_XER_SO)
			strOutput.add("xer.so");
		else if (inst.op_r_name.name == PPCREC_NAME_XER_OV)
			strOutput.add("xer.ov");
		else if (inst.op_r_name.name == PPCREC_NAME_CPU_MEMRES_EA)
			strOutput.add("cpuReservation.ea");
		else if (inst.op_r_name.name == PPCREC_NAME_CPU_MEMRES_VAL)
			strOutput.add("cpuReservation.value");
		else
		{
			strOutput.addFmt("name_ukn{}", inst.op_r_name.name);
		}
		if (inst.type != IMLType::R_NAME)
		{
			strOutput.add(", ");
			IMLDebug_AppendRegisterParam(strOutput, inst.op_r_name.regR, true);
		}

	}
	else if (inst.type == IMLType::R_R)
	{
		strOutput.addFmt("{}", IMLDebug_GetOpcodeName(&inst));
		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r.regR);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r.regA, true);
	}
	else if (inst.type == IMLType::R_R_R)
	{
		strOutput.addFmt("{}", IMLDebug_GetOpcodeName(&inst));
		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_r.regR);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_r.regA);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_r.regB, true);
	}
	else if (inst.type == IMLType::R_R_R_CARRY)
	{
		strOutput.addFmt("{}", IMLDebug_GetOpcodeName(&inst));
		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_r_carry.regR);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_r_carry.regA);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_r_carry.regB);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_r_carry.regCarry, true);
	}
	else if (inst.type == IMLType::COMPARE)
	{
		strOutput.add("CMP ");
		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");
		IMLDebug_AppendRegisterParam(strOutput, inst.op_compare.regA);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_compare.regB);
		strOutput.addFmt("{}", IMLDebug_GetConditionName(inst.op_compare.cond));
		strOutput.add(" -> ");
		IMLDebug_AppendRegisterParam(strOutput, inst.op_compare.regR, true);
	}
	else if (inst.type == IMLType::COMPARE_S32)
	{
		strOutput.add("CMP ");
		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");
		IMLDebug_AppendRegisterParam(strOutput, inst.op_compare_s32.regA);
		strOutput.addFmt("{}", inst.op_compare_s32.immS32);
		strOutput.addFmt(", {}", IMLDebug_GetConditionName(inst.op_compare_s32.cond));
		strOutput.add(" -> ");
		IMLDebug_AppendRegisterParam(strOutput, inst.op_compare_s32.regR, true);
	}
	else if (inst.type == IMLType::CONDITIONAL_JUMP)
	{
		strOutput.add("CJUMP ");
		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");
		IMLDebug_AppendRegisterParam(strOutput, inst.op_conditional_jump.registerBool, true);
		if (!inst.op_conditional_jump.mustBeTrue)
			strOutput.add("(inverted)");
	}
	else if (inst.type == IMLType::JUMP)
	{
		strOutput.add("JUMP");
	}
	else if (inst.type == IMLType::R_R_S32)
	{
		strOutput.addFmt("{}", IMLDebug_GetOpcodeName(&inst));
		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");

		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_s32.regR);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_s32.regA);
		IMLDebug_AppendS32Param(strOutput, inst.op_r_r_s32.immS32, true);
	}
	else if (inst.type == IMLType::R_R_S32_CARRY)
	{
		strOutput.addFmt("{}", IMLDebug_GetOpcodeName(&inst));
		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");

		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_s32_carry.regR);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_s32_carry.regA);
		IMLDebug_AppendS32Param(strOutput, inst.op_r_r_s32_carry.immS32);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_s32_carry.regCarry, true);
	}
	else if (inst.type == IMLType::R_S32)
	{
		strOutput.addFmt("{}", IMLDebug_GetOpcodeName(&inst));
		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");

		IMLDebug_AppendRegisterParam(strOutput, inst.op_r_immS32.regR);
		IMLDebug_AppendS32Param(strOutput, inst.op_r_immS32.immS32, true);
	}
	else if (inst.type == IMLType::LOAD || inst.type == IMLType::STORE ||
			 inst.type == IMLType::LOAD_INDEXED || inst.type == IMLType::STORE_INDEXED)
	{
		if (inst.type == IMLType::LOAD || inst.type == IMLType::LOAD_INDEXED)
			strOutput.add("LD_");
		else
			strOutput.add("ST_");

		if (inst.op_storeLoad.flags2.signExtend)
			strOutput.add("S");
		else
			strOutput.add("U");
		strOutput.addFmt("{}", inst.op_storeLoad.copyWidth);

		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");

		IMLDebug_AppendRegisterParam(strOutput, inst.op_storeLoad.registerData);

		if (inst.type == IMLType::LOAD_INDEXED || inst.type == IMLType::STORE_INDEXED)
			strOutput.addFmt("[{}+{}]", IMLDebug_GetRegName(inst.op_storeLoad.registerMem), IMLDebug_GetRegName(inst.op_storeLoad.registerMem2));
		else
			strOutput.addFmt("[{}+{}]", IMLDebug_GetRegName(inst.op_storeLoad.registerMem), inst.op_storeLoad.immS32);
	}
	else if (inst.type == IMLType::ATOMIC_CMP_STORE)
	{
		strOutput.add("ATOMIC_ST_U32");

		while ((sint32)strOutput.getLen() < lineOffsetParameters)
			strOutput.add(" ");

		IMLDebug_AppendRegisterParam(strOutput, inst.op_atomic_compare_store.regEA);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_atomic_compare_store.regCompareValue);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_atomic_compare_store.regWriteValue);
		IMLDebug_AppendRegisterParam(strOutput, inst.op_atomic_compare_store.regBoolOut, true);
	}
	else if (inst.type == IMLType::NO_OP)
	{
		strOutput.add("NOP");
	}
	else if (inst.type == IMLType::MACRO)
	{
		if (inst.operation == IMLOperation::MACRO_B_TO_REG)
		{
			strOutput.addFmt("MACRO B_TO_REG {}", IMLDebug_GetRegName(inst.op_macro.paramReg));
		}
		else if (inst.operation == IMLOperation::MACRO_BL)
		{
			strOutput.addFmt("MACRO BL 0x{:08x} -> 0x{:08x} cycles (depr): {}", inst.op_macro.param, inst.op_macro.param2, (sint32)inst.op_macro.paramU16);
		}
		else if (inst.operation == IMLOperation::MACRO_B_FAR)
		{
			strOutput.addFmt("MACRO B_FAR 0x{:08x} -> 0x{:08x} cycles (depr): {}", inst.op_macro.param, inst.op_macro.param2, (sint32)inst.op_macro.paramU16);
		}
		else if (inst.operation == IMLOperation::MACRO_LEAVE)
		{
			strOutput.addFmt("MACRO LEAVE ppc: 0x{:08x}", inst.op_macro.param);
		}
		else if (inst.operation == IMLOperation::MACRO_HLE)
		{
			strOutput.addFmt("MACRO HLE ppcAddr: 0x{:08x} funcId: 0x{:08x}", inst.op_macro.param, inst.op_macro.param2);
		}
		else if (inst.operation == IMLOperation::MACRO_COUNT_CYCLES)
		{
			strOutput.addFmt("MACRO COUNT_CYCLES cycles: {}", inst.op_macro.param);
		}
		else
		{
			strOutput.addFmt("MACRO ukn operation {}", static_cast<uint32>(inst.operation));
		}
	}
	else if (inst.type == IMLType::FPR_LOAD)
	{
		strOutput.addFmt("{} = ", IMLDebug_GetRegName(inst.op_storeLoad.registerData));
		if (inst.op_storeLoad.flags2.signExtend)
			strOutput.add("S");
		else
			strOutput.add("U");
		strOutput.addFmt("{} [{}+{}] mode {}", inst.op_storeLoad.copyWidth / 8, IMLDebug_GetRegName(inst.op_storeLoad.registerMem), inst.op_storeLoad.immS32, inst.op_storeLoad.mode);
		if (inst.op_storeLoad.flags2.notExpanded)
		{
			strOutput.addFmt(" <No expand>");
		}
	}
	else if (inst.type == IMLType::FPR_STORE || inst.type == IMLType::FPR_STORE_INDEXED)
	{
		if (inst.op_storeLoad.flags2.signExtend)
			strOutput.add("S");
		else
			strOutput.add("U");
		if (inst.type == IMLType::FPR_STORE_INDEXED)
			strOutput.addFmt("{} [{}+{}+{}]", inst.op_storeLoad.copyWidth / 8, IMLDebug_GetRegName(inst.op_storeLoad.registerMem), IMLDebug_GetRegName(inst.op_storeLoad.registerMem2), inst.op_storeLoad.immS32);
		else
			strOutput.addFmt("{} [t{}+{}]", inst.op_storeLoad.copyWidth / 8, inst.op_storeLoad.registerMem.GetRegID(), inst.op_storeLoad.immS32);
		strOutput.addFmt(" = {} mode {}", IMLDebug_GetRegName(inst.op_storeLoad.registerData), inst.op_storeLoad.mode);
		if (inst.op_storeLoad.flags2.notExpanded)
			strOutput.add(" <No expand>");
	}
	else if (inst.type == IMLType::FPR_R)
	{
		strOutput.addFmt("{:<6} ", IMLDebug_GetOpcodeName(&inst));
		strOutput.addFmt("{}", IMLDebug_GetRegName(inst.op_fpr_r.regR));
	}
	else if (inst.type == IMLType::FPR_R_R)
	{
		strOutput.addFmt("{:<6} ", IMLDebug_GetOpcodeName(&inst));
		strOutput.addFmt("{}, {}", IMLDebug_GetRegName(inst.op_fpr_r_r.regR), IMLDebug_GetRegName(inst.op_fpr_r_r.regA));
	}
	else if (inst.type == IMLType::FPR_R_R_R_R)
	{
		strOutput.addFmt("{:<6} ", IMLDebug_GetOpcodeName(&inst));
		strOutput.addFmt("{}, {}, {}, {}", IMLDebug_GetRegName(inst.op_fpr_r_r_r_r.regR), IMLDebug_GetRegName(inst.op_fpr_r_r_r_r.regA), IMLDebug_GetRegName(inst.op_fpr_r_r_r_r.regB), IMLDebug_GetRegName(inst.op_fpr_r_r_r_r.regC));
	}
	else if (inst.type == IMLType::FPR_R_R_R)
	{
		strOutput.addFmt("{:<6} ", IMLDebug_GetOpcodeName(&inst));
		strOutput.addFmt("{}, {}, {}", IMLDebug_GetRegName(inst.op_fpr_r_r_r.regR), IMLDebug_GetRegName(inst.op_fpr_r_r_r.regA), IMLDebug_GetRegName(inst.op_fpr_r_r_r.regB));
	}
	else if (inst.type == IMLType::CJUMP_CYCLE_CHECK)
	{
		strOutput.addFmt("CYCLE_CHECK");
	}
	else if (inst.type == IMLType::X86_EFLAGS_JCC)
	{
		strOutput.addFmt("X86_JCC {}", IMLDebug_GetConditionName(inst.op_x86_eflags_jcc.cond));
	}
	else
	{
		strOutput.addFmt("Unknown iml type {}", static_cast<uint32>(inst.type));
	}
	disassemblyLineOut.assign(strOutput.c_str());
}

void IMLDebug_DumpSegment(ppcImlGenContext_t* ctx, IMLSegment* imlSegment, bool printLivenessRangeInfo)
{
	StringBuf strOutput(4096);

	strOutput.addFmt("SEGMENT {} | PPC=0x{:08x} Loop-depth {}", IMLDebug_GetSegmentName(ctx, imlSegment), imlSegment->ppcAddress, imlSegment->loopDepth);
	if (imlSegment->isEnterable)
	{
		strOutput.addFmt(" ENTERABLE (0x{:08x})", imlSegment->enterPPCAddress);
	}
	if (imlSegment->deadCodeEliminationHintSeg)
	{
		strOutput.addFmt(" InheritOverwrite: {}", IMLDebug_GetSegmentName(ctx, imlSegment->deadCodeEliminationHintSeg));
	}
	cemuLog_log(LogType::Force, "{}", strOutput.c_str());

	if (printLivenessRangeInfo)
	{
		strOutput.reset();
		IMLDebug_PrintLivenessRangeInfo(strOutput, imlSegment, RA_INTER_RANGE_START);
		cemuLog_log(LogType::Force, "{}", strOutput.c_str());
	}
	//debug_printf("\n");
	strOutput.reset();

	std::string disassemblyLine;
	for (sint32 i = 0; i < imlSegment->imlList.size(); i++)
	{
		const IMLInstruction& inst = imlSegment->imlList[i];
		// don't log NOP instructions
		if (inst.type == IMLType::NO_OP)
			continue;
		strOutput.reset();
		strOutput.addFmt("{:02x} ", i);
		//cemuLog_log(LogType::Force, "{:02x} ", i);
		disassemblyLine.clear();
		IMLDebug_DisassembleInstruction(inst, disassemblyLine);
		strOutput.add(disassemblyLine);
		if (printLivenessRangeInfo)
		{
			IMLDebug_PrintLivenessRangeInfo(strOutput, imlSegment, i);
		}
		cemuLog_log(LogType::Force, "{}", strOutput.c_str());
	}
	// all ranges
	if (printLivenessRangeInfo)
	{
		strOutput.reset();
		strOutput.add("Ranges-VirtReg                                                        ");
		raLivenessRange* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while (subrangeItr)
		{
			strOutput.addFmt("v{:<4}", (uint32)subrangeItr->GetVirtualRegister());
			subrangeItr = subrangeItr->link_allSegmentRanges.next;
		}
		cemuLog_log(LogType::Force, "{}", strOutput.c_str());
		strOutput.reset();
		strOutput.add("Ranges-PhysReg                                                        ");
		subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while (subrangeItr)
		{
			strOutput.addFmt("p{:<4}", subrangeItr->GetPhysicalRegister());
			subrangeItr = subrangeItr->link_allSegmentRanges.next;
		}
		cemuLog_log(LogType::Force, "{}", strOutput.c_str());
	}
	// branch info
	strOutput.reset();
	strOutput.add("Links from: ");
	for (sint32 i = 0; i < imlSegment->list_prevSegments.size(); i++)
	{
		if (i)
			strOutput.add(", ");
		strOutput.addFmt("{}", IMLDebug_GetSegmentName(ctx, imlSegment->list_prevSegments[i]).c_str());
	}
	cemuLog_log(LogType::Force, "{}", strOutput.c_str());
	if (imlSegment->nextSegmentBranchNotTaken)
		cemuLog_log(LogType::Force, "BranchNotTaken: {}", IMLDebug_GetSegmentName(ctx, imlSegment->nextSegmentBranchNotTaken).c_str());
	if (imlSegment->nextSegmentBranchTaken)
		cemuLog_log(LogType::Force, "BranchTaken: {}", IMLDebug_GetSegmentName(ctx, imlSegment->nextSegmentBranchTaken).c_str());
	if (imlSegment->nextSegmentIsUncertain)
		cemuLog_log(LogType::Force, "Dynamic target");
}

void IMLDebug_Dump(ppcImlGenContext_t* ppcImlGenContext, bool printLivenessRangeInfo)
{
	for (size_t i = 0; i < ppcImlGenContext->segmentList2.size(); i++)
	{
		IMLDebug_DumpSegment(ppcImlGenContext, ppcImlGenContext->segmentList2[i], printLivenessRangeInfo);
		cemuLog_log(LogType::Force, "");
	}
}
