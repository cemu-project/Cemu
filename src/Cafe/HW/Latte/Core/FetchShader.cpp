#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Core/LatteShaderAssembly.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/OS/libs/gx2/GX2.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerInstructions.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/ISA/LatteInstructions.h"
#include "util/containers/LookupTableL3.h"
#include "util/helpers/fspinlock.h"
#include <openssl/sha.h> /* SHA1_DIGEST_LENGTH */
#include <openssl/evp.h> /* EVP_Digest */

uint32 LatteShaderRecompiler_getAttributeSize(LatteParsedFetchShaderAttribute_t* attrib)
{
	if (attrib->format == FMT_32_32_32_32 || attrib->format == FMT_32_32_32_32_FLOAT)
		return 4 * 4;
	else if (attrib->format == FMT_32_32_32 || attrib->format == FMT_32_32_32_FLOAT)
		return 3 * 4;
	else if (attrib->format == FMT_32_32 || attrib->format == FMT_32_32_FLOAT)
		return 2 * 4;
	else if (attrib->format == FMT_32 || attrib->format == FMT_32_FLOAT)
		return 1 * 4;
	else if (attrib->format == FMT_16_16_16_16 || attrib->format == FMT_16_16_16_16_FLOAT)
		return 4 * 2;
	else if (attrib->format == FMT_16_16 || attrib->format == FMT_16_16_FLOAT)
		return 2 * 2;
	else if (attrib->format == FMT_16 || attrib->format == FMT_16_FLOAT)
		return 1 * 2;
	else if (attrib->format == FMT_8_8_8_8)
		return 4 * 1;
	else if (attrib->format == FMT_8_8)
		return 2 * 1;
	else if (attrib->format == FMT_8)
		return 1 * 1;
	else if (attrib->format == FMT_2_10_10_10)
		return 4;
	else
		cemu_assert_unimplemented();
	return 0;
}

uint32 LatteShaderRecompiler_getAttributeAlignment(LatteParsedFetchShaderAttribute_t* attrib)
{
	if (attrib->format == FMT_32_32_32_32 || attrib->format == FMT_32_32_32_32_FLOAT)
		return 4;
	else if (attrib->format == FMT_32_32_32 || attrib->format == FMT_32_32_32_FLOAT)
		return 4;
	else if (attrib->format == FMT_32_32 || attrib->format == FMT_32_32_FLOAT)
		return 4;
	else if (attrib->format == FMT_32 || attrib->format == FMT_32_FLOAT)
		return 4;
	else if (attrib->format == FMT_16_16_16_16 || attrib->format == FMT_16_16_16_16_FLOAT)
		return 2;
	else if (attrib->format == FMT_16_16 || attrib->format == FMT_16_16_FLOAT)
		return 2;
	else if (attrib->format == FMT_16 || attrib->format == FMT_16_FLOAT)
		return 2;
	else if (attrib->format == FMT_8_8_8_8)
		return 1;
	else if (attrib->format == FMT_8_8)
		return 1;
	else if (attrib->format == FMT_8)
		return 1;
	else if (attrib->format == FMT_2_10_10_10)
		return 4;
	else
		cemu_assert_unimplemented();
	return 4;
}

