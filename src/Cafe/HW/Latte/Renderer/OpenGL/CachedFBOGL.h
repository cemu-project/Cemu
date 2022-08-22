#pragma once
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureViewGL.h"
#include "Cafe/HW/Latte/Core/LatteCachedFBO.h"
#include "Common/GLInclude/GLInclude.h"

class CachedFBOGL : public LatteCachedFBO
{
public:
	CachedFBOGL(uint64 key)
		: LatteCachedFBO(key)
	{
		// generate framebuffer
		if (glCreateFramebuffers && false)
			glCreateFramebuffers(1, &glId_fbo);
		else
			glGenFramebuffers(1, &glId_fbo);
		// bind framebuffer
		((OpenGLRenderer*)g_renderer.get())->rendertarget_bindFramebufferObject(this);
		// setup framebuffer
		for (sint32 i = 0; i < 8; i++)
		{
			LatteTextureViewGL* colorTexViewGL = (LatteTextureViewGL*)colorBuffer[i].texture;
			if (!colorTexViewGL)
				glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + i, GL_TEXTURE_2D, 0, 0);
			else if (colorTexViewGL->dim == Latte::E_DIM::DIM_2D)
				glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + i, GL_TEXTURE_2D, colorTexViewGL->glTexId, 0);
			else if (colorTexViewGL->dim == Latte::E_DIM::DIM_3D)
				glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0_EXT + i, colorTexViewGL->glTexId, 0, 0);
			else if (colorTexViewGL->dim == Latte::E_DIM::DIM_2D_ARRAY)
				glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0_EXT + i, colorTexViewGL->glTexId, 0, 0);
			else if (colorTexViewGL->dim == Latte::E_DIM::DIM_CUBEMAP)
				glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0_EXT + i, colorTexViewGL->glTexId, 0, 0);
			else
			{
				cemu_assert_suspicious();
			}
		}

		if (depthBuffer.texture)
		{
			LatteTextureViewGL* depthTexViewGL = (LatteTextureViewGL*)depthBuffer.texture;
			if (depthTexViewGL->dim == Latte::E_DIM::DIM_2D)
				glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexViewGL->glTexId, 0);
			else if (depthTexViewGL->dim == Latte::E_DIM::DIM_2D_ARRAY)
				glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexViewGL->glTexId, 0, 0);
			else
				cemu_assert_suspicious();
			if (depthBuffer.hasStencil)
			{
				if (depthTexViewGL->dim == Latte::E_DIM::DIM_2D)
					glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTexViewGL->glTexId, 0);
				else if (depthTexViewGL->dim == Latte::E_DIM::DIM_2D_ARRAY)
					glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, depthTexViewGL->glTexId, 0, 0);
				else
					cemu_assert_suspicious();
			}
			else
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			}
		}
		else
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		}
		SetDrawBuffers();
	}

private:
	void SetDrawBuffers()
	{
		GLenum buffers[8];
		uint32 bufferCount = 0;
		for (uint32 i = 0; i < 8; i++)
		{
			if (colorBuffer[i].texture)
			{
				buffers[bufferCount] = GL_COLOR_ATTACHMENT0_EXT + i;
				bufferCount++;
			}
			else
			{
				// pad with GL_NONE entries to make sure that draw buffer indices match up with color attachment indices
				buffers[bufferCount] = GL_NONE;
				bufferCount++;
			}
		}
		// trim trailing GL_NONE
		while (bufferCount > 0 && buffers[bufferCount - 1] == GL_NONE)
			bufferCount--;

		if (bufferCount == 0)
			glDrawBuffer(GL_NONE);
		else
			glDrawBuffers(bufferCount, buffers);
	}

public:
	GLuint glId_fbo{};
};
