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

	enum class GX2InvalidationFlag : uint32
	{
		GPU_ATTRIB        = (1<<0),
		GPU_TEXTURE       = (1<<1),
		GPU_UNIFORM_BLOCK = (1<<2),
		GPU_SHADER        = (1<<3),
		GPU_COLOR_BUFFER  = (1<<4),
		GPU_DEPTH_BUFFER  = (1<<5),
		CPU               = (1<<6),
		GPU_STREAM_OUT    = (1<<7),
		GPU_EXPORT_BUFFER = (1<<8)
	};

	void _GX2DriverReset();

	void GX2SetTVBuffer(void* imageBuffePtr, uint32 imageBufferSize, E_TVRES tvResolutionMode, uint32 surfaceFormat, E_TVBUFFERMODE bufferMode);
	void GX2SetTVGamma(float gamma);

	void GX2Invalidate(GX2InvalidationFlag invalidationFlags, MPTR invalidationAddr, uint32 invalidationSize);

	void GX2MiscInit();

};

ENABLE_BITMASK_OPERATORS(GX2::GX2InvalidationFlag);
