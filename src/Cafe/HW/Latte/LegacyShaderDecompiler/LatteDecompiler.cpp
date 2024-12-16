#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Core/LatteShaderAssembly.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerInternal.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerInstructions.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "util/helpers/helpers.h"

// parse instruction and if valid append it to instructionList
bool LatteDecompiler_ParseCFInstruction(LatteDecompilerShaderContext* shaderContext, uint32 cfIndex, uint32 cfWord0, uint32 cfWord1, bool* endOfProgram, std::vector<LatteDecompilerCFInstruction>& instructionList)
{
	LatteDecompilerShader* shaderObj = shaderContext->shader;
	uint32 cf_inst23_7 = (cfWord1 >> 23) & 0x7F;
	if (cf_inst23_7 < 0x40) // starting at 0x40 the bits overlap with the ALU instruction encoding
	{
		*endOfProgram = ((cfWord1 >> 21) & 1) != 0;
		uint32 addr = cfWord0 & 0xFFFFFFFF;
		uint32 count = (cfWord1 >> 10) & 7;
		if (((cfWord1 >> 19) & 1) != 0)
			count |= 0x8;
		count++;
		if (cf_inst23_7 == GPU7_CF_INST_CALL_FS)
		{
			// nop
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_NOP)
		{
			// nop
			if (((cfWord1 >> 0) & 7) != 0)
				debugBreakpoint(); // pop count is not zero
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_EXPORT || cf_inst23_7 == GPU7_CF_INST_EXPORT_DONE)
		{
			// export
			uint32 edType = (cfWord0 >> 13) & 0x3;
			uint32 edIndexGpr = (cfWord0 >> 23) & 0x7F;
			uint32 edRWRel = (cfWord0 >> 22) & 1;
			if (edRWRel != 0 || edIndexGpr != 0)
				debugBreakpoint();
			LatteDecompilerCFInstruction& cfInstruction = instructionList.emplace_back();
			// set type and address
			cfInstruction.type = cf_inst23_7;
			cfInstruction.cfAddr = cfIndex;
			// set cond
			cfInstruction.cfCond = (cfWord1 >> 8) & 3;
			// set export component selection
			cfInstruction.exportComponentSel[0] = (cfWord1 >> 0) & 0x7;
			cfInstruction.exportComponentSel[1] = (cfWord1 >> 3) & 0x7;
			cfInstruction.exportComponentSel[2] = (cfWord1 >> 6) & 0x7;
			cfInstruction.exportComponentSel[3] = (cfWord1 >> 9) & 0x7;
			// set export array base, index and burstcount
			cfInstruction.exportArrayBase = (cfWord0 >> 0) & 0x1FFF;
			cfInstruction.exportBurstCount = (cfWord1 >> 17) & 0xF;
			// set export source GPR and type
			cfInstruction.exportSourceGPR = (cfWord0 >> 15) & 0x7F;
			cfInstruction.exportType = edType;
			//cfInstruction->memWriteElemSize = (cfWord0>>29)&3; // unused
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_TEX)
		{
			LatteDecompilerCFInstruction& cfInstruction = instructionList.emplace_back();
			// set type and address
			cfInstruction.type = cf_inst23_7;
			cfInstruction.cfAddr = cfIndex;
			// set cond
			cfInstruction.cfCond = (cfWord1 >> 8) & 3;
			// set TEX clause related values
			cfInstruction.addr = addr; // index of first instruction in 64bit words
			cfInstruction.count = count; // number of instructions (each instruction is 128bit)
			// todo: CF_CONST and COND field and maybe other fields?
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_ELSE ||
			cf_inst23_7 == GPU7_CF_INST_POP)
		{
			LatteDecompilerCFInstruction& cfInstruction = instructionList.emplace_back();
			// set type and address
			cfInstruction.type = cf_inst23_7;
			cfInstruction.cfAddr = cfIndex;
			// set cond and popCount
			cfInstruction.cfCond = (cfWord1 >> 8) & 3;
			cfInstruction.popCount = (cfWord1 >> 0) & 7;
			// set TEX clause related values
			cfInstruction.addr = addr; // index of first instruction in 64bit words
			cfInstruction.count = count; // number of instructions (each instruction is 128bit)
			// todo: CF_CONST
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_JUMP)
		{
			// ignored (we use ALU/IF/ELSE/PUSH/POP clauses to determine code flow)
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_LOOP_START_DX10 || cf_inst23_7 == GPU7_CF_INST_LOOP_END ||
				 cf_inst23_7 == GPU7_CF_INST_LOOP_START_NO_AL)
		{
			LatteDecompilerCFInstruction& cfInstruction = instructionList.emplace_back();
			// set type and address
			cfInstruction.type = cf_inst23_7;
			cfInstruction.cfAddr = cfIndex;
			// set cond and popCount
			cfInstruction.cfCond = (cfWord1 >> 8) & 3;
			cfInstruction.popCount = (cfWord1 >> 0) & 7;
			// set TEX clause related values
			cfInstruction.addr = addr; // index of first instruction in 64bit words
			cfInstruction.count = count; // number of instructions (each instruction is 128bit)
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_LOOP_BREAK)
		{
			LatteDecompilerCFInstruction& cfInstruction = instructionList.emplace_back();
			// set type and address
			cfInstruction.type = cf_inst23_7;
			cfInstruction.cfAddr = cfIndex;
			// set cond and popCount
			cfInstruction.cfCond = (cfWord1 >> 8) & 3;
			cfInstruction.popCount = (cfWord1 >> 0) & 7;
			// set clause related values
			cfInstruction.addr = addr; // index of first instruction in 64bit words
			cfInstruction.count = count; // number of instructions (each instruction is 128bit)
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_MEM_STREAM0_WRITE ||
			cf_inst23_7 == GPU7_CF_INST_MEM_STREAM1_WRITE)
		{
			LatteDecompilerCFInstruction& cfInstruction = instructionList.emplace_back();
			// todo: Correctly read all the STREAM0_WRITE specific fields
			// set type and address
			cfInstruction.type = cf_inst23_7;
			cfInstruction.cfAddr = cfIndex;
			// set export array base
			cfInstruction.exportArrayBase = (cfWord0 >> 0) & 0x1FFF;
			cfInstruction.memWriteArraySize = (cfWord1 >> 0) & 0xFFF;
			cfInstruction.memWriteCompMask = (cfWord1 >> 12) & 0xF;
			// set export source GPR and type
			cfInstruction.exportSourceGPR = (cfWord0 >> 15) & 0x7F;
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_MEM_RING_WRITE)
		{
			// this CF instruction is only available when the geometry shader stage is active
			LatteDecompilerCFInstruction& cfInstruction = instructionList.emplace_back();
			// set type and address
			cfInstruction.type = cf_inst23_7;
			cfInstruction.cfAddr = cfIndex;
			// set export array base
			cfInstruction.exportArrayBase = (cfWord0 >> 0) & 0x1FFF;
			cfInstruction.memWriteArraySize = (cfWord1 >> 0) & 0xFFF;
			cfInstruction.memWriteCompMask = (cfWord1 >> 12) & 0xF;
			cfInstruction.memWriteElemSize = ((cfWord0 >> 30) & 0x3);
			cfInstruction.exportBurstCount = (cfWord1 >> 17) & 0xF;
			// set export source GPR and type
			cfInstruction.exportSourceGPR = (cfWord0 >> 15) & 0x7F;
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_EMIT_VERTEX)
		{
			// this CF instruction is only available when the geometry shader stage is active
			LatteDecompilerCFInstruction& cfInstruction = instructionList.emplace_back();
			// set type and address
			cfInstruction.type = cf_inst23_7;
			cfInstruction.cfAddr = cfIndex;
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_CALL)
		{
			// CALL subroutine
			LatteDecompilerCFInstruction& cfInstruction = instructionList.emplace_back();
			uint32 callCount = (cfWord1 >> 13) & 0x3F;
			cfInstruction.addr = addr; // index of call destination in 64bit words
			cfInstruction.count = callCount; // store callCount in count
			cfInstruction.type = cf_inst23_7;
			cfInstruction.cfAddr = cfIndex;
			// remember subroutine
			bool subroutineIsKnown = false;
			for (auto& it : shaderContext->list_subroutines)
			{
				if (it.cfAddr == addr)
				{
					subroutineIsKnown = true;
					break;
				}
			}
			if (subroutineIsKnown == false)
			{
				LatteDecompilerSubroutineInfo subroutineInfo = { 0 };
				subroutineInfo.cfAddr = addr;
				shaderContext->list_subroutines.push_back(subroutineInfo);
			}
			return true;
		}
		else if (cf_inst23_7 == GPU7_CF_INST_RETURN)
		{
			LatteDecompilerCFInstruction& cfInstruction = instructionList.emplace_back();
			// set type and address
			cfInstruction.type = cf_inst23_7;
			cfInstruction.cfAddr = cfIndex;
			// set cond and popCount
			cfInstruction.cfCond = (cfWord1 >> 8) & 3;
			cfInstruction.popCount = (cfWord1 >> 0) & 7;
			// todo - other fields?
			return true;
		}
		else
		{
			debug_printf("Unknown 23_7 clause 0x%x found\n", cf_inst23_7);
			shaderObj->hasError = true;
			return false;
		}
	}
	else
	{
		// ALU instruction
		uint32 cf_inst26_4 = ((cfWord1 >> 26) & 0xF) | GPU7_CF_INST_ALU_MASK;
		if (cf_inst26_4 == GPU7_CF_INST_ALU || cf_inst26_4 == GPU7_CF_INST_ALU_PUSH_BEFORE || cf_inst26_4 == GPU7_CF_INST_ALU_POP_AFTER || cf_inst26_4 == GPU7_CF_INST_ALU_POP2_AFTER || cf_inst26_4 == GPU7_CF_INST_ALU_BREAK || cf_inst26_4 == GPU7_CF_INST_ALU_ELSE_AFTER)
		{
			LatteDecompilerCFInstruction& cfInstruction = instructionList.emplace_back();
			// set type and address
			cfInstruction.type = cf_inst26_4;
			cfInstruction.cfAddr = cfIndex;
			// CF_ALU_* has no cond field
			cfInstruction.cfCond = 0;
			// set ALU clause related values
			cfInstruction.addr = (cfWord0 >> 0) & 0x3FFFFF; // index of first instruction in 64bit words
			cfInstruction.count = ((cfWord1 >> 18) & 0x7F) + 1; // number of instructions (each instruction is 64bit)
			// set constant file/bank values
			cfInstruction.cBank0Index = (cfWord0 >> 22) & 0xF;
			cfInstruction.cBank1Index = (cfWord0 >> 26) & 0xF;
			cfInstruction.cBank0AddrBase = ((cfWord1 >> 2) & 0xFF) * 16;
			cfInstruction.cBank1AddrBase = ((cfWord1 >> 10) & 0xFF) * 16;
			return true;
		}
		else
		{
			debug_printf("Unknown 26_4 clause 0x%x found\n", cf_inst26_4);
			shaderObj->hasError = true;
			return false;
		}
	}
	cemu_assert_unimplemented(); // should not reach
	return false;
}