void LatteShader_calculateFSKey(LatteFetchShader* fetchShader)
{
	uint64 key = 0;
	for (sint32 g = 0; g < fetchShader->bufferGroups.size(); g++)
	{
		LatteParsedFetchShaderBufferGroup_t& group = fetchShader->bufferGroups[g];
		for (sint32 f = 0; f < group.attribCount; f++)
		{
			LatteParsedFetchShaderAttribute_t* attrib = group.attrib + f;
			key += (uint64)attrib->endianSwap;
			key = std::rotl<uint64>(key, 3);
			key += (uint64)attrib->nfa;
			key = std::rotl<uint64>(key, 3);
			key += (uint64)(attrib->isSigned?1:0);
			key = std::rotl<uint64>(key, 1);
			key += (uint64)attrib->format;
			key = std::rotl<uint64>(key, 7);
			key += (uint64)attrib->fetchType;
			key = std::rotl<uint64>(key, 8);
			key += (uint64)attrib->ds[0];
			key = std::rotl<uint64>(key, 2);
			key += (uint64)attrib->ds[1];
			key = std::rotl<uint64>(key, 2);
			key += (uint64)attrib->ds[2];
			key = std::rotl<uint64>(key, 2);
			key += (uint64)attrib->ds[3];
			key = std::rotl<uint64>(key, 2);
			key += (uint64)(attrib->aluDivisor+1);
			key = std::rotl<uint64>(key, 2);
			key += (uint64)attrib->attributeBufferIndex;
			key = std::rotl<uint64>(key, 8);
			key += (uint64)attrib->semanticId;
			key = std::rotl<uint64>(key, 8);
			key += (uint64)(attrib->offset & 3);
			key = std::rotl<uint64>(key, 2);
		}
	}
	// todo - also hash invalid buffer groups?
	fetchShader->key = key;
}

uint32 LatteParsedFetchShaderBufferGroup_t::getCurrentBufferStride(uint32* contextRegister) const
{
	uint32 bufferIndex = this->attributeBufferIndex;
	uint32 bufferBaseRegisterIndex = mmSQ_VTX_ATTRIBUTE_BLOCK_START + bufferIndex * 7;
	uint32 bufferStride = (contextRegister[bufferBaseRegisterIndex + 2] >> 11) & 0xFFFF;
	return bufferStride;
}

void LatteFetchShader::CalculateFetchShaderVkHash()
{
	// calculate SHA1 of all states that are part of the Vulkan graphics pipeline
	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	EVP_DigestInit(ctx, EVP_sha1());
	for(auto& group : bufferGroups)
	{
		// offsets
		for (sint32 t = 0; t < group.attribCount; t++)
		{
			uint32 offset = group.attrib[t].offset;
			EVP_DigestUpdate(ctx, &t, sizeof(t));
			EVP_DigestUpdate(ctx, &offset, sizeof(offset));
		}
	}
	uint8 shaDigest[SHA_DIGEST_LENGTH];
	EVP_DigestFinal_ex(ctx, shaDigest, NULL);
	EVP_MD_CTX_free(ctx);

	// fold SHA1 hash into a 64bit value
	uint64 h = *(uint64*)(shaDigest + 0);
	h += *(uint64*)(shaDigest + 8);
	h += (uint64)*(uint32*)(shaDigest + 16);
	this->vkPipelineHashFragment = h;
}

