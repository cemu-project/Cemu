#include "Cafe/HW/Latte/ISA/RegDefines.h"

#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"
#include "Cafe/HW/Latte/Core/LatteBufferCache.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "Cafe/HW/Latte/Core/LatteCachedFBO.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Cafe/GraphicPack/GraphicPack2.h"
#include "config/ActiveSettings.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "gui/guiWrapper.h"
#include "Cafe/OS/libs/erreula/erreula.h"
#include "input/InputManager.h"
#include "Cafe/OS/libs/swkbd/swkbd.h"

uint32 prevScissorX = 0;
uint32 prevScissorY = 0;
uint32 prevScissorWidth = 0;
uint32 prevScissorHeight = 0;

bool hasValidFramebufferAttached = false;

struct LatteMRTQuad
{
	sint32 width;
	sint32 height;
};

struct
{
	LatteMRTQuad currentRenderSize;
	LatteMRTQuad currentEffectiveSize;
	struct
	{
		sint32 width;
		sint32 height;
	}currentGuestViewport;
	bool renderTargetIsResized;
	// tracking
	sint32 rtUpdateListCount;
	LatteTextureView* rtUpdateList[64];
	sint32 rtUpdateListSlice[64];
	sint32 rtUpdateListMip[64];
}sLatteRenderTargetState;

struct
{
	struct
	{
		LatteTextureView* view{};
	}colorBuffer[8];
	struct
	{
		LatteTextureView* view{};
		bool hasStencil{false};
	}depthBuffer;
}sLatteCurrentRendertargets{};

LatteCachedFBO::LatteCachedFBO(uint64 key) : key(key)
{
	for (sint32 i = 0; i < 8; i++)
	{
		LatteTextureView* colorTexView = sLatteCurrentRendertargets.colorBuffer[i].view;
		colorBuffer[i].texture = colorTexView;
		if (colorTexView)
		{
			vectorAppendUnique(colorTexView->list_associatedFbo, this);
			m_referencedTextures.emplace_back(colorTexView->baseTexture);
		}
	}

	if (sLatteCurrentRendertargets.depthBuffer.view)
	{
		LatteTextureView* depthTexView = sLatteCurrentRendertargets.depthBuffer.view;
		depthBuffer.texture = depthTexView;
		depthBuffer.hasStencil = sLatteCurrentRendertargets.depthBuffer.hasStencil;
		if (depthTexView)
		{
			vectorAppendUnique(depthTexView->list_associatedFbo, this);
			m_referencedTextures.emplace_back(depthTexView->baseTexture);
		}
	}

	calculateEffectiveRenderAreaSize();
}

void LatteMRT::NotifyTextureDeletion(LatteTexture* texture)
{
	for (sint32 i = 0; i < Latte::GPU_LIMITS::NUM_COLOR_ATTACHMENTS; i++)
	{
		if (sLatteCurrentRendertargets.colorBuffer[i].view && sLatteCurrentRendertargets.colorBuffer[i].view->baseTexture == texture)
		{
			sLatteCurrentRendertargets.colorBuffer[i].view = nullptr;
		}
	}
}

LatteCachedFBO* LatteMRT::CreateCachedFBO(uint64 key)
{
	return g_renderer->rendertarget_createCachedFBO(key);
}

void LatteMRT::DeleteCachedFBO(LatteCachedFBO* cfbo)
{
	// color textures
	for (sint32 i = 0; i < 8; i++)
	{
		if (cfbo->colorBuffer[i].texture == NULL)
			continue;
		cfbo->colorBuffer[i].texture->list_fboLookup.erase(std::remove(cfbo->colorBuffer[i].texture->list_fboLookup.begin(), cfbo->colorBuffer[i].texture->list_fboLookup.end(), cfbo), cfbo->colorBuffer[i].texture->list_fboLookup.end());
		cfbo->colorBuffer[i].texture->list_associatedFbo.erase(std::remove(cfbo->colorBuffer[i].texture->list_associatedFbo.begin(), cfbo->colorBuffer[i].texture->list_associatedFbo.end(), cfbo), cfbo->colorBuffer[i].texture->list_associatedFbo.end());
	}
	// depth texture
	if (cfbo->depthBuffer.texture)
	{
		cfbo->depthBuffer.texture->list_fboLookup.erase(std::remove(cfbo->depthBuffer.texture->list_fboLookup.begin(), cfbo->depthBuffer.texture->list_fboLookup.end(), cfbo), cfbo->depthBuffer.texture->list_fboLookup.end());
		cfbo->depthBuffer.texture->list_associatedFbo.erase(std::remove(cfbo->depthBuffer.texture->list_associatedFbo.begin(), cfbo->depthBuffer.texture->list_associatedFbo.end(), cfbo), cfbo->depthBuffer.texture->list_associatedFbo.end());
	}
	g_renderer->rendertarget_deleteCachedFBO(cfbo);
	delete cfbo;
}

LatteCachedFBO* g_emptyFBO = nullptr;

void LatteMRT::SetColorAttachment(uint32 index, LatteTextureView* view)
{
	sLatteCurrentRendertargets.colorBuffer[index].view = view;
}

void LatteMRT::SetDepthAndStencilAttachment(LatteTextureView* view, bool hasStencil)
{
	sLatteCurrentRendertargets.depthBuffer.view = view;
	sLatteCurrentRendertargets.depthBuffer.hasStencil = hasStencil;
}

LatteTextureView* LatteMRT::GetColorAttachment(uint32 index)
{
	cemu_assert_debug(index < 8);
	return sLatteCurrentRendertargets.colorBuffer[index].view;
}

LatteTextureView* LatteMRT::GetDepthAttachment()
{
	return sLatteCurrentRendertargets.depthBuffer.view;
}

void LatteMRT::ApplyCurrentState()
{
	uint64 key = 0;
	LatteTextureView* fboLookupView = NULL;
	for (sint32 i = 0; i < 8; i++)
	{
		LatteTextureView* colorView = sLatteCurrentRendertargets.colorBuffer[i].view;
		if (colorView)
		{
			key += ((uint64)colorView);
			key = std::rotl<uint64>(key, 5);
			fboLookupView = colorView;
		}
		key = std::rotl<uint64>(key, 7);
	}
	if (sLatteCurrentRendertargets.depthBuffer.view)
	{
		key += ((uint64)sLatteCurrentRendertargets.depthBuffer.view);
		key = std::rotl<uint64>(key, 5);
		key += (sLatteCurrentRendertargets.depthBuffer.hasStencil);
		if (fboLookupView == NULL)
		{
			fboLookupView = sLatteCurrentRendertargets.depthBuffer.view;
		}
	}
	// use fboLookupTexture to find cached FBO
	if (fboLookupView == nullptr)
	{
		if (!g_emptyFBO)
			g_emptyFBO = CreateCachedFBO(key);
		g_renderer->rendertarget_bindFramebufferObject(g_emptyFBO);
		return;
	}
	// look for FBO
	for (auto& fbo : fboLookupView->list_fboLookup)
	{
		if (fbo->key == key)
		{
			// found matching FBO
			g_renderer->rendertarget_bindFramebufferObject(fbo);
			return;
		}
	}
	// create new cached FBO
	LatteCachedFBO* cfbo = CreateCachedFBO(key);
	g_renderer->rendertarget_bindFramebufferObject(cfbo);
	fboLookupView->list_fboLookup.push_back(cfbo);
	// some extra checks to verify that looked up fbo matches active buffers
	cemu_assert_debug(cfbo->colorBuffer[0].texture == sLatteCurrentRendertargets.colorBuffer[0].view);
	cemu_assert_debug(cfbo->colorBuffer[1].texture == sLatteCurrentRendertargets.colorBuffer[1].view);
	cemu_assert_debug(cfbo->colorBuffer[2].texture == sLatteCurrentRendertargets.colorBuffer[2].view);
	cemu_assert_debug(cfbo->depthBuffer.texture == sLatteCurrentRendertargets.depthBuffer.view);
}

