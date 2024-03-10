#pragma once

#include "glext.h"

#if BOOST_OS_WINDOWS > 0
#include "wglext.h"
#endif

#if BOOST_OS_LINUX > 0

// from Xlib
#define Bool int
#define Status int
#define True 1
#define False 0

// from system glx.h
typedef XID GLXContextID;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
typedef XID GLXPbuffer;
typedef XID GLXWindow;
typedef XID GLXFBConfigID;
typedef struct __GLXcontextRec *GLXContext;
typedef struct __GLXFBConfigRec *GLXFBConfig;

#define EGL_EGL_PROTOTYPES 0
#include "egl.h"
#undef EGL_EGL_PROTOTYPES
#include "glxext.h"

#undef Bool
#undef Status
#undef True
#undef False

#endif

namespace CemuGL
{
#define GLFUNC(__type, __name)	extern __type __name;
#define EGLFUNC(__type, __name) extern __type __name;
#include "glFunctions.h"
#undef GLFUNC
#undef EGLFUNC

// DSA-style helpers with fallback to legacy API if DSA is not supported

#define DSA_FORCE_DISABLE false // set to true to simulate DSA not being supported

static GLenum GetGLBindingFromTextureTarget(GLenum texTarget)
{
	switch(texTarget)
	{
	case GL_TEXTURE_1D: return GL_TEXTURE_BINDING_1D;
	case GL_TEXTURE_2D: return GL_TEXTURE_BINDING_2D;
	case GL_TEXTURE_3D: return GL_TEXTURE_BINDING_3D;
	case GL_TEXTURE_2D_ARRAY: return GL_TEXTURE_BINDING_2D_ARRAY;
	case GL_TEXTURE_CUBE_MAP: return GL_TEXTURE_BINDING_CUBE_MAP;
	case GL_TEXTURE_CUBE_MAP_ARRAY: return GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
	default:
		cemu_assert_unimplemented();
		return 0;
	}
}

static GLuint glCreateTextureWrapper(GLenum target)
{
	GLuint tex;
	if (glCreateTextures && !DSA_FORCE_DISABLE)
	{
		glCreateTextures(target, 1, &tex);
		return tex;
	}
	GLint originalTexture;
	glGetIntegerv(GetGLBindingFromTextureTarget(target), &originalTexture);
	glGenTextures(1, &tex);
	glBindTexture(target, tex);
	glBindTexture(target, originalTexture);
	return tex;
}

static void glTextureStorage1DWrapper(GLenum target, GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width)
{
	if (glTextureStorage1D && !DSA_FORCE_DISABLE)
	{
		glTextureStorage1D(texture, levels, internalformat, width);
		return;
	}
	GLenum binding = GetGLBindingFromTextureTarget(target);
	GLint originalTexture;
	glGetIntegerv(binding, &originalTexture);
	glBindTexture(target, texture);
	glTexStorage1D(target, levels, internalformat, width);
	glBindTexture(target, originalTexture);
}

static void glTextureStorage2DWrapper(GLenum target, GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	if (glTextureStorage2D && !DSA_FORCE_DISABLE)
	{
		glTextureStorage2D(texture, levels, internalformat, width, height);
		return;
	}
	GLenum binding = GetGLBindingFromTextureTarget(target);
	GLint originalTexture;
	glGetIntegerv(binding, &originalTexture);
	glBindTexture(target, texture);
	glTexStorage2D(target, levels, internalformat, width, height);
	glBindTexture(target, originalTexture);
}

static void glTextureStorage3DWrapper(GLenum target, GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	if (glTextureStorage3D && !DSA_FORCE_DISABLE)
	{
		glTextureStorage3D(texture, levels, internalformat, width, height, depth);
		return;
	}
	GLenum binding = GetGLBindingFromTextureTarget(target);
	GLint originalTexture;
	glGetIntegerv(binding, &originalTexture);
	glBindTexture(target, texture);
	glTexStorage3D(target, levels, internalformat, width, height, depth);
	glBindTexture(target, originalTexture);
}

static void glTextureSubImage1DWrapper(GLenum target, GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels)
{
	if (glTextureSubImage1D && !DSA_FORCE_DISABLE)
	{
		glTextureSubImage1D(texture, level, xoffset, width, format, type, pixels);
		return;
	}
	GLenum binding = GetGLBindingFromTextureTarget(target);
	GLint originalTexture;
	glGetIntegerv(binding, &originalTexture);
	glBindTexture(target, texture);
	glTexSubImage1D(target, level, xoffset, width, format, type, pixels);
	glBindTexture(target, originalTexture);
}

static void glCompressedTextureSubImage1DWrapper(GLenum target, GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data)
{
	if (glCompressedTextureSubImage1D && !DSA_FORCE_DISABLE)
	{
		glCompressedTextureSubImage1D(texture, level, xoffset, width, format, imageSize, data);
		return;
	}
	GLenum binding = GetGLBindingFromTextureTarget(target);
	GLint originalTexture;
	glGetIntegerv(binding, &originalTexture);
	glBindTexture(target, texture);
	glCompressedTexSubImage1D(target, level, xoffset, width, format, imageSize, data);
	glBindTexture(target, originalTexture);
}

static void glTextureSubImage2DWrapper(GLenum target, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels)
{
	if (glTextureSubImage2D && !DSA_FORCE_DISABLE)
	{
		glTextureSubImage2D(texture, level, xoffset, yoffset, width, height, format, type, pixels);
		return;
	}
	GLenum binding = GetGLBindingFromTextureTarget(target);
	GLint originalTexture;
	glGetIntegerv(binding, &originalTexture);
	glBindTexture(target, texture);
	glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
	glBindTexture(target, originalTexture);
}

static void glCompressedTextureSubImage2DWrapper(GLenum target, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data)
{
	if (glCompressedTextureSubImage2D && !DSA_FORCE_DISABLE)
	{
		glCompressedTextureSubImage2D(texture, level, xoffset, yoffset, width, height, format, imageSize, data);
		return;
	}
	GLenum binding = GetGLBindingFromTextureTarget(target);
	GLint originalTexture;
	glGetIntegerv(binding, &originalTexture);
	glBindTexture(target, texture);
	glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
	glBindTexture(target, originalTexture);
}

static void glTextureSubImage3DWrapper(GLenum target, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels)
{
	if(glTextureSubImage3D && !DSA_FORCE_DISABLE)
	{
		glTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
		return;
	}
	GLenum binding = GetGLBindingFromTextureTarget(target);
	GLint originalTexture;
	glGetIntegerv(binding, &originalTexture);
	glBindTexture(target, texture);
	glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	glBindTexture(target, originalTexture);
}

static void glCompressedTextureSubImage3DWrapper(GLenum target, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data)
{
	if(glCompressedTextureSubImage3D && !DSA_FORCE_DISABLE)
	{
		glCompressedTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
		return;
	}
	GLenum binding = GetGLBindingFromTextureTarget(target);
	GLint originalTexture;
	glGetIntegerv(binding, &originalTexture);
	glBindTexture(target, texture);
	glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
	glBindTexture(target, originalTexture);
}

}
using namespace CemuGL;
// this prevents Windows GL.h from being included:
#define __gl_h_
#define __GL_H__