void _fetchShaderDecompiler_parseInstruction_VTX_SEMANTIC(LatteFetchShader* parsedFetchShader, uint32* contextRegister, const LatteClauseInstruction_VTX* instr)
{
	uint32 semanticId = instr->getFieldSEM_SEMANTIC_ID(); // location (attribute index inside shader)
	uint32 bufferId = instr->getField_BUFFER_ID(); // the index used for GX2SetAttribBuffer (+0xA0)
	LatteConst::VertexFetchType2 fetchType = instr->getField_FETCH_TYPE();
	auto srcSelX = instr->getField_SRC_SEL_X();
	auto dsx = instr->getField_DST_SEL(0);
	auto dsy = instr->getField_DST_SEL(1);
	auto dsz = instr->getField_DST_SEL(2);
	auto dsw = instr->getField_DST_SEL(3);
	auto dataFormat = instr->getField_DATA_FORMAT();
	uint32 offset = instr->getField_OFFSET();
	auto nfa = instr->getField_NUM_FORMAT_ALL();
	bool isSigned = instr->getField_FORMAT_COMP_ALL() == LatteClauseInstruction_VTX::FORMAT_COMP::COMP_SIGNED;
	auto endianSwap = instr->getField_ENDIAN_SWAP();
	
	// get buffer
	cemu_assert_debug(bufferId >= 0xA0 && bufferId < 0xB0);
	uint32 bufferIndex = (bufferId - 0xA0);

	// get or add new attribute group (by buffer index)
	LatteParsedFetchShaderBufferGroup_t* attribGroup = nullptr;
	if (LatteFetchShader::isValidBufferIndex(bufferIndex))
	{
		auto bufferGroupItr = std::find_if(parsedFetchShader->bufferGroups.begin(), parsedFetchShader->bufferGroups.end(), [bufferIndex](LatteParsedFetchShaderBufferGroup_t& bufferGroup) {return bufferGroup.attributeBufferIndex == bufferIndex; });
		if (bufferGroupItr != parsedFetchShader->bufferGroups.end())
			attribGroup = &(*bufferGroupItr);
	}
	else
	{
		auto bufferGroupItr = std::find_if(parsedFetchShader->bufferGroupsInvalid.begin(), parsedFetchShader->bufferGroupsInvalid.end(), [bufferIndex](LatteParsedFetchShaderBufferGroup_t& bufferGroup) {return bufferGroup.attributeBufferIndex == bufferIndex; });
		if (bufferGroupItr != parsedFetchShader->bufferGroupsInvalid.end())
			attribGroup = &(*bufferGroupItr);
	}
	// create new group if none found
	if (attribGroup == nullptr)
	{
		if (LatteFetchShader::isValidBufferIndex(bufferIndex))
			attribGroup = &parsedFetchShader->bufferGroups.emplace_back();
		else
			attribGroup = &parsedFetchShader->bufferGroupsInvalid.emplace_back();

		attribGroup->attributeBufferIndex = bufferIndex;
		attribGroup->minOffset = offset;
		attribGroup->maxOffset = offset;
	}
	// add attribute
	sint32 groupAttribIndex = attribGroup->attribCount;
	if (attribGroup->attribCount < (groupAttribIndex + 1))
	{
		attribGroup->attribCount = (groupAttribIndex + 1);
		attribGroup->attrib = (LatteParsedFetchShaderAttribute_t*)realloc(attribGroup->attrib, sizeof(LatteParsedFetchShaderAttribute_t) * attribGroup->attribCount);
	}
	attribGroup->attrib[groupAttribIndex].semanticId = semanticId;
	attribGroup->attrib[groupAttribIndex].format = (uint8)dataFormat;
	attribGroup->attrib[groupAttribIndex].fetchType = fetchType;
	attribGroup->attrib[groupAttribIndex].nfa = (uint8)nfa;
	attribGroup->attrib[groupAttribIndex].isSigned = isSigned;
	attribGroup->attrib[groupAttribIndex].offset = offset;
	attribGroup->attrib[groupAttribIndex].ds[0] = (uint8)dsx;
	attribGroup->attrib[groupAttribIndex].ds[1] = (uint8)dsy;
	attribGroup->attrib[groupAttribIndex].ds[2] = (uint8)dsz;
	attribGroup->attrib[groupAttribIndex].ds[3] = (uint8)dsw;
	attribGroup->attrib[groupAttribIndex].attributeBufferIndex = bufferIndex;
	attribGroup->attrib[groupAttribIndex].endianSwap = endianSwap;
	attribGroup->minOffset = (std::min)(attribGroup->minOffset, offset);
	attribGroup->maxOffset = (std::max)(attribGroup->maxOffset, offset);
	// get alu divisor
	if (srcSelX == LatteClauseInstruction_VTX::SRC_SEL::SEL_X)
	{
		cemu_assert_debug(fetchType != LatteConst::VertexFetchType2::INSTANCE_DATA); // aluDivisor 0 in combination with instanced data is not allowed?
		attribGroup->attrib[groupAttribIndex].aluDivisor = -1;
	}
	else if (srcSelX == LatteClauseInstruction_VTX::SRC_SEL::SEL_W)
	{
		cemu_assert_debug(fetchType == LatteConst::VertexFetchType2::INSTANCE_DATA); // using constant divisor 1 with per-vertex data seems strange? (divisor is instance-only)
		// aluDivisor is constant 1
		attribGroup->attrib[groupAttribIndex].aluDivisor = 1;
	}
	else if (srcSelX == LatteClauseInstruction_VTX::SRC_SEL::SEL_Y)
	{
		// use alu divisor 1
		attribGroup->attrib[groupAttribIndex].aluDivisor = (sint32)contextRegister[Latte::REGADDR::VGT_INSTANCE_STEP_RATE_0];
		cemu_assert_debug(attribGroup->attrib[groupAttribIndex].aluDivisor > 0);
	}
	else if (srcSelX == LatteClauseInstruction_VTX::SRC_SEL::SEL_Z)
	{
		// use alu divisor 2
		attribGroup->attrib[groupAttribIndex].aluDivisor = (sint32)contextRegister[Latte::REGADDR::VGT_INSTANCE_STEP_RATE_1];
		cemu_assert_debug(attribGroup->attrib[groupAttribIndex].aluDivisor > 0);
	}
}

