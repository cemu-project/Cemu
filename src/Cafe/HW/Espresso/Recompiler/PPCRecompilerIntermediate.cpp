#include "PPCRecompiler.h"
#include "PPCRecompilerIml.h"

PPCRecImlSegment_t* PPCRecompiler_getSegmentByPPCJumpAddress(ppcImlGenContext_t* ppcImlGenContext, uint32 ppcOffset)
{
	for(sint32 s=0; s<ppcImlGenContext->segmentListCount; s++)
	{
		if( ppcImlGenContext->segmentList[s]->isJumpDestination && ppcImlGenContext->segmentList[s]->jumpDestinationPPCAddress == ppcOffset )
		{
			return ppcImlGenContext->segmentList[s];
		}
	}
	debug_printf("PPCRecompiler_getSegmentByPPCJumpAddress(): Unable to find segment (ppcOffset 0x%08x)\n", ppcOffset);
	return NULL;
}

void PPCRecompilerIml_setLinkBranchNotTaken(PPCRecImlSegment_t* imlSegmentSrc, PPCRecImlSegment_t* imlSegmentDst)
{
	// make sure segments aren't already linked
	if (imlSegmentSrc->nextSegmentBranchNotTaken == imlSegmentDst)
		return;
	// add as next segment for source
	if (imlSegmentSrc->nextSegmentBranchNotTaken != NULL)
		assert_dbg();
	imlSegmentSrc->nextSegmentBranchNotTaken = imlSegmentDst;
	// add as previous segment for destination
	imlSegmentDst->list_prevSegments.push_back(imlSegmentSrc);
}

void PPCRecompilerIml_setLinkBranchTaken(PPCRecImlSegment_t* imlSegmentSrc, PPCRecImlSegment_t* imlSegmentDst)
{
	// make sure segments aren't already linked
	if (imlSegmentSrc->nextSegmentBranchTaken == imlSegmentDst)
		return;
	// add as next segment for source
	if (imlSegmentSrc->nextSegmentBranchTaken != NULL)
		assert_dbg();
	imlSegmentSrc->nextSegmentBranchTaken = imlSegmentDst;
	// add as previous segment for destination
	imlSegmentDst->list_prevSegments.push_back(imlSegmentSrc);
}

