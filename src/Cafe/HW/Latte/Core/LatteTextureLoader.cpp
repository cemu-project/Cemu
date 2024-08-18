#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.h"
#include "config/ActiveSettings.h"
#include "Cafe/CafeSystem.h"

//#define BENCHMARK_TEXTURE_DECODING		// if defined, time it takes to decode textures will be measured and logged to log.txt

#ifdef BENCHMARK_TEXTURE_DECODING
uint64 textureDecodeBenchmark_perFormatSum[0x40] = { 0 }; // duration sum per texture format (hw format) - in microseconds
uint64 textureDecodeBenchmark_totalSum = 0;
#endif

void LatteTextureLoader_begin(LatteTextureLoaderCtx* textureLoader, uint32 sliceIndex, uint32 mipIndex, MPTR physImagePtr, MPTR physMipPtr, Latte::E_GX2SURFFMT format, Latte::E_DIM dim, uint32 width, uint32 height, uint32 depth, uint32 mipLevels, uint32 pitch, Latte::E_HWTILEMODE tileMode, uint32 swizzle)
{
	textureLoader->physAddress = physImagePtr;
	textureLoader->physMipAddress = physMipPtr;
	textureLoader->sliceIndex = sliceIndex;
	cemu_assert_debug(mipLevels != 0);
	textureLoader->mipLevels = std::max<uint32>(1, mipLevels);
	textureLoader->tileMode = tileMode;
	textureLoader->bpp = Latte::GetFormatBits(format);
	textureLoader->stepX = 1;
	textureLoader->stepY = 1;
	if (Latte::IsCompressedFormat(format))
	{
		textureLoader->stepX = 4;
		textureLoader->stepY = 4;
	}

	textureLoader->pipeSwizzle = (swizzle >> 8) & 1;
	textureLoader->bankSwizzle = ((swizzle >> 9) & 3);

	uint32 surfaceAA = 0; // todo

	if (mipIndex > 0 && Latte::TM_IsMacroTiled(tileMode))
	{
		// separate swizzle from mip pointer if mip chain is not macro-tiled (and thus not swizzled)
		LatteAddrLib::AddrSurfaceInfo_OUT surfaceInfo;
		LatteAddrLib::GX2CalculateSurfaceInfo(format, width, height, depth, dim, Latte::MakeGX2TileMode(tileMode), surfaceAA, 1, &surfaceInfo);
		if (Latte::TM_IsMacroTiled(surfaceInfo.hwTileMode))
		{
			uint32 mipSwizzle = physMipPtr&0x700;
			physMipPtr &= ~0x700;
			textureLoader->physMipAddress = physMipPtr;
			textureLoader->pipeSwizzle = (mipSwizzle >> 8) & 1;
			textureLoader->bankSwizzle = ((mipSwizzle >> 9) & 3);
		}
	}

	// calculate surface info
	uint32 level = mipIndex;
	LatteAddrLib::AddrSurfaceInfo_OUT surfaceInfo;
	LatteAddrLib::GX2CalculateSurfaceInfo(format, width, height, depth, dim, Latte::MakeGX2TileMode(tileMode), surfaceAA, level, &surfaceInfo);
	textureLoader->levelOffset = LatteAddrLib::CalculateMipOffset(format, width, height, depth, dim, (Latte::E_HWTILEMODE)tileMode, swizzle, surfaceAA, level);
	textureLoader->tileMode = surfaceInfo.hwTileMode;

	textureLoader->minOffsetOutdated = 0;
	textureLoader->maxOffsetOutdated = (sint32)surfaceInfo.surfSize;

	textureLoader->surfaceInfoHeight = surfaceInfo.height;
	textureLoader->surfaceInfoDepth = surfaceInfo.depth;

	// correct handling for LINEAR_ALIGNED pitch alignment is still not fully understood:
	//seems like sometimes there is a conditional pitch alignment to 0x40 OR there is no pitch alignment at all and we have a bug somewhere else

	uint64 titleId = CafeSystem::GetForegroundTitleId();
	titleId &= ~0x300ULL;

	if (tileMode == Latte::E_HWTILEMODE::TM_LINEAR_ALIGNED && titleId == (0x000500301001200aULL))
	{
		// examples of titles that use linear textures:
		// Minecraft - Uses sprite atlases with mips and linear tilemode. Expects padding of pitch for smaller mips to be 0x40
		// Browser - Linear pitch must be used as-is, padding/alignment will break textures (uses a weird way to calculate pitch by using GX2CalcSurface on a texture with tileMode 0/4)
		// BotW - uses linear textures as render targets. With the smallest resolution being 3x3 with no pitch alignment expected at all (pitch = 3)? -> Not possible because both textures and rendertargets require a minimum alignment of 8 for pitch?
		surfaceInfo.pitch = std::max<uint32>(1, pitch >> mipIndex);
	}


	textureLoader->width = width >> (mipIndex);
	textureLoader->width = std::max(textureLoader->width, 1);
	textureLoader->height = height >> (mipIndex);
	textureLoader->height = std::max(textureLoader->height, 1);

	textureLoader->pitch = surfaceInfo.pitch;
	// calculate start address
	if (level == 0)
		textureLoader->inputData = (uint8*)memory_getPointerFromPhysicalOffset(physImagePtr);
	else
		textureLoader->inputData = (uint8*)memory_getPointerFromPhysicalOffset(physMipPtr) + textureLoader->levelOffset;

	SetupCachedSurfaceAddrInfo(&textureLoader->computeAddrInfo, textureLoader->sliceIndex, 0, textureLoader->bpp, textureLoader->pitch, surfaceInfo.height, depth, 1 * 1, textureLoader->tileMode, false, textureLoader->pipeSwizzle, textureLoader->bankSwizzle);
}

