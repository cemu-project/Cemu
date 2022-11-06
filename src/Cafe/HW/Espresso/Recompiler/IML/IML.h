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

// optimizer passes
// todo - rename
bool PPCRecompiler_reduceNumberOfFPRRegisters(ppcImlGenContext_t* ppcImlGenContext);
bool PPCRecompiler_manageFPRRegisters(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_removeRedundantCRUpdates(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_optimizeDirectFloatCopies(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_optimizeDirectIntegerCopies(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_optimizePSQLoadAndStore(ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_reorderConditionModifyInstructions(ppcImlGenContext_t* ppcImlGenContext);

// register allocator
void IMLRegisterAllocator_AllocateRegisters(ppcImlGenContext_t* ppcImlGenContext);

// debug
void IMLDebug_DumpSegment(struct IMLSegment* imlSegment, sint32 segmentIndex, bool printLivenessRangeInfo = false);
void IMLDebug_Dump(struct ppcImlGenContext_t* ppcImlGenContext);
