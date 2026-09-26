#pragma once
#include "Cafe/HW/Latte/Core/LatteConst.h"

struct LatteParsedFetchShaderAttribute
{
	uint8								attributeBufferIndex;
	uint8								semanticId;
	Latte::E_HWFMT						format;
	LatteConst::VertexFetchType2		fetchType;
	uint8								nfa;
	uint8								isSigned;
	LatteConst::VertexFetchEndianMode	endianSwap;
	uint8								ds[4]; // destination component select
	sint32								aluDivisor;
	uint32								offset;
};

struct LatteParsedFetchShaderBufferGroup
{
	uint8 attributeBufferIndex{}; // index of buffer (0 to 15 are valid)
	sint8 attribCount{};
	bool hasVtxIndexAccess : 1;
	bool hasInstanceIndexAccess : 1;
	uint32 minOffset{};
	uint32 totalAttribRangeSize{}; // max attribOffset+attribSize
	LatteParsedFetchShaderAttribute* attrib{}; // attributes for this buffer

	uint32 getCurrentBufferStride(uint32* contextRegister) const;
};

struct LatteFetchShader
{
	using CacheHash = uint64;

	~LatteFetchShader();

	std::vector<LatteParsedFetchShaderBufferGroup> bufferGroups;
	std::vector<LatteParsedFetchShaderBufferGroup> bufferGroupsInvalid; // groups with buffer index not being a valid buffer (dst components of these can affect shader code, but no actual vertex imports are done)

	uint64 key{};
	uint32 attributeBufferMask{}; // mask of buffers sourced by this fetch shader

	// Vulkan
	uint64 vkPipelineHashFragment{}; // hash of all fetch shader state that influences the Vulkan graphics pipeline

	// Metal
	bool mtlFetchVertexManually{};

	// cache info
	CacheHash m_cacheHash{};
	bool m_isRegistered{}; // if true, fetch shader is referenced by cache (RegisterInCache() succeeded)

	void CalculateFetchShaderVkHash();

#ifdef ENABLE_METAL
	void CheckIfVerticesNeedManualFetchMtl(uint32* contextRegister);
#endif

	uint64 getVkPipelineHashFragment() const { return vkPipelineHashFragment; };

	static bool isValidBufferIndex(const uint32 index) { return index < 0x10; };

	// keys in shader state cache
	std::vector<uint64> m_shaderStateCacheKeys;

	// fetch shader cache (move these to separate Cache class?)
	LatteFetchShader* RegisterInCache(CacheHash fsHash); // fails if another fetch shader object is already registered with the same fsHash. Returns the previously registered fetch shader or null
	void UnregisterInCache();
	static CacheHash CalculateCacheHash(void* programCode, uint32 programSize);
	static LatteFetchShader* FindInCacheByHash(CacheHash fsHash);
	static LatteFetchShader* FindByGPUState();

	static std::unordered_map<CacheHash, LatteFetchShader*> s_fetchShaderByHash;
};

LatteFetchShader* LatteShaderRecompiler_createFetchShader(LatteFetchShader::CacheHash fsHash, uint32* contextRegister, uint32* fsProgramCode, uint32 fsProgramSize);
