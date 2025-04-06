#include "Common/GLInclude/GLInclude.h"

#include "Cafe/HW/Latte/Core/LatteRingBuffer.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteSoftware.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"

#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureViewGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/CachedFBOGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/RendererShaderGL.h"

#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/OS/libs/gx2/GX2.h"

#include "Cafe/GameProfile/GameProfile.h"
#include "config/ActiveSettings.h"


using _INDEX_TYPE = Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE;

GLenum sGLActiveDrawMode = 0;

extern bool hasValidFramebufferAttached;

#define INDEX_CACHE_ENTRIES		(8)

typedef struct
{
	MPTR prevIndexDataMPTR;
	sint32 prevIndexType;
	sint32 prevCount;
	// index data
	uint8* indexData;
	uint8* indexData2;
	uint32 indexBufferOffset;
	sint32 indexDataSize; // current size
	sint32 indexDataLimit; // maximum size
	// info
	uint32 maxIndex;
	uint32 minIndex;
}indexDataCacheEntry_t;

struct
{
	indexDataCacheEntry_t indexCacheEntry[INDEX_CACHE_ENTRIES];
	sint32 nextCacheEntryIndex;
	// info about currently used index data
	uint32 maxIndex;
	uint32 minIndex;
	uint8* indexData;
	// buffer 
	GLuint glIndexCacheBuffer;
	VirtualBufferHeap_t* indexBufferVirtualHeap;
	uint8* mappedIndexBuffer;
	LatteRingBuffer_t* indexRingBuffer;
	uint8* tempIndexStorage;
	// misc
	bool initialized;
	GLuint glActiveElementArrayBuffer;
}indexState = { 0 };

struct
{
	uint8* vboOutput;
	uint32 vboStride;
	uint8 dataFormat;
	uint8 nfa;
	bool isSigned;
}activeAttributePointer[LATTE_VS_ATTRIBUTE_LIMIT] = { 0 };

void LatteDraw_resetAttributePointerCache()
{
	for (sint32 i = 0; i < LATTE_VS_ATTRIBUTE_LIMIT; i++)
	{
		activeAttributePointer[i].vboOutput = (uint8*)-1;
		activeAttributePointer[i].vboStride = (uint32)-1;
	}
}

void _setAttributeBufferPointerRaw(uint32 attributeShaderLoc, uint8* buffer, uint32 bufferSize, uint32 stride, LatteParsedFetchShaderAttribute_t* attrib, uint8* vboOutput, uint32 vboStride)
{
	uint32 dataFormat = attrib->format;
	bool isSigned = attrib->isSigned != 0;
	uint8 nfa = attrib->nfa;
	// don't call glVertexAttribIPointer if parameters have not changed
	if (activeAttributePointer[attributeShaderLoc].vboOutput == vboOutput && activeAttributePointer[attributeShaderLoc].vboStride == vboStride && activeAttributePointer[attributeShaderLoc].dataFormat == dataFormat && activeAttributePointer[attributeShaderLoc].nfa == nfa && activeAttributePointer[attributeShaderLoc].isSigned == isSigned)
	{
		return;
	}
	activeAttributePointer[attributeShaderLoc].vboOutput = vboOutput;
	activeAttributePointer[attributeShaderLoc].vboStride = vboStride;
	activeAttributePointer[attributeShaderLoc].dataFormat = dataFormat;
	activeAttributePointer[attributeShaderLoc].nfa = nfa;
	activeAttributePointer[attributeShaderLoc].isSigned = isSigned;
	// setup attribute pointer
	if (dataFormat == FMT_32_32_32_32_FLOAT || dataFormat == FMT_32_32_32_32)
	{
		glVertexAttribIPointer(attributeShaderLoc, 4, GL_UNSIGNED_INT, vboStride, vboOutput);
	}
	else if (dataFormat == FMT_32_32_32_FLOAT || dataFormat == FMT_32_32_32)
	{
		glVertexAttribIPointer(attributeShaderLoc, 3, GL_UNSIGNED_INT, vboStride, vboOutput);
	}
	else if (dataFormat == FMT_32_32_FLOAT || dataFormat == FMT_32_32)
	{
		glVertexAttribIPointer(attributeShaderLoc, 2, GL_UNSIGNED_INT, vboStride, vboOutput);
	}
	else if (dataFormat == FMT_32_FLOAT || dataFormat == FMT_32)
	{
		glVertexAttribIPointer(attributeShaderLoc, 1, GL_UNSIGNED_INT, vboStride, vboOutput);
	}
	else if (dataFormat == FMT_8_8_8_8)
	{
		glVertexAttribIPointer(attributeShaderLoc, 4, GL_UNSIGNED_BYTE, vboStride, vboOutput);
	}
	else if (dataFormat == FMT_8_8)
	{
		// workaround for AMD (alignment must be 4 for 2xbyte)
		if (((uint32)(size_t)vboOutput & 0x3) == 2 && LatteGPUState.glVendor == GLVENDOR_AMD)
		{
			glVertexAttribIPointer(attributeShaderLoc, 4, GL_UNSIGNED_BYTE, vboStride, vboOutput - 2);
		}
		else
		{
			glVertexAttribIPointer(attributeShaderLoc, 2, GL_UNSIGNED_BYTE, vboStride, vboOutput);
		}
	}
	else if (dataFormat == FMT_8)
	{
		glVertexAttribIPointer(attributeShaderLoc, 1, GL_UNSIGNED_BYTE, vboStride, vboOutput);
	}
	else if (dataFormat == FMT_16_16_16_16_FLOAT || dataFormat == FMT_16_16_16_16)
	{
		glVertexAttribIPointer(attributeShaderLoc, 4, GL_UNSIGNED_SHORT, vboStride, vboOutput);
	}
	else if (dataFormat == FMT_16_16_FLOAT || dataFormat == FMT_16_16)
	{
		glVertexAttribIPointer(attributeShaderLoc, 2, GL_UNSIGNED_SHORT, vboStride, vboOutput);
	}
	else if (dataFormat == FMT_16_FLOAT || dataFormat == FMT_16)
	{
		glVertexAttribIPointer(attributeShaderLoc, 1, GL_UNSIGNED_SHORT, vboStride, vboOutput);
	}
	else if (dataFormat == FMT_2_10_10_10)
	{
		glVertexAttribIPointer(attributeShaderLoc, 1, GL_UNSIGNED_INT, vboStride, vboOutput);
	}
	else
	{
		debug_printf("_setAttributeBufferPointerRaw(): Unsupported format %d\n", dataFormat);
		cemu_assert_unimplemented();
	}
}

bool glAttributeArrayIsEnabled[GPU_GL_MAX_NUM_ATTRIBUTE] = { 0 };
sint32 glAttributeArrayAluDivisor[GPU_GL_MAX_NUM_ATTRIBUTE] = { 0 };

void OpenGLRenderer::SetAttributeArrayState(uint32 index, bool isEnabled, sint32 aluDivisor)
{
	cemu_assert_debug(index < GPU_GL_MAX_NUM_ATTRIBUTE);
	catchOpenGLError();
	if (glAttributeArrayIsEnabled[index] != isEnabled)
	{
		if (isEnabled)
		{
			// enable
			glEnableVertexAttribArray(index);
			glAttributeArrayIsEnabled[index] = true;
		}
		else
		{
			// disable
			glDisableVertexAttribArray(index);
			glAttributeArrayIsEnabled[index] = false;
		}
		catchOpenGLError();
	}
	// set divisor state
	if (glAttributeArrayAluDivisor[index] != aluDivisor)
	{
		if (aluDivisor <= 0)
			glVertexAttribDivisor(index, 0);
		else
			glVertexAttribDivisor(index, aluDivisor);
		glAttributeArrayAluDivisor[index] = aluDivisor;
		catchOpenGLError();
	}
}

// Sets the currently active element array buffer and binds it
void OpenGLRenderer::SetArrayElementBuffer(GLuint arrayElementBuffer)
{
	if (arrayElementBuffer == indexState.glActiveElementArrayBuffer)
		return;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, arrayElementBuffer);
	indexState.glActiveElementArrayBuffer = arrayElementBuffer;
}

typedef struct
{
	MPTR physAddr;
	sint32 count;
	uint32 primitiveRestartIndex;
	uint32 primitiveMode;
}indexDataCacheKey_t;

typedef struct _indexDataCacheEntry_t
{
	indexDataCacheKey_t key;
	_indexDataCacheEntry_t* nextInBucket; // points to next element in same bucket
	uint32 physSize;
	uint32 hash;
	_INDEX_TYPE indexType;
	//sint32 indexType;
	uint32 minIndex;
	uint32 maxIndex;
	uint32 lastAccessFrameCount;
	VirtualBufferHeapEntry_t* heapEntry;
	_indexDataCacheEntry_t* nextInMostRecentUsage; // points to element which was used more recently
	_indexDataCacheEntry_t* prevInMostRecentUsage; // points to element which was used less recently
}indexDataCacheEntry2_t;

