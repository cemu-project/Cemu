#pragma once

#include "Cafe/HW/Latte/Core/LatteQueryObject.h"

#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"

class LatteQueryObjectMtl : public LatteQueryObject
{
public:
	LatteQueryObjectMtl(class MetalRenderer* mtlRenderer) : m_mtlr{mtlRenderer} {}
	~LatteQueryObjectMtl();

	bool getResult(uint64& numSamplesPassed) override;
	void begin() override;
	void end() override;

	void GrowRange()
	{
	    m_range.end++;
	}

private:
	class MetalRenderer* m_mtlr;

	MetalQueryRange m_range = {INVALID_UINT32, INVALID_UINT32};
	// TODO: make this a list of command buffers?
	MTL::CommandBuffer* m_commandBuffer = nullptr;
};
