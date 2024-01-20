#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Core/LatteShaderAssembly.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/OS/libs/gx2/GX2.h" // todo - remove dependency
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerInternal.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerInstructions.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "config/ActiveSettings.h"
#include "util/helpers/StringBuf.h"

#include <bitset>
#include <boost/container/small_vector.hpp>

#define _CRLF	"\r\n"

void LatteDecompiler_emitAttributeDecodeGLSL(LatteDecompilerShader* shaderContext, StringBuf* src, LatteParsedFetchShaderAttribute_t* attrib);

/*
 * Variable names:
 * R0-R127 temp
 * Most variables are multi-typed and the respective type is appended to the name
 * Type suffixes are: f (float), i (32bit int), ui (unsigned 32bit int)
 * Examples: R13ui.x, tempf.z
 */

// local prototypes
void _emitTypeConversionPrefix(LatteDecompilerShaderContext* shaderContext, sint32 sourceType, sint32 destinationType);
void _emitTypeConversionSuffix(LatteDecompilerShaderContext* shaderContext, sint32 sourceType, sint32 destinationType);
void LatteDecompiler_emitClauseCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction, bool isSubroutine);

const char* _getShaderUniformBlockInterfaceName(LatteConst::ShaderType mode)
{
	switch (mode)
	{
	case LatteConst::ShaderType::Vertex:
			return "uniformBlockVS";
	case LatteConst::ShaderType::Pixel:
			return "uniformBlockPS";
	case LatteConst::ShaderType::Geometry:
			return "uniformBlockGS";
	default:
		break;
	}
	cemu_assert_unimplemented();
	return nullptr;
}

const char* _getShaderUniformBlockVariableName(LatteConst::ShaderType mode)
{
	switch (mode)
	{
	case LatteConst::ShaderType::Vertex:
			return "uf_blockVS";
	case LatteConst::ShaderType::Pixel:
			return "uf_blockPS";
	case LatteConst::ShaderType::Geometry:
			return "uf_blockGS";
	default:
		break;
	}
	cemu_assert_unimplemented();
	return nullptr;
}

const char* _getTextureUnitVariablePrefixName(LatteConst::ShaderType mode)
{
	switch (mode)
	{
	case LatteConst::ShaderType::Vertex:
			return "textureUnitVS";
	case LatteConst::ShaderType::Pixel:
			return "textureUnitPS";
	case LatteConst::ShaderType::Geometry:
			return "textureUnitGS";
	}
	cemu_assert_unimplemented();
	return nullptr;
}

const char* _getElementStrByIndex(uint32 channel)
{
	switch (channel)
	{
		case 0:
			return "x";
		case 1:
			return "y";
		case 2:
			return "z";
		case 3:
			return "w";
	}
	return "UNDEFINED";
}

char _tempGenString[64][256];
uint32 _tempGenStringIndex = 0;

char* _getTempString()
{
	char* str = _tempGenString[_tempGenStringIndex];
	_tempGenStringIndex = (_tempGenStringIndex+1)%64;
	return str;
}

static char* _getActiveMaskVarName(LatteDecompilerShaderContext* shaderContext, sint32 index)
{
	char* varName = _getTempString();
	if (shaderContext->isSubroutine)
		sprintf(varName, "activeMaskStackSub%04x[%d]", shaderContext->subroutineInfo->cfAddr, index);
	else
		sprintf(varName, "activeMaskStack[%d]", index);
	return varName;
}

static char* _getActiveMaskCVarName(LatteDecompilerShaderContext* shaderContext, sint32 index)
{
	char* varName = _getTempString();
	if (shaderContext->isSubroutine)
		sprintf(varName, "activeMaskStackCSub%04x[%d]", shaderContext->subroutineInfo->cfAddr, index);
	else
		sprintf(varName, "activeMaskStackC[%d]", index);
	return varName;
}

static char* _getRegisterVarName(LatteDecompilerShaderContext* shaderContext, uint32 index, sint32 destRelIndexMode=-1)
{
	auto type = shaderContext->typeTracker.defaultDataType;
	char* tempStr = _getTempString();
	if (shaderContext->typeTracker.useArrayGPRs == false)
	{
		if (type == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
			sprintf(tempStr, "R%di", index);
		else if (type == LATTE_DECOMPILER_DTYPE_FLOAT)
			sprintf(tempStr, "R%df", index);
	}
	else
	{
		char destRelOffset[32];
		if (destRelIndexMode >= 0)
		{
			if (destRelIndexMode == GPU7_INDEX_AR_X)
				strcpy(destRelOffset, "ARi.x");
			else if (destRelIndexMode == GPU7_INDEX_AR_Y)
				strcpy(destRelOffset, "ARi.y");
			else if (destRelIndexMode == GPU7_INDEX_AR_Z)
				strcpy(destRelOffset, "ARi.z");
			else if (destRelIndexMode == GPU7_INDEX_AR_W)
				strcpy(destRelOffset, "ARi.w");
			else
				debugBreakpoint();
			if (type == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
			{
				sprintf(tempStr, "Ri[%d+%s]", index, destRelOffset);
			}
			else if (type == LATTE_DECOMPILER_DTYPE_FLOAT)
			{
				sprintf(tempStr, "Rf[%d+%s]", index, destRelOffset);
			}
		}
		else
		{
			if (type == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
			{
				sprintf(tempStr, "Ri[%d]", index);
			}
			else if (type == LATTE_DECOMPILER_DTYPE_FLOAT)
			{
				sprintf(tempStr, "Rf[%d]", index);
			}
		}
	}
	return tempStr;
}

static void _appendRegisterTypeSuffix(StringBuf* src, sint32 dataType)
{
	if (dataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
		src->add("i");
	else if (dataType == LATTE_DECOMPILER_DTYPE_UNSIGNED_INT)
		src->add("ui");
	else if (dataType == LATTE_DECOMPILER_DTYPE_FLOAT)
		src->add("f");
	else
		cemu_assert_unimplemented();
}

// appends x/y/z/w
static void _appendChannel(StringBuf* src, sint32 channelIndex)
{
	cemu_assert_debug(channelIndex >= 0 && channelIndex <= 3);
	switch (channelIndex)
	{
	case 0:
		src->add("x");
		return;
	case 1:
		src->add("y");
		return;
	case 2:
		src->add("z");
		return;
	case 3:
		src->add("w");
		return;
	}
}

// appends .x/.y/.z/.w
static void _appendChannelAccess(StringBuf* src, sint32 channelIndex)
{
	cemu_assert_debug(channelIndex >= 0 && channelIndex <= 3);
	switch (channelIndex)
	{
	case 0:
		src->add(".x");
		return;
	case 1:
		src->add(".y");
		return;
	case 2:
		src->add(".z");
		return;
	case 3:
		src->add(".w");
		return;
	}
}

static void _appendPVPS(LatteDecompilerShaderContext* shaderContext, StringBuf* src, uint32 groupIndex, uint8 aluUnit)
{
	cemu_assert_debug(aluUnit < 5);
	if (aluUnit == 4)
	{
		src->addFmt("PS{}", (groupIndex & 1));
		_appendRegisterTypeSuffix(src, shaderContext->typeTracker.defaultDataType);
		return;
	}
	src->addFmt("PV{}", (groupIndex & 1));
	_appendRegisterTypeSuffix(src, shaderContext->typeTracker.defaultDataType);
	_appendChannel(src, aluUnit);
}

std::string _FormatFloatAsGLSLConstant(float f)
{
	char floatAsStr[64];
	size_t floatAsStrLen = fmt::format_to_n(floatAsStr, 64, "{:#}", f).size;
	size_t floatAsStrLenOrg = floatAsStrLen;
	if(floatAsStrLen > 0 && floatAsStr[floatAsStrLen-1] == '.')
	{
		floatAsStr[floatAsStrLen] = '0';
		floatAsStrLen++;
	}
	cemu_assert(floatAsStrLen < 50); // constant suspiciously long?
	floatAsStr[floatAsStrLen] = '\0';
	cemu_assert_debug(floatAsStrLen >= 3); // shortest possible form is "0.0"
	return floatAsStr;
}

// tracks PV/PS and register backups
struct ALUClauseTemporariesState
{
	struct PVPSAlias
	{
		enum class LOCATION_TYPE : uint8
		{
			LOCATION_NONE,
			LOCATION_GPR,
			LOCATION_PVPS,
		};

		LOCATION_TYPE location{ LOCATION_TYPE::LOCATION_NONE };
		uint8 index; // GPR index or temporary index
		uint8 aluUnit; // x,y,z,w (or 5 for PS)

		void SetLocationGPR(uint8 gprIndex, uint8 channel)
		{
			cemu_assert_debug(channel < 4);
			this->location = LOCATION_TYPE::LOCATION_GPR;
			this->index = gprIndex;
			this->aluUnit = channel;
		}

		void SetLocationPSPVTemporary(uint8 aluUnit, uint32 groupIndex)
		{
			cemu_assert_debug(aluUnit < 5);
			this->location = LOCATION_TYPE::LOCATION_PVPS;
			this->index = groupIndex & 1;
			this->aluUnit = aluUnit;
		}
	};

	struct GPRTemporary
	{
		GPRTemporary(uint8 gprIndex, uint8 channel, uint8 backupVarIndex) : gprIndex(gprIndex), channel(channel), backupVarIndex(backupVarIndex) {}

		uint8 gprIndex;
		uint8 channel;
		uint8 backupVarIndex;
	};

	void TrackGroupOutputPVPS(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstr, size_t numInstr)
	{
		// unset current
		for (auto& it : m_pvps)
			it.location = PVPSAlias::LOCATION_TYPE::LOCATION_NONE;
		for (size_t i = 0; i < numInstr; i++)
		{
			LatteDecompilerALUInstruction& inst = aluInstr[i];
			if (!inst.isOP3 && inst.opcode == ALU_OP2_INST_NOP)
				continue; // skip NOP instruction

			if (inst.writeMask == 0)
			{
				// map to temporary
				m_pvps[inst.aluUnit].SetLocationPSPVTemporary(inst.aluUnit, aluInstr->instructionGroupIndex);
			}
			else
			{
				// map to GPR
				if(inst.destRel == 0) // is PV/PS set for indexed writes?
					m_pvps[inst.aluUnit].SetLocationGPR(inst.destGpr, inst.destElem);
			}
		}
	}

	bool HasPVPS(uint8 aluUnitIndex) const
	{
		cemu_assert_debug(aluUnitIndex < 5);
		return m_pvps[aluUnitIndex].location != PVPSAlias::LOCATION_TYPE::LOCATION_NONE;
	}

	void EmitPVPSAccess(LatteDecompilerShaderContext* shaderContext, uint8 aluUnitIndex, uint32 currentGroupIndex) const
	{
		switch (m_pvps[aluUnitIndex].location)
		{
		case PVPSAlias::LOCATION_TYPE::LOCATION_GPR:
		{
			sint32 temporaryIndex = GetTemporaryForGPR(m_pvps[aluUnitIndex].index, m_pvps[aluUnitIndex].aluUnit);
			if (temporaryIndex < 0)
			{
				shaderContext->shaderSource->add(_getRegisterVarName(shaderContext, m_pvps[aluUnitIndex].index, -1));
				_appendChannelAccess(shaderContext->shaderSource, m_pvps[aluUnitIndex].aluUnit);
			}
			else
			{
				// use temporary instead of GPR
				shaderContext->shaderSource->addFmt("backupReg{}", temporaryIndex);
				_appendRegisterTypeSuffix(shaderContext->shaderSource, shaderContext->typeTracker.defaultDataType);
			}
			break;
		}
		case PVPSAlias::LOCATION_TYPE::LOCATION_PVPS:
			_appendPVPS(shaderContext, shaderContext->shaderSource, currentGroupIndex-1, m_pvps[aluUnitIndex].aluUnit);
			break;
		default:
			cemuLog_log(LogType::Force, "Shader {:016x} accesses PV/PS without writing to it", shaderContext->shaderBaseHash);
			cemu_assert_suspicious();
			break;
		}
	}

	/*
	 * Check for GPR channels which are modified before they are read within the same group
	 * These registers need to be copied to a temporary
	 */
	void CreateGPRTemporaries(LatteDecompilerShaderContext* shaderContext, std::span<LatteDecompilerALUInstruction> aluInstructions)
	{
		uint8 registerChannelWriteMask[(LATTE_NUM_GPR * 4 + 7) / 8] = { 0 };

		m_gprTemporaries.clear();
		for (auto& aluInstruction : aluInstructions)
		{
			// ignore NOP instructions
			if (aluInstruction.isOP3 == false && aluInstruction.opcode == ALU_OP2_INST_NOP)
				continue;
			cemu_assert_debug(aluInstruction.destElem <= 3);
			// check if any previously written register is read
			for (sint32 f = 0; f < 3; f++)
			{
				uint32 readGPRIndex;
				uint32 readGPRChannel;
				if (GPU7_ALU_SRC_IS_GPR(aluInstruction.sourceOperand[f].sel))
				{
					readGPRIndex = GPU7_ALU_SRC_GET_GPR_INDEX(aluInstruction.sourceOperand[f].sel);
					cemu_assert_debug(aluInstruction.sourceOperand[f].chan <= 3);
					readGPRChannel = aluInstruction.sourceOperand[f].chan;
				}
				else if (GPU7_ALU_SRC_IS_PV(aluInstruction.sourceOperand[f].sel) || GPU7_ALU_SRC_IS_PS(aluInstruction.sourceOperand[f].sel))
				{
					uint8 aluUnitIndex = 0;
					if (GPU7_ALU_SRC_IS_PV(aluInstruction.sourceOperand[f].sel))
						aluUnitIndex = aluInstruction.sourceOperand[f].chan;
					else
						aluUnitIndex = 4;
					// if aliased to a GPR, then consider it a GPR read
					if(m_pvps[aluUnitIndex].location != PVPSAlias::LOCATION_TYPE::LOCATION_GPR)
						continue;
					readGPRIndex = m_pvps[aluUnitIndex].index;
					readGPRChannel = m_pvps[aluUnitIndex].aluUnit;
				}
				else
					continue;
				// track GPR read
				if ((registerChannelWriteMask[(readGPRIndex * 4 + aluInstruction.sourceOperand[f].chan) / 8] & (1 << ((readGPRIndex * 4 + aluInstruction.sourceOperand[f].chan) % 8))) != 0)
				{
					// register is overwritten by previous instruction, a temporary variable is required
					if (GetTemporaryForGPR(readGPRIndex, readGPRChannel) < 0)
						m_gprTemporaries.emplace_back(readGPRIndex, readGPRChannel, m_gprTemporaries.size());
				}
			}
			// track write
			if (aluInstruction.writeMask != 0)
				registerChannelWriteMask[(aluInstruction.destGpr * 4 + aluInstruction.destElem) / 8] |= (1 << ((aluInstruction.destGpr * 4 + aluInstruction.destElem) % 8));
		}
		// output code to move GPRs into temporaries
		StringBuf* src = shaderContext->shaderSource;
		for (auto& it : m_gprTemporaries)
		{
			src->addFmt("backupReg{}", it.backupVarIndex);
			_appendRegisterTypeSuffix(src, shaderContext->typeTracker.defaultDataType);
			src->add(" = ");
			src->add(_getRegisterVarName(shaderContext, it.gprIndex));
			_appendChannelAccess(src, it.channel);
			src->add(";" _CRLF);
		}
	}

	// returns -1 if none present
	sint32 GetTemporaryForGPR(uint8 gprIndex, uint8 channel) const
	{
		for (auto& it : m_gprTemporaries)
		{
			if (it.gprIndex == gprIndex && it.channel == channel)
				return (sint32)it.backupVarIndex;
		}
		return -1;
	}

private:
	PVPSAlias m_pvps[5]{};
	boost::container::small_vector<GPRTemporary, 4> m_gprTemporaries;
};

sint32 _getVertexShaderOutParamSemanticId(uint32* contextRegisters, sint32 index) // deprecated - move to LatteShaderPSInputTable
{
	uint32 vsSemanticId = (contextRegisters[mmSPI_VS_OUT_ID_0 + (index / 4)] >> (8 * (index % 4))) & 0xFF;
	// check if export exists since exports are generated based on PS inputs
	LatteShaderPSInputTable* psInputTable = LatteSHRC_GetPSInputTable();
	for (sint32 i = 0; i < psInputTable->count; i++)
	{
		if(psInputTable->import[i].semanticId == vsSemanticId)
			return vsSemanticId;
	}
	return 0xFF;
}

sint32 _getInputRegisterDataType(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstruction, sint32 operandIndex)
{
	return shaderContext->typeTracker.defaultDataType;
}

sint32 _getALUInstructionOutputDataType(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstruction)
{
	return shaderContext->typeTracker.defaultDataType;
}

// returns true if the ALU instruction is a OP2 reduction instruction
bool _isReductionInstruction(LatteDecompilerALUInstruction* aluInstruction)
{
	return aluInstruction->isOP3 == false && (aluInstruction->opcode == ALU_OP2_INST_DOT4 || aluInstruction->opcode == ALU_OP2_INST_DOT4_IEEE || aluInstruction->opcode == ALU_OP2_INST_CUBE);
}

/*
 * Writes the name of the output variable and channel
 * E.g. R5f.x or tempf.x if writeMask is 0
 */
void _emitInstructionOutputVariableName(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstruction)
{
	auto src = shaderContext->shaderSource;
	sint32 outputDataType = _getALUInstructionOutputDataType(shaderContext, aluInstruction);
	if( aluInstruction->writeMask == 0 )
	{
		// does not output to GPR
		if( !_isReductionInstruction(aluInstruction) )
		{
			// output to PV/PS
			_appendPVPS(shaderContext, src, aluInstruction->instructionGroupIndex, aluInstruction->aluUnit);
			return;
		}
		else
		{
			// output to temp
			src->add("temp");
			_appendRegisterTypeSuffix(src, outputDataType);
		}
		_appendChannelAccess(src, aluInstruction->aluUnit);
	}
	else
	{
		// output to GPR. Aliasing to PV/PS happens at the end of the group
		src->add(_getRegisterVarName(shaderContext, aluInstruction->destGpr, aluInstruction->destRel==0?-1:aluInstruction->indexMode));
		_appendChannelAccess(src, aluInstruction->destElem);
	}
}

void _emitInstructionPVPSOutputVariableName(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstruction)
{
	_appendPVPS(shaderContext, shaderContext->shaderSource, aluInstruction->instructionGroupIndex, aluInstruction->aluUnit);
}

void _emitRegisterAccessCode(LatteDecompilerShaderContext* shaderContext, sint32 gprIndex, sint32 channel0, sint32 channel1, sint32 channel2, sint32 channel3, sint32 dataType = -1)
{
	StringBuf* src = shaderContext->shaderSource;
	sint32 registerElementDataType = shaderContext->typeTracker.defaultDataType;
	cemu_assert_debug(gprIndex >= 0 && gprIndex <= 127);
	if (dataType >= 0)
	{
		_emitTypeConversionPrefix(shaderContext, registerElementDataType, dataType);
	}
	if (shaderContext->typeTracker.useArrayGPRs)
		src->add("R");
	else
		src->addFmt("R{}", gprIndex);
	_appendRegisterTypeSuffix(src, registerElementDataType);
	if (shaderContext->typeTracker.useArrayGPRs)
		src->addFmt("[{}]", gprIndex);

	src->add(".");

	sint32 channelArray[4];
	channelArray[0] = channel0;
	channelArray[1] = channel1;
	channelArray[2] = channel2;
	channelArray[3] = channel3;

	for (sint32 i = 0; i < 4; i++)
	{
		if (channelArray[i] >= 0 && channelArray[i] <= 3)
			src->add(_getElementStrByIndex(channelArray[i]));
		else if (channelArray[i] == -1)
		{
			// channel not used
		}
		else
		{
			cemu_assert_unimplemented();
		}
	}
	if (dataType >= 0)
		_emitTypeConversionSuffix(shaderContext, registerElementDataType, dataType);
}

// optimized variant of _emitRegisterAccessCode for raw one channel reads
void _emitRegisterChannelAccessCode(LatteDecompilerShaderContext* shaderContext, sint32 gprIndex, sint32 channel, sint32 dataType)
{
	cemu_assert_debug(gprIndex >= 0 && gprIndex <= 127);
	cemu_assert_debug(channel >= 0 && channel < 4);
	StringBuf* src = shaderContext->shaderSource;
	sint32 registerElementDataType = shaderContext->typeTracker.defaultDataType;
	_emitTypeConversionPrefix(shaderContext, registerElementDataType, dataType);
	if (shaderContext->typeTracker.useArrayGPRs)
		src->add("R");
	else
		src->addFmt("R{}", gprIndex);
	_appendRegisterTypeSuffix(src, registerElementDataType);
	if (shaderContext->typeTracker.useArrayGPRs)
		src->addFmt("[{}]", gprIndex);
	src->add(".");
	src->add(_getElementStrByIndex(channel));
	_emitTypeConversionSuffix(shaderContext, registerElementDataType, dataType);
}

void _emitALURegisterInputAccessCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstruction, sint32 operandIndex)
{
	StringBuf* src = shaderContext->shaderSource;
	sint32 currentRegisterElementType = _getInputRegisterDataType(shaderContext, aluInstruction, operandIndex);
	cemu_assert_debug(GPU7_ALU_SRC_IS_GPR(aluInstruction->sourceOperand[operandIndex].sel));
	sint32 gprIndex = GPU7_ALU_SRC_GET_GPR_INDEX(aluInstruction->sourceOperand[operandIndex].sel);	
	sint32 temporaryIndex = shaderContext->aluPVPSState->GetTemporaryForGPR(gprIndex, aluInstruction->sourceOperand[operandIndex].chan);
	if(temporaryIndex >= 0)
	{
		// access via backup variable
		src->addFmt("backupReg{}", temporaryIndex);
		_appendRegisterTypeSuffix(src, currentRegisterElementType);
	}
	else
	{
		// access via register variable
		_emitRegisterAccessCode(shaderContext, gprIndex, aluInstruction->sourceOperand[operandIndex].chan, -1, -1, -1);
	}
}

void _emitPVPSAccessCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstruction, sint32 operandIndex, uint8 aluUnitIndex)
{
	cemu_assert_debug(aluInstruction->instructionGroupIndex > 0); // PV/PS is uninitialized for group 0
	// PV/PS vars are currently always using the default type (shaderContext->typeTracker.defaultDataType)
	shaderContext->aluPVPSState->EmitPVPSAccess(shaderContext, aluUnitIndex, aluInstruction->instructionGroupIndex);
}

/*
 * Emits the expression used for calculating the index for uniform access
 * For static access, this is a number
 * For dynamic access, this is AR.* + base
 */
void _emitUniformAccessIndexCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstruction, sint32 operandIndex)
{
	StringBuf* src = shaderContext->shaderSource;
	bool isUniformRegister = GPU7_ALU_SRC_IS_CFILE(aluInstruction->sourceOperand[operandIndex].sel);
	sint32 uniformOffset = 0; // index into array, for relative accesses this is the base offset
	if( isUniformRegister )
	{
		uniformOffset = GPU7_ALU_SRC_GET_CFILE_INDEX(aluInstruction->sourceOperand[operandIndex].sel);
	}
	else
	{
		if( GPU7_ALU_SRC_IS_CBANK0(aluInstruction->sourceOperand[operandIndex].sel) )
		{
			uniformOffset = GPU7_ALU_SRC_GET_CBANK0_INDEX(aluInstruction->sourceOperand[operandIndex].sel) + aluInstruction->cfInstruction->cBank0AddrBase;
		}
		else
		{
			uniformOffset = GPU7_ALU_SRC_GET_CBANK1_INDEX(aluInstruction->sourceOperand[operandIndex].sel) + aluInstruction->cfInstruction->cBank1AddrBase;
		}
	}
	if( aluInstruction->sourceOperand[operandIndex].rel != 0 )
	{
		if (aluInstruction->indexMode == GPU7_INDEX_AR_X)
			src->addFmt("ARi.x+{}", uniformOffset);
		else if (aluInstruction->indexMode == GPU7_INDEX_AR_Y)
			src->addFmt("ARi.y+{}", uniformOffset);
		else if (aluInstruction->indexMode == GPU7_INDEX_AR_Z)
			src->addFmt("ARi.z+{}", uniformOffset);
		else if (aluInstruction->indexMode == GPU7_INDEX_AR_W)
			src->addFmt("ARi.w+{}", uniformOffset);
		else
			cemu_assert_unimplemented();
	}
	else
	{
		src->addFmt("{}", uniformOffset);
	}
}

void _emitUniformAccessCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstruction, sint32 operandIndex, sint32 requiredType)
{
	StringBuf* src = shaderContext->shaderSource;
	if(shaderContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_REMAPPED )
	{
		// uniform registers or buffers are accessed statically with predictable offsets
		// find entry in remapped uniform
		if( aluInstruction->sourceOperand[operandIndex].rel != 0 )
			debugBreakpoint();
		bool isUniformRegister = GPU7_ALU_SRC_IS_CFILE(aluInstruction->sourceOperand[operandIndex].sel);
		sint32 uniformOffset = 0; // index into array
		sint32 uniformBufferIndex = 0;
		if( isUniformRegister )
		{
			uniformOffset = GPU7_ALU_SRC_GET_CFILE_INDEX(aluInstruction->sourceOperand[operandIndex].sel);
			uniformBufferIndex = 0;
		}
		else
		{
			if( GPU7_ALU_SRC_IS_CBANK0(aluInstruction->sourceOperand[operandIndex].sel) )
			{
				uniformOffset = GPU7_ALU_SRC_GET_CBANK0_INDEX(aluInstruction->sourceOperand[operandIndex].sel) + aluInstruction->cfInstruction->cBank0AddrBase;
				uniformBufferIndex = aluInstruction->cfInstruction->cBank0Index;
			}
			else
			{
				uniformOffset = GPU7_ALU_SRC_GET_CBANK1_INDEX(aluInstruction->sourceOperand[operandIndex].sel) + aluInstruction->cfInstruction->cBank1AddrBase;
				uniformBufferIndex = aluInstruction->cfInstruction->cBank1Index;
			}
		}
		LatteDecompilerRemappedUniformEntry_t* remappedUniformEntry = NULL;
		for(size_t i=0; i< shaderContext->shader->list_remappedUniformEntries.size(); i++)
		{
			LatteDecompilerRemappedUniformEntry_t* remappedUniformEntryItr = shaderContext->shader->list_remappedUniformEntries.data() + i;
			if( remappedUniformEntryItr->isRegister && isUniformRegister )
			{
				if( remappedUniformEntryItr->index == uniformOffset )
				{
					remappedUniformEntry = remappedUniformEntryItr;
					break;
				}
			}
			else
			{
				if( remappedUniformEntryItr->kcacheBankId == uniformBufferIndex && remappedUniformEntryItr->index == uniformOffset )
				{
					remappedUniformEntry = remappedUniformEntryItr;
					break;
				}
			}
		}
		cemu_assert_debug(remappedUniformEntry);
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, requiredType);
		if(shaderContext->shader->shaderType == LatteConst::ShaderType::Vertex )
			src->addFmt("uf_remappedVS[{}]", remappedUniformEntry->mappedIndex);
		else if(shaderContext->shader->shaderType == LatteConst::ShaderType::Pixel )
			src->addFmt("uf_remappedPS[{}]", remappedUniformEntry->mappedIndex);
		else if(shaderContext->shader->shaderType == LatteConst::ShaderType::Geometry )
			src->addFmt("uf_remappedGS[{}]", remappedUniformEntry->mappedIndex);
		else
			debugBreakpoint();
		_appendChannelAccess(src, aluInstruction->sourceOperand[operandIndex].chan);
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, requiredType);
	}
	else if( shaderContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CFILE )
	{
		// uniform registers are accessed with unpredictable (dynamic) offset
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, requiredType);
		if(shaderContext->shader->shaderType == LatteConst::ShaderType::Vertex )
			src->add("uf_uniformRegisterVS[");
		else if (shaderContext->shader->shaderType == LatteConst::ShaderType::Pixel)
			src->add("uf_uniformRegisterPS[");
		else if(shaderContext->shader->shaderType == LatteConst::ShaderType::Geometry )
			src->add("uf_uniformRegisterGS[");
		else
			debugBreakpoint();
		_emitUniformAccessIndexCode(shaderContext, aluInstruction, operandIndex);
		src->add("]");

		_appendChannelAccess(src, aluInstruction->sourceOperand[operandIndex].chan);
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, requiredType);
	}
	else if( shaderContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK )
	{
		// uniform buffers are available as a whole
		bool isUniformRegister = GPU7_ALU_SRC_IS_CFILE(aluInstruction->sourceOperand[operandIndex].sel);
		if( isUniformRegister )
			debugBreakpoint();
		sint32 uniformBufferIndex = 0;
		if( GPU7_ALU_SRC_IS_CBANK0(aluInstruction->sourceOperand[operandIndex].sel) )
		{
			uniformBufferIndex = aluInstruction->cfInstruction->cBank0Index;
		}
		else
		{
			uniformBufferIndex = aluInstruction->cfInstruction->cBank1Index;
		}
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, requiredType);
		src->addFmt("{}{}[", _getShaderUniformBlockVariableName(shaderContext->shader->shaderType), uniformBufferIndex);
		_emitUniformAccessIndexCode(shaderContext, aluInstruction, operandIndex);
		src->addFmt("]");

		_appendChannelAccess(src, aluInstruction->sourceOperand[operandIndex].chan);
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, requiredType);
	}
	else
		debugBreakpoint();
}

