#include "Image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_TGA

#include <stb_image.h>

Image::Image(Image&& image)
{
	this->colors = image.colors;
	this->width = image.width;
	this->height = image.height;
	this->channels = image.channels;
	image.colors = nullptr;
}

Image::Image(const std::vector<uint8>& imageBytes)
{
	stbi_uc* stbImage = stbi_load_from_memory(imageBytes.data(), imageBytes.size(), &width, &height, &channels, STBI_rgb_alpha);
	if (!stbImage)
		return;
	for (size_t i = 0; i < width * height * 4; i += 4)
	{
		// RGBA -> BGRA
		std::swap(stbImage[i + 0], stbImage[i + 2]);
	}
	colors = reinterpret_cast<sint32*>(stbImage);
}

bool Image::IsOk() const
{
	return colors != nullptr;
}

Image::~Image()
{
	if (colors)
		stbi_image_free(colors);
}
