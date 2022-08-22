#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.h"
#include "Cafe/OS/libs/gx2/GX2_Surface.h"
#include <bit>

/*
	Info:

	- Extra samples for AA are stored in their own micro-tiles

	Macro-Tiling:

	- Contains one micro-tile for every combination of bank/channel select
	- Since there are 4 bank and 2 pipe bits this means 4*2 = 8 micro tiles (or 8*4 for thick?). But the arrangement varies per tilemode (aspect ratio)
	  Allowed layouts: 1x8, 2x4, 4x2

	- Address format: .... aaaaabbc aaaaaaaa		A = offset, b = bank, c = channel
	- Channel/Bank bits are determined by:
		channel0 = x[3] ^ y[3]
		bank0 = x[3] ^ y[5]
		bank1 = x[4] ^ y[4]

*/

using namespace Latte;

namespace LatteAddrLib
{
	
	enum class COMPUTE_SURFACE_RESULT
	{
		RESULT_OK = 0,
		UNKNOWN_FORMAT = 3,
		BAD_SIZE_FIELD = 6,
	};

	const uint32 m_configFlags = (1 << 29);

	uint32 GetSliceComputingFlags()
	{
		return (m_configFlags >> 26) & 3;
	}

	uint32 GetFillSizeFieldsFlags()
	{
		return (m_configFlags >> 25) & 1;
	}

	bool GetFlagUseTileIndex()
	{
		return ((m_configFlags >> 24) & 1) != 0;
	}

	bool GetFlagNoCubeMipSlicesPad()
	{
		return ((m_configFlags >> 28) & 1) != 0;
	}

	bool GetFlagNo1DTiledMSAA()
	{
		return ((m_configFlags >> 29) & 1) != 0;
	}

	bool IsPow2(uint32 dim)
	{
		return (dim & (dim - 1)) == 0;
	}

	uint32 PowTwoAlign(uint32 x, uint32 align)
	{
		return (x + align - 1) & ~(align - 1);
	}

	uint32 NextPow2(uint32 dim)
	{
		return std::bit_ceil<uint32>(dim);
	}

	uint32 GetBitsPerPixel(E_HWSURFFMT format, uint32* pElemMode, uint32* pExpandX, uint32* pExpandY)
	{
		uint32 bpp;
		uint32 elemMode = 3;
		switch (format)
		{
		case E_HWSURFFMT::INVALID_FORMAT:
			bpp = 0;
			*pExpandX = 1;
			*pExpandY = 1;
			break;
		case E_HWSURFFMT::HWFMT_8:
		case E_HWSURFFMT::HWFMT_4_4:
		case E_HWSURFFMT::HWFMT_3_3_2:
			bpp = 8;
			*pExpandX = 1;
			*pExpandY = 1;
			break;
		case E_HWSURFFMT::HWFMT_16:
		case E_HWSURFFMT::HWFMT_16_FLOAT:
		case E_HWSURFFMT::HWFMT_8_8:
		case E_HWSURFFMT::HWFMT_5_6_5:
		case E_HWSURFFMT::HWFMT_6_5_5:
		case E_HWSURFFMT::HWFMT_1_5_5_5:
		case E_HWSURFFMT::HWFMT_4_4_4_4:
			bpp = 16;
			*pExpandX = 1;
			*pExpandY = 1;
			break;
		case E_HWSURFFMT::HWFMT_5_5_5_1:
			bpp = 16;
			*pExpandX = 1;
			*pExpandY = 1;
			break;
		case E_HWSURFFMT::HWFMT_32:
		case E_HWSURFFMT::HWFMT_32_FLOAT:
		case E_HWSURFFMT::HWFMT_16_16:
		case E_HWSURFFMT::HWFMT_16_16_FLOAT:
		case E_HWSURFFMT::HWFMT_24_8:
		case E_HWSURFFMT::HWFMT_24_8_FLOAT:
		case E_HWSURFFMT::HWFMT_10_11_11:
		case E_HWSURFFMT::HWFMT_11_11_10:
		case E_HWSURFFMT::HWFMT_2_10_10_10:
		case E_HWSURFFMT::HWFMT_8_8_8_8:
		case E_HWSURFFMT::HWFMT_8_24:
		case E_HWSURFFMT::HWFMT_8_24_FLOAT:
		case E_HWSURFFMT::HWFMT_10_11_11_FLOAT:
		case E_HWSURFFMT::HWFMT_11_11_10_FLOAT:
		case E_HWSURFFMT::HWFMT_10_10_10_2:
			bpp = 32;
			*pExpandX = 1;
			*pExpandY = 1;
			break;
		case E_HWSURFFMT::HWFMT_32_32:
		case E_HWSURFFMT::HWFMT_32_32_FLOAT:
		case E_HWSURFFMT::HWFMT_16_16_16_16:
		case E_HWSURFFMT::HWFMT_16_16_16_16_FLOAT:
		case E_HWSURFFMT::HWFMT_X24_8_32_FLOAT:
			bpp = 64;
			*pExpandX = 1;
			*pExpandY = 1;
			break;
		case E_HWSURFFMT::HWFMT_32_32_32_32:
		case E_HWSURFFMT::HWFMT_32_32_32_32_FLOAT:
			bpp = 128;
			*pExpandX = 1;
			*pExpandY = 1;
			break;
		case E_HWSURFFMT::HWFMT_BC1:
			elemMode = 9;
			bpp = 64;
			*pExpandX = 4;
			*pExpandY = 4;
			break;
		case E_HWSURFFMT::HWFMT_BC2:
			elemMode = 10;
			bpp = 128;
			*pExpandX = 4;
			*pExpandY = 4;
			break;
		case E_HWSURFFMT::HWFMT_BC3:
			elemMode = 11;
			bpp = 128;
			*pExpandX = 4;
			*pExpandY = 4;
			break;
		case E_HWSURFFMT::HWFMT_BC4:
			elemMode = 12;
			bpp = 64;
			*pExpandX = 4;
			*pExpandY = 4;
			break;
		case E_HWSURFFMT::HWFMT_BC5:
		case E_HWSURFFMT::U_HWFMT_BC6:
		case E_HWSURFFMT::U_HWFMT_BC7:
			elemMode = 13;
			bpp = 128;
			*pExpandX = 4;
			*pExpandY = 4;
			break;
		default:
			cemu_assert_suspicious();
			bpp = 0;
			*pExpandX = 1;
			*pExpandY = 1;
			break;
		}
		*pElemMode = elemMode;
		return bpp;
	}