// Generates (slow) code to read an indexed GPR
void _emitCodeToReadRelativeGPR(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstruction, sint32 operandIndex, sint32 requiredType)
{
	StringBuf* src = shaderContext->shaderSource;
	uint32 gprBaseIndex = GPU7_ALU_SRC_GET_GPR_INDEX(aluInstruction->sourceOperand[operandIndex].sel);
	cemu_assert_debug(aluInstruction->sourceOperand[operandIndex].rel != 0);

	if( shaderContext->typeTracker.useArrayGPRs )
	{
		_emitTypeConversionPrefix(shaderContext, shaderContext->typeTracker.defaultDataType, requiredType);
		src->add(_getRegisterVarName(shaderContext, gprBaseIndex, aluInstruction->indexMode));
		_appendChannelAccess(src, aluInstruction->sourceOperand[operandIndex].chan);
		_emitTypeConversionSuffix(shaderContext, shaderContext->typeTracker.defaultDataType, requiredType);
		return;
	}

	char indexAccessCode[64];
	if (aluInstruction->indexMode == GPU7_INDEX_AR_X)
		sprintf(indexAccessCode, "ARi.x");
	else if (aluInstruction->indexMode == GPU7_INDEX_AR_Y)
		sprintf(indexAccessCode, "ARi.y");
	else if (aluInstruction->indexMode == GPU7_INDEX_AR_Z)
		sprintf(indexAccessCode, "ARi.z");
	else if (aluInstruction->indexMode == GPU7_INDEX_AR_W)
		sprintf(indexAccessCode, "ARi.w");
	else
		cemu_assert_unimplemented();
	
	if( LATTE_DECOMPILER_DTYPE_SIGNED_INT != requiredType )
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, requiredType);

	// generated code looks like this:
	// result = ((lookupIndex==0)?GPR5:(lookupIndex==1)?GPR6:(lookupIndex==2)?GPR7:...:(lookupIndex==122)?GPR127:0)
	src->add("(");
	for(sint32 i=gprBaseIndex; i<LATTE_NUM_GPR; i++)
	{
		// only emit access code for registers which are potentially written
		if((shaderContext->analyzer.gprUseMask[i / 8] & (1 << (i % 8))) == 0 )
			continue;
		src->addFmt("({}=={})?", indexAccessCode, i-gprBaseIndex);
		// code to access gpr
		uint32 gprIndex = i;
		src->add(_getRegisterVarName(shaderContext, i));
		_appendChannelAccess(src, aluInstruction->sourceOperand[operandIndex].chan);
		src->add(":");
	}
	src->add("0)");
	if( LATTE_DECOMPILER_DTYPE_SIGNED_INT != requiredType )
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, requiredType);
}