#define INDEX_DATA_CACHE_BUCKETS		(1783)

indexDataCacheEntry2_t* indexDataCacheBucket[INDEX_DATA_CACHE_BUCKETS] = { 0 };
indexDataCacheEntry2_t* indexDataCacheFirst = nullptr; // points to least recently used item
indexDataCacheEntry2_t* indexDataCacheLast = nullptr; // points to most recently used item
sint32 indexDataCacheEntryCount = 0;

void _appendToUsageLinkedList(indexDataCacheEntry2_t* entry)
{
	if (indexDataCacheLast == nullptr)
	{
		indexDataCacheLast = entry;
		indexDataCacheFirst = entry;
		entry->nextInMostRecentUsage = nullptr;
		entry->prevInMostRecentUsage = nullptr;
	}
	else
	{
		indexDataCacheLast->nextInMostRecentUsage = entry;
		entry->prevInMostRecentUsage = indexDataCacheLast;
		entry->nextInMostRecentUsage = nullptr;
		indexDataCacheLast = entry;
	}
}

void _removeFromUsageLinkedList(indexDataCacheEntry2_t* entry)
{
	if (entry->prevInMostRecentUsage)
	{
		entry->prevInMostRecentUsage->nextInMostRecentUsage = entry->nextInMostRecentUsage;
	}
	else
		indexDataCacheFirst = entry->nextInMostRecentUsage;
	if (entry->nextInMostRecentUsage)
	{
		entry->nextInMostRecentUsage->prevInMostRecentUsage = entry->prevInMostRecentUsage;
	}
	else
		indexDataCacheLast = entry->prevInMostRecentUsage;
	entry->prevInMostRecentUsage = nullptr;
	entry->nextInMostRecentUsage = nullptr;
}

void _removeFromBucket(indexDataCacheEntry2_t* entry)
{
	uint32 indexDataBucketIdx = (uint32)((entry->key.physAddr + entry->key.count) ^ (entry->key.physAddr >> 16)) % INDEX_DATA_CACHE_BUCKETS;
	if (indexDataCacheBucket[indexDataBucketIdx] == entry)
	{
		indexDataCacheBucket[indexDataBucketIdx] = entry->nextInBucket;
		entry->nextInBucket = nullptr;
		return;
	}
	indexDataCacheEntry2_t* cacheEntryItr = indexDataCacheBucket[indexDataBucketIdx];
	while (cacheEntryItr)
	{
		if (cacheEntryItr->nextInBucket == entry)
		{
			cacheEntryItr->nextInBucket = entry->nextInBucket;
			entry->nextInBucket = nullptr;
			return;
		}
		// next
		cacheEntryItr = cacheEntryItr->nextInBucket;
	}
}

void _decodeAndUploadIndexData(indexDataCacheEntry2_t* cacheEntry)
{
	uint32 count = cacheEntry->key.count;
	uint32 primitiveRestartIndex = cacheEntry->key.primitiveRestartIndex;
	if (cacheEntry->indexType == _INDEX_TYPE::U16_BE)
	{
		// 16bit indices
		uint16* indexInputU16 = (uint16*)memory_getPointerFromPhysicalOffset(cacheEntry->key.physAddr);
		uint16* indexOutputU16 = (uint16*)indexState.tempIndexStorage;
		cemu_assert_debug(count != 0);
		uint16 indexMinU16 = 0xFFFF;
		uint16 indexMaxU16 = 0;
		if (primitiveRestartIndex < 0x10000)
		{
			// with primitive restart index
			uint16 primitiveRestartIndexU16 = (uint16)primitiveRestartIndex;
			for (uint32 i = 0; i < count; i++)
			{
				uint16 idxU16 = _swapEndianU16(*indexInputU16);
				indexInputU16++;
				if (primitiveRestartIndexU16 != idxU16)
				{
					indexMinU16 = std::min(indexMinU16, idxU16);
					indexMaxU16 = std::max(indexMaxU16, idxU16);
				}
				*indexOutputU16 = idxU16;
				indexOutputU16++;
			}
		}
		else
		{
			// without primitive restart index
			for (uint32 i = 0; i < count; i++)
			{
				uint16 idxU16 = _swapEndianU16(*indexInputU16);
				indexInputU16++;
				indexMinU16 = std::min(indexMinU16, idxU16);
				indexMaxU16 = std::max(indexMaxU16, idxU16);
				*indexOutputU16 = idxU16;
				indexOutputU16++;
			}
		}
		cacheEntry->minIndex = indexMinU16;
		cacheEntry->maxIndex = indexMaxU16;
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, cacheEntry->heapEntry->startOffset, count * sizeof(uint16), indexState.tempIndexStorage);
		performanceMonitor.cycle[performanceMonitor.cycleIndex].indexDataUploaded += (count * sizeof(uint16));
	}
	else if(cacheEntry->indexType == _INDEX_TYPE::U32_BE)
	{
		// 32bit indices
		uint32* indexInputU32 = (uint32*)memory_getPointerFromPhysicalOffset(cacheEntry->key.physAddr);
		uint32* indexOutputU32 = (uint32*)indexState.tempIndexStorage;
		cemu_assert_debug(count != 0);
		uint32 indexMinU32 = _swapEndianU32(*indexInputU32);
		uint32 indexMaxU32 = _swapEndianU32(*indexInputU32);
		for (uint32 i = 0; i < count; i++)
		{
			uint32 idxU32 = _swapEndianU32(*indexInputU32);
			indexInputU32++;
			if (idxU32 != primitiveRestartIndex)
			{
				indexMinU32 = std::min(indexMinU32, idxU32);
				indexMaxU32 = std::max(indexMaxU32, idxU32);
			}
			*indexOutputU32 = idxU32;
			indexOutputU32++;
		}
		cacheEntry->minIndex = indexMinU32;
		cacheEntry->maxIndex = indexMaxU32;
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, cacheEntry->heapEntry->startOffset, count * sizeof(uint32), indexState.tempIndexStorage);
		performanceMonitor.cycle[performanceMonitor.cycleIndex].indexDataUploaded += (count * sizeof(uint32));
	}
	else
	{
		cemu_assert_debug(false);
	}
}


void LatteDraw_cleanupAfterFrame()
{
	// drop everything from cache that is older than 30 frames
	uint32 frameCounter = LatteGPUState.frameCounter;
	while (indexDataCacheFirst)
	{
		indexDataCacheEntry2_t* entry = indexDataCacheFirst;
		if ((frameCounter - entry->lastAccessFrameCount) < 30)
			break;
		// remove entry
		virtualBufferHeap_free(indexState.indexBufferVirtualHeap, entry->heapEntry);
		_removeFromUsageLinkedList(entry);
		_removeFromBucket(entry);
		free(entry);
	}
}

void LatteDrawGL_removeLeastRecentlyUsedIndexCacheEntries(sint32 count)
{
	while (indexDataCacheFirst && count > 0)
	{
		indexDataCacheEntry2_t* entry = indexDataCacheFirst;
		// remove entry
		virtualBufferHeap_free(indexState.indexBufferVirtualHeap, entry->heapEntry);
		_removeFromUsageLinkedList(entry);
		_removeFromBucket(entry);
		free(entry);
		count--;
	}
}

uint32 LatteDrawGL_calculateIndexDataHash(uint8* data, uint32 size)
{
	uint32 h = 0;
	if (size < 16)
	{
		// hash the bytes individually
		while (size != 0)
		{
			h += (uint32)*data;
			data++;
			size--;
		}
		return h;
	}
	// first 16 bytes
	h += *(uint32*)(data + 0);
	h += *(uint32*)(data + 4);
	h += *(uint32*)(data + 8);
	h += *(uint32*)(data + 12);
	// last 16 bytes
	data = data + ((size - 16)&~3);
	h += *(uint32*)(data + 0);
	h += *(uint32*)(data + 4);
	h += *(uint32*)(data + 8);
	h += *(uint32*)(data + 12);
	return h;
}