uint8* LatteTextureLoader_GetInput(LatteTextureLoaderCtx* textureLoader, sint32 x, sint32 y)
{
	// calculate address of input tile
	uint32 offset = 0;
	if (textureLoader->tileMode == Latte::E_HWTILEMODE::TM_LINEAR_GENERAL || textureLoader->tileMode == Latte::E_HWTILEMODE::TM_LINEAR_ALIGNED)
		offset = LatteAddrLib::ComputeSurfaceAddrFromCoordLinear(x / textureLoader->stepX, y / textureLoader->stepY, textureLoader->sliceIndex, 0, textureLoader->bpp, textureLoader->pitch, textureLoader->surfaceInfoHeight, textureLoader->surfaceInfoDepth);
	else if (textureLoader->tileMode == Latte::E_HWTILEMODE::TM_1D_TILED_THIN1 || textureLoader->tileMode == Latte::E_HWTILEMODE::TM_1D_TILED_THICK)
		offset = LatteAddrLib::ComputeSurfaceAddrFromCoordMicroTiled(x / textureLoader->stepX, y / textureLoader->stepY, textureLoader->sliceIndex, textureLoader->bpp, textureLoader->pitch, textureLoader->surfaceInfoHeight, (Latte::E_HWTILEMODE)textureLoader->tileMode, false);
	else
		offset = LatteAddrLib::ComputeSurfaceAddrFromCoordMacroTiledCached(x / textureLoader->stepX, y / textureLoader->stepY, &textureLoader->computeAddrInfo);
	uint8* blockData = textureLoader->inputData + offset;
	return blockData;
}

/*
 * Optimized version which assumes tileMode == 1
 * Also does not do any min/max offset tracking
 */
uint8* LatteTextureLoader_getInputLinearOptimized(LatteTextureLoaderCtx* textureLoader, sint32 x, sint32 y)
{
	// calculate address of input tile
	uint32 bitPos = 0;
	uint32 offset = 0;
	offset = LatteAddrLib::ComputeSurfaceAddrFromCoordLinear(x / textureLoader->stepX, y / textureLoader->stepY, textureLoader->sliceIndex, 0, textureLoader->bpp, textureLoader->pitch, textureLoader->surfaceInfoHeight, textureLoader->surfaceInfoDepth);
	return textureLoader->inputData + offset;
}

#define LatteTextureLoader_getInputLinearOptimized_(__textureLoader,__x,__y,__stepX,__stepY,__bpp,__sliceIndex,__numSlices,__sample,__pitch,__height) (textureLoader->inputData+((__x/__stepX) + __pitch * (__y/__stepY) + (__sliceIndex + __numSlices * __sample) * __height * __pitch)*(__bpp/8))

float SRGB_to_RGB(float cs)
{
	float cl;
	if (cs <= 0.04045f)
		cl = cs / 12.92f;
	else
		cl = powf(((cs + 0.055f) / 1.055f), 2.4f);
	return cl;
}

void decodeBC1Block(uint8* inputData, float* output4x4RGBA)
{
	// read colors
	uint16 c0 = *(uint16*)(inputData + 0);
	uint16 c1 = *(uint16*)(inputData + 2);
	// decode colors (RGB565 -> RGB888)
	float r[4];
	float g[4];
	float b[4];
	float a[4];
	b[0] = (float)((c0 >> 0) & 0x1F) / 31.0f;
	b[1] = (float)((c1 >> 0) & 0x1F) / 31.0f;
	g[0] = (float)((c0 >> 5) & 0x3F) / 63.0f;
	g[1] = (float)((c1 >> 5) & 0x3F) / 63.0f;
	r[0] = (float)((c0 >> 11) & 0x1F) / 31.0f;
	r[1] = (float)((c1 >> 11) & 0x1F) / 31.0f;
	a[0] = 1.0f;
	a[1] = 1.0f;
	a[2] = 1.0f;

	if (c0 > c1)
	{
		r[2] = (r[0] * 2.0f + r[1]) / 3.0f;
		r[3] = (r[0] * 1.0f + r[1] * 2.0f) / 3.0f;
		g[2] = (g[0] * 2.0f + g[1]) / 3.0f;
		g[3] = (g[0] * 1.0f + g[1] * 2.0f) / 3.0f;
		b[2] = (b[0] * 2.0f + b[1]) / 3.0f;
		b[3] = (b[0] * 1.0f + b[1] * 2.0f) / 3.0f;
		a[3] = 1.0f;
	}
	else
	{
		r[2] = (r[0] + r[1]) / 2.0f;
		r[3] = 0.0f;
		g[2] = (g[0] + g[1]) / 2.0f;
		g[3] = 0.0f;
		b[2] = (b[0] + b[1]) / 2.0f;
		b[3] = 0.0f;
		a[3] = 0.0f;
	}

	uint8* indexData = inputData + 4;
	float* colorOutputRGBA = output4x4RGBA;
	for (sint32 row = 0; row < 4; row++)
	{
		uint8 i0 = ((*indexData) >> 0) & 3;
		uint8 i1 = ((*indexData) >> 2) & 3;
		uint8 i2 = ((*indexData) >> 4) & 3;
		uint8 i3 = ((*indexData) >> 6) & 3;
		colorOutputRGBA[0] = r[i0];
		colorOutputRGBA[1] = g[i0];
		colorOutputRGBA[2] = b[i0];
		colorOutputRGBA[3] = a[i0];
		colorOutputRGBA += 4;
		colorOutputRGBA[0] = r[i1];
		colorOutputRGBA[1] = g[i1];
		colorOutputRGBA[2] = b[i1];
		colorOutputRGBA[3] = a[i1];
		colorOutputRGBA += 4;
		colorOutputRGBA[0] = r[i2];
		colorOutputRGBA[1] = g[i2];
		colorOutputRGBA[2] = b[i2];
		colorOutputRGBA[3] = a[i2];
		colorOutputRGBA += 4;
		colorOutputRGBA[0] = r[i3];
		colorOutputRGBA[1] = g[i3];
		colorOutputRGBA[2] = b[i3];
		colorOutputRGBA[3] = a[i3];
		colorOutputRGBA += 4;
		indexData++;
	}
}