void _emitOperandInputCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstruction, sint32 operandIndex, sint32 requiredType)
{
	StringBuf* src = shaderContext->shaderSource;
	if(	operandIndex < 0 || operandIndex >= 3 )
		debugBreakpoint();
	sint32 requiredTypeOut = requiredType;
	if( requiredType != LATTE_DECOMPILER_DTYPE_FLOAT && (aluInstruction->sourceOperand[operandIndex].abs != 0 || aluInstruction->sourceOperand[operandIndex].neg != 0) )
	{
		// we need to apply float operations on the input but it's not read as a float
		// force internal required type to float and then cast it back to whatever type is actually required
		requiredType = LATTE_DECOMPILER_DTYPE_FLOAT;
	}

	if( requiredTypeOut != requiredType )
		_emitTypeConversionPrefix(shaderContext, requiredType, requiredTypeOut);

	if( aluInstruction->sourceOperand[operandIndex].neg != 0 )
		src->add("-(");
	if( aluInstruction->sourceOperand[operandIndex].abs != 0 )
		src->add("abs(");

	if( GPU7_ALU_SRC_IS_GPR(aluInstruction->sourceOperand[operandIndex].sel) )
	{
		if( aluInstruction->sourceOperand[operandIndex].rel != 0 )
		{
			_emitCodeToReadRelativeGPR(shaderContext, aluInstruction, operandIndex, requiredType);
		}
		else
		{
			uint32 gprIndex = GPU7_ALU_SRC_GET_GPR_INDEX(aluInstruction->sourceOperand[operandIndex].sel);
			if( requiredType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
			{
				// signed int 32bit
				sint32 currentRegisterElementType = _getInputRegisterDataType(shaderContext, aluInstruction, operandIndex);
				// write code for register input
				_emitTypeConversionPrefix(shaderContext, currentRegisterElementType, requiredType);
				_emitALURegisterInputAccessCode(shaderContext, aluInstruction, operandIndex);
				_emitTypeConversionSuffix(shaderContext, currentRegisterElementType, requiredType);
			}
			else if( requiredType == LATTE_DECOMPILER_DTYPE_UNSIGNED_INT )
			{
				// unsigned int 32bit
				sint32 currentRegisterElementType = _getInputRegisterDataType(shaderContext, aluInstruction, operandIndex);
				if( currentRegisterElementType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
				{
					// need to convert from int to uint
					src->add("uint(");
				}
				else if( currentRegisterElementType == LATTE_DECOMPILER_DTYPE_UNSIGNED_INT )
				{
					// no extra work necessary
				}
				else
					debugBreakpoint();
				// write code for register input
				_emitALURegisterInputAccessCode(shaderContext, aluInstruction, operandIndex);
				if( currentRegisterElementType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
				{
					src->add(")");
				}
			}
			else if( requiredType == LATTE_DECOMPILER_DTYPE_FLOAT )
			{
				// float 32bit
				sint32 currentRegisterElementType = _getInputRegisterDataType(shaderContext, aluInstruction, operandIndex);
				if( currentRegisterElementType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
				{
					// need to convert (not cast) from int bits to float
					src->add("intBitsToFloat(");
				}
				else if( currentRegisterElementType == LATTE_DECOMPILER_DTYPE_FLOAT )
				{
					// no extra work necessary
				}
				else
					debugBreakpoint();
				// write code for register input
				_emitALURegisterInputAccessCode(shaderContext, aluInstruction, operandIndex);
				if( currentRegisterElementType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
				{
					src->add(")");
				}
			}
			else
				debugBreakpoint();
		}
	}
	else if( GPU7_ALU_SRC_IS_CONST_0F(aluInstruction->sourceOperand[operandIndex].sel) )
	{
		if(requiredType == LATTE_DECOMPILER_DTYPE_SIGNED_INT || requiredType == LATTE_DECOMPILER_DTYPE_UNSIGNED_INT)
			src->add("0");
		else if( requiredType == LATTE_DECOMPILER_DTYPE_FLOAT )
			src->add("0.0");
	}
	else if( GPU7_ALU_SRC_IS_CONST_1F(aluInstruction->sourceOperand[operandIndex].sel) )
	{
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, requiredType);
		src->add("1.0");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, requiredType);
	}
	else if( GPU7_ALU_SRC_IS_CONST_0_5F(aluInstruction->sourceOperand[operandIndex].sel) )
	{
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, requiredType);
		src->add("0.5");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, requiredType);
	}
	else if( GPU7_ALU_SRC_IS_CONST_1I(aluInstruction->sourceOperand[operandIndex].sel) )
	{
		if (requiredType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
			src->add("int(1)");
		else if (requiredType == LATTE_DECOMPILER_DTYPE_UNSIGNED_INT)
			src->add("uint(1)");
		else
			cemu_assert_suspicious();
	}
	else if( GPU7_ALU_SRC_IS_CONST_M1I(aluInstruction->sourceOperand[operandIndex].sel) )
	{
		if( requiredType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
			src->add("int(-1)");
		else
			cemu_assert_suspicious();
	}
	else if( GPU7_ALU_SRC_IS_LITERAL(aluInstruction->sourceOperand[operandIndex].sel) )
	{
		if( requiredType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
			src->addFmt("0x{:x}", aluInstruction->literalData.w[aluInstruction->sourceOperand[operandIndex].chan]);
		else if( requiredType == LATTE_DECOMPILER_DTYPE_UNSIGNED_INT )
			src->addFmt("uint(0x{:x})", aluInstruction->literalData.w[aluInstruction->sourceOperand[operandIndex].chan]);
		else if (requiredType == LATTE_DECOMPILER_DTYPE_FLOAT)
		{
			uint32 constVal = aluInstruction->literalData.w[aluInstruction->sourceOperand[operandIndex].chan];
			sint32 exponent = (constVal >> 23) & 0xFF;
			exponent -= 127;
			if ((constVal & 0xFF) == 0 && exponent >= -10 && exponent <= 10)
			{
				src->add(_FormatFloatAsGLSLConstant(*(float*)&constVal));
			}
			else
				src->addFmt("intBitsToFloat(0x{:08x})", constVal);
		}
	}
	else if( GPU7_ALU_SRC_IS_CFILE(aluInstruction->sourceOperand[operandIndex].sel) )
	{
		_emitUniformAccessCode(shaderContext, aluInstruction, operandIndex, requiredType);
	}
	else if( GPU7_ALU_SRC_IS_CBANK0(aluInstruction->sourceOperand[operandIndex].sel) ||
		GPU7_ALU_SRC_IS_CBANK1(aluInstruction->sourceOperand[operandIndex].sel) )
	{
		_emitUniformAccessCode(shaderContext, aluInstruction, operandIndex, requiredType);
	}
	else if( GPU7_ALU_SRC_IS_PV(aluInstruction->sourceOperand[operandIndex].sel) )
	{
		sint32 currentPVDataType = _getInputRegisterDataType(shaderContext, aluInstruction, operandIndex);
		_emitTypeConversionPrefix(shaderContext, currentPVDataType, requiredType);
		_emitPVPSAccessCode(shaderContext, aluInstruction, operandIndex, aluInstruction->sourceOperand[operandIndex].chan);
		_emitTypeConversionSuffix(shaderContext, currentPVDataType, requiredType);
	}
	else if( GPU7_ALU_SRC_IS_PS(aluInstruction->sourceOperand[operandIndex].sel) )
	{
		sint32 currentPSDataType = _getInputRegisterDataType(shaderContext, aluInstruction, operandIndex);
		_emitTypeConversionPrefix(shaderContext, currentPSDataType, requiredType);
		_emitPVPSAccessCode(shaderContext, aluInstruction, operandIndex, 4);
		_emitTypeConversionSuffix(shaderContext, currentPSDataType, requiredType);
	}
	else
	{
		cemuLog_log(LogType::Force, "Unsupported shader ALU operand sel 0x%x\n", aluInstruction->sourceOperand[operandIndex].sel);
		debugBreakpoint();
	}

	if( aluInstruction->sourceOperand[operandIndex].abs != 0 )
		src->add(")");
	if( aluInstruction->sourceOperand[operandIndex].neg != 0 )
		src->add(")");

	if( requiredTypeOut != requiredType )
		_emitTypeConversionSuffix(shaderContext, requiredType, requiredTypeOut);
}

void _emitTypeConversionPrefix(LatteDecompilerShaderContext* shaderContext, sint32 sourceType, sint32 destinationType)
{
	if( sourceType == destinationType )
		return;
	StringBuf* src = shaderContext->shaderSource;
	if (sourceType == LATTE_DECOMPILER_DTYPE_FLOAT && destinationType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
		src->add("floatBitsToInt(");
	else if (sourceType == LATTE_DECOMPILER_DTYPE_FLOAT && destinationType == LATTE_DECOMPILER_DTYPE_UNSIGNED_INT)
		src->add("floatBitsToUint(");
	else if( sourceType == LATTE_DECOMPILER_DTYPE_SIGNED_INT && destinationType == LATTE_DECOMPILER_DTYPE_FLOAT )
		src->add("intBitsToFloat(");
	else if( sourceType == LATTE_DECOMPILER_DTYPE_UNSIGNED_INT && destinationType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
		src->add("int(");
	else if( sourceType == LATTE_DECOMPILER_DTYPE_SIGNED_INT && destinationType == LATTE_DECOMPILER_DTYPE_UNSIGNED_INT )
		src->add("uint(");
	else
		cemu_assert_debug(false);
}

void _emitTypeConversionSuffix(LatteDecompilerShaderContext* shaderContext, sint32 sourceType, sint32 destinationType)
{
	if( sourceType == destinationType )
		return;
	StringBuf* src = shaderContext->shaderSource;
	src->add(")");
}

template<int TDataType>
void _emitALUOperationBinary(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluInstruction, const char* operandStr)
{
	StringBuf* src = shaderContext->shaderSource;
	sint32 outputType = _getALUInstructionOutputDataType(shaderContext, aluInstruction);
	_emitInstructionOutputVariableName(shaderContext, aluInstruction);
	src->add(" = ");
	_emitTypeConversionPrefix(shaderContext, TDataType, outputType);
	_emitOperandInputCode(shaderContext, aluInstruction, 0, TDataType);
	src->add((char*)operandStr);
	_emitOperandInputCode(shaderContext, aluInstruction, 1, TDataType);
	_emitTypeConversionSuffix(shaderContext, TDataType, outputType);
	src->add(";" _CRLF);
}

static bool _isSameGPROperand(LatteDecompilerALUInstruction* aluInstruction, sint32 opIndexA, sint32 opIndexB)
{
	if (aluInstruction->sourceOperand[opIndexA].sel != aluInstruction->sourceOperand[opIndexB].sel)
		return false;
	if (!GPU7_ALU_SRC_IS_GPR(aluInstruction->sourceOperand[opIndexA].sel))
		return false;
	if (aluInstruction->sourceOperand[opIndexA].chan != aluInstruction->sourceOperand[opIndexB].chan)
		return false;
	if (aluInstruction->sourceOperand[opIndexA].abs != aluInstruction->sourceOperand[opIndexB].abs)
		return false;
	if (aluInstruction->sourceOperand[opIndexA].neg != aluInstruction->sourceOperand[opIndexB].neg)
		return false;
	if (aluInstruction->sourceOperand[opIndexA].rel != aluInstruction->sourceOperand[opIndexB].rel)
		return false;
	return true;
}

static bool _operandHasModifiers(LatteDecompilerALUInstruction* aluInstruction, sint32 opIndex)
{
	return aluInstruction->sourceOperand[opIndex].abs != 0 || aluInstruction->sourceOperand[opIndex].neg != 0;
}

void _emitALUOP2InstructionCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction, LatteDecompilerALUInstruction* aluInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	sint32 outputType = _getALUInstructionOutputDataType(shaderContext, aluInstruction); // data type of output
	if( aluInstruction->opcode == ALU_OP2_INST_MOV )
	{
		bool requiresFloatMove = false;
		requiresFloatMove = aluInstruction->sourceOperand[0].abs != 0 || aluInstruction->sourceOperand[0].neg != 0;
		if( requiresFloatMove )
		{
			// abs/neg operations are applied to source operand, do float based move
			_emitInstructionOutputVariableName(shaderContext, aluInstruction);
			src->add(" = ");
			_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
			_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
			_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
			src->add(";" _CRLF);
		}
		else
		{
			_emitInstructionOutputVariableName(shaderContext, aluInstruction);
			src->add(" = ");
			_emitOperandInputCode(shaderContext, aluInstruction, 0, outputType);
			src->add(";" _CRLF);
		}
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_MOVA_FLOOR )
	{
		cemu_assert_debug(aluInstruction->writeMask == 0);
		cemu_assert_debug(aluInstruction->omod == 0);
		src->add("tempResultf = ");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(";" _CRLF);
		src->add("tempResultf = floor(tempResultf);" _CRLF);
		src->add("tempResultf = clamp(tempResultf, -256.0, 255.0);" _CRLF);
		// set AR
		if( aluInstruction->destElem == 0 )
			src->add("ARi.x = int(tempResultf);" _CRLF);
		else if( aluInstruction->destElem == 1 )
			src->add("ARi.y = int(tempResultf);" _CRLF);
		else if( aluInstruction->destElem == 2 )
			src->add("ARi.z = int(tempResultf);" _CRLF);
		else 
			src->add("ARi.w = int(tempResultf);" _CRLF);
		// set output
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		if( outputType != LATTE_DECOMPILER_DTYPE_SIGNED_INT )
			debugBreakpoint(); // todo
		src->add("floatBitsToInt(tempResultf)");
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_MOVA_INT )
	{
		cemu_assert_debug(aluInstruction->writeMask == 0);
		cemu_assert_debug(aluInstruction->omod == 0);
		src->add("tempResulti = ");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add(";" _CRLF);
		src->add("tempResulti = clamp(tempResulti, -256, 255);" _CRLF);
		// set AR
		if( aluInstruction->destElem == 0 )
			src->add("ARi.x = tempResulti;" _CRLF);
		else if( aluInstruction->destElem == 1 )
			src->add("ARi.y = tempResulti;" _CRLF);
		else if( aluInstruction->destElem == 2 )
			src->add("ARi.z = tempResulti;" _CRLF);
		else 
			src->add("ARi.w = tempResulti;" _CRLF);
		// set output
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		if( outputType != LATTE_DECOMPILER_DTYPE_SIGNED_INT )
			debugBreakpoint(); // todo
		src->add("tempResulti");
		src->add(";" _CRLF);

	}
	else if( aluInstruction->opcode == ALU_OP2_INST_ADD )
	{
		_emitALUOperationBinary<LATTE_DECOMPILER_DTYPE_FLOAT>(shaderContext, aluInstruction, " + ");
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_MUL )
	{
		// 0*anything is always 0
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);

		// if any operand is a non-zero literal or constant we can use standard multiplication
		bool useDefaultMul = false;
		if (GPU7_ALU_SRC_IS_CONST_0F(aluInstruction->sourceOperand[0].sel) || GPU7_ALU_SRC_IS_CONST_0F(aluInstruction->sourceOperand[1].sel))
		{
			// result is always zero
			src->add("0.0");
		}
		else
		{
			// multiply
			if (GPU7_ALU_SRC_IS_LITERAL(aluInstruction->sourceOperand[0].sel) || GPU7_ALU_SRC_IS_LITERAL(aluInstruction->sourceOperand[1].sel) ||
				GPU7_ALU_SRC_IS_ANY_CONST(aluInstruction->sourceOperand[0].sel) || GPU7_ALU_SRC_IS_ANY_CONST(aluInstruction->sourceOperand[1].sel))
			{
				useDefaultMul = true;
			}
			if (shaderContext->options->strictMul && useDefaultMul == false)
			{
				src->add("mul_nonIEEE(");
				_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
				src->add(", ");
				_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_FLOAT);
				src->add(")");
			}
			else
			{
				_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
				src->add(" * ");
				_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_FLOAT);
			}
		}
		
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_MUL_IEEE )
	{
		// 0*anything according to IEEE rules
		_emitALUOperationBinary<LATTE_DECOMPILER_DTYPE_FLOAT>(shaderContext, aluInstruction, " * ");
	}
	else if (aluInstruction->opcode == ALU_OP2_INST_RECIP_IEEE)
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("1.0");
		src->add(" / ");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if (aluInstruction->opcode == ALU_OP2_INST_RECIP_FF)
	{
		// untested (BotW bombs)
		src->add("tempResultf = 1.0 / (");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(");" _CRLF);
		// INF becomes 0.0
		src->add("if( isinf(tempResultf) == true && (floatBitsToInt(tempResultf)&0x80000000) == 0 ) tempResultf = 0.0;" _CRLF);
		// -INF becomes -0.0
		src->add("else if( isinf(tempResultf) == true && (floatBitsToInt(tempResultf)&0x80000000) != 0 ) tempResultf = -0.0;" _CRLF);
		// assign result to output
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("tempResultf");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_RECIPSQRT_IEEE ||
		aluInstruction->opcode == ALU_OP2_INST_RECIPSQRT_CLAMPED ||
		aluInstruction->opcode == ALU_OP2_INST_RECIPSQRT_FF )
	{
		// todo: This should be correct but testing is needed
		src->add("tempResultf = 1.0 / sqrt(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(");" _CRLF);
		if (aluInstruction->opcode == ALU_OP2_INST_RECIPSQRT_CLAMPED)
		{
			// note: if( -INF < 0.0 ) does not resolve to true
			src->add("if( isinf(tempResultf) == true && (floatBitsToInt(tempResultf)&0x80000000) != 0 ) tempResultf = -3.40282347E+38F;" _CRLF);
			src->add("else if( isinf(tempResultf) == true && (floatBitsToInt(tempResultf)&0x80000000) == 0 ) tempResultf = 3.40282347E+38F;" _CRLF);
		}
		else if (aluInstruction->opcode == ALU_OP2_INST_RECIPSQRT_FF)
		{
			// untested (BotW bombs)
			src->add("if( isinf(tempResultf) == true && (floatBitsToInt(tempResultf)&0x80000000) != 0 ) tempResultf = -0.0;" _CRLF);
			src->add("else if( isinf(tempResultf) == true && (floatBitsToInt(tempResultf)&0x80000000) == 0 ) tempResultf = 0.0;" _CRLF);
		}
		// assign result to output
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("tempResultf");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_MAX ||
		aluInstruction->opcode == ALU_OP2_INST_MIN ||
		aluInstruction->opcode == ALU_OP2_INST_MAX_DX10 ||
		aluInstruction->opcode == ALU_OP2_INST_MIN_DX10 )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		if( aluInstruction->opcode == ALU_OP2_INST_MAX )
			src->add("max");
		else if( aluInstruction->opcode == ALU_OP2_INST_MIN )
			src->add("min");
		else if (aluInstruction->opcode == ALU_OP2_INST_MAX_DX10)
			src->add("max");
		else if (aluInstruction->opcode == ALU_OP2_INST_MIN_DX10)
			src->add("min");
		src->add("(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(", ");
		_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_FLOOR ||
		aluInstruction->opcode == ALU_OP2_INST_FRACT ||
		aluInstruction->opcode == ALU_OP2_INST_TRUNC )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		if( aluInstruction->opcode == ALU_OP2_INST_FLOOR )
			src->add("floor");
		else if( aluInstruction->opcode == ALU_OP2_INST_FRACT )
			src->add("fract");
		else
			src->add("trunc");
		src->add("(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_LOG_CLAMPED ||
		aluInstruction->opcode == ALU_OP2_INST_LOG_IEEE )
	{
		src->add("tempResultf = max(0.0, ");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(");" _CRLF);

		src->add("tempResultf = log2(tempResultf);" _CRLF);
		if( aluInstruction->opcode == ALU_OP2_INST_LOG_CLAMPED )
		{
			src->add("if( isinf(tempResultf) == true ) tempResultf = -3.40282347E+38F;" _CRLF);
		}
		// assign result to output
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("tempResultf");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_RNDNE )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("roundEven");
		src->add("(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_EXP_IEEE )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("exp2");
		src->add("(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_SQRT_IEEE )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("sqrt");
		src->add("(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_SIN ||
		aluInstruction->opcode == ALU_OP2_INST_COS )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		if( aluInstruction->opcode == ALU_OP2_INST_SIN )
			src->add("sin");
		else
			src->add("cos");
		src->add("((");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")/0.1591549367)");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_FLT_TO_INT )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add("int");
		src->add("(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_FLT_TO_UINT )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_UNSIGNED_INT, outputType);
		src->add("uint");
		src->add("(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_UNSIGNED_INT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_INT_TO_FLOAT )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("float(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add(")");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_UINT_TO_FLOAT )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("float(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_UNSIGNED_INT);
		src->add(")");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if (aluInstruction->opcode == ALU_OP2_INST_AND_INT)
		_emitALUOperationBinary<LATTE_DECOMPILER_DTYPE_SIGNED_INT>(shaderContext, aluInstruction, " & ");
	else if (aluInstruction->opcode == ALU_OP2_INST_OR_INT)
		_emitALUOperationBinary<LATTE_DECOMPILER_DTYPE_SIGNED_INT>(shaderContext, aluInstruction, " | ");
	else if (aluInstruction->opcode == ALU_OP2_INST_XOR_INT)
		_emitALUOperationBinary<LATTE_DECOMPILER_DTYPE_SIGNED_INT>(shaderContext, aluInstruction, " ^ ");
	else if( aluInstruction->opcode == ALU_OP2_INST_NOT_INT )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add("~(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add(")");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_ADD_INT )
		_emitALUOperationBinary<LATTE_DECOMPILER_DTYPE_SIGNED_INT>(shaderContext, aluInstruction, " + ");
	else if( aluInstruction->opcode == ALU_OP2_INST_MAX_INT || aluInstruction->opcode == ALU_OP2_INST_MIN_INT )
	{
		// not verified
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		if( aluInstruction->opcode == ALU_OP2_INST_MAX_INT )
			src->add(" = max(");
		else
			src->add(" = min(");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add(", ");
		_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add(");" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_SUB_INT )
	{
		// note: The AMD doc says src1 is on the left side but tests indicate otherwise. It's src0 - src1.
		_emitALUOperationBinary<LATTE_DECOMPILER_DTYPE_SIGNED_INT>(shaderContext, aluInstruction, " - ");
	}
	else if (aluInstruction->opcode == ALU_OP2_INST_MULLO_INT)
		_emitALUOperationBinary<LATTE_DECOMPILER_DTYPE_SIGNED_INT>(shaderContext, aluInstruction, " * ");
	else if (aluInstruction->opcode == ALU_OP2_INST_MULLO_UINT)
		_emitALUOperationBinary<LATTE_DECOMPILER_DTYPE_UNSIGNED_INT>(shaderContext, aluInstruction, " * ");
	else if( aluInstruction->opcode == ALU_OP2_INST_LSHL_INT )
		_emitALUOperationBinary<LATTE_DECOMPILER_DTYPE_SIGNED_INT>(shaderContext, aluInstruction, " << ");
	else if( aluInstruction->opcode == ALU_OP2_INST_LSHR_INT )
		_emitALUOperationBinary<LATTE_DECOMPILER_DTYPE_SIGNED_INT>(shaderContext, aluInstruction, " >> ");
	else if( aluInstruction->opcode == ALU_OP2_INST_ASHR_INT )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add(" >> ");
		_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_SETGT ||
		aluInstruction->opcode == ALU_OP2_INST_SETGE ||
		aluInstruction->opcode == ALU_OP2_INST_SETNE ||
		aluInstruction->opcode == ALU_OP2_INST_SETE )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		if( aluInstruction->opcode == ALU_OP2_INST_SETGT )
			src->add(" > ");
		else if( aluInstruction->opcode == ALU_OP2_INST_SETGE )
			src->add(" >= ");
		else if (aluInstruction->opcode == ALU_OP2_INST_SETNE)
			src->add(" != ");
		else if (aluInstruction->opcode == ALU_OP2_INST_SETE)
			src->add(" == ");
		_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")?1.0:0.0");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_SETGT_DX10 ||
		aluInstruction->opcode == ALU_OP2_INST_SETE_DX10 ||
		aluInstruction->opcode == ALU_OP2_INST_SETNE_DX10 ||
		aluInstruction->opcode == ALU_OP2_INST_SETGE_DX10 )
	{
		if( aluInstruction->omod != 0 )
			debugBreakpoint();
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add("((");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		if( aluInstruction->opcode == ALU_OP2_INST_SETE_DX10 )
			src->add(" == ");
		else if( aluInstruction->opcode == ALU_OP2_INST_SETNE_DX10 )
			src->add(" != ");
		else if( aluInstruction->opcode == ALU_OP2_INST_SETGT_DX10 )
			src->add(" > ");
		else if( aluInstruction->opcode == ALU_OP2_INST_SETGE_DX10 )
			src->add(" >= ");
		_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")?-1:0)");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add(";");
		src->add(_CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_SETE_INT ||
		aluInstruction->opcode == ALU_OP2_INST_SETNE_INT ||
		aluInstruction->opcode == ALU_OP2_INST_SETGT_INT || 
		aluInstruction->opcode == ALU_OP2_INST_SETGE_INT )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add("(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		if( aluInstruction->opcode == ALU_OP2_INST_SETE_INT )
			src->add(" == ");
		else if( aluInstruction->opcode == ALU_OP2_INST_SETNE_INT )
			src->add(" != ");
		else if( aluInstruction->opcode == ALU_OP2_INST_SETGT_INT )
			src->add(" > ");
		else if( aluInstruction->opcode == ALU_OP2_INST_SETGE_INT )
			src->add(" >= ");
		_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add(")?-1:0");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_SETGE_UINT ||
		aluInstruction->opcode == ALU_OP2_INST_SETGT_UINT )
	{
		// todo: Unsure if the result is unsigned or signed
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add("(");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_UNSIGNED_INT);
		if( aluInstruction->opcode == ALU_OP2_INST_SETGE_UINT )
			src->add(" >= ");
		else if( aluInstruction->opcode == ALU_OP2_INST_SETGT_UINT )
			src->add(" > ");
		_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_UNSIGNED_INT);
		src->add(")?int(0xFFFFFFFF):int(0x0)");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_PRED_SETGT ||
		aluInstruction->opcode == ALU_OP2_INST_PRED_SETGE ||
		aluInstruction->opcode == ALU_OP2_INST_PRED_SETE ||
		aluInstruction->opcode == ALU_OP2_INST_PRED_SETNE ||
		aluInstruction->opcode == ALU_OP2_INST_PRED_SETNE_INT ||
		aluInstruction->opcode == ALU_OP2_INST_PRED_SETE_INT ||
		aluInstruction->opcode == ALU_OP2_INST_PRED_SETGE_INT ||
		aluInstruction->opcode == ALU_OP2_INST_PRED_SETGT_INT )
	{
		cemu_assert_debug(aluInstruction->writeMask == 0);
		bool isIntPred = (aluInstruction->opcode == ALU_OP2_INST_PRED_SETNE_INT) || (aluInstruction->opcode == ALU_OP2_INST_PRED_SETE_INT) || (aluInstruction->opcode == ALU_OP2_INST_PRED_SETGE_INT) || (aluInstruction->opcode == ALU_OP2_INST_PRED_SETGT_INT);

		src->add("predResult");
		src->add(" = (");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, isIntPred?LATTE_DECOMPILER_DTYPE_SIGNED_INT:LATTE_DECOMPILER_DTYPE_FLOAT);

		if (aluInstruction->opcode == ALU_OP2_INST_PRED_SETE || aluInstruction->opcode == ALU_OP2_INST_PRED_SETE_INT)
			src->add(" == ");
		else if (aluInstruction->opcode == ALU_OP2_INST_PRED_SETGT || aluInstruction->opcode == ALU_OP2_INST_PRED_SETGT_INT)
			src->add(" > ");
		else if (aluInstruction->opcode == ALU_OP2_INST_PRED_SETGE || aluInstruction->opcode == ALU_OP2_INST_PRED_SETGE_INT)
			src->add(" >= ");
		else if (aluInstruction->opcode == ALU_OP2_INST_PRED_SETNE || aluInstruction->opcode == ALU_OP2_INST_PRED_SETNE_INT)
			src->add(" != ");
		else
			cemu_assert_debug(false);

		_emitOperandInputCode(shaderContext, aluInstruction, 1, isIntPred?LATTE_DECOMPILER_DTYPE_SIGNED_INT:LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(");" _CRLF);
		// handle result of predicate instruction based on current ALU clause type
		if( cfInstruction->type == GPU7_CF_INST_ALU_PUSH_BEFORE )
		{
			src->addFmt("{} = predResult;" _CRLF, _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth));
			src->addFmt("{} = predResult == true && {} == true;" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1), _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth));
		}
		else if( cfInstruction->type == GPU7_CF_INST_ALU_BREAK )
		{
			// leave current loop
			src->add("if( predResult == false ) break;" _CRLF);
		}
		else
			cemu_assert_debug(false);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_KILLE_INT ||
		aluInstruction->opcode == ALU_OP2_INST_KILLNE_INT ||
		aluInstruction->opcode == ALU_OP2_INST_KILLGT_INT)
	{
		src->add("if( ");
		src->add(" (");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		if( aluInstruction->opcode == ALU_OP2_INST_KILLE_INT )
			src->add(" == ");
		else if (aluInstruction->opcode == ALU_OP2_INST_KILLNE_INT)
			src->add(" != ");
		else if (aluInstruction->opcode == ALU_OP2_INST_KILLGT_INT)
			src->add(" > ");
		else
			debugBreakpoint();
		_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add(")");
		src->add(") discard;");
		src->add(_CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP2_INST_KILLGT ||
		aluInstruction->opcode == ALU_OP2_INST_KILLGE ||
		aluInstruction->opcode == ALU_OP2_INST_KILLE )
	{
		src->add("if( ");
		src->add(" (");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		if( aluInstruction->opcode == ALU_OP2_INST_KILLGT )
			src->add(" > ");
		else if( aluInstruction->opcode == ALU_OP2_INST_KILLGE )
			src->add(" >= ");
		else if( aluInstruction->opcode == ALU_OP2_INST_KILLE )
			src->add(" == ");
		else
			debugBreakpoint();
		_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")");
		src->add(") discard;");
		src->add(_CRLF);
	}
	else
	{
		src->add("Unsupported instruction;" _CRLF);
		debug_printf("Unsupported ALU op2 instruction 0x%x\n", aluInstruction->opcode);
		shaderContext->shader->hasError = true;
	}
}

