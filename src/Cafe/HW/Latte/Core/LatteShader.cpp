#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Core/LatteShaderAssembly.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/OS/libs/gx2/GX2.h" // todo - remove dependency
#include "Cafe/GraphicPack/GraphicPack2.h"
#include "util/helpers/StringParser.h"
#include "config/ActiveSettings.h"
#include "Cafe/GameProfile/GameProfile.h"
#include "util/containers/flat_hash_map.hpp"
#include <cinttypes>

// experimental new decompiler (WIP)
#include "util/Zir/EmitterGLSL/ZpIREmitGLSL.h"
#include "util/Zir/Core/ZpIRDebug.h"
#include "Cafe/HW/Latte/Transcompiler/LatteTC.h"
#include "Cafe/HW/Latte/ShaderInfo/ShaderInfo.h"

struct _ShaderHashCache
{
	uint64 prevHash1;
	uint64 prevHash2;
	uint32* prevProgramCode;
	uint32 prevProgramSize;
};

_ShaderHashCache hashCacheVS = { 0 };
_ShaderHashCache hashCacheGS = { 0 };
_ShaderHashCache hashCachePS = { 0 };

LatteFetchShader* _activeFetchShader = nullptr;
LatteDecompilerShader* _activeVertexShader = nullptr;
LatteDecompilerShader* _activeGeometryShader = nullptr;
LatteDecompilerShader* _activePixelShader = nullptr;

// runtime shader cache
using SHRC_CACHE_TYPE = ska::flat_hash_map<uint64, LatteDecompilerShader*>;

SHRC_CACHE_TYPE sVertexShaders(512);
SHRC_CACHE_TYPE sGeometryShaders(512);
SHRC_CACHE_TYPE sPixelShaders(512);

uint64 _shaderBaseHash_vs;
uint64 _shaderBaseHash_gs;
uint64 _shaderBaseHash_ps;

std::atomic_int g_compiled_shaders_total = 0;
std::atomic_int g_compiled_shaders_async = 0;

LatteFetchShader* LatteSHRC_GetActiveFetchShader()
{
	return _activeFetchShader;
}

LatteDecompilerShader* LatteSHRC_GetActiveVertexShader()
{
	return _activeVertexShader;
}

LatteDecompilerShader* LatteSHRC_GetActiveGeometryShader()
{
	return _activeGeometryShader;
}

LatteDecompilerShader* LatteSHRC_GetActivePixelShader()
{
	return _activePixelShader;
}

inline ska::flat_hash_map<uint64, LatteDecompilerShader*>& LatteSHRC_GetCacheByType(LatteConst::ShaderType shaderType)
{
	if (shaderType == LatteConst::ShaderType::Vertex)
		return sVertexShaders;
	else if (shaderType == LatteConst::ShaderType::Geometry)
		return sGeometryShaders;	
	cemu_assert_debug(shaderType == LatteConst::ShaderType::Pixel);
	return sPixelShaders;
}

// calculate hash from shader binary
// this algorithm could be more efficient since we could leverage the fact that the size is always aligned to 8 byte
// but since this is baked into the shader names used for gfx packs and shader caches we can't really change this
void _calcShaderHashGeneric(uint32* programCode, uint32 programSize, uint64& outputHash1, uint64& outputHash2)
{
	outputHash1 = 0;
	outputHash2 = 0;
	for (uint32 i = 0; i < programSize / 4; i++)
	{
		uint32 temp = programCode[i];
		outputHash1 += (uint64)temp;
		outputHash2 ^= (uint64)temp;
		outputHash1 = (outputHash1 << 3) | (outputHash1 >> 61);
		outputHash2 = (outputHash2 >> 7) | (outputHash2 << 57);
	}
}

void _calculateShaderProgramHash(uint32* programCode, uint32 programSize, _ShaderHashCache* hashCache, uint64* outputHash1, uint64* outputHash2)
{
	uint64 progHash1 = 0;
	uint64 progHash2 = 0;
	if (!programCode)
	{
		hashCache->prevProgramCode = NULL;
		hashCache->prevProgramSize = 0;
		hashCache->prevHash1 = 0;
		hashCache->prevHash2 = 0;
	}
	else if (hashCache->prevProgramCode != programCode || hashCache->prevProgramSize != programSize)
	{
		_calcShaderHashGeneric(programCode, programSize, progHash1, progHash2);
		hashCache->prevProgramCode = programCode;
		hashCache->prevProgramSize = programSize;
		hashCache->prevHash1 = progHash1;
		hashCache->prevHash2 = progHash2;
	}
	else
	{
		progHash1 = hashCache->prevHash1;
		progHash2 = hashCache->prevHash2;
	}
	*outputHash1 = progHash1;
	*outputHash2 = progHash2;
}

void LatteSHRC_ResetCachedShaderHash()
{
	hashCacheVS.prevProgramCode = 0;
	hashCacheVS.prevProgramSize = 0;
	hashCacheGS.prevProgramCode = 0;
	hashCacheGS.prevProgramSize = 0;
	hashCachePS.prevProgramCode = 0;
	hashCachePS.prevProgramSize = 0;
}

LatteShaderPSInputTable _activePSImportTable;

LatteShaderPSInputTable* LatteSHRC_GetPSInputTable()
{
	return &_activePSImportTable;
}

void LatteSHRC_RemoveFromCache(LatteDecompilerShader* shader)
{
	bool removed = false;
	auto& cache = LatteSHRC_GetCacheByType(shader->shaderType);
	// remove from hashtable
	auto baseIt = cache.find(shader->baseHash);
	if (baseIt == cache.end())
	{
		cemu_assert_suspicious(); // deleting from runtime cache but shader is not present?
	}
	else if (baseIt->second == shader)
	{
        cemu_assert_debug(baseIt->second == shader);
        cache.erase(baseIt);
		if (shader->next)
        {
            cemu_assert_debug(shader->baseHash == shader->next->baseHash);
            cache.emplace(shader->baseHash, shader->next);
        }
        shader->next = 0;
		removed = true;
	}
	else
	{
		// remove from chain
		LatteDecompilerShader* shaderChain = baseIt->second;
		while (shaderChain->next)
		{
			if (shaderChain->next == shader)
			{
				shaderChain->next = shaderChain->next->next;
				removed = true;
				break;
			}
		}
	}
	cemu_assert(removed);
}

