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

// optimizer passes
// todo - rename
bool PPCRecompiler_reduceNumberOfFPRRegisters(struct ppcImlGenContext_t* ppcImlGenContext);
bool PPCRecompiler_manageFPRRegisters(struct ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_optimizeDirectFloatCopies(struct ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_optimizeDirectIntegerCopies(struct ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_optimizePSQLoadAndStore(struct ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_reorderConditionModifyInstructions(struct ppcImlGenContext_t* ppcImlGenContext);

// debug
void IMLDebug_DumpSegment(struct ppcImlGenContext_t* ctx, struct IMLSegment* imlSegment, sint32 segmentIndex, bool printLivenessRangeInfo = false);
void IMLDebug_Dump(struct ppcImlGenContext_t* ppcImlGenContext, bool printLivenessRangeInfo = false);
