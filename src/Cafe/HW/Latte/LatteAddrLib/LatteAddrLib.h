#pragma once
#include "Cafe/HW/Latte/ISA/LatteReg.h"

namespace LatteAddrLib
{
	static const uint32 m_banks = 4;
	static const uint32 m_banksBitcount = 2;
	static const uint32 m_pipes = 2;
	static const uint32 m_pipesBitcount = 1;
	static const uint32 m_pipeInterleaveBytes = 256;
	static const uint32 m_pipeInterleaveBytesBitcount = 8;
	static const uint32 m_rowSize = 2048;
	static const uint32 m_swapSize = 256;
	static const uint32 m_splitSize = 2048;
	static const uint32 m_chipFamily = 2;

	union AddrSurfaceFlags
	{
		uint32 rawValue;
		struct  
		{
			bool color : 1;
			bool depth : 1;
			bool stencil : 1;
			bool uknFlag4 : 1;
			bool dimCube : 1;
			bool dim3D : 1;
			bool fmask : 1;
			bool cubeAsArray : 1;
			bool uknFlag8 : 1;
			bool linearWA : 1;
			bool uknFlag10 : 1;
			bool uknFlag11 : 1;
			bool inputIsBase : 1;
			bool display : 1;
		};
	};

	struct AddrTileInfo
	{
		uint32 banks;
		uint32 bankWidth;
		uint32 bankHeight;
		uint32 macroAspectRatio;
		uint32 tileSplitBytes;
		uint32 pipeConfig;
	};

	struct AddrSurfaceInfo_IN
	{
		uint32 size;
		Latte::E_HWTILEMODE tileMode;
		Latte::E_HWSURFFMT format;
		uint32 bpp;
		uint32 numSamples;
		uint32 width;
		uint32 height;
		uint32 numSlices;
		uint32 slice;
		uint32 mipLevel;
		AddrSurfaceFlags flags;
		uint32 numFrags;
		AddrTileInfo* pTileInfo;
		sint32 tileIndex;
	};

	struct AddrSurfaceInfo_OUT
	{
		uint32 size;
		uint32 pitch;
		uint32 height;
		uint32 depth;
		uint64 surfSize;
		Latte::E_HWTILEMODE hwTileMode;
		uint32 baseAlign;
		uint32 pitchAlign;
		uint32 heightAlign;
		uint32 depthAlign;
		uint32 bpp;
		uint32 pixelPitch;
		uint32 pixelHeight;
		uint32 pixelBits;
		uint32 sliceSize;
		uint32 pitchTileMax;
		uint32 heightTileMax;
		uint32 sliceTileMax;
		AddrTileInfo* pTileInfo;
		uint32 tileType;
		sint32 tileIndex;
	};

	inline bool IsHWTileMode(Latte::E_GX2TILEMODE tileMode)
	{
		return (uint32)tileMode < 16;
	}

	inline bool IsValidHWTileMode(Latte::E_HWTILEMODE tileMode)
	{
		return (uint32)tileMode < 16;
	}

	inline bool TM_IsThick(Latte::E_HWTILEMODE tileMode)
	{
		return
			tileMode == Latte::E_HWTILEMODE::TM_1D_TILED_THICK ||
			tileMode == Latte::E_HWTILEMODE::TM_2D_TILED_THICK ||
			tileMode == Latte::E_HWTILEMODE::TM_2B_TILED_THICK ||
			tileMode == Latte::E_HWTILEMODE::TM_3D_TILED_THICK ||
			tileMode == Latte::E_HWTILEMODE::TM_3B_TILED_THICK;
	}

	inline uint32 TM_GetThickness(Latte::E_HWTILEMODE tileMode)
	{
		return TM_IsThick(tileMode) ? 4 : 1;
	}

	inline bool TM_IsThickAndMacroTiled(Latte::E_HWTILEMODE tileMode)
	{
		return
			tileMode == Latte::E_HWTILEMODE::TM_2D_TILED_THICK ||
			tileMode == Latte::E_HWTILEMODE::TM_2B_TILED_THICK ||
			tileMode == Latte::E_HWTILEMODE::TM_3D_TILED_THICK ||
			tileMode == Latte::E_HWTILEMODE::TM_3B_TILED_THICK;
	}

