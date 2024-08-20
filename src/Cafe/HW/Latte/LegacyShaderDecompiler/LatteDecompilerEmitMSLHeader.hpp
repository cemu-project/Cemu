#pragma once

#include "HW/Latte/Core/LatteConst.h"
namespace LatteDecompiler
{
	static void _emitUniformVariables(LatteDecompilerShaderContext* decompilerContext)
	{
	    auto src = decompilerContext->shaderSource;

		auto& uniformOffsets = decompilerContext->output->uniformOffsetsVK;

		src->add("struct SupportBuffer {" _CRLF);

		sint32 uniformCurrentOffset = 0;
		auto shader = decompilerContext->shader;
		auto shaderType = decompilerContext->shader->shaderType;
		if (decompilerContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_REMAPPED)
		{
			// uniform registers or buffers are accessed statically with predictable offsets
			// this allows us to remap the used entries into a more compact array
			if (shaderType == LatteConst::ShaderType::Vertex)
				src->addFmt("int4 remappedVS[{}];" _CRLF, (sint32)shader->list_remappedUniformEntries.size());
			else if (shaderType == LatteConst::ShaderType::Pixel)
				src->addFmt("int4 remappedPS[{}];" _CRLF, (sint32)shader->list_remappedUniformEntries.size());
			else if (shaderType == LatteConst::ShaderType::Geometry)
				src->addFmt("int4 remappedGS[{}];" _CRLF, (sint32)shader->list_remappedUniformEntries.size());
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
				src->addFmt("int4 uniformRegisterVS[{}];" _CRLF, cfileSize);
			else if (shaderType == LatteConst::ShaderType::Pixel)
				src->addFmt("int4 uniformRegisterPS[{}];" _CRLF, cfileSize);
			else if (shaderType == LatteConst::ShaderType::Geometry)
				src->addFmt("int4 uniformRegisterGS[{}];" _CRLF, cfileSize);
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
			src->add("float2 windowSpaceToClipSpaceTransform;" _CRLF);
			uniformOffsets.offset_windowSpaceToClipSpaceTransform = uniformCurrentOffset;
			uniformCurrentOffset += 8;
		}
		bool alphaTestEnable = decompilerContext->contextRegistersNew->SX_ALPHA_TEST_CONTROL.get_ALPHA_TEST_ENABLE();
		if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel && alphaTestEnable)
		{
			uniformCurrentOffset = (uniformCurrentOffset + 3)&~3;
			src->add("float alphaTestRef;" _CRLF);
			uniformOffsets.offset_alphaTestRef = uniformCurrentOffset;
			uniformCurrentOffset += 4;
		}
		if (decompilerContext->analyzer.outputPointSize && decompilerContext->analyzer.writesPointSize == false)
		{
			if ((decompilerContext->shaderType == LatteConst::ShaderType::Vertex && !decompilerContext->options->usesGeometryShader) ||
				decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
			{
				uniformCurrentOffset = (uniformCurrentOffset + 3)&~3;
				src->add("float pointSize;" _CRLF);
				uniformOffsets.offset_pointSize = uniformCurrentOffset;
				uniformCurrentOffset += 4;
			}
		}
		// define fragCoordScale which holds the xy scale for render target resolution vs effective resolution
		if (shader->shaderType == LatteConst::ShaderType::Pixel)
		{
			uniformCurrentOffset = (uniformCurrentOffset + 7)&~7;
			src->add("float2 fragCoordScale;" _CRLF);
			uniformOffsets.offset_fragCoordScale = uniformCurrentOffset;
			uniformCurrentOffset += 8;
		}
		// provide scale factor for every texture that is accessed via texel coordinates (texelFetch)
		for (sint32 t = 0; t < LATTE_NUM_MAX_TEX_UNITS; t++)
		{
			if (decompilerContext->analyzer.texUnitUsesTexelCoordinates.test(t) == false)
				continue;
			uniformCurrentOffset = (uniformCurrentOffset + 7) & ~7;
			src->addFmt("float2 tex{}Scale;" _CRLF, t);
			uniformOffsets.offset_texScale[t] = uniformCurrentOffset;
			uniformCurrentOffset += 8;
		}
		// define verticesPerInstance + streamoutBufferBaseX
		if ((shader->shaderType == LatteConst::ShaderType::Vertex && decompilerContext->options->usesGeometryShader == false) ||
			(shader->shaderType == LatteConst::ShaderType::Geometry))
		{
			src->add("int verticesPerInstance;" _CRLF);
			uniformOffsets.offset_verticesPerInstance = uniformCurrentOffset;
			uniformCurrentOffset += 4;
			for (uint32 i = 0; i < LATTE_NUM_STREAMOUT_BUFFER; i++)
			{
				if (decompilerContext->output->streamoutBufferWriteMask[i])
				{
					src->addFmt("int streamoutBufferBase{};" _CRLF, i);
					uniformOffsets.offset_streamoutBufferBase[i] = uniformCurrentOffset;
					uniformCurrentOffset += 4;
				}
			}
		}

		src->add("};" _CRLF _CRLF);

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

				cemu_assert_debug(decompilerContext->output->resourceMappingVK.uniformBuffersBindingPoint[i] >= 0);

				shaderSrc->addFmt("struct UBuff{} {{" _CRLF, i);
				shaderSrc->addFmt("float4 d[{}];" _CRLF, decompilerContext->analyzer.uniformBufferAccessTracker[i].DetermineSize(decompilerContext->shaderBaseHash, LATTE_GLSL_DYNAMIC_UNIFORM_BLOCK_SIZE));
				shaderSrc->add("};" _CRLF _CRLF);
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

	static void _emitAttributes(LatteDecompilerShaderContext* decompilerContext)
	{
		auto src = decompilerContext->shaderSource;

		if (decompilerContext->shader->shaderType == LatteConst::ShaderType::Vertex)
		{
		    src->add("struct VertexIn {" _CRLF);
			// attribute inputs
			for (uint32 i = 0; i < LATTE_NUM_MAX_ATTRIBUTE_LOCATIONS; i++)
			{
				if (decompilerContext->analyzer.inputAttributSemanticMask[i])
				{
					cemu_assert_debug(decompilerContext->output->resourceMappingVK.attributeMapping[i] >= 0);

					src->addFmt("uint4 attrDataSem{}", i);
					if (!decompilerContext->options->usesGeometryShader)
					    src->addFmt(" [[attribute({})]]", (sint32)decompilerContext->output->resourceMappingVK.attributeMapping[i]);
					src->add(";" _CRLF);
				}
			}
			src->add("};" _CRLF _CRLF);
		}
	}

	static void _emitVSOutputs(LatteDecompilerShaderContext* shaderContext)
	{
		auto* src = shaderContext->shaderSource;

		src->add("struct VertexOut {" _CRLF);
		src->add("float4 position [[position]];" _CRLF);
		if (shaderContext->analyzer.outputPointSize)
		    src->add("float pointSize [[point_size]];" _CRLF);

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

			src->addFmt("float4 passParameterSem{}", psInputTable->import[psInputIndex].semanticId);
 			src->addFmt(" [[user(locn{})]]", psInputIndex);
 			if (psInputTable->import[psInputIndex].isFlat)
				src->add(" [[flat]]");
 			if (psInputTable->import[psInputIndex].isNoPerspective)
				src->add(" [[center_no_perspective]]");
			src->addFmt(";" _CRLF);
		}

		src->add("};" _CRLF _CRLF);
	}

