#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"

#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.h"

#include "Cafe/GraphicPack/GraphicPack2.h"

#include <boost/container/small_vector.hpp>

struct TexMemOccupancyEntry
{
	uint32 addrStart;
	uint32 addrEnd;
	LatteTextureSliceMipInfo* sliceMipInfo;
};

#define TEX_OCCUPANCY_BUCKET_COUNT		(0x800) // each bucket covers a range of 2MB
#define TEX_OCCUPANCY_BUCKET_SIZE		(0x100000000/TEX_OCCUPANCY_BUCKET_COUNT)

#define loopItrMemOccupancyBuckets(__startAddr, __endAddr)		for(sint32 startBucketIndex = ((__startAddr)/TEX_OCCUPANCY_BUCKET_SIZE), bucketIndex=startBucketIndex; bucketIndex<=((__endAddr-1)/TEX_OCCUPANCY_BUCKET_SIZE); bucketIndex++)

std::vector<TexMemOccupancyEntry> list_texMemOccupancyBucket[TEX_OCCUPANCY_BUCKET_COUNT];

std::atomic_bool s_refreshTextureQueryList;
std::vector<LatteTextureInformation> s_cacheInfoList;

std::vector<LatteTextureInformation> LatteTexture_QueryCacheInfo()
{
	// raise request flag to refresh cache
	s_refreshTextureQueryList.store(true);
	// wait until cleared or until timeout occurred
	auto begin = std::chrono::high_resolution_clock::now();
	while (true)
	{
		if (!s_refreshTextureQueryList)
			break;
		auto dur = std::chrono::high_resolution_clock::now() - begin;
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
		if (ms >= 1000) // dont stall more than one second
			return std::vector<LatteTextureInformation>();
	}
	return s_cacheInfoList;
}

void LatteTexture_RefreshInfoCache()
{
	if (!s_refreshTextureQueryList)
		return;
	std::vector<LatteTextureInformation> infoCache;
	std::set<LatteTexture*> visitedTextures;

	std::unordered_set<LatteTextureView*> allViews = LatteTextureViewLookupCache::GetAllViews();
	for (auto& it : allViews)
	{
		LatteTexture* baseTexture = it->baseTexture;
		if(visitedTextures.find(baseTexture) != visitedTextures.end())
			continue;
		visitedTextures.emplace(baseTexture);
		// add cache info
		auto& entry = infoCache.emplace_back();
		entry.physAddress = baseTexture->physAddress;
		entry.physMipAddress = baseTexture->physMipAddress;
		entry.width = baseTexture->width;
		entry.height = baseTexture->height;
		entry.depth = baseTexture->depth;
		entry.pitch = baseTexture->pitch;
		entry.mipLevels = baseTexture->mipLevels;
		entry.format = baseTexture->format;
		entry.isDepth = baseTexture->isDepth;
		entry.dim = baseTexture->dim;
		entry.tileMode = baseTexture->tileMode;
		entry.lastAccessTick = baseTexture->lastAccessTick;
		entry.lastAccessFrameCount = baseTexture->lastAccessFrameCount;
		entry.isUpdatedOnGPU = baseTexture->isUpdatedOnGPU;
		// overwrite info
		entry.overwriteInfo.hasResolutionOverwrite = baseTexture->overwriteInfo.hasResolutionOverwrite;
		entry.overwriteInfo.width = baseTexture->overwriteInfo.width;
		entry.overwriteInfo.height = baseTexture->overwriteInfo.height;
		entry.overwriteInfo.depth = baseTexture->overwriteInfo.depth;
		// count number of alternative views
		entry.alternativeViewCount = 0;
		// views
		for (auto& viewItr : baseTexture->views)
		{
			if(viewItr == baseTexture->baseView)
				continue;
			auto& viewEntry = entry.views.emplace_back();
			viewEntry.physAddress = viewItr->baseTexture->physAddress;
			viewEntry.physMipAddress = viewItr->baseTexture->physMipAddress;
			viewEntry.width = viewItr->baseTexture->width;
			viewEntry.height = viewItr->baseTexture->height;
			viewEntry.pitch = viewItr->baseTexture->pitch;
			viewEntry.firstMip = viewItr->firstMip;
			viewEntry.numMip = viewItr->numMip;
			viewEntry.firstSlice = viewItr->firstSlice;
			viewEntry.numSlice = viewItr->numSlice;
			viewEntry.format = viewItr->format;
			viewEntry.dim = viewItr->dim;
		}
	}
	std::swap(s_cacheInfoList, infoCache);
	s_refreshTextureQueryList.store(false);
}

void LatteTexture_AddTexMemOccupancyInterval(LatteTextureSliceMipInfo* sliceMipInfo)
{
	TexMemOccupancyEntry entry;
	entry.addrStart = sliceMipInfo->addrStart;
	entry.addrEnd = sliceMipInfo->addrEnd;
	entry.sliceMipInfo = sliceMipInfo;
	loopItrMemOccupancyBuckets(entry.addrStart, entry.addrEnd)
		list_texMemOccupancyBucket[bucketIndex].push_back(entry);
}

void LatteTexture_RegisterTextureMemoryOccupancy(LatteTexture* texture)
{
	sint32 mipLevels = texture->mipLevels;
	sint32 sliceCount = texture->depth;
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
			LatteTexture_AddTexMemOccupancyInterval(sliceMipInfo);
		}
	}
}

void LatteTexture_RemoveTexMemOccupancyInterval(LatteTexture* texture, LatteTextureSliceMipInfo* sliceMipInfo)
{
	loopItrMemOccupancyBuckets(sliceMipInfo->addrStart, sliceMipInfo->addrEnd)
	{
		for (sint32 i = 0; i < list_texMemOccupancyBucket[bucketIndex].size(); i++)
		{
			if (list_texMemOccupancyBucket[bucketIndex][i].sliceMipInfo->texture == texture)
			{
				list_texMemOccupancyBucket[bucketIndex].erase(list_texMemOccupancyBucket[bucketIndex].begin() + i);
				i--;
				continue;
			}
		}
	}
}

void LatteTexture_UnregisterTextureMemoryOccupancy(LatteTexture* texture)
{
	sint32 mipLevels = texture->mipLevels;
	sint32 sliceCount = texture->depth;
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
			LatteTexture_RemoveTexMemOccupancyInterval(texture, sliceMipInfo);
		}
	}
}

// calculate the actually accessed data range
// the resulting range is an estimate and may be smaller than the actual slice size (but not larger) 
void LatteTexture_EstimateMipSliceAccessedDataRange(LatteTexture* texture, sint32 sliceIndex, sint32 mipIndex, LatteTextureSliceMipInfo* sliceMipInfo)
{
	uint32 estAddrStart;
	uint32 estAddrEnd;
	LatteTextureLoader_estimateAccessedDataRange(texture, sliceIndex, mipIndex, estAddrStart, estAddrEnd);

	cemu_assert_debug(estAddrStart >= sliceMipInfo->addrStart);
	cemu_assert_debug(estAddrEnd <= sliceMipInfo->addrEnd);
	cemu_assert_debug(estAddrStart <= estAddrEnd);

	sliceMipInfo->estDataAddrStart = estAddrStart;
	sliceMipInfo->estDataAddrEnd = estAddrEnd;
}

void LatteTexture_InitSliceAndMipInfo(LatteTexture* texture)
{
	cemu_assert_debug(texture->mipLevels > 0);
	cemu_assert_debug(texture->depth > 0);
	sint32 mipSliceCount = texture->GetSliceMipArraySize();
	texture->sliceMipInfo = new LatteTextureSliceMipInfo[mipSliceCount]();
	// todo - mipLevels can be greater than maximum possible mip count. How to handle this? Probably should differentiate between mipLevels and effective mip levels
	sint32 mipLevels = texture->mipLevels;
	sint32 sliceCount = texture->depth;

	for (sint32 mipIndex = 0; mipIndex < mipLevels; mipIndex++)
	{
		sint32 mipSliceCount;
		if (texture->Is3DTexture())
		{
			mipSliceCount = std::max(1, sliceCount >> mipIndex);
		}
		else
			mipSliceCount = sliceCount;
		for (sint32 sliceIndex = 0; sliceIndex < mipSliceCount; sliceIndex++)
		{
			uint32 calcSliceAddr;
			uint32 calcSliceSize;
			sint32 calcSubSliceIndex;
			LatteAddrLib::CalculateMipAndSliceAddr(texture->physAddress, texture->physMipAddress, texture->format, texture->width, texture->height, texture->depth, texture->dim, texture->tileMode, texture->swizzle, 0, mipIndex, sliceIndex, &calcSliceAddr, &calcSliceSize, &calcSubSliceIndex);
			LatteTextureSliceMipInfo* sliceMipInfo = texture->sliceMipInfo + texture->GetSliceMipArrayIndex(sliceIndex, mipIndex);
			sliceMipInfo->addrStart = calcSliceAddr;
			sliceMipInfo->addrEnd = calcSliceAddr + calcSliceSize;
			sliceMipInfo->subIndex = calcSubSliceIndex;
			sliceMipInfo->dataChecksum = 0;
			sliceMipInfo->sliceIndex = sliceIndex;
			sliceMipInfo->mipIndex = mipIndex;
			sliceMipInfo->texture = texture;
			// get additional slice/mip info
			LatteAddrLib::AddrSurfaceInfo_OUT surfaceInfo;
			LatteAddrLib::GX2CalculateSurfaceInfo(texture->format, texture->width, texture->height, texture->depth, texture->dim, Latte::MakeGX2TileMode(texture->tileMode), 0, mipIndex, &surfaceInfo);
			sliceMipInfo->tileMode = surfaceInfo.hwTileMode;
			
			if (mipIndex == 0)
				sliceMipInfo->pitch = texture->pitch; // for the base level, use the pitch value configured in hardware
			else
				sliceMipInfo->pitch = surfaceInfo.pitch;
			LatteTexture_EstimateMipSliceAccessedDataRange(texture, sliceIndex, mipIndex, sliceMipInfo);
		}
	}
}

