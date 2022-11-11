#pragma once
#include "util/helpers/ClassWrapper.h"
#include "Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.h"
#include "util/ImageWriter/tga.h"

struct LatteTextureLoaderCtx
{
	uint32 physAddress;
	uint32 physMipAddress;
	sint32 width;
	sint32 height;
	sint32 pitch; // stored elements per row
	uint32 mipLevels;
	uint32 sliceIndex;
	sint32 stepX;
	sint32 stepY;
	uint32 pipeSwizzle;
	uint32 bankSwizzle;
	Latte::E_HWTILEMODE tileMode;
	uint32 bpp;
	uint8* inputData;
	sint32 minOffsetOutdated;
	sint32 maxOffsetOutdated;
	// calculated info
	uint32 surfaceInfoHeight;
	uint32 surfaceInfoDepth;
	// mip
	uint32 levelOffset; // relative to physMipAddress
	// info for decoded texture
	sint32 decodedTexelCountX;
	sint32 decodedTexelCountY;
	// decoder
	LatteAddrLib::CachedSurfaceAddrInfo computeAddrInfo;
	// debug dump texture
	bool dump;
	uint8* dumpRGBA;
};

uint8* LatteTextureLoader_GetInput(LatteTextureLoaderCtx* textureLoader, sint32 x, sint32 y);

#include "Cafe/HW/Latte/LatteAddrLib/AddrLibFastDecode.h"

void decodeBC1Block(uint8* inputData, float* output4x4RGBA);
void decodeBC2Block_UNORM(uint8* inputData, float* imageRGBA);
void decodeBC3Block_UNORM(uint8* inputData, float* imageRGBA);
void decodeBC4Block_UNORM(uint8* blockStorage, float* rOutput);
void decodeBC5Block_UNORM(uint8* blockStorage, float* rgOutput);
void decodeBC5Block_SNORM(uint8* blockStorage, float* rgOutput);

inline void BC1_GetPixel(uint8* inputData, sint32 x, sint32 y, uint8 rgba[4])
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

	uint8 i = (indexData[y] >> (x * 2)) & 3;
	rgba[0] = (uint8)(r[i] * 255.0f);
	rgba[1] = (uint8)(g[i] * 255.0f);
	rgba[2] = (uint8)(b[i] * 255.0f);
	rgba[3] = (uint8)(a[i] * 255.0f);
}

// decodes a specific GPU7 texture format into a native linear format that can be used by the render API
class TextureDecoder
{
public:
	// properties
	virtual sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) = 0;
	virtual sint32 getTexelCountX(LatteTextureLoaderCtx* textureLoader)
	{
		return textureLoader->width;
	}

	virtual sint32 getTexelCountY(LatteTextureLoaderCtx* textureLoader)
	{
		return textureLoader->height;
	}

	// image size
	virtual sint32 calculateImageSize(LatteTextureLoaderCtx* textureLoader)
	{
		return getTexelCountX(textureLoader) * getTexelCountY(textureLoader) * getBytesPerTexel(textureLoader);
	}

	// decode loop
	virtual void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) = 0;

	virtual void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) = 0;
};

class TextureDecoder_R16_G16_B16_A16_FLOAT : public TextureDecoder, public SingletonClass<TextureDecoder_R16_G16_B16_A16_FLOAT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint64, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		// todo: dump 16 bit float formats properly
		float r16f = (float)(*((uint16*)blockData + 0));
		float g16f = (float)(*((uint16*)blockData + 1));
		float b16f = (float)(*((uint16*)blockData + 2));
		float a16f = (float)(*((uint16*)blockData + 3));
		*(outputPixel + 0) = (uint8)(r16f * 255.0f);
		*(outputPixel + 1) = (uint8)(g16f * 255.0f);
		*(outputPixel + 2) = (uint8)(b16f * 255.0f);
		*(outputPixel + 3) = (uint8)(a16f * 255.0f);
	}
};

class TextureDecoder_R16_G16_FLOAT : public TextureDecoder, public SingletonClass<TextureDecoder_R16_G16_FLOAT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2 * 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint32, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		// todo: dump 16 bit float formats properly
		float r16f = (float)(*((uint16*)blockData + 0));
		float g16f = (float)(*((uint16*)blockData + 1));
		*(outputPixel + 0) = (uint8)(r16f * 255.0f);
		*(outputPixel + 1) = (uint8)(g16f * 255.0f);
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R16_SNORM : public TextureDecoder, public SingletonClass<TextureDecoder_R16_SNORM>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 1 * 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint16, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = (uint8)(*(((uint16*)blockData) + 0) >> 8) / 2 + 128;
		*(outputPixel + 1) = 0;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R16_FLOAT : public TextureDecoder, public SingletonClass<TextureDecoder_R16_FLOAT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 1 * 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint16, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		// todo: dump 16 bit float formats properly
		float r16f = (float)(*((uint16*)blockData + 0));
		*(outputPixel + 0) = (uint8)(r16f * 255.0f);
		*(outputPixel + 1) = 0;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R32_FLOAT : public TextureDecoder, public SingletonClass<TextureDecoder_R32_FLOAT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 1 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint32, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		float r32f = *((float*)blockData + 0);
		*(outputPixel + 0) = (uint8)(r32f * 255.0f);
		*(outputPixel + 1) = 0;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R32_G32_FLOAT : public TextureDecoder, public SingletonClass<TextureDecoder_R32_G32_FLOAT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint64, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		float r32f = *((float*)blockData + 0);
		float g32f = *((float*)blockData + 1);
		*(outputPixel + 0) = (uint8)(r32f * 255.0f);
		*(outputPixel + 1) = (uint8)(g32f * 255.0f);
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R32_G32_UINT : public TextureDecoder, public SingletonClass<TextureDecoder_R32_G32_UINT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint64, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = (uint8)(*(((uint32*)blockData) + 0) >> 24);
		*(outputPixel + 1) = (uint8)(*(((uint32*)blockData) + 1) >> 24);
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};


class TextureDecoder_R32_UINT : public TextureDecoder, public SingletonClass<TextureDecoder_R32_UINT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 1 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint32, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = (uint8)(*(((uint32*)blockData) + 0) >> 24);
		*(outputPixel + 1) = 0;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R16_UINT : public TextureDecoder, public SingletonClass<TextureDecoder_R16_UINT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 1 * 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint16, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = (uint8)(*(((uint16*)blockData) + 0) >> 8);
		*(outputPixel + 1) = 0;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R8_UINT : public TextureDecoder, public SingletonClass<TextureDecoder_R8_UINT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 1;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint8, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = *(blockData + 0);
		*(outputPixel + 1) = 0;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R32_G32_B32_A32_FLOAT : public TextureDecoder, public SingletonClass<TextureDecoder_R32_G32_B32_A32_FLOAT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint64, 2, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		float r32f = *((float*)blockData + 0);
		float g32f = *((float*)blockData + 1);
		float b32f = *((float*)blockData + 2);
		float a32f = *((float*)blockData + 3);
		*(outputPixel + 0) = (uint8)(r32f * 255.0f);
		*(outputPixel + 1) = (uint8)(g32f * 255.0f);
		*(outputPixel + 2) = (uint8)(b32f * 255.0f);
		*(outputPixel + 3) = (uint8)(a32f * 255.0f);
	}
};