	void AdjustSurfaceInfo(uint32 elemMode, uint32 expandX, uint32 expandY, uint32* pBpp, uint32* pWidth, uint32* pHeight)
	{
		bool isBCFormat = false;
		if (pBpp)
		{
			uint32 bpp = *pBpp;
			uint32 packedBits;
			switch (elemMode)
			{
			case 4:
				packedBits = bpp / expandX / expandY;
				break;
			case 5:
			case 6:
				packedBits = expandY * expandX * bpp;
				break;
			case 7:
			case 8:
				packedBits = *pBpp;
				break;
			case 9:
			case 12:
				packedBits = 64;
				isBCFormat = true;
				break;
			case 10:
			case 11:
			case 13:
				packedBits = 128;
				isBCFormat = true;
				break;
			case 0:
			case 1:
			case 2:
			case 3:
				packedBits = *pBpp;
				break;
			default:
				packedBits = *pBpp;
				break;
			}
			*pBpp = packedBits;
		}
		if (pWidth)
		{
			if (pHeight)
			{
				uint32 width = *pWidth;
				uint32 height = *pHeight;
				if (expandX > 1 || expandY > 1)
				{
					uint32 widthAligned;
					uint32 heightAligned;
					if (elemMode == 4)
					{
						widthAligned = expandX * width;
						heightAligned = expandY * height;
					}
					else if (isBCFormat)
					{
						widthAligned = width / expandX;
						heightAligned = height / expandY;
					}
					else
					{
						widthAligned = (width + expandX - 1) / expandX;
						heightAligned = (height + expandY - 1) / expandY;
					}
					*pWidth = std::max<uint32>(widthAligned, 1);
					*pHeight = std::max<uint32>(heightAligned, 1);
				}
			}
		}
	}

	void ComputeMipLevelDimensions(uint32* pWidth, uint32* pHeight, uint32* pNumSlices, AddrSurfaceFlags flags, Latte::E_HWSURFFMT format, uint32 mipLevel)
	{
		bool isBCn = (uint32)format >= (uint32)Latte::E_HWSURFFMT::HWFMT_BC1 && (uint32)format <= (uint32)Latte::E_HWSURFFMT::U_HWFMT_BC7;
		if (isBCn && (mipLevel == 0 || flags.inputIsBase))
		{
			*pWidth = PowTwoAlign(*pWidth, 4);
			*pHeight = PowTwoAlign(*pHeight, 4);
		}
		if (isBCn)
		{
			if (mipLevel != 0)
			{
				uint32 width = *pWidth;
				uint32 height = *pHeight;
				uint32 slices = *pNumSlices;
				if (flags.inputIsBase)
				{
					if (!flags.dimCube)
						slices >>= mipLevel;
					width = std::max<uint32>(width >> mipLevel, 1);
					height = std::max<uint32>(height >> mipLevel, 1);
					slices = std::max<uint32>(slices, 1);
				}
				*pWidth = NextPow2(width);
				*pHeight = NextPow2(height);
				*pNumSlices = slices;
			}
		}
		else if (mipLevel && flags.inputIsBase)
		{
			uint32 width = *pWidth;
			uint32 height = *pHeight;
			uint32 slices = *pNumSlices;
			width >>= mipLevel;
			height >>= mipLevel;
			if (!flags.dimCube) // dim 3D
				slices >>= mipLevel;
			width = std::max<uint32>(1, width);
			height = std::max<uint32>(1, height);
			slices = std::max<uint32>(1, slices);
			if (format != E_HWSURFFMT::U_HWFMT_32_32_32 && format != E_HWSURFFMT::U_HWFMT_32_32_32_FLOAT)
			{
				width = NextPow2(width);
				height = NextPow2(height);
				slices = NextPow2(slices);
			}
			*pWidth = width;
			*pHeight = height;
			*pNumSlices = slices;
		}
	}

	E_HWTILEMODE ConvertTileModeToNonBankSwappedMode(E_HWTILEMODE tileMode)
	{
		switch (tileMode)
		{
		case E_HWTILEMODE::TM_2B_TILED_THIN1:
			return E_HWTILEMODE::TM_2D_TILED_THIN1;
		case E_HWTILEMODE::TM_2B_TILED_THIN2:
			return E_HWTILEMODE::TM_2D_TILED_THIN2;
		case E_HWTILEMODE::TM_2B_TILED_THIN4:
			return E_HWTILEMODE::TM_2D_TILED_THIN4;
		case E_HWTILEMODE::TM_2B_TILED_THICK:
			return E_HWTILEMODE::TM_2D_TILED_THICK;
		case E_HWTILEMODE::TM_3B_TILED_THIN1:
			return E_HWTILEMODE::TM_3D_TILED_THIN1;
		case E_HWTILEMODE::TM_3B_TILED_THICK:
			return E_HWTILEMODE::TM_3D_TILED_THICK;
		default:
			break;
		}
		return tileMode;
	}

	uint32 _CalculateSurfaceTileSlices(E_HWTILEMODE tileMode, uint32 bpp, uint32 numSamples)
	{
		uint32 bytePerSample = ((bpp << 6) + 7) >> 3;
		uint32 tileSlices = 1;
		if (TM_GetThickness(tileMode) > 1)
			numSamples = 4;
		if (bytePerSample)
		{
			uint32 samplePerTile = m_splitSize / bytePerSample;
			if (samplePerTile)
			{
				tileSlices = numSamples / samplePerTile;
				if (!(numSamples / samplePerTile))
					tileSlices = 1;
			}
		}
		return tileSlices;
	}

	uint32 ComputeSurfaceRotationFromTileMode(E_HWTILEMODE tileMode)
	{
		switch (tileMode)
		{
		case E_HWTILEMODE::TM_2D_TILED_THIN1:
		case E_HWTILEMODE::TM_2D_TILED_THIN2:
		case E_HWTILEMODE::TM_2D_TILED_THIN4:
		case E_HWTILEMODE::TM_2D_TILED_THICK:
		case E_HWTILEMODE::TM_2B_TILED_THIN1:
		case E_HWTILEMODE::TM_2B_TILED_THIN2:
		case E_HWTILEMODE::TM_2B_TILED_THIN4:
		case E_HWTILEMODE::TM_2B_TILED_THICK:
			return m_pipes * ((m_banks >> 1) - 1);
		case E_HWTILEMODE::TM_3D_TILED_THIN1:
		case E_HWTILEMODE::TM_3D_TILED_THICK:
		case E_HWTILEMODE::TM_3B_TILED_THIN1:
		case E_HWTILEMODE::TM_3B_TILED_THICK:
			if (m_pipes >= 4)
				return (m_pipes >> 1) - 1;
			return 1;
		default:
			break;
		}
		return 0;
	}