void LatteSHRC_RemoveFromCacheByHash(uint64 shader_base_hash, uint64 shader_aux_hash, LatteConst::ShaderType type)
{
	LatteDecompilerShader* shader = nullptr;
	if (type == LatteConst::ShaderType::Vertex)
		shader = LatteSHRC_FindVertexShader(shader_base_hash, shader_aux_hash);
	else if (type == LatteConst::ShaderType::Geometry)
		shader = LatteSHRC_FindGeometryShader(shader_base_hash, shader_aux_hash);
	else if (type == LatteConst::ShaderType::Pixel)
		shader = LatteSHRC_FindPixelShader(shader_base_hash, shader_aux_hash);
	if (shader)
		LatteSHRC_RemoveFromCache(shader);
}

void LatteShader_free(LatteDecompilerShader* shader)
{
	LatteSHRC_RemoveFromCache(shader);
	if (shader->shader)
		delete shader->shader;
	shader->shader = nullptr;
	delete shader;
}

// both vertex and geometry/pixel shader depend on PS inputs
// we prepare the PS import info in advance
void LatteShader_UpdatePSInputs(uint32* contextRegisters)
{
	// PS control
	uint32 psControl0 = contextRegisters[mmSPI_PS_IN_CONTROL_0];
	uint32 spi0_positionEnable = (psControl0 >> 8) & 1;
	uint32 spi0_positionCentroid = (psControl0 >> 9) & 1;
	cemu_assert_debug(spi0_positionCentroid == 0); // controls gl_FragCoord
	uint32 spi0_positionAddr = (psControl0 >> 10) & 0x1F; // controls gl_FragCoord
	uint32 spi0_paramGen = (psControl0 >> 15) & 0xF; // used for gl_PointCoords
	uint32 spi0_paramGenAddr = (psControl0 >> 19) & 0x7F;
	sint32 importIndex = 0;

	//cemu_assert_debug(((psControl0>>26)&3) == 1); // BARYC_SAMPLE_CNTL
	//cemu_assert_debug((psControl0&(1 << 28)) == 0); // PERSP_GRADIENT_ENA
	//cemu_assert_debug((psControl0&(1 << 29)) == 0); // LINEAR_GRADIENT_ENA
	// if LINEAR_GRADIENT_ENA_bit is enabled, the pixel shader accesses gl_ClipSize?

	// VS/GS parameters
	uint32 numPSInputs = contextRegisters[mmSPI_PS_IN_CONTROL_0] & 0x3F;
	uint64 key = 0;

	if (spi0_positionEnable)
	{
		key += (uint64)spi0_positionAddr + 1;
	}

	// parameter gen
	if (spi0_paramGen != 0)
	{
		key += std::rotr<uint64>(spi0_paramGen, 7);
		key += std::rotr<uint64>(spi0_paramGenAddr, 3);
		_activePSImportTable.paramGen = spi0_paramGen;
		_activePSImportTable.paramGenGPR = spi0_paramGenAddr;
	}
	else
	{
		_activePSImportTable.paramGen = 0;
	}

	// semantic imports from vertex shader
#ifdef CEMU_DEBUG_ASSERT
	uint8 semanticMask[256 / 8] = { 0 };
#endif
	cemu_assert_debug(numPSInputs <= GPU7_PS_MAX_INPUTS);
	numPSInputs = std::min<uint32>(numPSInputs, GPU7_PS_MAX_INPUTS);

	for (uint32 f = 0; f < numPSInputs; f++)
	{
		uint32 psInputControl = contextRegisters[mmSPI_PS_INPUT_CNTL_0 + f];
		uint32 psSemanticId = (psInputControl & 0xFF);

		uint8 defaultValue = (psInputControl>>8)&3;
		// default:
		// 0 -> 0.0 0.0 0.0 0.0
		// 1 -> 0.0 0.0 0.0 1.0
		// 2 -> 1.0 1.0 1.0 0.0
		// 3 -> 1.0 1.0 1.0 1.0
		cemu_assert_debug(defaultValue <= 1);

		uint32 uknBits = psInputControl & ~((0xFF)|(0x3<<8) | (1 << 10) | (1 << 12));
		uknBits &= ~0x800; // FLAT_SHADE
		//cemu_assert_debug(uknBits == 0);
		//cemu_assert_debug(((psInputControl >> 11) & 1) == 0); // centroid
		//cemu_assert_debug(((psInputControl >> 17) & 1) == 0); // point sprite coord
		cemu_assert_debug(psSemanticId != 0xFF);

		key += (uint64)psInputControl;
		key = std::rotl<uint64>(key, 7);
		if (spi0_positionEnable && f == spi0_positionAddr)
		{
			_activePSImportTable.import[f].semanticId = LATTE_ANALYZER_IMPORT_INDEX_SPIPOSITION;
			_activePSImportTable.import[f].isFlat = false;
			_activePSImportTable.import[f].isNoPerspective = false;
			key += (uint64)0x33;
		}
		else
		{
#ifdef CEMU_DEBUG_ASSERT
			if (semanticMask[psSemanticId >> 3] & (1 << (psSemanticId & 7)))
			{
				cemuLog_logDebug(LogType::Force, "SemanticId already used");
			}
			semanticMask[psSemanticId >> 3] |= (1 << (psSemanticId & 7));
#endif

			_activePSImportTable.import[f].semanticId = psSemanticId;
			_activePSImportTable.import[f].isFlat = (psInputControl&(1 << 10)) != 0;
			_activePSImportTable.import[f].isNoPerspective = (psInputControl&(1 << 12)) != 0;
		}
	}
	_activePSImportTable.key = key;
	_activePSImportTable.count = numPSInputs;
}

