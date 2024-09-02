// Based on https://github.com/cohenrotem/Rgb2NV12
#include "Rgb2Nv12.h"

constexpr static glm::mat3x3 coefficientMatrix =
	{
		+0.257f, -0.148f, -0.439f,
		-0.504f, -0.291f, -0.368f,
		+0.098f, +0.439f, -0.071f};

constexpr static glm::mat4x3 offsetMatrix = {
	16.0f + 0.5f, 128.0f + 2.0f, 128.0f + 2.0f,
	16.0f + 0.5f, 128.0f + 2.0f, 128.0f + 2.0f,
	16.0f + 0.5f, 128.0f + 2.0f, 128.0f + 2.0f,
	16.0f + 0.5f, 128.0f + 2.0f, 128.0f + 2.0f};

static void Rgb2Nv12TwoRows(const uint8* topLine,
							const uint8* bottomLine,
							unsigned imageWidth,
							uint8* topLineY,
							uint8* bottomLineY,
							uint8* uv)
{
	auto* topIn = reinterpret_cast<const glm::u8vec3*>(topLine);
	auto* botIn = reinterpret_cast<const glm::u8vec3*>(bottomLine);

	for (auto x = 0u; x < imageWidth; x += 2)
	{
		const glm::mat4x3 rgbMatrix{
			topIn[x],
			topIn[x + 1],
			botIn[x],
			botIn[x + 1],
		};

		const auto result = coefficientMatrix * rgbMatrix + offsetMatrix;

		topLineY[x + 0] = result[0].s;
		topLineY[x + 1] = result[1].s;
		bottomLineY[x + 0] = result[2].s;
		bottomLineY[x + 1] = result[3].s;

		uv[x + 0] = (result[0].t + result[1].t + result[2].t + result[3].t) * 0.25f;
		uv[x + 1] = (result[0].p + result[1].p + result[2].p + result[3].p) * 0.25f;
	}
}

void Rgb2Nv12(const uint8* rgbImage,
			  unsigned imageWidth,
			  unsigned imageHeight,
			  uint8* outNv12Image,
			  unsigned nv12Pitch)
{
	cemu_assert_debug(!((imageWidth | imageHeight) & 1));
	unsigned char* UV = outNv12Image + imageWidth * imageHeight;

	for (auto row = 0u; row < imageHeight; row += 2)
	{
		Rgb2Nv12TwoRows(&rgbImage[row * imageWidth * 3],
						&rgbImage[(row + 1) * imageWidth * 3],
						imageWidth,
						&outNv12Image[row * nv12Pitch],
						&outNv12Image[(row + 1) * nv12Pitch],
						&UV[(row / 2) * nv12Pitch]);
	}
}
