#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Cafe/GameProfile/GameProfile.h"

#include "Cafe/HW/Latte/Core/LatteBufferCache.h"
#ifdef ENABLE_VULKAN
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#endif

template<int vectorLen>
void rectGenerate4thVertex(uint32be* output, uint32be* input0, uint32be* input1, uint32be* input2)
{
	float* v = (float*)output;

	for (sint32 i = 0; i < vectorLen; i++)
		output[vectorLen * 0 + i] = _swapEndianU32(input0[i]);
	for (sint32 i = 0; i < vectorLen; i++)
		output[vectorLen * 1 + i] = _swapEndianU32(input1[i]);
	for (sint32 i = 0; i < vectorLen; i++)
		output[vectorLen * 2 + i] = _swapEndianU32(input2[i]);

	float minX = std::min(v[vectorLen * 0 + 0], std::min(v[vectorLen * 1 + 0], v[vectorLen * 2 + 0]));
	float maxX = std::max(v[vectorLen * 0 + 0], std::max(v[vectorLen * 1 + 0], v[vectorLen * 2 + 0]));
	float minY = std::min(v[vectorLen * 0 + 1], std::min(v[vectorLen * 1 + 1], v[vectorLen * 2 + 1]));;
	float maxY = std::max(v[vectorLen * 0 + 1], std::max(v[vectorLen * 1 + 1], v[vectorLen * 2 + 1]));;

	float totalX = minX;
	totalX += maxY;
	float halfX = totalX / 2.0f;

	float totalY = minY;
	totalY += maxY;
	float halfY = totalY / 2.0f;

	sint32 countX =
		((v[vectorLen * 0 + 0] < halfX) ? 1 : 0) +
		((v[vectorLen * 1 + 0] < halfX) ? 1 : 0) +
		((v[vectorLen * 2 + 0] < halfX) ? 1 : 0);

	sint32 countY =
		((v[vectorLen * 0 + 1] < halfY) ? 1 : 0) +
		((v[vectorLen * 1 + 1] < halfY) ? 1 : 0) +
		((v[vectorLen * 2 + 1] < halfY) ? 1 : 0);

	if (countX < 2)
		v[vectorLen * 3 + 0] = minX;
	else
		v[vectorLen * 3 + 0] = maxX;
	if (countY < 2)
		v[vectorLen * 3 + 1] = minY;
	else
		v[vectorLen * 3 + 1] = maxY;

	if (vectorLen >= 3)
		v[vectorLen * 3 + 2] = v[vectorLen * 0 + 2]; // z from v0
	if (vectorLen >= 4)
		v[vectorLen * 3 + 3] = v[vectorLen * 0 + 3]; // w from v0

	// order of rectangle vertices is
	// v0 v1
	// v2 v3

	for (sint32 f = 0; f < vectorLen*4; f++)
		output[f] = _swapEndianU32(output[f]);
}

bool LatteBufferCache_LoadRemappedUniforms(LatteDecompilerShader* shader, float* uniformData, bool aluConstDirty, uint32 uniformBufferDirtyMask)
{
	bool hasChange = false;
	uint32 shaderAluConst;
	uint32 shaderUniformRegisterOffset;

	switch (shader->shaderType)
	{
	case LatteConst::ShaderType::Vertex:
		shaderAluConst = 0x400;
		shaderUniformRegisterOffset = mmSQ_VTX_UNIFORM_BLOCK_START;
		break;
	case LatteConst::ShaderType::Pixel:
		shaderAluConst = 0;
		shaderUniformRegisterOffset = mmSQ_PS_UNIFORM_BLOCK_START;
		break;
	case LatteConst::ShaderType::Geometry:
		shaderAluConst = 0; // geometry shader has no ALU const
		shaderUniformRegisterOffset = mmSQ_GS_UNIFORM_BLOCK_START;
		break;
	default:
		UNREACHABLE;
	}

	// sourced from uniform registers
	if (aluConstDirty)
	{
		uint32* aluConstBase = LatteGPUState.contextRegister + mmSQ_ALU_CONSTANT0_0 + shaderAluConst;
		for (auto it : shader->list_remappedUniformEntries_register)
		{
			uint64* __restrict uniformRegData = (uint64*)(aluConstBase + it.indexOffset / 4);
			uint64* __restrict regDest = (uint64*)((uint8*)uniformData + it.mappedIndexOffset);
			regDest[0] = uniformRegData[0];
			regDest[1] = uniformRegData[1];
		}
		if (!shader->list_remappedUniformEntries_register.empty())
			hasChange = true;
	}
	// sourced from uniform buffers
	if (uniformBufferDirtyMask)
	{
		for (auto& bufferGroup : shader->list_remappedUniformEntries_bufferGroups)
		{
			if ((uniformBufferDirtyMask&(1<<bufferGroup.bufferId)) == 0)
				continue;
			MPTR physicalAddr = LatteGPUState.contextRegister[shaderUniformRegisterOffset + bufferGroup.kcacheBankIdOffset / 4];
			if (physicalAddr)
			{
				uint8* __restrict uniformBase = memory_base + physicalAddr;
				for (auto& it : bufferGroup.entries)
				{
					uint64* __restrict regDest = (uint64*)((uint8*)uniformData + it.mappedIndexOffset);
					uint64* __restrict uniformEntrySrc = (uint64*)(uniformBase + it.indexOffset);
					memcpy(regDest, uniformEntrySrc, 16);
				}
			}
			else
			{
				for (auto& it : bufferGroup.entries)
				{
					uint64* regDest = (uint64*)((uint8*)uniformData + it.mappedIndexOffset);
					regDest[0] = 0;
					regDest[1] = 0;
				}
			}
			hasChange = true;
		}
	}
	return hasChange;
}