void decodeBC2Block_UNORM(uint8* inputData, float* imageRGBA)
{
	uint32 color0 = *(uint16*)(inputData + 8);
	uint32 color1 = *(uint16*)(inputData + 10);
	uint32 colorIndices = *(uint32*)(inputData + 12);

	uint8 r0 = (color0 >> 11) & 0x1F;
	uint8 g0 = (color0 >> 5) & 0x3F;
	uint8 b0 = (color0 >> 0) & 0x1F;

	uint8 r1 = (color1 >> 11) & 0x1F;
	uint8 g1 = (color1 >> 5) & 0x3F;
	uint8 b1 = (color1 >> 0) & 0x1F;

	float r[4];
	float g[4];
	float b[4];
	r[0] = (float)r0 / 31.0f;
	r[1] = (float)r1 / 31.0f;
	r[2] = (r[0] * 2.0f + r[1]) / 3.0f;
	r[3] = (r[0] + r[1] * 2.0f) / 3.0f;
	g[0] = (float)g0 / 63.0f;
	g[1] = (float)g1 / 63.0f;
	g[2] = (g[0] * 2.0f + g[1]) / 3.0f;
	g[3] = (g[0] + g[1] * 2.0f) / 3.0f;
	b[0] = (float)b0 / 31.0f;
	b[1] = (float)b1 / 31.0f;
	b[2] = (b[0] * 2.0f + b[1]) / 3.0f;
	b[3] = (b[0] + b[1] * 2.0f) / 3.0f;

	for (sint32 py = 0; py < 4; py++)
	{
		for (sint32 px = 0; px < 4; px++)
		{
			uint8 colorIndex = (colorIndices >> (2 * (px + 4 * py))) & 0x03;
			sint32 pixelOffset = (px + py * 4) * 4;
			imageRGBA[pixelOffset + 0] = r[colorIndex];
			imageRGBA[pixelOffset + 1] = g[colorIndex];
			imageRGBA[pixelOffset + 2] = b[colorIndex];
		}
	}

	// decode alpha
	uint8* alphaData = (uint8*)(inputData + 0);
	for (sint32 py = 0; py < 4; py++)
	{
		for (sint32 px = 0; px < 4; px++)
		{
			uint32 alphaIndex = (px + py * 4);
			uint8 alphaCode = (alphaData[alphaIndex / 2] >> ((alphaIndex & 1) * 4)) & 0xF;
			alphaCode |= (alphaCode << 4);
			sint32 pixelOffset = (px + py * 4) * 4;
			imageRGBA[pixelOffset + 3] = (float)alphaCode / 255.0f; // alpha
		}
	}
}

void decodeBC3Block_UNORM(uint8* inputData, float* imageRGBA)
{
	uint32 color0 = *(uint16*)(inputData + 8);
	uint32 color1 = *(uint16*)(inputData + 10);
	uint32 colorIndices = *(uint32*)(inputData + 12);

	uint8 r0 = (color0 >> 11) & 0x1F;
	uint8 g0 = (color0 >> 5) & 0x3F;
	uint8 b0 = (color0 >> 0) & 0x1F;

	uint8 r1 = (color1 >> 11) & 0x1F;
	uint8 g1 = (color1 >> 5) & 0x3F;
	uint8 b1 = (color1 >> 0) & 0x1F;

	float r[4];
	float g[4];
	float b[4];
	r[0] = (float)r0 / 31.0f;
	r[1] = (float)r1 / 31.0f;
	r[2] = (r[0] * 2.0f + r[1]) / 3.0f;
	r[3] = (r[0] + r[1] * 2.0f) / 3.0f;
	g[0] = (float)g0 / 63.0f;
	g[1] = (float)g1 / 63.0f;
	g[2] = (g[0] * 2.0f + g[1]) / 3.0f;
	g[3] = (g[0] + g[1] * 2.0f) / 3.0f;
	b[0] = (float)b0 / 31.0f;
	b[1] = (float)b1 / 31.0f;
	b[2] = (b[0] * 2.0f + b[1]) / 3.0f;
	b[3] = (b[0] + b[1] * 2.0f) / 3.0f;

	for (sint32 py = 0; py < 4; py++)
	{
		for (sint32 px = 0; px < 4; px++)
		{
			uint8 colorIndex = (colorIndices >> (2 * (px + 4 * py))) & 0x03;
			sint32 pixelOffset = (px + py * 4) * 4;
			imageRGBA[pixelOffset + 0] = r[colorIndex];
			imageRGBA[pixelOffset + 1] = g[colorIndex];
			imageRGBA[pixelOffset + 2] = b[colorIndex];
			//imageRGBA[pixelOffset+3] = 1.0f; // alpha
		}
	}

	// decode alpha
	uint8 alpha0 = *(uint8*)(inputData + 0);
	uint8 alpha1 = *(uint8*)(inputData + 1);
	uint32 alphaCodeRow[2] = { 0 };
	alphaCodeRow[0] |= ((*(uint8*)(inputData + 2)) << 0);
	alphaCodeRow[0] |= ((*(uint8*)(inputData + 3)) << 8);
	alphaCodeRow[0] |= ((*(uint8*)(inputData + 4)) << 16);
	alphaCodeRow[1] |= ((*(uint8*)(inputData + 5)) << 0);
	alphaCodeRow[1] |= ((*(uint8*)(inputData + 6)) << 8);
	alphaCodeRow[1] |= ((*(uint8*)(inputData + 7)) << 16);

	float a[8];
	a[0] = (float)alpha0 / 255.0f;
	a[1] = (float)alpha1 / 255.0f;

	if (alpha0 > alpha1)
	{
		// 6 interpolated alpha values.
		a[2] = (a[0] * 6.0f + a[1] * 1.0f) / 7.0f;
		a[3] = (a[0] * 5.0f + a[1] * 2.0f) / 7.0f;
		a[4] = (a[0] * 4.0f + a[1] * 3.0f) / 7.0f;
		a[5] = (a[0] * 3.0f + a[1] * 4.0f) / 7.0f;
		a[6] = (a[0] * 2.0f + a[1] * 5.0f) / 7.0f;
		a[7] = (a[0] * 1.0f + a[1] * 6.0f) / 7.0f;
	}
	else
	{
		// 4 interpolated alpha values.
		a[2] = (a[0] * 4.0f + a[1] * 1.0f) / 5.0f;
		a[3] = (a[0] * 3.0f + a[1] * 2.0f) / 5.0f;
		a[4] = (a[0] * 2.0f + a[1] * 3.0f) / 5.0f;
		a[5] = (a[0] * 1.0f + a[1] * 4.0f) / 5.0f;
		a[6] = 0.0f;
		a[7] = 1.0f;
	}

	for (sint32 py = 0; py < 4; py++)
	{
		for (sint32 px = 0; px < 4; px++)
		{
			uint8 alphaCode = (alphaCodeRow[py / 2] >> 3 * (px + 4 * (py & 1))) & 0x07;
			sint32 pixelOffset = (px + py * 4) * 4;
			imageRGBA[pixelOffset + 3] = a[alphaCode]; // alpha
		}
	}
}

