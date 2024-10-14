#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/LatteTextureVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/RendererShaderVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanPipelineCompiler.h"

#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/Core/LatteIndices.h"
#include "Cafe/OS/libs/gx2/GX2.h"
#include "imgui/imgui_impl_vulkan.h"
#include "Cafe/GameProfile/GameProfile.h"
#include "util/helpers/helpers.h"

extern bool hasValidFramebufferAttached;

// includes only states that may change during minimal drawcalls
uint64 VulkanRenderer::draw_calculateMinimalGraphicsPipelineHash(const LatteFetchShader* fetchShader, const LatteContextRegister& lcr)
{
	uint64 stateHash = 0;

	// fetch shader
	for (auto& group : fetchShader->bufferGroups)
	{
		uint32 bufferStride = group.getCurrentBufferStride(lcr.GetRawView());
		stateHash = std::rotl<uint64>(stateHash, 7);
		stateHash += bufferStride * 3;
	}

	stateHash += fetchShader->getVkPipelineHashFragment();
	stateHash = std::rotl<uint64>(stateHash, 7);

	stateHash += lcr.GetRawView()[mmVGT_PRIMITIVE_TYPE];
	stateHash = std::rotl<uint64>(stateHash, 7);

	stateHash += lcr.GetRawView()[mmVGT_STRMOUT_EN];
	stateHash = std::rotl<uint64>(stateHash, 7);

	if(lcr.PA_CL_CLIP_CNTL.get_DX_RASTERIZATION_KILL())
		stateHash += 0x333333;

	return stateHash;
}

uint64 VulkanRenderer::draw_calculateGraphicsPipelineHash(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, const VKRObjectRenderPass* renderPassObj, const LatteContextRegister& lcr)
{
	// note: vertexShader references a fetchShader (vertexShader->fetchShader) but it's not necessarily the one that is currently active
	// this is because we try to separate dynamic state (mainly attribute offsets) from the actual attribute data layout and mapping (types and slots)
	// on Vulkan this causes issues because we bake the attribute offsets, which may not match vertexShader->compatibleFetchShader, into the pipeline
	// To avoid issues always use the active fetch shader. Not the one associated with the vertexShader object
	// note 2:
	// there is a secondary issue where we dont store all fetch shaders into the pipeline cache (only a single fetch shader is tied to each stored vertex shader)
	// but we can probably trust drivers to not require pipeline recompilation if only the offsets differ
	// An alternative would be to use VK_EXT_vertex_input_dynamic_state but it comes with minor overhead
	// Regardless, the extension is not well supported as of writing this (July 2021, only 10% of GPUs support it on Windows. Nvidia only)

	cemu_assert_debug(vertexShader->compatibleFetchShader->key == fetchShader->key); // fetch shaders must be layout compatible, but may have different offsets

	uint64 stateHash;
	stateHash = draw_calculateMinimalGraphicsPipelineHash(fetchShader, lcr);
	stateHash = (stateHash >> 8) + (stateHash * 0x370531ull) % 0x7F980D3BF9B4639Dull;
	
	uint32* ctxRegister = lcr.GetRawView();

	if (vertexShader)
		stateHash += vertexShader->baseHash;

	stateHash = std::rotl<uint64>(stateHash, 13);

	if (geometryShader)
		stateHash += geometryShader->baseHash;

	stateHash = std::rotl<uint64>(stateHash, 13);

	if (pixelShader)
		stateHash += pixelShader->baseHash + pixelShader->auxHash;

	stateHash = std::rotl<uint64>(stateHash, 13);

	uint32 polygonCtrl = lcr.PA_SU_SC_MODE_CNTL.getRawValue();
	stateHash += polygonCtrl;
	stateHash = std::rotl<uint64>(stateHash, 7);

	stateHash += ctxRegister[Latte::REGADDR::PA_CL_CLIP_CNTL];
	stateHash = std::rotl<uint64>(stateHash, 7);

	const auto colorControlReg = ctxRegister[Latte::REGADDR::CB_COLOR_CONTROL];
	stateHash += colorControlReg;

	stateHash += ctxRegister[Latte::REGADDR::CB_TARGET_MASK];

	const uint32 blendEnableMask = (colorControlReg >> 8) & 0xFF;
	if (blendEnableMask)
	{
		for (auto i = 0; i < 8; ++i)
		{
			if (((blendEnableMask & (1 << i))) == 0)
				continue;
			stateHash = std::rotl<uint64>(stateHash, 7);
			stateHash += ctxRegister[Latte::REGADDR::CB_BLEND0_CONTROL + i];
		}
	}

	stateHash += renderPassObj->m_hashForPipeline;
	
	uint32 depthControl = ctxRegister[Latte::REGADDR::DB_DEPTH_CONTROL];
	bool stencilTestEnable = depthControl & 1;
	if (stencilTestEnable)
	{
		stateHash += ctxRegister[mmDB_STENCILREFMASK];
		stateHash = std::rotl<uint64>(stateHash, 17);
		if(depthControl & (1<<7)) // back stencil enable
		{ 
			stateHash += ctxRegister[mmDB_STENCILREFMASK_BF];
			stateHash = std::rotl<uint64>(stateHash, 13);
		}
	}
	else
	{
		// zero out stencil related bits (8-31)
		depthControl &= 0xFF;
	}

	stateHash = std::rotl<uint64>(stateHash, 17);
	stateHash += depthControl;

	// polygon offset
	if (polygonCtrl & (1 << 11))
	{
		// front offset enabled
		stateHash += 0x1111;
	}

	return stateHash;
}

void VulkanRenderer::draw_debugPipelineHashState()
{
	cemu_assert_debug(false);
}

PipelineInfo* VulkanRenderer::draw_getCachedPipeline()
{
	// todo - optimize m_pipeline_info_cache away and store directly in vk vertex shader
	const auto fetchShader = LatteSHRC_GetActiveFetchShader();
	const auto vertexShader = LatteSHRC_GetActiveVertexShader();
	const auto it = m_pipeline_info_cache.find(vertexShader->baseHash);
	if (it == m_pipeline_info_cache.cend())
		return nullptr;

	const auto geometryShader = LatteSHRC_GetActiveGeometryShader();
	const auto pixelShader = LatteSHRC_GetActivePixelShader();
	auto cachedFboVk = (CachedFBOVk*)m_state.activeFBO;

	const uint64 stateHash = draw_calculateGraphicsPipelineHash(fetchShader, vertexShader, geometryShader, pixelShader, cachedFboVk->GetRenderPassObj(), LatteGPUState.contextNew);

	const auto innerit = it->second.find(stateHash);
	if (innerit == it->second.cend())
		return nullptr;

	return innerit->second;
}

void VulkanRenderer::unregisterGraphicsPipeline(PipelineInfo* pipelineInfo)
{
	bool removedFromCache = false;
	for (auto& topMapItr : m_pipeline_info_cache)
	{
		auto& subMap = topMapItr.second;
		for (auto it = subMap.cbegin(); it != subMap.cend();)
		{
			if (it->second == pipelineInfo)
			{
				subMap.erase(it);
				removedFromCache = true;
				break;
			}
			++it;
		}
		if (removedFromCache)
			break;
	}
}

bool g_compilePipelineThreadInit{false};
std::mutex g_compilePipelineMutex;
std::condition_variable g_compilePipelineCondVar;
std::queue<PipelineCompiler*> g_compilePipelineRequests;

void compilePipeline_thread(sint32 threadIndex)
{
	SetThreadName("compilePl");
#ifdef _WIN32
	// one thread runs at normal priority while the others run at lower priority
	if(threadIndex != 0)
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
	while (true)
	{
		std::unique_lock lock(g_compilePipelineMutex);
		while (g_compilePipelineRequests.empty())
			g_compilePipelineCondVar.wait(lock);

		PipelineCompiler* request = g_compilePipelineRequests.front();

		g_compilePipelineRequests.pop();

		lock.unlock();

		request->Compile(true, false, true);
		delete request;
	}
}

void compilePipelineThread_init()
{
	uint32 numCompileThreads;

	uint32 cpuCoreCount = GetPhysicalCoreCount();
	if (cpuCoreCount <= 2)
		numCompileThreads = 1;
	else
		numCompileThreads = 2 + (cpuCoreCount - 3); // 2 plus one additionally for every extra core above 3

	numCompileThreads = std::min(numCompileThreads, 8u); // cap at 8

	for (uint32_t i = 0; i < numCompileThreads; i++)
	{
		std::thread compileThread(compilePipeline_thread, i);
		compileThread.detach();
	}
}

void compilePipelineThread_queue(PipelineCompiler* v)
{
	std::unique_lock lock(g_compilePipelineMutex);
	g_compilePipelineRequests.push(std::move(v));
	lock.unlock();
	g_compilePipelineCondVar.notify_one();
}