void _fetchShaderDecompiler_parseVTXClause(LatteFetchShader* parsedFetchShader, uint32* contextRegister, std::span<uint8> clauseCode, size_t numInstructions)
{
	const LatteClauseInstruction_VTX* instr = (LatteClauseInstruction_VTX*)clauseCode.data();
	const LatteClauseInstruction_VTX* end = instr + numInstructions;
	while (instr < end)
	{
		if (instr->getField_VTX_INST() == LatteClauseInstruction_VTX::VTX_INST::_VTX_INST_SEMANTIC)
		{
			_fetchShaderDecompiler_parseInstruction_VTX_SEMANTIC(parsedFetchShader, contextRegister, instr);
		}
		else
		{
			assert_dbg();
		}
		instr++;
	}
}

void _fetchShaderDecompiler_parseCF(LatteFetchShader* parsedFetchShader, uint32* contextRegister, std::span<uint8> programCode)
{
	size_t maxCountCFInstructions = programCode.size_bytes() / sizeof(LatteCFInstruction);
	const LatteCFInstruction* cfInstruction = (LatteCFInstruction*)programCode.data();
	const LatteCFInstruction* end = cfInstruction + maxCountCFInstructions;
	while (cfInstruction < end)
	{
		if (cfInstruction->getField_Opcode() == LatteCFInstruction::INST_VTX_TC)
		{
			auto vtxInstruction = cfInstruction->getParserIfOpcodeMatch<LatteCFInstruction_DEFAULT>();
			cemu_assert_debug(vtxInstruction->getField_COND() == LatteCFInstruction::CF_COND::CF_COND_ACTIVE);
			_fetchShaderDecompiler_parseVTXClause(parsedFetchShader, contextRegister, vtxInstruction->getClauseCode(programCode), vtxInstruction->getField_COUNT());
		}
		else if (cfInstruction->getField_Opcode() == LatteCFInstruction::INST_RETURN)
		{
			cemu_assert_debug(!cfInstruction->getField_END_OF_PROGRAM());
			return;
		}
		else
		{
			cemu_assert_debug(false); // unhandled / unexpected CF instruction
		}
		if (cfInstruction->getField_END_OF_PROGRAM())
		{
			cemu_assert_debug(false); // unusual for fetch shader? They should end with a return instruction
			break;
		}
		cfInstruction++;
	}
	cemu_assert_debug(false); // program must be terminated with an instruction that has EOP set?
}

