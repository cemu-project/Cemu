#include "Cafe/OS/common/OSCommon.h"
#include "GX2.h"
#include "GX2_Shader.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"
#include "Cafe/HW/Latte/ISA/LatteInstructions.h"

uint32 memory_getVirtualOffsetFromPointer(void* ptr); // remove once we updated everything to MEMPTR

namespace GX2
{
	using namespace Latte;

	LatteConst::VertexFetchEndianMode _getVtxFormatEndianSwapDefault(uint32 vertexFormat)
	{
		switch (vertexFormat)
		{
		case 0:
		case 1:
		case 4:
		case 10:
			return LatteConst::VertexFetchEndianMode::SWAP_NONE; // 0
		case 2:
		case 3:
		case 7:
		case 8:
		case 14:
		case 15:
			return LatteConst::VertexFetchEndianMode::SWAP_U16; // 1
		case 5:
		case 6:
		case 9:
		case 11:
		case 12:
		case 13:
		case 16:
		case 17:
		case 18:
		case 19:
			return LatteConst::VertexFetchEndianMode::SWAP_U32; // 2
		default:
			break;
		}
		cemu_assert_suspicious();
		return LatteConst::VertexFetchEndianMode::SWAP_NONE;
	}

	uint32 rawFormatToFetchFormat[] =
	{
		1, 2, 5, 6,
		7, 0xD, 0xE, 0xF,
		0x10, 0x16, 0x1A, 0x19,
		0x1D, 0x1E, 0x1F, 0x20,
		0x2F, 0x30, 0x22, 0x23,
	};

	struct GX2AttribDescription
	{
		/* +0x00 */ uint32 location;
		/* +0x04 */ uint32 buffer;
		/* +0x08 */ uint32be offset;
		/* +0x0C */ uint32 format;
		/* +0x10 */ uint32 indexType;
		/* +0x14 */ uint32 aluDivisor;
		/* +0x18 */ uint32 destSel;
		/* +0x1C */ betype<LatteConst::VertexFetchEndianMode> endianSwap;
	};

	static_assert(sizeof(GX2AttribDescription) == 0x20);
	static_assert(sizeof(betype<LatteConst::VertexFetchEndianMode>) == 0x4);

	// calculate size of CF program subpart, includes alignment padding for clause instructions
	size_t _calcFetchShaderCFCodeSize(uint32 attributeCount, GX2FetchShader_t::FetchShaderType fetchShaderType, uint32 tessellationMode)
	{
		cemu_assert_debug(fetchShaderType == GX2FetchShader_t::FetchShaderType::NO_TESSELATION);
		cemu_assert_debug(tessellationMode == 0);
		uint32 numCFInstructions = ((attributeCount + 15) / 16) + 1; // one VTX clause can have up to 16 instructions + final CF instruction is RETURN
		size_t cfSize = numCFInstructions * 8;
		cfSize = (cfSize + 0xF) & ~0xF; // pad to 16 byte alignment
		return cfSize;
	}

	size_t _calcFetchShaderClauseCodeSize(uint32 attributeCount, GX2FetchShader_t::FetchShaderType fetchShaderType, uint32 tessellationMode)
	{
		cemu_assert_debug(fetchShaderType == GX2FetchShader_t::FetchShaderType::NO_TESSELATION);
		cemu_assert_debug(tessellationMode == 0);
		uint32 numClauseInstructions = attributeCount;
		size_t clauseSize = numClauseInstructions * 16;
		return clauseSize;
	}

	void _writeFetchShaderCFCode(void* programBufferOut, uint32 attributeCount, GX2FetchShader_t::FetchShaderType fetchShaderType, uint32 tessellationMode)
	{
		LatteCFInstruction* cfInstructionWriter = (LatteCFInstruction*)programBufferOut;
		uint32 attributeIndex = 0;
		uint32 cfSize = (uint32)_calcFetchShaderCFCodeSize(attributeCount, fetchShaderType, tessellationMode);
		while (attributeIndex < attributeCount)
		{
			LatteCFInstruction_DEFAULT defaultInstr;
			defaultInstr.setField_Opcode(LatteCFInstruction::INST_VTX_TC);
			defaultInstr.setField_COUNT(std::min(attributeCount - attributeIndex, 16u));
			defaultInstr.setField_ADDR(cfSize + attributeIndex*16);
			memcpy(cfInstructionWriter, &defaultInstr, sizeof(LatteCFInstruction));
			attributeIndex += 16;
			cfInstructionWriter++;
		}
		// write RETURN instruction
		LatteCFInstruction_DEFAULT returnInstr;
		returnInstr.setField_Opcode(LatteCFInstruction::INST_RETURN);
		returnInstr.setField_BARRIER(true);
		memcpy(cfInstructionWriter, &returnInstr, sizeof(LatteCFInstruction));
	}

