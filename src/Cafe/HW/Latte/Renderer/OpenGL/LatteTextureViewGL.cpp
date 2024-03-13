#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureViewGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureGL.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"
#include "config/LaunchSettings.h"

LatteTextureViewGL::LatteTextureViewGL(LatteTextureGL* texture, Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount, bool registerView, bool forceCreateNewTexId)
	: LatteTextureView(texture, firstMip, mipCount, firstSlice, sliceCount, dim, format, registerView)
{
	if (dim != texture->dim || format != texture->format ||
		firstSlice != 0 || firstMip != 0 || mipCount != texture->mipLevels || sliceCount != texture->depth ||	
		forceCreateNewTexId)
	{
		LatteTextureGL::GenerateEmptyTextureFromGX2Dim(dim, glTexId, glTexTarget, false);
		this->glInternalFormat = 0;
		InitAliasView();
	}
	else
	{
		glTexId = texture->glId_texture;
		glTexTarget = texture->glTexTarget;
		glInternalFormat = texture->glInternalFormat;
	}
	// mark all sampler properties as undefined
	samplerState.maxAniso = 0xFF;
	samplerState.filterMin = 0xFFFFFFFF;
	samplerState.filterMag = 0xFFFFFFFF;
	samplerState.maxMipLevels = 0xFF;
	samplerState.borderType = 0xFF;
	samplerState.borderColor[0] = 9999.0f;
	samplerState.borderColor[1] = 9999.0f;
	samplerState.borderColor[2] = 9999.0f;
	samplerState.borderColor[3] = 9999.0f;
	samplerState.clampS = 0xFF;
	samplerState.clampT = 0xFF;
	samplerState.clampR = 0xFF;
	samplerState.minLod = 0xFFFF;
	samplerState.maxLod = 0xFFFF;
	samplerState.lodBias = 0x7FFF;
	samplerState.depthCompareMode = 0xFF;
	samplerState.depthCompareFunc = 0xFF;
	swizzleR = 0xFF;
	swizzleG = 0xFF;
	swizzleB = 0xFF;
	swizzleA = 0xFF;
}

LatteTextureViewGL::~LatteTextureViewGL()
{
	delete m_alternativeView;
	((OpenGLRenderer*)g_renderer.get())->texture_notifyDelete(this);
	glDeleteTextures(1, &glTexId);
}

void LatteTextureViewGL::InitAliasView()
{
	const auto texture = (LatteTextureGL*)baseTexture;
	// compute internal format
	if(texture->overwriteInfo.hasFormatOverwrite)
	{
		cemu_assert_debug(format == texture->format);
		glInternalFormat = texture->glInternalFormat; // for format overwrite no aliasing is allowed and thus we always inherit the internal format of the base texture
	}
	else if (baseTexture->isDepth)
	{
		// depth is handled differently
		cemu_assert(format == texture->format); // is depth alias with different format intended?
		glInternalFormat = texture->glInternalFormat;
	}
	else
	{
		LatteTextureGL::FormatInfoGL glFormatInfo;
		LatteTextureGL::GetOpenGLFormatInfo(baseTexture->isDepth, format, dim, &glFormatInfo);
		glInternalFormat = glFormatInfo.glInternalFormat;
	}

	catchOpenGLError();
	if (firstMip >= texture->maxPossibleMipLevels)
	{
		cemuLog_logDebug(LogType::Force, "InitAliasView(): Out of bounds mip level requested");
		glTextureView(glTexId, glTexTarget, texture->glId_texture, glInternalFormat, texture->maxPossibleMipLevels - 1, numMip, firstSlice, this->numSlice);
	}
	else
		glTextureView(glTexId, glTexTarget, texture->glId_texture, glInternalFormat, firstMip, numMip, firstSlice, numSlice);

	catchOpenGLError();

	if (glTextureParameteri)
	{
		glTextureParameteri(glTexId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(glTexId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(glTexId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(glTexId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(glTexId, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTextureParameteri(glTexId, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	}
	else
	{
		// todo - fallback for when DSA isn't supported
	}

	// set debug name
	bool useGLDebugNames = false;
#ifdef CEMU_DEBUG_ASSERT
	useGLDebugNames = true;
#endif
	if (LaunchSettings::NSightModeEnabled())
		useGLDebugNames = true;
	if (useGLDebugNames)
	{
		char textureDebugLabel[512];
		sprintf(textureDebugLabel, "%08x_f%04x_p%04x_viewFMT%04x%s_org%d", baseTexture->physAddress, (uint32)baseTexture->format, baseTexture->pitch, (uint32)this->format, baseTexture->isDepth?"_d":"", texture->glId_texture);
		glObjectLabel(GL_TEXTURE, glTexId, -1, textureDebugLabel);
	}
}

LatteTextureViewGL* LatteTextureViewGL::GetAlternativeView()
{
	if (!m_alternativeView)
		m_alternativeView = new LatteTextureViewGL((LatteTextureGL*)baseTexture, dim, format, firstMip, numMip, firstSlice, numSlice, false, true);
	return m_alternativeView;
}
