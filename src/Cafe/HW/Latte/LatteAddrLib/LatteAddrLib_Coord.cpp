#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.h"
#include "Cafe/OS/libs/gx2/GX2_Surface.h"

using namespace Latte;

namespace LatteAddrLib
{

#if BOOST_OS_LINUX || BOOST_OS_MACOS
	unsigned char _BitScanReverse(uint32* _Index, uint32 _Mask)
	{
		if (!_Mask)
			return 0;
		*_Index = 31 - __builtin_clzl(_Mask);
		return 1;
	}
#endif

	static const uint32 bankSwapOrder[] = { 0, 1, 3, 2 };

	uint32 _GetMicroTileType(bool isDepth)
	{
		return isDepth ? 1 : 0;
	}

	uint32 _ComputePixelIndexWithinMicroTile(uint32 x, uint32 y, uint32 z, uint32 bpp, E_HWTILEMODE tileMode, uint32 microTileType)
	{
		cemu_assert_debug(microTileType == 0 || microTileType == 1);
		uint32 pixelBit0, pixelBit1, pixelBit2, pixelBit3, pixelBit4, pixelBit5, pixelBit6, pixelBit7, pixelBit8;
		pixelBit6 = 0;
		pixelBit7 = 0;
		pixelBit8 = 0;
		uint32 thickness = LatteAddrLib::TM_GetThickness(tileMode);
		if (microTileType)
		{
			pixelBit0 = x & 1;
			pixelBit1 = y & 1;
			pixelBit2 = (x & 2) >> 1;
			pixelBit3 = (y & 2) >> 1;
			pixelBit4 = (x & 4) >> 2;
			pixelBit5 = (y & 4) >> 2;
		}
		else
		{
			switch (bpp)
			{
			case 8:
				pixelBit0 = x & 1;
				pixelBit1 = (x & 2) >> 1;
				pixelBit2 = (x & 4) >> 2;
				pixelBit3 = (y & 2) >> 1;
				pixelBit4 = y & 1;
				pixelBit5 = (y & 4) >> 2;
				break;
			case 0x10:
				pixelBit0 = x & 1;
				pixelBit1 = (x & 2) >> 1;
				pixelBit2 = (x & 4) >> 2;
				pixelBit3 = y & 1;
				pixelBit4 = (y & 2) >> 1;
				pixelBit5 = (y & 4) >> 2;
				break;
			case 0x20:
			case 0x60:
				pixelBit0 = x & 1;
				pixelBit1 = (x & 2) >> 1;
				pixelBit2 = y & 1;
				pixelBit3 = (x & 4) >> 2;
				pixelBit4 = (y & 2) >> 1;
				pixelBit5 = (y & 4) >> 2;
				break;
			case 0x40:
				pixelBit0 = x & 1;
				pixelBit1 = y & 1;
				pixelBit2 = (x & 2) >> 1;
				pixelBit3 = (x & 4) >> 2;
				pixelBit4 = (y & 2) >> 1;
				pixelBit5 = (y & 4) >> 2;
				break;
			case 0x80:
				pixelBit0 = y & 1;
				pixelBit1 = x & 1;
				pixelBit2 = (x & 2) >> 1;
				pixelBit3 = (x & 4) >> 2;
				pixelBit4 = (y & 2) >> 1;
				pixelBit5 = (y & 4) >> 2;
				break;
			default:
				pixelBit0 = x & 1;
				pixelBit1 = (x & 2) >> 1;
				pixelBit2 = y & 1;
				pixelBit3 = (x & 4) >> 2;
				pixelBit4 = (y & 2) >> 1;
				pixelBit5 = (y & 4) >> 2;
				break;
			}
		}
		if (thickness > 1)
		{
			pixelBit6 = z & 1;
			pixelBit7 = (z & 2) >> 1;
		}
		return (pixelBit8 << 8) | (pixelBit7 << 7) | (pixelBit6 << 6) | (pixelBit5 << 5) | (pixelBit4 << 4) | (pixelBit3 << 3) | (pixelBit2 << 2) | (pixelBit1 << 1) | pixelBit0;
	}

	uint32 _ComputePipeFromCoordWoRotation(uint32 x, uint32 y)
	{
		// hardcoded to assume 2 pipes
		uint32 pipe;
		pipe = ((y >> 3) ^ (x >> 3)) & 1;
		return pipe;
	}

