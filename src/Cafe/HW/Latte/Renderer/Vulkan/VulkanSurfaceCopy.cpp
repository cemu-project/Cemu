#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

struct CopyShaderPushConstantData_t
{
	float vertexOffsets[4 * 2];
	sint32 srcTexelOffset[2];
};

struct CopySurfacePipelineInfo
{
	template<typename T>
	struct TexSliceMipMapping
	{
		TexSliceMipMapping(LatteTextureVk* texture) : m_texture(texture) {};
		~TexSliceMipMapping()
		{
			//delete vkObjPipeline;
			//delete vkObjRenderPass;
			for (auto itr : m_array)
			{
				if (itr != nullptr)
					delete itr;
			}
		}

		T* create(sint32 sliceIndex, sint32 mipIndex)
		{
			sint32 idx = m_texture->GetSliceMipArrayIndex(sliceIndex, mipIndex);
			if (idx >= m_array.size())
				m_array.resize(idx + 1);
			T* v = new T();
			m_array[idx] = v;
			return v;
		}

		T* get(sint32 sliceIndex, sint32 mipIndex) const
		{
			sint32 idx = m_texture->GetSliceMipArrayIndex(sliceIndex, mipIndex);
			if (idx >= m_array.size())
				return nullptr;
			return m_array[idx];
		}

		TexSliceMipMapping(const TexSliceMipMapping&) = delete;
		TexSliceMipMapping& operator=(const TexSliceMipMapping&) = delete;
		TexSliceMipMapping(TexSliceMipMapping&& rhs)
		{
			m_texture = rhs.m_texture;
			m_array = std::move(rhs.m_array);
		}

		TexSliceMipMapping& operator=(TexSliceMipMapping&& rhs)
		{
			m_texture = rhs.m_texture;
			m_array = std::move(rhs.m_array);
		}

		LatteTextureVk* m_texture;
		std::vector<T*> m_array;
	};

	struct FramebufferValue
	{
		VKRObjectFramebuffer* vkObjFramebuffer;
		VKRObjectTextureView* vkObjImageView;
	};

	struct DescriptorValue
	{
		VKRObjectDescriptorSet* vkObjDescriptorSet;
		VKRObjectTextureView* vkObjImageView;
		//VKRObjectSampler* vkObjSampler;
	};

	CopySurfacePipelineInfo() = default;
	CopySurfacePipelineInfo(VkDevice device) : m_device(device) {}
	CopySurfacePipelineInfo(const CopySurfacePipelineInfo& info) = delete;

	VkDevice m_device = nullptr;

	VKRObjectPipeline* vkObjPipeline{};
	VKRObjectRenderPass* vkObjRenderPass{};

	// map of framebuffers used with this pipeline
	std::unordered_map<LatteTextureVk*, TexSliceMipMapping<FramebufferValue>> map_framebuffers;

	// map of descriptor sets used with this pipeline
	std::unordered_map<LatteTextureVk*, TexSliceMipMapping<DescriptorValue>> map_descriptors;
};

struct VkCopySurfaceState_t
{
	LatteTextureVk* sourceTexture;
	sint32 srcMip;
	sint32 srcSlice;
	LatteTextureVk* destinationTexture;
	sint32 dstMip;
	sint32 dstSlice;
	sint32 width;
	sint32 height;
};

extern std::atomic_int g_compiling_pipelines;

uint64 VulkanRenderer::copySurface_getPipelineStateHash(VkCopySurfaceState_t& state)
{
	uint64 h = 0;

	h += (uintptr_t)state.destinationTexture->GetFormat();
	h = std::rotr<uint64>(h, 7);

	h += state.sourceTexture->isDepth ? 0x1111ull : 0;
	h = std::rotr<uint64>(h, 7);
	h += state.destinationTexture->isDepth ? 0x1112ull : 0;
	h = std::rotr<uint64>(h, 7);

	return h;
}

CopySurfacePipelineInfo* VulkanRenderer::copySurface_getCachedPipeline(VkCopySurfaceState_t& state)
{
	const uint64 stateHash = copySurface_getPipelineStateHash(state);

	const auto it = m_copySurfacePipelineCache.find(stateHash);
	if (it == m_copySurfacePipelineCache.cend())
		return nullptr;
	return it->second;
}

