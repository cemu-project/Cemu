#pragma once

#define PERFORMANCE_MONITOR_TRACK_CYCLES (5) // one cycle lasts one second

// todo - replace PPCTimer with HighResolutionTimer.h
uint64 PPCTimer_getRawTsc();
uint64 PPCTimer_tscToMicroseconds(uint64 us);

class LattePerfStatTimer
{
  public:
	void beginMeasuring()
	{
		timerStart = PPCTimer_getRawTsc();
	}

	void endMeasuring()
	{
		uint64 dif = PPCTimer_getRawTsc() - timerStart;
		currentSum += dif;
	}

	void frameFinished()
	{
		previousFrame = currentSum;
		currentSum = 0;
	}

	uint64 getPreviousFrameValue()
	{
		return previousFrame;
	}

  private:
	uint64 currentSum{};
	uint64 previousFrame{};
	uint64 timerStart{};
};

class LattePerfStatCounter
{
  public:
	void increment()
	{
		m_value++;
	}

	void decrement()
	{
		cemu_assert_debug(m_value > 0);
		m_value--;
	}

	void decrement(uint32 count)
	{
		cemu_assert_debug(count <= m_value);
		m_value -= count;
	}

	uint32 get()
	{
		return m_value;
	}

	void reset()
	{
		m_value = 0;
	}

  private:
	std::atomic_uint32_t m_value{};
};

typedef struct
{
	struct
	{
		// CPU
		volatile uint64 lastCycleCount;
		volatile uint64 skippedCycles;
		volatile uint32
			recompilerLeaveCount; // increased everytime the recompiler switches back to interpreter
		volatile uint32 threadLeaveCount; // increased everytime a thread gives up it's timeslice
		// GPU
		volatile uint32 lastUpdate;
		volatile uint32 frameCounter;
		volatile uint32 drawCallCounter;
		volatile uint32 shaderBindCount;
		volatile uint64 vertexDataUploaded; // amount of vertex data uploaded to GPU (bytes)
		volatile uint64 vertexDataCached;	// amount of vertex data reused from GPU cache (bytes)
		volatile uint64 uniformBankUploadedData; // amount of uniform buffer data (excluding
												 // remapped uniforms) uploaded to GPU
		volatile uint64
			uniformBankUploadedCount; // number of separate uploads for uniformBankDataUploaded
		volatile uint64 indexDataUploaded;
		volatile uint64 indexDataCached;
	} cycle[PERFORMANCE_MONITOR_TRACK_CYCLES];
	volatile sint32 cycleIndex;
	// new stats
	LattePerfStatTimer gpuTime_frameTime;
	LattePerfStatTimer gpuTime_shaderCreate;
	LattePerfStatTimer gpuTime_idleTime;  // time spent waiting for new commands from CPU
	LattePerfStatTimer gpuTime_fenceTime; // time spent waiting for fence condition

	LattePerfStatTimer gpuTime_dcStageTextures;	 // drawcall texture/mrt setup
	LattePerfStatTimer gpuTime_dcStageVertexMgr; // drawcall vertex setup and upload
	LattePerfStatTimer
		gpuTime_dcStageShaderAndUniformMgr; // drawcall shader setup and uniform management/upload
	LattePerfStatTimer gpuTime_dcStageIndexMgr; // drawcall index data setup and upload
	LattePerfStatTimer gpuTime_dcStageMRT;		// drawcall render target API

	LattePerfStatTimer gpuTime_dcStageDrawcallAPI; // drawcall api call
	LattePerfStatTimer gpuTime_waitForAsync; // waiting for operations to complete (e.g. GX2DrawDone
											 // or force texture readback) Also includes texture
											 // readback and occlusion query polling logic

	// generic
	uint32 numCompiledVS; // number of compiled vertex shader programs
	uint32 numCompiledGS; // number of compiled geometry shader programs
	uint32 numCompiledPS; // number of compiled pixel shader programs

	// Vulkan
	struct
	{
		LattePerfStatCounter numDescriptorSets;
		LattePerfStatCounter numDescriptorDynUniformBuffers;
		LattePerfStatCounter numDescriptorStorageBuffers;
		LattePerfStatCounter numDescriptorSamplerTextures;
		LattePerfStatCounter numGraphicPipelines;
		LattePerfStatCounter numImages;
		LattePerfStatCounter numImageViews;
		LattePerfStatCounter numRenderPass;
		LattePerfStatCounter numFramebuffer;

		// per frame
		LattePerfStatCounter numDrawBarriersPerFrame;
		LattePerfStatCounter numBeginRenderpassPerFrame;
	} vk;
} performanceMonitor_t;

extern performanceMonitor_t performanceMonitor;

void LattePerformanceMonitor_frameEnd();
void LattePerformanceMonitor_frameBegin();

#define beginPerfMonProfiling(__obj)                                                               \
	if (THasProfiling)                                                                             \
	__obj.beginMeasuring()
#define endPerfMonProfiling(__obj)                                                                 \
	if (THasProfiling)                                                                             \
	__obj.endMeasuring()