	uint32 _ComputeBankFromCoordWoRotation(uint32 x, uint32 y)
	{
		uint32 bank;
		if (m_banks == 4)
		{
			uint32 bankNew = (y >> 4) & 3;
			bankNew = ((bankNew >> 1) | (bankNew << 1)); // swap lowest two bits
			bankNew ^= (x >> 3);
			bankNew &= 3;
			bank = bankNew;
		}
		else if (m_banks == 8)
		{
			cemu_assert_unimplemented();
			bank = 0;
		}
		else
		{
			bank = 0;
		}
		return bank;
	}

	uint32 ComputeSurfaceAddrFromCoordLinear(uint32 x, uint32 y, uint32 slice, uint32 sample, uint32 bpp, uint32 pitch, uint32 height, uint32 numSlices)
	{
		uint32 pixelIndex = x + pitch * y + (slice + numSlices * sample) * height * pitch;
		return (pixelIndex * bpp) / 8;
	}

	uint32 ComputeSurfaceAddrFromCoordMicroTiled(uint32 x, uint32 y, uint32 slice, uint32 bpp, uint32 pitch, uint32 height, Latte::E_HWTILEMODE tileMode, bool isDepth)
	{
		uint32 microTileThickness = (tileMode == Latte::E_HWTILEMODE::TM_1D_TILED_THICK) ? 4 : 1;
		uint32 microTilesPerRow = pitch >> 3;
		uint32 microTileIndexX = x >> 3;
		uint32 microTileIndexY = y >> 3;
		uint32 microTileBytes = microTileThickness * (((bpp << 6) + 7) >> 3); // each tile is 8x8 or 8x8x4
		uint32 microTileOffset = microTileBytes * (uint64)((x >> 3) + (pitch >> 3) * (y >> 3));
		uint32 sliceBytes = (height * (uint64)pitch * microTileThickness * bpp + 7) / 8;
		uint32 sliceOffset = sliceBytes * (slice / microTileThickness);
		uint32 pixelIndex = _ComputePixelIndexWithinMicroTile(x, y, slice, bpp, tileMode, _GetMicroTileType(isDepth));
		uint32 pixelOffset = bpp * pixelIndex;
		pixelOffset >>= 3;
		return pixelOffset + microTileOffset + sliceOffset;
	}