void _emitALUOP3InstructionCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction, LatteDecompilerALUInstruction* aluInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	cemu_assert_debug(aluInstruction->destRel == 0); // todo
	sint32 outputType = _getALUInstructionOutputDataType(shaderContext, aluInstruction);

	/* check for common no-op or mov-like instructions */
	if (aluInstruction->opcode == ALU_OP3_INST_CMOVGE || 
		aluInstruction->opcode == ALU_OP3_INST_CMOVE || 
		aluInstruction->opcode == ALU_OP3_INST_CMOVGT || 
		aluInstruction->opcode == ALU_OP3_INST_CNDE_INT || 
		aluInstruction->opcode == ALU_OP3_INST_CNDGT_INT || 
		aluInstruction->opcode == ALU_OP3_INST_CMOVGE_INT)
	{
		if (_isSameGPROperand(aluInstruction, 1, 2) && !_operandHasModifiers(aluInstruction, 1))
		{
			// the condition is irrelevant as both operands are the same
			_emitInstructionOutputVariableName(shaderContext, aluInstruction);
			src->add(" = ");
			_emitOperandInputCode(shaderContext, aluInstruction, 1, outputType);
			src->add(";" _CRLF);
			return;
		}
	}


	/* generic handlers */
	if( aluInstruction->opcode == ALU_OP3_INST_MULADD ||
		aluInstruction->opcode == ALU_OP3_INST_MULADD_D2 ||
		aluInstruction->opcode == ALU_OP3_INST_MULADD_M2 ||
		aluInstruction->opcode == ALU_OP3_INST_MULADD_M4 ||
		aluInstruction->opcode == ALU_OP3_INST_MULADD_IEEE )
	{
		// todo: The difference between MULADD and MULADD IEEE is that the former has 0*anything=0 rule similar to MUL/MUL_IEEE?
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		if (aluInstruction->opcode != ALU_OP3_INST_MULADD) // avoid unnecessary parenthesis to improve code readability slightly
			src->add("(");
		
		bool useDefaultMul = false;
		if (GPU7_ALU_SRC_IS_LITERAL(aluInstruction->sourceOperand[0].sel) || GPU7_ALU_SRC_IS_LITERAL(aluInstruction->sourceOperand[1].sel) ||
			GPU7_ALU_SRC_IS_ANY_CONST(aluInstruction->sourceOperand[0].sel) || GPU7_ALU_SRC_IS_ANY_CONST(aluInstruction->sourceOperand[1].sel))
		{
			useDefaultMul = true;
		}
		if (aluInstruction->opcode == ALU_OP3_INST_MULADD_IEEE)
			useDefaultMul = true;

		if (shaderContext->options->strictMul && useDefaultMul == false)
		{
			src->add("mul_nonIEEE(");
			_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
			src->add(",");
			_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_FLOAT);
			src->add(")");
		}
		else
		{
			_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
			src->add(" * ");
			_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		}

		src->add(" + ");
		_emitOperandInputCode(shaderContext, aluInstruction, 2, LATTE_DECOMPILER_DTYPE_FLOAT);
		if(aluInstruction->opcode != ALU_OP3_INST_MULADD)
			src->add(")");
		if( aluInstruction->opcode == ALU_OP3_INST_MULADD_D2 )
			src->add("/2.0");
		else if( aluInstruction->opcode == ALU_OP3_INST_MULADD_M2 )
			src->add("*2.0");
		else if( aluInstruction->opcode == ALU_OP3_INST_MULADD_M4 )
			src->add("*4.0");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if(aluInstruction->opcode == ALU_OP3_INST_CNDE_INT || aluInstruction->opcode == ALU_OP3_INST_CNDGT_INT || aluInstruction->opcode == ALU_OP3_INST_CMOVGE_INT)
	{
		bool requiresFloatResult = (aluInstruction->sourceOperand[1].neg != 0) || (aluInstruction->sourceOperand[2].neg != 0);
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, requiresFloatResult?LATTE_DECOMPILER_DTYPE_FLOAT:LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add("((");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		if (aluInstruction->opcode == ALU_OP3_INST_CNDE_INT)
			src->add(" == ");
		else if (aluInstruction->opcode == ALU_OP3_INST_CNDGT_INT)
			src->add(" > ");
		else if (aluInstruction->opcode == ALU_OP3_INST_CMOVGE_INT)
			src->add(" >= ");
		src->add("0)?(");

		_emitOperandInputCode(shaderContext, aluInstruction, 1, requiresFloatResult?LATTE_DECOMPILER_DTYPE_FLOAT:LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add("):(");
		_emitOperandInputCode(shaderContext, aluInstruction, 2, requiresFloatResult?LATTE_DECOMPILER_DTYPE_FLOAT:LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add("))");
		_emitTypeConversionSuffix(shaderContext, requiresFloatResult?LATTE_DECOMPILER_DTYPE_FLOAT:LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluInstruction->opcode == ALU_OP3_INST_CMOVGE ||
		aluInstruction->opcode == ALU_OP3_INST_CMOVE ||
		aluInstruction->opcode == ALU_OP3_INST_CMOVGT )
	{
		_emitInstructionOutputVariableName(shaderContext, aluInstruction);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add("((");
		_emitOperandInputCode(shaderContext, aluInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		if (aluInstruction->opcode == ALU_OP3_INST_CMOVE)
			src->add(" == ");
		else if (aluInstruction->opcode == ALU_OP3_INST_CMOVGE)
			src->add(" >= ");
		else if (aluInstruction->opcode == ALU_OP3_INST_CMOVGT)
			src->add(" > ");
		src->add("0.0)?(");
		_emitOperandInputCode(shaderContext, aluInstruction, 1, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add("):(");
		_emitOperandInputCode(shaderContext, aluInstruction, 2, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add("))");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add(";" _CRLF);
	}
	else
	{
		src->add("Unsupported instruction;" _CRLF);
		debug_printf("Unsupported ALU op3 instruction 0x%x\n", aluInstruction->opcode);
		shaderContext->shader->hasError = true;
	}
}

void _emitALUReductionInstructionCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction* aluRedcInstruction[4])
{
	StringBuf* src = shaderContext->shaderSource;
	if( aluRedcInstruction[0]->isOP3 == false && (aluRedcInstruction[0]->opcode == ALU_OP2_INST_DOT4 || aluRedcInstruction[0]->opcode == ALU_OP2_INST_DOT4_IEEE) )
	{
		// todo: Figure out and implement the difference between normal DOT4 and DOT4_IEEE
		sint32 outputType = _getALUInstructionOutputDataType(shaderContext, aluRedcInstruction[0]);
		_emitInstructionOutputVariableName(shaderContext, aluRedcInstruction[0]);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);

		// dot(vec4(op0),vec4(op1))
		src->add("dot(vec4(");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[0], 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[1], 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[2], 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[3], 0, LATTE_DECOMPILER_DTYPE_FLOAT);	
		src->add("),vec4(");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[0], 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[1], 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[2], 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[3], 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add("))");

		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
	}
	else if( aluRedcInstruction[0]->isOP3 == false && (aluRedcInstruction[0]->opcode == ALU_OP2_INST_CUBE) )
	{
		/*
		 * How the CUBE instruction works (guessed mostly, based on DirectX/OpenGL spec):
		   Input: vec4, 3d direction vector (can be unnormalized) + w component (which can be ignored, since it only scales the vector but does not affect the direction)
	
		   First we figure out the major axis (closest axis-aligned vector). There are six possible vectors:
		   +rx	0
		   -rx	1
		   +ry	2
		   -ry	3
		   +rz	4
		   -rz	5
		   The major axis vector is calculated by looking at the largest (absolute) 3d vector component and then setting the other components to 0.0
		   The value that remains in the axis vector is referred to as 'MajorAxis' by the AMD documentation.
		   The S,T coordinates are taken from the other two components.
		   Example:	-0.5,0.2,0.4 -> -rx -> -0.5,0.0,0.0 MajorAxis: -0.5, S: 0.2 T: 0.4

		   The CUBE reduction instruction requires a specific mapping for the input vector:
		   src0 = Rn.zzxy 
		   src1 = Rn.yxzz
		   It's probably related to the way the instruction works internally?
		   If we look at the individual components per ALU unit:
		   z y	-> Compare y/z
		   z x  -> Compare x/z
		   x z  -> Compare x/z
		   y z  -> Compare y/z
		*/

		sint32 outputType;

		src->add("redcCUBE(");
		src->add("vec4(");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[0], 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[1], 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[2], 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[3], 0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add("),");
		src->add("vec4(");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[0], 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[1], 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[2], 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluRedcInstruction[3], 1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add("),");
		src->add("cubeMapSTM,cubeMapFaceId);" _CRLF);

		// dst.X (S)
		outputType = _getALUInstructionOutputDataType(shaderContext, aluRedcInstruction[0]);
		_emitInstructionOutputVariableName(shaderContext, aluRedcInstruction[0]);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("cubeMapSTM.x");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
		// dst.Y (T)
		outputType = _getALUInstructionOutputDataType(shaderContext, aluRedcInstruction[1]);
		_emitInstructionOutputVariableName(shaderContext, aluRedcInstruction[1]);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("cubeMapSTM.y");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
		// dst.Z (MajorAxis)
		outputType = _getALUInstructionOutputDataType(shaderContext, aluRedcInstruction[2]);
		_emitInstructionOutputVariableName(shaderContext, aluRedcInstruction[2]);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add("cubeMapSTM.z");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, outputType);
		src->add(";" _CRLF);
		// dst.W (FaceId)
		outputType = _getALUInstructionOutputDataType(shaderContext, aluRedcInstruction[3]);
		_emitInstructionOutputVariableName(shaderContext, aluRedcInstruction[3]);
		src->add(" = ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add("cubeMapFaceId");
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, outputType);
		src->add(";" _CRLF);
	}
	else
		cemu_assert_unimplemented();
}

void _emitALUClauseRegisterBackupCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction, sint32 startIndex)
{
	sint32 instructionGroupIndex = cfInstruction->instructionsALU[startIndex].instructionGroupIndex;
	size_t groupSize = 1;
	while ((startIndex + groupSize) < cfInstruction->instructionsALU.size())
	{
		if (instructionGroupIndex != cfInstruction->instructionsALU[startIndex + groupSize].instructionGroupIndex)
			break;
		groupSize++;
	}
	shaderContext->aluPVPSState->CreateGPRTemporaries(shaderContext, { cfInstruction->instructionsALU.data() + startIndex, groupSize });
}

/*
bool _isPVUsedInNextGroup(LatteDecompilerCFInstruction* cfInstruction, sint32 startIndex, sint32 pvUnit)
{
	sint32 currentGroupIndex = cfInstruction->instructionsALU[startIndex].instructionGroupIndex;
	for (sint32 i = startIndex + 1; i < (sint32)cfInstruction->instructionsALU.size(); i++)
	{
		LatteDecompilerALUInstruction& aluInstructionItr = cfInstruction->instructionsALU[i];
		if(aluInstructionItr.instructionGroupIndex == currentGroupIndex )
			continue;
		if ((sint32)aluInstructionItr.instructionGroupIndex > currentGroupIndex + 1)
			return false;
		// check OP code type
		if (aluInstructionItr.isOP3)
		{
			// op0
			if (GPU7_ALU_SRC_IS_PV(aluInstructionItr.sourceOperand[0].sel))
			{
				uint32 chan = aluInstructionItr.sourceOperand[0].chan;
				if (pvUnit == chan)
					return true;
			}
			// op1
			if (GPU7_ALU_SRC_IS_PV(aluInstructionItr.sourceOperand[1].sel))
			{
				uint32 chan = aluInstructionItr.sourceOperand[1].chan;
				if (pvUnit == chan)
					return true;
			}
			// op2
			if (GPU7_ALU_SRC_IS_PV(aluInstructionItr.sourceOperand[2].sel))
			{
				uint32 chan = aluInstructionItr.sourceOperand[2].chan;
				if (pvUnit == chan)
					return true;
			}
		}
		else
		{
			// op0
			if (GPU7_ALU_SRC_IS_PV(aluInstructionItr.sourceOperand[0].sel))
			{
				uint32 chan = aluInstructionItr.sourceOperand[0].chan;
				if (pvUnit == chan)
					return true;
			}
			// op1
			if (GPU7_ALU_SRC_IS_PV(aluInstructionItr.sourceOperand[1].sel))
			{
				uint32 chan = aluInstructionItr.sourceOperand[1].chan;
				if (pvUnit == chan)
					return true;
			}
			// todo: Not all operations use both operands
		}
	}
	return false;
}
*/

