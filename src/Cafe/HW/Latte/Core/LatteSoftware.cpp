#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Core/LatteShaderAssembly.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerInstructions.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"

#include "Cafe/HW/Latte/Core/LattePM4.h"

#define GPU7_ENDIAN_8IN32 2

typedef struct  
{
	union
	{
		float f[4];
		uint32 u32[4];
		sint32 s32[4];
	};
}LatteReg_t;

#define REG_AR	(128)

typedef struct  
{
	LatteReg_t reg[128+1];
	union
	{
		uint32 u32[5];
		sint32 s32[5];
		float f[5];
	}pvps;
	union
	{
		uint32 u32[5];
		sint32 s32[5];
		float f[5];
	}pvpsUpdate;
	void* cfilePtr;
	LatteReg_t* literalPtr;
	// cbank
	LatteReg_t* cbank0;
	LatteReg_t* cbank1;
	// vertex shader exports
	LatteReg_t export_pos;

	uint32* shaderBase;
	sint32 shaderSize;
	// shaders
	LatteFetchShader* fetchShader;
}LatteSoftwareExecContext_t;

LatteSoftwareExecContext_t LatteSWCtx;

char _tempStringBuf[2048];
sint32 tempStringIndex = 0;

char* getTempString()
{
	tempStringIndex = (tempStringIndex + 1) % 10;
	return _tempStringBuf + tempStringIndex * (sizeof(_tempStringBuf) / 10);
}

const char* _getSrcName(uint32 srcSel, uint32 srcChan)
{
	if (GPU7_ALU_SRC_IS_GPR(srcSel))
	{
		char* tempStr = getTempString();
		sprintf(tempStr, "R%d", srcSel & 0x7F);
		if (srcChan == 0)
			strcat(tempStr, ".x");
		else if (srcChan == 1)
			strcat(tempStr, ".y");
		else if (srcChan == 2)
			strcat(tempStr, ".z");
		else
			strcat(tempStr, ".w");
		return tempStr;
	}
	else if (GPU7_ALU_SRC_IS_CFILE(srcSel))
	{
		return "CFILE";
	}
	else if (GPU7_ALU_SRC_IS_PV(srcSel))
	{
		return "PV";
	}
	else if (GPU7_ALU_SRC_IS_PS(srcSel))
	{
		return "PS";
	}
	else if (GPU7_ALU_SRC_IS_CBANK0(srcSel))
	{
		return "CBANK0";
	}
	else if (GPU7_ALU_SRC_IS_CBANK1(srcSel))
	{
		return "CBANK1";
	}
	else if (GPU7_ALU_SRC_IS_CONST_0F(srcSel))
	{
		return "0.0";
	}
	else if (GPU7_ALU_SRC_IS_CONST_1F(srcSel))
	{
		return "1.0";
	}
	else if (GPU7_ALU_SRC_IS_CONST_0_5F(srcSel))
	{
		return "0.5";
	}
	else if (GPU7_ALU_SRC_IS_LITERAL(srcSel))
	{
		return "LITERAL";
	}
	else
	{
		cemu_assert_unimplemented();
	}
	return "UKN";
}

