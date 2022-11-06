#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"

IMLSegment* PPCRecompiler_getSegmentByPPCJumpAddress(ppcImlGenContext_t* ppcImlGenContext, uint32 ppcOffset)
{
	for(IMLSegment* segIt : ppcImlGenContext->segmentList2)
	{
		if(segIt->isJumpDestination && segIt->jumpDestinationPPCAddress == ppcOffset )
		{
			return segIt;
		}
	}
	debug_printf("PPCRecompiler_getSegmentByPPCJumpAddress(): Unable to find segment (ppcOffset 0x%08x)\n", ppcOffset);
	return nullptr;
}

void PPCRecompilerIML_linkSegments(ppcImlGenContext_t* ppcImlGenContext)
{
	size_t segCount = ppcImlGenContext->segmentList2.size();
	for(size_t s=0; s<segCount; s++)
	{
		IMLSegment* imlSegment = ppcImlGenContext->segmentList2[s];

		bool isLastSegment = (s+1)>=ppcImlGenContext->segmentList2.size();
		IMLSegment* nextSegment = isLastSegment?nullptr:ppcImlGenContext->segmentList2[s+1];
		// handle empty segment
		if( imlSegment->imlList.empty())
		{
			if (isLastSegment == false)
				IMLSegment_SetLinkBranchNotTaken(imlSegment, ppcImlGenContext->segmentList2[s+1]); // continue execution to next segment
			else
				imlSegment->nextSegmentIsUncertain = true;
			continue;
		}
		// check last instruction of segment
		IMLInstruction* imlInstruction = imlSegment->imlList.data() + (imlSegment->imlList.size() - 1);
		if( imlInstruction->type == PPCREC_IML_TYPE_CJUMP || imlInstruction->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK )
		{
			// find destination segment by ppc jump address
			IMLSegment* jumpDestSegment = PPCRecompiler_getSegmentByPPCJumpAddress(ppcImlGenContext, imlInstruction->op_conditionalJump.jumpmarkAddress);
			if( jumpDestSegment )
			{
				if (imlInstruction->op_conditionalJump.condition != PPCREC_JUMP_CONDITION_NONE)
					IMLSegment_SetLinkBranchNotTaken(imlSegment, nextSegment);
				IMLSegment_SetLinkBranchTaken(imlSegment, jumpDestSegment);
			}
			else
			{
				imlSegment->nextSegmentIsUncertain = true;
			}
		}
		else if( imlInstruction->type == PPCREC_IML_TYPE_MACRO )
		{
			// currently we assume that the next segment is unknown for all macros
			imlSegment->nextSegmentIsUncertain = true;
		}
		else
		{
			// all other instruction types do not branch
			IMLSegment_SetLinkBranchNotTaken(imlSegment, nextSegment);
		}
	}
}

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
			PPCRecompilerImlGen_generateNewInstruction_jumpSegment(ppcImlGenContext, entrySegment->imlList.data() + 0);
			IMLSegment_SetLinkBranchTaken(entrySegment, imlSegment);
			// remove enterable flag from original segment
			imlSegment->isEnterable = false;
			imlSegment->enterPPCAddress = 0;
		}
	}
}