void _emitVec3(LatteDecompilerShaderContext* shaderContext, uint32 dataType, LatteDecompilerALUInstruction* aluInst0, sint32 opIdx0, LatteDecompilerALUInstruction* aluInst1, sint32 opIdx1, LatteDecompilerALUInstruction* aluInst2, sint32 opIdx2)
{
	StringBuf* src = shaderContext->shaderSource;
	if (dataType == LATTE_DECOMPILER_DTYPE_FLOAT)
	{
		src->add("vec3(");
		_emitOperandInputCode(shaderContext, aluInst0, opIdx0, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluInst1, opIdx1, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluInst2, opIdx2, LATTE_DECOMPILER_DTYPE_FLOAT);
		src->add(")");
	}
	else if (dataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
	{
		src->add("ivec3(");
		_emitOperandInputCode(shaderContext, aluInst0, opIdx0, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluInst1, opIdx1, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add(",");
		_emitOperandInputCode(shaderContext, aluInst2, opIdx2, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
		src->add(")");
	}
	else
		cemu_assert_unimplemented();
}

void _emitGPRVectorAssignment(LatteDecompilerShaderContext* shaderContext, LatteDecompilerALUInstruction** aluInstructions, sint32 count)
{
	StringBuf* src = shaderContext->shaderSource;
	// output var name (GPR)
	src->add(_getRegisterVarName(shaderContext, aluInstructions[0]->destGpr, -1));
	src->add(".");
	for (sint32 f = 0; f < count; f++)
	{
		src->add(_getElementStrByIndex(aluInstructions[f]->destElem));
	}
	src->add(" = ");
}

void _emitALUClauseCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction)
{
	ALUClauseTemporariesState pvpsState;
	shaderContext->aluPVPSState = &pvpsState;
	StringBuf* src = shaderContext->shaderSource;
	LatteDecompilerALUInstruction* aluRedcInstruction[4];
	size_t groupStartIndex = 0;
	for(size_t i=0; i<cfInstruction->instructionsALU.size(); i++)
	{
		LatteDecompilerALUInstruction& aluInstruction = cfInstruction->instructionsALU[i];
		if( aluInstruction.indexInGroup == 0 )
		{
			src->addFmt("// {}" _CRLF, aluInstruction.instructionGroupIndex);
			// apply PV/PS updates for previous group
			if (i > 0)
			{
				pvpsState.TrackGroupOutputPVPS(shaderContext, cfInstruction->instructionsALU.data() + groupStartIndex, i - groupStartIndex);
			}
			groupStartIndex = i;
			// backup registers which are read after being written
			_emitALUClauseRegisterBackupCode(shaderContext, cfInstruction, i);
		}
		// detect reduction instructions and use a special handler
		bool isReductionOperation = _isReductionInstruction(&aluInstruction);
		if( isReductionOperation )
		{
			cemu_assert_debug((i + 4) <= cfInstruction->instructionsALU.size());
			aluRedcInstruction[0] = &aluInstruction;
			aluRedcInstruction[1] = &cfInstruction->instructionsALU[i + 1];
			aluRedcInstruction[2] = &cfInstruction->instructionsALU[i + 2];
			aluRedcInstruction[3] = &cfInstruction->instructionsALU[i + 3];
			if( aluRedcInstruction[0]->isOP3 != aluRedcInstruction[1]->isOP3 || aluRedcInstruction[1]->isOP3 != aluRedcInstruction[2]->isOP3 || aluRedcInstruction[2]->isOP3 != aluRedcInstruction[3]->isOP3 )
				debugBreakpoint();
			if( aluRedcInstruction[0]->opcode != aluRedcInstruction[1]->opcode || aluRedcInstruction[1]->opcode != aluRedcInstruction[2]->opcode || aluRedcInstruction[2]->opcode != aluRedcInstruction[3]->opcode )
				debugBreakpoint();
			if( aluRedcInstruction[0]->omod != aluRedcInstruction[1]->omod || aluRedcInstruction[1]->omod != aluRedcInstruction[2]->omod || aluRedcInstruction[2]->omod != aluRedcInstruction[3]->omod )
				debugBreakpoint();
			if( aluRedcInstruction[0]->destClamp != aluRedcInstruction[1]->destClamp || aluRedcInstruction[1]->destClamp != aluRedcInstruction[2]->destClamp || aluRedcInstruction[2]->destClamp != aluRedcInstruction[3]->destClamp )
				debugBreakpoint();
			_emitALUReductionInstructionCode(shaderContext, aluRedcInstruction);
			i += 3; // skip the instructions that are part of the reduction operation
		}
		else /* not a reduction operation */
		{
			if( aluInstruction.isOP3 )
			{
				// op3
				_emitALUOP3InstructionCode(shaderContext, cfInstruction, &aluInstruction);
			}
			else
			{
				// op2
				if( aluInstruction.opcode == ALU_OP2_INST_NOP )
					continue; // skip NOP instruction
				_emitALUOP2InstructionCode(shaderContext, cfInstruction, &aluInstruction);
			}
		}
		// handle omod
		sint32 outputDataType = _getALUInstructionOutputDataType(shaderContext, &aluInstruction);
		if( aluInstruction.omod != ALU_OMOD_NONE )
		{
			if( outputDataType == LATTE_DECOMPILER_DTYPE_FLOAT )
			{
				_emitInstructionOutputVariableName(shaderContext, &aluInstruction);
				if( aluInstruction.omod == ALU_OMOD_MUL2 )
					src->add(" *= 2.0;" _CRLF);
				else if( aluInstruction.omod == ALU_OMOD_MUL4 )
					src->add(" *= 4.0;" _CRLF);
				else if( aluInstruction.omod == ALU_OMOD_DIV2 )
					src->add(" /= 2.0;" _CRLF);
			}
			else if( outputDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
			{
				_emitInstructionOutputVariableName(shaderContext, &aluInstruction);
				src->add(" = ");
				src->add("floatBitsToInt(intBitsToFloat(");
				_emitInstructionOutputVariableName(shaderContext, &aluInstruction);
				src->add(")");
				if( aluInstruction.omod == 1 )
					src->add(" * 2.0");
				else if( aluInstruction.omod == 2 )
					src->add(" * 4.0");
				else if( aluInstruction.omod == 3 )
					src->add(" / 2.0");
				src->add(");" _CRLF);
			}
			else
			{
				cemu_assert_unimplemented();
			}
		}
		// handle clamp
		if( aluInstruction.destClamp != 0 )
		{
			if( outputDataType == LATTE_DECOMPILER_DTYPE_FLOAT )
			{
				_emitInstructionOutputVariableName(shaderContext, &aluInstruction);
				src->add(" = clamp(");
				_emitInstructionOutputVariableName(shaderContext, &aluInstruction);
				src->add(", 0.0, 1.0);" _CRLF);
			}
			else if( outputDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
			{
				_emitInstructionOutputVariableName(shaderContext, &aluInstruction);
				src->add(" = clampFI32(");
				_emitInstructionOutputVariableName(shaderContext, &aluInstruction);
				src->add(");" _CRLF);
			}
			else
			{
				cemu_assert_unimplemented();
			}
		}
		// handle result broadcasting for reduction instructions
		if( isReductionOperation )
		{
			// reduction operations set all four PV components (todo: Needs further research. According to AMD docs, dot4 only sets PV.x? update: Unlike DOT4, CUBE sets all PV elements accordingly to their GPR output?)
			if( aluRedcInstruction[0]->opcode == ALU_OP2_INST_CUBE )
			{
				// CUBE
				for (sint32 f = 0; f < 4; f++)
				{
					if (aluRedcInstruction[f]->writeMask != 0)
						continue;
					_emitInstructionPVPSOutputVariableName(shaderContext, aluRedcInstruction[f]);
					src->add(" = ");
					_emitInstructionOutputVariableName(shaderContext, aluRedcInstruction[0]);
					src->add(";" _CRLF);
				}
			}
			else
			{
				// DOT4, DOT4_IEEE, etc.
				// reduction operation result is only set for output in redc[0], we also need to update redc[1] to redc[3]
				for(sint32 f=0; f<4; f++)
				{
					if( aluRedcInstruction[f]->writeMask == 0 )
						_emitInstructionPVPSOutputVariableName(shaderContext, aluRedcInstruction[f]);
					else
					{
						if (f == 0)
							continue;
						_emitInstructionOutputVariableName(shaderContext, aluRedcInstruction[f]);
					}
					src->add(" = ");
					_emitInstructionOutputVariableName(shaderContext, aluRedcInstruction[0]);
					src->add(";" _CRLF);
				}
			}
		}
	}
	shaderContext->aluPVPSState = nullptr;
}

/*
 * Emits code to access one component (xyzw) of the texture coordinate input vector
 */
void _emitTEXSampleCoordInputComponent(LatteDecompilerShaderContext* shaderContext, LatteDecompilerTEXInstruction* texInstruction, sint32 componentIndex, sint32 interpretSrcAsType)
{
	cemu_assert(componentIndex >= 0 && componentIndex < 4);
	cemu_assert_debug(interpretSrcAsType == LATTE_DECOMPILER_DTYPE_SIGNED_INT || interpretSrcAsType == LATTE_DECOMPILER_DTYPE_FLOAT);
	StringBuf* src = shaderContext->shaderSource;
	sint32 elementSel = texInstruction->textureFetch.srcSel[componentIndex];
	if (elementSel < 4)
	{
		_emitRegisterChannelAccessCode(shaderContext, texInstruction->srcGpr, elementSel, interpretSrcAsType);
		return;
	}
	const char* resultElemTable[4] = {"x","y","z","w"};
	if(interpretSrcAsType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
	{
		if( elementSel == 4 )
			src->add("floatBitsToInt(0.0)");
		else if( elementSel == 5 )
			src->add("floatBitsToInt(1.0)");
	}
	else if(interpretSrcAsType == LATTE_DECOMPILER_DTYPE_FLOAT )
	{
		if( elementSel == 4 )
			src->add("0.0");
		else if( elementSel == 5 )
			src->add("1.0");
	}
}

const char* _texGprAccessElemTable[8] = {"x","y","z","w","_","_","_","_"};

char* _getTexGPRAccess(LatteDecompilerShaderContext* shaderContext, sint32 gprIndex, uint32 dataType, sint8 selX, sint8 selY, sint8 selZ, sint8 selW, char* tempBuffer)
{
	// intBitsToFloat(R{}i.w)
	*tempBuffer = '\0';
	uint8 elemCount = (selX > 0 ? 1 : 0) + (selY > 0 ? 1 : 0) + (selZ > 0 ? 1 : 0) + (selW > 0 ? 1 : 0);
	if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
	{
		if (dataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
			; // no conversion
		else if (dataType == LATTE_DECOMPILER_DTYPE_FLOAT)
			strcat(tempBuffer, "intBitsToFloat(");
		else
			cemu_assert_unimplemented();
		strcat(tempBuffer, _getRegisterVarName(shaderContext, gprIndex));
		// _texGprAccessElemTable
		strcat(tempBuffer, ".");
		if (selX >= 0)
			strcat(tempBuffer, _texGprAccessElemTable[selX]);
		if (selY >= 0)
			strcat(tempBuffer, _texGprAccessElemTable[selY]);
		if (selZ >= 0)
			strcat(tempBuffer, _texGprAccessElemTable[selZ]);
		if (selW >= 0)
			strcat(tempBuffer, _texGprAccessElemTable[selW]);
		if (dataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
			; // no conversion
		else if (dataType == LATTE_DECOMPILER_DTYPE_FLOAT)
			strcat(tempBuffer, ")");
		else
			cemu_assert_unimplemented();
	}
	else if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
	{
		if (dataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
			cemu_assert_unimplemented();
		else if (dataType == LATTE_DECOMPILER_DTYPE_FLOAT)
			; // no conversion
		else
			cemu_assert_unimplemented();
		strcat(tempBuffer, _getRegisterVarName(shaderContext, gprIndex));
		// _texGprAccessElemTable
		strcat(tempBuffer, ".");
		if (selX >= 0)
			strcat(tempBuffer, _texGprAccessElemTable[selX]);
		if (selY >= 0)
			strcat(tempBuffer, _texGprAccessElemTable[selY]);
		if (selZ >= 0)
			strcat(tempBuffer, _texGprAccessElemTable[selZ]);
		if (selW >= 0)
			strcat(tempBuffer, _texGprAccessElemTable[selW]);
		if (dataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
			cemu_assert_unimplemented();
		else if (dataType == LATTE_DECOMPILER_DTYPE_FLOAT)
			; // no conversion
		else
			cemu_assert_unimplemented();
	}
	else
		cemu_assert_unimplemented();
	return tempBuffer;
}

void _emitTEXSampleTextureCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerTEXInstruction* texInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	if (texInstruction->textureFetch.textureIndex < 0 || texInstruction->textureFetch.textureIndex >= LATTE_NUM_MAX_TEX_UNITS)
	{
		// skip out of bounds texture unit access
		return;
	}

	auto texDim = shaderContext->shader->textureUnitDim[texInstruction->textureFetch.textureIndex];

	char tempBuffer0[32];
	char tempBuffer1[32];
	src->add(_getRegisterVarName(shaderContext, texInstruction->dstGpr));
	src->add(".");
	const char* resultElemTable[4] = {"x","y","z","w"};
	sint32 numWrittenElements = 0;
	for(sint32 f=0; f<4; f++)
	{
		if( texInstruction->dstSel[f] < 4 )
		{
			src->add(resultElemTable[f]);
			numWrittenElements++;
		}
		else if( texInstruction->dstSel[f] == 7 )
		{
			// masked and not written
		}
		else
		{
			debugBreakpoint();
		}
	}
	// texture sampler opcode
	uint32 texOpcode = texInstruction->opcode;
	if (shaderContext->shaderType == LatteConst::ShaderType::Vertex)
	{
		// vertex shader forces LOD to zero, but certain sampler types don't support textureLod(...) API
		if (texOpcode == GPU7_TEX_INST_SAMPLE_C_LZ)
			texOpcode = GPU7_TEX_INST_SAMPLE_C;
	}
	// check if offset is used
	bool hasOffset = false;
	if( texInstruction->textureFetch.offsetX != 0 || texInstruction->textureFetch.offsetY != 0 || texInstruction->textureFetch.offsetZ != 0 )
		hasOffset = true;
	// emit sample code
	if (shaderContext->shader->textureIsIntegerFormat[texInstruction->textureFetch.textureIndex])
	{
		// integer samplers
		if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT) // uint to int
		{
			if(numWrittenElements == 1)
				src->add(" = int(");
			else
				shaderContext->shaderSource->addFmt(" = ivec{}(", numWrittenElements);
		}
		else if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
			src->add(" = uintBitsToFloat(");
	}
	else
	{
		// float samplers
		if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
			src->add(" = floatBitsToInt(");
		else if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
			src->add(" = (");
	}

	bool unnormalizationHandled = false;
	bool useTexelCoordinates = false;

	// handle illegal combinations
	if (texOpcode == GPU7_TEX_INST_FETCH4 && (texDim == Latte::E_DIM::DIM_1D || texDim == Latte::E_DIM::DIM_1D_ARRAY))
	{
		// fetch4 is not allowed on 1D textures
		// seen in YWW during boss fight of Level 1-4
		// todo - investigate what this returns on actual HW
		if (numWrittenElements == 1)
			shaderContext->shaderSource->add("0.0");
		else
			shaderContext->shaderSource->addFmt("vec{}(0.0)", numWrittenElements);
		shaderContext->shaderSource->add(");" _CRLF);
		return;
	}


	if (texOpcode == GPU7_TEX_INST_SAMPLE && (texInstruction->textureFetch.unnormalized[0] && texInstruction->textureFetch.unnormalized[1] && texInstruction->textureFetch.unnormalized[2] && texInstruction->textureFetch.unnormalized[3]) )
	{
		// texture is likely a RECT
		if (hasOffset)
			cemu_assert_unimplemented();
		src->add("texelFetch(");
		unnormalizationHandled = true;
		useTexelCoordinates = true;
	}
	else if( texOpcode == GPU7_TEX_INST_FETCH4 )
	{
		if( hasOffset )
			cemu_assert_unimplemented();
		src->add("textureGather(");
	}
	else if( texOpcode == GPU7_TEX_INST_LD )
	{
		if( hasOffset )
			cemu_assert_unimplemented();
		src->add("texelFetch(");
		unnormalizationHandled = true;
		useTexelCoordinates = true;
	}
	else if( texOpcode == GPU7_TEX_INST_SAMPLE_L )
	{
		// sample with LOD value set in gpr.w (replaces computed LOD value)
		if( hasOffset )
			src->add("textureLodOffset(");
		else
			src->add("textureLod(");
	}
	else if (texOpcode == GPU7_TEX_INST_SAMPLE_LZ)
	{
		// sample with LOD set to 0.0 (replaces computed LOD value)
		if (hasOffset)
			src->add("textureLodOffset(");
		else
			src->add("textureLod(");
	}
	else if (texOpcode == GPU7_TEX_INST_SAMPLE_LB)
	{
		// sample with LOD biased
		// note: AMD doc says LOD bias is calculated from instruction LOD_BIAS field. But it appears that LOD bias is taken from input register. Might actually be both?
		if (hasOffset)
			src->add("textureOffset(");
		else
			src->add("texture(");
	}
	else if (texOpcode == GPU7_TEX_INST_SAMPLE)
	{
		if (hasOffset)
			src->add("textureOffset(");
		else
			src->add("texture(");
	}
	else if (texOpcode == GPU7_TEX_INST_SAMPLE_C_L)
	{
		// sample with LOD value set in gpr.w (replaces computed LOD value)
		if (hasOffset)
			src->add("textureLodOffset(");
		else
			src->add("textureLod(");
	}
	else if (texOpcode == GPU7_TEX_INST_SAMPLE_C_LZ)
	{
		// sample with LOD set to 0.0 (replaces computed LOD value)
		if (hasOffset)
			src->add("textureLodOffset(");
		else
			src->add("textureLod(");
	}
	else if (texOpcode == GPU7_TEX_INST_SAMPLE_C)
	{
		if (hasOffset)
			src->add("textureOffset(");
		else
			src->add("texture(");
	}
	else if (texOpcode == GPU7_TEX_INST_SAMPLE_G)
	{
		if (hasOffset)
			cemu_assert_unimplemented();
		src->add("textureGrad(");
	}
	else
	{
		if( hasOffset )
			cemu_assert_unimplemented();
		cemu_assert_unimplemented();
		src->add("texture(");
	}
	src->addFmt("{}{}, ", _getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex);

	// for textureGather() add shift (todo: depends on rounding mode set in sampler registers?)
	if (texOpcode == GPU7_TEX_INST_FETCH4)
	{
		if (texDim == Latte::E_DIM::DIM_2D)
		{
			//src->addFmt2("(vec2(-0.1) / vec2(textureSize({}{},0).xy)) + ", gpu7Decompiler_getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureIndex);
			
			// vec2(-0.00001) is minimum to break Nvidia
			// vec2(0.0001) is minimum to fix shadows on Intel, also fixes it on AMD (Windows and Linux)

			// todo - emulating coordinate rounding mode correctly is tricky
			// GX2 supports two modes: Truncate or rounding according to DX9 rules
			// Vulkan uses truncate mode when point sampling (min and mag is both nearest) otherwise it uses rounding
			
			// adding a small fixed bias is enough to avoid vendor-specific cases where small inaccuracies cause the number to get rounded down due to truncation
			src->addFmt("vec2(0.0001) + ");
		}
	}

	const sint32 texCoordDataType = (texOpcode == GPU7_TEX_INST_LD) ? LATTE_DECOMPILER_DTYPE_SIGNED_INT : LATTE_DECOMPILER_DTYPE_FLOAT;
	if(useTexelCoordinates)
	{
		// handle integer coordinates for texelFetch
		if (texDim == Latte::E_DIM::DIM_2D || texDim == Latte::E_DIM::DIM_2D_MSAA)
		{
			src->add("ivec2(");
			src->add("vec2(");
			_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 0, texCoordDataType);
			src->addFmt(", ");
			_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 1, texCoordDataType);

			src->addFmt(")*uf_tex{}Scale", texInstruction->textureFetch.textureIndex); // close vec2 and scale

			src->add("), 0"); // close ivec2 and lod param
			// todo - lod
		}
		else if (texDim == Latte::E_DIM::DIM_1D)
		{
			// VC DS games forget to initialize textures and use texel fetch on an uninitialized texture (a dim of 0 maps to 1D)
			src->add("int(");
			src->add("float(");
			_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 0, (texOpcode == GPU7_TEX_INST_LD) ? LATTE_DECOMPILER_DTYPE_SIGNED_INT : LATTE_DECOMPILER_DTYPE_FLOAT);
			src->addFmt(")*uf_tex{}Scale.x", texInstruction->textureFetch.textureIndex);
			src->add("), 0");
			// todo - lod
		}
		else
			cemu_assert_debug(false);
	}
	else /* useTexelCoordinates == false */
	{
		// float coordinates
		if ( (texOpcode == GPU7_TEX_INST_SAMPLE_C || texOpcode == GPU7_TEX_INST_SAMPLE_C_L || texOpcode == GPU7_TEX_INST_SAMPLE_C_LZ) )
		{
			// shadow sampler
			if (texDim == Latte::E_DIM::DIM_2D_ARRAY)
			{
				// 3 coords + compare value (as vec4)
				src->add("vec4(");
				_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
				src->add(",");
				_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 1, LATTE_DECOMPILER_DTYPE_FLOAT);
				src->add(",");
				_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 2, LATTE_DECOMPILER_DTYPE_FLOAT);

				src->addFmt(",{})", _getTexGPRAccess(shaderContext, texInstruction->srcGpr, LATTE_DECOMPILER_DTYPE_FLOAT, texInstruction->textureFetch.srcSel[3], -1, -1, -1, tempBuffer0));
			}
			else if (texDim == Latte::E_DIM::DIM_CUBEMAP)
			{
				// 2 coords + faceId
				if (texInstruction->textureFetch.srcSel[0] >= 4 || texInstruction->textureFetch.srcSel[1] >= 4)
				{
					debugBreakpoint();
				}
				src->add("vec4(");
				src->addFmt("redcCUBEReverse({},", _getTexGPRAccess(shaderContext, texInstruction->srcGpr, LATTE_DECOMPILER_DTYPE_FLOAT, texInstruction->textureFetch.srcSel[0], texInstruction->textureFetch.srcSel[1], -1, -1, tempBuffer0));
				_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 2, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
				src->addFmt(")");
				src->addFmt(",cubeMapArrayIndex{})", texInstruction->textureFetch.textureIndex); // cubemap index
			}
			else if (texDim == Latte::E_DIM::DIM_1D)
			{
				// 1 coord + 1 unused coord (per GLSL spec) + compare value
				if (texInstruction->textureFetch.srcSel[0] >= 4)
				{
					debugBreakpoint();
				}
				src->addFmt("vec3({},0.0,{})", _getTexGPRAccess(shaderContext, texInstruction->srcGpr, LATTE_DECOMPILER_DTYPE_FLOAT, texInstruction->textureFetch.srcSel[0], -1, -1, -1, tempBuffer0), _getTexGPRAccess(shaderContext, texInstruction->srcGpr, LATTE_DECOMPILER_DTYPE_FLOAT, texInstruction->textureFetch.srcSel[3], -1, -1, -1, tempBuffer1));
			}
			else
			{
				// 2 coords + compare value (as vec3)
				if (texInstruction->textureFetch.srcSel[0] >= 4 && texInstruction->textureFetch.srcSel[1] >= 4)
				{
					debugBreakpoint();
				}
				src->addFmt("vec3({}, {})", _getTexGPRAccess(shaderContext, texInstruction->srcGpr, LATTE_DECOMPILER_DTYPE_FLOAT, texInstruction->textureFetch.srcSel[0], texInstruction->textureFetch.srcSel[1], -1, -1, tempBuffer0), _getTexGPRAccess(shaderContext, texInstruction->srcGpr, LATTE_DECOMPILER_DTYPE_FLOAT, texInstruction->textureFetch.srcSel[3], -1, -1, -1, tempBuffer1));
			}
		}
		else if( texDim == Latte::E_DIM::DIM_3D || texDim == Latte::E_DIM::DIM_2D_ARRAY )
		{
			// 3 coords
			src->add("vec3(");
			_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
			src->add(",");
			_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 1, LATTE_DECOMPILER_DTYPE_FLOAT);
			src->add(",");
			_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 2, LATTE_DECOMPILER_DTYPE_FLOAT);
			src->add(")");
		}
		else if( texDim == Latte::E_DIM::DIM_CUBEMAP )
		{
			// 2 coords + faceId
			cemu_assert_debug(texInstruction->textureFetch.srcSel[0] < 4);
			cemu_assert_debug(texInstruction->textureFetch.srcSel[1] < 4);
			src->add("vec4(");
			src->addFmt("redcCUBEReverse({},", _getTexGPRAccess(shaderContext, texInstruction->srcGpr, LATTE_DECOMPILER_DTYPE_FLOAT, texInstruction->textureFetch.srcSel[0], texInstruction->textureFetch.srcSel[1], -1, -1, tempBuffer0));
			_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 2, LATTE_DECOMPILER_DTYPE_SIGNED_INT);
			src->add(")");
			src->addFmt(",cubeMapArrayIndex{})", texInstruction->textureFetch.textureIndex); // cubemap index
		}
		else if( texDim == Latte::E_DIM::DIM_1D )
		{
			// 1 coord
			src->add(_getTexGPRAccess(shaderContext, texInstruction->srcGpr, LATTE_DECOMPILER_DTYPE_FLOAT, texInstruction->textureFetch.srcSel[0], -1, -1, -1, tempBuffer0));
		}
		else
		{
			// 2 coords
			src->add("vec2(");
			_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 0, LATTE_DECOMPILER_DTYPE_FLOAT);
			src->add(",");
			_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 1, LATTE_DECOMPILER_DTYPE_FLOAT);
			src->add(")");
			// avoid truncate to effectively round downwards on texel edges
			if (ActiveSettings::ForceSamplerRoundToPrecision())
				src->addFmt("+ vec2(1.0)/vec2(textureSize({}{}, 0))/512.0", _getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex);
		}
		// lod or lod bias parameter
		if( texOpcode == GPU7_TEX_INST_SAMPLE_L || texOpcode == GPU7_TEX_INST_SAMPLE_LB || texOpcode == GPU7_TEX_INST_SAMPLE_C_L)
		{
			src->add(",");
			if(texOpcode == GPU7_TEX_INST_SAMPLE_LB)
				src->add(_FormatFloatAsGLSLConstant((float)texInstruction->textureFetch.lodBias / 16.0f));
			else
				_emitTEXSampleCoordInputComponent(shaderContext, texInstruction, 3, LATTE_DECOMPILER_DTYPE_FLOAT);
		}
		else if( texOpcode == GPU7_TEX_INST_SAMPLE_LZ || texOpcode == GPU7_TEX_INST_SAMPLE_C_LZ )
		{
			src->add(",0.0");
		}
	}
	// gradient parameters
	if (texOpcode == GPU7_TEX_INST_SAMPLE_G)
	{
		if (texDim == Latte::E_DIM::DIM_2D ||
			texDim == Latte::E_DIM::DIM_1D )
		{
			src->add(",gradH.xy,gradV.xy");
		}
		else	
		{
			cemu_assert_unimplemented();
		}
	}
	// offset
	if( texOpcode == GPU7_TEX_INST_SAMPLE_L || texOpcode == GPU7_TEX_INST_SAMPLE_LZ || texOpcode == GPU7_TEX_INST_SAMPLE_C_LZ || texOpcode == GPU7_TEX_INST_SAMPLE || texOpcode == GPU7_TEX_INST_SAMPLE_C )
	{
		if( hasOffset )
		{
			uint8 offsetComponentCount = 0;
			if( texDim == Latte::E_DIM::DIM_1D )
				offsetComponentCount = 1;
			else if( texDim == Latte::E_DIM::DIM_2D )
				offsetComponentCount = 2;
			else if( texDim == Latte::E_DIM::DIM_3D )
				offsetComponentCount = 3;
			else if( texDim == Latte::E_DIM::DIM_2D_ARRAY )
				offsetComponentCount = 2;
			else
				cemu_assert_unimplemented();

			if( (texInstruction->textureFetch.offsetX&1) )
				cemu_assert_unimplemented();
			if( (texInstruction->textureFetch.offsetY&1) )
				cemu_assert_unimplemented();
			if ((texInstruction->textureFetch.offsetZ & 1))
				cemu_assert_unimplemented();

			if( offsetComponentCount == 1 )
				src->addFmt(",{}", texInstruction->textureFetch.offsetX/2);
			else if( offsetComponentCount == 2 )
				src->addFmt(",ivec2({},{})", texInstruction->textureFetch.offsetX/2, texInstruction->textureFetch.offsetY/2, texInstruction->textureFetch.offsetZ/2);
			else if( offsetComponentCount == 3 )
				src->addFmt(",ivec3({},{},{})", texInstruction->textureFetch.offsetX/2, texInstruction->textureFetch.offsetY/2, texInstruction->textureFetch.offsetZ/2);
		}
	}
	// lod bias
	if( texOpcode == GPU7_TEX_INST_SAMPLE_C || texOpcode == GPU7_TEX_INST_SAMPLE_C_LZ )
	{
		src->add(")");

		if (numWrittenElements > 1)
		{
			// result is copied into multiple channels
			src->add(".");
			for (sint32 f = 0; f < numWrittenElements; f++)
			{
				cemu_assert_debug(texInstruction->dstSel[f] == 0); // only x component is defined
				src->add("x");
			}
		}
	}
	else
	{
		src->add(").");
		for (sint32 f = 0; f < 4; f++)
		{
			if( texInstruction->dstSel[f] < 4 )
			{
				uint8 elemIndex = texInstruction->dstSel[f];
				if (texOpcode == GPU7_TEX_INST_FETCH4)
				{
					// GLSL's textureGather() and GPU7's FETCH4 instruction have a different order of elements
					// xyzw: top-left, top-right, bottom-right, bottom-left
					// textureGather	xyzw
					// fetch4			yzxw
					// translate index from fetch4 to textureGather order
					static uint8 fetchToGather[4] =
					{
						2, // x -> z
						0, // y -> x
						1, // z -> y
						3, // w -> w
					};
					elemIndex = fetchToGather[elemIndex];
				}
				src->add(resultElemTable[elemIndex]);
				numWrittenElements++;
			}
			else if( texInstruction->dstSel[f] == 7 )
			{
				// masked and not written
			}
			else
			{
				cemu_assert_unimplemented();
			}
		}
	}
	src->add(");");

	// debug
#ifdef CEMU_DEBUG_ASSERT
	if(texInstruction->opcode == GPU7_TEX_INST_LD )
		src->add(" // TEX_INST_LD");
	else if(texInstruction->opcode == GPU7_TEX_INST_SAMPLE )
		src->add(" // TEX_INST_SAMPLE");
	else if(texInstruction->opcode == GPU7_TEX_INST_SAMPLE_L )
		src->add(" // TEX_INST_SAMPLE_L");
	else if(texInstruction->opcode == GPU7_TEX_INST_SAMPLE_LZ )
		src->add(" // TEX_INST_SAMPLE_LZ");
	else if(texInstruction->opcode == GPU7_TEX_INST_SAMPLE_C )
		src->add(" // TEX_INST_SAMPLE_C");
	else if(texInstruction->opcode == GPU7_TEX_INST_SAMPLE_G )
		src->add(" // TEX_INST_SAMPLE_G");
	else
		src->addFmt(" // 0x{:02x}", texInstruction->opcode);
	if (texInstruction->opcode != texOpcode)
		src->addFmt(" (applied as 0x{:02x})", texOpcode);
	src->addFmt(" OffsetXYZ {:02x} {:02x} {:02x}", (uint8)texInstruction->textureFetch.offsetX&0xFF, (uint8)texInstruction->textureFetch.offsetY&0xFF, (uint8)texInstruction->textureFetch.offsetZ&0xFF);
#endif
	src->add("" _CRLF);
}

void _emitTEXGetTextureResInfoCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerTEXInstruction* texInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	src->addFmt("R{}", texInstruction->dstGpr);
	src->add("i");
	src->add(".");

	const char* resultElemTable[4] = {"x","y","z","w"};
	sint32 numWrittenElements = 0;
	for(sint32 f=0; f<4; f++)
	{
		if( texInstruction->dstSel[f] < 4 )
		{
			src->add(resultElemTable[f]);
			numWrittenElements++;
		}
		else if( texInstruction->dstSel[f] == 7 )
		{
			// masked and not written
		}
		else
		{
			cemu_assert_unimplemented();
		}
	}

	// todo - mip index parameter?

	auto texDim = shaderContext->shader->textureUnitDim[texInstruction->textureFetch.textureIndex];

	if (texDim == Latte::E_DIM::DIM_1D)
		src->addFmt(" = ivec4(textureSize({}{}, 0),1,1,1).", _getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex);
	else if (texDim == Latte::E_DIM::DIM_1D_ARRAY)
		src->addFmt(" = ivec4(textureSize({}{}, 0),1,1).", _getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex);
	else if (texDim == Latte::E_DIM::DIM_2D || texDim == Latte::E_DIM::DIM_2D_MSAA)
		src->addFmt(" = ivec4(textureSize({}{}, 0),1,1).", _getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex);
	else if (texDim == Latte::E_DIM::DIM_2D_ARRAY)
		src->addFmt(" = ivec4(textureSize({}{}, 0),1).", _getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex);
	else
	{
		cemu_assert_debug(false);
		src->addFmt(" = ivec4(textureSize({}{}, 0),1,1).", _getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex);
	}

	for(sint32 f=0; f<4; f++)
	{
		if( texInstruction->dstSel[f] < 4 )
		{
			src->add(resultElemTable[texInstruction->dstSel[f]]);
			numWrittenElements++;
		}
		else if( texInstruction->dstSel[f] == 7 )
		{
			// masked and not written
		}
		else
		{
			debugBreakpoint();
		}
	}
	src->add(";" _CRLF);
}

void _emitTEXGetCompTexLodCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerTEXInstruction* texInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	src->add(_getRegisterVarName(shaderContext, texInstruction->dstGpr));
	src->add(".");

	const char* resultElemTable[4] = {"x","y","z","w"};
	sint32 numWrittenElements = 0;
	for(sint32 f=0; f<4; f++)
	{
		if( texInstruction->dstSel[f] < 4 )
		{
			src->add(resultElemTable[f]);
			numWrittenElements++;
		}
		else if( texInstruction->dstSel[f] == 7 )
		{
			// masked and not written
		}
		else
		{
			debugBreakpoint();
		}
	}

	src->add(" = ");
	_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, shaderContext->typeTracker.defaultDataType);

	if( shaderContext->shader->textureUnitDim[texInstruction->textureFetch.textureIndex] == Latte::E_DIM::DIM_CUBEMAP )
	{
		// 3 coordinates
		if(shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
			src->addFmt("vec4(textureQueryLod({}{}, {}.{}{}{}),0.0,0.0)", _getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex, _getRegisterVarName(shaderContext, texInstruction->srcGpr), resultElemTable[texInstruction->textureFetch.srcSel[0]], resultElemTable[texInstruction->textureFetch.srcSel[1]], resultElemTable[texInstruction->textureFetch.srcSel[2]]);
		else
			src->addFmt("vec4(textureQueryLod({}{}, intBitsToFloat({}.{}{}{})),0.0,0.0)", _getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex, _getRegisterVarName(shaderContext, texInstruction->srcGpr), resultElemTable[texInstruction->textureFetch.srcSel[0]], resultElemTable[texInstruction->textureFetch.srcSel[1]], resultElemTable[texInstruction->textureFetch.srcSel[2]]);
	}
	else
	{
		if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
			src->addFmt("vec4(textureQueryLod({}{}, {}.{}{}),0.0,0.0)", _getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex, _getRegisterVarName(shaderContext, texInstruction->srcGpr), resultElemTable[texInstruction->textureFetch.srcSel[0]], resultElemTable[texInstruction->textureFetch.srcSel[1]]);
		else
			src->addFmt("vec4(textureQueryLod({}{}, intBitsToFloat({}.{}{})),0.0,0.0)", _getTextureUnitVariablePrefixName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex, _getRegisterVarName(shaderContext, texInstruction->srcGpr), resultElemTable[texInstruction->textureFetch.srcSel[0]], resultElemTable[texInstruction->textureFetch.srcSel[1]]);
		debugBreakpoint();
	}


	_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, shaderContext->typeTracker.defaultDataType);
	src->add(".");

	for(sint32 f=0; f<4; f++)
	{
		if( texInstruction->dstSel[f] < 4 )
		{
			src->add(resultElemTable[texInstruction->dstSel[f]]);
			numWrittenElements++;
		}
		else if( texInstruction->dstSel[f] == 7 )
		{
			// masked and not written
		}
		else
		{
			debugBreakpoint();
		}
	}
	src->add(";" _CRLF);
}

void _emitTEXSetCubemapIndexCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerTEXInstruction* texInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	src->addFmt("cubeMapArrayIndex{}", texInstruction->textureFetch.textureIndex);
	const char* resultElemTable[4] = {"x","y","z","w"};

	if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
		src->addFmt(" = intBitsToFloat(R{}i.{});" _CRLF, texInstruction->srcGpr, resultElemTable[texInstruction->textureFetch.srcSel[0]]);
	else if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
		src->addFmt(" = R{}f.{};" _CRLF, texInstruction->srcGpr, resultElemTable[texInstruction->textureFetch.srcSel[0]]);
	else
		cemu_assert_unimplemented();
}

void _emitTEXGetGradientsHV(LatteDecompilerShaderContext* shaderContext, LatteDecompilerTEXInstruction* texInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	sint32 componentCount = 0;
	for (sint32 i = 0; i < 4; i++)
	{
		if(texInstruction->dstSel[i] == 7)
			continue;
		componentCount++;
	}
	src->add(_getRegisterVarName(shaderContext, texInstruction->dstGpr));
	src->add(".");
	const char* resultElemTable[4] = { "x","y","z","w" };
	sint32 numWrittenElements = 0;
	for (sint32 f = 0; f < 4; f++)
	{
		if (texInstruction->dstSel[f] < 4)
		{
			src->add(resultElemTable[f]);
			numWrittenElements++;
		}
		else if (texInstruction->dstSel[f] == 7)
		{
			// masked and not written
		}
		else
		{
			debugBreakpoint();
		}
	}

	const char* funcName;
	if (texInstruction->opcode == GPU7_TEX_INST_GET_GRADIENTS_H)
		funcName = "dFdx";
	else
		funcName = "dFdy";

	src->add(" = ");

	_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, shaderContext->typeTracker.defaultDataType);

	src->addFmt("{}(", funcName);
	_emitRegisterAccessCode(shaderContext, texInstruction->srcGpr, (componentCount >= 1) ? texInstruction->textureFetch.srcSel[0] : -1, (componentCount >= 2) ? texInstruction->textureFetch.srcSel[1] : -1, (componentCount >= 3) ? texInstruction->textureFetch.srcSel[2] : -1, (componentCount >= 4)?texInstruction->textureFetch.srcSel[3]:-1, LATTE_DECOMPILER_DTYPE_FLOAT);

	src->add(")");

	_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_FLOAT, shaderContext->typeTracker.defaultDataType);

	src->add(";" _CRLF);

}

void _emitTEXSetGradientsHV(LatteDecompilerShaderContext* shaderContext, LatteDecompilerTEXInstruction* texInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	if (texInstruction->opcode == GPU7_TEX_INST_SET_GRADIENTS_H)
		src->add("gradH = ");
	else
		src->add("gradV = ");

	_emitRegisterAccessCode(shaderContext, texInstruction->srcGpr, texInstruction->textureFetch.srcSel[0], texInstruction->textureFetch.srcSel[1], texInstruction->textureFetch.srcSel[2], texInstruction->textureFetch.srcSel[3], LATTE_DECOMPILER_DTYPE_FLOAT);

	src->add(";" _CRLF);
}

void _emitGSReadInputVFetchCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerTEXInstruction* texInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	src->add(_getRegisterVarName(shaderContext, texInstruction->dstGpr));

	src->add(".");
	
	const char* resultElemTable[4] = {"x","y","z","w"};
	sint32 numWrittenElements = 0;
	for(sint32 f=0; f<4; f++)
	{
		if( texInstruction->dstSel[f] < 4 )
		{
			src->add(resultElemTable[f]);
			numWrittenElements++;
		}
		else if( texInstruction->dstSel[f] == 7 )
		{
			// masked and not written
		}
		else
		{
			cemu_assert_unimplemented();
		}
	}

	src->add(" = ");
	_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, shaderContext->typeTracker.defaultDataType);
	src->add("(v2g[");
	if (texInstruction->textureFetch.srcSel[0] >= 4)
		cemu_assert_unimplemented();
	if (texInstruction->textureFetch.srcSel[1] >= 4)
		cemu_assert_unimplemented();
	// todo: Index type
	src->add("0");
	src->addFmt("].passV2GParameter{}.", texInstruction->textureFetch.offset/16);


	for(sint32 f=0; f<4; f++)
	{
		if( texInstruction->dstSel[f] < 4 )
		{
			src->add(resultElemTable[texInstruction->dstSel[f]]);
			numWrittenElements++;
		}
		else if( texInstruction->dstSel[f] == 7 )
		{
			// masked and not written
		}
		else
		{
			cemu_assert_unimplemented();
		}
	}
	src->add(")");
	_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, shaderContext->typeTracker.defaultDataType);
	src->add(";" _CRLF);
}

sint32 _writeDestMaskXYZW(LatteDecompilerShaderContext* shaderContext, sint8* dstSel)
{
	StringBuf* src = shaderContext->shaderSource;
	const char* resultElemTable[4] = { "x","y","z","w" };
	sint32 numWrittenElements = 0;
	for (sint32 f = 0; f < 4; f++)
	{
		if (dstSel[f] < 4)
		{
			src->add(resultElemTable[f]);
			numWrittenElements++;
		}
		else if (dstSel[f] == 7)
		{
			// masked and not written
		}
		else
		{
			cemu_assert_unimplemented();
		}
	}
	return numWrittenElements;
}

void _emitTEXVFetchCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerTEXInstruction* texInstruction)
{
	// handle special case where geometry shader reads input attributes from vertex shader via ringbuffer
	StringBuf* src = shaderContext->shaderSource;
	if( texInstruction->textureFetch.textureIndex == 0x9F && shaderContext->shaderType == LatteConst::ShaderType::Geometry )
	{
		_emitGSReadInputVFetchCode(shaderContext, texInstruction);
		return;
	}

	src->add(_getRegisterVarName(shaderContext, texInstruction->dstGpr));
	src->add(".");

	_writeDestMaskXYZW(shaderContext, texInstruction->dstSel);
	const char* resultElemTable[4] = {"x","y","z","w"};

	src->add(" = ");

	if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
		src->add("floatBitsToInt(");
	else
		src->add("(");

	src->addFmt("{}{}[", _getShaderUniformBlockVariableName(shaderContext->shader->shaderType), texInstruction->textureFetch.textureIndex - 0x80);

	if( shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
		src->addFmt("{}.{}", _getRegisterVarName(shaderContext, texInstruction->srcGpr), resultElemTable[texInstruction->textureFetch.srcSel[0]]);
	else
		src->addFmt("floatBitsToInt({}.{})", _getRegisterVarName(shaderContext, texInstruction->srcGpr), resultElemTable[texInstruction->textureFetch.srcSel[0]]);
	src->add("].");


	for(sint32 f=0; f<4; f++)
	{
		if( texInstruction->dstSel[f] < 4 )
		{
			src->add(resultElemTable[texInstruction->dstSel[f]]);
		}
		else if( texInstruction->dstSel[f] == 7 )
		{
			// masked and not written
		}
		else
		{
			debugBreakpoint();
		}
	}
	src->add(");" _CRLF);
}

void _emitTEXReadMemCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerTEXInstruction* texInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	src->add(_getRegisterVarName(shaderContext, texInstruction->dstGpr));
	src->add(".");
	sint32 count = _writeDestMaskXYZW(shaderContext, texInstruction->dstSel);

	src->add(" = ");

	if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
		src->add("floatBitsToInt(");
	else
		src->add("(");

	sint32 readCount;

	if (texInstruction->memRead.format == FMT_32_FLOAT)
	{
		readCount = 1;
		// todo
		src->add("0.0");
	}
	else if (texInstruction->memRead.format == FMT_32_32_FLOAT)
	{
		readCount = 2;
		// todo
		src->add("vec2(0.0,0.0)");
	}
	else if (texInstruction->memRead.format == FMT_32_32_32_FLOAT)
	{
		readCount = 3;
		// todo
		src->add("vec3(0.0,0.0,0.0)");
	}
	else
	{
		cemu_assert_unimplemented();
	}
	if (count < readCount)
	{
		if (count == 1)
			src->add(".x");
		else if (count == 2)
			src->add(".xy");
		else if (count == 3)
			src->add(".xyz");
	}
	src->add(");" _CRLF);
}

void _emitTEXClauseCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction)
{
	cemu_assert_debug(cfInstruction->instructionsALU.empty());
	for(auto& texInstruction : cfInstruction->instructionsTEX)
	{
		if( texInstruction.opcode == GPU7_TEX_INST_SAMPLE || texInstruction.opcode == GPU7_TEX_INST_SAMPLE_L || texInstruction.opcode == GPU7_TEX_INST_SAMPLE_LB || texInstruction.opcode == GPU7_TEX_INST_SAMPLE_LZ || texInstruction.opcode == GPU7_TEX_INST_SAMPLE_C || texInstruction.opcode == GPU7_TEX_INST_SAMPLE_C_L || texInstruction.opcode == GPU7_TEX_INST_SAMPLE_C_LZ || texInstruction.opcode == GPU7_TEX_INST_FETCH4 || texInstruction.opcode == GPU7_TEX_INST_SAMPLE_G || texInstruction.opcode == GPU7_TEX_INST_LD )
			_emitTEXSampleTextureCode(shaderContext, &texInstruction);
		else if( texInstruction.opcode == GPU7_TEX_INST_GET_TEXTURE_RESINFO )
			_emitTEXGetTextureResInfoCode(shaderContext, &texInstruction);
		else if( texInstruction.opcode == GPU7_TEX_INST_GET_COMP_TEX_LOD )
			_emitTEXGetCompTexLodCode(shaderContext, &texInstruction);
		else if( texInstruction.opcode == GPU7_TEX_INST_SET_CUBEMAP_INDEX )
			_emitTEXSetCubemapIndexCode(shaderContext, &texInstruction);
		else if (texInstruction.opcode == GPU7_TEX_INST_GET_GRADIENTS_H ||
			texInstruction.opcode == GPU7_TEX_INST_GET_GRADIENTS_V)
			_emitTEXGetGradientsHV(shaderContext, &texInstruction);
		else if (texInstruction.opcode == GPU7_TEX_INST_SET_GRADIENTS_H ||
			texInstruction.opcode == GPU7_TEX_INST_SET_GRADIENTS_V)
			_emitTEXSetGradientsHV(shaderContext, &texInstruction);
		else if (texInstruction.opcode == GPU7_TEX_INST_VFETCH)
			_emitTEXVFetchCode(shaderContext, &texInstruction);
		else if (texInstruction.opcode == GPU7_TEX_INST_MEM)
			_emitTEXReadMemCode(shaderContext, &texInstruction);
		else
			cemu_assert_unimplemented();
	}
}

// generate the code for reading the source input GPR (or constants) for exports
void _emitExportGPRReadCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction, sint32 requiredType, uint32 burstIndex)
{
	StringBuf* src = shaderContext->shaderSource;
	uint32 numOutputs = 4;
	if( cfInstruction->type == GPU7_CF_INST_MEM_RING_WRITE )
	{
		numOutputs = (cfInstruction->memWriteCompMask&1)?1:0;
		numOutputs += (cfInstruction->memWriteCompMask&2)?1:0;
		numOutputs += (cfInstruction->memWriteCompMask&4)?1:0;
		numOutputs += (cfInstruction->memWriteCompMask&8)?1:0;
	}
	if (requiredType == LATTE_DECOMPILER_DTYPE_FLOAT)
	{
		if(numOutputs == 1)
			src->add("float(");
		else
			src->addFmt("vec{}(", numOutputs);
	}
	else if (requiredType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
	{
		if (numOutputs == 1)
			src->add("int(");
		else
			src->addFmt("ivec{}(", numOutputs);
	}
	else
		cemu_assert_unimplemented();
	sint32 actualOutputs = 0;
	for(sint32 i=0; i<4; i++)
	{
		// todo: Use type of register element based on information from type tracker (currently we assume it's always a signed integer)
		uint32 exportSel = 0;
		if( cfInstruction->type == GPU7_CF_INST_MEM_RING_WRITE )
		{
			exportSel = i;
			if( (cfInstruction->memWriteCompMask&(1<<i)) == 0 )
				continue; // dont output
		}
		else
		{
			exportSel = cfInstruction->exportComponentSel[i];
		}
		if( actualOutputs > 0 )
			src->add(", ");
		actualOutputs++;
		if( exportSel < 4 )
		{
			_emitRegisterAccessCode(shaderContext, cfInstruction->exportSourceGPR+burstIndex, exportSel, -1, -1, -1, requiredType);
		}
		else if (exportSel == 4)
		{
			// constant zero
			src->add("0");
		}
		else if (exportSel == 5)
		{
			// constant one
			src->add("1.0");
		}
		else if( exportSel == 7 )
		{
			// element masked (which means 0 is exported?)
			src->add("0");
		}
		else
		{
			cemu_assert_debug(false);
			src->add("0");
		}
	}
	if( requiredType == LATTE_DECOMPILER_DTYPE_FLOAT )
		src->add(")");
	else if( requiredType == LATTE_DECOMPILER_DTYPE_SIGNED_INT )
		src->add(")");
	else
		cemu_assert_unimplemented();
}

void _emitExportCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	src->add("// export" _CRLF);
	if(shaderContext->shaderType == LatteConst::ShaderType::Vertex )
	{
		if( cfInstruction->exportBurstCount != 0 )
			debugBreakpoint();
		if (cfInstruction->exportType == 1 && cfInstruction->exportArrayBase == GPU7_DECOMPILER_CF_EXPORT_BASE_POSITION)
		{
			// export position
			// GX2 special state 0 disables rasterizer viewport offset and scaling (probably, exact mechanism is not known). Handle this here
			bool hasAnyViewportScaleDisabled =
				!shaderContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_X_SCALE_ENA() ||
				!shaderContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_Y_SCALE_ENA() ||
				!shaderContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_Z_SCALE_ENA();

			if (hasAnyViewportScaleDisabled)
			{
				src->add("vec4 finalPos = ");
				_emitExportGPRReadCode(shaderContext, cfInstruction, LATTE_DECOMPILER_DTYPE_FLOAT, 0);
				src->add(";" _CRLF);
				src->add("finalPos.xy = finalPos.xy * uf_windowSpaceToClipSpaceTransform - vec2(1.0,1.0);");
				src->add("SET_POSITION(finalPos);");
			}
			else
			{
				src->add("SET_POSITION(");
				_emitExportGPRReadCode(shaderContext, cfInstruction, LATTE_DECOMPILER_DTYPE_FLOAT, 0);
				src->add(");" _CRLF);
			}
		}
		else if (cfInstruction->exportType == 1 && cfInstruction->exportArrayBase == GPU7_DECOMPILER_CF_EXPORT_POINT_SIZE )
		{
			// export gl_PointSize
			if (shaderContext->analyzer.outputPointSize)
			{
				cemu_assert_debug(shaderContext->analyzer.writesPointSize);
				src->add("gl_PointSize = (");
				_emitExportGPRReadCode(shaderContext, cfInstruction, LATTE_DECOMPILER_DTYPE_FLOAT, 0);
				src->add(").x");
				src->add(";" _CRLF);
			}
		}
		else if( cfInstruction->exportType == 2 && cfInstruction->exportArrayBase < 32 )
		{
			// export parameter
			sint32 paramIndex = cfInstruction->exportArrayBase;
			uint32 vsSemanticId = _getVertexShaderOutParamSemanticId(shaderContext->contextRegisters, paramIndex);
			if (vsSemanticId != 0xFF)
			{
				src->addFmt("passParameterSem{} = ", vsSemanticId);
				_emitExportGPRReadCode(shaderContext, cfInstruction, LATTE_DECOMPILER_DTYPE_FLOAT, 0);
				src->add(";" _CRLF);
			}
			else
			{
				src->add("// skipped export to semanticId 255" _CRLF);
			}
		}
		else
			cemu_assert_unimplemented();
	}
	else if(shaderContext->shaderType == LatteConst::ShaderType::Pixel )
	{
		if( cfInstruction->exportType == 0 && cfInstruction->exportArrayBase < 8 )
		{
			for(uint32 i=0; i<(cfInstruction->exportBurstCount+1); i++)
			{
				sint32 pixelColorOutputIndex = LatteDecompiler_getColorOutputIndexFromExportIndex(shaderContext, cfInstruction->exportArrayBase+i);
				// if color output is for target 0, then also handle alpha test
				bool alphaTestEnable = shaderContext->contextRegistersNew->SX_ALPHA_TEST_CONTROL.get_ALPHA_TEST_ENABLE();
				auto alphaTestFunc = shaderContext->contextRegistersNew->SX_ALPHA_TEST_CONTROL.get_ALPHA_FUNC();
				if( pixelColorOutputIndex == 0 && alphaTestEnable && alphaTestFunc == Latte::E_COMPAREFUNC::NEVER )
				{
					// never pass alpha test
					src->add("discard;" _CRLF);
				}
				else if( pixelColorOutputIndex == 0 && alphaTestEnable && alphaTestFunc != Latte::E_COMPAREFUNC::ALWAYS)
				{
					src->add("if( ((");
					_emitExportGPRReadCode(shaderContext, cfInstruction, LATTE_DECOMPILER_DTYPE_FLOAT, i);
					src->add(").a ");

					switch( alphaTestFunc )
					{
					case Latte::E_COMPAREFUNC::LESS:
						src->add("<");
						break;
					case Latte::E_COMPAREFUNC::EQUAL:
						src->add("==");
						break;
					case Latte::E_COMPAREFUNC::LEQUAL:
						src->add("<=");
						break;
					case Latte::E_COMPAREFUNC::GREATER:
						src->add(">");
						break;
					case Latte::E_COMPAREFUNC::NOTEQUAL:
						src->add("!=");
						break;
					case Latte::E_COMPAREFUNC::GEQUAL:
						src->add(">=");
						break;
					}
					src->add(" uf_alphaTestRef");
					src->add(") == false) discard;" _CRLF);
				}
				// pixel color output
				src->addFmt("passPixelColor{} = ", pixelColorOutputIndex);
				_emitExportGPRReadCode(shaderContext, cfInstruction, LATTE_DECOMPILER_DTYPE_FLOAT, i);
				src->add(";" _CRLF);

				if( cfInstruction->exportArrayBase+i >= 8 )
					cemu_assert_unimplemented();
			}
		}
		else if( cfInstruction->exportType == 0 && cfInstruction->exportArrayBase == 61 )
		{
			// pixel depth or gl_FragStencilRefARB
			if( cfInstruction->exportBurstCount > 0 )
				cemu_assert_unimplemented();

			if (cfInstruction->exportComponentSel[0] == 7)
			{
				cemu_assert_unimplemented(); // gl_FragDepth ?
			}
			if (cfInstruction->exportComponentSel[1] != 7)
			{
				cemu_assert_unimplemented(); // exporting to gl_FragStencilRefARB
			}
			if (cfInstruction->exportComponentSel[2] != 7)
			{
				cemu_assert_unimplemented(); // ukn
			}
			if (cfInstruction->exportComponentSel[3] != 7)
			{
				cemu_assert_unimplemented(); // ukn
			}

			src->add("gl_FragDepth = ");
			_emitExportGPRReadCode(shaderContext, cfInstruction, LATTE_DECOMPILER_DTYPE_FLOAT, 0);
			src->add(".x");
			src->add(";" _CRLF);
		}
		else
			cemu_assert_unimplemented();
	}
}