void LatteDecompiler_ParseCFSubroutine(LatteDecompilerShaderContext* shaderContext, uint8* programData, uint32 programSize, LatteDecompilerSubroutineInfo* subroutineInfo)
{
	LatteDecompilerShader* shaderObj = shaderContext->shader;
	// parse control flow instructions
	for (uint32 i = subroutineInfo->cfAddr; i < programSize / 8; i++)
	{
		uint32 cfWord0 = *(uint32*)(programData + i * 8 + 0);
		uint32 cfWord1 = *(uint32*)(programData + i * 8 + 4);
		bool isEndOfProgram = false;
		if( !LatteDecompiler_ParseCFInstruction(shaderContext, i, cfWord0, cfWord1, &isEndOfProgram, subroutineInfo->instructions) )
			continue;
		cemu_assert_debug(!isEndOfProgram); // should never be encountered in a subroutine?
		if (shaderObj->hasError)
			return;
		auto& cfInstruction = subroutineInfo->instructions.back();
		if (cfInstruction.type == GPU7_CF_INST_RETURN)
			return; // todo - should check if this return statement is conditional
	}
	cemu_assert_debug(false); // should not reach (subroutines have to end with RETURN)
}

void LatteDecompiler_ParseCF(LatteDecompilerShaderContext* shaderContext, uint8* programData, uint32 programSize)
{
	LatteDecompilerShader* shaderObj = shaderContext->shader;
	// parse control flow instructions for main entry point
	bool endOfProgram = false;
	for (uint32 i = 0; i < programSize / 8; i++)
	{
		uint32 cfWord0 = *(uint32*)(programData + i * 8 + 0);
		uint32 cfWord1 = *(uint32*)(programData + i * 8 + 4);
		LatteDecompiler_ParseCFInstruction(shaderContext, i, cfWord0, cfWord1, &endOfProgram, shaderContext->cfInstructions);
		if (endOfProgram)
			break;
	}
	// parse CF instructions for subroutines
	for (auto& subroutineInfo : shaderContext->list_subroutines)
	{
		LatteDecompiler_ParseCFSubroutine(shaderContext, programData, programSize, &subroutineInfo);
	}
}