void LatteMRT::BindColorBufferOnly(LatteTextureView* view)
{
	cemu_assert_debug(!view->baseTexture->isDepth);
	SetColorAttachment(0, view);
	for (sint32 i = 1; i < 8; i++)
		SetColorAttachment(i, nullptr);
	SetDepthAndStencilAttachment(nullptr, false);
	ApplyCurrentState();
}

void LatteMRT::BindDepthBufferOnly(LatteTextureView* view)
{
	cemu_assert_debug(view->baseTexture->isDepth);
	for (sint32 i = 0; i < 8; i++)
		SetColorAttachment(i, nullptr);
	SetDepthAndStencilAttachment(view, view->baseTexture->hasStencil);
	ApplyCurrentState();
}

/***************************************************/

LatteTextureView* LatteMRT_FindColorBufferForClearing(MPTR colorBufferPtr, sint32 colorBufferWidth, sint32 colorBufferHeight, sint32 colorBufferPitch, uint32 format, sint32 sliceIndex, sint32* searchIndex)
{
	LatteTextureView* view = LatteTC_LookupTextureByData(colorBufferPtr, colorBufferWidth, colorBufferHeight, colorBufferPitch, 0, 1, sliceIndex, 1, searchIndex);
	if (view == nullptr)
		return nullptr;
	return view;
}

LatteTextureView* LatteMRT_CreateColorBuffer(MPTR colorBufferPhysMem, uint32 width, uint32 height, uint32 pitch, Latte::E_GX2SURFFMT format, Latte::E_HWTILEMODE tileMode, uint32 swizzle, uint32 viewSlice)
{
	cemu_assert_debug(colorBufferPhysMem != MPTR_NULL);
	LatteTextureView* textureView;
	if(viewSlice != 0)
		textureView = LatteTexture_CreateMapping(colorBufferPhysMem, MPTR_NULL, width, height, viewSlice+1, pitch, tileMode, swizzle, 0, 1, viewSlice, 1, format, Latte::E_DIM::DIM_2D_ARRAY, Latte::E_DIM::DIM_2D, false);
	else
		textureView = LatteTexture_CreateMapping(colorBufferPhysMem, MPTR_NULL, width, height, 1, pitch, tileMode, swizzle, 0, 1, viewSlice, 1, format, Latte::E_DIM::DIM_2D, Latte::E_DIM::DIM_2D, false);
	// unbind texture
	g_renderer->texture_bindAndActivate(nullptr, 0);
	return textureView;
}

LatteTextureView* LatteMRT_CreateDepthBuffer(MPTR depthBufferPhysMem, uint32 width, uint32 height, uint32 pitch, Latte::E_HWTILEMODE tileMode, Latte::E_GX2SURFFMT format, uint32 swizzle, sint32 viewSlice)
{
	LatteTextureView* textureView;
	if(viewSlice == 0)
		textureView = LatteTexture_CreateMapping(depthBufferPhysMem, MPTR_NULL, width, height, 1, pitch, tileMode, swizzle, 0, 1, viewSlice, 1, format, Latte::E_DIM::DIM_2D, Latte::E_DIM::DIM_2D, true);
	else
		textureView = LatteTexture_CreateMapping(depthBufferPhysMem, MPTR_NULL, width, height, viewSlice+1, pitch, tileMode, swizzle, 0, 1, viewSlice, 1, format, Latte::E_DIM::DIM_2D_ARRAY, Latte::E_DIM::DIM_2D, true);

	LatteMRT::SetDepthAndStencilAttachment(textureView, textureView->baseTexture->hasStencil);
	// unbind texture
	g_renderer->texture_bindAndActivate(nullptr, 0);
	return textureView;
}

sint32 _depthBufferSizeWarningCount = 0;

LatteTextureView* LatteMRT::GetColorAttachmentTexture(uint32 index, bool createNew, bool checkForTextureChanges)
{
	uint32* colorBufferRegBase = LatteGPUState.contextRegister+(mmCB_COLOR0_BASE + index);
	uint32 regColorBufferBase = colorBufferRegBase[mmCB_COLOR0_BASE - mmCB_COLOR0_BASE] & 0xFFFFFF00; // the low 8 bits are ignored? How to Survive seems to rely on this
	uint32 regColorSize = colorBufferRegBase[mmCB_COLOR0_SIZE - mmCB_COLOR0_BASE];
	uint32 regColorInfo = colorBufferRegBase[mmCB_COLOR0_INFO - mmCB_COLOR0_BASE];
	uint32 regColorView = colorBufferRegBase[mmCB_COLOR0_VIEW - mmCB_COLOR0_BASE];
	// decode color buffer reg info
	Latte::E_HWTILEMODE colorBufferTileMode = (Latte::E_HWTILEMODE)((regColorInfo >> 8) & 0xF);
	uint32 numberType = (regColorInfo >> 12) & 7;
	Latte::E_GX2SURFFMT colorBufferFormat = GetColorBufferFormat(index, LatteGPUState.contextNew);

	MPTR colorBufferPhysMem = regColorBufferBase;

	uint32 colorBufferSwizzle = 0;
	if ( Latte::TM_IsMacroTiled(colorBufferTileMode) )
	{
		colorBufferSwizzle = colorBufferPhysMem & 0x700;
		colorBufferPhysMem = colorBufferPhysMem & ~(7 << 8);
	}

	// get view slice and view slice num
	uint32 viewFirstSlice = (regColorView & 0x7FF);
	uint32 viewNumSlices = ((regColorView >> 13) & 0x7FF) - viewFirstSlice + 1;
	if (viewNumSlices != 1)
	{
		debug_printf("viewNumSlices is not 1! (%d)\n", viewNumSlices);
	}
	uint32 colorBufferPitch = (((regColorSize >> 0) & 0x3FF) + 1);
	colorBufferPitch <<= 3;
	uint32 pitchHeight = (((regColorSize >> 10) & 0xFFFFF) + 1);
	pitchHeight <<= 6;
	uint32 colorBufferHeight = pitchHeight / colorBufferPitch;
	uint32 colorBufferWidth = colorBufferPitch;

	bool colorBufferWasFound = false;
	sint32 viewFirstMip = 0; // todo

	cemu_assert_debug(viewNumSlices == 1);

	LatteTextureView* colorBufferView = LatteTextureViewLookupCache::lookupSliceEx(colorBufferPhysMem, colorBufferWidth, colorBufferHeight, colorBufferPitch, viewFirstMip, viewFirstSlice, colorBufferFormat, false);

	if (colorBufferView == nullptr)
	{
		// create color buffer view
		colorBufferView = LatteTexture_CreateMapping(colorBufferPhysMem, 0, colorBufferWidth, colorBufferHeight, (viewFirstSlice + viewNumSlices), colorBufferPitch, colorBufferTileMode, colorBufferSwizzle>>8, viewFirstMip, 1, viewFirstSlice, viewNumSlices, (Latte::E_GX2SURFFMT)colorBufferFormat, (viewFirstSlice + viewNumSlices)>1? Latte::E_DIM::DIM_2D_ARRAY: Latte::E_DIM::DIM_2D, Latte::E_DIM::DIM_2D, false);
		LatteGPUState.repeatTextureInitialization = true;
		checkForTextureChanges = false;
	}

	if (colorBufferView->baseTexture->swizzle != colorBufferSwizzle)
	{
		colorBufferView->baseTexture->lastRenderTargetSwizzle = colorBufferSwizzle;
	}

	// check for texture changes
	if (checkForTextureChanges)
		LatteTexture_UpdateDataToLatest(colorBufferView->baseTexture);
	// mark as used
	LatteTC_MarkTextureStillInUse(colorBufferView->baseTexture);
	return colorBufferView;
}