// if this function returns false, textures will not be synchronized even if their data overlaps
bool LatteTexture_IsFormatViewCompatible(Latte::E_GX2SURFFMT formatA, Latte::E_GX2SURFFMT formatB)
{
	if(formatA == formatB)
		return true; // if the format is identical then compatibility must be guaranteed (otherwise we can't create the necessary default view of a texture)

	// todo - find a better way to handle this
	for (sint32 swap = 0; swap < 2; swap++)
	{
		// other formats
		// seems like format 0x19 (RGB10_A2) has issues on OpenGL Intel and AMD when copying texture data
		Latte::E_HWSURFFMT hwFormatA = Latte::GetHWFormat(formatA);
		Latte::E_HWSURFFMT hwFormatB = Latte::GetHWFormat(formatB);
		if (hwFormatA == Latte::E_HWSURFFMT::HWFMT_2_10_10_10 && formatB == Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT)
			return false;
		if (formatA == Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT && hwFormatB == Latte::E_HWSURFFMT::HWFMT_2_10_10_10)
			return false;

		if (hwFormatA == Latte::E_HWSURFFMT::HWFMT_2_10_10_10 && formatB == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM)
			return false;
		if (formatA == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM && hwFormatB == Latte::E_HWSURFFMT::HWFMT_2_10_10_10)
			return false;

		// format A1B5G5R5 views are not compatible with other 16-bit formats in OpenGL
		if (formatA == Latte::E_GX2SURFFMT::A1_B5_G5_R5_UNORM || formatB == Latte::E_GX2SURFFMT::A1_B5_G5_R5_UNORM)
			return false;
		// used in N64 VC (E.g. Super Mario 64)

		// used in Smash
		if (formatA == Latte::E_GX2SURFFMT::D24_S8_UNORM && formatB == Latte::E_GX2SURFFMT::R10_G10_B10_A2_SNORM)
			return false;
		if (formatA == Latte::E_GX2SURFFMT::R32_FLOAT && formatB == Latte::E_GX2SURFFMT::R10_G10_B10_A2_SNORM)
			return false;

		// loop again with swapped vars
		Latte::E_GX2SURFFMT temp = formatA;
		formatA = formatB;
		formatB = temp;
	}
	return true;
}

bool LatteTexture_IsTexelSizeCompatibleFormat(Latte::E_GX2SURFFMT formatA, Latte::E_GX2SURFFMT formatB)
{
	// handle some special cases where formats are incompatible regardless of equal bpp
	if (formatA == Latte::E_GX2SURFFMT::D24_S8_UNORM && formatB == Latte::E_GX2SURFFMT::D32_FLOAT)
		return false;
	if (Latte::IsCompressedFormat(formatA) && Latte::IsCompressedFormat(formatB))
	{
		if (Latte::GetHWFormat(formatA) != Latte::GetHWFormat(formatB))
			return false; // compressed formats with different encodings are considered incompatible
	}
	return Latte::GetFormatBits((Latte::E_GX2SURFFMT)formatA) == Latte::GetFormatBits((Latte::E_GX2SURFFMT)formatB);
}

void LatteTexture_copyData(LatteTexture* srcTexture, LatteTexture* dstTexture, sint32 mipCount, sint32 sliceCount)
{
	cemu_assert_debug(mipCount != 0);
	cemu_assert_debug(sliceCount != 0);
	sint32 effectiveCopyWidth = srcTexture->width;
	sint32 effectiveCopyHeight = srcTexture->height;
	if (LatteTexture_doesEffectiveRescaleRatioMatch(dstTexture, 0, srcTexture, 0))
	{
		// adjust copy size
		LatteTexture_scaleToEffectiveSize(dstTexture, &effectiveCopyWidth, &effectiveCopyHeight, 0);
	}
	else
	{
		sint32 effectiveWidth_dst, effectiveHeight_dst;
		srcTexture->GetEffectiveSize(effectiveWidth_dst, effectiveHeight_dst, 0);
		sint32 effectiveWidth_src, effectiveHeight_src;
		dstTexture->GetEffectiveSize(effectiveWidth_src, effectiveHeight_src, 0);

		debug_printf("texture_copyData(): Effective size mismatch\n");
		cemuLog_logDebug(LogType::Force, "texture_copyData(): Effective size mismatch (due to texture rule)");
		cemuLog_logDebug(LogType::Force, "Destination: origResolution {:04}x{:04} effectiveResolution {:04}x{:04} fmt {:04x} mipIndex {}", srcTexture->width, srcTexture->height, effectiveWidth_dst, effectiveHeight_dst, (uint32)dstTexture->format, 0);
		cemuLog_logDebug(LogType::Force, "Source:      origResolution {:04}x{:04} effectiveResolution {:04}x{:04} fmt {:04x} mipIndex {}", srcTexture->width, srcTexture->height, effectiveWidth_src, effectiveHeight_src, (uint32)srcTexture->format, 0);
		return;
	}
	for (sint32 mipIndex = 0; mipIndex < mipCount; mipIndex++)
	{
		sint32 sliceCopyWidth = std::max(effectiveCopyWidth >> mipIndex, 1);
		sint32 sliceCopyHeight = std::max(effectiveCopyHeight >> mipIndex, 1);
		g_renderer->texture_copyImageSubData(srcTexture, mipIndex, 0, 0, 0, dstTexture, mipIndex, 0, 0, 0, sliceCopyWidth, sliceCopyHeight, sliceCount);
		sint32 mipSliceCount = sliceCount;
		if (dstTexture->Is3DTexture())
			mipSliceCount >>= mipIndex;
		for (sint32 sliceIndex = 0; sliceIndex < mipSliceCount; sliceIndex++)
		{
			LatteTextureSliceMipInfo* srcTexSliceInfo = srcTexture->sliceMipInfo + srcTexture->GetSliceMipArrayIndex(sliceIndex, mipIndex);
			LatteTextureSliceMipInfo* dstTexSliceInfo = dstTexture->sliceMipInfo + dstTexture->GetSliceMipArrayIndex(sliceIndex, mipIndex);
			dstTexSliceInfo->lastDynamicUpdate = srcTexSliceInfo->lastDynamicUpdate;
		}
	}
}

template<bool bothMustMatch>
bool LatteTexture_DoesWidthHeightMatch(Latte::E_GX2SURFFMT format1, uint32 width1, uint32 height1, Latte::E_GX2SURFFMT format2, uint32 width2, uint32 height2)
{
	if (Latte::IsCompressedFormat(format1))
	{
		width1 <<= 2;
		height1 <<= 2;
	}
	if (Latte::IsCompressedFormat(format2))
	{
		width2 <<= 2;
		height2 <<= 2;
	}
	if constexpr(bothMustMatch)
		return width1 == width2 && height1 == height2;
	else
		return width1 == width2 || height1 == height2;
}

