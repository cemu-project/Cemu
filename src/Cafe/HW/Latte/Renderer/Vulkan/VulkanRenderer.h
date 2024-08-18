#pragma once

#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/RendererShaderVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/LatteTextureVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/LatteTextureViewVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/CachedFBOVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VKRMemoryManager.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/SwapchainInfoVk.h"
#include "util/math/vector2.h"
#include "util/helpers/Semaphore.h"
#include "util/containers/flat_hash_map.hpp"
#include "util/containers/robin_hood.h"

struct VkSupportedFormatInfo_t
{
	bool fmt_d24_unorm_s8_uint{};
	bool fmt_r4g4_unorm_pack{};
	bool fmt_r5g6b5_unorm_pack{};
	bool fmt_r4g4b4a4_unorm_pack{};
	bool fmt_a1r5g5b5_unorm_pack{};
};

struct VkDescriptorSetInfo
{
	VKRObjectDescriptorSet* m_vkObjDescriptorSet{};

	~VkDescriptorSetInfo();

	std::vector<LatteTextureViewVk*> list_referencedViews;
	std::vector<LatteTextureVk*> list_fboCandidates; // prefiltered list of textures which may need a barrier
	LatteConst::ShaderType shaderType{};
	uint64 stateHash{};
	class PipelineInfo* pipeline_info{};

	// tracking for allocated descriptors
	uint8 statsNumSamplerTextures{ 0 };
	uint8 statsNumDynUniformBuffers{ 0 };
	uint8 statsNumStorageBuffers{ 0 };
};

class VkException : public std::runtime_error
{
public:
	VkException(VkResult result, const std::string& message)
		: runtime_error(message), m_result(result)
	{}

	VkResult GetResult() const { return m_result; }

private:
	VkResult m_result;
};

namespace VulkanRendererConst
{
	static const inline int SHADER_STAGE_INDEX_VERTEX = 0;
	static const inline int SHADER_STAGE_INDEX_FRAGMENT = 1;
	static const inline int SHADER_STAGE_INDEX_GEOMETRY = 2;
	static const inline int SHADER_STAGE_INDEX_COUNT = 3;
};

class PipelineInfo
{
public:
	PipelineInfo(uint64 minimalStateHash, uint64 pipelineHash, struct LatteFetchShader* fetchShader, LatteDecompilerShader* vertexShader, LatteDecompilerShader* pixelShader, LatteDecompilerShader* geometryShader);
	PipelineInfo(const PipelineInfo& info) = delete;
	~PipelineInfo();

	bool operator==(const PipelineInfo& pipeline_info) const
	{
		return true;
	}

	
	template<typename T>
	struct direct_hash
	{
		size_t operator()(const uint64& k) const noexcept 
		{
			return k;
		}
	};

	// std::unordered_map<uint64, VkDescriptorSetInfo*> 3.16% (total CPU time)
	// robin_hood::unordered_flat_map<uint64, VkDescriptorSetInfo*> vertex_ds_cache, pixel_ds_cache, geometry_ds_cache; ~1.80%
	// ska::bytell_hash_map<uint64, VkDescriptorSetInfo*, direct_hash<uint64>> vertex_ds_cache, pixel_ds_cache, geometry_ds_cache; -> 1.91%
	ska::flat_hash_map<uint64, VkDescriptorSetInfo*, direct_hash<uint64>> vertex_ds_cache, pixel_ds_cache, geometry_ds_cache; // 1.71%

	VKRObjectPipeline* m_vkrObjPipeline;

	LatteDecompilerShader* vertexShader = nullptr;
	LatteDecompilerShader* geometryShader = nullptr;
	LatteDecompilerShader* pixelShader = nullptr;
	LatteFetchShader* fetchShader = nullptr;
	Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE primitiveMode{};

	RendererShaderVk* vertexShaderVk = nullptr;
	RendererShaderVk* geometryShaderVk = nullptr;
	RendererShaderVk* pixelShaderVk = nullptr;

	uint64 minimalStateHash;
	uint64 stateHash;

	bool usesBlendConstants{ false };
	bool usesDepthBias{ false };

	struct
	{
		bool hasUniformVar[VulkanRendererConst::SHADER_STAGE_INDEX_COUNT];
		bool hasUniformBuffers[VulkanRendererConst::SHADER_STAGE_INDEX_COUNT];
		std::vector<uint8> list_uniformBuffers[VulkanRendererConst::SHADER_STAGE_INDEX_COUNT];
	}dynamicOffsetInfo{};

	// primitive rects emulation
	RendererShaderVk* rectEmulationGS = nullptr;

	// hack - accurate barrier needed for this pipeline
	bool neverSkipAccurateBarrier{false};
};

class VulkanRenderer : public Renderer
{
	friend class LatteQueryObjectVk;
	friend class LatteTextureReadbackInfoVk;
	friend class PipelineCompiler;

	using VSync = SwapchainInfoVk::VSync;

	static const inline int UNIFORMVAR_RINGBUFFER_SIZE = 1024 * 1024 * 16; // 16MB

	static const inline int TEXTURE_READBACK_SIZE = 32 * 1024 * 1024; // 32 MB