	E_HWTILEMODE _ComputeSurfaceMipLevelTileMode(E_HWTILEMODE baseTileMode, uint32 bpp, uint32 level, uint32 width, uint32 height, uint32 numSlices, uint32 numSamples, bool isDepth, bool noRecursive)
	{
		E_HWTILEMODE result;
		E_HWTILEMODE expTileMode = baseTileMode;
		uint32 tileSlices = _CalculateSurfaceTileSlices(baseTileMode, bpp, numSamples);
		switch (baseTileMode)
		{
		case E_HWTILEMODE::TM_1D_TILED_THIN1:
			if (numSamples > 1 && GetFlagNo1DTiledMSAA())
				expTileMode = E_HWTILEMODE::TM_2D_TILED_THIN1;
			break;
		case E_HWTILEMODE::TM_1D_TILED_THICK:
			if (numSamples > 1 || isDepth)
				expTileMode = E_HWTILEMODE::TM_1D_TILED_THIN1;
			if (numSamples == 2 || numSamples == 4)
				expTileMode = E_HWTILEMODE::TM_2D_TILED_THICK;
		break;
		case E_HWTILEMODE::TM_2D_TILED_THIN2:
			if (2 * m_pipeInterleaveBytes > m_splitSize)
				expTileMode = E_HWTILEMODE::TM_2D_TILED_THIN1;
			break;
		case E_HWTILEMODE::TM_2D_TILED_THIN4:
			if (4 * m_pipeInterleaveBytes > m_splitSize)
				expTileMode = E_HWTILEMODE::TM_2D_TILED_THIN2;
			break;
		case E_HWTILEMODE::TM_2D_TILED_THICK:
			if (numSamples > 1 || tileSlices > 1 || isDepth)
				expTileMode = E_HWTILEMODE::TM_2D_TILED_THIN1;
			break;
		case E_HWTILEMODE::TM_2B_TILED_THIN2:
			if (2 * m_pipeInterleaveBytes > m_splitSize)
				expTileMode = E_HWTILEMODE::TM_2B_TILED_THIN1;
			break;
		case E_HWTILEMODE::TM_2B_TILED_THIN4:
			if (4 * m_pipeInterleaveBytes > m_splitSize)
				expTileMode = E_HWTILEMODE::TM_2B_TILED_THIN2;
			break;
		case E_HWTILEMODE::TM_2B_TILED_THICK:
			if (numSamples > 1 || tileSlices > 1 || isDepth)
				expTileMode = E_HWTILEMODE::TM_2B_TILED_THIN1;
			break;
		case E_HWTILEMODE::TM_3D_TILED_THICK:
			if (numSamples > 1 || tileSlices > 1 || isDepth)
				expTileMode = E_HWTILEMODE::TM_3D_TILED_THIN1;
			break;
		case E_HWTILEMODE::TM_3B_TILED_THICK:
			if (numSamples > 1 || tileSlices > 1 || isDepth)
				expTileMode = E_HWTILEMODE::TM_3B_TILED_THIN1;
			break;
		default:
			expTileMode = baseTileMode;
			break;
		}
		uint32 rotation = ComputeSurfaceRotationFromTileMode(expTileMode);
		if (!(rotation % m_pipes))
		{
			switch (expTileMode)
			{
			case E_HWTILEMODE::TM_3D_TILED_THIN1:
				expTileMode = E_HWTILEMODE::TM_2D_TILED_THIN1;
				break;
			case E_HWTILEMODE::TM_3D_TILED_THICK:
				expTileMode = E_HWTILEMODE::TM_2D_TILED_THICK;
				break;
			case E_HWTILEMODE::TM_3B_TILED_THIN1:
				expTileMode = E_HWTILEMODE::TM_2B_TILED_THIN1;
				break;
			case E_HWTILEMODE::TM_3B_TILED_THICK:
				expTileMode = E_HWTILEMODE::TM_2B_TILED_THICK;
				break;
			default:
				break;
			}
		}
		if (noRecursive)
		{
			result = expTileMode;
		}
		else
		{
			if (bpp == 96 || bpp == 48 || bpp == 24)
				bpp /= 3u;
			uint32 widthAligned = NextPow2(width);
			uint32 heightAligned = NextPow2(height);
			uint32 numSlicesAligned = NextPow2(numSlices);
			if (level)
			{
				expTileMode = ConvertTileModeToNonBankSwappedMode(expTileMode);
				uint32 thickness = TM_GetThickness(expTileMode);
				uint32 microTileBytes = (numSamples * bpp * (thickness << 6) + 7) >> 3;
				uint32 widthAlignFactor;
				if (microTileBytes >= m_pipeInterleaveBytes)
					widthAlignFactor = 1;
				else
					widthAlignFactor = m_pipeInterleaveBytes / microTileBytes;
				uint32 macroTileWidth = 8 * m_banks;
				uint32 macroTileHeight = 8 * m_pipes;
				switch (expTileMode)
				{
				case E_HWTILEMODE::TM_2D_TILED_THIN1:
				case E_HWTILEMODE::TM_3D_TILED_THIN1:
					if (widthAligned < widthAlignFactor * macroTileWidth || heightAligned < macroTileHeight)
						expTileMode = E_HWTILEMODE::TM_1D_TILED_THIN1;
					break;
				case E_HWTILEMODE::TM_2D_TILED_THIN2:
					macroTileWidth >>= 1;
					macroTileHeight *= 2;
					if (widthAligned < widthAlignFactor * macroTileWidth || heightAligned < macroTileHeight)
						expTileMode = E_HWTILEMODE::TM_1D_TILED_THIN1;
					break;
				case E_HWTILEMODE::TM_2D_TILED_THIN4:
					macroTileWidth >>= 2;
					macroTileHeight *= 4;
					if (widthAligned < widthAlignFactor * macroTileWidth || heightAligned < macroTileHeight)
						expTileMode = E_HWTILEMODE::TM_1D_TILED_THIN1;
					break;
				case E_HWTILEMODE::TM_2D_TILED_THICK:
				case E_HWTILEMODE::TM_3D_TILED_THICK:
					if (widthAligned < widthAlignFactor * macroTileWidth || heightAligned < macroTileHeight)
						expTileMode = E_HWTILEMODE::TM_1D_TILED_THICK;
					break;
				default:
					break;
				}
				if (expTileMode == E_HWTILEMODE::TM_1D_TILED_THICK)
				{
					if (numSlicesAligned < 4)
						expTileMode = E_HWTILEMODE::TM_1D_TILED_THIN1;
				}
				else if (expTileMode == E_HWTILEMODE::TM_2D_TILED_THICK)
				{
					if (numSlicesAligned < 4)
						expTileMode = E_HWTILEMODE::TM_2D_TILED_THIN1;
				}
				else if (expTileMode == E_HWTILEMODE::TM_3D_TILED_THICK && numSlicesAligned < 4)
				{
					expTileMode = E_HWTILEMODE::TM_3D_TILED_THIN1;
				}
				result = _ComputeSurfaceMipLevelTileMode(expTileMode, bpp, level, widthAligned, heightAligned, numSlicesAligned, numSamples, isDepth, true);
			}
			else
			{
				result = expTileMode;
			}
		}
		return result;
	}

	uint32 _ComputeMacroTileAspectRatio(E_HWTILEMODE tileMode)
	{
		switch (tileMode)
		{
		case E_HWTILEMODE::TM_2B_TILED_THIN1:
		case E_HWTILEMODE::TM_3D_TILED_THIN1:
		case E_HWTILEMODE::TM_3B_TILED_THIN1:
			return 1;
		case E_HWTILEMODE::TM_2D_TILED_THIN2:
		case E_HWTILEMODE::TM_2B_TILED_THIN2:
			return 2;
		case E_HWTILEMODE::TM_2D_TILED_THIN4:
		case E_HWTILEMODE::TM_2B_TILED_THIN4:
			return 4;
		default:
			break;
		}
		return 1;
	}