void LatteTexture_CopySlice(LatteTexture* srcTexture, sint32 srcSlice, sint32 srcMip, LatteTexture* dstTexture, sint32 dstSlice, sint32 dstMip, sint32 srcX, sint32 srcY, sint32 dstX, sint32 dstY, sint32 width, sint32 height)
{
	if (srcTexture->isDepth != dstTexture->isDepth)
	{
		g_renderer->surfaceCopy_copySurfaceWithFormatConversion(srcTexture, srcMip, srcSlice, dstTexture, dstMip, dstSlice, width, height);
		return;
	}
	// rescale copy size
	sint32 effectiveCopyWidth = width;
	sint32 effectiveCopyHeight = height;
	LatteTexture_scaleToEffectiveSize(srcTexture, &effectiveCopyWidth, &effectiveCopyHeight, 0);

	sint32 effectiveSrcX = srcX;
	sint32 effectiveSrcY = srcY;
	LatteTexture_scaleToEffectiveSize(srcTexture, &effectiveSrcX, &effectiveSrcY, 0);

	sint32 effectiveDstX = dstX;
	sint32 effectiveDstY = dstY;
	LatteTexture_scaleToEffectiveSize(dstTexture, &effectiveDstX, &effectiveDstY, 0);

	// check if rescale is compatible
	if (LatteTexture_doesEffectiveRescaleRatioMatch(dstTexture, 0, srcTexture, 0) == false)
	{
		sint32 effectiveWidth_src = srcTexture->overwriteInfo.hasResolutionOverwrite ? srcTexture->overwriteInfo.width : srcTexture->width;
		sint32 effectiveHeight_src = srcTexture->overwriteInfo.hasResolutionOverwrite ? srcTexture->overwriteInfo.height : srcTexture->height;
		sint32 effectiveWidth_dst = dstTexture->overwriteInfo.hasResolutionOverwrite ? dstTexture->overwriteInfo.width : dstTexture->width;
		sint32 effectiveHeight_dst = dstTexture->overwriteInfo.hasResolutionOverwrite ? dstTexture->overwriteInfo.height : dstTexture->height;
		if (cemuLog_isLoggingEnabled(LogType::TextureCache))
		{
			cemuLog_log(LogType::Force, "_copySlice(): Unable to sync textures with mismatching scale ratio (due to texture rule)");
			float ratioWidth_src = (float)effectiveWidth_src / (float)srcTexture->width;
			float ratioHeight_src = (float)effectiveHeight_src / (float)srcTexture->height;
			float ratioWidth_dst = (float)effectiveWidth_dst / (float)dstTexture->width;
			float ratioHeight_dst = (float)effectiveHeight_dst / (float)dstTexture->height;
			cemuLog_log(LogType::Force, "Source:      {:08x} origResolution {:4}/{:4} effectiveResolution {:4}/{:4} fmt {:04x} mipIndex {} ratioW/H: {:.4}/{:.4}", srcTexture->physAddress, srcTexture->width, srcTexture->height, effectiveWidth_src, effectiveHeight_src, (uint32)srcTexture->format, srcMip, ratioWidth_src, ratioHeight_src);
			cemuLog_log(LogType::Force, "Destination: {:08x} origResolution {:4}/{:4} effectiveResolution {:4}/{:4} fmt {:04x} mipIndex {} ratioW/H: {:.4}/{:.4}", dstTexture->physAddress, dstTexture->width, dstTexture->height, effectiveWidth_dst, effectiveHeight_dst, (uint32)dstTexture->format, dstMip, ratioWidth_dst, ratioHeight_dst);
		}
		//cemuLog_logDebug(LogType::Force, "If these textures are not meant to share data you can ignore this");
		return;
	}
	// todo - store 'lastUpdated' value per slice/mip and copy it's value when copying the slice data
	g_renderer->texture_copyImageSubData(srcTexture, srcMip, effectiveSrcX, effectiveSrcY, srcSlice, dstTexture, dstMip, effectiveDstX, effectiveDstY, dstSlice, effectiveCopyWidth, effectiveCopyHeight, 1);
}

bool LatteTexture_GetSubtextureSliceAndMip(LatteTexture* baseTexture, LatteTexture* mipTexture, sint32* baseSliceIndex, sint32* baseMipIndex)
{
	LatteTextureSliceMipInfo* mipTextureSliceInfo = mipTexture->sliceMipInfo + mipTexture->GetSliceMipArrayIndex(0, 0);
	// todo - this can be optimized by first determining the mip level from pitch
	for (sint32 mipIndex = 0; mipIndex < baseTexture->mipLevels; mipIndex++)
	{
		sint32 sliceCount;
		if (baseTexture->Is3DTexture())
			sliceCount = std::max(baseTexture->depth >> mipIndex, 1);
		else
			sliceCount = baseTexture->depth;
		for (sint32 sliceIndex = 0; sliceIndex < sliceCount; sliceIndex++)
		{
			LatteTextureSliceMipInfo* sliceMipInfo = baseTexture->sliceMipInfo + baseTexture->GetSliceMipArrayIndex(sliceIndex, mipIndex);
			if (sliceMipInfo->addrStart == mipTextureSliceInfo->addrStart && sliceMipInfo->subIndex == mipTextureSliceInfo->subIndex)
			{
				*baseSliceIndex = sliceIndex;
				*baseMipIndex = mipIndex;
				return true;
			}
			// todo - support overlapping textures with a non-zero y-offset
		}
	}
	return false;
}

// if a texture shares memory with another texture then flag those textures as invalidated (on next use, synchronize data)
void LatteTexture_MarkDynamicTextureAsChanged(LatteTextureView* textureView, sint32 sliceIndex, sint32 mipIndex, uint64 eventCounter)
{
	LatteTexture* baseTexture = textureView->baseTexture;
	baseTexture->lastWriteEventCounter = eventCounter;

	sint32 aSliceIndex = textureView->firstSlice + sliceIndex;
	sint32 aMipIndex = textureView->firstMip + mipIndex;
	LatteTextureSliceMipInfo* baseSliceMipInfo = baseTexture->sliceMipInfo + baseTexture->GetSliceMipArrayIndex(aSliceIndex, aMipIndex);
	baseSliceMipInfo->lastDynamicUpdate = eventCounter;

	LatteTexture_MarkConnectedTexturesForReloadFromDynamicTextures(textureView->baseTexture);
}

void LatteTexture_SyncSlice(LatteTexture* srcTexture, sint32 srcSliceIndex, sint32 srcMipIndex, LatteTexture* dstTexture, sint32 dstSliceIndex, sint32 dstMipIndex)
{
	sint32 srcWidth = srcTexture->width;
	sint32 srcHeight = srcTexture->height;
	sint32 dstWidth = dstTexture->width;
	sint32 dstHeight = dstTexture->height;

	if(srcTexture->overwriteInfo.hasFormatOverwrite != dstTexture->overwriteInfo.hasFormatOverwrite)
		return; // dont sync: format overwrite state needs to match. Not strictly necessary but it simplifies logic down the road
	else if(srcTexture->overwriteInfo.hasFormatOverwrite && srcTexture->overwriteInfo.format != dstTexture->overwriteInfo.format)
		return; // both are overwritten but with different formats

	if (srcMipIndex == 0 && dstMipIndex == 0 && (srcTexture->tileMode == Latte::E_HWTILEMODE::TM_LINEAR_ALIGNED || srcTexture->tileMode == Latte::E_HWTILEMODE::TM_1D_TILED_THIN1) && srcTexture->height > dstTexture->height && (srcTexture->height % dstTexture->height) == 0)
	{
		bool isMatch = srcTexture->tileMode == Latte::E_HWTILEMODE::TM_LINEAR_ALIGNED;
		if (srcTexture->tileMode == Latte::E_HWTILEMODE::TM_1D_TILED_THIN1 && srcTexture->width == 32)
		{
			// special case for CoD BO2, where 1024x32 and 32x32x32 textures share memory
			isMatch = true;
		}

		if (isMatch && srcTexture->IsCompressedFormat() == false && dstTexture->IsCompressedFormat() == false)
		{
			sint32 virtualSlices = srcTexture->height / dstTexture->height;
			if (dstTexture->depth == virtualSlices)
			{
				// special case for Ninja Gaiden
				// it initializes a 24x24x24 texture array as a 24x576x1 2D texture (using tilemode 1)
				sint32 copyWidth = std::min(srcWidth, dstWidth);
				sint32 copyHeight = std::min(srcHeight, dstHeight);
				for (sint32 slice = 0; slice < virtualSlices; slice++)
					LatteTexture_CopySlice(srcTexture, srcSliceIndex, srcMipIndex, dstTexture, dstSliceIndex + slice, dstMipIndex, 0, slice * dstTexture->height, 0, 0, copyWidth, copyHeight);
			}
			return;
		}
	}

	bool srcIsCompressed = srcTexture->IsCompressedFormat();
	bool dstIsCompressed = dstTexture->IsCompressedFormat();

	if (srcIsCompressed != dstIsCompressed)
	{
		// convert into unit of source texture
		if (srcIsCompressed == false)
		{
			// destination compressed, source uncompressed (integer format)
			dstWidth >>= 2;
			dstHeight >>= 2;
		}
		else
		{
			// destination uncompressed (integer format), source compressed
			dstWidth <<= 2;
			dstHeight <<= 2;
		}
	}

	srcWidth = std::max(srcWidth >> srcMipIndex, 1);
	srcHeight = std::max(srcHeight >> srcMipIndex, 1);
	dstWidth = std::max(dstWidth >> dstMipIndex, 1);
	dstHeight = std::max(dstHeight >> dstMipIndex, 1);

	sint32 copyWidth = std::min(srcWidth, dstWidth);
	sint32 copyHeight = std::min(srcHeight, dstHeight);

	LatteTexture_CopySlice(srcTexture, srcSliceIndex, srcMipIndex, dstTexture, dstSliceIndex, dstMipIndex, 0, 0, 0, 0, copyWidth, copyHeight);

}