	uint32 ComputeSurfaceAddrFromCoordMacroTiled(uint32 x, uint32 y, uint32 slice, uint32 sample, uint32 bpp, uint32 pitch, uint32 height, uint32 numSamples, Latte::E_HWTILEMODE tileMode, bool isDepth, uint32 pipeSwizzle, uint32 bankSwizzle)
	{
		uint32 microTileThickness = LatteAddrLib::TM_GetThickness((E_HWTILEMODE)tileMode);
		uint32 microTileBits = numSamples * bpp * (microTileThickness * (8 * 8));
		uint32 microTileBytes = microTileBits >> 3;
		uint32 pixelIndex = _ComputePixelIndexWithinMicroTile(x, y, slice, bpp, tileMode, _GetMicroTileType(isDepth));
		uint32 sampleOffset, pixelOffset;
		if (isDepth)
		{
			sampleOffset = bpp * sample;
			pixelOffset = numSamples * bpp * pixelIndex;
		}
		else
		{
			sampleOffset = sample * (microTileBits / numSamples);
			pixelOffset = bpp * pixelIndex;
		}
		uint32 elemOffset = pixelOffset + sampleOffset;
		uint32 bytesPerSample = microTileBytes / numSamples;
		uint32 sampleSlice, numSampleSplits;
		if (numSamples <= 1 || microTileBytes <= m_splitSize)
		{
			numSampleSplits = 1;
			sampleSlice = 0;
		}
		else
		{
			uint32 samplesPerSlice = m_splitSize / bytesPerSample;
			numSampleSplits = numSamples / samplesPerSlice;
			numSamples = samplesPerSlice;
			sampleSlice = elemOffset / (microTileBits / numSampleSplits);
			elemOffset %= microTileBits / numSampleSplits;
		}
		elemOffset >>= 3;
		uint32 pipe = _ComputePipeFromCoordWoRotation(x, y);
		uint32 bank = _ComputeBankFromCoordWoRotation(x, y);
		uint32 bankPipe = pipe + m_pipes * bank;
		uint32 rotation = ComputeSurfaceRotationFromTileMode(tileMode);
		uint32 swizzle = pipeSwizzle + m_pipes * bankSwizzle;
		uint32 sliceIn = slice;
		if (TM_IsThickAndMacroTiled(tileMode))
			sliceIn >>= 2;
		bankPipe ^= m_pipes * sampleSlice * ((m_banks >> 1) + 1) ^ (swizzle + sliceIn * rotation);
		bankPipe %= m_pipes * m_banks;
		pipe = bankPipe % m_pipes;
		bank = bankPipe / m_pipes;
		uint64 sliceBytes = (((uint64)height * pitch * microTileThickness * bpp * numSamples + 7) / 8);
		uint64 sliceOffset = sliceBytes * ((sampleSlice + numSampleSplits * slice) / microTileThickness);
		uint32 macroTilePitch = 8 * m_banks;
		uint32 macroTileHeight = 8 * m_pipes;
		switch (tileMode)
		{
		case Latte::E_HWTILEMODE::TM_2D_TILED_THIN2:
		case Latte::E_HWTILEMODE::TM_2B_TILED_THIN2:
			macroTilePitch >>= 1;
			macroTileHeight <<= 1;
			break;
		case Latte::E_HWTILEMODE::TM_2D_TILED_THIN4:
		case Latte::E_HWTILEMODE::TM_2B_TILED_THIN4:
			macroTilePitch >>= 2;
			macroTileHeight <<= 2;
			break;
		default:
			break;
		}
		uint32 macroTilesPerRow = pitch / macroTilePitch;
		uint32 macroTileBytes = (numSamples * microTileThickness * bpp * macroTileHeight * macroTilePitch + 7) >> 3;
		uint32 macroTileIndexX = x / macroTilePitch;
		uint32 macroTileIndexY = y / macroTileHeight;
		uint32 macroTileOffset = (x / macroTilePitch + pitch / macroTilePitch * (y / macroTileHeight)) * macroTileBytes;
		if (TM_IsBankSwapped(tileMode))
		{
			uint32 bankSwapWidth = ComputeSurfaceBankSwappedWidth(tileMode, bpp, numSamples, pitch);
			uint32 swapIndex = macroTilePitch * macroTileIndexX / bankSwapWidth;
			uint32 bankMask = m_banks - 1;
			bank ^= bankSwapOrder[swapIndex & bankMask];
		}
		uint32 pipeOffset = (pipe << m_pipeInterleaveBytesBitcount);
		uint32 bankOffset = (bank << (m_pipesBitcount + m_pipeInterleaveBytesBitcount));
		uint32 numSwizzleBits = (m_banksBitcount + m_pipesBitcount);
		uint32 macroSliceOffset = (uint32)((macroTileOffset + sliceOffset) >> numSwizzleBits);
		macroSliceOffset += elemOffset;

		uint32 macroSliceOffsetHigh = macroSliceOffset & ~((1 << m_pipeInterleaveBytesBitcount) - 1);
		uint32 macroSliceOffsetLow = macroSliceOffset & ((1 << m_pipeInterleaveBytesBitcount) - 1);

		uint32 finalMacroTileOffset = (macroSliceOffsetHigh << numSwizzleBits) | macroSliceOffsetLow;
		return finalMacroTileOffset | pipeOffset | bankOffset;
	}

