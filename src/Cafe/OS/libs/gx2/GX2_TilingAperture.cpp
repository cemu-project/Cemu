#include "Cafe/OS/common/OSCommon.h"
#include "GX2.h"
#include "GX2_Resource.h"
#include "GX2_Surface.h"
#include "Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.h"
#include "Cafe/HW/Latte/Core/LatteTextureLoader.h"

void LatteTextureLoader_begin(LatteTextureLoaderCtx* textureLoader, uint32 sliceIndex, uint32 mipIndex, MPTR physImagePtr, MPTR physMipPtr, Latte::E_GX2SURFFMT format, Latte::E_DIM dim, uint32 width, uint32 height, uint32 depth, uint32 mipLevels, uint32 pitch, Latte::E_HWTILEMODE tileMode, uint32 swizzle);

#define GX2_MAX_ACTIVE_TILING_APERTURES	(32)

namespace GX2
{
	struct TilingAperture
	{
		uint32 addr;
		uint32 size;
		uint32 endianMode;
		// surface info
		GX2Surface surface;
		uint32 mipLevel;
		uint32 sliceIndex;
	};

	std::mutex s_apertureMtx;
	TilingAperture s_tilingApertures[GX2_MAX_ACTIVE_TILING_APERTURES];
	uint32 s_apertureUseMask{0};
	std::atomic_int s_numActiveTilingThreads{0};

	MPTR GX2TilingAperture_AllocateTilingMemory(uint32 size)
	{
		uint32 currentOffset = 0;
		while( true )
		{
			// align offset
			currentOffset = (currentOffset+0xFFF)&~0xFFF;
			// check if out of range
			if( (currentOffset+size) >= MEMORY_TILINGAPERTURE_AREA_SIZE )
				break;
			// check if range intersects with any already allocated range
			bool isAvailable = true;
			uint32 nextOffset = 0xFFFFFFFF;
			for (uint32 i=0; i<GX2_MAX_ACTIVE_TILING_APERTURES; i++)
			{
				if (!HAS_FLAG(s_apertureUseMask, (1u<<i)))
					continue;
				auto& other = s_tilingApertures[i];
				uint32 startA = currentOffset;
				uint32 endA = startA+size;
				uint32 startB = other.addr - MEMORY_TILINGAPERTURE_AREA_ADDR;
				uint32 endB = startB+other.size;
				if( startA < endB && endA >= startB )
				{
					isAvailable = false;
					nextOffset = std::min(nextOffset, endB);
				}
			}
			if( isAvailable )
				return currentOffset + MEMORY_TILINGAPERTURE_AREA_ADDR;
			currentOffset = nextOffset;
		}
		return MPTR_NULL;
	}

	std::optional<uint32> GX2TilingAperture_GetFreeHandle()
	{
		for (uint32 i=0; i<GX2_MAX_ACTIVE_TILING_APERTURES; i++)
		{
			if (!HAS_FLAG(s_apertureUseMask, (1u<<i)))
				return i;
		}
		return std::nullopt;
	}

	template<typename copyType, int count, bool isWrite>
	static void copyValue(uint8* outputBlockData, uint8* inputBlockData)
	{
		if (isWrite)
		{
			*(copyType*)outputBlockData = *(copyType*)inputBlockData;
			if (count >= 2)
				((copyType*)outputBlockData)[1] = ((copyType*)inputBlockData)[1];
			if (count >= 3)
				((copyType*)outputBlockData)[2] = ((copyType*)inputBlockData)[2];
			if (count >= 4)
				((copyType*)outputBlockData)[3] = ((copyType*)inputBlockData)[3];
		}
		else
		{
			*(copyType*)inputBlockData = *(copyType*)outputBlockData;
			if (count >= 2)
				((copyType*)inputBlockData)[1] = ((copyType*)outputBlockData)[1];
			if (count >= 3)
				((copyType*)inputBlockData)[2] = ((copyType*)outputBlockData)[2];
			if (count >= 4)
				((copyType*)inputBlockData)[3] = ((copyType*)outputBlockData)[3];
		}
	}