// returns true if the given op2/op3 ALU instruction is always executed on the transcendental unit
bool LatteDecompiler_IsALUTransInstruction(bool isOP3, uint32 opcode)
{
	if( isOP3 == true )
		return false; // OP3 has no transcendental instructions?

	if( opcode == ALU_OP2_INST_COS ||
		opcode == ALU_OP2_INST_SIN ||
		opcode == ALU_OP2_INST_RECIP_FF ||
		opcode == ALU_OP2_INST_RECIP_IEEE ||
		opcode == ALU_OP2_INST_RECIPSQRT_IEEE ||
		opcode == ALU_OP2_INST_RECIPSQRT_CLAMPED ||
		opcode == ALU_OP2_INST_RECIPSQRT_FF ||
		opcode == ALU_OP2_INST_MULLO_INT ||
		opcode == ALU_OP2_INST_MULLO_UINT ||
		opcode == ALU_OP2_INST_FLT_TO_INT ||
		opcode == ALU_OP2_INST_FLT_TO_UINT ||
		opcode == ALU_OP2_INST_INT_TO_FLOAT ||
		opcode == ALU_OP2_INST_UINT_TO_FLOAT ||
		opcode == ALU_OP2_INST_LOG_CLAMPED ||
		opcode == ALU_OP2_INST_LOG_IEEE ||
		opcode == ALU_OP2_INST_EXP_IEEE ||
		opcode == ALU_OP2_INST_UINT_TO_FLOAT ||
		opcode == ALU_OP2_INST_SQRT_IEEE
		)
	{
		// transcendental
		return true;
	}
	else if( opcode == ALU_OP2_INST_MOV ||
		opcode == ALU_OP2_INST_ADD ||
		opcode == ALU_OP2_INST_NOP || 
		opcode == ALU_OP2_INST_MUL || 
		opcode == ALU_OP2_INST_DOT4 ||
		opcode == ALU_OP2_INST_DOT4_IEEE ||
		opcode == ALU_OP2_INST_MAX || // Not sure if MIN/MAX are non-transcendental?
		opcode == ALU_OP2_INST_MIN ||
		opcode == ALU_OP2_INST_AND_INT ||
		opcode == ALU_OP2_INST_OR_INT ||
		opcode == ALU_OP2_INST_XOR_INT ||
		opcode == ALU_OP2_INST_NOT_INT ||
		opcode == ALU_OP2_INST_ADD_INT ||
		opcode == ALU_OP2_INST_SUB_INT ||
		opcode == ALU_OP2_INST_SETGT ||
		opcode == ALU_OP2_INST_SETGE ||
		opcode == ALU_OP2_INST_SETNE ||
		opcode == ALU_OP2_INST_SETE ||
		opcode == ALU_OP2_INST_SETE_INT ||
		opcode == ALU_OP2_INST_SETNE_INT ||
		opcode == ALU_OP2_INST_SETGT_INT ||
		opcode == ALU_OP2_INST_SETGE_INT ||
		opcode == ALU_OP2_INST_SETGE_UINT ||
		opcode == ALU_OP2_INST_SETGT_UINT ||
		opcode == ALU_OP2_INST_MAX_DX10 ||
		opcode == ALU_OP2_INST_MIN_DX10 ||
		opcode == ALU_OP2_INST_PRED_SETE ||
		opcode == ALU_OP2_INST_PRED_SETNE ||
		opcode == ALU_OP2_INST_PRED_SETGE ||
		opcode == ALU_OP2_INST_PRED_SETGT ||
		opcode == ALU_OP2_INST_PRED_SETE_INT ||
		opcode == ALU_OP2_INST_PRED_SETNE_INT ||
		opcode == ALU_OP2_INST_PRED_SETGT_INT ||
		opcode == ALU_OP2_INST_PRED_SETGE_INT ||
		opcode == ALU_OP2_INST_KILLE_INT ||
		opcode == ALU_OP2_INST_KILLGT_INT ||
		opcode == ALU_OP2_INST_KILLNE_INT ||
		opcode == ALU_OP2_INST_KILLGT ||
		opcode == ALU_OP2_INST_KILLGE ||
		opcode == ALU_OP2_INST_KILLE ||
		opcode == ALU_OP2_INST_MUL_IEEE ||
		opcode == ALU_OP2_INST_FLOOR ||
		opcode == ALU_OP2_INST_FRACT ||
		opcode == ALU_OP2_INST_TRUNC ||
		opcode == ALU_OP2_INST_LSHL_INT ||
		opcode == ALU_OP2_INST_ASHR_INT ||
		opcode == ALU_OP2_INST_LSHR_INT ||
		opcode == ALU_OP2_INST_MAX_INT ||
		opcode == ALU_OP2_INST_MIN_INT ||
		opcode == ALU_OP2_INST_MAX_UINT ||
		opcode == ALU_OP2_INST_MIN_UINT ||
		opcode == ALU_OP2_INST_MOVA_FLOOR ||
		opcode == ALU_OP2_INST_MOVA_INT ||
		opcode == ALU_OP2_INST_SETE_DX10 ||
		opcode == ALU_OP2_INST_SETNE_DX10 ||
		opcode == ALU_OP2_INST_SETGT_DX10 ||
		opcode == ALU_OP2_INST_SETGE_DX10 ||
		opcode == ALU_OP2_INST_RNDNE ||
		opcode == ALU_OP2_INST_CUBE // reduction instruction
		)
	{
		// not transcendental
		return false;
	}
	else
	{
		debug_printf("_isALUTransInstruction(): Unknown instruction 0x%x (%s)\n", opcode, isOP3?"op3":"op2");
	}

	// ALU.Trans instructions:
	// [x] FLT_TO_INT
	// [x] FLT_TO_UINT
	// [x] INT_TO_FLT
	// MULHI_INT
	// MULHI_UINT
	// [x] MULLO_INT
	// [x] MULLO_UINT
	// RECIP_INT
	// RECIP_UINT
	// [x] UINT_TO_FLT
	// [x] COS
	// [x] EXP_IEEE
	// [x] LOG_CLAMPED
	// [x] LOG_IEEE
	// MUL_LIT
	// MUL_LIT_D2
	// MUL_LIT_M2
	// MUL_LIT_M4
	// RECIP_CLAMPED
	// [x] RECIP_FF
	// [x] RECIP_IEEE
	// [x] RECIPSQRT_CLAMPED
	// [x] RECIPSQRT_FF
	// [x] RECIPSQRT_IEEE
	// [x] SIN
	// [x] SQRT_IEEE

	return false;
}

