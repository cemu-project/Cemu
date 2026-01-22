#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Common/cpu_features.h"

std::unordered_set<LatteTexture*> g_allTextures;

void LatteTC_Init()
{
	cemu_assert_debug(g_allTextures.empty());
}

void LatteTC_RegisterTexture(LatteTexture* tex)
{
	g_allTextures.emplace(tex);
}

void LatteTC_UnregisterTexture(LatteTexture* tex)
{
	g_allTextures.erase(tex);
}

// sample few uint64s uniformly over memory range
uint32 _quickStochasticHash(void* texData, uint32 memRange)
{
	uint64* texDataU64 = (uint64*)texData;

	uint64 hashVal = 0;
	memRange /= sizeof(uint64);

	uint32 memStep = memRange / 37; // use prime here to avoid memStep aligning nicely with pitch of texture, leading to sampling only along the border of a texture
	for (sint32 i = 0; i < 37; i++)
	{
		hashVal += *texDataU64;
		hashVal = (hashVal << 3) | (hashVal >> 61);
		texDataU64 += memStep;
	}
	return (uint32)hashVal ^ (uint32)(hashVal >> 32);
}

uint32 LatteTexture_CalculateTextureDataHash(LatteTexture* hostTexture)
{
	if( hostTexture->texDataPtrHigh == hostTexture->texDataPtrLow )
	{
		return 0;
	}
	if (hostTexture->format == Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT)
	{
		// this is an exotic format that usually isn't generated or updated CPU-side
		// therefore as an optimization we can risk to only check a minimal amount of bytes at the beginning of the texture data
		// updates which change the entire texture should still be detected this way
		// this also helps with a bug in BotW which seems to fill the empty areas of the textures with other data which causes unnecessary invalidations and texture reloads

		// Wonderful 101 generates this format in a 8x8x8 3D texture using tiling aperture
		if (hostTexture->tileMode == Latte::E_HWTILEMODE::TM_1D_TILED_THICK && hostTexture->depth == 8 && hostTexture->width == 8 && hostTexture->height == 8)
		{
			// special case for Wonderful 101
			uint32* texDataU32 = (uint32*)memory_getPointerFromPhysicalOffset(hostTexture->texDataPtrLow);
			return texDataU32[0] ^ texDataU32[0x100/4] ^ texDataU32[0x200/4] ^ texDataU32[0x300/4]; // check the first thick slice (each slice has 0x400 bytes, with 0x100 bytes between layers)
		}
		uint32* texDataU32 = (uint32*)memory_getPointerFromPhysicalOffset(hostTexture->texDataPtrLow);
		return texDataU32[0] ^ texDataU32[1] ^ texDataU32[2] ^ texDataU32[3];
	}

	uint32 memRange = hostTexture->texDataPtrHigh - hostTexture->texDataPtrLow;
	uint32* texDataU32 = (uint32*)memory_getPointerFromPhysicalOffset(hostTexture->texDataPtrLow);
	uint32 hashVal = 0;
	uint32 pixelCount = hostTexture->width*hostTexture->height;

	bool isCompressedFormat = hostTexture->IsCompressedFormat();
	if (isCompressedFormat || hostTexture->useLightHash)
	{
		// check only 32 samples of the texture
		if (memRange < 256)
		{
			memRange /= sizeof(uint32);
			while (memRange--)
			{
				hashVal += *texDataU32;
				hashVal = (hashVal << 3) | (hashVal >> 29);
				texDataU32++;
			}
		}
		else
		{
			hashVal = _quickStochasticHash(texDataU32, memRange);
		}
		return hashVal;
	}


	if( pixelCount <= (700*700) )
	{
		// small texture size
		bool isCompressedFormat = hostTexture->IsCompressedFormat();
		if( isCompressedFormat == false || memRange < 0x200 )
		{
			memRange /= (4*sizeof(uint32));
			while( memRange-- )
			{
				hashVal += *texDataU32;
				hashVal = (hashVal<<3)|(hashVal>>29);
				texDataU32 += 4;
			}
		}
		else
		{
			memRange /= (32*sizeof(uint32));
			while( memRange-- )
			{
				hashVal += *texDataU32;
				hashVal = (hashVal<<3)|(hashVal>>29);
				texDataU32 += 32;
			}
		}
	}
	else if( pixelCount <= (1200*1200) )
	{
		// medium texture size
		bool isCompressedFormat = hostTexture->IsCompressedFormat();
		if( isCompressedFormat == false )
		{
			memRange /= (12*sizeof(uint32));
			while( memRange-- )
			{
				hashVal += *texDataU32;
				hashVal = (hashVal<<3)|(hashVal>>29);
				texDataU32 += 12;
			}
		}
		else
		{
			memRange /= (96*sizeof(uint32));
			while( memRange-- )
			{
				hashVal += *texDataU32;
				hashVal = (hashVal<<3)|(hashVal>>29);
				texDataU32 += 96;
			}
		}
	}
	else
	{
		// huge texture size
		bool isCompressedFormat = hostTexture->IsCompressedFormat();
		if( isCompressedFormat == false )
		{
#if BOOST_OS_WINDOWS
			if (g_CPUFeatures.x86.avx2)
			{
				__m256i h256 = { 0 };
				__m256i* readPtr = (__m256i*)texDataU32;
				memRange /= (288);
				while (memRange--)
				{
					__m256i temp = _mm256_load_si256(readPtr);
					readPtr += (288 / 32);
					h256 = _mm256_xor_si256(h256, temp);
				}
#ifdef __clang__
				hashVal = h256[0] + h256[1] + h256[2] + h256[3] + h256[4] + h256[5] + h256[6] + h256[7];
#else
				hashVal = h256.m256i_u32[0] + h256.m256i_u32[1] + h256.m256i_u32[2] + h256.m256i_u32[3] + h256.m256i_u32[4] + h256.m256i_u32[5] + h256.m256i_u32[6] + h256.m256i_u32[7];
#endif
			}
#else
			if( false ) {}
#endif
			else
			{
				memRange /= (32 * sizeof(uint64));
				uint64 h64 = 0;
				uint64* texDataU64 = (uint64*)texDataU32;
				while (memRange--)
				{
					h64 += *texDataU64;
					h64 = (h64 << 3) | (h64 >> 61);
					texDataU64 += 32;
				}
				hashVal = (h64 & 0xFFFFFFFF) + (h64 >> 32);
			}
		}
		else
		{
			memRange /= (512*sizeof(uint32));
			while( memRange-- )
			{
				hashVal += *texDataU32;
				hashVal = (hashVal<<3)|(hashVal>>29);
				texDataU32 += 512;
			}
		}
	}

	return hashVal;
}

