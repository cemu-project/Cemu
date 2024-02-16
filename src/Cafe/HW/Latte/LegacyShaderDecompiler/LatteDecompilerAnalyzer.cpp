#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Core/LatteShaderAssembly.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerInternal.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerInstructions.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"

/*
 * Return index of used color attachment based on shader pixel export index (0-7)
 */
sint32 LatteDecompiler_getColorOutputIndexFromExportIndex(LatteDecompilerShaderContext* shaderContext, sint32 exportIndex)
{
	sint32 colorOutputIndex = -1;
	sint32 outputCounter = 0;
	uint32 cbShaderMask = shaderContext->contextRegisters[mmCB_SHADER_MASK];
	uint32 cbShaderControl = shaderContext->contextRegisters[mmCB_SHADER_CONTROL];
	for(sint32 m=0; m<8; m++)
	{
		uint32 outputMask = (cbShaderMask>>(m*4))&0xF;
		if( outputMask == 0 )
			continue;
		cemu_assert_debug(outputMask == 0xF); // mask is unsupported
		if( outputCounter == exportIndex )
		{
			colorOutputIndex = m;
			break;
		}
		outputCounter++;
	}
	cemu_assert_debug(colorOutputIndex != -1); // real outputs and outputs defined via mask do not match up
	return colorOutputIndex;
}

void _remapUniformAccess(LatteDecompilerShaderContext* shaderContext, bool isRegisterUniform, uint32 kcacheBankId, uint32 uniformIndex)
{
	auto& list_uniformMapping = shaderContext->shader->list_remappedUniformEntries;
	for(uint32 i=0; i<list_uniformMapping.size(); i++)
	{
		LatteDecompilerRemappedUniformEntry_t* ufMapping = list_uniformMapping.data()+i;
		if( isRegisterUniform )
		{
			if( ufMapping->isRegister == true && ufMapping->index == uniformIndex )
			{
				return;
			}
		}
		else
		{
			if( ufMapping->isRegister == false && ufMapping->kcacheBankId == kcacheBankId && ufMapping->index == uniformIndex )
			{
				return;
			}
		}
	}
	// add new mapping
	LatteDecompilerRemappedUniformEntry_t newMapping = {0};
	if( isRegisterUniform )
	{
		newMapping.isRegister = true;
		newMapping.index = uniformIndex;
		newMapping.mappedIndex = (uint32)list_uniformMapping.size();
	}
	else
	{
		newMapping.isRegister = false;
		newMapping.kcacheBankId = kcacheBankId;
		newMapping.index = uniformIndex;
		newMapping.mappedIndex = (uint32)list_uniformMapping.size();
	}
	list_uniformMapping.emplace_back(newMapping);
}

/*
 * Returns true if the instruction takes integer operands or returns a integer value
 */
bool _isIntegerInstruction(const LatteDecompilerALUInstruction& aluInstruction)
{
	if (aluInstruction.isOP3 == false)
	{
		// OP2
		switch (aluInstruction.opcode)
		{
		case ALU_OP2_INST_ADD:
		case ALU_OP2_INST_MUL:
		case ALU_OP2_INST_MUL_IEEE:
		case ALU_OP2_INST_MAX:
		case ALU_OP2_INST_MIN:
		case ALU_OP2_INST_FLOOR:
		case ALU_OP2_INST_FRACT:
		case ALU_OP2_INST_TRUNC:
		case ALU_OP2_INST_MOV:
		case ALU_OP2_INST_NOP:
		case ALU_OP2_INST_DOT4:
		case ALU_OP2_INST_DOT4_IEEE:
		case ALU_OP2_INST_CUBE:
		case ALU_OP2_INST_EXP_IEEE:
		case ALU_OP2_INST_LOG_CLAMPED:
		case ALU_OP2_INST_LOG_IEEE:
		case ALU_OP2_INST_SQRT_IEEE:
		case ALU_OP2_INST_SIN:
		case ALU_OP2_INST_COS:
		case ALU_OP2_INST_RNDNE:
		case ALU_OP2_INST_MAX_DX10:
		case ALU_OP2_INST_MIN_DX10:
		case ALU_OP2_INST_SETGT:
		case ALU_OP2_INST_SETGE:
		case ALU_OP2_INST_SETNE:
		case ALU_OP2_INST_SETE:
		case ALU_OP2_INST_PRED_SETE:
		case ALU_OP2_INST_PRED_SETGT:
		case ALU_OP2_INST_PRED_SETGE:
		case ALU_OP2_INST_PRED_SETNE:
		case ALU_OP2_INST_KILLE:
		case ALU_OP2_INST_KILLGT:
		case ALU_OP2_INST_KILLGE:
		case ALU_OP2_INST_RECIP_FF:
		case ALU_OP2_INST_RECIP_IEEE:
		case ALU_OP2_INST_RECIPSQRT_CLAMPED:
		case ALU_OP2_INST_RECIPSQRT_FF:
		case ALU_OP2_INST_RECIPSQRT_IEEE:
			return false;
		case ALU_OP2_INST_FLT_TO_INT:
		case ALU_OP2_INST_INT_TO_FLOAT:
		case ALU_OP2_INST_UINT_TO_FLOAT:
		case ALU_OP2_INST_ASHR_INT:
		case ALU_OP2_INST_LSHR_INT:
		case ALU_OP2_INST_LSHL_INT:
		case ALU_OP2_INST_MULLO_INT:
		case ALU_OP2_INST_MULLO_UINT:
		case ALU_OP2_INST_FLT_TO_UINT:
		case ALU_OP2_INST_AND_INT:
		case ALU_OP2_INST_OR_INT:
		case ALU_OP2_INST_XOR_INT:
		case ALU_OP2_INST_NOT_INT:
		case ALU_OP2_INST_ADD_INT:
		case ALU_OP2_INST_SUB_INT:
		case ALU_OP2_INST_MAX_INT:
		case ALU_OP2_INST_MIN_INT:
		case ALU_OP2_INST_SETE_INT:
		case ALU_OP2_INST_SETGT_INT:
		case ALU_OP2_INST_SETGE_INT:
		case ALU_OP2_INST_SETNE_INT:
		case ALU_OP2_INST_SETGT_UINT:
		case ALU_OP2_INST_SETGE_UINT:
		case ALU_OP2_INST_PRED_SETE_INT:
		case ALU_OP2_INST_PRED_SETGT_INT:
		case ALU_OP2_INST_PRED_SETGE_INT:
		case ALU_OP2_INST_PRED_SETNE_INT:
		case ALU_OP2_INST_KILLE_INT:
		case ALU_OP2_INST_KILLGT_INT:
		case ALU_OP2_INST_KILLNE_INT:
		case ALU_OP2_INST_MOVA_FLOOR:
		case ALU_OP2_INST_MOVA_INT:
			return true;
		// these return an integer result but are usually used only for conditionals
		case ALU_OP2_INST_SETE_DX10:
		case ALU_OP2_INST_SETGT_DX10:
		case ALU_OP2_INST_SETGE_DX10:
		case ALU_OP2_INST_SETNE_DX10:
			return true;
		default:
#ifdef CEMU_DEBUG_ASSERT
			debug_printf("_isIntegerInstruction(): OP3=%s opcode=%02x\n", aluInstruction.isOP3 ? "true" : "false", aluInstruction.opcode);
			cemu_assert_debug(false);
#endif
			break;
		}
	}
	else
	{
		// OP3
		switch (aluInstruction.opcode)
		{
		case ALU_OP3_INST_MULADD:
		case ALU_OP3_INST_MULADD_D2:
		case ALU_OP3_INST_MULADD_M2:
		case ALU_OP3_INST_MULADD_M4:
		case ALU_OP3_INST_MULADD_IEEE:
		case ALU_OP3_INST_CMOVE:
		case ALU_OP3_INST_CMOVGT:
		case ALU_OP3_INST_CMOVGE:
			return false;
		case ALU_OP3_INST_CNDE_INT:
		case ALU_OP3_INST_CNDGT_INT:
		case ALU_OP3_INST_CMOVGE_INT:
			return true;
		default:
#ifdef CEMU_DEBUG_ASSERT
			debug_printf("_isIntegerInstruction(): OP3=%s opcode=%02x\n", aluInstruction.isOP3?"true":"false", aluInstruction.opcode);
#endif
			break;
		}
	}
	return false;
}