	void _writeFetchShaderVTXCode(GX2FetchShader_t* fetchShader, void* programOut, uint32 attributeCount, GX2AttribDescription* attributeDescription, GX2FetchShader_t::FetchShaderType fetchShaderType, uint32 tessellationMode)
	{
		uint8* writePtr = (uint8*)programOut;
		// one instruction per attribute (hardcoded into _writeFetchShaderCFCode)
		for (uint32 i = 0; i < attributeCount; i++)
		{
			uint32 attrFormat = _swapEndianU32(attributeDescription[i].format);
			uint32 attrDestSel = _swapEndianU32(attributeDescription[i].destSel);
			uint32 attrLocation = _swapEndianU32(attributeDescription[i].location);
			uint32 attrBufferId = _swapEndianU32(attributeDescription[i].buffer);
			uint32 attrIndexType = _swapEndianU32(attributeDescription[i].indexType);
			uint32 attrAluDivisor = _swapEndianU32(attributeDescription[i].aluDivisor);

			cemu_assert_debug(attrIndexType <= 1);

			LatteConst::VertexFetchEndianMode endianSwap = attributeDescription[i].endianSwap;
			if (endianSwap == LatteConst::VertexFetchEndianMode::SWAP_DEFAULT) // use per-format default
				endianSwap = _getVtxFormatEndianSwapDefault(attrFormat & 0x3F);

			uint32 srcSelX = 0; // this field is used to store the divisor index/mode (0 -> per-vertex index, 1 -> alu divisor 0, 2 -> alu divisor 1, 3 -> per-instance index)
			if (attrIndexType == 0)
			{
				srcSelX = 0; // increase index per vertex
			}
			else if (attrIndexType == 1)
			{
				// instance based index
				if (attrAluDivisor == 1)
				{
					// special encoding if alu divisor is 1
					srcSelX = 3;
				}
				else
				{
					cemu_assert_debug(attrAluDivisor != 0); // divisor should not be zero if instance based index is selected?
					// store alu divisor in divisor table (up to two entries)
					uint32 numDivisors = _swapEndianU32(fetchShader->divisorCount);
					bool divisorFound = false;
					for (uint32 i = 0; i < numDivisors; i++)
					{
						if (_swapEndianU32(fetchShader->divisors[i]) == attrAluDivisor)
						{
							srcSelX = i != 0 ? 2 : 1;
							divisorFound = true;
							break;
						}
					}
					if (divisorFound == false)
					{
						// add new divisor
						if (numDivisors >= 2)
						{
							cemu_assert_debug(false); // not enough space for additional divisor
						}
						else
						{
							srcSelX = numDivisors != 0 ? 2 : 1;
							fetchShader->divisors[numDivisors] = _swapEndianU32(attrAluDivisor);
							numDivisors++;
							fetchShader->divisorCount = _swapEndianU32(numDivisors);
						}
					}
				}
			}
			else
			{
				cemu_assert_debug(false);
			}

			// convert attribute format to fetch format
			uint32 fetchFormat = rawFormatToFetchFormat[attrFormat & 0x3F] & 0x3F;

			uint32 nfa = 0;
			if ((attrFormat & 0x800) != 0)
				nfa = 2;
			else if ((attrFormat & 0x100) != 0)
				nfa = 1;
			else
				nfa = 0;

			LatteClauseInstruction_VTX vtxInstruction;
			vtxInstruction.setField_VTX_INST(LatteClauseInstruction_VTX::VTX_INST::_VTX_INST_SEMANTIC);
			vtxInstruction.setFieldSEM_SEMANTIC_ID(attrLocation&0xFF);
			vtxInstruction.setField_BUFFER_ID(attrBufferId + 0xA0);
			vtxInstruction.setField_FETCH_TYPE((LatteConst::VertexFetchType2)attrIndexType);
			vtxInstruction.setField_SRC_SEL_X((LatteClauseInstruction_VTX::SRC_SEL)srcSelX);
			vtxInstruction.setField_DATA_FORMAT((LatteConst::VertexFetchFormat)fetchFormat);
			vtxInstruction.setField_NUM_FORMAT_ALL((LatteClauseInstruction_VTX::NUM_FORMAT_ALL)nfa);
			vtxInstruction.setField_OFFSET(attributeDescription[i].offset);
			if ((attrFormat & 0x200) != 0)
				vtxInstruction.setField_FORMAT_COMP_ALL(LatteClauseInstruction_VTX::FORMAT_COMP::COMP_SIGNED);
			vtxInstruction.setField_ENDIAN_SWAP((LatteConst::VertexFetchEndianMode)endianSwap);
			vtxInstruction.setField_DST_SEL(0, (LatteClauseInstruction_VTX::DST_SEL)((attrDestSel >> 24) & 0x7));
			vtxInstruction.setField_DST_SEL(1, (LatteClauseInstruction_VTX::DST_SEL)((attrDestSel >> 16) & 0x7));
			vtxInstruction.setField_DST_SEL(2, (LatteClauseInstruction_VTX::DST_SEL)((attrDestSel >> 8) & 0x7));
			vtxInstruction.setField_DST_SEL(3, (LatteClauseInstruction_VTX::DST_SEL)((attrDestSel >> 0) & 0x7));

			memcpy(writePtr, &vtxInstruction, 16);
			writePtr += 16;
		}
	}

