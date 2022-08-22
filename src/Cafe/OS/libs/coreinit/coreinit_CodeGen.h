#pragma once

namespace coreinit
{
	void OSGetCodegenVirtAddrRangeInternal(uint32& rangeStart, uint32& rangeSize);
	void codeGenHandleICBI(uint32 ea);
	bool codeGenShouldAvoid();

	void InitializeCodeGen();
}