void LatteTexture_UpdateTextureFromDynamicChanges(LatteTexture* texture)
{
	// note: Currently this function assumes that only one other texture is updated per slice/mip (if multiple overlap, we should merge the one with the latest timestamp the latest of each individually)
	for (auto& texRel : texture->list_compatibleRelations)
	{
		LatteTexture* baseTexture = texRel->baseTexture;
		LatteTexture* subTexture = texRel->subTexture;
		for (sint32 cMipIndex = 0; cMipIndex < texRel->mipCount; cMipIndex++)
		{
			sint32 mipSliceCount = texRel->sliceCount;
			if (texRel->baseTexture->Is3DTexture())
			{
				cemu_assert_debug(cMipIndex == 0); // values above 0 need testing
				mipSliceCount >>= cMipIndex;
			}

			for (sint32 cSliceIndex = 0; cSliceIndex < mipSliceCount; cSliceIndex++)
			{
				LatteTextureSliceMipInfo* baseSliceMipInfo = baseTexture->sliceMipInfo + baseTexture->GetSliceMipArrayIndex(texRel->baseSliceIndex + cSliceIndex, texRel->baseMipIndex + cMipIndex);
				LatteTextureSliceMipInfo* subSliceMipInfo = subTexture->sliceMipInfo + subTexture->GetSliceMipArrayIndex(cSliceIndex, cMipIndex);
				if (texture == baseTexture)
				{
					// baseTexture is target texture
					if (baseSliceMipInfo->lastDynamicUpdate < subSliceMipInfo->lastDynamicUpdate)
					{
						LatteTexture_SyncSlice(subTexture, cSliceIndex, cMipIndex, baseTexture, texRel->baseSliceIndex + cSliceIndex, texRel->baseMipIndex + cMipIndex);
						baseSliceMipInfo->lastDynamicUpdate = subSliceMipInfo->lastDynamicUpdate;
						if(subTexture->isUpdatedOnGPU)
							texture->isUpdatedOnGPU = true;
					}
				}
				else
				{
					// subTexture is target texture
					if (subSliceMipInfo->lastDynamicUpdate < baseSliceMipInfo->lastDynamicUpdate)
					{
						LatteTexture_SyncSlice(baseTexture, texRel->baseSliceIndex + cSliceIndex, texRel->baseMipIndex + cMipIndex, subTexture, cSliceIndex, cMipIndex);
						subSliceMipInfo->lastDynamicUpdate = baseSliceMipInfo->lastDynamicUpdate;
						if (baseTexture->isUpdatedOnGPU)
							texture->isUpdatedOnGPU = true;
					}
				}
			}
		}
	}
}

bool _LatteTexture_IsTileModeCompatible(LatteTexture* texture1, sint32 mipIndex1, LatteTexture* texture2, sint32 mipIndex2)
{
	if (mipIndex1 == 0 && mipIndex2 == 0)
		return texture1->tileMode == texture2->tileMode;

	LatteTextureSliceMipInfo* texture1SliceInfo = texture1->sliceMipInfo + texture1->GetSliceMipArrayIndex(0, mipIndex1);
	LatteTextureSliceMipInfo* texture2SliceInfo = texture2->sliceMipInfo + texture2->GetSliceMipArrayIndex(0, mipIndex2);
	if (texture1SliceInfo->tileMode == texture2SliceInfo->tileMode)
		return true;
	return false;
}

bool __LatteTexture_IsBlockedFormatRelation(LatteTexture* texture1, LatteTexture* texture2)
{
	if (texture1->isDepth && texture2->isDepth == false)
	{
		// necessary for Smash? (currently our depth to color copy always converts and the depth ends up in R only)
		if (texture1->format == Latte::E_GX2SURFFMT::D32_FLOAT && Latte::GetHWFormat(texture2->format) == Latte::E_HWSURFFMT::HWFMT_8_8_8_8)
			return true;
	}
	// Vulkan has stricter rules
	if (g_renderer->GetType() == RendererAPI::Vulkan)
	{
		// found in Smash (Wii Fit Stage)
		if (texture1->format == Latte::E_GX2SURFFMT::D32_FLOAT && Latte::GetHWFormat(texture2->format) == Latte::E_HWSURFFMT::HWFMT_8_24)
			return true;
	}

	return false;
}

bool LatteTexture_IsBlockedFormatRelation(LatteTexture* texture1, LatteTexture* texture2)
{
	if (__LatteTexture_IsBlockedFormatRelation(texture1, texture2))
		return true;
	return __LatteTexture_IsBlockedFormatRelation(texture2, texture1);
}

// called if two textures are known to overlap in memory
// this function then tries to figure out the details and registers the relation in texture*->list_compatibleRelations
void LatteTexture_TrackTextureRelation(LatteTexture* texture1, LatteTexture* texture2)
{
	// make sure texture 2 is always at texture 1 mip level 0 or beyond
	if (texture1->physAddress > texture2->physAddress)
		return LatteTexture_TrackTextureRelation(texture2, texture1);
	// check if this texture relation is already tracked
	cemu_assert_debug(texture1->physAddress != 0);
	cemu_assert_debug(texture2->physAddress != 0);
	for (auto& it : texture1->list_compatibleRelations)
	{
		if (it->baseTexture == texture1 && it->subTexture == texture2)
			return; // association already known
	}
	// check for blocked format combination
	if (LatteTexture_IsBlockedFormatRelation(texture1, texture2))
		return;

	if (texture1->physAddress == texture2->physAddress && false)
	{
		// both textures overlap at mip level 0
		cemu_assert_debug(texture1->swizzle == texture2->swizzle);
		cemu_assert_debug(texture1->tileMode == texture2->tileMode);
		if (LatteTexture_DoesWidthHeightMatch<false>(texture1->format, texture1->width, texture1->height, texture2->format, texture2->width, texture2->height))
		{
			cemu_assert_unimplemented();
		}
	}
	else
	{
		sint32 baseSliceIndex;
		sint32 baseMipIndex;
		if (texture1->physAddress == texture2->physAddress)
		{
			baseSliceIndex = 0;
			baseMipIndex = 0;
		}
		else
		{
			if (LatteTexture_GetSubtextureSliceAndMip(texture1, texture2, &baseSliceIndex, &baseMipIndex) == false)
			{
				return;
			}
		}
		sint32 sharedMipLevels = 1;
		// todo - support for multiple shared mip levels
		// check if pitch is compatible
		LatteTextureSliceMipInfo* texture1SliceInfo = texture1->sliceMipInfo + texture1->GetSliceMipArrayIndex(baseSliceIndex, baseMipIndex);
		LatteTextureSliceMipInfo* texture2SliceInfo = texture2->sliceMipInfo + texture2->GetSliceMipArrayIndex(0, 0);
		if (_LatteTexture_IsTileModeCompatible(texture1, baseMipIndex, texture2, 0) == false)
			return; // not compatible
		if (texture1SliceInfo->pitch != texture2SliceInfo->pitch)
			return; // not compatible
		// calculate compatible depth range
		sint32 baseRemainingDepth = texture1->GetMipDepth(baseMipIndex) - baseSliceIndex;
		cemu_assert_debug(baseRemainingDepth >= 0);
		sint32 compatibleDepthRange = std::min(baseRemainingDepth, texture2->depth);
		cemu_assert_debug(compatibleDepthRange > 0);

		// create association
		LatteTextureRelation* rel = (LatteTextureRelation*)malloc(sizeof(LatteTextureRelation));
		memset(rel, 0, sizeof(LatteTextureRelation));
		rel->baseTexture = texture1;
		rel->subTexture = texture2;
		rel->baseMipIndex = baseMipIndex;
		rel->baseSliceIndex = baseSliceIndex;
		rel->mipCount = sharedMipLevels;
		rel->sliceCount = compatibleDepthRange;
		rel->yOffset = 0; // todo
		texture1->list_compatibleRelations.push_back(rel);
		texture2->list_compatibleRelations.push_back(rel);
	}
}