void decodeBC4Block_UNORM(uint8* blockStorage, float* rOutput)
{
	uint8* blockInput = (uint8*)blockStorage;
	float red[8];

	red[0] = ((float)(*(uint8*)(blockInput + 0))) / 255.0f;
	red[1] = ((float)(*(uint8*)(blockInput + 1))) / 255.0f;

	if (blockInput[0] > blockInput[1])
	{
		// 6 interpolated color values
		red[2] = (6 * red[0] + 1 * red[1]) / 7.0f; // bit code 010
		red[3] = (5 * red[0] + 2 * red[1]) / 7.0f; // bit code 011
		red[4] = (4 * red[0] + 3 * red[1]) / 7.0f; // bit code 100
		red[5] = (3 * red[0] + 4 * red[1]) / 7.0f; // bit code 101
		red[6] = (2 * red[0] + 5 * red[1]) / 7.0f; // bit code 110
		red[7] = (1 * red[0] + 6 * red[1]) / 7.0f; // bit code 111
	}
	else
	{
		// 4 interpolated color values
		red[2] = (4 * red[0] + 1 * red[1]) / 5.0f; // bit code 010
		red[3] = (3 * red[0] + 2 * red[1]) / 5.0f; // bit code 011
		red[4] = (2 * red[0] + 3 * red[1]) / 5.0f; // bit code 100
		red[5] = (1 * red[0] + 4 * red[1]) / 5.0f; // bit code 101
		red[6] = 0.0f;                       // bit code 110
		red[7] = 1.0f;                       // bit code 111
	}

	uint8* bitIndices = blockInput + 2;
	uint32 redRow0 = (((uint32)bitIndices[2]) << 16) | (((uint32)bitIndices[1]) << 8) | (((uint32)bitIndices[0]) << 0);
	uint32 redRow1 = (((uint32)bitIndices[5]) << 16) | (((uint32)bitIndices[4]) << 8) | (((uint32)bitIndices[3]) << 0);

	uint8 pRed[16];
	for (sint32 i = 0; i < 8; i++)
	{
		pRed[i] = (redRow0 >> (i * 3)) & 7;
		pRed[i + 8] = (redRow1 >> (i * 3)) & 7;
	}

	float* pixelOutput = rOutput;
	for (sint32 py = 0; py < 4; py++)
	{
		for (sint32 px = 0; px < 4; px++)
		{
			float c = red[pRed[px + py * 4]];
			*pixelOutput = c;
			pixelOutput++;
		}
	}
}

void decodeBC5Block_UNORM(uint8* blockStorage, float* rgOutput)
{
	uint8* blockInput = (uint8*)blockStorage;
	float red[8];
	float green[8];

	red[0] = ((float)(*(uint8*)(blockInput + 0))) / 255.0f;
	red[1] = ((float)(*(uint8*)(blockInput + 1))) / 255.0f;

	if (red[0] > red[1])
	{
		// 6 interpolated color values
		red[2] = (6 * red[0] + 1 * red[1]) / 7.0f; // bit code 010
		red[3] = (5 * red[0] + 2 * red[1]) / 7.0f; // bit code 011
		red[4] = (4 * red[0] + 3 * red[1]) / 7.0f; // bit code 100
		red[5] = (3 * red[0] + 4 * red[1]) / 7.0f; // bit code 101
		red[6] = (2 * red[0] + 5 * red[1]) / 7.0f; // bit code 110
		red[7] = (1 * red[0] + 6 * red[1]) / 7.0f; // bit code 111
	}
	else
	{
		// 4 interpolated color values
		red[2] = (4 * red[0] + 1 * red[1]) / 5.0f; // bit code 010
		red[3] = (3 * red[0] + 2 * red[1]) / 5.0f; // bit code 011
		red[4] = (2 * red[0] + 3 * red[1]) / 5.0f; // bit code 100
		red[5] = (1 * red[0] + 4 * red[1]) / 5.0f; // bit code 101
		red[6] = 0.0f;                       // bit code 110
		red[7] = 1.0f;                       // bit code 111
	}

	green[0] = ((float)(*(uint8*)(blockInput + 8))) / 255.0f;
	green[1] = ((float)(*(uint8*)(blockInput + 9))) / 255.0f;

	if (green[0] > green[1])
	{
		// 6 interpolated color values
		green[2] = (6 * green[0] + 1 * green[1]) / 7.0f; // bit code 010
		green[3] = (5 * green[0] + 2 * green[1]) / 7.0f; // bit code 011
		green[4] = (4 * green[0] + 3 * green[1]) / 7.0f; // bit code 100
		green[5] = (3 * green[0] + 4 * green[1]) / 7.0f; // bit code 101
		green[6] = (2 * green[0] + 5 * green[1]) / 7.0f; // bit code 110
		green[7] = (1 * green[0] + 6 * green[1]) / 7.0f; // bit code 111
	}
	else
	{
		// 4 interpolated color values
		green[2] = (4 * green[0] + 1 * green[1]) / 5.0f; // bit code 010
		green[3] = (3 * green[0] + 2 * green[1]) / 5.0f; // bit code 011
		green[4] = (2 * green[0] + 3 * green[1]) / 5.0f; // bit code 100
		green[5] = (1 * green[0] + 4 * green[1]) / 5.0f; // bit code 101
		green[6] = 0.0f;						   // bit code 110
		green[7] = 1.0f;                           // bit code 111
	}


	uint8* bitIndices = blockInput + 2;
	uint32 redRow0 = (((uint32)bitIndices[2]) << 16) | (((uint32)bitIndices[1]) << 8) | (((uint32)bitIndices[0]) << 0);
	uint32 redRow1 = (((uint32)bitIndices[5]) << 16) | (((uint32)bitIndices[4]) << 8) | (((uint32)bitIndices[3]) << 0);
	bitIndices = blockInput + 8 + 2;
	uint32 greenRow0 = (((uint32)bitIndices[2]) << 16) | (((uint32)bitIndices[1]) << 8) | (((uint32)bitIndices[0]) << 0);
	uint32 greenRow1 = (((uint32)bitIndices[5]) << 16) | (((uint32)bitIndices[4]) << 8) | (((uint32)bitIndices[3]) << 0);

	uint8 pRed[16];
	uint8 pGreen[16];
	for (sint32 i = 0; i < 8; i++)
	{
		pRed[i] = (redRow0 >> (i * 3)) & 7;
		pRed[i + 8] = (redRow1 >> (i * 3)) & 7;
		pGreen[i] = (greenRow0 >> (i * 3)) & 7;
		pGreen[i + 8] = (greenRow1 >> (i * 3)) & 7;
	}

	float* pixelOutput = rgOutput;
	for (sint32 py = 0; py < 4; py++)
	{
		for (sint32 px = 0; px < 4; px++)
		{
			float c = red[pRed[px + py * 4]];
			*pixelOutput = c;
			pixelOutput++;
			c = green[pGreen[px + py * 4]];
			*pixelOutput = c;
			pixelOutput++;
		}
	}
}