// index handling with cache
// todo - Outdated cache implementation. Update OpenGL renderer to use the generic implementation that is also used by the Vulkan renderer
void LatteDrawGL_prepareIndicesWithGPUCache(MPTR indexDataMPTR, _INDEX_TYPE indexType, sint32 count, sint32 primitiveMode)
{
	if (indexType == _INDEX_TYPE::AUTO)
	{
		indexState.minIndex = 0;
		indexState.maxIndex = count - 1;
		// since no indices are used we don't need to unbind the element array buffer
		return; // automatic indices
	}

	OpenGLRenderer::SetArrayElementBuffer(indexState.glIndexCacheBuffer);
	uint32 indexDataBucketIdx = (uint32)((indexDataMPTR + count) ^ (indexDataMPTR >> 16)) % INDEX_DATA_CACHE_BUCKETS;
	// find matching entry
	uint32 primitiveRestartIndex = LatteGPUState.contextNew.VGT_MULTI_PRIM_IB_RESET_INDX.get_RESTART_INDEX();
	indexDataCacheEntry2_t* cacheEntryItr = indexDataCacheBucket[indexDataBucketIdx];
	indexDataCacheKey_t compareKey;
	compareKey.physAddr = indexDataMPTR;
	compareKey.count = count;
	compareKey.primitiveMode = primitiveMode;
	compareKey.primitiveRestartIndex = primitiveRestartIndex;
	while (cacheEntryItr)
	{
		if (memcmp(&(cacheEntryItr->key), &compareKey, sizeof(compareKey)) != 0)
		{
			// next
			cacheEntryItr = cacheEntryItr->nextInBucket;
			continue;
		}
		// entry found
		indexState.minIndex = cacheEntryItr->minIndex;
		indexState.maxIndex = cacheEntryItr->maxIndex;
		indexState.indexData = (uint8*)(size_t)cacheEntryItr->heapEntry->startOffset;
		cacheEntryItr->lastAccessFrameCount = LatteGPUState.frameCounter;
		// check if the data changed
		uint32 h = LatteDrawGL_calculateIndexDataHash(memory_getPointerFromPhysicalOffset(indexDataMPTR), cacheEntryItr->physSize);
		if (cacheEntryItr->hash != h)
		{
			cemuLog_logDebug(LogType::Force, "IndexData hash changed");
			_decodeAndUploadIndexData(cacheEntryItr);
			cacheEntryItr->hash = h;
		}
		// move entry to the front
		_removeFromUsageLinkedList(cacheEntryItr);
		_appendToUsageLinkedList(cacheEntryItr);
		return;
	}
	// calculate size of index data in cache
	sint32 cacheIndexDataSize = 0;
	if (indexType == _INDEX_TYPE::U16_BE || indexType == _INDEX_TYPE::U16_LE)
		cacheIndexDataSize = count * sizeof(uint16);
	else
		cacheIndexDataSize = count * sizeof(uint32);
	// no matching entry, create new one
	VirtualBufferHeapEntry_t* heapEntry = virtualBufferHeap_allocate(indexState.indexBufferVirtualHeap, cacheIndexDataSize);
	if (heapEntry == nullptr)
	{
		while (true)
		{
			LatteDrawGL_removeLeastRecentlyUsedIndexCacheEntries(10);
			heapEntry = virtualBufferHeap_allocate(indexState.indexBufferVirtualHeap, cacheIndexDataSize);
			if (heapEntry != nullptr)
				break;
			if (indexDataCacheFirst == nullptr)
			{
				cemuLog_log(LogType::Force, "Unable to allocate entry in index cache");
				assert_dbg();
			}
		}
	}
	indexDataCacheEntry2_t* cacheEntry = (indexDataCacheEntry2_t*)malloc(sizeof(indexDataCacheEntry2_t));
	memset(cacheEntry, 0, sizeof(indexDataCacheEntry2_t));
	cacheEntry->key.physAddr = indexDataMPTR;
	cacheEntry->physSize = (indexType == _INDEX_TYPE::U16_BE || indexType == _INDEX_TYPE::U16_LE) ? (count * sizeof(uint16)) : (count * sizeof(uint32));
	cacheEntry->hash = LatteDrawGL_calculateIndexDataHash(memory_getPointerFromPhysicalOffset(indexDataMPTR), cacheEntry->physSize);
	cacheEntry->key.count = count;
	cacheEntry->key.primitiveRestartIndex = primitiveRestartIndex;
	cacheEntry->indexType = indexType;
	cacheEntry->key.primitiveMode = primitiveMode;
	cacheEntry->heapEntry = heapEntry;
	cacheEntry->lastAccessFrameCount = LatteGPUState.frameCounter;
	// append entry in bucket list
	cacheEntry->nextInBucket = indexDataCacheBucket[indexDataBucketIdx];
	indexDataCacheBucket[indexDataBucketIdx] = cacheEntry;
	// append as most recently used entry
	_appendToUsageLinkedList(cacheEntry);
	// decode and upload the data
	_decodeAndUploadIndexData(cacheEntry);

	indexDataCacheEntryCount++;
	indexState.minIndex = cacheEntry->minIndex;
	indexState.maxIndex = cacheEntry->maxIndex;
	indexState.indexData = (uint8*)(size_t)cacheEntry->heapEntry->startOffset;
}

