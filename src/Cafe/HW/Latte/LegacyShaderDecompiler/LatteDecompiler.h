#pragma once
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Renderer/RendererShader.h"
#include <boost/container/static_vector.hpp>

namespace LatteDecompiler
{
	enum class DataType
	{
		UNDEFINED = 0,
		U32 = 0,
		S32 = 0,
		FLOAT = 0
	};
};

// decompiler shader types

typedef struct
{
	bool	isRegister; // if true -> Uniform register, if false -> Uniform buffer
	uint8	kcacheBankId; // uniform buffer id (if uniform buffer)
	uint32	index; // uniform address (in 4-DWORD tuples)
	uint32	mappedIndex; // index in remapped uniform array
}LatteDecompilerRemappedUniformEntry_t;

typedef struct
{
	uint32	indexOffset; // uniform address (in 4-DWORD tuples)
	uint32	mappedIndexOffset; // index in remapped uniform array
}LatteFastAccessRemappedUniformEntry_register_t;

typedef struct
{
	uint16	indexOffset; // uniform address (in 4-DWORD tuples)
	uint16	mappedIndexOffset; // index in remapped uniform array
}LatteFastAccessRemappedUniformEntry_buffer_t;

typedef struct  
{
	uint32 texUnit;
	sint32 uniformLocation;
	float currentValue[2];
}LatteUniformTextureScaleEntry_t;

struct LatteDecompilerShaderResourceMapping
{
	LatteDecompilerShaderResourceMapping()
	{
		std::fill(textureUnitToBindingPoint, textureUnitToBindingPoint + LATTE_NUM_MAX_TEX_UNITS, -1);
		std::fill(uniformBuffersBindingPoint, uniformBuffersBindingPoint + LATTE_NUM_MAX_UNIFORM_BUFFERS, -1);
		std::fill(attributeMapping, attributeMapping + LATTE_NUM_MAX_ATTRIBUTE_LOCATIONS, -1);
	}
	static const sint8 UNUSED_BINDING = -1;
	// most of this is for Vulkan
	sint8 setIndex{};
	// texture
	sint8 textureUnitToBindingPoint[LATTE_NUM_MAX_TEX_UNITS];
	// uniform buffer
	sint8 uniformVarsBufferBindingPoint{}; // special block for uniform registers/remapped array/custom variables
	sint8 uniformBuffersBindingPoint[LATTE_NUM_MAX_UNIFORM_BUFFERS];
	// shader storage buffer for transform feedback (if alternative mode is used)
	sint8 tfStorageBindingPoint{-1};
	// attributes (vertex shader only)
	sint8 attributeMapping[LATTE_NUM_MAX_ATTRIBUTE_LOCATIONS];

	sint32 getTextureCount()
	{
		sint32 count = 0;
		for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
		{
			if (textureUnitToBindingPoint[i] >= 0)
				count++;
		}
		return count;
	}

	sint32 getTextureUnitFromBindingPoint(sint8 bindingPoint)
	{
		for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
		{
			if (textureUnitToBindingPoint[i] == bindingPoint)
				return i;
		}

		cemu_assert_debug(false);
		return -1;
	}

	// returns -1 if no there is no texture binding point
	sint32 getTextureBaseBindingPoint()
	{
		sint32 bindingPoint = 9999;
		for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
		{
			if (textureUnitToBindingPoint[i] >= 0)
				bindingPoint = std::min(bindingPoint, (sint32)textureUnitToBindingPoint[i]);
		}
		if (bindingPoint == 9999)
			return -1;
		return bindingPoint;
	}

	bool getUniformBufferBindingRange(sint32& minBinding, sint32& maxBinding)
	{
		sint32 minBindingPoint = 9999;
		sint32 maxBindingPoint = -9999;
		if (uniformVarsBufferBindingPoint >= 0)
		{
			minBindingPoint = std::min(minBindingPoint, (sint32)uniformVarsBufferBindingPoint);
			maxBindingPoint = std::max(maxBindingPoint, (sint32)uniformVarsBufferBindingPoint);
		}
		for (sint32 i = 0; i < LATTE_NUM_MAX_UNIFORM_BUFFERS; i++)
		{
			if (uniformBuffersBindingPoint[i] >= 0)
			{
				minBindingPoint = std::min(minBindingPoint, (sint32)uniformBuffersBindingPoint[i]);
				maxBindingPoint = std::max(maxBindingPoint, (sint32)uniformBuffersBindingPoint[i]);
			}
		}
		if (minBindingPoint == 9999)
			return false;
		minBinding = minBindingPoint;
		maxBinding = maxBindingPoint;
		return true;
	}

	sint32 getTFStorageBufferBindingPoint()
	{
		return tfStorageBindingPoint;
	}