	static void _emitPSInputs(LatteDecompilerShaderContext* shaderContext)
	{
		auto* src = shaderContext->shaderSource;

		src->add("#define GET_FRAGCOORD() float4(in.position.xy * supportBuffer.fragCoordScale.xy, in.position.z, 1.0 / in.position.w)" _CRLF);

		src->add("struct FragmentIn {" _CRLF);
		src->add("float4 position [[position]];" _CRLF);

		LatteShaderPSInputTable* psInputTable = LatteSHRC_GetPSInputTable();
		for (sint32 i = 0; i < psInputTable->count; i++)
		{
			if (psInputTable->import[i].semanticId > LATTE_ANALYZER_IMPORT_INDEX_PARAM_MAX)
				continue;
			src->addFmt("float4 passParameterSem{}", psInputTable->import[i].semanticId);
			src->addFmt(" [[user(locn{})]]", i);
			if (psInputTable->import[i].isFlat)
				src->add(" [[flat]]");
			if (psInputTable->import[i].isNoPerspective)
				src->add(" [[center_no_perspective]]");
			src->add(";" _CRLF);
		}

		src->add("};" _CRLF _CRLF);
	}

	static void _emitInputsAndOutputs(LatteDecompilerShaderContext* decompilerContext)
	{
		auto src = decompilerContext->shaderSource;

		if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex)
		{
		    _emitAttributes(decompilerContext);
		}
		else if (decompilerContext->shaderType == LatteConst::ShaderType::Pixel)
		{
		    _emitPSInputs(decompilerContext);

			src->add("struct FragmentOut {" _CRLF);

            // generate pixel outputs for pixel shader
            for (uint32 i = 0; i < LATTE_NUM_COLOR_TARGET; i++)
            {
               	if ((decompilerContext->shader->pixelColorOutputMask & (1 << i)) != 0)
               	{
                    src->addFmt("#ifdef {}" _CRLF, GetColorAttachmentTypeStr(i));
              		src->addFmt("{} passPixelColor{} [[color({})]];" _CRLF, GetColorAttachmentTypeStr(i), i, i);
                    src->add("#endif" _CRLF);
               	}
            }

            // generate depth output for pixel shader
            if (decompilerContext->shader->depthWritten)
            {
                src->add("float passDepth [[depth(any)]];" _CRLF);
            }

            src->add("};" _CRLF _CRLF);
		}