void PPCRecompilerIML_removeLink(PPCRecImlSegment_t* imlSegmentSrc, PPCRecImlSegment_t* imlSegmentDst)
{
	if (imlSegmentSrc->nextSegmentBranchNotTaken == imlSegmentDst)
	{
		imlSegmentSrc->nextSegmentBranchNotTaken = NULL;
	}
	else if (imlSegmentSrc->nextSegmentBranchTaken == imlSegmentDst)
	{
		imlSegmentSrc->nextSegmentBranchTaken = NULL;
	}
	else
		assert_dbg();

	bool matchFound = false;
	for (sint32 i = 0; i < imlSegmentDst->list_prevSegments.size(); i++)
	{
		if (imlSegmentDst->list_prevSegments[i] == imlSegmentSrc)
		{
			imlSegmentDst->list_prevSegments.erase(imlSegmentDst->list_prevSegments.begin()+i);
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
void PPCRecompilerIML_relinkInputSegment(PPCRecImlSegment_t* imlSegmentOrig, PPCRecImlSegment_t* imlSegmentNew)
{
	while (imlSegmentOrig->list_prevSegments.size() != 0)
	{
		PPCRecImlSegment_t* prevSegment = imlSegmentOrig->list_prevSegments[0];
		if (prevSegment->nextSegmentBranchNotTaken == imlSegmentOrig)
		{
			PPCRecompilerIML_removeLink(prevSegment, imlSegmentOrig);
			PPCRecompilerIml_setLinkBranchNotTaken(prevSegment, imlSegmentNew);
		}
		else if (prevSegment->nextSegmentBranchTaken == imlSegmentOrig)
		{
			PPCRecompilerIML_removeLink(prevSegment, imlSegmentOrig);
			PPCRecompilerIml_setLinkBranchTaken(prevSegment, imlSegmentNew);
		}
		else
		{
			assert_dbg();
		}
	}
}

void PPCRecompilerIML_linkSegments(ppcImlGenContext_t* ppcImlGenContext)
{
	for(sint32 s=0; s<ppcImlGenContext->segmentListCount; s++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[s];

		bool isLastSegment = (s+1)>=ppcImlGenContext->segmentListCount;
		PPCRecImlSegment_t* nextSegment = isLastSegment?NULL:ppcImlGenContext->segmentList[s+1];
		// handle empty segment
		if( imlSegment->imlListCount == 0 )
		{
			if (isLastSegment == false)
				PPCRecompilerIml_setLinkBranchNotTaken(imlSegment, ppcImlGenContext->segmentList[s+1]); // continue execution to next segment
			else
				imlSegment->nextSegmentIsUncertain = true;
			continue;
		}
		// check last instruction of segment
		PPCRecImlInstruction_t* imlInstruction = imlSegment->imlList+(imlSegment->imlListCount-1);
		if( imlInstruction->type == PPCREC_IML_TYPE_CJUMP || imlInstruction->type == PPCREC_IML_TYPE_CJUMP_CYCLE_CHECK )
		{
			// find destination segment by ppc jump address
			PPCRecImlSegment_t* jumpDestSegment = PPCRecompiler_getSegmentByPPCJumpAddress(ppcImlGenContext, imlInstruction->op_conditionalJump.jumpmarkAddress);
			if( jumpDestSegment )
			{
				if (imlInstruction->op_conditionalJump.condition != PPCREC_JUMP_CONDITION_NONE)
					PPCRecompilerIml_setLinkBranchNotTaken(imlSegment, nextSegment);
				PPCRecompilerIml_setLinkBranchTaken(imlSegment, jumpDestSegment);
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
			//imlSegment->nextSegment[0] = nextSegment;
			PPCRecompilerIml_setLinkBranchNotTaken(imlSegment, nextSegment);
			//imlSegment->nextSegmentIsUncertain = true;
		}
	}
}

void PPCRecompilerIML_isolateEnterableSegments(ppcImlGenContext_t* ppcImlGenContext)
{
	sint32 initialSegmentCount = ppcImlGenContext->segmentListCount;
	for (sint32 i = 0; i < ppcImlGenContext->segmentListCount; i++)
	{
		PPCRecImlSegment_t* imlSegment = ppcImlGenContext->segmentList[i];
		if (imlSegment->list_prevSegments.empty() == false && imlSegment->isEnterable)
		{
			// spawn new segment at end
			PPCRecompilerIml_insertSegments(ppcImlGenContext, ppcImlGenContext->segmentListCount, 1);
			PPCRecImlSegment_t* entrySegment = ppcImlGenContext->segmentList[ppcImlGenContext->segmentListCount-1];
			entrySegment->isEnterable = true;
			entrySegment->enterPPCAddress = imlSegment->enterPPCAddress;
			// create jump instruction
			PPCRecompiler_pushBackIMLInstructions(entrySegment, 0, 1);
			PPCRecompilerImlGen_generateNewInstruction_jumpSegment(ppcImlGenContext, entrySegment->imlList + 0);
			PPCRecompilerIml_setLinkBranchTaken(entrySegment, imlSegment);
			// remove enterable flag from original segment
			imlSegment->isEnterable = false;
			imlSegment->enterPPCAddress = 0;
		}
	}
}

PPCRecImlInstruction_t* PPCRecompilerIML_getLastInstruction(PPCRecImlSegment_t* imlSegment)
{
	if (imlSegment->imlListCount == 0)
		return nullptr;
	return imlSegment->imlList + (imlSegment->imlListCount - 1);
}
