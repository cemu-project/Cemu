#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"

GLint _gl_remappedUniformData[4 * 256];

void OpenGLRenderer::uniformData_update()
{
	// update uniforms
	LatteDecompilerShader* shaderArray[3];
	shaderArray[0] = LatteSHRC_GetActiveVertexShader();
	shaderArray[1] = LatteSHRC_GetActivePixelShader();
	shaderArray[2] = LatteSHRC_GetActiveGeometryShader();;

	uint32 shaderBlockUniformRegisterOffset[3];
	shaderBlockUniformRegisterOffset[0] = mmSQ_VTX_UNIFORM_BLOCK_START;
	shaderBlockUniformRegisterOffset[1] = mmSQ_PS_UNIFORM_BLOCK_START;
	shaderBlockUniformRegisterOffset[2] = mmSQ_GS_UNIFORM_BLOCK_START;

	uint32 shaderALUConstOffset[3];
	shaderALUConstOffset[0] = 0x400;
	shaderALUConstOffset[1] = 0;
	shaderALUConstOffset[2] = 0xFFFFFFFF; // GS has no uniform registers

	for (sint32 s = 0; s < 3; s++)
	{
		// update block uniforms
		LatteDecompilerShader* shader = shaderArray[s];
		if (!shader)
			continue;

		auto hostShader = shader->shader;

		if (shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_REMAPPED)
		{
			auto& list_uniformMapping = shader->list_remappedUniformEntries;
			cemu_assert_debug(list_uniformMapping.size() <= 256);
			sint32 remappedArraySize = (sint32)list_uniformMapping.size();
			LatteBufferCache_LoadRemappedUniforms(shader, (float*)(_gl_remappedUniformData));
			// update values only when the hash changed
			if (remappedArraySize > 0)
			{
				uint64 uniformDataHash[2] = { 0 };
				uint64* remappedUniformData64 = (uint64*)_gl_remappedUniformData;
				for (sint32 f = 0; f < remappedArraySize; f++)
				{
					uniformDataHash[0] ^= remappedUniformData64[0];
					uniformDataHash[0] = std::rotl<uint64>(uniformDataHash[0], 11);
					uniformDataHash[1] ^= remappedUniformData64[1];
					uniformDataHash[1] = std::rotl<uint64>(uniformDataHash[1], 11);
					remappedUniformData64 += 2;
				}
				if (shader->uniformDataHash64[0] != uniformDataHash[0] || shader->uniformDataHash64[1] != uniformDataHash[1])
				{
					shader->uniformDataHash64[0] = uniformDataHash[0];
					shader->uniformDataHash64[1] = uniformDataHash[1];
					hostShader->SetUniform4iv(shader->uniform.loc_remapped, _gl_remappedUniformData, remappedArraySize);
				}
			}
		}
		else if (shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CFILE)
		{
			if (shaderALUConstOffset[s] == 0xFFFFFFFF)
				assert_dbg();
			GLint* uniformRegData = (GLint*)(LatteGPUState.contextRegister + mmSQ_ALU_CONSTANT0_0 + shaderALUConstOffset[s]);
			hostShader->SetUniform4iv(shader->uniform.loc_uniformRegister, uniformRegData, shader->uniform.count_uniformRegister);
		}
		else if (shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK)
		{
			// handled by _syncGPUUniformBuffers()
		}
		else if (shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_NONE)
		{
			// no uniforms used
		}
	}
}