// make a guess if a pipeline is not essential
// non-essential means that skipping these drawcalls shouldn't lead to permanently corrupted graphics
bool VulkanRenderer::IsAsyncPipelineAllowed(uint32 numIndices)
{
	// frame debuggers dont handle async well (as of 2020)
	if (IsDebugUtilsEnabled() && vkSetDebugUtilsObjectNameEXT)
		return false;

	CachedFBOVk* currentFBO = m_state.activeFBO;
	auto fboExtend = currentFBO->GetExtend();

	if (fboExtend.width == 1600 && fboExtend.height == 1600)
		return false; // Splatoon ink mechanics use 1600x1600 R8 and R8G8 framebuffers, this resolution is rare enough that we can just blacklist it globally

	if (currentFBO->hasDepthBuffer())
		return true; // aggressive filter but seems to work well so far

	// small index count (3,4,5,6) is often associated with full-viewport quads (which are considered essential due to often being used to generate persistent textures)
	if (numIndices <= 6)
	{

		return false;
	}

	return true;
}

// create graphics pipeline for current state
PipelineInfo* VulkanRenderer::draw_createGraphicsPipeline(uint32 indexCount)
{
	if (!g_compilePipelineThreadInit)
	{
		compilePipelineThread_init();
		g_compilePipelineThreadInit = true;
	}

	const auto fetchShader = LatteSHRC_GetActiveFetchShader();
	const auto vertexShader = LatteSHRC_GetActiveVertexShader();
	const auto geometryShader = LatteSHRC_GetActiveGeometryShader();
	const auto pixelShader = LatteSHRC_GetActivePixelShader();
	auto cachedFboVk = (CachedFBOVk*)m_state.activeFBO;

	uint64 minimalStateHash = draw_calculateMinimalGraphicsPipelineHash(fetchShader, LatteGPUState.contextNew);
	uint64 pipelineHash = draw_calculateGraphicsPipelineHash(fetchShader, vertexShader, geometryShader, pixelShader, cachedFboVk->GetRenderPassObj(), LatteGPUState.contextNew);

	// create PipelineInfo
	auto vkFBO = (CachedFBOVk*)(VulkanRenderer::GetInstance()->m_state.activeFBO);
	PipelineInfo* pipelineInfo = new PipelineInfo(minimalStateHash, pipelineHash, fetchShader, vertexShader, pixelShader, geometryShader);

	// register pipeline
	uint64 vsBaseHash = vertexShader->baseHash;
	auto it = m_pipeline_info_cache.emplace(vsBaseHash, robin_hood::unordered_flat_map<uint64, PipelineInfo*>());
	auto& cache_map = it.first->second;
	cache_map.emplace(pipelineHash, pipelineInfo);

	// init pipeline compiler
	PipelineCompiler* pipelineCompiler = new PipelineCompiler();

	pipelineCompiler->InitFromCurrentGPUState(pipelineInfo, LatteGPUState.contextNew, vkFBO->GetRenderPassObj());
	pipelineCompiler->TrackAsCached(vsBaseHash, pipelineHash);

	// use heuristics based on parameter patterns to determine if the current drawcall is essential (non-skipable)
	bool allowAsyncCompile = false; 
	if (GetConfig().async_compile)
		allowAsyncCompile = IsAsyncPipelineAllowed(indexCount);

	if (allowAsyncCompile)
	{
		// even when async is allowed, attempt synchronous creation first (which will immediately fail if the pipeline is not cached)
		if (pipelineCompiler->Compile(false, true, true) == false)
		{
			// shaders or pipeline not cached -> asynchronous compilation
			compilePipelineThread_queue(pipelineCompiler);
		}
		else
		{
			delete pipelineCompiler;
		}
	}
	else
	{
		// synchronous compilation
		pipelineCompiler->Compile(true, true, true);
		delete pipelineCompiler;
	}

	return pipelineInfo;
}

PipelineInfo* VulkanRenderer::draw_getOrCreateGraphicsPipeline(uint32 indexCount)
{
	auto cache_object = draw_getCachedPipeline();
	if (cache_object != nullptr)
	{

#ifdef CEMU_DEBUG_ASSERT
		cemu_assert_debug(cache_object->vertexShader == LatteSHRC_GetActiveVertexShader());
		cemu_assert_debug(cache_object->geometryShader == LatteSHRC_GetActiveGeometryShader());
		cemu_assert_debug(cache_object->pixelShader == LatteSHRC_GetActivePixelShader());
		if (cache_object->fetchShader->key != LatteSHRC_GetActiveFetchShader()->key ||
			cache_object->fetchShader->vkPipelineHashFragment != LatteSHRC_GetActiveFetchShader()->vkPipelineHashFragment)
		{
			debug_printf("Incompatible fetch shader %p %p\n", cache_object->fetchShader, LatteSHRC_GetActiveFetchShader());
			assert_dbg();
		}
		uint64 calcMinimalHash = draw_calculateMinimalGraphicsPipelineHash(LatteSHRC_GetActiveFetchShader(), LatteGPUState.contextNew);
		auto currentPrimitiveMode = LatteGPUState.contextNew.VGT_PRIMITIVE_TYPE.get_PRIMITIVE_MODE();
		cemu_assert_debug(cache_object->primitiveMode == currentPrimitiveMode);
		cemu_assert_debug(cache_object->minimalStateHash == calcMinimalHash);
#endif
		return cache_object;
	}
	//draw_debugPipelineHashState();

	return draw_createGraphicsPipeline(indexCount);
}

void* VulkanRenderer::indexData_reserveIndexMemory(uint32 size, uint32& offset, uint32& bufferIndex)
{
	auto& indexAllocator = this->memoryManager->getIndexAllocator();
	auto resv = indexAllocator.AllocateBufferMemory(size, 32);
	offset = resv.bufferOffset;
	bufferIndex = resv.bufferIndex;
	return resv.memPtr;
}

void VulkanRenderer::indexData_uploadIndexMemory(uint32 offset, uint32 size)
{
	// does nothing since the index buffer memory is coherent
}

float s_vkUniformData[512 * 4];

