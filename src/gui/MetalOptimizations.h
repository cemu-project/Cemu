#pragma once

#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

namespace MetalOptimizations {
    
    // Metal Performance Shaders integration
    class MetalPerformanceShaders {
    public:
        static void Initialize(id<MTLDevice> device);
        static void OptimizeTexture(id<MTLTexture> texture);
        static void OptimizeComputePipeline(id<MTLComputePipelineState> pipeline);
    };
    
    // Grand Central Dispatch integration
    class ThreadManager {
    public:
        static void Initialize();
        static void SetThreadAffinity();
        static void OptimizeThreadPool();
    };
    
    // Memory management
    class MemoryOptimizer {
    public:
        static void InitializeMemoryPools();
        static void* AllocateAligned(size_t size, size_t alignment);
        static void FreeAligned(void* ptr);
    };
    
    // Power management
    class PowerManager {
    public:
        static void InitializePowerManagement();
        static void SetPerformanceMode(bool highPerformance);
        static void OptimizeForBatteryLife();
    };
    
    // Display management
    class DisplayManager {
    public:
        static void InitializeDisplayModes();
        static void SetOptimalDisplayMode();
        static void HandleDisplayChanges();
    };
    
    // Metal presentation optimization
    class PresentationOptimizer {
    public:
        static void InitializeTripleBuffering();
        static void OptimizeFramePacing();
        static void SetOptimalPresentationMode();
    };
    
    // Compute shader optimization
    class ComputeOptimizer {
    public:
        static void InitializeComputePipelines();
        static void OptimizeComputeWorkload();
        static void SetOptimalThreadGroupSize();
    };
} 