/*
 * Analyze ALU CF instruction and all instructions within the ALU clause
 */
void LatteDecompiler_analyzeALUClause(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction)
{
	// check if this shader has any clause that potentially modifies the pixel execution state
	if( cfInstruction->type == GPU7_CF_INST_ALU_PUSH_BEFORE || cfInstruction->type == GPU7_CF_INST_ALU_POP_AFTER || cfInstruction->type == GPU7_CF_INST_ALU_POP2_AFTER || cfInstruction->type == GPU7_CF_INST_ALU_BREAK || cfInstruction->type == GPU7_CF_INST_ALU_ELSE_AFTER )
	{
		shaderContext->analyzer.modifiesPixelActiveState = true;
	}
	// analyze ALU instructions
	for(auto& aluInstruction : cfInstruction->instructionsALU)
	{
		// ignore NOP instruction
		if( !aluInstruction.isOP3 && aluInstruction.opcode == ALU_OP2_INST_NOP )
			continue;
		// check for CUBE instruction
		if( !aluInstruction.isOP3 && aluInstruction.opcode == ALU_OP2_INST_CUBE )
		{
			shaderContext->analyzer.hasRedcCUBE = true;
		}
		// check for integer instruction
		if (_isIntegerInstruction(aluInstruction))
			shaderContext->analyzer.usesIntegerValues = true;
		// process all available operands (inputs)
		for(sint32 f=0; f<3; f++)
		{
			// check input for uniform access
			if( aluInstruction.sourceOperand[f].sel == 0xFFFFFFFF )
				continue; // source operand not set/used
			// about uniform register and buffer access tracking:
			// for absolute indices we can determine a maximum size that is accessed
			// relative accesses are tricky because the upper bound of accessed indices is unknown
			// worst case we have to load the full file (256 * 16 byte entries) or for buffers an arbitrary upper bound (64KB in our case)
			if( GPU7_ALU_SRC_IS_CFILE(aluInstruction.sourceOperand[f].sel) )
			{
				if (aluInstruction.sourceOperand[f].rel)
				{
					shaderContext->analyzer.uniformRegisterAccessTracker.TrackAccess(GPU7_ALU_SRC_GET_CFILE_INDEX(aluInstruction.sourceOperand[f].sel), true);
				}
				else
				{
					_remapUniformAccess(shaderContext, true, 0, GPU7_ALU_SRC_GET_CFILE_INDEX(aluInstruction.sourceOperand[f].sel));
					shaderContext->analyzer.uniformRegisterAccessTracker.TrackAccess(GPU7_ALU_SRC_GET_CFILE_INDEX(aluInstruction.sourceOperand[f].sel), false);
				}
			}
			else if( GPU7_ALU_SRC_IS_CBANK0(aluInstruction.sourceOperand[f].sel) )
			{
				// uniform bank 0 (uniform buffer with index cfInstruction->cBank0Index)
				uint32 uniformBufferIndex = cfInstruction->cBank0Index;
				cemu_assert(uniformBufferIndex < LATTE_NUM_MAX_UNIFORM_BUFFERS);
				uint32 offset = GPU7_ALU_SRC_GET_CBANK0_INDEX(aluInstruction.sourceOperand[f].sel)+cfInstruction->cBank0AddrBase;
				_remapUniformAccess(shaderContext, false, uniformBufferIndex, offset);
				shaderContext->analyzer.uniformBufferAccessTracker[uniformBufferIndex].TrackAccess(offset, aluInstruction.sourceOperand[f].rel);
			}
			else if( GPU7_ALU_SRC_IS_CBANK1(aluInstruction.sourceOperand[f].sel) )
			{
				// uniform bank 1 (uniform buffer with index cfInstruction->cBank1Index)
				uint32 uniformBufferIndex = cfInstruction->cBank1Index;
				cemu_assert(uniformBufferIndex < LATTE_NUM_MAX_UNIFORM_BUFFERS);
				uint32 offset = GPU7_ALU_SRC_GET_CBANK1_INDEX(aluInstruction.sourceOperand[f].sel)+cfInstruction->cBank1AddrBase;
				_remapUniformAccess(shaderContext, false, uniformBufferIndex, offset);
				shaderContext->analyzer.uniformBufferAccessTracker[uniformBufferIndex].TrackAccess(offset, aluInstruction.sourceOperand[f].rel);
			}
			else if( GPU7_ALU_SRC_IS_GPR(aluInstruction.sourceOperand[f].sel) )
			{
				sint32 gprIndex = GPU7_ALU_SRC_GET_GPR_INDEX(aluInstruction.sourceOperand[f].sel);
				shaderContext->analyzer.gprUseMask[gprIndex/8] |= (1<<(gprIndex%8));
				if( aluInstruction.sourceOperand[f].rel != 0 )
				{
					// if indexed register access is used, all possibly referenced registers are stored to a separate array at the beginning of the group
					shaderContext->analyzer.usesRelativeGPRRead = true;
					continue;
				}

			}
		}
		if( aluInstruction.destRel != 0 )
			shaderContext->analyzer.usesRelativeGPRWrite = true;
		shaderContext->analyzer.gprUseMask[aluInstruction.destGpr/8] |= (1<<(aluInstruction.destGpr%8));
	}
}