void VulkanRenderer::uniformData_updateUniformVars(uint32 shaderStageIndex, LatteDecompilerShader* shader)
{
	auto GET_UNIFORM_DATA_PTR = [](size_t index) { return s_vkUniformData + (index / 4); };

	sint32 shaderAluConst;

	switch (shader->shaderType)
	{
	case LatteConst::ShaderType::Vertex:
		shaderAluConst = 0x400;
		break;
	case LatteConst::ShaderType::Pixel:
		shaderAluConst = 0;
		break;
	case LatteConst::ShaderType::Geometry:
		shaderAluConst = 0; // geometry shader has no ALU const
		break;
	default:
		UNREACHABLE;
	}

	if (shader->resourceMapping.uniformVarsBufferBindingPoint >= 0)
	{
		if (shader->uniform.list_ufTexRescale.empty() == false)
		{
			for (auto& entry : shader->uniform.list_ufTexRescale)
			{
				float* xyScale = LatteTexture_getEffectiveTextureScale(shader->shaderType, entry.texUnit);
				memcpy(entry.currentValue, xyScale, sizeof(float) * 2);
				memcpy(GET_UNIFORM_DATA_PTR(entry.uniformLocation), xyScale, sizeof(float) * 2);
			}
		}
		if (shader->uniform.loc_alphaTestRef >= 0)
		{
			*GET_UNIFORM_DATA_PTR(shader->uniform.loc_alphaTestRef) = LatteGPUState.contextNew.SX_ALPHA_REF.get_ALPHA_TEST_REF();
		}
		if (shader->uniform.loc_pointSize >= 0)
		{
			const auto& pointSizeReg = LatteGPUState.contextNew.PA_SU_POINT_SIZE;
			float pointWidth = (float)pointSizeReg.get_WIDTH() / 8.0f;
			if (pointWidth == 0.0f)
				pointWidth = 1.0f / 8.0f; // minimum size
			*GET_UNIFORM_DATA_PTR(shader->uniform.loc_pointSize) = pointWidth;
		}
		if (shader->uniform.loc_remapped >= 0)
		{
			LatteBufferCache_LoadRemappedUniforms(shader, GET_UNIFORM_DATA_PTR(shader->uniform.loc_remapped));
		}
		if (shader->uniform.loc_uniformRegister >= 0)
		{
			uint32* uniformRegData = (uint32*)(LatteGPUState.contextRegister + mmSQ_ALU_CONSTANT0_0 + shaderAluConst);
			memcpy(GET_UNIFORM_DATA_PTR(shader->uniform.loc_uniformRegister), uniformRegData, shader->uniform.count_uniformRegister * 16);
		}
		if (shader->uniform.loc_windowSpaceToClipSpaceTransform >= 0)
		{
			sint32 viewportWidth;
			sint32 viewportHeight;
			LatteRenderTarget_GetCurrentVirtualViewportSize(&viewportWidth, &viewportHeight); // always call after _updateViewport()
			float* v = GET_UNIFORM_DATA_PTR(shader->uniform.loc_windowSpaceToClipSpaceTransform);
			v[0] = 2.0f / (float)viewportWidth;
			v[1] = 2.0f / (float)viewportHeight;
		}
		if (shader->uniform.loc_fragCoordScale >= 0)
		{
			LatteMRT::GetCurrentFragCoordScale(GET_UNIFORM_DATA_PTR(shader->uniform.loc_fragCoordScale));
		}
		if (shader->uniform.loc_verticesPerInstance >= 0)
		{
			*(int*)GET_UNIFORM_DATA_PTR(shader->uniform.loc_verticesPerInstance) = m_streamoutState.verticesPerInstance;
			for (sint32 b = 0; b < LATTE_NUM_STREAMOUT_BUFFER; b++)
			{
				if (shader->uniform.loc_streamoutBufferBase[b] >= 0)
				{
					*(uint32*)GET_UNIFORM_DATA_PTR(shader->uniform.loc_streamoutBufferBase[b]) = m_streamoutState.buffer[b].ringBufferOffset;
				}
			}
		}
		// upload
		const uint32 bufferAlignmentM1 = std::max(m_featureControl.limits.minUniformBufferOffsetAlignment, m_featureControl.limits.nonCoherentAtomSize) - 1;
		const uint32 uniformSize = (shader->uniform.uniformRangeSize + bufferAlignmentM1) & ~bufferAlignmentM1;

		auto waitWhileCondition = [&](std::function<bool()> condition) {
			while (condition())
			{
				if (m_commandBufferSyncIndex == m_commandBufferIndex)
				{
					if (m_cmdBufferUniformRingbufIndices[m_commandBufferIndex] != m_uniformVarBufferReadIndex)
					{
						draw_endRenderPass();
						SubmitCommandBuffer();
					}
					else
					{
						// submitting work would not change readIndex, so there's no way for conditions based on it to change
						cemuLog_log(LogType::Force, "draw call overflowed and corrupted uniform ringbuffer. expect visual corruption");
						cemu_assert_suspicious();
						break;
					}
				}
				WaitForNextFinishedCommandBuffer();
			}
		};

		// wrap around if it doesnt fit consecutively
		if (m_uniformVarBufferWriteIndex + uniformSize > UNIFORMVAR_RINGBUFFER_SIZE)
		{
			waitWhileCondition([&]() {
				return m_uniformVarBufferReadIndex > m_uniformVarBufferWriteIndex || m_uniformVarBufferReadIndex == 0;
			});
			m_uniformVarBufferWriteIndex = 0;
		}

		auto ringBufRemaining = [&]() {
			ssize_t ringBufferUsedBytes = (ssize_t)m_uniformVarBufferWriteIndex - m_uniformVarBufferReadIndex;
			if (ringBufferUsedBytes < 0)
				ringBufferUsedBytes += UNIFORMVAR_RINGBUFFER_SIZE;
			return UNIFORMVAR_RINGBUFFER_SIZE - 1 - ringBufferUsedBytes;
		};
		waitWhileCondition([&]() {
			return ringBufRemaining() < uniformSize;
		});

		const uint32 uniformOffset = m_uniformVarBufferWriteIndex;
		memcpy(m_uniformVarBufferPtr + uniformOffset, s_vkUniformData, shader->uniform.uniformRangeSize);
		m_uniformVarBufferWriteIndex += uniformSize;
		// update dynamic offset
		dynamicOffsetInfo.uniformVarBufferOffset[shaderStageIndex] = uniformOffset;
		// flush if not coherent
		if (!m_uniformVarBufferMemoryIsCoherent)
		{
			VkMappedMemoryRange flushedRange{};
			flushedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			flushedRange.memory = m_uniformVarBufferMemory;
			flushedRange.offset = uniformOffset;
			flushedRange.size = uniformSize;
			vkFlushMappedMemoryRanges(m_logicalDevice, 1, &flushedRange);
		}
	}
}

void VulkanRenderer::draw_prepareDynamicOffsetsForDescriptorSet(uint32 shaderStageIndex, uint32* dynamicOffsets,
	sint32& numDynOffsets,
	const PipelineInfo* pipeline_info)
{
	numDynOffsets = 0;
	if (pipeline_info->dynamicOffsetInfo.hasUniformVar[shaderStageIndex])
	{
		dynamicOffsets[0] = dynamicOffsetInfo.uniformVarBufferOffset[shaderStageIndex];
		numDynOffsets++;
	}
	if (pipeline_info->dynamicOffsetInfo.hasUniformBuffers[shaderStageIndex])
	{
		for (auto& itr : pipeline_info->dynamicOffsetInfo.list_uniformBuffers[shaderStageIndex])
		{
			dynamicOffsets[numDynOffsets] = dynamicOffsetInfo.shaderUB[shaderStageIndex].uniformBufferOffset[itr];
			numDynOffsets++;
		}
	}
}

uint64 VulkanRenderer::GetDescriptorSetStateHash(LatteDecompilerShader* shader)
{
	uint64 hash = 0;

	const sint32 textureCount = shader->resourceMapping.getTextureCount();
	for (int i = 0; i < textureCount; ++i)
	{
		const auto relative_textureUnit = shader->resourceMapping.getTextureUnitFromBindingPoint(i);
		auto hostTextureUnit = relative_textureUnit;
		auto textureDim = shader->textureUnitDim[relative_textureUnit];
		auto texUnitRegIndex = hostTextureUnit * 7;
		switch (shader->shaderType)
		{
		case LatteConst::ShaderType::Vertex:
			hostTextureUnit += LATTE_CEMU_VS_TEX_UNIT_BASE;
			texUnitRegIndex += Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_VS;
			break;
		case LatteConst::ShaderType::Pixel:
			hostTextureUnit += LATTE_CEMU_PS_TEX_UNIT_BASE;
			texUnitRegIndex += Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_PS;
			break;
		case LatteConst::ShaderType::Geometry:
			hostTextureUnit += LATTE_CEMU_GS_TEX_UNIT_BASE;
			texUnitRegIndex += Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_GS;
			break;
		default:
			UNREACHABLE;
		}

		auto texture = m_state.boundTexture[hostTextureUnit];
		if (!texture)
			continue;

		const uint32 word4 = LatteGPUState.contextRegister[texUnitRegIndex + 4];

		uint32 samplerIndex = shader->textureUnitSamplerAssignment[relative_textureUnit];
		if (samplerIndex != LATTE_DECOMPILER_SAMPLER_NONE)
		{
			samplerIndex += LatteDecompiler_getTextureSamplerBaseIndex(shader->shaderType);
			hash += LatteGPUState.contextRegister[Latte::REGADDR::SQ_TEX_SAMPLER_WORD0_0 + samplerIndex * 3 + 0];
			hash = std::rotl<uint64>(hash, 7);
			hash += LatteGPUState.contextRegister[Latte::REGADDR::SQ_TEX_SAMPLER_WORD0_0 + samplerIndex * 3 + 1];
			hash = std::rotl<uint64>(hash, 7);
			hash += LatteGPUState.contextRegister[Latte::REGADDR::SQ_TEX_SAMPLER_WORD0_0 + samplerIndex * 3 + 2];
			hash = std::rotl<uint64>(hash, 7);
		}
		hash = std::rotl<uint64>(hash, 7);
		// hash view id + swizzle mask
		hash += (uint64)texture->GetUniqueId();
		hash = std::rotr<uint64>(hash, 21);
		hash += (uint64)(word4 & 0x0FFF0000);
	}

	return hash;
}

