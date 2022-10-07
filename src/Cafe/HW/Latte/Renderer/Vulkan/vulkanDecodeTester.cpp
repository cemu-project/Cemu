#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "config/CemuConfig.h"

//2668
sint32 TextureDecoder_A1_B5_G5_R5_UNORM_vulkan_To_RGBA8::getBytesPerTexel(LatteTextureLoaderCtx* textureLoader)
{
	return 4;
}

void TextureDecoder_A1_B5_G5_R5_UNORM_vulkan_To_RGBA8::decode(LatteTextureLoaderCtx* textureLoader, uint8* outputData)
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

            colorData = blue | (green << 8) | (red << 16) | (alpha << 24);
			*(uint32*)(outputData + pixelOffset + 0) = colorData;
		}
	}
}

void TextureDecoder_A1_B5_G5_R5_UNORM_vulkan_To_RGBA8::decodePixelToRGBA(uint8* blockData, uint8* outputPixel, uint8 blockOffsetX, uint8 blockOffsetY)
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