// analyze TEX CF instruction and all instructions within the TEX clause
void LatteDecompiler_analyzeTEXClause(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction)
{
	LatteDecompilerShader* shader = shaderContext->shader;
	for(auto& texInstruction : cfInstruction->instructionsTEX)
	{
		if( texInstruction.opcode == GPU7_TEX_INST_SAMPLE || 
			texInstruction.opcode == GPU7_TEX_INST_SAMPLE_L || 
			texInstruction.opcode == GPU7_TEX_INST_SAMPLE_LB || 
			texInstruction.opcode == GPU7_TEX_INST_SAMPLE_LZ || 
			texInstruction.opcode == GPU7_TEX_INST_SAMPLE_C || 
			texInstruction.opcode == GPU7_TEX_INST_SAMPLE_C_L ||
			texInstruction.opcode == GPU7_TEX_INST_SAMPLE_C_LZ ||
			texInstruction.opcode == GPU7_TEX_INST_FETCH4 || 
			texInstruction.opcode == GPU7_TEX_INST_SAMPLE_G || 
			texInstruction.opcode == GPU7_TEX_INST_LD )
		{
			if (texInstruction.textureFetch.textureIndex < 0 || texInstruction.textureFetch.textureIndex >= LATTE_NUM_MAX_TEX_UNITS)
			{
				cemuLog_logDebug(LogType::Force, "Shader {:16x} has out of bounds texture access (texture {})", shaderContext->shader->baseHash, (sint32)texInstruction.textureFetch.textureIndex);
				continue;
			}
			if( texInstruction.textureFetch.samplerIndex < 0 || texInstruction.textureFetch.samplerIndex >= 0x12 )
				cemu_assert_debug(false);
			if(shaderContext->output->textureUnitMask[texInstruction.textureFetch.textureIndex] && shader->textureUnitSamplerAssignment[texInstruction.textureFetch.textureIndex] != texInstruction.textureFetch.samplerIndex && shader->textureUnitSamplerAssignment[texInstruction.textureFetch.textureIndex] != LATTE_DECOMPILER_SAMPLER_NONE )
			{
				cemu_assert_debug(false);
			}
			shaderContext->output->textureUnitMask[texInstruction.textureFetch.textureIndex] = true;
			shader->textureUnitSamplerAssignment[texInstruction.textureFetch.textureIndex] = texInstruction.textureFetch.samplerIndex;
			if( texInstruction.opcode == GPU7_TEX_INST_SAMPLE_C || texInstruction.opcode == GPU7_TEX_INST_SAMPLE_C_L || texInstruction.opcode == GPU7_TEX_INST_SAMPLE_C_LZ)
				shader->textureUsesDepthCompare[texInstruction.textureFetch.textureIndex] = true;
			
			bool useTexelCoords = false;
			if (texInstruction.opcode == GPU7_TEX_INST_SAMPLE && (texInstruction.textureFetch.unnormalized[0] && texInstruction.textureFetch.unnormalized[1] && texInstruction.textureFetch.unnormalized[2] && texInstruction.textureFetch.unnormalized[3]))
				useTexelCoords = true;
			else if (texInstruction.opcode == GPU7_TEX_INST_LD)
				useTexelCoords = true;
			if (useTexelCoords)
			{
				shaderContext->analyzer.texUnitUsesTexelCoordinates.set(texInstruction.textureFetch.textureIndex);
			}
		}
		else if( texInstruction.opcode == GPU7_TEX_INST_GET_COMP_TEX_LOD || texInstruction.opcode == GPU7_TEX_INST_GET_TEXTURE_RESINFO )
		{
			if( texInstruction.textureFetch.textureIndex < 0 || texInstruction.textureFetch.textureIndex >= LATTE_NUM_MAX_TEX_UNITS )
				debugBreakpoint();
			if( texInstruction.textureFetch.samplerIndex != 0 )
				debugBreakpoint(); // sampler is ignored and should be 0
			shaderContext->output->textureUnitMask[texInstruction.textureFetch.textureIndex] = true;
		}
		else if( texInstruction.opcode == GPU7_TEX_INST_SET_CUBEMAP_INDEX )
		{
			// no analysis required
		}
		else if (texInstruction.opcode == GPU7_TEX_INST_GET_GRADIENTS_H || texInstruction.opcode == GPU7_TEX_INST_GET_GRADIENTS_V)
		{
			// no analysis required
		}
		else if (texInstruction.opcode == GPU7_TEX_INST_SET_GRADIENTS_H || texInstruction.opcode == GPU7_TEX_INST_SET_GRADIENTS_V)
		{
			shaderContext->analyzer.hasGradientLookup = true;
		}
		else if( texInstruction.opcode == GPU7_TEX_INST_VFETCH )
		{
			// VFETCH is used to access uniform buffers dynamically
			if( texInstruction.textureFetch.textureIndex >= 0x80 && texInstruction.textureFetch.textureIndex <= 0x8F )
			{
				uint32 uniformBufferIndex = texInstruction.textureFetch.textureIndex - 0x80;
				shaderContext->analyzer.uniformBufferAccessTracker[uniformBufferIndex].TrackAccess(0, true);
			}
			else if( texInstruction.textureFetch.textureIndex == 0x9F && shader->shaderType == LatteConst::ShaderType::Geometry )
			{
				// instruction to read geometry shader input from ringbuffer
			}
			else
				debugBreakpoint();
		}
		else if (texInstruction.opcode == GPU7_TEX_INST_MEM)
		{
			// SSBO access
			shaderContext->analyzer.hasSSBORead = true;
		}
		else
			debugBreakpoint();
		// mark read and written registers as used
		if(texInstruction.dstGpr < LATTE_NUM_GPR)
			shaderContext->analyzer.gprUseMask[texInstruction.dstGpr/8] |= (1<<(texInstruction.dstGpr%8));
		if(texInstruction.srcGpr < LATTE_NUM_GPR)
			shaderContext->analyzer.gprUseMask[texInstruction.srcGpr/8] |= (1<<(texInstruction.srcGpr%8));
	}
}