// parse fetch shader and create LatteFetchShader object
// also registers the fs in the cache (s_fetchShaderByHash)
// can be assumed to be thread-safe, if called simultaneously on the same fetch shader only one shader will become registered. The others will be destroyed
LatteFetchShader* LatteShaderRecompiler_createFetchShader(LatteFetchShader::CacheHash fsHash, uint32* contextRegister, uint32* fsProgramCode, uint32 fsProgramSize)
{
	LatteFetchShader* newFetchShader = new LatteFetchShader();
	newFetchShader->m_cacheHash = fsHash;
	if( (fsProgramSize&0xF) != 0 )
		debugBreakpoint();
	uint32 index = 0;

	// if the first instruction is a CF instruction then parse shader properly
	// otherwise fall back to our broken legacy method (where we assumed fetch shaders had no CF program)
	// this workaround is required to make sure old shader caches dont break

	// from old fetch shader gen (CF part missing):
	//			{0x0000a001, 0x27961000, 0x00020000, 0x00000000}
	//			{0x0000a001, 0x2c151002, 0x00020000, 0x00000000, 0x0000a001, 0x068d1000, 0x0000000c, ...}
	//			{0x0000a001, 0x2c151000, 0x00020000, 0x00000000}
	//          {0x0300aa21, 0x28cd1006, 0x00000000, 0x00000000, 0x0300ab21, 0x28cd1007, 0x00000000, ...}

	// shaders shipped with games (e.g. BotW):
	//			{0x00000002, 0x01800400, 0x00000000, 0x8a000000, 0x1c00a001, 0x280d1000, 0x00090000, ...}
	//			{0x00000002, 0x01800000, 0x00000000, 0x8a000000, 0x1c00a001, 0x27961000, 0x000a0000, ...}
	//			{0x00000002, 0x01800c00, 0x00000000, 0x8a000000, 0x2c00a001, 0x2c151000, 0x000a0000, ...} // size 0x50
	//          {0x00000002, 0x01801000, 0x00000000, 0x8a000000, 0x1c00a001, 0x280d1000, 0x00090000, ...} // size 0x60
	//			{0x00000002, 0x01801c00, 0x00000000, 0x8a000000, 0x1c00a001, 0x280d1000, 0x00090000, ...} // size 0x90
	
	// our new implementation:
	//			{0x00000002, 0x01800400, 0x00000000, 0x8a000000, 0x0000a001, 0x2c151000, 0x00020000, ...}

	// for ALU instructions everything except the 01 is dynamic
	newFetchShader->bufferGroups.reserve(16);
	if (fsProgramSize == 0)
	{
		// empty fetch shader, seen in Minecraft
		// these only make sense when vertex shader does not call FS?
		LatteShader_calculateFSKey(newFetchShader);
		newFetchShader->CalculateFetchShaderVkHash();
		return newFetchShader;
	}

	if ((fsProgramCode[0] & 1) == 0 && fsProgramCode[0] <= 0x30 && (fsProgramCode[1]&~((3 << 10)| (1 << 19))) == 0x01800000)
	{
		// very likely a CF instruction
		_fetchShaderDecompiler_parseCF(newFetchShader, contextRegister, { (uint8*)fsProgramCode, fsProgramSize });
	}
	else
	{
		while (index < (fsProgramSize / 4))
		{
			uint32 dword0 = fsProgramCode[index];
			uint32 opcode = dword0 & 0x1F;
			index++;
			if (opcode == VTX_INST_MEM)
			{
				// this might be the clause initialization instruction? (Seems to be the first instruction always)
				// todo - upon further investigation, it seems like fetch shaders also start with a CF program. Our implementation doesnt emit one right now
				uint32 opcode2 = (dword0 >> 8) & 7;

				index += 3;
			}
			else if (opcode == VTX_INST_SEMANTIC)
			{
				_fetchShaderDecompiler_parseInstruction_VTX_SEMANTIC(newFetchShader, contextRegister, (const LatteClauseInstruction_VTX*)(fsProgramCode + index - 1));
				index += 3;
			}
		}
	}
	newFetchShader->bufferGroups.shrink_to_fit();
	// calculate group information
	// VBO offsets and stride
	uint32 vboOffset = 0;
	for (auto& bufferGroup : newFetchShader->bufferGroups)
	{
		for(sint32 i=0; i< bufferGroup.attribCount; i++)
		{
			uint32 attribSize = LatteShaderRecompiler_getAttributeSize(bufferGroup.attrib+i);
			uint32 attribAlignment = LatteShaderRecompiler_getAttributeAlignment(bufferGroup.attrib+i);
			// fix alignment
			vboOffset = (vboOffset+attribAlignment-1)&~(attribAlignment-1);
			vboOffset += attribSize;
			// index type
			if(bufferGroup.attrib[i].fetchType == LatteConst::VERTEX_DATA)
				bufferGroup.hasVtxIndexAccess = true;
			else if (bufferGroup.attrib[i].fetchType == LatteConst::INSTANCE_DATA)
				bufferGroup.hasInstanceIndexAccess = true;
		}
		// fix alignment of whole vertex
		if(bufferGroup.attribCount > 0 )
		{
			uint32 attribAlignment = LatteShaderRecompiler_getAttributeAlignment(bufferGroup.attrib+0);
			vboOffset = (vboOffset+attribAlignment-1)&~(attribAlignment-1);
		}
		bufferGroup.vboStride = vboOffset;
	}
	LatteShader_calculateFSKey(newFetchShader);
	newFetchShader->CalculateFetchShaderVkHash();

	// register in cache
	// its possible that during multi-threaded shader cache loading, two identical (same hash) fetch shaders get created simultaneously
	// we catch and handle this case here. RegisterInCache() is atomic and if another fetch shader is already registered, we abandon the local instance
	LatteFetchShader* registeredFS = newFetchShader->RegisterInCache(fsHash);
	if (registeredFS)
	{
		delete newFetchShader;
		newFetchShader = registeredFS;
	}
	else
	{
		newFetchShader->m_isRegistered = true;
	}


	return newFetchShader;
}