void decodeBC5Block_SNORM(uint8* blockStorage, float* rgOutput) // todo - can merge this with the UNORM implementation by using a template?
{
	uint8* blockInput = (uint8*)blockStorage;
	float red[8];
	float green[8];

	red[0] = ((float)(*(sint8*)(blockInput + 0)) + 128.0f) / 255.0f;
	red[1] = ((float)(*(sint8*)(blockInput + 1)) + 128.0f) / 255.0f;
	red[0] = (red[0] * 2.0f - 1.0f);
	red[1] = (red[1] * 2.0f - 1.0f);

	if (red[0] > red[1])
	{
		// 6 interpolated color values
		red[2] = (6 * red[0] + 1 * red[1]) / 7.0f; // bit code 010
		red[3] = (5 * red[0] + 2 * red[1]) / 7.0f; // bit code 011
		red[4] = (4 * red[0] + 3 * red[1]) / 7.0f; // bit code 100
		red[5] = (3 * red[0] + 4 * red[1]) / 7.0f; // bit code 101
		red[6] = (2 * red[0] + 5 * red[1]) / 7.0f; // bit code 110
		red[7] = (1 * red[0] + 6 * red[1]) / 7.0f; // bit code 111
	}
	else
	{
		// 4 interpolated color values
		red[2] = (4 * red[0] + 1 * red[1]) / 5.0f; // bit code 010
		red[3] = (3 * red[0] + 2 * red[1]) / 5.0f; // bit code 011
		red[4] = (2 * red[0] + 3 * red[1]) / 5.0f; // bit code 100
		red[5] = (1 * red[0] + 4 * red[1]) / 5.0f; // bit code 101
		red[6] = -1.0f;                       // bit code 110
		red[7] = 1.0f;                       // bit code 111
	}

	green[0] = ((float)(*(sint8*)(blockInput + 8)) + 128.0f) / 255.0f;
	green[1] = ((float)(*(sint8*)(blockInput + 9)) + 128.0f) / 255.0f;
	green[0] = (green[0] * 2.0f - 1.0f);
	green[1] = (green[1] * 2.0f - 1.0f);

	if (green[0] > green[1])
	{
		// 6 interpolated color values
		green[2] = (6 * green[0] + 1 * green[1]) / 7.0f; // bit code 010
		green[3] = (5 * green[0] + 2 * green[1]) / 7.0f; // bit code 011
		green[4] = (4 * green[0] + 3 * green[1]) / 7.0f; // bit code 100
		green[5] = (3 * green[0] + 4 * green[1]) / 7.0f; // bit code 101
		green[6] = (2 * green[0] + 5 * green[1]) / 7.0f; // bit code 110
		green[7] = (1 * green[0] + 6 * green[1]) / 7.0f; // bit code 111
	}
	else
	{
		// 4 interpolated color values
		green[2] = (4 * green[0] + 1 * green[1]) / 5.0f; // bit code 010
		green[3] = (3 * green[0] + 2 * green[1]) / 5.0f; // bit code 011
		green[4] = (2 * green[0] + 3 * green[1]) / 5.0f; // bit code 100
		green[5] = (1 * green[0] + 4 * green[1]) / 5.0f; // bit code 101
		green[6] = -1.0f;                       // bit code 110
		green[7] = 1.0f;                       // bit code 111
	}


	uint8* bitIndices = blockInput + 2;
	uint32 redRow0 = (((uint32)bitIndices[2]) << 16) | (((uint32)bitIndices[1]) << 8) | (((uint32)bitIndices[0]) << 0);
	uint32 redRow1 = (((uint32)bitIndices[5]) << 16) | (((uint32)bitIndices[4]) << 8) | (((uint32)bitIndices[3]) << 0);
	bitIndices = blockInput + 8 + 2;
	uint32 greenRow0 = (((uint32)bitIndices[2]) << 16) | (((uint32)bitIndices[1]) << 8) | (((uint32)bitIndices[0]) << 0);
	uint32 greenRow1 = (((uint32)bitIndices[5]) << 16) | (((uint32)bitIndices[4]) << 8) | (((uint32)bitIndices[3]) << 0);

	uint8 pRed[16];
	uint8 pGreen[16];
	for (sint32 i = 0; i < 8; i++)
	{
		pRed[i] = (redRow0 >> (i * 3)) & 7;
		pRed[i + 8] = (redRow1 >> (i * 3)) & 7;
		pGreen[i] = (greenRow0 >> (i * 3)) & 7;
		pGreen[i + 8] = (greenRow1 >> (i * 3)) & 7;
	}

	for (sint32 py = 0; py < 4; py++)
	{
		float* pixelOutput = rgOutput + (py * 4) * 2;
		for (sint32 px = 0; px < 4; px++)
		{
			float c = red[pRed[px + py * 4]];
			pixelOutput[0] = c;
			c = green[pGreen[px + py * 4]];
			pixelOutput[1] = c;
			pixelOutput += 2;
		}
	}
}

