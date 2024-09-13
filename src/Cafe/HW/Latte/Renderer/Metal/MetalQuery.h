#pragma once

#include "Cafe/HW/Latte/Core/LatteQueryObject.h"

#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"

class LatteQueryObjectMtl : public LatteQueryObject
{
public:
	LatteQueryObjectMtl(class MetalRenderer* mtlRenderer) : m_mtlr{mtlRenderer} {}

	bool getResult(uint64& numSamplesPassed) override;
	void begin() override;
	void end() override;

private:
	class MetalRenderer* m_mtlr;

	uint32 m_queryIndex;
	MTL::CommandBuffer* m_commandBuffer;
	uint64 m_acccumulatedSum;
};