// get mask of all used color buffers
uint8 LatteMRT::GetActiveColorBufferMask(const LatteDecompilerShader* pixelShader, const LatteContextRegister& lcr)
{
	const uint32* regView = lcr.GetRawView();

	uint8 colorBufferMask = 0;
	for (uint32 i = 0; i < 8; i++)
	{
		if (regView[mmCB_COLOR0_BASE + i] != MPTR_NULL)
			colorBufferMask |= (1 << i);
	}
	// check if color buffer output is active
	const Latte::LATTE_CB_COLOR_CONTROL& colorControlReg = lcr.CB_COLOR_CONTROL;
	uint32 colorBufferDisable = colorControlReg.get_SPECIAL_OP() == Latte::LATTE_CB_COLOR_CONTROL::E_SPECIALOP::DISABLE;
	if (colorBufferDisable)
		return 0;
	cemu_assert_debug(colorControlReg.get_DEGAMMA_ENABLE() == false); // not supported
	// combine color buffer mask with pixel output mask from pixel shader
	colorBufferMask &= pixelShader->pixelColorOutputMask;
	// combine color buffer mask with color channel mask from mmCB_TARGET_MASK (disable render buffer if all colors are blocked)
	uint32 channelTargetMask = lcr.CB_TARGET_MASK.get_MASK();
	for (uint32 i = 0; i < 8; i++)
	{
		if (((channelTargetMask >> (i * 4)) & 0xF) == 0)
			colorBufferMask &= ~(1 << i);
	}

	// render targets smaller than the scissor size are not allowed
	// this fixes a few render issues in Cemu but we dont know if this matches HW behavior
	cemu_assert_debug(lcr.PA_SC_GENERIC_SCISSOR_TL.get_WINDOW_OFFSET_DISABLE() == true); // todo (not exposed by GX2 API)
	uint32 scissorAccessWidth = lcr.PA_SC_GENERIC_SCISSOR_BR.get_BR_X();
	uint32 scissorAccessHeight = lcr.PA_SC_GENERIC_SCISSOR_BR.get_BR_Y();
	for (uint32 i = 0; i < 8; i++)
	{
		if( (colorBufferMask&(1<<i)) == 0 )
			continue;
		// get width/height
		uint32 regColorSize = regView[mmCB_COLOR0_SIZE + i];
		uint32 regColorInfo = regView[mmCB_COLOR0_INFO + i];
		// decode color buffer reg info
		uint32 colorBufferPitch = (((regColorSize >> 0) & 0x3FF) + 1);
		colorBufferPitch <<= 3;
		uint32 pitchHeight = (((regColorSize >> 10) & 0xFFFFF) + 1);
		pitchHeight <<= 6;
		uint32 colorBufferHeight = pitchHeight / colorBufferPitch;
		uint32 colorBufferWidth = colorBufferPitch;

		if ((colorBufferWidth < (sint32)scissorAccessWidth) ||
			(colorBufferHeight < (sint32)scissorAccessHeight))
		{
            // log this?
			colorBufferMask &= ~(1<<i);
		}

	}

	return colorBufferMask;
}

// returns true if depth/stencil buffer is used
bool LatteMRT::GetActiveDepthBufferMask(const LatteContextRegister& lcr)
{
	bool depthBufferMask = true;
	// if depth test is not used then detach the depth buffer
	bool depthEnable = lcr.DB_DEPTH_CONTROL.get_Z_ENABLE();
	bool stencilTestEnable = lcr.DB_DEPTH_CONTROL.get_STENCIL_ENABLE();
	bool backStencilEnable = lcr.DB_DEPTH_CONTROL.get_BACK_STENCIL_ENABLE();

	if (!depthEnable && !stencilTestEnable && !backStencilEnable)
		depthBufferMask = false;

	return depthBufferMask;
}

const uint32 _colorBufferFormatBits[] =
{
	0, // 0
	0x200, // 1
	0, // 2
	0, // 3
	0x100, // 4
	0x300, // 5
	0x400, // 6
	0x800, // 7
};

Latte::E_GX2SURFFMT LatteMRT::GetColorBufferFormat(const uint32 index, const LatteContextRegister& lcr)
{
	cemu_assert_debug(index < Latte::GPU_LIMITS::NUM_COLOR_ATTACHMENTS);
	uint32 regColorInfo = lcr.GetRawView()[mmCB_COLOR0_INFO + index];
	uint32 colorBufferFormat = (regColorInfo >> 2) & 0x3F; // base HW format
	uint32 numberType = (regColorInfo >> 12) & 7;
	colorBufferFormat |= _colorBufferFormatBits[numberType];
	return (Latte::E_GX2SURFFMT)colorBufferFormat;
}

// return GX2 format of current depth buffer
Latte::E_GX2SURFFMT LatteMRT::GetDepthBufferFormat(const LatteContextRegister& lcr)
{
	uint32 regDepthBufferInfo = lcr.GetRawView()[mmDB_DEPTH_INFO];
	switch (regDepthBufferInfo & 7)
	{
	case 1:
		return Latte::E_GX2SURFFMT::D16_UNORM;
	case 3:
		return Latte::E_GX2SURFFMT::D24_S8_UNORM;
	case 5:
		return Latte::E_GX2SURFFMT::D24_S8_FLOAT;
	case 6:
		return Latte::E_GX2SURFFMT::D32_FLOAT;
	case 7:
		return Latte::E_GX2SURFFMT::D32_S8_FLOAT;
	default:
		debug_printf("Invalid DB_DEPTH_INFO depthbuffer format (%d)\n", (regDepthBufferInfo & 7));
		break;
	}
	return Latte::E_GX2SURFFMT::D16_UNORM;
}