uint64 _botwLargeTexHax = 0;

bool LatteTC_HasTextureChanged(LatteTexture* hostTexture, bool force)
{
	if (hostTexture->forceInvalidate)
	{
		force = true;
		debug_printf("Force invalidate 0x%08x\n", hostTexture->physAddress);
		hostTexture->forceInvalidate = false;
	}
	// if texture is written by GPU operations we switch to a faster hash implementation
	if (hostTexture->isUpdatedOnGPU && hostTexture->useLightHash == false)
	{
		hostTexture->useLightHash = true;
		// update hash
		hostTexture->texDataHash2 = LatteTexture_CalculateTextureDataHash(hostTexture);
	}
	// only check each texture for updates once a frame
	// todo: Instead of relying on frames, it would be better to recheck only after any GPU wait operation occurred.
	if( hostTexture->lastDataUpdateFrameCounter == LatteGPUState.frameCounter && force == false)
		return false;
	hostTexture->lastDataUpdateFrameCounter = LatteGPUState.frameCounter;
	// we assume that certain texture properties indicate that the texture will never be written by the CPU
	if (hostTexture->width == 1280 && hostTexture->format != Latte::E_GX2SURFFMT::R8_UNORM && force == false)
	{
		// todo - remove this or find a better way to handle excluded texture invalidation checks (maybe via game profile?)
		return false;
	}
	// workaround for corrupted terrain texture in BotW after video playback
	// probably would be fixed if we added support for invalidating individual slices/mips of a texture
	uint32 texDataHash = LatteTexture_CalculateTextureDataHash(hostTexture);
	if( texDataHash != hostTexture->texDataHash2 )
	{
		hostTexture->texDataHash2 = texDataHash;
		if (hostTexture->depth == 83 && hostTexture->width == 1024 && hostTexture->height == 1024)
		{
			_botwLargeTexHax = LatteGPUState.frameCounter;
		}
		return true;
	}
	if (_botwLargeTexHax != 0 && hostTexture->depth == 83 && hostTexture->width == 1024 && hostTexture->height == 1024 && _botwLargeTexHax != LatteGPUState.frameCounter)
	{
		_botwLargeTexHax = 0;
		return true;
	}
	return false;
}

