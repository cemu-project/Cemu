#include "IML.h"
#include "IMLInstruction.h"
#include "IMLSegment.h"
#include "IMLRegisterAllocatorRanges.h"
#include "util/helpers/StringBuf.h"

#include "../PPCRecompiler.h"

const char* IMLDebug_GetOpcodeName(const IMLInstruction* iml)
{
	static char _tempOpcodename[32];
	uint32 op = iml->operation;
	if (op == PPCREC_IML_OP_ASSIGN)
		return "MOV";
	else if (op == PPCREC_IML_OP_ADD)
		return "ADD";
	else if (op == PPCREC_IML_OP_SUB)
		return "SUB";
	else if (op == PPCREC_IML_OP_ADD_CARRY_UPDATE_CARRY)
		return "ADDCSC";
	else if (op == PPCREC_IML_OP_OR)
		return "OR";
	else if (op == PPCREC_IML_OP_AND)
		return "AND";
	else if (op == PPCREC_IML_OP_XOR)
		return "XOR";
	else if (op == PPCREC_IML_OP_LEFT_SHIFT)
		return "LSH";
	else if (op == PPCREC_IML_OP_RIGHT_SHIFT)
		return "RSH";
	else if (op == PPCREC_IML_OP_MULTIPLY_SIGNED)
		return "MULS";
	else if (op == PPCREC_IML_OP_DIVIDE_SIGNED)
		return "DIVS";

	sprintf(_tempOpcodename, "OP0%02x_T%d", iml->operation, iml->type);
	return _tempOpcodename;
}

void IMLDebug_AppendRegisterParam(StringBuf& strOutput, sint32 virtualRegister, bool isLast = false)
{
	if (isLast)
	{
		if (virtualRegister < 10)
			strOutput.addFmt("t{} ", virtualRegister);
		else
			strOutput.addFmt("t{}", virtualRegister);
		return;
	}
	if (virtualRegister < 10)
		strOutput.addFmt("t{} , ", virtualRegister);
	else
		strOutput.addFmt("t{}, ", virtualRegister);
}

void IMLDebug_AppendS32Param(StringBuf& strOutput, sint32 val, bool isLast = false)
{
	if (isLast)
	{
		strOutput.addFmt("0x{:08x}", val);
		return;
	}
	strOutput.addFmt("0x{:08x}, ", val);
}

