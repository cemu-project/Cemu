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

#define GLFUNC(__type, __name)	extern __type __name;
#define EGLFUNC(__type, __name) extern __type __name;
#include "glFunctions.h"
#undef GLFUNC
#undef EGLFUNC

// this prevents Windows GL.h from being included:
#define __gl_h_
#define __GL_H__
