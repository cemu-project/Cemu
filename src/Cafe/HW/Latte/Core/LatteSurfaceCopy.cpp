#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteDefaultShaders.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"

#include "Cafe/HW/Latte/Renderer/Renderer.h"

void LatteSurfaceCopy_copySurfaceNew(MPTR srcPhysAddr, MPTR srcMipAddr, uint32 srcSwizzle, Latte::E_GX2SURFFMT srcSurfaceFormat, sint32 srcWidth, sint32 srcHeight, sint32 srcDepth, uint32 srcPitch, sint32 srcSlice, Latte::E_DIM srcDim, Latte::E_HWTILEMODE srcTilemode, sint32 srcAA, sint32 srcLevel, MPTR dstPhysAddr, MPTR dstMipAddr, uint32 dstSwizzle, Latte::E_GX2SURFFMT dstSurfaceFormat, sint32 dstWidth, sint32 dstHeight, sint32 dstDepth, uint32 dstPitch, sint32 dstSlice, Latte::E_DIM dstDim, Latte::E_HWTILEMODE dstTilemode, sint32 dstAA, sint32 dstLevel)
{
	// check if source is within valid mip range
	if (srcDim == Latte::E_DIM::DIM_3D && (srcDepth >> srcLevel) == 0 && (srcWidth >> srcLevel) == 0 && (srcHeight >> srcLevel) == 0)
		return;
	else if ((srcWidth >> srcLevel) == 0 && (srcHeight >> srcLevel) == 0)
		return;
	// look up source texture
	LatteTexture* sourceTexture = nullptr;
	LatteTextureView* sourceView = LatteTC_GetTextureSliceViewOrTryCreate(srcPhysAddr, srcMipAddr, srcSurfaceFormat, srcTilemode, srcWidth, srcHeight, srcDepth, srcPitch, srcSwizzle, srcSlice, srcLevel);
	if (sourceView == nullptr)
	{
		debug_printf("HLECopySurface(): Source texture is not in list of dynamic textures\n");
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
	LatteTextureView* destinationView = LatteTextureViewLookupCache::lookupSlice(dstPhysAddr, dstWidth, dstHeight, dstPitch, dstLevel, dstSlice, dstSurfaceFormat);
	if (destinationView)
		destinationTexture = destinationView->baseTexture;

	// create destination texture if it doesnt exist
	if (!destinationTexture)
	{
		LatteTexture* renderTargetConf = nullptr;
		destinationView = LatteTexture_CreateMapping(dstPhysAddr, dstMipAddr, dstWidth, dstHeight, dstDepth, dstPitch, dstTilemode, dstSwizzle, dstLevel, 1, dstSlice, 1, dstSurfaceFormat, dstDim, Latte::IsMSAA(dstDim) ? Latte::E_DIM::DIM_2D_MSAA : Latte::E_DIM::DIM_2D, false);
		destinationTexture = destinationView->baseTexture;
	}
	// copy texture
	if (sourceTexture && destinationTexture)
	{
		// mark source and destination texture as still in use
		LatteTC_MarkTextureStillInUse(destinationTexture);
		LatteTC_MarkTextureStillInUse(sourceTexture);
		sint32 realSrcSlice = srcSlice;
		if (LatteTexture_doesEffectiveRescaleRatioMatch(sourceTexture, sourceView->firstMip, destinationTexture, destinationView->firstMip))
		{
			// adjust copy size
			sint32 copyWidth = std::max(srcWidth >> srcLevel, 1);
			sint32 copyHeight = std::max(srcHeight >> srcLevel, 1);
			// use the smaller width/height as copy size
			copyWidth = std::min(copyWidth, std::max(dstWidth >> dstLevel, 1));
			copyHeight = std::min(copyHeight, std::max(dstHeight >> dstLevel, 1));
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

	// download destination texture if it matches known accessed formats
	if (destinationTexture->width == 8 && destinationTexture->height == 8 && destinationTexture->tileMode == Latte::E_HWTILEMODE::TM_1D_TILED_THIN1)
	{
		cemuLog_logDebug(LogType::Force, "Texture readback after copy for Bayonetta 2 (phys: 0x{:08x})", destinationTexture->physAddress);
		LatteTextureReadback_Initate(destinationView);
	}
}