	template<int bpp, bool isWrite, int surfaceTileMode>
	static void ApertureRetileTexture(TilingAperture* tilingAperture, uint8* inputData, uint8* outputData, sint32 texelWidth, sint32 texelHeight, sint32 surfaceSlice, sint32 surfacePitch, sint32 surfaceHeight, sint32 surfaceDepth, LatteAddrLib::CachedSurfaceAddrInfo* cachedInfo)
	{
		for (sint32 y = 0; y < texelHeight; y++)
		{
			uint32 srcOffset;
			uint8* inputBlockData;
			if (bpp != 8)
			{
				srcOffset = (0 + y*surfacePitch)*(bpp / 8);
				inputBlockData = inputData + srcOffset;
			}
			for (sint32 x = 0; x < texelWidth; x++)
			{
				// calculate address of input block
				sint32 texelX = x;
				sint32 texelY = y;
				if (bpp == 8)
				{
					texelX ^= 8;
					texelY ^= 2;
					srcOffset = (texelX + texelY*surfacePitch)*(bpp / 8);
					inputBlockData = inputData + srcOffset;
				}
				// calculate address of output block
				uint32 dstBitPos = 0;
				uint32 dstOffset = 0;
				if (surfaceTileMode == 4)
					dstOffset = ComputeSurfaceAddrFromCoordMacroTiledCached_tm04_sample1(x, y, cachedInfo);
				else if (surfaceTileMode == 2 || surfaceTileMode == 3)
				{
					dstOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordMicroTiled(x, y, cachedInfo->slice, cachedInfo->bpp, cachedInfo->pitch, cachedInfo->height, (Latte::E_HWTILEMODE)cachedInfo->tileMode, false);
				}
				else if (surfaceTileMode == 1 || surfaceTileMode == 0)
				{
					dstOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordLinear(x, y, cachedInfo->slice, 0, cachedInfo->bpp, cachedInfo->pitch, cachedInfo->height, cachedInfo->depth);
				}
				else
					dstOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordMacroTiledCached(x, y, cachedInfo);
				uint8* outputBlockData = outputData + dstOffset;
				if (bpp == 32)
					copyValue<uint32, 1, isWrite>(outputBlockData, inputBlockData);
				else if (bpp == 16)
					copyValue<uint16, 1, isWrite>(outputBlockData, inputBlockData);
				else if (bpp == 8)
					copyValue<uint8, 1, isWrite>(outputBlockData, inputBlockData);
				else if (bpp == 64)
					copyValue<uint64, 1, isWrite>(outputBlockData, inputBlockData);
				else if (bpp == 128)
					copyValue<uint64, 2, isWrite>(outputBlockData, inputBlockData);
				else
				{
					cemu_assert_unimplemented();
				}
				if (bpp != 8)
				{
					inputBlockData += (bpp / 8);
				}
			}
		}
	}