sint32 _getSrc_genericS32(uint32 srcSel, uint32 srcChan, uint32 srcRel, uint32 indexMode)
{
	sint32 v = 0;
	if (GPU7_ALU_SRC_IS_GPR(srcSel))
	{
		cemu_assert_debug(srcRel == 0);
		v = LatteSWCtx.reg[GPU7_ALU_SRC_GET_GPR_INDEX(srcSel)].s32[srcChan];
	}
	else if (GPU7_ALU_SRC_IS_CFILE(srcSel))
	{
		if (srcRel)
		{
			if (indexMode <= GPU7_INDEX_AR_W)
			{
				v = ((sint32*)LatteSWCtx.cfilePtr)[LatteSWCtx.reg[REG_AR].s32[indexMode] * 4 + GPU7_ALU_SRC_GET_CFILE_INDEX(srcSel) * 4 + srcChan];
			}
			else
				cemu_assert_debug(false);
		}
		else
		{
			v = ((sint32*)LatteSWCtx.cfilePtr)[GPU7_ALU_SRC_GET_CFILE_INDEX(srcSel) * 4 + srcChan];
		}
	}
	else if (GPU7_ALU_SRC_IS_PV(srcSel))
	{
		cemu_assert_debug(srcRel == 0);
		v = LatteSWCtx.pvps.s32[srcChan];
	}
	else if (GPU7_ALU_SRC_IS_PS(srcSel))
	{
		cemu_assert_debug(srcRel == 0);
		v = LatteSWCtx.pvps.s32[4];
	}
	else if (GPU7_ALU_SRC_IS_CBANK0(srcSel))
	{
		if (srcRel)
		{
			if (indexMode <= GPU7_INDEX_AR_W)
			{
				v = LatteSWCtx.cbank0[LatteSWCtx.reg[REG_AR].s32[indexMode] + GPU7_ALU_SRC_GET_CBANK0_INDEX(srcSel)].s32[srcChan];
			}
			else
				assert_dbg();
		}
		else
		{
			v = LatteSWCtx.cbank0[GPU7_ALU_SRC_GET_CBANK0_INDEX(srcSel)].s32[srcChan];
		}
	}
	else if (GPU7_ALU_SRC_IS_CBANK1(srcSel))
	{
		if (srcRel)
		{
			if (indexMode <= GPU7_INDEX_AR_W)
			{
				v = LatteSWCtx.cbank1[LatteSWCtx.reg[REG_AR].s32[indexMode] + GPU7_ALU_SRC_GET_CBANK1_INDEX(srcSel)].s32[srcChan];
			}
			else
				assert_dbg();
		}
		else
		{
			v = LatteSWCtx.cbank1[GPU7_ALU_SRC_GET_CBANK1_INDEX(srcSel)].s32[srcChan];
		}
	}
	else if (GPU7_ALU_SRC_IS_CONST_0F(srcSel))
	{
		cemu_assert_debug(srcRel == 0);
		v = 0; // 0.0f
	}
	else if (GPU7_ALU_SRC_IS_CONST_1F(srcSel))
	{
		cemu_assert_debug(srcRel == 0);
		v = 0x3f800000; // 1.0f
	}
	else if (GPU7_ALU_SRC_IS_CONST_0_5F(srcSel))
	{
		cemu_assert_debug(srcRel == 0);
		v = 0x3f000000; // 0.5f
	}
	else if (GPU7_ALU_SRC_IS_LITERAL(srcSel))
	{
		v = LatteSWCtx.literalPtr->s32[srcChan];
	}
	else
		assert_dbg();
	return v;
}

sint32 _getSrc_s32(uint32 srcSel, uint32 srcChan, uint32 srcNeg, uint32 srcAbs, uint32 srcRel, uint32 indexMode)
{
	sint32 v = _getSrc_genericS32(srcSel, srcChan, srcRel, indexMode);
	cemu_assert_debug(srcNeg == 0);
	cemu_assert_debug(srcAbs == 0);
	return v;
}

float _getSrc_f(uint32 srcSel, uint32 srcChan, uint32 srcNeg, uint32 srcAbs, uint32 srcRel, uint32 indexMode)
{
	float v = 0;
	*(sint32*)&v = _getSrc_genericS32(srcSel, srcChan, srcRel, indexMode);
	if (srcAbs) // todo - how does this interact with srcNeg
		v = fabs(v);
	if (srcNeg)
		v = -v;
	return v;
}

#define _updateGPR_S32(__gprIdx,__channel,__v) {gprUpdate[updateQueueLength].gprIndex = __gprIdx; gprUpdate[updateQueueLength].channel = __channel; gprUpdate[updateQueueLength].s32 = __v; updateQueueLength++;}
#define _updateGPR_F(__gprIdx,__channel,__v) {gprUpdate[updateQueueLength].gprIndex = __gprIdx; gprUpdate[updateQueueLength].channel = __channel; gprUpdate[updateQueueLength].f = __v; updateQueueLength++;}

#define _updatePVPS_S32(__pvIndex, __v) {LatteSWCtx.pvpsUpdate.s32[__pvIndex] = __v;}
#define _updatePVPS_F(__pvIndex, __v) {LatteSWCtx.pvpsUpdate.f[__pvIndex] = __v;}

float LatteSoftware_omod(uint32 omod, float f)
{
	switch (omod)
	{
	case ALU_OMOD_NONE:
		return f;
	case ALU_OMOD_MUL2:
		return f * 2.0f;
	case ALU_OMOD_MUL4:
		return f * 4.0f;
	case ALU_OMOD_DIV2:
		return f / 2.0f;
	}
	cemu_assert_debug(false);
	return 0.0f;
}

#ifdef CEMU_DEBUG_ASSERT
#define _clamp(__v) if(destClamp != 0) cemu_assert_unimplemented()
#else
#define _clamp(__v) // todo
#endif

#define _omod(__v) __v = LatteSoftware_omod(omod, __v)

bool LatteDecompiler_IsALUTransInstruction(bool isOP3, uint32 opcode);