RendererShaderVk* _vkGenSurfaceCopyShader_vs()
{
	const char* vsShaderSrc = 
		"#version 450\r\n"
		"layout(location = 0) out ivec2 passSrcTexelOffset;\r\n"
		"layout(push_constant) uniform pushConstants {\r\n"
		"vec2 vertexOffsets[4];\r\n"
		"ivec2 srcTexelOffset;\r\n"
		"}uf_pushConstants;\r\n"
		"\r\n"
		"void main(){\r\n"
		//"ivec2 tUV;\r\n"
		"vec2 tPOS;\r\n"
		"switch(gl_VertexIndex)"
		"{\r\n"
		// AMD driver has issues with indexed push constant access, therefore use this workaround
		"case 0: tPOS = uf_pushConstants.vertexOffsets[0].xy; break;\r\n"
		"case 1: tPOS = uf_pushConstants.vertexOffsets[1].xy; break;\r\n"
		"case 2: tPOS = uf_pushConstants.vertexOffsets[3].xy; break;\r\n"
		"case 3: tPOS = uf_pushConstants.vertexOffsets[0].xy; break;\r\n"
		"case 4: tPOS = uf_pushConstants.vertexOffsets[2].xy; break;\r\n"
		"case 5: tPOS = uf_pushConstants.vertexOffsets[3].xy; break;\r\n"
		"}"
		"passSrcTexelOffset = uf_pushConstants.srcTexelOffset;\r\n"
		"gl_Position = vec4(tPOS, 0, 1.0);\r\n"
		"}\r\n";

	std::string shaderStr(vsShaderSrc);
	auto vkShader = new RendererShaderVk(RendererShader::ShaderType::kVertex, 0, 0, false, false, shaderStr);
	vkShader->PreponeCompilation(true);
	return vkShader;
}

RendererShaderVk* _vkGenSurfaceCopyShader_ps_colorToDepth()
{
	const char* psShaderSrc = ""
		"#version 450\r\n"
		"layout(location = 0) in flat ivec2 passSrcTexelOffset;\r\n"
		"layout(binding = 0) uniform sampler2D textureSrc;\r\n"
		"in vec4 gl_FragCoord;\r\n"
		"\r\n"
		"void main(){\r\n"
		"gl_FragDepth = texelFetch(textureSrc, passSrcTexelOffset + ivec2(gl_FragCoord.xy), 0).r;\r\n"
		"}\r\n";

	std::string shaderStr(psShaderSrc);
	auto vkShader = new RendererShaderVk(RendererShader::ShaderType::kFragment, 0, 0, false, false, shaderStr);
	vkShader->PreponeCompilation(true);
	return vkShader;
}

RendererShaderVk* _vkGenSurfaceCopyShader_ps_depthToColor()
{
	const char* psShaderSrc = ""
		"#version 450\r\n"
		"layout(location = 0) in flat ivec2 passSrcTexelOffset;\r\n"
		"layout(binding = 0) uniform sampler2D textureSrc;\r\n"
		"layout(location = 0) out vec4 colorOut0;\r\n"
		"in vec4 gl_FragCoord;\r\n"
		"\r\n"
		"void main(){\r\n"
		"colorOut0.r = texelFetch(textureSrc, passSrcTexelOffset + ivec2(gl_FragCoord.xy), 0).r;\r\n"
		"}\r\n";

	std::string shaderStr(psShaderSrc);
	auto vkShader = new RendererShaderVk(RendererShader::ShaderType::kFragment, 0, 0, false, false, shaderStr);
	vkShader->PreponeCompilation(true);
	return vkShader;
}

