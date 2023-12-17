#pragma once
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"

void LatteSHRC_Init();
void LatteSHRC_UnloadAll();

void LatteSHRC_ResetCachedShaderHash();
void LatteShaderSHRC_UpdateFetchShader();

void LatteSHRC_UpdateActiveShaders();

struct LatteFetchShader* LatteSHRC_GetActiveFetchShader();
LatteDecompilerShader* LatteSHRC_GetActiveVertexShader();
LatteDecompilerShader* LatteSHRC_GetActiveGeometryShader();
LatteDecompilerShader* LatteSHRC_GetActivePixelShader();

LatteDecompilerShader* LatteSHRC_FindVertexShader(uint64 baseHash, uint64 auxHash);
LatteDecompilerShader* LatteSHRC_FindGeometryShader(uint64 baseHash, uint64 auxHash);
LatteDecompilerShader* LatteSHRC_FindPixelShader(uint64 baseHash, uint64 auxHash);


#define GPU7_PS_MAX_INPUTS	32

struct LatteShaderPSInputTable
{
	struct psImport_t
	{
		uint32 semanticId;
		bool isFlat;
		bool isNoPerspective;
	};
	psImport_t import[GPU7_PS_MAX_INPUTS]{}; // GPR is index
	sint32 count;
	uint64 key;
	uint8 paramGen;
	uint8 paramGenGPR;

	// returns semantic id of vertex shader output by index
	// the returned semanticId may not have a match in the pixel shader (use hasPSImportForSemanticId to determine matched exports)
	static sint32 getVertexShaderOutParamSemanticId(uint32* contextRegisters, sint32 index)
	{
		cemu_assert_debug(index >= 0 && index < 32);
		uint32 vsSemanticId = (contextRegisters[mmSPI_VS_OUT_ID_0 + (index / 4)] >> (8 * (index % 4))) & 0xFF;
		return (sint32)vsSemanticId;
	}

	bool hasPSImportForSemanticId(sint32 semanticId)
	{
		for (sint32 i = 0; i < this->count; i++)
		{
			if (this->import[i].semanticId == semanticId)
				return true;
		}
		return false;
	}

	psImport_t* getPSImportBySemanticId(sint32 semanticId)
	{
		if (semanticId > LATTE_ANALYZER_IMPORT_INDEX_PARAM_MAX)
			return nullptr;
		// get import based on semanticId
		sint32 psInputIndex = -1;
		for (sint32 f = 0; f < this->count; f++)
		{
			if (this->import[f].semanticId == semanticId)
				return this->import + f;
		}
		return nullptr;
	}

	sint32 getPSImportLocationBySemanticId(sint32 semanticId)
	{
		if (semanticId > LATTE_ANALYZER_IMPORT_INDEX_PARAM_MAX)
			return -1;
		// get import based on semanticId
		sint32 psInputIndex = -1;
		for (sint32 f = 0; f < this->count; f++)
		{
			if (this->import[f].semanticId == semanticId)
				return f;
		}
		return -1;
	}
};

void LatteShader_UpdatePSInputs(uint32* contextRegisters);
LatteShaderPSInputTable* LatteSHRC_GetPSInputTable();

void LatteShader_free(LatteDecompilerShader* shader);
void LatteSHRC_RemoveFromCacheByHash(uint64 shader_base_hash, uint64 shader_aux_hash, LatteConst::ShaderType type);

extern uint64 _shaderBaseHash_fs;
extern uint64 _shaderBaseHash_vs;
extern uint64 _shaderBaseHash_gs;
extern uint64 _shaderBaseHash_ps;

void LatteShader_GetDecompilerOptions(struct LatteDecompilerOptions& options, LatteConst::ShaderType shaderType, bool geometryShaderEnabled);
LatteDecompilerShader* LatteShader_CreateShaderFromDecompilerOutput(LatteDecompilerOutput_t& decompilerOutput, uint64 baseHash, bool calculateAuxHash, uint64 optionalAuxHash, uint32* contextRegister);

void LatteShader_CreateRendererShader(LatteDecompilerShader* shader, bool compileAsync);
void LatteShader_FinishCompilation(LatteDecompilerShader* shader);

void LatteShader_prepareSeparableUniforms(LatteDecompilerShader* shader);

void LatteSHRC_RegisterShader(LatteDecompilerShader* shader, uint64 baseHash, uint64 auxHash);

void LatteShader_CleanupAfterCompile(LatteDecompilerShader* shader);

#define SHADER_DUMP_TYPE_FETCH		0
#define SHADER_DUMP_TYPE_VERTEX		1
#define SHADER_DUMP_TYPE_GEOMETRY	2
#define SHADER_DUMP_TYPE_PIXEL		3
#define SHADER_DUMP_TYPE_COPY		4
#define SHADER_DUMP_TYPE_COMPUTE	5

void LatteShader_DumpShader(uint64 baseHash, uint64 auxHash, LatteDecompilerShader* shader);
void LatteShader_DumpRawShader(uint64 baseHash, uint64 auxHash, uint32 type, uint8* programCode, uint32 programLen);

// shader cache file
void LatteShaderCache_Load();
void LatteShaderCache_Close();

void LatteShaderCache_writeSeparableVertexShader(uint64 shaderBaseHash, uint64 shaderAuxHash, uint8* fetchShader, uint32 fetchShaderSize, uint8* vertexShader, uint32 vertexShaderSize, uint32* contextRegisters, bool usesGeometryShader);
void LatteShaderCache_writeSeparableGeometryShader(uint64 shaderBaseHash, uint64 shaderAuxHash, uint8* geometryShader, uint32 geometryShaderSize, uint8* gsCopyShader, uint32 gsCopyShaderSize, uint32* contextRegisters, uint32* hleSpecialState, uint32 vsRingParameterCount);
void LatteShaderCache_writeSeparablePixelShader(uint64 shaderBaseHash, uint64 shaderAuxHash, uint8* pixelShader, uint32 pixelShaderSize, uint32* contextRegisters, bool usesGeometryShader);

// todo - refactor this
sint32 LatteDecompiler_getTextureSamplerBaseIndex(LatteConst::ShaderType shaderType);