void LatteShader_CreateRendererShader(LatteDecompilerShader* shader, bool compileAsync)
{
	if (shader->hasError )
	{
		cemuLog_log(LogType::Force, "Unable to compile shader {:016x}", shader->baseHash);
		return;
	}

	GraphicPack2::GP_SHADER_TYPE gpShaderType;
	RendererShader::ShaderType shaderType;
	if (shader->shaderType == LatteConst::ShaderType::Vertex)
	{
		shaderType = RendererShader::ShaderType::kVertex;
		gpShaderType = GraphicPack2::GP_SHADER_TYPE::VERTEX;
	}
	else if (shader->shaderType == LatteConst::ShaderType::Geometry)
	{
		shaderType = RendererShader::ShaderType::kGeometry;
		gpShaderType = GraphicPack2::GP_SHADER_TYPE::GEOMETRY;
	}	
	else if (shader->shaderType == LatteConst::ShaderType::Pixel)
	{
		shaderType = RendererShader::ShaderType::kFragment;
		gpShaderType = GraphicPack2::GP_SHADER_TYPE::PIXEL;
	}

	// check if a custom shader is present
	std::string shaderSrc;

	const std::string* customShaderSrc = GraphicPack2::FindCustomShaderSource(shader->baseHash, shader->auxHash, gpShaderType, g_renderer->GetType() == RendererAPI::Vulkan);
	if (customShaderSrc)
	{
		shaderSrc.assign(*customShaderSrc);
		shader->isCustomShader = true;
	}
	else
		shaderSrc.assign(shader->strBuf_shaderSource->c_str());

	if (shaderType == RendererShader::ShaderType::kVertex &&
		(shader->baseHash == 0x15bc7edf9de2ed30 || shader->baseHash == 0x83a697d61a3b9202 ||
		 shader->baseHash == 0x97bc44a5028381c6 || shader->baseHash == 0x24838b84d15a1da1))
	{
		cemuLog_logDebug(LogType::Force, "Filtered shader to avoid AMD crash");
		shader->shader = nullptr;
		shader->hasError = true;
		return;
	}

	// create shader
	shader->shader = g_renderer->shader_create(shaderType, shader->baseHash, shader->auxHash, shaderSrc, true, shader->isCustomShader);
	if (shader->shader == nullptr)
		shader->hasError = true;
	// after renderer shader creation we can throw away any intermediate info
	LatteShader_CleanupAfterCompile(shader);
}

void LatteShader_FinishCompilation(LatteDecompilerShader* shader)
{
	if (shader->hasError)
	{
		cemuLog_logDebug(LogType::Force, "LatteShader_finishCompilation(): Skipped because of error in shader {:x}", shader->baseHash);
		return;
	}
	shader->shader->WaitForCompiled();

	LatteShader_prepareSeparableUniforms(shader);
	LatteShader_CleanupAfterCompile(shader);
}

void LatteSHRC_RegisterShader(LatteDecompilerShader* shader, uint64 baseHash, uint64 auxHash)
{
	auto& cache = LatteSHRC_GetCacheByType(shader->shaderType);
	shader->baseHash = baseHash;
	shader->auxHash = auxHash;

	auto it = cache.find(baseHash);
	if (it == cache.end())
	{
		shader->next = nullptr;
		cache.emplace(shader->baseHash, shader);
	}
	else
	{
		shader->next = it->second->next;
		it->second->next = shader;
	}
}

LatteDecompilerShader* LatteSHRC_GetFromChain(LatteDecompilerShader* baseShader, uint64 baseHash, uint64 auxHash)
{
	while (baseShader && baseShader->auxHash != auxHash)
		baseShader = baseShader->next;
	return baseShader;
}

LatteDecompilerShader* LatteSHRC_Get(SHRC_CACHE_TYPE& cache, uint64 baseHash, uint64 auxHash)
{
	auto it = cache.find(baseHash);
	if (it == cache.end())
		return nullptr;
	LatteDecompilerShader* baseShader = it->second;
	if (!baseShader)
		return nullptr;
	while (baseShader && baseShader->auxHash != auxHash)
		baseShader = baseShader->next;
	return baseShader;
}

LatteDecompilerShader* LatteSHRC_FindVertexShader(uint64 baseHash, uint64 auxHash)
{
	return LatteSHRC_Get(sVertexShaders, baseHash, auxHash);
}

LatteDecompilerShader* LatteSHRC_FindGeometryShader(uint64 baseHash, uint64 auxHash)
{
	return LatteSHRC_Get(sGeometryShaders, baseHash, auxHash);
}

LatteDecompilerShader* LatteSHRC_FindPixelShader(uint64 baseHash, uint64 auxHash)
{
	return LatteSHRC_Get(sPixelShaders, baseHash, auxHash);
}

// update the currently active fetch shader
void LatteShaderSHRC_UpdateFetchShader()
{
	_activeFetchShader = LatteFetchShader::FindByGPUState();
}

void LatteShader_CleanupAfterCompile(LatteDecompilerShader* shader)
{
	if (shader->strBuf_shaderSource)
	{
		delete shader->strBuf_shaderSource;
		shader->strBuf_shaderSource = nullptr;
	}
}

void LatteShader_DumpShader(uint64 baseHash, uint64 auxHash, LatteDecompilerShader* shader)
{
	if (!ActiveSettings::DumpShadersEnabled())
		return;
	
	const char* suffix = "";
	if (shader->shaderType == LatteConst::ShaderType::Vertex)
		suffix = "vs";
	else if (shader->shaderType == LatteConst::ShaderType::Geometry)
		suffix = "gs";
	else if (shader->shaderType == LatteConst::ShaderType::Pixel)
		suffix = "ps";
	fs::path dumpPath = "dump/shaders";
	dumpPath /= fmt::format("{:016x}_{:016x}_{}.txt", baseHash, auxHash, suffix);
	FileStream* fs = FileStream::createFile2(dumpPath);
	if (fs)
	{
		if (shader->strBuf_shaderSource)
			fs->writeData(shader->strBuf_shaderSource->c_str(), shader->strBuf_shaderSource->getLen());
		delete fs;
	}
}