void _emitXYZWByMask(StringBuf* src, uint32 mask)
{
	if( (mask&(1<<0)) != 0 )
		src->add("x");
	if( (mask&(1<<1)) != 0 )
		src->add("y");
	if( (mask&(1<<2)) != 0 )
		src->add("z");
	if( (mask&(1<<3)) != 0 )
		src->add("w");
}

void _emitCFRingWriteCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	// calculate parameter output (based on ring buffer output offset relative to GS unit)
	uint32 bytesPerVertex = shaderContext->contextRegisters[mmSQ_GS_VERT_ITEMSIZE] * 4;
	bytesPerVertex = std::max(bytesPerVertex, (uint32)1); // avoid division by zero
	uint32 parameterOffset = ((cfInstruction->exportArrayBase * 4) % bytesPerVertex);
	// for geometry shaders with streamout, MEM_RING_WRITE is used to pass the data to the copy shader, which then uses STREAM*_WRITE
	if (shaderContext->shaderType == LatteConst::ShaderType::Geometry && shaderContext->analyzer.hasStreamoutEnable)
	{
		// if streamout is enabled, we generate transform feedback output code instead of the normal gs output
		for (uint32 burstIndex = 0; burstIndex < (cfInstruction->exportBurstCount + 1); burstIndex++)
		{
			parameterOffset = ((cfInstruction->exportArrayBase * 4 + burstIndex*0x10) % bytesPerVertex);
			// find matching stream write in copy shader
			LatteGSCopyShaderStreamWrite_t* streamWrite = nullptr;
			for (auto& it : shaderContext->parsedGSCopyShader->list_streamWrites)
			{
				if (it.offset == parameterOffset)
				{
					streamWrite = &it;
					break;
				}
			}
			if (streamWrite == nullptr)
			{
				cemu_assert_suspicious();
				return;
			}

			for (sint32 i = 0; i < 4; i++)
			{
				if ((cfInstruction->memWriteCompMask&(1 << i)) == 0)
					continue;

				if (shaderContext->options->useTFViaSSBO)
				{
					uint32 u32Offset = streamWrite->exportArrayBase + i;
					src->addFmt("sb_buffer[sbBase{} + {}]", streamWrite->bufferIndex, u32Offset);
				}
				else
				{
					src->addFmt("sb{}[{}]", streamWrite->bufferIndex, streamWrite->exportArrayBase + i);
				}

				src->add(" = ");

				_emitTypeConversionPrefix(shaderContext, shaderContext->typeTracker.defaultDataType, LATTE_DECOMPILER_DTYPE_SIGNED_INT);

				src->addFmt("{}.", _getRegisterVarName(shaderContext, cfInstruction->exportSourceGPR+burstIndex));
				if (i == 0)
					src->add("x");
				else if (i == 1)
					src->add("y");
				else if (i == 2)
					src->add("z");
				else if (i == 3)
					src->add("w");

				_emitTypeConversionSuffix(shaderContext, shaderContext->typeTracker.defaultDataType, LATTE_DECOMPILER_DTYPE_SIGNED_INT);

				src->add(";" _CRLF);
			}
		}
		return;
	}

	if (shaderContext->shaderType == LatteConst::ShaderType::Vertex)
	{
		if (cfInstruction->memWriteElemSize != 3)
			cemu_assert_unimplemented();
		if ((cfInstruction->exportArrayBase & 3) != 0)
			cemu_assert_unimplemented();
		for (sint32 burstIndex = 0; burstIndex < (sint32)(cfInstruction->exportBurstCount + 1); burstIndex++)
		{
			src->addFmt("v2g.passV2GParameter{}.", (cfInstruction->exportArrayBase) / 4 + burstIndex);
			_emitXYZWByMask(src, cfInstruction->memWriteCompMask);
			src->addFmt(" = ");
			_emitExportGPRReadCode(shaderContext, cfInstruction, LATTE_DECOMPILER_DTYPE_SIGNED_INT, burstIndex);
			src->add(";" _CRLF);
		}
	}
	else if (shaderContext->shaderType == LatteConst::ShaderType::Geometry)
	{
		cemu_assert_debug(cfInstruction->memWriteElemSize == 3);
		//if (cfInstruction->memWriteElemSize != 3)
		//	debugBreakpoint();
		cemu_assert_debug((cfInstruction->exportArrayBase & 3) == 0);

		for (uint32 burstIndex = 0; burstIndex < (cfInstruction->exportBurstCount + 1); burstIndex++)
		{
			uint32 parameterExportType = 0;
			uint32 parameterExportBase = 0;
			if (LatteGSCopyShaderParser_getExportTypeByOffset(shaderContext->parsedGSCopyShader, parameterOffset + burstIndex * (cfInstruction->memWriteElemSize+1)*4, &parameterExportType, &parameterExportBase) == false)
			{
				cemu_assert_debug(false);
				shaderContext->hasError = true;
				return;
			}

			if (parameterExportType == 1 && parameterExportBase == GPU7_DECOMPILER_CF_EXPORT_BASE_POSITION)
			{
				src->add("{" _CRLF);
				src->addFmt("vec4 pos = vec4(0.0,0.0,0.0,1.0);" _CRLF);
				src->addFmt("pos.");
				_emitXYZWByMask(src, cfInstruction->memWriteCompMask);
				src->addFmt(" = ");
				_emitExportGPRReadCode(shaderContext, cfInstruction, LATTE_DECOMPILER_DTYPE_FLOAT, burstIndex);
				src->add(";" _CRLF);
				src->add("SET_POSITION(pos);" _CRLF);
				src->add("}" _CRLF);
			}
			else if (parameterExportType == 2 && parameterExportBase < 16)
			{
				src->addFmt("passG2PParameter{}.", parameterExportBase);
				_emitXYZWByMask(src, cfInstruction->memWriteCompMask);
				src->addFmt(" = ");
				_emitExportGPRReadCode(shaderContext, cfInstruction, LATTE_DECOMPILER_DTYPE_FLOAT, burstIndex);
				src->add(";" _CRLF);
			}
			else
				cemu_assert_debug(false);
		}
	}
	else
		debugBreakpoint(); // todo
}

void _emitStreamWriteCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	if (shaderContext->analyzer.hasStreamoutEnable == false)
	{
#ifdef CEMU_DEBUG_ASSERT
		src->add("// omitted streamout write" _CRLF);
#endif
		return;
	}
	uint32 streamoutBufferIndex;
	if (cfInstruction->type == GPU7_CF_INST_MEM_STREAM0_WRITE)
		streamoutBufferIndex = 0;
	else if (cfInstruction->type == GPU7_CF_INST_MEM_STREAM1_WRITE)
		streamoutBufferIndex = 1;
	else
		cemu_assert_unimplemented();

	if (shaderContext->shaderType == LatteConst::ShaderType::Vertex)
	{
		uint32 arraySize = cfInstruction->memWriteArraySize + 1;

		for (sint32 i = 0; i < (sint32)arraySize; i++)
		{
			if ((cfInstruction->memWriteCompMask&(1 << i)) == 0)
				continue;

			if (shaderContext->options->useTFViaSSBO)
			{
				uint32 u32Offset = cfInstruction->exportArrayBase + i;
				src->addFmt("sb_buffer[sbBase{} + {}]", streamoutBufferIndex, u32Offset);
			}
			else
			{
				src->addFmt("sb{}[{}]", streamoutBufferIndex, cfInstruction->exportArrayBase + i);
			}

			src->add(" = ");

			_emitTypeConversionPrefix(shaderContext, shaderContext->typeTracker.defaultDataType, LATTE_DECOMPILER_DTYPE_SIGNED_INT);

			src->add(_getRegisterVarName(shaderContext, cfInstruction->exportSourceGPR));
			_appendChannelAccess(src, i);
			_emitTypeConversionSuffix(shaderContext, shaderContext->typeTracker.defaultDataType, LATTE_DECOMPILER_DTYPE_SIGNED_INT);

			src->add(";" _CRLF);
		}
	}
	else
		cemu_assert_debug(false);
}

void _emitCFCall(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction)
{
	StringBuf* src = shaderContext->shaderSource;
	uint32 subroutineAddr = cfInstruction->addr;
	LatteDecompilerSubroutineInfo* subroutineInfo = nullptr;
	// find subroutine
	for (auto& subroutineItr : shaderContext->list_subroutines)
	{
		if (subroutineItr.cfAddr == subroutineAddr)
		{
			subroutineInfo = &subroutineItr;
			break;
		}
	}
	if (subroutineInfo == nullptr)
	{
		cemu_assert_debug(false);
		return;
	}
	// inline function
	if (shaderContext->isSubroutine)
	{
		cemu_assert_debug(false); // inlining with cascaded function calls not supported
		return;
	}
	// init CF stack variables
	src->addFmt("activeMaskStackSub{:04x}[0] = true;" _CRLF, subroutineInfo->cfAddr);
	src->addFmt("activeMaskStackCSub{:04x}[0] = true;" _CRLF, subroutineInfo->cfAddr);
	src->addFmt("activeMaskStackCSub{:04x}[1] = true;" _CRLF, subroutineInfo->cfAddr);

	shaderContext->isSubroutine = true;
	shaderContext->subroutineInfo = subroutineInfo;
	for(auto& cfInstruction : subroutineInfo->instructions)
		LatteDecompiler_emitClauseCode(shaderContext, &cfInstruction, true);
	shaderContext->isSubroutine = false;
	shaderContext->subroutineInfo = nullptr;
}

void LatteDecompiler_emitClauseCode(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction, bool isSubroutine)
{
	StringBuf* src = shaderContext->shaderSource;

	if( cfInstruction->type == GPU7_CF_INST_ALU || cfInstruction->type == GPU7_CF_INST_ALU_PUSH_BEFORE || cfInstruction->type == GPU7_CF_INST_ALU_POP_AFTER || cfInstruction->type == GPU7_CF_INST_ALU_POP2_AFTER || cfInstruction->type == GPU7_CF_INST_ALU_BREAK || cfInstruction->type == GPU7_CF_INST_ALU_ELSE_AFTER )
	{
		// emit ALU code
		if (shaderContext->analyzer.modifiesPixelActiveState)
		{
			if(cfInstruction->type == GPU7_CF_INST_ALU_PUSH_BEFORE)
				src->addFmt("if( {} == true ) {{" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1 - 1));
			else
				src->addFmt("if( {} == true ) {{" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1));
		}
		if (cfInstruction->type == GPU7_CF_INST_ALU_PUSH_BEFORE)
		{
			src->addFmt("{} = {};" _CRLF, _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth), _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth-1));
			src->addFmt("{} = {};" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1), _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth));
		}
		_emitALUClauseCode(shaderContext, cfInstruction);
		if( shaderContext->analyzer.modifiesPixelActiveState )
			src->add("}" _CRLF);
		cemu_assert_debug(!(shaderContext->analyzer.modifiesPixelActiveState == false && cfInstruction->type != GPU7_CF_INST_ALU));
		// handle ELSE case of PUSH_BEFORE
		if( cfInstruction->type == GPU7_CF_INST_ALU_PUSH_BEFORE )
		{
			src->add("else {" _CRLF);
			src->addFmt("{} = false;" _CRLF, _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth));
			src->addFmt("{} = false;" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1));
			src->add("}" _CRLF);
		}
		// post clause handler
		if( cfInstruction->type == GPU7_CF_INST_ALU_POP_AFTER )
		{
			src->addFmt("{} = {} == true && {} == true;" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1 - 1), _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth - 1), _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth - 1));
		}
		else if( cfInstruction->type == GPU7_CF_INST_ALU_POP2_AFTER )
		{
			src->addFmt("{} = {} == true && {} == true;" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1 - 2), _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth - 2), _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth - 2));
		}
		else if( cfInstruction->type == GPU7_CF_INST_ALU_ELSE_AFTER )
		{
			// no condition test
			// pop stack
			if( cfInstruction->popCount != 0 )
				debugBreakpoint();
			// else operation
			src->addFmt("{} = {} == false;" _CRLF, _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth), _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth));
			src->addFmt("{} = {} == true && {} == true;" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1), _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth), _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth));
		}
	}
	else if( cfInstruction->type == GPU7_CF_INST_TEX )
	{
		// emit TEX code
		if (shaderContext->analyzer.modifiesPixelActiveState)
		{
			src->addFmt("if( {} == true ) {{" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth+1));
		}
		_emitTEXClauseCode(shaderContext, cfInstruction);
		if (shaderContext->analyzer.modifiesPixelActiveState)
		{
			src->add("}" _CRLF);
		}
	}
	else if( cfInstruction->type == GPU7_CF_INST_EXPORT || cfInstruction->type == GPU7_CF_INST_EXPORT_DONE )
	{
		// emit export code
		_emitExportCode(shaderContext, cfInstruction);
	}
	else if( cfInstruction->type == GPU7_CF_INST_ELSE )
	{
		// todo: Condition test, popCount?
		src->addFmt("{} = {} == false;" _CRLF, _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth), _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth));
		src->addFmt("{} = {} == true && {} == true;" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1), _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth), _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth));
	}
	else if( cfInstruction->type == GPU7_CF_INST_POP )
	{
		src->addFmt("{} = {} == true && {} == true;" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1 - cfInstruction->popCount), _getActiveMaskVarName(shaderContext, cfInstruction->activeStackDepth - cfInstruction->popCount), _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth - cfInstruction->popCount));
	}
	else if( cfInstruction->type == GPU7_CF_INST_LOOP_START_DX10 )
	{
		// start of loop
		// if pixel is disabled, then skip loop
		if (ActiveSettings::ShaderPreventInfiniteLoopsEnabled())
		{
			// with iteration limit to prevent infinite loops
			src->addFmt("int loopCounter{} = 0;" _CRLF, (sint32)cfInstruction->cfAddr);
			src->addFmt("while( {} == true && loopCounter{} < 500 )" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1), (sint32)cfInstruction->cfAddr);
			src->add("{" _CRLF);
			src->addFmt("loopCounter{}++;" _CRLF, (sint32)cfInstruction->cfAddr);
		}
		else
		{
			src->addFmt("while( {} == true )" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1));
			src->add("{" _CRLF);
		}
	}
	else if( cfInstruction->type == GPU7_CF_INST_LOOP_END )
	{
		// this might not always work
		if( cfInstruction->popCount != 0 )
			debugBreakpoint();
		src->add("}" _CRLF);
	}
	else if( cfInstruction->type == GPU7_CF_INST_LOOP_BREAK )
	{
		if( cfInstruction->popCount != 0 )
			debugBreakpoint();
		if (shaderContext->analyzer.modifiesPixelActiveState)
		{
			src->addFmt("if( {} == true ) {{" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1));
		}
		// note: active stack level is set to the same level as the loop begin. popCount is ignored
		src->add("break;" _CRLF);

		if (shaderContext->analyzer.modifiesPixelActiveState)
			src->add("}" _CRLF);

	}
	else if( cfInstruction->type == GPU7_CF_INST_MEM_STREAM0_WRITE ||
		cfInstruction->type == GPU7_CF_INST_MEM_STREAM1_WRITE )
	{
		_emitStreamWriteCode(shaderContext, cfInstruction);
	}
	else if( cfInstruction->type == GPU7_CF_INST_MEM_RING_WRITE )
	{
		_emitCFRingWriteCode(shaderContext, cfInstruction);
	}
	else if( cfInstruction->type == GPU7_CF_INST_EMIT_VERTEX )
	{
		if( shaderContext->analyzer.modifiesPixelActiveState )
			src->addFmt("if( {} == true ) {{" _CRLF, _getActiveMaskCVarName(shaderContext, cfInstruction->activeStackDepth + 1));
		// write point size
		if (shaderContext->analyzer.outputPointSize && shaderContext->analyzer.writesPointSize == false)
			src->add("gl_PointSize = uf_pointSize;" _CRLF);
		// emit vertex
		src->add("EmitVertex();" _CRLF);
		// increment transform feedback pointer
		if (shaderContext->analyzer.useSSBOForStreamout)
		{
			for (sint32 i = 0; i < LATTE_NUM_STREAMOUT_BUFFER; i++)
			{
				if (!shaderContext->output->streamoutBufferWriteMask[i])
					continue;
				cemu_assert_debug((shaderContext->output->streamoutBufferStride[i] & 3) == 0);
				src->addFmt("sbBase{} += {};" _CRLF, i, shaderContext->output->streamoutBufferStride[i] / 4);
			}
		}

		if( shaderContext->analyzer.modifiesPixelActiveState )
			src->add("}" _CRLF);
	}
	else if (cfInstruction->type == GPU7_CF_INST_CALL)
	{
		_emitCFCall(shaderContext, cfInstruction);
	}
	else if (cfInstruction->type == GPU7_CF_INST_RETURN)
	{
		// todo (handle properly)
	}
	else
	{
		cemu_assert_debug(false);
	}
}

