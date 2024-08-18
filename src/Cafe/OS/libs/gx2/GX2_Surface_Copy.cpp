#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteAsyncCommands.h"
#include "Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.h"
#include "util/highresolutiontimer/HighResolutionTimer.h"
#include "GX2.h"
#include "GX2_Resource.h"

template<uint32 copyBpp>
void gx2SurfaceCopySoftware_specialized(
	uint8* inputData, sint32 surfSrcHeight, sint32 srcPitch, sint32 srcDepth, uint32 srcSlice, uint32 srcSwizzle, uint32 srcHwTileMode,
	uint8* outputData, sint32 surfDstHeight, sint32 dstPitch, sint32 dstDepth, uint32 dstSlice, uint32 dstSwizzle, uint32 dstHwTileMode,
	uint32 copyWidth, uint32 copyHeight)
{
	uint32 srcPipeSwizzle = (srcSwizzle >> 8) & 1;
	uint32 srcBankSwizzle = ((srcSwizzle >> 9) & 3);
	uint32 dstPipeSwizzle = (dstSwizzle >> 8) & 1;
	uint32 dstBankSwizzle = ((dstSwizzle >> 9) & 3);

	for (uint32 y = 0; y < copyHeight; y++)
	{
		for (uint32 x = 0; x < copyWidth; x++)
		{
			// calculate address of input block
			uint32 srcOffset = 0;
			if (srcHwTileMode == 0 || srcHwTileMode == 1)
				srcOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordLinear(x, y, srcSlice, 0, copyBpp, srcPitch, surfSrcHeight, srcDepth);
			else if (srcHwTileMode == 2 || srcHwTileMode == 3)
				srcOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordMicroTiled(x, y, srcSlice, copyBpp, srcPitch, surfSrcHeight, (Latte::E_HWTILEMODE)srcHwTileMode, false);
			else
				srcOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordMacroTiled(x, y, srcSlice, 0, copyBpp, srcPitch, surfSrcHeight, 1 * 1, (Latte::E_HWTILEMODE)srcHwTileMode, false, srcPipeSwizzle, srcBankSwizzle);
			uint8* inputBlockData = inputData + srcOffset;
			// calculate address of output block
			uint32 dstOffset = 0;
			if (dstHwTileMode == 0 || dstHwTileMode == 1)
				dstOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordLinear(x, y, dstSlice, 0, copyBpp, dstPitch, surfDstHeight, dstDepth);
			else if (dstHwTileMode == 2 || dstHwTileMode == 3)
				dstOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordMicroTiled(x, y, dstSlice, copyBpp, dstPitch, surfDstHeight, (Latte::E_HWTILEMODE)dstHwTileMode, false);
			else
				dstOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordMacroTiled(x, y, dstSlice, 0, copyBpp, dstPitch, surfDstHeight, 1 * 1, (Latte::E_HWTILEMODE)dstHwTileMode, false, dstPipeSwizzle, dstBankSwizzle);
			uint8* outputBlockData = outputData + dstOffset;

			if constexpr (copyBpp == 8)
			{
				outputBlockData[0] = inputBlockData[0];
			}
			else if constexpr (copyBpp == 16)
			{
				*(uint16*)outputBlockData = *(uint16*)inputBlockData;
			}
			else if constexpr (copyBpp == 32)
			{
				*(uint32*)outputBlockData = *(uint32*)inputBlockData;
			}
			else if constexpr (copyBpp == 64)
			{
				*(uint32*)(outputBlockData + 0) = *(uint32*)(inputBlockData + 0);
				*(uint32*)(outputBlockData + 4) = *(uint32*)(inputBlockData + 4);
			}
			else if constexpr (copyBpp == 128)
			{
				*(uint32*)(outputBlockData + 0) = *(uint32*)(inputBlockData + 0);
				*(uint32*)(outputBlockData + 4) = *(uint32*)(inputBlockData + 4);
				*(uint32*)(outputBlockData + 8) = *(uint32*)(inputBlockData + 8);
				*(uint32*)(outputBlockData + 12) = *(uint32*)(inputBlockData + 12);
			}
		}
	}
}