VKRObjectRenderPass* VulkanRenderer::copySurface_createRenderpass(VkCopySurfaceState_t& state)
{
	VKRObjectRenderPass::AttachmentInfo_t attachmentInfo{};

	if (state.destinationTexture->isDepth)
	{
		attachmentInfo.depthAttachment.viewObj = ((LatteTextureViewVk*)state.destinationTexture->baseView)->GetViewRGBA();
		attachmentInfo.depthAttachment.format = state.destinationTexture->GetFormat();
		attachmentInfo.depthAttachment.hasStencil = state.destinationTexture->hasStencil;
	}
	else
	{
		attachmentInfo.colorAttachment[0].viewObj = ((LatteTextureViewVk*)state.destinationTexture->baseView)->GetViewRGBA();
		attachmentInfo.colorAttachment[0].format = state.destinationTexture->GetFormat();
	}

	VKRObjectRenderPass* vkObjRenderPass = new VKRObjectRenderPass(attachmentInfo, 1);

	return vkObjRenderPass;
}

CopySurfacePipelineInfo* VulkanRenderer::copySurface_getOrCreateGraphicsPipeline(VkCopySurfaceState_t& state)
{
	auto cache_object = copySurface_getCachedPipeline(state);
	if (cache_object != nullptr)
		return cache_object;

	if (defaultShaders.copySurface_vs == nullptr)
	{
		// on first call generate shaders
		defaultShaders.copySurface_vs = _vkGenSurfaceCopyShader_vs();
		defaultShaders.copySurface_psColor2Depth = _vkGenSurfaceCopyShader_ps_colorToDepth(); 
		defaultShaders.copySurface_psDepth2Color = _vkGenSurfaceCopyShader_ps_depthToColor();
	}

	RendererShaderVk* vertexShader = defaultShaders.copySurface_vs;
	RendererShaderVk* pixelShader = nullptr;
	if (state.sourceTexture->isDepth && !state.destinationTexture->isDepth)
		pixelShader = defaultShaders.copySurface_psDepth2Color;
	else if (!state.sourceTexture->isDepth && state.destinationTexture->isDepth)
		pixelShader = defaultShaders.copySurface_psColor2Depth;
	else
	{
		cemu_assert(false);
	}

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	shaderStages.emplace_back(CreatePipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader->GetShaderModule(), "main"));
	shaderStages.emplace_back(CreatePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, pixelShader->GetShaderModule(), "main"));

	// ##########################################################################################################################################

	const uint64 stateHash = copySurface_getPipelineStateHash(state);

	CopySurfacePipelineInfo* copyPipeline = new CopySurfacePipelineInfo();

	m_copySurfacePipelineCache.try_emplace(stateHash, copyPipeline);

	VKRObjectPipeline* vkObjPipeline = new VKRObjectPipeline();

	// ##########################################################################################################################################

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	// ##########################################################################################################################################

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.primitiveRestartEnable = VK_FALSE;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// ##########################################################################################################################################

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// ##########################################################################################################################################

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	// ##########################################################################################################################################

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// ##########################################################################################################################################

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	VkPipelineColorBlendAttachmentState blendAttachment{};

	if (!state.destinationTexture->isDepth)
	{
		blendAttachment.blendEnable = VK_FALSE;
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &blendAttachment;
		colorBlending.logicOpEnable = VK_FALSE;
	}


	// ##########################################################################################################################################

	std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings;

	VkDescriptorSetLayoutBinding entry{};
	entry.binding = 0;
	entry.descriptorCount = 1;
	entry.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	entry.pImmutableSamplers = nullptr;
	entry.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	descriptorSetLayoutBindings.emplace_back(entry);

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = (uint32_t)descriptorSetLayoutBindings.size();
	layoutInfo.pBindings = descriptorSetLayoutBindings.data();

	if (vkCreateDescriptorSetLayout(m_logicalDevice, &layoutInfo, nullptr, &vkObjPipeline->pixelDSL) != VK_SUCCESS)
		UnrecoverableError(fmt::format("Failed to create descriptor set layout for surface copy shader").c_str());

	// ##########################################################################################################################################

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(CopyShaderPushConstantData_t);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &vkObjPipeline->pixelDSL;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	pipelineLayoutInfo.pushConstantRangeCount = 1;

	VkResult result = vkCreatePipelineLayout(m_logicalDevice, &pipelineLayoutInfo, nullptr, &vkObjPipeline->pipeline_layout);
	if (result != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Failed to create pipeline layout: {}", result);
		vkObjPipeline->pipeline = VK_NULL_HANDLE;
		return copyPipeline;
	}

	// ###################################################

	bool writeDepth = state.destinationTexture->isDepth;

	VkPipelineDepthStencilStateCreateInfo depthStencilState{};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthTestEnable = writeDepth ? VK_TRUE : VK_FALSE;
	depthStencilState.depthWriteEnable = writeDepth ? VK_TRUE : VK_FALSE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	depthStencilState.depthBoundsTestEnable = false;
	depthStencilState.minDepthBounds = 0.0f;
	depthStencilState.maxDepthBounds = 1.0f;

	depthStencilState.stencilTestEnable = VK_FALSE;

	// ##########################################################################################################################################

	std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
	dynamicState.pDynamicStates = dynamicStates.data();

	// ##########################################################################################################################################

	copyPipeline->vkObjRenderPass = copySurface_createRenderpass(state);
	vkObjPipeline->addRef(copyPipeline->vkObjRenderPass);

	// ###########################################################

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = state.destinationTexture->isDepth?nullptr:&colorBlending;
	pipelineInfo.layout = vkObjPipeline->pipeline_layout;
	pipelineInfo.renderPass = copyPipeline->vkObjRenderPass->m_renderPass;
	pipelineInfo.pDepthStencilState = &depthStencilState;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = nullptr;
	pipelineInfo.flags = 0;

	copyPipeline->vkObjPipeline = vkObjPipeline;

	result = vkCreateGraphicsPipelines(m_logicalDevice, m_pipeline_cache, 1, &pipelineInfo, nullptr, &copyPipeline->vkObjPipeline->pipeline);
	if (result != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Failed to create graphics pipeline for surface copy. Error {} Info:", (sint32)result);
		cemu_assert_debug(false);
		copyPipeline->vkObjPipeline->pipeline = VK_NULL_HANDLE;
	}
	//performanceMonitor.vk.numGraphicPipelines.increment();

	//m_pipeline_cache_semaphore.notify();

	return copyPipeline;
}

