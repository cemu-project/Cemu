#include "Cafe/HW/Latte/Renderer/Metal/MetalQuery.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "HW/Latte/Renderer/Metal/MetalCommon.h"

bool LatteQueryObjectMtl::getResult(uint64& numSamplesPassed)
{
    if (!m_mtlr->CommandBufferCompleted(m_commandBuffer))
        return false;

    numSamplesPassed = m_mtlr->GetOcclusionQueryResultsPtr()[m_queryIndex];

    return true;
}

void LatteQueryObjectMtl::begin()
{
    m_queryIndex = m_mtlr->GetAvailableOcclusionQueryIndex();
    m_mtlr->SetActiveOcclusionQueryIndex(m_queryIndex);
}

void LatteQueryObjectMtl::end()
{
    m_mtlr->SetActiveOcclusionQueryIndex(INVALID_UINT32);
    // TODO: request soon submit of the command buffer
}