/*
 * Analyze export CF instruction
 */
void LatteDecompiler_analyzeExport(LatteDecompilerShaderContext* shaderContext, LatteDecompilerCFInstruction* cfInstruction)
{
	LatteDecompilerShader* shader = shaderContext->shader;
	if( shader->shaderType == LatteConst::ShaderType::Pixel )
	{
		if( cfInstruction->exportType == 0 && cfInstruction->exportArrayBase < 8 )
		{
			// remember color outputs that are written
			for(uint32 i=0; i<(cfInstruction->exportBurstCount+1); i++)
			{
				sint32 colorOutputIndex = LatteDecompiler_getColorOutputIndexFromExportIndex(shaderContext, cfInstruction->exportArrayBase+i);
				shader->pixelColorOutputMask |= (1<<colorOutputIndex);
			}
		}
		else if( cfInstruction->exportType == 0 && cfInstruction->exportArrayBase == 61 )
		{
			// writes pixel depth
		}
		else
			debugBreakpoint();
	}
	else if (shader->shaderType == LatteConst::ShaderType::Vertex)
	{
		if (cfInstruction->exportType == 2 && cfInstruction->exportArrayBase < 32)
		{
			shaderContext->shader->outputParameterMask |= (1<<cfInstruction->exportArrayBase);
		}
		else if (cfInstruction->exportType == 1 && cfInstruction->exportArrayBase == GPU7_DECOMPILER_CF_EXPORT_POINT_SIZE)
		{
			shaderContext->analyzer.writesPointSize = true;
		}
	}
	// mark input GPRs as used
	for(uint32 i=0; i<(cfInstruction->exportBurstCount+1); i++)
	{
		shaderContext->analyzer.gprUseMask[(cfInstruction->exportSourceGPR+i)/8] |= (1<<((cfInstruction->exportSourceGPR+i)%8));
	}
}

void LatteDecompiler_analyzeSubroutine(LatteDecompilerShaderContext* shaderContext, uint32 cfAddr)
{
	// analyze CF and clauses up to RET statement
	
	// todo - find cfInstruction index from cfAddr
	cemu_assert_debug(false);

	for(auto& cfInstruction : shaderContext->cfInstructions)
	{
		if (cfInstruction.type == GPU7_CF_INST_ALU || cfInstruction.type == GPU7_CF_INST_ALU_PUSH_BEFORE || cfInstruction.type == GPU7_CF_INST_ALU_POP_AFTER || cfInstruction.type == GPU7_CF_INST_ALU_POP2_AFTER || cfInstruction.type == GPU7_CF_INST_ALU_BREAK || cfInstruction.type == GPU7_CF_INST_ALU_ELSE_AFTER)
		{
			LatteDecompiler_analyzeALUClause(shaderContext, &cfInstruction);
		}
		else if (cfInstruction.type == GPU7_CF_INST_TEX)
		{
			LatteDecompiler_analyzeTEXClause(shaderContext, &cfInstruction);
		}
		else if (cfInstruction.type == GPU7_CF_INST_EXPORT || cfInstruction.type == GPU7_CF_INST_EXPORT_DONE)
		{
			LatteDecompiler_analyzeExport(shaderContext, &cfInstruction);
		}
		else if (cfInstruction.type == GPU7_CF_INST_ELSE || cfInstruction.type == GPU7_CF_INST_POP)
		{
			shaderContext->analyzer.modifiesPixelActiveState = true;
		}
		else if (cfInstruction.type == GPU7_CF_INST_LOOP_START_DX10 || cfInstruction.type == GPU7_CF_INST_LOOP_END)
		{
			shaderContext->analyzer.modifiesPixelActiveState = true;
		}
		else if (cfInstruction.type == GPU7_CF_INST_LOOP_BREAK)
		{
			shaderContext->analyzer.modifiesPixelActiveState = true;
		}
		else if (cfInstruction.type == GPU7_CF_INST_EMIT_VERTEX)
		{
			// nothing to analyze
		}
		else if (cfInstruction.type == GPU7_CF_INST_CALL)
		{
			cemu_assert_debug(false); // CALLs inside subroutines are still todo
		}
		else
		{
			cemu_assert_unimplemented();
		}
	}
}

namespace LatteDecompiler
{
	void _initTextureBindingPointsGL(LatteDecompilerShaderContext* decompilerContext)
	{
		// for OpenGL we use the relative texture unit index
		for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
		{
			if (!decompilerContext->output->textureUnitMask[i])
				continue;
			sint32 textureBindingPoint;
			if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex)
				textureBindingPoint = i + LATTE_CEMU_VS_TEX_UNIT_BASE;
			else if (decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
				textureBindingPoint = i + LATTE_CEMU_GS_TEX_UNIT_BASE;
			else if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
				textureBindingPoint = i + LATTE_CEMU_PS_TEX_UNIT_BASE;

			decompilerContext->output->resourceMappingGL.textureUnitToBindingPoint[i] = textureBindingPoint;
		}
	}

