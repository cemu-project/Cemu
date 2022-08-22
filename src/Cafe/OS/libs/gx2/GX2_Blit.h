#pragma once

namespace GX2
{
	enum class GX2ClearFlags : uint32
	{
		CLEAR_DEPTH = 0x01, // clear depth to given clear value
		CLEAR_STENCIL = 0x02, // clear stencil to given stencil clear value
		SET_DEPTH_REG = 0x04, // 
		SET_STENCIL_REG = 0x08,
	};

	void GX2BlitInit();
}
ENABLE_BITMASK_OPERATORS(GX2::GX2ClearFlags);