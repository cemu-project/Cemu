#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <memory>

namespace PerformanceOptimizer {
    
    // CPU optimization
    class CPUOptimizer {
    public:
        static void Initialize();
        static void SetCPUPriority();
        static void OptimizeCPUCache();
        static void SetOptimalThreadCount();
    };
    
    // Memory optimization
    class MemoryManager {
    public:
        static void Initialize();
        static void PreallocateMemory(size_t size);
        static void OptimizeMemoryLayout();
        static void EnableHugePages();
    };
    
    // I/O optimization
    class IOManager {
    public:
        static void Initialize();
        static void EnableDirectIO();
        static void OptimizeFileAccess();
        static void PreloadCriticalFiles();
    };
    
    // Cache optimization
    class CacheManager {
    public:
        static void Initialize();
        static void WarmupCache();
        static void OptimizeCacheLayout();
        static void PrefetchData();
    };
    
    // System optimization
    class SystemOptimizer {
    public:
        static void Initialize();
        static void SetSystemPriority();
        static void OptimizeSystemSettings();
        static void DisableUnnecessaryServices();
    };
    
    // Performance monitoring
    class PerformanceMonitor {
    public:
        static void Initialize();
        static void StartMonitoring();
        static void LogPerformanceMetrics();
        static void AnalyzeBottlenecks();
    };
    
    // Main optimization interface
    class Optimizer {
    public:
        static void InitializeAll();
        static void OptimizeForGame(const std::string& gameId);
        static void SetPerformanceProfile(const std::string& profile);
        static void ApplyDynamicOptimizations();
    };
} 