#pragma once

class PipelineCompiler : public VKRMoveableRefCounter
{
private:
	// helper functions
	VkFormat GetVertexFormat(uint8 format);
	bool ConsumesBlendConstants(VkBlendFactor blendFactor);

	void CreateDescriptorSetLayout(VulkanRenderer* vkRenderer, LatteDecompilerShader* shader, VkDescriptorSetLayout& layout, PipelineInfo* vkrPipelineInfo);

private:

	/* shader stages (requires compiled shader) */

	RendererShaderVk* m_rectEmulationGS{};

	bool InitShaderStages(VulkanRenderer* vkRenderer, RendererShaderVk* vkVertexShader, RendererShaderVk* vkPixelShader, RendererShaderVk* vkGeometryShader);

	/* vertex input state */

	void InitVertexInputState(const LatteContextRegister& latteRegister, LatteDecompilerShader* vertexShader, LatteFetchShader* fetchShader);
	void InitInputAssemblyState(const Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE primitiveMode);
	void InitViewportState();
	void InitRasterizerState(const LatteContextRegister& latteRegister, VulkanRenderer* vkRenderer, bool isPrimitiveRect, bool& usesDepthBias);
	void InitBlendState(const LatteContextRegister& latteRegister, PipelineInfo* pipelineInfo, bool& usesBlendConstants, VKRObjectRenderPass* renderPassObj);
	void InitDescriptorSetLayouts(VulkanRenderer* vkRenderer, PipelineInfo* vkrPipelineInfo, LatteDecompilerShader* vertexShader, LatteDecompilerShader* pixelShader, LatteDecompilerShader* geometryShader);
	void InitDepthStencilState();
	void InitDynamicState(PipelineInfo* pipelineInfo, bool usesBlendConstants, bool usesDepthBias);

public:
	PipelineCompiler();
	~PipelineCompiler();

	VKRObjectPipeline* m_vkrObjPipeline{};
	LatteFetchShader* m_fetchShader{};
	RendererShaderVk* m_vkVertexShader{};
	RendererShaderVk* m_vkPixelShader{};
	RendererShaderVk* m_vkGeometryShader{};

	bool InitFromCurrentGPUState(PipelineInfo* pipelineInfo, const LatteContextRegister& latteRegister, VKRObjectRenderPass* renderPassObj);
	void TrackAsCached(uint64 baseHash, uint64 pipelineStateHash); // stores pipeline to permanent cache if not yet cached. Must be called synchronously from render thread due to dependency on GPU state

	VkPipelineLayout m_pipeline_layout;
	VKRObjectRenderPass* m_renderPassObj{};

	/* shader stages */
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	/* vertex attribute description */
	std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescription;
	std::vector<VkVertexInputBindingDescription> vertexInputBindingDescription;
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};

	/* input assembly state */
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};

	/* viewport state */
	VkPipelineViewportStateCreateInfo viewportState{};

	/* rasterizer state */
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	VkPipelineRasterizationDepthClipStateCreateInfoEXT rasterizerExt{};
	VkPipelineMultisampleStateCreateInfo multisampling{};

	/* blend state */
	std::array<VkPipelineColorBlendAttachmentState, 8> colorBlendAttachments{};
	VkPipelineColorBlendStateCreateInfo colorBlending{};

	/* descriptor set layouts */
	VkDescriptorSetLayout descriptorSetLayout[3];
	sint32 descriptorSetLayoutCount = 0;

	/* depth stencil state */
	VkPipelineDepthStencilStateCreateInfo depthStencilState{};

	/* dynamic state */
	std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState = {};

	// returns true if the shader was compiled (even if errors occurred)
	bool Compile(bool forceCompile, bool isRenderThread, bool showInOverlay);

};