	void _AdjustPitchAlignment(AddrSurfaceFlags flags, uint32& pitchAlign)
	{
		if (flags.display)
			pitchAlign = PowTwoAlign(pitchAlign, 32);
	}

	void _ComputeSurfaceAlignmentsMacroTiled(E_HWTILEMODE tileMode, uint32 bpp, AddrSurfaceFlags flags, uint32 numSamples, uint32* pBaseAlign, uint32* pPitchAlign, uint32* pHeightAlign, uint32* pMacroWidth, uint32* pMacroHeight)
	{
		uint32 aspectRatio = _ComputeMacroTileAspectRatio(tileMode);
		uint32 thickness = TM_GetThickness(tileMode);
		if (bpp == 96 || bpp == 48 || bpp == 24)
			bpp /= 3;
		if (bpp == 3)
			bpp = 1;
		uint32 macroTileWidth = (8 * m_banks) / aspectRatio;
		uint32 macroTileHeight = aspectRatio * 8 * m_pipes;
		uint32 pitchAlign = std::max(macroTileWidth, macroTileWidth * (m_pipeInterleaveBytes / bpp / (8 * thickness) / numSamples));
		_AdjustPitchAlignment(flags, *pPitchAlign);
		uint32 heightAlign = macroTileHeight;
		uint32 macroTileBytes = numSamples * ((bpp * macroTileHeight * macroTileWidth + 7) >> 3);
		if (m_chipFamily == 1 && numSamples == 1)
			macroTileBytes *= 2;
		uint32 baseAlign;
		if (thickness == 1)
			baseAlign = std::max(macroTileBytes, (numSamples * heightAlign * bpp * pitchAlign + 7) >> 3);
		else
			baseAlign = std::max(m_pipeInterleaveBytes, (4 * heightAlign * bpp * pitchAlign + 7) >> 3);
		uint32 microTileBytes = (thickness * numSamples * (bpp << 6) + 7) >> 3;
		uint32 splits;
		if (microTileBytes < m_splitSize)
			splits = 1;
		else
			splits = microTileBytes / m_splitSize;
		baseAlign /= splits;
		*pBaseAlign = baseAlign;
		*pPitchAlign = pitchAlign;
		*pHeightAlign = heightAlign;
		*pMacroWidth = macroTileWidth;
		*pMacroHeight = macroTileHeight;
	}

	uint32 ComputeSurfaceBankSwappedWidth(E_HWTILEMODE tileMode, uint32 bpp, uint32 numSamples, uint32 pitch)
	{
		uint32 bankSwapWidth = 0;
		uint32 slicesPerTile = 1;
		uint32 bytesPerSample = 8 * bpp;
		uint32 samplesPerTile = m_splitSize / bytesPerSample;
		if (m_splitSize / bytesPerSample)
		{
			slicesPerTile = numSamples / samplesPerTile;
			if (!(numSamples / samplesPerTile))
				slicesPerTile = 1;
		}
		if (TM_IsThickAndMacroTiled(tileMode) == 1)
			numSamples = 4;
		if (TM_IsBankSwapped(tileMode))
		{
			uint32 bytesPerTileSlice = numSamples * bytesPerSample / slicesPerTile;
			uint32 aspectRatioFactor = _ComputeMacroTileAspectRatio(tileMode);
			uint32 swapTiles = std::max<uint32>((m_swapSize >> 1) / bpp, 1);
			uint32 swapWidth = swapTiles * 8 * m_banks;
			uint32 heightBytes = numSamples * aspectRatioFactor * m_pipes * bpp / slicesPerTile;
			uint32 swapMax = m_pipes * m_banks * m_rowSize / heightBytes;
			uint32 swapMin = m_pipeInterleaveBytes * 8 * m_banks / bytesPerTileSlice;
			uint32 swapVal;
			if (swapMax >= swapWidth)
				swapVal = std::max(swapWidth, swapMin);
			else
				swapVal = swapMax;
			for (bankSwapWidth = swapVal; bankSwapWidth >= 2 * pitch; bankSwapWidth >>= 1);
		}
		return bankSwapWidth;
	}

	void PadDimensions(E_HWTILEMODE tileMode, uint32 padDims, int isCube, int cubeAsArray, uint32* pPitch, uint32 pitchAlign, uint32* pHeight, uint32 heightAlign, uint32* pSlices, uint32 sliceAlign)
	{
		uint32 thickness = TM_GetThickness(tileMode);
		if (!padDims)
			padDims = 3;
		if (IsPow2(pitchAlign))
		{
			*pPitch = PowTwoAlign(*pPitch, pitchAlign);
		}
		else
		{
			*pPitch = pitchAlign + *pPitch - 1;
			*pPitch /= pitchAlign;
			*pPitch *= pitchAlign;
		}
		if (padDims > 1)
			*pHeight = PowTwoAlign(*pHeight, heightAlign);
		if (padDims > 2 || thickness > 1)
		{
			if (isCube && (!GetFlagNoCubeMipSlicesPad() || cubeAsArray))
				*pSlices = NextPow2(*pSlices);
			if (thickness > 1)
				*pSlices = PowTwoAlign(*pSlices, sliceAlign);
		}
	}

	void _ComputeSurfaceAlignmentsMicroTiled(E_HWTILEMODE tileMode, uint32 bpp, AddrSurfaceFlags flags, uint32 numSamples, uint32& baseAlignOut, uint32& pitchAlignOut, uint32& heightAlignOut)
	{
		if (bpp == 96 || bpp == 48 || bpp == 24)
			bpp /= 3u;
		uint32 thickness = TM_GetThickness(tileMode);
		baseAlignOut = m_pipeInterleaveBytes;
		pitchAlignOut = std::max(8u, m_pipeInterleaveBytes / bpp / numSamples / thickness);
		heightAlignOut = 8;
		_AdjustPitchAlignment(flags, pitchAlignOut);
	}

	void _ComputeSurfaceAlignmentsLinear(E_HWTILEMODE tileMode, uint32 bpp, AddrSurfaceFlags flags, uint32* pBaseAlign, uint32* pPitchAlign, uint32* pHeightAlign)
	{
		cemu_assert_debug(tileMode == E_HWTILEMODE::TM_LINEAR_GENERAL || tileMode == E_HWTILEMODE::TM_LINEAR_ALIGNED);
		if (tileMode == E_HWTILEMODE::TM_LINEAR_ALIGNED)
		{
			uint32 pixelsPerPipeInterleave = 8 * m_pipeInterleaveBytes / bpp;
			*pBaseAlign = m_pipeInterleaveBytes;
			*pPitchAlign = std::max<uint32>(64, pixelsPerPipeInterleave);
			*pHeightAlign = 1;
		}
		else if (tileMode == E_HWTILEMODE::TM_LINEAR_GENERAL)
		{
			*pBaseAlign = 1;
			*pPitchAlign = 1;
			*pHeightAlign = 1;
		}
		_AdjustPitchAlignment(flags, *pPitchAlign);
	}