void LatteTexture_TrackDataOverlap(LatteTexture* texture, LatteTextureSliceMipInfo* sliceMipInfo, TexMemOccupancyEntry& occupancy)
{
	// todo - handle tile thickness and z offset

	// todo - check address range overlap
	auto& occMipSliceInfo = occupancy.sliceMipInfo;

	if ((sliceMipInfo->addrEnd > occMipSliceInfo->addrStart && sliceMipInfo->addrStart < occMipSliceInfo->addrEnd) == false)
		return;

	// check if this overlap is already tracked
	for (auto& it : sliceMipInfo->list_dataOverlap)
	{
		if (it.destMipSliceInfo == occupancy.sliceMipInfo)
			return;
	}
	// register texture->dest
	LatteTextureSliceMipDataOverlap_t overlapEntry;
	overlapEntry.destMipSliceInfo = occupancy.sliceMipInfo;
	overlapEntry.destTexture = occupancy.sliceMipInfo->texture;
	sliceMipInfo->list_dataOverlap.push_back(overlapEntry);
	// register dest->texture
	LatteTextureSliceMipDataOverlap_t overlapEntry2;
	overlapEntry2.destMipSliceInfo = sliceMipInfo;
	overlapEntry2.destTexture = sliceMipInfo->texture;
	occupancy.sliceMipInfo->list_dataOverlap.push_back(overlapEntry2);
}

void _LatteTexture_RemoveDataOverlapTracking(LatteTexture* texture, LatteTextureSliceMipInfo* sliceMipInfo, LatteTextureSliceMipDataOverlap_t& dataOverlap)
{
	LatteTexture* destTexture = dataOverlap.destTexture;
	LatteTextureSliceMipInfo* destSliceMipInfo = dataOverlap.destMipSliceInfo;
	// delete from dest
	for (auto it = destSliceMipInfo->list_dataOverlap.begin(); it != destSliceMipInfo->list_dataOverlap.end();)
	{
		if (it->destTexture == texture)
			it = destSliceMipInfo->list_dataOverlap.erase(it);
		else if (it->destTexture == destTexture)
			cemu_assert_unimplemented();
		else
			it++;
	}
}

void LatteTexture_DeleteDataOverlapTracking(LatteTexture* texture, LatteTextureSliceMipInfo* sliceMipInfo)
{
	for(auto& it : sliceMipInfo->list_dataOverlap)
		_LatteTexture_RemoveDataOverlapTracking(texture, sliceMipInfo, it);
	sliceMipInfo->list_dataOverlap.resize(0);
}

void LatteTexture_DeleteDataOverlapTracking(LatteTexture* texture)
{
	sint32 mipLevels = texture->mipLevels;
	sint32 sliceCount = texture->depth;
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
			LatteTexture_DeleteDataOverlapTracking(texture, sliceMipInfo);
		}
	}
}

void LatteTexture_GatherTextureRelations(LatteTexture* texture)
{
	for (sint32 mipIndex = 0; mipIndex < texture->mipLevels; mipIndex++)
	{
		sint32 mipSliceCount;
		if (texture->Is3DTexture())
			mipSliceCount = std::max(1, texture->depth >> mipIndex);
		else
			mipSliceCount = texture->depth;
		for (sint32 sliceIndex = 0; sliceIndex < mipSliceCount; sliceIndex++)
		{
			LatteTextureSliceMipInfo* sliceMipInfo = texture->sliceMipInfo + texture->GetSliceMipArrayIndex(sliceIndex, mipIndex);
			loopItrMemOccupancyBuckets(sliceMipInfo->addrStart, sliceMipInfo->addrEnd)
			{
				for (auto& occupancy : list_texMemOccupancyBucket[bucketIndex])
				{
					LatteTexture* itrTexture = occupancy.sliceMipInfo->texture;
					if (itrTexture == texture)
						continue; // ignore self
					if (sliceMipInfo->addrEnd >= occupancy.addrStart && sliceMipInfo->addrStart < occupancy.addrEnd)
					{
						if (sliceMipInfo->addrStart == occupancy.addrStart && sliceMipInfo->subIndex == occupancy.sliceMipInfo->subIndex)
						{
							// overlapping with zero x/y offset
							if (sliceMipInfo->pitch == occupancy.sliceMipInfo->pitch && LatteTexture_IsTexelSizeCompatibleFormat(texture->format, itrTexture->format)
								&& sliceMipInfo->tileMode == occupancy.sliceMipInfo->tileMode &&
								LatteTexture_IsFormatViewCompatible(texture->format, itrTexture->format))
							{
								LatteTexture_TrackTextureRelation(texture, itrTexture);
							}
							else
							{
								// pitch not compatible or format not compatible
							}
						}
						else
						{
							LatteTexture_TrackDataOverlap(texture, sliceMipInfo, occupancy);
						}
					}
				}
			}
		}
	}
}

void LatteTexture_DeleteTextureRelations(LatteTexture* texture)
{
	while (texture->list_compatibleRelations.empty() == false)
	{
		LatteTextureRelation* rel = texture->list_compatibleRelations[0];
		rel->baseTexture->list_compatibleRelations.erase(std::find(rel->baseTexture->list_compatibleRelations.begin(), rel->baseTexture->list_compatibleRelations.end(), rel));
		rel->subTexture->list_compatibleRelations.erase(std::find(rel->subTexture->list_compatibleRelations.begin(), rel->subTexture->list_compatibleRelations.end(), rel));
		free(rel);
	}
	texture->list_compatibleRelations.clear();
}

enum VIEWCOMPATIBILITY
{
	VIEW_COMPATIBLE, // subtexture can be represented as view into base texture
	VIEW_BASE_TOO_SMALL, // base texture must be extended (depth or mip levels) to fit sub texture
	VIEW_NOT_COMPATIBLE,
};

bool IsDimensionCompatibleForGX2View(Latte::E_DIM baseDim, Latte::E_DIM viewDim)
{
	// Note that some combinations depend on the exact view/slice index and count which we currently ignore (like a 3D view of a 3D texture)
	bool isCompatible =
		(baseDim == viewDim) ||
		(baseDim == Latte::E_DIM::DIM_CUBEMAP && viewDim == Latte::E_DIM::DIM_2D) ||
		(baseDim == Latte::E_DIM::DIM_2D && viewDim == Latte::E_DIM::DIM_2D_ARRAY) ||
		(baseDim == Latte::E_DIM::DIM_2D_ARRAY && viewDim == Latte::E_DIM::DIM_2D) ||
		(baseDim == Latte::E_DIM::DIM_CUBEMAP && viewDim == Latte::E_DIM::DIM_2D_ARRAY) ||
		(baseDim == Latte::E_DIM::DIM_2D_ARRAY && viewDim == Latte::E_DIM::DIM_CUBEMAP) ||
		(baseDim == Latte::E_DIM::DIM_3D && viewDim == Latte::E_DIM::DIM_2D_ARRAY);
	if(isCompatible)
		return true;
	// these combinations have been seen in use by games and are considered incompatible:
	// (baseDim == Latte::E_DIM::DIM_2D_ARRAY && viewDim == Latte::E_DIM::DIM_3D) -> Not allowed on OpenGL
	// (baseDim == Latte::E_DIM::DIM_2D && viewDim == Latte::E_DIM::DIM_2D_MSAA)
	// (baseDim == Latte::E_DIM::DIM_2D && viewDim == Latte::E_DIM::DIM_1D)
	// (baseDim == Latte::E_DIM::DIM_2D && viewDim == Latte::E_DIM::DIM_3D)
	// (baseDim == Latte::E_DIM::DIM_3D && viewDim == Latte::E_DIM::DIM_2D)
	// (baseDim == Latte::E_DIM::DIM_3D && viewDim == Latte::E_DIM::DIM_3D) -> Only compatible if the same depth and shared at mip/slice 0
	// (baseDim == Latte::E_DIM::DIM_2D && viewDim == Latte::E_DIM::DIM_CUBEMAP)
	// (baseDim == Latte::E_DIM::DIM_2D_MSAA && viewDim == Latte::E_DIM::DIM_2D)
	// (baseDim == Latte::E_DIM::DIM_1D && viewDim == Latte::E_DIM::DIM_2D)
	return false;
}

