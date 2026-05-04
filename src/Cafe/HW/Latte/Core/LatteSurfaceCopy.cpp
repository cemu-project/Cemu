#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteDefaultShaders.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "Cafe/HW/Latte/Core/LatteSurfaceCopy.h"

#include "Cafe/HW/Latte/Renderer/Renderer.h"

/* Surface copies are tricky to handle because we simulate unified memory on top of two separate memory systems: RAM and VRAM
 * Cemu may only have texture data in either RAM or VRAM, or both. And doing transfers to or from CPU can quickly become prohibitively expensive
 * So we need to make best guesses on where the data needs to come from and where it needs to go
 * A complicating factor is that texture copies are more like memcpy, in the sense that they don't care about the actual underlying pixel format
 * For example a R32F texture can be copied as RGBA8. Similiarly, different tile modes can sometimes be identical under specific circumstances
 */

void gx2SurfaceCopySoftware(
	uint8* inputData, sint32 surfSrcHeight, sint32 srcPitch, sint32 srcDepth, uint32 srcSlice, uint32 srcSwizzle, uint32 srcHwTileMode,
	uint8* outputData, sint32 surfDstHeight, sint32 dstPitch, sint32 dstDepth, uint32 dstSlice, uint32 dstSwizzle, uint32 dstHwTileMode,
	uint32 copyWidth, uint32 copyHeight, uint32 copyBpp);

void LatteSurfaceCopy_CopyInRAM(const LatteSurfaceCopyParam& src, const LatteSurfaceCopyParam& dst, const LatteSurfaceCopyRect& rect)
{
	Latte::E_HWSURFFMT dstHwFormat = Latte::GetHWFormat(dst.surfaceFormat);

	sint32 copyWidth = rect.width;
	sint32 copyHeight = rect.height;
	if (Latte::IsCompressedFormat(dstHwFormat))
	{
		copyWidth = (copyWidth + 3) / 4;
		copyHeight = (copyHeight + 3) / 4;
	}

	uint32 dstBpp = Latte::GetFormatBits(dstHwFormat);

	gx2SurfaceCopySoftware((uint8*)MEMPTR<void>(src.physDataAddr).GetPtr(), src.heightInTexels, src.pitch, 1, src.sliceIndex, src.swizzle, (uint32)src.tilemode,
		(uint8*)MEMPTR<void>(dst.physDataAddr).GetPtr(), dst.heightInTexels, dst.pitch, 1, dst.sliceIndex, dst.swizzle, (uint32)dst.tilemode,
		copyWidth, copyHeight, dstBpp);
}