class TextureDecoder_R32_G32_B32_A32_UINT : public TextureDecoder, public SingletonClass<TextureDecoder_R32_G32_B32_A32_UINT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		// note - before 1.15.4 this format was implemented as big-endian
		//optimizedDecodeLoops<uint64, 2, false>(textureLoader, outputData);

		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 4 * sizeof(uint32); // write to target buffer
				*(uint32*)(outputData + pixelOffset + 0) = _swapEndianU32(*(uint32*)(blockData + 0));
				*(uint32*)(outputData + pixelOffset + 4) = _swapEndianU32(*(uint32*)(blockData + 4));
				*(uint32*)(outputData + pixelOffset + 8) = _swapEndianU32(*(uint32*)(blockData + 8));
				*(uint32*)(outputData + pixelOffset + 12) = _swapEndianU32(*(uint32*)(blockData + 12));
				// todo: Verify if this format is big-endian
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = (uint8)(*(((uint32*)blockData) + 0) >> 24);
		*(outputPixel + 1) = (uint8)(*(((uint32*)blockData) + 1) >> 24);
		*(outputPixel + 2) = (uint8)(*(((uint32*)blockData) + 2) >> 24);
		*(outputPixel + 3) = (uint8)(*(((uint32*)blockData) + 3) >> 24);
	}
};

class TextureDecoder_R16_G16_B16_A16_UINT : public TextureDecoder, public SingletonClass<TextureDecoder_R16_G16_B16_A16_UINT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		// note - before 1.15.4 this format was implemented as big-endian
		//optimizedDecodeLoops<uint64, 1, false>(textureLoader, outputData);

		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 4 * sizeof(uint16); // write to target buffer
				*(uint16*)(outputData + pixelOffset + 0) = _swapEndianU16(*(uint16*)(blockData + 0));
				*(uint16*)(outputData + pixelOffset + 2) = _swapEndianU16(*(uint16*)(blockData + 2));
				*(uint16*)(outputData + pixelOffset + 4) = _swapEndianU16(*(uint16*)(blockData + 4));
				*(uint16*)(outputData + pixelOffset + 6) = _swapEndianU16(*(uint16*)(blockData + 6));
				// todo: Verify if this format is big-endian
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = (uint8)(*(((uint16*)blockData) + 0) >> 8);
		*(outputPixel + 1) = (uint8)(*(((uint16*)blockData) + 1) >> 8);
		*(outputPixel + 2) = (uint8)(*(((uint16*)blockData) + 2) >> 8);
		*(outputPixel + 3) = (uint8)(*(((uint16*)blockData) + 3) >> 8);
	}
};


class TextureDecoder_R8_G8_B8_A8_UINT : public TextureDecoder, public SingletonClass<TextureDecoder_R8_G8_B8_A8_UINT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 1;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint32, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = *(blockData + 0);
		*(outputPixel + 1) = *(blockData + 1);
		*(outputPixel + 2) = *(blockData + 2);
		*(outputPixel + 3) = *(blockData + 3);
	}
};

class TextureDecoder_R24_X8 : public TextureDecoder, public SingletonClass<TextureDecoder_R24_X8>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 1 * sizeof(uint32);
				*(uint32*)(outputData + pixelOffset + 0) = 0;
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
	}
};

class TextureDecoder_X24_G8_UINT : public TextureDecoder, public SingletonClass<TextureDecoder_X24_G8_UINT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 1 * sizeof(uint32);
				*(uint32*)(outputData + pixelOffset + 0) = 0;
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
	}
};

class TextureDecoder_D32_S8_UINT_X24 : public TextureDecoder, public SingletonClass<TextureDecoder_D32_S8_UINT_X24>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 8;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint64, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
	}
};

class TextureDecoder_R4_G4_UNORM_To_RGBA4 : public TextureDecoder, public SingletonClass<TextureDecoder_R4_G4_UNORM_To_RGBA4>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 2;
				uint8 v = (*(uint8*)(blockData + 0));
				*(uint8*)(outputData + pixelOffset + 0) = 0; // OpenGL has no RG4 format so we use RGBA4 instead and these two values (blue + alpha) are always set to zero
				*(uint8*)(outputData + pixelOffset + 1) = ((v >> 4) & 0xF) | ((v << 4) & 0xF0); // todo: Is this nibble swap correct?
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint8 v0 = *(blockData + 0);
		uint8 c0 = (v0 & 0xF);
		uint8 c1 = (v0 >> 4) & 0xF;
		c0 = (c0 << 4) | c0;
		c1 = (c1 << 4) | c1;
		*(outputPixel + 0) = c0;
		*(outputPixel + 1) = c1;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R4_G4_UNORM_To_RGBA4_vk : public TextureDecoder, public SingletonClass<TextureDecoder_R4_G4_UNORM_To_RGBA4_vk>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 2;
				uint8 v = (*(uint8*)(blockData + 0));
				*(uint8*)(outputData + pixelOffset + 0) = 0;
				*(uint8*)(outputData + pixelOffset + 1) = v;
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint8 v0 = *(blockData + 0);
		uint8 c0 = (v0 & 0xF);
		uint8 c1 = (v0 >> 4) & 0xF;
		c0 = (c0 << 4) | c0;
		c1 = (c1 << 4) | c1;
		*(outputPixel + 0) = c0;
		*(outputPixel + 1) = c1;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R4G4_UNORM_To_RGBA8 : public TextureDecoder, public SingletonClass<TextureDecoder_R4G4_UNORM_To_RGBA8>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 4;
				uint8 v0 = (*(uint8*)(blockData + 0));

				uint8 red4 = (v0 >> 4) & 0xF;
				uint8 green4 = (v0 & 0xF);

				red4 = (red4 << 4) | red4;
				green4 = (green4 << 4) | green4;

				*(uint8*)(outputData + pixelOffset + 0) = red4;
				*(uint8*)(outputData + pixelOffset + 1) = green4;
				*(uint8*)(outputData + pixelOffset + 2) = 0;
				*(uint8*)(outputData + pixelOffset + 3) = 255;
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint8 v0 = *(blockData + 0);
		uint8 red4 = (v0 >> 4) & 0xF;
		uint8 green4 = (v0 & 0xF);
		red4 = (red4 << 4) | red4;
		green4 = (green4 << 4) | green4;
		*(outputPixel + 0) = red4;
		*(outputPixel + 1) = green4;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R4_G4_B4_A4_UNORM : public TextureDecoder, public SingletonClass<TextureDecoder_R4_G4_B4_A4_UNORM>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 2;
				uint8 v0 = (*(uint8*)(blockData + 0));
				uint8 v1 = (*(uint8*)(blockData + 1));
				*(uint8*)(outputData + pixelOffset + 0) = ((v1 >> 4) & 0xF) | ((v1 << 4) & 0xF0); // todo: Verify
				*(uint8*)(outputData + pixelOffset + 1) = ((v0 >> 4) & 0xF) | ((v0 << 4) & 0xF0); // todo: Verify
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint8 v0 = *(blockData + 0);
		uint8 v1 = *(blockData + 1);
		uint8 c0 = (v0 & 0xF);
		uint8 c1 = (v0 >> 4) & 0xF;
		uint8 c2 = (v1 & 0xF);
		uint8 c3 = (v1 >> 4) & 0xF;
		c0 = (c0 << 4) | c0;
		c1 = (c1 << 4) | c1;
		c2 = (c2 << 4) | c2;
		c3 = (c3 << 4) | c3;
		*(outputPixel + 0) = c0;
		*(outputPixel + 1) = c1;
		*(outputPixel + 2) = c2;
		*(outputPixel + 3) = c3;
	}
};