void LatteShader_DumpRawShader(uint64 baseHash, uint64 auxHash, uint32 type, uint8* programCode, uint32 programLen)
{
	if (!ActiveSettings::DumpShadersEnabled())
		return;
	const char* suffix = "";
	if (type == SHADER_DUMP_TYPE_FETCH)
		suffix = "fs";
	else if (type == SHADER_DUMP_TYPE_VERTEX)
		suffix = "vs";
	else if (type == SHADER_DUMP_TYPE_GEOMETRY)
		suffix = "gs";
	else if (type == SHADER_DUMP_TYPE_PIXEL)
		suffix = "ps";
	else if (type == SHADER_DUMP_TYPE_COPY)
		suffix = "copy";
	else if (type == SHADER_DUMP_TYPE_COMPUTE)
		suffix = "compute";
	fs::path dumpPath = "dump/shaders";
	dumpPath /= fmt::format("{:016x}_{:016x}_{}.bin", baseHash, auxHash, suffix);
	FileStream* fs = FileStream::createFile2(dumpPath);
	if (fs)
	{
		fs->writeData(programCode, programLen);
		delete fs;
	}
}

void LatteSHRC_UpdateVSBaseHash(uint8* vertexShaderPtr, uint32 vertexShaderSize, bool usesGeometryShader)
{
	uint32* vsProgramCode = (uint32*)vertexShaderPtr;
	// update hash from vertex shader data
	uint64 vsHash1 = 0;
	uint64 vsHash2 = 0;
	_calculateShaderProgramHash(vsProgramCode, vertexShaderSize, &hashCacheVS, &vsHash1, &vsHash2);
	uint64 vsHash = vsHash1 + vsHash2 + _activeFetchShader->key + _activePSImportTable.key + (usesGeometryShader ? 0x1111ULL : 0ULL);

	uint32 tmp = LatteGPUState.contextNew.PA_CL_VTE_CNTL.getRawValue() ^ 0x43F;
	vsHash += tmp;

	auto primitiveType = LatteGPUState.contextNew.VGT_PRIMITIVE_TYPE.get_PRIMITIVE_MODE();
	if (primitiveType == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::RECTS)
	{
		vsHash += 13ULL;
	}
	else if (primitiveType == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::POINTS)
	{
		// required for Vulkan since we have to write the pointsize in the shader
		vsHash += 71ULL;
	}
	vsHash += (LatteGPUState.contextRegister[mmVGT_STRMOUT_EN] ? 21 : 0);
	// halfZ
	if (LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_DX_CLIP_SPACE_DEF())
		vsHash += 0x1537;

	_shaderBaseHash_vs = vsHash;
}

void LatteSHRC_UpdateGSBaseHash(uint8* geometryShaderPtr, uint32 geometryShaderSize, uint8* geometryCopyShader, uint32 geometryCopyShaderSize)
{
	// update hash from geometry shader data
	uint64 gsHash1 = 0;
	uint64 gsHash2 = 0;
	_calculateShaderProgramHash((uint32*)geometryShaderPtr, geometryShaderSize, &hashCacheVS, &gsHash1, &gsHash2);
	// get geometry shader
	uint64 gsHash = gsHash1 + gsHash2;
	gsHash += (uint64)_activeVertexShader->ringParameterCount;
	gsHash += (LatteGPUState.contextRegister[mmVGT_STRMOUT_EN] ? 21 : 0);
	_shaderBaseHash_gs = gsHash;
}

void LatteSHRC_UpdatePSBaseHash(uint8* pixelShaderPtr, uint32 pixelShaderSize, bool usesGeometryShader)
{
	uint32* psProgramCode = (uint32*)pixelShaderPtr;
	// update hash from pixel shader data
	uint64 psHash1 = 0;
	uint64 psHash2 = 0;
	_calculateShaderProgramHash(psProgramCode, pixelShaderSize, &hashCachePS, &psHash1, &psHash2);
	// get vertex shader
	uint64 psHash = psHash1 + psHash2 + _activePSImportTable.key + (usesGeometryShader ? hashCacheGS.prevHash1 : 0ULL);
	_shaderBaseHash_ps = psHash;
}

uint64 LatteSHRC_CalcVSAuxHash(LatteDecompilerShader* vertexShader, uint32* contextRegisters)
{
	// todo - include texture types in aux hash similar to how it is already done in pixel shader
	//        or maybe there is a way to figure out the proper texture types?
	uint64 auxHash = 0;
	if(vertexShader->hasStreamoutBufferWrite)
	{
		// hash stride for streamout buffers
		for (uint32 i = 0; i < LATTE_NUM_STREAMOUT_BUFFER; i++)
		{
			if(!vertexShader->streamoutBufferWriteMask[i])
				continue;
			uint32 bufferStride = contextRegisters[mmVGT_STRMOUT_VTX_STRIDE_0 + i * 4];
			auxHash = std::rotl<uint64>(auxHash, 7);
			auxHash += (uint64)bufferStride;
		}
	}
	// textures can affect the shader. Either by their type (2D, 3D, cubemap) or by their format (float vs integer)
	uint64 auxHashTex = 0;
	for (uint8 i = 0; i < vertexShader->textureUnitListCount; i++)
	{
		uint8 t = vertexShader->textureUnitList[i];
		uint32 word4 = contextRegisters[Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_VS + t * 7 + 4];
		if ((word4 & 0x300) == 0x100)
		{
			// integer format
			auxHashTex = std::rotl<uint64>(auxHashTex, 7);
			auxHashTex += 0x333;
		}
	}
	return auxHash + auxHashTex;
}

uint64 LatteSHRC_CalcGSAuxHash(LatteDecompilerShader* geometryShader)
{
	// todo - include texture types in aux hash similar to how it is already done in pixel shader
	return 0;
}