LatteFetchShader::~LatteFetchShader()
{
	UnregisterInCache();
}

struct FetchShaderLookupInfo 
{
	LatteFetchShader* fetchShader;
	uint32 programSize;
	uint32 lastFrameAccessed;
};

LookupTableL3<8, 8, 8, FetchShaderLookupInfo*> g_fetchShaderLookupCache;

LatteFetchShader::CacheHash LatteFetchShader::CalculateCacheHash(void* programCode, uint32 programSize)
{
	uint32* programCodeU32 = (uint32*)programCode;
	uint64 progHash1 = 0;
	uint64 progHash2 = 0;
	for (uint32 i = 0; i < programSize / 4; i++)
	{
		uint32 temp = programCodeU32[i];
		progHash1 += (uint64)temp;
		progHash2 ^= (uint64)temp;
		progHash1 = (progHash1 << 3) | (progHash1 >> 61);
		progHash2 = (progHash2 >> 7) | (progHash2 << 57);
	}

	// todo - we should incorporate the value of VGT_INSTANCE_STEP_RATE_0/1 into the hash since it affects the generated LatteFetchShader object
	//        However, this would break compatibility with shader caches and gfx packs due to altering the shader base hashes

	return progHash1 + progHash2;
}

LatteFetchShader* LatteFetchShader::FindInCacheByHash(LatteFetchShader::CacheHash fsHash)
{
	// does not hold s_fetchShaderCache for better performance. Be careful not to call this while another thread invokes RegisterInCache()
	auto itr = s_fetchShaderByHash.find(fsHash);
	if (itr == s_fetchShaderByHash.end())
		return nullptr;
	return itr->second;
}

void* _getFSProgramPtr()
{
	return memory_getPointerFromPhysicalOffset(LatteGPUState.contextRegister[mmSQ_PGM_START_FS + 0] << 8);
}

uint32 _getFSProgramSize()
{
	return LatteGPUState.contextRegister[mmSQ_PGM_START_FS + 1] << 3;
}