void LatteTC_ResetTextureChangeTracker(LatteTexture* hostTexture, bool force)
{
	if( hostTexture->lastDataUpdateFrameCounter == LatteGPUState.frameCounter && force == false)
		return;
	hostTexture->lastDataUpdateFrameCounter = LatteGPUState.frameCounter;
	LatteTC_HasTextureChanged(hostTexture, true);
}

/*
 * This function should be called whenever the texture is still used in some form (any kind of access counts)
 * The purpose of this function is to prevent garbage collection of textures that are still actively used
 */
void LatteTC_MarkTextureStillInUse(LatteTexture* texture)
{
	texture->lastAccessTick = LatteGPUState.currentDrawCallTick;
	texture->lastAccessFrameCount = LatteGPUState.frameCounter;
}

// check if a texture has been overwritten by another texture using GPU-writes
bool LatteTC_IsTextureDataOverwritten(LatteTexture* texture)
{
	// check overlaps
	sint32 mipLevels = texture->mipLevels;
	sint32 sliceCount = texture->depth;
	mipLevels = std::min(mipLevels, 3); // only check first 3 mip levels
	for (sint32 mipIndex = 0; mipIndex < mipLevels; mipIndex++)
	{
		sint32 mipSliceCount;
		if (texture->Is3DTexture())
			mipSliceCount = std::max(1, sliceCount >> mipIndex);
		else
			mipSliceCount = sliceCount;
		for (sint32 sliceIndex = 0; sliceIndex < mipSliceCount; sliceIndex++)
		{
			LatteTextureSliceMipInfo* sliceMipInfo = texture->sliceMipInfo + texture->GetSliceMipArrayIndex(sliceIndex, mipIndex);
			bool isSliceMipOutdated = false;
			for (auto& overlapData : sliceMipInfo->list_dataOverlap)
			{
				if (sliceMipInfo->lastDynamicUpdate < overlapData.destMipSliceInfo->lastDynamicUpdate)
				{
					isSliceMipOutdated = true;
					break;
				}
			}
			if (isSliceMipOutdated == false)
				return false;
		}
	}
	return true;
}

void LatteTexture_Delete(LatteTexture* texture)
{
	LatteTC_UnregisterTexture(texture);
	LatteMRT::NotifyTextureDeletion(texture);
	LatteTextureReadback_NotifyTextureDeletion(texture);
	LatteTexture_DeleteTextureRelations(texture);
	// delete views
	while (!texture->views.empty())
		delete texture->views[0];
	cemu_assert_debug(texture->views.empty());
	cemu_assert_debug(texture->baseView == nullptr);
	// free data overlap tracking
	LatteTexture_DeleteDataOverlapTracking(texture);
	// remove from lists
	LatteTexture_UnregisterTextureMemoryOccupancy(texture);
	// free memory
	if (texture->sliceMipInfo)
	{
		delete[] texture->sliceMipInfo;
		texture->sliceMipInfo = nullptr;
	}
	delete texture;
}