bool LatteMRT::UpdateCurrentFBO()
{
	catchOpenGLError();
	sLatteRenderTargetState.rtUpdateListCount = 0;
	// combine color buffer mask with pixel output mask from pixel shader
	LatteDecompilerShader* pixelShader = LatteSHRC_GetActivePixelShader();
	uint8 colorBufferMask = GetActiveColorBufferMask(pixelShader, LatteGPUState.contextNew);
	bool depthBufferMask = GetActiveDepthBufferMask(LatteGPUState.contextNew);

	// if depth test is not used then detach the depth buffer
	bool depthEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_Z_ENABLE();
	bool stencilTestEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ENABLE();
	bool backStencilEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_BACK_STENCIL_ENABLE();

	if (!depthEnable && !stencilTestEnable && !backStencilEnable)
		depthBufferMask = false;

	bool hasResizedTexture = false; // set to true if any of the color buffers or the depth buffer reference a resized texture (via graphic pack texture rules)
	sLatteRenderTargetState.renderTargetIsResized = false;
	// real size
	LatteMRTQuad* rtRealSize = &sLatteRenderTargetState.currentRenderSize;
	rtRealSize->width = 0;
	rtRealSize->height = 0;
	// effective size (differs from real size only if graphic pack rules overwrite texture sizes)
	LatteMRTQuad* rtEffectiveSize = &sLatteRenderTargetState.currentEffectiveSize;
	rtEffectiveSize->width = 0;
	rtEffectiveSize->height = 0;
	// get scissor box
	cemu_assert_debug(LatteGPUState.contextNew.PA_SC_GENERIC_SCISSOR_TL.get_WINDOW_OFFSET_DISABLE() == true); // todo (not exposed by GX2 API?)
	uint32 scissorX = LatteGPUState.contextNew.PA_SC_GENERIC_SCISSOR_TL.get_TL_X();
	uint32 scissorY = LatteGPUState.contextNew.PA_SC_GENERIC_SCISSOR_TL.get_TL_Y();
	uint32 scissorWidth = LatteGPUState.contextNew.PA_SC_GENERIC_SCISSOR_BR.get_BR_X() - scissorX;
	uint32 scissorHeight = LatteGPUState.contextNew.PA_SC_GENERIC_SCISSOR_BR.get_BR_Y() - scissorY;
	uint32 scissorAccessWidth = scissorX + scissorWidth;
	uint32 scissorAccessHeight = scissorY + scissorHeight;
	// color buffers
	for (uint32 i = 0; i < Latte::GPU_LIMITS::NUM_COLOR_ATTACHMENTS; i++)
	{
		if (((colorBufferMask)&(1 << i)) == 0)
		{
			// unbind
			SetColorAttachment(i, nullptr);
			continue;
		}
		LatteTextureView* colorAttachmentView = GetColorAttachmentTexture(i, true, true);
		SetColorAttachment(i, colorAttachmentView);
		// after the drawcall mark the texture as updated
		sLatteRenderTargetState.rtUpdateList[sLatteRenderTargetState.rtUpdateListCount] = colorAttachmentView;
		sLatteRenderTargetState.rtUpdateListCount++;

		sint32 colorAttachmentWidth;
		sint32 colorAttachmentHeight;

		LatteTexture_getSize(colorAttachmentView->baseTexture, &colorAttachmentWidth, &colorAttachmentHeight, nullptr, colorAttachmentView->firstMip);

		// set effective size
		sint32 effectiveWidth, effectiveHeight;
		LatteTexture_getEffectiveSize(colorAttachmentView->baseTexture, &effectiveWidth, &effectiveHeight, nullptr, colorAttachmentView->firstMip);
		if (rtEffectiveSize->width == 0 && rtEffectiveSize->height == 0)
		{
			rtEffectiveSize->width = effectiveWidth;
			rtEffectiveSize->height = effectiveHeight;
		}
		else if (rtEffectiveSize->width != effectiveWidth && rtEffectiveSize->height != effectiveHeight)
		{
#ifdef CEMU_DEBUG_ASSERT
			cemuLog_log(LogType::Force, "Color buffer size mismatch ({}x{}). Effective size: {}x{} Real size: {}x{} Mismatching texture: {:08x} {}x{} fmt {:04x}", rtEffectiveSize->width, rtEffectiveSize->height, effectiveWidth, effectiveHeight, colorAttachmentView->baseTexture->width, colorAttachmentView->baseTexture->height, colorAttachmentView->baseTexture->physAddress, colorAttachmentView->baseTexture->width, colorAttachmentView->baseTexture->height, (uint32)colorAttachmentView->baseTexture->format);
#endif
		}
		// currently the first color attachment defines the size of the current render target
		if (rtRealSize->width == 0 && rtRealSize->height == 0)
		{
			rtRealSize->width = colorAttachmentWidth;
			rtRealSize->height = colorAttachmentHeight;
		}

		if (colorAttachmentView)
			continue;
	}
	// depth buffer
	if (depthBufferMask)
	{
		uint32 regDepthBuffer = LatteGPUState.contextRegister[mmDB_HTILE_DATA_BASE];
		uint32 regDepthSize = LatteGPUState.contextRegister[mmDB_DEPTH_SIZE];
		uint32 regDepthBufferInfo = LatteGPUState.contextRegister[mmDB_DEPTH_INFO];

		// get format and tileMode from info reg
		Latte::E_GX2SURFFMT depthBufferFormat = GetDepthBufferFormat(LatteGPUState.contextNew);
		Latte::E_HWTILEMODE depthBufferTileMode = (Latte::E_HWTILEMODE)((regDepthBufferInfo >> 15) & 0xF);
		MPTR depthBufferPhysMem = regDepthBuffer << 8;

		uint32 depthBufferPitch = (((regDepthSize >> 0) & 0x3FF) + 1);
		uint32 depthBufferHeight = ((((regDepthSize >> 10) & 0xFFFFF) + 1) / depthBufferPitch);
		depthBufferPitch <<= 3;
		depthBufferHeight <<= 3;
		uint32 depthBufferWidth = depthBufferPitch;

		if (depthBufferWidth < 2)
		{
			debug_printf("depthBufferWidth has invalid value %d\n", depthBufferWidth);
			depthBufferWidth = 2;
		}
		if (depthBufferHeight < 2)
		{
			debug_printf("depthBufferHeight has invalid value %d\n", depthBufferHeight);
			depthBufferHeight = 2;
		}

		bool blockDepthBuffer = false;
		if (scissorAccessWidth > depthBufferPitch || scissorAccessHeight > depthBufferHeight)
		{
			SetDepthAndStencilAttachment(nullptr, false);
			blockDepthBuffer = true;
			// set effective size
			if( rtEffectiveSize->width == 0 && rtEffectiveSize->height == 0 )
			{
				rtEffectiveSize->width = rtRealSize->width;
				rtEffectiveSize->height = rtRealSize->height;
			}
		}

		if (blockDepthBuffer == false)
		{
			if (rtRealSize->width == 0)
			{
				rtRealSize->width = depthBufferWidth;
				rtRealSize->height = depthBufferHeight;
			}
			uint32 regDepthView = LatteGPUState.contextRegister[mmDB_DEPTH_VIEW];
			uint32 depthBufferViewFirstSlice = (regDepthView & 0x7FF);
			uint32 depthBufferViewNumSlices = ((regDepthView >> 13) & 0x7FF) - depthBufferViewFirstSlice + 1;
			cemu_assert_debug(depthBufferViewNumSlices == 1); // binding multiple layers makes no sense?

			uint32 depthBufferSwizzle = 0;
			if (Latte::TM_IsMacroTiled(depthBufferTileMode))
			{
				depthBufferSwizzle = (depthBufferPhysMem >> 8) & 7;
				depthBufferPhysMem = depthBufferPhysMem & ~(7 << 8);
			}

			if (depthBufferPhysMem != MPTR_NULL)
			{
				bool depthBufferWasFound = false;
				LatteTextureView* depthBufferView = LatteTextureViewLookupCache::lookupSliceEx(depthBufferPhysMem, depthBufferWidth, depthBufferHeight, depthBufferPitch, 0, depthBufferViewFirstSlice, depthBufferFormat, true);
				if (depthBufferView == nullptr)
				{
					// create depth buffer view
					if(depthBufferViewFirstSlice == 0)
						depthBufferView = LatteTexture_CreateMapping(depthBufferPhysMem, 0, depthBufferWidth, depthBufferHeight, 1, depthBufferPitch, depthBufferTileMode, depthBufferSwizzle, 0, 1, 0, 1, depthBufferFormat, Latte::E_DIM::DIM_2D, Latte::E_DIM::DIM_2D, true);
					else
						depthBufferView = LatteTexture_CreateMapping(depthBufferPhysMem, 0, depthBufferWidth, depthBufferHeight, depthBufferViewFirstSlice+1, depthBufferPitch, depthBufferTileMode, depthBufferSwizzle, 0, 1, depthBufferViewFirstSlice, 1, depthBufferFormat, Latte::E_DIM::DIM_2D_ARRAY, Latte::E_DIM::DIM_2D, true);
					LatteGPUState.repeatTextureInitialization = true;
				}
				else
				{
					// check for texture changes
					LatteTexture_UpdateDataToLatest(depthBufferView->baseTexture);
				}
				// set effective size
				sint32 effectiveWidth, effectiveHeight;
				LatteTexture_getEffectiveSize(depthBufferView->baseTexture, &effectiveWidth, &effectiveHeight, NULL);
				if (rtEffectiveSize->width == 0 && rtEffectiveSize->height == 0)
				{
					rtEffectiveSize->width = effectiveWidth;
					rtEffectiveSize->height = effectiveHeight;
				}
				else if (rtEffectiveSize->width > effectiveWidth && rtEffectiveSize->height > effectiveHeight)
				{
					if (_depthBufferSizeWarningCount < 100)
					{
						cemuLog_logDebug(LogType::Force, "Depth buffer size too small. Effective size: {}x{} Real size: {}x{} Mismatching texture: {:08x} {}x{} fmt {:04x}", effectiveWidth, effectiveHeight, depthBufferView->baseTexture->width, depthBufferView->baseTexture->height, depthBufferView->baseTexture->physAddress, depthBufferView->baseTexture->width, depthBufferView->baseTexture->height, (uint32)depthBufferView->baseTexture->format);
						_depthBufferSizeWarningCount++;
					}
				}
				LatteTC_MarkTextureStillInUse(depthBufferView->baseTexture);
				// after the drawcall mark the texture as updated
				sLatteRenderTargetState.rtUpdateList[sLatteRenderTargetState.rtUpdateListCount] = depthBufferView;
				sLatteRenderTargetState.rtUpdateListCount++;
				SetDepthAndStencilAttachment(depthBufferView, depthBufferView->baseTexture->hasStencil);
			}
		}
		else
		{
			SetDepthAndStencilAttachment(nullptr, false);
		}
	}
	else
	{
		SetDepthAndStencilAttachment(nullptr, false);
	}
	catchOpenGLError();
	if (colorBufferMask || depthBufferMask)
	{
		hasValidFramebufferAttached = true;
	}
	else
	{
		hasValidFramebufferAttached = false;
		return true;
	}
	if( rtEffectiveSize->width != rtRealSize->width || rtEffectiveSize->height != rtRealSize->height )
	{
		//debug_printf("RenderTarget rescaled. Real: %dx%d Resized: %dx%d\n", rtRealSize->width, rtRealSize->height, rtEffectiveSize->width, rtEffectiveSize->height);
		sLatteRenderTargetState.renderTargetIsResized = true;
	}
	if (sLatteRenderTargetState.currentEffectiveSize.width == 0)
	{
		debug_printf("Render target effective size is 0. May indicate a bug in Cemu or invalid color/depth buffers\n");
		return false;
	}
	return true;
}