LatteFetchShader* LatteFetchShader::FindByGPUState()
{
	// retrieve fetch shader that matches the currently set GPU context registers
	uint32 fsPhysAddr24 = LatteGPUState.contextRegister[mmSQ_PGM_START_FS + 0];
	cemu_assert_debug(fsPhysAddr24 < 0x1000000); // should only contain the upper 24 bit of the address in the lower 24 bit of the register

	FetchShaderLookupInfo* lookupInfo = g_fetchShaderLookupCache.lookup(fsPhysAddr24);
	if (lookupInfo)
	{
		// return fetch shader if still the same
		uint32 fsSize = _getFSProgramSize();
		uint32 framesSinceLastAccess = LatteGPUState.frameCounter - lookupInfo->lastFrameAccessed;
		if (lookupInfo->programSize == fsSize && framesSinceLastAccess == 0)
		{
			lookupInfo->lastFrameAccessed = LatteGPUState.frameCounter;
			return lookupInfo->fetchShader;
		}
		// update lookup info
		CacheHash fsHash = CalculateCacheHash(_getFSProgramPtr(), _getFSProgramSize());
		LatteFetchShader* fetchShader = FindInCacheByHash(fsHash);
		if (!fetchShader)
		{
			fetchShader = LatteShaderRecompiler_createFetchShader(fsHash, LatteGPUState.contextNew.GetRawView(), (uint32*)_getFSProgramPtr(), _getFSProgramSize());
			cemu_assert(fetchShader);
		}
		lookupInfo->fetchShader = fetchShader;
		lookupInfo->programSize = fsSize;
		lookupInfo->lastFrameAccessed = LatteGPUState.frameCounter;
		return fetchShader;
	}
	else
	{
		// try to find fetch shader by data hash
		CacheHash fsHash = CalculateCacheHash(_getFSProgramPtr(), _getFSProgramSize());
		LatteFetchShader* fetchShader = FindInCacheByHash(fsHash);
		if (!fetchShader)
		{
			fetchShader = LatteShaderRecompiler_createFetchShader(fsHash, LatteGPUState.contextNew.GetRawView(), (uint32*)_getFSProgramPtr(), _getFSProgramSize());
			cemu_assert(fetchShader);
		}
		// create new lookup entry
		lookupInfo = new FetchShaderLookupInfo();
		lookupInfo->fetchShader = fetchShader;
		lookupInfo->programSize = _getFSProgramSize();
		lookupInfo->lastFrameAccessed = LatteGPUState.frameCounter;
		g_fetchShaderLookupCache.store(fsPhysAddr24, lookupInfo);
#ifdef CEMU_DEBUG_ASSERT
		cemu_assert_debug(g_fetchShaderLookupCache.lookup(fsPhysAddr24) == lookupInfo);
#endif
	}
	return lookupInfo->fetchShader;
}

FSpinlock s_spinlockFetchShaderCache;

LatteFetchShader* LatteFetchShader::RegisterInCache(CacheHash fsHash)
{
	s_spinlockFetchShaderCache.lock();
	auto itr = s_fetchShaderByHash.find(fsHash);
	if (itr != s_fetchShaderByHash.end())
	{
		LatteFetchShader* fs = itr->second;
		s_spinlockFetchShaderCache.unlock();
		return fs;
	}
	s_fetchShaderByHash.emplace(fsHash, this);
	s_spinlockFetchShaderCache.unlock();
	return nullptr;
}

void LatteFetchShader::UnregisterInCache()
{
	if (!m_isRegistered)
		return;
	s_spinlockFetchShaderCache.lock();
	auto itr = s_fetchShaderByHash.find(m_cacheHash);
	cemu_assert(itr == s_fetchShaderByHash.end());
	s_fetchShaderByHash.erase(itr);
	s_spinlockFetchShaderCache.unlock();
}

std::unordered_map<LatteFetchShader::CacheHash, LatteFetchShader*> LatteFetchShader::s_fetchShaderByHash;