VIEWCOMPATIBILITY LatteTexture_CanTextureBeRepresentedAsView(LatteTexture* baseTexture, uint32 physAddr, sint32 width, sint32 height, sint32 pitch, Latte::E_DIM dimView, Latte::E_GX2SURFFMT format, bool isDepth, sint32 firstMip, sint32 numMip, sint32 firstSlice, sint32 numSlice, sint32& relativeMipIndex, sint32& relativeSliceIndex)
{
	relativeMipIndex = 0;
	relativeSliceIndex = 0;
	if (baseTexture->overwriteInfo.hasFormatOverwrite)
	{
		// if the base format is overwritten, then we only allow aliasing if the view format matches the base format
		if (baseTexture->format != format)
			return VIEW_NOT_COMPATIBLE;
	}
	if (LatteTexture_IsFormatViewCompatible(baseTexture->format, format) == false)
		return VIEW_NOT_COMPATIBLE;
	if (baseTexture->physAddress == physAddr && baseTexture->pitch == pitch)
	{
		if (baseTexture->isDepth != isDepth)
			return VIEW_NOT_COMPATIBLE; // depth and non-depth formats are never compatible (on OpenGL)
		if (!LatteTexture_IsTexelSizeCompatibleFormat(baseTexture->format, format) || baseTexture->width != width || baseTexture->height != height)
			return VIEW_NOT_COMPATIBLE;
		// 3D views are only compatible on Vulkan if they match the base texture in regards to mip and slice count
		bool isCompatible3DView = dimView == Latte::E_DIM::DIM_3D && baseTexture->dim == dimView && firstSlice == 0 && firstMip == 0 && baseTexture->mipLevels == numMip && baseTexture->depth == numSlice;
		if (!isCompatible3DView && !IsDimensionCompatibleForGX2View(baseTexture->dim, dimView))
			return VIEW_NOT_COMPATIBLE;
		if (baseTexture->isDepth && baseTexture->format != format)
		{
			// depth view with different format
			cemuLog_logDebug(LogType::Force, "_createMapping(): Incompatible depth view format");
			return VIEW_NOT_COMPATIBLE;
		}

		// AMD has a bug on OpenGL where it ignores the internal format of texture views when they are bound as render targets,
		// as a result we cant use texture views when they have a different format
		if (baseTexture->format != format)
			return VIEW_NOT_COMPATIBLE;

		if ((firstMip + numMip) > baseTexture->mipLevels || (firstSlice + numSlice) > baseTexture->depth)
		{
			// view has more slices or mips than existing texture
			return VIEW_BASE_TOO_SMALL;

		}
		return VIEW_COMPATIBLE;
	}
	else
	{
		if (numMip > 1)
			return VIEW_NOT_COMPATIBLE;
		if (baseTexture->Is3DTexture())
			return VIEW_NOT_COMPATIBLE; // todo - add support for mapping views into 3D textures

		// if phys address or pitch differs then it might be pointing to a mip
		for (sint32 m = 0; m < baseTexture->mipLevels; m++)
		{
			auto sliceMipInfo = baseTexture->sliceMipInfo + baseTexture->GetSliceMipArrayIndex(0, m);
			// check pitch
			if(sliceMipInfo->pitch != pitch)
				continue;
			// check all slices			
			if(LatteAddrLib::TM_IsThickAndMacroTiled(baseTexture->tileMode))
				continue; // todo - check only every 4th slice?
			for (sint32 s=0; s<baseTexture->GetMipDepth(m); s++)
			{
				sliceMipInfo = baseTexture->sliceMipInfo + baseTexture->GetSliceMipArrayIndex(s, m);
				if (sliceMipInfo->addrStart != physAddr || sliceMipInfo->pitch != pitch)
					continue;

				if (baseTexture->isDepth != isDepth)
					return VIEW_NOT_COMPATIBLE;
				if (baseTexture->GetMipWidth(m) != width || baseTexture->GetMipHeight(m) != height)
					return VIEW_NOT_COMPATIBLE;
				if (!LatteTexture_IsTexelSizeCompatibleFormat(baseTexture->format, format) )
					return VIEW_NOT_COMPATIBLE;

				if (!IsDimensionCompatibleForGX2View(baseTexture->dim, dimView))
					return VIEW_NOT_COMPATIBLE;
				if (baseTexture->isDepth && baseTexture->format != format)
				{
					// depth view with different format
					cemuLog_logDebug(LogType::Force, "_createMapping(): Incompatible depth view format");
					return VIEW_NOT_COMPATIBLE;
				}

				// AMD has a bug on OpenGL where it ignores the internal format of texture views when they are bound as render targets,
				// as a result we cant use texture views when they have a different format
				if (baseTexture->format != format)
					return VIEW_NOT_COMPATIBLE;

				if ((m + firstMip + numMip) > baseTexture->mipLevels || (s + firstSlice + numSlice) > baseTexture->depth)
				{
					relativeMipIndex = m;
					relativeSliceIndex = s;
					return VIEW_BASE_TOO_SMALL;
				}
				relativeMipIndex = m;
				relativeSliceIndex = s;
				return VIEW_COMPATIBLE;
			}
		}
	}
	return VIEW_NOT_COMPATIBLE;
}

// deletes any related textures that have become redundant (aka textures that can also be represented entirely as a view into the new texture)
void LatteTexture_DeleteAbsorbedSubtextures(LatteTexture* texture)
{
	for(size_t i=0; i<texture->list_compatibleRelations.size(); i++)
	{
		LatteTextureRelation* textureRelation = texture->list_compatibleRelations[i];
		LatteTexture* relatedTexture = (textureRelation->baseTexture!=texture)? textureRelation->baseTexture:textureRelation->subTexture;

		sint32 relativeMipIndex;
		sint32 relativeSliceIndex;
		if (LatteTexture_CanTextureBeRepresentedAsView(texture, relatedTexture->physAddress, relatedTexture->width, relatedTexture->height, relatedTexture->pitch, relatedTexture->dim, relatedTexture->format, relatedTexture->isDepth, 0, relatedTexture->mipLevels, 0, relatedTexture->depth, relativeMipIndex, relativeSliceIndex) == VIEW_COMPATIBLE)
		{
			LatteTexture_Delete(relatedTexture);
			LatteGPUState.repeatTextureInitialization = true;
		}
	}
}

void LatteTexture_RecreateTextureWithDifferentMipSliceCount(LatteTexture* texture, MPTR physMipAddr, sint32 newMipCount, sint32 newDepth)
{
	Latte::E_DIM newDim = texture->dim;
	if (newDim == Latte::E_DIM::DIM_2D && newDepth > 1)
		newDim = Latte::E_DIM::DIM_2D_ARRAY;
	else if (newDim == Latte::E_DIM::DIM_1D && newDepth > 1)
		newDim = Latte::E_DIM::DIM_1D_ARRAY;
	LatteTextureView* view = LatteTexture_CreateTexture(newDim, texture->physAddress, physMipAddr, texture->format, texture->width, texture->height, newDepth, texture->pitch, newMipCount, texture->swizzle, texture->tileMode, texture->isDepth);
	cemu_assert(!(view->baseTexture->mipLevels <= 1 && physMipAddr == MPTR_NULL && newMipCount > 1));
	// copy data from old texture if its dynamically updated
	if (texture->isUpdatedOnGPU)
	{
		LatteTexture_copyData(texture, view->baseTexture, texture->mipLevels, texture->depth);
		view->baseTexture->isUpdatedOnGPU = true;
	}
	// remove old texture
	LatteTexture_Delete(texture);
	// gather texture relations for new texture
	LatteTexture_GatherTextureRelations(view->baseTexture);
	LatteTexture_UpdateTextureFromDynamicChanges(view->baseTexture);
	// todo - inherit 'isUpdatedOnGPU' flag for each mip/slice

	// delete any individual smaller slices/mips that have become redundant
	LatteTexture_DeleteAbsorbedSubtextures(view->baseTexture);
}