void LatteDraw_handleSpecialState8_clearAsDepth()
{
	if (LatteGPUState.contextNew.GetSpecialStateValues()[0] == 0)
		cemuLog_logDebug(LogType::Force, "Special state 8 requires special state 0 but it is not set?");
	// get depth buffer information
	uint32 regDepthBuffer = LatteGPUState.contextRegister[mmDB_HTILE_DATA_BASE];
	uint32 regDepthSize = LatteGPUState.contextRegister[mmDB_DEPTH_SIZE];
	uint32 regDepthBufferInfo = LatteGPUState.contextRegister[mmDB_DEPTH_INFO];
	// get format and tileMode from info reg
	uint32 depthBufferTileMode = (regDepthBufferInfo >> 15) & 0xF;

	MPTR depthBufferPhysMem = regDepthBuffer << 8;
	uint32 depthBufferPitch = (((regDepthSize >> 0) & 0x3FF) + 1);
	uint32 depthBufferHeight = ((((regDepthSize >> 10) & 0xFFFFF) + 1) / depthBufferPitch);
	depthBufferPitch <<= 3;
	depthBufferHeight <<= 3;
	uint32 depthBufferWidth = depthBufferPitch;

	sint32 sliceIndex = 0; // todo
	sint32 mipIndex = 0;

	// clear all color buffers that match the format of the depth buffer
	sint32 searchIndex = 0;
	bool targetFound = false;
	while (true)
	{
		LatteTextureView* view = LatteTC_LookupTextureByData(depthBufferPhysMem, depthBufferWidth, depthBufferHeight, depthBufferPitch, 0, 1, sliceIndex, 1, &searchIndex);
		if (!view)
		{
			// should we clear in RAM instead?
			break;
		}
		sint32 effectiveClearWidth = view->baseTexture->width;
		sint32 effectiveClearHeight = view->baseTexture->height;
		LatteTexture_scaleToEffectiveSize(view->baseTexture, &effectiveClearWidth, &effectiveClearHeight, 0);

		// hacky way to get clear color
		float* regClearColor = (float*)(LatteGPUState.contextRegister + 0xC000 + 0); // REG_BASE_ALU_CONST

		uint8 clearColor[4] = { 0 };
		clearColor[0] = (uint8)(regClearColor[0] * 255.0f);
		clearColor[1] = (uint8)(regClearColor[1] * 255.0f);
		clearColor[2] = (uint8)(regClearColor[2] * 255.0f);
		clearColor[3] = (uint8)(regClearColor[3] * 255.0f);

		// todo - use fragment shader software emulation (evoke for one pixel) to determine clear color
		// todo - dont clear entire slice, use effectiveClearWidth, effectiveClearHeight

		if (g_renderer->GetType() == RendererAPI::OpenGL)
		{
			//cemu_assert_debug(false); // implement g_renderer->texture_clearColorSlice properly for OpenGL renderer
			if (glClearTexSubImage)
				glClearTexSubImage(((LatteTextureViewGL*)view)->glTexId, mipIndex, 0, 0, 0, effectiveClearWidth, effectiveClearHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE, clearColor);
		}
		else
		{
			if (view->baseTexture->isDepth)
				g_renderer->texture_clearDepthSlice(view->baseTexture, sliceIndex + view->firstSlice, mipIndex + view->firstMip, true, view->baseTexture->hasStencil, 0.0f, 0);
			else
				g_renderer->texture_clearColorSlice(view->baseTexture, sliceIndex + view->firstSlice, mipIndex + view->firstMip, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		}
	}
}

void LatteDrawGL_doDraw(_INDEX_TYPE indexType, uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count)
{
	if (indexType == _INDEX_TYPE::U16_BE)
	{
		// 16bit index, big endian
		if (instanceCount > 1 || baseInstance != 0)
		{
			glDrawElementsInstancedBaseVertexBaseInstance(sGLActiveDrawMode, count, GL_UNSIGNED_SHORT, indexState.indexData, instanceCount, baseVertex, baseInstance);
		}
		else
		{
			if (baseVertex != 0)
				glDrawRangeElementsBaseVertex(sGLActiveDrawMode, indexState.minIndex, indexState.maxIndex, count, GL_UNSIGNED_SHORT, indexState.indexData, baseVertex);
			else
				glDrawRangeElements(sGLActiveDrawMode, indexState.minIndex, indexState.maxIndex, count, GL_UNSIGNED_SHORT, indexState.indexData);
		}
	}
	else if (indexType == _INDEX_TYPE::U32_BE)
	{
		// 32bit index, big endian
		if (instanceCount > 1 || baseInstance != 0)
		{
			//debug_printf("Render instanced\n");
			glDrawElementsInstancedBaseVertexBaseInstance(sGLActiveDrawMode, count, GL_UNSIGNED_INT, indexState.indexData, instanceCount, baseVertex, baseInstance);
		}
		else
		{
			glDrawRangeElementsBaseVertex(sGLActiveDrawMode, indexState.minIndex, indexState.maxIndex, count, GL_UNSIGNED_INT, indexState.indexData, baseVertex);
		}
	}
	else if (indexType == _INDEX_TYPE::AUTO)
	{
		// render without index (automatic index generation)
		cemu_assert_debug(baseInstance == 0);
		if (instanceCount > 1)
			glDrawArraysInstanced(sGLActiveDrawMode, baseVertex, count, instanceCount);
		else
		{
			glDrawArrays(sGLActiveDrawMode, baseVertex, count);
		}
	}
	else
	{
		cemu_assert_debug(false);
	}
}

uint32 _glVertexBufferOffset[32] = { 0 };

void OpenGLRenderer::buffer_bindVertexBuffer(uint32 bufferIndex, uint32 offset, uint32 size)
{
	_glVertexBufferOffset[bufferIndex] = offset;
}

void OpenGLRenderer::buffer_bindUniformBuffer(LatteConst::ShaderType shaderType, uint32 bufferIndex, uint32 offset, uint32 size)
{
	switch (shaderType)
	{
	case LatteConst::ShaderType::Vertex:
		bufferIndex += 0;
		break;
	case LatteConst::ShaderType::Pixel:
		bufferIndex += 32;
		break;
	case LatteConst::ShaderType::Geometry:
		bufferIndex += 64;
		break;
	}

	if (offset == 0 && size == 0)
	{
		// when binding NULL we just bind some arbitrary undefined data so the OpenGL driver is happy since a size of 0 is not allowed (should we bind a buffer filled with zeroes instead?)
		glBindBufferRange(GL_UNIFORM_BUFFER, bufferIndex, glAttributeCacheAB, 0, 32);
		return;
	}
	glBindBufferRange(GL_UNIFORM_BUFFER, bufferIndex, glAttributeCacheAB, offset, size);
}

void LatteDraw_resetAttributePointerCache();

void _resetAttributes(LatteParsedFetchShaderBufferGroup_t* attribGroup, bool* attributeArrayUsed)
{
	for (sint32 i = 0; i < attribGroup->attribCount; i++)
	{
		LatteParsedFetchShaderAttribute_t* attrib = attribGroup->attrib + i;
		sint32 attributeShaderLocation = attrib->semanticId; // we now bind to the semanticId instead
		attributeArrayUsed[attributeShaderLocation] = false;
	}
}

void OpenGLRenderer::_setupVertexAttributes()
{
	LatteFetchShader* fetchShader = LatteSHRC_GetActiveFetchShader();
	LatteDecompilerShader* vertexShader = LatteSHRC_GetActiveVertexShader();

	catchOpenGLError();

	// bind buffer
	attributeStream_bindVertexCacheBuffer();

	catchOpenGLError();
	LatteFetchShader* parsedFetchShader = LatteSHRC_GetActiveFetchShader();
	bool attributeArrayUsed[32] = { 0 }; // used to keep track of enabled vertex attributes for this shader
	sint32 attributeDataIndex = 0;
	uint32 vboDataOffset = 0;

	bool tfBufferIsBound = false;
	sint32 maxReallocAttemptLimit = 1;

	for(auto& bufferGroup : parsedFetchShader->bufferGroups)
	{
		uint32 bufferIndex = bufferGroup.attributeBufferIndex;
		uint32 bufferBaseRegisterIndex = mmSQ_VTX_ATTRIBUTE_BLOCK_START + bufferIndex * 7;
		MPTR bufferAddress = LatteGPUState.contextRegister[bufferBaseRegisterIndex + 0];
		uint32 bufferSize = LatteGPUState.contextRegister[bufferBaseRegisterIndex + 1] + 1;
		uint32 bufferStride = (LatteGPUState.contextRegister[bufferBaseRegisterIndex + 2] >> 11) & 0xFFFF;

		if (bufferAddress == MPTR_NULL)
		{
			_resetAttributes(&bufferGroup, attributeArrayUsed);
			continue;
		}

		vboDataOffset = _glVertexBufferOffset[bufferIndex];

		for (sint32 i = 0; i < bufferGroup.attribCount; i++)
		{
			LatteParsedFetchShaderAttribute_t* attrib = bufferGroup.attrib + i;
			sint32 attributeShaderLocation = attrib->semanticId; // we now bind to the semanticId instead

			attributeShaderLocation = vertexShader->resourceMapping.getAttribHostShaderIndex(attrib->semanticId);
			if (attributeShaderLocation == -1)
				continue; // attribute not used
			if (attributeShaderLocation >= GPU_GL_MAX_NUM_ATTRIBUTE)
				continue;
			if (attributeArrayUsed[attributeShaderLocation] == true)
			{
				debug_printf("Fetch shader attribute is bound multiple times\n");
			}

			// get buffer
			uint32 bufferIndex = attrib->attributeBufferIndex;
			cemu_assert_debug(bufferIndex < 0x10);

			cemu_assert_debug(attrib->fetchType == LatteConst::VERTEX_DATA || attrib->fetchType == LatteConst::INSTANCE_DATA); // unsupported fetch type

			SetAttributeArrayState(attributeShaderLocation, true, (bufferStride == 0) ? 99999999 : attrib->aluDivisor);

			uint8* bufferInput = memory_getPointerFromPhysicalOffset(bufferAddress) + attrib->offset;
			uint32 bufferSizeInput = bufferSize - attrib->offset;

			uint8* vboGLPtr;
			vboGLPtr = (uint8*)(size_t)(vboDataOffset + attrib->offset);
			_setAttributeBufferPointerRaw(attributeShaderLocation, NULL, 0, bufferStride, attrib, vboGLPtr, bufferStride);

			attributeArrayUsed[attributeShaderLocation] = true;
			attributeDataIndex++;
			catchOpenGLError();
		}
	}
	for (uint32 i = 0; i < GPU_GL_MAX_NUM_ATTRIBUTE; i++)
	{
		if (attributeArrayUsed[i] == false && glAttributeArrayIsEnabled[i] == true)
			SetAttributeArrayState(i, false, -1);
	}
}

void rectsEmulationGS_outputSingleVertex(std::string& gsSrc, LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, sint32 vIdx);
void rectsEmulationGS_outputGeneratedVertex(std::string& gsSrc, LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, const char* variant);
void rectsEmulationGS_outputVerticesCode(std::string& gsSrc, LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, sint32 p0, sint32 p1, sint32 p2, sint32 p3, const char* variant, const LatteContextRegister& latteRegister);

std::map<uint64, RendererShaderGL*> g_mapGLRectEmulationGS;

RendererShaderGL* rectsEmulationGS_generateShaderGL(LatteDecompilerShader* vertexShader)
{
	LatteShaderPSInputTable* psInputTable = LatteSHRC_GetPSInputTable();

	std::string gsSrc;
	gsSrc.append("#version 450\r\n");

	// layout
	gsSrc.append("layout(triangles) in;\r\n");
	gsSrc.append("layout(triangle_strip) out;\r\n");
	gsSrc.append("layout(max_vertices = 4) out;\r\n");

	// gl_PerVertex input

	gsSrc.append("in gl_PerVertex {\r\n");
	gsSrc.append("vec4 gl_Position;\r\n");
	gsSrc.append("} gl_in[];\r\n");

	// gl_PerVertex output

	gsSrc.append("out gl_PerVertex {\r\n");
	gsSrc.append("vec4 gl_Position;\r\n");
	gsSrc.append("};\r\n");

	// inputs & outputs
	auto parameterMask = vertexShader->outputParameterMask;
	for (sint32 f = 0; f < 2; f++)
	{
		for (uint32 i = 0; i < 32; i++)
		{
			if ((parameterMask & (1 << i)) == 0)
				continue;
			sint32 vsSemanticId = psInputTable->getVertexShaderOutParamSemanticId(LatteGPUState.contextRegister, i);
			if (vsSemanticId < 0)
				continue;
			auto psImport = psInputTable->getPSImportBySemanticId(vsSemanticId);
			if (psImport == nullptr)
				continue;

			gsSrc.append(fmt::format("layout(location = {}) ", psInputTable->getPSImportLocationBySemanticId(vsSemanticId)));
			if (psImport->isFlat)
				gsSrc.append("flat ");
			if (psImport->isNoPerspective)
				gsSrc.append("noperspective ");

			if (f == 0)
				gsSrc.append("in");
			else
				gsSrc.append("out");

			if (f == 0)
				gsSrc.append(fmt::format(" vec4 passParameterSem{}In[];\r\n", vsSemanticId));
			else
				gsSrc.append(fmt::format(" vec4 passParameterSem{}Out;\r\n", vsSemanticId));
		}
	}

	// gen function
	gsSrc.append("vec4 gen4thVertexA(vec4 a, vec4 b, vec4 c)\r\n");
	gsSrc.append("{\r\n");
	gsSrc.append("return b - (c - a);\r\n");
	gsSrc.append("}\r\n");

	gsSrc.append("vec4 gen4thVertexB(vec4 a, vec4 b, vec4 c)\r\n");
	gsSrc.append("{\r\n");
	gsSrc.append("return c - (b - a);\r\n");
	gsSrc.append("}\r\n");

	gsSrc.append("vec4 gen4thVertexC(vec4 a, vec4 b, vec4 c)\r\n");
	gsSrc.append("{\r\n");
	gsSrc.append("return c + (b - a);\r\n");
	gsSrc.append("}\r\n");

	// main
	gsSrc.append("void main()\r\n");
	gsSrc.append("{\r\n");

	// there are two possible winding orders that need different triangle generation:
	// 0 1
	// 2 3
	// and
	// 0 1
	// 3 2
	// all others are just symmetries of these cases

	// we can determine the case by comparing the distance 0<->1 and 0<->2

	gsSrc.append("float dist0_1 = length(gl_in[1].gl_Position.xy - gl_in[0].gl_Position.xy);\r\n");
	gsSrc.append("float dist0_2 = length(gl_in[2].gl_Position.xy - gl_in[0].gl_Position.xy);\r\n");
	gsSrc.append("float dist1_2 = length(gl_in[2].gl_Position.xy - gl_in[1].gl_Position.xy);\r\n");

	// emit vertices
	gsSrc.append("if(dist0_1 > dist0_2 && dist0_1 > dist1_2)\r\n");
	gsSrc.append("{\r\n");
	// p0 to p1 is diagonal
	rectsEmulationGS_outputVerticesCode(gsSrc, vertexShader, psInputTable, 2, 1, 0, 3, "A", LatteGPUState.contextNew);
	gsSrc.append("} else if ( dist0_2 > dist0_1 && dist0_2 > dist1_2 ) {\r\n");
	// p0 to p2 is diagonal
	rectsEmulationGS_outputVerticesCode(gsSrc, vertexShader, psInputTable, 1, 2, 0, 3, "B", LatteGPUState.contextNew);
	gsSrc.append("} else {\r\n");
	// p1 to p2 is diagonal
	rectsEmulationGS_outputVerticesCode(gsSrc, vertexShader, psInputTable, 0, 1, 2, 3, "C", LatteGPUState.contextNew);
	gsSrc.append("}\r\n");

	gsSrc.append("}\r\n");

	auto glShader = new RendererShaderGL(RendererShader::ShaderType::kGeometry, 0, 0, false, false, gsSrc);
	glShader->PreponeCompilation(true);

	return glShader;
}

RendererShaderGL* rectsEmulationGS_getShaderGL(LatteDecompilerShader* vertexShader)
{
	LatteShaderPSInputTable* psInputTable = LatteSHRC_GetPSInputTable();

	uint64 h = vertexShader->baseHash + psInputTable->key;

	auto itr = g_mapGLRectEmulationGS.find(h);
	if (itr != g_mapGLRectEmulationGS.end())
		return (*itr).second;

	auto glShader = rectsEmulationGS_generateShaderGL(vertexShader);

	g_mapGLRectEmulationGS.emplace(h, glShader);
	return glShader;
}

uint32 sPrevTextureReadbackDrawcallUpdate = 0;

template<bool TIsMinimal, bool THasProfiling>
void OpenGLRenderer::draw_genericDrawHandler(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType)
{
	ReleaseBufferCacheEntries();

	catchOpenGLError();
	void* indexData = indexDataMPTR != MPTR_NULL ? memory_getPointerFromPhysicalOffset(indexDataMPTR) : NULL;
	auto primitiveMode = LatteGPUState.contextNew.VGT_PRIMITIVE_TYPE.get_PRIMITIVE_MODE();
	// handle special state 8 (clear as depth)
	if (LatteGPUState.contextNew.GetSpecialStateValues()[8] != 0)
	{
		LatteDraw_handleSpecialState8_clearAsDepth();
		LatteGPUState.drawCallCounter++;
		return;
	}

	// update shaders and uniforms
	if constexpr (!TIsMinimal)
	{
		beginPerfMonProfiling(performanceMonitor.gpuTime_dcStageShaderAndUniformMgr);
		LatteSHRC_UpdateActiveShaders();
		LatteDecompilerShader* vs = (LatteDecompilerShader*)LatteSHRC_GetActiveVertexShader();
		LatteDecompilerShader* gs = (LatteDecompilerShader*)LatteSHRC_GetActiveGeometryShader();
		LatteDecompilerShader* ps = (LatteDecompilerShader*)LatteSHRC_GetActivePixelShader();
		if (vs)
			shader_bind(vs->shader);
		else
			shader_unbind(RendererShader::ShaderType::kVertex);
		if (ps && LatteGPUState.contextRegister[mmVGT_STRMOUT_EN] == 0)
			shader_bind(ps->shader);
		else
			shader_unbind(RendererShader::ShaderType::kFragment);
		if (gs)
			shader_bind(gs->shader);
		else
			shader_unbind(RendererShader::ShaderType::kGeometry);
		endPerfMonProfiling(performanceMonitor.gpuTime_dcStageShaderAndUniformMgr);
	}
	if (LatteGPUState.activeShaderHasError)
	{
		debug_printf("Skipped drawcall due to shader error\n");
		return;
	}
	// check for blacklisted shaders
	uint64 vsShaderHash = 0;
	if (LatteSHRC_GetActiveVertexShader())
		vsShaderHash = LatteSHRC_GetActiveVertexShader()->baseHash;
	uint64 psShaderHash = 0;
	if (LatteSHRC_GetActivePixelShader())
		psShaderHash = LatteSHRC_GetActivePixelShader()->baseHash;

	// setup streamout (if enabled)
	bool rasterizerEnable = LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_DX_RASTERIZATION_KILL() == false;
	if (!LatteGPUState.contextNew.PA_CL_VTE_CNTL.get_VPORT_X_OFFSET_ENA())
		rasterizerEnable = true;

	bool streamoutEnable = LatteGPUState.contextRegister[mmVGT_STRMOUT_EN] != 0;
	if (streamoutEnable)
	{
		if (glBeginTransformFeedback == nullptr)
		{
			cemu_assert_debug(false);
			return; // transform feedback not supported
		}
	}
	// skip draw if output is not used
	if (rasterizerEnable == false && streamoutEnable == false)
	{
		// rasterizer and streamout disabled
		LatteGPUState.drawCallCounter++;
		return;
	}
	// get primitive
	if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::TRIANGLES)
		sGLActiveDrawMode = GL_TRIANGLES;
	else if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::TRIANGLE_STRIP)
		sGLActiveDrawMode = GL_TRIANGLE_STRIP;
	else if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::QUADS)
		sGLActiveDrawMode = GL_QUADS;
	else if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::TRIANGLE_FAN)
		sGLActiveDrawMode = GL_TRIANGLE_FAN;
	else if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::RECTS)
		sGLActiveDrawMode = GL_TRIANGLES;
	else if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::POINTS)
		sGLActiveDrawMode = GL_POINTS;
	else if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::LINES)
		sGLActiveDrawMode = GL_LINES;
	else if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::LINE_STRIP)
		sGLActiveDrawMode = GL_LINE_STRIP;
	else if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::LINE_LOOP)
		sGLActiveDrawMode = GL_LINE_LOOP;
	else if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::QUAD_STRIP)
		sGLActiveDrawMode = GL_QUAD_STRIP;
	else
	{
		cemu_assert_debug(false); // unsupported primitive type
		LatteGPUState.drawCallCounter++;
		return;
	}

	if constexpr (!TIsMinimal)
	{
		// update render targets and textures
		LatteGPUState.requiresTextureBarrier = false;
		beginPerfMonProfiling(performanceMonitor.gpuTime_dcStageTextures);
		while (true)
		{
			LatteGPUState.repeatTextureInitialization = false;
			if (streamoutEnable == false)
			{
				// only handle rendertargets if streamout is inactive
				if (LatteMRT::UpdateCurrentFBO() == false)
					return; // no render target
				if (hasValidFramebufferAttached == false)
					return;
			}
			LatteTexture_updateTextures(); // caution: Do not call any functions that potentially modify texture bindings after this line
			if (LatteGPUState.repeatTextureInitialization == false)
				break;
			catchOpenGLError();
		}
		endPerfMonProfiling(performanceMonitor.gpuTime_dcStageTextures);

		beginPerfMonProfiling(performanceMonitor.gpuTime_dcStageMRT);
		LatteMRT::ApplyCurrentState();
		endPerfMonProfiling(performanceMonitor.gpuTime_dcStageMRT);

		// texture barrier for write-read patterns
		if (LatteGPUState.requiresTextureBarrier && glTextureBarrier)
		{
			glTextureBarrier();
		}

	}

	catchOpenGLError();
	// handle RECT primitive
	if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::RECTS)
	{
		RendererShaderGL* rectsEmulationShader = rectsEmulationGS_getShaderGL(LatteSHRC_GetActiveVertexShader());
		shader_bind(rectsEmulationShader);
	}

	// prepare index cache
	beginPerfMonProfiling(performanceMonitor.gpuTime_dcStageIndexMgr);
	LatteDrawGL_prepareIndicesWithGPUCache(indexDataMPTR, indexType, count, (sint32)primitiveMode);
	endPerfMonProfiling(performanceMonitor.gpuTime_dcStageIndexMgr);

	// synchronize vertex and uniform buffers
	LatteBufferCache_Sync(indexState.minIndex + baseVertex, indexState.maxIndex + baseVertex, baseInstance, instanceCount);

	_setupVertexAttributes();

	// update renderstate
	LatteRenderTarget_updateViewport();
	LatteRenderTarget_updateScissorBox();
	renderstate_updateBlendingAndColorControl();
	catchOpenGLError();
	// handle special state 5 (convert depth to color)
	if (LatteGPUState.contextNew.GetSpecialStateValues()[5] != 0)
	{
		debug_printf("GPU7 special state 5 used\n");
		LatteTextureView* rt_color = LatteMRT::GetColorAttachment(0);
		LatteTextureView* rt_depth = LatteMRT::GetDepthAttachment();
		if (!rt_depth || !rt_color)
		{
			cemuLog_log(LogType::Force, "GPU7 special state 5 used but render target not setup correctly");
			return;
		}
		surfaceCopy_copySurfaceWithFormatConversion(rt_depth->baseTexture, rt_depth->firstMip, rt_depth->firstSlice, rt_color->baseTexture, rt_color->firstMip, rt_color->firstSlice, rt_depth->baseTexture->width, rt_depth->baseTexture->height);
		LatteGPUState.drawCallCounter++;
		return;
	}

	beginPerfMonProfiling(performanceMonitor.gpuTime_dcStageShaderAndUniformMgr);
	// update uniform values
	uniformData_update();
	endPerfMonProfiling(performanceMonitor.gpuTime_dcStageShaderAndUniformMgr);
	catchOpenGLError();
	// upload special uniforms
	LatteDecompilerShader* vertexShader = LatteSHRC_GetActiveVertexShader();
	LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
	LatteDecompilerShader* geometryShader = LatteSHRC_GetActiveGeometryShader();
	if (vertexShader)
	{
		auto vertexShaderGL = (RendererShaderGL*)vertexShader->shader;
		if (vertexShader->uniform.loc_windowSpaceToClipSpaceTransform >= 0)
		{
			sint32 viewportWidth;
			sint32 viewportHeight;
			LatteRenderTarget_GetCurrentVirtualViewportSize(&viewportWidth, &viewportHeight); // always call after _updateViewport()
			float t[2];
			t[0] = 2.0f / (float)viewportWidth;
			t[1] = 2.0f / (float)viewportHeight;
			glProgramUniform2fv(vertexShaderGL->GetProgram(), vertexShader->uniform.loc_windowSpaceToClipSpaceTransform, 1, t);
		}
		// update uf_texRescaleFactors
		for (auto& entry : vertexShader->uniform.list_ufTexRescale)
		{
			float* xyScale = LatteTexture_getEffectiveTextureScale(LatteConst::ShaderType::Vertex, entry.texUnit);
			if (memcmp(entry.currentValue, xyScale, sizeof(float) * 2) == 0)
				continue; // value unchanged
			memcpy(entry.currentValue, xyScale, sizeof(float) * 2);
			glProgramUniform2fv(vertexShaderGL->GetProgram(), entry.uniformLocation, 1, xyScale);
		}
		// update uf_pointSize
		if (vertexShader->uniform.loc_pointSize >= 0)
		{
			float t[1];
			float pointWidth = (float)LatteGPUState.contextNew.PA_SU_POINT_SIZE.get_WIDTH() / 8.0f;
			if (pointWidth == 0.0f)
				pointWidth = 1.0f / 8.0f; // minimum size
			t[0] = pointWidth;
			glProgramUniform1fv(vertexShaderGL->GetProgram(), vertexShader->uniform.loc_pointSize, 1, t);
		}
	}
	if (geometryShader)
	{
		auto geometryShaderGL = (RendererShaderGL*)geometryShader->shader;
		// update uf_texRescaleFactors
		for (auto& entry : geometryShader->uniform.list_ufTexRescale)
		{
			float* xyScale = LatteTexture_getEffectiveTextureScale(LatteConst::ShaderType::Geometry, entry.texUnit);
			if (memcmp(entry.currentValue, xyScale, sizeof(float) * 2) == 0)
				continue; // value unchanged
			memcpy(entry.currentValue, xyScale, sizeof(float) * 2);
			glProgramUniform2fv(geometryShaderGL->GetProgram(), entry.uniformLocation, 1, xyScale);
		}
		// update uf_pointSize
		if (geometryShader->uniform.loc_pointSize >= 0)
		{
			float t[1];
			float pointWidth = (float)LatteGPUState.contextNew.PA_SU_POINT_SIZE.get_WIDTH() / 8.0f;
			if (pointWidth == 0.0f)
				pointWidth = 1.0f / 8.0f; // minimum size
			t[0] = pointWidth;
			glProgramUniform1fv(geometryShaderGL->GetProgram(), geometryShader->uniform.loc_pointSize, 1, t);
		}
	}
	if (pixelShader)
	{
		auto pixelShaderGL = (RendererShaderGL*)pixelShader->shader;
		if (pixelShader->uniform.loc_alphaTestRef >= 0)
		{
			float t[1];
			t[0] = LatteGPUState.contextNew.SX_ALPHA_REF.get_ALPHA_TEST_REF();
			if (pixelShader->uniform.ufCurrentValueAlphaTestRef != t[0])
			{
				glProgramUniform1fv(pixelShaderGL->GetProgram(), pixelShader->uniform.loc_alphaTestRef, 1, t);
				pixelShader->uniform.ufCurrentValueAlphaTestRef = t[0];
			}
		}
		// update uf_fragCoordScale
		if (pixelShader->uniform.loc_fragCoordScale >= 0)
		{
			float coordScale[4];
			LatteMRT::GetCurrentFragCoordScale(coordScale);
			if (pixelShader->uniform.ufCurrentValueFragCoordScale[0] != coordScale[0] || pixelShader->uniform.ufCurrentValueFragCoordScale[1] != coordScale[1])
			{
				glProgramUniform2fv(pixelShaderGL->GetProgram(), pixelShader->uniform.loc_fragCoordScale, 1, coordScale);
				pixelShader->uniform.ufCurrentValueFragCoordScale[0] = coordScale[0];
				pixelShader->uniform.ufCurrentValueFragCoordScale[1] = coordScale[1];
			}
		}
		// update uf_texRescaleFactors
		for (auto& entry : pixelShader->uniform.list_ufTexRescale)
		{
			float* xyScale = LatteTexture_getEffectiveTextureScale(LatteConst::ShaderType::Pixel, entry.texUnit);
			if (memcmp(entry.currentValue, xyScale, sizeof(float) * 2) == 0)
				continue; // value unchanged
			memcpy(entry.currentValue, xyScale, sizeof(float) * 2);
			glProgramUniform2fv(pixelShaderGL->GetProgram(), entry.uniformLocation, 1, xyScale);
		}
	}
	catchOpenGLError();
	// prepare streamout
	LatteStreamout_PrepareDrawcall(count, instanceCount);

	if (streamoutEnable && rasterizerEnable)
	{
		if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::QUADS)
			sGLActiveDrawMode = GL_POINTS;
	}
	catchOpenGLError();
	// render
	beginPerfMonProfiling(performanceMonitor.gpuTime_dcStageDrawcallAPI);
	LatteDrawGL_doDraw(indexType, baseVertex, baseInstance, instanceCount, count);
	endPerfMonProfiling(performanceMonitor.gpuTime_dcStageDrawcallAPI);
	// post-drawcall logic
	if(pixelShader)
		LatteRenderTarget_trackUpdates();
	LatteStreamout_FinishDrawcall(false);
	catchOpenGLError();

	if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::RECTS)
		shader_unbind(RendererShader::ShaderType::kGeometry);

	LatteGPUState.drawCallCounter++;
	if (streamoutEnable && rasterizerEnable)
	{
		// streamout and rasterizer enabled, repeat drawcall with streamout disabled
		uint32 strmOutEnOrg = LatteGPUState.contextRegister[mmVGT_STRMOUT_EN];
		LatteGPUState.contextRegister[mmVGT_STRMOUT_EN] = 0;
		draw_genericDrawHandler<false, THasProfiling>(baseVertex, baseInstance, instanceCount, count, indexDataMPTR, indexType);
		LatteGPUState.contextRegister[mmVGT_STRMOUT_EN] = strmOutEnOrg;
		return;
	}
	LatteTextureReadback_Update();
	uint32 dcSinceLastReadbackCheck = LatteGPUState.drawCallCounter - sPrevTextureReadbackDrawcallUpdate;
	if (dcSinceLastReadbackCheck >= 150)
	{
		LatteTextureReadback_UpdateFinishedTransfers(false);
		sPrevTextureReadbackDrawcallUpdate = LatteGPUState.drawCallCounter;
	}	
	catchOpenGLError();
}