		if (!decompilerContext->options->usesGeometryShader)
		{
    		if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex)
    			_emitVSOutputs(decompilerContext);
		}
		else
		{
            if (decompilerContext->shaderType == LatteConst::ShaderType::Vertex || decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
    		{
                src->add("struct VertexOut {" _CRLF);
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
    				src->addFmt("int4 passParameterSem{};" _CRLF, f);
    			src->add("};" _CRLF _CRLF);
                src->add("struct ObjectPayload {" _CRLF);
                src->add("VertexOut vertexOut[VERTICES_PER_PRIMITIVE];" _CRLF);
                src->add("};" _CRLF _CRLF);
    		}
    		if (decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
    		{
    			// parameters shared between geometry and pixel shader
    			uint32 ringItemSize = decompilerContext->contextRegisters[mmSQ_GSVS_RING_ITEMSIZE] & 0x7FFF;
    			if ((ringItemSize & 0xF) != 0)
    				debugBreakpoint();
    			if (((decompilerContext->contextRegisters[mmSQ_GSVS_RING_ITEMSIZE] & 0x7FFF) & 0xF) != 0)
    				debugBreakpoint();

                src->add("struct GeometryOut {" _CRLF);
                src->add("float4 position [[position]];" _CRLF);
    			for (sint32 p = 0; p < decompilerContext->parsedGSCopyShader->numParam; p++)
    			{
    				if (decompilerContext->parsedGSCopyShader->paramMapping[p].exportType != 2)
    					continue;
    				src->addFmt("float4 passParameterSem{} [[user(locn)]];" _CRLF, (sint32)decompilerContext->parsedGSCopyShader->paramMapping[p].exportParam, decompilerContext->parsedGSCopyShader->paramMapping[p].exportParam & 0x7F);
    			}
                src->add("};" _CRLF _CRLF);

                const uint32 MAX_PRIMITIVE_COUNT = 8;

                // Define the mesh shader output type
                src->addFmt("using MeshType = mesh<GeometryOut, void, {} * VERTICES_PER_PRIMITIVE, {}, topology::PRIMITIVE_TYPE>;" _CRLF, MAX_PRIMITIVE_COUNT, MAX_PRIMITIVE_COUNT);
    		}
		}
	}

	static void emitHeader(LatteDecompilerShaderContext* decompilerContext)
	{
		const bool dump_shaders_enabled = ActiveSettings::DumpShadersEnabled();
		if(dump_shaders_enabled)
			decompilerContext->shaderSource->add("// start of shader inputs/outputs, predetermined by Cemu. Do not touch" _CRLF);
		// uniform variables
		_emitUniformVariables(decompilerContext);
		// uniform buffers
		_emitUniformBuffers(decompilerContext);
		// inputs and outputs
		_emitInputsAndOutputs(decompilerContext);

		if (dump_shaders_enabled)
			decompilerContext->shaderSource->add("// end of shader inputs/outputs" _CRLF);
	}

	static void _emitUniformBufferDefinitions(LatteDecompilerShaderContext* decompilerContext)
	{
		auto src = decompilerContext->shaderSource;
		// uniform buffer definition
		if (decompilerContext->shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK)
		{
			for (uint32 i = 0; i < LATTE_NUM_MAX_UNIFORM_BUFFERS; i++)
			{
				if (!decompilerContext->analyzer.uniformBufferAccessTracker[i].HasAccess())
					continue;

				cemu_assert_debug(decompilerContext->output->resourceMappingVK.uniformBuffersBindingPoint[i] >= 0);

				src->addFmt(", constant UBuff{}& ubuff{} [[buffer({})]]", i, i, (sint32)decompilerContext->output->resourceMappingVK.uniformBuffersBindingPoint[i]);
			}
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

			src->add(", ");

			if (shaderContext->shader->textureUsesDepthCompare[i])
			    src->add("depth");
			else
                src->add("texture");

			if (shaderContext->shader->textureIsIntegerFormat[i])
			{
				// integer samplers
				if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_1D)
					src->add("1d<uint>");
				else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D || shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D_MSAA)
					src->add("2d<uint>");
				else
					cemu_assert_unimplemented();
			}
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D || shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D_MSAA)
				src->add("2d<float>");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_1D)
				src->add("1d<float>");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_2D_ARRAY)
				src->add("2d_array<float>");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_CUBEMAP)
				src->add("cube_array<float>");
			else if (shaderContext->shader->textureUnitDim[i] == Latte::E_DIM::DIM_3D)
				src->add("3d<float>");
			else
			{
				cemu_assert_unimplemented();
			}

			uint32 binding = shaderContext->output->resourceMappingVK.textureUnitToBindingPoint[i];
			//uint32 textureBinding = shaderContext->output->resourceMappingVK.textureUnitToBindingPoint[i] % 31;
			//uint32 samplerBinding = textureBinding % 16;
			src->addFmt(" tex{} [[texture({})]]", i, binding);
			src->addFmt(", sampler samplr{} [[sampler({})]]", i, binding);
		}
	}

	static void emitInputs(LatteDecompilerShaderContext* decompilerContext)
	{
	    auto src = decompilerContext->shaderSource;

		switch (decompilerContext->shaderType)
		{
		case LatteConst::ShaderType::Vertex:
		    if (!decompilerContext->options->usesGeometryShader)
                src->add("VertexIn in [[stage_in]], ");
            break;
        case LatteConst::ShaderType::Pixel:
            src->add("FragmentIn in [[stage_in]], ");
            break;
        default:
            break;
		}

		src->add("constant SupportBuffer& supportBuffer [[buffer(30)]]");
		switch (decompilerContext->shaderType)
		{
		case LatteConst::ShaderType::Vertex:
		    if (decompilerContext->options->usesGeometryShader)
			{
			    src->add(", object_data ObjectPayload& objectPayload [[payload]]");
			    src->add(", mesh_grid_properties meshGridProperties");
				src->add(", uint tig [[threadgroup_position_in_grid]]");
				src->add(", uint tid [[thread_index_in_threadgroup]]");
				src->add(", VERTEX_BUFFER_DEFINITIONS");
			}
			else
			{
                src->add(", uint vid [[vertex_id]]");
                src->add(", uint iid [[instance_id]]");
			}
            break;
        case LatteConst::ShaderType::Geometry:
            src->add(", MeshType mesh");
            src->add(", const object_data ObjectPayload& objectPayload [[payload]]");
            src->add(", uint tid [[thread_index_in_threadgroup]]");
            break;
        case LatteConst::ShaderType::Pixel:
            src->add(", bool frontFacing [[front_facing]]");
            break;
		}

        // streamout buffer (transform feedback)
        if ((decompilerContext->shaderType == LatteConst::ShaderType::Vertex && !decompilerContext->options->usesGeometryShader) || decompilerContext->shaderType == LatteConst::ShaderType::Geometry)
        {
            if (decompilerContext->analyzer.hasStreamoutEnable && decompilerContext->analyzer.hasStreamoutWrite)
                src->addFmt(", device int* sb [[buffer({})]]" _CRLF, decompilerContext->output->resourceMappingVK.tfStorageBindingPoint);
        }

		// uniform buffers
		_emitUniformBufferDefinitions(decompilerContext);
		// textures
		_emitTextureDefinitions(decompilerContext);
	}
}
