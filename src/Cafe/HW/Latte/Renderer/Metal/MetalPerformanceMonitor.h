#pragma once

class MetalPerformanceMonitor
{
public:
    size_t m_bufferAllocatorMemory = 0;

    // Per frame data
    uint32 m_renderPasses = 0;

    MetalPerformanceMonitor() = default;
    ~MetalPerformanceMonitor() = default;

    void ResetPerFrameData()
    {
        m_renderPasses = 0;
    }
};