void OpenGLRenderer::draw_beginSequence()
{
	// no-op
}

void OpenGLRenderer::draw_execute(uint32 baseVertex, uint32 baseInstance, uint32 instanceCount, uint32 count, MPTR indexDataMPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE indexType, bool isFirst)
{
	bool isMinimal = !isFirst;
    if (isMinimal)
        draw_genericDrawHandler<true, false>(baseVertex, baseInstance, instanceCount, count, indexDataMPTR, indexType);
    else
        draw_genericDrawHandler<false, false>(baseVertex, baseInstance, instanceCount, count, indexDataMPTR, indexType);
}

void OpenGLRenderer::draw_endSequence()
{
	// no-op
}

#define GPU7_INDEX_BUFFER_CACHE_SIZE_DEPR		(18*1024*1024) // 18MB

void OpenGLRenderer::draw_init()
{
	if (indexState.initialized)
		return;
	indexState.initialized = true;
	// create index buffer
	glGenBuffers(1, &indexState.glIndexCacheBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexState.glIndexCacheBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, GPU7_INDEX_BUFFER_CACHE_SIZE_DEPR, NULL, GL_DYNAMIC_DRAW);
#if BOOST_OS_WINDOWS
	indexState.mappedIndexBuffer = (uint8*)_aligned_malloc(GPU7_INDEX_BUFFER_CACHE_SIZE_DEPR, 256);
#else
	indexState.mappedIndexBuffer = (uint8*)aligned_alloc(256, GPU7_INDEX_BUFFER_CACHE_SIZE_DEPR);
#endif
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	indexState.indexRingBuffer = LatteRingBuffer_create(indexState.mappedIndexBuffer, GPU7_INDEX_BUFFER_CACHE_SIZE_DEPR);
	indexState.tempIndexStorage = (uint8*)malloc(1024 * 1024 * 8);
	// create virtual heap for index buffer
	indexState.indexBufferVirtualHeap = virtualBufferHeap_create(GPU7_INDEX_BUFFER_CACHE_SIZE_DEPR);
}

