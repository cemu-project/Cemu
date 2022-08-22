#pragma once
#include "Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.h"

template<typename texelBaseType, int texelBaseTypeCount, bool isEncodeDirection, bool isCompressed>
void optimizedDecodeLoop_tm04_numSamples1_8x8(LatteTextureLoaderCtx* textureLoader, uint8* outputData, sint32 texelCountX, sint32 texelCountY)
{
	uint16* tableBase = textureLoader->computeAddrInfo.microTilePixelIndexTable + ((textureLoader->computeAddrInfo.slice & 7) << 6);
	for (sint32 yt = 0; yt < texelCountY; yt += 8)
	{
		for (sint32 xt = 0; xt < texelCountX; xt += 8)
		{
			sint32 baseOffset = LatteAddrLib::ComputeSurfaceAddrFromCoordMacroTiledCached_tm04_sample1(xt, yt, &textureLoader->computeAddrInfo); // this is only 10-20% of execution time
			for (sint32 ry = 0; ry < 8; ry++)
			{
				sint32 pixelOffset = ((yt + ry)*textureLoader->decodedTexelCountX + (xt)) * (sizeof(texelBaseType)*texelBaseTypeCount);
				texelBaseType* blockOutput = (texelBaseType*)(outputData + pixelOffset);

				uint16* pixelOffsets = tableBase + (ry << 3);

				for (sint32 rx = 0; rx < 8; rx++)
				{
					uint32 pixelIndex = *pixelOffsets;
					pixelOffsets++;
					uint32 pixelOffset = pixelIndex * sizeof(texelBaseType)*texelBaseTypeCount;
					uint32 elemOffset = pixelOffset;
					if ((sizeof(texelBaseType)*texelBaseTypeCount * 8 * 8) > 256)
					{
						// separate group bytes, for small formats this step is not necessary since elemOffset is never over 0xFF (maximum is 8*8*bpp)
						elemOffset = (elemOffset & 0xFF) | ((elemOffset&~0xFF) << 3);
					}

					sint32 offset = baseOffset + elemOffset;

					uint8* blockData = textureLoader->inputData + offset;
					// copy as-is
					if (texelBaseTypeCount == 1)
					{
						if (isEncodeDirection)
							*(texelBaseType*)blockData = *blockOutput;
						else
							*blockOutput = *(texelBaseType*)blockData;
						blockOutput++;
					}
					else if (texelBaseTypeCount == 2)
					{
						if (isEncodeDirection)
						{
							((texelBaseType*)blockData)[0] = blockOutput[0];
							((texelBaseType*)blockData)[1] = blockOutput[1];
						}
						else
						{
							blockOutput[0] = ((texelBaseType*)blockData)[0];
							blockOutput[1] = ((texelBaseType*)blockData)[1];
						}
						blockOutput += 2;
					}
					else
						assert_dbg();
				}
			}
		}
	}
}