	void SetupCachedSurfaceAddrInfo(CachedSurfaceAddrInfo* info, uint32 slice, uint32 sample, uint32 bpp, uint32 pitch, uint32 height, uint32 depth, uint32 numSamples, E_HWTILEMODE tileMode, int isDepth, uint32 pipeSwizzle, uint32 bankSwizzle)
	{
		info->slice = slice;
		info->sample = sample;
		info->bpp = bpp;
		info->pitch = pitch;
		info->height = height;
		info->depth = depth;
		info->numSamples = numSamples;
		info->tileMode = tileMode;
		info->isDepth = isDepth;
		info->pipeSwizzle = pipeSwizzle;
		info->bankSwizzle = bankSwizzle;
		// calculate static info
		info->microTileThickness = LatteAddrLib::TM_GetThickness((E_HWTILEMODE)tileMode);
		info->microTileBits = info->numSamples * info->bpp * (info->microTileThickness * (8 * 8));
		info->microTileBytes = info->microTileBits >> 3;
		info->microTileType = (info->isDepth != 0) ? 1 : 0;
		cemu_assert_debug(sample == 0); // non-zero not supported
		info->rotation = ComputeSurfaceRotationFromTileMode((E_HWTILEMODE)tileMode);
		// macro tile
		info->macroTilePitch = 8 * m_banks;
		info->macroTileHeight = 8 * m_pipes;
		switch (info->tileMode)
		{
		case E_HWTILEMODE::TM_2D_TILED_THIN2:
		case E_HWTILEMODE::TM_2B_TILED_THIN2:
			info->macroTilePitch >>= 1;
			info->macroTileHeight <<= 1;
			break;
		case E_HWTILEMODE::TM_2D_TILED_THIN4:
		case E_HWTILEMODE::TM_2B_TILED_THIN4:
			info->macroTilePitch >>= 2;
			info->macroTileHeight <<= 2;
			break;
		default:
			break;
		}
		_BitScanReverse((DWORD*)&info->macroTilePitchBits, info->macroTilePitch);
		_BitScanReverse((DWORD*)&info->macroTileHeightBits, info->macroTileHeight);
		info->macroTilesPerRow = info->pitch / info->macroTilePitch;
		info->macroTileBytes = (info->numSamples * info->microTileThickness * info->bpp * info->macroTileHeight * info->macroTilePitch + 7) >> 3;
		// slice
		info->sliceBytes = (info->height * (uint64)info->pitch * info->microTileThickness * info->bpp * info->numSamples + 7) / 8;
		info->sliceIn = info->slice;
		if (TM_IsThickAndMacroTiled(tileMode))
			info->sliceIn >>= 2;
		// bank swap
		if (TM_IsBankSwapped(tileMode))
			info->bankSwapWidth = ComputeSurfaceBankSwappedWidth(tileMode, info->bpp, info->numSamples, info->pitch);
		// pixel offset multiplier
		if (info->isDepth)
		{
			info->pixelOffsetMul = info->numSamples * info->bpp;
		}
		else
		{
			info->pixelOffsetMul = info->bpp;
		}
		info->bytesPerPixel = info->pixelOffsetMul >> 3;
		// table for micro tile offset calculation (we could pre-generate these)
		for (sint32 z = 0; z < 8; z++)
		{
			for (sint32 y = 0; y < 8; y++)
			{
				for (sint32 x = 0; x < 8; x++)
				{
					uint16 v = _ComputePixelIndexWithinMicroTile(x, y, z, info->bpp, info->tileMode, info->microTileType);
					info->microTilePixelIndexTable[x + y * 8 + z * 8 * 8] = v;
				}
			}
		}
		// other constant values
		uint32 swizzle = info->pipeSwizzle + m_pipes * info->bankSwizzle;
		info->c0 = (swizzle + info->sliceIn * info->rotation);
	}