	static const inline int OCCLUSION_QUERY_POOL_SIZE = 1024;

public:

	// memory management
	VKRMemoryManager* memoryManager{};
	VKRMemoryManager* GetMemoryManager() const { return memoryManager; };

	VkSupportedFormatInfo_t m_supportedFormatInfo;

	typedef struct
	{
		// Vulkan image info
		VkFormat vkImageFormat;
		VkImageAspectFlags vkImageAspect;
		bool isCompressed;

		// texture decoder info
		TextureDecoder* decoder;

		sint32 texelCountX;
		sint32 texelCountY;
	}FormatInfoVK;

	struct DeviceInfo
	{
		DeviceInfo(const std::string name, uint8* uuid)
			: name(name)
		{
			std::copy(uuid, uuid + VK_UUID_SIZE, this->uuid.data());
		}

		std::string name;
		std::array<uint8, VK_UUID_SIZE> uuid;
	};

	static std::vector<DeviceInfo> GetDevices();
	VulkanRenderer();
	virtual ~VulkanRenderer();

	RendererAPI GetType() override { return RendererAPI::Vulkan; }

	static VulkanRenderer* GetInstance();

	void UnrecoverableError(const char* errMsg) const;

	void GetDeviceFeatures();
	void DetermineVendor();
	void InitializeSurface(const Vector2i& size, bool mainWindow);

	const std::unique_ptr<SwapchainInfoVk>& GetChainInfoPtr(bool mainWindow) const;
	SwapchainInfoVk& GetChainInfo(bool mainWindow) const;

	void StopUsingPadAndWait();
	bool IsPadWindowActive() override;

	void HandleScreenshotRequest(LatteTextureView* texView, bool padView) override;

	void QueryMemoryInfo();
	void QueryAvailableFormats();

#if BOOST_OS_WINDOWS
	static VkSurfaceKHR CreateWinSurface(VkInstance instance, HWND hwindow);
#endif
#if BOOST_OS_LINUX
	static VkSurfaceKHR CreateXlibSurface(VkInstance instance, Display* dpy, Window window);
    static VkSurfaceKHR CreateXcbSurface(VkInstance instance, xcb_connection_t* connection, xcb_window_t window);
	#ifdef HAS_WAYLAND
	static VkSurfaceKHR CreateWaylandSurface(VkInstance instance, wl_display* display, wl_surface* surface);
	#endif
#endif

	static VkSurfaceKHR CreateFramebufferSurface(VkInstance instance, struct WindowHandleInfo& windowInfo);

	void AppendOverlayDebugInfo() override;