VKRObjectTextureView* VulkanRenderer::surfaceCopy_createImageView(LatteTextureVk* textureVk, uint32 sliceIndex, uint32 mipIndex)
{

	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = textureVk->GetImageObj()->m_image;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = textureVk->GetFormat();
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	if (textureVk->isDepth)
		viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	else
		viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCreateInfo.subresourceRange.baseMipLevel = mipIndex;
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.subresourceRange.baseArrayLayer = sliceIndex;
	viewCreateInfo.subresourceRange.layerCount = 1;
	VkImageView imageView;
	if (vkCreateImageView(m_logicalDevice, &viewCreateInfo, nullptr, &imageView) != VK_SUCCESS)
		UnrecoverableError("Failed to create framebuffer image view for copy surface operation");
	return new VKRObjectTextureView(textureVk->GetImageObj(), imageView);
}

VKRObjectFramebuffer* VulkanRenderer::surfaceCopy_getOrCreateFramebuffer(VkCopySurfaceState_t& state, CopySurfacePipelineInfo* pipelineInfo)
{
	auto itr = pipelineInfo->map_framebuffers.find(state.destinationTexture);
	if (itr != pipelineInfo->map_framebuffers.end())
	{
		auto p = itr->second.get(state.dstSlice, state.dstMip);
		if (p != nullptr)
			return p->vkObjFramebuffer;
	}

	// create view
	VKRObjectTextureView* vkObjTextureView = surfaceCopy_createImageView(state.destinationTexture, state.dstSlice, state.dstMip);

	// create new framebuffer
	sint32 effectiveWidth, effectiveHeight;
	state.destinationTexture->GetEffectiveSize(effectiveWidth, effectiveHeight, state.dstMip);

	std::array<VKRObjectTextureView*, 1> fbAttachments;
	fbAttachments[0] = vkObjTextureView;

	VKRObjectFramebuffer* vkObjFramebuffer = new VKRObjectFramebuffer(pipelineInfo->vkObjRenderPass, fbAttachments, Vector2i(effectiveWidth, effectiveHeight));

	// register
	auto insertResult = pipelineInfo->map_framebuffers.try_emplace(state.destinationTexture, state.destinationTexture);
	CopySurfacePipelineInfo::FramebufferValue* framebufferVal = insertResult.first->second.create(state.dstSlice, state.dstMip);
	framebufferVal->vkObjFramebuffer = vkObjFramebuffer;
	framebufferVal->vkObjImageView = vkObjTextureView;
	
	return vkObjFramebuffer;
}

