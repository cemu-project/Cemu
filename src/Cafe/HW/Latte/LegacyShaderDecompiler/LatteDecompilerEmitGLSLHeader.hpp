#pragma once

namespace LatteDecompiler
{
	void _emitUniformVariables(LatteDecompilerShaderContext* decompilerContext, RendererAPI rendererType, LatteDecompilerOutputUniformOffsets& uniformOffsets)
	{
		LatteDecompilerShaderResourceMapping& resourceMapping = (rendererType == RendererAPI::Vulkan) ? decompilerContext->output->resourceMappingVK : decompilerContext->output->resourceMappingGL;

		if (rendererType == RendererAPI::Vulkan)
		{
			// for Vulkan uniform vars are in a uniform buffer
			if (decompilerContext->hasUniformVarBlock)
			{
				cemu_assert_debug(resourceMapping.uniformVarsBufferBindingPoint >= 0);
				decompilerContext->shaderSource->addFmt("layout(set = {}, binding = {}) uniform ufBlock" _CRLF "{{" _CRLF, (sint32)resourceMapping.setIndex, (sint32)resourceMapping.uniformVarsBufferBindingPoint);
			}
		}

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
			if (rendererType == RendererAPI::OpenGL)
			{
				uniformCurrentOffset = (uniformCurrentOffset + 7)&~7;
				shaderSrc->add("uniform vec2 uf_fragCoordScale;" _CRLF);
				uniformOffsets.offset_fragCoordScale = uniformCurrentOffset;
				uniformCurrentOffset += 8;
			}
			else
			{
				// in Vulkan uf_fragCoordScale stores the origin in zw
				uniformCurrentOffset = (uniformCurrentOffset + 15)&~15;
				shaderSrc->add("uniform vec4 uf_fragCoordScale;" _CRLF);
				uniformOffsets.offset_fragCoordScale = uniformCurrentOffset;
				uniformCurrentOffset += 16;
			}
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
		if (rendererType == RendererAPI::Vulkan)
		{
			if (decompilerContext->hasUniformVarBlock)
				shaderSrc->add("};" _CRLF); // end of push-constant block
		}
	}

	void _emitUniformBuffers(LatteDecompilerShaderContext* decompilerContext)
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

				shaderSrc->addFmt("uniform {}{}" _CRLF, _getShaderUniformBlockInterfaceName(decompilerContext->shaderType), i);
				shaderSrc->add("{" _CRLF);
				shaderSrc->addFmt("vec4 {}{}[{}];" _CRLF, _getShaderUniformBlockVariableName(decompilerContext->shaderType), i, decompilerContext->analyzer.uniformBufferAccessTracker[i].DetermineSize(decompilerContext->shaderBaseHash, LATTE_GLSL_DYNAMIC_UNIFORM_BLOCK_SIZE));
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

