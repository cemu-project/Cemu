#include "Cafe/HW/Latte/Renderer/Metal/MetalAttachmentsInfo.h"
#include "Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"

MetalAttachmentsInfo::MetalAttachmentsInfo(class CachedFBOMtl* fbo)
{
    for (uint8 i = 0; i < LATTE_NUM_COLOR_TARGET; i++)
	{
	    const auto& colorBuffer = fbo->colorBuffer[i];
		auto texture = static_cast<LatteTextureViewMtl*>(colorBuffer.texture);
		if (!texture)
		    continue;

		colorFormats[i] = texture->format;
	}

	// Depth stencil attachment
	if (fbo->depthBuffer.texture)
	{
	    auto texture = static_cast<LatteTextureViewMtl*>(fbo->depthBuffer.texture);
        depthFormat = texture->format;
        hasStencil = fbo->depthBuffer.hasStencil;
	}
}

MetalAttachmentsInfo::MetalAttachmentsInfo(const LatteContextRegister& lcr, const LatteDecompilerShader* pixelShader)
{
    uint8 cbMask = LatteMRT::GetActiveColorBufferMask(pixelShader, lcr);
	bool dbMask = LatteMRT::GetActiveDepthBufferMask(lcr);

	// Color attachments
	for (int i = 0; i < 8; ++i)
	{
		if ((cbMask & (1 << i)) == 0)
			continue;

		colorFormats[i] = LatteMRT::GetColorBufferFormat(i, lcr);
	}

	// Depth stencil attachment
	if (dbMask)
	{
		Latte::E_GX2SURFFMT format = LatteMRT::GetDepthBufferFormat(lcr);
		depthFormat = format;
		hasStencil = GetMtlPixelFormatInfo(format, true).hasStencil;
	}
}