void LatteDecompiler_ParseALUClause(LatteDecompilerShader* shaderContext, LatteDecompilerCFInstruction* cfInstruction, uint8* programData, uint32 programSize)
{
	sint32 instructionGroupIndex = 0;
	sint32 indexInGroup = 0; // index of instruction within instruction group
	uint32 elementsWrittenMask = 0; // used to determine ALU/Trans unit for instructions
	uint8 literalMask = 0; // mask of used literals for current instruction group
	sint32 parserIndex = 0;
	while( parserIndex < cfInstruction->count )
	{
		uint32 aluWord0 = *(uint32*)(programData+(cfInstruction->addr+parserIndex)*8+0);
		uint32 aluWord1 = *(uint32*)(programData+(cfInstruction->addr+parserIndex)*8+4);
		parserIndex++;
		bool isLastInGroup = (aluWord0&0x80000000) != 0;
		uint32 alu_inst13_5 = (aluWord1>>13)&0x1F;
		// parameters from ALU word 0 (shared for ALU OP2 and OP3)
		uint32 src0Sel = (aluWord0>>0)&0x1FF; // source selection
		uint32 src1Sel = (aluWord0>>13)&0x1FF;
		uint32 src0Rel = (aluWord0>>9)&0x1; // relative addressing mode
		uint32 src1Rel = (aluWord0>>22)&0x1;
		uint32 src0Chan = (aluWord0>>10)&0x3; // component selection x/y/z/w
		uint32 src1Chan = (aluWord0>>23)&0x3;
		uint32 src0Neg = (aluWord0>>12)&0x1; // negate input
		uint32 src1Neg = (aluWord0>>25)&0x1;
		uint32 indexMode = (aluWord0>>26)&7;
		uint32 predSel = (aluWord0>>29)&3;
		if( predSel != 0 )
			debugBreakpoint();
		if( alu_inst13_5 >= 0x8 )
		{
			// op3
			// parameters from ALU word 1
			uint32 src2Sel = (aluWord1>>0)&0x1FF; // source selection
			uint32 src2Rel = (aluWord1>>9)&0x1; // relative addressing mode
			uint32 src2Chan = (aluWord1>>10)&0x3; // component selection x/y/z/w
			uint32 src2Neg = (aluWord1>>12)&0x1; // negate input

			uint32 destGpr = (aluWord1>>21)&0x7F;
			uint32 destRel = (aluWord1>>28)&1;
			uint32 destElem = (aluWord1>>29)&3;
			uint32 destClamp = (aluWord1>>31)&1;

			LatteDecompilerALUInstruction aluInstruction;
			aluInstruction.cfInstruction = cfInstruction;
			aluInstruction.isOP3 = true;
			aluInstruction.opcode = alu_inst13_5;
			aluInstruction.instructionGroupIndex = instructionGroupIndex;
			aluInstruction.indexMode = indexMode;
			aluInstruction.destGpr = destGpr;
			aluInstruction.destRel = destRel;
			aluInstruction.destElem = destElem;
			aluInstruction.destClamp = destClamp;
			aluInstruction.writeMask = 1;
			aluInstruction.omod = 0; // op3 has no omod
			aluInstruction.sourceOperand[0].sel = src0Sel;
			aluInstruction.sourceOperand[0].rel = src0Rel;
			aluInstruction.sourceOperand[0].abs = 0;
			aluInstruction.sourceOperand[0].neg = src0Neg;
			aluInstruction.sourceOperand[0].chan = src0Chan;
			aluInstruction.sourceOperand[1].sel = src1Sel;
			aluInstruction.sourceOperand[1].rel = src1Rel;
			aluInstruction.sourceOperand[1].abs = 0;
			aluInstruction.sourceOperand[1].neg = src1Neg;
			aluInstruction.sourceOperand[1].chan = src1Chan;
			aluInstruction.sourceOperand[2].sel = src2Sel;
			aluInstruction.sourceOperand[2].rel = src2Rel;
			aluInstruction.sourceOperand[2].abs = 0;
			aluInstruction.sourceOperand[2].neg = src2Neg;
			aluInstruction.sourceOperand[2].chan = src2Chan;
			// check for literal access
			if( GPU7_ALU_SRC_IS_LITERAL(src0Sel) )
				literalMask |= (1<<src0Chan);
			if( GPU7_ALU_SRC_IS_LITERAL(src1Sel) )
				literalMask |= (1<<src1Chan);
			if( GPU7_ALU_SRC_IS_LITERAL(src2Sel) )
				literalMask |= (1<<src2Chan);
			// determine used ALU unit (x,y,z,w,t)
			uint32 aluUnit = destElem;
			if( aluUnit < 4 && (elementsWrittenMask & (1<<aluUnit)) != 0 )
			{
				aluUnit = 4; // ALU unit already used, this instruction uses the transcendental unit
			}
			elementsWrittenMask |= (1<<aluUnit);
			aluInstruction.aluUnit = aluUnit;
			aluInstruction.indexInGroup = indexInGroup;
			aluInstruction.isLastInstructionOfGroup = isLastInGroup;
			// add instruction to list of sub-instructions
			cfInstruction->instructionsALU.emplace_back(aluInstruction);
		}
		else
		{
			uint32 alu_inst7_11 = (aluWord1>>7)&0x7FF;

			uint32 src0Abs = (aluWord1>>0)&1;
			uint32 src1Abs = (aluWord1>>1)&1;
			uint32 updateExecuteMask = (aluWord1>>2)&1;
			uint32 updatePredicate = (aluWord1>>3)&1;
			uint32 writeMask = (aluWord1>>4)&1;
			uint32 omod = (aluWord1>>5)&3;

			uint32 destGpr = (aluWord1>>21)&0x7F;
			uint32 destRel = (aluWord1>>28)&1;
			uint32 destElem = (aluWord1>>29)&3;
			uint32 destClamp = (aluWord1>>31)&1;

			LatteDecompilerALUInstruction aluInstruction;
			aluInstruction.cfInstruction = cfInstruction;
			aluInstruction.isOP3 = false;
			aluInstruction.opcode = alu_inst7_11;
			aluInstruction.instructionGroupIndex = instructionGroupIndex;
			aluInstruction.indexMode = indexMode;
			aluInstruction.destGpr = destGpr;
			aluInstruction.destRel = destRel;
			aluInstruction.destElem = destElem;
			aluInstruction.destClamp = destClamp;
			aluInstruction.writeMask = writeMask;
			aluInstruction.updateExecuteMask = updateExecuteMask;
			aluInstruction.updatePredicate = updatePredicate;
			aluInstruction.omod = omod;
			aluInstruction.sourceOperand[0].sel = src0Sel;
			aluInstruction.sourceOperand[0].rel = src0Rel;
			aluInstruction.sourceOperand[0].abs = src0Abs;
			aluInstruction.sourceOperand[0].neg = src0Neg;
			aluInstruction.sourceOperand[0].chan = src0Chan;
			aluInstruction.sourceOperand[1].sel = src1Sel;
			aluInstruction.sourceOperand[1].rel = src1Rel;
			aluInstruction.sourceOperand[1].abs = src1Abs;
			aluInstruction.sourceOperand[1].neg = src1Neg;
			aluInstruction.sourceOperand[1].chan = src1Chan;
			aluInstruction.sourceOperand[2].sel = 0xFFFFFFFF;
			// check for literal access
			if( GPU7_ALU_SRC_IS_LITERAL(src0Sel) )
				literalMask |= (1<<src0Chan);
			if( GPU7_ALU_SRC_IS_LITERAL(src1Sel) )
				literalMask |= (1<<src1Chan);
			// determine ALU unit (x,y,z,w,t)
			uint32 aluUnit = destElem;
			// some instructions always use the transcendental unit
			bool isTranscendentalOperation = LatteDecompiler_IsALUTransInstruction(false, alu_inst7_11);
			if( isTranscendentalOperation )
				aluUnit = 4;
			if( aluUnit < 4 && (elementsWrittenMask & (1<<aluUnit)) != 0 )
			{
				aluUnit = 4; // ALU unit already used, this instruction uses the transcendental unit
			}
			elementsWrittenMask |= (1<<aluUnit);
			aluInstruction.aluUnit = aluUnit;
			aluInstruction.indexInGroup = indexInGroup;
			aluInstruction.isLastInstructionOfGroup = isLastInGroup;
			// add instruction to list of sub-instructions
			cfInstruction->instructionsALU.emplace_back(aluInstruction);
		}
		indexInGroup++;
		if( isLastInGroup )
		{
			// load literal data
			if( literalMask )
			{
				bool useLiteralDataXY = false;
				bool useLiteralDataZW = false;
				if( (literalMask&(1|2)) )
				{
					useLiteralDataXY = true;
				}
				if( (literalMask&(4|8)) )
				{
					useLiteralDataXY = true;
					useLiteralDataZW = true;
				}
				uint32 literalWords[4] = {0};
				literalWords[0] = *(uint32*)(programData+(cfInstruction->addr+parserIndex)*8+0);
				literalWords[1] = *(uint32*)(programData+(cfInstruction->addr+parserIndex)*8+4);
				if( useLiteralDataZW )
				{
					literalWords[2] = *(uint32*)(programData+(cfInstruction->addr+parserIndex+1)*8+0);
					literalWords[3] = *(uint32*)(programData+(cfInstruction->addr+parserIndex+1)*8+4);
				}
				if( useLiteralDataZW )
					parserIndex += 2;
				else
					parserIndex += 1;
				// set literal data for all instructions of the current instruction group
				for(auto& aluInstructionItr : reverse_itr(cfInstruction->instructionsALU) )
				{
					if( aluInstructionItr.instructionGroupIndex != instructionGroupIndex )
						break;
					aluInstructionItr.literalData.w[0] = literalWords[0];
					aluInstructionItr.literalData.w[1] = literalWords[1];
					aluInstructionItr.literalData.w[2] = literalWords[2];
					aluInstructionItr.literalData.w[3] = literalWords[3];
				}
			}
			// reset instruction group related tracking variables
			literalMask = 0;
			elementsWrittenMask = 0;
			indexInGroup = 0;
			// start next group
			instructionGroupIndex++;
		}
	}
}