	uint32 ComputeSurfaceRotationFromTileMode(Latte::E_HWTILEMODE tileMode);
	uint32 ComputeSurfaceBankSwappedWidth(Latte::E_HWTILEMODE tileMode, uint32 bpp, uint32 numSamples, uint32 pitch);
	
	void GX2CalculateSurfaceInfo(Latte::E_GX2SURFFMT surfaceFormat, uint32 surfaceWidth, uint32 surfaceHeight, uint32 surfaceDepth, Latte::E_DIM surfaceDim, Latte::E_GX2TILEMODE surfaceTileMode, uint32 surfaceAA, uint32 level, AddrSurfaceInfo_OUT* pSurfOut, bool optimizeForDepthBuffer = false, bool optimizeForScanBuffer = false);
	uint32 CalculateMipOffset(Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, Latte::E_DIM dim, Latte::E_HWTILEMODE tileMode, uint32 swizzle, uint32 surfaceAA, sint32 mipIndex);
	void CalculateMipAndSliceAddr(uint32 physAddr, uint32 physMipAddr, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, Latte::E_DIM dim, Latte::E_HWTILEMODE tileMode, uint32 swizzle, uint32 surfaceAA, sint32 mipIndex, sint32 sliceIndex, uint32* outputSliceOffset, uint32* outputSliceSize, sint32* subSliceIndex);

	/* Pixel access */

	uint32 ComputeSurfaceAddrFromCoordLinear(uint32 x, uint32 y, uint32 slice, uint32 sample, uint32 bpp, uint32 pitch, uint32 height, uint32 numSlices);
	uint32 ComputeSurfaceAddrFromCoordMicroTiled(uint32 x, uint32 y, uint32 slice, uint32 bpp, uint32 pitch, uint32 height, Latte::E_HWTILEMODE tileMode, bool isDepth);
	uint32 ComputeSurfaceAddrFromCoordMacroTiled(uint32 x, uint32 y, uint32 slice, uint32 sample, uint32 bpp, uint32 pitch, uint32 height, uint32 numSamples, Latte::E_HWTILEMODE tileMode, bool isDepth, uint32 pipeSwizzle, uint32 bankSwizzle);

	uint32 _ComputePixelIndexWithinMicroTile(uint32 x, uint32 y, uint32 z, uint32 bpp, Latte::E_HWTILEMODE tileMode, uint32 microTileType);

	// addr lib (optimized access)
	struct CachedSurfaceAddrInfo
	{
		uint32 slice;
		uint32 sample;
		uint32 bpp;
		uint32 pitch;
		uint32 height;
		uint32 depth;
		uint32 numSamples; // for AA
		Latte::E_HWTILEMODE tileMode;
		int isDepth;
		uint32 pipeSwizzle;
		uint32 bankSwizzle;
		uint32 pixelOffsetMul;
		uint32 bytesPerPixel;
		// calculated data
		uint32 microTileThickness;
		uint32 microTileBits;
		uint32 microTileBytes;
		uint32 microTileType;
		uint32 rotation; // bank rotation
		uint32 macroTilePitch;
		uint32 macroTileHeight;
		uint32 macroTilePitchBits;
		uint32 macroTileHeightBits;
		uint32 macroTilesPerRow;
		uint32 macroTileBytes;
		uint32 bankSwapWidth;
		uint32 sliceBytes;
		uint32 sliceIn;
		// const
		uint32 c0;
		// micro tile pixel index table
		uint16 microTilePixelIndexTable[8 * 8 * 8];
	};

	void SetupCachedSurfaceAddrInfo(CachedSurfaceAddrInfo* info, uint32 slice, uint32 sample, uint32 bpp, uint32 pitch, uint32 height, uint32 depth, uint32 numSamples, Latte::E_HWTILEMODE tileMode, int isDepth, uint32 pipeSwizzle, uint32 bankSwizzle);
	uint32 ComputeSurfaceAddrFromCoordMacroTiledCached(uint32 x, uint32 y, CachedSurfaceAddrInfo* info);
	uint32 ComputeSurfaceAddrFromCoordMacroTiledCached_tm04_sample1(uint32 x, uint32 y, CachedSurfaceAddrInfo* info);
};