	uint32 ComputeSurfaceAddrFromCoordMacroTiledCached(uint32 x, uint32 y, CachedSurfaceAddrInfo* info)
	{
		uint32 numSamples = info->numSamples;
		uint32 pixelIndex = (uint32)info->microTilePixelIndexTable[(x & 7) + ((y & 7) << 3) + ((info->slice & 7) << 6)];
		uint32 pixelOffset = pixelIndex * info->pixelOffsetMul;
		uint32 bytesPerSample = info->microTileBytes / numSamples;
		uint32 sampleSlice, numSampleSplits, samplesPerSlice;
		if (numSamples <= 1 || info->microTileBytes <= m_splitSize)
		{
			samplesPerSlice = numSamples;
			numSampleSplits = 1;
			sampleSlice = 0;
		}
		else
		{
			samplesPerSlice = m_splitSize / bytesPerSample;
			numSampleSplits = numSamples / samplesPerSlice;
			numSamples = samplesPerSlice;
			sampleSlice = pixelOffset / (info->microTileBits / numSampleSplits);
			pixelOffset %= info->microTileBits / numSampleSplits;
		}
		pixelOffset >>= 3;
		uint32 pipe = _ComputePipeFromCoordWoRotation(x, y);
		uint32 bank = _ComputeBankFromCoordWoRotation(x, y);
		uint32 bankPipe = pipe + m_pipes * bank;
		bankPipe ^= m_pipes * sampleSlice * ((m_banks >> 1) + 1) ^ info->c0;
		bankPipe %= m_pipes * m_banks;
		pipe = bankPipe % m_pipes;
		bank = bankPipe / m_pipes;
		uint32 sliceOffset = info->sliceBytes * ((sampleSlice + numSampleSplits * info->slice) / info->microTileThickness);
		uint32 macroTileIndexX = x >> info->macroTilePitchBits;
		uint32 macroTileIndexY = y >> info->macroTileHeightBits;
		uint32 macroTileOffset = (macroTileIndexX + (info->pitch >> info->macroTilePitchBits) * macroTileIndexY) * info->macroTileBytes;
		if (TM_IsBankSwapped(info->tileMode))
		{
			uint32 swapIndex = info->macroTilePitch * macroTileIndexX / info->bankSwapWidth;
			bank ^= bankSwapOrder[swapIndex & (m_banks - 1)];
		}
		uint32 pipeOffset = (pipe << m_pipeInterleaveBytesBitcount);
		uint32 bankOffset = (bank << (m_pipesBitcount + m_pipeInterleaveBytesBitcount));
		uint32 numSwizzleBits = (m_banksBitcount + m_pipesBitcount);
		uint32 macroSliceOffset = (uint32)((macroTileOffset + sliceOffset) >> numSwizzleBits);
		macroSliceOffset += pixelOffset;
		uint32 macroSliceOffsetHigh = macroSliceOffset & ~((1 << m_pipeInterleaveBytesBitcount) - 1);
		uint32 macroSliceOffsetLow = macroSliceOffset & ((1 << m_pipeInterleaveBytesBitcount) - 1);
		uint32 finalMacroTileOffset = (macroSliceOffsetHigh << numSwizzleBits) | macroSliceOffsetLow;
		return finalMacroTileOffset | pipeOffset | bankOffset;
	}

	/*
	 * Optimized routine with following assumptions:
	 * tileMode is 4
	 * samples is 1
	 */
	uint32 ComputeSurfaceAddrFromCoordMacroTiledCached_tm04_sample1(uint32 x, uint32 y, CachedSurfaceAddrInfo* info)
	{
		uint32 pixelIndex = (uint32)info->microTilePixelIndexTable[(x & 7) + ((y & 7) << 3) + ((info->slice & 7) << 6)];
		uint32 pixelOffset = pixelIndex * info->pixelOffsetMul;
		pixelOffset >>= 3; // bits to bytes
		uint32 pipe = _ComputePipeFromCoordWoRotation(x, y); // pipe = ((y >> 3) ^ (x >> 3)) & 1;
		uint32 bank = _ComputeBankFromCoordWoRotation(x, y); // based on (x>>3)&3 and (y>>4)&3
		pipe ^= (info->c0 >> 0) & 1;
		bank ^= (info->c0 >> 1) & 3;
		uint32 sliceOffset = info->sliceBytes * (info->slice / info->microTileThickness);
		uint32 macroTileIndexX = x >> info->macroTilePitchBits;
		uint32 macroTileIndexY = y >> info->macroTileHeightBits;
		uint32 macroTileOffset = (macroTileIndexX + (info->pitch >> info->macroTilePitchBits) * macroTileIndexY) * info->macroTileBytes;
		uint32 pipeOffset = (pipe << m_pipeInterleaveBytesBitcount);
		uint32 bankOffset = (bank << (m_pipesBitcount + m_pipeInterleaveBytesBitcount));
		uint32 numSwizzleBits = (m_banksBitcount + m_pipesBitcount);
		uint32 macroSliceOffset = (uint32)((macroTileOffset + sliceOffset) >> numSwizzleBits);
		macroSliceOffset += pixelOffset;
		uint32 macroSliceOffsetHigh = macroSliceOffset & ~((1 << m_pipeInterleaveBytesBitcount) - 1);
		uint32 macroSliceOffsetLow = macroSliceOffset & ((1 << m_pipeInterleaveBytesBitcount) - 1);
		uint32 finalMacroTileOffset = (macroSliceOffsetHigh << numSwizzleBits) | macroSliceOffsetLow;
		return finalMacroTileOffset | pipeOffset | bankOffset;
	}

};