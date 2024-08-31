#pragma once

#include "HW/Espresso/Recompiler/IML/IMLInstruction.h"
#include "../PPCRecompiler.h"
struct CodeContext
{
	virtual ~CodeContext() = default;
};

std::unique_ptr<CodeContext> PPCRecompiler_generateAArch64Code(struct PPCRecFunction_t* PPCRecFunction, struct ppcImlGenContext_t* ppcImlGenContext);

void PPCRecompilerAArch64Gen_generateRecompilerInterfaceFunctions();
