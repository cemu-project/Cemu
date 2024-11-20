#pragma once

class MetalPerformanceMonitor
{
public:
    size_t m_bufferAllocatorMemory = 0;

    // Per frame data
    uint32 m_commandBuffers = 0;
    uint32 m_renderPasses = 0;
    uint32 m_clears = 0;
    uint32 m_manualVertexFetchDraws = 0;
    uint32 m_meshDraws = 0;
    uint32 m_triangleFans = 0;

    MetalPerformanceMonitor() = default;
    ~MetalPerformanceMonitor() = default;

    void ResetPerFrameData()
    {
        m_commandBuffers = 0;
        m_renderPasses = 0;
        m_clears = 0;
        m_manualVertexFetchDraws = 0;
        m_meshDraws = 0;
        m_triangleFans = 0;
    }
};
