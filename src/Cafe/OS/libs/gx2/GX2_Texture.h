#pragma once
#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "GX2_Surface.h"

namespace GX2
{
	struct GX2Texture
	{
		/* +0x00 */ GX2Surface surface;
		/* +0x74 */ uint32be viewFirstMip;
		/* +0x78 */ uint32be viewNumMips;
		/* +0x7C */ uint32be viewFirstSlice;
		/* +0x80 */ uint32be viewNumSlices;
		/* +0x84 */ uint32be compSel;
		/* +0x88 */ betype<Latte::LATTE_SQ_TEX_RESOURCE_WORD0_N> regTexWord0;
		/* +0x8C */ betype<Latte::LATTE_SQ_TEX_RESOURCE_WORD1_N> regTexWord1;
		// word2 and word3 are the base/mip address and are not stored
		/* +0x90 */ betype<Latte::LATTE_SQ_TEX_RESOURCE_WORD4_N> regTexWord4;
		/* +0x94 */ betype<Latte::LATTE_SQ_TEX_RESOURCE_WORD5_N> regTexWord5;
		/* +0x98 */ betype<Latte::LATTE_SQ_TEX_RESOURCE_WORD6_N> regTexWord6;
	};

	static_assert(sizeof(GX2Texture) == 0x9C);

	struct GX2Sampler
	{
		betype<Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0> word0;
		betype<Latte::LATTE_SQ_TEX_SAMPLER_WORD1_0> word1;
		betype<Latte::LATTE_SQ_TEX_SAMPLER_WORD2_0> word2;
	};

	static_assert(sizeof(GX2Sampler) == 12);

	void GX2InitTextureRegs(GX2Texture* texture);

	void GX2TextureInit();
};