void IMLDebug_PrintLivenessRangeInfo(StringBuf& currentLineText, IMLSegment* imlSegment, sint32 offset)
{
	// pad to 70 characters
	sint32 index = currentLineText.getLen();
	while (index < 70)
	{
		debug_printf(" ");
		index++;
	}
	raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
	while (subrangeItr)
	{
		if (offset == subrangeItr->start.index)
		{
			if (false)//subrange->isDirtied && i == subrange->becomesDirtyAtIndex.index)
			{
				debug_printf("*%-2d", subrangeItr->range->virtualRegister);
			}
			else
			{
				debug_printf("|%-2d", subrangeItr->range->virtualRegister);
			}
		}
		else if (false)//subrange->isDirtied && i == subrange->becomesDirtyAtIndex.index )
		{
			debug_printf("*  ");
		}
		else if (offset >= subrangeItr->start.index && offset < subrangeItr->end.index)
		{
			debug_printf("|  ");
		}
		else
		{
			debug_printf("   ");
		}
		index += 3;
		// next
		subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
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

void IMLDebug_DumpSegment(ppcImlGenContext_t* ctx, IMLSegment* imlSegment, bool printLivenessRangeInfo)
{
	StringBuf strOutput(1024);

	strOutput.addFmt("SEGMENT {} | PPC=0x{:08x} Loop-depth {}", IMLDebug_GetSegmentName(ctx, imlSegment), imlSegment->ppcAddress, imlSegment->loopDepth);
	if (imlSegment->isEnterable)
	{
		strOutput.addFmt(" ENTERABLE (0x{:08x})", imlSegment->enterPPCAddress);
	}
	//else if (imlSegment->isJumpDestination)
	//{
	//	strOutput.addFmt(" JUMP-DEST (0x{:08x})", imlSegment->jumpDestinationPPCAddress);
	//}

	debug_printf("%s\n", strOutput.c_str());

	//strOutput.reset();
	//strOutput.addFmt("SEGMENT NAME 0x{:016x}", (uintptr_t)imlSegment);
	//debug_printf("%s", strOutput.c_str());

	if (printLivenessRangeInfo)
	{
		IMLDebug_PrintLivenessRangeInfo(strOutput, imlSegment, RA_INTER_RANGE_START);
	}
	//debug_printf("\n");

	sint32 lineOffsetParameters = 18;

	for (sint32 i = 0; i < imlSegment->imlList.size(); i++)
	{
		const IMLInstruction& inst = imlSegment->imlList[i];
		// don't log NOP instructions unless they have an associated PPC address
		if (inst.type == PPCREC_IML_TYPE_NO_OP && inst.associatedPPCAddress == MPTR_NULL)
			continue;
		strOutput.reset();
		strOutput.addFmt("{:08x} ", inst.associatedPPCAddress);
		if (inst.type == PPCREC_IML_TYPE_R_NAME || inst.type == PPCREC_IML_TYPE_NAME_R)
		{
			if (inst.type == PPCREC_IML_TYPE_R_NAME)
				strOutput.add("LD_NAME");
			else
				strOutput.add("ST_NAME");
			while ((sint32)strOutput.getLen() < lineOffsetParameters)
				strOutput.add(" ");

			IMLDebug_AppendRegisterParam(strOutput, inst.op_r_name.registerIndex);

			strOutput.addFmt("name_{} (", inst.op_r_name.registerIndex, inst.op_r_name.name);
			if (inst.op_r_name.name >= PPCREC_NAME_R0 && inst.op_r_name.name < (PPCREC_NAME_R0 + 999))
			{
				strOutput.addFmt("r{}", inst.op_r_name.name - PPCREC_NAME_R0);
			}
			else if (inst.op_r_name.name >= PPCREC_NAME_SPR0 && inst.op_r_name.name < (PPCREC_NAME_SPR0 + 999))
			{
				strOutput.addFmt("spr{}", inst.op_r_name.name - PPCREC_NAME_SPR0);
			}
			else
				strOutput.add("ukn");
			strOutput.add(")");
		}
		else if (inst.type == PPCREC_IML_TYPE_R_R)
		{
			strOutput.addFmt("{}", IMLDebug_GetOpcodeName(&inst));
			while ((sint32)strOutput.getLen() < lineOffsetParameters)
				strOutput.add(" ");
			IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r.registerResult);
			IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r.registerA, true);

			if (inst.crRegister != PPC_REC_INVALID_REGISTER)
			{
				strOutput.addFmt(" -> CR{}", inst.crRegister);
			}
		}
		else if (inst.type == PPCREC_IML_TYPE_R_R_R)
		{
			strOutput.addFmt("{}", IMLDebug_GetOpcodeName(&inst));
			while ((sint32)strOutput.getLen() < lineOffsetParameters)
				strOutput.add(" ");
			IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_r.registerResult);
			IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_r.registerA);
			IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_r.registerB, true);
			if (inst.crRegister != PPC_REC_INVALID_REGISTER)
			{
				strOutput.addFmt(" -> CR{}", inst.crRegister);
			}
		}
		else if (inst.type == PPCREC_IML_TYPE_R_R_S32)
		{
			strOutput.addFmt("{}", IMLDebug_GetOpcodeName(&inst));
			while ((sint32)strOutput.getLen() < lineOffsetParameters)
				strOutput.add(" ");

			IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_s32.registerResult);
			IMLDebug_AppendRegisterParam(strOutput, inst.op_r_r_s32.registerA);
			IMLDebug_AppendS32Param(strOutput, inst.op_r_r_s32.immS32, true);

			if (inst.crRegister != PPC_REC_INVALID_REGISTER)
			{
				strOutput.addFmt(" -> CR{}", inst.crRegister);
			}
		}
		else if (inst.type == PPCREC_IML_TYPE_R_S32)
		{
			strOutput.addFmt("{}", IMLDebug_GetOpcodeName(&inst));
			while ((sint32)strOutput.getLen() < lineOffsetParameters)
				strOutput.add(" ");

			IMLDebug_AppendRegisterParam(strOutput, inst.op_r_immS32.registerIndex);
			IMLDebug_AppendS32Param(strOutput, inst.op_r_immS32.immS32, true);

			if (inst.crRegister != PPC_REC_INVALID_REGISTER)
			{
				strOutput.addFmt(" -> CR{}", inst.crRegister);
			}
		}
		else if (inst.type == PPCREC_IML_TYPE_JUMPMARK)
		{
			strOutput.addFmt("jm_{:08x}:", inst.op_jumpmark.address);
		}
		else if (inst.type == PPCREC_IML_TYPE_PPC_ENTER)
		{
			strOutput.addFmt("ppcEnter_{:08x}:", inst.op_ppcEnter.ppcAddress);
		}
		else if (inst.type == PPCREC_IML_TYPE_LOAD || inst.type == PPCREC_IML_TYPE_STORE ||
			inst.type == PPCREC_IML_TYPE_LOAD_INDEXED || inst.type == PPCREC_IML_TYPE_STORE_INDEXED)
		{
			if (inst.type == PPCREC_IML_TYPE_LOAD || inst.type == PPCREC_IML_TYPE_LOAD_INDEXED)
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

			if (inst.type == PPCREC_IML_TYPE_LOAD_INDEXED || inst.type == PPCREC_IML_TYPE_STORE_INDEXED)
				strOutput.addFmt("[t{}+t{}]", inst.op_storeLoad.registerMem, inst.op_storeLoad.registerMem2);
			else
				strOutput.addFmt("[t{}+{}]", inst.op_storeLoad.registerMem, inst.op_storeLoad.immS32);
		}
		else if (inst.type == PPCREC_IML_TYPE_CJUMP)
		{
			if (inst.op_conditionalJump.condition == PPCREC_JUMP_CONDITION_E)
				strOutput.add("JE");
			else if (inst.op_conditionalJump.condition == PPCREC_JUMP_CONDITION_NE)
				strOutput.add("JNE");
			else if (inst.op_conditionalJump.condition == PPCREC_JUMP_CONDITION_G)
				strOutput.add("JG");
			else if (inst.op_conditionalJump.condition == PPCREC_JUMP_CONDITION_GE)
				strOutput.add("JGE");
			else if (inst.op_conditionalJump.condition == PPCREC_JUMP_CONDITION_L)
				strOutput.add("JL");
			else if (inst.op_conditionalJump.condition == PPCREC_JUMP_CONDITION_LE)
				strOutput.add("JLE");
			else if (inst.op_conditionalJump.condition == PPCREC_JUMP_CONDITION_NONE)
				strOutput.add("JALW"); // jump always
			else
				cemu_assert_unimplemented();
			strOutput.addFmt(" jm_{:08x} (cr{})", inst.op_conditionalJump.jumpmarkAddress, inst.crRegister);
		}
		else if (inst.type == PPCREC_IML_TYPE_NO_OP)
		{
			strOutput.add("NOP");
		}
		else if (inst.type == PPCREC_IML_TYPE_MACRO)
		{
			if (inst.operation == PPCREC_IML_MACRO_BLR)
			{
				strOutput.addFmt("MACRO BLR 0x{:08x} cycles (depr): {}", inst.op_macro.param, (sint32)inst.op_macro.paramU16);
			}
			else if (inst.operation == PPCREC_IML_MACRO_BLRL)
			{
				strOutput.addFmt("MACRO BLRL 0x{:08x} cycles (depr): {}", inst.op_macro.param, (sint32)inst.op_macro.paramU16);
			}
			else if (inst.operation == PPCREC_IML_MACRO_BCTR)
			{
				strOutput.addFmt("MACRO BCTR 0x{:08x} cycles (depr): {}", inst.op_macro.param, (sint32)inst.op_macro.paramU16);
			}
			else if (inst.operation == PPCREC_IML_MACRO_BCTRL)
			{
				strOutput.addFmt("MACRO BCTRL 0x{:08x} cycles (depr): {}", inst.op_macro.param, (sint32)inst.op_macro.paramU16);
			}
			else if (inst.operation == PPCREC_IML_MACRO_BL)
			{
				strOutput.addFmt("MACRO BL 0x{:08x} -> 0x{:08x} cycles (depr): {}", inst.op_macro.param, inst.op_macro.param2, (sint32)inst.op_macro.paramU16);
			}
			else if (inst.operation == PPCREC_IML_MACRO_B_FAR)
			{
				strOutput.addFmt("MACRO B_FAR 0x{:08x} -> 0x{:08x} cycles (depr): {}", inst.op_macro.param, inst.op_macro.param2, (sint32)inst.op_macro.paramU16);
			}
			else if (inst.operation == PPCREC_IML_MACRO_LEAVE)
			{
				strOutput.addFmt("MACRO LEAVE ppc: 0x{:08x}", inst.op_macro.param);
			}
			else if (inst.operation == PPCREC_IML_MACRO_HLE)
			{
				strOutput.addFmt("MACRO HLE ppcAddr: 0x{:08x} funcId: 0x{:08x}", inst.op_macro.param, inst.op_macro.param2);
			}
			else if (inst.operation == PPCREC_IML_MACRO_MFTB)
			{
				strOutput.addFmt("MACRO MFTB ppcAddr: 0x{:08x} sprId: 0x{:08x}", inst.op_macro.param, inst.op_macro.param2);
			}
			else if (inst.operation == PPCREC_IML_MACRO_COUNT_CYCLES)
			{
				strOutput.addFmt("MACRO COUNT_CYCLES cycles: {}", inst.op_macro.param);
			}
			else
			{
				strOutput.addFmt("MACRO ukn operation {}", inst.operation);
			}
		}
		else if (inst.type == PPCREC_IML_TYPE_FPR_R_NAME)
		{
			strOutput.addFmt("fpr_t{} = name_{} (", inst.op_r_name.registerIndex, inst.op_r_name.name);
			if (inst.op_r_name.name >= PPCREC_NAME_FPR0 && inst.op_r_name.name < (PPCREC_NAME_FPR0 + 999))
			{
				strOutput.addFmt("fpr{}", inst.op_r_name.name - PPCREC_NAME_FPR0);
			}
			else if (inst.op_r_name.name >= PPCREC_NAME_TEMPORARY_FPR0 && inst.op_r_name.name < (PPCREC_NAME_TEMPORARY_FPR0 + 999))
			{
				strOutput.addFmt("tempFpr{}", inst.op_r_name.name - PPCREC_NAME_TEMPORARY_FPR0);
			}
			else
				strOutput.add("ukn");
			strOutput.add(")");
		}
		else if (inst.type == PPCREC_IML_TYPE_FPR_NAME_R)
		{
			strOutput.addFmt("name_{} (", inst.op_r_name.name);
			if (inst.op_r_name.name >= PPCREC_NAME_FPR0 && inst.op_r_name.name < (PPCREC_NAME_FPR0 + 999))
			{
				strOutput.addFmt("fpr{}", inst.op_r_name.name - PPCREC_NAME_FPR0);
			}
			else if (inst.op_r_name.name >= PPCREC_NAME_TEMPORARY_FPR0 && inst.op_r_name.name < (PPCREC_NAME_TEMPORARY_FPR0 + 999))
			{
				strOutput.addFmt("tempFpr{}", inst.op_r_name.name - PPCREC_NAME_TEMPORARY_FPR0);
			}
			else
				strOutput.add("ukn");
			strOutput.addFmt(") = fpr_t{}", inst.op_r_name.registerIndex);
		}
		else if (inst.type == PPCREC_IML_TYPE_FPR_LOAD)
		{
			strOutput.addFmt("fpr_t{} = ", inst.op_storeLoad.registerData);
			if (inst.op_storeLoad.flags2.signExtend)
				strOutput.add("S");
			else
				strOutput.add("U");
			strOutput.addFmt("{} [t{}+{}] mode {}", inst.op_storeLoad.copyWidth / 8, inst.op_storeLoad.registerMem, inst.op_storeLoad.immS32, inst.op_storeLoad.mode);
			if (inst.op_storeLoad.flags2.notExpanded)
			{
				strOutput.addFmt(" <No expand>");
			}
		}
		else if (inst.type == PPCREC_IML_TYPE_FPR_STORE)
		{
			if (inst.op_storeLoad.flags2.signExtend)
				strOutput.add("S");
			else
				strOutput.add("U");
			strOutput.addFmt("{} [t{}+{}]", inst.op_storeLoad.copyWidth / 8, inst.op_storeLoad.registerMem, inst.op_storeLoad.immS32);
			strOutput.addFmt("= fpr_t{} mode {}\n", inst.op_storeLoad.registerData, inst.op_storeLoad.mode);
		}
		else if (inst.type == PPCREC_IML_TYPE_FPR_R_R)
		{
			strOutput.addFmt("{:>6} ", IMLDebug_GetOpcodeName(&inst));
			strOutput.addFmt("fpr{:02}, fpr{:02}", inst.op_fpr_r_r.registerResult, inst.op_fpr_r_r.registerOperand);
		}
		else if (inst.type == PPCREC_IML_TYPE_FPR_R_R_R_R)
		{
			strOutput.addFmt("{:>6} ", IMLDebug_GetOpcodeName(&inst));
			strOutput.addFmt("fpr{:02}, fpr{:02}, fpr{:02}, fpr{:02}", inst.op_fpr_r_r_r_r.registerResult, inst.op_fpr_r_r_r_r.registerOperandA, inst.op_fpr_r_r_r_r.registerOperandB, inst.op_fpr_r_r_r_r.registerOperandC);
		}
		else if (inst.type == PPCREC_IML_TYPE_FPR_R_R_R)
		{
			strOutput.addFmt("{:>6} ", IMLDebug_GetOpcodeName(&inst));
			strOutput.addFmt("fpr{:02}, fpr{:02}, fpr{:02}", inst.op_fpr_r_r_r.registerResult, inst.op_fpr_r_r_r.registerOperandA, inst.op_fpr_r_r_r.registerOperandB);
		}
		else if (inst.type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK)
		{
			strOutput.addFmt("CYCLE_CHECK\n");
		}
		else if (inst.type == PPCREC_IML_TYPE_CONDITIONAL_R_S32)
		{
			strOutput.addFmt("t{} ", inst.op_conditional_r_s32.registerIndex);
			bool displayAsHex = false;
			if (inst.operation == PPCREC_IML_OP_ASSIGN)
			{
				displayAsHex = true;
				strOutput.add("=");
			}
			else
				strOutput.addFmt("(unknown operation CONDITIONAL_R_S32 {})", inst.operation);
			if (displayAsHex)
				strOutput.addFmt(" 0x{:x}", inst.op_conditional_r_s32.immS32);
			else
				strOutput.addFmt(" {}", inst.op_conditional_r_s32.immS32);
			strOutput.add(" (conditional)");
			if (inst.crRegister != PPC_REC_INVALID_REGISTER)
			{
				strOutput.addFmt(" -> and update CR{}", inst.crRegister);
			}
		}
		else
		{
			strOutput.addFmt("Unknown iml type {}", inst.type);
		}
		debug_printf("%s", strOutput.c_str());
		if (printLivenessRangeInfo)
		{
			IMLDebug_PrintLivenessRangeInfo(strOutput, imlSegment, i);
		}
		debug_printf("\n");
	}
	// all ranges
	if (printLivenessRangeInfo)
	{
		debug_printf("Ranges-VirtReg                                                        ");
		raLivenessSubrange_t* subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while (subrangeItr)
		{
			debug_printf("v%-2d", subrangeItr->range->virtualRegister);
			subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
		}
		debug_printf("\n");
		debug_printf("Ranges-PhysReg                                                        ");
		subrangeItr = imlSegment->raInfo.linkedList_allSubranges;
		while (subrangeItr)
		{
			debug_printf("p%-2d", subrangeItr->range->physicalRegister);
			subrangeItr = subrangeItr->link_segmentSubrangesGPR.next;
		}
		debug_printf("\n");
	}
	// branch info
	debug_printf("Links from: ");
	for (sint32 i = 0; i < imlSegment->list_prevSegments.size(); i++)
	{
		if (i)
			debug_printf(", ");
		debug_printf("%s", IMLDebug_GetSegmentName(ctx, imlSegment->list_prevSegments[i]).c_str());
	}
	debug_printf("\n");
	if (imlSegment->nextSegmentBranchNotTaken)
		debug_printf("BranchNotTaken: %s\n", IMLDebug_GetSegmentName(ctx, imlSegment->nextSegmentBranchNotTaken).c_str());
	if (imlSegment->nextSegmentBranchTaken)
		debug_printf("BranchTaken: %s\n", IMLDebug_GetSegmentName(ctx, imlSegment->nextSegmentBranchTaken).c_str());
	if (imlSegment->nextSegmentIsUncertain)
		debug_printf("Dynamic target\n");
	debug_printf("\n");
}

void IMLDebug_Dump(ppcImlGenContext_t* ppcImlGenContext)
{
	for (size_t i = 0; i < ppcImlGenContext->segmentList2.size(); i++)
	{
		IMLDebug_DumpSegment(ppcImlGenContext, ppcImlGenContext->segmentList2[i], false);
		debug_printf("\n");
	}
}