uint64 LatteSHRC_CalcPSAuxHash(LatteDecompilerShader* pixelShader, uint32* contextRegisters)
{
	uint64 auxHash = 0;
	// CB_SHADER_MASK can remap pixel shader outputs
	auxHash = (auxHash >> 3) | (auxHash << 61);
	auxHash += (uint64)contextRegisters[mmCB_SHADER_MASK];
	// alpha test
	uint8 alphaTestFunc = contextRegisters[Latte::REGADDR::SX_ALPHA_TEST_CONTROL] & 0x7;
	uint8 alphaTestEnable = (contextRegisters[Latte::REGADDR::SX_ALPHA_TEST_CONTROL] >> 3) & 1;
	if (alphaTestEnable)
	{
		auxHash += (uint64)alphaTestFunc;
		auxHash = (auxHash >> 3) | (auxHash << 61);
		auxHash += 1;
	}
	// texture types (2D, 3D, cubemap etc.) affect the shader too
	for (uint8 i = 0; i < pixelShader->textureUnitListCount; i++)
	{
		uint8 t = pixelShader->textureUnitList[i];
		uint32 word0 = contextRegisters[Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_PS + t * 7 + 0];
		uint32 dim = (word0 & 7);
		auxHash = (auxHash << 3) | (auxHash >> 61);
		auxHash += (uint64)dim;
	}
	return auxHash;
}

LatteDecompilerShader* LatteShader_CreateShaderFromDecompilerOutput(LatteDecompilerOutput_t& decompilerOutput, uint64 baseHash, bool calculateAuxHash, uint64 optionalAuxHash, uint32* contextRegister)
{
	LatteDecompilerShader* shader = decompilerOutput.shader;
	shader->baseHash = baseHash;
	// copy resource mapping
	if(g_renderer->GetType() == RendererAPI::Vulkan)
		shader->resourceMapping = decompilerOutput.resourceMappingVK;
	else
		shader->resourceMapping = decompilerOutput.resourceMappingGL;
	// copy texture info
	shader->textureUnitMask2 = decompilerOutput.textureUnitMask;
	// copy streamout info
	shader->streamoutBufferWriteMask = decompilerOutput.streamoutBufferWriteMask;
	shader->hasStreamoutBufferWrite = decompilerOutput.streamoutBufferWriteMask.any();
	// copy uniform offsets
	// for OpenGL these are retrieved in _prepareSeparableUniforms()
	if (g_renderer->GetType() == RendererAPI::Vulkan)
	{
		shader->uniform.loc_remapped = decompilerOutput.uniformOffsetsVK.offset_remapped;
		shader->uniform.loc_uniformRegister = decompilerOutput.uniformOffsetsVK.offset_uniformRegister;
		shader->uniform.count_uniformRegister = decompilerOutput.uniformOffsetsVK.count_uniformRegister;
		shader->uniform.loc_windowSpaceToClipSpaceTransform = decompilerOutput.uniformOffsetsVK.offset_windowSpaceToClipSpaceTransform;
		shader->uniform.loc_alphaTestRef = decompilerOutput.uniformOffsetsVK.offset_alphaTestRef;
		shader->uniform.loc_pointSize = decompilerOutput.uniformOffsetsVK.offset_pointSize;
		shader->uniform.loc_fragCoordScale = decompilerOutput.uniformOffsetsVK.offset_fragCoordScale;
		for (sint32 t = 0; t < LATTE_NUM_MAX_TEX_UNITS; t++)
		{
			if (decompilerOutput.uniformOffsetsVK.offset_texScale[t] >= 0)
			{
				LatteUniformTextureScaleEntry_t entry = { 0 };
				entry.texUnit = t;
				entry.uniformLocation = decompilerOutput.uniformOffsetsVK.offset_texScale[t];
				shader->uniform.list_ufTexRescale.push_back(entry);
			}
		}
		shader->uniform.loc_verticesPerInstance = decompilerOutput.uniformOffsetsVK.offset_verticesPerInstance;
		for (sint32 t = 0; t < LATTE_NUM_STREAMOUT_BUFFER; t++)
			shader->uniform.loc_streamoutBufferBase[t] = decompilerOutput.uniformOffsetsVK.offset_streamoutBufferBase[t];
		shader->uniform.uniformRangeSize = decompilerOutput.uniformOffsetsVK.offset_endOfBlock;
	}
	else
	{
		shader->uniform.count_uniformRegister = decompilerOutput.uniformOffsetsVK.count_uniformRegister;
	}
	// calculate aux hash
	if (calculateAuxHash)
	{
		if (decompilerOutput.shaderType == LatteConst::ShaderType::Vertex)
		{
			uint64 vsAuxHash = LatteSHRC_CalcVSAuxHash(shader, contextRegister);
			shader->auxHash = vsAuxHash;
		}
		else if (decompilerOutput.shaderType == LatteConst::ShaderType::Geometry)
		{
			uint64 gsAuxHash = LatteSHRC_CalcGSAuxHash(shader);
			shader->auxHash = gsAuxHash;
		}
		else if (decompilerOutput.shaderType == LatteConst::ShaderType::Pixel)
		{
			uint64 psAuxHash = LatteSHRC_CalcPSAuxHash(shader, contextRegister);
			shader->auxHash = psAuxHash;
		}
		else
			cemu_assert_debug(false);
	}
	else
	{
		shader->auxHash = optionalAuxHash;
	}
	return shader;
}

void LatteShader_GetDecompilerOptions(LatteDecompilerOptions& options, LatteConst::ShaderType shaderType, bool geometryShaderEnabled)
{
	options.usesGeometryShader = geometryShaderEnabled;
	options.spirvInstrinsics.hasRoundingModeRTEFloat32 = false;
	if (g_renderer->GetType() == RendererAPI::Vulkan)
	{
		options.useTFViaSSBO = VulkanRenderer::GetInstance()->UseTFViaSSBO();
		options.spirvInstrinsics.hasRoundingModeRTEFloat32 = VulkanRenderer::GetInstance()->HasSPRIVRoundingModeRTE32();
	}
	options.strictMul = g_current_game_profile->GetAccurateShaderMul() != AccurateShaderMulOption::False;
}