	uint32 GX2CalcFetchShaderSizeEx(uint32 attributeCount, GX2FetchShader_t::FetchShaderType fetchShaderType, uint32 tessellationMode)
	{
		cemu_assert_debug(fetchShaderType == GX2FetchShader_t::FetchShaderType::NO_TESSELATION); // other types are todo
		cemu_assert_debug(tessellationMode == 0); // other modes are todo

		uint32 finalSize =
			(uint32)_calcFetchShaderCFCodeSize(attributeCount, fetchShaderType, tessellationMode) +
			(uint32)_calcFetchShaderClauseCodeSize(attributeCount, fetchShaderType, tessellationMode);

		return finalSize;
	}

	void GX2InitFetchShaderEx(GX2FetchShader_t* fetchShader, void* programBufferOut, uint32 attributeCount, GX2AttribDescription* attributeDescription, GX2FetchShader_t::FetchShaderType fetchShaderType, uint32 tessellationMode)
	{
		cemu_assert_debug(fetchShaderType == GX2FetchShader_t::FetchShaderType::NO_TESSELATION);
		cemu_assert_debug(tessellationMode == 0);

		/*
			Fetch shader program:
			[CF_PROGRAM]
			[Last CF instruction: 0x0 0x8A000000 (INST_RETURN)]
			[PAD_TO_16_ALIGNMENT]
			[CLAUSES]
		*/

		memset(fetchShader, 0x00, sizeof(GX2FetchShader_t));
		fetchShader->attribCount = _swapEndianU32(attributeCount);
		fetchShader->shaderPtr = (MPTR)_swapEndianU32(memory_getVirtualOffsetFromPointer(programBufferOut));

		uint8* shaderStart = (uint8*)programBufferOut;
		uint8* shaderOutput = shaderStart;

		_writeFetchShaderCFCode(shaderOutput, attributeCount, fetchShaderType, tessellationMode);
		shaderOutput += _calcFetchShaderCFCodeSize(attributeCount, fetchShaderType, tessellationMode);
		_writeFetchShaderVTXCode(fetchShader, shaderOutput, attributeCount, attributeDescription, fetchShaderType, tessellationMode);
		shaderOutput += _calcFetchShaderClauseCodeSize(attributeCount, fetchShaderType, tessellationMode);

		uint32 shaderSize = (uint32)(shaderOutput - shaderStart);
		cemu_assert_debug(shaderSize == GX2CalcFetchShaderSizeEx(attributeCount, GX2FetchShader_t::FetchShaderType::NO_TESSELATION, tessellationMode));

		fetchShader->shaderSize = _swapEndianU32((uint32)(shaderOutput - shaderStart));
	}

	void GX2ShaderInit()
	{
		cafeExportRegister("gx2", GX2CalcFetchShaderSizeEx, LogType::GX2);
		cafeExportRegister("gx2", GX2InitFetchShaderEx, LogType::GX2);
	}
}