class TextureDecoder_R4G4B4A4_UNORM_To_RGBA8 : public TextureDecoder, public SingletonClass<TextureDecoder_R4G4B4A4_UNORM_To_RGBA8>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 4;
				uint8 v0 = (*(uint8*)(blockData + 0));
				uint8 v1 = (*(uint8*)(blockData + 1));

				uint8 red4 = (v0 & 0xF);
				uint8 green4 = (v0 >> 4) & 0xF;
				uint8 blue4 = (v1) & 0xF;
				uint8 alpha4 = (v1 >> 4) & 0xF;

				red4 = (red4 << 4) | red4;
				green4 = (green4 << 4) | green4;
				blue4 = (blue4 << 4) | blue4;
				alpha4 = (alpha4 << 4) | alpha4;

				*(uint8*)(outputData + pixelOffset + 0) = red4;
				*(uint8*)(outputData + pixelOffset + 1) = green4;
				*(uint8*)(outputData + pixelOffset + 2) = blue4;
				*(uint8*)(outputData + pixelOffset + 3) = alpha4;
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint8 v0 = *(blockData + 0);
		uint8 v1 = *(blockData + 1);

		uint8 red4 = (v0 & 0xF);
		uint8 green4 = (v0 >> 4) & 0xF;
		uint8 blue4 = (v1) & 0xF;
		uint8 alpha4 = (v1 >> 4) & 0xF;

		red4 = (red4 << 4) | red4;
		green4 = (green4 << 4) | green4;
		blue4 = (blue4 << 4) | blue4;
		alpha4 = (alpha4 << 4) | alpha4;

		*(outputPixel + 0) = red4;
		*(outputPixel + 1) = green4;
		*(outputPixel + 2) = blue4;
		*(outputPixel + 3) = alpha4;
	}
};

class TextureDecoder_R8_G8_B8_A8 : public TextureDecoder, public SingletonClass<TextureDecoder_R8_G8_B8_A8>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint32, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = *(blockData + 0);
		*(outputPixel + 1) = *(blockData + 1);
		*(outputPixel + 2) = *(blockData + 2);
		*(outputPixel + 3) = *(blockData + 3);
	}
};

class TextureDecoder_D24_S8 : public TextureDecoder, public SingletonClass<TextureDecoder_D24_S8>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint32, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint32 d24 = (*(uint32*)blockData) & 0xFFFFFF;
		uint8 s8 = (*(uint32*)blockData >> 24) & 0xFF;
		*(outputPixel + 0) = (uint8)(d24 >> 16);
		*(outputPixel + 1) = (uint8)(d24 >> 16);
		*(outputPixel + 2) = (uint8)(d24 >> 16);
		*(outputPixel + 3) = s8;
	}
};

class TextureDecoder_NullData32 : public TextureDecoder, public SingletonClass<TextureDecoder_NullData32>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		memset(outputData, 0, sizeof(uint32) * getTexelCountX(textureLoader) * getTexelCountY(textureLoader));
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
	}
};

class TextureDecoder_NullData64 : public TextureDecoder, public SingletonClass<TextureDecoder_NullData64>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 8;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		memset(outputData, 0, sizeof(uint64) * getTexelCountX(textureLoader) * getTexelCountY(textureLoader));
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
	}
};

class TextureDecoder_R8 : public TextureDecoder, public SingletonClass<TextureDecoder_R8>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 1;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint8, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = *(blockData + 0);
		*(outputPixel + 1) = 0;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R8_G8 : public TextureDecoder, public SingletonClass<TextureDecoder_R8_G8>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint16, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = *(blockData + 0);
		*(outputPixel + 1) = *(blockData + 1);
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R4_G4 : public TextureDecoder, public SingletonClass<TextureDecoder_R4_G4>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 1;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint8, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint8 v0 = (*(uint8*)(blockData + 0));
		uint8 c0 = (v0 & 0xF);
		uint8 c1 = (v0 >> 4) & 0xF;
		c0 = (c0 << 4) | c0;
		c1 = (c1 << 4) | c1;
		*(outputPixel + 0) = c0;
		*(outputPixel + 1) = c1;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R16_UNORM : public TextureDecoder, public SingletonClass<TextureDecoder_R16_UNORM>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint16, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = (uint8)(*(((uint16*)blockData) + 0) >> 8);
		*(outputPixel + 1) = 0;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R16_G16_B16_A16 : public TextureDecoder, public SingletonClass<TextureDecoder_R16_G16_B16_A16>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint64, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = (uint8)(*(((uint16*)blockData) + 0) >> 8);
		*(outputPixel + 1) = (uint8)(*(((uint16*)blockData) + 1) >> 8);
		*(outputPixel + 2) = (uint8)(*(((uint16*)blockData) + 2) >> 8);
		*(outputPixel + 3) = (uint8)(*(((uint16*)blockData) + 3) >> 8);
	}
};