// create new texture representation
// if allowCreateNewDataTexture is true, a new texture will be created if necessary. If it is false, only existing textures may be used, except if a data-compatible version of the requested texture already exists and it's not view compatible (todo - we should differentiate between Latte compatible views and renderer compatible)
// the returned view will map to the provided mip and slice range within the created texture, this is to match the behavior of lookupSliceEx
LatteTextureView* LatteTexture_CreateMapping(MPTR physAddr, MPTR physMipAddr, sint32 width, sint32 height, sint32 depth, sint32 pitch, Latte::E_HWTILEMODE tileMode, uint32 swizzle, sint32 firstMip, sint32 numMip, sint32 firstSlice, sint32 numSlice, Latte::E_GX2SURFFMT format, Latte::E_DIM dimBase, Latte::E_DIM dimView, bool isDepth, bool allowCreateNewDataTexture)
{
	if (format == Latte::E_GX2SURFFMT::INVALID_FORMAT)
	{
		cemuLog_logDebug(LogType::Force, "LatteTexture_CreateMapping(): Invalid format");
		return nullptr;
	}
	// note: When creating an existing texture, we only allow mip and slice expansion at the end
	cemu_assert_debug(depth);
	
	cemu_assert_debug(!(depth > 1 && dimBase == Latte::E_DIM::DIM_2D));
	cemu_assert_debug(!(numSlice > 1 && dimView == Latte::E_DIM::DIM_2D));
	// todo, depth and numSlice are redundant

	sint32 sliceCount = firstSlice + numSlice;
	boost::container::small_vector<LatteTexture*, 16> list_overlappingTextures;
	for (sint32 sliceIndex = 0; sliceIndex < sliceCount; sliceIndex++)
	{
		sint32 mipIndex = 0;
		uint32 calcSliceAddrStart;
		uint32 calcSliceSize;
		sint32 calcSubSliceIndex;
		LatteAddrLib::CalculateMipAndSliceAddr(physAddr, physMipAddr, format, width, height, depth, dimBase, tileMode, swizzle, 0, mipIndex, sliceIndex, &calcSliceAddrStart, &calcSliceSize, &calcSubSliceIndex);
		uint32 calcSliceAddrEnd = calcSliceAddrStart + calcSliceSize;
		// attempt to create view in already existing texture first (we may have to recreate the texture with new specifications)
		loopItrMemOccupancyBuckets(calcSliceAddrStart, calcSliceAddrEnd)
		{
			for (auto& occupancy : list_texMemOccupancyBucket[bucketIndex])
			{
				if (calcSliceAddrEnd >= occupancy.addrStart && calcSliceAddrStart < occupancy.addrEnd)
				{
					if (calcSliceAddrStart == occupancy.addrStart)
					{
						// overlapping with zero x/y offset
						if (std::find(list_overlappingTextures.begin(), list_overlappingTextures.end(), occupancy.sliceMipInfo->texture) == list_overlappingTextures.end())
						{
							list_overlappingTextures.push_back(occupancy.sliceMipInfo->texture);
						}
					}
					else
					{
						// overlapping but not matching directly
						// todo - check if they match with a y offset
					}
				}
			}
		}
	}
	// try to merge textures if possible
	for (auto& tex : list_overlappingTextures)
	{
		sint32 relativeMipIndex;
		sint32 relativeSliceIndex;
		VIEWCOMPATIBILITY viewCompatibility = LatteTexture_CanTextureBeRepresentedAsView(tex, physAddr, width, height, pitch, dimView, format, isDepth, firstMip, numMip, firstSlice, numSlice, relativeMipIndex, relativeSliceIndex);
		if (viewCompatibility == VIEW_NOT_COMPATIBLE)
		{
			allowCreateNewDataTexture = true;
			continue;
		}
		if (viewCompatibility == VIEW_BASE_TOO_SMALL)
		{
			if (relativeMipIndex != 0 || relativeSliceIndex != 0)
			{
				// not yet supported
				allowCreateNewDataTexture = true;
				continue;
			}
			// new mapping has more slices/mips than known texture -> expand texture
			sint32 newDepth = std::max(relativeSliceIndex + firstSlice + numSlice, std::max(depth, tex->depth));
			sint32 newMipCount = std::max(relativeMipIndex + firstMip + numMip, tex->mipLevels);
			uint32 newPhysMipAddr;
			if ((relativeMipIndex + firstMip + numMip) > 1)
			{
				newPhysMipAddr = physMipAddr;
			}
			else
			{
				newPhysMipAddr = tex->physMipAddress;
			}
			LatteTexture_RecreateTextureWithDifferentMipSliceCount(tex, newPhysMipAddr, newMipCount, newDepth);
			return LatteTexture_CreateMapping(physAddr, physMipAddr, width, height, depth, pitch, tileMode, swizzle, firstMip, numMip, firstSlice, numSlice, format, dimBase, dimView, isDepth);
		}
		else if(viewCompatibility == VIEW_COMPATIBLE)
		{
			LatteTextureView* view = tex->GetOrCreateView(dimView, format, relativeMipIndex + firstMip, numMip, relativeSliceIndex + firstSlice, numSlice);
			if (relativeMipIndex != 0 || relativeSliceIndex != 0)
			{
				// for accesses to mips/slices using a physAddress offset we manually need to create a new view lookup
				// by default views only create a lookup for the base texture physAddress
				view->CreateLookupForSubTexture(relativeMipIndex, relativeSliceIndex);
#ifdef CEMU_DEBUG_ASSERT
				LatteTextureView* testView = LatteTextureViewLookupCache::lookup(physAddr, width, height, depth, pitch, firstMip, numMip, firstSlice, numSlice, format, dimView);
				cemu_assert(testView);
#endif
			}
			return view;
		}
		else
		{
			cemu_assert_debug(false);
		}
	}
	// create new texture
	if (allowCreateNewDataTexture == false)
		return nullptr;
	LatteTextureView* view = LatteTexture_CreateTexture(dimBase, physAddr, physMipAddr, format, width, height, depth, pitch, firstMip + numMip, swizzle, tileMode, isDepth);
	LatteTexture* newTexture = view->baseTexture;
	LatteTexture_GatherTextureRelations(view->baseTexture);
	LatteTexture_UpdateTextureFromDynamicChanges(view->baseTexture);
	// delete any individual smaller slices/mips that have become redundant
	LatteTexture_DeleteAbsorbedSubtextures(view->baseTexture);
	// create view
	sint32 relativeMipIndex;
	sint32 relativeSliceIndex;
	VIEWCOMPATIBILITY viewCompatibility = LatteTexture_CanTextureBeRepresentedAsView(newTexture, physAddr, width, height, pitch, dimView, format, isDepth, firstMip, numMip, firstSlice, numSlice, relativeMipIndex, relativeSliceIndex);
	cemu_assert(viewCompatibility == VIEW_COMPATIBLE);
	return view->baseTexture->GetOrCreateView(dimView, format, relativeMipIndex + firstMip, numMip, relativeSliceIndex + firstSlice, numSlice);
}

LatteTextureView* LatteTC_LookupTextureByData(MPTR physAddr, sint32 width, sint32 height, sint32 pitch, sint32 firstMip, sint32 numMip, sint32 firstSlice, sint32 numSlice, sint32* searchIndex)
{
	cemu_assert_debug(firstMip == 0);
	sint32 cSearchIndex = 0;
	loopItrMemOccupancyBuckets(physAddr, physAddr+1)
	{
		auto& bucket = list_texMemOccupancyBucket[bucketIndex];
		for (sint32 i = 0; i < bucket.size(); i++)
		{
			if (bucket[i].addrStart == physAddr)
			{
				LatteTexture* tex = bucket[i].sliceMipInfo->texture;
				if (tex->physAddress == physAddr && tex->pitch == pitch)
				{
					if (firstSlice >= 0 && firstSlice < (tex->depth))
					{
						if (cSearchIndex >= *searchIndex)
						{
							(*searchIndex)++;
							return tex->baseView;
						}
						cSearchIndex++;
					}
				}
			}
		}
	}

	return nullptr;
}

void LatteTC_LookupTexturesByPhysAddr(MPTR physAddr, std::vector<LatteTexture*>& list_textures)
{
	sint32 cSearchIndex = 0;
	loopItrMemOccupancyBuckets(physAddr, physAddr + 1)
	{
		for (sint32 i = 0; i < list_texMemOccupancyBucket[bucketIndex].size(); i++)
		{
			if (list_texMemOccupancyBucket[bucketIndex][i].addrStart == physAddr)
			{
				LatteTexture* tex = list_texMemOccupancyBucket[bucketIndex][i].sliceMipInfo->texture;
				if (tex->physAddress == physAddr)
				{
					vectorAppendUnique(list_textures, tex);
				}
			}
		}
	}
}

LatteTextureView* LatteTC_GetTextureSliceViewOrTryCreate(MPTR srcImagePtr, MPTR srcMipPtr, Latte::E_GX2SURFFMT srcFormat, Latte::E_HWTILEMODE srcTileMode, uint32 srcWidth, uint32 srcHeight, uint32 srcDepth, uint32 srcPitch, uint32 srcSwizzle, uint32 srcSlice, uint32 srcMip, const bool requireExactResolution)
{
	LatteTextureView* sourceView;
	if(requireExactResolution == false)
		sourceView = LatteTextureViewLookupCache::lookupSliceMinSize(srcImagePtr, srcWidth, srcHeight, srcPitch, srcMip, srcSlice, srcFormat);
	else
		sourceView = LatteTextureViewLookupCache::lookupSlice(srcImagePtr, srcWidth, srcHeight, srcPitch, srcMip, srcSlice, srcFormat);
	if (sourceView)
		return sourceView;
	return LatteTexture_CreateMapping(srcImagePtr, srcMipPtr, srcWidth, srcHeight, srcDepth, srcPitch, srcTileMode, srcSwizzle, srcMip, 1, srcSlice, 1, srcFormat, srcDepth > 1 ? Latte::E_DIM::DIM_2D_ARRAY : Latte::E_DIM::DIM_2D, Latte::E_DIM::DIM_2D, false, false);
}