	void _ComputeSurfaceInfoLinear(E_HWTILEMODE tileMode, uint32 bpp, uint32 numSamples, uint32 pitch, uint32 height, uint32 numSlices, uint32 mipLevel, uint32 padDims, AddrSurfaceFlags flags, AddrSurfaceInfo_OUT* pOut)
	{
		uint32 heightAlign;
		uint32 pitchAlign;
		uint32 baseAlign;
		uint32 expPitch = pitch;
		uint32 expHeight = height;
		uint32 expNumSlices = numSlices;
		_ComputeSurfaceAlignmentsLinear(tileMode, bpp, flags, &baseAlign, &pitchAlign, &heightAlign);
		if (flags.linearWA && mipLevel == 0)
		{
			expPitch /= 3u;
			expPitch = NextPow2(expPitch);
		}
		if (mipLevel)
		{
			expPitch = NextPow2(expPitch);
			expHeight = NextPow2(expHeight);
			if (flags.dimCube)
			{
				expNumSlices = numSlices;
				if (numSlices <= 1)
					padDims = 2;
				else
					padDims = 0;
			}
			else
			{
				expNumSlices = NextPow2(numSlices);
			}
		}
		uint32 microTileThickness = TM_GetThickness(tileMode);
		PadDimensions(tileMode, padDims, flags.dimCube, flags.cubeAsArray, &expPitch, pitchAlign, &expHeight, heightAlign, &expNumSlices, microTileThickness);
		if (flags.linearWA && mipLevel == 0)
			expPitch *= 3;
		uint32 slices = expNumSlices * numSamples / microTileThickness;
		pOut->pitch = expPitch;
		pOut->height = expHeight;
		pOut->depth = expNumSlices;
		pOut->surfSize = (((uint64)expHeight * expPitch * slices * bpp * numSamples + 7) / 8);
		pOut->baseAlign = baseAlign;
		pOut->pitchAlign = pitchAlign;
		pOut->heightAlign = heightAlign;
		pOut->depthAlign = microTileThickness;
	}

	void _ComputeSurfaceInfoMicroTiled(E_HWTILEMODE tileMode, uint32 bpp, uint32 numSamples, uint32 pitch, uint32 height, uint32 numSlices, uint32 mipLevel, uint32 padDims, AddrSurfaceFlags flags, AddrSurfaceInfo_OUT* pOut)
	{
		E_HWTILEMODE expTileMode = tileMode;
		uint32 expPitch = pitch;
		uint32 expHeight = height;
		uint32 expNumSlices = numSlices;
		uint32 microTileThickness = TM_GetThickness(tileMode);
		if (mipLevel)
		{
			expPitch = NextPow2(pitch);
			expHeight = NextPow2(height);
			if (flags.dimCube)
			{
				expNumSlices = numSlices;
				if (numSlices <= 1)
					padDims = 2;
				else
					padDims = 0;
			}
			else
			{
				expNumSlices = NextPow2(numSlices);
			}
			if (expTileMode == E_HWTILEMODE::TM_1D_TILED_THICK && expNumSlices < 4)
			{
				expTileMode = E_HWTILEMODE::TM_1D_TILED_THIN1;
				microTileThickness = 1;
			}
		}
		uint32 heightAlign;
		uint32 pitchAlign;
		uint32 baseAlign;
		_ComputeSurfaceAlignmentsMicroTiled(expTileMode, bpp, flags, numSamples, /* outputs: */ baseAlign, pitchAlign, heightAlign);
		PadDimensions(expTileMode, padDims, flags.dimCube, flags.cubeAsArray, &expPitch, pitchAlign, &expHeight, heightAlign, &expNumSlices, microTileThickness);
		pOut->pitch = expPitch;
		pOut->height = expHeight;
		pOut->depth = expNumSlices;
		pOut->surfSize = (((uint64)expHeight * expPitch * expNumSlices * bpp * numSamples + 7) / 8);
		pOut->hwTileMode = expTileMode;
		pOut->baseAlign = baseAlign;
		pOut->pitchAlign = pitchAlign;
		pOut->heightAlign = heightAlign;
		pOut->depthAlign = microTileThickness;
	}

	void _ComputeSurfaceInfoMacroTiled(E_HWTILEMODE tileMode, E_HWTILEMODE baseTileMode, uint32 bpp, uint32 numSamples, uint32 pitch, uint32 height, uint32 numSlices, uint32 mipLevel, uint32 padDims, AddrSurfaceFlags flags, AddrSurfaceInfo_OUT* pOut)
	{
		uint32 macroWidth, macroHeight;
		uint32 baseAlign, heightAlign, pitchAlign;

		uint32 expPitch = pitch;
		uint32 expHeight = height;
		uint32 expNumSlices = numSlices;
		E_HWTILEMODE expTileMode = tileMode;
		uint32 microTileThickness = TM_GetThickness(tileMode);
		if (mipLevel)
		{
			expPitch = NextPow2(pitch);
			expHeight = NextPow2(height);
			expNumSlices = NextPow2(numSlices);
			if (flags.dimCube)
			{
				// cubemap
				expNumSlices = numSlices;
				padDims = numSlices <= 1 ? 2 : 0;
			}
			if (expTileMode == E_HWTILEMODE::TM_2D_TILED_THICK && expNumSlices < 4)
			{
				expTileMode = E_HWTILEMODE::TM_2D_TILED_THIN1;
				microTileThickness = 1;
			}
		}
		uint32 pitchAlignFactor = std::max<uint32>((m_pipeInterleaveBytes >> 3) / bpp, 1);
		if (tileMode != baseTileMode && mipLevel != 0 && TM_IsThickAndMacroTiled(baseTileMode) && !TM_IsThickAndMacroTiled(tileMode))
		{
			_ComputeSurfaceAlignmentsMacroTiled(baseTileMode, bpp, flags, numSamples, &baseAlign, &pitchAlign, &heightAlign, &macroWidth, &macroHeight);
			if (expPitch < pitchAlign * pitchAlignFactor || expHeight < heightAlign)
			{
				_ComputeSurfaceInfoMicroTiled(E_HWTILEMODE::TM_1D_TILED_THIN1, bpp, numSamples, pitch, height, numSlices, mipLevel, padDims, flags, pOut);
				return;
			}
		}
		_ComputeSurfaceAlignmentsMacroTiled(tileMode, bpp, flags, numSamples, &baseAlign, &pitchAlign, &heightAlign, &macroWidth, &macroHeight);
		uint32 bankSwappedWidth = ComputeSurfaceBankSwappedWidth(tileMode, bpp, numSamples, pitch);
		if (bankSwappedWidth > pitchAlign)
			pitchAlign = bankSwappedWidth;
		PadDimensions(tileMode, padDims, flags.dimCube, flags.cubeAsArray, &expPitch, pitchAlign, &expHeight, heightAlign, &expNumSlices, microTileThickness);
		pOut->pitch = expPitch;
		pOut->height = expHeight;
		pOut->depth = expNumSlices;
		pOut->surfSize = (((uint64)expHeight * expPitch * expNumSlices * bpp * numSamples + 7) / 8);
		pOut->hwTileMode = expTileMode;
		pOut->baseAlign = baseAlign;
		pOut->pitchAlign = pitchAlign;
		pOut->heightAlign = heightAlign;
		pOut->depthAlign = microTileThickness;
	}

