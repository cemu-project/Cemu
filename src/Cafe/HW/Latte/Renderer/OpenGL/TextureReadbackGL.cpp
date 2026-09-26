#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLTextureReadback.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureGL.h"

LatteTextureReadbackInfoGL::LatteTextureReadbackInfoGL(LatteTextureView* textureView)
	: LatteTextureReadbackInfo(textureView, textureView->firstMip)
{
	LatteTexture* baseTexture = textureView->baseTexture;
	// handle format
	if (textureView->format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM)
	{
		m_texFormatGL = GL_RGBA;
		m_texDataTypeGL = GL_UNSIGNED_BYTE;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB)
	{
		m_texFormatGL = GL_RGBA;
		m_texDataTypeGL = GL_UNSIGNED_BYTE;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R32_G32_B32_A32_FLOAT)
	{
		m_texFormatGL = GL_RGBA;
		m_texDataTypeGL = GL_FLOAT;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R32_G32_B32_A32_UINT)
	{
		m_texFormatGL = GL_RGBA_INTEGER;
		m_texDataTypeGL = GL_UNSIGNED_INT;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R32_G32_B32_A32_SINT)
	{
		m_texFormatGL = GL_RGBA_INTEGER;
		m_texDataTypeGL = GL_INT;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R32_FLOAT)
	{
		if (baseTexture->isDepth)
		{
			m_texFormatGL = GL_DEPTH_COMPONENT;
			m_texDataTypeGL = GL_FLOAT;
		}
		else
		{
			m_texFormatGL = GL_RED;
			m_texDataTypeGL = GL_FLOAT;
		}
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R16_UNORM)
	{
		if (baseTexture->isDepth)
		{
			m_texFormatGL = GL_DEPTH_COMPONENT;
			m_texDataTypeGL = GL_UNSIGNED_SHORT;
			cemu_assert_unimplemented();
		}
		else
		{
			m_texFormatGL = GL_RED;
			m_texDataTypeGL = GL_UNSIGNED_SHORT;
		}
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT)
	{
		m_texFormatGL = GL_RGBA;
		m_texDataTypeGL = GL_HALF_FLOAT;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R8_G8_UNORM)
	{
		m_texFormatGL = GL_RG;
		m_texDataTypeGL = GL_UNSIGNED_BYTE;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM)
	{
		m_texFormatGL = GL_RGBA;
		m_texDataTypeGL = GL_UNSIGNED_SHORT;
	}
	else
	{
		cemuLog_logDebug(LogType::Force, "Unsupported texture readback format {:04x}", (uint32)textureView->format);
		return;
	}
	// OpenGL uses the default 4 byte pack alignment because it gives us slightly better row copy performance
	m_rowPitch = GetReadbackRowPitch(textureView, 4);
	m_image_size = GetReadbackImageSize(textureView, m_rowPitch);
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
	LatteTextureGL* baseTexture = (LatteTextureGL*)m_textureView->baseTexture;
	cemu_assert_debug(m_textureView->baseTexture->dim != Latte::E_DIM::DIM_3D);
	// create unsynchronized buffer
	glGenBuffers(1, &texImageBufferGL);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, texImageBufferGL);
	glBufferData(GL_PIXEL_PACK_BUFFER, m_image_size, NULL, GL_DYNAMIC_READ);
	// request texture read into buffer
	glGetTextureSubImage(baseTexture->glId_texture, m_firstMip, 0, 0, m_firstSlice, baseTexture->GetMipWidth(m_firstMip), baseTexture->GetMipHeight(m_firstMip), 1, m_texFormatGL, m_texDataTypeGL, m_image_size, NULL);
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