class TextureDecoder_R16_G16 : public TextureDecoder, public SingletonClass<TextureDecoder_R16_G16>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2 * 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint32, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		*(outputPixel + 0) = (uint8)(*(((uint16*)blockData) + 0) >> 8);
		*(outputPixel + 1) = (uint8)(*(((uint16*)blockData) + 1) >> 8);
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R5_G6_B5 : public TextureDecoder, public SingletonClass<TextureDecoder_R5_G6_B5>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint16, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint16 colorData = (*(uint16*)blockData);
		uint8 red5 = (colorData >> 0) & 0x1F;
		uint8 green6 = (colorData >> 5) & 0x3F;
		uint8 blue5 = (colorData >> 11) & 0x1F;
		*(outputPixel + 0) = (red5 << 3) | (red5 >> 2);
		*(outputPixel + 1) = (green6 << 2) | (green6 >> 4);
		*(outputPixel + 2) = (blue5 << 3) | (blue5 >> 2);
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R5_G6_B5_swappedRB : public TextureDecoder, public SingletonClass<TextureDecoder_R5_G6_B5_swappedRB>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 2;
				uint16 colorData = (*(uint16*)(blockData + 0));

				// colorData >> 0 -> red
				// colorData >> 5 -> green
				// colorData >> 11 -> blue

				colorData = ((colorData >> 11) & 0x1F) | (colorData & (0x3F << 5)) | ((colorData << 11) & (0x1F << 11));

				//// swap order of components
				////uint8 red5 = (colorData>>0)&0x1F;
				////uint8 green5 = (colorData>>5)&0x1F;
				////uint8 blue5 = (colorData>>10)&0x1F;
				////uint8 alpha1 = (colorData>>15)&0x1;
				////colorData = blue5|(green5<<5)|(red5<<10)|(alpha1<<15);
				////*(uint16*)(outputData+pixelOffset+0) = colorData;
				//uint8 red5 = (colorData >> 11) & 0x1F;
				//uint8 green5 = (colorData >> 6) & 0x1F;
				//uint8 blue5 = (colorData >> 1) & 0x1F;
				//uint8 alpha1 = (colorData >> 0) & 0x1;
				////colorData = blue5|(green5<<5)|(red5<<10)|(alpha1<<15);
				////colorData = (blue5<<11)|(green5<<6)|(red5<<1)|(alpha1<<0);
				* (uint16*)(outputData + pixelOffset + 0) = colorData;
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint16 colorData = (*(uint16*)blockData);
		uint8 red5 = (colorData >> 0) & 0x1F;
		uint8 green6 = (colorData >> 5) & 0x3F;
		uint8 blue5 = (colorData >> 11) & 0x1F;
		*(outputPixel + 0) = (red5 << 3) | (red5 >> 2);
		*(outputPixel + 1) = (green6 << 2) | (green6 >> 4);
		*(outputPixel + 2) = (blue5 << 3) | (blue5 >> 2);
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_R5G6B5_UNORM_To_RGBA8 : public TextureDecoder, public SingletonClass<TextureDecoder_R5G6B5_UNORM_To_RGBA8>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 4;
				uint16 v0 = (*(uint16*)(blockData + 0));

				uint8 c0 = (v0 & 0x1F);
				uint8 c1 = (v0 >> 5) & 0x3F;
				uint8 c2 = (v0 >> 11) & 0x1F;

				c0 = (c0 << 3) | c0 >> 3;// blue
				c1 = (c1 << 2) | c1 >> 4;// green
				c2 = (c2 << 3) | c2 >> 3;// red

				*(uint8*)(outputData + pixelOffset + 0) = c0;// blue
				*(uint8*)(outputData + pixelOffset + 1) = c1;// green
				*(uint8*)(outputData + pixelOffset + 2) = c2;// red
				*(uint8*)(outputData + pixelOffset + 3) = 255;//alpha
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint16 v0 = *(uint16*)(blockData + 0);
		uint8 c0 = (v0 & 0x1F);// red
		uint8 c1 = (v0 >> 5) & 0x3F;// green
		uint8 c2 = (v0 >> 11) & 0x1F; // blue
		c0 = (c0 << 3) | c0 >> 3;
		c1 = (c1 << 2) | c1 >> 4;
		c2 = (c2 << 3) | c2 >> 3;
		*(outputPixel + 0) = c0;// red
		*(outputPixel + 1) = c1;// green
		*(outputPixel + 2) = c2;// blue
		*(outputPixel + 3) = 255;// alpha
	}
};

class TextureDecoder_R5_G5_B5_A1_UNORM : public TextureDecoder, public SingletonClass<TextureDecoder_R5_G5_B5_A1_UNORM>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint16, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint16 colorData = (*(uint16*)blockData);
		uint8 red5 = (colorData >> 0) & 0x1F;
		uint8 green5 = (colorData >> 5) & 0x1F;
		uint8 blue5 = (colorData >> 10) & 0x1F;
		uint8 alpha1 = (colorData >> 11) & 0x1;
		*(outputPixel + 0) = (red5 << 3) | (red5 >> 2);
		*(outputPixel + 1) = (green5 << 3) | (green5 >> 2);
		*(outputPixel + 2) = (blue5 << 3) | (blue5 >> 2);
		*(outputPixel + 3) = (alpha1 << 3);
	}
};

class uint16_R5_G5_B5_A1_swapRB
{
public:
	void operator=(const uint16_R5_G5_B5_A1_swapRB& v)
	{
		internalVal = ((v.internalVal >> 10) & (0x1F << 0)) | ((v.internalVal << 10) & (0x1F << 10)) | (v.internalVal & ((0x1F << 5) | 0x8000));
	}

	uint16 internalVal;
};

static_assert(sizeof(uint16_R5_G5_B5_A1_swapRB) == 2);

class TextureDecoder_R5_G5_B5_A1_UNORM_swappedRB : public TextureDecoder, public SingletonClass<TextureDecoder_R5_G5_B5_A1_UNORM_swappedRB>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint16_R5_G5_B5_A1_swapRB, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint16 colorData = (*(uint16*)blockData);
		uint8 red5 = (colorData >> 0) & 0x1F;
		uint8 green5 = (colorData >> 5) & 0x1F;
		uint8 blue5 = (colorData >> 10) & 0x1F;
		uint8 alpha1 = (colorData >> 11) & 0x1;
		*(outputPixel + 0) = (red5 << 3) | (red5 >> 2);
		*(outputPixel + 1) = (green5 << 3) | (green5 >> 2);
		*(outputPixel + 2) = (blue5 << 3) | (blue5 >> 2);
		*(outputPixel + 3) = (alpha1 << 3);
	}
};

class TextureDecoder_R5_G5_B5_A1_UNORM_swappedRB_To_RGBA8 : public TextureDecoder, public SingletonClass<TextureDecoder_R5_G5_B5_A1_UNORM_swappedRB_To_RGBA8>
{
public:
//2656
    sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
    {
        return 4;
    }

    void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
    {
        for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
        {
            sint32 yc = y;
            for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
            {
                uint16* blockData = (uint16*)LatteTextureLoader_GetInput(textureLoader, x, y);
                sint32 pixelOffset = (x + yc * textureLoader->width) * 4;
                uint32 colorData = (*(uint16*)(blockData + 0));
                // swap order of components
                uint8 red = (colorData >> 0) & 0x1F;
                uint8 green = (colorData >> 5) & 0x1F;
                uint8 blue = (colorData >> 10) & 0x1F;
                uint8 alpha = (colorData >> 15) & 0x1;

                red = red << 3 | red >> 2;
                green = green << 3 | green >> 2;
                blue = blue << 3 | blue >> 2;
                alpha = alpha * 0xff;

                // MSB...LSB : ABGR
                colorData = (alpha << 24) | (blue << 16) | (green << 8) | red;
                *(uint32*)(outputData + pixelOffset + 0) = colorData;
            }
        }
    }