	void ImguiInit();
	VkInstance GetVkInstance() const { return m_instance; }
	VkDevice GetLogicalDevice() const { return m_logicalDevice; }
	VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }

	VkDescriptorPool GetDescriptorPool() const { return m_descriptorPool; }

	void WaitDeviceIdle() const { vkDeviceWaitIdle(m_logicalDevice); }

	void Initialize() override;
	void Shutdown() override;

	void SwapBuffers(bool swapTV = true, bool swapDRC = true) override;

	void Flush(bool waitIdle = false) override;
	void NotifyLatteCommandProcessorIdle() override;

	uint64 GenUniqueId(); // return unique id (uses incrementing counter)

	void DrawEmptyFrame(bool mainWindow) override;

	void InitFirstCommandBuffer();
	void ProcessFinishedCommandBuffers();
	void WaitForNextFinishedCommandBuffer();
	void SubmitCommandBuffer(VkSemaphore signalSemaphore = VK_NULL_HANDLE, VkSemaphore waitSemaphore = VK_NULL_HANDLE);
	void RequestSubmitSoon();
	void RequestSubmitOnIdle();

	// command buffer synchronization
	uint64 GetCurrentCommandBufferId() const;
	bool HasCommandBufferFinished(uint64 commandBufferId) const;
	void WaitCommandBufferFinished(uint64 commandBufferId);

	// resource destruction queue
	void ReleaseDestructibleObject(VKRDestructibleObject* destructibleObject);
	void ProcessDestructionQueue();

	FSpinlock m_spinlockDestructionQueue;
	std::vector<VKRDestructibleObject*> m_destructionQueue;

	void PipelineCacheSaveThread(size_t cache_size);

	void ClearColorbuffer(bool padView) override;
	void ClearColorImageRaw(VkImage image, uint32 sliceIndex, uint32 mipIndex, const VkClearColorValue& color, VkImageLayout inputLayout, VkImageLayout outputLayout);
	void ClearColorImage(LatteTextureVk* vkTexture, uint32 sliceIndex, uint32 mipIndex, const VkClearColorValue& color, VkImageLayout outputLayout);

	void DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter, sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight, bool padView, bool clearBackground) override;
	void CreateDescriptorPool();
	VkDescriptorSet backbufferBlit_createDescriptorSet(VkDescriptorSetLayout descriptor_set_layout, LatteTextureViewVk* texViewVk, bool useLinearTexFilter);

	robin_hood::unordered_flat_map<uint64, robin_hood::unordered_flat_map<uint64, PipelineInfo*> > m_pipeline_info_cache; // using robin_hood::unordered_flat_map is twice as fast (1-2% overall CPU time reduction)
	void draw_debugPipelineHashState();
	PipelineInfo* draw_getCachedPipeline();

	// pipeline state hash
	static uint64 draw_calculateMinimalGraphicsPipelineHash(const LatteFetchShader* fetchShader, const LatteContextRegister& lcr);
	static uint64 draw_calculateGraphicsPipelineHash(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, const VKRObjectRenderPass* renderPassObj, const LatteContextRegister& lcr);

	// rendertarget
	void renderTarget_setViewport(float x, float y, float width, float height, float nearZ, float farZ, bool halfZ = false) override;
	void renderTarget_setScissor(sint32 scissorX, sint32 scissorY, sint32 scissorWidth, sint32 scissorHeight) override;

	LatteCachedFBO* rendertarget_createCachedFBO(uint64 key) override;
	void rendertarget_deleteCachedFBO(LatteCachedFBO* cfbo) override;
	void rendertarget_bindFramebufferObject(LatteCachedFBO* cfbo) override;

	// texture functions
	void* texture_acquireTextureUploadBuffer(uint32 size) override;
	void texture_releaseTextureUploadBuffer(uint8* mem) override;
	

	TextureDecoder* texture_chooseDecodedFormat(Latte::E_GX2SURFFMT format, bool isDepth, Latte::E_DIM dim, uint32 width, uint32 height) override;

	void texture_clearSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex) override;
	void texture_clearColorSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex, float r, float g, float b, float a) override;
	void texture_clearDepthSlice(LatteTexture* hostTexture, uint32 sliceIndex, sint32 mipIndex, bool clearDepth, bool clearStencil, float depthValue, uint32 stencilValue) override;

	void texture_loadSlice(LatteTexture* hostTexture, sint32 width, sint32 height, sint32 depth, void* pixelData, sint32 sliceIndex, sint32 mipIndex, uint32 compressedImageSize) override;

	LatteTexture* texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth) override;

	void texture_setLatteTexture(LatteTextureView* textureView, uint32 textureUnit) override;

	void texture_copyImageSubData(LatteTexture* src, sint32 srcMip, sint32 effectiveSrcX, sint32 effectiveSrcY, sint32 srcSlice, LatteTexture* dst, sint32 dstMip, sint32 effectiveDstX, sint32 effectiveDstY, sint32 dstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight, sint32 srcDepth) override;
	LatteTextureReadbackInfo* texture_createReadback(LatteTextureView* textureView) override;

	// surface copy
	void surfaceCopy_copySurfaceWithFormatConversion(LatteTexture* sourceTexture, sint32 srcMip, sint32 srcSlice, LatteTexture* destinationTexture, sint32 dstMip, sint32 dstSlice, sint32 width, sint32 height) override;
	void surfaceCopy_notifyTextureRelease(LatteTextureVk* hostTexture);

	private:
	void surfaceCopy_viaDrawcall(LatteTextureVk* srcTextureVk, sint32 texSrcMip, sint32 texSrcSlice, LatteTextureVk* dstTextureVk, sint32 texDstMip, sint32 texDstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight);

	void surfaceCopy_cleanup();

private:
	uint64 copySurface_getPipelineStateHash(struct VkCopySurfaceState_t& state);
	struct CopySurfacePipelineInfo* copySurface_getCachedPipeline(struct VkCopySurfaceState_t& state);
	struct CopySurfacePipelineInfo* copySurface_getOrCreateGraphicsPipeline(struct VkCopySurfaceState_t& state);
	VKRObjectTextureView* surfaceCopy_createImageView(LatteTextureVk* textureVk, uint32 sliceIndex, uint32 mipIndex);
	VKRObjectFramebuffer* surfaceCopy_getOrCreateFramebuffer(struct VkCopySurfaceState_t& state, struct CopySurfacePipelineInfo* pipelineInfo);
	VKRObjectDescriptorSet* surfaceCopy_getOrCreateDescriptorSet(struct VkCopySurfaceState_t& state, struct CopySurfacePipelineInfo* pipelineInfo);

	VKRObjectRenderPass* copySurface_createRenderpass(struct VkCopySurfaceState_t& state);

	std::unordered_map<uint64, struct CopySurfacePipelineInfo*> m_copySurfacePipelineCache;

public:
	// renderer interface
	void bufferCache_init(const sint32 bufferSize) override;
	void bufferCache_upload(uint8* buffer, sint32 size, uint32 bufferOffset) override;
	void bufferCache_copy(uint32 srcOffset, uint32 dstOffset, uint32 size) override;

	void buffer_bindVertexBuffer(uint32 bufferIndex, uint32 buffer, uint32 size) override;
	void buffer_bindVertexStrideWorkaroundBuffer(VkBuffer fixedBuffer, uint32 offset, uint32 bufferIndex, uint32 size);
	std::pair<VkBuffer, uint32> buffer_genStrideWorkaroundVertexBuffer(MPTR buffer, uint32 size, uint32 oldStride);
	void buffer_bindUniformBuffer(LatteConst::ShaderType shaderType, uint32 bufferIndex, uint32 offset, uint32 size) override;

	RendererShader* shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool isGameShader, bool isGfxPackShader) override;

	void* indexData_reserveIndexMemory(uint32 size, uint32& offset, uint32& bufferIndex) override;
	void indexData_uploadIndexMemory(uint32 offset, uint32 size) override;

	// externally callable
	void GetTextureFormatInfoVK(Latte::E_GX2SURFFMT format, bool isDepth, Latte::E_DIM dim, sint32 width, sint32 height, FormatInfoVK* formatInfoOut);
	void unregisterGraphicsPipeline(PipelineInfo* pipelineInfo);