void LatteTextureLoader_loadTextureDataIntoSlice(LatteTexture* hostTexture, sint32 width, sint32 height, sint32 depth, sint32 mipLevels, void* pixelData, sint32 sliceIndex, sint32 mipIndex, uint32 compressedImageSize)
{
	if (mipIndex == 0)
	{
		cemu_assert_debug(width == hostTexture->width);
		cemu_assert_debug(height == hostTexture->height);
		cemu_assert_debug(depth == hostTexture->depth);
	}
	cemu_assert_debug(mipLevels == hostTexture->mipLevels);
	if (hostTexture->overwriteInfo.hasResolutionOverwrite || hostTexture->overwriteInfo.hasFormatOverwrite)
	{
		// todo - ideally, we should scale/convert the data to the new format and resolution
		g_renderer->texture_clearSlice(hostTexture, sliceIndex, mipIndex);
	}
	else
	{
		g_renderer->texture_loadSlice(hostTexture, width, height, depth, pixelData, sliceIndex, mipIndex, compressedImageSize);
	}
}

void LatteTextureLoader_UpdateTextureSliceData(LatteTexture* tex, uint32 sliceIndex, uint32 mipIndex, MPTR physImagePtr, MPTR physMipPtr, Latte::E_DIM dim, uint32 width, uint32 height, uint32 depth, uint32 mipLevels, uint32 pitch, Latte::E_HWTILEMODE tileMode, uint32 swizzle, bool dumpTex)
{
	LatteTextureLoaderCtx textureLoader = { 0 };
	
	Latte::E_GX2SURFFMT format = tex->format;
	LatteTextureLoader_begin(&textureLoader, sliceIndex, mipIndex, physImagePtr, physMipPtr, format, dim, width, height, depth, mipLevels, pitch, tileMode, swizzle);

	// enable texture dumping
	textureLoader.dump = ActiveSettings::DumpTexturesEnabled();
	if (textureLoader.dump)
	{
		uint32 dumpSize = (((textureLoader.width + 4)&~4) * ((textureLoader.height + 4)&~4)) * 4;
		textureLoader.dumpRGBA = (uint8*)malloc(dumpSize);
		memset(textureLoader.dumpRGBA, 0x00, dumpSize);
	}

	// query texture decoder from renderer
	TextureDecoder* texDecoder = nullptr;
	texDecoder = g_renderer->texture_chooseDecodedFormat(format, tex->isDepth, dim, width, height);

	if (tex->isDataDefined == false)
	{
		tex->AllocateOnHost();
		tex->isDataDefined = true;
		// if decoder is not set then clear texture
		// on Vulkan this is used to make sure the texture is no longer in UNDEFINED layout
		if (!texDecoder)
		{
			if(tex->isDepth)
				g_renderer->texture_clearDepthSlice(tex, 0, 0, true, tex->hasStencil, 0.0f, 0);
			else
				g_renderer->texture_clearColorSlice(tex, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
		}
	}

	if (texDecoder == nullptr)
		return;

	textureLoader.decodedTexelCountX = texDecoder->getTexelCountX(&textureLoader);
	textureLoader.decodedTexelCountY = texDecoder->getTexelCountY(&textureLoader);

	// allocate memory for decoded texture
	uint32 imageSize = texDecoder->calculateImageSize(&textureLoader);

	uint8* pixelData = (uint8*)g_renderer->texture_acquireTextureUploadBuffer(imageSize);
	// decode texture (if data is required)
#ifdef BENCHMARK_TEXTURE_DECODING
	LARGE_INTEGER benchmark_begin;
	LARGE_INTEGER benchmark_end;
	LARGE_INTEGER benchmark_freq;
	QueryPerformanceCounter(&benchmark_begin);
#endif
	if (tex->overwriteInfo.hasFormatOverwrite == false && tex->overwriteInfo.hasResolutionOverwrite == false)
	{
		texDecoder->decode(&textureLoader, pixelData);
	}
#ifdef BENCHMARK_TEXTURE_DECODING
	QueryPerformanceCounter(&benchmark_end);
	QueryPerformanceFrequency(&benchmark_freq);
	uint64 benchmarkResultMicroSeconds = (benchmark_end.QuadPart - benchmark_begin.QuadPart) * 1000000ULL / benchmark_freq.QuadPart;
	textureDecodeBenchmark_perFormatSum[(int)tex->format & 0x3F] += benchmarkResultMicroSeconds;
	textureDecodeBenchmark_totalSum += benchmarkResultMicroSeconds;
	cemuLog_log(LogType::Force, "TexDecode {:04}x{:04}x{:04} Fmt {:04x} Dim {} TileMode {:02x} Took {:03}.{:03}ms Sum(format) {:06}ms Sum(total) {:06}ms", textureLoader.width, textureLoader.height, textureLoader.surfaceInfoDepth, (int)tex->format, (int)tex->dim, textureLoader.tileMode, (uint32)(benchmarkResultMicroSeconds / 1000ULL), (uint32)(benchmarkResultMicroSeconds % 1000ULL), (uint32)(textureDecodeBenchmark_perFormatSum[tex->gx2Format & 0x3F] / 1000ULL), (uint32)(textureDecodeBenchmark_totalSum / 1000ULL));
#endif

	// convert texture to RGBA when dumping is enabled
	if (textureLoader.dump)
	{
		for (sint32 y = 0; y < textureLoader.height; y++)
		{
			sint32 pixelOffset = (y * textureLoader.width) * 4;
			uint8* pixelOutput = textureLoader.dumpRGBA + pixelOffset;
			for (sint32 x = 0; x < textureLoader.width; x++)
			{
				uint8* blockData = LatteTextureLoader_GetInput(&textureLoader, x, y);
				texDecoder->decodePixelToRGBA(blockData, pixelOutput, x % textureLoader.stepX, y % textureLoader.stepY);
				pixelOutput += 4;
			}
		}
	}

	// update texture data offsets and hashes
	// this has to be done before the texture data is decoded & uploaded to prevent a race condition where updates during upload are missed
	if (mipIndex == 0 || (tex->texDataPtrLow == 0 && tex->texDataPtrHigh == 0))
	{
		tex->texDataPtrLow = physImagePtr + textureLoader.minOffsetOutdated; // always zero
		tex->texDataPtrHigh = physImagePtr + textureLoader.maxOffsetOutdated; // currently set to surface size
		LatteTC_ResetTextureChangeTracker(tex, true);
	}
	// load slice
	//debug_printf("[Load Slice] Addr: %08x MIP: %02d Slice: %02d Res %04x/%04x Texel Res %04x/%04x Fmt %04x Tm %d\n", textureLoader.physAddress, mipIndex, sliceIndex, textureLoader.width, textureLoader.height, textureLoader.texelCountX, textureLoader.texelCountY, (int)format, tileMode);
	LatteTextureLoader_loadTextureDataIntoSlice(tex, textureLoader.width, textureLoader.height, depth, mipLevels, pixelData, sliceIndex, mipIndex, imageSize);
	// write texture dump
	if (textureLoader.dump)
	{
		wchar_t path[1024];
		swprintf(path, 1024, L"dump/textures/%08x_fmt%04x_slice%d_mip%02d_%dx%d_tm%02d.tga", physImagePtr, (uint32)tex->format, sliceIndex, mipIndex, tex->width, tex->height, tileMode);
		tga_write_rgba(path, textureLoader.width, textureLoader.height, textureLoader.dumpRGBA);
		free(textureLoader.dumpRGBA);
	}
	// clean up
	g_renderer->texture_releaseTextureUploadBuffer(pixelData);
	catchOpenGLError();
}

template<typename copyType>
void optimizedLinearReadbackWriteLoop(LatteTextureLoaderCtx* textureLoader, uint8* linearPixelData)
{
	uint32 pitch = textureLoader->width;
	// optimized for linear
	for (sint32 y = 0; y < textureLoader->height; y++)
	{
		sint32 yc = y;
		sint32 pixelOffset = yc * pitch;
		copyType* rowPixelData = (copyType*)(linearPixelData + pixelOffset * sizeof(copyType));
		copyType* blockData = (copyType*)LatteTextureLoader_getInputLinearOptimized_(textureLoader, 0, y, 1, 1, sizeof(copyType) * 8, 0, 1, 0, textureLoader->pitch, textureLoader->height);
		if constexpr (sizeof(copyType) == 4)
		{
			memcpy_dwords(blockData, rowPixelData, textureLoader->width);
		}
		else
		{
			for (sint32 x = 0; x < textureLoader->width; x++)
			{
				*blockData = *rowPixelData;
				rowPixelData++;
				blockData++;
			}
		}
	}
}

void LatteTextureLoader_writeReadbackTextureToMemory(LatteTextureDefinition* textureData, uint32 sliceIndex, uint32 mipIndex, uint8* linearPixelData)
{
	LatteTextureLoaderCtx textureLoader = { 0 };
	LatteTextureLoader_begin(&textureLoader, sliceIndex, mipIndex, textureData->physAddress, textureData->physMipAddress, textureData->format, textureData->dim, textureData->width, textureData->height, textureData->depth, textureData->mipLevels, textureData->pitch, textureData->tileMode, textureData->swizzle);

#ifdef CEMU_DEBUG_ASSERT
	if (textureData->depth != 1)
		cemuLog_log(LogType::Force, "_writeReadbackTextureToMemory(): Texture has multiple slices (not supported)");
#endif
	if (textureLoader.physAddress == MPTR_NULL)
	{
		cemuLog_log(LogType::Force, "_writeReadbackTextureToMemory(): Texture has invalid address");
		return;
	}

	cemuLog_log(LogType::TextureReadback, "[TextureReadback-Write] PhysAddr {:08x} Res {}x{} Fmt {} Slice {} Mip {}", textureData->physAddress, textureData->width, textureData->height, textureData->format, sliceIndex, mipIndex);

	if (textureData->tileMode == Latte::E_HWTILEMODE::TM_LINEAR_ALIGNED)
	{
		uint32 pitch = textureLoader.width;
		if (textureData->format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM ||
			textureData->format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB)
		{
			optimizedLinearReadbackWriteLoop<uint32>(&textureLoader, linearPixelData);
		}
		else if (textureData->format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM)
		{
			optimizedLinearReadbackWriteLoop<uint64>(&textureLoader, linearPixelData);
		}
		else if (textureData->format == Latte::E_GX2SURFFMT::R32_G32_B32_A32_FLOAT)
		{
			for (sint32 y = 0; y < textureLoader.height; y += textureLoader.stepY)
			{
				sint32 yc = y;
				sint32 pixelOffset = (0 + yc * pitch) * 16;
				for (sint32 x = 0; x < textureLoader.width; x += textureLoader.stepX)
				{
					uint8* blockData = LatteTextureLoader_getInputLinearOptimized(&textureLoader, x, y);
					(*(uint32*)(blockData + 0)) = *(uint32*)(linearPixelData + pixelOffset + 0);
					(*(uint32*)(blockData + 4)) = *(uint32*)(linearPixelData + pixelOffset + 4);
					(*(uint32*)(blockData + 8)) = *(uint32*)(linearPixelData + pixelOffset + 8);
					(*(uint32*)(blockData + 12)) = *(uint32*)(linearPixelData + pixelOffset + 12);
					pixelOffset += 16;
				}
			}
		}
		else if (textureData->format == Latte::E_GX2SURFFMT::R32_FLOAT)
		{
			for (sint32 y = 0; y < textureLoader.height; y += textureLoader.stepY)
			{
				sint32 yc = y;
				for (sint32 x = 0; x < textureLoader.width; x += textureLoader.stepX)
				{
					uint8* blockData = LatteTextureLoader_getInputLinearOptimized(&textureLoader, x, y);
					sint32 pixelOffset = (x + yc * pitch) * 4;
					(*(uint32*)(blockData + 0)) = *(uint32*)(linearPixelData + pixelOffset + 0);
				}
			}
		}
		else if (textureData->format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT)
		{
			for (sint32 y = 0; y < textureLoader.height; y += textureLoader.stepY)
			{
				sint32 yc = y;
				for (sint32 x = 0; x < textureLoader.width; x += textureLoader.stepX)
				{
					uint8* blockData = LatteTextureLoader_getInputLinearOptimized(&textureLoader, x, y);
					sint32 pixelOffset = (x + yc * pitch) * 8;
					(*(uint32*)(blockData + 0)) = *(uint32*)(linearPixelData + pixelOffset + 0);
					(*(uint32*)(blockData + 4)) = *(uint32*)(linearPixelData + pixelOffset + 4);
				}
			}
		}
		else if (textureData->format == Latte::E_GX2SURFFMT::R8_G8_UNORM)
		{
			optimizedLinearReadbackWriteLoop<uint16>(&textureLoader, linearPixelData);
		}
		else if (textureData->format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM)
		{
			cemu_assert_unimplemented();
		}
		else if (textureData->format == Latte::E_GX2SURFFMT::R16_UNORM)
		{
			optimizedLinearReadbackWriteLoop<uint16>(&textureLoader, linearPixelData);
		}
		else
		{
			cemuLog_logDebug(LogType::Force, "Linear texture readback unsupported for format 0x{:04x}", (uint32)textureData->format);
			debugBreakpoint();
		}
		return;
	}
	// generic and slow decode loops
	Latte::E_HWSURFFMT hwFormat = Latte::GetHWFormat(textureData->format);
	if (hwFormat == Latte::E_HWSURFFMT::HWFMT_8_8_8_8)
	{
		// used in Bayonetta 2
		for (sint32 y = 0; y < textureLoader.height; y++)
		{
			uint8* pixelInput = linearPixelData + (y * textureLoader.width) * 4;
			for (sint32 x = 0; x < textureLoader.width; x++)
			{
				uint8* outputData = LatteTextureLoader_GetInput(&textureLoader, x, y);
				*(uint32*)(outputData + 0) = *(uint32*)pixelInput;
				pixelInput += 4;
			}
		}
	}
	else if (hwFormat == Latte::E_HWSURFFMT::HWFMT_32_FLOAT)
	{
		// required by Wind Waker for direct access to depth buffer
		// Bayonetta 2 also uses this but it converts the depth buffer to a color texture first
		for (sint32 y = 0; y < textureLoader.height; y++)
		{
			uint8* pixelInput = linearPixelData + (y * textureLoader.width) * 4;
			for (sint32 x = 0; x < textureLoader.width; x++)
			{
				uint8* outputData = LatteTextureLoader_GetInput(&textureLoader, x, y);
				*(uint32*)(outputData + 0) = *(uint32*)pixelInput;
				pixelInput += 4;
			}
		}
	}	
	else
	{
		cemuLog_logDebug(LogType::Force, "Texture readback unsupported format {:04x} for tileMode 0x{:02x}", (uint32)textureData->format, textureData->tileMode);
	}

}

void LatteTextureLoader_estimateAccessedDataRange(LatteTexture* texture, sint32 sliceIndex, sint32 mipIndex, uint32& addrStart, uint32& addrEnd)
{
	LatteTextureLoaderCtx textureLoader = { 0 };
	LatteTextureLoader_begin(&textureLoader, sliceIndex, mipIndex, texture->physAddress, texture->physMipAddress, texture->format, texture->dim, texture->width, texture->height, texture->depth, texture->mipLevels, texture->pitch, texture->tileMode, texture->swizzle);

	cemu_assert_debug(textureLoader.width > 0);
	cemu_assert_debug(textureLoader.height > 0);

	// estimate data range by checking addresses of corner pixels
	// this isn't very reliable, find a better solution
	uint32 estimatedMinAddr = 0xFFFFFFFF;
	uint32 estimatedMaxAddr = 0x00000000;
	uint32 tempAddr;
	tempAddr = memory_getVirtualOffsetFromPointer(LatteTextureLoader_GetInput(&textureLoader, 0, 0));
	estimatedMinAddr = std::min(estimatedMinAddr, tempAddr);
	estimatedMaxAddr = std::max(estimatedMaxAddr, tempAddr);
	tempAddr = memory_getVirtualOffsetFromPointer(LatteTextureLoader_GetInput(&textureLoader, textureLoader.width - 1, 0));
	estimatedMinAddr = std::min(estimatedMinAddr, tempAddr);
	estimatedMaxAddr = std::max(estimatedMaxAddr, tempAddr);
	tempAddr = memory_getVirtualOffsetFromPointer(LatteTextureLoader_GetInput(&textureLoader, 0, textureLoader.height - 1));
	estimatedMinAddr = std::min(estimatedMinAddr, tempAddr);
	estimatedMaxAddr = std::max(estimatedMaxAddr, tempAddr);
	tempAddr = memory_getVirtualOffsetFromPointer(LatteTextureLoader_GetInput(&textureLoader, textureLoader.width - 1, textureLoader.height - 1));
	estimatedMinAddr = std::min(estimatedMinAddr, tempAddr);
	estimatedMaxAddr = std::max(estimatedMaxAddr, tempAddr);

	addrStart = estimatedMinAddr;
	addrEnd = estimatedMaxAddr;
}