// fast copy for tilemode 4 to tilemode 4
// assumes aa 1
// this only supports the cases where every micro tile fits within 256 bytes (group size)
// we could accelerate this even further if we copied whole macro blocks
void gx2SurfaceCopySoftware_fastPath_tm4Copy(uint8* inputData, sint32 surfSrcHeight, sint32 srcPitch, sint32 srcDepth, uint32 srcSlice, uint32 srcSwizzle,
	uint8* outputData, sint32 surfDstHeight, sint32 dstPitch, sint32 dstDepth, uint32 dstSlice, uint32 dstSwizzle,
	uint32 copyWidth, uint32 copyHeight, uint32 copyBpp)
{
	cemu_assert_debug((copyWidth & 7) == 0);
	cemu_assert_debug((copyHeight & 7) == 0);

	uint32 srcPipeSwizzle = (srcSwizzle >> 8) & 1;
	uint32 srcBankSwizzle = ((srcSwizzle >> 9) & 3);
	uint32 dstPipeSwizzle = (dstSwizzle >> 8) & 1;
	uint32 dstBankSwizzle = ((dstSwizzle >> 9) & 3);

	uint32 texelBytes = copyBpp / 8;

	if (srcSlice == dstSlice && srcSwizzle == dstSwizzle && surfSrcHeight == surfDstHeight && srcPitch == dstPitch)
	{
		// shared tile offsets
		for (uint32 y = 0; y < copyHeight; y += 8)
		{
			for (uint32 x = 0; x < copyWidth; x += 8)
			{
				// copy 8x8 micro tile
				uint32 offset = LatteAddrLib::ComputeSurfaceAddrFromCoordMacroTiled(x, y, srcSlice, 0, copyBpp, srcPitch, surfSrcHeight, 1 * 1, Latte::E_HWTILEMODE::TM_2D_TILED_THIN1, false, srcPipeSwizzle, srcBankSwizzle);
				uint8* inputBlockData = inputData + offset;
				uint8* outputBlockData = outputData + offset;
				memcpy(outputBlockData, inputBlockData, texelBytes * (8 * 8));
			}
		}
	}
	else
	{
		// separate tile offsets
		for (uint32 y = 0; y < copyHeight; y += 8)
		{
			for (uint32 x = 0; x < copyWidth; x += 8)
			{
				// copy 8x8 micro tile
				uint32 srcOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordMacroTiled(x, y, srcSlice, 0, copyBpp, srcPitch, surfSrcHeight, 1 * 1, Latte::E_HWTILEMODE::TM_2D_TILED_THIN1, false, srcPipeSwizzle, srcBankSwizzle);
				uint8* inputBlockData = inputData + srcOffset;
				uint32 dstOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordMacroTiled(x, y, dstSlice, 0, copyBpp, dstPitch, surfDstHeight, 1 * 1, Latte::E_HWTILEMODE::TM_2D_TILED_THIN1, false, dstPipeSwizzle, dstBankSwizzle);
				uint8* outputBlockData = outputData + dstOffset;
				memcpy(outputBlockData, inputBlockData, texelBytes * (8 * 8));
			}
		}
	}
}

void gx2SurfaceCopySoftware(
	uint8* inputData, sint32 surfSrcHeight, sint32 srcPitch, sint32 srcDepth, uint32 srcSlice, uint32 srcSwizzle, uint32 srcHwTileMode,
	uint8* outputData, sint32 surfDstHeight, sint32 dstPitch, sint32 dstDepth, uint32 dstSlice, uint32 dstSwizzle, uint32 dstHwTileMode,
	uint32 copyWidth, uint32 copyHeight, uint32 copyBpp)
{
	if (srcHwTileMode == 4 && dstHwTileMode == 4 && (copyWidth & 7) == 0 && (copyHeight & 7) == 0 && copyBpp <= 32) // todo - check sample == 1
	{
		gx2SurfaceCopySoftware_fastPath_tm4Copy(inputData, surfSrcHeight, srcPitch, srcDepth, srcSlice, srcSwizzle, outputData, surfDstHeight, dstPitch, dstDepth, dstSlice, dstSwizzle, copyWidth, copyHeight, copyBpp);
		return;
	}

	if (copyBpp == 8)
		gx2SurfaceCopySoftware_specialized<8>(inputData, surfSrcHeight, srcPitch, srcDepth, srcSlice, srcSwizzle, srcHwTileMode, outputData, surfDstHeight, dstPitch, dstDepth, dstSlice, dstSwizzle, dstHwTileMode, copyWidth, copyHeight);
	else if (copyBpp == 16)
		gx2SurfaceCopySoftware_specialized<16>(inputData, surfSrcHeight, srcPitch, srcDepth, srcSlice, srcSwizzle, srcHwTileMode, outputData, surfDstHeight, dstPitch, dstDepth, dstSlice, dstSwizzle, dstHwTileMode, copyWidth, copyHeight);
	else if (copyBpp == 32)
		gx2SurfaceCopySoftware_specialized<32>(inputData, surfSrcHeight, srcPitch, srcDepth, srcSlice, srcSwizzle, srcHwTileMode, outputData, surfDstHeight, dstPitch, dstDepth, dstSlice, dstSwizzle, dstHwTileMode, copyWidth, copyHeight);
	else if (copyBpp == 64)
		gx2SurfaceCopySoftware_specialized<64>(inputData, surfSrcHeight, srcPitch, srcDepth, srcSlice, srcSwizzle, srcHwTileMode, outputData, surfDstHeight, dstPitch, dstDepth, dstSlice, dstSwizzle, dstHwTileMode, copyWidth, copyHeight);
	else if (copyBpp == 128)
		gx2SurfaceCopySoftware_specialized<128>(inputData, surfSrcHeight, srcPitch, srcDepth, srcSlice, srcSwizzle, srcHwTileMode, outputData, surfDstHeight, dstPitch, dstDepth, dstSlice, dstSwizzle, dstHwTileMode, copyWidth, copyHeight);
	else
		cemu_assert_debug(false);
}