/*
 * Parse TEX clause
 */
void LatteDecompiler_ParseTEXClause(LatteDecompilerShader* shaderContext, LatteDecompilerCFInstruction* cfInstruction, uint8* programData, uint32 programSize)
{
	for(sint32 i=0; i<cfInstruction->count; i++)
	{
		// each instruction is 128bit
		uint32 instructionAddr = cfInstruction->addr*2+i*4;
		uint32 word0 = *(uint32*)(programData+instructionAddr*4+0);
		uint32 word1 = *(uint32*)(programData+instructionAddr*4+4);
		uint32 word2 = *(uint32*)(programData+instructionAddr*4+8);
		uint32 word3 = *(uint32*)(programData+instructionAddr*4+12);
		uint32 inst0_4 = (word0>>0)&0x1F;
		if (inst0_4 == GPU7_TEX_INST_SAMPLE || inst0_4 == GPU7_TEX_INST_SAMPLE_L || inst0_4 == GPU7_TEX_INST_SAMPLE_LZ || inst0_4 == GPU7_TEX_INST_SAMPLE_LB || inst0_4 == GPU7_TEX_INST_SAMPLE_C || inst0_4 == GPU7_TEX_INST_SAMPLE_C_L || inst0_4 == GPU7_TEX_INST_SAMPLE_C_LZ || inst0_4 == GPU7_TEX_INST_FETCH4 || inst0_4 == GPU7_TEX_INST_SAMPLE_G || inst0_4 == GPU7_TEX_INST_LD
			|| inst0_4 == GPU7_TEX_INST_GET_TEXTURE_RESINFO || inst0_4 == GPU7_TEX_INST_GET_COMP_TEX_LOD)
		{
			uint32 fetchType = (word0 >> 5) & 3;
			uint32 bufferId = (word0 >> 8) & 0xFF;
			uint32 samplerId = (word2 >> 15) & 0x1F;
			uint32 srcGpr = (word0 >> 16) & 0x7F;
			uint32 srcRel = (word0 >> 23) & 1;
			if (srcRel != 0)
				debugBreakpoint();
			uint32 destGpr = (word1 >> 0) & 0x7F;
			uint32 destRel = (word1 >> 7) & 1;
			if (destRel != 0)
				debugBreakpoint();
			uint32 dstSelX = (word1 >> 9) & 0x7;
			uint32 dstSelY = (word1 >> 12) & 0x7;
			uint32 dstSelZ = (word1 >> 15) & 0x7;
			uint32 dstSelW = (word1 >> 18) & 0x7;

			uint32 coordTypeX = (word1 >> 28) & 1;
			uint32 coordTypeY = (word1 >> 29) & 1;
			uint32 coordTypeZ = (word1 >> 30) & 1;
			uint32 coordTypeW = (word1 >> 31) & 1;

			uint32 srcSelX = (word2 >> 20) & 0x7;
			uint32 srcSelY = (word2 >> 23) & 0x7;
			uint32 srcSelZ = (word2 >> 26) & 0x7;
			uint32 srcSelW = (word2 >> 29) & 0x7;

			uint32 offsetX = (word2 >> 0) & 0x1F;
			uint32 offsetY = (word2 >> 5) & 0x1F;
			uint32 offsetZ = (word2 >> 10) & 0x1F;

			sint8 lodBias = (word2 >> 21) & 0x7F;
			if ((lodBias&0x40) != 0)
				lodBias |= 0x80;
			// bufferID -> Texture index
			// samplerId -> Sampler index
			sint32 textureIndex = bufferId - 0x00;

			// create new tex instruction
			LatteDecompilerTEXInstruction texInstruction;
			texInstruction.cfInstruction = cfInstruction;
			texInstruction.opcode = inst0_4;
			texInstruction.textureFetch.textureIndex = textureIndex;
			texInstruction.textureFetch.samplerIndex = samplerId;
			texInstruction.dstSel[0] = dstSelX;
			texInstruction.dstSel[1] = dstSelY;
			texInstruction.dstSel[2] = dstSelZ;
			texInstruction.dstSel[3] = dstSelW;
			texInstruction.textureFetch.srcSel[0] = srcSelX;
			texInstruction.textureFetch.srcSel[1] = srcSelY;
			texInstruction.textureFetch.srcSel[2] = srcSelZ;
			texInstruction.textureFetch.srcSel[3] = srcSelW;
			texInstruction.textureFetch.offsetX = (sint8)((offsetX & 0x10) ? (offsetX | 0xE0) : (offsetX));
			texInstruction.textureFetch.offsetY = (sint8)((offsetY & 0x10) ? (offsetY | 0xE0) : (offsetY));
			texInstruction.textureFetch.offsetZ = (sint8)((offsetZ & 0x10) ? (offsetZ | 0xE0) : (offsetZ));
			texInstruction.dstGpr = destGpr;
			texInstruction.srcGpr = srcGpr;
			texInstruction.textureFetch.unnormalized[0] = coordTypeX == 0;
			texInstruction.textureFetch.unnormalized[1] = coordTypeY == 0;
			texInstruction.textureFetch.unnormalized[2] = coordTypeZ == 0;
			texInstruction.textureFetch.unnormalized[3] = coordTypeW == 0;
			texInstruction.textureFetch.lodBias = (sint8)lodBias;
			cfInstruction->instructionsTEX.emplace_back(texInstruction);
		}
		else if( inst0_4 == GPU7_TEX_INST_SET_CUBEMAP_INDEX )
		{
			// todo: check if the encoding of fields matches with that of GPU7_TEX_INST_SAMPLE* (it should, according to AMD doc)
			uint32 fetchType = (word0>>5)&3;
			uint32 bufferId = (word0>>8)&0xFF;
			uint32 samplerId = (word2>>15)&0x1F;
			uint32 srcGpr = (word0>>16)&0x7F;
			uint32 srcRel = (word0>>23)&1;
			if( srcRel != 0 )
				debugBreakpoint();
			uint32 destGpr = (word1>>0)&0x7F;
			uint32 destRel = (word1>>7)&1;
			if( destRel != 0 )
				debugBreakpoint();
			uint32 dstSelX = (word1>>9)&0x7;
			uint32 dstSelY = (word1>>12)&0x7;
			uint32 dstSelZ = (word1>>15)&0x7;
			uint32 dstSelW = (word1>>18)&0x7;

			uint32 srcSelX = (word2>>20)&0x7;
			uint32 srcSelY = (word2>>23)&0x7;
			uint32 srcSelZ = (word2>>26)&0x7;
			uint32 srcSelW = (word2>>29)&0x7;

			sint32 textureIndex = bufferId-0x00;

			// create new tex instruction
			LatteDecompilerTEXInstruction texInstruction;
			texInstruction.cfInstruction = cfInstruction;
			texInstruction.opcode = inst0_4;
			texInstruction.textureFetch.textureIndex = textureIndex;
			texInstruction.textureFetch.samplerIndex = samplerId;
			texInstruction.dstSel[0] = dstSelX;
			texInstruction.dstSel[1] = dstSelY;
			texInstruction.dstSel[2] = dstSelZ;
			texInstruction.dstSel[3] = dstSelW;
			texInstruction.textureFetch.srcSel[0] = srcSelX;
			texInstruction.textureFetch.srcSel[1] = srcSelY;
			texInstruction.textureFetch.srcSel[2] = srcSelZ;
			texInstruction.textureFetch.srcSel[3] = srcSelW;
			texInstruction.dstGpr = destGpr;
			texInstruction.srcGpr = srcGpr;
			cfInstruction->instructionsTEX.emplace_back(texInstruction);
		}
		else if (inst0_4 == GPU7_TEX_INST_GET_GRADIENTS_H || inst0_4 == GPU7_TEX_INST_GET_GRADIENTS_V)
		{
			uint32 fetchType = (word0 >> 5) & 3;
			uint32 bufferId = (word0 >> 8) & 0xFF;
			uint32 samplerId = (word2 >> 15) & 0x1F;
			uint32 srcGpr = (word0 >> 16) & 0x7F;
			uint32 srcRel = (word0 >> 23) & 1;
			if (srcRel != 0)
				debugBreakpoint();
			uint32 destGpr = (word1 >> 0) & 0x7F;
			uint32 destRel = (word1 >> 7) & 1;
			if (destRel != 0)
				debugBreakpoint();
			uint32 dstSelX = (word1 >> 9) & 0x7;
			uint32 dstSelY = (word1 >> 12) & 0x7;
			uint32 dstSelZ = (word1 >> 15) & 0x7;
			uint32 dstSelW = (word1 >> 18) & 0x7;

			uint32 coordTypeX = (word1 >> 28) & 1;
			uint32 coordTypeY = (word1 >> 29) & 1;
			uint32 coordTypeZ = (word1 >> 30) & 1;
			uint32 coordTypeW = (word1 >> 31) & 1;
			cemu_assert_debug(coordTypeX != GPU7_TEX_UNNORMALIZED);
			cemu_assert_debug(coordTypeY != GPU7_TEX_UNNORMALIZED);
			cemu_assert_debug(coordTypeZ != GPU7_TEX_UNNORMALIZED);
			cemu_assert_debug(coordTypeW != GPU7_TEX_UNNORMALIZED);

			uint32 srcSelX = (word2 >> 20) & 0x7;
			uint32 srcSelY = (word2 >> 23) & 0x7;
			uint32 srcSelZ = (word2 >> 26) & 0x7;
			uint32 srcSelW = (word2 >> 29) & 0x7;

			uint32 offsetX = (word2 >> 0) & 0x1F;
			uint32 offsetY = (word2 >> 5) & 0x1F;
			uint32 offsetZ = (word2 >> 10) & 0x1F;
			cemu_assert_debug(offsetX == 0);
			cemu_assert_debug(offsetY == 0);
			cemu_assert_debug(offsetZ == 0);

			// create new tex instruction
			LatteDecompilerTEXInstruction texInstruction;
			texInstruction.cfInstruction = cfInstruction;
			texInstruction.opcode = inst0_4;
			texInstruction.dstSel[0] = dstSelX;
			texInstruction.dstSel[1] = dstSelY;
			texInstruction.dstSel[2] = dstSelZ;
			texInstruction.dstSel[3] = dstSelW;
			texInstruction.textureFetch.srcSel[0] = srcSelX;
			texInstruction.textureFetch.srcSel[1] = srcSelY;
			texInstruction.textureFetch.srcSel[2] = srcSelZ;
			texInstruction.textureFetch.srcSel[3] = srcSelW;
			texInstruction.dstGpr = destGpr;
			texInstruction.srcGpr = srcGpr;
			cfInstruction->instructionsTEX.emplace_back(texInstruction);
		}
		else if (inst0_4 == GPU7_TEX_INST_SET_GRADIENTS_H || inst0_4 == GPU7_TEX_INST_SET_GRADIENTS_V)
		{
			uint32 bufferId = (word0 >> 8) & 0xFF;
			uint32 samplerId = (word2 >> 15) & 0x1F;
			uint32 srcGpr = (word0 >> 16) & 0x7F;
			uint32 srcRel = (word0 >> 23) & 1;
			if (srcRel != 0)
				debugBreakpoint();
			uint32 coordTypeX = (word1 >> 28) & 1;
			uint32 coordTypeY = (word1 >> 29) & 1;
			uint32 coordTypeZ = (word1 >> 30) & 1;
			uint32 coordTypeW = (word1 >> 31) & 1;
			cemu_assert_debug(coordTypeX != GPU7_TEX_UNNORMALIZED);
			cemu_assert_debug(coordTypeY != GPU7_TEX_UNNORMALIZED);
			cemu_assert_debug(coordTypeZ != GPU7_TEX_UNNORMALIZED);
			cemu_assert_debug(coordTypeW != GPU7_TEX_UNNORMALIZED);

			uint32 srcSelX = (word2 >> 20) & 0x7;
			uint32 srcSelY = (word2 >> 23) & 0x7;
			uint32 srcSelZ = (word2 >> 26) & 0x7;
			uint32 srcSelW = (word2 >> 29) & 0x7;

			sint32 textureIndex = bufferId - 0x00;
			// create new tex instruction
			LatteDecompilerTEXInstruction texInstruction;
			texInstruction.cfInstruction = cfInstruction;
			texInstruction.opcode = inst0_4;
			texInstruction.textureFetch.textureIndex = textureIndex;
			texInstruction.textureFetch.samplerIndex = samplerId;
			texInstruction.textureFetch.srcSel[0] = srcSelX;
			texInstruction.textureFetch.srcSel[1] = srcSelY;
			texInstruction.textureFetch.srcSel[2] = srcSelZ;
			texInstruction.textureFetch.srcSel[3] = srcSelW;
			texInstruction.srcGpr = srcGpr;
			texInstruction.dstGpr = 0xFFFFFFFF;
			cfInstruction->instructionsTEX.emplace_back(texInstruction);
		}
		else if( inst0_4 == GPU7_TEX_INST_VFETCH )
		{
			// this uses the VTX_WORD* encoding
			uint32 fetchType = (word0>>5)&3;
			uint32 bufferId = (word0>>8)&0xFF;
			uint32 offset = (word2>>0)&0xFFFF;
			uint32 endianSwap = (word2>>16)&0x3;
			uint32 constNoStride = (word2>>18)&0x1;
			uint32 srcGpr = (word0>>16)&0x7F;
			uint32 srcRel = (word0>>23)&1;
			if( srcRel != 0 )
				debugBreakpoint();
			uint32 destGpr = (word1>>0)&0x7F;
			uint32 destRel = (word1>>7)&1;
			if( destRel != 0 )
				debugBreakpoint();
			uint32 dstSelX = (word1>>9)&0x7;
			uint32 dstSelY = (word1>>12)&0x7;
			uint32 dstSelZ = (word1>>15)&0x7;
			uint32 dstSelW = (word1>>18)&0x7;

			uint32 srcSelX = (word0>>24)&0x3;
			uint32 srcSelY = 0;
			uint32 srcSelZ = 0;
			uint32 srcSelW = 0;

			// create new tex instruction
			LatteDecompilerTEXInstruction texInstruction;
			texInstruction.cfInstruction = cfInstruction;
			texInstruction.opcode = inst0_4;
			texInstruction.textureFetch.textureIndex = bufferId;
			texInstruction.textureFetch.samplerIndex = 0;
			texInstruction.textureFetch.offset = offset;
			texInstruction.dstSel[0] = dstSelX;
			texInstruction.dstSel[1] = dstSelY;
			texInstruction.dstSel[2] = dstSelZ;
			texInstruction.dstSel[3] = dstSelW;
			texInstruction.textureFetch.srcSel[0] = srcSelX;
			texInstruction.textureFetch.srcSel[1] = srcSelY;
			texInstruction.textureFetch.srcSel[2] = srcSelZ;
			texInstruction.textureFetch.srcSel[3] = srcSelW;
			texInstruction.dstGpr = destGpr;
			texInstruction.srcGpr = srcGpr;
			cfInstruction->instructionsTEX.emplace_back(texInstruction);
		}
		else if (inst0_4 == GPU7_TEX_INST_MEM)
		{
			// memory access
			// MEM_RD_WORD0
			uint32 elementSize = (word0 >> 5) & 3;
			uint32 memOp = (word0 >> 8) & 7;
			uint8 indexed = (word0 >> 12) & 1;
			uint32 srcGPR = (word0 >> 16) & 0x7F;
			uint8 srcREL = (word0 >> 23) & 1;
			uint8 srcSelX = (word0 >> 24) & 3;
			// MEM_RD_WORD1
			uint32 dstGPR = (word1 >> 0) & 0x7F;
			uint8 dstREL = (word1 >> 7) & 1;
			uint8 dstSelX = (word1 >> 9) & 7;
			uint8 dstSelY = (word1 >> 12) & 7;
			uint8 dstSelZ = (word1 >> 15) & 7;
			uint8 dstSelW = (word1 >> 18) & 7;
			uint8 dataFormat = (word1 >> 22) & 0x3F;
			uint8 nfa = (word1 >> 28) & 3;
			uint8 isSigned = (word1 >> 30) & 1;
			uint8 srfMode = (word1 >> 31) & 1;
			// MEM_RD_WORD2
			uint32 arrayBase = (word2 & 0x1FFF);
			uint8 endianSwap = (word2 >> 16) & 3;
			uint32 arraySize = (word2 >> 20) & 0xFFF;
			if (memOp == 2)
			{
				// read from scatter buffer (SSBO)
				LatteDecompilerTEXInstruction texInstruction;
				texInstruction.cfInstruction = cfInstruction;
				texInstruction.opcode = inst0_4;

				cemu_assert_debug(srcREL == 0 || dstREL == 0); // unsupported relative access

				texInstruction.memRead.arrayBase = arrayBase;
				texInstruction.srcGpr = srcGPR;
				texInstruction.dstGpr = dstGPR;
				texInstruction.memRead.srcSelX = srcSelX;
				texInstruction.dstSel[0] = dstSelX;
				texInstruction.dstSel[1] = dstSelY;
				texInstruction.dstSel[2] = dstSelZ;
				texInstruction.dstSel[3] = dstSelW;

				texInstruction.memRead.format = dataFormat;
				texInstruction.memRead.nfa = nfa;
				texInstruction.memRead.isSigned = isSigned;
	
				cfInstruction->instructionsTEX.emplace_back(texInstruction);
			}
			else
			{
				cemu_assert_unimplemented();
			}
		}
		else
		{
			cemuLog_logDebug(LogType::Force, "Unsupported tex instruction {}", inst0_4);
			shaderContext->hasError = true;
			break;
		}
	}
	cemu_assert_debug(cfInstruction->instructionsALU.empty()); // clause may only contain texture instructions
}

