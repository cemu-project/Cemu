#include "IMLInstruction.h"
#include "IMLSegment.h"

void IMLSegment::SetEnterable(uint32 enterAddress)
{
	cemu_assert_debug(!isEnterable || enterPPCAddress == enterAddress);
	isEnterable = true;
	enterPPCAddress = enterAddress;
}

bool IMLSegment::HasSuffixInstruction() const
{
	if (imlList.empty())
		return false;
	const IMLInstruction& imlInstruction = imlList.back();
	return imlInstruction.IsSuffixInstruction();
}

sint32 IMLSegment::GetSuffixInstructionIndex() const
{
	cemu_assert_debug(HasSuffixInstruction());
	return (sint32)(imlList.size() - 1);
}

IMLInstruction* IMLSegment::GetLastInstruction()
{
	if (imlList.empty())
		return nullptr;
	return &imlList.back();
}

void IMLSegment::SetLinkBranchNotTaken(IMLSegment* imlSegmentDst)
{
	if (nextSegmentBranchNotTaken)
		nextSegmentBranchNotTaken->list_prevSegments.erase(std::find(nextSegmentBranchNotTaken->list_prevSegments.begin(), nextSegmentBranchNotTaken->list_prevSegments.end(), this));
	nextSegmentBranchNotTaken = imlSegmentDst;
	if(imlSegmentDst)
		imlSegmentDst->list_prevSegments.push_back(this);
}

void IMLSegment::SetLinkBranchTaken(IMLSegment* imlSegmentDst)
{
	if (nextSegmentBranchTaken)
		nextSegmentBranchTaken->list_prevSegments.erase(std::find(nextSegmentBranchTaken->list_prevSegments.begin(), nextSegmentBranchTaken->list_prevSegments.end(), this));
	nextSegmentBranchTaken = imlSegmentDst;
	if (imlSegmentDst)
		imlSegmentDst->list_prevSegments.push_back(this);
}

IMLInstruction* IMLSegment::AppendInstruction()
{
	IMLInstruction& inst = imlList.emplace_back();
	memset(&inst, 0, sizeof(IMLInstruction));
	return &inst;
}

void IMLSegment_SetLinkBranchNotTaken(IMLSegment* imlSegmentSrc, IMLSegment* imlSegmentDst)
{
	// make sure segments aren't already linked
	if (imlSegmentSrc->nextSegmentBranchNotTaken == imlSegmentDst)
		return;
	// add as next segment for source
	if (imlSegmentSrc->nextSegmentBranchNotTaken != nullptr)
		assert_dbg();
	imlSegmentSrc->nextSegmentBranchNotTaken = imlSegmentDst;
	// add as previous segment for destination
	imlSegmentDst->list_prevSegments.push_back(imlSegmentSrc);
}

void IMLSegment_SetLinkBranchTaken(IMLSegment* imlSegmentSrc, IMLSegment* imlSegmentDst)
{
	// make sure segments aren't already linked
	if (imlSegmentSrc->nextSegmentBranchTaken == imlSegmentDst)
		return;
	// add as next segment for source
	if (imlSegmentSrc->nextSegmentBranchTaken != nullptr)
		assert_dbg();
	imlSegmentSrc->nextSegmentBranchTaken = imlSegmentDst;
	// add as previous segment for destination
	imlSegmentDst->list_prevSegments.push_back(imlSegmentSrc);
}

void IMLSegment_RemoveLink(IMLSegment* imlSegmentSrc, IMLSegment* imlSegmentDst)
{
	if (imlSegmentSrc->nextSegmentBranchNotTaken == imlSegmentDst)
	{
		imlSegmentSrc->nextSegmentBranchNotTaken = nullptr;
	}
	else if (imlSegmentSrc->nextSegmentBranchTaken == imlSegmentDst)
	{
		imlSegmentSrc->nextSegmentBranchTaken = nullptr;
	}
	else
		assert_dbg();

	bool matchFound = false;
	for (sint32 i = 0; i < imlSegmentDst->list_prevSegments.size(); i++)
	{
		if (imlSegmentDst->list_prevSegments[i] == imlSegmentSrc)
		{
			imlSegmentDst->list_prevSegments.erase(imlSegmentDst->list_prevSegments.begin() + i);
			matchFound = true;
			break;
		}
	}
	if (matchFound == false)
		assert_dbg();
}

/*
 * Replaces all links to segment orig with linkts to segment new
 */
void IMLSegment_RelinkInputSegment(IMLSegment* imlSegmentOrig, IMLSegment* imlSegmentNew)
{
	while (imlSegmentOrig->list_prevSegments.size() != 0)
	{
		IMLSegment* prevSegment = imlSegmentOrig->list_prevSegments[0];
		if (prevSegment->nextSegmentBranchNotTaken == imlSegmentOrig)
		{
			IMLSegment_RemoveLink(prevSegment, imlSegmentOrig);
			IMLSegment_SetLinkBranchNotTaken(prevSegment, imlSegmentNew);
		}
		else if (prevSegment->nextSegmentBranchTaken == imlSegmentOrig)
		{
			IMLSegment_RemoveLink(prevSegment, imlSegmentOrig);
			IMLSegment_SetLinkBranchTaken(prevSegment, imlSegmentNew);
		}
		else
		{
			assert_dbg();
		}
	}
}