VKRObjectDescriptorSet* VulkanRenderer::surfaceCopy_getOrCreateDescriptorSet(VkCopySurfaceState_t& state, CopySurfacePipelineInfo* pipelineInfo)
{
	auto itr = pipelineInfo->map_descriptors.find(state.sourceTexture);
	if (itr != pipelineInfo->map_descriptors.end())
	{
		auto p = itr->second.get(state.srcSlice, state.srcMip);
		if (p != nullptr)
			return p->vkObjDescriptorSet;
	}

	VKRObjectDescriptorSet* vkObjDescriptorSet = new VKRObjectDescriptorSet();

	// allocate new descriptor set
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &(pipelineInfo->vkObjPipeline->pixelDSL);

	if (vkAllocateDescriptorSets(m_logicalDevice, &allocInfo, &vkObjDescriptorSet->descriptorSet) != VK_SUCCESS)
	{
		UnrecoverableError("failed to allocate descriptor set for surface copy operation");
	}

	// create view
	VKRObjectTextureView* vkObjImageView = surfaceCopy_createImageView(state.sourceTexture, state.srcSlice, state.srcMip);
	vkObjDescriptorSet->addRef(vkObjImageView);

	// create sampler
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = 0;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.mipLodBias = 0;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

	if (vkCreateSampler(m_logicalDevice, &samplerInfo, nullptr, &vkObjImageView->m_textureDefaultSampler[0]) != VK_SUCCESS)
		UnrecoverableError("Failed to create texture sampler for surface copy operation");

	// create descriptor image info
	VkDescriptorImageInfo descriptorImageInfo{};

	descriptorImageInfo.sampler = vkObjImageView->m_textureDefaultSampler[0];
	descriptorImageInfo.imageView = vkObjImageView->m_textureImageView;
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet write_descriptor{};
	write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_descriptor.dstSet = vkObjDescriptorSet->descriptorSet;
	write_descriptor.dstBinding = 0;
	write_descriptor.dstArrayElement = 0;
	write_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write_descriptor.descriptorCount = 1;
	write_descriptor.pImageInfo = &descriptorImageInfo;

	vkUpdateDescriptorSets(m_logicalDevice, 1, &write_descriptor, 0, nullptr);

	// register
	auto insertResult = pipelineInfo->map_descriptors.try_emplace(state.sourceTexture, state.sourceTexture);
	CopySurfacePipelineInfo::DescriptorValue* descriptorValue = insertResult.first->second.create(state.srcSlice, state.srcMip);
	descriptorValue->vkObjDescriptorSet = vkObjDescriptorSet;
	descriptorValue->vkObjImageView = vkObjImageView;
	
	return vkObjDescriptorSet;
}