// iterate all CF instructions and parse clause sub-instructions (if present)
void LatteDecompiler_ParseClauses(LatteDecompilerShaderContext* decompilerContext, uint8* programData, uint32 programSize, std::vector<LatteDecompilerCFInstruction> &list_instructions)
{
	LatteDecompilerShader* shader = decompilerContext->shader;
	for (auto& cfInstruction : list_instructions)
	{
		if (cfInstruction.type == GPU7_CF_INST_ALU || cfInstruction.type == GPU7_CF_INST_ALU_PUSH_BEFORE || cfInstruction.type == GPU7_CF_INST_ALU_POP_AFTER || cfInstruction.type == GPU7_CF_INST_ALU_POP2_AFTER || cfInstruction.type == GPU7_CF_INST_ALU_BREAK || cfInstruction.type == GPU7_CF_INST_ALU_ELSE_AFTER)
		{
			LatteDecompiler_ParseALUClause(shader, &cfInstruction, programData, programSize);
		}
		else if (cfInstruction.type == GPU7_CF_INST_TEX)
		{
			LatteDecompiler_ParseTEXClause(shader, &cfInstruction, programData, programSize);
		}
		else if (cfInstruction.type == GPU7_CF_INST_EXPORT || cfInstruction.type == GPU7_CF_INST_EXPORT_DONE)
		{
			// no sub-instructions
		}
		else if (cfInstruction.type == GPU7_CF_INST_ELSE || cfInstruction.type == GPU7_CF_INST_POP)
		{
			// no sub-instructions
		}
		else if (cfInstruction.type == GPU7_CF_INST_LOOP_START_DX10 || cfInstruction.type == GPU7_CF_INST_LOOP_END ||
				 cfInstruction.type == GPU7_CF_INST_LOOP_START_NO_AL)
		{
			// no sub-instructions
		}
		else if (cfInstruction.type == GPU7_CF_INST_LOOP_BREAK)
		{
			// no sub-instructions
		}
		else if (cfInstruction.type == GPU7_CF_INST_MEM_STREAM0_WRITE ||
			cfInstruction.type == GPU7_CF_INST_MEM_STREAM1_WRITE)
		{
			// no sub-instructions
		}
		else if (cfInstruction.type == GPU7_CF_INST_MEM_RING_WRITE)
		{
			// no sub-instructions
		}
		else if (cfInstruction.type == GPU7_CF_INST_CALL)
		{
			// no sub-instructions
		}
		else if (cfInstruction.type == GPU7_CF_INST_RETURN)
		{
			// no sub-instructions
		}
		else if (cfInstruction.type == GPU7_CF_INST_EMIT_VERTEX)
		{
			// no sub-instructions
		}
		else
		{
			debug_printf("_parseClauses(): Unsupported clause 0x%x\n", cfInstruction.type);
			cemu_assert_unimplemented();
		}
	}
}

