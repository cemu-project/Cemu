#include "CommonRendererCore.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "config/CemuConfig.h"

#ifdef ENABLE_OPENGL
#include "Common/GLInclude/GLInclude.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureViewGL.h"
#endif

indexState_t indexState = { 0 };
AttributePointerCacheEntry_t activeAttributePointer[LATTE_VS_ATTRIBUTE_LIMIT] = { 0 };

indexDataCacheEntry2_t* indexDataCacheBucket[INDEX_DATA_CACHE_BUCKETS] = { 0 };
indexDataCacheEntry2_t* indexDataCacheFirst = nullptr; // points to least recently used item
indexDataCacheEntry2_t* indexDataCacheLast = nullptr; // points to most recently used item
sint32 indexDataCacheEntryCount = 0;

void CommonRenderer_resetAttributePointerCache()
{
	for (sint32 i = 0; i < LATTE_VS_ATTRIBUTE_LIMIT; i++)
	{
		activeAttributePointer[i].vboOutput = (uint8*)-1;
		activeAttributePointer[i].vboStride = (uint32)-1;
	}
}

bool CommonRenderer_checkIfAttributePointerCacheChanged(uint32 attributeShaderLoc, uint8* vboOutput, uint32 vboStride, uint8 dataFormat, uint8 nfa, bool isSigned)
{
	// don't call glVertexAttribPointer if parameters have not changed
	if (
		activeAttributePointer[attributeShaderLoc].vboOutput == vboOutput &&
		activeAttributePointer[attributeShaderLoc].vboStride == vboStride &&
		activeAttributePointer[attributeShaderLoc].dataFormat == dataFormat &&
		activeAttributePointer[attributeShaderLoc].nfa == nfa &&
		activeAttributePointer[attributeShaderLoc].isSigned == isSigned
	)
	{
		return false;
	}
	activeAttributePointer[attributeShaderLoc].vboOutput = vboOutput;
	activeAttributePointer[attributeShaderLoc].vboStride = vboStride;
	activeAttributePointer[attributeShaderLoc].dataFormat = dataFormat;
	activeAttributePointer[attributeShaderLoc].nfa = nfa;
	activeAttributePointer[attributeShaderLoc].isSigned = isSigned;

	return true;
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
		CommonRenderer_removeFromUsageLinkedList(entry);
		CommonRenderer_removeFromBucket(entry);
		free(entry);
	}
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

		switch (g_renderer->GetType())
		{
#ifdef ENABLE_OPENGL
		case RendererAPI::OpenGL:
		{
			//cemu_assert_debug(false); // implement g_renderer->texture_clearColorSlice properly for OpenGL renderer
			if (glClearTexSubImage)
				glClearTexSubImage(((LatteTextureViewGL*)view)->glTexId, mipIndex, 0, 0, 0, effectiveClearWidth, effectiveClearHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE, clearColor);
		}
#endif
		default:
		{
			if (view->baseTexture->isDepth)
				g_renderer->texture_clearDepthSlice(view->baseTexture, sliceIndex + view->firstSlice, mipIndex + view->firstMip, true, view->baseTexture->hasStencil, 0.0f, 0);
			else
				g_renderer->texture_clearColorSlice(view->baseTexture, sliceIndex + view->firstSlice, mipIndex + view->firstMip, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		}
		}
	}
}