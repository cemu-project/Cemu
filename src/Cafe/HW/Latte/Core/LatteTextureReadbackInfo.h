#pragma once

#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "util/highresolutiontimer/HighResolutionTimer.h"

class LatteTextureReadbackInfo
{
public:
	LatteTextureReadbackInfo(LatteTextureView* textureView)
		: hostTextureCopy(textureView->baseTexture), m_textureView(textureView)
	{}

	virtual ~LatteTextureReadbackInfo() = default;

	virtual void StartTransfer() = 0;
	virtual bool IsFinished() = 0;
	virtual void ForceFinish() {};

	virtual uint8* GetData() = 0;
	virtual void ReleaseData() {};

	HRTick transferStartTime;
	HRTick waitStartTime;
	bool forceFinish{ false }; // set to true if not finished in time for dependent operation
	// texture info
	LatteTextureDefinition hostTextureCopy{};

protected:
	LatteTextureView* m_textureView;
	uint32 m_image_size = 0;
};