	COMPUTE_SURFACE_RESULT ComputeSurfaceInfoEx(const AddrSurfaceInfo_IN* pIn, AddrSurfaceInfo_OUT* pOut)
	{
		Latte::E_HWTILEMODE tileMode = pIn->tileMode;
		Latte::E_HWTILEMODE baseTileMode = tileMode;
		uint32 bpp = pIn->bpp;
		uint32 numSamples = std::max<uint32>(pIn->numSamples, 1);
		uint32 pitch = pIn->width;
		uint32 height = pIn->height;
		uint32 numSlices = pIn->numSlices;
		uint32 mipLevel = pIn->mipLevel;
		AddrSurfaceFlags flags = pIn->flags;
		uint32 padDims = 0;
		if (flags.dimCube && mipLevel == 0)
			padDims = 2;
		if (flags.fmask)
			tileMode = ConvertTileModeToNonBankSwappedMode(tileMode);
		else
			tileMode = _ComputeSurfaceMipLevelTileMode(tileMode, bpp, mipLevel, pitch, height, numSlices, numSamples, flags.depth, false);
		switch (tileMode)
		{
		case E_HWTILEMODE::TM_LINEAR_GENERAL:
		case E_HWTILEMODE::TM_LINEAR_ALIGNED:
			_ComputeSurfaceInfoLinear(tileMode, bpp, numSamples, pitch, height, numSlices, mipLevel, padDims, flags, pOut);
			pOut->hwTileMode = tileMode;
			break;
		case E_HWTILEMODE::TM_1D_TILED_THIN1:
		case E_HWTILEMODE::TM_1D_TILED_THICK:
			_ComputeSurfaceInfoMicroTiled(tileMode, bpp, numSamples, pitch, height, numSlices, mipLevel, padDims, flags, pOut);
			break;
		case E_HWTILEMODE::TM_2D_TILED_THIN1:
		case E_HWTILEMODE::TM_2D_TILED_THIN2:
		case E_HWTILEMODE::TM_2D_TILED_THIN4:
		case E_HWTILEMODE::TM_2D_TILED_THICK:
		case E_HWTILEMODE::TM_2B_TILED_THIN1:
		case E_HWTILEMODE::TM_2B_TILED_THIN2:
		case E_HWTILEMODE::TM_2B_TILED_THIN4:
		case E_HWTILEMODE::TM_2B_TILED_THICK:
		case E_HWTILEMODE::TM_3D_TILED_THIN1:
		case E_HWTILEMODE::TM_3D_TILED_THICK:
		case E_HWTILEMODE::TM_3B_TILED_THIN1:
		case E_HWTILEMODE::TM_3B_TILED_THICK:
			_ComputeSurfaceInfoMacroTiled(tileMode, baseTileMode, bpp, numSamples, pitch, height, numSlices, mipLevel, padDims, flags, pOut);
			break;
		default:
			return COMPUTE_SURFACE_RESULT::UNKNOWN_FORMAT;
		}
		return COMPUTE_SURFACE_RESULT::RESULT_OK;
	}

	void RestoreSurfaceInfo(uint32 elemMode, uint32 expandX, uint32 expandY, uint32*pBpp, uint32 *pWidth, uint32*pHeight)
	{
		if (pBpp)
		{
			uint32 bpp = *pBpp;
			uint32 originalBits;
			switch (elemMode)
			{
			case 4:
				originalBits = expandY * expandX * bpp;
				break;
			case 5:
			case 6:
				originalBits = bpp / expandX / expandY;
				break;
			case 7:
			case 8:
				originalBits = *pBpp;
				break;
			case 9:
			case 12:
				originalBits = 64;
				break;
			case 10:
			case 11:
			case 13:
				originalBits = 128;
				break;
			case 0:
			case 1:
			case 2:
			case 3:
				originalBits = *pBpp;
				break;
			default:
				originalBits = *pBpp;
				break;
			}
			*pBpp = originalBits;
		}
		if (pWidth && pHeight)
		{
			uint32 width = *pWidth;
			uint32 height = *pHeight;
			if (expandX > 1 || expandY > 1)
			{
				if (elemMode == 4)
				{
					width /= expandX;
					height /= expandY;
				}
				else
				{
					width *= expandX;
					height *= expandY;
				}
			}
			*pWidth = std::max<uint32>(width, 1);
			*pHeight = std::max<uint32>(height, 1);
		}
	}

	COMPUTE_SURFACE_RESULT ComputeSurfaceInfo(AddrSurfaceInfo_IN* pIn, AddrSurfaceInfo_OUT* pOut)
	{
		if (GetFillSizeFieldsFlags() == 1 && (pIn->size != sizeof(AddrSurfaceInfo_IN) || pOut->size != sizeof(AddrSurfaceInfo_OUT)))
			return COMPUTE_SURFACE_RESULT::BAD_SIZE_FIELD;
		cemu_assert_debug(pIn->bpp <= 128);
		ComputeMipLevelDimensions(&pIn->width, &pIn->height, &pIn->numSlices, pIn->flags, pIn->format, pIn->mipLevel);
		uint32 width = pIn->width;
		uint32 height = pIn->height;
		uint32 bpp = pIn->bpp;
		uint32 elemMode;
		uint32 expandX = 1;
		uint32 expandY = 1;
		cemu_assert_debug(pIn->tileIndex == 0 && pIn->pTileInfo == nullptr);
		pOut->pixelBits = pIn->bpp;
		if (pIn->format != E_HWSURFFMT::INVALID_FORMAT)
		{
			bpp = GetBitsPerPixel(pIn->format, &elemMode, &expandX, &expandY);
			if (pIn->tileMode == E_HWTILEMODE::TM_LINEAR_ALIGNED && elemMode == 4 && expandX == 3)
				pIn->flags.linearWA = true;
			AdjustSurfaceInfo(elemMode, expandX, expandY, &bpp, &width, &height);
			pIn->width = width;
			pIn->height = height;
			pIn->bpp = bpp;
		}
		else if (pIn->bpp != 0)
		{
			pIn->width = std::max<uint32>(pIn->width, 1);
			pIn->height = std::max<uint32>(pIn->height, 1);
		}
		else
			return COMPUTE_SURFACE_RESULT::UNKNOWN_FORMAT;
		COMPUTE_SURFACE_RESULT r = ComputeSurfaceInfoEx(pIn, pOut);
		if (r != COMPUTE_SURFACE_RESULT::RESULT_OK)
			return r;
		pOut->bpp = pIn->bpp;
		pOut->pixelPitch = pOut->pitch;
		pOut->pixelHeight = pOut->height;
		if (pIn->format != E_HWSURFFMT::INVALID_FORMAT && (!pIn->flags.linearWA || pIn->mipLevel == 0))
		{
			RestoreSurfaceInfo(elemMode, expandX, expandY, &bpp, &pOut->pixelPitch, &pOut->pixelHeight);
		}
		uint32 sliceFlags = GetSliceComputingFlags();
		if (sliceFlags)
		{
			if (sliceFlags == 1)
				pOut->sliceSize = (pOut->height * pOut->pitch * pOut->bpp * pIn->numSamples + 7) / 8;
		}
		else if (pIn->flags.dim3D)
		{
			pOut->sliceSize = (uint32)(pOut->surfSize);
		}
		else
		{
			if(pOut->surfSize == 0 && pOut->depth == 0)
				pOut->sliceSize = 0; // edge case for (1D)_ARRAY textures with res 0/0/0
			else
				pOut->sliceSize = (uint32)(pOut->surfSize / pOut->depth);
			if (pIn->slice == pIn->numSlices - 1 && pIn->numSlices > 1)
				pOut->sliceSize += pOut->sliceSize * (pOut->depth - pIn->numSlices);
		}
		pOut->pitchTileMax = (pOut->pitch >> 3) - 1;
		pOut->heightTileMax = (pOut->height >> 3) - 1;
		pOut->sliceTileMax = ((pOut->height * pOut->pitch >> 6) - 1);
		return COMPUTE_SURFACE_RESULT::RESULT_OK;
	}