void OpenGLRenderer::bufferCache_upload(uint8* buffer, sint32 size, uint32 bufferOffset)
{
	attributeStream_bindVertexCacheBuffer();
	glBufferSubData(GL_ARRAY_BUFFER, bufferOffset, size, buffer);
}

void OpenGLRenderer::bufferCache_copy(uint32 srcOffset, uint32 dstOffset, uint32 size)
{
	attributeStream_bindVertexCacheBuffer();
	glCopyBufferSubData(GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, srcOffset, dstOffset, size);
}


GLint glClampTable[] =
{
	GL_REPEAT,
	GL_MIRRORED_REPEAT,
	GL_CLAMP_TO_EDGE,
	GL_MIRROR_CLAMP_TO_EDGE,
	GL_CLAMP_TO_EDGE,
	GL_MIRROR_CLAMP_TO_BORDER_EXT,
	GL_CLAMP_TO_BORDER,
	GL_MIRROR_CLAMP_TO_BORDER_EXT
};

GLint glCompSelTable[8] =
{
	GL_RED,
	GL_GREEN,
	GL_BLUE,
	GL_ALPHA,
	GL_ZERO,
	GL_ONE,
	0,
	0
};

GLint glDepthCompareTable[8] = {
	GL_NEVER,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS
};

// Remaps component selection if the underlying OpenGL texture format would behave differently than it's GPU7 counterpart
uint32 _correctTextureCompSelGL(Latte::E_GX2SURFFMT format, uint32 compSel)
{
	switch (format)
	{
	case Latte::E_GX2SURFFMT::R8_UNORM: // R8 is replicated on all channels (while OpenGL would return 1.0 for BGA instead)
	case Latte::E_GX2SURFFMT::R8_SNORM: // probably the same as _UNORM, but needs testing
		if (compSel >= 1 && compSel <= 3)
			compSel = 0;
		break;
	case Latte::E_GX2SURFFMT::A1_B5_G5_R5_UNORM: // order of components is reversed (RGBA -> ABGR)
		if (compSel >= 0 && compSel <= 3)
			compSel = 3 - compSel;
		break;
	case Latte::E_GX2SURFFMT::BC4_UNORM:
	case Latte::E_GX2SURFFMT::BC4_SNORM:
		if (compSel >= 1 && compSel <= 3)
			compSel = 0;
		break;
	case Latte::E_GX2SURFFMT::BC5_UNORM:
	case Latte::E_GX2SURFFMT::BC5_SNORM:
		// RG maps to RG
		// B maps to ?
		// A maps to G (guessed)
		if (compSel == 3)
			compSel = 1; // read Alpha as Green
		break;
	case Latte::E_GX2SURFFMT::A2_B10_G10_R10_UNORM:
		// reverse components (Wii U: ABGR, OpenGL: RGBA)
		// used in Resident Evil Revelations
		if (compSel >= 0 && compSel <= 3)
			compSel = 3 - compSel;
		break;
	case Latte::E_GX2SURFFMT::X24_G8_UINT:
		// map everything to alpha?
		if (compSel >= 0 && compSel <= 3)
			compSel = 3;
		break;
	default:
		break;
	}
	return compSel;
}