// return a vec4 with xy being the scale ratio for render target extend and zw being the virtual viewport dimensions
void LatteMRT::GetCurrentFragCoordScale(float* coordScale)
{
	if (sLatteRenderTargetState.renderTargetIsResized)
	{
		coordScale[0] = (float)sLatteRenderTargetState.currentRenderSize.width / (float)sLatteRenderTargetState.currentEffectiveSize.width;
		coordScale[1] = (float)sLatteRenderTargetState.currentRenderSize.height / (float)sLatteRenderTargetState.currentEffectiveSize.height;
		coordScale[2] = sLatteRenderTargetState.currentGuestViewport.width;
		coordScale[3] = sLatteRenderTargetState.currentGuestViewport.height;
	}
	else
	{
		coordScale[0] = 1.0f;
		coordScale[1] = 1.0f;
		coordScale[2] = sLatteRenderTargetState.currentGuestViewport.width;
		coordScale[3] = sLatteRenderTargetState.currentGuestViewport.height;
	}
}

void LatteMRT::GetVirtualViewportDimensions(sint32& width, sint32& height)
{
	width = sLatteRenderTargetState.currentGuestViewport.width;
	height = sLatteRenderTargetState.currentGuestViewport.height;
}

// flag all FBO textures as updated via GPU
// also handle texture readback
void LatteRenderTarget_trackUpdates()
{
	// after the drawcall, mark the render target textures as dynamically updated
	uint64 eventCounter = LatteTexture_getNextUpdateEventCounter();
	for (sint32 i = 0; i < sLatteRenderTargetState.rtUpdateListCount; i++)
	{
		LatteTextureView* texView = sLatteRenderTargetState.rtUpdateList[i];
		LatteTexture* baseTexture = texView->baseTexture;
		LatteTexture_TrackTextureGPUWrite(baseTexture, texView->firstSlice, texView->firstMip, eventCounter);
		// texture readback
		if (baseTexture->enableReadback)
		{
			LatteTextureReadback_Initate(texView);
		}
	}
}