	template<int bpp, bool isWrite>
	static void ApertureRetileTexture_tm04_sample1(TilingAperture* tilingAperture, uint8* inputData, uint8* outputData, sint32 texelWidth, sint32 texelHeight, sint32 surfaceSlice, sint32 surfacePitch, sint32 surfaceHeight, sint32 surfaceDepth, LatteAddrLib::CachedSurfaceAddrInfo* cachedInfo)
	{
		uint16* tableBase = cachedInfo->microTilePixelIndexTable + ((cachedInfo->slice & 7) << 6);
		for (sint32 y = 0; y < texelHeight; y++)
		{
			uint32 srcOffset;
			uint8* inputBlockData;
			if (bpp != 8)
			{
				srcOffset = (0 + y*surfacePitch)*(bpp / 8);
				inputBlockData = inputData + srcOffset;
			}
			for (sint32 bx = 0; bx < texelWidth; bx += 8)
			{
				uint16* pixelOffsets = tableBase + ((y&7) << 3);
				uint32 baseOffset = ComputeSurfaceAddrFromCoordMacroTiledCached_tm04_sample1(bx, y, cachedInfo);
				for (sint32 x = bx; x < bx+8; x++)
				{
					// calculate address of input block
					if (bpp == 8)
					{
						sint32 texelX = x;
						sint32 texelY = y;
						texelX ^= 8;
						texelY ^= 2;
						srcOffset = (texelX + texelY*surfacePitch)*(bpp / 8);
						inputBlockData = inputData + srcOffset;
					}
					// calculate address of output block
					uint32 dstBitPos = 0;
					uint32 pixelIndex = *pixelOffsets;
					pixelOffsets++;
					uint32 pixelOffset = pixelIndex * (bpp/8);
					uint32 elemOffset = pixelOffset;
					if ((bpp * 8) > 256)
					{
						// separate group bytes, for small formats this step is not necessary since elemOffset is never over 0xFF (maximum is 8*8*bpp)
						elemOffset = (elemOffset & 0xFF) | ((elemOffset&~0xFF) << 3);
					}
					sint32 offset = baseOffset + elemOffset;

					uint8* outputBlockData = outputData + offset;
					if (bpp == 32)
						copyValue<uint32, 1, isWrite>(outputBlockData, inputBlockData);
					else if (bpp == 16)
						copyValue<uint16, 1, isWrite>(outputBlockData, inputBlockData);
					else if (bpp == 8)
						copyValue<uint8, 1, isWrite>(outputBlockData, inputBlockData);
					else if (bpp == 64)
						copyValue<uint64, 1, isWrite>(outputBlockData, inputBlockData);
					else if (bpp == 128)
						copyValue<uint64, 2, isWrite>(outputBlockData, inputBlockData);
					else
					{
						cemu_assert_unimplemented();
					}
					if (bpp != 8)
					{
						inputBlockData += (bpp / 8);
					}
				}
			}
			// copy remaining partial block
			for (sint32 x = (texelWidth&~7); x < texelWidth; x++)
			{
				// calculate address of input block
				sint32 texelX = x;
				sint32 texelY = y;
				if (bpp == 8)
				{
					texelX ^= 8;
					texelY ^= 2;
					srcOffset = (texelX + texelY*surfacePitch)*(bpp / 8);
					inputBlockData = inputData + srcOffset;
				}
				// calculate address of output block
				uint32 dstBitPos = 0;
				uint32 dstOffset = 0;
				dstOffset = ComputeSurfaceAddrFromCoordMacroTiledCached_tm04_sample1(x, y, cachedInfo);

				uint8* outputBlockData = outputData + dstOffset;
				if (bpp == 32)
					copyValue<uint32, 1, isWrite>(outputBlockData, inputBlockData);
				else if (bpp == 16)
					copyValue<uint16, 1, isWrite>(outputBlockData, inputBlockData);
				else if (bpp == 8)
					copyValue<uint8, 1, isWrite>(outputBlockData, inputBlockData);
				else if (bpp == 64)
					copyValue<uint64, 1, isWrite>(outputBlockData, inputBlockData);
				else if (bpp == 128)
					copyValue<uint64, 2, isWrite>(outputBlockData, inputBlockData);
				else
				{
					cemu_assert_unimplemented();
				}
				if (bpp != 8)
				{
					inputBlockData += (bpp / 8);
				}
			}
		}
	}

