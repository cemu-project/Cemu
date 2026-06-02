#pragma once

#include "Cafe/HW/Latte/Core/LatteRingBuffer.h"
#include "Cafe/HW/Latte/Core/Latte.h"

void LatteDraw_handleSpecialState8_clearAsDepth();

class LatteShaderPSInputTable;
void rectsEmulationGS_outputSingleVertex(std::string& gsSrc, LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, sint32 vIdx);
void rectsEmulationGS_outputGeneratedVertex(std::string& gsSrc, LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, const char* variant);
void rectsEmulationGS_outputVerticesCode(std::string& gsSrc, LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, sint32 p0, sint32 p1, sint32 p2, sint32 p3, const char* variant, const LatteContextRegister& latteRegister);