/*
 * Checks if the texture can be dropped from the cache and if yes, delete it
 * Returns true if the texture was deleted
 */
bool LatteTC_CleanupCheckTexture(LatteTexture* texture, uint32 currentTick)
{
	uint32 currentFrameCount = LatteGPUState.frameCounter;
	uint32 ticksSinceLastAccess = currentTick - texture->lastAccessTick;
	uint32 framesSinceLastAccess = currentFrameCount - texture->lastAccessFrameCount;
	if( !texture->isUpdatedOnGPU )
	{
		// RAM-only textures are safe to be deleted since we can always restore them from RAM
		if( ticksSinceLastAccess >= (120*1000) && framesSinceLastAccess >= 2000 )
		{
			LatteTexture_Delete(texture);
			return true;
		}
	}

	if ((LatteGPUState.currentDrawCallTick - texture->lastAccessTick) >= 100 && 
		LatteTC_IsTextureDataOverwritten(texture))
	{
		LatteTexture_Delete(texture);
		return true;
	}
	// if unused for more than 5 seconds, start deleting views since they are cheap to recreate
	if (ticksSinceLastAccess >= 5 * 1000 && framesSinceLastAccess >= 30)
	{
		for (sint32 i = 0; i < 3; i++)
		{
			if (texture->views.size() <= 1)
				break;
			LatteTextureView* view = texture->views[0];
			if (view == texture->baseView)
				view = texture->views[1];
			delete view;
		}
	}
	return false;
}

void LatteTexture_RefreshInfoCache();

/*
 * Scans for unused textures and deletes them
 * Called at the end of every frame
 */
void LatteTC_CleanupUnusedTextures()
{
	static size_t currentScanIndex = 0;
	uint32 currentTick = GetTickCount();
	sint32 maxDelete = 10;
	std::vector<LatteTexture*>& allTextures = LatteTexture::GetAllTextures();
	if (!allTextures.empty())
	{
		for (sint32 c = 0; c < 25; c++)
		{
			if (currentScanIndex >= allTextures.size())
				currentScanIndex = 0;
			LatteTexture* texItr = allTextures[currentScanIndex];
			currentScanIndex++;
			if (!texItr)
				continue;
			if (LatteTC_CleanupCheckTexture(texItr, currentTick))
			{
				maxDelete--;
				if (maxDelete <= 0)
					break; // deleting can be an expensive operation, dont delete too many at once to avoid micro stutter
				if (allTextures.empty())
					break;
			}
		}
	}
	LatteTexture_RefreshInfoCache(); // find a better place to call this from?
}

std::vector<LatteTexture*> LatteTC_GetDeleteableTextures()
{
	std::vector<LatteTexture*> texList;
	uint32 currentFrameCount = LatteGPUState.frameCounter;

	for (auto& itr : g_allTextures)
	{
		if(itr->lastAccessFrameCount == 0)
			continue; // not initialized
		uint32 framesSinceLastAccess = currentFrameCount - itr->lastAccessFrameCount;
		if(framesSinceLastAccess < 3)
			continue;
		if (itr->isUpdatedOnGPU)
		{
			if (LatteTC_IsTextureDataOverwritten(itr))
				texList.emplace_back(itr);
		}
		else
		{
			texList.emplace_back(itr);
		}
	}

	return texList;
}

void LatteTC_UnloadAllTextures()
{
	std::vector<LatteTexture*> allTexturesCopy = LatteTexture::GetAllTextures();
	for (auto& itr : allTexturesCopy)
	{
		if(itr)
			LatteTexture_Delete(itr);
	}
	LatteRenderTarget_unloadAll();
}