template<typename texelBaseType, int texelBaseTypeCount, bool isEncodeDirection, bool isCompressed>
void optimizedDecodeLoop_tm04_numSamples1_8x8_optimizedRowCopy(LatteTextureLoaderCtx* textureLoader, uint8* outputData, sint32 texelCountX, sint32 texelCountY)
{
	uint16* tableBase = textureLoader->computeAddrInfo.microTilePixelIndexTable + ((textureLoader->computeAddrInfo.slice & 7) << 6);
	for (sint32 yt = 0; yt < texelCountY; yt += 8)
	{
		for (sint32 xt = 0; xt < texelCountX; xt += 8)
		{
			sint32 baseOffset = ComputeSurfaceAddrFromCoordMacroTiledCached_tm04_sample1(xt, yt, &textureLoader->computeAddrInfo); // this is only 10-20% of execution time
			for (sint32 ry = 0; ry < 8; ry++)
			{
				sint32 pixelOffset = ((yt + ry)*textureLoader->decodedTexelCountX + (xt)) * (sizeof(texelBaseType)*texelBaseTypeCount);
				texelBaseType* blockOutput = (texelBaseType*)(outputData + pixelOffset);

				uint16* pixelOffsets = tableBase + (ry << 3);

				uint32 pixelIndex = *pixelOffsets;
				pixelOffsets++;
				uint32 elemOffset = pixelIndex * sizeof(texelBaseType)*texelBaseTypeCount;
				if ((sizeof(texelBaseType)*texelBaseTypeCount * 8 * 8) > 256)
				{
					// separate group bytes, for small formats this step is not necessary since elemOffset is never over 0xFF (maximum is 8*8*bpp)
					elemOffset = (elemOffset & 0xFF) | ((elemOffset&~0xFF) << 3);
				}

				sint32 offset = baseOffset + elemOffset;

				texelBaseType* blockData = (texelBaseType*)(textureLoader->inputData + offset);
				// x-to-offset translation table (for bpp = 64)
				// 0	->	0
				// 1	->	1
				// 2	->	4
				// 3	->	5
				// 4	->	8
				// 5	->	9
				// 6	->	12
				// 7	->	13

				// x-to-offset translation table (for bpp = 32)
				// 0	->	0
				// 1	->	1
				// 2	->	2
				// 3	->	3
				// 4	->	8
				// 5	->	9
				// 6	->	10
				// 7	->	11


				if ((sizeof(texelBaseType)*texelBaseTypeCount) == 8)
				{
					// bpp = 64
					if (texelBaseTypeCount == 1)
					{
						if (isEncodeDirection)
						{
							blockData[0] = blockOutput[0];
							blockData[1] = blockOutput[1];
							blockData[4] = blockOutput[2];
							blockData[5] = blockOutput[3];
							blockData[8] = blockOutput[4];
							blockData[9] = blockOutput[5];
							blockData[12] = blockOutput[6];
							blockData[13] = blockOutput[7];
						}
						else
						{
							blockOutput[0] = blockData[0];
							blockOutput[1] = blockData[1];
							blockOutput[2] = blockData[4];
							blockOutput[3] = blockData[5];
							blockOutput[4] = blockData[8];
							blockOutput[5] = blockData[9];
							blockOutput[6] = blockData[12];
							blockOutput[7] = blockData[13];
						}
						blockOutput += 8;
					}
					else
						assert_dbg();
				}
				else if ((sizeof(texelBaseType)*texelBaseTypeCount) == 4)
				{
					// bpp = 32
					if (texelBaseTypeCount == 1)
					{
						uint64* blockOutput64 = (uint64*)blockOutput;
						uint64* blockData64 = (uint64*)blockData;
						if (isEncodeDirection)
						{
							blockData64[0] = blockOutput64[0];
							blockData64[1] = blockOutput64[1];
							blockData64[4] = blockOutput64[2];
							blockData64[5] = blockOutput64[3];
						}
						else
						{
							blockOutput64[0] = blockData64[0];
							blockOutput64[1] = blockData64[1];
							blockOutput64[2] = blockData64[4];
							blockOutput64[3] = blockData64[5];
						}
						blockOutput += 8;
					}
					else
						cemu_assert_unimplemented();
				}
				else if ((sizeof(texelBaseType)*texelBaseTypeCount) == 1)
				{
					// bpp = 8
					if (texelBaseTypeCount == 1)
					{
						uint64* blockOutput64 = (uint64*)blockOutput;
						uint64* blockData64 = (uint64*)blockData;
						if (isEncodeDirection)
							blockData64[0] = blockOutput64[0];
						else
							blockOutput64[0] = blockData64[0];
						blockOutput += 8;
					}
					else
						cemu_assert_unimplemented();
				}
				else
					cemu_assert_unimplemented();
			}
		}
	}
}

