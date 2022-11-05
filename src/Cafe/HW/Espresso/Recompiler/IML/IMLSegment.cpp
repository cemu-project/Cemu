#include "IMLInstruction.h"
#include "IMLSegment.h"

bool IMLSegment::HasSuffixInstruction() const
{
	if (imlList.empty())
		return false;
	const IMLInstruction& imlInstruction = imlList.back();
	return imlInstruction.IsSuffixInstruction();
}
