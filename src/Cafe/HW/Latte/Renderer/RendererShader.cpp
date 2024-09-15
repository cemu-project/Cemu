#include "Cafe/HW/Latte/Renderer/RendererShader.h"
#include "Cafe/GameProfile/GameProfile.h"

// generate a Cemu version and setting dependent id
uint32 RendererShader::GeneratePrecompiledCacheId()
{
	uint32 v = 0;
	const char* s = EMULATOR_VERSION_SUFFIX;
	while (*s)
	{
		v = std::rotl<uint32>(v, 7);
		v += (uint32)(*s);
		s++;
	}
	v += (EMULATOR_VERSION_MAJOR * 1000000u);
	v += (EMULATOR_VERSION_MINOR * 10000u);
	v += (EMULATOR_VERSION_PATCH * 100u);

	// settings that can influence shaders
	v += (uint32)g_current_game_profile->GetAccurateShaderMul() * 133;

	return v;
}

void RendererShader::GenerateShaderPrecompiledCacheFilename(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, uint64& h1, uint64& h2)
{
	h1 = baseHash;
	h2 = auxHash;

	if (type == RendererShader::ShaderType::kVertex)
		h2 += 0xA16374cull;
	else if (type == RendererShader::ShaderType::kFragment)
		h2 += 0x8752deull;
	else if (type == RendererShader::ShaderType::kGeometry)
		h2 += 0x65a035ull;
}