template<typename texelBaseType, int texelBaseTypeCount, bool isEncodeDirection, bool isCompressed>
void optimizedDecodeLoops(LatteTextureLoaderCtx* textureLoader, uint8* outputData)
{
	sint32 texelCountX;
	sint32 texelCountY;
	if (isCompressed)
	{
		texelCountX = (textureLoader->width + 3) / 4;
		texelCountY = (textureLoader->height + 3) / 4;
	}
	else
	{
		texelCountX = textureLoader->width;
		texelCountY = textureLoader->height;
	}

	if (textureLoader->tileMode == Latte::E_HWTILEMODE::TM_2D_TILED_THIN1 && textureLoader->computeAddrInfo.numSamples == 1)
	{
		sint32 texelCountOrigX = texelCountX;
		sint32 texelCountOrigY = texelCountY;
		texelCountX &= ~7;
		texelCountY &= ~7;
		// full tiles (assuming tileMode=4 and numSamples=1)
		// only recalculate tile related offset at the beginning of each block
		// calculate offsets in loop

		// unsure if this variant is faster:
		if (textureLoader->computeAddrInfo.microTileType == 0 && (sizeof(texelBaseType)*texelBaseTypeCount) == 8)
		{
			optimizedDecodeLoop_tm04_numSamples1_8x8_optimizedRowCopy<texelBaseType, texelBaseTypeCount, isEncodeDirection, isCompressed>(textureLoader, outputData, texelCountX, texelCountY);
		}
		else if (textureLoader->computeAddrInfo.microTileType == 0 && (sizeof(texelBaseType)*texelBaseTypeCount) == 4)
		{
			optimizedDecodeLoop_tm04_numSamples1_8x8_optimizedRowCopy<texelBaseType, texelBaseTypeCount, isEncodeDirection, isCompressed>(textureLoader, outputData, texelCountX, texelCountY);
		}
		else if (textureLoader->computeAddrInfo.microTileType == 0 && (sizeof(texelBaseType)*texelBaseTypeCount) == 1)
		{
			optimizedDecodeLoop_tm04_numSamples1_8x8_optimizedRowCopy<texelBaseType, texelBaseTypeCount, isEncodeDirection, isCompressed>(textureLoader, outputData, texelCountX, texelCountY);
		}
		else
		{
			optimizedDecodeLoop_tm04_numSamples1_8x8<texelBaseType, texelBaseTypeCount, isEncodeDirection, isCompressed>(textureLoader, outputData, texelCountX, texelCountY);
		}
		// the above code only handles full 8x8 pixel blocks, for uneven sizes we need to process the remaining pixels here
		// right border
		for (sint32 yt = 0; yt < texelCountY; yt++)
		{
			sint32 pixelOffset = (yt*textureLoader->decodedTexelCountX + texelCountX) * (sizeof(texelBaseType)*texelBaseTypeCount);
			texelBaseType* blockOutput = (texelBaseType*)(outputData + pixelOffset);
			for (sint32 xt = texelCountX; xt < texelCountOrigX; xt++)
			{
				sint32 offset = ComputeSurfaceAddrFromCoordMacroTiledCached_tm04_sample1(xt, yt, &textureLoader->computeAddrInfo);
				uint8* blockData = textureLoader->inputData + offset;
				// copy as-is
				if (texelBaseTypeCount == 1)
				{
					if (isEncodeDirection)
						*(texelBaseType*)blockData = *blockOutput;
					else
						*blockOutput = *(texelBaseType*)blockData;
					blockOutput++;
				}
				else if (texelBaseTypeCount == 2)
				{
					if (isEncodeDirection)
					{
						((texelBaseType*)blockData)[0] = blockOutput[0];
						((texelBaseType*)blockData)[1] = blockOutput[1];
					}
					else
					{
						blockOutput[0] = ((texelBaseType*)blockData)[0];
						blockOutput[1] = ((texelBaseType*)blockData)[1];
					}
					blockOutput += 2;
				}
			}
		}
		// bottom border (with bottom right corner)
		for (sint32 yt = texelCountY; yt < texelCountOrigY; yt++)
		{
			sint32 pixelOffset = (yt*textureLoader->decodedTexelCountX) * (sizeof(texelBaseType)*texelBaseTypeCount);
			texelBaseType* blockOutput = (texelBaseType*)(outputData + pixelOffset);
			for (sint32 xt = 0; xt < texelCountOrigX; xt++)
			{
				sint32 offset = ComputeSurfaceAddrFromCoordMacroTiledCached_tm04_sample1(xt, yt, &textureLoader->computeAddrInfo);
				uint8* blockData = textureLoader->inputData + offset;
				// copy as-is
				if (texelBaseTypeCount == 1)
				{
					if (isEncodeDirection)
						*(texelBaseType*)blockData = *blockOutput;
					else
						*blockOutput = *(texelBaseType*)blockData;
					blockOutput++;
				}
				else if (texelBaseTypeCount == 2)
				{
					if (isEncodeDirection)
					{
						((texelBaseType*)blockData)[0] = blockOutput[0];
						((texelBaseType*)blockData)[1] = blockOutput[1];
					}
					else
					{
						blockOutput[0] = ((texelBaseType*)blockData)[0];
						blockOutput[1] = ((texelBaseType*)blockData)[1];
					}
					blockOutput += 2;
				}
			}
		}
	}
	else if (textureLoader->tileMode == Latte::E_HWTILEMODE::TM_LINEAR_ALIGNED)
	{
		// optimized handler for linear textures
		uint32 sliceOffset = textureLoader->sliceIndex * textureLoader->height * textureLoader->pitch;
		for (sint32 y = 0; y < texelCountY; y++)
		{
			sint32 pixelOffset = (y*textureLoader->decodedTexelCountX) * (sizeof(texelBaseType)*texelBaseTypeCount);
			texelBaseType* blockOutput = (texelBaseType*)(outputData + pixelOffset);
			texelBaseType* blockData = (texelBaseType*)(textureLoader->inputData + (textureLoader->pitch * y + sliceOffset) * (sizeof(texelBaseType)*texelBaseTypeCount));
			for (sint32 x = 0; x < texelCountX; x++)
			{
				// copy as-is
				if (texelBaseTypeCount == 1)
				{
					if(isEncodeDirection)
						*(texelBaseType*)blockData = *blockOutput;
					else
						*blockOutput = *(texelBaseType*)blockData;
					blockData++;
					blockOutput++;
				}
				else if (texelBaseTypeCount == 2)
				{
					if (isEncodeDirection)
					{
						((texelBaseType*)blockData)[0] = blockOutput[0];
						((texelBaseType*)blockData)[1] = blockOutput[1];
					}
					else
					{
						blockOutput[0] = ((texelBaseType*)blockData)[0];
						blockOutput[1] = ((texelBaseType*)blockData)[1];
					}
					blockData += 2;
					blockOutput += 2;
				}
			}
		}
	}
	else
	{
		// generic handler
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 pixelOffset = ((y / textureLoader->stepY)*textureLoader->decodedTexelCountX) * (sizeof(texelBaseType)*texelBaseTypeCount);
			texelBaseType* blockOutput = (texelBaseType*)(outputData + pixelOffset);
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				// copy as-is
				if (texelBaseTypeCount == 1)
				{
					if (isEncodeDirection)
						*(texelBaseType*)blockData = *blockOutput;
					else
						*blockOutput = *(texelBaseType*)blockData;
					blockOutput++;
				}
				else if (texelBaseTypeCount == 2)
				{
					if (isEncodeDirection)
					{
						((texelBaseType*)blockData)[0] = blockOutput[0];
						((texelBaseType*)blockData)[1] = blockOutput[1];
					}
					else
					{
						blockOutput[0] = ((texelBaseType*)blockData)[0];
						blockOutput[1] = ((texelBaseType*)blockData)[1];
					}
					blockOutput += 2;
				}
			}
		}
	}
}