	template<int bpp, bool isWrite>
	static void ApertureRetileTextureWrapper(TilingAperture* tilingAperture, uint8* inputData, uint8* outputData, sint32 texelWidth, sint32 texelHeight, sint32 surfaceSlice, sint32 surfaceTileMode, sint32 surfacePitch, sint32 surfaceHeight, sint32 surfaceDepth, LatteAddrLib::CachedSurfaceAddrInfo* cachedInfo)
	{
		if (surfaceTileMode == 0)
			ApertureRetileTexture<bpp, isWrite, 0>(tilingAperture, inputData, outputData, texelWidth, texelHeight, surfaceSlice, surfacePitch, surfaceHeight, surfaceDepth, cachedInfo);
		else if (surfaceTileMode == 1)
			ApertureRetileTexture<bpp, isWrite, 1>(tilingAperture, inputData, outputData, texelWidth, texelHeight, surfaceSlice, surfacePitch, surfaceHeight, surfaceDepth, cachedInfo);
		else if (surfaceTileMode == 2)
			ApertureRetileTexture<bpp, isWrite, 2>(tilingAperture, inputData, outputData, texelWidth, texelHeight, surfaceSlice, surfacePitch, surfaceHeight, surfaceDepth, cachedInfo);
		else if (surfaceTileMode == 3)
			ApertureRetileTexture<bpp, isWrite, 3>(tilingAperture, inputData, outputData, texelWidth, texelHeight, surfaceSlice, surfacePitch, surfaceHeight, surfaceDepth, cachedInfo);
		else if (surfaceTileMode == 4)
			ApertureRetileTexture<bpp, isWrite, 4>(tilingAperture, inputData, outputData, texelWidth, texelHeight, surfaceSlice, surfacePitch, surfaceHeight, surfaceDepth, cachedInfo);
		else if (surfaceTileMode == 7)
			ApertureRetileTexture<bpp, isWrite, 7>(tilingAperture, inputData, outputData, texelWidth, texelHeight, surfaceSlice, surfacePitch, surfaceHeight, surfaceDepth, cachedInfo);
		else
		{
			cemu_assert_unimplemented();
		}
	}

