#pragma once

namespace coreinit
{
	void InitializeMemory();

	void OSGetMemBound(sint32 memType, MPTR* offsetOutput, uint32* sizeOutput);
}