LatteDecompilerShader* LatteShader_CompileSeparableVertexShader2(uint64 baseHash, uint64& vsAuxHash, uint8* vertexShaderPtr, uint32 vertexShaderSize, bool usesGeometryShader, LatteFetchShader* fetchShader)
{
	/* Analyze shader to gather general information about inputs/outputs */
	Latte::ShaderDescription shaderDescription;
	if (!shaderDescription.analyzeShaderCode(vertexShaderPtr, vertexShaderSize, LatteConst::ShaderType::Vertex))
	{
		assert_dbg();
		return nullptr;
	}
	/* Create context dependent IO info for this shader */
	//Latte::ShaderInstanceInfo
	assert_dbg();

	// todo - Use ShaderInstanceInfo when generating the GLSL (GLSL::Emit() should take a 'GLSLInfoSource' class which has a bunch of virtual methods for retrieving uniform names etc. We then override this class and plug in logic using ShaderInstanceInfo

	/* Translate R600Plus to GLSL */
	ZpIR::DebugPrinter irDebugPrinter;
	LatteTCGenIR genIR;
	genIR.setVertexShaderContext(fetchShader, LatteGPUState.contextRegister + mmSQ_VTX_SEMANTIC_0);
	auto irObj = genIR.transcompileLatteToIR(vertexShaderPtr, vertexShaderSize, LatteTCGenIR::VERTEX);
	// debug output (before register allocation)
	irDebugPrinter.setShowPhysicalRegisters(false);
	irDebugPrinter.debugPrint(irObj);
	// register allocation
	ZirPass::RegisterAllocatorForGLSL ra(irObj);
	ra.applyPass();
	// debug output (after register allocation)
	irDebugPrinter.setShowPhysicalRegisters(true);
	irDebugPrinter.setPhysicalRegisterNameSource(ZirPass::RegisterAllocatorForGLSL::DebugPrintHelper_getPhysRegisterName);
	irDebugPrinter.debugPrint(irObj);
	// gen GLSL
	StringBuf glslSourceBuffer(64 * 1024);
	// emit GLSL header
	assert_dbg(); // todo
	// emit main
	ZirEmitter::GLSL emitter;
	emitter.Emit(irObj, &glslSourceBuffer);

	// debug copy to string
	std::string dbg;
	dbg.insert(0, glslSourceBuffer.c_str(), glslSourceBuffer.getLen());
	assert_dbg();


	return nullptr;
}

// compile new vertex shader (relies partially on current state)
LatteDecompilerShader* LatteShader_CompileSeparableVertexShader(uint64 baseHash, uint64& vsAuxHash, uint8* vertexShaderPtr, uint32 vertexShaderSize, bool usesGeometryShader, LatteFetchShader* fetchShader)
{
	// new decompiler test
	//LatteShader_CompileSeparableVertexShader2(baseHash, vsAuxHash, vertexShaderPtr, vertexShaderSize, usesGeometryShader, fetchShader);

	// legacy decompiler
	LatteDecompilerOptions options;
	LatteShader_GetDecompilerOptions(options, LatteConst::ShaderType::Vertex, usesGeometryShader);

	LatteDecompilerOutput_t decompilerOutput{};
	LatteDecompiler_DecompileVertexShader(_shaderBaseHash_vs, LatteGPUState.contextRegister, vertexShaderPtr, vertexShaderSize, fetchShader, options, &decompilerOutput);
	LatteDecompilerShader* vertexShader = LatteShader_CreateShaderFromDecompilerOutput(decompilerOutput, baseHash, true, 0, LatteGPUState.contextRegister);
	vsAuxHash = vertexShader->auxHash;
	if (vertexShader->hasError == false)
	{
		uint8* fsProgramCode = (uint8*)memory_getPointerFromPhysicalOffset(LatteGPUState.contextRegister[mmSQ_PGM_START_FS + 0] << 8);
		uint32 fsProgramSize = LatteGPUState.contextRegister[mmSQ_PGM_START_FS + 1] << 3;
		LatteShaderCache_writeSeparableVertexShader(vertexShader->baseHash, vertexShader->auxHash, fsProgramCode, fsProgramSize, vertexShaderPtr, vertexShaderSize, LatteGPUState.contextRegister, usesGeometryShader);
	}
	LatteShader_DumpShader(vertexShader->baseHash, vertexShader->auxHash, vertexShader);
	LatteShader_DumpRawShader(vertexShader->baseHash, vertexShader->auxHash, SHADER_DUMP_TYPE_VERTEX, vertexShaderPtr, vertexShaderSize);
	LatteShader_CreateRendererShader(vertexShader, false);
	performanceMonitor.numCompiledVS++;

	if (g_renderer->GetType() == RendererAPI::OpenGL)
	{
		if (vertexShader->shader)
			vertexShader->shader->PreponeCompilation(true);
		LatteShader_FinishCompilation(vertexShader);
	}

	LatteSHRC_RegisterShader(vertexShader, vertexShader->baseHash, vertexShader->auxHash);
	return vertexShader;
}

LatteDecompilerShader* LatteShader_CompileSeparableGeometryShader(uint64 baseHash, uint8* geometryShaderPtr, uint32 geometryShaderSize, uint8* geometryCopyShader, uint32 geometryCopyShaderSize)
{
	LatteDecompilerOptions options;
	LatteShader_GetDecompilerOptions(options, LatteConst::ShaderType::Geometry, true);

	LatteDecompilerOutput_t decompilerOutput{};
	LatteDecompiler_DecompileGeometryShader(_shaderBaseHash_gs, LatteGPUState.contextRegister, geometryShaderPtr, geometryShaderSize, geometryCopyShader, geometryCopyShaderSize, _activeVertexShader->ringParameterCount, options, &decompilerOutput);
	LatteDecompilerShader* geometryShader = LatteShader_CreateShaderFromDecompilerOutput(decompilerOutput, baseHash, true, 0, LatteGPUState.contextRegister);
	if (geometryShader->hasError == false)
	{
		LatteShaderCache_writeSeparableGeometryShader(geometryShader->baseHash, geometryShader->auxHash, geometryShaderPtr, geometryShaderSize, geometryCopyShader, geometryCopyShaderSize, LatteGPUState.contextRegister, LatteGPUState.contextNew.GetSpecialStateValues(), _activeVertexShader->ringParameterCount);
	}
	LatteShader_DumpShader(geometryShader->baseHash, geometryShader->auxHash, geometryShader);
	LatteShader_DumpRawShader(geometryShader->baseHash, geometryShader->auxHash, SHADER_DUMP_TYPE_GEOMETRY, geometryShaderPtr, geometryShaderSize);
	LatteShader_DumpRawShader(geometryShader->baseHash, geometryShader->auxHash, SHADER_DUMP_TYPE_COPY, geometryCopyShader, geometryCopyShaderSize);
	LatteShader_CreateRendererShader(geometryShader, false);
	performanceMonitor.numCompiledGS++;

	if (g_renderer->GetType() == RendererAPI::OpenGL)
	{
		if (geometryShader->shader)
			geometryShader->shader->PreponeCompilation(true);
		LatteShader_FinishCompilation(geometryShader);
	}

	LatteSHRC_RegisterShader(geometryShader, geometryShader->baseHash, geometryShader->auxHash);
	return geometryShader;
}