bool LatteBufferCache_syncGPUUniformBuffers(LatteDecompilerShader* shader, const uint32 uniformBufferRegOffset, LatteConst::ShaderType shaderType, uint32 bufferDirtyMask)
{
	cemu_assert_debug(shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK);
	bool hasChange = false;
	for(const auto& buf : shader->list_quickBufferList)
	{
		sint32 i = buf.index;
		if ((bufferDirtyMask&(1<<i)) == 0)
			continue;
		hasChange = true;
		MPTR physicalAddr = LatteGPUState.contextRegister[uniformBufferRegOffset + i * 7 + 0];
		uint32 uniformSize = LatteGPUState.contextRegister[uniformBufferRegOffset + i * 7 + 1] + 1;
		if (physicalAddr == MPTR_NULL) [[unlikely]]
		{
			g_renderer->buffer_bindUniformBuffer(shaderType, i, 0, 0);
			continue;
		}
		uniformSize = std::min<uint32>(uniformSize, buf.size);
		uint32 bindOffset = LatteBufferCache_retrieveDataInCache(physicalAddr, uniformSize);
		g_renderer->buffer_bindUniformBuffer(shaderType, i, bindOffset, uniformSize);
	}
	return hasChange;
}

// for detecting when vertex buffer size needs to be extended during incremental rendering
static sint32 s_vtxStateMaxIndex{};
static sint32 s_vtxStateMaxInstance{};

void LatteBufferCache_ProcessQueues()
{
	static uint32 s_syncBufferCounter = 0;

	s_syncBufferCounter++;
	if (s_syncBufferCounter >= 25)
	{
		LatteBufferCache_incrementalCleanup();
		s_syncBufferCounter = 0;
	}
	LatteBufferCache_processDCFlushQueue();
	// process queued deallocations from previous drawcall
	LatteBufferCache_processDeallocations();
}

