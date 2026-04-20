#include "CommonRendererCore.h"

indexState_t indexState = { 0 };
AttributePointerCacheEntry_t activeAttributePointer[LATTE_VS_ATTRIBUTE_LIMIT] = { 0 };

indexDataCacheEntry2_t* indexDataCacheBucket[INDEX_DATA_CACHE_BUCKETS] = { 0 };
indexDataCacheEntry2_t* indexDataCacheFirst = nullptr; // points to least recently used item
indexDataCacheEntry2_t* indexDataCacheLast = nullptr; // points to most recently used item
sint32 indexDataCacheEntryCount = 0;

uint8* CommonRenderer_getAttributePointerCacheVboOutput(uint32 attributeShaderLoc)
{
	return activeAttributePointer[attributeShaderLoc].vboOutput;
}

void CommonRenderer_setAttributePointerCacheVboOutput(uint32 attributeShaderLoc, uint8* vboOutput)
{
	activeAttributePointer[attributeShaderLoc].vboOutput = vboOutput;
}

uint32 CommonRenderer_getAttributePointerCacheVboStride(uint32 attributeShaderLoc)
{
	return activeAttributePointer[attributeShaderLoc].vboStride;
}

void CommonRenderer_setAttributePointerCacheVboStride(uint32 attributeShaderLoc, uint32 vboStride)
{
	activeAttributePointer[attributeShaderLoc].vboStride = vboStride;
}

uint8 CommonRenderer_getAttributePointerCacheDataFormat(uint32 attributeShaderLoc)
{
	return activeAttributePointer[attributeShaderLoc].dataFormat;
}

void CommonRenderer_setAttributePointerCacheDataFormat(uint32 attributeShaderLoc, uint8 dataFormat)
{
	activeAttributePointer[attributeShaderLoc].dataFormat = dataFormat;
}

uint8 CommonRenderer_getAttributePointerCacheNfa(uint32 attributeShaderLoc)
{
	return activeAttributePointer[attributeShaderLoc].nfa;
}

void CommonRenderer_setAttributePointerCacheNfa(uint32 attributeShaderLoc, uint8 nfa)
{
	activeAttributePointer[attributeShaderLoc].nfa = nfa;
}

bool CommonRenderer_getAttributePointerCacheIsSigned(uint32 attributeShaderLoc)
{
	return activeAttributePointer[attributeShaderLoc].isSigned;
}

void CommonRenderer_setAttributePointerCacheIsSigned(uint32 attributeShaderLoc, bool isSigned)
{
	activeAttributePointer[attributeShaderLoc].isSigned = isSigned;
}

indexState_t* CommonRenderer_getIndexState()
{
	return &indexState;
}

indexDataCacheEntry2_t** CommonRenderer_getIndexDataCacheFirst()
{
	return &indexDataCacheFirst;
}

indexDataCacheEntry2_t** CommonRenderer_getIndexDataCacheBucket(uint32 bucketIdx)
{
	return &indexDataCacheBucket[bucketIdx];
}

sint32* CommonRenderer_getIndexDataCacheEntryCount()
{
	return &indexDataCacheEntryCount;
}

void CommonRenderer_appendToUsageLinkedList(indexDataCacheEntry2_t* entry)
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

void CommonRenderer_removeFromUsageLinkedList(indexDataCacheEntry2_t* entry)
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

void CommonRenderer_removeFromBucket(indexDataCacheEntry2_t* entry)
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

void CommonRenderer_cleanupAfterFrame()
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
		CommonRenderer_removeFromUsageLinkedList(entry);
		CommonRenderer_removeFromBucket(entry);
		free(entry);
	}
}