void VulkanRenderer::surfaceCopy_viaDrawcall(LatteTextureVk* srcTextureVk, sint32 texSrcMip, sint32 texSrcSlice, LatteTextureVk* dstTextureVk, sint32 texDstMip, sint32 texDstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight)
{
	draw_endRenderPass();

	//debug_printf("surfaceCopy_viaDrawcall Src %04d %04d Dst %04d %04d CopySize %04d %04d\n", srcTextureVk->width, srcTextureVk->height, dstTextureVk->width, dstTextureVk->height, effectiveCopyWidth, effectiveCopyHeight);


	VkImageSubresourceLayers srcImageSubresource;
	srcImageSubresource.aspectMask = srcTextureVk->GetImageAspect();
	srcImageSubresource.baseArrayLayer = texSrcSlice;
	srcImageSubresource.mipLevel = texSrcMip;
	srcImageSubresource.layerCount = 1;

	VkImageSubresourceLayers dstImageSubresource;
	dstImageSubresource.aspectMask = dstTextureVk->GetImageAspect();
	dstImageSubresource.baseArrayLayer = texDstSlice;
	dstImageSubresource.mipLevel = texDstMip;
	dstImageSubresource.layerCount = 1;

	VkCopySurfaceState_t copySurfaceState;
	copySurfaceState.sourceTexture = srcTextureVk;
	copySurfaceState.srcSlice = texSrcSlice;
	copySurfaceState.srcMip = texSrcMip;
	copySurfaceState.destinationTexture = dstTextureVk;
	copySurfaceState.dstSlice = texDstSlice;
	copySurfaceState.dstMip = texDstMip;
	copySurfaceState.width = effectiveCopyWidth;
	copySurfaceState.height = effectiveCopyHeight;

	CopySurfacePipelineInfo* copySurfacePipelineInfo = copySurface_getOrCreateGraphicsPipeline(copySurfaceState);

	// get framebuffer
	VKRObjectFramebuffer* vkObjFramebuffer = surfaceCopy_getOrCreateFramebuffer(copySurfaceState, copySurfacePipelineInfo);
	vkObjFramebuffer->flagForCurrentCommandBuffer();

	// get descriptor set
	VKRObjectDescriptorSet* vkObjDescriptorSet = surfaceCopy_getOrCreateDescriptorSet(copySurfaceState, copySurfacePipelineInfo);
	
	sint32 dstEffectiveWidth, dstEffectiveHeight;
	dstTextureVk->GetEffectiveSize(dstEffectiveWidth, dstEffectiveHeight, texDstMip);

	sint32 srcEffectiveWidth, srcEffectiveHeight;
	srcTextureVk->GetEffectiveSize(srcEffectiveWidth, srcEffectiveHeight, texSrcMip);

	CopyShaderPushConstantData_t pushConstantData;

	float srcCopyWidth = (float)1.0f;
	float srcCopyHeight = (float)1.0f;
	// q0 vertex
	pushConstantData.vertexOffsets[0] = -1.0f;
	pushConstantData.vertexOffsets[1] = 1.0f;
	// q1
	pushConstantData.vertexOffsets[2] = 1.0f;
	pushConstantData.vertexOffsets[3] = 1.0f;
	// q2
	pushConstantData.vertexOffsets[4] = -1.0f;
	pushConstantData.vertexOffsets[5] = -1.0f;
	// q3
	pushConstantData.vertexOffsets[6] = 1.0f;
	pushConstantData.vertexOffsets[7] = -1.0f;

	pushConstantData.srcTexelOffset[0] = 0;
	pushConstantData.srcTexelOffset[1] = 0;

	vkCmdPushConstants(m_state.currentCommandBuffer, copySurfacePipelineInfo->vkObjPipeline->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstantData), &pushConstantData);

	// draw
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = copySurfacePipelineInfo->vkObjRenderPass->m_renderPass;
	renderPassInfo.framebuffer = vkObjFramebuffer->m_frameBuffer;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = { (uint32_t)effectiveCopyWidth, (uint32_t)effectiveCopyHeight };
	renderPassInfo.clearValueCount = 0;

	VkViewport viewport{};
	viewport.x = 0;
	viewport.y = (float)effectiveCopyHeight;
	viewport.width = (float)effectiveCopyWidth;
	viewport.height = (float)-effectiveCopyHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = effectiveCopyWidth;
	scissor.extent.height = effectiveCopyHeight;

	vkCmdSetViewport(m_state.currentCommandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(m_state.currentCommandBuffer, 0, 1, &scissor);

	cemu_assert_debug(srcTextureVk->GetImageObj()->m_image != dstTextureVk->GetImageObj()->m_image);

	barrier_image<SYNC_OP::IMAGE_WRITE | SYNC_OP::ANY_TRANSFER, SYNC_OP::IMAGE_READ>(srcTextureVk, srcImageSubresource, VK_IMAGE_LAYOUT_GENERAL); // wait for any modifying operations on source image to complete
	barrier_image<SYNC_OP::IMAGE_READ | SYNC_OP::IMAGE_WRITE | SYNC_OP::ANY_TRANSFER, SYNC_OP::IMAGE_WRITE>(dstTextureVk, dstImageSubresource, VK_IMAGE_LAYOUT_GENERAL); // wait for any operations on destination image to complete

	
	vkCmdBeginRenderPass(m_state.currentCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(m_state.currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, copySurfacePipelineInfo->vkObjPipeline->pipeline);
	copySurfacePipelineInfo->vkObjPipeline->flagForCurrentCommandBuffer();

	m_state.currentPipeline = copySurfacePipelineInfo->vkObjPipeline->pipeline;

	vkCmdBindDescriptorSets(m_state.currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		copySurfacePipelineInfo->vkObjPipeline->pipeline_layout, 0, 1, &vkObjDescriptorSet->descriptorSet, 0, nullptr);
	vkObjDescriptorSet->flagForCurrentCommandBuffer();

	vkCmdDraw(m_state.currentCommandBuffer, 6, 1, 0, 0);

	vkCmdEndRenderPass(m_state.currentCommandBuffer);

	barrier_image<SYNC_OP::IMAGE_READ, SYNC_OP::IMAGE_READ | SYNC_OP::IMAGE_WRITE | SYNC_OP::ANY_TRANSFER>(srcTextureVk, srcImageSubresource, VK_IMAGE_LAYOUT_GENERAL); // wait for drawcall to complete before any other operations on the source image
	barrier_image<SYNC_OP::IMAGE_WRITE, SYNC_OP::IMAGE_READ | SYNC_OP::IMAGE_WRITE | SYNC_OP::ANY_TRANSFER>(dstTextureVk, dstImageSubresource, VK_IMAGE_LAYOUT_GENERAL); // wait for drawcall to complete before any other operations on the destination image

	// restore viewport and scissor box
	vkCmdSetViewport(m_state.currentCommandBuffer, 0, 1, &m_state.currentViewport);
	vkCmdSetScissor(m_state.currentCommandBuffer, 0, 1, &m_state.currentScissorRect);

	LatteTexture_TrackTextureGPUWrite(dstTextureVk, texDstSlice, texDstMip, LatteTexture_getNextUpdateEventCounter());
}

struct vkComponentDesc_t
{
	enum class TYPE : uint8
	{
		NONE,
		UNORM,
		SNORM,
		FLOAT
	};
	uint8 bits;
	TYPE type;

	vkComponentDesc_t(uint8 b, TYPE t) : bits(b), type(t) {};
	friend bool operator==(const vkComponentDesc_t& lhs, const vkComponentDesc_t& rhs)
	{
		return lhs.bits == rhs.bits && lhs.type == rhs.type;
	}
};

bool vkIsDepthFormat(VkFormat imageFormat)
{
	switch (imageFormat)
	{
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT:
	case VK_FORMAT_D16_UNORM:
		return true;
	default:
		break;
	}
	return false;
}

vkComponentDesc_t vkGetFormatDepthBits(VkFormat imageFormat)
{
	switch (imageFormat)
	{
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return vkComponentDesc_t(32, vkComponentDesc_t::TYPE::FLOAT);
	case VK_FORMAT_D24_UNORM_S8_UINT:
		return vkComponentDesc_t(24, vkComponentDesc_t::TYPE::UNORM);
	case VK_FORMAT_D32_SFLOAT:
		return vkComponentDesc_t(32, vkComponentDesc_t::TYPE::FLOAT);
	case VK_FORMAT_D16_UNORM:
		return vkComponentDesc_t(16, vkComponentDesc_t::TYPE::UNORM);
	default:
		break;
	}
	return vkComponentDesc_t(0, vkComponentDesc_t::TYPE::NONE);
}

bool vkIsBitCompatibleColorDepthFormat(VkFormat format1, VkFormat format2)
{
	cemu_assert_debug(vkIsDepthFormat(format1) != vkIsDepthFormat(format2));

	VkFormat depthFormat, colorFormat;
	if (vkIsDepthFormat(format1))
	{
		depthFormat = format1;
		colorFormat = format2;
	}
	else
	{
		depthFormat = format2;
		colorFormat = format1;
	}

	switch (depthFormat)
	{
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return colorFormat == VK_FORMAT_R32_SFLOAT;
	case VK_FORMAT_D24_UNORM_S8_UINT:
		return false; // there is no 24-bit color format
	case VK_FORMAT_D32_SFLOAT:
		return colorFormat == VK_FORMAT_R32_SFLOAT;
	case VK_FORMAT_D16_UNORM:
		return colorFormat == VK_FORMAT_R16_UNORM;
	default:
		break;
	}
	return false;
}

void VulkanRenderer::surfaceCopy_copySurfaceWithFormatConversion(LatteTexture* sourceTexture, sint32 srcMip, sint32 srcSlice, LatteTexture* destinationTexture, sint32 dstMip, sint32 dstSlice, sint32 width, sint32 height)
{
	// scale copy size to effective size
	sint32 effectiveCopyWidth = width;
	sint32 effectiveCopyHeight = height;
	LatteTexture_scaleToEffectiveSize(sourceTexture, &effectiveCopyWidth, &effectiveCopyHeight, 0);
	sint32 sourceEffectiveWidth, sourceEffectiveHeight;
	sourceTexture->GetEffectiveSize(sourceEffectiveWidth, sourceEffectiveHeight, srcMip);

	sint32 texSrcMip = srcMip;
	sint32 texSrcSlice = srcSlice;
	sint32 texDstMip = dstMip;
	sint32 texDstSlice = dstSlice;

	LatteTextureVk* srcTextureVk = (LatteTextureVk*)sourceTexture;
	LatteTextureVk* dstTextureVk = (LatteTextureVk*)destinationTexture;

	// check if texture rescale ratios match
	// todo - if not, we have to use drawcall based copying
	if (!LatteTexture_doesEffectiveRescaleRatioMatch(srcTextureVk, texSrcMip, dstTextureVk, texDstMip))
	{
		cemuLog_logDebug(LogType::Force, "surfaceCopy_copySurfaceViaDrawcall(): Mismatching dimensions");
		return;
	}

	// check if bpp size matches
	if (srcTextureVk->GetBPP() != dstTextureVk->GetBPP())
	{
		cemuLog_logDebug(LogType::Force, "surfaceCopy_copySurfaceViaDrawcall(): Mismatching BPP");
		return;
	}

	surfaceCopy_viaDrawcall(srcTextureVk, texSrcMip, texSrcSlice, dstTextureVk, texDstMip, texDstSlice, effectiveCopyWidth, effectiveCopyHeight);
}

// called whenever a texture is destroyed
// it is guaranteed that the texture is not in use and all associated resources (descriptor sets, framebuffers) can be destroyed safely
void VulkanRenderer::surfaceCopy_notifyTextureRelease(LatteTextureVk* hostTexture)
{
	for (auto& itr : m_copySurfacePipelineCache)
	{
		auto& pipelineInfo = itr.second;
		
		auto itrDescriptors = pipelineInfo->map_descriptors.find(hostTexture);
		if (itrDescriptors != pipelineInfo->map_descriptors.end())
		{
			for (auto p : itrDescriptors->second.m_array)
			{
				if (p)
				{
					VulkanRenderer::GetInstance()->ReleaseDestructibleObject(p->vkObjDescriptorSet);
					p->vkObjDescriptorSet = nullptr;
					VulkanRenderer::GetInstance()->ReleaseDestructibleObject(p->vkObjImageView);
					p->vkObjImageView = nullptr;
				}
			}
			pipelineInfo->map_descriptors.erase(itrDescriptors);
		}

		auto itrFramebuffers = pipelineInfo->map_framebuffers.find(hostTexture);
		if (itrFramebuffers != pipelineInfo->map_framebuffers.end())
		{
			for (auto p : itrFramebuffers->second.m_array)
			{
				if (p)
				{
					VulkanRenderer::GetInstance()->ReleaseDestructibleObject(p->vkObjFramebuffer);
					p->vkObjFramebuffer = nullptr;
					VulkanRenderer::GetInstance()->ReleaseDestructibleObject(p->vkObjImageView);
					p->vkObjImageView = nullptr;
				}
			}
			pipelineInfo->map_framebuffers.erase(itrFramebuffers);
		}
	}
}

void VulkanRenderer::surfaceCopy_cleanup()
{
	// todo - release m_copySurfacePipelineCache etc
}