private:
	struct VkRendererState
	{
		VkRendererState() = default;
		VkRendererState(const VkRendererState&) = delete;
		VkRendererState(VkRendererState&&) noexcept = delete;

		// textures
		LatteTextureViewVk* boundTexture[128]{};

		// rendertarget
		CachedFBOVk* activeFBO{}; // the FBO active for the emulated GPU

		// command buffer
		VkCommandBuffer currentCommandBuffer{};

		// pipeline
		VkPipeline currentPipeline{ VK_NULL_HANDLE };

		// renderpass
		CachedFBOVk* activeRenderpassFBO{}; // the FBO of the currently active Vulkan renderpass

		// drawcall state
		PipelineInfo* activePipelineInfo{ nullptr };
		VkDescriptorSetInfo* activeVertexDS{ nullptr };
		VkDescriptorSetInfo* activePixelDS{ nullptr };
		VkDescriptorSetInfo* activeGeometryDS{ nullptr };
		bool descriptorSetsChanged{ false };
		bool hasRenderSelfDependency{ false }; // set if current drawcall samples textures which are also output as a rendertarget

		// viewport and scissor box
		VkViewport currentViewport{};
		VkRect2D currentScissorRect{};

		// vertex bindings
		struct  
		{
			uint32 offset;
		}currentVertexBinding[LATTE_MAX_VERTEX_BUFFERS]{};

		// transform feedback
		bool hasActiveXfb{};

		// index buffer
		Renderer::INDEX_TYPE activeIndexType{};
		uint32 activeIndexBufferIndex{};
		uint32 activeIndexBufferOffset{};

		// polygon offset
		uint32 prevPolygonFrontOffsetU32{ 0xFFFFFFFF };
		uint32 prevPolygonFrontScaleU32{ 0xFFFFFFFF };
		uint32 prevPolygonFrontClampU32{ 0xFFFFFFFF };

		void resetCommandBufferState()
		{
			prevPolygonFrontOffsetU32 = 0xFFFFFFFF;
			prevPolygonFrontScaleU32 = 0xFFFFFFFF;
			prevPolygonFrontClampU32 = 0xFFFFFFFF;
			currentPipeline = VK_NULL_HANDLE;
			for (auto& itr : currentVertexBinding)
			{
				itr.offset = 0xFFFFFFFF;
			}
			activeIndexType = Renderer::INDEX_TYPE::NONE;
			activeIndexBufferIndex = std::numeric_limits<uint32>::max();
			activeIndexBufferOffset = std::numeric_limits<uint32>::max();
		}

		// invalidation / flushing
		uint64 currentFlushIndex{0};
		bool requestFlush{ false }; // flush after every draw operation. The renderpass dependencies dont handle dependencies across multiple drawcalls inside a single renderpass

		// draw sequence
		bool drawSequenceSkip; // if true, skip draw_execute()
	}m_state;

	std::unique_ptr<SwapchainInfoVk> m_mainSwapchainInfo{}, m_padSwapchainInfo{};
	std::atomic_flag m_destroyPadSwapchainNextAcquire{};
	bool IsSwapchainInfoValid(bool mainWindow) const;

	VkRenderPass m_imguiRenderPass = VK_NULL_HANDLE;

	VkDescriptorPool m_descriptorPool;

  public:
	struct QueueFamilyIndices
	{
		int32_t graphicsFamily = -1;
		int32_t presentFamily = -1;

		bool IsComplete() const	{ return graphicsFamily >= 0 && presentFamily >= 0;	}
	};
	static QueueFamilyIndices FindQueueFamilies(VkSurfaceKHR surface, VkPhysicalDevice device);

  private:

	struct FeatureControl
	{
		struct
		{
			// if using new optional extensions add to CheckDeviceExtensionSupport and CreateDeviceCreateInfo
			bool tooling_info = false; // VK_EXT_tooling_info
			bool transform_feedback = false;
			bool depth_range_unrestricted = false;
			bool nv_fill_rectangle = false; // NV_fill_rectangle
			bool pipeline_feedback = false;
			bool pipeline_creation_cache_control = false; // VK_EXT_pipeline_creation_cache_control
			bool custom_border_color = false; // VK_EXT_custom_border_color
			bool custom_border_color_without_format = false; // VK_EXT_custom_border_color (specifically customBorderColorWithoutFormat)
			bool cubic_filter = false; // VK_EXT_FILTER_CUBIC_EXTENSION_NAME
			bool driver_properties = false; // VK_KHR_driver_properties
			bool external_memory_host = false; // VK_EXT_external_memory_host
			bool synchronization2 = false; // VK_KHR_synchronization2
			bool dynamic_rendering = false; // VK_KHR_dynamic_rendering
			bool shader_float_controls = false; // VK_KHR_shader_float_controls
		}deviceExtensions;

		struct
		{
			bool shaderRoundingModeRTEFloat32{ false };
		}shaderFloatControls; // from VK_KHR_shader_float_controls

		struct  
		{
			bool debug_utils = false; // VK_EXT_DEBUG_UTILS
		}instanceExtensions;

		struct  
		{
			bool useTFEmulationViaSSBO = true; // emulate transform feedback via shader writes to a storage buffer
		}mode;

		struct  
		{
			uint32 minUniformBufferOffsetAlignment = 256;
			uint32 nonCoherentAtomSize = 256;
		}limits;

		bool debugMarkersSupported{ false }; // frame debugger is attached
		bool disableMultithreadedCompilation{ false }; // for old nvidia drivers

	}m_featureControl{};
	static bool CheckDeviceExtensionSupport(const VkPhysicalDevice device, FeatureControl& info);
	static std::vector<const char*> CheckInstanceExtensionSupport(FeatureControl& info);

	bool UpdateSwapchainProperties(bool mainWindow);
	void SwapBuffer(bool mainWindow);

	VkDescriptorSetLayout m_swapchainDescriptorSetLayout;

	VkQueue m_graphicsQueue, m_presentQueue;

	// swapchain

	std::vector<VkDeviceQueueCreateInfo> CreateQueueCreateInfos(const std::set<int>& uniqueQueueFamilies) const;
	VkDeviceCreateInfo CreateDeviceCreateInfo(const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos, const VkPhysicalDeviceFeatures& deviceFeatures, const void* deviceExtensionStructs, std::vector<const char*>& used_extensions) const;
	static bool IsDeviceSuitable(VkSurfaceKHR surface, const VkPhysicalDevice& device);

	void CreateCommandPool();
	void CreateCommandBuffers();

	void swapchain_createDescriptorSetLayout();
	
	// shader

	bool IsAsyncPipelineAllowed(uint32 numIndices);

	uint64 GetDescriptorSetStateHash(LatteDecompilerShader* shader);

	// imgui
	bool ImguiBegin(bool mainWindow) override;
	void ImguiEnd() override;
	ImTextureID GenerateTexture(const std::vector<uint8>& data, const Vector2i& size) override;
	void DeleteTexture(ImTextureID id) override;
	void DeleteFontTextures() override;
	bool BeginFrame(bool mainWindow) override;

	// drawcall emulation
	PipelineInfo* draw_createGraphicsPipeline(uint32 indexCount);
	PipelineInfo* draw_getOrCreateGraphicsPipeline(uint32 indexCount);

	void draw_updateVkBlendConstants();
	void draw_updateDepthBias(bool forceUpdate);

	void draw_setRenderPass();
	void draw_endRenderPass();

	void draw_beginSequence() override;
	void draw_execute(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, bool isFirst) override;
	void draw_endSequence() override;

	void draw_updateVertexBuffersDirectAccess();
	void draw_updateUniformBuffersDirectAccess(LatteDecompilerShader* shader, const uint32 uniformBufferRegOffset, LatteConst::ShaderType shaderType);

	void draw_prepareDynamicOffsetsForDescriptorSet(uint32 shaderStageIndex, uint32* dynamicOffsets, sint32& numDynOffsets, const PipelineInfo* pipeline_info);
	VkDescriptorSetInfo* draw_getOrCreateDescriptorSet(PipelineInfo* pipeline_info, LatteDecompilerShader* shader);
	void draw_prepareDescriptorSets(PipelineInfo* pipeline_info, VkDescriptorSetInfo*& vertexDS, VkDescriptorSetInfo*& pixelDS, VkDescriptorSetInfo*& geometryDS);
	void draw_handleSpecialState5();

	// draw synchronization helper
	void sync_inputTexturesChanged();
	void sync_RenderPassLoadTextures(CachedFBOVk* fboVk);
	void sync_RenderPassStoreTextures(CachedFBOVk* fboVk);

	// command buffer
	VkCommandBuffer getCurrentCommandBuffer() const { return m_state.currentCommandBuffer; }

	// uniform
	void uniformData_updateUniformVars(uint32 shaderStageIndex, LatteDecompilerShader* shader);

	// misc
	void CreatePipelineCache();
	VkPipelineShaderStageCreateInfo CreatePipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule& module, const char* entryName) const;
	VkPipeline backbufferBlit_createGraphicsPipeline(VkDescriptorSetLayout descriptorLayout, bool padView, RendererOutputShader* shader);
	bool AcquireNextSwapchainImage(bool mainWindow);
	void RecreateSwapchain(bool mainWindow, bool skipCreate = false);

	// streamout
	void streamout_setupXfbBuffer(uint32 bufferIndex, sint32 ringBufferOffset, uint32 rangeAddr, uint32 rangeSize) override;
	void streamout_begin() override;
	void streamout_applyTransformFeedbackState();
	void bufferCache_copyStreamoutToMainBuffer(uint32 srcOffset, uint32 dstOffset, uint32 size) override;
	void streamout_rendererFinishDrawcall() override;

	// occlusion queries
	LatteQueryObject* occlusionQuery_create() override;
	void occlusionQuery_destroy(LatteQueryObject* queryObj) override;
	void occlusionQuery_flush() override;
	void occlusionQuery_updateState() override;
	void occlusionQuery_notifyEndCommandBuffer();
	void occlusionQuery_notifyBeginCommandBuffer();

