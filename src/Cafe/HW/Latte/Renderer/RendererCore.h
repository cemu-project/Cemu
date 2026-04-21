#pragma once

#include "Cafe/HW/Latte/Core/LatteRingBuffer.h"
#include "Cafe/HW/Latte/Core/Latte.h"

using _INDEX_TYPE = Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE;

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

typedef struct
{
	indexDataCacheEntry_t indexCacheEntry[INDEX_CACHE_ENTRIES];
	sint32 nextCacheEntryIndex;
	// info about currently used index data
	uint32 maxIndex;
	uint32 minIndex;
	uint8* indexData;
	// buffer 
	unsigned int glIndexCacheBuffer;
	VirtualBufferHeap_t* indexBufferVirtualHeap;
	uint8* mappedIndexBuffer;
	LatteRingBuffer_t* indexRingBuffer;
	uint8* tempIndexStorage;
	// misc
	bool initialized;
	unsigned int glActiveElementArrayBuffer;
}indexState_t;

typedef struct
{
	uint8* vboOutput;
	uint32 vboStride;
	uint8 dataFormat;
	uint8 nfa;
	bool isSigned;
} AttributePointerCacheEntry_t;

#define INDEX_DATA_CACHE_BUCKETS		(1783)

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

void RendererCore_resetAttributePointerCache();
bool RendererCore_checkIfAttributePointerCacheChanged(uint32 attributeShaderLoc, uint8* vboOutput, uint32 vboStride, uint8 dataFormat, uint8 nfa, bool isSigned);

indexState_t* RendererCore_getIndexState();
indexDataCacheEntry2_t** RendererCore_getIndexDataCacheFirst();
indexDataCacheEntry2_t** RendererCore_getIndexDataCacheBucket(uint32 bucketIdx);
sint32* RendererCore_getIndexDataCacheEntryCount();

void RendererCore_appendToUsageLinkedList(indexDataCacheEntry2_t* entry);
void RendererCore_removeFromUsageLinkedList(indexDataCacheEntry2_t* entry);
void RendererCore_removeFromBucket(indexDataCacheEntry2_t* entry);
void LatteDraw_cleanupAfterFrame();
void LatteDraw_handleSpecialState8_clearAsDepth();