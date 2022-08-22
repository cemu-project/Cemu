#pragma once
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "util/containers/SmallBitset.h"

namespace Latte
{
	class ShaderDescription
	{
	public:
		bool analyzeShaderCode(void* shaderProgram, size_t sizeInBytes, LatteConst::ShaderType shaderType);

		// public data

		// pixel shader only
		SmallBitset<8> renderTargetWriteMask; // true if render target is written

	};
};