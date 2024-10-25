#pragma once 

#include "IMLInstruction.h"
#include "IMLSegment.h"

// optimizer passes
void IMLOptimizer_OptimizeDirectFloatCopies(struct ppcImlGenContext_t* ppcImlGenContext);
void IMLOptimizer_OptimizeDirectIntegerCopies(struct ppcImlGenContext_t* ppcImlGenContext);
void PPCRecompiler_optimizePSQLoadAndStore(struct ppcImlGenContext_t* ppcImlGenContext);

void IMLOptimizer_StandardOptimizationPass(ppcImlGenContext_t& ppcImlGenContext);

// debug
void IMLDebug_DisassembleInstruction(const IMLInstruction& inst, std::string& disassemblyLineOut);
void IMLDebug_DumpSegment(struct ppcImlGenContext_t* ctx, IMLSegment* imlSegment, bool printLivenessRangeInfo = false);
void IMLDebug_Dump(struct ppcImlGenContext_t* ppcImlGenContext, bool printLivenessRangeInfo = false);