    void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
    {
        uint16 colorData = (*(uint16*)blockData);
        uint8 red = (colorData >> 0) & 0x1F;
        uint8 green = (colorData >> 5) & 0x1F;
        uint8 blue = (colorData >> 10) & 0x1F;
        uint8 alpha = (colorData >> 15) & 0x1;
        *(outputPixel + 0) = (red << 3) | (red >> 2);
        *(outputPixel + 1) = (green << 3) | (green >> 2);
        *(outputPixel + 2) = (blue << 3) | (blue >> 2);
        *(outputPixel + 3) = alpha * 0xff;
    }

};

class uint16_R5_G5_B5_A1_swapOpenGL
{
public:
	void operator=(const uint16_R5_G5_B5_A1_swapOpenGL& v)
	{
		uint16 red = (v.internalVal >> 0) & 0x1F;
		uint16 green = (v.internalVal >> 5) & 0x1F;
		uint16 blue = (v.internalVal >> 10) & 0x1F;
		uint16 alpha = (v.internalVal >> 15) & 0x1;
		internalVal = (red << 11) | (green << 6) | (blue << 1) | alpha;
	}
	uint16 internalVal;
};

static_assert(sizeof(uint16_R5_G5_B5_A1_swapOpenGL) == 2);

class TextureDecoder_R5_G5_B5_A1_UNORM_swappedOpenGL : public TextureDecoder, public SingletonClass<TextureDecoder_R5_G5_B5_A1_UNORM_swappedOpenGL>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint16_R5_G5_B5_A1_swapOpenGL, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint16 colorData = (*(uint16*)blockData);
		uint8 red5 = (colorData >> 0) & 0x1F;
		uint8 green5 = (colorData >> 5) & 0x1F;
		uint8 blue5 = (colorData >> 10) & 0x1F;
		uint8 alpha1 = (colorData >> 11) & 0x1;
		*(outputPixel + 0) = (red5 << 3) | (red5 >> 2);
		*(outputPixel + 1) = (green5 << 3) | (green5 >> 2);
		*(outputPixel + 2) = (blue5 << 3) | (blue5 >> 2);
		*(outputPixel + 3) = (alpha1 << 3);
	}
};

class TextureDecoder_A1_B5_G5_R5_UNORM : public TextureDecoder, public SingletonClass<TextureDecoder_A1_B5_G5_R5_UNORM>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint16, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint16 colorData = (*(uint16*)blockData);
		uint8 red5 = (colorData >> 11) & 0x1F;
		uint8 green5 = (colorData >> 6) & 0x1F;
		uint8 blue5 = (colorData >> 1) & 0x1F;
		uint8 alpha1 = (colorData >> 0) & 0x1;
		*(outputPixel + 0) = (red5 << 3) | (red5 >> 2);
		*(outputPixel + 1) = (green5 << 3) | (green5 >> 2);
		*(outputPixel + 2) = (blue5 << 3) | (blue5 >> 2);
		*(outputPixel + 3) = (alpha1 << 3);
	}
};

class TextureDecoder_A1_B5_G5_R5_UNORM_vulkan : public TextureDecoder, public SingletonClass<TextureDecoder_A1_B5_G5_R5_UNORM_vulkan>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 pixelOffset = (x + yc * textureLoader->width) * 2;
				uint16 colorData = (*(uint16*)(blockData + 0));
				// swap order of components
				uint8 red5 = (colorData >> 11) & 0x1F;
				uint8 green5 = (colorData >> 6) & 0x1F;
				uint8 blue5 = (colorData >> 1) & 0x1F;
				uint8 alpha1 = (colorData >> 0) & 0x1;
				colorData = blue5 | (green5 << 5) | (red5 << 10) | (alpha1 << 15);
				*(uint16*)(outputData + pixelOffset + 0) = colorData;
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint16 colorData = (*(uint16*)blockData);
		uint8 red5 = (colorData >> 11) & 0x1F;
		uint8 green5 = (colorData >> 6) & 0x1F;
		uint8 blue5 = (colorData >> 1) & 0x1F;
		uint8 alpha1 = (colorData >> 0) & 0x1;
		*(outputPixel + 0) = (red5 << 3) | (red5 >> 2);
		*(outputPixel + 1) = (green5 << 3) | (green5 >> 2);
		*(outputPixel + 2) = (blue5 << 3) | (blue5 >> 2);
		*(outputPixel + 3) = (alpha1 << 3);
	}
};

class TextureDecoder_A1_B5_G5_R5_UNORM_vulkan_To_RGBA8 : public TextureDecoder, public SingletonClass<TextureDecoder_A1_B5_G5_R5_UNORM_vulkan_To_RGBA8>
{
public:
    sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
    {
        return 4;
    }

    void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
    {
        for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
        {
            sint32 yc = y;
            for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
            {
                uint16* blockData = (uint16*)LatteTextureLoader_GetInput(textureLoader, x, y);
                sint32 pixelOffset = (x + yc * textureLoader->width) * 4;
                uint32 colorData = (*(uint16*)(blockData + 0));
                // swap order of components
                uint8 red = (colorData >> 11) & 0x1F;
                uint8 green = (colorData >> 6) & 0x1F;
                uint8 blue = (colorData >> 1) & 0x1F;
                uint8 alpha = (colorData >> 0) & 0x1;

                red = red << 3 | red >> 2;
                green = green << 3 | green >> 2;
                blue = blue << 3 | blue >> 2;
                alpha = alpha * 0xff;

                // MSB...LSB ABGR
                colorData = red | (green << 8) | (blue << 16) | (alpha << 24);
                *(uint32*)(outputData + pixelOffset + 0) = colorData;
            }
        }
    }

    void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
    {
        uint16 colorData = (*(uint16*)blockData);
        uint8 red5 = (colorData >> 11) & 0x1F;
        uint8 green5 = (colorData >> 6) & 0x1F;
        uint8 blue5 = (colorData >> 1) & 0x1F;
        uint8 alpha1 = (colorData >> 0) & 0x1;
        *(outputPixel + 0) = (red5 << 3) | (red5 >> 2);
        *(outputPixel + 1) = (green5 << 3) | (green5 >> 2);
        *(outputPixel + 2) = (blue5 << 3) | (blue5 >> 2);
        *(outputPixel + 3) = (alpha1 << 3);
    }
};