	void GX2CalculateSurfaceInfo(Latte::E_GX2SURFFMT surfaceFormat, uint32 surfaceWidth, uint32 surfaceHeight, uint32 surfaceDepth, E_DIM surfaceDim, E_GX2TILEMODE surfaceTileMode, uint32 surfaceAA, uint32 level, AddrSurfaceInfo_OUT* pSurfOut, bool optimizeForDepthBuffer, bool optimizeForScanBuffer)
	{
		AddrSurfaceInfo_IN surfInfoIn = { 0 };
		Latte::E_HWSURFFMT hwFormat = Latte::GetHWFormat(surfaceFormat);
		memset(pSurfOut, 0, sizeof(AddrSurfaceInfo_OUT));
		pSurfOut->size = sizeof(AddrSurfaceInfo_OUT);
		if (surfaceTileMode == E_GX2TILEMODE::TM_LINEAR_SPECIAL)
		{
			uint32 numSamples = 1 << surfaceAA;
			uint32 blockSize = IsCompressedFormat(hwFormat) ? 4 : 1;
			uint32 width = ((surfaceWidth >> level) + blockSize - 1) & ~(blockSize - 1);
			pSurfOut->bpp = Latte::GetFormatBits(hwFormat);
			pSurfOut->pitch = width / blockSize;
			pSurfOut->pixelBits = pSurfOut->bpp;
			pSurfOut->baseAlign = 1;
			pSurfOut->pitchAlign = 1;
			pSurfOut->heightAlign = 1;
			pSurfOut->depthAlign = 1;
			switch (surfaceDim)
			{
			case E_DIM::DIM_1D:
				pSurfOut->height = 1;
				pSurfOut->depth = 1;
				break;
			case E_DIM::DIM_2D:
				pSurfOut->height = std::max<uint32>(surfaceHeight >> level, 1);
				pSurfOut->depth = 1;
				break;
			case E_DIM::DIM_3D:
				pSurfOut->height = std::max<uint32>(surfaceHeight >> level, 1);
				pSurfOut->depth = std::max<uint32>(surfaceDepth >> level, 1);
				break;
			case E_DIM::DIM_CUBEMAP:
				pSurfOut->height = std::max<uint32>(surfaceHeight >> level, 1);
				pSurfOut->depth = std::max<uint32>(surfaceDepth, 6);
				break;
			case E_DIM::DIM_1D_ARRAY:
				pSurfOut->height = 1;
				pSurfOut->depth = surfaceDepth;
				break;
			case E_DIM::DIM_2D_ARRAY:
				pSurfOut->height = std::max<uint32>(surfaceHeight >> level, 1);
				pSurfOut->depth = surfaceDepth;
				break;
			default:
				break;
			}
			pSurfOut->height = (~(blockSize - 1) & (pSurfOut->height + blockSize - 1)) / (uint64)blockSize;
			pSurfOut->pixelPitch = ~(blockSize - 1) & ((surfaceWidth >> level) + blockSize - 1);
			pSurfOut->pixelPitch = std::max<uint32>(pSurfOut->pixelPitch, blockSize);
			pSurfOut->pixelHeight = ~(blockSize - 1) & ((surfaceHeight >> level) + blockSize - 1);
			pSurfOut->pixelHeight = std::max<uint32>(pSurfOut->pixelHeight, blockSize);
			pSurfOut->pitch = std::max<uint32>(pSurfOut->pitch, 1);
			pSurfOut->height = std::max<uint32>(pSurfOut->height, 1);
			pSurfOut->surfSize = pSurfOut->bpp * numSamples * pSurfOut->depth * pSurfOut->height * pSurfOut->pitch >> 3;
			if (surfaceDim == E_DIM::DIM_3D)
				pSurfOut->sliceSize = (uint32)pSurfOut->surfSize;
			else
			{
				if(pSurfOut->surfSize == 0 && pSurfOut->depth == 0)
					pSurfOut->sliceSize = 0;
				else
					pSurfOut->sliceSize = (uint32)(pSurfOut->surfSize / pSurfOut->depth);
			}
			pSurfOut->pitchTileMax = (pSurfOut->pitch >> 3) - 1;
			pSurfOut->heightTileMax = (pSurfOut->height >> 3) - 1;
			pSurfOut->sliceTileMax = (pSurfOut->height * pSurfOut->pitch >> 6) - 1;
		}
		else
		{
			memset(&surfInfoIn, 0, sizeof(AddrSurfaceInfo_IN));
			surfInfoIn.size = sizeof(AddrSurfaceInfo_IN);
			if (!IsValidHWTileMode((E_HWTILEMODE)surfaceTileMode))
			{
				// cemuLog_log(LogType::Force, "Unexpected TileMode {} in AddrLib", (uint32)surfaceTileMode);
				surfaceTileMode = (E_GX2TILEMODE)((uint32)surfaceTileMode & 0xF);
			}
			surfInfoIn.tileMode = MakeHWTileMode(surfaceTileMode);
			surfInfoIn.format = hwFormat;
			surfInfoIn.bpp = Latte::GetFormatBits(hwFormat);
			surfInfoIn.numSamples = 1 << surfaceAA;
			surfInfoIn.numFrags = surfInfoIn.numSamples;
			surfInfoIn.width = std::max<uint32>(surfaceWidth >> level, 1);
			switch (surfaceDim)
			{
			case E_DIM::DIM_1D:
				surfInfoIn.height = 1;
				surfInfoIn.numSlices = 1;
				break;
			case E_DIM::DIM_2D:
				surfInfoIn.height = std::max<uint32>(surfaceHeight >> level, 1);
				surfInfoIn.numSlices = 1;
				break;
			case E_DIM::DIM_3D:
				surfInfoIn.height = std::max<uint32>(surfaceHeight >> level, 1);
				surfInfoIn.numSlices = std::max<uint32>(surfaceDepth >> level, 1);
				surfInfoIn.flags.dim3D = true;
				break;
			case E_DIM::DIM_CUBEMAP:
				surfInfoIn.height = std::max<uint32>(surfaceHeight >> level, 1);
				surfInfoIn.numSlices = std::max<uint32>(surfaceDepth, 6);
				surfInfoIn.flags.dimCube = true;
				break;
			case E_DIM::DIM_1D_ARRAY:
				surfInfoIn.height = 1;
				surfInfoIn.numSlices = surfaceDepth;
				break;
			case E_DIM::DIM_2D_ARRAY:
				surfInfoIn.height = std::max<uint32>(surfaceHeight >> level, 1);
				surfInfoIn.numSlices = surfaceDepth;
				break;
			case E_DIM::DIM_2D_MSAA:
				surfInfoIn.height = std::max<uint32>(surfaceHeight >> level, 1);
				surfInfoIn.numSlices = 1;
				break;
			case E_DIM::DIM_2D_ARRAY_MSAA:
				surfInfoIn.height = std::max<uint32>(surfaceHeight >> level, 1);
				surfInfoIn.numSlices = surfaceDepth;
				break;
			default:
				break;
			}
			surfInfoIn.slice = 0;
			surfInfoIn.mipLevel = level;
			surfInfoIn.flags.inputIsBase = (level == 0);
			surfInfoIn.flags.depth = optimizeForDepthBuffer;
			surfInfoIn.flags.display = optimizeForScanBuffer;
			ComputeSurfaceInfo(&surfInfoIn, pSurfOut);
		}
	}