#define quickBindTexture() 		if( textureIsActive == false ) { texture_bindAndActivate(hostTextureView, hostTextureUnit); textureIsActive = true; }

uint32 _getGLMinFilter(Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER filterMin, Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_Z_FILTER filterMip)
{
	bool isMinPointFilter = (filterMin == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::POINT) || (filterMin == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::ANISO_POINT);

	if (filterMip == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_Z_FILTER::NONE)
	{
		// no mip
		return isMinPointFilter ? GL_NEAREST : GL_LINEAR;
	}
	else if (filterMip == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_Z_FILTER::POINT)
	{
		// nearest neighbor
		return isMinPointFilter ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST;
	}
	// else -> filterMip == LINEAR
	return isMinPointFilter ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
}

/*
* Update channel swizzling and other texture settings for a texture unit
* hostTextureView is the texture unit view used on the host side
*/
void OpenGLRenderer::renderstate_updateTextureSettingsGL(LatteDecompilerShader* shaderContext, LatteTextureView* _hostTextureView, uint32 hostTextureUnit, const Latte::LATTE_SQ_TEX_RESOURCE_WORD4_N texUnitWord4, uint32 texUnitIndex, bool isDepthSampler)
{
	auto hostTextureView = (LatteTextureViewGL*)_hostTextureView;

	LatteTexture* baseTexture = hostTextureView->baseTexture;
	// get texture register word 0
	catchOpenGLError();
	bool textureIsActive = false;

	uint32 compSelR = (uint32)texUnitWord4.get_DST_SEL_X();
	uint32 compSelG = (uint32)texUnitWord4.get_DST_SEL_Y();
	uint32 compSelB = (uint32)texUnitWord4.get_DST_SEL_Z();
	uint32 compSelA = (uint32)texUnitWord4.get_DST_SEL_W();
	// on OpenGL some channels might be mapped differently
	compSelR = _correctTextureCompSelGL(hostTextureView->format, compSelR);
	compSelG = _correctTextureCompSelGL(hostTextureView->format, compSelG);
	compSelB = _correctTextureCompSelGL(hostTextureView->format, compSelB);
	compSelA = _correctTextureCompSelGL(hostTextureView->format, compSelA);
	// update swizzle parameters
	if (hostTextureView->swizzleR != compSelR)
	{
		quickBindTexture();
		glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_SWIZZLE_R, glCompSelTable[compSelR]);
		hostTextureView->swizzleR = compSelR;
	}
	if (hostTextureView->swizzleG != compSelG)
	{
		quickBindTexture();
		glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_SWIZZLE_G, glCompSelTable[compSelG]);
		hostTextureView->swizzleG = compSelG;
	}
	if (hostTextureView->swizzleB != compSelB)
	{
		quickBindTexture();
		glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_SWIZZLE_B, glCompSelTable[compSelB]);
		hostTextureView->swizzleB = compSelB;
	}
	if (hostTextureView->swizzleA != compSelA)
	{
		quickBindTexture();
		glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_SWIZZLE_A, glCompSelTable[compSelA]);
		hostTextureView->swizzleA = compSelA;
	}
	catchOpenGLError();

	uint32 stageSamplerIndex = shaderContext->textureUnitSamplerAssignment[texUnitIndex];
	if (stageSamplerIndex != LATTE_DECOMPILER_SAMPLER_NONE)
	{
		uint32 samplerIndex = stageSamplerIndex;
		samplerIndex += LatteDecompiler_getTextureSamplerBaseIndex(shaderContext->shaderType);

		const _LatteRegisterSetSampler* samplerWords = LatteGPUState.contextNew.SQ_TEX_SAMPLER + samplerIndex;

		auto filterMag = samplerWords->WORD0.get_XY_MAG_FILTER();
		auto filterMin = samplerWords->WORD0.get_XY_MAG_FILTER();
		//auto filterZ = samplerWords->WORD0.get_Z_FILTER();
		auto filterMip = samplerWords->WORD0.get_MIP_FILTER();

		// get OpenGL constant for min filter
		uint32 filterMinGL = _getGLMinFilter(filterMin, filterMip);
		uint32 filterMagGL = (filterMag == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::POINT || filterMag == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_XY_FILTER::ANISO_POINT) ? GL_NEAREST : GL_LINEAR;

		// todo: z-filter is customizable for GPU7 but OpenGL doesn't offer the same functionality?

		LatteSamplerState* samplerState = &hostTextureView->samplerState;

		catchOpenGLError();

		uint32 clampX = (uint32)samplerWords->WORD0.get_CLAMP_X();
		uint32 clampY = (uint32)samplerWords->WORD0.get_CLAMP_Y();
		uint32 clampZ = (uint32)samplerWords->WORD0.get_CLAMP_Z();
		if (samplerState->clampS != clampX)
		{
			quickBindTexture();
			glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_WRAP_S, glClampTable[clampX]);
			samplerState->clampS = clampX;
		}
		if (samplerState->clampT != clampY)
		{
			quickBindTexture();
			glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_WRAP_T, glClampTable[clampY]);
			samplerState->clampT = clampY;
		}
		if (samplerState->clampR != clampZ)
		{
			quickBindTexture();
			glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_WRAP_R, glClampTable[clampZ]);
			samplerState->clampR = clampZ;
		}
		catchOpenGLError();

		uint32 maxAniso = (uint32)samplerWords->WORD0.get_MAX_ANISO_RATIO();

		if (baseTexture->overwriteInfo.anisotropicLevel >= 0)
			maxAniso = baseTexture->overwriteInfo.anisotropicLevel;

		if (samplerState->maxAniso != maxAniso)
		{
			quickBindTexture();
			glTexParameterf(hostTextureView->glTexTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)(1 << maxAniso));
			samplerState->maxAniso = maxAniso;
			catchOpenGLError();
		}
		if (samplerState->filterMin != filterMinGL)
		{
			quickBindTexture();
			glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_MIN_FILTER, filterMinGL);
			samplerState->filterMin = filterMinGL;
			catchOpenGLError();
		}

		if (samplerState->filterMag != filterMagGL)
		{
			quickBindTexture();
			glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_MAG_FILTER, filterMagGL);
			samplerState->filterMag = filterMagGL;
			catchOpenGLError();
		}
		if (samplerState->maxMipLevels != hostTextureView->numMip)
		{
			quickBindTexture();
			glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_MAX_LEVEL, std::max(hostTextureView->numMip, 1) - 1);
			samplerState->maxMipLevels = hostTextureView->numMip;
			catchOpenGLError();
		}
		// lod
		uint32 iMinLOD = samplerWords->WORD1.get_MIN_LOD();
		uint32 iMaxLOD = samplerWords->WORD1.get_MAX_LOD();
		sint32 iLodBias = samplerWords->WORD1.get_LOD_BIAS();

		// apply relative lod bias from graphic pack
		if (baseTexture->overwriteInfo.hasRelativeLodBias)
		{
			iLodBias += baseTexture->overwriteInfo.relativeLodBias;
		}
		// apply absolute lod bias from graphic pack
		if (baseTexture->overwriteInfo.hasLodBias)
		{
			iLodBias = baseTexture->overwriteInfo.lodBias;
		}

		if (samplerState->minLod != iMinLOD)
		{
			quickBindTexture();
			glTexParameterf(hostTextureView->glTexTarget, GL_TEXTURE_MIN_LOD, (float)iMinLOD / 64.0f);
			samplerState->minLod = iMinLOD;
		}
		if (samplerState->maxLod != iMaxLOD)
		{
			quickBindTexture();
			glTexParameterf(hostTextureView->glTexTarget, GL_TEXTURE_MAX_LOD, (float)iMaxLOD / 64.0f);
			samplerState->maxLod = iMaxLOD;
		}
		if (samplerState->lodBias != iLodBias)
		{
			quickBindTexture();
			glTexParameterf(hostTextureView->glTexTarget, GL_TEXTURE_LOD_BIAS, (float)iLodBias / 64.0f);
			samplerState->lodBias = iLodBias;
		}
		// depth compare
		uint32 samplerDepthCompare = (uint32)samplerWords->WORD0.get_DEPTH_COMPARE_FUNCTION();
		uint8 depthCompareMode = isDepthSampler ? 1 : 0;

		if (samplerDepthCompare != samplerState->depthCompareFunc)
		{
			quickBindTexture();
			glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_COMPARE_FUNC, glDepthCompareTable[samplerDepthCompare]);
			samplerState->depthCompareFunc = samplerDepthCompare;
		}

		if (depthCompareMode != samplerState->depthCompareMode)
		{
			quickBindTexture();
			if (depthCompareMode != 0)
				glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			else
				glTexParameteri(hostTextureView->glTexTarget, GL_TEXTURE_COMPARE_MODE, GL_NONE);
			samplerState->depthCompareMode = depthCompareMode;
		}

		catchOpenGLError();
		// border
		auto borderType = samplerWords->WORD0.get_BORDER_COLOR_TYPE();
		if (samplerState->borderType != (uint8)borderType || borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::REGISTER)
		{
			// todo: Should we use integer border color (glTexParameteriv) for integer texture formats?
			GLfloat borderColor[4];
			if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::TRANSPARENT_BLACK)
			{
				borderColor[0] = 0.0f;
				borderColor[1] = 0.0f;
				borderColor[2] = 0.0f;
				borderColor[3] = 0.0f;
			}
			else if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::OPAQUE_BLACK)
			{
				borderColor[0] = 0.0f;
				borderColor[1] = 0.0f;
				borderColor[2] = 0.0f;
				borderColor[3] = 1.0f;
			}
			else if (borderType == Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_BORDER_COLOR_TYPE::OPAQUE_WHITE)
			{
				borderColor[0] = 1.0f;
				borderColor[1] = 1.0f;
				borderColor[2] = 1.0f;
				borderColor[3] = 1.0f;
			}
			else
			{
				// border color from register
				_LatteRegisterSetSamplerBorderColor* borderColorReg;
				if (shaderContext->shaderType == LatteConst::ShaderType::Vertex || shaderContext->shaderType == LatteConst::ShaderType::Compute)
					borderColorReg = LatteGPUState.contextNew.TD_VS_SAMPLER_BORDER_COLOR + stageSamplerIndex;
				else if (shaderContext->shaderType == LatteConst::ShaderType::Pixel)
					borderColorReg = LatteGPUState.contextNew.TD_PS_SAMPLER_BORDER_COLOR + stageSamplerIndex;
				else // geometry
					borderColorReg = LatteGPUState.contextNew.TD_GS_SAMPLER_BORDER_COLOR + stageSamplerIndex;

				borderColor[0] = borderColorReg->red.get_channelValue();
				borderColor[1] = borderColorReg->green.get_channelValue();
				borderColor[2] = borderColorReg->blue.get_channelValue();
				borderColor[3] = borderColorReg->alpha.get_channelValue();
			}

			if (samplerState->borderColor[0] != borderColor[0] || samplerState->borderColor[1] != borderColor[1] || samplerState->borderColor[2] != borderColor[2] || samplerState->borderColor[3] != borderColor[3])
			{
				quickBindTexture();
				glTexParameterfv(hostTextureView->glTexTarget, GL_TEXTURE_BORDER_COLOR, borderColor);
				samplerState->borderColor[0] = borderColor[0];
				samplerState->borderColor[1] = borderColor[1];
				samplerState->borderColor[2] = borderColor[2];
				samplerState->borderColor[3] = borderColor[3];
			}
			samplerState->borderType = (uint8)borderType;
		}
		catchOpenGLError();
	}
}