void LatteRenderTarget_itHLESwapScanBuffer()
{
	performanceMonitor.cycle[performanceMonitor.cycleIndex].frameCounter++;
	if(LatteGPUState.frameCounter > 5)
		performanceMonitor.gpuTime_frameTime.endMeasuring();
	LattePerformanceMonitor_frameEnd();
	LatteGPUState.frameCounter++;
	g_renderer->SwapBuffers(true, true);

	catchOpenGLError();
	performanceMonitor.gpuTime_frameTime.beginMeasuring();

	LatteTC_CleanupUnusedTextures();
	LatteDraw_cleanupAfterFrame();
	LatteQuery_CancelActiveGPU7Queries();
	LatteBufferCache_notifySwapTVScanBuffer();
	LattePerformanceMonitor_frameBegin();
}

void LatteRenderTarget_applyTextureColorClear(LatteTexture* texture, uint32 sliceIndex, uint32 mipIndex, float r, float g, float b, float a, uint64 eventCounter)
{
	if (texture->isDepth)
	{
		cemuLog_logDebug(LogType::Force, "Unsupported clear depth as color");
	}
	else
	{
		g_renderer->texture_clearColorSlice(texture, sliceIndex, mipIndex, r, g, b, a);
		LatteTexture_MarkDynamicTextureAsChanged(texture->baseView, sliceIndex, mipIndex, eventCounter);
	}
}

void LatteRenderTarget_applyTextureDepthClear(LatteTexture* texture, uint32 sliceIndex, uint32 mipIndex, bool hasDepthClear, bool hasStencilClear, float depthValue, uint8 stencilValue, uint64 eventCounter)
{
	if(texture->isDepth)	
	{ 
		g_renderer->texture_clearDepthSlice(texture, sliceIndex, mipIndex, hasDepthClear, hasStencilClear, depthValue, stencilValue);
	}
	else
	{
		// clearing a color texture using depth clear
		if (hasStencilClear)
			return; // operation likely not intended as a color clear
		//cemu_assert_debug(!hasStencilClear);
		if (hasDepthClear)
		{
			g_renderer->texture_clearColorSlice(texture, sliceIndex, mipIndex, depthValue, depthValue, depthValue, depthValue);
		}

	}
	LatteTexture_MarkDynamicTextureAsChanged(texture->baseView, sliceIndex, mipIndex, eventCounter);
}

void LatteRenderTarget_itHLEClearColorDepthStencil(uint32 clearMask, MPTR colorBufferMPTR, MPTR colorBufferFormat, Latte::E_HWTILEMODE colorBufferTilemode, uint32 colorBufferWidth, uint32 colorBufferHeight, uint32 colorBufferPitch, uint32 colorBufferViewFirstSlice, uint32 colorBufferViewNumSlice, MPTR depthBufferMPTR, MPTR depthBufferFormat, Latte::E_HWTILEMODE depthBufferTileMode, sint32 depthBufferWidth, sint32 depthBufferHeight, sint32 depthBufferPitch, sint32 depthBufferViewFirstSlice, sint32 depthBufferViewNumSlice, float r, float g, float b, float a, float clearDepth, uint32 clearStencil)
{
	uint32 depthBufferMipIndex = 0; // todo
	uint32 colorBufferMipIndex = 0; // todo

	bool hasColorClear = (clearMask & 1);
	bool hasDepthClear = (clearMask & 2);
	bool hasStencilClear = (clearMask & 4);

	// extract swizzle offset from pointer
	uint32 colorBufferSwizzle = 0;
	uint32 depthBufferSwizzle = 0;
	if (Latte::TM_IsMacroTiled(colorBufferTilemode))
	{
		colorBufferSwizzle = (colorBufferMPTR >> 8) & 7;
		colorBufferMPTR = colorBufferMPTR & ~(7 << 8);
	}
	if (Latte::TM_IsMacroTiled(depthBufferTileMode))
	{
		depthBufferSwizzle = (depthBufferMPTR >> 8) & 7;
		depthBufferMPTR = depthBufferMPTR & ~(7 << 8);
	}

	cemu_assert_debug(colorBufferViewNumSlice <= 1);
	cemu_assert_debug(depthBufferViewNumSlice <= 1);

	// clear color buffer (if flag set)
	uint64 eventCounter = LatteTexture_getNextUpdateEventCounter();
	if ((clearMask & 1) != 0 && colorBufferMPTR != MPTR_NULL && colorBufferWidth > 0 && colorBufferHeight > 0)
	{
		// clear color
		sint32 searchIndex = 0;
		bool targetFound = false;
		while (true)
		{
			LatteTextureView* colorView = LatteMRT_FindColorBufferForClearing(colorBufferMPTR, colorBufferWidth, colorBufferHeight, colorBufferPitch, colorBufferFormat, colorBufferViewFirstSlice, &searchIndex);
			if (!colorView)
				break;
			if (Latte::GetFormatBits((Latte::E_GX2SURFFMT)colorBufferFormat) != Latte::GetFormatBits(colorView->baseTexture->format))
			{
				continue;
			}

			if (colorView->baseTexture->pitch == colorBufferPitch && colorView->baseTexture->height == colorBufferHeight)
				targetFound = true;

			LatteRenderTarget_applyTextureColorClear(colorView->baseTexture, colorBufferViewFirstSlice, colorBufferMipIndex, r, g, b, a, eventCounter);

		}
		if (targetFound == false)
		{
			// create new texture with matching format
			cemu_assert_debug(colorBufferViewNumSlice <= 1);
			LatteTextureView* newColorView = LatteMRT_CreateColorBuffer(colorBufferMPTR, colorBufferWidth, colorBufferHeight, colorBufferPitch, (Latte::E_GX2SURFFMT)colorBufferFormat, colorBufferTilemode, colorBufferSwizzle, colorBufferViewFirstSlice);
			LatteRenderTarget_applyTextureColorClear(newColorView->baseTexture, colorBufferViewFirstSlice, colorBufferMipIndex, r, g, b, a, eventCounter);
		}
	}
	// clear depth or stencil buffer (if flag set)
	if ((hasDepthClear || hasStencilClear) && depthBufferMPTR != MPTR_NULL)
	{
		std::vector<LatteTexture*> list_depthClearTextures;
		LatteTC_LookupTexturesByPhysAddr(depthBufferMPTR, list_depthClearTextures);
		bool foundMatchingDepthBuffer = false;
		// todo - support for clearing depth mips?
		cemu_assert_debug(depthBufferViewNumSlice == 1);

		for (auto& texItr : list_depthClearTextures)
		{
			// only clear depth buffers if they are smaller
			if (texItr->pitch > depthBufferPitch)
				continue;
			if (depthBufferViewFirstSlice >= texItr->depth)
				continue; // slice out of range

			if (texItr->pitch == depthBufferPitch && texItr->height == depthBufferHeight)
				foundMatchingDepthBuffer = true;

			// todo - calculate actual sliceIndex and mipIndex since the textures in list_depthClearTextures dont necessarily share the same base
			LatteRenderTarget_applyTextureDepthClear(texItr, depthBufferViewFirstSlice, depthBufferMipIndex, hasDepthClear, hasStencilClear, clearDepth, clearStencil, eventCounter);
		}

		if (foundMatchingDepthBuffer == false)
		{
			LatteTextureView* newDepthBufferView = LatteMRT_CreateDepthBuffer(depthBufferMPTR, depthBufferWidth, depthBufferHeight, depthBufferPitch, depthBufferTileMode, (Latte::E_GX2SURFFMT)depthBufferFormat, depthBufferSwizzle, depthBufferViewFirstSlice);
			LatteRenderTarget_applyTextureDepthClear(newDepthBufferView->baseTexture, depthBufferViewFirstSlice, depthBufferMipIndex, hasDepthClear, hasStencilClear, clearDepth, clearStencil, eventCounter);
		}
	}
}