	bool hasUniformBuffers()
	{
		for (sint32 i = 0; i < LATTE_NUM_MAX_UNIFORM_BUFFERS; i++)
		{
			if (uniformBuffersBindingPoint[i] >= 0)
				return true;
		}
		return false;
	}

	sint32 getAttribHostShaderIndex(uint32 semanticId)
	{
		cemu_assert_debug(semanticId < 0x100);
		return attributeMapping[semanticId];
	}
};

struct LatteDecompilerShader
{
	LatteDecompilerShader(LatteConst::ShaderType shaderType) : shaderType(shaderType) {}

	LatteDecompilerShader* next{nullptr};
	LatteConst::ShaderType shaderType;
	uint64 baseHash{0};
	uint64 auxHash{0};
	// vertex shader
	struct LatteFetchShader* compatibleFetchShader{};
	// error tracking
	bool hasError{false}; // if set, the shader cannot be used
	// compact resource lists for optimized access
	struct QuickBufferEntry
	{
		uint32 index : 8;
		uint32 size : 24;
	};
	boost::container::static_vector<QuickBufferEntry, LATTE_NUM_MAX_UNIFORM_BUFFERS> list_quickBufferList;
	uint8 textureUnitList[LATTE_NUM_MAX_TEX_UNITS];
	uint8 textureUnitListCount{ 0 };
	// input
	Latte::E_DIM textureUnitDim[LATTE_NUM_MAX_TEX_UNITS]{}; // dimension of texture unit, from the currently set texture
	bool textureIsIntegerFormat[LATTE_NUM_MAX_TEX_UNITS]{};
	// analyzer stage (uniforms)
	uint8 uniformMode{0}; // determines how uniforms are managed within the shader (see LATTE_DECOMPILER_UNIFORM_MODE_* constants)
	uint64 uniformDataHash64[2]{0}; // used to avoid redundant calls to glUniform*
	std::vector<LatteDecompilerRemappedUniformEntry_t> list_remappedUniformEntries;
	// analyzer stage (textures)
	std::bitset<LATTE_NUM_MAX_TEX_UNITS> textureUnitMask2;
	uint16 textureUnitSamplerAssignment[LATTE_NUM_MAX_TEX_UNITS]{ 0 }; // LATTE_DECOMPILER_SAMPLER_NONE means undefined
	bool textureUsesDepthCompare[LATTE_NUM_MAX_TEX_UNITS]{};

	// analyzer stage (pixel outputs)
	uint32 pixelColorOutputMask{ 0 }; // from LSB to MSB, 1 bit per written output. 1 if written (indices of color attachments)
	// analyzer stage (geometry shader parameters/inputs)
	uint32 ringParameterCount{ 0 };
	uint32 ringParameterCountFromPrevStage{ 0 }; // used in geometry shader to hold VS ringParameterCount
	// analyzer stage (misc)
	std::bitset<LATTE_NUM_STREAMOUT_BUFFER> streamoutBufferWriteMask;
	bool hasStreamoutBufferWrite{ false };
	// output code
	class StringBuf* strBuf_shaderSource{ nullptr };
	// separable shaders
	RendererShader* shader{ nullptr };
	bool isCustomShader{ false };

	uint32 outputParameterMask{ 0 };
	// resource mapping (binding points)
	LatteDecompilerShaderResourceMapping resourceMapping{};
	// uniforms
	struct  
	{
		sint32 loc_remapped; // uf_remappedVS/uf_remappedGS/uf_remappedPS
		sint32 loc_uniformRegister; // uf_uniformRegisterVS/uf_uniformRegisterGS/uf_uniformRegisterPS
		sint32 count_uniformRegister;
		sint32 loc_windowSpaceToClipSpaceTransform; // uf_windowSpaceToClipSpaceTransform
		sint32 loc_alphaTestRef; // uf_alphaTestRef
		sint32 loc_pointSize; // uf_pointSize
		sint32 loc_fragCoordScale;
		std::vector<LatteUniformTextureScaleEntry_t> list_ufTexRescale; // list of mappings for uf_tex*Scale <-> uniform location
		float ufCurrentValueAlphaTestRef;
		float ufCurrentValueFragCoordScale[2];
		sint32 loc_verticesPerInstance;
		sint32 loc_streamoutBufferBase[LATTE_NUM_STREAMOUT_BUFFER];
		sint32 uniformRangeSize; // entire size of uniform variable block
	}uniform{ 0 };
	// fast access
	struct _RemappedUniformBufferGroup 
	{
		_RemappedUniformBufferGroup(uint32 _kcacheBankIdOffset) : kcacheBankIdOffset(_kcacheBankIdOffset) {};