LatteDecompilerShader* LatteShader_CompileSeparablePixelShader(uint64 baseHash, uint64& psAuxHash, uint8* pixelShaderPtr, uint32 pixelShaderSize, bool usesGeometryShader)
{
	LatteDecompilerOptions options;
	LatteShader_GetDecompilerOptions(options, LatteConst::ShaderType::Pixel, usesGeometryShader);

	LatteDecompilerOutput_t decompilerOutput{};
	LatteDecompiler_DecompilePixelShader(baseHash, LatteGPUState.contextRegister, pixelShaderPtr, pixelShaderSize, options, &decompilerOutput);
	LatteDecompilerShader* pixelShader = LatteShader_CreateShaderFromDecompilerOutput(decompilerOutput, baseHash, true, 0, LatteGPUState.contextRegister);
	psAuxHash = pixelShader->auxHash;
	LatteShader_DumpShader(_shaderBaseHash_ps, psAuxHash, pixelShader);
	LatteShader_DumpRawShader(_shaderBaseHash_ps, psAuxHash, SHADER_DUMP_TYPE_PIXEL, pixelShaderPtr, pixelShaderSize);
	LatteShader_CreateRendererShader(pixelShader, false);
	performanceMonitor.numCompiledPS++;
	if (pixelShader->hasError == false)
	{
		LatteShaderCache_writeSeparablePixelShader(_shaderBaseHash_ps, psAuxHash, pixelShaderPtr, pixelShaderSize, LatteGPUState.contextRegister, usesGeometryShader);
	}

	if (g_renderer->GetType() == RendererAPI::OpenGL)
	{
		if (pixelShader->shader)
			pixelShader->shader->PreponeCompilation(true);
		LatteShader_FinishCompilation(pixelShader);
	}

	LatteSHRC_RegisterShader(pixelShader, _shaderBaseHash_ps, psAuxHash);
	return pixelShader;
}

void LatteSHRC_UpdateVertexShader(uint8* vertexShaderPtr, uint32 vertexShaderSize, bool usesGeometryShader)
{
	// todo - should include VTX_SEMANTIC table in state

	LatteSHRC_UpdateVSBaseHash(vertexShaderPtr, vertexShaderSize, usesGeometryShader);
	uint64 vsAuxHash = 0;
	auto itBaseShader = sVertexShaders.find(_shaderBaseHash_vs);
	LatteDecompilerShader* vertexShader = nullptr;
	if (itBaseShader != sVertexShaders.end())
	{
		vsAuxHash = LatteSHRC_CalcVSAuxHash(itBaseShader->second, LatteGPUState.contextRegister);
		vertexShader = LatteSHRC_GetFromChain(itBaseShader->second, _shaderBaseHash_vs, vsAuxHash);
	}
	if (!vertexShader)
		vertexShader = LatteShader_CompileSeparableVertexShader(_shaderBaseHash_vs, vsAuxHash, vertexShaderPtr, vertexShaderSize, usesGeometryShader, _activeFetchShader);
	if (vertexShader->hasError)
	{
		LatteGPUState.activeShaderHasError = true;
		return;
	}
	g_renderer->shader_bind(vertexShader->shader);
	_activeVertexShader = vertexShader;
}

void LatteSHRC_UpdateGeometryShader(bool usesGeometryShader, uint8* geometryShaderPtr, uint32 geometryShaderSize, uint8* geometryCopyShader, uint32 geometryCopyShaderSize)
{
	if (usesGeometryShader == false || _activeVertexShader == nullptr)
	{
		g_renderer->shader_unbind(RendererShader::ShaderType::kGeometry);
		_shaderBaseHash_gs = 0;
		_activeGeometryShader = nullptr;
		return;
	}
	LatteSHRC_UpdateGSBaseHash(geometryShaderPtr, geometryShaderSize, geometryCopyShader, geometryCopyShaderSize);
	auto itBaseShader = sGeometryShaders.find(_shaderBaseHash_gs);
	LatteDecompilerShader* geometryShader;
	if (itBaseShader != sGeometryShaders.end())
	{
		// geometry shader already known
		geometryShader = itBaseShader->second;
		cemu_assert_debug(LatteSHRC_CalcGSAuxHash(geometryShader) == 0);
	}
	else
	{
		// decompile geometry shader
		geometryShader = LatteShader_CompileSeparableGeometryShader(_shaderBaseHash_gs, geometryShaderPtr, geometryShaderSize, geometryCopyShader, geometryCopyShaderSize);
	}
	if (geometryShader->hasError)
	{
		LatteGPUState.activeShaderHasError = true;
		return;
	}
	g_renderer->shader_bind(geometryShader->shader);
	_activeGeometryShader = geometryShader;
}

void LatteSHRC_UpdatePixelShader(uint8* pixelShaderPtr, uint32 pixelShaderSize, bool usesGeometryShader)
{
	if (LatteGPUState.contextRegister[mmVGT_STRMOUT_EN] != 0 && g_renderer->GetType() == RendererAPI::OpenGL)
	{
		if (_activePixelShader)
		{
			g_renderer->shader_unbind(RendererShader::ShaderType::kFragment);
			_activePixelShader = nullptr;
		}
		return;
	}
	LatteSHRC_UpdatePSBaseHash(pixelShaderPtr, pixelShaderSize, usesGeometryShader);
	uint64 psAuxHash = 0;
	auto itBaseShader = sPixelShaders.find(_shaderBaseHash_ps);
	LatteDecompilerShader* pixelShader = nullptr;
	if (itBaseShader != sPixelShaders.end())
	{
		psAuxHash = LatteSHRC_CalcPSAuxHash(itBaseShader->second, LatteGPUState.contextRegister);
		pixelShader = LatteSHRC_GetFromChain(itBaseShader->second, _shaderBaseHash_ps, psAuxHash);
	}
	if (!pixelShader)
		pixelShader = LatteShader_CompileSeparablePixelShader(_shaderBaseHash_ps, psAuxHash, pixelShaderPtr, pixelShaderSize, usesGeometryShader);
	if (pixelShader->hasError)
	{
		LatteGPUState.activeShaderHasError = true;
		return;
	}
	g_renderer->shader_bind(pixelShader->shader);
	_activePixelShader = pixelShader;
}