	void _initTextureBindingPointsVK(LatteDecompilerShaderContext* decompilerContext)
	{
		// for Vulkan we use consecutive indices
		for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
		{
			if (!decompilerContext->output->textureUnitMask[i])
				continue;
			decompilerContext->output->resourceMappingVK.textureUnitToBindingPoint[i] = decompilerContext->currentBindingPointVK;
			decompilerContext->currentBindingPointVK++;
		}
	}

	void _initHasUniformVarBlock(LatteDecompilerShaderContext* decompilerContext)
	{
		decompilerContext->hasUniformVarBlock = false;
		if (decompilerContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_REMAPPED)
			decompilerContext->hasUniformVarBlock = true;
		else if (decompilerContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CFILE)
			decompilerContext->hasUniformVarBlock = true;
		
		bool hasAnyViewportScaleDisabled = 
			!decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_X_SCALE_ENA() || 
			!decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_Y_SCALE_ENA() ||
			!decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_Z_SCALE_ENA();
		// we currently only support all on/off. Individual component scaling is not supported
		cemu_assert_debug(decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_X_SCALE_ENA() == !hasAnyViewportScaleDisabled);
		cemu_assert_debug(decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_Y_SCALE_ENA() == !hasAnyViewportScaleDisabled);
		cemu_assert_debug(decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_Z_SCALE_ENA() == !hasAnyViewportScaleDisabled);
		cemu_assert_debug(decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_X_OFFSET_ENA() == !hasAnyViewportScaleDisabled);
		cemu_assert_debug(decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_Y_OFFSET_ENA() == !hasAnyViewportScaleDisabled);
		cemu_assert_debug(decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_Z_OFFSET_ENA() == !hasAnyViewportScaleDisabled);

		if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex && hasAnyViewportScaleDisabled)
			decompilerContext->hasUniformVarBlock = true; // uf_windowSpaceToClipSpaceTransform
		bool alphaTestEnable = decompilerContext->contextRegistersNew->SX_ALPHA_TEST_CONTROL.get_ALPHA_TEST_ENABLE();
		if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel && alphaTestEnable != 0)
			decompilerContext->hasUniformVarBlock = true; // uf_alphaTestRef
		if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
			decompilerContext->hasUniformVarBlock = true; // uf_fragCoordScale
		if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex && decompilerContext->analyzer.outputPointSize && decompilerContext->analyzer.writesPointSize == false)
			decompilerContext->hasUniformVarBlock = true; // uf_pointSize
		if (decompilerContext->shaderType == LatteConst::ShaderType::Geometry && decompilerContext->analyzer.outputPointSize && decompilerContext->analyzer.writesPointSize == false)
			decompilerContext->hasUniformVarBlock = true; // uf_pointSize
		if (decompilerContext->analyzer.useSSBOForStreamout &&
			(decompilerContext->shaderType == LatteConst::ShaderType::Vertex && !decompilerContext->options->usesGeometryShader) ||
			(decompilerContext->shaderType == LatteConst::ShaderType::Geometry))
		{
			decompilerContext->hasUniformVarBlock = true; // uf_verticesPerInstance and uf_streamoutBufferBase*
		}
	}

	void _initUniformBindingPoints(LatteDecompilerShaderContext* decompilerContext)
	{
		// check if uniform vars block has at least one variable
		_initHasUniformVarBlock(decompilerContext);

		if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
		{
			for (sint32 t = 0; t < LATTE_NUM_MAX_TEX_UNITS; t++)
			{
				if (decompilerContext->analyzer.texUnitUsesTexelCoordinates.test(t) == false)
					continue;
				decompilerContext->hasUniformVarBlock = true; // uf_tex%dScale
			}
		}
		// assign binding point to uniform var block
		decompilerContext->output->resourceMappingGL.uniformVarsBufferBindingPoint = -1; // OpenGL currently doesnt use a uniform block
		if (decompilerContext->hasUniformVarBlock)
		{
			decompilerContext->output->resourceMappingVK.uniformVarsBufferBindingPoint = decompilerContext->currentBindingPointVK;
			decompilerContext->currentBindingPointVK++;
		}
		else
			decompilerContext->output->resourceMappingVK.uniformVarsBufferBindingPoint = -1;
		// assign binding points to uniform buffers
		if (decompilerContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK)
		{
			// for Vulkan we use consecutive indices
			for (uint32 i = 0; i < LATTE_NUM_MAX_UNIFORM_BUFFERS; i++)
			{
				if (!decompilerContext->analyzer.uniformBufferAccessTracker[i].HasAccess())
					continue;
				sint32 uniformBindingPoint = i;
				if (decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
					uniformBindingPoint += 64;
				else if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex)
					uniformBindingPoint += 0;
				else if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
					uniformBindingPoint += 32;

				decompilerContext->output->resourceMappingVK.uniformBuffersBindingPoint[i] = decompilerContext->currentBindingPointVK;
				decompilerContext->currentBindingPointVK++;
			}
			// for OpenGL we use the relative buffer index
			for (uint32 i = 0; i < LATTE_NUM_MAX_UNIFORM_BUFFERS; i++)
			{
				if (!decompilerContext->analyzer.uniformBufferAccessTracker[i].HasAccess())
					continue;
				sint32 uniformBindingPoint = i;
				if (decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
					uniformBindingPoint += 64;
				else if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex)
					uniformBindingPoint += 0;
				else if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
					uniformBindingPoint += 32;
				decompilerContext->output->resourceMappingGL.uniformBuffersBindingPoint[i] = uniformBindingPoint;
			}
		}
		// shader storage buffer for alternative transform feedback path
		if (decompilerContext->analyzer.useSSBOForStreamout)
		{
			decompilerContext->output->resourceMappingVK.tfStorageBindingPoint = decompilerContext->currentBindingPointVK;
			decompilerContext->currentBindingPointVK++;
		}
	}

	void _initAttributeBindingPoints(LatteDecompilerShaderContext* decompilerContext)
	{
		if (decompilerContext->shaderType != LatteConst::ShaderType::Vertex)
			return;
		// create input attribute binding mapping
		// OpenGL and Vulkan use consecutive indices starting at 0
		sint8 bindingIndex = 0;
		for (sint32 i = 0; i < LATTE_NUM_MAX_ATTRIBUTE_LOCATIONS; i++)
		{
			if (decompilerContext->analyzer.inputAttributSemanticMask[i])
			{
				decompilerContext->output->resourceMappingGL.attributeMapping[i] = bindingIndex;
				decompilerContext->output->resourceMappingVK.attributeMapping[i] = bindingIndex;
				bindingIndex++;
			}
		}
	}

}

