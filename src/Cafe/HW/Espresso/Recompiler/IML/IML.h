#pragma once 

#include "IMLInstruction.h"
#include "IMLSegment.h"

// analyzer
struct PPCRecCRTracking_t
{
	uint32 readCRBits;
	uint32 writtenCRBits;
};

bool IMLAnalyzer_IsTightFiniteLoop(IMLSegment* imlSegment);
bool IMLAnalyzer_CanTypeWriteCR(IMLInstruction* imlInstruction);
void IMLAnalyzer_GetCRTracking(IMLInstruction* imlInstruction, PPCRecCRTracking_t* crTracking);

// debug
void IMLDebug_DumpSegment(struct IMLSegment* imlSegment, sint32 segmentIndex, bool printLivenessRangeInfo = false);
void IMLDebug_Dump(struct ppcImlGenContext_t* ppcImlGenContext);