	void _emitTextureDefinitions(LatteDecompilerShaderContext* shaderContext)
	{
		auto src = shaderContext->shaderSource;
		// texture sampler definition
		for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
		{
			if (!shaderContext->output->textureUnitMask[i])
				continue;

			src->addFmt("TEXTURE_LAYOUT({}, {}, {}) ", (sint32)shaderContext->output->resourceMappingGL.textureUnitToBindingPoint[i], (sint32)shaderContext->output->resourceMappingVK.setIndex, (sint32)shaderContext->output->resourceMappingVK.textureUnitToBindingPoint[i]);

			src->add("uniform ");
			if (shaderContext->shader->textureIsIntegerFormat[i])
			{
				// integer samplers
				if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_1D)
					src->add("usampler1D");
				else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D || shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D_MSAA)
					src->add("usampler2D");
				else
					cemu_assert_unimplemented();
			}
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D || shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D_MSAA)
				src->add("sampler2D");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_1D)
				src->add("sampler1D");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D_ARRAY)
				src->add("sampler2DArray");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_CUBEMAP)
				src->add("samplerCubeArray");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_3D)
				src->add("sampler3D");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_1D)
			{
				cemu_assert_unimplemented();
				src->add("sampler2D");
			}
			else
			{
				cemu_assert_unimplemented();
			}
			if (shaderContext->shader->textureUsesDepthCompare[i])
				src->add("Shadow"); // shadow sampler

			src->addFmt(" {}{};", _getTextureUnitVariablePrefixName(shaderContext->shaderType), i);
			
			src->add(_CRLF);
		}
	}

	void _emitAttributes(LatteDecompilerShaderContext* decompilerContext)
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

	void _emitHeaderMacros(LatteDecompilerShaderContext* decompilerContext)
	{
		auto src = decompilerContext->shaderSource;
		// OpenGL/Vulkan ifdefs
		src->add("#ifdef VULKAN" _CRLF);
		// Vulkan defines
		src->add("#define ATTR_LAYOUT(__vkSet, __location) layout(set = __vkSet, location = __location)" _CRLF);
		src->add("#define UNIFORM_BUFFER_LAYOUT(__glLocation, __vkSet, __vkLocation) layout(set = __vkSet, binding = __vkLocation, std140)" _CRLF);
		src->add("#define TEXTURE_LAYOUT(__glLocation, __vkSet, __vkLocation) layout(set = __vkSet, binding = __vkLocation)" _CRLF);
		if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex || decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
		{
			src->add("#define gl_VertexID gl_VertexIndex" _CRLF);
			src->add("#define gl_InstanceID gl_InstanceIndex" _CRLF);
			if (decompilerContext->analyzer.hasStreamoutWrite)
				src->add("#define XFB_BLOCK_LAYOUT(__bufferIndex, __stride, __location) layout(location = __location, xfb_buffer = __bufferIndex, xfb_stride = __stride, xfb_offset = 0)" _CRLF);

			if (decompilerContext->contextRegistersNew->PA_CL_CLIP_CNTL.get_DX_CLIP_SPACE_DEF())
			{
				src->add("#define SET_POSITION(_v) gl_Position = _v" _CRLF);
			}
			else
			{
				src->add("#define SET_POSITION(_v) gl_Position = _v; gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0" _CRLF);
			}

			if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex || decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
			{
				if (decompilerContext->options->usesGeometryShader)
					src->add("#define V2G_LAYOUT layout(location = 0)" _CRLF);
			}
		}
		else if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
		{
			src->add("#define GET_FRAGCOORD() vec4(gl_FragCoord.xy*uf_fragCoordScale.xy,gl_FragCoord.z, 1.0/gl_FragCoord.w)" _CRLF);
		}
		if (decompilerContext->options->spirvInstrinsics.hasRoundingModeRTEFloat32)
		{
			src->add("#extension GL_EXT_spirv_intrinsics: enable" _CRLF);
			// set RoundingModeRTE
			src->add("spirv_execution_mode(4462, 16);" _CRLF);
			src->add("spirv_execution_mode(4462, 32);" _CRLF);
			src->add("spirv_execution_mode(4462, 64);" _CRLF);
		}
		src->add("#else" _CRLF);
		// OpenGL defines
		src->add("#define ATTR_LAYOUT(__vkSet, __location) layout(location = __location)" _CRLF);
		src->add("#define UNIFORM_BUFFER_LAYOUT(__glLocation, __vkSet, __vkLocation) layout(binding = __glLocation, std140) " _CRLF);
		src->add("#define TEXTURE_LAYOUT(__glLocation, __vkSet, __vkLocation) layout(binding = __glLocation)" _CRLF);
		if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex || decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
		{
			if (decompilerContext->analyzer.hasStreamoutWrite)
				src->add("#define XFB_BLOCK_LAYOUT(__bufferIndex, __stride, __location) layout(xfb_buffer = __bufferIndex, xfb_stride = __stride)" _CRLF);

			src->add("#define SET_POSITION(_v) gl_Position = _v\r\n");
			if (decompilerContext->options->usesGeometryShader)
				src->add("#define V2G_LAYOUT" _CRLF);
		}
		else if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
		{
			src->add("#define GET_FRAGCOORD() vec4(gl_FragCoord.xy*uf_fragCoordScale,gl_FragCoord.zw)" _CRLF);
		}
		src->add("#endif" _CRLF);

		// geometry shader input/output primitive info
		if (decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
		{
			auto drawPrimitiveMode = decompilerContext->contextRegistersNew->VGT_PRIMITIVE_TYPE.get_PRIMITIVE_MODE();
			const char* gsInputPrimitive = "";
			if (drawPrimitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::POINTS)
				gsInputPrimitive = "points";
			else if (drawPrimitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::TRIANGLES)
				gsInputPrimitive = "triangles";
			else if (drawPrimitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::LINE_STRIP)
				gsInputPrimitive = "lines_adjacency";
			else
			{
				debug_printf("drawPrimitiveMode %d\n", drawPrimitiveMode);
				cemu_assert_debug(false);
			}
			// note: The input primitive type is not stored with the geometry shader object (and registers). Therefore we have to rely on whatever primitive mode was set for the draw call.
			src->addFmt("layout({}) in;" _CRLF, gsInputPrimitive);

			uint32 gsOutPrimType = decompilerContext->contextRegisters[mmVGT_GS_OUT_PRIM_TYPE];
			uint32 bytesPerVertex = decompilerContext->contextRegisters[mmSQ_GS_VERT_ITEMSIZE] * 4;
			
			uint32 maxVerticesInGS = ((decompilerContext->contextRegisters[mmSQ_GSVS_RING_ITEMSIZE] & 0x7FFF) * 4) / bytesPerVertex;

			if (!decompilerContext->analyzer.hasLoops && decompilerContext->analyzer.numEmitVertex < maxVerticesInGS)
				maxVerticesInGS = decompilerContext->analyzer.numEmitVertex;

			src->add("layout (");
			if (gsOutPrimType == 0)
				src->add("points");
			else if (gsOutPrimType == 1)
				src->add("line_strip");
			else if (gsOutPrimType == 2)
				src->add("triangle_strip");
			else
			{
				cemu_assert_debug(false);
			}
			src->addFmt(", max_vertices={}) out;" _CRLF, (sint32)maxVerticesInGS);
		}
	}

	void _emitVSExports(LatteDecompilerShaderContext* shaderContext)
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

	void _emitPSImports(LatteDecompilerShaderContext* shaderContext)
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

	void _emitMisc(LatteDecompilerShaderContext* decompilerContext)
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

	void emitHeader(LatteDecompilerShaderContext* decompilerContext)
	{
		const bool dump_shaders_enabled = ActiveSettings::DumpShadersEnabled();
		if(dump_shaders_enabled)
			decompilerContext->shaderSource->add("// start of shader inputs/outputs, predetermined by Cemu. Do not touch" _CRLF);
		// macros
		_emitHeaderMacros(decompilerContext);
		// uniform variables
		decompilerContext->shaderSource->add("#ifdef VULKAN" _CRLF);
		_emitUniformVariables(decompilerContext, RendererAPI::Vulkan, decompilerContext->output->uniformOffsetsVK);
		decompilerContext->shaderSource->add("#else" _CRLF);
		_emitUniformVariables(decompilerContext, RendererAPI::OpenGL, decompilerContext->output->uniformOffsetsGL);
		decompilerContext->shaderSource->add("#endif" _CRLF);
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
