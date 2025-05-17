#pragma once

void Rgb2Nv12(const uint8* rgbImage,
			  unsigned imageWidth,
			  unsigned imageHeight,
			  uint8* outNv12Image,
			  unsigned nv12Pitch);