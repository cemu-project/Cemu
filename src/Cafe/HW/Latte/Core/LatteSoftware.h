#pragma once

void LatteSoftware_setupVertexShader(struct LatteFetchShader* fetchShader, void* shaderPtr, sint32 size);
void LatteSoftware_executeVertex(sint32 index);

float* LatteSoftware_getPositionExport();