	void GX2TilingAperture_RetileTextureAsync(TilingAperture* tilingAperture, bool doWrite)
	{
		//uint64 timerTilingStart = benchmarkTimer_start();

		Latte::E_GX2SURFFMT surfaceFormat = tilingAperture->surface.format;
		uint32 surfaceSlice = tilingAperture->sliceIndex;
		LatteAddrLib::AddrSurfaceInfo_OUT surfaceInfo = {0};
		GX2::GX2CalculateSurfaceInfo(&tilingAperture->surface, tilingAperture->mipLevel, &surfaceInfo);
		uint32 surfacePitch = surfaceInfo.pitch;
		uint32 surfaceSwizzle = tilingAperture->surface.swizzle;
		uint32 surfacePipeSwizzle = (surfaceSwizzle>>8)&1;
		uint32 surfaceBankSwizzle = ((surfaceSwizzle>>9)&3);
		Latte::E_HWTILEMODE surfaceTileMode = surfaceInfo.hwTileMode;
		uint32 surfaceDepth = std::max<uint32>(surfaceInfo.depth, 1);
		sint32 width = std::max<uint32>((uint32)tilingAperture->surface.width >> tilingAperture->mipLevel, 1);
		sint32 height = std::max<uint32>((uint32)tilingAperture->surface.height >> tilingAperture->mipLevel, 1);

		Latte::E_DIM surfaceDim = tilingAperture->surface.dim;
		uint32 surfaceMipSwizzle = 0; // todo
		uint32 mipLevels = tilingAperture->surface.numLevels;
		// get texture info
		uint8* inputData = (uint8*)memory_getPointerFromVirtualOffset(tilingAperture->addr);
		uint8* outputData;
		if( tilingAperture->mipLevel == 0 )
			outputData = (uint8*)memory_getPointerFromVirtualOffset(tilingAperture->surface.imagePtr);
		else if( tilingAperture->mipLevel == 1 )
			outputData = (uint8*)memory_getPointerFromVirtualOffset(tilingAperture->surface.mipPtr);
		else
			outputData = (uint8*)memory_getPointerFromVirtualOffset(tilingAperture->surface.mipPtr + tilingAperture->surface.mipOffset[tilingAperture->mipLevel-1]);

		sint32 stepX = 1;
		sint32 stepY = 1;
		bool isCompressed = false;
		if( Latte::IsCompressedFormat(surfaceFormat) )
		{
			isCompressed = true;
			stepX = 4;
			stepY = 4;
		}
		uint32 bpp = surfaceInfo.bpp;

		LatteAddrLib::CachedSurfaceAddrInfo computeAddrInfo = { 0 };

		SetupCachedSurfaceAddrInfo(&computeAddrInfo, surfaceSlice, 0, bpp, surfacePitch, surfaceInfo.height, surfaceInfo.depth, 1 * 1, surfaceTileMode, false, surfacePipeSwizzle, surfaceBankSwizzle);

		// init info for swizzle encoder/decoder
		LatteTextureLoaderCtx textureLoaderCtx{};
		LatteTextureLoader_begin(&textureLoaderCtx, surfaceSlice, 0, tilingAperture->surface.imagePtr, tilingAperture->surface.mipPtr, surfaceFormat, surfaceDim, width, height, surfaceDepth, mipLevels, surfacePitch, surfaceTileMode, surfaceSwizzle);

		textureLoaderCtx.decodedTexelCountX = surfacePitch;
		textureLoaderCtx.decodedTexelCountY = isCompressed ? (height + 3) / 4 : height;

		if( doWrite )
		{
			if (surfaceTileMode == Latte::E_HWTILEMODE::TM_2D_TILED_THIN1 && bpp == 32 && isCompressed == false)
			{
				optimizedDecodeLoops<uint32, 1, true, false>(&textureLoaderCtx, inputData);
			}
			else if (bpp == 8)
				ApertureRetileTextureWrapper<8, true>(tilingAperture, inputData, outputData, width / stepX, height / stepY, surfaceSlice, (uint32)surfaceTileMode, surfacePitch, surfaceInfo.height, surfaceDepth, &computeAddrInfo);
			else if (bpp == 16)
				ApertureRetileTextureWrapper<16, true>(tilingAperture, inputData, outputData, width / stepX, height / stepY, surfaceSlice, (uint32)surfaceTileMode, surfacePitch, surfaceInfo.height, surfaceDepth, &computeAddrInfo);
			else if (bpp == 32)
				ApertureRetileTextureWrapper<32, true>(tilingAperture, inputData, outputData, width / stepX, height / stepY, surfaceSlice, (uint32)surfaceTileMode, surfacePitch, surfaceInfo.height, surfaceDepth, &computeAddrInfo);
			else if (bpp == 64)
				ApertureRetileTextureWrapper<64, true>(tilingAperture, inputData, outputData, width / stepX, height / stepY, surfaceSlice, (uint32)surfaceTileMode, surfacePitch, surfaceInfo.height, surfaceDepth, &computeAddrInfo);
			else if (bpp == 128)
				ApertureRetileTextureWrapper<128, true>(tilingAperture, inputData, outputData, width / stepX, height / stepY, surfaceSlice, (uint32)surfaceTileMode, surfacePitch, surfaceInfo.height, surfaceDepth, &computeAddrInfo);
			else
			{
				cemu_assert_unimplemented();
			}
		}
		else
		{
			if (surfaceTileMode == Latte::E_HWTILEMODE::TM_2D_TILED_THIN1 && bpp == 32 && isCompressed == false)
			{
				optimizedDecodeLoops<uint32, 1, false, false>(&textureLoaderCtx, inputData);
			}
			else if (bpp == 8)
				ApertureRetileTextureWrapper<8, false>(tilingAperture, inputData, outputData, width / stepX, height / stepY, surfaceSlice, (uint32)surfaceTileMode, surfacePitch, surfaceInfo.height, surfaceDepth, &computeAddrInfo);
			else if (bpp == 16)
				ApertureRetileTextureWrapper<16, false>(tilingAperture, inputData, outputData, width / stepX, height / stepY, surfaceSlice, (uint32)surfaceTileMode, surfacePitch, surfaceInfo.height, surfaceDepth, &computeAddrInfo);
			else if (bpp == 32)
				ApertureRetileTextureWrapper<32, false>(tilingAperture, inputData, outputData, width / stepX, height / stepY, surfaceSlice, (uint32)surfaceTileMode, surfacePitch, surfaceInfo.height, surfaceDepth, &computeAddrInfo);
			else if (bpp == 64)
				ApertureRetileTextureWrapper<64, false>(tilingAperture, inputData, outputData, width / stepX, height / stepY, surfaceSlice, (uint32)surfaceTileMode, surfacePitch, surfaceInfo.height, surfaceDepth, &computeAddrInfo);
			else if (bpp == 128)
				ApertureRetileTextureWrapper<128, false>(tilingAperture, inputData, outputData, width / stepX, height / stepY, surfaceSlice, (uint32)surfaceTileMode, surfacePitch, surfaceInfo.height, surfaceDepth, &computeAddrInfo);
			else
			{
				cemu_assert_unimplemented();
			}
		}

		//double benchmarkTime = benchmarkTimer_stop(timerTilingStart);
		//cemuLog_logDebug(LogType::Force, "TilingAperture res {:04}x{:04} fmt {:04x} tm {:02x} mip {} isWrite {}", (uint32)tilingAperture->surface.width, (uint32)tilingAperture->surface.height, (uint32)tilingAperture->surface.format, (uint32)tilingAperture->surface.tileMode, tilingAperture->mipLevel, doWrite?1:0);
		//cemuLog_logDebug(LogType::Force, "Tiling took {:.4}ms", benchmarkTime);
	}