		uint32 kcacheBankIdOffset;
		std::vector<LatteFastAccessRemappedUniformEntry_buffer_t> entries;
	};
	std::vector<LatteFastAccessRemappedUniformEntry_register_t>	list_remappedUniformEntries_register;
	std::vector<_RemappedUniformBufferGroup> list_remappedUniformEntries_bufferGroups;
};

struct LatteDecompilerOutputUniformOffsets
{
	sint32 offset_remapped;
	sint32 offset_uniformRegister;
	sint32 count_uniformRegister; // in vec4
	sint32 offset_alphaTestRef;
	sint32 offset_pointSize;
	sint32 offset_fragCoordScale;
	sint32 offset_windowSpaceToClipSpaceTransform;
	sint32 offset_texScale[LATTE_NUM_MAX_TEX_UNITS];
	sint32 offset_verticesPerInstance{-1};
	sint32 offset_streamoutBufferBase[LATTE_NUM_STREAMOUT_BUFFER]{ -1, -1, -1, -1 };
	sint32 offset_endOfBlock; // stores size of uniform variable block

	LatteDecompilerOutputUniformOffsets()
	{
		offset_remapped = -1;
		offset_uniformRegister = -1;
		count_uniformRegister = 0;
		offset_alphaTestRef = -1;
		offset_pointSize = -1;
		offset_fragCoordScale = -1;
		offset_windowSpaceToClipSpaceTransform = -1;
		for (sint32 i = 0; i < LATTE_NUM_MAX_TEX_UNITS; i++)
			offset_texScale[i] = -1;
		offset_endOfBlock = 0;
	}
};

struct LatteDecompilerOptions 
{
	bool usesGeometryShader{ false };
	// floating point math
	bool strictMul{}; // if true, 0*anything=0 rule is emulated
	// Vulkan-specific
	bool useTFViaSSBO{ false };
	struct  
	{
		bool hasRoundingModeRTEFloat32{ false };
	}spirvInstrinsics;
};

struct LatteDecompilerOutput_t
{
	LatteDecompilerShader* shader;
	LatteConst::ShaderType shaderType;

	// texture info
	std::bitset<LATTE_NUM_MAX_TEX_UNITS> textureUnitMask;

	// streamout info
	std::bitset<LATTE_NUM_STREAMOUT_BUFFER> streamoutBufferWriteMask;
	uint32 streamoutBufferStride[LATTE_NUM_STREAMOUT_BUFFER]{};

	// uniform locations
	LatteDecompilerOutputUniformOffsets uniformOffsetsGL;
	LatteDecompilerOutputUniformOffsets uniformOffsetsVK;
	// mapping and binding information
	LatteDecompilerShaderResourceMapping resourceMappingGL;
	LatteDecompilerShaderResourceMapping resourceMappingVK;
};

struct LatteDecompilerSubroutineInfo;

void LatteDecompiler_DecompileVertexShader(uint64 shaderBaseHash, uint32* contextRegisters, uint8* programData, uint32 programSize, struct LatteFetchShader* fetchShader, LatteDecompilerOptions& options, LatteDecompilerOutput_t* output);
void LatteDecompiler_DecompileGeometryShader(uint64 shaderBaseHash, uint32* contextRegisters, uint8* programData, uint32 programSize, uint8* gsCopyProgramData, uint32 gsCopyProgramSize, uint32 vsRingParameterCount, LatteDecompilerOptions& options, LatteDecompilerOutput_t* output);
void LatteDecompiler_DecompilePixelShader(uint64 shaderBaseHash, uint32* contextRegisters, uint8* programData, uint32 programSize, LatteDecompilerOptions& options, LatteDecompilerOutput_t* output);

// specialized shader parsers

#define GPU7_COPY_SHADER_MAX_PARAMS	(32)

struct LatteGSCopyShaderStreamWrite_t
{
	uint8 bufferIndex;
	uint16 offset; // offset in ring buffer from GS
	uint32 exportArrayBase;
	uint32 memWriteArraySize;
	uint32 memWriteCompMask;
};

struct LatteParsedGSCopyShader
{
	struct
	{
		uint16 offset;
		uint16 gprIndex;
		uint8  exportType;
		uint8  exportParam;
	}paramMapping[GPU7_COPY_SHADER_MAX_PARAMS];
	sint32 numParam;
	// streamout writes
	std::vector<LatteGSCopyShaderStreamWrite_t> list_streamWrites;
};

LatteParsedGSCopyShader* LatteGSCopyShaderParser_parse(uint8* programData, uint32 programSize);
bool LatteGSCopyShaderParser_getExportTypeByOffset(LatteParsedGSCopyShader* shaderContext, uint32 offset, uint32* exportType, uint32* exportParam);