#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"

class LatteQueryObjectVk : public LatteQueryObject
{
	friend class VulkanRenderer;

	LatteQueryObjectVk(VulkanRenderer* rendererVk) : m_rendererVk(rendererVk) 
	{

	};

	bool getResult(uint64& numSamplesPassed) override;
	void begin() override;
	void end() override;
	void beginFragment();
	void endFragment();
	void handleFinishedFragments();

	uint32 acquireQueryIndex();
	void releaseQueryIndex(uint32 queryIndex);

private:
	struct queryFragment 
	{
		uint32 queryIndex;
		uint64 m_finishCommandBuffer;
		bool isFinished;
	};

	VulkanRenderer* m_rendererVk;
	//sint32 m_queryIndex;
	std::vector<queryFragment> list_queryFragments;
	bool m_vkQueryEnded{};
	bool m_hasActiveQuery{};
	bool m_hasActiveFragment{};
	uint64 m_finishCommandBuffer;
	uint64 m_acccumulatedSum;
};

bool LatteQueryObjectVk::getResult(uint64& numSamplesPassed)
{
	if (!m_vkQueryEnded)
		return false;
	if (!m_rendererVk->HasCommandBufferFinished(m_finishCommandBuffer))
		return false;
	handleFinishedFragments();
	cemu_assert_debug(list_queryFragments.empty());
	numSamplesPassed = m_acccumulatedSum;
	//numSamplesPassed = m_rendererVk->m_occlusionQueries.ptrQueryResults[m_queryIndex];
	return true;
}

void LatteQueryObjectVk::beginFragment()
{
	m_rendererVk->draw_endRenderPass();

	handleFinishedFragments();
	uint32 newQueryIndex = acquireQueryIndex();

	queryFragment qf{};
	qf.queryIndex = newQueryIndex;
	qf.isFinished = false;
	qf.m_finishCommandBuffer = 0;
	list_queryFragments.emplace_back(qf);


	vkCmdResetQueryPool(m_rendererVk->m_state.currentCommandBuffer, m_rendererVk->m_occlusionQueries.queryPool, newQueryIndex, 1);
	vkCmdBeginQuery(m_rendererVk->m_state.currentCommandBuffer, m_rendererVk->m_occlusionQueries.queryPool, newQueryIndex, VK_QUERY_CONTROL_PRECISE_BIT);
	// todo - we already synchronize with command buffers, should we also set wait bits?

	m_hasActiveFragment = true;
}

void LatteQueryObjectVk::begin()
{
	m_vkQueryEnded = false;
	m_hasActiveQuery = true;
	beginFragment();
}