void LatteSurfaceCopy_copySurfaceNew(const LatteSurfaceCopyParam& src, const LatteSurfaceCopyParam& dst, const LatteSurfaceCopyRect& rect)
{
	cemu_assert_debug(rect.x == 0 && rect.y == 0); // origin offset not yet supported

	cemu_assert_debug((rect.x + rect.width) <= dst.pitch * (Latte::IsCompressedFormat(dst.surfaceFormat)?4:1));
	cemu_assert_debug((rect.x + rect.width) <= src.pitch * (Latte::IsCompressedFormat(src.surfaceFormat)?4:1));

	if (src.tilemode == Latte::E_GX2TILEMODE::TM_LINEAR_SPECIAL || dst.tilemode == Latte::E_GX2TILEMODE::TM_LINEAR_SPECIAL)
	{
		// todo - it's technically possible for a matching linear texture to be in the texture cache already
		//        there is also a case of tiled to linear_special where we should trigger a readback without an actual destination texture
		LatteSurfaceCopy_CopyInRAM(src, dst, rect);
		return;
	}

	// look up source texture
	// todo - for non-zero slices heightInTexels matters (used to calculate the slice size). We should take this into account during lookup
	LatteTexture* sourceTexture = nullptr;
	LatteTextureView* sourceView = LatteTC_GetTextureSliceViewOrTryCreate(src.physDataAddr, MPTR_NULL, src.surfaceFormat, Latte::MakeHWTileMode(src.tilemode), rect.x + rect.width, rect.y + rect.height, 1, src.pitch, src.swizzle, src.sliceIndex, 0);
	if (sourceView == nullptr)
	{
		// source texture doesn't exist (yet) in texture cache
		// operate on RAM instead
		LatteSurfaceCopy_CopyInRAM(src, dst, rect);
		return;
	}
	sourceTexture = sourceView->baseTexture;
	if (sourceTexture->reloadFromDynamicTextures)
	{
		LatteTexture_UpdateCacheFromDynamicTextures(sourceTexture);
		sourceTexture->reloadFromDynamicTextures = false;
	}
	// look up destination texture
	LatteTexture* destinationTexture = nullptr;
	LatteTextureView* destinationView = LatteTextureViewLookupCache::lookupSliceMinSize(dst.physDataAddr, rect.x + rect.width, rect.y + rect.height, dst.pitch, 0, dst.sliceIndex, dst.surfaceFormat);
	// todo - Instead of lookupSliceMinSize lookup the base texture by data range instead and return mip/slice index
	if (destinationView)
		destinationTexture = destinationView->baseTexture;
	// create destination texture if it doesnt exist
	if (!destinationTexture)
	{
		destinationView = LatteTexture_CreateMapping(dst.physDataAddr, MPTR_NULL, rect.x + rect.width, rect.y + rect.height, 1, dst.pitch, Latte::MakeHWTileMode(dst.tilemode), dst.swizzle, 0, 1, dst.sliceIndex, 1, dst.surfaceFormat, dst.dim, Latte::IsMSAA(dst.dim) ? Latte::E_DIM::DIM_2D_MSAA : Latte::E_DIM::DIM_2D, false);
		destinationTexture = destinationView->baseTexture;
	}
	// copy texture
	if (sourceTexture && destinationTexture)
	{
		// mark source and destination texture as still in use
		LatteTC_MarkTextureStillInUse(destinationTexture);
		LatteTC_MarkTextureStillInUse(sourceTexture);
		sint32 realSrcSlice = src.sliceIndex;
		if (LatteTexture_doesEffectiveRescaleRatioMatch(sourceTexture, sourceView->firstMip, destinationTexture, destinationView->firstMip))
		{
			cemu_assert_debug(rect.x == 0 && rect.y == 0);
			sint32 copyWidth = rect.width;
			sint32 copyHeight = rect.height;
			sint32 effectiveCopyWidth = copyWidth;
			sint32 effectiveCopyHeight = copyHeight;
			LatteTexture_scaleToEffectiveSize(sourceTexture, &effectiveCopyWidth, &effectiveCopyHeight, 0);
			// copy slice
			if (sourceView->baseTexture->isDepth != destinationView->baseTexture->isDepth)
				g_renderer->surfaceCopy_copySurfaceWithFormatConversion(sourceTexture, sourceView->firstMip, sourceView->firstSlice, destinationTexture, destinationView->firstMip, destinationView->firstSlice, copyWidth, copyHeight);
			else
				g_renderer->texture_copyImageSubData(sourceTexture, sourceView->firstMip, 0, 0, realSrcSlice, destinationTexture, destinationView->firstMip, 0, 0, destinationView->firstSlice, effectiveCopyWidth, effectiveCopyHeight, 1);
			const uint64 eventCounter = LatteTexture_getNextUpdateEventCounter();
			LatteTexture_MarkDynamicTextureAsChanged(destinationTexture->baseView, destinationView->firstSlice, destinationView->firstMip, eventCounter);
		}
		else
		{
			debug_printf("gx2CP_itHLECopySurface(): Copy texture with non-matching effective size\n");
		}
		LatteTC_ResetTextureChangeTracker(destinationTexture);
		// flag texture as updated
		destinationTexture->lastUpdateEventCounter = LatteTexture_getNextUpdateEventCounter();
		destinationTexture->isUpdatedOnGPU = true; // todo - also track update flag per-slice
	}
	else
		debug_printf("Source or destination texture does not exist\n");
	// if the texture is updated from a tiled to a linear format it's a strong indicator for CPU reads
	// in which case we should sync the texture back to CPU RAM
	const bool sourceIsLinear = sourceTexture->tileMode == Latte::E_HWTILEMODE::TM_LINEAR_ALIGNED || sourceTexture->tileMode == Latte::E_HWTILEMODE::TM_LINEAR_GENERAL;
	const bool destinationIsLinear = destinationTexture->tileMode == Latte::E_HWTILEMODE::TM_LINEAR_ALIGNED || destinationTexture->tileMode == Latte::E_HWTILEMODE::TM_LINEAR_GENERAL;
	bool shouldReadback = !sourceIsLinear && destinationIsLinear;
	// special case for Bayonetta 2
	if (destinationTexture->width == 8 && destinationTexture->height == 8 && destinationTexture->tileMode == Latte::E_HWTILEMODE::TM_1D_TILED_THIN1)
	{
		cemuLog_logDebug(LogType::Force, "Texture readback after copy for Bayonetta 2 (phys: 0x{:08x})", destinationTexture->physAddress);
		shouldReadback = true;
	}
	if (shouldReadback)
	{
		LatteTextureReadback_Initate(destinationView);
	}
}