// iterate all CF instructions and parse sub-instructions
void LatteDecompiler_ParseClauses(LatteDecompilerShaderContext* shaderContext, uint8* programData, uint32 programSize)
{
	LatteDecompilerShader* shader = shaderContext->shader;
	LatteDecompiler_ParseClauses(shaderContext, programData, programSize, shaderContext->cfInstructions);
	// parse subroutines
	for (auto& subroutineInfo : shaderContext->list_subroutines)
	{
		LatteDecompiler_ParseClauses(shaderContext, programData, programSize, subroutineInfo.instructions);
	}
}

void _LatteDecompiler_GenerateDataForFastAccess(LatteDecompilerShader* shader)
{
	if (shader->hasError)
		return;
	for (size_t i = 0; i < shader->list_remappedUniformEntries.size(); i++)
	{
		LatteDecompilerRemappedUniformEntry_t* entry = shader->list_remappedUniformEntries.data() + i;
		if (entry->isRegister)
		{
			LatteFastAccessRemappedUniformEntry_register_t entryReg;
			entryReg.indexOffset = entry->index * 16;
			entryReg.mappedIndexOffset = entry->mappedIndex * 16;
			shader->list_remappedUniformEntries_register.push_back(entryReg);
		}
		else
		{
			LatteFastAccessRemappedUniformEntry_buffer_t entryBuf;
			uint32 kcacheBankIdOffset = entry->kcacheBankId* (7 * 4);
			entryBuf.indexOffset = entry->index * 16;
			entryBuf.mappedIndexOffset = entry->mappedIndex * 16;
			// find or create buffer group
			auto bufferGroup = std::find_if(shader->list_remappedUniformEntries_bufferGroups.begin(), shader->list_remappedUniformEntries_bufferGroups.end(), [kcacheBankIdOffset](const LatteDecompilerShader::_RemappedUniformBufferGroup& v) { return v.kcacheBankIdOffset == kcacheBankIdOffset; });
			if (bufferGroup != shader->list_remappedUniformEntries_bufferGroups.end())
			{
				(*bufferGroup).entries.emplace_back(entryBuf);
			}
			else
			{
				shader->list_remappedUniformEntries_bufferGroups.emplace_back(kcacheBankIdOffset).entries.emplace_back(entryBuf);
			}
		}
	}
}