void gx2Surface_GX2CopySurface(GX2Surface* srcSurface, uint32 srcMip, uint32 srcSlice, GX2Surface* dstSurface, uint32 dstMip, uint32 dstSlice)
{
	sint32 dstWidth = dstSurface->width;
	sint32 dstHeight = dstSurface->height;
	sint32 srcWidth = srcSurface->width;
	sint32 srcHeight = srcSurface->height;

	sint32 dstMipWidth = std::max(dstWidth>>dstMip, 1);
	sint32 dstMipHeight = std::max(dstHeight>>dstMip, 1);
	sint32 srcMipWidth = std::max(srcWidth>>srcMip, 1);
	sint32 srcMipHeight = std::max(srcHeight>>srcMip, 1);

	if( dstMipWidth != srcMipWidth || dstMipHeight != srcMipHeight )
	{
		cemu_assert_debug(false);
		return;
	}
	// handle format
	Latte::E_GX2SURFFMT srcFormat = srcSurface->format;
	Latte::E_GX2SURFFMT dstFormat = dstSurface->format;
	uint32 srcBPP = Latte::GetFormatBits(srcFormat);
	uint32 dstBPP = Latte::GetFormatBits(dstFormat);
	auto srcHwFormat = Latte::GetHWFormat(srcFormat);
	auto dstHwFormat = Latte::GetHWFormat(dstFormat);
	// get texture info
	LatteAddrLib::AddrSurfaceInfo_OUT surfOutSrc = {0};
	GX2::GX2CalculateSurfaceInfo(srcSurface, srcMip, &surfOutSrc);
	LatteAddrLib::AddrSurfaceInfo_OUT surfOutDst = {0};
	GX2::GX2CalculateSurfaceInfo(dstSurface, dstMip, &surfOutDst);
	// check parameters
	if (srcSurface->numLevels == 0)
	{
		debug_printf("GX2CopySurface(): mip count is 0\n");
		return;
	}
	// get input pointer
	uint8* inputData = NULL;
	cemu_assert(srcMip < srcSurface->numLevels);
	if( srcMip == 0 )
		inputData = (uint8*)memory_getPointerFromVirtualOffset(srcSurface->imagePtr);
	else if( srcMip == 1 )
		inputData = (uint8*)memory_getPointerFromVirtualOffset(srcSurface->mipPtr);
	else
	{
		inputData = (uint8*)memory_getPointerFromVirtualOffset(srcSurface->mipPtr + srcSurface->mipOffset[srcMip - 1]);
	}
	// get output pointer
	uint8* outputData = NULL;
	cemu_assert(dstMip < dstSurface->numLevels);
	if( dstMip == 0 )
		outputData = (uint8*)memory_getPointerFromVirtualOffset(dstSurface->imagePtr);
	else if( dstMip == 1 )
		outputData = (uint8*)memory_getPointerFromVirtualOffset(dstSurface->mipPtr);
	else
	{
		outputData = (uint8*)memory_getPointerFromVirtualOffset(dstSurface->mipPtr + dstSurface->mipOffset[dstMip - 1]);
	}
	
	if( srcHwFormat != dstHwFormat )
	{
		// mismatching format
		cemuLog_logDebug(LogType::Force, "GX2CopySurface(): Format mismatch");
		return;
	}

	// note: Do not trust values from the input GX2Surface* structs but rely on surfOutDst/surfOutSrc instead if possible.
	// src
	uint32 srcPitch = surfOutSrc.pitch;
	uint32 srcSwizzle = srcSurface->swizzle;
	uint32 srcHwTileMode = (uint32)surfOutSrc.hwTileMode;
	uint32 srcDepth = std::max<uint32>(surfOutSrc.depth, 1);
	if (srcHwTileMode == 0) // linear
	{
		srcPitch = srcSurface->pitch >> srcMip;
		srcPitch = std::max<uint32>(srcPitch, 1);
	}
	// dst
	uint32 dstPitch = surfOutDst.pitch;
	uint32 dstSwizzle = dstSurface->swizzle;
	uint32 dstHwTileMode = (uint32)surfOutDst.hwTileMode;
	uint32 dstDepth = std::max<uint32>(surfOutDst.depth, 1);
	uint32 dstBpp = surfOutDst.bpp;

	//debug_printf("Src Tex: %08X %dx%d Swizzle: %08x tm: %d fmt: %04x use: %02x\n", _swapEndianU32(srcSurface->imagePtr), _swapEndianU32(srcSurface->width), _swapEndianU32(srcSurface->height), _swapEndianU32(srcSurface->swizzle), _swapEndianU32(srcSurface->tileMode), _swapEndianU32(srcSurface->format), (uint32)srcSurface->resFlag);
	//debug_printf("Dst Tex: %08X %dx%d Swizzle: %08x tm: %d fmt: %04x use: %02x\n", _swapEndianU32(dstSurface->imagePtr), _swapEndianU32(dstSurface->width), _swapEndianU32(dstSurface->height), _swapEndianU32(dstSurface->swizzle), _swapEndianU32(dstSurface->tileMode), _swapEndianU32(dstSurface->format), (uint32)dstSurface->resFlag);

	bool requestGPURAMCopy = false;
	bool debugTestForceCPUCopy = false;

	if (srcSurface->tileMode == Latte::E_GX2TILEMODE::TM_LINEAR_SPECIAL && dstSurface->tileMode == Latte::E_GX2TILEMODE::TM_2D_TILED_THIN1)
		debugTestForceCPUCopy = true;

	if (srcSurface->tileMode == Latte::E_GX2TILEMODE::TM_2D_TILED_THIN1 && dstSurface->tileMode == Latte::E_GX2TILEMODE::TM_LINEAR_SPECIAL )
	{
		LatteAsyncCommands_queueForceTextureReadback(
			srcSurface->imagePtr,
			srcSurface->mipPtr,
			srcSurface->swizzle,
			(uint32)srcSurface->format.value(),
			srcSurface->width,
			srcSurface->height,
			srcSurface->depth,
			srcSurface->pitch,
			srcSlice,
			(uint32)srcSurface->dim.value(),
			Latte::MakeHWTileMode(srcSurface->tileMode),
			srcSurface->aa,
			srcMip);

		LatteAsyncCommands_waitUntilAllProcessed();

		debugTestForceCPUCopy = true;
	}

	// send copy command to GPU
	if( srcHwTileMode > 0 && srcHwTileMode < 16 && dstHwTileMode > 0 && dstHwTileMode < 16 || requestGPURAMCopy )
	{
		GX2ReserveCmdSpace(1+13*2);

		gx2WriteGather_submit(pm4HeaderType3(IT_HLE_COPY_SURFACE_NEW, 13*2),
		// src
			(uint32)srcSurface->imagePtr,
			(uint32)srcSurface->mipPtr,
			(uint32)srcSurface->swizzle,
			(uint32)srcSurface->format.value(),
			(uint32)srcSurface->width,
			(uint32)srcSurface->height,
			(uint32)srcSurface->depth,
			(uint32)srcSurface->pitch,
			srcSlice,
			(uint32)srcSurface->dim.value(),
			(uint32)srcSurface->tileMode.value(),
			(uint32)srcSurface->aa,
			srcMip,
		// dst
			(uint32)dstSurface->imagePtr,
			(uint32)dstSurface->mipPtr,
			(uint32)dstSurface->swizzle,
			(uint32)dstSurface->format.value(),
			(uint32)dstSurface->width,
			(uint32)dstSurface->height,
			(uint32)dstSurface->depth,
			(uint32)dstSurface->pitch,
			dstSlice,
			(uint32)dstSurface->dim.value(),
			(uint32)dstSurface->tileMode.value(),
			(uint32)dstSurface->aa,
			dstMip);
	}

	if (requestGPURAMCopy)
		return; // if RAM copy happens on the GPU side we skip it here

	// manually exclude expensive CPU texture copies for some known game framebuffer textures
	// todo - find a better way to solve this
	bool isDynamicTexCopy = false;
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->imagePtr >= 0xF4000000 && srcSurface->width >= 800 && srcFormat == Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT); // SM3DW
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->imagePtr >= 0xF4000000 && srcSurface->width >= 800 && srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM); // Trine 2
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->imagePtr >= 0xF4000000 && srcSurface->width == 0xA0 && srcFormat == Latte::E_GX2SURFFMT::R32_FLOAT); // Little Inferno
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->imagePtr >= 0xF4000000 && srcSurface->width == 1280 && srcFormat == Latte::E_GX2SURFFMT::R32_FLOAT); // Donkey Kong Tropical Freeze
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->imagePtr >= 0xF4000000 && srcSurface->width == 640 && srcSurface->height == 320 && srcFormat == Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT); // SM3DW Switch Scramble Circus
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 1280 && srcSurface->height == 720 && srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM && srcSurface->tileMode != Latte::E_GX2TILEMODE::TM_LINEAR_ALIGNED ); // Affordable Space Adventures
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 854 && srcSurface->height == 480 && srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM); // Affordable Space Adventures
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->imagePtr >= 0xF4000000 && srcSurface->width == 1152 && srcSurface->height == 720 && srcFormat == Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT && (srcSurface->resFlag&GX2_RESFLAG_USAGE_COLOR_BUFFER) != 0 ); // Star Fox Zero
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->imagePtr >= 0xF4000000 && srcSurface->width == 680 && srcSurface->height == 480 && srcFormat == Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT && (srcSurface->resFlag&GX2_RESFLAG_USAGE_COLOR_BUFFER) != 0 ); // Star Fox Zero
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->imagePtr >= 0xF4000000 && srcSurface->width == 1280 && srcSurface->height == 720 && srcFormat == Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT ); // Qube
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 322 && srcSurface->height == 182 && srcFormat == Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM ); // Qube
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 640 && srcSurface->height == 360 && srcFormat == Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT ); // Qube
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 1920 && srcSurface->height == 1080 && srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM && dstSurface->resFlag == 0x80000003); // Cosmophony
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 854 && srcSurface->height == 480 && srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM && dstSurface->resFlag == 0x3); // Cosmophony
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 1280 && srcSurface->height == 720 && srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB && dstSurface->resFlag == 0x3); // The Fall
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 854 && srcSurface->height == 480 && srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB && dstSurface->resFlag == 0x3); // The Fall
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 1280 && srcSurface->height == 720 && srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB && dstSurface->resFlag == 0x80000003); // The Fall
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 1280 && srcSurface->height == 720 && srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB && srcSurface->resFlag == 0x80000003); // Nano Assault Neo
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->imagePtr >= 0xF4000000 && srcSurface->width == 1280 && srcSurface->height == 720 && srcFormat == Latte::E_GX2SURFFMT::R10_G10_B10_A2_UNORM); // Mario Party 10
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->imagePtr >= 0xF4000000 && srcSurface->width == 854 && srcSurface->height == 480 && srcFormat == Latte::E_GX2SURFFMT::R10_G10_B10_A2_UNORM); // Mario Party 10
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 1920 && srcSurface->height == 1080 && srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM && dstSurface->resFlag == 0x3); // Hello Kitty Kruisers
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 1024 && srcSurface->height == 1024 && srcFormat == Latte::E_GX2SURFFMT::R32_FLOAT && dstSurface->resFlag == 0x5); // Art Academy
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 260 && srcSurface->height == 148 && srcFormat == Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT && dstSurface->resFlag == 0x3); // Transformers: Rise of the Dark Spark
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 1040 && srcSurface->height == 592 && srcFormat == Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT && dstSurface->resFlag == 0x3); // Transformers: Rise of the Dark Spark
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 854 && srcSurface->height == 480 && srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB && srcSurface->resFlag == 0x3); // Nano Assault Neo
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 1024 && srcSurface->height == 576 && srcFormat == Latte::E_GX2SURFFMT::D24_S8_UNORM && srcSurface->resFlag == 0x1); // Skylanders SuperChargers
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 1152 && srcSurface->height == 648 && (srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM || srcFormat == Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT) && srcSurface->resFlag == 0x1); // Watch Dogs
	isDynamicTexCopy = isDynamicTexCopy || (srcSurface->width == 576 && srcSurface->height == 324 && (srcFormat == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM || srcFormat == Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT) && srcSurface->resFlag == 0x1); // Watch Dogs

	if( isDynamicTexCopy && debugTestForceCPUCopy == false)
	{
		debug_printf("Software tex copy blocked\n");
		return;
	}

	sint32 copyWidth = dstMipWidth;
	sint32 copyHeight = dstMipHeight;
	if (Latte::IsCompressedFormat(dstHwFormat))
	{
		copyWidth = (copyWidth + 3) / 4;
		copyHeight = (copyHeight + 3) / 4;
	}

	gx2SurfaceCopySoftware(inputData, surfOutSrc.height, srcPitch, srcDepth, srcSlice, srcSwizzle, srcHwTileMode,
		outputData, surfOutDst.height, dstPitch, dstDepth, dstSlice, dstSwizzle, dstHwTileMode,
		copyWidth, copyHeight, dstBpp);
}