class TextureDecoder_R10_G10_B10_A2_UNORM : public TextureDecoder, public SingletonClass<TextureDecoder_R10_G10_B10_A2_UNORM>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint32, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint16 r10 = ((*(uint32*)blockData) >> 0) & 0x3FF;
		uint16 g10 = ((*(uint32*)blockData) >> 10) & 0x3FF;
		uint16 b10 = ((*(uint32*)blockData) >> 20) & 0x3FF;
		uint8 a2 = ((*(uint32*)blockData) >> 30) & 0x3;
		*(outputPixel + 0) = (uint8)(r10 >> 6);
		*(outputPixel + 1) = (uint8)(g10 >> 6);
		*(outputPixel + 2) = (uint8)(b10 >> 6);
		*(outputPixel + 3) = a2;
	}
};

class TextureDecoder_R10_G10_B10_A2_SNORM_To_RGBA16 : public TextureDecoder, public SingletonClass<TextureDecoder_R10_G10_B10_A2_SNORM_To_RGBA16>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		// todo - implement
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			sint32 pixelOffset = (yc * textureLoader->width) * (2 * 4);
			sint16* pixelOutput = (sint16*)(outputData + pixelOffset);
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				uint32 v = (*(uint32*)(blockData + 0));
				*pixelOutput = 0;
				pixelOffset++;
				*pixelOutput = 0;
				pixelOffset++;
				*pixelOutput = 0;
				pixelOffset++;
				*pixelOutput = 0;
				pixelOffset++;
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint16 r10 = ((*(uint32*)blockData) >> 0) & 0x3FF;
		uint16 g10 = ((*(uint32*)blockData) >> 10) & 0x3FF;
		uint16 b10 = ((*(uint32*)blockData) >> 20) & 0x3FF;
		uint8 a2 = ((*(uint32*)blockData) >> 30) & 0x3;
		*(outputPixel + 0) = (uint8)(r10 >> 6) / 2 + 128;
		*(outputPixel + 1) = (uint8)(g10 >> 6) / 2 + 128;
		*(outputPixel + 2) = (uint8)(b10 >> 6) / 2 + 128;
		*(outputPixel + 3) = a2 / 2 + 128;
	}
};

class TextureDecoder_A2_B10_G10_R10_UNORM_To_RGBA16 : public TextureDecoder, public SingletonClass<TextureDecoder_A2_B10_G10_R10_UNORM_To_RGBA16>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 2;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		// todo - implement
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			sint32 yc = y;
			sint32 pixelOffset = (yc * textureLoader->width) * (2 * 4);
			sint16* pixelOutput = (sint16*)(outputData + pixelOffset);
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				uint32 v = (*(uint32*)(blockData + 0));
				*pixelOutput = 0;
				pixelOffset++;
				*pixelOutput = 0;
				pixelOffset++;
				*pixelOutput = 0;
				pixelOffset++;
				*pixelOutput = 0;
				pixelOffset++;
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		uint8 a2 = ((*(uint32*)blockData) >> 0) & 0x3;
		uint16 r10 = ((*(uint32*)blockData) >> 2) & 0x3FF;
		uint16 g10 = ((*(uint32*)blockData) >> 12) & 0x3FF;
		uint16 b10 = ((*(uint32*)blockData) >> 22) & 0x3FF;
		*(outputPixel + 0) = (uint8)(r10 >> 6);
		*(outputPixel + 1) = (uint8)(g10 >> 6);
		*(outputPixel + 2) = (uint8)(b10 >> 6);
		*(outputPixel + 3) = a2;
	}
};

class TextureDecoder_R11_G11_B10_FLOAT : public TextureDecoder, public SingletonClass<TextureDecoder_R11_G11_B10_FLOAT>
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint32, 1, false, false>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		// todo: add dumping support
	}
};

class TextureDecoder_BC1_UNORM_uncompress_generic : public TextureDecoder
{
public:
	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 blockSizeX = (std::min)(4, textureLoader->width - x);
				sint32 blockSizeY = (std::min)(4, textureLoader->height - y);
				// decode 4x4 pixels at once
				float rgbaBlock[4 * 4 * 4];
				decodeBC1Block(blockData, rgbaBlock);
				for (sint32 py = 0; py < blockSizeY; py++)
				{
					sint32 yc = y + py;
					for (sint32 px = 0; px < blockSizeX; px++)
					{
						sint32 pixelOffset = (x + px + yc * textureLoader->width) * 16; // write to target buffer
						float red = rgbaBlock[(px + py * 4) * 4 + 0];
						float green = rgbaBlock[(px + py * 4) * 4 + 1];
						float blue = rgbaBlock[(px + py * 4) * 4 + 2];
						float alpha = rgbaBlock[(px + py * 4) * 4 + 3];
						*(float*)(outputData + pixelOffset + 0) = red;
						*(float*)(outputData + pixelOffset + 4) = green;
						*(float*)(outputData + pixelOffset + 8) = blue;
						*(float*)(outputData + pixelOffset + 12) = alpha;
					}
				}
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		BC1_GetPixel(blockData, blockOffsetX, blockOffsetY, outputPixel);
	}
};

class TextureDecoder_BC1_UNORM_uncompress : public TextureDecoder_BC1_UNORM_uncompress_generic, public SingletonClass<TextureDecoder_BC1_UNORM_uncompress>
{
	// reuse TextureDecoder_BC1_UNORM_uncompress_generic
};

class TextureDecoder_BC1_SRGB_uncompress : public TextureDecoder_BC1_UNORM_uncompress_generic, public SingletonClass<TextureDecoder_BC1_SRGB_uncompress>
{
	// reuse TextureDecoder_BC1_UNORM_uncompress_generic
};

class TextureDecoder_BC1 : public TextureDecoder, public SingletonClass<TextureDecoder_BC1>
{
public:

	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 8;
	}

	sint32 getTexelCountX(LatteTextureLoaderCtx* textureLoader) override
	{
		return (textureLoader->width + 3) / 4;
	}

	sint32 getTexelCountY(LatteTextureLoaderCtx* textureLoader) override
	{
		return (textureLoader->height + 3) / 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint64, 1, false, true>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		BC1_GetPixel(blockData, blockOffsetX, blockOffsetY, outputPixel);
	}
};

class TextureDecoder_BC2 : public TextureDecoder, public SingletonClass<TextureDecoder_BC2>
{
public:

	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 4;
	}

	sint32 getTexelCountX(LatteTextureLoaderCtx* textureLoader) override
	{
		return (textureLoader->width + 3) / 4;
	}

	sint32 getTexelCountY(LatteTextureLoaderCtx* textureLoader) override
	{
		return (textureLoader->height + 3) / 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint64, 2, false, true>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		float rgbaBlock[4 * 4 * 4];
		decodeBC2Block_UNORM(blockData, rgbaBlock);
		float red = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 0];
		float green = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 1];
		float blue = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 2];
		float alpha = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 3];
		*(outputPixel + 0) = (uint8)(red * 255.0f);
		*(outputPixel + 1) = (uint8)(green * 255.0f);
		*(outputPixel + 2) = (uint8)(blue * 255.0f);
		*(outputPixel + 3) = (uint8)(alpha * 255.0f);
	}
};

