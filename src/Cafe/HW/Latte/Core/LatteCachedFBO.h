#pragma once

#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "util/math/vector2.h"

class LatteCachedFBO
{
public:
	LatteCachedFBO(uint64 key);

	uint32 calculateNumColorBuffers()
	{
		uint32 n = 0;
		for (sint32 i = 0; i < 8; i++)
			if (colorBuffer[i].texture)
				n++;
		return n;
	}

	bool hasDepthBuffer()
	{
		return depthBuffer.texture;
	}

	std::vector<LatteTexture*>& GetTextures()
	{
		return m_referencedTextures;
	}

	virtual ~LatteCachedFBO() {};

private:
	void calculateEffectiveRenderAreaSize()
	{
		Vector2i rtEffectiveSize;
		rtEffectiveSize.x = 0;
		rtEffectiveSize.y = 0;
		sint32 numViews = 0;
		// derive extent from color buffers
		for (sint32 i = 0; i < 8; i++)
		{
			if(colorBuffer[i].texture == nullptr)
				continue;
			sint32 effectiveWidth, effectiveHeight;
			LatteTexture_getEffectiveSize(colorBuffer[i].texture->baseTexture, &effectiveWidth, &effectiveHeight, nullptr, colorBuffer[i].texture->firstMip);
			if (rtEffectiveSize.x == 0 && rtEffectiveSize.y == 0)
			{
				rtEffectiveSize.x = effectiveWidth;
				rtEffectiveSize.y = effectiveHeight;
			}
			if (effectiveWidth < rtEffectiveSize.x)
			{
				forceLogDebug_printf("Framebuffer has color texture with smaller effective width (%d -> %d)", rtEffectiveSize.x, effectiveWidth);
				rtEffectiveSize.x = effectiveWidth;
			}
			if (effectiveHeight < rtEffectiveSize.y)
			{
				forceLogDebug_printf("Framebuffer has color texture with smaller effective height (%d -> %d)", rtEffectiveSize.y, effectiveHeight);
				rtEffectiveSize.y = effectiveHeight;
			}
			numViews++;
		}
		// derive extent from depth buffer
		if (depthBuffer.texture)
		{
			sint32 effectiveWidth, effectiveHeight;
			LatteTexture_getEffectiveSize(depthBuffer.texture->baseTexture, &effectiveWidth, &effectiveHeight, nullptr, depthBuffer.texture->firstMip);
			if (rtEffectiveSize.x == 0 && rtEffectiveSize.y == 0)
			{
				rtEffectiveSize.x = effectiveWidth;
				rtEffectiveSize.y = effectiveHeight;
			}
			if (effectiveWidth < rtEffectiveSize.x)
			{
				forceLogDebug_printf("Framebuffer has depth texture with smaller effective width (%d -> %d)", rtEffectiveSize.x, effectiveWidth);
				rtEffectiveSize.x = effectiveWidth;
			}
			if (effectiveHeight < rtEffectiveSize.y)
			{
				forceLogDebug_printf("Framebuffer has depth texture with smaller effective height (%d -> %d)", rtEffectiveSize.y, effectiveHeight);
				rtEffectiveSize.y = effectiveHeight;
			}
			numViews++;
		}
		if (numViews == 0)
		{
			// empty FBO
			m_size = Vector2i(32, 32);
			return;
		}
		cemu_assert_debug(rtEffectiveSize.x != 0);
		cemu_assert_debug(rtEffectiveSize.y != 0);
		m_size = rtEffectiveSize;
	}

public:
	uint64 key;
	Vector2i m_size;

	struct
	{
		LatteTextureView* texture{};
	}colorBuffer[8]{};
	struct
	{
		LatteTextureView* texture{};
		bool hasStencil{};
	}depthBuffer{};
	uint32 drawBuffersMask{};

	std::vector<LatteTexture*> m_referencedTextures; // color and depth views combined
};

class LatteMRT
{
public:
	static void NotifyTextureDeletion(LatteTexture* texture);

	// GPU state
	static LatteTextureView* GetColorAttachmentTexture(uint32 index, bool createNew, bool checkForTextureChanges);
	static uint8 GetActiveColorBufferMask(const LatteDecompilerShader* pixelShader, const struct LatteContextRegister& lcr);
	static bool GetActiveDepthBufferMask(const struct LatteContextRegister& lcr);
	static Latte::E_GX2SURFFMT GetColorBufferFormat(const uint32 index, const LatteContextRegister& lcr);
	static Latte::E_GX2SURFFMT GetDepthBufferFormat(const struct LatteContextRegister& lcr);

	// FBO state management
	static void SetColorAttachment(uint32 index, LatteTextureView* view);
	static void SetDepthAndStencilAttachment(LatteTextureView* view, bool hasStencil);
	static LatteTextureView* GetColorAttachment(uint32 index);
	static LatteTextureView* GetDepthAttachment();

	static void ApplyCurrentState();

	static bool UpdateCurrentFBO(); // update FBO with info from current GPU state

	// helper functions
	static void BindColorBufferOnly(LatteTextureView* view);
	static void BindDepthBufferOnly(LatteTextureView* view);

	static void GetCurrentFragCoordScale(float* coordScale);
	static void GetVirtualViewportDimensions(sint32& width, sint32& height); // returns the width and height of the current GPU viewport (unaffected by graphic pack rules)

	// todo - move this into FBO destructor (?)
	static void DeleteCachedFBO(LatteCachedFBO* cfbo);

private:
	static LatteCachedFBO* CreateCachedFBO(uint64 key);
};