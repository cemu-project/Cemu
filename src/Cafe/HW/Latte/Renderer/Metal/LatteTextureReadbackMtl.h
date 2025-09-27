#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cafe/HW/Latte/Core/LatteTextureReadbackInfo.h"

class LatteTextureReadbackInfoMtl : public LatteTextureReadbackInfo
{
public:
	LatteTextureReadbackInfoMtl(class MetalRenderer* mtlRenderer, LatteTextureView* textureView, uint32 bufferOffset) : LatteTextureReadbackInfo(textureView), m_mtlr{mtlRenderer}, m_bufferOffset{bufferOffset} {}
	~LatteTextureReadbackInfoMtl();

	void StartTransfer() override;

	bool IsFinished() override;
	void ForceFinish() override;

	uint8* GetData() override;

private:
	class MetalRenderer* m_mtlr;

	MTL::CommandBuffer* m_commandBuffer = nullptr;

	uint32 m_bufferOffset = 0;
};