void LatteSoftware_setupCBankPointers(uint32 cBank0Index, uint32 cBank1Index, uint32 cBank0AddrBase, uint32 cBank1AddrBase)
{
	MPTR cBank0Ptr = LatteGPUState.contextRegister[mmSQ_VTX_UNIFORM_BLOCK_START + cBank0Index * 7 + 0];
	MPTR cBank1Ptr = LatteGPUState.contextRegister[mmSQ_VTX_UNIFORM_BLOCK_START + cBank1Index * 7 + 0];
	LatteSWCtx.cbank0 = (LatteReg_t*)memory_getPointerFromPhysicalOffset(cBank0Ptr + cBank0AddrBase);
	LatteSWCtx.cbank1 = (LatteReg_t*)memory_getPointerFromPhysicalOffset(cBank1Ptr + cBank1AddrBase);
}

void LatteSoftware_executeALUClause(uint32 cfType, uint32 addr, uint32 count, uint32 cBank0Index, uint32 cBank1Index, uint32 cBank0AddrBase, uint32 cBank1AddrBase)
{
	cemu_assert_debug(cfType == GPU7_CF_INST_ALU); // todo - handle other ALU clauses
	uint32* aluWordPtr = LatteSWCtx.shaderBase + addr * 2;
	uint32* aluWordPtrEnd = aluWordPtr + count * 2;

	LatteSoftware_setupCBankPointers(cBank0Index, cBank1Index, cBank0AddrBase, cBank1AddrBase);

	struct  
	{
		sint16 gprIndex;
		sint16 channel;
		union
		{
			float f;
			uint32 u32;
			sint32 s32;
		};
	}gprUpdate[16];

	sint32 updateQueueLength = 0;
	uint32 aluUnitWriteMask = 0;
	uint8 literalAccessMask = 0;
	while (aluWordPtr < aluWordPtrEnd)
	{
		// calculate number of instructions in group
		sint32 groupInstructionCount;
		for (sint32 i = 0; i < 6; i++)
		{
			if (aluWordPtr[i * 2] & 0x80000000)
			{
				groupInstructionCount = i + 1;
				break;
			}
			cemu_assert_debug(i < 5);
		}
		LatteSWCtx.literalPtr = (LatteReg_t*)(aluWordPtr + groupInstructionCount*2);
		// process group
		bool hasReductionInstruction = false;
		float reductionResult = 0.0f;
		for (sint32 s = 0; s < groupInstructionCount; s++)
		{
			uint32 aluWord0 = aluWordPtr[0];
			uint32 aluWord1 = aluWordPtr[1];
			aluWordPtr += 2;
			uint32 alu_inst13_5 = (aluWord1 >> 13) & 0x1F;
			// parameters from ALU word 0 (shared for ALU OP2 and OP3)
			uint32 src0Sel = (aluWord0 >> 0) & 0x1FF; // source selection
			uint32 src1Sel = (aluWord0 >> 13) & 0x1FF;
			uint32 src0Rel = (aluWord0 >> 9) & 0x1; // relative addressing mode
			uint32 src1Rel = (aluWord0 >> 22) & 0x1;
			uint32 src0Chan = (aluWord0 >> 10) & 0x3; // component selection x/y/z/w
			uint32 src1Chan = (aluWord0 >> 23) & 0x3;
			uint32 src0Neg = (aluWord0 >> 12) & 0x1; // negate input
			uint32 src1Neg = (aluWord0 >> 25) & 0x1;
			uint32 indexMode = (aluWord0 >> 26) & 7;
			uint32 predSel = (aluWord0 >> 29) & 3;
			if (GPU7_ALU_SRC_IS_LITERAL(src0Sel))
				literalAccessMask |= (1 << src0Chan);
			if (GPU7_ALU_SRC_IS_LITERAL(src1Sel))
				literalAccessMask |= (1 << src1Chan);

			if (alu_inst13_5 >= 0x8)
			{
				// op3
				// parameters from ALU word 1
				uint32 src2Sel = (aluWord1 >> 0) & 0x1FF; // source selection
				uint32 src2Rel = (aluWord1 >> 9) & 0x1; // relative addressing mode
				uint32 src2Chan = (aluWord1 >> 10) & 0x3; // component selection x/y/z/w
				uint32 src2Neg = (aluWord1 >> 12) & 0x1; // negate input
				if (GPU7_ALU_SRC_IS_LITERAL(src2Sel))
					literalAccessMask |= (1 << src2Chan);

				uint32 destGpr = (aluWord1 >> 21) & 0x7F;
				uint32 destRel = (aluWord1 >> 28) & 1;
				uint32 destElem = (aluWord1 >> 29) & 3;
				uint32 destClamp = (aluWord1 >> 31) & 1;

				uint32 aluUnit = destElem;
				if (aluUnitWriteMask&(1 << destElem))
				{
					aluUnit = 4; // PV
				}
				else
					aluUnitWriteMask |= (1 << destElem);

				switch (alu_inst13_5)
				{
				case ALU_OP3_INST_CMOVE:
				{
					float f = _getSrc_f(src0Sel, src0Chan, src0Neg, 0, src0Rel, indexMode);
					sint32 result;
					if (f == 0.0f)
						result = _getSrc_s32(src1Sel, src1Chan, src1Neg, 0, src1Rel, indexMode);
					else
						result = _getSrc_s32(src2Sel, src2Chan, src2Neg, 0, src2Rel, indexMode);
					cemu_assert_debug(destClamp == 0);
					_updateGPR_S32(destGpr, destElem, result);
					_updatePVPS_S32(aluUnit, result);
					break;
				}
				case ALU_OP3_INST_CMOVGT:
				{
					float f = _getSrc_f(src0Sel, src0Chan, src0Neg, 0, src0Rel, indexMode);
					sint32 result;
					if (f > 0.0f)
						result = _getSrc_s32(src1Sel, src1Chan, src1Neg, 0, src1Rel, indexMode);
					else
						result = _getSrc_s32(src2Sel, src2Chan, src2Neg, 0, src2Rel, indexMode);
					cemu_assert_debug(destClamp == 0);
					_updateGPR_S32(destGpr, destElem, result);
					_updatePVPS_S32(aluUnit, result);
					break;
				}
				case ALU_OP3_INST_MULADD:
				{
					float f0 = _getSrc_f(src0Sel, src0Chan, src0Neg, 0, src0Rel, indexMode);
					float f1 = _getSrc_f(src1Sel, src1Chan, src1Neg, 0, src1Rel, indexMode);
					float f2 = _getSrc_f(src2Sel, src2Chan, src2Neg, 0, src2Rel, indexMode);
					float f = f0*f1+f2;
					_updateGPR_F(destGpr, destElem, f);
					_updatePVPS_F(aluUnit, f);
					break;
				}
				default:
					cemu_assert_debug(false);
				}
			}
			else
			{
				uint32 alu_inst7_11 = (aluWord1 >> 7) & 0x7FF;

				uint32 src0Abs = (aluWord1 >> 0) & 1;
				uint32 src1Abs = (aluWord1 >> 1) & 1;
				uint32 updateExecuteMask = (aluWord1 >> 2) & 1;
				uint32 updatePredicate = (aluWord1 >> 3) & 1;
				uint32 writeMask = (aluWord1 >> 4) & 1;
				uint32 omod = (aluWord1 >> 5) & 3;

				uint32 destGpr = (aluWord1 >> 21) & 0x7F;
				uint32 destRel = (aluWord1 >> 28) & 1;
				uint32 destElem = (aluWord1 >> 29) & 3;
				uint32 destClamp = (aluWord1 >> 31) & 1;

				uint32 aluUnit = destElem;
				if (LatteDecompiler_IsALUTransInstruction(false, alu_inst7_11))
					aluUnit = 4;

				if (aluUnitWriteMask&(1 << destElem))
				{
					aluUnit = 4; // PV
				}
				else
					aluUnitWriteMask |= (1 << destElem);

				switch (alu_inst7_11)
				{
				case ALU_OP2_INST_NOP:
				{
					break;
				}
				case ALU_OP2_INST_MOV:
				{
					if (src0Neg || src0Abs || omod != 0)
					{
						float v = _getSrc_f(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
						_omod(v);
						_clamp(f);
						if (writeMask)
							_updateGPR_F(destGpr, destElem, v);
						_updatePVPS_F(aluUnit, v);
					}
					else
					{
						sint32 v = _getSrc_s32(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
						// todo - omod/clamp for float moves
						if (writeMask)
							_updateGPR_S32(destGpr, destElem, v);
						_updatePVPS_S32(aluUnit, v);
					}
					break;
				}
				case ALU_OP2_INST_ADD:
				{
					float f0 = _getSrc_f(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
					float f1 = _getSrc_f(src1Sel, src1Chan, src1Neg, src1Abs, src1Rel, indexMode);
					float f = f0 + f1;
					_omod(f);
					_clamp(f);
					if (writeMask)
						_updateGPR_F(destGpr, destElem, f);
					_updatePVPS_F(aluUnit, f);
					break;
				}
				case ALU_OP2_INST_MUL:
				case ALU_OP2_INST_MUL_IEEE:
				{
					float f0 = _getSrc_f(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
					float f1 = _getSrc_f(src1Sel, src1Chan, src1Neg, src1Abs, src1Rel, indexMode);
					float f = f0 * f1;
					if (f0 == 0.0f || f1 == 0.0f)
						f = 0.0f;
					_omod(f);
					_clamp(f);
					if (writeMask)
						_updateGPR_F(destGpr, destElem, f);
					_updatePVPS_F(aluUnit, f);
					break;
				}
				case ALU_OP2_INST_TRUNC:
				{
					float f0 = _getSrc_f(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
					float f = truncf(f0);
					_omod(f);
					_clamp(f);
					if (writeMask)
						_updateGPR_F(destGpr, destElem, f);
					_updatePVPS_F(aluUnit, f);
					break;
				}
				case ALU_OP2_INST_RECIP_IEEE:
				{
					float f0 = _getSrc_f(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
					float f = 1.0f / f0;
					_omod(f);
					_clamp(f);
					if (writeMask)
						_updateGPR_F(destGpr, destElem, f);
					_updatePVPS_F(aluUnit, f);
					break;
				}
				case ALU_OP2_INST_SETGT:
				{
					float f0 = _getSrc_f(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
					float f1 = _getSrc_f(src1Sel, src1Chan, src1Neg, src1Abs, src1Rel, indexMode);
					float f = (f0 > f1) ? 1.0f : 0.0f;
					_omod(f);
					_clamp(f);
					if (writeMask)
						_updateGPR_F(destGpr, destElem, f);
					_updatePVPS_F(aluUnit, f);
					break;
				}
				case ALU_OP2_INST_SETGE:
				{
					float f0 = _getSrc_f(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
					float f1 = _getSrc_f(src1Sel, src1Chan, src1Neg, src1Abs, src1Rel, indexMode);
					float f = (f0 >= f1) ? 1.0f : 0.0f;
					_omod(f);
					_clamp(f);
					if (writeMask)
						_updateGPR_F(destGpr, destElem, f);
					_updatePVPS_F(aluUnit, f);
					break;
				}
				case ALU_OP2_INST_INT_TO_FLOAT:
				{
					sint32 v = _getSrc_s32(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
					float f = (float)v;
					_omod(f);
					_clamp(f);
					if (writeMask)
						_updateGPR_F(destGpr, destElem, f);
					_updatePVPS_F(aluUnit, f);
					break;
				}
				case ALU_OP2_INST_FLT_TO_INT:
				{
					float v = _getSrc_f(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
					sint32 f = (sint32)v;
					if (writeMask)
						_updateGPR_S32(destGpr, destElem, f);
					_updatePVPS_S32(aluUnit, f);
					break;
				}
				case ALU_OP2_INST_MOVA_FLOOR:
				{
					float f = _getSrc_f(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
					f = floor(f);
					f = std::min(std::max(f, -256.0f), 255.0f);
					// omod, clamp?
					_updateGPR_S32(REG_AR, destElem, (sint32)f);
					if (writeMask)
						_updateGPR_F(destGpr, destElem, f);
					_updatePVPS_F(aluUnit, f);
					break;
				}
				case ALU_OP2_INST_DOT4:
				{
					float f0 = _getSrc_f(src0Sel, src0Chan, src0Neg, src0Abs, src0Rel, indexMode);
					float f1 = _getSrc_f(src1Sel, src1Chan, src1Neg, src1Abs, src1Rel, indexMode);
					float f = f0 * f1;
					reductionResult += f;
					_omod(f);
					_clamp(f);
					if (writeMask)
					{
						_updateGPR_F(destGpr, destElem, f);
					}
					_updatePVPS_F(aluUnit, f);
					hasReductionInstruction = true;
					break;
				}
				default:
					cemu_assert_debug(false);
				}
			}
		}
		// apply updates
		if (hasReductionInstruction == false)
		{
			for (sint32 i = 0; i < updateQueueLength; i++)
			{
				LatteSWCtx.reg[gprUpdate[i].gprIndex].s32[gprUpdate[i].channel] = gprUpdate[i].s32;
			}
			LatteSWCtx.pvps.s32[0] = LatteSWCtx.pvpsUpdate.s32[0];
			LatteSWCtx.pvps.s32[1] = LatteSWCtx.pvpsUpdate.s32[1];
			LatteSWCtx.pvps.s32[2] = LatteSWCtx.pvpsUpdate.s32[2];
			LatteSWCtx.pvps.s32[3] = LatteSWCtx.pvpsUpdate.s32[3];
			LatteSWCtx.pvps.s32[4] = LatteSWCtx.pvpsUpdate.s32[4];
		}
		else
		{
			for (sint32 i = 0; i < updateQueueLength; i++)
			{
				LatteSWCtx.reg[gprUpdate[i].gprIndex].f[gprUpdate[i].channel] = reductionResult;
			}
			LatteSWCtx.pvps.f[0] = reductionResult;
			LatteSWCtx.pvps.f[1] = reductionResult;
			LatteSWCtx.pvps.f[2] = reductionResult;
			LatteSWCtx.pvps.f[3] = reductionResult;
		}
		updateQueueLength = 0;
		// skip literals
		if (literalAccessMask&(3 << 0))
			aluWordPtr += 2;
		if (literalAccessMask&(3 << 2))
		{
			cemu_assert_debug((literalAccessMask &3) != 0);
			aluWordPtr += 2;
		}
		// reset group state tracking variables
		aluUnitWriteMask = 0;
		literalAccessMask = 0;
	}
}

sint32 _getRegValueByCompSel(uint32 gprIndex, uint32 compSel)
{
	if (compSel < 4)
		return LatteSWCtx.reg[gprIndex].s32[compSel];
	cemu_assert_unimplemented();
	return 0;
}

void LatteSoftware_singleRun()
{
	uint32* cfWords = LatteSWCtx.shaderBase;
	sint32 instructionIndex = 0;
	while (true)
	{
		uint32 cfWord0 = cfWords[instructionIndex + 0];
		uint32 cfWord1 = cfWords[instructionIndex + 1];
		instructionIndex += 2;

		uint32 cf_inst23_7 = (cfWord1 >> 23) & 0x7F;
		if (cf_inst23_7 < 0x40) // starting at 0x40 the bits overlap with the ALU instruction encoding
		{
			bool isEndOfProgram = ((cfWord1 >> 21) & 1) != 0;
			uint32 addr = cfWord0 & 0xFFFFFFFF;
			uint32 count = (cfWord1 >> 10) & 7;
			if (((cfWord1 >> 19) & 1) != 0)
				count |= 0x8;
			count++;
			if (cf_inst23_7 == GPU7_CF_INST_CALL_FS)
			{
				
			}
			else if (cf_inst23_7 == GPU7_CF_INST_NOP)
			{
				// nop
				if (((cfWord1 >> 0) & 7) != 0)
					cemu_assert_debug(false); // pop count is not zero
			}
			else if (cf_inst23_7 == GPU7_CF_INST_EXPORT || cf_inst23_7 == GPU7_CF_INST_EXPORT_DONE)
			{
				// export
				uint32 edType = (cfWord0 >> 13) & 0x3;
				uint32 edIndexGpr = (cfWord0 >> 23) & 0x7F;
				uint32 edRWRel = (cfWord0 >> 22) & 1;
				if (edRWRel != 0 || edIndexGpr != 0)
					cemu_assert_debug(false);

				uint8 exportComponentSel[4];
				exportComponentSel[0] = (cfWord1 >> 0) & 0x7;
				exportComponentSel[1] = (cfWord1 >> 3) & 0x7;
				exportComponentSel[2] = (cfWord1 >> 6) & 0x7;
				exportComponentSel[3] = (cfWord1 >> 9) & 0x7;
				uint32 exportArrayBase = (cfWord0 >> 0) & 0x1FFF;
				uint32 exportBurstCount = (cfWord1 >> 17) & 0xF;
				uint32 exportSourceGPR = (cfWord0 >> 15) & 0x7F;
				uint32 memWriteElemSize = (cfWord0>>29)&3; // unused

				cemu_assert_debug(exportBurstCount == 0);
				if (edType == 1 && exportArrayBase == GPU7_DECOMPILER_CF_EXPORT_BASE_POSITION)
				{
					LatteSWCtx.export_pos.s32[0] = _getRegValueByCompSel(exportSourceGPR, exportComponentSel[0]);
					LatteSWCtx.export_pos.s32[1] = _getRegValueByCompSel(exportSourceGPR, exportComponentSel[1]);
					LatteSWCtx.export_pos.s32[2] = _getRegValueByCompSel(exportSourceGPR, exportComponentSel[2]);
					LatteSWCtx.export_pos.s32[3] = _getRegValueByCompSel(exportSourceGPR, exportComponentSel[3]);
				}
				else
				{
					// unhandled export
					cemu_assert_unimplemented();
				}
			}
			else if (cf_inst23_7 == GPU7_CF_INST_TEX)
			{
				cemu_assert_unimplemented();
			}
			else if (cf_inst23_7 == GPU7_CF_INST_ELSE ||
				cf_inst23_7 == GPU7_CF_INST_POP)
			{
				cemu_assert_unimplemented();
			}
			else if (cf_inst23_7 == GPU7_CF_INST_JUMP)
			{
				cemu_assert_unimplemented();
			}
			else if (cf_inst23_7 == GPU7_CF_INST_LOOP_START_DX10 || cf_inst23_7 == GPU7_CF_INST_LOOP_END)
			{
				cemu_assert_unimplemented();
			}
			else if (cf_inst23_7 == GPU7_CF_INST_LOOP_BREAK)
			{
				cemu_assert_unimplemented();
			}
			else if (cf_inst23_7 == GPU7_CF_INST_MEM_STREAM0_WRITE ||
				cf_inst23_7 == GPU7_CF_INST_MEM_STREAM1_WRITE)
			{
				cemu_assert_unimplemented();
			}
			else if (cf_inst23_7 == GPU7_CF_INST_MEM_RING_WRITE)
			{
				cemu_assert_unimplemented();
			}
			else if (cf_inst23_7 == GPU7_CF_INST_EMIT_VERTEX)
			{
				cemu_assert_unimplemented();
			}
			else
			{
				cemu_assert_unimplemented();
			}
			if (isEndOfProgram)
			{
				return;
			}
		}
		else
		{
			// ALU instruction
			uint32 cf_inst26_4 = ((cfWord1 >> 26) & 0xF) | GPU7_CF_INST_ALU_MASK;
			if (cf_inst26_4 == GPU7_CF_INST_ALU || cf_inst26_4 == GPU7_CF_INST_ALU_PUSH_BEFORE || cf_inst26_4 == GPU7_CF_INST_ALU_POP_AFTER || cf_inst26_4 == GPU7_CF_INST_ALU_POP2_AFTER || cf_inst26_4 == GPU7_CF_INST_ALU_BREAK || cf_inst26_4 == GPU7_CF_INST_ALU_ELSE_AFTER)
			{
				uint32 addr = (cfWord0 >> 0) & 0x3FFFFF;
				uint32 count = ((cfWord1 >> 18) & 0x7F) + 1;
				uint32 cBank0Index = (cfWord0 >> 22) & 0xF;
				uint32 cBank1Index = (cfWord0 >> 26) & 0xF;
				uint32 cBank0AddrBase = ((cfWord1 >> 2) & 0xFF) * 16;
				uint32 cBank1AddrBase = ((cfWord1 >> 10) & 0xFF) * 16;
				LatteSoftware_executeALUClause(cf_inst26_4, addr, count, cBank0Index, cBank1Index, cBank0AddrBase, cBank1AddrBase);
			}
			else
			{
				cemu_assert_unimplemented();
			}
		}
	}
	cemu_assert_debug(false);
}

template<int endianMode>
uint32 _readVtxU32(void* ptr)
{
	uint32 v = *(uint32*)ptr;
	if constexpr (endianMode == GPU7_ENDIAN_8IN32)
		v = _swapEndianU32(v);
	return v;
}

template<int endianMode, int nfa>
void _readAttr_FLOAT_32_32(void* ptr, LatteReg_t& output)
{
	output.s32[0] = _readVtxU32<endianMode>((uint32*)ptr);
	output.s32[1] = _readVtxU32<endianMode>((uint32*)ptr + 1);
	output.s32[2] = 0;
	output.s32[3] = 0;
}

template<int endianMode, int nfa>
void _readAttr_FLOAT_32_32_32(void* ptr, LatteReg_t& output)
{
	output.s32[0] = _readVtxU32<endianMode>((uint32*)ptr);
	output.s32[1] = _readVtxU32<endianMode>((uint32*)ptr + 1);
	output.s32[2] = _readVtxU32<endianMode>((uint32*)ptr + 2);
	output.s32[3] = 0;
}

template<int endianMode, int nfa>
void _readAttr_FLOAT_32_32_32_32(void* ptr, LatteReg_t& output)
{
	output.s32[0] = _readVtxU32<endianMode>((uint32*)ptr);
	output.s32[1] = _readVtxU32<endianMode>((uint32*)ptr + 1);
	output.s32[2] = _readVtxU32<endianMode>((uint32*)ptr + 2);
	output.s32[3] = _readVtxU32<endianMode>((uint32*)ptr + 3);
}

#define _fmtKey(__fmt, __endianSwap, __nfa, __isSigned) ((__endianSwap)|((__nfa)<<2)|((__isSigned)<<4)|((__fmt)<<5))

void LatteSoftware_loadVertexAttributes(sint32 index)
{
	LatteSWCtx.reg[0].s32[0] = index;
	for (auto& bufferGroup : LatteSWCtx.fetchShader->bufferGroups)
	{
		for (sint32 f = 0; f < bufferGroup.attribCount; f++)
		{
			auto attrib = bufferGroup.attrib + f;
			// calculate element index
			sint32 elementIndex = index;
			// todo - handle instance index and attr divisor
			
			// get buffer
			uint32 bufferIndex = attrib->attributeBufferIndex;
			if (bufferIndex >= 0x10)
			{
				continue;
			}
			uint32 bufferBaseRegisterIndex = mmSQ_VTX_ATTRIBUTE_BLOCK_START + bufferIndex * 7;
			MPTR bufferAddress = LatteGPUState.contextRegister[bufferBaseRegisterIndex + 0];
			uint32 bufferSize = LatteGPUState.contextRegister[bufferBaseRegisterIndex + 1] + 1;
			uint32 bufferStride = (LatteGPUState.contextRegister[bufferBaseRegisterIndex + 2] >> 11) & 0xFFFF;
			if (bufferAddress == MPTR_NULL)
			{
				debug_printf("Warning: Attribute uses NULL buffer during software emulation\n");
				continue;
			}

			// translate semanticId to gpr index
			uint32 gprIndex = 0xFFFFFFFF;
			for (sint32 f = 0; f < 32; f++)
			{
				if (LatteGPUState.contextRegister[mmSQ_VTX_SEMANTIC_0 + f] == attrib->semanticId)
				{
					gprIndex = f;
					break;
				}
			}
			if (gprIndex == 0xFFFFFFFF)
				continue; // attribute is not mapped to VS input
			gprIndex = gprIndex + 1;

			sint32 formatKey = _fmtKey((sint32)attrib->format, (sint32)attrib->endianSwap, (sint32)attrib->nfa, (sint32)attrib->isSigned);

			void* inputData = memory_getPointerFromPhysicalOffset(bufferAddress + elementIndex * bufferStride);

			LatteReg_t attrData;

			switch (formatKey)
			{
			case _fmtKey(FMT_32_32_FLOAT, GPU7_ENDIAN_8IN32, LATTE_NFA_2, LATTE_VTX_UNSIGNED):
				_readAttr_FLOAT_32_32<GPU7_ENDIAN_8IN32, LATTE_NFA_2>(inputData, attrData);
				break;
			case _fmtKey(FMT_32_32_32_FLOAT, GPU7_ENDIAN_8IN32, LATTE_NFA_2, LATTE_VTX_UNSIGNED):
				_readAttr_FLOAT_32_32_32<GPU7_ENDIAN_8IN32, LATTE_NFA_2>(inputData, attrData);
				break;
			case _fmtKey(FMT_32_32_32_32_FLOAT, GPU7_ENDIAN_8IN32, LATTE_NFA_2, LATTE_VTX_UNSIGNED):
				_readAttr_FLOAT_32_32_32_32<GPU7_ENDIAN_8IN32, LATTE_NFA_2>(inputData, attrData);
				break;
			default:
				cemu_assert_debug(false);
			}

			LatteReg_t* gprOutput = LatteSWCtx.reg+gprIndex;
			for (uint32 f = 0; f < 4; f++)
			{
				if (attrib->ds[f] < 4)
					gprOutput->s32[f] = attrData.s32[f];
				else if (attrib->ds[f] == 4)
					gprOutput->s32[f] = 0;
				else if (attrib->ds[f] == 5)
					gprOutput->f[f] = 1.0;
				else
					cemu_assert_debug(false);
			}
		}
	}
}

float* LatteSoftware_getPositionExport()
{
	return LatteSWCtx.export_pos.f;
}

void LatteSoftware_executeVertex(sint32 index)
{
	LatteSoftware_loadVertexAttributes(index);
	LatteSoftware_singleRun();
}

void LatteSoftware_setupVertexShader(LatteFetchShader* fetchShader, void* shaderPtr, sint32 size)
{
	LatteSWCtx.fetchShader = fetchShader;
	LatteSWCtx.shaderBase = (uint32*)shaderPtr;
	LatteSWCtx.shaderSize = size;
	LatteSWCtx.cfilePtr = (void*)(LatteGPUState.contextRegister + LATTE_REG_BASE_ALU_CONST + 0x400);
}