class TextureDecoder_BC2_UNORM_uncompress : public TextureDecoder, public SingletonClass<TextureDecoder_BC2_UNORM_uncompress>
{
public:

	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 blockSizeX = (std::min)(4, textureLoader->width - x);
				sint32 blockSizeY = (std::min)(4, textureLoader->height - y);
				// decode 4x4 pixels at once
				float rgbaBlock[4 * 4 * 4];
				decodeBC2Block_UNORM(blockData, rgbaBlock);
				for (sint32 py = 0; py < blockSizeY; py++)
				{
					sint32 yc = y + py;
					for (sint32 px = 0; px < blockSizeX; px++)
					{
						sint32 pixelOffset = (x + px + yc * textureLoader->width) * 16; // write to target buffer
						float red = rgbaBlock[(px + py * 4) * 4 + 0];
						float green = rgbaBlock[(px + py * 4) * 4 + 1];
						float blue = rgbaBlock[(px + py * 4) * 4 + 2];
						float alpha = rgbaBlock[(px + py * 4) * 4 + 3];
						*(float*)(outputData + pixelOffset + 0) = red;
						*(float*)(outputData + pixelOffset + 4) = green;
						*(float*)(outputData + pixelOffset + 8) = blue;
						*(float*)(outputData + pixelOffset + 12) = alpha;
					}
				}
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		float rgbaBlock[4 * 4 * 4];
		decodeBC2Block_UNORM(blockData, rgbaBlock);
		float red = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 0];
		float green = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 1];
		float blue = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 2];
		float alpha = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 3];
		*(outputPixel + 0) = (uint8)(red * 255.0f);
		*(outputPixel + 1) = (uint8)(green * 255.0f);
		*(outputPixel + 2) = (uint8)(blue * 255.0f);
		*(outputPixel + 3) = (uint8)(alpha * 255.0f);
	}
};

class TextureDecoder_BC2_SRGB_uncompress : public TextureDecoder, public SingletonClass<TextureDecoder_BC2_SRGB_uncompress>
{
public:

	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		// todo - apply srgb conversion
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 blockSizeX = (std::min)(4, textureLoader->width - x);
				sint32 blockSizeY = (std::min)(4, textureLoader->height - y);
				// decode 4x4 pixels at once
				float rgbaBlock[4 * 4 * 4];
				decodeBC2Block_UNORM(blockData, rgbaBlock);
				for (sint32 py = 0; py < blockSizeY; py++)
				{
					sint32 yc = y + py;
					for (sint32 px = 0; px < blockSizeX; px++)
					{
						sint32 pixelOffset = (x + px + yc * textureLoader->width) * 16; // write to target buffer
						float red = rgbaBlock[(px + py * 4) * 4 + 0];
						float green = rgbaBlock[(px + py * 4) * 4 + 1];
						float blue = rgbaBlock[(px + py * 4) * 4 + 2];
						float alpha = rgbaBlock[(px + py * 4) * 4 + 3];
						*(float*)(outputData + pixelOffset + 0) = red;
						*(float*)(outputData + pixelOffset + 4) = green;
						*(float*)(outputData + pixelOffset + 8) = blue;
						*(float*)(outputData + pixelOffset + 12) = alpha;
					}
				}
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		// todo - apply srgb conversion
		float rgbaBlock[4 * 4 * 4];
		decodeBC2Block_UNORM(blockData, rgbaBlock);
		float red = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 0];
		float green = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 1];
		float blue = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 2];
		float alpha = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 3];
		*(outputPixel + 0) = (uint8)(red * 255.0f);
		*(outputPixel + 1) = (uint8)(green * 255.0f);
		*(outputPixel + 2) = (uint8)(blue * 255.0f);
		*(outputPixel + 3) = (uint8)(alpha * 255.0f);
	}
};

class TextureDecoder_BC3_uncompress_generic : public TextureDecoder
{
public:

	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 4 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 blockSizeX = (std::min)(4, textureLoader->width - x);
				sint32 blockSizeY = (std::min)(4, textureLoader->height - y);
				// decode 4x4 pixels at once
				float rgbaBlock[4 * 4 * 4];
				decodeBC3Block_UNORM(blockData, rgbaBlock);
				for (sint32 py = 0; py < blockSizeY; py++)
				{
					sint32 yc = y + py;
					for (sint32 px = 0; px < blockSizeX; px++)
					{
						sint32 pixelOffset = (x + px + yc * textureLoader->width) * 16; // write to target buffer
						float red = rgbaBlock[(px + py * 4) * 4 + 0];
						float green = rgbaBlock[(px + py * 4) * 4 + 1];
						float blue = rgbaBlock[(px + py * 4) * 4 + 2];
						float alpha = rgbaBlock[(px + py * 4) * 4 + 3];
						*(float*)(outputData + pixelOffset + 0) = red;
						*(float*)(outputData + pixelOffset + 4) = green;
						*(float*)(outputData + pixelOffset + 8) = blue;
						*(float*)(outputData + pixelOffset + 12) = alpha;
					}
				}
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		float rgbaBlock[4 * 4 * 4];
		decodeBC3Block_UNORM(blockData, rgbaBlock);
		float red = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 0];
		float green = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 1];
		float blue = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 2];
		float alpha = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 3];
		*(outputPixel + 0) = (uint8)(red * 255.0f);
		*(outputPixel + 1) = (uint8)(green * 255.0f);
		*(outputPixel + 2) = (uint8)(blue * 255.0f);
		*(outputPixel + 3) = (uint8)(alpha * 255.0f);
	}
};

class TextureDecoder_BC3_UNORM_uncompress : public TextureDecoder_BC3_uncompress_generic, public SingletonClass<TextureDecoder_BC3_UNORM_uncompress>
{
	// reuse TextureDecoder_BC3_uncompress_generic
};

class TextureDecoder_BC3_SRGB_uncompress : public TextureDecoder_BC3_uncompress_generic, public SingletonClass<TextureDecoder_BC3_SRGB_uncompress>
{
	// reuse TextureDecoder_BC3_uncompress_generic
};

class TextureDecoder_BC3 : public TextureDecoder, public SingletonClass<TextureDecoder_BC3>
{
public:

	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 16;
	}

	sint32 getTexelCountX(LatteTextureLoaderCtx* textureLoader) override
	{
		return (textureLoader->width + 3) / 4;
	}

	sint32 getTexelCountY(LatteTextureLoaderCtx* textureLoader) override
	{
		return (textureLoader->height + 3) / 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint64, 2, false, true>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		float rgbaBlock[4 * 4 * 4];
		decodeBC3Block_UNORM(blockData, rgbaBlock);
		float red = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 0];
		float green = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 1];
		float blue = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 2];
		float alpha = rgbaBlock[(blockOffsetX + blockOffsetY * 4) * 4 + 3];
		*(outputPixel + 0) = (uint8)(red * 255.0f);
		*(outputPixel + 1) = (uint8)(green * 255.0f);
		*(outputPixel + 2) = (uint8)(blue * 255.0f);
		*(outputPixel + 3) = (uint8)(alpha * 255.0f);
	}
};