void LatteSHRC_UpdateActiveShaders()
{
	// check if geometry shader is used
	auto gsMode = LatteGPUState.contextNew.VGT_GS_MODE.get_MODE();

	cemu_assert_debug(LatteGPUState.contextNew.VGT_GS_MODE.get_ES_PASSTHRU() == false);
	// todo: Support for ES passthrough and cut mode in mmVGT_GS_MODE

	bool geometryShaderUsed = false;
	if (gsMode == Latte::LATTE_VGT_GS_MODE::E_MODE::OFF)
	{
		geometryShaderUsed = false;
	}
	else if (gsMode == Latte::LATTE_VGT_GS_MODE::E_MODE::SCENARIO_G)
	{
		// could also be compute shader?
		geometryShaderUsed = true;
	}
	else
	{
		cemu_assert_debug(false);
	}
	// get shader programs
	uint8* psProgramCode = (uint8*)memory_getPointerFromPhysicalOffset((LatteGPUState.contextRegister[mmSQ_PGM_START_PS] & 0xFFFFFF) << 8);
	uint32 psProgramSize = LatteGPUState.contextRegister[mmSQ_PGM_START_PS + 1] << 3;
	uint8* gsProgramCode = (uint8*)memory_getPointerFromPhysicalOffset((LatteGPUState.contextRegister[mmSQ_PGM_START_GS] & 0xFFFFFF) << 8);
	uint32 gsProgramSize = LatteGPUState.contextRegister[mmSQ_PGM_START_GS + 1] << 3;

	uint8* vsProgramCode;
	uint32 vsProgramSize;
	uint8* copyProgramCode = NULL;
	uint32 copyProgramSize = 0;
	if (geometryShaderUsed)
	{
		vsProgramCode = (uint8*)memory_getPointerFromPhysicalOffset((LatteGPUState.contextRegister[mmSQ_PGM_START_ES] & 0xFFFFFF) << 8);
		vsProgramSize = LatteGPUState.contextRegister[mmSQ_PGM_START_ES + 1] << 3;
		copyProgramCode = (uint8*)memory_getPointerFromPhysicalOffset((LatteGPUState.contextRegister[mmSQ_PGM_START_VS] & 0xFFFFFF) << 8);
		if (LatteGPUState.contextRegister[mmSQ_PGM_START_VS] == 0)
		{
			copyProgramCode = NULL;
			debug_printf("copyProgram is NULL but used. Might be because of unsupported vertex/geometry mode?");
		}
		copyProgramSize = LatteGPUState.contextRegister[mmSQ_PGM_START_VS + 1] << 3;
	}
	else
	{
		if (LatteGPUState.contextRegister[mmSQ_PGM_START_VS] == 0)
		{
			debug_printf("No vertex shader program set\n");
			LatteGPUState.activeShaderHasError = true;
			return;
		}
		vsProgramCode = (uint8*)memory_getPointerFromPhysicalOffset((LatteGPUState.contextRegister[mmSQ_PGM_START_VS] & 0xFFFFFF) << 8);
		vsProgramSize = LatteGPUState.contextRegister[mmSQ_PGM_START_VS + 1] << 3;
	}
	// set new shaders
	LatteGPUState.activeShaderHasError = false;
	LatteShader_UpdatePSInputs(LatteGPUState.contextRegister);
	LatteShaderSHRC_UpdateFetchShader();
	LatteSHRC_UpdateVertexShader(vsProgramCode, vsProgramSize, geometryShaderUsed);
	if (LatteGPUState.activeShaderHasError)
		return;
	LatteSHRC_UpdateGeometryShader(geometryShaderUsed, gsProgramCode, gsProgramSize, copyProgramCode, copyProgramSize);
	if (LatteGPUState.activeShaderHasError)
		return;
	LatteSHRC_UpdatePixelShader(psProgramCode, psProgramSize, geometryShaderUsed);
	if (LatteGPUState.activeShaderHasError)
		return;
}

// returns the sampler base index for the given shader type
sint32 LatteDecompiler_getTextureSamplerBaseIndex(LatteConst::ShaderType shaderType)
{
	uint32 samplerId = LATTE_DECOMPILER_SAMPLER_NONE;
	if (shaderType == LatteConst::ShaderType::Vertex)
		return Latte::SAMPLER_BASE_INDEX_VERTEX;
	else if (shaderType == LatteConst::ShaderType::Pixel)
		return Latte::SAMPLER_BASE_INDEX_PIXEL;
	else if (shaderType == LatteConst::ShaderType::Geometry)
		return Latte::SAMPLER_BASE_INDEX_GEOMETRY;
	else
		cemu_assert_suspicious();
	return 0;
}

void LatteSHRC_Init()
{
	cemu_assert_debug(sVertexShaders.empty());
	cemu_assert_debug(sGeometryShaders.empty());
	cemu_assert_debug(sPixelShaders.empty());
}

void LatteSHRC_UnloadAll()
{
    while(!sVertexShaders.empty())
        LatteShader_free(sVertexShaders.begin()->second);
    cemu_assert_debug(sVertexShaders.empty());
    while(!sGeometryShaders.empty())
        LatteShader_free(sGeometryShaders.begin()->second);
    cemu_assert_debug(sGeometryShaders.empty());
    while(!sPixelShaders.empty())
        LatteShader_free(sPixelShaders.begin()->second);
    cemu_assert_debug(sPixelShaders.empty());
}