sint32 _currentOutputImageWidth = 0;
sint32 _currentOutputImageHeight = 0;

void LatteRenderTarget_getScreenImageArea(sint32* x, sint32* y, sint32* width, sint32* height, sint32* fullWidth, sint32* fullHeight, bool padView)
{
	int w, h;
	if(padView && gui_isPadWindowOpen())
		gui_getPadWindowPhysSize(w, h);
	else
		gui_getWindowPhysSize(w, h);

	sint32 scaledOutputX;
	sint32 scaledOutputY;
	if (GetConfig().fullscreen_scaling == kKeepAspectRatio)
	{
		// calculate maximum possible resolution with intact aspect ratio
		scaledOutputX = w;
		scaledOutputY = _currentOutputImageHeight * w / std::max(_currentOutputImageWidth, 1);
		if (scaledOutputY > h)
		{
			scaledOutputX = _currentOutputImageWidth * h / std::max(_currentOutputImageHeight, 1);
			scaledOutputY = h;
		}
	}
	else
	{
		scaledOutputX = w;
		scaledOutputY = h;
	}

	*x = (w - scaledOutputX) / 2;
	*y = (h - scaledOutputY) / 2;
	*width = scaledOutputX;
	*height = scaledOutputY;
	if (fullWidth)
		*fullWidth = w;
	if (fullHeight)
		*fullHeight = h;
}

void LatteRenderTarget_copyToBackbuffer(LatteTextureView* textureView, bool isPadView)
{
	if (g_renderer->GetType() == RendererAPI::Vulkan)
	{
		((VulkanRenderer*)g_renderer.get())->PreparePresentationFrame(!isPadView);
	}

	// make sure texture is updated to latest data in cache
	LatteTexture_UpdateDataToLatest(textureView->baseTexture);
	// mark source texture as still in use
	LatteTC_MarkTextureStillInUse(textureView->baseTexture);

	sint32 effectiveWidth;
	sint32 effectiveHeight;
	sint32 effectiveDepth;
	LatteTexture_getEffectiveSize(textureView->baseTexture, &effectiveWidth, &effectiveHeight, &effectiveDepth, 0);
	_currentOutputImageWidth = effectiveWidth;
	_currentOutputImageHeight = effectiveHeight;
	
	sint32 imageX, imageY;
	sint32 imageWidth, imageHeight;
	sint32 fullscreenWidth, fullscreenHeight;

	LatteRenderTarget_getScreenImageArea(&imageX, &imageY, &imageWidth, &imageHeight, &fullscreenWidth, &fullscreenHeight, isPadView);

	bool clearBackground = false;
	if (imageWidth != fullscreenWidth || imageHeight != fullscreenHeight)
		clearBackground = true;

	const bool renderUpsideDown = ActiveSettings::RenderUpsideDownEnabled();
	// force disable bicubic scaling if output resolution is equal/smaller than input resolution
	const bool downscaling = (imageWidth <= effectiveWidth || imageHeight <= effectiveHeight);
	// check for graphic pack shaders
	RendererOutputShader* shader = nullptr;
	LatteTextureView::MagFilter filter = LatteTextureView::MagFilter::kLinear;
	for(const auto& gp : GraphicPack2::GetActiveGraphicPacks())
	{
		if(downscaling)
		{
			shader = gp->GetDownscalingShader(renderUpsideDown);
			if (shader)
			{
				filter = gp->GetDownscalingMagFilter();
				break;
			}
		}
		else
		{
			shader = gp->GetUpscalingShader(renderUpsideDown);
			if (shader)
			{
				filter = gp->GetUpscalingMagFilter();
				break;
			}
		}

		shader = gp->GetOuputShader(renderUpsideDown);
		if (shader)
		{
			filter = downscaling ? gp->GetDownscalingMagFilter() : gp->GetUpscalingMagFilter();
			break;
		}
	}

	if (shader == nullptr)
	{
		sint32 scaling_filter = downscaling ? GetConfig().downscale_filter : GetConfig().upscale_filter;
		
		if (g_renderer->GetType() == RendererAPI::Vulkan)
		{
			// force linear or nearest neighbor filter
			if(scaling_filter != kLinearFilter && scaling_filter != kNearestNeighborFilter)
				scaling_filter = kLinearFilter;
		}

		if (scaling_filter == kLinearFilter)
		{
			if(renderUpsideDown)
				shader = RendererOutputShader::s_copy_shader_ud;
			else
				shader = RendererOutputShader::s_copy_shader;

			filter = LatteTextureView::MagFilter::kLinear;
		}
		else if (scaling_filter == kBicubicFilter)
		{
			if (renderUpsideDown)
				shader = RendererOutputShader::s_bicubic_shader_ud;
			else
				shader = RendererOutputShader::s_bicubic_shader;

			filter = LatteTextureView::MagFilter::kNearestNeighbor;
		}
		else if (scaling_filter == kBicubicHermiteFilter)
		{
			if (renderUpsideDown)
				shader = RendererOutputShader::s_hermit_shader_ud;
			else
				shader = RendererOutputShader::s_hermit_shader;

			filter = LatteTextureView::MagFilter::kLinear;
		}
		else if (scaling_filter == kNearestNeighborFilter)
		{
			if (renderUpsideDown)
				shader = RendererOutputShader::s_copy_shader_ud;
			else
				shader = RendererOutputShader::s_copy_shader;

			filter = LatteTextureView::MagFilter::kNearestNeighbor;
		}
	}
	cemu_assert(shader);
	g_renderer->DrawBackbufferQuad(textureView, shader, filter==LatteTextureView::MagFilter::kLinear, imageX, imageY, imageWidth, imageHeight, isPadView, clearBackground);
	g_renderer->HandleScreenshotRequest(textureView, isPadView);
	if (!g_renderer->ImguiBegin(!isPadView))
		return;
	swkbd_render(!isPadView);
	nn::erreula::render(!isPadView);
	LatteOverlay_render(isPadView);
	g_renderer->ImguiEnd();
}

bool ctrlTabHotkeyPressed = false;