void LatteDecompiler_emitGLSLHelperFunctions(LatteDecompilerShaderContext* shaderContext, StringBuf* fCStr_shaderSource)
{
	if( shaderContext->analyzer.hasRedcCUBE )
	{
		fCStr_shaderSource->add("void redcCUBE(vec4 src0, vec4 src1, out vec3 stm, out int faceId)\r\n"
		"{\r\n"
		"// stm -> x .. s, y .. t, z .. MajorAxis*2.0\r\n"

		"vec3 inputCoord = normalize(vec3(src1.y, src1.x, src0.x));\r\n"

		"float rx = inputCoord.x;\r\n"
		"float ry = inputCoord.y;\r\n"
		"float rz = inputCoord.z;\r\n"
		"if( abs(rx) > abs(ry) && abs(rx) > abs(rz) )\r\n"
		"{\r\n"
		"stm.z = rx*2.0;\r\n"
		"stm.xy = vec2(ry,rz);	\r\n"
		"if( rx >= 0.0 )\r\n"
		"{\r\n"
		"faceId = 0;\r\n"
		"}\r\n"
		"else\r\n"
		"{\r\n"
		"faceId = 1;\r\n"
		"}\r\n"
		"}\r\n"
		"else if( abs(ry) > abs(rx) && abs(ry) > abs(rz) )\r\n"
		"{\r\n"
		"stm.z = ry*2.0;\r\n"
		"stm.xy = vec2(rx,rz);	\r\n"
		"if( ry >= 0.0 )\r\n"
		"{\r\n"
		"faceId = 2;\r\n"
		"}\r\n"
		"else\r\n"
		"{\r\n"
		"faceId = 3;\r\n"
		"}\r\n"
		"}\r\n"
		"else //if( abs(rz) > abs(ry) && abs(rz) > abs(rx) )\r\n"
		"{\r\n"
		"stm.z = rz*2.0;\r\n"
		"stm.xy = vec2(rx,ry);	\r\n"
		"if( rz >= 0.0 )\r\n"
		"{\r\n"
		"faceId = 4;\r\n"
		"}\r\n"
		"else\r\n"
		"{\r\n"
		"faceId = 5;\r\n"
		"}\r\n"
		"}\r\n"
		"}\r\n");
	}

	if( shaderContext->analyzer.hasCubeMapTexture )
	{
		fCStr_shaderSource->add("vec3 redcCUBEReverse(vec2 st, int faceId)\r\n"
		"{\r\n"
		"st.yx = st.xy;\r\n"
		"vec3 v;\r\n"
		"float majorAxis = 1.0;\r\n"
		"if( faceId == 0 )\r\n"
		"{\r\n"
		"v.yz = (st-vec2(1.5))*(majorAxis*2.0);\r\n"
		"v.x = 1.0;\r\n"
		"}\r\n"
		"else if( faceId == 1 )\r\n"
		"{\r\n"
		"v.yz = (st-vec2(1.5))*(majorAxis*2.0);\r\n"
		"v.x = -1.0;\r\n"
		"}\r\n"
		"else if( faceId == 2 )\r\n"
		"{\r\n"
		"v.xz = (st-vec2(1.5))*(majorAxis*2.0);\r\n"
		"v.y = 1.0;\r\n"
		"}\r\n"
		"else if( faceId == 3 )\r\n"
		"{\r\n"
		"v.xz = (st-vec2(1.5))*(majorAxis*2.0);\r\n"
		"v.y = -1.0;\r\n"
		"}\r\n"
		"else if( faceId == 4 )\r\n"
		"{\r\n"
		"v.xy = (st-vec2(1.5))*(majorAxis*2.0);\r\n"
		"v.z = 1.0;\r\n"
		"}\r\n"
		"else\r\n"
		"{\r\n"
		"v.xy = (st-vec2(1.5))*(majorAxis*2.0);\r\n"
		"v.z = -1.0;\r\n"
		"}\r\n"

		"return v;\r\n"
		"}\r\n");
	}

	// clamp
	fCStr_shaderSource->add(""
	"int clampFI32(int v)\r\n"
	"{\r\n"
		"if( v == 0x7FFFFFFF )\r\n"
		"	return floatBitsToInt(1.0);\r\n"
		"else if( v == 0xFFFFFFFF )\r\n"
		"	return floatBitsToInt(0.0);\r\n"
		"return floatBitsToInt(clamp(intBitsToFloat(v), 0.0, 1.0));\r\n"
	"}\r\n");
	// mul non-ieee way (0*NaN/INF => 0.0)
	if (shaderContext->options->strictMul)
	{
		// things we tried:
		//fCStr_shaderSource->add("float mul_nonIEEE(float a, float b){ return mix(a*b,0.0,a==0.0||b==0.0); }" STR_LINEBREAK);
		//fCStr_shaderSource->add("float mul_nonIEEE(float a, float b){ return mix(vec2(a*b,0.0),vec2(0.0,0.0),(equal(vec2(a),vec2(0.0,0.0))||equal(vec2(b),vec2(0.0,0.0)))).x; }" STR_LINEBREAK);
		//fCStr_shaderSource->add("float mul_nonIEEE(float a, float b){ if( a == 0.0 || b == 0.0 ) return 0.0; return a*b; }" STR_LINEBREAK);
		//fCStr_shaderSource->add("float mul_nonIEEE(float a, float b){float r = a*b;r = intBitsToFloat(floatBitsToInt(r)&(((floatBitsToInt(a) != 0) && (floatBitsToInt(b) != 0))?0xFFFFFFFF:0));return r;}" STR_LINEBREAK); works

		// for "min" it used to be: float mul_nonIEEE(float a, float b){ return min(a*b,min(abs(a)*3.40282347E+38F,abs(b)*3.40282347E+38F)); }

		if( LatteGPUState.glVendor == GLVENDOR_NVIDIA && !ActiveSettings::DumpShadersEnabled())
			fCStr_shaderSource->add("float mul_nonIEEE(float a, float b){return mix(0.0, a*b, (a != 0.0) && (b != 0.0));}" _CRLF); // compiles faster on Nvidia and also results in lower RAM usage (OpenGL)
		else
			fCStr_shaderSource->add("float mul_nonIEEE(float a, float b){ if( a == 0.0 || b == 0.0 ) return 0.0; return a*b; }" _CRLF);

		// DXKV-like: fCStr_shaderSource->add("float mul_nonIEEE(float a, float b){ return (b==0.0 ? 0.0 : a) * (a==0.0 ? 0.0 : b); }" _CRLF);
	}
}

#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerEmitGLSLHeader.hpp"

void LatteDecompiler_emitAttributeImport(LatteDecompilerShaderContext* shaderContext, LatteParsedFetchShaderAttribute_t& attrib)
{
	auto src = shaderContext->shaderSource;

	static const char* dsMappingTableFloat[6] = { "int(attrDecoder.x)", "int(attrDecoder.y)", "int(attrDecoder.z)", "int(attrDecoder.w)", /*"floatBitsToInt(0.0)"*/ "0", /*"floatBitsToInt(1.0)"*/ "0x3f800000" };
	static const char* dsMappingTableInt[6] = { "int(attrDecoder.x)", "int(attrDecoder.y)", "int(attrDecoder.z)", "int(attrDecoder.w)", "0", "1" };

	// get register index based on vtx semantic table
	uint32 attributeShaderLoc = 0xFFFFFFFF;
	for (sint32 f = 0; f < 32; f++)
	{
		if (shaderContext->contextRegisters[mmSQ_VTX_SEMANTIC_0 + f] == attrib.semanticId)
		{
			attributeShaderLoc = f;
			break;
		}
	}
	if (attributeShaderLoc == 0xFFFFFFFF)
		return; // attribute is not mapped to VS input
	uint32 registerIndex = attributeShaderLoc + 1; // R0 is skipped
	// is register used?
	if ((shaderContext->analyzer.gprUseMask[registerIndex / 8] & (1 << (registerIndex % 8))) == 0)
	{
		src->addFmt("// skipped unused attribute for r{}" _CRLF, registerIndex);
		return;
	}

	LatteDecompiler_emitAttributeDecodeGLSL(shaderContext->shader, src, &attrib);

	if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
		src->addFmt("{} = ivec4(", _getRegisterVarName(shaderContext, registerIndex));
	else
		src->addFmt("{} = vec4(", _getRegisterVarName(shaderContext, registerIndex));
	for (sint32 f = 0; f < 4; f++)
	{
		uint8 ds = attrib.ds[f];
		if (f > 0)
			src->add(", ");
		_emitTypeConversionPrefix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, shaderContext->typeTracker.defaultDataType);
		if (ds >= 6)
		{
			cemu_assert_unimplemented();
			ds = 4; // read as 0.0
		}
		if (attrib.nfa != 1)
		{
			src->add(dsMappingTableFloat[ds]);
		}
		else
		{
			src->add(dsMappingTableInt[ds]);
		}
		_emitTypeConversionSuffix(shaderContext, LATTE_DECOMPILER_DTYPE_SIGNED_INT, shaderContext->typeTracker.defaultDataType);
	}
	src->add(");" _CRLF);
}

void LatteDecompiler_emitGLSLShader(LatteDecompilerShaderContext* shaderContext, LatteDecompilerShader* shader)
{
	StringBuf* src = new StringBuf(1024*1024*12); // reserve 12MB for generated source (we resize-to-fit at the end)
	shaderContext->shaderSource = src;
	// GLSL shader header
	src->add("#version 430" _CRLF); // 430 is required for shader storage (Vulkan alternative TF path)
	src->add("#extension GL_ARB_texture_gather : enable" _CRLF);
	src->add("#extension GL_ARB_separate_shader_objects : enable" _CRLF);

	if (shaderContext->analyzer.hasStreamoutWrite || shaderContext->options->usesGeometryShader )
		src->add("#extension GL_ARB_enhanced_layouts : enable" _CRLF);
	
	// debug info
	src->addFmt("// shader {:016x}" _CRLF, shaderContext->shaderBaseHash);
#ifdef CEMU_DEBUG_ASSERT
	src->addFmt("// usesIntegerValues: {}" _CRLF, shaderContext->analyzer.usesIntegerValues?"true":"false");
	src->addFmt(_CRLF);
#endif
	// header part (definitions for inputs and outputs)
	LatteDecompiler::emitHeader(shaderContext);
	// helper functions
	LatteDecompiler_emitGLSLHelperFunctions(shaderContext, src);
	// start of main
	src->add("void main()" _CRLF);
	src->add("{" _CRLF);
	// variable definition
	if (shaderContext->typeTracker.useArrayGPRs == false)
	{
		// each register is a separate variable
		for (sint32 i = 0; i < 128; i++)
		{
			if (shaderContext->analyzer.usesRelativeGPRRead || (shaderContext->analyzer.gprUseMask[i / 8] & (1 << (i & 7))) != 0)
			{
				if (shaderContext->typeTracker.genIntReg)
					src->addFmt("ivec4 R{}i = ivec4(0);" _CRLF, i);
				else if (shaderContext->typeTracker.genFloatReg)
					src->addFmt("vec4 R{}f = vec4(0.0);" _CRLF, i);
			}
		}
	}
	else
	{
		// registers are represented using a single large array
		if (shaderContext->typeTracker.genIntReg)
			src->addFmt("ivec4 Ri[128];" _CRLF);
		else if (shaderContext->typeTracker.genFloatReg)
			src->addFmt("vec4 Rf[128];" _CRLF);
		for (sint32 i = 0; i < 128; i++)
		{
			if (shaderContext->typeTracker.genIntReg)
				src->addFmt("Ri[{}] = ivec4(0);" _CRLF, i);
			else if (shaderContext->typeTracker.genFloatReg)
				src->addFmt("Rf[{}] = vec4(0.0);" _CRLF, i);
		}
	}

	if( shader->shaderType == LatteConst::ShaderType::Vertex )
		src->addFmt("uvec4 attrDecoder;" _CRLF);
	if (shaderContext->typeTracker.genIntReg)
		src->addFmt("int backupReg0i, backupReg1i, backupReg2i, backupReg3i, backupReg4i;" _CRLF);
	if (shaderContext->typeTracker.genFloatReg)
		src->addFmt("float backupReg0f, backupReg1f, backupReg2f, backupReg3f, backupReg4f;" _CRLF);
	if (shaderContext->typeTracker.genIntReg)
	{
		src->addFmt("int PV0ix = 0, PV0iy = 0, PV0iz = 0, PV0iw = 0, PV1ix = 0, PV1iy = 0, PV1iz = 0, PV1iw = 0;" _CRLF);
		src->addFmt("int PS0i = 0, PS1i = 0;" _CRLF);
		src->addFmt("ivec4 tempi = ivec4(0);" _CRLF);
	}
	if (shaderContext->typeTracker.genFloatReg)
	{
		src->addFmt("float PV0fx = 0.0, PV0fy = 0.0, PV0fz = 0.0, PV0fw = 0.0, PV1fx = 0.0, PV1fy = 0.0, PV1fz = 0.0, PV1fw = 0.0;" _CRLF);
		src->addFmt("float PS0f = 0.0, PS1f = 0.0;" _CRLF);
		src->addFmt("vec4 tempf = vec4(0.0);" _CRLF);
	}
	if (shaderContext->analyzer.hasGradientLookup)
	{
		src->add("vec4 gradH;" _CRLF);
		src->add("vec4 gradV;" _CRLF);
	}
	src->add("float tempResultf;" _CRLF);
	src->add("int tempResulti;" _CRLF);
	src->add("ivec4 ARi = ivec4(0);" _CRLF);
	src->add("bool predResult = true;" _CRLF);
	if(shaderContext->analyzer.modifiesPixelActiveState )
	{
		src->addFmt("bool activeMaskStack[{}];" _CRLF, shaderContext->analyzer.activeStackMaxDepth+1);
		src->addFmt("bool activeMaskStackC[{}];" _CRLF, shaderContext->analyzer.activeStackMaxDepth+2);
		for (sint32 i = 0; i < shaderContext->analyzer.activeStackMaxDepth; i++)
		{
			src->addFmt("activeMaskStack[{}] = false;" _CRLF, i);
		}
		for (sint32 i = 0; i < shaderContext->analyzer.activeStackMaxDepth+1; i++)
		{
			src->addFmt("activeMaskStackC[{}] = false;" _CRLF, i);
		}
		src->addFmt("activeMaskStack[0] = true;" _CRLF);
		src->addFmt("activeMaskStackC[0] = true;" _CRLF);
		src->addFmt("activeMaskStackC[1] = true;" _CRLF);	
		// generate vars for each subroutine
		for (auto& subroutineInfo : shaderContext->list_subroutines)
		{
			sint32 subroutineMaxStackDepth = 0;
			src->addFmt("bool activeMaskStackSub{:04x}[{}];" _CRLF, subroutineInfo.cfAddr, subroutineMaxStackDepth + 1);
			src->addFmt("bool activeMaskStackCSub{:04x}[{}];" _CRLF, subroutineInfo.cfAddr, subroutineMaxStackDepth + 2);
		}
	}
	// helper variables for cube maps (todo: Only emit when used)
	if (shaderContext->analyzer.hasRedcCUBE)
	{
		src->add("vec3 cubeMapSTM;" _CRLF);
		src->add("int cubeMapFaceId;" _CRLF);
	}
	for(sint32 i=0; i<LATTE_NUM_MAX_TEX_UNITS; i++)
	{
		if(!shaderContext->output->textureUnitMask[i])
			continue;
		if( shader->textureUnitDim[i] != Latte::E_DIM::DIM_CUBEMAP )
			continue;
		src->addFmt("float cubeMapArrayIndex{} = 0.0;" _CRLF, i);
	}
	// init base offset for streamout buffer writes
	if (shaderContext->analyzer.useSSBOForStreamout && (shader->shaderType == LatteConst::ShaderType::Vertex || shader->shaderType == LatteConst::ShaderType::Geometry))
	{
		for (sint32 i = 0; i < LATTE_NUM_STREAMOUT_BUFFER; i++)
		{
			if(!shaderContext->output->streamoutBufferWriteMask[i])
				continue;

			cemu_assert_debug((shaderContext->output->streamoutBufferStride[i]&3) == 0);

			if (shader->shaderType == LatteConst::ShaderType::Vertex) // vertex shader
				src->addFmt("int sbBase{} = uf_streamoutBufferBase{}/4 + (gl_VertexID + uf_verticesPerInstance * gl_InstanceID)*{};" _CRLF, i, i, shaderContext->output->streamoutBufferStride[i] / 4);
			else // geometry shader
			{
				uint32 gsOutPrimType = shaderContext->contextRegisters[mmVGT_GS_OUT_PRIM_TYPE];
				uint32 bytesPerVertex = shaderContext->contextRegisters[mmSQ_GS_VERT_ITEMSIZE] * 4;
				uint32 maxVerticesInGS = ((shaderContext->contextRegisters[mmSQ_GSVS_RING_ITEMSIZE] & 0x7FFF) * 4) / bytesPerVertex;
				
				cemu_assert_debug(gsOutPrimType == 0); // currently we only properly handle GS output primitive points
				
				src->addFmt("int sbBase{} = uf_streamoutBufferBase{}/4 + (gl_PrimitiveIDIn * {})*{};" _CRLF, i, i, maxVerticesInGS, shaderContext->output->streamoutBufferStride[i] / 4);
			}
		}

	}
	// code to load inputs from previous stage
	if( shader->shaderType == LatteConst::ShaderType::Vertex )
	{
		if( (shaderContext->analyzer.gprUseMask[0/8]&(1<<(0%8))) != 0 )
		{
			if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
				src->addFmt("{} = ivec4(gl_VertexID, 0, 0, gl_InstanceID);" _CRLF, _getRegisterVarName(shaderContext, 0));
			else if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
				src->addFmt("{} = floatBitsToInt(ivec4(gl_VertexID, 0, 0, gl_InstanceID));" _CRLF, _getRegisterVarName(shaderContext, 0));
			else
				cemu_assert_unimplemented();
		}

		LatteFetchShader* parsedFetchShader = shaderContext->fetchShader;
		for(auto& bufferGroup : parsedFetchShader->bufferGroups)
		{
			for(sint32 i=0; i<bufferGroup.attribCount; i++)
				LatteDecompiler_emitAttributeImport(shaderContext, bufferGroup.attrib[i]);
		}
		for (auto& bufferGroup : parsedFetchShader->bufferGroupsInvalid)
		{
			// these attributes point to non-existent buffers
			// todo - figure out how the hardware actually handles this, currently we assume the input values are zero
			for (sint32 i = 0; i < bufferGroup.attribCount; i++)
				LatteDecompiler_emitAttributeImport(shaderContext, bufferGroup.attrib[i]);
		}
	}
	else if (shader->shaderType == LatteConst::ShaderType::Pixel)
	{
		LatteShaderPSInputTable* psInputTable = LatteSHRC_GetPSInputTable();

		uint32 psControl0 = shaderContext->contextRegisters[mmSPI_PS_IN_CONTROL_0];
		uint32 psControl1 = shaderContext->contextRegisters[mmSPI_PS_IN_CONTROL_1];

		uint32 spiInterpControl = shaderContext->contextRegisters[mmSPI_INTERP_CONTROL_0];
		uint8 spriteEnable = (spiInterpControl >> 1) & 1;
		cemu_assert_debug(spriteEnable == 0);

		uint8 frontFace_enabled = (psControl1 >> 8) & 1;
		uint8 frontFace_chan = (psControl1 >> 9) & 3;
		uint8 frontFace_allBits = (psControl1 >> 11) & 1;
		uint8 frontFace_regIndex = (psControl1 >> 12) & 0x1F;

		// handle param_gen
		if (psInputTable->paramGen != 0)
		{
			cemu_assert_debug((psInputTable->paramGen) == 1); // handle the other bits (the same set of coordinates with different perspective/projection settings?)
			uint32 paramGenGPRIndex = psInputTable->paramGenGPR;
			if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
				src->addFmt("{} = gl_PointCoord.xyxy;" _CRLF, _getRegisterVarName(shaderContext, paramGenGPRIndex));
			else
				src->addFmt("{} = floatBitsToInt(gl_PointCoord.xyxy);" _CRLF, _getRegisterVarName(shaderContext, paramGenGPRIndex));
		}

		for (sint32 i = 0; i < psInputTable->count; i++)
		{
			uint32 psControl0 = shaderContext->contextRegisters[mmSPI_PS_IN_CONTROL_0];
			uint32 spi0_paramGen = (psControl0 >> 15) & 0xF;

			sint32 gprIndex = i;// +spi0_paramGen + paramRegOffset;
			if ((shaderContext->analyzer.gprUseMask[gprIndex / 8] & (1 << (gprIndex % 8))) == 0 && shaderContext->analyzer.usesRelativeGPRRead == false)
				continue;
			uint32 psInputSemanticId = psInputTable->import[i].semanticId;
			if (psInputSemanticId == LATTE_ANALYZER_IMPORT_INDEX_SPIPOSITION)
			{
				if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
					src->addFmt("{} = GET_FRAGCOORD();" _CRLF, _getRegisterVarName(shaderContext, gprIndex));
				else
					src->addFmt("{} = floatBitsToInt(GET_FRAGCOORD());" _CRLF, _getRegisterVarName(shaderContext, gprIndex));
				continue;
			}

			if (shaderContext->options->usesGeometryShader)
			{
				// import from geometry shader
				if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
					src->addFmt("{} = floatBitsToInt(passG2PParameter{});" _CRLF, _getRegisterVarName(shaderContext, gprIndex), psInputSemanticId & 0x7F);
				else if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
					src->addFmt("{} = passG2PParameter{};" _CRLF, _getRegisterVarName(shaderContext, gprIndex), psInputSemanticId & 0x7F);
				else
					cemu_assert_unimplemented();
			}
			else
			{
				// import from vertex shader
				if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
					src->addFmt("{} = floatBitsToInt(passParameterSem{});" _CRLF, _getRegisterVarName(shaderContext, gprIndex), psInputSemanticId);
				else if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
					src->addFmt("{} = passParameterSem{};" _CRLF, _getRegisterVarName(shaderContext, gprIndex), psInputSemanticId);
				else
					cemu_assert_unimplemented();
			}
		}
		// front facing attribute
		if (frontFace_enabled)
		{
			if ((shaderContext->analyzer.gprUseMask[0 / 8] & (1 << (0 % 8))) != 0)
			{
				if (frontFace_allBits)
					cemu_assert_debug(false);
				if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_SIGNED_INT)
					src->addFmt("{}.{} = floatBitsToInt(gl_FrontFacing?1.0:0.0);" _CRLF, _getRegisterVarName(shaderContext, frontFace_regIndex), _getElementStrByIndex(frontFace_chan));
				else if (shaderContext->typeTracker.defaultDataType == LATTE_DECOMPILER_DTYPE_FLOAT)
					src->addFmt("{}.{} = gl_FrontFacing?1.0:0.0;" _CRLF, _getRegisterVarName(shaderContext, frontFace_regIndex), _getElementStrByIndex(frontFace_chan));
				else
					cemu_assert_debug(false);
			}
		}
	}
	for(auto& cfInstruction : shaderContext->cfInstructions)
		LatteDecompiler_emitClauseCode(shaderContext, &cfInstruction, false);
	if( shader->shaderType == LatteConst::ShaderType::Geometry )
		src->add("EndPrimitive();" _CRLF);
	// vertex shader should write renderstate point size at the end if required but not modified by shader
	if (shaderContext->analyzer.outputPointSize && shaderContext->analyzer.writesPointSize == false)
	{
		if (shader->shaderType == LatteConst::ShaderType::Vertex && shaderContext->options->usesGeometryShader == false)
			src->add("gl_PointSize = uf_pointSize;" _CRLF);
	}
	// end of shader main
	src->add("}" _CRLF);
	src->shrink_to_fit();
	shader->strBuf_shaderSource = src;
}