	uint32 CalculateMipOffset(Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, E_DIM dim, E_HWTILEMODE tileMode, uint32 swizzle, uint32 surfaceAA, sint32 mipIndex)
	{
		cemu_assert_debug(IsValidHWTileMode(tileMode));
		AddrSurfaceInfo_OUT surfaceInfo;
		uint32 currentMipOffset = 0;
		E_HWTILEMODE lastTileMode = tileMode;
		uint32 prevSize = 0;
		for (sint32 level = 1; level <= mipIndex; level++)
		{
			GX2CalculateSurfaceInfo(format, width, height, depth, dim, MakeGX2TileMode(tileMode), surfaceAA, level, &surfaceInfo);
			if (level)
			{
				uint32 pad = 0;
				if (TM_IsMacroTiled(lastTileMode) && !TM_IsMacroTiled(surfaceInfo.hwTileMode))
				{
					if (level > 1)
						pad = swizzle & 0xFFFF;
				}
				pad += (surfaceInfo.baseAlign - (currentMipOffset % surfaceInfo.baseAlign)) % surfaceInfo.baseAlign;
				currentMipOffset = currentMipOffset + pad + prevSize;
			}
			else
			{
				currentMipOffset = prevSize;
			}
			lastTileMode = surfaceInfo.hwTileMode;
			prevSize = (uint32)surfaceInfo.surfSize;
		}
		return currentMipOffset;
	}

	// Calculate aligned address and size of a given slice and mip level
	// For thick-tiled surfaces this returns the area of the whole thick tile (4 slices per thick tile) and the relative slice index within the tile is returned in subSliceIndex
	void CalculateMipAndSliceAddr(uint32 physAddr, uint32 physMipAddr, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, Latte::E_DIM dim, Latte::E_HWTILEMODE tileMode, uint32 swizzle, uint32 surfaceAA, sint32 mipIndex, sint32 sliceIndex, uint32* outputSliceOffset, uint32* outputSliceSize, sint32* subSliceIndex)
	{
		cemu_assert_debug((uint32)tileMode < 16); // only hardware tilemodes allowed

		AddrSurfaceInfo_OUT surfaceInfo;
		uint32 currentMipOffset = 0;
		Latte::E_HWTILEMODE lastTileMode = tileMode;
		uint32 prevSize = 0;
		for (sint32 level = 1; level <= mipIndex; level++)
		{
			GX2CalculateSurfaceInfo(format, width, height, depth, dim, MakeGX2TileMode(tileMode), surfaceAA, level, &surfaceInfo);
			// extract swizzle from mip-pointer if macro tiled
			if (level == 1 && TM_IsMacroTiled(surfaceInfo.hwTileMode))
			{
				swizzle = physMipAddr & 0x700;
				physMipAddr &= ~0x700;
			}
			cemu_assert_debug(IsValidHWTileMode(surfaceInfo.hwTileMode));
			if (level)
			{
				uint32 pad = 0;
				if (TM_IsMacroTiled(lastTileMode) && !TM_IsMacroTiled(surfaceInfo.hwTileMode))
				{
					if (level > 1)
						pad = swizzle & 0xFFFF;
				}
				pad += (surfaceInfo.baseAlign - (currentMipOffset % surfaceInfo.baseAlign)) % surfaceInfo.baseAlign;
				currentMipOffset = currentMipOffset + pad + prevSize;
			}
			else
			{
				currentMipOffset = prevSize;
			}
			lastTileMode = surfaceInfo.hwTileMode;
			prevSize = (uint32)surfaceInfo.surfSize;
		}
		// calculate slice offset
		if( mipIndex == 0 ) // make sure surfaceInfo is initialized
			GX2CalculateSurfaceInfo(format, width, height, depth, dim, MakeGX2TileMode(tileMode), surfaceAA, 0, &surfaceInfo);
		uint32 sliceOffset = 0;
		uint32 sliceSize = 0;

		// surfaceInfo.sliceSize isn't always correct (especially when depth is misaligned with 4 for THICK tile modes?) so we calculate it manually
		// this formula only works because both pitch and height are aligned to micro/macro blocks by GX2CalculateSurfaceInfo, normally we would have to use the tile dimensions to calculate the size
		uint32 correctedSliceSize = surfaceInfo.pitch*surfaceInfo.height*surfaceInfo.bpp / 8;
	
		if (TM_IsThick(surfaceInfo.hwTileMode))
		{
			// 4 slices are interleaved
			sliceOffset = (sliceIndex&~3) * correctedSliceSize;
			sliceSize = correctedSliceSize * 4;
			*subSliceIndex = sliceIndex & 3;
		}
		else
		{
			sliceOffset = sliceIndex * correctedSliceSize;
			sliceSize = correctedSliceSize;
			*subSliceIndex = 0;
		}
		if (mipIndex)
		{
			sliceOffset += physMipAddr;
		}
		else
		{
			sliceOffset += physAddr;
		}
		*outputSliceOffset = currentMipOffset + sliceOffset;
		*outputSliceSize = sliceSize;
	}

};