void _LatteDecompiler_Process(LatteDecompilerShaderContext* shaderContext, uint8* programData, uint32 programSize)
{
	// parse control flow instructions
	if (shaderContext->shader->hasError == false)
		LatteDecompiler_ParseCF(shaderContext, programData, programSize);
	// parse individual clauses
	if (shaderContext->shader->hasError == false)
		LatteDecompiler_ParseClauses(shaderContext, programData, programSize);
	// analyze
	if (shaderContext->shader->hasError == false)
		LatteDecompiler_analyze(shaderContext, shaderContext->shader);
	if (shaderContext->shader->hasError == false)
		LatteDecompiler_analyzeDataTypes(shaderContext);
	// emit code
	if (shaderContext->shader->hasError == false)
		LatteDecompiler_emitGLSLShader(shaderContext, shaderContext->shader);
	LatteDecompiler_cleanup(shaderContext);
	// fast access 
	_LatteDecompiler_GenerateDataForFastAccess(shaderContext->shader);
}

void LatteDecompiler_InitContext(LatteDecompilerShaderContext& dCtx, const LatteDecompilerOptions& options, LatteDecompilerOutput_t* output, LatteConst::ShaderType shaderType, uint64 shaderBaseHash, uint32* contextRegisters)
{
	dCtx.output = output;
	dCtx.shaderType = shaderType;
	dCtx.options = &options;
	dCtx.shaderBaseHash = shaderBaseHash;
	dCtx.contextRegisters = contextRegisters;
	dCtx.contextRegistersNew = (LatteContextRegister*)contextRegisters;
	output->shaderType = shaderType;
}

void LatteDecompiler_DecompileVertexShader(uint64 shaderBaseHash, uint32* contextRegisters, uint8* programData, uint32 programSize, struct LatteFetchShader* fetchShader, LatteDecompilerOptions& options, LatteDecompilerOutput_t* output)
{
	cemu_assert_debug(fetchShader);
	cemu_assert_debug((programSize & 3) == 0);
	performanceMonitor.gpuTime_shaderCreate.beginMeasuring();
	// prepare decompiler context
	LatteDecompilerShaderContext shaderContext = { 0 };
	LatteDecompiler_InitContext(shaderContext, options, output, LatteConst::ShaderType::Vertex, shaderBaseHash, contextRegisters);
	shaderContext.fetchShader = fetchShader;
	// prepare shader (deprecated)
	LatteDecompilerShader* shader = new LatteDecompilerShader(LatteConst::ShaderType::Vertex);
	shader->compatibleFetchShader = shaderContext.fetchShader;
	output->shaderType = LatteConst::ShaderType::Vertex;
	shaderContext.shader = shader;
	output->shader = shader;
	for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
	{
		shader->textureUnitSamplerAssignment[i] = LATTE_DECOMPILER_SAMPLER_NONE;
		shader->textureUsesDepthCompare[i] = false;
	}
	// parse & compile
	_LatteDecompiler_Process(&shaderContext, programData, programSize);
	performanceMonitor.gpuTime_shaderCreate.endMeasuring();
}

void LatteDecompiler_DecompileGeometryShader(uint64 shaderBaseHash, uint32* contextRegisters, uint8* programData, uint32 programSize, uint8* gsCopyProgramData, uint32 gsCopyProgramSize, uint32 vsRingParameterCount, LatteDecompilerOptions& options, LatteDecompilerOutput_t* output)
{
	cemu_assert_debug((programSize & 3) == 0);
	performanceMonitor.gpuTime_shaderCreate.beginMeasuring();
	// prepare decompiler context
	LatteDecompilerShaderContext shaderContext = { 0 };
	LatteDecompiler_InitContext(shaderContext, options, output, LatteConst::ShaderType::Geometry, shaderBaseHash, contextRegisters);
	// prepare shader
	LatteDecompilerShader* shader = new LatteDecompilerShader(LatteConst::ShaderType::Geometry);
	shader->ringParameterCountFromPrevStage = vsRingParameterCount;
	output->shaderType = LatteConst::ShaderType::Geometry;
	shaderContext.shader = shader;
	output->shader = shader;
	if (gsCopyProgramData == NULL)
	{
		shader->hasError = true;
	}
	else
	{
		shaderContext.parsedGSCopyShader = LatteGSCopyShaderParser_parse(gsCopyProgramData, gsCopyProgramSize);
	}
	for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
	{
		shader->textureUnitSamplerAssignment[i] = LATTE_DECOMPILER_SAMPLER_NONE;
		shader->textureUsesDepthCompare[i] = false;
	}
	// parse & compile
	_LatteDecompiler_Process(&shaderContext, programData, programSize);
	performanceMonitor.gpuTime_shaderCreate.endMeasuring();
}

void LatteDecompiler_DecompilePixelShader(uint64 shaderBaseHash, uint32* contextRegisters, uint8* programData, uint32 programSize, LatteDecompilerOptions& options, LatteDecompilerOutput_t* output)
{
	cemu_assert_debug((programSize & 3) == 0);
	performanceMonitor.gpuTime_shaderCreate.beginMeasuring();
	// prepare decompiler context
	LatteDecompilerShaderContext shaderContext = { 0 };
	LatteDecompiler_InitContext(shaderContext, options, output, LatteConst::ShaderType::Pixel, shaderBaseHash, contextRegisters);
	shaderContext.contextRegisters = contextRegisters;
	// prepare shader
	LatteDecompilerShader* shader = new LatteDecompilerShader(LatteConst::ShaderType::Pixel);
	output->shaderType = LatteConst::ShaderType::Pixel;
	shaderContext.shader = shader;
	output->shader = shader;
	for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
	{
		shader->textureUnitSamplerAssignment[i] = LATTE_DECOMPILER_SAMPLER_NONE;
		shader->textureUsesDepthCompare[i] = false;
	}
	// parse & compile
	_LatteDecompiler_Process(&shaderContext, programData, programSize);
	performanceMonitor.gpuTime_shaderCreate.endMeasuring();
}

void LatteDecompiler_cleanup(LatteDecompilerShaderContext* shaderContext)
{
	shaderContext->cfInstructions.clear();
}