void gx2Export_GX2CopySurface(PPCInterpreter_t* hCPU)
{
	GX2Surface* srcSurface = (GX2Surface*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	uint32 srcMip = hCPU->gpr[4];
	uint32 srcSlice = hCPU->gpr[5];
	GX2Surface* dstSurface = (GX2Surface*)memory_getPointerFromVirtualOffset(hCPU->gpr[6]);
	uint32 dstMip = hCPU->gpr[7];
	uint32 dstSlice = hCPU->gpr[8];
	gx2Surface_GX2CopySurface(srcSurface, srcMip, srcSlice, dstSurface, dstMip, dstSlice);
	osLib_returnFromFunction(hCPU, 0);
}

typedef struct  
{
	sint32 left;
	sint32 top;
	sint32 right;
	sint32 bottom;
}GX2Rect_t;

typedef struct  
{
	sint32 x;
	sint32 y;
}GX2Point_t;

void gx2Export_GX2CopySurfaceEx(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "GX2CopySurfaceEx(0x{:08x},{},{},0x{:08x},{},{},{},0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7], hCPU->gpr[8], hCPU->gpr[9], hCPU->gpr[10], memory_readU32(hCPU->gpr[1]+0x8));
	GX2Surface* srcSurface = (GX2Surface*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	uint32 srcMip = hCPU->gpr[4];
	uint32 srcSlice = hCPU->gpr[5];
	GX2Surface* dstSurface = (GX2Surface*)memory_getPointerFromVirtualOffset(hCPU->gpr[6]);
	uint32 dstMip = hCPU->gpr[7];
	uint32 dstSlice = hCPU->gpr[8];
	sint32 rectCount = hCPU->gpr[9];
	MPTR rectSrcArrayMPTR = hCPU->gpr[10];
	MPTR pointDstArrayMPTR = memory_readU32(hCPU->gpr[1]+0x8);

	GX2Rect_t* rectSrc = (GX2Rect_t*)memory_getPointerFromVirtualOffset(rectSrcArrayMPTR);
	GX2Point_t* rectDst = (GX2Point_t*)memory_getPointerFromVirtualOffset(pointDstArrayMPTR);
	for (sint32 i = 0; i < rectCount; i++)
	{
		cemuLog_logDebug(LogType::Force, "rect left-top: {}/{} size: {}/{}", _swapEndianU32(rectSrc->left), _swapEndianU32(rectSrc->top), _swapEndianU32(rectSrc->right) - _swapEndianU32(rectSrc->left), _swapEndianU32(rectSrc->bottom) - _swapEndianU32(rectSrc->top));
	}

#ifdef CEMU_DEBUG_ASSERT
	if( rectCount != 1 )
		assert_dbg();
	if( srcMip != 0 )
		assert_dbg();
	if( srcSlice != 0 )
		assert_dbg();
	if( dstMip != 0 )
		assert_dbg();
	if( dstSlice != 0 )
		assert_dbg();
#endif

	for(sint32 i=0; i<rectCount; i++)
	{
		uint32 srcWidth = srcSurface->width;
		uint32 srcHeight = srcSurface->height;
		// calculate rect size
		sint32 rectSrcX = (sint32)_swapEndianU32((uint32)rectSrc[i].left);
		sint32 rectSrcY = (sint32)_swapEndianU32((uint32)rectSrc[i].top);
		sint32 rectWidth = (sint32)_swapEndianU32((uint32)rectSrc[i].right) - rectSrcX;
		sint32 rectHeight = (sint32)_swapEndianU32((uint32)rectSrc[i].bottom) - rectSrcY;
		if( rectSrcX == 0 && rectSrcY == 0 && rectWidth == srcWidth && rectHeight == srcHeight )
		{
			// special case in which GX2CopySurfaceEx acts like GX2CopySurface()
			gx2Surface_GX2CopySurface(srcSurface, srcMip, srcSlice, dstSurface, dstMip, dstSlice);
		}
	}

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2ResolveAAColorBuffer(PPCInterpreter_t* hCPU)
{
	debug_printf("GX2ResolveAAColorBuffer(0x%08x,0x%08x,%d,%d)\n", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
	GX2ColorBuffer* srcColorBuffer = (GX2ColorBuffer*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	GX2Surface* srcSurface = &srcColorBuffer->surface;
	GX2Surface* dstSurface = (GX2Surface*)memory_getPointerFromVirtualOffset(hCPU->gpr[4]);
	uint32 srcMip = _swapEndianU32(srcColorBuffer->viewMip);
	uint32 dstMip = hCPU->gpr[5];
	uint32 srcSlice = _swapEndianU32(srcColorBuffer->viewFirstSlice);
	uint32 dstSlice = hCPU->gpr[6];

#ifdef CEMU_DEBUG_ASSERT
	if( _swapEndianU32(srcColorBuffer->viewMip) != 0 || _swapEndianU32(srcColorBuffer->viewFirstSlice) != 0 )
		assert_dbg();
#endif

	// allocate pixel buffer
	sint32 dstWidth = dstSurface->width;
	sint32 dstHeight = dstSurface->height;
	sint32 srcWidth = srcSurface->width;
	sint32 srcHeight = srcSurface->height;

	uint32 dstMipWidth = std::max(dstWidth>>dstMip, 1);
	uint32 dstMipHeight = std::max(dstHeight>>dstMip, 1);
	uint32 srcMipWidth = std::max(srcWidth>>srcMip, 1);
	uint32 srcMipHeight = std::max(srcHeight>>srcMip, 1);

	// check if surface properties match
	if( srcSurface->width != dstSurface->width || srcSurface->height != dstSurface->height )
	{
		osLib_returnFromFunction(hCPU, 0);
		return;
	}
	if( dstMipWidth != srcMipWidth || dstMipHeight != srcMipHeight )
	{
		cemu_assert_suspicious();
		osLib_returnFromFunction(hCPU, 0);
		return;
	}
	// handle format
	Latte::E_GX2SURFFMT srcFormat = srcSurface->format;
	Latte::E_GX2SURFFMT dstFormat = dstSurface->format;
	uint32 srcBPP = Latte::GetFormatBits(srcFormat);
	uint32 dstBPP = Latte::GetFormatBits(dstFormat);
	sint32 srcStepX = 1;
	sint32 srcStepY = 1;
	sint32 dstStepX = 1;
	sint32 dstStepY = 1;
	auto srcHwFormat = Latte::GetHWFormat(srcFormat);
	auto dstHwFormat = Latte::GetHWFormat(dstFormat);
	// get texture info
	LatteAddrLib::AddrSurfaceInfo_OUT surfOutSrc = {0};
	GX2::GX2CalculateSurfaceInfo(srcSurface, srcMip, &surfOutSrc);
	LatteAddrLib::AddrSurfaceInfo_OUT surfOutDst = {0};
	GX2::GX2CalculateSurfaceInfo(dstSurface, dstMip, &surfOutDst);
	// get input pointer
	uint8* inputData = NULL;
	cemu_assert(srcMip < srcSurface->numLevels);
	if( srcMip == 0 )
		inputData = (uint8*)memory_getPointerFromVirtualOffset(srcSurface->imagePtr);
	else if( srcMip == 1 )
		inputData = (uint8*)memory_getPointerFromVirtualOffset(srcSurface->mipPtr);
	else
		inputData = (uint8*)memory_getPointerFromVirtualOffset(srcSurface->mipPtr+srcSurface->mipOffset[srcMip-1]);
	// get output pointer
	uint8* outputData = NULL;
	cemu_assert(dstMip < dstSurface->numLevels);
	if( dstMip == 0 )
		outputData = (uint8*)memory_getPointerFromVirtualOffset(dstSurface->imagePtr);
	else if( dstMip == 1 )
		outputData = (uint8*)memory_getPointerFromVirtualOffset(dstSurface->mipPtr);
	else
		outputData = (uint8*)memory_getPointerFromVirtualOffset(dstSurface->mipPtr+dstSurface->mipOffset[dstMip-1]);
	// calculate step size for compressed textures
	if( Latte::IsCompressedFormat(srcHwFormat) )
	{
		srcStepX = 4;
		srcStepY = 4;
	}
	if(Latte::IsCompressedFormat(dstHwFormat) )
	{
		dstStepX = 4;
		dstStepY = 4;
	}
	if( srcStepX != dstStepX || srcStepY != dstStepY )
		assert_dbg();

	if( srcHwFormat != dstHwFormat )
	{
		// mismatching format
		debug_printf("GX2CopySurface(): Format mismatch\n");
		osLib_returnFromFunction(hCPU, 0);
		return;
	}

	// src
	uint32 srcPitch = surfOutSrc.pitch;
	uint32 srcSwizzle = srcSurface->swizzle;
	uint32 srcPipeSwizzle = (srcSwizzle>>8)&1;
	uint32 srcBankSwizzle = ((srcSwizzle>>9)&3);
	uint32 srcTileMode = (uint32)surfOutSrc.hwTileMode;
	uint32 srcDepth = std::max<uint32>(surfOutSrc.depth, 1);
	// dst
	uint32 dstPitch = surfOutDst.pitch;
	uint32 dstSwizzle = dstSurface->swizzle;
	uint32 dstPipeSwizzle = (dstSwizzle>>8)&1;
	uint32 dstBankSwizzle = ((dstSwizzle>>9)&3);
	uint32 dstTileMode = (uint32)surfOutDst.hwTileMode;
	uint32 dstDepth = std::max<uint32>(surfOutDst.depth, 1);

	// send copy command to GPU
	GX2ReserveCmdSpace(1 + 13 * 2);
	gx2WriteGather_submit(pm4HeaderType3(IT_HLE_COPY_SURFACE_NEW, 13 * 2),
		// src
		(uint32)srcSurface->imagePtr,
		(uint32)srcSurface->mipPtr,
		(uint32)srcSurface->swizzle,
		(uint32)srcSurface->format.value(),
		(uint32)srcSurface->width,
		(uint32)srcSurface->height,
		(uint32)srcSurface->depth,
		(uint32)srcSurface->pitch,
		srcSlice,
		(uint32)srcSurface->dim.value(),
		(uint32)srcSurface->tileMode.value(),
		(uint32)srcSurface->aa,
		srcMip,
		// dst
		(uint32)dstSurface->imagePtr,
		(uint32)dstSurface->mipPtr,
		(uint32)dstSurface->swizzle,
		(uint32)dstSurface->format.value(),
		(uint32)dstSurface->width,
		(uint32)dstSurface->height,
		(uint32)dstSurface->depth,
		(uint32)dstSurface->pitch,
		dstSlice,
		(uint32)dstSurface->dim.value(),
		(uint32)dstSurface->tileMode.value(),
		(uint32)dstSurface->aa,
		dstMip);

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2ConvertDepthBufferToTextureSurface(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2ConvertDepthBufferToTextureSurface(0x{:x}, 0x{:x}, {}, {})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
	GX2DepthBuffer* depthBuffer = (GX2DepthBuffer*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	GX2Surface* dstSurface = (GX2Surface*)memory_getPointerFromVirtualOffset(hCPU->gpr[4]);
	uint32 dstMip = hCPU->gpr[5];
	uint32 dstSlice = hCPU->gpr[6];

	if (dstMip != 0 || dstSlice != 0)
		debugBreakpoint();
	// get texture info
	LatteAddrLib::AddrSurfaceInfo_OUT surfOutSrc = { 0 };
	GX2::GX2CalculateSurfaceInfo(&depthBuffer->surface, 0, &surfOutSrc);
	LatteAddrLib::AddrSurfaceInfo_OUT surfOutDst = { 0 };
	GX2::GX2CalculateSurfaceInfo(dstSurface, 0, &surfOutDst);

	if (depthBuffer->surface.imagePtr == dstSurface->imagePtr)
	{
		// in-place re-tiling doesn't need any actual copy operation?
		if (dstMip != 0 || dstSlice != 0)
			debugBreakpoint();
		debug_printf("In-place re-tiling\n");
		osLib_returnFromFunction(hCPU, 0);
		return;
	}

	// note: Do not trust values from the input GX2Surface* structs but rely on surfOutDst/surfOutSrc instead if possible.
	// src
	uint32 srcPitch = surfOutSrc.pitch;
	uint32 srcSwizzle = depthBuffer->surface.swizzle;
	uint32 srcPipeSwizzle = (srcSwizzle >> 8) & 1;
	uint32 srcBankSwizzle = ((srcSwizzle >> 9) & 3);
	uint32 srcTileMode = (uint32)surfOutSrc.hwTileMode;
	uint32 srcDepth = std::max<uint32>(surfOutSrc.depth, 1);
	// dst
	uint32 dstPitch = surfOutDst.pitch;
	uint32 dstSwizzle = dstSurface->swizzle;
	uint32 dstPipeSwizzle = (dstSwizzle >> 8) & 1;
	uint32 dstBankSwizzle = ((dstSwizzle >> 9) & 3);
	uint32 dstTileMode = (uint32)surfOutDst.hwTileMode;
	uint32 dstDepth = srcDepth;

	sint32 srcMip = 0;

	uint32 numSlices = std::max<uint32>(_swapEndianU32(depthBuffer->viewNumSlices), 1);
	GX2ReserveCmdSpace((1 + 13 * 2) * numSlices);
	for (uint32 subSliceIndex = 0; subSliceIndex < numSlices; subSliceIndex++)
	{
		// send copy command to GPU
		gx2WriteGather_submit(pm4HeaderType3(IT_HLE_COPY_SURFACE_NEW, 13 * 2),
			// src
			(uint32)(depthBuffer->surface.imagePtr),
			(uint32)(depthBuffer->surface.mipPtr),
			(uint32)(depthBuffer->surface.swizzle),
			(uint32)(depthBuffer->surface.format.value()),
			(uint32)(depthBuffer->surface.width),
			(uint32)(depthBuffer->surface.height),
			(uint32)(depthBuffer->surface.depth),
			(uint32)(depthBuffer->surface.pitch),
			(uint32)(depthBuffer->viewFirstSlice) + subSliceIndex,
			(uint32)(depthBuffer->surface.dim.value()),
			(uint32)(depthBuffer->surface.tileMode.value()),
			(uint32)(depthBuffer->surface.aa),
			srcMip,
			// dst
			(uint32)(dstSurface->imagePtr),
			(uint32)(dstSurface->mipPtr),
			(uint32)(dstSurface->swizzle),
			(uint32)(dstSurface->format.value()),
			(uint32)(dstSurface->width),
			(uint32)(dstSurface->height),
			(uint32)(dstSurface->depth),
			(uint32)(dstSurface->pitch),
			dstSlice + subSliceIndex,
			(uint32)(dstSurface->dim.value()),
			(uint32)(dstSurface->tileMode.value()),
			(uint32)(dstSurface->aa),
			dstMip);
	}

	osLib_returnFromFunction(hCPU, 0);
}

namespace GX2
{
	void GX2SurfaceCopyInit()
	{
		osLib_addFunction("gx2", "GX2CopySurface", gx2Export_GX2CopySurface);
		osLib_addFunction("gx2", "GX2CopySurfaceEx", gx2Export_GX2CopySurfaceEx);
		osLib_addFunction("gx2", "GX2ResolveAAColorBuffer", gx2Export_GX2ResolveAAColorBuffer);
		osLib_addFunction("gx2", "GX2ConvertDepthBufferToTextureSurface", gx2Export_GX2ConvertDepthBufferToTextureSurface);
	}
};

void gx2CopySurfaceTest()
{

	return;

	BenchmarkTimer bt;

	// copy 0
	bt.Start();
	for(sint32 i=0; i<100; i++)
		gx2SurfaceCopySoftware(
			memory_base + 0x10000000, 256, 256, 1, 0, 0, 4,
			memory_base + 0x20000000, 256, 256, 1, 0, 0, 4,
			64, 64, 32
		);
	bt.Stop();
	debug_printf("Copy 0 - %lfms\n", bt.GetElapsedMilliseconds());
	// copy 1
	bt.Start();
	for (sint32 i = 0; i < 100; i++)
		gx2SurfaceCopySoftware(
			memory_base + 0x11000000, 256, 256, 1, 0, 0, 4,
			memory_base + 0x21000000, 256, 256, 1, 0, 0, 2,
			64, 64, 32
		);
	bt.Stop();
	debug_printf("Copy 1 - %lfms\n", bt.GetElapsedMilliseconds());
	// copy 2
	bt.Start();
	for (sint32 i = 0; i < 100; i++)
		gx2SurfaceCopySoftware(
			memory_base + 0x12000000, 256, 256, 1, 0, 0, 1,
			memory_base + 0x22000000, 256, 256, 1, 0, 0, 4,
			64, 64, 128
		);
	bt.Stop();
	debug_printf("Copy 2 - %lfms\n", bt.GetElapsedMilliseconds());
	// copy 3
	bt.Start();
	for (sint32 i = 0; i < 100; i++)
		gx2SurfaceCopySoftware(
			memory_base + 0x12000000, 256, 256, 1, 0, 0, 4,
			memory_base + 0x22000000, 256, 256, 1, 0, 0, 4,
			64, 512, 32
		);
	bt.Stop();
	debug_printf("Copy 3 - %lfms\n", bt.GetElapsedMilliseconds());

	cemu_assert_debug(false);

	// with bpp switch optimized away:
	// Copy 0 - 19.777100ms
	// Copy 1 - 14.311300ms
	// Copy 2 - 10.837700ms
	// Copy 3 - 158.174400ms

	// Copy 0 - 19.846800ms
	// Copy 1 - 14.054000ms
	// Copy 2 - 11.013500ms
	// Copy 3 - 159.916000ms

	// with fast path added:
	// Copy 0 - 0.222400ms
	// Copy 1 - 14.125700ms
	// Copy 2 - 13.298100ms
	// Copy 3 - 1.764500ms

	// with shared offset:
	// Copy 0 - 0.143300ms
	// Copy 1 - 13.814200ms
	// Copy 2 - 10.309500ms
	// Copy 3 - 1.191900ms
}