// upload vertex and uniform buffers and update bindings
void LatteBufferCache_Sync(uint32 maxIndex, uint32 baseInstance, uint32 instanceCount, uint32 attribBufferDirtyMask, uint32 vsUniformBufferDirtyMask, uint32 psUniformBufferDirtyMask, uint32 gsUniformBufferDirtyMask, uint8& stageUniformModifiedMask, bool isIncremental)
{
	LatteFetchShader* parsedFetchShader = LatteSHRC_GetActiveFetchShader();
	cemu_assert_debug(parsedFetchShader);

	// todo - vertex attribute offsets may eventually be allowed to change between incremental draws, we should set the attrib dirty bits in that case
	if (isIncremental)
	{
		// dont process flush queue and dont process deallocations yet, we are in the middle of a sequence of drawcalls that (most likely) reuse previous bindings
		if ( maxIndex > s_vtxStateMaxIndex )
		{
			attribBufferDirtyMask = 0xFFFFFFFF;
			s_vtxStateMaxIndex = maxIndex;
		}
		uint32 maxInstance = baseInstance + instanceCount - 1;
		if ( maxInstance > s_vtxStateMaxInstance )
		{
			attribBufferDirtyMask = 0xFFFFFFFF;
			s_vtxStateMaxInstance = maxInstance;
		}
	}
	else
	{
		LatteBufferCache_ProcessQueues();
		s_vtxStateMaxIndex = maxIndex;
		uint32 maxInstance = baseInstance + instanceCount - 1;
		s_vtxStateMaxInstance = maxInstance;
	}
	attribBufferDirtyMask &= parsedFetchShader->attributeBufferMask;

	// sync and bind dirty vertex buffers
	if (attribBufferDirtyMask != 0)
	{
		uint32* __restrict bufferRegStartPtr = LatteGPUState.contextRegister + mmSQ_VTX_ATTRIBUTE_BLOCK_START;
		for (auto& bufferGroup : parsedFetchShader->bufferGroups)
		{
			uint32 bufferIndex = bufferGroup.attributeBufferIndex;
			if ((attribBufferDirtyMask&(1<<bufferIndex)) == 0)
				continue;
			uint32* __restrict bufferRegs = bufferRegStartPtr + bufferIndex * 7;
			MPTR bufferAddress = bufferRegs[0];
			uint32 bufferStride = (bufferRegs[2] >> 11) & 0xFFFF;

			if (bufferAddress == MPTR_NULL) [[unlikely]]
			{
				g_renderer->buffer_bindVertexBuffer(bufferIndex, 0, 0);
				continue;
			}

			// dont rely on buffer size given by game
			uint32 fixedBufferSize = 0;
			if (bufferGroup.hasVtxIndexAccess)
				fixedBufferSize = bufferStride * (maxIndex + 1) + bufferGroup.maxOffset;
			if (bufferGroup.hasInstanceIndexAccess)
			{
				uint32 fixedBufferSizeInstance = bufferStride * ((baseInstance + instanceCount) + 1) + bufferGroup.maxOffset;
				fixedBufferSize = std::max(fixedBufferSize, fixedBufferSizeInstance);
			}
			if (fixedBufferSize == 0 || bufferStride == 0)
				fixedBufferSize += 128;


#if BOOST_OS_MACOS && defined(ENABLE_VULKAN)
			if(bufferStride % 4 != 0)
			{
				if (g_renderer->GetType() == RendererAPI::Vulkan)
				{
					if (VulkanRenderer* vkRenderer = VulkanRenderer::GetInstance())
					{
						auto fixedBuffer = vkRenderer->buffer_genStrideWorkaroundVertexBuffer(bufferAddress, fixedBufferSize, bufferStride);
						vkRenderer->buffer_bindVertexStrideWorkaroundBuffer(fixedBuffer.first, fixedBuffer.second, bufferIndex, fixedBufferSize);
						continue;
					}
				}
			}
#endif

			uint32 bindOffset = LatteBufferCache_retrieveDataInCache(bufferAddress, fixedBufferSize);
			g_renderer->buffer_bindVertexBuffer(bufferIndex, bindOffset, fixedBufferSize);
		}
	}
	// sync uniform buffers
	LatteDecompilerShader* vertexShader = LatteSHRC_GetActiveVertexShader();
	LatteDecompilerShader* geometryShader = LatteSHRC_GetActiveGeometryShader();
	LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
	// todo - if we AND the shader uniform buffer mask and the dirty mask we can completely skip calling syncGPUUniformBuffers if no relevant buffer was updated
	if (vertexShader && vertexShader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK)
	{
		if (LatteBufferCache_syncGPUUniformBuffers(vertexShader, mmSQ_VTX_UNIFORM_BLOCK_START, LatteConst::ShaderType::Vertex, vsUniformBufferDirtyMask))
			stageUniformModifiedMask |= (1<<VulkanRendererConst::SHADER_STAGE_INDEX_VERTEX);
	}
	if (pixelShader && pixelShader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK)
	{
		if (LatteBufferCache_syncGPUUniformBuffers(pixelShader, mmSQ_PS_UNIFORM_BLOCK_START, LatteConst::ShaderType::Pixel, psUniformBufferDirtyMask))
			stageUniformModifiedMask |= (1<<VulkanRendererConst::SHADER_STAGE_INDEX_FRAGMENT); // todo - move this enum to Latte?
	}
	if (geometryShader && geometryShader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK)
	{
		if ( LatteBufferCache_syncGPUUniformBuffers(geometryShader, mmSQ_GS_UNIFORM_BLOCK_START, LatteConst::ShaderType::Geometry, gsUniformBufferDirtyMask) )
			stageUniformModifiedMask |= (1<<VulkanRendererConst::SHADER_STAGE_INDEX_GEOMETRY);
	}
}