VkDescriptorSetInfo* VulkanRenderer::draw_getOrCreateDescriptorSet(PipelineInfo* pipeline_info, LatteDecompilerShader* shader)
{
	const uint64 stateHash = GetDescriptorSetStateHash(shader);

	VkDescriptorSetLayout descriptor_set_layout;
	switch (shader->shaderType)
	{
	case LatteConst::ShaderType::Vertex:
	{
		const auto it = pipeline_info->vertex_ds_cache.find(stateHash);
		if (it != pipeline_info->vertex_ds_cache.cend())
			return it->second;
		descriptor_set_layout = pipeline_info->m_vkrObjPipeline->vertexDSL;
		break;
	}
	case LatteConst::ShaderType::Pixel:
	{
		const auto it = pipeline_info->pixel_ds_cache.find(stateHash);
		if (it != pipeline_info->pixel_ds_cache.cend())
			return it->second;
		descriptor_set_layout = pipeline_info->m_vkrObjPipeline->pixelDSL;
		break;
	}
	case LatteConst::ShaderType::Geometry:
	{
		const auto it = pipeline_info->geometry_ds_cache.find(stateHash);
		if (it != pipeline_info->geometry_ds_cache.cend())
			return it->second;
		descriptor_set_layout = pipeline_info->m_vkrObjPipeline->geometryDSL;
		break;
	}
	default:
		UNREACHABLE;
	}

	// create new descriptor set
	VkDescriptorSetInfo* dsInfo = new VkDescriptorSetInfo();
	dsInfo->stateHash = stateHash;
	dsInfo->shaderType = shader->shaderType;
	dsInfo->pipeline_info = pipeline_info;

	dsInfo->m_vkObjDescriptorSet = new VKRObjectDescriptorSet();
	auto vkObjDS = dsInfo->m_vkObjDescriptorSet;

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptor_set_layout;

	VkDescriptorSet result;
	if (vkAllocateDescriptorSets(m_logicalDevice, &allocInfo, &result) != VK_SUCCESS)
	{
		UnrecoverableError(fmt::format("Failed to allocate descriptor sets. Currently allocated: Descriptors={} TextureSamplers={} DynUniformBuffers={} StorageBuffers={}",
			performanceMonitor.vk.numDescriptorSets.get(),
			performanceMonitor.vk.numDescriptorSamplerTextures.get(),
			performanceMonitor.vk.numDescriptorDynUniformBuffers.get(),
			performanceMonitor.vk.numDescriptorStorageBuffers.get()
		).c_str());
	}
	vkObjDS->descriptorSet = result;

	sint32 textureCount = shader->resourceMapping.getTextureCount();

	std::vector<VkWriteDescriptorSet> descriptorWrites;
	std::vector<VkDescriptorImageInfo> textureArray;
	for (int i = 0; i < textureCount; ++i)
	{
		VkDescriptorImageInfo info{};
		const auto relative_textureUnit = shader->resourceMapping.getTextureUnitFromBindingPoint(i);
		auto hostTextureUnit = relative_textureUnit;
		auto textureDim = shader->textureUnitDim[relative_textureUnit];
		auto texUnitRegIndex = hostTextureUnit * 7;
		switch (shader->shaderType)
		{
		case LatteConst::ShaderType::Vertex:
			hostTextureUnit += LATTE_CEMU_VS_TEX_UNIT_BASE;
			texUnitRegIndex += Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_VS;
			break;
		case LatteConst::ShaderType::Pixel:
			hostTextureUnit += LATTE_CEMU_PS_TEX_UNIT_BASE;
			texUnitRegIndex += Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_PS;
			break;
		case LatteConst::ShaderType::Geometry:
			hostTextureUnit += LATTE_CEMU_GS_TEX_UNIT_BASE;
			texUnitRegIndex += Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_GS;
			break;
		default:
			UNREACHABLE;
		}

		auto textureView = m_state.boundTexture[hostTextureUnit];
		if (!textureView)
		{
			if (textureDim == Latte::E_DIM::DIM_1D)
			{
				info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				info.imageView = nullTexture1D.view;
				info.sampler = nullTexture1D.sampler;
				textureArray.emplace_back(info);
			}
			else
			{
				info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				info.imageView = nullTexture2D.view;
				info.sampler = nullTexture2D.sampler;
				textureArray.emplace_back(info);
			}
			continue;
		}
		// safeguard to avoid using mismatching texture dimensions
		// if we properly support aux hash for vs/gs this should never trigger
		if (textureDim == Latte::E_DIM::DIM_1D && (textureView->dim != Latte::E_DIM::DIM_1D))
		{
			// should be 1D
			info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			info.imageView = nullTexture1D.view;
			info.sampler = nullTexture1D.sampler;
			textureArray.emplace_back(info);
			// cemuLog_log(LogType::Force, "Vulkan-Info: Shader 0x{:016x} uses 1D texture but bound texture has mismatching type (dim: 0x{:02x})", shader->baseHash, textureView->gx2Dim);
			continue;
		}
		else if (textureDim == Latte::E_DIM::DIM_2D && (textureView->dim != Latte::E_DIM::DIM_2D && textureView->dim != Latte::E_DIM::DIM_2D_MSAA))
		{
			// should be 2D
			// is GPU7 fine with 2D access to a 2D_ARRAY texture?
			info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			info.imageView = nullTexture2D.view;
			info.sampler = nullTexture2D.sampler;
			textureArray.emplace_back(info);
			// cemuLog_log(LogType::Force, "Vulkan-Info: Shader 0x{:016x} uses 2D texture but bound texture has mismatching type (dim: 0x{:02x})", shader->baseHash, textureView->gx2Dim);
			continue;
		}

		info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkSamplerCustomBorderColorCreateInfoEXT samplerCustomBorderColor{};

		VkSampler sampler;
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

		LatteTexture* baseTexture = textureView->baseTexture;
		// get texture register word 0
		uint32 word4 = LatteGPUState.contextRegister[texUnitRegIndex + 4];
		
		auto imageViewObj = textureView->GetSamplerView(word4);		
		info.imageView = imageViewObj->m_textureImageView;
		vkObjDS->addRef(imageViewObj);

		// track relation between view and descriptor set
		vectorAppendUnique(dsInfo->list_referencedViews, textureView);
		textureView->AddDescriptorSetReference(dsInfo);

		if (!baseTexture->IsCompressedFormat())
			vectorAppendUnique(dsInfo->list_fboCandidates, (LatteTextureVk*)baseTexture);

		uint32 stageSamplerIndex = shader->textureUnitSamplerAssignment[relative_textureUnit];
		if (stageSamplerIndex != LATTE_DECOMPILER_SAMPLER_NONE)
		{
			uint32 samplerIndex = stageSamplerIndex + LatteDecompiler_getTextureSamplerBaseIndex(shader->shaderType);
			const _LatteRegisterSetSampler* samplerWords = LatteGPUState.contextNew.SQ_TEX_SAMPLER + samplerIndex;

			// lod
			uint32 iMinLOD = samplerWords->WORD1.get_MIN_LOD();
			uint32 iMaxLOD = samplerWords->WORD1.get_MAX_LOD();
			sint32 iLodBias = samplerWords->WORD1.get_LOD_BIAS();

			// apply relative lod bias from graphic pack
			if (baseTexture->overwriteInfo.hasRelativeLodBias)
				iLodBias += baseTexture->overwriteInfo.relativeLodBias;
			// apply absolute lod bias from graphic pack
			if (baseTexture->overwriteInfo.hasLodBias)
				iLodBias = baseTexture->overwriteInfo.lodBias;

			auto filterMip = samplerWords->WORD0.get_MIP_FILTER();
			if (filterMip == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_Z_FILTER::NONE)
			{
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
				samplerInfo.minLod = 0;
				samplerInfo.maxLod = 0.25f;
			}
			else if (filterMip == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_Z_FILTER::POINT)
			{
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
				samplerInfo.minLod = (float)iMinLOD / 64.0f;
				samplerInfo.maxLod = (float)iMaxLOD / 64.0f;
			}
			else if (filterMip == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_Z_FILTER::LINEAR)
			{
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				samplerInfo.minLod = (float)iMinLOD / 64.0f;
				samplerInfo.maxLod = (float)iMaxLOD / 64.0f;
			}
			else
			{
				// fallback for invalid constants
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				samplerInfo.minLod = (float)iMinLOD / 64.0f;
				samplerInfo.maxLod = (float)iMaxLOD / 64.0f;
			}

			auto filterMin = samplerWords->WORD0.get_XY_MIN_FILTER();
			cemu_assert_debug(filterMin != Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::BICUBIC); // todo
			samplerInfo.minFilter = (filterMin == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::POINT || filterMin == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::ANISO_POINT) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;

			auto filterMag = samplerWords->WORD0.get_XY_MAG_FILTER();
			samplerInfo.magFilter = (filterMag == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::POINT || filterMin == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::ANISO_POINT) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;

			auto filterZ = samplerWords->WORD0.get_Z_FILTER();
			// todo: z-filter for texture array samplers is customizable for GPU7 but OpenGL/Vulkan doesn't expose this functionality?

			static const VkSamplerAddressMode s_vkClampTable[] = {
				VK_SAMPLER_ADDRESS_MODE_REPEAT, // WRAP
				VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, // MIRROR
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // CLAMP_LAST_TEXEL
				VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, // MIRROR_ONCE_LAST_TEXEL 
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // unsupported HALF_BORDER
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, // unsupported MIRROR_ONCE_HALF_BORDER
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, // CLAMP_BORDER
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER // MIRROR_ONCE_BORDER
			};

			auto clampX = samplerWords->WORD0.get_CLAMP_X();
			auto clampY = samplerWords->WORD0.get_CLAMP_Y();
			auto clampZ = samplerWords->WORD0.get_CLAMP_Z();

			samplerInfo.addressModeU = s_vkClampTable[(size_t)clampX];
			samplerInfo.addressModeV = s_vkClampTable[(size_t)clampY];
			samplerInfo.addressModeW = s_vkClampTable[(size_t)clampZ];

			auto maxAniso = samplerWords->WORD0.get_MAX_ANISO_RATIO();

			if (baseTexture->overwriteInfo.anisotropicLevel >= 0)
				maxAniso = baseTexture->overwriteInfo.anisotropicLevel;

			if (maxAniso > 0)
			{
				samplerInfo.anisotropyEnable = VK_TRUE;
				samplerInfo.maxAnisotropy = (float)(1 << maxAniso);
			}
			else
			{
				samplerInfo.anisotropyEnable = VK_FALSE;
				samplerInfo.maxAnisotropy = 1.0f;
			}

			samplerInfo.mipLodBias = (float)iLodBias / 64.0f;

			// depth compare
			uint8 depthCompareMode = shader->textureUsesDepthCompare[relative_textureUnit] ? 1 : 0;
			static const VkCompareOp s_vkCompareOps[]
			{
				VK_COMPARE_OP_NEVER,
				VK_COMPARE_OP_LESS,
				VK_COMPARE_OP_EQUAL,
				VK_COMPARE_OP_LESS_OR_EQUAL,
				VK_COMPARE_OP_GREATER,
				VK_COMPARE_OP_NOT_EQUAL,
				VK_COMPARE_OP_GREATER_OR_EQUAL,
				VK_COMPARE_OP_ALWAYS,
			};
			if (depthCompareMode == 1)
			{
				samplerInfo.compareEnable = VK_TRUE;
				samplerInfo.compareOp = s_vkCompareOps[(size_t)samplerWords->WORD0.get_DEPTH_COMPARE_FUNCTION()];
			}
			else
			{
				samplerInfo.compareEnable = VK_FALSE;
			}

			// border
			auto borderType = samplerWords->WORD0.get_BORDER_COLOR_TYPE();

			if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::TRANSPARENT_BLACK)
				samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
			else if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::OPAQUE_BLACK)
				samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
			else if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::OPAQUE_WHITE)
				samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			else
			{
				if (this->m_featureControl.deviceExtensions.custom_border_color_without_format)
				{
					samplerCustomBorderColor.sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT;
					samplerCustomBorderColor.format = VK_FORMAT_UNDEFINED;

					_LatteRegisterSetSamplerBorderColor* borderColorReg;
					if (shader->shaderType == LatteConst::ShaderType::Vertex)
						borderColorReg = LatteGPUState.contextNew.TD_VS_SAMPLER_BORDER_COLOR + stageSamplerIndex;
					else if (shader->shaderType == LatteConst::ShaderType::Pixel)
						borderColorReg = LatteGPUState.contextNew.TD_PS_SAMPLER_BORDER_COLOR + stageSamplerIndex;
					else // geometry
						borderColorReg = LatteGPUState.contextNew.TD_GS_SAMPLER_BORDER_COLOR + stageSamplerIndex;
					samplerCustomBorderColor.customBorderColor.float32[0] = borderColorReg->red.get_channelValue();
					samplerCustomBorderColor.customBorderColor.float32[1] = borderColorReg->green.get_channelValue();
					samplerCustomBorderColor.customBorderColor.float32[2] = borderColorReg->blue.get_channelValue();
					samplerCustomBorderColor.customBorderColor.float32[3] = borderColorReg->alpha.get_channelValue();

					samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
					samplerInfo.pNext = &samplerCustomBorderColor;
				}
				else
				{
					// default to transparent black if custom border color is not supported
					samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
				}
			}
		}

		if (vkCreateSampler(m_logicalDevice, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
			UnrecoverableError("Failed to create texture sampler");
		info.sampler = sampler;
		textureArray.emplace_back(info);
	}

	if (textureCount > 0)
	{
		for (sint32 i = 0; i < textureCount; i++)
		{
			VkWriteDescriptorSet write_descriptor{};
			write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_descriptor.dstSet = result;
			write_descriptor.dstBinding = shader->resourceMapping.getTextureBaseBindingPoint() + i;
			write_descriptor.dstArrayElement = 0;
			write_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write_descriptor.descriptorCount = 1;
			write_descriptor.pImageInfo = textureArray.data() + i;
			descriptorWrites.emplace_back(write_descriptor);
			performanceMonitor.vk.numDescriptorSamplerTextures.increment();
			dsInfo->statsNumSamplerTextures++;
		}
	}

	// descriptor for uniform vars (as buffer)

	VkDescriptorBufferInfo uniformVarsBufferInfo{};

	if (shader->resourceMapping.uniformVarsBufferBindingPoint >= 0)
	{
		uniformVarsBufferInfo.buffer = m_uniformVarBuffer;
		uniformVarsBufferInfo.offset = 0; // fixed offset is always zero since we only use dynamic offsets
		uniformVarsBufferInfo.range = shader->uniform.uniformRangeSize;
		
		VkWriteDescriptorSet write_descriptor{};
		write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor.dstSet = result;
		write_descriptor.dstBinding = shader->resourceMapping.uniformVarsBufferBindingPoint;
		write_descriptor.dstArrayElement = 0;
		write_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		write_descriptor.descriptorCount = 1;
		write_descriptor.pBufferInfo = &uniformVarsBufferInfo;
		descriptorWrites.emplace_back(write_descriptor);
		performanceMonitor.vk.numDescriptorDynUniformBuffers.increment();
		dsInfo->statsNumDynUniformBuffers++;
	}

	// descriptor for uniform buffers

	VkDescriptorBufferInfo uniformBufferInfo{};
	uniformBufferInfo.buffer = m_useHostMemoryForCache ? m_importedMem : m_bufferCache;
	uniformBufferInfo.offset = 0; // fixed offset is always zero since we only use dynamic offsets

	if (m_vendor == GfxVendor::AMD)
	{
		// on AMD we enable robust buffer access and map the remaining range of the buffer
		uniformBufferInfo.range = VK_WHOLE_SIZE;
	}
	else
	{
		// on other vendors (which may not allow large range values) we disable robust buffer access and use a fixed size
		// update: starting with their Vulkan 1.2 drivers Nvidia now also prevents out-of-bounds access. Unlike on AMD, we can't use VK_WHOLE_SIZE due to 64KB size limit of uniforms
		// as a workaround we set the size to the allowed maximum. A proper solution would be to use SSBOs for large uniforms / uniforms with unknown size?
		uniformBufferInfo.range = 1024 * 16 * 4; // XCX
	}

	for (sint32 i = 0; i < LATTE_NUM_MAX_UNIFORM_BUFFERS; i++)
	{
		if (shader->resourceMapping.uniformBuffersBindingPoint[i] >= 0)
		{
			VkWriteDescriptorSet write_descriptor{};
			write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_descriptor.dstSet = result;
			write_descriptor.dstBinding = shader->resourceMapping.uniformBuffersBindingPoint[i];
			write_descriptor.dstArrayElement = 0;
			write_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			write_descriptor.descriptorCount = 1;
			write_descriptor.pBufferInfo = &uniformBufferInfo;
			descriptorWrites.emplace_back(write_descriptor);
			performanceMonitor.vk.numDescriptorDynUniformBuffers.increment();
			dsInfo->statsNumDynUniformBuffers++;
		}
	}


	VkDescriptorBufferInfo tfStorageBufferInfo{};

	if (shader->resourceMapping.tfStorageBindingPoint >= 0)
	{
		tfStorageBufferInfo.buffer = m_xfbRingBuffer;
		tfStorageBufferInfo.offset = 0; // offset is calculated in shader
		tfStorageBufferInfo.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet write_descriptor{};
		write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor.dstSet = result;
		write_descriptor.dstBinding = shader->resourceMapping.tfStorageBindingPoint;
		write_descriptor.dstArrayElement = 0;
		write_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write_descriptor.descriptorCount = 1;
		write_descriptor.pBufferInfo = &tfStorageBufferInfo;
		descriptorWrites.emplace_back(write_descriptor);
		performanceMonitor.vk.numDescriptorStorageBuffers.increment();
		dsInfo->statsNumStorageBuffers++;
	}

	if (!descriptorWrites.empty())
		vkUpdateDescriptorSets(m_logicalDevice, (uint32)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);

	switch (shader->shaderType)
	{
	case LatteConst::ShaderType::Vertex:
	{
		pipeline_info->vertex_ds_cache[stateHash] = dsInfo;
		break;
	}
	case LatteConst::ShaderType::Pixel:
	{
		pipeline_info->pixel_ds_cache[stateHash] = dsInfo;
		break;
	}
	case LatteConst::ShaderType::Geometry:
	{
		pipeline_info->geometry_ds_cache[stateHash] = dsInfo;
		break;
	}
	default:
		UNREACHABLE;
	}

	return dsInfo;
}

