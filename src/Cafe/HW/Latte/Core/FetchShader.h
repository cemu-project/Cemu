#pragma once
#include "Cafe/HW/Latte/Core/LatteConst.h"

struct LatteParsedFetchShaderAttribute_t
{
	uint8					semanticId;
	uint8					format;
	LatteConst::VertexFetchType2	fetchType;
	uint8					nfa;
	uint8					isSigned;
	LatteConst::VertexFetchEndianMode	endianSwap;
	uint8					ds[4]; // destination component select
	sint32					aluDivisor;
	uint32					offset;
	uint32					attributeBufferIndex;
};

struct LatteParsedFetchShaderBufferGroup_t
{
	uint32 attributeBufferIndex{}; // index of buffer (0 to 15 are valid)
	LatteParsedFetchShaderAttribute_t* attrib{}; // attributes for this buffer
	sint32 attribCount{};
	// offset range of attributes
	uint32 minOffset{};
	uint32 maxOffset{};
	// output
	uint32  vboStride{};
	// calculated info
	bool hasVtxIndexAccess{};
	bool hasInstanceIndexAccess{};

	uint32 getCurrentBufferStride(uint32* contextRegister) const;
};

struct LatteFetchShader
{
	using CacheHash = uint64;

	~LatteFetchShader();

	std::vector<LatteParsedFetchShaderBufferGroup_t> bufferGroups;
	std::vector<LatteParsedFetchShaderBufferGroup_t> bufferGroupsInvalid; // groups with buffer index not being a valid buffer (dst components of these can affect shader code, but no actual vertex imports are done)

	uint64 key{};

	// Vulkan
	uint64 vkPipelineHashFragment{}; // hash of all fetch shader state that influences the Vulkan graphics pipeline

	// cache info
	CacheHash m_cacheHash{};
	bool m_isRegistered{}; // if true, fetch shader is referenced by cache (RegisterInCache() succeeded)


	void CalculateFetchShaderVkHash();

	uint64 getVkPipelineHashFragment() const { return vkPipelineHashFragment; };

	static bool isValidBufferIndex(const uint32 index) { return index < 0x10; };

	// cache
	LatteFetchShader* RegisterInCache(CacheHash fsHash); // Fails if another fetch shader object is already registered with the same fsHash. Returns the previously registered fetch shader or null
	void UnregisterInCache();

	// fetch shader cache (move these to separate Cache class?)
	static CacheHash CalculateCacheHash(void* programCode, uint32 programSize);
	static LatteFetchShader* FindInCacheByHash(CacheHash fsHash);
	static LatteFetchShader* FindByGPUState();

	static std::unordered_map<CacheHash, LatteFetchShader*> s_fetchShaderByHash;
};

LatteFetchShader* LatteShaderRecompiler_createFetchShader(LatteFetchShader::CacheHash fsHash, uint32* contextRegister, uint32* fsProgramCode, uint32 fsProgramSize);