void LatteTexture_UpdateDataToLatest(LatteTexture* texture)
{
	if (LatteTC_HasTextureChanged(texture))
		LatteTexture_ReloadData(texture);

	if (texture->reloadFromDynamicTextures)
	{
		LatteTexture_UpdateCacheFromDynamicTextures(texture);
		texture->reloadFromDynamicTextures = false;
	}
}

LatteTextureSliceMipInfo* LatteTexture::GetSliceMipArrayEntry(sint32 sliceIndex, sint32 mipIndex)
{
	return sliceMipInfo + GetSliceMipArrayIndex(sliceIndex, mipIndex);
}

std::vector<LatteTexture*> sAllTextures; // entries can be nullptr
std::vector<size_t> sAllTextureFreeIndices;

void _AddTextureToGlobalList(LatteTexture* tex)
{
	if (sAllTextureFreeIndices.empty())
	{
		tex->globalListIndex = sAllTextures.size();
		sAllTextures.emplace_back(tex);
		return;
	}
	size_t index = sAllTextureFreeIndices.back();
	sAllTextureFreeIndices.pop_back();
	sAllTextures[index] = tex;
	tex->globalListIndex = index;
}

void _RemoveTextureFromGlobalList(LatteTexture* tex)
{
	cemu_assert_debug(tex->globalListIndex >= 0 && tex->globalListIndex < sAllTextures.size());
	cemu_assert_debug(sAllTextures[tex->globalListIndex] == tex);
	if (tex->globalListIndex + 1 == sAllTextures.size())
	{
		// if the index is at the end, make the list smaller instead of freeing the index
		sAllTextures.pop_back();
		return;
	}
	sAllTextures[tex->globalListIndex] = nullptr;
	sAllTextureFreeIndices.emplace_back(tex->globalListIndex);
}

std::vector<LatteTexture*>& LatteTexture::GetAllTextures()
{
	return sAllTextures;
}

bool LatteTexture_GX2FormatHasStencil(bool isDepth, Latte::E_GX2SURFFMT format)
{
	if (!isDepth)
		return false;
	return format == Latte::E_GX2SURFFMT::D24_S8_UNORM ||
		   format == Latte::E_GX2SURFFMT::D24_S8_FLOAT ||
		   format == Latte::E_GX2SURFFMT::D32_S8_FLOAT;
}

LatteTexture::LatteTexture(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle,
	Latte::E_HWTILEMODE tileMode, bool isDepth)
{
	_AddTextureToGlobalList(this);
	if (depth < 1)
		depth = 1;
	// setup texture object
	this->physAddress = physAddress;
	this->dim = dim;
	this->format = format;
	this->width = width;
	this->height = height;
	this->depth = depth;
	this->swizzle = swizzle;
	this->pitch = pitch;
	this->mipLevels = mipLevels;
	this->tileMode = tileMode;
	this->isDepth = isDepth;
	this->hasStencil = LatteTexture_GX2FormatHasStencil(isDepth, format);
	this->physMipAddress = physMipAddress;
	this->lastUpdateEventCounter = LatteTexture_getNextUpdateEventCounter();
	this->lastWriteEventCounter = LatteTexture_getNextUpdateEventCounter();

	// handle graphic pack overwrite rules
	for (const auto& gp : GraphicPack2::GetActiveGraphicPacks())
	{
		for (const auto& rule : gp->GetTextureRules())
		{
			if (!rule.filter_settings.format_whitelist.empty() && std::find(rule.filter_settings.format_whitelist.begin(), rule.filter_settings.format_whitelist.end(), (uint32)format) == rule.filter_settings.format_whitelist.end())
				continue;

			if (!rule.filter_settings.format_blacklist.empty() && std::find(rule.filter_settings.format_blacklist.begin(), rule.filter_settings.format_blacklist.end(), (uint32)format) != rule.filter_settings.format_blacklist.end())
				continue;

			if (!rule.filter_settings.tilemode_whitelist.empty() && std::find(rule.filter_settings.tilemode_whitelist.begin(), rule.filter_settings.tilemode_whitelist.end(), (int)tileMode) == rule.filter_settings.tilemode_whitelist.end())
				continue;

			if (!rule.filter_settings.tilemode_blacklist.empty() && std::find(rule.filter_settings.tilemode_blacklist.begin(), rule.filter_settings.tilemode_blacklist.end(), (int)tileMode) != rule.filter_settings.tilemode_blacklist.end())
				continue;

			if (rule.filter_settings.width != -1 && rule.filter_settings.width != width)
				continue;
			if (rule.filter_settings.height != -1 && rule.filter_settings.height != height)
				continue;
			if (rule.filter_settings.depth != -1 && rule.filter_settings.depth != depth)
				continue;
			if (rule.filter_settings.inMEM1 == GraphicPack2::TextureRule::FILTER_SETTINGS::MEM1_FILTER::OUTSIDE && mmuRange_MEM1.containsAddress(this->physAddress))
				continue;
			if (rule.filter_settings.inMEM1 == GraphicPack2::TextureRule::FILTER_SETTINGS::MEM1_FILTER::INSIDE && !mmuRange_MEM1.containsAddress(this->physAddress))
				continue;

			this->overwriteInfo.width = width;
			this->overwriteInfo.height = height;
			this->overwriteInfo.depth = depth;

			if (rule.overwrite_settings.width != -1)
			{
				this->overwriteInfo.hasResolutionOverwrite = true;
				this->overwriteInfo.width = rule.overwrite_settings.width;
			}
			if (rule.overwrite_settings.height != -1)
			{
				this->overwriteInfo.hasResolutionOverwrite = true;
				this->overwriteInfo.height = rule.overwrite_settings.height;
			}
			if (rule.overwrite_settings.depth != -1)
			{
				this->overwriteInfo.hasResolutionOverwrite = true;
				this->overwriteInfo.depth = rule.overwrite_settings.depth;
			}
			if (rule.overwrite_settings.format != -1)
			{
				this->overwriteInfo.hasFormatOverwrite = true;
				this->overwriteInfo.format = rule.overwrite_settings.format;
			}
			if (rule.overwrite_settings.lod_bias != -1)
			{
				this->overwriteInfo.hasLodBias = true;
				this->overwriteInfo.lodBias = rule.overwrite_settings.lod_bias;
			}
			if (rule.overwrite_settings.relative_lod_bias != -1)
			{
				this->overwriteInfo.hasRelativeLodBias = true;
				this->overwriteInfo.relativeLodBias = rule.overwrite_settings.relative_lod_bias;
			}
			if (rule.overwrite_settings.anistropic_value != -1)
			{
				this->overwriteInfo.anisotropicLevel = rule.overwrite_settings.anistropic_value;
			}
		}
	}
	// determine if this texture should ever be mirrored to CPU RAM
	if (this->tileMode == Latte::E_HWTILEMODE::TM_LINEAR_ALIGNED)
	{
		this->enableReadback = true;
	}
}

LatteTexture::~LatteTexture()
{
	_RemoveTextureFromGlobalList(this);
	cemu_assert_debug(baseView == nullptr);
	cemu_assert_debug(views.empty());
};

// sync texture data between overlapping textures
void LatteTexture_UpdateCacheFromDynamicTextures(LatteTexture* textureDest)
{
	LatteTexture_UpdateTextureFromDynamicChanges(textureDest);
}

void LatteTexture_MarkConnectedTexturesForReloadFromDynamicTextures(LatteTexture* texture)
{
	for (auto& it : texture->list_compatibleRelations)
	{
		if (texture == it->baseTexture)
			it->subTexture->reloadFromDynamicTextures = true;
		else
			it->baseTexture->reloadFromDynamicTextures = true;
	}
}

void LatteTexture_TrackTextureGPUWrite(LatteTexture* texture, uint32 slice, uint32 mip, uint64 eventCounter)
{
	LatteTexture_MarkDynamicTextureAsChanged(texture->baseView, slice, mip, eventCounter);
	LatteTC_ResetTextureChangeTracker(texture);
	texture->isUpdatedOnGPU = true;
	texture->lastUnflushedRTDrawcallIndex = LatteGPUState.drawCallCounter;
}
