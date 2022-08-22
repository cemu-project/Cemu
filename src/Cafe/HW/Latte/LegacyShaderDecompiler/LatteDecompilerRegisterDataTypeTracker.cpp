#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Core/LatteShaderAssembly.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerInstructions.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerInternal.h"

void LatteDecompiler_analyzeDataTypes(LatteDecompilerShaderContext* shaderContext)
{
	// determine default type
	if (shaderContext->analyzer.usesIntegerValues)
	{
		shaderContext->typeTracker.defaultDataType = LATTE_DECOMPILER_DTYPE_SIGNED_INT;
		shaderContext->typeTracker.genIntReg = true;
	}
	else
	{
		shaderContext->typeTracker.defaultDataType = LATTE_DECOMPILER_DTYPE_FLOAT;
		shaderContext->typeTracker.genFloatReg = true;
	}
	// determine register representation
	if (shaderContext->analyzer.usesRelativeGPRWrite)
	{
		shaderContext->typeTracker.useArrayGPRs = true;
	}
}