#include "Cafe/HW/Latte/Renderer/Metal/MetalQuery.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"

bool LatteQueryObjectMtl::getResult(uint64& numSamplesPassed)
{
    if (m_commandBuffer && !CommandBufferCompleted(m_commandBuffer))
        return false;

    uint64* resultPtr = m_mtlr->GetOcclusionQueryResultsPtr();

    numSamplesPassed = 0;
    for (uint32 i = m_range.begin; i != m_range.end; i = (i + 1) % MetalRenderer::OCCLUSION_QUERY_POOL_SIZE)
        numSamplesPassed += resultPtr[i];

    return true;
}

LatteQueryObjectMtl::~LatteQueryObjectMtl()
{
    if (m_commandBuffer)
        m_commandBuffer->release();
}

void LatteQueryObjectMtl::begin()
{
    m_range.begin = m_mtlr->GetOcclusionQueryIndex();
    m_mtlr->BeginOcclusionQuery();
}

void LatteQueryObjectMtl::end()
{
    m_range.end = m_mtlr->GetOcclusionQueryIndex();
    m_mtlr->EndOcclusionQuery();

    m_commandBuffer = m_mtlr->GetAndRetainCurrentCommandBufferIfNotCompleted();
    if (m_commandBuffer)
        m_mtlr->RequestSoonCommit();
}