	void GX2TilingAperture_RetileTexture(TilingAperture* tilingAperture, bool doWrite)
	{
		// retiling can be expensive so avoid blocking the CPU emulation thread for bigger textures
		if (tilingAperture->surface.width < 16 && tilingAperture->surface.height < 16)
		{
			GX2TilingAperture_RetileTextureAsync(tilingAperture, doWrite);
		}
		else
		{
			// offload to thread and execute other PPC threads in the meantime
			StackAllocator<coreinit::OSEvent> event;
			coreinit::OSInitEvent(&event, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_AUTO);
			++s_numActiveTilingThreads;
			std::future<void> res = std::async(std::launch::async, [](TilingAperture* tilingAperture, bool doWrite, coreinit::OSEvent* event) {
				GX2TilingAperture_RetileTextureAsync(tilingAperture, doWrite);
				coreinit::OSSignalEvent(event);
				--s_numActiveTilingThreads;
			}, tilingAperture, doWrite, &event);
			coreinit::OSWaitEvent(&event);
		}
	}

	void GX2AllocateTilingApertureEx(GX2Surface* surface, uint32 mipLevel, uint32 sliceIndex, uint32 endianMode, uint32be* handleOut, MEMPTR<void>* apertureMemOut)
	{
		cemuLog_log(LogType::GX2, "GX2AllocateTilingApertureEx Tex: {:08x} {}x{} Swizzle: {:08x} tm: {} fmt: {:04x} use: {:02x}", (uint32)surface->imagePtr, (uint32)surface->width, (uint32)surface->height, (uint32)surface->swizzle, (uint32)surface->tileMode.value(), (uint32)surface->format.value(), (uint32)surface->resFlag);

		std::unique_lock _l(s_apertureMtx);
		// get aperture handle
		std::optional<uint32> handle = GX2TilingAperture_GetFreeHandle();
		if (!handle)
		{
			cemuLog_log(LogType::APIErrors, "GX2AllocateTilingApertureEx: Exceeded more than 32 tiling apertures");
			return;
		}
		s_apertureUseMask |= (1u<<*handle);

		uint32 tilingSize = 0;
		// calculate size of texture
		Latte::E_GX2SURFFMT surfaceFormat = surface->format;
		uint32 bitsPerPixel = Latte::GetFormatBits(surfaceFormat);
		if (Latte::IsCompressedFormat(surfaceFormat))
			bitsPerPixel /= (4*4);

		// get surface pitch
		LatteAddrLib::AddrSurfaceInfo_OUT surfaceInfo = {0};
		GX2::GX2CalculateSurfaceInfo(surface, 0, &surfaceInfo);
		uint32 surfacePitch = surfaceInfo.pitch;
		uint32 width = std::max<uint32>((uint32)surface->width >> mipLevel, 1);
		uint32 height = std::max<uint32>((uint32)surface->height >> mipLevel, 1);
		uint32 alignedWidth = (width+3)&~3;
		uint32 alignedHeight = (height+3)&~3;
		tilingSize = (surfacePitch*alignedHeight*bitsPerPixel+7)/8;
		// allocate memory for tiling space
		MPTR tilingAddress = GX2TilingAperture_AllocateTilingMemory(tilingSize);
		if( tilingAddress == MPTR_NULL )
		{
			cemuLog_log(LogType::APIErrors, "GX2AllocateTilingApertureEx: Exceeded available tiling aperture memory");
			s_apertureUseMask &= ~(1u<<*handle);
			return;
		}
		// add tiling aperture entry
		auto& apertureEntry = s_tilingApertures[*handle];
		apertureEntry.addr = tilingAddress;
		apertureEntry.size = tilingSize;
		apertureEntry.endianMode = endianMode;
		apertureEntry.sliceIndex = sliceIndex;
		apertureEntry.mipLevel = mipLevel;
		memcpy(&apertureEntry.surface, surface, sizeof(GX2Surface));
		// return values
		*handleOut = *handle;
		*apertureMemOut = MEMPTR<void>(tilingAddress);
		// load texture data into tiling area
		_l.unlock();
		GX2TilingAperture_RetileTexture(&apertureEntry, false);
	}

