#pragma once
#include <cstdint>

void Rgb2Nv12(const uint8_t* rgbImage,
			  unsigned imageWidth,
			  unsigned imageHeight,
			  uint8_t* outNv12Image,
			  unsigned nv12Pitch);