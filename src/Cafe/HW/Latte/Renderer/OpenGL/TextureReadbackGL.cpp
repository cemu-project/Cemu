#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLTextureReadback.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureViewGL.h"

LatteTextureReadbackInfoGL::LatteTextureReadbackInfoGL(LatteTextureView* textureView)
	: LatteTextureReadbackInfo(textureView)
{
	LatteTexture* baseTexture = textureView->baseTexture;
	// handle format
	if (textureView->format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM)
	{
		m_image_size = baseTexture->width*baseTexture->height * 4;
		m_texFormatGL = GL_RGBA;
		m_texDataTypeGL = GL_UNSIGNED_BYTE;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB)
	{
		m_image_size = baseTexture->width*baseTexture->height * 4;
		m_texFormatGL = GL_RGBA;
		m_texDataTypeGL = GL_UNSIGNED_BYTE;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R32_G32_B32_A32_FLOAT)
	{
		m_image_size = baseTexture->width*baseTexture->height * 16;
		m_texFormatGL = GL_RGBA;
		m_texDataTypeGL = GL_FLOAT;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R32_FLOAT)
	{
		if (baseTexture->isDepth)
		{
			m_image_size = baseTexture->width*baseTexture->height * 4;
			m_texFormatGL = GL_DEPTH_COMPONENT;
			m_texDataTypeGL = GL_FLOAT;
		}
		else
		{
			m_image_size = baseTexture->width*baseTexture->height * 4;
			m_texFormatGL = GL_RED;
			m_texDataTypeGL = GL_FLOAT;
		}
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R16_UNORM)
	{
		if (baseTexture->isDepth)
		{
			m_image_size = baseTexture->width*baseTexture->height * 2;
			m_texFormatGL = GL_DEPTH_COMPONENT;
			m_texDataTypeGL = GL_UNSIGNED_SHORT;
			cemu_assert_unimplemented();
		}
		else
		{
			m_image_size = baseTexture->width*baseTexture->height * 2;
			m_texFormatGL = GL_RED;
			m_texDataTypeGL = GL_UNSIGNED_SHORT;
		}
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT)
	{
		m_image_size = baseTexture->width*baseTexture->height * 8;
		m_texFormatGL = GL_RGBA;
		m_texDataTypeGL = GL_HALF_FLOAT;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R8_G8_UNORM)
	{
		m_image_size = baseTexture->width*baseTexture->height * 2;
		m_texFormatGL = GL_RG;
		m_texDataTypeGL = GL_UNSIGNED_BYTE;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM)
	{
		m_image_size = baseTexture->width*baseTexture->height * 8;
		m_texFormatGL = GL_RGBA;
		m_texDataTypeGL = GL_UNSIGNED_SHORT;
	}
	else
	{
		cemuLog_logDebug(LogType::Force, "Unsupported texture readback format {:04x}", (uint32)textureView->format);
		return;
	}
}

LatteTextureReadbackInfoGL::~LatteTextureReadbackInfoGL()
{
	if(imageCopyFinSync != 0)
		glDeleteSync(imageCopyFinSync);

	if(texImageBufferGL)
		glDeleteBuffers(1, &texImageBufferGL);
}

void LatteTextureReadbackInfoGL::StartTransfer()
{
	cemu_assert(m_textureView);
	((OpenGLRenderer*)g_renderer.get())->texture_bindAndActivate(m_textureView, 0);
	// create unsynchronized buffer
	glGenBuffers(1, &texImageBufferGL);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, texImageBufferGL);
	glBufferData(GL_PIXEL_PACK_BUFFER, m_image_size, NULL, GL_DYNAMIC_READ);
	// request texture read into buffer
	glGetTexImage(((LatteTextureViewGL*)m_textureView)->glTexTarget, 0, m_texFormatGL, m_texDataTypeGL, NULL);
	glFlush();
	// create fence sync (so we can check if the image copy operation finished)
	imageCopyFinSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	m_textureView = nullptr;
}

bool LatteTextureReadbackInfoGL::IsFinished()
{
	GLenum status = glClientWaitSync(imageCopyFinSync, 0, 0);
		if (status == GL_TIMEOUT_EXPIRED)
			return false;
		else if (status == GL_ALREADY_SIGNALED || status == GL_SIGNALED)
			return true;
		else
			throw std::runtime_error("_updateFinishedTransfers(): Error during readback sync check\n");
}

uint8* LatteTextureReadbackInfoGL::GetData()
{
		glBindBuffer(GL_PIXEL_PACK_BUFFER, texImageBufferGL);
		return (uint8*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
}


void LatteTextureReadbackInfoGL::ReleaseData()
{
	glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
}