class TextureDecoder_BC4_UNORM_uncompress : public TextureDecoder, public SingletonClass<TextureDecoder_BC4_UNORM_uncompress>
{
public:

	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 blockSizeX = (std::min)(4, textureLoader->width - x);
				sint32 blockSizeY = (std::min)(4, textureLoader->height - y);
				// decode 4x4 pixels at once
				float rBlock[4 * 4 * 1];
				decodeBC4Block_UNORM(blockData, rBlock);

				for (sint32 py = 0; py < blockSizeY; py++)
				{
					sint32 yc = y + py;
					for (sint32 px = 0; px < blockSizeX; px++)
					{
						sint32 pixelOffset = (x + px + yc * textureLoader->width) * 8; // write to target buffer
						float red = rBlock[(px + py * 4) * 1 + 0];
						*(float*)(outputData + pixelOffset + 0) = red;
						*(float*)(outputData + pixelOffset + 4) = 0.0f;
					}
				}
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		float rBlock[4 * 4 * 1];
		decodeBC4Block_UNORM(blockData, rBlock);
		float red = rBlock[(blockOffsetX + blockOffsetY * 4) * 1 + 0];
		*(outputPixel + 0) = (uint8)(red * 255.0f);
		*(outputPixel + 1) = 0;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_BC4 : public TextureDecoder, public SingletonClass<TextureDecoder_BC4>
{
public:

	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 8;
	}

	sint32 getTexelCountX(LatteTextureLoaderCtx* textureLoader) override
	{
		return (textureLoader->width + 3) / 4;
	}

	sint32 getTexelCountY(LatteTextureLoaderCtx* textureLoader) override
	{
		return (textureLoader->height + 3) / 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint64, 1, false, true>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		float rBlock[4 * 4 * 1];
		decodeBC4Block_UNORM(blockData, rBlock);
		float red = rBlock[(blockOffsetX + blockOffsetY * 4) * 1 + 0];
		*(outputPixel + 0) = (uint8)(red * 255.0f);
		*(outputPixel + 1) = 0;
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_BC5_UNORM_uncompress : public TextureDecoder, public SingletonClass<TextureDecoder_BC5_UNORM_uncompress>
{
public:

	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 blockSizeX = (std::min)(4, textureLoader->width - x);
				sint32 blockSizeY = (std::min)(4, textureLoader->height - y);
				// decode 4x4 pixels at once
				float rgBlock[4 * 4 * 2];
				decodeBC5Block_UNORM(blockData, rgBlock);

				for (sint32 py = 0; py < blockSizeY; py++)
				{
					sint32 yc = y + py;
					for (sint32 px = 0; px < blockSizeX; px++)
					{
						sint32 pixelOffset = (x + px + yc * textureLoader->width) * 8; // write to target buffer
						float red = rgBlock[(px + py * 4) * 2 + 0];
						float green = rgBlock[(px + py * 4) * 2 + 1];
						*(float*)(outputData + pixelOffset + 0) = red;
						*(float*)(outputData + pixelOffset + 4) = green;
					}
				}
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		float rgBlock[4 * 4 * 2];
		decodeBC5Block_UNORM(blockData, rgBlock);
		float red = rgBlock[(blockOffsetX + blockOffsetY * 4) * 2 + 0];
		float green = rgBlock[(blockOffsetX + blockOffsetY * 4) * 2 + 1];
		*(outputPixel + 0) = (uint8)(red * 255.0f);
		*(outputPixel + 1) = (uint8)(green * 255.0f);
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_BC5_SNORM_uncompress : public TextureDecoder, public SingletonClass<TextureDecoder_BC5_SNORM_uncompress>
{
public:

	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 2 * 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		for (sint32 y = 0; y < textureLoader->height; y += textureLoader->stepY)
		{
			for (sint32 x = 0; x < textureLoader->width; x += textureLoader->stepX)
			{
				uint8* blockData = LatteTextureLoader_GetInput(textureLoader, x, y);
				sint32 blockSizeX = (std::min)(4, textureLoader->width - x);
				sint32 blockSizeY = (std::min)(4, textureLoader->height - y);
				// decode 4x4 pixels at once
				float rgBlock[4 * 4 * 2];
				decodeBC5Block_SNORM(blockData, rgBlock);

				for (sint32 py = 0; py < blockSizeY; py++)
				{
					sint32 yc = y + py;
					for (sint32 px = 0; px < blockSizeX; px++)
					{
						sint32 pixelOffset = (x + px + yc * textureLoader->width) * 8; // write to target buffer
						float red = rgBlock[(px + py * 4) * 2 + 0];
						float green = rgBlock[(px + py * 4) * 2 + 1];
						*(float*)(outputData + pixelOffset + 0) = red;
						*(float*)(outputData + pixelOffset + 4) = green;
					}
				}
			}
		}
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		// todo: fix BC5 SNORM dumping
		float rgBlock[4 * 4 * 2];
		decodeBC5Block_SNORM(blockData, rgBlock);
		float red = rgBlock[(blockOffsetX + blockOffsetY * 4) * 2 + 0];
		float green = rgBlock[(blockOffsetX + blockOffsetY * 4) * 2 + 1];
		*(outputPixel + 0) = (uint8)((0.5f + red * 0.5f) * 255.0f);
		*(outputPixel + 1) = (uint8)((0.5f + green * 0.5f) * 255.0f);
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};

class TextureDecoder_BC5 : public TextureDecoder, public SingletonClass<TextureDecoder_BC5>
{
public:

	sint32 getBytesPerTexel(LatteTextureLoaderCtx* textureLoader) override
	{
		return 16;
	}

	sint32 getTexelCountX(LatteTextureLoaderCtx* textureLoader) override
	{
		return (textureLoader->width + 3) / 4;
	}

	sint32 getTexelCountY(LatteTextureLoaderCtx* textureLoader) override
	{
		return (textureLoader->height + 3) / 4;
	}

	void decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData) override
	{
		optimizedDecodeLoops<uint64, 2, false, true>(textureLoader, outputData);
	}

	void decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY) override
	{
		float rgBlock[4 * 4 * 2];
		decodeBC5Block_UNORM(blockData, rgBlock);
		float red = rgBlock[(blockOffsetX + blockOffsetY * 4) * 2 + 0];
		float green = rgBlock[(blockOffsetX + blockOffsetY * 4) * 2 + 1];
		*(outputPixel + 0) = (uint8)(red * 255.0f);
		*(outputPixel + 1) = (uint8)(green * 255.0f);
		*(outputPixel + 2) = 0;
		*(outputPixel + 3) = 255;
	}
};