void VulkanRenderer::sync_inputTexturesChanged()
{
	bool writeFlushRequired = false;

	if (m_state.activeVertexDS)
	{
		for (auto& tex : m_state.activeVertexDS->list_fboCandidates)
		{
			tex->m_vkFlushIndex_read = m_state.currentFlushIndex;
			if (tex->m_vkFlushIndex_write == m_state.currentFlushIndex)
				writeFlushRequired = true;
		}
	}
	if (m_state.activeGeometryDS)
	{
		for (auto& tex : m_state.activeGeometryDS->list_fboCandidates)
		{
			tex->m_vkFlushIndex_read = m_state.currentFlushIndex;
			if (tex->m_vkFlushIndex_write == m_state.currentFlushIndex)
				writeFlushRequired = true;
		}
	}
	if (m_state.activePixelDS)
	{
		for (auto& tex : m_state.activePixelDS->list_fboCandidates)
		{
			tex->m_vkFlushIndex_read = m_state.currentFlushIndex;
			if (tex->m_vkFlushIndex_write == m_state.currentFlushIndex)
				writeFlushRequired = true;
		}
	}
	// barrier here
	if (writeFlushRequired)
	{
		VkMemoryBarrier memoryBarrier{};
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.srcAccessMask = 0;
		memoryBarrier.dstAccessMask = 0;

		VkPipelineStageFlags srcStage = 0;
		VkPipelineStageFlags dstStage = 0;

		// src
		srcStage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		memoryBarrier.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		srcStage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		memoryBarrier.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		// dst
		dstStage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		memoryBarrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

		dstStage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		memoryBarrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(m_state.currentCommandBuffer, srcStage, dstStage, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

		performanceMonitor.vk.numDrawBarriersPerFrame.increment();

		m_state.currentFlushIndex++;
	}
}

void VulkanRenderer::sync_RenderPassLoadTextures(CachedFBOVk* fboVk)
{
	bool readFlushRequired = false;
	// always called after draw_inputTexturesChanged()
	for (auto& tex : fboVk->GetTextures())
	{
		LatteTextureVk* texVk = (LatteTextureVk*)tex;
		// write-before-write
		if (texVk->m_vkFlushIndex_write == m_state.currentFlushIndex)
			readFlushRequired = true;


		texVk->m_vkFlushIndex_write = m_state.currentFlushIndex;
		// todo - also check for write-before-write ?
		if (texVk->m_vkFlushIndex_read == m_state.currentFlushIndex)
			readFlushRequired = true;
	}
	// barrier here
	if (readFlushRequired)
	{
		VkMemoryBarrier memoryBarrier{};
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.srcAccessMask = 0;
		memoryBarrier.dstAccessMask = 0;

		VkPipelineStageFlags srcStage = 0;
		VkPipelineStageFlags dstStage = 0;

		// src
		srcStage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		memoryBarrier.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		srcStage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		memoryBarrier.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		// dst
		dstStage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		memoryBarrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

		dstStage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		memoryBarrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(m_state.currentCommandBuffer, srcStage, dstStage, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

		performanceMonitor.vk.numDrawBarriersPerFrame.increment();

		m_state.currentFlushIndex++;
	}
}

void VulkanRenderer::sync_RenderPassStoreTextures(CachedFBOVk* fboVk)
{
	uint32 flushIndex = m_state.currentFlushIndex;
	for (auto& tex : fboVk->GetTextures())
	{
		LatteTextureVk* texVk = (LatteTextureVk*)tex;
		texVk->m_vkFlushIndex_write = flushIndex;
	}
}

void VulkanRenderer::draw_prepareDescriptorSets(PipelineInfo* pipeline_info, VkDescriptorSetInfo*& vertexDS, VkDescriptorSetInfo*& pixelDS, VkDescriptorSetInfo*& geometryDS)
{
	const auto vertexShader = LatteSHRC_GetActiveVertexShader();
	const auto geometryShader = LatteSHRC_GetActiveGeometryShader();
	const auto pixelShader = LatteSHRC_GetActivePixelShader();


	if (vertexShader)
	{
		auto descriptorSetInfo = draw_getOrCreateDescriptorSet(pipeline_info, vertexShader);
		descriptorSetInfo->m_vkObjDescriptorSet->flagForCurrentCommandBuffer();
		vertexDS = descriptorSetInfo;
	}

	if (pixelShader)
	{
		auto descriptorSetInfo = draw_getOrCreateDescriptorSet(pipeline_info, pixelShader);
		descriptorSetInfo->m_vkObjDescriptorSet->flagForCurrentCommandBuffer();
		pixelDS = descriptorSetInfo;

	}

	if (geometryShader)
	{
		auto descriptorSetInfo = draw_getOrCreateDescriptorSet(pipeline_info, geometryShader);
		descriptorSetInfo->m_vkObjDescriptorSet->flagForCurrentCommandBuffer();
		geometryDS = descriptorSetInfo;
	}
}

void VulkanRenderer::draw_updateVkBlendConstants()
{
	uint32* blendColorConstant = LatteGPUState.contextRegister + Latte::REGADDR::CB_BLEND_RED;
	vkCmdSetBlendConstants(m_state.currentCommandBuffer, (const float*)blendColorConstant);
}

void VulkanRenderer::draw_updateDepthBias(bool forceUpdate)
{
	uint32 frontScaleU32 = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_SCALE.getRawValue();
	uint32 frontOffsetU32 = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_OFFSET.getRawValue();
	uint32 offsetClampU32 = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_CLAMP.getRawValue();

	if (forceUpdate == false &&
		m_state.prevPolygonFrontScaleU32 == frontScaleU32 &&
		m_state.prevPolygonFrontOffsetU32 == frontOffsetU32 &&
		m_state.prevPolygonFrontClampU32 == offsetClampU32)
		return;

	m_state.prevPolygonFrontScaleU32 = frontScaleU32;
	m_state.prevPolygonFrontOffsetU32 = frontOffsetU32;
	m_state.prevPolygonFrontClampU32 = offsetClampU32;

	float frontScale = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_SCALE.get_SCALE();
	float frontOffset = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_FRONT_OFFSET.get_OFFSET();
	float offsetClamp = LatteGPUState.contextNew.PA_SU_POLY_OFFSET_CLAMP.get_CLAMP();

	frontScale /= 16.0f;

	vkCmdSetDepthBias(m_state.currentCommandBuffer, frontOffset, offsetClamp, frontScale);
}

bool s_syncOnNextDraw = false;

void VulkanRenderer::draw_setRenderPass()
{
	CachedFBOVk* fboVk = m_state.activeFBO;

	// update self-dependency flag
	if (m_state.descriptorSetsChanged || m_state.activeRenderpassFBO != fboVk)
	{
		m_state.hasRenderSelfDependency = fboVk->CheckForCollision(m_state.activeVertexDS, m_state.activeGeometryDS, m_state.activePixelDS);
	}

	auto vkObjRenderPass = fboVk->GetRenderPassObj();
	auto vkObjFramebuffer = fboVk->GetFramebufferObj();

	bool overridePassReuse = m_state.hasRenderSelfDependency && (GetConfig().vk_accurate_barriers || m_state.activePipelineInfo->neverSkipAccurateBarrier);

	if (!overridePassReuse && m_state.activeRenderpassFBO == fboVk)
	{
		if (m_state.descriptorSetsChanged)
			sync_inputTexturesChanged();
		return;
	}
	draw_endRenderPass();
	if (m_state.descriptorSetsChanged)
		sync_inputTexturesChanged();
	
	// assume that FBO changed, update self-dependency state
	m_state.hasRenderSelfDependency = fboVk->CheckForCollision(m_state.activeVertexDS, m_state.activeGeometryDS, m_state.activePixelDS);

	sync_RenderPassLoadTextures(fboVk);

	if (m_featureControl.deviceExtensions.dynamic_rendering)
	{
		vkCmdBeginRenderingKHR(m_state.currentCommandBuffer, fboVk->GetRenderingInfo());
	}
	else
	{
		VkRenderPass renderPass = vkObjRenderPass->m_renderPass;
		VkFramebuffer framebuffer = vkObjFramebuffer->m_frameBuffer;
		VkExtent2D extend = fboVk->GetExtend();

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = framebuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = extend;
		renderPassInfo.clearValueCount = 0;
		vkCmdBeginRenderPass(m_state.currentCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	m_state.activeRenderpassFBO = fboVk;

	vkObjRenderPass->flagForCurrentCommandBuffer();
	vkObjFramebuffer->flagForCurrentCommandBuffer();

	performanceMonitor.vk.numBeginRenderpassPerFrame.increment();
}

void VulkanRenderer::draw_endRenderPass()
{
	if (!m_state.activeRenderpassFBO)
		return;
	if (m_featureControl.deviceExtensions.dynamic_rendering)
		vkCmdEndRenderingKHR(m_state.currentCommandBuffer);
	else
		vkCmdEndRenderPass(m_state.currentCommandBuffer);
	sync_RenderPassStoreTextures(m_state.activeRenderpassFBO);
	m_state.activeRenderpassFBO = nullptr;
}

void LatteDraw_handleSpecialState8_clearAsDepth();

// transfer depth buffer data to color buffer
void VulkanRenderer::draw_handleSpecialState5()
{
	LatteMRT::UpdateCurrentFBO();
	LatteRenderTarget_updateViewport();

	LatteTextureView* colorBuffer = LatteMRT::GetColorAttachment(0);
	LatteTextureView* depthBuffer = LatteMRT::GetDepthAttachment();

	sint32 vpWidth, vpHeight;
	LatteMRT::GetVirtualViewportDimensions(vpWidth, vpHeight);

	surfaceCopy_copySurfaceWithFormatConversion(
		depthBuffer->baseTexture, depthBuffer->firstMip, depthBuffer->firstSlice,
		colorBuffer->baseTexture, colorBuffer->firstMip, colorBuffer->firstSlice,
		vpWidth, vpHeight);
}

void VulkanRenderer::draw_beginSequence()
{
	m_state.drawSequenceSkip = false;

	bool streamoutEnable = LatteGPUState.contextRegister[mmVGT_STRMOUT_EN] != 0;

	// update shader state
	LatteSHRC_UpdateActiveShaders();
	if (LatteGPUState.activeShaderHasError)
	{
		cemuLog_logDebugOnce(LogType::Force, "Skipping drawcalls due to shader error");
		m_state.drawSequenceSkip = true;
		cemu_assert_debug(false);
		return;
	}

	// update render target and texture state
	LatteGPUState.requiresTextureBarrier = false;
	while (true)
	{
		LatteGPUState.repeatTextureInitialization = false;
		if (!LatteMRT::UpdateCurrentFBO())
		{
			debug_printf("Rendertarget invalid\n");
			m_state.drawSequenceSkip = true;
			return; // no render target
		}

		if (!hasValidFramebufferAttached && !streamoutEnable)
		{
			debug_printf("Drawcall with no color buffer or depth buffer attached\n");
			m_state.drawSequenceSkip = true;
			return; // no render target
		}
		LatteTexture_updateTextures();
		if (!LatteGPUState.repeatTextureInitialization)
			break;
	}

	// apply render target
	LatteMRT::ApplyCurrentState();

	// viewport and scissor box
	LatteRenderTarget_updateViewport();
	LatteRenderTarget_updateScissorBox();

	// check for conditions which would turn the drawcalls into no-ops
	bool rasterizerEnable = LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_DX_RASTERIZATION_KILL() == false;

	// GX2SetSpecialState(0, true) enables DX_RASTERIZATION_KILL, but still expects depth writes to happen? -> Research which stages are disabled by DX_RASTERIZATION_KILL exactly
	// for now we use a workaround:
	if (!LatteGPUState.contextNew.PA_CL_VTE_CNTL.get_VPORT_X_OFFSET_ENA())
		rasterizerEnable = true;

	if (rasterizerEnable == false && streamoutEnable == false)
		m_state.drawSequenceSkip = true;
}

void VulkanRenderer::draw_execute(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, bool isFirst)
{
	if (m_state.drawSequenceSkip)
	{
		LatteGPUState.drawCallCounter++;
		return;
	}

	// fast clear color as depth
	if (LatteGPUState.contextNew.GetSpecialStateValues()[8] != 0)
	{
		LatteDraw_handleSpecialState8_clearAsDepth();
		LatteGPUState.drawCallCounter++;
		return;
	}
	else if (LatteGPUState.contextNew.GetSpecialStateValues()[5] != 0)
	{
		draw_handleSpecialState5();
		LatteGPUState.drawCallCounter++;
		return;
	}

	// prepare streamout
	m_streamoutState.verticesPerInstance = count;
	LatteStreamout_PrepareDrawcall(count, instanceCount);

	// update uniform vars
	LatteDecompilerShader* vertexShader = LatteSHRC_GetActiveVertexShader();
	LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
	LatteDecompilerShader* geometryShader = LatteSHRC_GetActiveGeometryShader();

	if (vertexShader)
		uniformData_updateUniformVars(VulkanRendererConst::SHADER_STAGE_INDEX_VERTEX, vertexShader);
	if (pixelShader)
		uniformData_updateUniformVars(VulkanRendererConst::SHADER_STAGE_INDEX_FRAGMENT, pixelShader);
	if (geometryShader)
		uniformData_updateUniformVars(VulkanRendererConst::SHADER_STAGE_INDEX_GEOMETRY, geometryShader);
	// store where the read pointer should go after command buffer execution
	m_cmdBufferUniformRingbufIndices[m_commandBufferIndex] = m_uniformVarBufferWriteIndex;

	// process index data
	const LattePrimitiveMode primitiveMode = static_cast<LattePrimitiveMode>(LatteGPUState.contextRegister[mmVGT_PRIMITIVE_TYPE]);

	Renderer::INDEX_TYPE hostIndexType;
	uint32 hostIndexCount;
	uint32 indexMin = 0;
	uint32 indexMax = 0;
	uint32 indexBufferOffset = 0;
	uint32 indexBufferIndex = 0;
	LatteIndices_decode(memory_getPointerFromVirtualOffset(indexDataMPTR), indexType, count, primitiveMode, indexMin, indexMax, hostIndexType, hostIndexCount, indexBufferOffset, indexBufferIndex);

	// update index binding
	bool isPrevIndexData = false;
	if (hostIndexType != INDEX_TYPE::NONE)
	{
		if (m_state.activeIndexBufferOffset != indexBufferOffset || m_state.activeIndexBufferIndex != indexBufferIndex || m_state.activeIndexType != hostIndexType)
		{
			m_state.activeIndexType = hostIndexType;
			m_state.activeIndexBufferOffset = indexBufferOffset;
			m_state.activeIndexBufferIndex = indexBufferIndex;
			VkIndexType vkType;
			if (hostIndexType == INDEX_TYPE::U16)
				vkType = VK_INDEX_TYPE_UINT16;
			else if (hostIndexType == INDEX_TYPE::U32)
				vkType = VK_INDEX_TYPE_UINT32;
			else
				cemu_assert(false);
			vkCmdBindIndexBuffer(m_state.currentCommandBuffer, memoryManager->getIndexAllocator().GetBufferByIndex(indexBufferIndex), indexBufferOffset, vkType);
		}
		else
			isPrevIndexData = true;
	}

	if (m_useHostMemoryForCache)
	{
		// direct memory access (Wii U memory space imported as a Vulkan buffer), update buffer bindings
		draw_updateVertexBuffersDirectAccess();
		LatteDecompilerShader* vertexShader = LatteSHRC_GetActiveVertexShader();
		if (vertexShader)
			draw_updateUniformBuffersDirectAccess(vertexShader, mmSQ_VTX_UNIFORM_BLOCK_START, LatteConst::ShaderType::Vertex);
		LatteDecompilerShader* geometryShader = LatteSHRC_GetActiveGeometryShader();
		if (geometryShader)
			draw_updateUniformBuffersDirectAccess(geometryShader, mmSQ_GS_UNIFORM_BLOCK_START, LatteConst::ShaderType::Geometry);
		LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
		if (pixelShader)
			draw_updateUniformBuffersDirectAccess(pixelShader, mmSQ_PS_UNIFORM_BLOCK_START, LatteConst::ShaderType::Pixel);
	}
	else
	{
		// synchronize vertex and uniform cache and update buffer bindings
		LatteBufferCache_Sync(indexMin + baseVertex, indexMax + baseVertex, baseInstance, instanceCount);
	}

	PipelineInfo* pipeline_info;

	if (!isFirst)
	{
		if (m_state.activePipelineInfo->minimalStateHash != draw_calculateMinimalGraphicsPipelineHash(vertexShader->compatibleFetchShader, LatteGPUState.contextNew))
		{
			// pipeline changed
			pipeline_info = draw_getOrCreateGraphicsPipeline(count);
			m_state.activePipelineInfo = pipeline_info;
		}
		else
		{
			pipeline_info = m_state.activePipelineInfo;
#ifdef CEMU_DEBUG_ASSERT
			auto pipeline_info2 = draw_getOrCreateGraphicsPipeline(count);
			if (pipeline_info != pipeline_info2)
			{
				cemu_assert_debug(false);
			}
#endif
		}
	}
	else
	{
		pipeline_info = draw_getOrCreateGraphicsPipeline(count);
		m_state.activePipelineInfo = pipeline_info;
	}

	auto vkObjPipeline = pipeline_info->m_vkrObjPipeline;

	if (vkObjPipeline->pipeline == VK_NULL_HANDLE)
	{
		// invalid/uninitialized pipeline
		m_state.activeVertexDS = nullptr;
		return;
	}


	VkDescriptorSetInfo *vertexDS = nullptr, *pixelDS = nullptr, *geometryDS = nullptr;
	if (!isFirst && m_state.activeVertexDS)
	{
		vertexDS = m_state.activeVertexDS;
		pixelDS = m_state.activePixelDS;
		geometryDS = m_state.activeGeometryDS;
		m_state.descriptorSetsChanged = false;
	}
	else
	{
		draw_prepareDescriptorSets(pipeline_info, vertexDS, pixelDS, geometryDS);
		m_state.activeVertexDS = vertexDS;
		m_state.activePixelDS = pixelDS;
		m_state.activeGeometryDS = geometryDS;
		m_state.descriptorSetsChanged = true;
	}

	draw_setRenderPass();

	if (m_state.currentPipeline != vkObjPipeline->pipeline)
	{
		vkCmdBindPipeline(m_state.currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkObjPipeline->pipeline);
		vkObjPipeline->flagForCurrentCommandBuffer();
		m_state.currentPipeline = vkObjPipeline->pipeline;
		// depth bias
		if (pipeline_info->usesDepthBias)
			draw_updateDepthBias(true);
	}
	else
	{
		if (pipeline_info->usesDepthBias)
			draw_updateDepthBias(false);
	}

	// update blend constants
	if (pipeline_info->usesBlendConstants)
		draw_updateVkBlendConstants();

	// update descriptor sets
	uint32_t dynamicOffsets[17 * 2];
	if (vertexDS && pixelDS)
	{
		// update vertex and pixel descriptor set in a single call to vkCmdBindDescriptorSets
		sint32 numDynOffsetsVS;
		sint32 numDynOffsetsPS;
		draw_prepareDynamicOffsetsForDescriptorSet(VulkanRendererConst::SHADER_STAGE_INDEX_VERTEX, dynamicOffsets, numDynOffsetsVS,
			pipeline_info);
		draw_prepareDynamicOffsetsForDescriptorSet(VulkanRendererConst::SHADER_STAGE_INDEX_FRAGMENT, dynamicOffsets + numDynOffsetsVS, numDynOffsetsPS,
			pipeline_info);

		VkDescriptorSet dsArray[2];
		dsArray[0] = vertexDS->m_vkObjDescriptorSet->descriptorSet;
		dsArray[1] = pixelDS->m_vkObjDescriptorSet->descriptorSet;

		vkCmdBindDescriptorSets(m_state.currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vkObjPipeline->pipeline_layout, 0, 2, dsArray, numDynOffsetsVS + numDynOffsetsPS,
			dynamicOffsets);
	}
	else if (vertexDS)
	{
		sint32 numDynOffsets;
		draw_prepareDynamicOffsetsForDescriptorSet(VulkanRendererConst::SHADER_STAGE_INDEX_VERTEX, dynamicOffsets, numDynOffsets,
			pipeline_info);
		vkCmdBindDescriptorSets(m_state.currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vkObjPipeline->pipeline_layout, 0, 1, &vertexDS->m_vkObjDescriptorSet->descriptorSet, numDynOffsets,
			dynamicOffsets);
	}
	else if (pixelDS)
	{
		sint32 numDynOffsets;
		draw_prepareDynamicOffsetsForDescriptorSet(VulkanRendererConst::SHADER_STAGE_INDEX_FRAGMENT, dynamicOffsets, numDynOffsets,
			pipeline_info);
		vkCmdBindDescriptorSets(m_state.currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vkObjPipeline->pipeline_layout, 1, 1, &pixelDS->m_vkObjDescriptorSet->descriptorSet, numDynOffsets,
			dynamicOffsets);
	}
	if (geometryDS)
	{
		sint32 numDynOffsets;
		draw_prepareDynamicOffsetsForDescriptorSet(VulkanRendererConst::SHADER_STAGE_INDEX_GEOMETRY, dynamicOffsets, numDynOffsets,
			pipeline_info);
		vkCmdBindDescriptorSets(m_state.currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vkObjPipeline->pipeline_layout, 2, 1, &geometryDS->m_vkObjDescriptorSet->descriptorSet, numDynOffsets,
			dynamicOffsets);
	}

	// draw
	if (hostIndexType != INDEX_TYPE::NONE)
		vkCmdDrawIndexed(m_state.currentCommandBuffer, hostIndexCount, instanceCount, 0, baseVertex, baseInstance);
	else
		vkCmdDraw(m_state.currentCommandBuffer, count, instanceCount, baseVertex, baseInstance);

	LatteStreamout_FinishDrawcall(m_useHostMemoryForCache);

	LatteGPUState.drawCallCounter++;
}

// used in place of vertex/uniform caching when direct memory access is possible
void VulkanRenderer::draw_updateVertexBuffersDirectAccess()
{
	LatteFetchShader* parsedFetchShader = LatteSHRC_GetActiveFetchShader();
	if (!parsedFetchShader)
		return;
	for (auto& bufferGroup : parsedFetchShader->bufferGroups)
	{
		uint32 bufferIndex = bufferGroup.attributeBufferIndex;
		uint32 bufferBaseRegisterIndex = mmSQ_VTX_ATTRIBUTE_BLOCK_START + bufferIndex * 7;
		MPTR bufferAddress = LatteGPUState.contextRegister[bufferBaseRegisterIndex + 0];
		uint32 bufferSize = LatteGPUState.contextRegister[bufferBaseRegisterIndex + 1] + 1;
		uint32 bufferStride = (LatteGPUState.contextRegister[bufferBaseRegisterIndex + 2] >> 11) & 0xFFFF;

		if (bufferAddress == MPTR_NULL) [[unlikely]]
		{
			bufferAddress = 0x10000000;
		}
		if (m_state.currentVertexBinding[bufferIndex].offset == bufferAddress)
			continue;
		cemu_assert_debug(bufferAddress < 0x50000000);
		VkBuffer attrBuffer = m_importedMem;
		VkDeviceSize attrOffset = bufferAddress - m_importedMemBaseAddress;
		vkCmdBindVertexBuffers(m_state.currentCommandBuffer, bufferIndex, 1, &attrBuffer, &attrOffset);
	}
}

void VulkanRenderer::draw_updateUniformBuffersDirectAccess(LatteDecompilerShader* shader, const uint32 uniformBufferRegOffset, LatteConst::ShaderType shaderType)
{
	if (shader->uniformMode == LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK)
	{
		for(const auto& buf : shader->list_quickBufferList)
		{
			sint32 i = buf.index;
			MPTR physicalAddr = LatteGPUState.contextRegister[uniformBufferRegOffset + i * 7 + 0];
			uint32 uniformSize = LatteGPUState.contextRegister[uniformBufferRegOffset + i * 7 + 1] + 1;

			if (physicalAddr == MPTR_NULL) [[unlikely]]
			{
				cemu_assert_unimplemented();
				continue;
			}
			uniformSize = std::min<uint32>(uniformSize, buf.size);

			cemu_assert_debug(physicalAddr < 0x50000000);

			uint32 bufferIndex = i;
			cemu_assert_debug(bufferIndex < 16);

			switch (shaderType)
			{
			case LatteConst::ShaderType::Vertex:
				dynamicOffsetInfo.shaderUB[VulkanRendererConst::SHADER_STAGE_INDEX_VERTEX].uniformBufferOffset[bufferIndex] = physicalAddr - m_importedMemBaseAddress;
				break;
			case LatteConst::ShaderType::Geometry:
				dynamicOffsetInfo.shaderUB[VulkanRendererConst::SHADER_STAGE_INDEX_GEOMETRY].uniformBufferOffset[bufferIndex] = physicalAddr - m_importedMemBaseAddress;
				break;
			case LatteConst::ShaderType::Pixel:
				dynamicOffsetInfo.shaderUB[VulkanRendererConst::SHADER_STAGE_INDEX_FRAGMENT].uniformBufferOffset[bufferIndex] = physicalAddr - m_importedMemBaseAddress;
				break;
			default:
				UNREACHABLE;
			}
		}
	}
}

void VulkanRenderer::draw_endSequence()
{
	LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
	// post-drawcall logic
	if (pixelShader)
		LatteRenderTarget_trackUpdates();
	bool hasReadback = LatteTextureReadback_Update();
	m_recordedDrawcalls++;
	if (m_recordedDrawcalls >= m_submitThreshold || hasReadback)
	{
		SubmitCommandBuffer();
	}
}

void VulkanRenderer::debug_genericBarrier()
{
	draw_endRenderPass();

	VkMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
		VK_ACCESS_INDEX_READ_BIT |
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
		VK_ACCESS_UNIFORM_READ_BIT |
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
		VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_SHADER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_TRANSFER_READ_BIT |
		VK_ACCESS_TRANSFER_WRITE_BIT |
		VK_ACCESS_HOST_READ_BIT |
		VK_ACCESS_HOST_WRITE_BIT |
		VK_ACCESS_MEMORY_READ_BIT |
		VK_ACCESS_MEMORY_WRITE_BIT;

	memoryBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
		VK_ACCESS_INDEX_READ_BIT |
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
		VK_ACCESS_UNIFORM_READ_BIT |
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
		VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_SHADER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_TRANSFER_READ_BIT |
		VK_ACCESS_TRANSFER_WRITE_BIT |
		VK_ACCESS_HOST_READ_BIT |
		VK_ACCESS_HOST_WRITE_BIT |
		VK_ACCESS_MEMORY_READ_BIT |
		VK_ACCESS_MEMORY_WRITE_BIT;

	vkCmdPipelineBarrier(m_state.currentCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
}
