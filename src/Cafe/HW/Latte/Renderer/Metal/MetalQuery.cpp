#include "Cafe/HW/Latte/Renderer/Metal/MetalQuery.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "HW/Latte/Renderer/Metal/MetalCommon.h"

bool LatteQueryObjectMtl::getResult(uint64& numSamplesPassed)
{
    if (!m_commandBuffer)
    {
        numSamplesPassed = 0;
        return true;
    }

    if (!CommandBufferCompleted(m_commandBuffer))
        return false;

    numSamplesPassed = m_mtlr->GetOcclusionQueryResultsPtr()[m_queryIndex];

    return true;
}

LatteQueryObjectMtl::~LatteQueryObjectMtl()
{
    if (m_queryIndex != INVALID_UINT32)
        m_mtlr->ReleaseOcclusionQueryIndex(m_queryIndex);

    if (m_commandBuffer)
        m_commandBuffer->release();
}

void LatteQueryObjectMtl::begin()
{
    m_queryIndex = m_mtlr->GetAvailableOcclusionQueryIndex();
    m_mtlr->SetActiveOcclusionQueryIndex(m_queryIndex);
}

void LatteQueryObjectMtl::end()
{
    m_mtlr->SetActiveOcclusionQueryIndex(INVALID_UINT32);
    if (m_mtlr->IsCommandBufferActive())
    {
        m_commandBuffer = m_mtlr->GetCurrentCommandBuffer()->retain();
        m_mtlr->RequestSoonCommit();
    }
}