	void GX2FreeTilingAperture(uint32 apertureHandle)
	{
		std::unique_lock _l(s_apertureMtx);
		if (apertureHandle >= GX2_MAX_ACTIVE_TILING_APERTURES || !HAS_FLAG(s_apertureUseMask, (1u<<apertureHandle)))
		{
			cemuLog_log(LogType::APIErrors, "GX2FreeTilingAperture: Invalid handle");
			return;
		}
		TilingAperture& tilingAperture = s_tilingApertures[apertureHandle];
		// heuristic for when writes can be skipped
		bool canSkipWrite = false;
		if (tilingAperture.surface.width > 100 && tilingAperture.surface.format == Latte::E_GX2SURFFMT::D24_S8_UNORM && HAS_FLAG(tilingAperture.surface.resFlag, GX2_RESFLAG_USAGE_COLOR_BUFFER) ) // TP depth buffer (as color) read-only access
			canSkipWrite = true;
		// store texture from tiling area
		_l.unlock();
		if (!canSkipWrite)
			GX2TilingAperture_RetileTexture(&tilingAperture, true);
		_l.lock();
		// remove entry
		tilingAperture = {};
		s_apertureUseMask &= ~(1u<<apertureHandle);
	}

	void GX2ApertureInit()
	{
		cafeExportRegister("gx2", GX2AllocateTilingApertureEx, LogType::GX2);
		cafeExportRegister("gx2", GX2FreeTilingAperture, LogType::GX2);

		s_apertureUseMask = 0;
		for (auto& it : s_tilingApertures)
			it = {};
	}

	void GX2ApertureShutdown()
	{
		while (s_numActiveTilingThreads != 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

}