/*
 * Analyze the shader program
 * This will help to determine:
 * 1) Uniform usage
 * 2) Texture usage
 * 3) Data types
 * 4) CF stack and execution flow
 */
void LatteDecompiler_analyze(LatteDecompilerShaderContext* shaderContext, LatteDecompilerShader* shader)
{
	// analyze render state
	shaderContext->analyzer.isPointsPrimitive = shaderContext->contextRegistersNew->VGT_PRIMITIVE_TYPE.get_PRIMITIVE_MODE() == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::POINTS;
	shaderContext->analyzer.hasStreamoutEnable = shaderContext->contextRegisters[mmVGT_STRMOUT_EN] != 0; // set if the shader is used for transform feedback operations
	if (shaderContext->shaderType == LatteConst::ShaderType::Vertex && !shaderContext->options->usesGeometryShader)
		shaderContext->analyzer.outputPointSize = shaderContext->analyzer.isPointsPrimitive;
	else if (shaderContext->shaderType == LatteConst::ShaderType::Geometry)
	{
		uint32 gsOutPrimType = shaderContext->contextRegisters[mmVGT_GS_OUT_PRIM_TYPE];
		if (gsOutPrimType == 0) // points
			shaderContext->analyzer.outputPointSize = true;
	}
	// analyze input attributes for vertex/geometry shader
	if (shader->shaderType == LatteConst::ShaderType::Vertex || shader->shaderType == LatteConst::ShaderType::Geometry)
	{
		if(shaderContext->fetchShader)
		{
			LatteFetchShader* parsedFetchShader = shaderContext->fetchShader;
			for(auto& bufferGroup : parsedFetchShader->bufferGroups)
			{
				for (sint32 i = 0; i < bufferGroup.attribCount; i++)
				{
					uint8 semanticId = bufferGroup.attrib[i].semanticId;
					if (semanticId == 0xFF)
					{
						// unused attribute? Found in Hot Wheels: World's best driver
						continue;
					}
					cemu_assert_debug(semanticId < 0x80);
					shaderContext->analyzer.inputAttributSemanticMask[semanticId] = true;
				}
			}
		}
	}
	// list of subroutines (call destinations)
	std::vector<uint32> list_subroutineAddrs;
	// analyze CF and clauses
	for(auto& cfInstruction : shaderContext->cfInstructions)
	{
		if (cfInstruction.type == GPU7_CF_INST_ALU || cfInstruction.type == GPU7_CF_INST_ALU_PUSH_BEFORE || cfInstruction.type == GPU7_CF_INST_ALU_POP_AFTER || cfInstruction.type == GPU7_CF_INST_ALU_POP2_AFTER || cfInstruction.type == GPU7_CF_INST_ALU_BREAK || cfInstruction.type == GPU7_CF_INST_ALU_ELSE_AFTER)
		{
			LatteDecompiler_analyzeALUClause(shaderContext, &cfInstruction);
		}
		else if (cfInstruction.type == GPU7_CF_INST_TEX)
		{
			LatteDecompiler_analyzeTEXClause(shaderContext, &cfInstruction);
		}
		else if (cfInstruction.type == GPU7_CF_INST_EXPORT || cfInstruction.type == GPU7_CF_INST_EXPORT_DONE)
		{
			LatteDecompiler_analyzeExport(shaderContext, &cfInstruction);
		}
		else if (cfInstruction.type == GPU7_CF_INST_ELSE || cfInstruction.type == GPU7_CF_INST_POP)
		{
			shaderContext->analyzer.modifiesPixelActiveState = true;
		}
		else if (cfInstruction.type == GPU7_CF_INST_LOOP_START_DX10 || cfInstruction.type == GPU7_CF_INST_LOOP_END)
		{
			shaderContext->analyzer.modifiesPixelActiveState = true;
			shaderContext->analyzer.hasLoops = true;
		}
		else if (cfInstruction.type == GPU7_CF_INST_LOOP_BREAK)
		{
			shaderContext->analyzer.modifiesPixelActiveState = true;
			shaderContext->analyzer.hasLoops = true;
		}
		else if (cfInstruction.type == GPU7_CF_INST_MEM_STREAM0_WRITE ||
			cfInstruction.type == GPU7_CF_INST_MEM_STREAM1_WRITE)
		{
			uint32 streamoutBufferIndex;
			if (cfInstruction.type == GPU7_CF_INST_MEM_STREAM0_WRITE)
				streamoutBufferIndex = 0;
			else if (cfInstruction.type == GPU7_CF_INST_MEM_STREAM1_WRITE)
				streamoutBufferIndex = 1;
			else
				cemu_assert_debug(false);
			shaderContext->analyzer.hasStreamoutWrite = true;
			cemu_assert(streamoutBufferIndex < shaderContext->output->streamoutBufferWriteMask.size());
			shaderContext->output->streamoutBufferWriteMask[streamoutBufferIndex] = true;
			uint32 vectorWriteSize = 0;
			for (sint32 f = 0; f < 4; f++)
			{
				if ((cfInstruction.memWriteCompMask & (1 << f)) != 0)
					vectorWriteSize = (f + 1) * 4;
				shaderContext->output->streamoutBufferStride[f] = shaderContext->contextRegisters[mmVGT_STRMOUT_VTX_STRIDE_0 + f * 4] << 2;
			}

			cemu_assert_debug((cfInstruction.exportArrayBase * 4 + vectorWriteSize) <= shaderContext->output->streamoutBufferStride[streamoutBufferIndex]);
		}
		else if (cfInstruction.type == GPU7_CF_INST_MEM_RING_WRITE)
		{
			// track number of parameters that are output (simplified by just tracking the offset of the last one)
			if (cfInstruction.memWriteElemSize != 3)
				debugBreakpoint();
			if (cfInstruction.exportBurstCount != 0 && cfInstruction.memWriteElemSize != 3)
			{
				debugBreakpoint();
			}
			uint32 dwordWriteCount = (cfInstruction.exportBurstCount + 1) * 4;
			uint32 numRingParameter = (cfInstruction.exportArrayBase + dwordWriteCount) / 4;
			shader->ringParameterCount = std::max(shader->ringParameterCount, numRingParameter);
			// mark input GPRs as used
			for (uint32 i = 0; i < (cfInstruction.exportBurstCount + 1); i++)
			{
				shaderContext->analyzer.gprUseMask[(cfInstruction.exportSourceGPR + i) / 8] |= (1 << ((cfInstruction.exportSourceGPR + i) % 8));
			}
		}
		else if (cfInstruction.type == GPU7_CF_INST_EMIT_VERTEX)
		{
			shaderContext->analyzer.numEmitVertex++;
		}
		else if (cfInstruction.type == GPU7_CF_INST_CALL)
		{
			// CALL instruction does not need analyzing
			// and subroutines are analyzed separately
		}
		else
			cemu_assert_unimplemented();
	}
	// analyze subroutines
	for (auto subroutineAddr : list_subroutineAddrs)
	{
		LatteDecompiler_analyzeSubroutine(shaderContext, subroutineAddr);
	}
	// decide which uniform mode to use
	bool hasAnyDynamicBufferAccess = false;
	bool hasAnyBufferAccess = false;
	for(auto& it : shaderContext->analyzer.uniformBufferAccessTracker)
	{
		if( it.HasRelativeAccess() )
			hasAnyDynamicBufferAccess = true;
		if( it.HasAccess() )
			hasAnyBufferAccess = true;
	}
	if (hasAnyDynamicBufferAccess)
	{
		shader->uniformMode = LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK;
	}
	else if(shaderContext->analyzer.uniformRegisterAccessTracker.HasRelativeAccess() )
	{
		shader->uniformMode = LATTE_DECOMPILER_UNIFORM_MODE_FULL_CFILE;
	}
	else if(hasAnyBufferAccess || shaderContext->analyzer.uniformRegisterAccessTracker.HasAccess() )
	{
		shader->uniformMode = LATTE_DECOMPILER_UNIFORM_MODE_REMAPPED;
	}
	else
	{
		shader->uniformMode = LATTE_DECOMPILER_UNIFORM_MODE_NONE;
	}
	// generate compact list of uniform buffers (for faster access)
	cemu_assert_debug(shader->list_quickBufferList.empty());
	for (uint32 i = 0; i < LATTE_NUM_MAX_UNIFORM_BUFFERS; i++)
	{
		if( !shaderContext->analyzer.uniformBufferAccessTracker[i].HasAccess() )
			continue;
		LatteDecompilerShader::QuickBufferEntry entry;
		entry.index = i;
		entry.size = shaderContext->analyzer.uniformBufferAccessTracker[i].DetermineSize(LATTE_GLSL_DYNAMIC_UNIFORM_BLOCK_SIZE) * 16;
		shader->list_quickBufferList.push_back(entry);
	}
	// get dimension of each used texture
	_LatteRegisterSetTextureUnit* texRegs = nullptr;
	if( shader->shaderType == LatteConst::ShaderType::Vertex )
		texRegs = shaderContext->contextRegistersNew->SQ_TEX_START_VS;
	else if( shader->shaderType == LatteConst::ShaderType::Pixel )
		texRegs = shaderContext->contextRegistersNew->SQ_TEX_START_PS;
	else if( shader->shaderType == LatteConst::ShaderType::Geometry )
		texRegs = shaderContext->contextRegistersNew->SQ_TEX_START_GS;

	for(sint32 i=0; i<LATTE_NUM_MAX_TEX_UNITS; i++)
	{
		if (!shaderContext->output->textureUnitMask[i]) 
		{
			// texture unit not used
			shader->textureUnitDim[i] = (Latte::E_DIM)0xFF;
			continue;
		}
		auto& texUnit = texRegs[i];
		auto dim = texUnit.word0.get_DIM();
		shader->textureUnitDim[i] = dim;
		if(dim == Latte::E_DIM::DIM_CUBEMAP)
			shaderContext->analyzer.hasCubeMapTexture = true;
		shader->textureIsIntegerFormat[i] = texUnit.word4.get_NUM_FORM_ALL() == Latte::LATTE_SQ_TEX_RESOURCE_WORD4_N::E_NUM_FORMAT_ALL::NUM_FORMAT_INT;
	}
	// generate list of used texture units
	shader->textureUnitListCount = 0;
	for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
	{
		if (shaderContext->output->textureUnitMask[i])
		{
			shader->textureUnitList[shader->textureUnitListCount] = i;
			shader->textureUnitListCount++;
		}
	}
	// for geometry shaders check the copy shader for stream writes
	if (shader->shaderType == LatteConst::ShaderType::Geometry && shaderContext->parsedGSCopyShader->list_streamWrites.empty() == false)
	{
		shaderContext->analyzer.hasStreamoutWrite = true;
		if (shaderContext->contextRegisters[mmVGT_STRMOUT_EN] != 0)
			shaderContext->analyzer.hasStreamoutEnable = true;
		for (auto& it : shaderContext->parsedGSCopyShader->list_streamWrites)
		{
			shaderContext->output->streamoutBufferWriteMask[it.bufferIndex] = true;
			uint32 vectorWriteSize = 0;
			for (sint32 f = 0; f < 4; f++)
			{
				if ((it.memWriteCompMask&(1 << f)) != 0)
					vectorWriteSize = (f + 1) * 4;
			}
			shaderContext->output->streamoutBufferStride[it.bufferIndex] = std::max(shaderContext->output->streamoutBufferStride[it.bufferIndex], it.exportArrayBase * 4 + vectorWriteSize);
		}
	}
	// analyze input attributes again (if shader has relative GPR read)
	if(shaderContext->analyzer.usesRelativeGPRRead && (shader->shaderType == LatteConst::ShaderType::Vertex || shader->shaderType == LatteConst::ShaderType::Geometry) )
	{
		if(shaderContext->fetchShader)
		{
			LatteFetchShader* parsedFetchShader = shaderContext->fetchShader;
			for(auto& bufferGroup : parsedFetchShader->bufferGroups)
			{
				for (sint32 i = 0; i < bufferGroup.attribCount; i++)
				{
					uint32 registerIndex;
					// get register index based on vtx semantic table
					uint32 attributeShaderLoc = 0xFFFFFFFF;
					for (sint32 f = 0; f < 32; f++)
					{
						if (shaderContext->contextRegisters[mmSQ_VTX_SEMANTIC_0 + f] == bufferGroup.attrib[i].semanticId)
						{
							attributeShaderLoc = f;
							break;
						}
					}
					if (attributeShaderLoc == 0xFFFFFFFF)
						continue; // attribute is not mapped to VS input
					registerIndex = attributeShaderLoc + 1;
					shaderContext->analyzer.gprUseMask[registerIndex / 8] |= (1 << (registerIndex % 8));
				}
			}
		}
	}
	else if (shaderContext->analyzer.usesRelativeGPRRead && shader->shaderType == LatteConst::ShaderType::Pixel)
	{
		// mark pixel shader inputs as used if there is any relative GPR access
		LatteShaderPSInputTable* psInputTable = LatteSHRC_GetPSInputTable();
		for (sint32 i = 0; i < psInputTable->count; i++)
		{
			shaderContext->analyzer.gprUseMask[i / 8] |= (1 << (i % 8));
		}
	}
	// analyze CF stack
	sint32 cfCurrentStackDepth = 0;
	sint32 cfCurrentMaxStackDepth = 0;
	for(auto& cfInstruction : shaderContext->cfInstructions)
	{
		if (cfInstruction.type == GPU7_CF_INST_ALU)
		{
			// no effect on stack depth
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_ALU_PUSH_BEFORE )
		{
			cfCurrentStackDepth++;
			cfCurrentMaxStackDepth = std::max(cfCurrentMaxStackDepth, cfCurrentStackDepth);
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_ALU_POP_AFTER)
		{
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
			cfCurrentStackDepth--;
		}
		else if (cfInstruction.type == GPU7_CF_INST_ALU_POP2_AFTER)
		{
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
			cfCurrentStackDepth -= 2;
		}
		else if (cfInstruction.type == GPU7_CF_INST_ALU_BREAK )
		{
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_ALU_ELSE_AFTER)
		{
			if (cfInstruction.popCount != 0)
				debugBreakpoint();
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_ELSE )
		{
			//if (cfInstruction.popCount != 0)
			//	debugBreakpoint(); -> Only relevant when ELSE jump is taken
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_POP)
		{
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
			cfCurrentStackDepth -= cfInstruction.popCount;
			if (cfCurrentStackDepth < 0)
				debugBreakpoint();
		}
		else if (cfInstruction.type == GPU7_CF_INST_LOOP_START_DX10 || cfInstruction.type == GPU7_CF_INST_LOOP_END)
		{
			// no effect on stack depth
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_LOOP_BREAK)
		{
			// since we assume that the break is not taken (for all pixels), we also don't need to worry about the stack depth adjustment
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_TEX)
		{
			// no effect on stack depth
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_EXPORT || cfInstruction.type == GPU7_CF_INST_EXPORT_DONE)
		{
			// no effect on stack depth
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_MEM_STREAM0_WRITE ||
			cfInstruction.type == GPU7_CF_INST_MEM_STREAM1_WRITE)
		{
			// no effect on stack depth
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_MEM_RING_WRITE)
		{
			// no effect on stack depth
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_EMIT_VERTEX)
		{
			// no effect on stack depth
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else if (cfInstruction.type == GPU7_CF_INST_CALL)
		{
			// no effect on stack depth
			cfInstruction.activeStackDepth = cfCurrentStackDepth;
		}
		else
		{
			cemu_assert_debug(false);
		}
	}
	shaderContext->analyzer.activeStackMaxDepth = cfCurrentMaxStackDepth;
	if (cfCurrentStackDepth != 0)
	{
		debug_printf("cfCurrentStackDepth is not zero after all CF instructions. depth is %d\n", cfCurrentStackDepth);
		cemu_assert_debug(false);
	}
	if(list_subroutineAddrs.empty() == false)
		cemuLog_logDebug(LogType::Force, "Todo - analyze shader subroutine CF stack");
	// TF mode
	if (shaderContext->options->useTFViaSSBO && shaderContext->output->streamoutBufferWriteMask.any())
	{
		shaderContext->analyzer.useSSBOForStreamout = true;
	}
	// assign binding points
	if (shaderContext->shaderType == LatteConst::ShaderType::Vertex)
		shaderContext->output->resourceMappingVK.setIndex = 0;
	else if (shaderContext->shaderType == LatteConst::ShaderType::Pixel)
		shaderContext->output->resourceMappingVK.setIndex = 1;
	else if (shaderContext->shaderType == LatteConst::ShaderType::Geometry)
		shaderContext->output->resourceMappingVK.setIndex = 2;
	LatteDecompiler::_initTextureBindingPointsGL(shaderContext);
	LatteDecompiler::_initTextureBindingPointsVK(shaderContext);
	LatteDecompiler::_initUniformBindingPoints(shaderContext);
	LatteDecompiler::_initAttributeBindingPoints(shaderContext);
}