void LatteQueryObjectVk::endFragment()
{
	m_rendererVk->draw_endRenderPass();

	cemu_assert_debug(m_hasActiveFragment);
	uint32 queryIndex = list_queryFragments.back().queryIndex;
	vkCmdEndQuery(m_rendererVk->m_state.currentCommandBuffer, m_rendererVk->m_occlusionQueries.queryPool, queryIndex);

	vkCmdCopyQueryPoolResults(m_rendererVk->m_state.currentCommandBuffer, m_rendererVk->m_occlusionQueries.queryPool, queryIndex, 1, m_rendererVk->m_occlusionQueries.bufferQueryResults, queryIndex * sizeof(uint64), 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	list_queryFragments.back().m_finishCommandBuffer = m_rendererVk->GetCurrentCommandBufferId();
	list_queryFragments.back().isFinished = true;
	m_hasActiveFragment = false;
}

void LatteQueryObjectVk::handleFinishedFragments()
{
	// remove finished fragments and add to m_acccumulatedSum
	while (!list_queryFragments.empty())
	{
		auto& it = list_queryFragments.front();
		if (!it.isFinished)
			break;
		if (!m_rendererVk->HasCommandBufferFinished(it.m_finishCommandBuffer))
			break;
		m_acccumulatedSum += m_rendererVk->m_occlusionQueries.ptrQueryResults[it.queryIndex];
		releaseQueryIndex(it.queryIndex);
		list_queryFragments.erase(list_queryFragments.begin());
	}
}

uint32 LatteQueryObjectVk::acquireQueryIndex()
{
	if (m_rendererVk->m_occlusionQueries.list_availableQueryIndices.empty())
	{
		cemuLog_log(LogType::Force, "Vulkan-Error: Exhausted query pool");
		assert_dbg();
	}
	uint32 queryIndex = m_rendererVk->m_occlusionQueries.list_availableQueryIndices.back();
	m_rendererVk->m_occlusionQueries.list_availableQueryIndices.pop_back();
	return queryIndex;
}

void LatteQueryObjectVk::releaseQueryIndex(uint32 queryIndex)
{
	m_rendererVk->m_occlusionQueries.list_availableQueryIndices.emplace_back(queryIndex);
}

void LatteQueryObjectVk::end()
{
	cemu_assert_debug(!list_queryFragments.empty());
	if(m_hasActiveFragment)
		endFragment();
	m_vkQueryEnded = true;
	m_hasActiveQuery = false;
	m_finishCommandBuffer = m_rendererVk->GetCurrentCommandBufferId();
	m_rendererVk->m_occlusionQueries.m_lastCommandBuffer = m_finishCommandBuffer;
	m_rendererVk->RequestSubmitSoon(); // make sure the current command buffer gets submitted soon
	m_rendererVk->RequestSubmitOnIdle();
}

LatteQueryObject* VulkanRenderer::occlusionQuery_create()
{
	// create query pool if it doesn't already exist
	if(m_occlusionQueries.queryPool == VK_NULL_HANDLE)
	{ 
		VkQueryPoolCreateInfo queryPoolCreateInfo{};
		queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolCreateInfo.flags = 0;
		queryPoolCreateInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
		queryPoolCreateInfo.queryCount = OCCLUSION_QUERY_POOL_SIZE;
		queryPoolCreateInfo.pipelineStatistics = 0;
		auto r = vkCreateQueryPool(m_logicalDevice, &queryPoolCreateInfo, nullptr, &m_occlusionQueries.queryPool);
		if (r != VK_SUCCESS)
		{
			cemuLog_log(LogType::Force, "Vulkan-Error: Failed to create query pool with error {}", (sint32)r);
			return nullptr;
		}
	}
	LatteQueryObjectVk* queryObjVk = nullptr;
	if (m_occlusionQueries.list_cachedQueries.empty())
	{
		queryObjVk = new LatteQueryObjectVk(this);
	}
	else
	{
		queryObjVk = m_occlusionQueries.list_cachedQueries.front();
		m_occlusionQueries.list_cachedQueries.erase(m_occlusionQueries.list_cachedQueries.begin()+0);
	}
	queryObjVk->queryEnded = false;
	queryObjVk->queryEventStart = 0;
	queryObjVk->queryEventEnd = 0;
	queryObjVk->m_vkQueryEnded = false;
	queryObjVk->m_acccumulatedSum = 0;
	cemu_assert_debug(queryObjVk->list_queryFragments.empty()); // query fragment list should always be cleared in _destroy()
	m_occlusionQueries.list_currentlyActiveQueries.emplace_back(queryObjVk);
	return queryObjVk;
}

void VulkanRenderer::occlusionQuery_destroy(LatteQueryObject* queryObj)
{
	LatteQueryObjectVk* queryObjVk = static_cast<LatteQueryObjectVk*>(queryObj);
	m_occlusionQueries.list_currentlyActiveQueries.erase(std::remove(m_occlusionQueries.list_currentlyActiveQueries.begin(), m_occlusionQueries.list_currentlyActiveQueries.end(), queryObj), m_occlusionQueries.list_currentlyActiveQueries.end());
	m_occlusionQueries.list_cachedQueries.emplace_back(queryObjVk);
	for (auto& it : queryObjVk->list_queryFragments)
		queryObjVk->releaseQueryIndex(it.queryIndex);
	queryObjVk->list_queryFragments.clear();
}

void VulkanRenderer::occlusionQuery_flush()
{
	WaitCommandBufferFinished(m_occlusionQueries.m_lastCommandBuffer);
}

void VulkanRenderer::occlusionQuery_updateState()
{
	// check for finished command buffers here since query states are tied to buffers
	ProcessFinishedCommandBuffers();
}

void VulkanRenderer::occlusionQuery_notifyEndCommandBuffer()
{
	for (auto& it : m_occlusionQueries.list_currentlyActiveQueries)
		if(it->m_hasActiveQuery)
			it->endFragment();
}

void VulkanRenderer::occlusionQuery_notifyBeginCommandBuffer()
{
	for (auto& it : m_occlusionQueries.list_currentlyActiveQueries)
		if (it->m_hasActiveQuery)
			it->beginFragment();
}
