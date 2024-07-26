#pragma once

namespace LatteDecompiler
{
	static void _emitUniformVariables(LatteDecompilerShaderContext* decompilerContext, LatteDecompilerOutputUniformOffsets& uniformOffsets)
	{
		LatteDecompilerShaderResourceMapping& resourceMapping = decompilerContext->output->resourceMappingVK;

		sint32 uniformCurrentOffset = 0;
		auto shader = decompilerContext->shader;
		auto shaderType = decompilerContext->shader->shaderType;
		auto shaderSrc = decompilerContext->shaderSource;
		if (decompilerContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_REMAPPED)
		{
			// uniform registers or buffers are accessed statically with predictable offsets
			// this allows us to remap the used entries into a more compact array
			if (shaderType == LatteConst::ShaderType::Vertex)
				shaderSrc->addFmt("uniform ivec4 uf_remappedVS[{}];" _CRLF, (sint32)shader->list_remappedUniformEntries.size());
			else if (shaderType == LatteConst::ShaderType::Pixel)
				shaderSrc->addFmt("uniform ivec4 uf_remappedPS[{}];" _CRLF, (sint32)shader->list_remappedUniformEntries.size());
			else if (shaderType == LatteConst::ShaderType::Geometry)
				shaderSrc->addFmt("uniform ivec4 uf_remappedGS[{}];" _CRLF, (sint32)shader->list_remappedUniformEntries.size());
			else
				debugBreakpoint();
			uniformOffsets.offset_remapped = uniformCurrentOffset;
			uniformCurrentOffset += 16 * shader->list_remappedUniformEntries.size();
		}
		else if (decompilerContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CFILE)
		{
			uint32 cfileSize = decompilerContext->analyzer.uniformRegisterAccessTracker.DetermineSize(decompilerContext->shaderBaseHash, 256);
			// full or partial uniform register file has to be present
			if (shaderType == LatteConst::ShaderType::Vertex)
				shaderSrc->addFmt("uniform ivec4 uf_uniformRegisterVS[{}];" _CRLF, cfileSize);
			else if (shaderType == LatteConst::ShaderType::Pixel)
				shaderSrc->addFmt("uniform ivec4 uf_uniformRegisterPS[{}];" _CRLF, cfileSize);
			else if (shaderType == LatteConst::ShaderType::Geometry)
				shaderSrc->addFmt("uniform ivec4 uf_uniformRegisterGS[{}];" _CRLF, cfileSize);
			uniformOffsets.offset_uniformRegister = uniformCurrentOffset;
			uniformOffsets.count_uniformRegister = cfileSize;
			uniformCurrentOffset += 16 * cfileSize;
		}
		// special uniforms
		bool hasAnyViewportScaleDisabled =
			!decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_X_SCALE_ENA() ||
			!decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_Y_SCALE_ENA() ||
			!decompilerContext->contextRegistersNew->PA_CL_VTE_CNTL.get_VPORT_Z_SCALE_ENA();

		if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex && hasAnyViewportScaleDisabled)
		{
			// aka GX2 special state 0
			uniformCurrentOffset = (uniformCurrentOffset + 7)&~7;
			shaderSrc->add("uniform vec2 uf_windowSpaceToClipSpaceTransform;" _CRLF);
			uniformOffsets.offset_windowSpaceToClipSpaceTransform = uniformCurrentOffset;
			uniformCurrentOffset += 8;
		}
		bool alphaTestEnable = decompilerContext->contextRegistersNew->SX_ALPHA_TEST_CONTROL.get_ALPHA_TEST_ENABLE();
		if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel && alphaTestEnable)
		{
			uniformCurrentOffset = (uniformCurrentOffset + 3)&~3;
			shaderSrc->add("uniform float uf_alphaTestRef;" _CRLF);
			uniformOffsets.offset_alphaTestRef = uniformCurrentOffset;
			uniformCurrentOffset += 4;
		}
		if (decompilerContext->analyzer.outputPointSize && decompilerContext->analyzer.writesPointSize == false)
		{
			if ((decompilerContext->shaderType == LatteConst::ShaderType::Vertex && !decompilerContext->options->usesGeometryShader) ||
				decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
			{
				uniformCurrentOffset = (uniformCurrentOffset + 3)&~3;
				shaderSrc->add("uniform float uf_pointSize;" _CRLF);
				uniformOffsets.offset_pointSize = uniformCurrentOffset;
				uniformCurrentOffset += 4;
			}
		}
		// define uf_fragCoordScale which holds the xy scale for render target resolution vs effective resolution
		if (shader->shaderType == LatteConst::ShaderType::Pixel)
		{
			uniformCurrentOffset = (uniformCurrentOffset + 7)&~7;
			shaderSrc->add("uniform vec2 uf_fragCoordScale;" _CRLF);
			uniformOffsets.offset_fragCoordScale = uniformCurrentOffset;
			uniformCurrentOffset += 8;
		}
		// provide scale factor for every texture that is accessed via texel coordinates (texelFetch)
		for (sint32 t = 0; t < LATTE_NUM_MAX_TEX_UNITS; t++)
		{
			if (decompilerContext->analyzer.texUnitUsesTexelCoordinates.test(t) == false)
				continue;
			uniformCurrentOffset = (uniformCurrentOffset + 7) & ~7;
			shaderSrc->addFmt("uniform vec2 uf_tex{}Scale;" _CRLF, t);
			uniformOffsets.offset_texScale[t] = uniformCurrentOffset;
			uniformCurrentOffset += 8;
		}
		// define uf_verticesPerInstance + uf_streamoutBufferBaseX
		if (decompilerContext->analyzer.useSSBOForStreamout &&
			(shader->shaderType == LatteConst::ShaderType::Vertex && decompilerContext->options->usesGeometryShader == false) ||
			(shader->shaderType == LatteConst::ShaderType::Geometry) )
		{
			shaderSrc->add("uniform int uf_verticesPerInstance;" _CRLF);
			uniformOffsets.offset_verticesPerInstance = uniformCurrentOffset;
			uniformCurrentOffset += 4;
			for (uint32 i = 0; i < LATTE_NUM_STREAMOUT_BUFFER; i++)
			{
				if (decompilerContext->output->streamoutBufferWriteMask[i])
				{
					shaderSrc->addFmt("uniform int uf_streamoutBufferBase{};" _CRLF, i);
					uniformOffsets.offset_streamoutBufferBase[i] = uniformCurrentOffset;
					uniformCurrentOffset += 4;
				}
			}
		}

		uniformOffsets.offset_endOfBlock = uniformCurrentOffset;
	}

	static void _emitUniformBuffers(LatteDecompilerShaderContext* decompilerContext)
	{
		auto shaderSrc = decompilerContext->shaderSource;
		// uniform buffer definition
		if (decompilerContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK)
		{
			for (uint32 i = 0; i < LATTE_NUM_MAX_UNIFORM_BUFFERS; i++)
			{
				if (!decompilerContext->analyzer.uniformBufferAccessTracker[i].HasAccess())
					continue;

				cemu_assert_debug(decompilerContext->output->resourceMappingGL.uniformBuffersBindingPoint[i] >= 0);
				cemu_assert_debug(decompilerContext->output->resourceMappingVK.uniformBuffersBindingPoint[i] >= 0);

				shaderSrc->addFmt("UNIFORM_BUFFER_LAYOUT({}, {}, {}) ", (sint32)decompilerContext->output->resourceMappingGL.uniformBuffersBindingPoint[i], (sint32)decompilerContext->output->resourceMappingVK.setIndex, (sint32)decompilerContext->output->resourceMappingVK.uniformBuffersBindingPoint[i]);

				shaderSrc->addFmt("uniform ubuff{}" _CRLF, i);
				shaderSrc->add("{" _CRLF);
				shaderSrc->addFmt("float4 ubuff{}[{}];" _CRLF, i, decompilerContext->analyzer.uniformBufferAccessTracker[i].DetermineSize(decompilerContext->shaderBaseHash, LATTE_GLSL_DYNAMIC_UNIFORM_BLOCK_SIZE));
				shaderSrc->add("};" _CRLF _CRLF);
				shaderSrc->add(_CRLF);
			}
		}
		else if (decompilerContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_REMAPPED)
		{
			// already generated in _emitUniformVariables
		}
		else if (decompilerContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CFILE)
		{
			// already generated in _emitUniformVariables
		}
		else if (decompilerContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_NONE)
		{
			// no uniforms used
		}
		else
		{
			cemu_assert_debug(false);
		}
	}

	static void _emitTextureDefinitions(LatteDecompilerShaderContext* shaderContext)
	{
		auto src = shaderContext->shaderSource;
		// texture sampler definition
		for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
		{
			if (!shaderContext->output->textureUnitMask[i])
				continue;

			if (shaderContext->shader->textureIsIntegerFormat[i])
			{
				// integer samplers
				if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_1D)
					src->add("texture1d<uint>");
				else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D || shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D_MSAA)
					src->add("texture2d<uint>");
				else
					cemu_assert_unimplemented();
			}
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D || shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D_MSAA)
				src->add("texture2d<float>");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_1D)
				src->add("texture1d<float>");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D_ARRAY)
				src->add("texture2d_array<float>");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_CUBEMAP)
				src->add("texturecube_array<float>");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_3D)
				src->add("texture3d<float>");
			else
			{
				cemu_assert_unimplemented();
			}

			src->addFmt(" tex{} [[texture({})]], ", i, shaderContext->output->resourceMappingGL.textureUnitToBindingPoint[i]);
			src->addFmt("sampler samplr{} [[sampler({})]], ", i, shaderContext->output->resourceMappingGL.textureUnitToBindingPoint[i]);
		}
	}

	static void _emitAttributes(LatteDecompilerShaderContext* decompilerContext)
	{
		auto shaderSrc = decompilerContext->shaderSource;
		if (decompilerContext->shader->shaderType == LatteConst::ShaderType::Vertex)
		{
			// attribute inputs
			for (uint32 i = 0; i < LATTE_NUM_MAX_ATTRIBUTE_LOCATIONS; i++)
			{
				if (decompilerContext->analyzer.inputAttributSemanticMask[i])
				{
					cemu_assert_debug(decompilerContext->output->resourceMappingGL.attributeMapping[i] >= 0);
					cemu_assert_debug(decompilerContext->output->resourceMappingVK.attributeMapping[i] >= 0);
					cemu_assert_debug(decompilerContext->output->resourceMappingGL.attributeMapping[i] == decompilerContext->output->resourceMappingVK.attributeMapping[i]);

					shaderSrc->addFmt("ATTR_LAYOUT({}, {}) in uvec4 attrDataSem{};" _CRLF, (sint32)decompilerContext->output->resourceMappingVK.setIndex, (sint32)decompilerContext->output->resourceMappingVK.attributeMapping[i], i);
				}
			}
		}
	}

	static void _emitVSExports(LatteDecompilerShaderContext* shaderContext)
	{
		auto* src = shaderContext->shaderSource;
		LatteShaderPSInputTable* psInputTable = LatteSHRC_GetPSInputTable();
		auto parameterMask = shaderContext->shader->outputParameterMask;
		for (uint32 i = 0; i < 32; i++)
		{
			if ((parameterMask&(1 << i)) == 0)
				continue;
			uint32 vsSemanticId = _getVertexShaderOutParamSemanticId(shaderContext->contextRegisters, i);
			if (vsSemanticId > LATTE_ANALYZER_IMPORT_INDEX_PARAM_MAX)
				continue;
			// get import based on semanticId
			sint32 psInputIndex = -1;
			for (sint32 f = 0; f < psInputTable->count; f++)
			{
				if (psInputTable->import[f].semanticId == vsSemanticId)
				{
					psInputIndex = f;
					break;
				}
			}
			if (psInputIndex == -1)
				continue; // no ps input

			src->addFmt("layout(location = {}) ", psInputIndex);
			if (psInputTable->import[psInputIndex].isFlat)
				src->add("flat ");
			if (psInputTable->import[psInputIndex].isNoPerspective)
				src->add("noperspective ");
			src->add("out");
			src->addFmt(" vec4 passParameterSem{};" _CRLF, psInputTable->import[psInputIndex].semanticId);
		}
	}

	static void _emitPSImports(LatteDecompilerShaderContext* shaderContext)
	{
		auto* src = shaderContext->shaderSource;
		LatteShaderPSInputTable* psInputTable = LatteSHRC_GetPSInputTable();
		for (sint32 i = 0; i < psInputTable->count; i++)
		{
			if (psInputTable->import[i].semanticId > LATTE_ANALYZER_IMPORT_INDEX_PARAM_MAX)
				continue;
			src->addFmt("layout(location = {}) ", i);
			if (psInputTable->import[i].isFlat)
				src->add("flat ");
			if (psInputTable->import[i].isNoPerspective)
				src->add("noperspective ");
			src->add("in");
			src->addFmt(" vec4 passParameterSem{};" _CRLF, psInputTable->import[i].semanticId);
		}
	}

	static void _emitMisc(LatteDecompilerShaderContext* decompilerContext)
	{
		auto src = decompilerContext->shaderSource;
		// per-vertex output (VS or GS)
		if ((decompilerContext->shaderType == LatteConst::ShaderType::Vertex && !decompilerContext->options->usesGeometryShader) ||
			(decompilerContext->shaderType == LatteConst::ShaderType::Geometry))
		{
			src->add("out gl_PerVertex" _CRLF);
			src->add("{" _CRLF);
			src->add("	vec4 gl_Position;" _CRLF);
			if (decompilerContext->analyzer.outputPointSize)
				src->add("	float gl_PointSize;" _CRLF);
			src->add("};" _CRLF);
		}
		// varyings (variables passed from vertex to pixel shader, only if geometry stage is disabled
		if (decompilerContext->options->usesGeometryShader == false)
		{
			if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex)
			{
				_emitVSExports(decompilerContext);
			}
			else if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
			{
				_emitPSImports(decompilerContext);
			}
		}
		else
		{
			if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex || decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
			{
				// parameters shared between vertex shader and geometry shader
				src->add("V2G_LAYOUT ");

				if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex)
					src->add("out Vertex" _CRLF);
				else if (decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
					src->add("in Vertex" _CRLF);
				src->add("{" _CRLF);
				uint32 ringParameterCountVS2GS = 0;
				if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex)
				{
					ringParameterCountVS2GS = decompilerContext->shader->ringParameterCount;
				}
				else
				{
					ringParameterCountVS2GS = decompilerContext->shader->ringParameterCountFromPrevStage;
				}
				for (uint32 f = 0; f < ringParameterCountVS2GS; f++)
					src->addFmt(" ivec4 passV2GParameter{};" _CRLF, f);
				if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex)
					src->add("}v2g;" _CRLF);
				else if (decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
					src->add("}v2g[];" _CRLF);
			}
			if (decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
			{
				// parameters shared between geometry and pixel shader
				uint32 ringItemSize = decompilerContext->contextRegisters[mmSQ_GSVS_RING_ITEMSIZE] & 0x7FFF;
				if ((ringItemSize & 0xF) != 0)
					debugBreakpoint();
				if (((decompilerContext->contextRegisters[mmSQ_GSVS_RING_ITEMSIZE] & 0x7FFF) & 0xF) != 0)
					debugBreakpoint();

				for (sint32 p = 0; p < decompilerContext->parsedGSCopyShader->numParam; p++)
				{
					if (decompilerContext->parsedGSCopyShader->paramMapping[p].exportType != 2)
						continue;
					src->addFmt("layout(location = {}) out vec4 passG2PParameter{};" _CRLF, decompilerContext->parsedGSCopyShader->paramMapping[p].exportParam & 0x7F, (sint32)decompilerContext->parsedGSCopyShader->paramMapping[p].exportParam);
				}
			}
			else if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
			{
				// pixel shader with geometry shader
				LatteShaderPSInputTable* psInputTable = LatteSHRC_GetPSInputTable();
				for (sint32 i = 0; i < psInputTable->count; i++)
				{
					if (psInputTable->import[i].semanticId > LATTE_ANALYZER_IMPORT_INDEX_PARAM_MAX)
						continue;
					uint32 location = psInputTable->import[i].semanticId & 0x7F; // todo - the range above 128 has special meaning?

					src->addFmt("layout(location = {}) ", location);
					if (psInputTable->import[i].isFlat)
						src->add("flat ");
					if (psInputTable->import[i].isNoPerspective)
						src->add("noperspective ");
					if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex)
						src->add("out");
					else if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
						src->add("in");
					else
						debugBreakpoint();

					src->addFmt(" vec4 passG2PParameter{};" _CRLF, (sint32)location);
				}
			}
		}
		// output defines
		if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
		{
			// generate pixel outputs for pixel shader
			for (uint32 i = 0; i < LATTE_NUM_COLOR_TARGET; i++)
			{
				if ((decompilerContext->shader->pixelColorOutputMask&(1 << i)) != 0)
				{
					src->addFmt("layout(location = {}) out vec4 passPixelColor{};" _CRLF, i, i);
				}
			}
		}
		// streamout buffer (transform feedback)
		if ((decompilerContext->shaderType == LatteConst::ShaderType::Vertex || decompilerContext->shaderType == LatteConst::ShaderType::Geometry) && decompilerContext->analyzer.hasStreamoutEnable)
		{
			if (decompilerContext->options->useTFViaSSBO)
			{
				if (decompilerContext->analyzer.useSSBOForStreamout && decompilerContext->analyzer.hasStreamoutWrite)
				{
					src->addFmt("layout(set = {}, binding = {}) buffer StreamoutBuffer" _CRLF, decompilerContext->output->resourceMappingVK.setIndex, decompilerContext->output->resourceMappingVK.getTFStorageBufferBindingPoint());
					src->add("{" _CRLF);
					src->add("int sb_buffer[];" _CRLF);
					src->add("};" _CRLF);
				}
			}
			else
			{
				sint32 locationOffset = 0; // glslang wants a location for xfb outputs
				for (uint32 i = 0; i < LATTE_NUM_STREAMOUT_BUFFER; i++)
				{
					if (!decompilerContext->output->streamoutBufferWriteMask[i])
						continue;
					uint32 bufferStride = decompilerContext->output->streamoutBufferStride[i];
					src->addFmt("XFB_BLOCK_LAYOUT({}, {}, {}) out XfbBlock{} " _CRLF, i, bufferStride, locationOffset, i);
					src->add("{" _CRLF);
					src->addFmt("layout(xfb_buffer = {}, xfb_offset = 0) int sb{}[{}];" _CRLF, i, i, decompilerContext->output->streamoutBufferStride[i] / 4);
					src->add("};" _CRLF);
					locationOffset += (decompilerContext->output->streamoutBufferStride[i] / 4);
				}
			}
		}
	}

	static void emitHeader(LatteDecompilerShaderContext* decompilerContext)
	{
		const bool dump_shaders_enabled = ActiveSettings::DumpShadersEnabled();
		if(dump_shaders_enabled)
			decompilerContext->shaderSource->add("// start of shader inputs/outputs, predetermined by Cemu. Do not touch" _CRLF);
		// uniform variables
		_emitUniformVariables(decompilerContext, decompilerContext->output->uniformOffsetsVK);
		// uniform buffers
		_emitUniformBuffers(decompilerContext);
		// textures
		_emitTextureDefinitions(decompilerContext);
		// attributes
		_emitAttributes(decompilerContext);
		// misc stuff
		_emitMisc(decompilerContext);

		if (dump_shaders_enabled)
			decompilerContext->shaderSource->add("// end of shader inputs/outputs" _CRLF);
	}
}
