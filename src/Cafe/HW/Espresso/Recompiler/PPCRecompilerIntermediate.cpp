#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"

void PPCRecompilerIML_isolateEnterableSegments(ppcImlGenContext_t* ppcImlGenContext)
{
	size_t initialSegmentCount = ppcImlGenContext->segmentList2.size();
	for (size_t i = 0; i < initialSegmentCount; i++)
	{
		IMLSegment* imlSegment = ppcImlGenContext->segmentList2[i];
		if (imlSegment->list_prevSegments.empty() == false && imlSegment->isEnterable)
		{
			// spawn new segment at end
			PPCRecompilerIml_insertSegments(ppcImlGenContext, ppcImlGenContext->segmentList2.size(), 1);
			IMLSegment* entrySegment = ppcImlGenContext->segmentList2[ppcImlGenContext->segmentList2.size()-1];
			entrySegment->isEnterable = true;
			entrySegment->enterPPCAddress = imlSegment->enterPPCAddress;
			// create jump instruction
			PPCRecompiler_pushBackIMLInstructions(entrySegment, 0, 1);
			entrySegment->imlList.data()[0].make_jump();
			IMLSegment_SetLinkBranchTaken(entrySegment, imlSegment);
			// remove enterable flag from original segment
			imlSegment->isEnterable = false;
			imlSegment->enterPPCAddress = 0;
		}
	}
}