void LatteRenderTarget_itHLECopyColorBufferToScanBuffer(MPTR colorBufferPtr, uint32 colorBufferWidth, uint32 colorBufferHeight, uint32 colorBufferSliceIndex, uint32 colorBufferFormat, uint32 colorBufferPitch, Latte::E_HWTILEMODE colorBufferTilemode, uint32 colorBufferSwizzle, uint32 renderTarget)
{
	cemu_assert_debug(colorBufferSliceIndex == 0); // todo - support for non-zero slice
	LatteTextureView* texView = LatteTC_GetTextureSliceViewOrTryCreate(colorBufferPtr, MPTR_NULL, (Latte::E_GX2SURFFMT)colorBufferFormat, colorBufferTilemode, colorBufferWidth, colorBufferHeight, 1, colorBufferPitch, colorBufferSwizzle, 0, 0, true);
	if (!texView)
	{
		return;
	}

	const bool tabPressed = gui_isKeyDown(PlatformKeyCodes::TAB);
	const bool ctrlPressed = gui_isKeyDown(PlatformKeyCodes::LCONTROL);

	bool showDRC = swkbd_hasKeyboardInputHook() == false && tabPressed;
	bool& alwaysDisplayDRC = LatteGPUState.alwaysDisplayDRC;

	if (ctrlPressed && tabPressed)
	{
		if (ctrlTabHotkeyPressed == false)
		{
			alwaysDisplayDRC = !alwaysDisplayDRC;
			ctrlTabHotkeyPressed = true;
		}
	}
	else
		ctrlTabHotkeyPressed = false;

	if (alwaysDisplayDRC)
		showDRC = !tabPressed;

	if (!showDRC)
	{
		auto controller = InputManager::instance().get_vpad_controller(0);
		if (controller && controller->is_screen_active())
			showDRC = true;
		if (!showDRC)
		{
			controller = InputManager::instance().get_vpad_controller(1);
			if (controller && controller->is_screen_active())
				showDRC = true;	
		}
	}

	if ((renderTarget & RENDER_TARGET_DRC) && g_renderer->IsPadWindowActive())
		LatteRenderTarget_copyToBackbuffer(texView, true);
	if (((renderTarget & RENDER_TARGET_TV) && !showDRC) || ((renderTarget & RENDER_TARGET_DRC) && showDRC))
		LatteRenderTarget_copyToBackbuffer(texView, false);
}



// returns the current size of the virtual viewport (not the same as effective size, which can be influenced by texture rules)
void LatteRenderTarget_GetCurrentVirtualViewportSize(sint32* viewportWidth, sint32* viewportHeight)
{
	*viewportWidth = sLatteRenderTargetState.currentGuestViewport.width;
	*viewportHeight = sLatteRenderTargetState.currentGuestViewport.height;
}

void LatteRenderTarget_updateViewport()
{
	float vpWidth = LatteGPUState.contextNew.PA_CL_VPORT_XSCALE.get_SCALE() / 0.5f;
	float vpX = LatteGPUState.contextNew.PA_CL_VPORT_XOFFSET.get_OFFSET() - LatteGPUState.contextNew.PA_CL_VPORT_XSCALE.get_SCALE();
	float vpHeight = LatteGPUState.contextNew.PA_CL_VPORT_YSCALE.get_SCALE() / -0.5f;
	float vpY = LatteGPUState.contextNew.PA_CL_VPORT_YOFFSET.get_OFFSET() + LatteGPUState.contextNew.PA_CL_VPORT_YSCALE.get_SCALE();
	
	bool halfZ = LatteGPUState.contextNew.PA_CL_CLIP_CNTL.get_DX_CLIP_SPACE_DEF();

	// calculate near/far
	float farZ;
	float nearZ;
	float s = LatteGPUState.contextNew.PA_CL_VPORT_ZSCALE.get_SCALE();
	float b = LatteGPUState.contextNew.PA_CL_VPORT_ZOFFSET.get_OFFSET();
	if (halfZ == false)
	{
		farZ = s + b;
		nearZ = b - s;
	}
	else
	{
		farZ = s + b;
		nearZ = b;
	}

	sLatteRenderTargetState.currentGuestViewport.width = vpWidth;
	sLatteRenderTargetState.currentGuestViewport.height = vpHeight;
	if (sLatteRenderTargetState.renderTargetIsResized)
	{
		vpX *= ((float)sLatteRenderTargetState.currentEffectiveSize.width / (float)sLatteRenderTargetState.currentRenderSize.width);
		vpY *= ((float)sLatteRenderTargetState.currentEffectiveSize.height / (float)sLatteRenderTargetState.currentRenderSize.height);
		vpWidth *= ((float)sLatteRenderTargetState.currentEffectiveSize.width / (float)sLatteRenderTargetState.currentRenderSize.width);
		vpHeight *= ((float)sLatteRenderTargetState.currentEffectiveSize.height / (float)sLatteRenderTargetState.currentRenderSize.height);
	}
	g_renderer->renderTarget_setViewport(vpX, vpY, vpWidth, vpHeight, nearZ, farZ, halfZ);
}

void LatteRenderTarget_updateScissorBox()
{
	// update scissor box
	uint32 scissorX = LatteGPUState.contextNew.PA_SC_GENERIC_SCISSOR_TL.get_TL_X();
	uint32 scissorY = LatteGPUState.contextNew.PA_SC_GENERIC_SCISSOR_TL.get_TL_Y();
	uint32 scissorWidth = LatteGPUState.contextNew.PA_SC_GENERIC_SCISSOR_BR.get_BR_X() - scissorX;
	uint32 scissorHeight = LatteGPUState.contextNew.PA_SC_GENERIC_SCISSOR_BR.get_BR_Y() - scissorY;
	if( sLatteRenderTargetState.renderTargetIsResized )
	{
		scissorX = (sint32)((float)scissorX * ((float)sLatteRenderTargetState.currentEffectiveSize.width / (float)sLatteRenderTargetState.currentRenderSize.width));
		scissorY = (sint32)((float)scissorY * ((float)sLatteRenderTargetState.currentEffectiveSize.height / (float)sLatteRenderTargetState.currentRenderSize.height));
		scissorWidth = (sint32)((float)scissorWidth * ((float)sLatteRenderTargetState.currentEffectiveSize.width / (float)sLatteRenderTargetState.currentRenderSize.width));
		scissorHeight = (sint32)((float)scissorHeight * ((float)sLatteRenderTargetState.currentEffectiveSize.height / (float)sLatteRenderTargetState.currentRenderSize.height));
	}

	if( scissorX != prevScissorX || scissorY != prevScissorY || scissorWidth != prevScissorWidth || scissorHeight != prevScissorHeight )
	{
		g_renderer->renderTarget_setScissor(scissorX, scissorY, scissorWidth, scissorHeight);
		prevScissorX = scissorX;
		prevScissorY = scissorY;
		prevScissorWidth = scissorWidth;
		prevScissorHeight = scissorHeight;
	}
}

void LatteRenderTarget_unloadAll()
{
	if (g_emptyFBO)
	{
		LatteMRT::DeleteCachedFBO(g_emptyFBO);
		g_emptyFBO = nullptr;
	}
}