private:
	std::vector<const char*> m_layerNames;
	VkInstance m_instance = VK_NULL_HANDLE;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice  m_logicalDevice = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_debugCallback = nullptr;
	volatile bool m_destructionRequested = false;
	
	QueueFamilyIndices m_indices{};

	Semaphore m_pipeline_cache_semaphore;
	std::shared_mutex m_pipeline_cache_save_mutex;
	std::thread m_pipeline_cache_save_thread;
	VkPipelineCache m_pipeline_cache{ nullptr };
	VkPipelineLayout m_pipelineLayout{nullptr};
	VkCommandPool m_commandPool{ nullptr };
	
	// buffer to cache uniform vars
	VkBuffer m_uniformVarBuffer = VK_NULL_HANDLE;
	VkDeviceMemory m_uniformVarBufferMemory = VK_NULL_HANDLE;
	bool m_uniformVarBufferMemoryIsCoherent{false};
	uint8* m_uniformVarBufferPtr = nullptr;
	uint32 m_uniformVarBufferWriteIndex = 0;

	// transform feedback ringbuffer
	VkBuffer m_xfbRingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory m_xfbRingBufferMemory = VK_NULL_HANDLE;

	// buffer cache (attributes, uniforms and streamout)
	VkBuffer m_bufferCache = VK_NULL_HANDLE;
	VkDeviceMemory m_bufferCacheMemory = VK_NULL_HANDLE;

	// texture readback
	VkBuffer m_textureReadbackBuffer = VK_NULL_HANDLE;
	VkDeviceMemory m_textureReadbackBufferMemory = VK_NULL_HANDLE;
	uint8* m_textureReadbackBufferPtr = nullptr;
	uint32 m_textureReadbackBufferWriteIndex = 0;

	// placeholder objects to simulate NULL buffers and textures
	struct NullTexture
	{
		VkImage image;
		VkImageView view;
		VkSampler sampler;
		VkImageMemAllocation* allocation;
	};

	NullTexture nullTexture1D{};
	NullTexture nullTexture2D{};

	void CreateNullTexture(NullTexture& nullTex, VkImageType imageType);
	void CreateNullObjects();
	void DeleteNullTexture(NullTexture& nullTex);
	void DeleteNullObjects();

	// if VK_EXT_external_memory_host is supported we can (optionally) import all of the Wii U memory into a Vulkan memory object
	// this allows us to skip any vertex/uniform caching logic and let the GPU directly read the memory from main RAM
	// Wii U memory imported into a buffer
	bool m_useHostMemoryForCache{ false };
	VkBuffer m_importedMem = VK_NULL_HANDLE;
	VkDeviceMemory m_importedMemMemory = VK_NULL_HANDLE;
	MPTR m_importedMemBaseAddress = 0;

	// command buffer, garbage collection, synchronization
	static constexpr uint32 kCommandBufferPoolSize = 128;

	size_t m_commandBufferIndex = 0; // current buffer being filled
	size_t m_commandBufferSyncIndex = 0; // latest buffer that finished execution (updated on submit)
	std::array<VkFence, kCommandBufferPoolSize> m_cmd_buffer_fences;
	std::array<VkCommandBuffer, kCommandBufferPoolSize> m_commandBuffers;
	std::array<VkSemaphore, kCommandBufferPoolSize> m_commandBufferSemaphores;

	VkSemaphore GetLastSubmittedCmdBufferSemaphore()
	{
		return m_commandBufferSemaphores[(m_commandBufferIndex + m_commandBufferSemaphores.size() - 1) % m_commandBufferSemaphores.size()];
	}

	uint64 m_numSubmittedCmdBuffers{};
	uint64 m_countCommandBufferFinished{};

	uint32 m_recordedDrawcalls{}; // number of drawcalls recorded into current command buffer
	uint32 m_submitThreshold{}; // submit current buffer if recordedDrawcalls exceeds this number
	bool m_submitOnIdle{}; // submit current buffer if Latte command processor goes into idle state (no more commands or waiting for externally signaled condition)

	// tracking for dynamic offsets
	struct  
	{
		uint32 uniformVarBufferOffset[VulkanRendererConst::SHADER_STAGE_INDEX_COUNT];
		struct  
		{
			uint32 unformBufferOffset[LATTE_NUM_MAX_UNIFORM_BUFFERS];
		}shaderUB[VulkanRendererConst::SHADER_STAGE_INDEX_COUNT];
	}dynamicOffsetInfo{};

	// streamout
	struct  
	{
		struct  
		{
			bool enabled;
			uint32 ringBufferOffset;
		}buffer[LATTE_NUM_STREAMOUT_BUFFER];
		sint32 verticesPerInstance;
	}m_streamoutState{};

	struct
	{
		VkQueryPool queryPool{VK_NULL_HANDLE};
		sint32 currentQueryIndex{};
		std::vector<class LatteQueryObjectVk*> list_cachedQueries;
		std::vector<class LatteQueryObjectVk*> list_currentlyActiveQueries;
		uint64 m_lastCommandBuffer{};
		// query result buffer
		VkBuffer bufferQueryResults;
		VkDeviceMemory memoryQueryResults;
		uint64* ptrQueryResults;
		std::vector<uint16> list_availableQueryIndices;
	}m_occlusionQueries;

	// barrier

	enum SYNC_OP : uint32
	{
		/* name */						/* operations */
		HOST_WRITE			= 0x01,
		HOST_READ			= 0x02,

		// BUFFER_INDEX_READ (should be separated?)
		BUFFER_SHADER_READ	= 0x04,		// any form of shader read access
		BUFFER_SHADER_WRITE	= 0x08,		// any form of shader write access
		ANY_TRANSFER		= 0x10,		// previous transfer to/from buffer or image
		TRANSFER_READ		= 0x80,		// transfer from image/buffer
		TRANSFER_WRITE		= 0x100,	// transfer to image/buffer


		IMAGE_READ			= 0x20,
		IMAGE_WRITE			= 0x40,

	};

	template<uint32 TSyncOp>
	void barrier_calcStageAndMask(VkPipelineStageFlags& stages, VkAccessFlags& accessFlags)
	{
		stages = 0;
		accessFlags = 0;
		if constexpr ((TSyncOp & BUFFER_SHADER_READ) != 0)
		{
			// in theory: VK_ACCESS_INDEX_READ_BIT should be set here too but indices are currently separated			
			stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			accessFlags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
		}
		
		if constexpr ((TSyncOp & BUFFER_SHADER_WRITE) != 0)
		{
			stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			accessFlags |= VK_ACCESS_SHADER_WRITE_BIT;
		}

		if constexpr ((TSyncOp & ANY_TRANSFER) != 0)
		{
			//stages |= VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT;
			//accessFlags |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
			stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			accessFlags |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

			//accessFlags |= VK_ACCESS_MEMORY_READ_BIT;
			//accessFlags |= VK_ACCESS_MEMORY_WRITE_BIT;
		}

		if constexpr ((TSyncOp & TRANSFER_READ) != 0)
		{
			stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			accessFlags |= VK_ACCESS_TRANSFER_READ_BIT;

			//accessFlags |= VK_ACCESS_MEMORY_READ_BIT;
		}

		if constexpr ((TSyncOp & TRANSFER_WRITE) != 0)
		{
			stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			accessFlags |= VK_ACCESS_TRANSFER_WRITE_BIT;

			//accessFlags |= VK_ACCESS_MEMORY_WRITE_BIT;
		}

		if constexpr ((TSyncOp & HOST_WRITE) != 0)
		{
			stages |= VK_PIPELINE_STAGE_HOST_BIT;
			accessFlags |= VK_ACCESS_HOST_WRITE_BIT;
		}

		if constexpr ((TSyncOp & HOST_READ) != 0)
		{
			stages |= VK_PIPELINE_STAGE_HOST_BIT;
			accessFlags |= VK_ACCESS_HOST_READ_BIT;
		}

		if constexpr ((TSyncOp & IMAGE_READ) != 0)
		{
			stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			accessFlags |= VK_ACCESS_SHADER_READ_BIT;

			stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			accessFlags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

			stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			accessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}

		if constexpr ((TSyncOp & IMAGE_WRITE) != 0)
		{
			stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			accessFlags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			accessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
	}

	template<uint32 TSrcSyncOp, uint32 TDstSyncOp>
	void barrier_bufferRange(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size)
	{
		VkBufferMemoryBarrier bufMemBarrier{};
		bufMemBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufMemBarrier.pNext = nullptr;
		bufMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		VkPipelineStageFlags srcStages = 0;
		VkPipelineStageFlags dstStages = 0;

		bufMemBarrier.srcAccessMask = 0;
		bufMemBarrier.dstAccessMask = 0;

		barrier_calcStageAndMask<TSrcSyncOp>(srcStages, bufMemBarrier.srcAccessMask);
		barrier_calcStageAndMask<TDstSyncOp>(dstStages, bufMemBarrier.dstAccessMask);

		bufMemBarrier.buffer = buffer;
		bufMemBarrier.offset = offset;
		bufMemBarrier.size = size;
		vkCmdPipelineBarrier(m_state.currentCommandBuffer, srcStages, dstStages, 0, 0, nullptr, 1, &bufMemBarrier, 0, nullptr);
	}

	template<uint32 TSrcSyncOpA, uint32 TDstSyncOpA, uint32 TSrcSyncOpB, uint32 TDstSyncOpB>
	void barrier_bufferRange(VkBuffer bufferA, VkDeviceSize offsetA, VkDeviceSize sizeA,
							 VkBuffer bufferB, VkDeviceSize offsetB, VkDeviceSize sizeB)
	{
		VkPipelineStageFlags srcStagesA = 0;
		VkPipelineStageFlags dstStagesA = 0;
		VkPipelineStageFlags srcStagesB = 0;
		VkPipelineStageFlags dstStagesB = 0;

		VkBufferMemoryBarrier bufMemBarrier[2];

		bufMemBarrier[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufMemBarrier[0].pNext = nullptr;
		bufMemBarrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufMemBarrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufMemBarrier[0].srcAccessMask = 0;
		bufMemBarrier[0].dstAccessMask = 0;
		barrier_calcStageAndMask<TSrcSyncOpA>(srcStagesA, bufMemBarrier[0].srcAccessMask);
		barrier_calcStageAndMask<TDstSyncOpA>(dstStagesA, bufMemBarrier[0].dstAccessMask);
		bufMemBarrier[0].buffer = bufferA;
		bufMemBarrier[0].offset = offsetA;
		bufMemBarrier[0].size = sizeA;

		bufMemBarrier[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufMemBarrier[1].pNext = nullptr;
		bufMemBarrier[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufMemBarrier[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufMemBarrier[1].srcAccessMask = 0;
		bufMemBarrier[1].dstAccessMask = 0;
		barrier_calcStageAndMask<TSrcSyncOpB>(srcStagesB, bufMemBarrier[1].srcAccessMask);
		barrier_calcStageAndMask<TDstSyncOpB>(dstStagesB, bufMemBarrier[1].dstAccessMask);
		bufMemBarrier[1].buffer = bufferB;
		bufMemBarrier[1].offset = offsetB;
		bufMemBarrier[1].size = sizeB;

		vkCmdPipelineBarrier(m_state.currentCommandBuffer, srcStagesA|srcStagesB, dstStagesA|dstStagesB, 0, 0, nullptr, 2, bufMemBarrier, 0, nullptr);
	}

	void barrier_sequentializeTransfer()
	{
		VkMemoryBarrier memBarrier{};
		memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memBarrier.pNext = nullptr;

		VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		memBarrier.dstAccessMask = 0;

		memBarrier.srcAccessMask |= (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
		memBarrier.dstAccessMask |= (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);

		vkCmdPipelineBarrier(m_state.currentCommandBuffer, srcStages, dstStages, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);
	}

	void barrier_sequentializeCommand()
	{
		VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		vkCmdPipelineBarrier(m_state.currentCommandBuffer, srcStages, dstStages, 0, 0, nullptr, 0, nullptr, 0, nullptr);
	}

	template<uint32 TSrcSyncOp, uint32 TDstSyncOp>
	void barrier_image(VkImage imageVk, VkImageSubresourceRange& subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkPipelineStageFlags srcStages = 0;
		VkPipelineStageFlags dstStages = 0;

		VkImageMemoryBarrier imageMemBarrier{};
		imageMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemBarrier.srcAccessMask = 0;
		imageMemBarrier.dstAccessMask = 0;
		barrier_calcStageAndMask<TSrcSyncOp>(srcStages, imageMemBarrier.srcAccessMask);
		barrier_calcStageAndMask<TDstSyncOp>(dstStages, imageMemBarrier.dstAccessMask);
		imageMemBarrier.image = imageVk;
		imageMemBarrier.subresourceRange = subresourceRange;
		imageMemBarrier.oldLayout = oldLayout;
		imageMemBarrier.newLayout = newLayout;

		vkCmdPipelineBarrier(m_state.currentCommandBuffer,
							 srcStages, dstStages,
							 0,
							 0, NULL,
							 0, NULL,
							 1, &imageMemBarrier);
	}

	template<uint32 TSrcSyncOp, uint32 TDstSyncOp>
	void barrier_image(LatteTextureVk* vkTexture, VkImageSubresourceLayers& subresourceLayers, VkImageLayout newLayout)
	{
		VkImage imageVk = vkTexture->GetImageObj()->m_image;

		VkImageSubresourceRange subresourceRange;
		subresourceRange.aspectMask = subresourceLayers.aspectMask;
		subresourceRange.baseArrayLayer = subresourceLayers.baseArrayLayer;
		subresourceRange.layerCount = subresourceLayers.layerCount;
		subresourceRange.baseMipLevel = subresourceLayers.mipLevel;
		subresourceRange.levelCount = 1;

		barrier_image<TSrcSyncOp, TDstSyncOp>(imageVk, subresourceRange, vkTexture->GetImageLayout(subresourceRange), newLayout);

		vkTexture->SetImageLayout(subresourceRange, newLayout);
	}


public:
	bool GetDisableMultithreadedCompilation() const { return m_featureControl.disableMultithreadedCompilation; }
	bool UseTFViaSSBO() const { return m_featureControl.mode.useTFEmulationViaSSBO; }
	bool HasSPRIVRoundingModeRTE32() const { return m_featureControl.shaderFloatControls.shaderRoundingModeRTEFloat32; }
	bool IsDebugUtilsEnabled() const { return m_featureControl.debugMarkersSupported && m_featureControl.instanceExtensions.debug_utils; }

private:

	// debug
	void debug_genericBarrier();

	// shaders
	struct  
	{
		RendererShaderVk* copySurface_vs{};
		RendererShaderVk* copySurface_psDepth2Color{};
		RendererShaderVk* copySurface_psColor2Depth{};
	}defaultShaders;


};
