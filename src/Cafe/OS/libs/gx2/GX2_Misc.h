#pragma once

namespace GX2
{
	extern uint32 sGX2MainCoreIndex;

	enum class E_TVRES
	{
		TODO,
	};

	enum class E_TVBUFFERMODE
	{
		DOUBLE_BUFFER = 2,
	};

	void _GX2DriverReset();

	void GX2SetTVBuffer(void* imageBuffePtr, uint32 imageBufferSize, E_TVRES tvResolutionMode, uint32 surfaceFormat, E_TVBUFFERMODE bufferMode);
	void GX2SetTVGamma(float gamma);

	void GX2Invalidate(uint32 invalidationFlags, MPTR invalidationAddr, uint32 invalidationSize);

	void GX2MiscInit();
};